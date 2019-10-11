/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_model.c -- model loading and caching

#include "r_local.h"
#include "iqm.h"

typedef struct {
	unsigned number;
	int cluster;
	unsigned drawSurfIndex;
	msurface_t *surf;
} msortedSurface_t;

void Mod_LoadAliasMD3Model( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *unused );
void Mod_LoadSkeletalModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *unused );
void Mod_LoadQ3BrushModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *format );
void Mod_LoadQ2BrushModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *format );
void Mod_LoadQ1BrushModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *format );
void Mod_FixupQ1MipTex( model_t *mod );

static void R_InitMapConfig( const char *model );
static void R_FinishMapConfig( const model_t *mod );
static void R_LoadWorldRtLights( model_t *model );
static void R_LoadWorldRtSkyLights( model_t *model );

static uint8_t mod_novis[MAX_MAP_LEAFS / 8];

#define MAX_MOD_KNOWN   512 * MOD_MAX_LODS
static model_t mod_known[MAX_MOD_KNOWN];
static int mod_numknown;
static int modfilelen;
static bool mod_isworldmodel;
model_t *r_prevworldmodel;
static mapconfig_t *mod_mapConfigs;

static mempool_t *mod_mempool;

static const modelFormatDescr_t mod_supportedformats[] =
{
	// Quake III Arena .md3 models
	{ IDMD3HEADER, 4, NULL, MOD_MAX_LODS, ( const modelLoader_t )Mod_LoadAliasMD3Model },

	// Skeletal models
	{ IQM_MAGIC, sizeof( IQM_MAGIC ), NULL, MOD_MAX_LODS, ( const modelLoader_t )Mod_LoadSkeletalModel },

	// Q3-alike .bsp models
	{ "*", 4, q3BSPFormats, 0, ( const modelLoader_t )Mod_LoadQ3BrushModel },

	// Q2 .bsp models
	{ "*", 4, q2BSPFormats, 0, ( const modelLoader_t )Mod_LoadQ2BrushModel },

	// Q1 .bsp models
	{ "*", 0, q1BSPFormats, 0, ( const modelLoader_t )Mod_LoadQ1BrushModel },

	// trailing NULL
	{ NULL, 0, NULL, 0, NULL }
};

//===============================================================================

/*
* Mod_PointInLeaf
*/
mleaf_t *Mod_PointInLeaf( const vec3_t p, mbrushmodel_t *bmodel ) {
	mnode_t *node;
	cplane_t *plane;

	if( !bmodel || !bmodel->nodes ) {
		ri.Com_Error( ERR_DROP, "Mod_PointInLeaf: bad model" );
		return NULL;
	}

	node = bmodel->nodes;
	do {
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	} while( node->plane != NULL );

	return ( mleaf_t * )node;
}

/*
* Mod_ClusterPVS
*/
uint8_t *Mod_ClusterPVS( int cluster, mbrushmodel_t *bmodel ) {
	dvis_t *vis = bmodel->pvs;

	if( cluster < 0 || !vis ) {
		return mod_novis;
	}

	return ( (uint8_t *)vis->data + cluster * vis->rowsize );
}

/*
* Mod_SpherePVS_r
*/
static void Mod_SpherePVS_r( mnode_t *node, const vec3_t origin, float radius, const dvis_t *vis, uint8_t *fatpvs ) {
	int i;
	const mleaf_t *leaf;
	const uint8_t *row;

	while( node->plane != NULL ) {
		float d = PlaneDiff( origin, node->plane );

		if( d > radius - ON_EPSILON ) {
			node = node->children[0];
		} else if( d < -radius + ON_EPSILON ) {
			node = node->children[1];
		}  else {
			Mod_SpherePVS_r( node->children[0], origin, radius, vis, fatpvs );
			node = node->children[1];
		}
	}

	leaf = ( const mleaf_t * )node;
	if( leaf->cluster < 0 ) {
		return;
	}

	row = (uint8_t *)vis->data + leaf->cluster * vis->rowsize;
	for( i = 0; i < vis->rowsize; i++ )
		fatpvs[i] |= row[i];
}

/*
* Mod_SpherePVS
*/
uint8_t *Mod_SpherePVS( const vec3_t origin, float radius, mbrushmodel_t *bmodel, uint8_t *fatpvs ) {
	const dvis_t *vis;
	
	vis = bmodel->pvs;
	if( !vis ) {
		return mod_novis;
	}

	memset( fatpvs, 0, vis->rowsize );
	Mod_SpherePVS_r( bmodel->nodes, origin, radius, vis, fatpvs );
	return fatpvs;
}

//===============================================================================

/*
* Mod_CreateVisLeafs
*/
static void Mod_CreateVisLeafs( model_t *mod ) {
	unsigned i, j;
	unsigned count;
	unsigned numVisSurfaces, numFragmentSurfaces;
	mleaf_t *leaf;
	msurface_t *surf;
	mbrushmodel_t *loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	count = loadbmodel->numleafs;

	for( i = 0; i < count; i++ ) {
		numVisSurfaces = numFragmentSurfaces = 0;

		leaf = loadbmodel->leafs + i;
		if( leaf->cluster < 0 || !leaf->numVisSurfaces ) {
			leaf->visSurfaces = NULL;
			leaf->numVisSurfaces = 0;
			leaf->fragmentSurfaces = NULL;
			leaf->numFragmentSurfaces = 0;
			leaf->fragmentSurfaces = NULL;
			continue;
		}

		for( j = 0; j < leaf->numVisSurfaces; j++ ) {
			unsigned surfNum;

			surfNum = leaf->visSurfaces[j];
			surf = loadbmodel->surfaces + surfNum;

			if( !R_SurfNoDraw( surf ) ) {
				leaf->visSurfaces[numVisSurfaces++] = surfNum;
				if( R_SurfPotentiallyFragmented( surf ) ) {
					leaf->fragmentSurfaces[numFragmentSurfaces++] = surfNum;
				}
			}
		}

		leaf->numVisSurfaces = numVisSurfaces;
		leaf->numFragmentSurfaces = numFragmentSurfaces;
	}
}

/*
* Mod_CalculateAutospriteBounds
*
* Make bounding box of an autosprite surf symmetric and enlarges it
* to account for rotation along the longest axis.
*/
static void Mod_CalculateAutospriteBounds( msurface_t *surf ) {
	int j;
	int l_axis, s1_axis, s2_axis;
	vec_t dist, max_dist;
	vec_t radius[3];
	vec3_t centre;
	vec_t *mins = surf->mins, *maxs = surf->maxs;

	// find the longest axis
	l_axis = 2;
	max_dist = -9999999;
	for( j = 0; j < 3; j++ ) {
		dist = maxs[j] - mins[j];
		if( dist > max_dist ) {
			l_axis = j;
			max_dist = dist;
		}

		// make the bbox symmetrical
		radius[j] = dist * 0.5;
		centre[j] = ( maxs[j] + mins[j] ) * 0.5;
		mins[j] = centre[j] - radius[j];
		maxs[j] = centre[j] + radius[j];
	}

	// shorter axis
	s1_axis = ( l_axis + 1 ) % 3;
	s2_axis = ( l_axis + 2 ) % 3;

	// enlarge the bounding box, accouting for rotation along the longest axis
	maxs[s1_axis] = max( maxs[s1_axis], centre[s1_axis] + radius[s2_axis] );
	maxs[s2_axis] = max( maxs[s2_axis], centre[s2_axis] + radius[s1_axis] );

	mins[s1_axis] = min( mins[s1_axis], centre[s1_axis] - radius[s2_axis] );
	mins[s2_axis] = min( mins[s2_axis], centre[s2_axis] - radius[s1_axis] );
}

