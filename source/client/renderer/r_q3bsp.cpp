/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2008 Victor Luchits

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

// r_q3bsp.c -- Q3 BSP model loading

#include "r_local.h"

typedef struct {
	vec3_t mins, maxs;
	int flatness[2];
} mpatchgroup_t;

static model_t *loadmodel;
static int loadmodel_numverts;
static vec3_t *loadmodel_xyz_array;
static vec3_t *loadmodel_normals_array;
static vec2_t *loadmodel_st_array;
static byte_vec4_t *loadmodel_colors_array;

static int loadmodel_numsurfelems;
static elem_t *loadmodel_surfelems;

static int loadmodel_numshaderrefs;
static mshaderref_t *loadmodel_shaderrefs;

static int loadmodel_numsurfaces;
static rdface_t *loadmodel_dsurfaces;

static int loadmodel_numpatchgroups;
static int loadmodel_maxpatchgroups;
static mpatchgroup_t *loadmodel_patchgroups;
static int *loadmodel_patchgrouprefs;

// current model format descriptor
static const bspFormatDesc_t *mod_bspFormat;

/*
===============================================================================

BRUSHMODEL LOADING

===============================================================================
*/

static uint8_t *mod_base;
static mbrushmodel_t *loadbmodel;

/*
* Mod_FaceToRavenFace
*/
static void Mod_FaceToRavenFace( const dface_t *in, rdface_t *rdf ) {
	int j;

	rdf->facetype = in->facetype;
	rdf->vertexStyles[0] = 0;

	for( j = 0; j < 3; j++ ) {
		rdf->origin[j] = in->origin[j];
		rdf->normal[j] = in->normal[j];
		rdf->mins[j] = in->mins[j];
		rdf->maxs[j] = in->maxs[j];
	}

	rdf->shadernum = in->shadernum;
	rdf->numverts = in->numverts;
	rdf->firstvert = in->firstvert;
	rdf->patch_cp[0] = in->patch_cp[0];
	rdf->patch_cp[1] = in->patch_cp[1];
	rdf->firstelem = in->firstelem;
	rdf->numelems = in->numelems;
}

/*
* Mod_PreloadFaces
*/
static void Mod_PreloadFaces( const lump_t *l ) {
	int i;
	rdface_t *in;

	if( mod_bspFormat->flags & BSP_RAVEN ) {
		in = ( rdface_t * )( mod_base + l->fileofs );
		if( l->filelen % sizeof( *in ) ) {
			ri.Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
		}

		loadmodel_numsurfaces = l->filelen / sizeof( *in );
		loadmodel_dsurfaces = in;
	} else {
		dface_t *din = ( dface_t * )( mod_base + l->fileofs );
		if( l->filelen % sizeof( *din ) ) {
			ri.Com_Error( ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name );
		}

		loadmodel_numsurfaces = l->filelen / sizeof( *din );
		loadmodel_dsurfaces = in = ( rdface_t * ) Mod_Malloc( loadmodel, loadmodel_numsurfaces * sizeof( *in ) );

		// convert from q3a format to rtcw/qfusion format
		for( i = 0; i < loadmodel_numsurfaces; i++, din++, in++ ) {
			Mod_FaceToRavenFace( din, in );
		}
	}

	// preload shaders (images will start loading in background threads while we're still busy with the map)
	in = loadmodel_dsurfaces;
	for( i = 0; i < loadmodel_numsurfaces; i++, in++ ) {
		// load shader
		int shaderNum;
		mshaderref_t *shaderRef;
		shaderType_e shaderType;

		shaderNum = LittleLong( in->shadernum );
		if( shaderNum < 0 || shaderNum >= loadmodel_numshaderrefs ) {
			ri.Com_Error( ERR_DROP, "MOD_LoadBmodel: bad shader number" );
		}
		shaderRef = loadmodel_shaderrefs + shaderNum;
		if( !shaderRef->name[0] ) {
			continue;
		}

		shaderType = SHADER_TYPE_VERTEX;

		if( !shaderRef->shaders[shaderType - SHADER_TYPE_BSP_MIN] ) {
			shaderRef->shaders[shaderType - SHADER_TYPE_BSP_MIN] = R_RegisterShader( shaderRef->name, shaderType );
		}
	}
}

