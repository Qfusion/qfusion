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

static void R_InitMapConfig( const char *model );
static void R_FinishMapConfig( const model_t *mod );

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

	return 0;
}

/*
* R_SortSurfacesCmp
*/
static int R_SortSurfacesCmp( const void *ps1, const void *ps2 ) {
	const msurface_t *s1 = *( const msurface_t ** ) ps1;
	const msurface_t *s2 = *( const msurface_t ** ) ps2;
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

	// merge faces if they have the same shader

	floatVattribs = VATTRIB_POSITION_BIT|VATTRIB_SURFINDEX_BIT;

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

			// allocate a drawsurf
			drawSurf = &loadbmodel->drawSurfaces[loadbmodel->numDrawSurfaces++];
			drawSurf->type = ST_BSP;
			drawSurf->instances = surf->instances;
			drawSurf->numInstances = surf->numInstances;
			drawSurf->shader = surf->shader;
			drawSurf->surfFlags = surf->flags;

			drawSurf->numWorldSurfaces = 1;
			drawSurf->worldSurfaces = worldSurfaces;
			drawSurf->worldSurfaces[0] = surf - loadbmodel->surfaces;

			// upload vertex and elements data for face itself
			surf->drawSurf = loadbmodel->numDrawSurfaces;
			surf->firstDrawSurfVert = 0;
			surf->firstDrawSurfElem = 0;

			// foliage surfaces can not be batched
			mergable = surf->numInstances == 0;

			fcount = 1;
			vcount = surf->mesh.numVerts;
			ecount = surf->mesh.numElems;
			CopyBounds( surf->mins, surf->maxs, mins, maxs );

			if( mergable ) {
				vec3_t testmins, testmaxs;

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
					tempVBOs = ( mesh_vbo_t * ) Mod_Realloc( tempVBOs, maxTempVBOs * sizeof( *tempVBOs ) );
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
		vbo = ( mesh_vbo_t * ) tempVBOs[drawSurf->vbo].owner;
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
	loadbmodel->drawSurfaces = ( drawSurfaceBSP_t * ) Mod_Malloc( mod, sizeof( *loadbmodel->drawSurfaces ) * loadbmodel->numsurfaces );

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

	if( loadbmodel->skydome ) {
		R_TouchSkydome( loadbmodel->skydome );
	}
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
		size = Mem_PoolTotalSize( mod->mempool );
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
	mod_mapConfigs = ( mapconfig_t * ) R_MallocExt( mod_mempool, sizeof( *mod_mapConfigs ) * MAX_MOD_KNOWN, 0, 1 );
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
		if( modnum < 1 || !rsh.worldModel || (unsigned)modnum >= rsh.worldBrushModel->numsubmodels ) {
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
	mod->name = ( char * ) Mod_Malloc( mod, strlen( name ) + 1 );
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
		lod->name = ( char * ) Mod_Malloc( lod, strlen( lodname ) + 1 );
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
}

/*
* R_FinishMapConfig
*
* Called after loading the map from disk.
*/
static void R_FinishMapConfig( const model_t *mod ) {
	// ambient lighting
	ColorNormalize( mapConfig.ambient,  mapConfig.ambient );

	mod_mapConfigs[mod - mod_known] = mapConfig;
}

//=============================================================================

/*
* R_RegisterWorldModel
*
* Specifies the model that will be used as the world
*/
void R_RegisterWorldModel( const char *model ) {
	r_prevworldmodel = rsh.worldModel;
	rsh.worldModel = NULL;
	rsh.worldBrushModel = NULL;
	rsh.worldModelSequence++;

	mod_isworldmodel = true;

	rsh.worldModel = Mod_ForName( model, true );

	mod_isworldmodel = false;

	if( !rsh.worldModel ) {
		return;
	}

	// FIXME: this is ugly...
	mapConfig = mod_mapConfigs[rsh.worldModel - mod_known];

	R_TouchModel( rsh.worldModel );
	rsh.worldBrushModel = ( mbrushmodel_t * )rsh.worldModel->extradata;
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
		r_modelTransformBuf = ( vec4_t * ) R_Malloc( bufSize );
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