/*
* Mod_FinishFaces
*/
static void Mod_FinishFaces( model_t *mod ) {
	unsigned int i, j;
	shader_t *shader;
	mbrushmodel_t *loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	for( i = 0; i < loadbmodel->numsurfaces; i++ ) {
		vec_t *vert;
		msurface_t *surf;
		const mesh_t *mesh;

		surf = loadbmodel->surfaces + i;
		mesh = &surf->mesh;
		shader = surf->shader;

		if( R_SurfNoDraw( surf ) ) {
			continue;
		}

		// calculate bounding box of a surface
		vert = mesh->xyzArray[0];
		VectorCopy( vert, surf->mins );
		VectorCopy( vert, surf->maxs );
		for( j = 1, vert += 4; j < mesh->numVerts; j++, vert += 4 ) {
			AddPointToBounds( vert, surf->mins, surf->maxs );
		}

		// foliage surfaces need special treatment for bounds
		if( surf->facetype == FACETYPE_FOLIAGE ) {
			vec3_t temp;

			for( j = 0, vert = &surf->instances[0][4]; j < surf->numInstances; j++, vert += 8 ) {
				VectorMA( vert, vert[3], surf->mins, temp );
				AddPointToBounds( temp, surf->mins, surf->maxs );

				VectorMA( vert, vert[3], surf->maxs, temp );
				AddPointToBounds( temp, surf->mins, surf->maxs );
			}
		}

		// handle autosprites
		if( shader->flags & SHADER_AUTOSPRITE ) {
			// handle autosprites as trisurfs to avoid backface culling
			surf->facetype = FACETYPE_TRISURF;

			Mod_CalculateAutospriteBounds( surf );
		}
	}
}

/*
* Mod_SetupSubmodels
*/
static void Mod_SetupSubmodels( model_t *mod ) {
	unsigned int i;
	mmodel_t *bm;
	model_t *starmod;
	mbrushmodel_t *bmodel;
	mbrushmodel_t *loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	// set up the submodels
	for( i = 0; i < loadbmodel->numsubmodels; i++ ) {
		bm = &loadbmodel->submodels[i];
		starmod = &loadbmodel->inlines[i];
		bmodel = ( mbrushmodel_t * )starmod->extradata;

		memcpy( starmod, mod, sizeof( model_t ) );
		if( i ) {
			memcpy( bmodel, loadbmodel, sizeof( mbrushmodel_t ) );
		}

		bmodel->firstModelSurface = bm->firstModelSurface;
		bmodel->numModelSurfaces = bm->numModelSurfaces;

		bmodel->firstModelDrawSurface = bm->firstModelDrawSurface;
		bmodel->numModelDrawSurfaces = bm->numModelDrawSurfaces;

		starmod->extradata = bmodel;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		if( i == 0 ) {
			*mod = *starmod;
		} else {
			bmodel->numsubmodels = 0;
		}
	}
}

#define VBO_Printf ri.Com_DPrintf

static mbrushmodel_t *loadbmodel_; // FIXME

/*
* R_SurfaceCmp
*/
static int R_SurfaceCmp( const msurface_t *s1, const msurface_t *s2 ) {
	struct superLightStyle_s *sl1 = NULL;
	struct superLightStyle_s *sl2 = NULL;
	int va1 = 0, va2  = 0;
	int sid1 = 0, sid2 = 0;

	if( s1->shader != NULL ) {
		va1 = s1->shader->vattribs;
		sid1 = s1->shader->id;
	}

	if( s2->shader != NULL ) {
		va2 = s2->shader->vattribs;
		sid2 = s2->shader->id;
	}

	if( s1->superLightStyle >= 0 ) {
		sl1 = &loadbmodel_->superLightStyles[s1->superLightStyle];
		va1 |= sl1->vattribs;
	}
	if( s2->superLightStyle >= 0 ) {
		sl2 = &loadbmodel_->superLightStyles[s2->superLightStyle];
		va2 |= sl2->vattribs;
	}

	if( s1->numInstances ) {
		va1 |= VATTRIB_INSTANCES_BITS;
	}
	if( s2->numInstances ) {
		va2 |= VATTRIB_INSTANCES_BITS;
	}

	if( va1 > va2 ) {
		return 1;
	}
	if( va1 < va2 ) {
		return -1;
	}

	if( sid1 > sid2 ) {
		return 1;
	}
	if( sid1 < sid2 ) {
		return -1;
	}

	if( s1->fog > s2->fog ) {
		return 1;
	}
	if( s1->fog < s2->fog ) {
		return -1;
	}

	if( s1->superLightStyle > s2->superLightStyle ) {
		return 1;
	}
	if( s1->superLightStyle < s2->superLightStyle ) {
		return -1;
	}

	return 0;
}

/*
* R_SortSurfacesCmp
*/
static int R_SortSurfacesCmp( const void *ps1, const void *ps2 ) {
	const msurface_t *s1 = *((const msurface_t **)ps1);
	const msurface_t *s2 = *((const msurface_t **)ps2);
	int cmp;
	
	cmp = R_SurfaceCmp( s1, s2 );
	if( cmp == 0 ) {
		return s1 - s2;
	}

	return cmp;
}