/*
* Mod_LoadFaces
*/
static void Mod_LoadFaces( const lump_t *l ) {
	int i;
	int count;
	const rdface_t *in;
	msurface_t *out;

	in = loadmodel_dsurfaces;
	count = loadmodel_numsurfaces;
	out = ( msurface_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		mshaderref_t *shaderRef;
		shaderType_e shaderType;

		out->facetype = LittleLong( in->facetype );

		// load shader
		shaderRef = loadmodel_shaderrefs + LittleLong( in->shadernum );
		shaderType = SHADER_TYPE_VERTEX;

		out->shader = shaderRef->shaders[shaderType - SHADER_TYPE_BSP_MIN];
		out->flags = shaderRef->flags;
	}
}

/*
* Mod_LoadVertexes
*/
static void Mod_LoadVertexes( const lump_t *l ) {
	int i, count, j;
	dvertex_t *in;
	float *out_xyz, *out_normals, *out_st;
	uint8_t *buffer, *out_colors;
	size_t bufSize;

	in = ( dvertex_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );

	bufSize = count * ( sizeof( vec3_t ) + sizeof( vec3_t ) + sizeof( vec2_t ) * 2 + sizeof( byte_vec4_t ) );
	buffer = ( uint8_t * ) Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec3_t * )buffer; buffer += count * sizeof( vec3_t );
	loadmodel_normals_array = ( vec3_t * )buffer; buffer += count * sizeof( vec3_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count * sizeof( vec2_t );
	loadmodel_colors_array = ( byte_vec4_t * )buffer; buffer += count * sizeof( byte_vec4_t );

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	out_colors = loadmodel_colors_array[0];

	for( i = 0; i < count; i++, in++, out_xyz += 3, out_normals += 3, out_st += 2, out_colors += 4 ) {
		for( j = 0; j < 3; j++ ) {
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}

		for( j = 0; j < 2; j++ ) {
			out_st[j] = LittleFloat( in->tex_st[j] );
		}

		for( j = 0; j < 4; j++ ) {
			out_colors[j] = in->color[j];
		}
	}
}

/*
* Mod_LoadVertexes_RBSP
*/
static void Mod_LoadVertexes_RBSP( const lump_t *l ) {
	int i, count, j;
	rdvertex_t *in;
	float *out_xyz;
	float *out_normals;
	float *out_st;
	uint8_t *buffer;
	uint8_t *out_colors;
	size_t bufSize;

	in = ( rdvertex_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );

	bufSize = count * ( sizeof( vec3_t ) + sizeof( vec3_t ) + sizeof( vec2_t ) + sizeof( vec2_t ) + sizeof( byte_vec4_t ) );
	buffer = ( uint8_t * ) Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec3_t * )buffer; buffer += count * sizeof( vec3_t );
	loadmodel_normals_array = ( vec3_t * )buffer; buffer += count * sizeof( vec3_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count * sizeof( vec2_t );
	loadmodel_colors_array = ( byte_vec4_t * )buffer; buffer += count * sizeof( byte_vec4_t );

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	out_colors = loadmodel_colors_array[0];

	for( i = 0; i < count; i++, in++, out_xyz += 3, out_normals += 3, out_st += 2 ) {
		for( j = 0; j < 3; j++ ) {
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}

		for( j = 0; j < 2; j++ ) {
			out_st[j] = LittleFloat( in->tex_st[j] );
		}

		for( j = 0; j < 4; j++ ) {
			out_colors[j] = in->color[j][0];
		}
	}
}