/*
* Mod_CreateSubmodelBufferObjects
*/
static int Mod_CreateSubmodelBufferObjects( model_t *mod, size_t *vbo_total_size ) {
	unsigned int i, j;
	unsigned int modnum;
	mmodel_t *bm;
	msurface_t *surf, *surf2;
	msurface_t **sortedSurfaces;
	drawSurfaceBSP_t *drawSurf;
	int num_vbos;
	vattribmask_t floatVattribs;
	mesh_vbo_t *tempVBOs;
	unsigned numTempVBOs, maxTempVBOs;
	unsigned *worldSurfaces;
	mesh_vbo_t *vbo;
	mbrushmodel_t *loadbmodel;

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );
	loadbmodel_ = loadbmodel; // FIXME

	worldSurfaces = ( unsigned * )Mod_Malloc( mod, loadbmodel->numsurfaces * sizeof( *worldSurfaces ) );
	sortedSurfaces = ( msurface_t ** )Mod_Malloc( mod, loadbmodel->numsurfaces * sizeof( *sortedSurfaces ) );
	for( i = 0, surf = loadbmodel->surfaces; i < loadbmodel->numsurfaces; i++, surf++ ) {
		sortedSurfaces[i] = surf;
	}

	numTempVBOs = 0;
	maxTempVBOs = 1024;
	tempVBOs = ( mesh_vbo_t * )Mod_Malloc( mod, maxTempVBOs * sizeof( *tempVBOs ) );

	// now linearly scan all faces for this submodel, merging them into
	// vertex buffer objects if they share shader, lightmap texture and we can render
	// them in hardware (some Q3A shaders require GLSL for that)

	// don't use half-floats for XYZ due to precision issues
	floatVattribs = VATTRIB_POSITION_BIT|VATTRIB_SURFINDEX_BIT;
	if( mapConfig.maxLightmapSize > 1024 ) {
		// don't use half-floats for lightmaps if there's not enough precision (half mantissa is 10 bits)
		floatVattribs |= VATTRIB_LMCOORDS_BITS;
	}

	num_vbos = 0;
	*vbo_total_size = 0;
	vbo = NULL;

	for( modnum = 0; modnum < loadbmodel->numsubmodels; modnum++ ) {
		bm = loadbmodel->submodels + modnum;
		bm->numModelDrawSurfaces = 0;
		bm->firstModelDrawSurface = loadbmodel->numDrawSurfaces;

		qsort( sortedSurfaces + bm->firstModelSurface, bm->numModelSurfaces, sizeof( *sortedSurfaces ), &R_SortSurfacesCmp );

		for( i = 0; i < bm->numModelSurfaces;  ) {
			shader_t *shader;
			int fcount;
			int vcount, ecount;
			vattribmask_t vattribs;
			bool mergable;
			vec3_t mins, maxs;

			surf = sortedSurfaces[bm->firstModelSurface+i];
			shader = surf->shader;

			if( R_SurfNoDraw( surf ) || surf->drawSurf != 0 || !shader ) {
				i++;
				continue;
			}

			// create vertex buffer object for this face then upload data
			vattribs = shader->vattribs | VATTRIB_NORMAL_BIT | VATTRIB_SURFINDEX_BIT;
			if( surf->numInstances ) {
				vattribs |= VATTRIB_INSTANCES_BITS;
			}
			if( surf->superLightStyle >= 0 ) {
				vattribs |= loadbmodel->superLightStyles[surf->superLightStyle].vattribs;
			}

			// allocate a drawsurf
			drawSurf = &loadbmodel->drawSurfaces[loadbmodel->numDrawSurfaces++];
			drawSurf->type = ST_BSP;
			drawSurf->superLightStyle = surf->superLightStyle;
			drawSurf->instances = surf->instances;
			drawSurf->numInstances = surf->numInstances;
			drawSurf->fog = surf->fog;
			drawSurf->shader = surf->shader;
			drawSurf->numLightmaps = 0;
			drawSurf->surfFlags = surf->flags;
			drawSurf->numLightmaps = 0;

			// count lightmaps
			if( surf->superLightStyle >= 0 ) {
				for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
					if( loadbmodel->superLightStyles[surf->superLightStyle].lightmapStyles[j] == 255 )
						break;
					drawSurf->numLightmaps++;
				}
			}

			drawSurf->numWorldSurfaces = 1;
			drawSurf->worldSurfaces = worldSurfaces;
			drawSurf->worldSurfaces[0] = surf - loadbmodel->surfaces;

			// upload vertex and elements data for face itself
			surf->drawSurf = loadbmodel->numDrawSurfaces;
			surf->firstDrawSurfVert = 0;
			surf->firstDrawSurfElem = 0;

			// portal or foliage surfaces can not be batched
			mergable = true;
			if( ( shader->flags & ( SHADER_PORTAL_CAPTURE | SHADER_PORTAL_CAPTURE2 ) ) || surf->numInstances ) {
				mergable = false;
			}

			fcount = 1;
			vcount = surf->mesh.numVerts;
			ecount = surf->mesh.numElems;
			CopyBounds( surf->mins, surf->maxs, mins, maxs );

			if( mergable ) {
				vec_t testlen;
				vec3_t testmins, testmaxs, testsize;

				// scan remaining face checking whether we merge them with the current one
				for( j = i + 1; j < bm->numModelSurfaces; j++ ) {
					surf2 = sortedSurfaces[bm->firstModelSurface+j];

					if( R_SurfaceCmp( surf, surf2 ) || surf2->numInstances )  {
						break;
					}
					if( R_SurfNoDraw( surf2 ) || surf2->drawSurf != 0 ) {
						continue;
					}

					// keep the draw surface spatially compact
					CopyBounds( mins, maxs, testmins, testmaxs );
					AddPointToBounds( surf2->mins, testmins, testmaxs );
					AddPointToBounds( surf2->maxs, testmins, testmaxs );

					testsize[0] = testmaxs[0] - testmins[0];
					testsize[1] = testmaxs[1] - testmins[1];
					testsize[2] = testmaxs[2] - testmins[2];
					testlen = max( max( testsize[0], testsize[1] ), testsize[2] );

					if( fcount == MAX_DRAWSURF_SURFS ) {
						break;
					}
					if( vcount + surf2->mesh.numVerts >= USHRT_MAX ) {
						break;
					}

					drawSurf->worldSurfaces[fcount] = surf2 - loadbmodel->surfaces;

					surf2->drawSurf = loadbmodel->numDrawSurfaces;
					surf2->firstDrawSurfVert = vcount;
					surf2->firstDrawSurfElem = ecount;

					fcount++;
					vcount += surf2->mesh.numVerts;
					ecount += surf2->mesh.numElems;

					CopyBounds( testmins, testmaxs, mins, maxs );
				}
			}

			if( !vbo || vbo->vertexAttribs != vattribs || vbo->numVerts + vcount >= USHRT_MAX || vbo->instancesOffset ) {
				// create temp VBO to hold pre-batched info
				if( numTempVBOs == maxTempVBOs ) {
					maxTempVBOs += 1024;
					tempVBOs = Mod_Realloc( tempVBOs, maxTempVBOs * sizeof( *tempVBOs ) );
				}

				vbo = &tempVBOs[numTempVBOs];
				vbo->owner = NULL;
				vbo->numVerts = 0;
				vbo->numElems = 0;
				vbo->vertexAttribs = vattribs;
				vbo->index = numTempVBOs;
				vbo->instancesOffset = drawSurf->numInstances;
				numTempVBOs++;
			}

			drawSurf->numVerts = vcount;
			drawSurf->numElems = ecount;
			drawSurf->numWorldSurfaces = fcount;
			drawSurf->vbo = vbo->index;
			drawSurf->firstVboVert = vbo->numVerts;
			drawSurf->firstVboElem = vbo->numElems;

			vbo->numVerts += vcount;
			vbo->numElems += ecount;

			*vbo_total_size += vbo->arrayBufferSize + vbo->elemBufferSize;

			i++;
			worldSurfaces += fcount;
		}

		bm->numModelDrawSurfaces = loadbmodel->numDrawSurfaces - bm->firstModelDrawSurface;
	}

	// create real VBOs and assign owner pointers
	for( i = 0; i < numTempVBOs; i++ ) {
		vbo = &tempVBOs[i];

		// don't use half-floats for XYZ due to precision issues
		vbo->owner = R_CreateMeshVBO( vbo, vbo->numVerts, vbo->numElems, (int)vbo->instancesOffset,
			vbo->vertexAttribs, VBO_TAG_WORLD, vbo->vertexAttribs & ~floatVattribs );

		num_vbos++;
	}

	// upload data to merged VBO's and assign offsets to drawSurfs
	for( i = 0; i < loadbmodel->numDrawSurfaces; i++ ) {
		const mesh_t *mesh;
		int vertsOffset, elemsOffset;

		drawSurf = &loadbmodel->drawSurfaces[i];
		vbo = tempVBOs[drawSurf->vbo].owner;
		drawSurf->vbo = vbo->index;

		if( !vbo ) {
			continue;
		}

		for( j = 0; j < drawSurf->numWorldSurfaces; j++ ) {
			unsigned si = drawSurf->worldSurfaces[j];
			
			surf = loadbmodel->surfaces + si;
			mesh = &surf->mesh;

			vertsOffset = drawSurf->firstVboVert + surf->firstDrawSurfVert;
			elemsOffset = drawSurf->firstVboElem + surf->firstDrawSurfElem;

			R_UploadVBOVertexData( vbo, vertsOffset, vbo->vertexAttribs, mesh, j );
			R_UploadVBOElemData( vbo, vertsOffset, elemsOffset, mesh );
			R_UploadVBOInstancesData( vbo, 0, surf->numInstances, surf->instances );
		}
	}

	R_Free( tempVBOs );
	R_Free( sortedSurfaces );

	return num_vbos;
}

/*
* Mod_CreateVertexBufferObjects
*/
void Mod_CreateVertexBufferObjects( model_t *mod ) {
	unsigned int total_vbos = 0;
	size_t total_size = 0;
	mbrushmodel_t *loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	// free all VBO's allocated for previous world map so
	// we won't end up with both maps residing in video memory
	// until R_FreeUnusedVBOs call
	if( r_prevworldmodel && r_prevworldmodel->registrationSequence != rsh.registrationSequence ) {
		R_FreeVBOsByTag( VBO_TAG_WORLD );
	}

	// allocate memory for drawsurfs
	loadbmodel->numDrawSurfaces = 0;
	loadbmodel->drawSurfaces = Mod_Malloc( mod, sizeof( *loadbmodel->drawSurfaces ) * loadbmodel->numsurfaces );

	total_vbos = Mod_CreateSubmodelBufferObjects( mod, &total_size );

	if( total_vbos ) {
		VBO_Printf( "Created %i VBOs, totalling %.1f MiB of memory\n", total_vbos, ( total_size + 1048574 ) / 1048576.0f );
	}
}

/*
* Mod_CreateSkydome
*/
static void Mod_CreateSkydome( model_t *mod ) {
	unsigned int i, j;
	mbrushmodel_t *loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	for( i = 0; i < loadbmodel->numsubmodels; i++ ) {
		mmodel_t *bm = loadbmodel->submodels + i;
		msurface_t *surf = loadbmodel->surfaces + bm->firstModelSurface;

		for( j = 0; j < bm->numModelSurfaces; j++ ) {
			if( !R_SurfNoDraw( surf ) && ( surf->shader->flags & SHADER_SKY ) ) {
				loadbmodel->skydome = R_CreateSkydome( mod );
				return;
			}
			surf++;
		}
	}
}

/*
* Mod_FinalizeBrushModel
*/
static void Mod_FinalizeBrushModel( model_t *model ) {
	Mod_FinishFaces( model );

	Mod_CreateVisLeafs( model );

	Mod_CreateVertexBufferObjects( model );

	Mod_SetupSubmodels( model );

	Mod_CreateSkydome( model );
}

/*
* Mod_TouchBrushModel
*/
static void Mod_TouchBrushModel( model_t *model ) {
	unsigned int i;
	unsigned int modnum;
	mbrushmodel_t *loadbmodel;

	assert( model );

	loadbmodel = ( ( mbrushmodel_t * )model->extradata );
	assert( loadbmodel );

	for( modnum = 0; modnum < loadbmodel->numsubmodels; modnum++ ) {
		loadbmodel->inlines[modnum].registrationSequence = rsh.registrationSequence;
	}

	// touch all shaders and vertex buffer objects for this bmodel

	for( i = 0; i < loadbmodel->numDrawSurfaces; i++ ) {
		drawSurfaceBSP_t *drawSurf = &loadbmodel->drawSurfaces[i];
		R_TouchShader( drawSurf->shader );
		R_TouchMeshVBO( R_GetVBOByIndex( drawSurf->vbo ) );
	}

	for( i = 0; i < loadbmodel->numfogs; i++ ) {
		if( loadbmodel->fogs[i].shader ) {
			R_TouchShader( loadbmodel->fogs[i].shader );
		}
	}

	if( loadbmodel->skydome ) {
		R_TouchSkydome( loadbmodel->skydome );
	}

	for( i = 0; i < loadbmodel->numRtLights; i++ ) {
		R_TouchRtLight( loadbmodel->rtLights + i );
	}

	for( i = 0; i < loadbmodel->numRtSkyLights; i++ ) {
		R_TouchRtLight( loadbmodel->rtSkyLights + i );
	}

	R_TouchLightmapImages( model );
}

//===============================================================================

/*
* Mod_Modellist_f
*/
void Mod_Modellist_f( void ) {
	int i;
	model_t *mod;
	size_t size, total;

	total = 0;
	Com_Printf( "Loaded models:\n" );
	for( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ ) {
		if( !mod->name ) {
			continue;
		}
		size = ri.Mem_PoolTotalSize( mod->mempool );
		Com_Printf( "%8" PRIuPTR " : %s\n", (uintptr_t)size, mod->name );
		total += size;
	}
	Com_Printf( "Total: %i\n", mod_numknown );
	Com_Printf( "Total resident: %" PRIuPTR "\n", (uintptr_t)total );
}

/*
* R_InitModels
*/
void R_InitModels( void ) {
	mod_mempool = R_AllocPool( r_mempool, "Models" );
	memset( mod_novis, 0xff, sizeof( mod_novis ) );
	mod_isworldmodel = false;
	r_prevworldmodel = NULL;
	mod_mapConfigs = R_MallocExt( mod_mempool, sizeof( *mod_mapConfigs ) * MAX_MOD_KNOWN, 0, 1 );
}

/*
* Mod_Free
*/
static void Mod_Free( model_t *model ) {
	R_FreePool( &model->mempool );
	memset( model, 0, sizeof( *model ) );
	model->type = mod_free;
}

/*
* R_FreeUnusedModels
*/
void R_FreeUnusedModels( void ) {
	int i;
	model_t *mod;

	for( i = 0, mod = mod_known; i < mod_numknown; i++, mod++ ) {
		if( !mod->name ) {
			continue;
		}
		if( mod->registrationSequence == rsh.registrationSequence ) {
			// we need this model
			continue;
		}

		Mod_Free( mod );
	}

	// check whether the world model has been freed
	if( rsh.worldModel && rsh.worldModel->type == mod_free ) {
		rsh.worldModel = NULL;
		rsh.worldBrushModel = NULL;
	}
}

/*
* R_ShutdownModels
*/
void R_ShutdownModels( void ) {
	int i;

	if( !mod_mempool ) {
		return;
	}

	for( i = 0; i < mod_numknown; i++ ) {
		if( mod_known[i].name ) {
			Mod_Free( &mod_known[i] );
		}
	}

	rsh.worldModel = NULL;
	rsh.worldBrushModel = NULL;

	mod_numknown = 0;
	memset( mod_known, 0, sizeof( mod_known ) );

	R_FreePool( &mod_mempool );
}

/*
* Mod_StripLODSuffix
*/
void Mod_StripLODSuffix( char *name ) {
	size_t len;

	len = strlen( name );
	if( len <= 2 ) {
		return;
	}
	if( name[len - 2] != '_' ) {
		return;
	}

	if( name[len - 1] >= '0' && name[len - 1] <= '0' + MOD_MAX_LODS ) {
		name[len - 2] = 0;
	}
}

/*
* Mod_FindSlot
*/
static model_t *Mod_FindSlot( const char *name ) {
	int i;
	model_t *mod, *best;

	//
	// search the currently loaded models
	//
	for( i = 0, mod = mod_known, best = NULL; i < mod_numknown; i++, mod++ ) {
		if( mod->type == mod_free ) {
			if( !best ) {
				best = mod;
			}
			continue;
		}
		if( !Q_stricmp( mod->name, name ) ) {
			return mod;
		}
	}

	//
	// return best candidate
	//
	if( best ) {
		return best;
	}

	//
	// find a free model slot spot
	//
	if( mod_numknown == MAX_MOD_KNOWN ) {
		ri.Com_Error( ERR_DROP, "mod_numknown == MAX_MOD_KNOWN" );
	}
	return &mod_known[mod_numknown++];
}

/*
* Mod_Handle
*/
unsigned int Mod_Handle( const model_t *mod ) {
	return mod - mod_known;
}