/*
* Mod_LoadSubmodels
*/
static void Mod_LoadSubmodels( const lump_t *l ) {
	int i, j, count;
	dmodel_t *in;
	mmodel_t *out;
	mbrushmodel_t *bmodel;
	model_t *mod_inline;

	in = ( dmodel_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( mmodel_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	mod_inline = ( model_t * ) Mod_Malloc( loadmodel, count * ( sizeof( *mod_inline ) + sizeof( *bmodel ) ) );
	loadmodel->extradata = bmodel = ( mbrushmodel_t * )( ( uint8_t * )mod_inline + count * sizeof( *mod_inline ) );

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;
	loadbmodel->inlines = mod_inline;

	for( i = 0; i < count; i++, in++, out++ ) {
		vec3_t origin, mins, maxs;

		mod_inline[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
			origin[j] = (out->mins[j] + out->maxs[j]) * 0.5f;
		}

		// the bounds are from world to local coordinates
		// otherwise bmodel radius isn't going make any sense
		for( j = 0; j < 3; j++ ) {
			mins[j] = out->mins[j] - origin[j];
			maxs[j] = out->maxs[j] - origin[j];
		}

		out->radius = RadiusFromBounds( mins, maxs );
		out->firstModelSurface = LittleLong( in->firstface );
		out->numModelSurfaces = LittleLong( in->numfaces );
	}
}

/*
* Mod_LoadShaderrefs
*/
static void Mod_LoadShaderrefs( const lump_t *l ) {
	int i, count;
	dshaderref_t *in;
	mshaderref_t *out;
	bool newMap;

	in = ( dshaderref_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadShaderrefs: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( mshaderref_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_shaderrefs = out;
	loadmodel_numshaderrefs = count;

	// see if the map is new and we need to free shaders from the previous one
	newMap = r_prevworldmodel && ( r_prevworldmodel->registrationSequence != rsh.registrationSequence );

	for( i = 0; i < count; i++, in++ ) {
		Q_strncpyz( out[i].name, in->name, sizeof( out[i].name ) );
		out[i].flags = LittleLong( in->flags );

		if( newMap ) {
			R_TouchShadersByName( out[i].name );
		}
	}

	// free world textures from the previous map that are not used on the new map
	if( newMap ) {
		const shaderType_e shaderTypes[] = { SHADER_TYPE_VERTEX };
		R_FreeUnusedShadersByType( shaderTypes, ARRAY_COUNT( shaderTypes ) );
		R_FreeUnusedImagesByTags( IMAGE_TAG_WORLD );
	}
}

/*
* Mod_AddUpdatePatchGroup
*/
static int Mod_AddUpdatePatchGroup( const rdface_t *in ) {
	int i;
	int patch_cp[2], flatness[2];
	float subdivLevel;
	vec3_t lodMins, lodMaxs;
	int inFirstVert;
	mpatchgroup_t *group;

	patch_cp[0] = LittleLong( in->patch_cp[0] );
	patch_cp[1] = LittleLong( in->patch_cp[1] );
	if( !patch_cp[0] || !patch_cp[1] ) {
		return -1;
	}

	// load LOD group bounds
	for( i = 0; i < 3; i++ ) {
		lodMins[i] = in->mins[i];
		lodMaxs[i] = in->maxs[i];
	}

	subdivLevel = bound( SUBDIVISIONS_MIN, r_subdivisions->value, SUBDIVISIONS_MAX );
	inFirstVert = LittleLong( in->firstvert );

	// find the degree of subdivision in the u and v directions
	Patch_GetFlatness( subdivLevel, (vec_t *)loadmodel_xyz_array[inFirstVert], 3, patch_cp, flatness );

	// track LOD bounds, which hold group of all curves that must subdivide the same to avoid cracking
	for( i = 0, group = loadmodel_patchgroups; i < loadmodel_numpatchgroups; i++, group++ ) {
		if( VectorCompare( group->mins, lodMins ) && VectorCompare( group->maxs, lodMaxs ) ) {
			break;
		}
	}

	// new group
	if( i == loadmodel_numpatchgroups ) {
		if( i == loadmodel_maxpatchgroups ) {
			assert( 0 );
			Com_Printf( S_COLOR_YELLOW "Mod_AddUpdatePatchGroup: i == loadmodel_maxpatchgroups\n" );
			return -1;
		}

		VectorCopy( lodMins, group->mins );
		VectorCopy( lodMaxs, group->maxs );
		group->flatness[0] = flatness[0];
		group->flatness[1] = flatness[1];

		loadmodel_numpatchgroups++;
	} else {
		group->flatness[0] = max( group->flatness[0], flatness[0] );
		group->flatness[1] = max( group->flatness[1], flatness[1] );
	}

	return i;
}

/*
* Mod_CreateMeshForSurface
*/
void Mod_CreateMeshForSurface( const rdface_t *in, msurface_t *out, int patchGroupRef ) {
	mesh_t *mesh = NULL;
	uint8_t *buffer;
	size_t bufSize, bufPos = 0;

	memset( &out->mesh, 0, sizeof( mesh_t ) );

	switch( out->facetype ) {
		case FACETYPE_PATCH:
		{
			int i, u, v, p;
			int patch_cp[2], step[2], size[2], flat[2];
			int numVerts, numElems;
			int inFirstVert;
			int numattribs = 0;
			uint8_t *attribs[2];
			int attribsizes[2];
			elem_t *elems;

			if( patchGroupRef < 0 ) {
				// not a patch at all
				break;
			}

			patch_cp[0] = LittleLong( in->patch_cp[0] );
			patch_cp[1] = LittleLong( in->patch_cp[1] );

			flat[0] = loadmodel_patchgroups[patchGroupRef].flatness[0];
			flat[1] = loadmodel_patchgroups[patchGroupRef].flatness[1];

			inFirstVert = LittleLong( in->firstvert );

			// allocate space for mesh
			step[0] = ( 1 << flat[0] );
			step[1] = ( 1 << flat[1] );
			size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
			size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
			numVerts = size[0] * size[1];
			numElems = ( size[0] - 1 ) * ( size[1] - 1 ) * 6;

			bufSize = numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + sizeof( byte_vec4_t ) );
			bufSize = ALIGN( bufSize, sizeof( elem_t ) ) + numElems * sizeof( elem_t );
			buffer = ( uint8_t * )Mod_Malloc( loadmodel, bufSize );
			bufPos = 0;

			mesh = &out->mesh;
			mesh->numVerts = numVerts;
			mesh->numElems = numElems;

			mesh->xyzArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->sVectorsArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec2_t );
			mesh->colorsArray = ( byte_vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( byte_vec4_t );

			Patch_Evaluate( vec_t, 3, loadmodel_xyz_array[inFirstVert],
							patch_cp, step, mesh->xyzArray[0], 4 );

			attribs[numattribs] = ( uint8_t * )mesh->normalsArray[0];
			attribsizes[numattribs++] = sizeof( vec4_t );
			Patch_Evaluate( vec_t, 3, loadmodel_normals_array[inFirstVert],
							patch_cp, step, mesh->normalsArray[0], 4 );

			attribs[numattribs] = ( uint8_t * )mesh->stArray[0];
			attribsizes[numattribs++] = sizeof( vec2_t );
			Patch_Evaluate( vec_t, 2, loadmodel_st_array[inFirstVert],
							patch_cp, step, mesh->stArray[0], 0 );

			Patch_RemoveLinearColumnsRows( mesh->xyzArray[0], 4, &size[0], &size[1], numattribs, attribs, attribsizes );
			numVerts = size[0] * size[1];
			numElems = ( size[0] - 1 ) * ( size[1] - 1 ) * 6;
			if( numVerts != mesh->numVerts ) {
				size_t normalsPos, sVectorsPos, stPos, colorsPos;
				uint8_t *oldBuffer = buffer;

				mesh->numVerts = numVerts;
				mesh->numElems = numElems;

				bufPos = numVerts * sizeof( vec4_t );

				normalsPos = bufPos;
				memmove( buffer + normalsPos, mesh->normalsArray, numVerts * sizeof( vec4_t ) );
				bufPos += numVerts * sizeof( vec4_t );

				sVectorsPos = bufPos;
				bufPos += numVerts * sizeof( vec4_t );

				stPos = bufPos;
				memmove( buffer + stPos, mesh->stArray, numVerts * sizeof( vec2_t ) );
				bufPos += numVerts * sizeof( vec2_t );

				colorsPos = bufPos;
				memmove( buffer + colorsPos, mesh->colorsArray, numVerts * sizeof( byte_vec4_t ) );
				bufPos += numVerts * sizeof( byte_vec4_t );

				bufSize = ALIGN( bufPos, sizeof( elem_t ) ) + numElems * sizeof( elem_t );
				buffer = ( uint8_t * )Mod_Malloc( loadmodel, bufSize );
				memcpy( buffer, oldBuffer, bufSize );
				R_Free( oldBuffer );

				mesh = &out->mesh;
				mesh->xyzArray = ( vec4_t * )( buffer );
				mesh->normalsArray = ( vec4_t * )( buffer + normalsPos );
				mesh->sVectorsArray = ( vec4_t * )( buffer + sVectorsPos );
				mesh->stArray = ( vec2_t * )( buffer + stPos );
				mesh->colorsArray = ( byte_vec4_t * )( buffer + colorsPos );
			}

			// compute new elems
			bufPos = ALIGN( bufPos, sizeof( elem_t ) );
			mesh->elems = elems = ( elem_t * )( buffer + bufPos ); bufPos += numElems * sizeof( elem_t );
			for( v = 0, i = 0; v < size[1] - 1; v++ ) {
				for( u = 0; u < size[0] - 1; u++ ) {
					p = v * size[0] + u;
					elems[0] = p;
					elems[1] = p + size[0];
					elems[2] = p + 1;
					elems[3] = p + 1;
					elems[4] = p + size[0];
					elems[5] = p + size[0] + 1;
					elems += 6;
				}
			}

			for( i = 0; i < numVerts; i++ ) {
				mesh->xyzArray[i][3] = 1;
				mesh->normalsArray[i][3] = 0;
				VectorNormalize( mesh->normalsArray[i] );
			}

			R_BuildTangentVectors( mesh->numVerts, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
			break;
		}
		case FACETYPE_PLANAR:
		case FACETYPE_TRISURF:
		case FACETYPE_FOLIAGE:
		{
			unsigned j;
			unsigned numVerts, firstVert, numElems, firstElem;
			unsigned numFoliageInstances;

			if( out->facetype == FACETYPE_FOLIAGE ) {
				// foliage needs special care for instanced drawing
				numFoliageInstances = LittleLong( in->patch_cp[0] );
				numVerts = LittleLong( in->patch_cp[1] );
			} else {
				numFoliageInstances = 0;
				numVerts = LittleLong( in->numverts );
			}

			firstVert = LittleLong( in->firstvert );
			numElems = LittleLong( in->numelems );
			firstElem = LittleLong( in->firstelem );

			bufSize = numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + sizeof( byte_vec4_t ) );
			bufSize = ALIGN( bufSize, sizeof( elem_t ) ) + numElems * sizeof( elem_t );
			bufSize = ALIGN( bufSize, 16 ) + numFoliageInstances * sizeof( instancePoint_t );

			buffer = ( uint8_t * )Mod_Malloc( loadmodel, bufSize );
			bufPos = 0;

			mesh = &out->mesh;
			mesh->numVerts = numVerts;
			mesh->numElems = numElems;

			mesh->xyzArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->sVectorsArray = ( vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( vec2_t );

			for( j = 0; j < numVerts; j++ ) {
				VectorCopy( loadmodel_xyz_array[firstVert + j], mesh->xyzArray[j] );
				mesh->xyzArray[j][3] = 1;

				VectorCopy( loadmodel_normals_array[firstVert + j], mesh->normalsArray[j] );
				mesh->normalsArray[j][3] = 0;
			}

			memcpy( mesh->stArray, loadmodel_st_array + firstVert, numVerts * sizeof( vec2_t ) );

			mesh->colorsArray = ( byte_vec4_t * )( buffer + bufPos ); bufPos += numVerts * sizeof( byte_vec4_t );
			memcpy( mesh->colorsArray, loadmodel_colors_array + firstVert, numVerts * sizeof( byte_vec4_t ) );

			bufPos = ALIGN( bufPos, sizeof( elem_t ) );
			mesh->elems = ( elem_t * )( buffer + bufPos ); bufPos += numElems * sizeof( elem_t );
			memcpy( mesh->elems, loadmodel_surfelems + firstElem, numElems * sizeof( elem_t ) );

			R_BuildTangentVectors( mesh->numVerts, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );

			if( out->facetype == FACETYPE_PLANAR ) {
				vec3_t v[3];
				cplane_t plane;

				// don't trust q3map, recalculate surface plane from the first triangle
				for( j = 0; j < 3; j++ ) {
					VectorCopy( mesh->xyzArray[mesh->elems[j]], v[j] );
				}

				PlaneFromPoints( v, &plane );
				CategorizePlane( &plane );

				VectorCopy( plane.normal, out->plane );
				out->plane[3] = plane.dist;
			}

			if( numFoliageInstances > 0 ) {
				vec3_t *origins = loadmodel_xyz_array + firstVert, *origin;
				instancePoint_t *instance;

				out->numInstances = numFoliageInstances;
				out->instances = ( instancePoint_t * )( buffer + ALIGN( bufPos, 16 ) );

				for( j = 0; j < out->numInstances; j++ ) {
					// add pseudo random YAW-angle rotation
					vec3_t angles = { 0, 0, 0 };
					mat3_t rot;

					origin = origins + j;
					instance = out->instances + j;

					angles[YAW] = anglemod( j );
					AnglesToAxis( angles, rot );
					Quat_FromMatrix3( rot, *instance );

					VectorCopy( *origin, &( ( *instance )[4] ) );
					( *instance )[7] = 1.0f;
				}
			}
			break;
		}
	}
}

/*
* Mod_LoadPatchGroups
*/
static void Mod_LoadPatchGroups( const lump_t *l ) {
	int i, j, count;
	int *out = NULL;
	int *patches = NULL, patchcount;
	int facetype;

	count = loadbmodel->numsurfaces;
	out = ( int * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );
	memset( out, -1, count * sizeof( *out ) );

	patchcount = 0;
	patches = ( int * ) Mod_Malloc( loadmodel, count * sizeof( *patches ) );
	for( i = 0; i < count; i++ ) {
		facetype = loadbmodel->surfaces[i].facetype;
		if( facetype != FACETYPE_PATCH ) {
			continue;
		}
		patches[patchcount++] = i;
	}

	loadmodel_numpatchgroups = 0;
	loadmodel_maxpatchgroups = 0;
	loadmodel_patchgroups = NULL;
	loadmodel_patchgrouprefs = out;

	if( !patchcount ) {
		R_Free( patches );
		return;
	}

	// allocate patch groups to possibly hold all patches individually
	loadmodel_maxpatchgroups = patchcount;
	loadmodel_patchgroups = ( mpatchgroup_t * ) Mod_Malloc( loadmodel, loadmodel_maxpatchgroups * sizeof( *loadmodel_patchgroups ) );

	// assign patches to groups based on LOD bounds
	for( i = 0; i < patchcount; i++ ) {
		j = patches[i];
		out[j] = Mod_AddUpdatePatchGroup( loadmodel_dsurfaces + j );
	}

	R_Free( patches );

	ri.Com_DPrintf( "Mod_LoadPatchGroups: count (%i), groups(%i)\n", patchcount, loadmodel_numpatchgroups );

#undef Mod_PreloadPatches_PROLOGUE
#undef Mod_PreloadPatches_COUNT
}

/*
* Mod_LoadNodes
*/
static void Mod_LoadNodes( const lump_t *l ) {
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = ( dnode_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( mnode_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = loadbmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ ) {
			p = LittleLong( in->children[j] );
			if( p >= 0 ) {
				out->children[j] = loadbmodel->nodes + p;
			} else {
				out->children[j] = ( mnode_t * )( loadbmodel->leafs + ( -1 - p ) );
			}
		}
	}
}

/*
* Mod_LoadLeafs
*/
static void Mod_LoadLeafs( const lump_t *l, const lump_t *msLump ) {
	int i, j, count, countMarkSurfaces;
	dleaf_t *in;
	mleaf_t *out;
	size_t size;
	uint8_t *buffer;
	bool badBounds;
	int *inMarkSurfaces;
	int numMarkSurfaces, firstMarkSurface;
	int numVisSurfaces, numFragmentSurfaces;

	inMarkSurfaces = ( int * )( mod_base + msLump->fileofs );
	if( msLump->filelen % sizeof( *inMarkSurfaces ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	}
	countMarkSurfaces = msLump->filelen / sizeof( *inMarkSurfaces );

	in = ( dleaf_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( mleaf_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		badBounds = false;
		for( j = 0; j < 3; j++ ) {
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) {
				badBounds = true;
			}
		}
		out->cluster = LittleLong( in->cluster );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs ) ) && out->cluster >= 0 ) {
			ri.Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf bounds\n" );
			out->cluster = -1;
		}

		if( loadbmodel->pvs && ( out->cluster >= loadbmodel->pvs->numclusters ) ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: leaf cluster > numclusters" );
			out->cluster = -1;
		}

		out->plane = NULL;
		out->area = LittleLong( in->area );
		if( out->area >= loadbmodel->numareas ) {
			loadbmodel->numareas = out->area + 1;
		}

		numVisSurfaces = numFragmentSurfaces = 0;
		numMarkSurfaces = LittleLong( in->numleaffaces );
		if( !numMarkSurfaces ) {
			//out->cluster = -1;
			continue;
		}

		firstMarkSurface = LittleLong( in->firstleafface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces ) {
			ri.Com_Error( ERR_DROP, "MOD_LoadBmodel: bad marksurfaces in leaf %i", i );
		}

		numVisSurfaces = numMarkSurfaces;
		numFragmentSurfaces = numMarkSurfaces;

		size = ( numVisSurfaces + numFragmentSurfaces ) * sizeof( unsigned );
		buffer = ( uint8_t * )Mod_Malloc( loadmodel, size );

		out->visSurfaces = ( unsigned * )buffer;
		buffer += numVisSurfaces * sizeof( unsigned );

		out->fragmentSurfaces = ( unsigned * )buffer;
		buffer += numFragmentSurfaces * sizeof( unsigned );

		numVisSurfaces = numFragmentSurfaces = 0;
		for( j = 0; j < numMarkSurfaces; j++ ) {
			unsigned k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			out->visSurfaces[numVisSurfaces++] = k;
			out->fragmentSurfaces[numFragmentSurfaces++] = k;
		}

		out->numVisSurfaces = numVisSurfaces;
		out->numFragmentSurfaces = numFragmentSurfaces;
	}
}

/*
* Mod_LoadElems
*/
static void Mod_LoadElems( const lump_t *l ) {
	int i, count;
	int *in;
	elem_t  *out;

	in = ( int * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadElems: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( elem_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_surfelems = out;
	loadmodel_numsurfelems = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
* Mod_LoadPlanes
*/
static void Mod_LoadPlanes( const lump_t *l ) {
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;

	in = ( dplane_t * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = ( cplane_t * ) Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->type = PLANE_NONAXIAL;
		out->signbits = 0;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 ) {
				out->signbits |= 1 << j;
			}
			if( out->normal[j] == 1.0f ) {
				out->type = j;
			}
		}
		out->dist = LittleFloat( in->dist );
	}
}

/*
* Mod_LoadVisibility
*/
static void Mod_LoadVisibility( const lump_t *l ) {
	dvis_t *out;
	const dvis_t *in;

	if( !l->filelen ) {
		loadbmodel->pvs = NULL;
		return;
	}

	in = ( const dvis_t * ) ( mod_base + l->fileofs );
	out = ( dvis_t * ) Mod_Malloc( loadmodel, l->filelen );
	loadbmodel->pvs = out;

	memcpy( out, mod_base + l->fileofs, l->filelen );
	out->numclusters = LittleLong( in->numclusters );
	out->rowsize = LittleLong( in->rowsize );
}

/*
* Mod_LoadEntities
*/
static void Mod_LoadEntities( const lump_t *l, vec3_t gridSize, vec3_t ambient, vec3_t outline ) {
	int n;
	char *data;
	bool isworld;
	float gridsizef[3] = { 0, 0, 0 }, colorf[3] = { 0, 0, 0 }, originf[3], ambientf = 0;
	char key[MAX_KEY], value[MAX_VALUE], *token;
	float celcolorf[3] = { 0, 0, 0 };

	assert( gridSize );
	assert( ambient );
	assert( outline );

	VectorClear( gridSize );
	VectorClear( ambient );
	VectorClear( outline );

	data = (char *)mod_base + l->fileofs;
	if( !data[0] ) {
		return;
	}

	loadbmodel->entityStringLen = l->filelen;
	loadbmodel->entityString = ( char * )Mod_Malloc( loadmodel, l->filelen + 1 );
	memcpy( loadbmodel->entityString, data, l->filelen );
	loadbmodel->entityString[l->filelen] = '\0';

	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; ) {
		isworld = false;
		VectorClear( colorf );

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
				if( !strcmp( value, "worldspawn" ) ) {
					isworld = true;
				}
			} else if( !strcmp( key, "gridsize" ) ) {
				int gridsizei[3] = { 0, 0, 0 };
				sscanf( value, "%4i %4i %4i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
				VectorCopy( gridsizei, gridsizef );
			} else if( !strcmp( key, "_ambient" ) || ( !strcmp( key, "ambient" ) && ambientf == 0.0f ) ) {
				n = sscanf( value, "%8f", &ambientf );
				if( n != 1 ) {
					int ia = 0;
					sscanf( value, "%3i", &ia );
					ambientf = ia;
				}
			} else if( !strcmp( key, "_color" ) ) {
				n = sscanf( value, "%8f %8f %8f", &colorf[0], &colorf[1], &colorf[2] );
				if( n != 3 ) {
					int colori[3] = { 0, 0, 0 };
					sscanf( value, "%3i %3i %3i", &colori[0], &colori[1], &colori[2] );
					VectorCopy( colori, colorf );
				}
			} else if( !strcmp( key, "color" ) ) {
				n = sscanf( value, "%8f %8f %8f", &colorf[0], &colorf[1], &colorf[2] );
				if( n != 3 ) {
					int colori[3] = { 0, 0, 0 };
					sscanf( value, "%3i %3i %3i", &colori[0], &colori[1], &colori[2] );
					VectorCopy( colori, colorf );
				}
			} else if( !strcmp( key, "origin" ) ) {
				n = sscanf( value, "%8f %8f %8f", &originf[0], &originf[1], &originf[2] );
			} else if( !strcmp( key, "_outlinecolor" ) ) {
				n = sscanf( value, "%8f %8f %8f", &celcolorf[0], &celcolorf[1], &celcolorf[2] );
				if( n != 3 ) {
					int celcolori[3] = { 0, 0, 0 };
					sscanf( value, "%3i %3i %3i", &celcolori[0], &celcolori[1], &celcolori[2] );
					VectorCopy( celcolori, celcolorf );
				}
			}
		}

		if( isworld ) {
			VectorCopy( gridsizef, gridSize );

			if( VectorCompare( colorf, vec3_origin ) ) {
				VectorSet( colorf, 1.0, 1.0, 1.0 );
			}
			VectorScale( colorf, ambientf, ambient );

			if( max( celcolorf[0], max( celcolorf[1], celcolorf[2] ) ) > 1.0f ) {
				VectorScale( celcolorf, 1.0f / 255.0f, celcolorf );   // [0..1] RGB -> [0..255] RGB
			}
			VectorCopy( celcolorf, outline );
		}
	}
}

/*
* Mod_Finish
*/
static void Mod_Finish( const lump_t *faces, vec3_t gridSize, vec3_t ambient, vec3_t outline ) {
	unsigned int i, j;
	msurface_t *surf;
	rdface_t *in;

	// remembe the BSP format just in case
	loadbmodel->format = mod_bspFormat;

	// set up lightgrid
	if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 ) {
		VectorSet( loadbmodel->gridSize, 64, 64, 128 );
	} else {
		VectorCopy( gridSize, loadbmodel->gridSize );
	}

	for( j = 0; j < 3; j++ ) {
		vec3_t maxs;

		loadbmodel->gridMins[j] = loadbmodel->gridSize[j] * ceil( ( loadbmodel->submodels[0].mins[j] + 1 ) / loadbmodel->gridSize[j] );
		maxs[j] = loadbmodel->gridSize[j] * floor( ( loadbmodel->submodels[0].maxs[j] - 1 ) / loadbmodel->gridSize[j] );
		loadbmodel->gridBounds[j] = ( maxs[j] - loadbmodel->gridMins[j] ) / loadbmodel->gridSize[j];
		loadbmodel->gridBounds[j] = max( loadbmodel->gridBounds[j], 0 ) + 1;
	}
	loadbmodel->gridBounds[3] = loadbmodel->gridBounds[1] * loadbmodel->gridBounds[0];

	// ambient lighting
	VectorScale( ambient, 1.0f / 255.0f, mapConfig.ambient );

	// outline color
	for( i = 0; i < 3; i++ )
		mapConfig.outlineColor[i] = (uint8_t)( bound( 0, outline[i] * 255.0f, 255 ) );
	mapConfig.outlineColor[3] = 255;

	in = loadmodel_dsurfaces;
	surf = loadbmodel->surfaces;
	for( i = 0; i < loadbmodel->numsurfaces; i++, in++, surf++ ) {
		Mod_CreateMeshForSurface( in, surf, loadmodel_patchgrouprefs[i] );
	}

	if( !( mod_bspFormat->flags & BSP_RAVEN ) ) {
		Mod_MemFree( loadmodel_dsurfaces );
	}
	loadmodel_dsurfaces = NULL;
	loadmodel_numsurfaces = 0;

	Mod_MemFree( loadmodel_xyz_array );
	loadmodel_xyz_array = NULL;
	loadmodel_numverts = 0;

	Mod_MemFree( loadmodel_surfelems );
	loadmodel_surfelems = NULL;
	loadmodel_numsurfelems = 0;

	Mod_MemFree( loadmodel_shaderrefs );
	loadmodel_shaderrefs = NULL;
	loadmodel_numshaderrefs = 0;

	Mod_MemFree( loadmodel_patchgrouprefs );
	loadmodel_patchgrouprefs = NULL;

	Mod_MemFree( loadmodel_patchgroups );
	loadmodel_patchgroups = NULL;
	loadmodel_numpatchgroups = loadmodel_maxpatchgroups = 0;
}