/*
* Mod_ForHandle
*/
model_t *Mod_ForHandle( unsigned int elem ) {
	return mod_known + elem;
}

/*
* Mod_ForName
*
* Loads in a model for the given name
*/
model_t *Mod_ForName( const char *name, bool crash ) {
	int i;
	model_t *mod, *lod;
	unsigned *buf;
	char shortname[MAX_QPATH], lodname[MAX_QPATH];
	const char *extension;
	const modelFormatDescr_t *descr;
	bspFormatDesc_t *bspFormat = NULL;

	if( !name[0] ) {
		ri.Com_Error( ERR_DROP, "Mod_ForName: NULL name" );
	}

	//
	// inline models are grabbed only from worldmodel
	//
	if( name[0] == '*' ) {
		int modnum = atoi( name + 1 );
		if( modnum < 1 || !rsh.worldBrushModel || (unsigned)modnum >= rsh.worldBrushModel->numsubmodels ) {
			ri.Com_Error( ERR_DROP, "bad inline model number" );
		}
		return &rsh.worldBrushModel->inlines[modnum];
	}

	Q_strncpyz( shortname, name, sizeof( shortname ) );
	COM_StripExtension( shortname );
	extension = &name[strlen( shortname ) + 1];

	mod = Mod_FindSlot( name );
	if( mod->type == mod_bad ) {
		return NULL;
	}
	if( mod->type != mod_free ) {
		return mod;
	}

	//
	// load the file
	//
	modfilelen = R_LoadFile( name, (void **)&buf );
	if( !buf && crash ) {
		ri.Com_Error( ERR_DROP, "Mod_NumForName: %s not found", name );
	}

	// free data we may still have from the previous load attempt for this model slot
	if( mod->mempool ) {
		R_FreePool( &mod->mempool );
	}

	mod->type = mod_bad;
	mod->mempool = R_AllocPool( mod_mempool, name );
	mod->name = Mod_Malloc( mod, strlen( name ) + 1 );
	strcpy( mod->name, name );

	// return the NULL model
	if( !buf ) {
		return NULL;
	}

	// call the apropriate loader
	descr = Q_FindFormatDescriptor( mod_supportedformats, ( const uint8_t * )buf, (const bspFormatDesc_t **)&bspFormat );
	if( !descr ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Mod_NumForName: unknown fileid for %s", mod->name );
		return NULL;
	}

	if( mod_isworldmodel ) {
		// we only init map config when loading the map from disk
		R_InitMapConfig( name );
	} else if( rsh.worldModel != NULL ) {
		if( bspFormat != NULL ) {
			ri.Com_Error( ERR_DROP, "Loaded a brush model after the world" );
		}
	}

	descr->loader( mod, NULL, buf, bspFormat );
	R_FreeFile( buf );

	if( mod->type == mod_bad ) {
		return NULL;
	}

	if( mod_isworldmodel ) {
		// we only init map config when loading the map from disk
		R_FinishMapConfig( mod );
	}

	// do some common things
	if( mod->type == mod_brush ) {
		Mod_FinalizeBrushModel( mod );
		mod->touch = &Mod_TouchBrushModel;
	}

	if( mod_isworldmodel ) {
		R_LoadWorldRtLights( mod );
	
		R_LoadWorldRtSkyLights( mod );

		r_lighting_realtime_sky_color->modified = false;
		r_lighting_realtime_sky_direction->modified = false;
	}

	if( !descr->maxLods ) {
		return mod;
	}

	//
	// load level-of-detail models
	//
	mod->lodnum = 0;
	mod->numlods = 0;
	for( i = 0; i < descr->maxLods; i++ ) {
		Q_snprintfz( lodname, sizeof( lodname ), "%s_%i.%s", shortname, i + 1, extension );
		R_LoadFile( lodname, (void **)&buf );
		if( !buf || strncmp( (const char *)buf, descr->header, descr->headerLen ) ) {
			break;
		}

		lod = mod->lods[i] = Mod_FindSlot( lodname );
		if( lod->name && !strcmp( lod->name, lodname ) ) {
			continue;
		}

		lod->type = mod_bad;
		lod->lodnum = i + 1;
		lod->mempool = R_AllocPool( mod_mempool, lodname );
		lod->name = Mod_Malloc( lod, strlen( lodname ) + 1 );
		strcpy( lod->name, lodname );

		mod_numknown++;

		descr->loader( lod, mod, buf, bspFormat );
		R_FreeFile( buf );

		mod->numlods++;
	}

	return mod;
}

/*
* R_TouchModel
*/
static void R_TouchModel( model_t *mod ) {
	int i;
	model_t *lod;

	if( mod->registrationSequence == rsh.registrationSequence ) {
		return;
	}

	// touching a model precaches all images and possibly other assets
	mod->registrationSequence = rsh.registrationSequence;
	if( mod->touch ) {
		mod->touch( mod );
	}

	// handle Level Of Details
	for( i = 0; i < mod->numlods; i++ ) {
		lod = mod->lods[i];
		lod->registrationSequence = rsh.registrationSequence;
		if( lod->touch ) {
			lod->touch( lod );
		}
	}
}

//=============================================================================

/*
* R_InitMapConfig
*
* Clears map config before loading the map from disk. NOT called when the map
* is reloaded from model cache.
*/
static void R_InitMapConfig( const char *model ) {
	memset( &mapConfig, 0, sizeof( mapConfig ) );

	mapConfig.lightmapsPacking = false;
	mapConfig.lightmapArrays = false;
	mapConfig.maxLightmapSize = 0;
	mapConfig.deluxeMaps = false;
	mapConfig.deluxeMappingEnabled = false;
	mapConfig.forceClear = false;
	mapConfig.averageLightingIntensity = 1;
	mapConfig.writeSkyDepth = false;

	VectorClear( mapConfig.ambient );

	if( r_lighting_packlightmaps->integer ) {
		char lightmapsPath[MAX_QPATH], *p;

		mapConfig.lightmapsPacking = true;

		Q_strncpyz( lightmapsPath, model, sizeof( lightmapsPath ) );
		p = strrchr( lightmapsPath, '.' );
		if( p ) {
			*p = 0;
			Q_strncatz( lightmapsPath, "/lm_0000.tga", sizeof( lightmapsPath ) );
			if( ri.FS_FOpenFile( lightmapsPath, NULL, FS_READ ) != -1 ) {
				ri.Com_DPrintf( S_COLOR_YELLOW "External lightmap stage: lightmaps packing is disabled\n" );
				mapConfig.lightmapsPacking = false;
			}
		}
	}
}

/*
* R_FinishMapConfig
*
* Called after loading the map from disk.
*/
static void R_FinishMapConfig( const model_t *mod ) {
	// ambient lighting
	if( r_fullbright->integer ) {
		VectorSet( mapConfig.ambient, 1, 1, 1 );
		mapConfig.averageLightingIntensity = 1;
	} else {
		ColorNormalize( mapConfig.ambient,  mapConfig.ambient );
	}

	mod_mapConfigs[mod - mod_known] = mapConfig;
}

//=============================================================================

/*
* R_RegisterWorldModel
*
* Specifies the model that will be used as the world
*/
void R_RegisterWorldModel( const char *model ) {
	model_t *newworldmodel;

	r_prevworldmodel = rsh.worldModel;
	mod_isworldmodel = true;
	newworldmodel = Mod_ForName( model, true );
	mod_isworldmodel = false;

	if( newworldmodel && newworldmodel == rsh.worldModel ) {
		R_TouchModel( rsh.worldModel );
		return;
	}

	rsh.worldModel = NULL;
	rsh.worldBrushModel = NULL;
	rsh.worldModelSequence++;

	if( !newworldmodel ) {
		return;
	}

	rsh.worldModel = newworldmodel;
	rsh.worldBrushModel = (mbrushmodel_t *)rsh.worldModel->extradata;

	// FIXME: this is ugly...
	mapConfig = mod_mapConfigs[rsh.worldModel - mod_known];

	R_TouchModel( rsh.worldModel );

	// lazy-compile realtime light shadows
	r_lighting_realtime_world_shadows->modified = true;
}

/*
* R_WaitWorldModel
*/
void R_WaitWorldModel( void ) {
	// load all world images if not yet
	R_FinishLoadingImages();

	// if it's a Quake1 .bsp, load default miptex's for all missing high res images
	Mod_FixupQ1MipTex( rsh.worldModel );
}

/*
* R_RegisterModel
*/
struct model_s *R_RegisterModel( const char *name ) {
	model_t *mod;

	mod = Mod_ForName( name, false );
	if( mod ) {
		R_TouchModel( mod );
	}
	return mod;
}

/*
* R_LoadWorldRtLightsFromMap
*/
static void R_LoadWorldRtLightsFromMap( model_t *model ) {
	char *data;
	char key[MAX_KEY], value[MAX_VALUE], *token;
	char cubemap[MAX_QPATH];
	bool islight, shadow, radiusset;
	int style, flags, noshadow;
	float radius;
	float colorf[3], originf[3], angles[3];
	mbrushmodel_t *bmodel;
	unsigned numLights, maxLights;
	rtlight_t *lights;

	if( !model || !( bmodel = ( mbrushmodel_t * )model->extradata ) ) {
		return;
	}

	numLights = 0;
	maxLights = 128;
	lights = ( rtlight_t * )Mod_Malloc( model, maxLights * sizeof( *lights ) );

	data = bmodel->entityString;
	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; ) {
		islight = false;
		radiusset = false;
		radius = 0;
		style = 0;
		shadow = true;
		noshadow = -1;
		flags = LIGHTFLAG_REALTIMEMODE;
		VectorSet( colorf, 1, 1, 1 );
		VectorClear( angles );
		cubemap[0] = 0;

		while( 1 ) {
			token = COM_Parse( &data );
			if( !token[0] ) {
				break; // error
			}
			if( token[0] == '}' ) {
				break; // end of entity

			}
			Q_strncpyz( key, token, sizeof( key ) );
			Q_trim( key );

			token = COM_Parse( &data );
			if( !token[0] ) {
				break; // error

			}
			Q_strncpyz( value, token, sizeof( value ) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "classname" ) ) {
				if( !strcmp( value, "light") ) {
					islight = true;
				}
			} else if( !strcmp( key, "_color" ) || !strcmp( key, "color" ) ) {
				sscanf( value, "%f %f %f", &colorf[0], &colorf[1], &colorf[2] );
			} else if( !strcmp( key, "origin" ) ) {
				sscanf( value, "%f %f %f", &originf[0], &originf[1], &originf[2] );
			} else if( !strcmp( key, "light" ) || !strcmp( key, "_light" ) ) {
				sscanf( value, "%f", &radius ), radiusset = true;
			} else if( !strcmp( key, "style" ) ) {
				sscanf( value, "%d", &style );
			} else if( !strcmp( key, "_cubemap" ) ) {
				Q_strncpyz( cubemap, value, sizeof( cubemap ) );
			} else if( !strcmp( key, "angles" ) ) {
				sscanf( value, "%f %f %f", &angles[0], &angles[1], &angles[2] );
			} else if( !strcmp( key, "_noshadow" ) ) {
				noshadow = 0;
				sscanf( value, "%d", &noshadow );
			} else if( !strcmp( key, "_rtflags" ) ) {
				sscanf( value, "%d", &flags );
			}
		}

		if( islight ) {
			rtlight_t *l;
			mat3_t axis;

			if( numLights == maxLights ) {
				maxLights = maxLights + 128;
				lights = Mod_Realloc( lights, maxLights * sizeof( *lights ) );
			}

			if( !radiusset )
				radius = MAPLIGHT_DEFAULT_RADIUS;
			else if( radius < 0.01 )
				continue;

			if( style >= MAX_LIGHTSTYLES )
				style = 0;

			if( noshadow >= 0 ) {
				shadow = noshadow == 0;
			} else {
				shadow = radius >= MAPLIGHT_MIN_SHADOW_RADIUS;
			}

			AnglesToAxis( angles, axis );

			l = &lights[numLights++];
			R_InitRtLight( l, originf, axis, radius, colorf );

			l->flags = flags;
			l->shadow = shadow;
			l->style = style;
			l->world = true;
			l->worldModel = model;

			if( cubemap[0] != '\0' ) {
				l->cubemapFilter = R_FindImage( cubemap, NULL, IT_SRGB | IT_CLAMP | IT_CUBEMAP, 1, IMAGE_TAG_WORLD );
			}

			R_GetRtLightVisInfo( bmodel, l );
		}
	}

	bmodel->numRtLights = numLights;
	if( numLights ) {
		bmodel->rtLights = Mod_Malloc( model, numLights * sizeof( rtlight_t ) );
		memcpy( bmodel->rtLights, lights, numLights * sizeof( rtlight_t ) );
	}

	R_Free( lights );
}