/*
* Mod_LoadQ3BrushModel
*/
void Mod_LoadQ3BrushModel( model_t *mod, const model_t *parent, void *buffer, bspFormatDesc_t *format ) {
	dheader_t *header;
	vec3_t gridSize, ambient, outline;

	mod->type = mod_brush;
	mod->registrationSequence = rsh.registrationSequence;
	if( rsh.worldModel != NULL ) {
		ri.Com_Error( ERR_DROP, "Loaded a brush model after the world" );
	}

	loadmodel = mod;

	mod_bspFormat = format;

	header = (dheader_t *)buffer;
	mod_base = (uint8_t *)header;

	// swap all the lumps
	for( size_t i = 0; i < sizeof( dheader_t ) / sizeof( int ); i++ )
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );

	// load into heap
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	Mod_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], gridSize, ambient, outline );
	Mod_LoadShaderrefs( &header->lumps[LUMP_SHADERREFS] );
	Mod_PreloadFaces( &header->lumps[LUMP_FACES] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFaces( &header->lumps[LUMP_FACES] );
	if( mod_bspFormat->flags & BSP_RAVEN ) {
		Mod_LoadVertexes_RBSP( &header->lumps[LUMP_VERTEXES] );
	} else {
		Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	}
	Mod_LoadElems( &header->lumps[LUMP_ELEMENTS] );
	Mod_LoadPatchGroups( &header->lumps[LUMP_FACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_LEAFFACES] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );

	Mod_Finish( &header->lumps[LUMP_FACES], gridSize, ambient, outline );
}