/*
* R_LoadWorldRtLights
*/
static void R_LoadWorldRtLights( model_t *model ) {
	mbrushmodel_t *bmodel;
	char shortname[MAX_QPATH];
	char *buf;
	int n;
	char tempchar, *s, *t;
	char cubemap[MAX_QPATH];
	char format[128];
	unsigned numLights, maxLights;
	rtlight_t *lights;

	if( r_lighting_realtime_world_importfrommap->integer == 2 ) {
		R_LoadWorldRtLightsFromMap( model );
		return;
	}

	if( !model || !( bmodel = ( mbrushmodel_t * )model->extradata ) ) {
		return;
	}

	bmodel->numRtLights = 0;

	Q_strncpyz( shortname, model->name, sizeof( shortname ) );
	COM_ReplaceExtension( shortname, ".rtlights", sizeof( shortname ) );

	R_LoadFile( shortname, ( void ** )&buf );
	if( !buf ) {
		if( r_lighting_realtime_world_importfrommap->integer ) {
			R_LoadWorldRtLightsFromMap( model );
		}
		return;
	}

	Q_snprintfz( format, sizeof( format ), "%%f %%f %%f %%f %%f %%f %%f %%d %%%zus %%f %%f %%f %%f %%f %%f %%f %%f %%i", sizeof( cubemap ) );

	numLights = 0;
	maxLights = 128;
	lights = ( rtlight_t * )Mod_Malloc( model, maxLights * sizeof( *lights ) );

	s = buf;
	n = 0;
	while( *s ) {
		int a;
		bool shadow;
		int style, flags;
		float origin[3], radius, color[3], angles[3], corona, coronasizescale, ambientscale, diffusescale, specularscale;
		mat3_t axis;
		rtlight_t *l;

		t = s;
		while( *s && *s != '\n' && *s != '\r' ) {
			s++;
		}
		if( !*s )
			break;

		tempchar = *s;
		shadow = true;

		// check for modifier flags
		if( *t == '!' ) {
			shadow = false;
			t++;
		}

		cubemap[0] = '\0';

		*s = 0;
		a = sscanf( t, format, &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemap, 
			&corona, &angles[0], &angles[1], &angles[2], &coronasizescale, &ambientscale, &diffusescale, &specularscale, &flags );
		*s = tempchar;

		if( a < 8 ) {
			Com_Printf( S_COLOR_YELLOW "Found %d parameters on line %i, should be 8 or more parameters "
				"(origin[0] origin[1] origin[2] radius color[0] color[1] color[2] style \"cubemapname\" corona "
				"angles[0] angles[1] angles[2] coronasizescale ambientscale diffusescale specularscale flags)\n", 
				a, n + 1);
			break;
		}

		if( a < 18 )
			flags = LIGHTFLAG_REALTIMEMODE;
		if( a < 17 )
			specularscale = 1;
		if( a < 16 )
			diffusescale = 1;
		if( a < 15 )
			ambientscale = 0;
		if( a < 14 )
			coronasizescale = 0.25f;
		if( a < 13 )
			VectorClear( angles );
		if( a < 10 )
			corona = 0;
		AnglesToAxis( angles, axis );

		if( numLights == maxLights ) {
			maxLights = maxLights + 64;
			lights = Mod_Realloc( lights, maxLights * sizeof( *lights ) );
		}

		l = &lights[numLights++];
		R_InitRtLight( l, origin, axis, radius, color );
		l->flags = flags;
		l->style = style;
		l->shadow = shadow;
		l->world = true;
		l->worldModel = model;

		if( cubemap[0] ) {
			// strip quotes
			if( cubemap[0] == '"' && cubemap[strlen(cubemap) - 1] == '"' ) {
				size_t namelen;
				namelen = strlen( cubemap ) - 2;
				memmove( cubemap, cubemap + 1, namelen );
				cubemap[namelen] = '\0';
			}
			l->cubemapFilter = R_FindImage( cubemap, NULL, IT_SRGB | IT_CLAMP | IT_CUBEMAP, 1, IMAGE_TAG_WORLD );
		}

		R_GetRtLightVisInfo( bmodel, l );

		if( *s == '\r' )
			s++;
		if( *s == '\n' )
			s++;
		n++;
	}

	bmodel->numRtLights = numLights;
	if( numLights ) {
		bmodel->rtLights = Mod_Malloc( model, numLights * sizeof( rtlight_t ) );
		memcpy( bmodel->rtLights, lights, numLights * sizeof( rtlight_t ) );
	}

	R_Free( buf );
	R_Free( lights );
}

/*
* R_SetWorldRtSkyLightsColors
*/
void R_SetWorldRtSkyLightsColors( model_t *model ) {
	unsigned i;
	vec3_t color = { 0.7, 0.7, 0.7 };
	const char *cvarcolor = r_lighting_realtime_sky_color->string;
	mbrushmodel_t *bmodel;

	if( !model || model->type != mod_brush || !( bmodel = ( mbrushmodel_t * )model->extradata ) ) {
		return;
	}

	// cvar override
	if( cvarcolor[0] != '\0' ) {
		float c[3];
		if( sscanf( cvarcolor, "%f %f %f", &c[0], &c[1], &c[2] ) == 3 ) {
			ColorNormalize( c, color );
		}
	}

	for( i = 0; i < bmodel->numRtSkyLights; i++ ) {
		rtlight_t *l = bmodel->rtSkyLights + i;
		const vec_t *shaderColor = l->skycolor;

		if( shaderColor[0] != 0.0 || shaderColor[1] != 0.0 || shaderColor[2] != 0.0 ) {
			R_SetRtLightColor( l, shaderColor );
		} else {
			R_SetRtLightColor( l, color );
		}
	}
}

#define MAX_BRUSHSKIES 1000

typedef struct {
	int area;
	float radius;
	vec3_t dir;
	vec3_t mins, maxs;
	vec3_t skymins, skymaxs;
	vec3_t frustumCorners[8];
	shader_t *shader;
} mbrushsky_t;

/*
* R_LoadWorldRtSkyLights
*/
static void R_LoadWorldRtSkyLights( model_t *model ) {
	unsigned i, j, k;
	mbrushmodel_t *bmodel;
	unsigned numskies;
	static mbrushsky_t skies[MAX_BRUSHSKIES];

	if( !model || model->type != mod_brush || !( bmodel = ( mbrushmodel_t * )model->extradata ) ) {
		return;
	}

	numskies = 0;

	for( i = 0; i < bmodel->numRtSkyLights; i++ ) {
		R_UncompileRtLight( bmodel->rtSkyLights + i );
	}

	R_Free( bmodel->rtSkyLights );
	bmodel->rtSkyLights = NULL;
	bmodel->numRtSkyLights = 0;

	for( i = 0; i < bmodel->numleafs; i++ ) {
		mleaf_t *leaf = bmodel->leafs + i;
		uint8_t *pvs = Mod_ClusterPVS( leaf->cluster, bmodel );
		unsigned numCasters, numReceivers;
		mbrushsky_t *sky = &skies[numskies];

		ClearBounds( sky->mins, sky->maxs );
		ClearBounds( sky->skymins, sky->skymaxs );
		sky->area = leaf->area;

		numCasters = 0;
		for( j = 0; j < leaf->numVisSurfaces; j++ ) {
			msurface_t *surf = bmodel->surfaces + leaf->visSurfaces[j];

			if( surf->flags & SURF_SKY ) {
				if( numCasters != 0 ) {
					if( sky->shader != surf->shader ) {
						continue;
					}
					UnionBounds( sky->skymins, sky->skymaxs, surf->mins, surf->maxs );
					numCasters++;
					continue;
				}

				CopyBounds( surf->mins, surf->maxs, sky->skymins, sky->skymaxs );
				sky->shader = surf->shader;
				numCasters++;
			}
		}

		if( numCasters == 0 ) {
			continue;
		}

		numReceivers = 0;

		for( j = 0; j < bmodel->numleafs; j++ ) {
			mleaf_t *leaf2 = bmodel->leafs + j;

			if( leaf2->cluster < 0 ) {
				continue;
			}
			if( leaf2->area != leaf->area ) {
				continue;
			}
			if( !( pvs[leaf2->cluster >> 3] & ( 1 << ( leaf2->cluster & 7 ) ) ) ) {
				continue;
			}

			for( k = 0; k < leaf2->numVisSurfaces; k++ ) {
				msurface_t *surf = bmodel->surfaces + leaf2->visSurfaces[k];

				UnionBounds( sky->mins, sky->maxs, surf->mins, surf->maxs );

				if( !(surf->flags & SURF_SKY) ) {
					numReceivers++;
				}
			}
		}

		if( numReceivers == 0 ) {
			continue;
		}

		numskies++;
		if( numskies == MAX_BRUSHSKIES ) {
			break;
		}
	}

	for( i = 0; i < numskies; i++ ) {
		vec_t farclip;
		vec3_t dir = { 0, 0, -1 };
		vec3_t cmins, cmaxs, v;
		mbrushsky_t *sky = &skies[i];
		const char *cvardir = r_lighting_realtime_sky_direction->string;
		const vec_t *shaderDir = sky->shader->skyParms.lightDir;

		if( shaderDir[0] != 0.0 || shaderDir[1] != 0.0 || shaderDir[2] != 0.0 ) {
			VectorCopy( shaderDir, dir );
		} else {
			if( cvardir[0] != '\0' ) {
				float cdir[3];
				if( sscanf( cvardir, "%f %f %f", &cdir[0], &cdir[1], &cdir[2] ) == 3 ) {
					VectorCopy( cdir, dir );
				}
			}
		}

		VectorNormalize( dir );
		VectorCopy( dir, sky->dir );
		
		// expand the volume a bit so that nearby volumes can be merged together due to intersection
		for( j = 0; j < 3; j++ ) {
			sky->skymins[j] -= 32;
			sky->skymaxs[j] += 32;
		}
		for( j = 0; j < 3; j++ ) {
			sky->mins[j] -= 32;
			sky->maxs[j] += 32;
		}

		// extend the skyvolume along light direction
		CopyBounds( sky->skymins, sky->skymaxs, cmins, cmaxs );

		farclip = 2.0 * LocalBounds( sky->mins, sky->maxs, NULL, NULL, NULL );

		VectorMA( sky->skymins, farclip, dir, v );
		AddPointToBounds( v, cmins, cmaxs );
		VectorMA( sky->skymaxs, farclip, dir, v );
		AddPointToBounds( v, cmins, cmaxs );

		CopyBounds( cmins, cmaxs, sky->mins, sky->maxs );
		sky->radius = LocalBounds( cmins, cmaxs, NULL, NULL, NULL );
	}

	// merge intersecting sky volumes
	while( true ) {
		int merged = 0;

		for( i = 0; i < numskies; i++ ) {
			mbrushsky_t *sky = &skies[i];

			for( j = 0; j < numskies; j++ ) {
				mbrushsky_t *sky2 = &skies[j];

				if( i == j ) {
					continue;
				}

				if( sky->area != sky2->area ) {
					continue;
				}
				if( sky->shader != sky2->shader )  {
					continue;
				}

				if( BoundsOverlap( sky->skymins, sky->skymaxs, sky2->skymins, sky2->skymaxs ) ) {
					// merge j into i
					UnionBounds( sky->mins, sky->maxs, sky2->mins, sky2->maxs );
					UnionBounds( sky->skymins, sky->skymaxs, sky2->skymins, sky2->skymaxs );

					// remove j
					memmove( sky2, sky2 + 1, sizeof( *sky2 ) * (numskies-j-1) );

					if( i > j ) i--;
					j--;
					numskies--;
					merged++;
					break;
				}
			}
		}

		if( !merged ) {
			break;
		}
	}

	for( i = 0; i < numskies; i++ ) {
		int a;
		vec_t l = 0;
		vec3_t dir;
		vec3_t corners[8];
		mbrushsky_t *sky = &skies[i];

		VectorCopy( sky->dir, dir );

		for( a = 0; a < 3; a++ ) {
			if( fabs( dir[a] ) == 1.0f ) {
				break;
			}
		}

		if( a == 3 ) {
			// find the shortest magnitude axially aligned vector
			l = 1000000;
			for( j = 0; j < 3; j++ ) {
				if( sky->skymaxs[j] - sky->skymins[j] < l ) {
					a = j;
					l = sky->skymaxs[j] - sky->skymins[j];
				}
			}
		}

		// compute 8 frustum corners
		for( j = 0; j < 4; j++ ) {
			corners[j][a] = dir[a] < 0 ? sky->skymaxs[a] : sky->skymins[a];
			corners[j][(a+1)%3] = j & 1 ? sky->skymins[(a+1)%3] : sky->skymaxs[(a+1)%3];
			corners[j][(a+2)%3] = j & 2 ? sky->skymins[(a+2)%3] : sky->skymaxs[(a+2)%3];
		}

		for( j = 0; j < 4; j++ ) {
			VectorCopy( corners[j], sky->frustumCorners[j] );
			VectorMA( corners[j], 1.0, dir, sky->frustumCorners[j+4] );
		}

		R_ProjectFarFrustumCornersOnBounds( sky->frustumCorners, sky->mins, sky->maxs );
	}

	bmodel->rtSkyLights = Mod_Malloc( model, numskies * sizeof( rtlight_t ) );
	bmodel->numRtSkyLights = numskies;

	for( i = 0; i < numskies; i++ ) {
		rtlight_t *l = &bmodel->rtSkyLights[i];
		mbrushsky_t *sky = &skies[i];
		const vec_t *shaderColor = sky->shader->skyParms.lightColor;

		R_InitRtDirectionalLight( l, sky->frustumCorners, shaderColor );

		l->flags = LIGHTFLAG_REALTIMEMODE;
		l->style = 0;
		l->shadow = true;
		l->world = true;
		l->worldModel = model;
		l->sky = true;
		l->cascaded = true;
		VectorCopy( shaderColor, l->skycolor );

		R_GetRtLightVisInfo( bmodel, l );

		CopyBounds( sky->skymins, sky->skymaxs, l->skymins, l->skymaxs );

		if( l->numReceiveSurfaces == 0 ) {
			l->radius = 0;
			l->cluster = CLUSTER_INVALID;
			l->area = -1;
			continue;
		}

		l->cluster = CLUSTER_INVALID;
		l->area = sky->area;
	}

	R_SetWorldRtSkyLightsColors( model );
}

/*
* R_UpdateWorldRtSkyLights
*/
bool R_UpdateWorldRtSkyLights( model_t *model ) {
	bool updated = false;

	if( r_lighting_realtime_sky_direction->modified ) {
		R_LoadWorldRtSkyLights( model );
		r_lighting_realtime_sky_direction->modified = false;
		updated = true;
	}

	if( r_lighting_realtime_sky_color->modified ) {
		R_SetWorldRtSkyLightsColors( model );
		r_lighting_realtime_sky_color->modified = false;
	}

	return updated;
}

/*
* R_ModelBounds
*/
void R_ModelBounds( const model_t *model, vec3_t mins, vec3_t maxs ) {
	if( model ) {
		VectorCopy( model->mins, mins );
		VectorCopy( model->maxs, maxs );
	} else if( rsh.worldModel ) {
		VectorCopy( rsh.worldModel->mins, mins );
		VectorCopy( rsh.worldModel->maxs, maxs );
	}
}

/*
* R_ModelFrameBounds
*/
void R_ModelFrameBounds( const struct model_s *model, int frame, vec3_t mins, vec3_t maxs ) {
	if( model ) {
		switch( model->type ) {
			case mod_alias:
				R_AliasModelFrameBounds( model, frame, mins, maxs );
				break;
			case mod_skeletal:
				R_SkeletalModelFrameBounds( model, frame, mins, maxs );
				break;
			default:
				break;
		}
	}
}

static vec4_t *r_modelTransformBuf;
static size_t r_modelTransformBufSize;

/*
* R_GetTransformBufferForMesh
*/
void R_GetTransformBufferForMesh( mesh_t *mesh, bool positions, bool normals, bool sVectors ) {
	size_t bufSize = 0;
	int numVerts = mesh->numVerts;
	vec4_t *bufPtr;

	assert( numVerts );

	if( !numVerts || ( !positions && !normals && !sVectors ) ) {
		return;
	}

	if( positions ) {
		bufSize += numVerts;
	}
	if( normals ) {
		bufSize += numVerts;
	}
	if( sVectors ) {
		bufSize += numVerts;
	}
	bufSize *= sizeof( vec4_t );
	if( bufSize > r_modelTransformBufSize ) {
		r_modelTransformBufSize = bufSize;
		if( r_modelTransformBuf ) {
			R_Free( r_modelTransformBuf );
		}
		r_modelTransformBuf = R_Malloc( bufSize );
	}

	bufPtr = r_modelTransformBuf;
	if( positions ) {
		mesh->xyzArray = bufPtr;
		bufPtr += numVerts;
	}
	if( normals ) {
		mesh->normalsArray = bufPtr;
		bufPtr += numVerts;
	}
	if( sVectors ) {
		mesh->sVectorsArray = bufPtr;
	}
}
