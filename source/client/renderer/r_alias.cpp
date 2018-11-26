/*
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

// r_alias.c: Quake III Arena .md3 model format support

#include "r_local.h"
#include "../../qcommon/qfiles.h"

#define MD3SURF_DISTANCE(s, d) ((s)->flags & SHADER_AUTOSPRITE ? d : 0)

/*
* Mod_AliasBuildStaticVBOForMesh
*
* Builds a static vertex buffer object for given alias model mesh
*/
static void Mod_AliasBuildStaticVBOForMesh( maliasmesh_t *mesh ) {
	int i;
	mesh_t aliasmesh;
	vattribmask_t vattribs;

	vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT | VATTRIB_NORMAL_BIT | VATTRIB_SVECTOR_BIT;
	for( i = 0; i < mesh->numskins; i++ ) {
		if( mesh->skins[i].shader ) {
			vattribs |= mesh->skins[i].shader->vattribs;
		}
	}

	mesh->vbo = R_CreateMeshVBO( ( void * )mesh,
								 mesh->numverts, mesh->numtris * 3, 0, vattribs, VBO_TAG_MODEL, vattribs );

	if( !mesh->vbo ) {
		return;
	}

	memset( &aliasmesh, 0, sizeof( aliasmesh ) );

	aliasmesh.elems = mesh->elems;
	aliasmesh.numElems = mesh->numtris * 3;
	aliasmesh.numVerts = mesh->numverts;

	aliasmesh.xyzArray = mesh->xyzArray;
	aliasmesh.stArray = mesh->stArray;
	aliasmesh.normalsArray = mesh->normalsArray;
	aliasmesh.sVectorsArray = mesh->sVectorsArray;

	R_UploadVBOVertexData( mesh->vbo, 0, vattribs, &aliasmesh, 0 );
	R_UploadVBOElemData( mesh->vbo, 0, 0, &aliasmesh );
}

/*
* Mod_AliasBuildMeshesForFrame0
*/
static void Mod_AliasBuildMeshesForFrame0( model_t *mod ) {
	int i, j, k;
	size_t size;
	maliasframe_t *frame;
	maliasmodel_t *aliasmodel = ( maliasmodel_t * )mod->extradata;

	frame = &aliasmodel->frames[0];
	for( k = 0; k < aliasmodel->nummeshes; k++ ) {
		maliasmesh_t *mesh = &aliasmodel->meshes[k];

		size = sizeof( vec4_t ) + sizeof( vec4_t ); // xyz and normals
		size += sizeof( vec4_t );       // s-vectors
		size *= mesh->numverts;

		mesh->xyzArray = ( vec4_t * )Mod_Malloc( mod, size );
		mesh->normalsArray = ( vec4_t * )( ( uint8_t * )mesh->xyzArray + mesh->numverts * sizeof( vec4_t ) );
		mesh->sVectorsArray = ( vec4_t * )( ( uint8_t * )mesh->normalsArray + mesh->numverts * sizeof( vec4_t ) );

		for( i = 0; i < mesh->numverts; i++ ) {
			for( j = 0; j < 3; j++ ) {
				mesh->xyzArray[i][j] = frame->translate[j] + frame->scale[j] * mesh->vertexes[i].point[j];
			}
			mesh->xyzArray[i][3] = 1;

			R_LatLongToNorm4( mesh->vertexes[i].latlong, mesh->normalsArray[i] );
		}

		R_BuildTangentVectors( mesh->numverts, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numtris, mesh->elems, mesh->sVectorsArray );

		// build a static vertex buffer object to be used for rendering simple models, such as items
		Mod_AliasBuildStaticVBOForMesh( mesh );
	}
}

/*
* Mod_TouchAliasModel
*/
static void Mod_TouchAliasModel( model_t *mod ) {
	int i, j;
	maliasmesh_t *mesh;
	maliasskin_t *skin;
	maliasmodel_t *aliasmodel = ( maliasmodel_t * )mod->extradata;

	mod->registrationSequence = rsh.registrationSequence;

	for( i = 0, mesh = aliasmodel->meshes; i < aliasmodel->nummeshes; i++, mesh++ ) {
		// register needed skins and images
		for( j = 0, skin = mesh->skins; j < mesh->numskins; j++, skin++ ) {
			if( skin->shader ) {
				R_TouchShader( skin->shader );
			}
		}

		if( mesh->vbo ) {
			R_TouchMeshVBO( mesh->vbo );
		}
	}
}

/*
==============================================================================

MD3 MODELS

==============================================================================
*/

/*
* Mod_LoadAliasMD3Model
*/
void Mod_LoadAliasMD3Model( model_t *mod, const model_t *parent, void *buffer, bspFormatDesc_t *unused ) {
	int version, i, j, l;
	int bufsize, numverts;
	uint8_t *buf;
	dmd3header_t *pinmodel;
	dmd3frame_t *pinframe;
	dmd3tag_t *pintag;
	dmd3mesh_t *pinmesh;
	dmd3skin_t *pinskin;
	dmd3coord_t *pincoord;
	dmd3vertex_t *pinvert;
	unsigned int *pinelem;
	elem_t *poutelem;
	maliasvertex_t *poutvert;
	vec2_t *poutcoord;
	maliasskin_t *poutskin;
	maliasmesh_t *poutmesh;
	maliastag_t *pouttag;
	maliasframe_t *poutframe;
	maliasmodel_t *poutmodel;
	drawSurfaceAlias_t *drawSurf;

	pinmodel = ( dmd3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if( version != MD3_ALIAS_VERSION ) {
		ri.Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
					  mod->name, version, MD3_ALIAS_VERSION );
	}

	mod->type = mod_alias;
	mod->extradata = poutmodel = ( maliasmodel_t * )Mod_Malloc( mod, sizeof( maliasmodel_t ) );
	mod->radius = 0;
	mod->registrationSequence = rsh.registrationSequence;
	mod->touch = &Mod_TouchAliasModel;

	ClearBounds( mod->mins, mod->maxs );

	// byte swap the header fields and sanity check
	poutmodel->numframes = LittleLong( pinmodel->num_frames );
	poutmodel->numtags = LittleLong( pinmodel->num_tags );
	poutmodel->nummeshes = LittleLong( pinmodel->num_meshes );
	poutmodel->numskins = 0;
	poutmodel->numverts = 0;
	poutmodel->numtris = 0;

	if( poutmodel->numframes <= 0 ) {
		ri.Com_Error( ERR_DROP, "model %s has no frames", mod->name );
	}
	//	else if( poutmodel->numframes > MD3_MAX_FRAMES )
	//		ri.Com_Error( ERR_DROP, "model %s has too many frames", mod->name );

	if( poutmodel->numtags > MD3_MAX_TAGS ) {
		ri.Com_Error( ERR_DROP, "model %s has too many tags", mod->name );
	} else if( poutmodel->numtags < 0 ) {
		ri.Com_Error( ERR_DROP, "model %s has invalid number of tags", mod->name );
	}

	if( poutmodel->nummeshes < 0 ) {
		ri.Com_Error( ERR_DROP, "model %s has invalid number of meshes", mod->name );
	} else if( !poutmodel->nummeshes && !poutmodel->numtags ) {
		ri.Com_Error( ERR_DROP, "model %s has no meshes and no tags", mod->name );
	}
	//	else if( poutmodel->nummeshes > MD3_MAX_MESHES )
	//		ri.Com_Error( ERR_DROP, "model %s has too many meshes", mod->name );

	bufsize = poutmodel->numframes * ( sizeof( maliasframe_t ) + sizeof( maliastag_t ) * poutmodel->numtags ) +
			  poutmodel->nummeshes * sizeof( maliasmesh_t ) +
			  poutmodel->nummeshes * sizeof( drawSurfaceAlias_t );
	buf = ( uint8_t * )Mod_Malloc( mod, bufsize );

	//
	// load the frames
	//
	pinframe = ( dmd3frame_t * )( ( uint8_t * )pinmodel + LittleLong( pinmodel->ofs_frames ) );
	poutframe = poutmodel->frames = ( maliasframe_t * )buf; buf += sizeof( maliasframe_t ) * poutmodel->numframes;
	for( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ ) {
		memcpy( poutframe->translate, pinframe->translate, sizeof( vec3_t ) );
		for( j = 0; j < 3; j++ ) {
			poutframe->scale[j] = MD3_XYZ_SCALE;
			poutframe->translate[j] = LittleFloat( poutframe->translate[j] );
		}

		// never trust the modeler utility and recalculate bbox and radius
		ClearBounds( poutframe->mins, poutframe->maxs );
	}

	//
	// load the tags
	//
	pintag = ( dmd3tag_t * )( ( uint8_t * )pinmodel + LittleLong( pinmodel->ofs_tags ) );
	pouttag = poutmodel->tags = ( maliastag_t * )buf; buf += sizeof( maliastag_t ) * poutmodel->numframes * poutmodel->numtags;
	for( i = 0; i < poutmodel->numframes; i++ ) {
		for( l = 0; l < poutmodel->numtags; l++, pintag++, pouttag++ ) {
			dmd3tag_t intag;
			mat3_t axis;

			memcpy( &intag, pintag, sizeof( dmd3tag_t ) );

			for( j = 0; j < 3; j++ ) {
				axis[AXIS_FORWARD + j] = LittleFloat( intag.axis[0][j] );
				axis[AXIS_RIGHT + j] = LittleFloat( intag.axis[1][j] );
				axis[AXIS_UP + j] = LittleFloat( intag.axis[2][j] );
				pouttag->origin[j] = LittleFloat( intag.origin[j] );
			}

			Quat_FromMatrix3( axis, pouttag->quat );
			Quat_Normalize( pouttag->quat );

			Q_strncpyz( pouttag->name, intag.name, MD3_MAX_PATH );
		}
	}

	//
	// allocate drawSurfs
	//
	drawSurf = poutmodel->drawSurfs = ( drawSurfaceAlias_t * )buf; buf += sizeof( drawSurfaceAlias_t ) * poutmodel->nummeshes;
	for( i = 0; i < poutmodel->nummeshes; i++, drawSurf++ ) {
		drawSurf->type = ST_ALIAS;
		drawSurf->model = mod;
		drawSurf->mesh = poutmodel->meshes + i;
	}

	//
	// load meshes
	//
	pinmesh = ( dmd3mesh_t * )( ( uint8_t * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = ( maliasmesh_t * )buf; buf += sizeof( maliasmesh_t ) * poutmodel->nummeshes;
	for( i = 0; i < poutmodel->nummeshes; i++, poutmesh++ ) {
		dmd3mesh_t inmesh;

		memcpy( &inmesh, pinmesh, sizeof( dmd3mesh_t ) );

		if( strncmp( (const char *)inmesh.id, IDMD3HEADER, 4 ) ) {
			ri.Com_Error( ERR_DROP, "mesh %s in model %s has wrong id (%s should be %s)",
						  inmesh.name, mod->name, inmesh.id, IDMD3HEADER );
		}

		Q_strncpyz( poutmesh->name, inmesh.name, MD3_MAX_PATH );

		Mod_StripLODSuffix( poutmesh->name );

		poutmesh->numtris = LittleLong( inmesh.num_tris );
		poutmesh->numskins = LittleLong( inmesh.num_skins );
		poutmesh->numverts = numverts = LittleLong( inmesh.num_verts );

		poutmodel->numverts += poutmesh->numverts;
		poutmodel->numtris += poutmesh->numtris;

		/*		if( poutmesh->numskins <= 0 )
		ri.Com_Error( ERR_DROP, "mesh %i in model %s has no skins", i, mod->name );
		else*/if( poutmesh->numskins > MD3_MAX_SHADERS ) {
			ri.Com_Error( ERR_DROP, "mesh %i in model %s has too many skins", i, mod->name );
		}
		if( poutmesh->numtris <= 0 ) {
			ri.Com_Error( ERR_DROP, "mesh %i in model %s has no elements", i, mod->name );
		} else if( poutmesh->numtris > MD3_MAX_TRIANGLES ) {
			ri.Com_Error( ERR_DROP, "mesh %i in model %s has too many triangles", i, mod->name );
		}
		if( poutmesh->numverts <= 0 ) {
			ri.Com_Error( ERR_DROP, "mesh %i in model %s has no vertices", i, mod->name );
		} else if( poutmesh->numverts > MD3_MAX_VERTS ) {
			ri.Com_Error( ERR_DROP, "mesh %i in model %s has too many vertices", i, mod->name );
		}

		bufsize = ALIGN( sizeof( maliasskin_t ) * poutmesh->numskins, sizeof( vec_t ) ) +
				  numverts * ( sizeof( vec2_t ) + sizeof( maliasvertex_t ) * poutmodel->numframes ) +
				  poutmesh->numtris * sizeof( elem_t ) * 3;
		buf = ( uint8_t * )Mod_Malloc( mod, bufsize );

		//
		// load the skins
		//
		pinskin = ( dmd3skin_t * )( ( uint8_t * )pinmesh + LittleLong( inmesh.ofs_skins ) );
		poutskin = poutmesh->skins = ( maliasskin_t * )buf;
		buf += ALIGN( sizeof( maliasskin_t ) * poutmesh->numskins, sizeof( vec_t ) );
		for( j = 0; j < poutmesh->numskins; j++, pinskin++, poutskin++ ) {
			Q_strncpyz( poutskin->name, pinskin->name, sizeof( poutskin->name ) );
			poutskin->shader = R_RegisterSkin( poutskin->name );
		}

		//
		// load the texture coordinates
		//
		pincoord = ( dmd3coord_t * )( ( uint8_t * )pinmesh + LittleLong( inmesh.ofs_tcs ) );
		poutcoord = poutmesh->stArray = ( vec2_t * )buf; buf += poutmesh->numverts * sizeof( vec2_t );
		for( j = 0; j < poutmesh->numverts; j++, pincoord++ ) {
			memcpy( poutcoord[j], pincoord->st, sizeof( vec2_t ) );
			poutcoord[j][0] = LittleFloat( poutcoord[j][0] );
			poutcoord[j][1] = LittleFloat( poutcoord[j][1] );
		}

		//
		// load the vertexes and normals
		//
		pinvert = ( dmd3vertex_t * )( ( uint8_t * )pinmesh + LittleLong( inmesh.ofs_verts ) );
		poutvert = poutmesh->vertexes = ( maliasvertex_t * )buf;
		buf += poutmesh->numverts * sizeof( maliasvertex_t ) * poutmodel->numframes;
		for( l = 0, poutframe = poutmodel->frames; l < poutmodel->numframes; l++, poutframe++, pinvert += poutmesh->numverts, poutvert += poutmesh->numverts ) {
			vec3_t v;

			for( j = 0; j < poutmesh->numverts; j++ ) {
				dmd3vertex_t invert;

				memcpy( &invert, &( pinvert[j] ), sizeof( dmd3vertex_t ) );

				poutvert[j].point[0] = LittleShort( invert.point[0] );
				poutvert[j].point[1] = LittleShort( invert.point[1] );
				poutvert[j].point[2] = LittleShort( invert.point[2] );

				poutvert[j].latlong[0] = invert.norm[0];
				poutvert[j].latlong[1] = invert.norm[1];

				VectorCopy( poutvert[j].point, v );
				AddPointToBounds( v, poutframe->mins, poutframe->maxs );
			}
		}

		//
		// load the elems
		//
		pinelem = ( unsigned int * )( ( uint8_t * )pinmesh + LittleLong( inmesh.ofs_elems ) );
		poutelem = poutmesh->elems = ( elem_t * )buf;
		for( j = 0; j < poutmesh->numtris; j++, pinelem += 3, poutelem += 3 ) {
			unsigned int inelem[3];

			memcpy( inelem, pinelem, sizeof( int ) * 3 );

			poutelem[0] = (elem_t)LittleLong( inelem[0] );
			poutelem[1] = (elem_t)LittleLong( inelem[1] );
			poutelem[2] = (elem_t)LittleLong( inelem[2] );
		}

		pinmesh = ( dmd3mesh_t * )( ( uint8_t * )pinmesh + LittleLong( inmesh.meshsize ) );
	}

	//
	// setup drawSurfs
	//
	for( i = 0; i < poutmodel->nummeshes; i++ ) {
		drawSurf = poutmodel->drawSurfs + i;
		drawSurf->type = ST_ALIAS;
		drawSurf->model = mod;
		drawSurf->mesh = poutmodel->meshes + i;
	}

	//
	// build S and T vectors for frame 0
	//
	Mod_AliasBuildMeshesForFrame0( mod );

	//
	// calculate model bounds
	//
	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numframes; i++, poutframe++ ) {
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->mins, poutframe->mins );
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->maxs, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max( mod->radius, poutframe->radius );
	}
}

/*
* R_AliasModelLerpBBox
*/
static float R_AliasModelLerpBBox( const entity_t *e, const model_t *mod, vec3_t mins, vec3_t maxs ) {
	int i;
	int framenum = e->frame, oldframenum = e->oldframe;
	const maliasmodel_t *aliasmodel = ( const maliasmodel_t * )mod->extradata;
	const maliasframe_t *pframe, *poldframe;

	if( !aliasmodel->nummeshes ) {
		ClearBounds( mins, maxs );
		return 0;
	}

	if( ( framenum >= aliasmodel->numframes ) || ( framenum < 0 ) ) {
#ifndef PUBLIC_BUILD
		ri.Com_DPrintf( "R_AliasModelLerpBBox %s: no such frame %d\n", mod->name, framenum );
#endif
		framenum = 0;
	}

	if( ( oldframenum >= aliasmodel->numframes ) || ( oldframenum < 0 ) ) {
#ifndef PUBLIC_BUILD
		ri.Com_DPrintf( "R_AliasModelLerpBBox %s: no such oldframe %d\n", mod->name, oldframenum );
#endif
		oldframenum = 0;
	}

	pframe = aliasmodel->frames + framenum;
	poldframe = aliasmodel->frames + oldframenum;

	// compute axially aligned mins and maxs
	if( pframe == poldframe ) {
		VectorCopy( pframe->mins, mins );
		VectorCopy( pframe->maxs, maxs );
		if( e->scale == 1 ) {
			return pframe->radius;
		}
	} else {
		const float
		*thismins = pframe->mins,
		*oldmins = poldframe->mins,
		*thismaxs = pframe->maxs,
		*oldmaxs = poldframe->maxs;

		for( i = 0; i < 3; i++ ) {
			mins[i] = min( thismins[i], oldmins[i] );
			maxs[i] = max( thismaxs[i], oldmaxs[i] );
		}
	}

	VectorScale( mins, e->scale, mins );
	VectorScale( maxs, e->scale, maxs );
	return RadiusFromBounds( mins, maxs );
}

/*
* R_AliasModelLerpTag
*/
bool R_AliasModelLerpTag( orientation_t *orient, const maliasmodel_t *aliasmodel, int oldframenum, int framenum, float lerpfrac, const char *name ) {
	int i;
	quat_t quat;
	maliastag_t *tag, *oldtag;

	// find the appropriate tag
	for( i = 0; i < aliasmodel->numtags; i++ ) {
		if( !Q_stricmp( aliasmodel->tags[i].name, name ) ) {
			break;
		}
	}

	if( i == aliasmodel->numtags ) {
		//ri.Com_DPrintf ("R_AliasModelLerpTag: no such tag %s\n", name );
		return false;
	}

	// ignore invalid frames
	if( ( framenum >= aliasmodel->numframes ) || ( framenum < 0 ) ) {
#ifndef PUBLIC_BUILD
		ri.Com_DPrintf( "R_AliasModelLerpTag %s: no such oldframe %i\n", name, framenum );
#endif
		framenum = 0;
	}
	if( ( oldframenum >= aliasmodel->numframes ) || ( oldframenum < 0 ) ) {
#ifndef PUBLIC_BUILD
		ri.Com_DPrintf( "R_AliasModelLerpTag %s: no such oldframe %i\n", name, oldframenum );
#endif
		oldframenum = 0;
	}

	tag = aliasmodel->tags + framenum * aliasmodel->numtags + i;
	oldtag = aliasmodel->tags + oldframenum * aliasmodel->numtags + i;

	// interpolate axis and origin
	Quat_Lerp( oldtag->quat, tag->quat, lerpfrac, quat );
	Quat_ToMatrix3( quat, orient->axis );

	orient->origin[0] = oldtag->origin[0] + ( tag->origin[0] - oldtag->origin[0] ) * lerpfrac;
	orient->origin[1] = oldtag->origin[1] + ( tag->origin[1] - oldtag->origin[1] ) * lerpfrac;
	orient->origin[2] = oldtag->origin[2] + ( tag->origin[2] - oldtag->origin[2] ) * lerpfrac;

	return true;
}

/*
* R_DrawAliasSurf
*
* Interpolates between two frames and origins
*/
void R_DrawAliasSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, int lightStyleNum, 
	const portalSurface_t *portalSurface, drawSurfaceAlias_t *drawSurf ) {
	int i;
	int framenum = e->frame, oldframenum = e->oldframe;
	float backv[3], frontv[3];
	vec3_t normal, oldnormal;
	bool calcVerts, calcNormals, calcSTVectors;
	vec3_t move;
	const maliasframe_t *frame, *oldframe;
	const maliasvertex_t *v, *ov;
	float backlerp = e->backlerp;
	const maliasmodel_t *model = ( const maliasmodel_t * )drawSurf->model->extradata;
	const maliasmesh_t *aliasmesh = drawSurf->mesh;
	vattribmask_t vattribs;

	// see what vertex attribs backend needs
	vattribs = RB_GetVertexAttribs();

	if( ( framenum >= model->numframes ) || ( framenum < 0 ) ) {
		framenum = 0;
	}

	if( ( oldframenum >= model->numframes ) || ( oldframenum < 0 ) ) {
		oldframenum = 0;
	}

	frame = model->frames + framenum;
	oldframe = model->frames + oldframenum;
	for( i = 0; i < 3; i++ )
		move[i] = frame->translate[i] + ( oldframe->translate[i] - frame->translate[i] ) * backlerp;

	if( aliasmesh->vbo != NULL && !framenum && !oldframenum ) {
		RB_BindVBO( aliasmesh->vbo->index, GL_TRIANGLES );

		RB_DrawElements( 0, aliasmesh->numverts, 0, aliasmesh->numtris * 3 );
	} else {
		mesh_t dynamicMesh;
		vec4_t *inVertsArray;
		vec4_t *inNormalsArray;
		vec4_t *inSVectorsArray;

		// based on backend's needs
		calcVerts = ( framenum || oldframenum ) ? true : false;
		calcNormals = ( ( ( vattribs & VATTRIB_NORMAL_BIT ) != 0 ) && calcVerts ) ? true : false;
		calcSTVectors = ( ( ( vattribs & VATTRIB_SVECTOR_BIT ) != 0 ) && calcNormals ) ? true : false;

		memset( &dynamicMesh, 0, sizeof( dynamicMesh ) );

		dynamicMesh.elems = aliasmesh->elems;
		dynamicMesh.numElems = aliasmesh->numtris * 3;
		dynamicMesh.numVerts = aliasmesh->numverts;

		R_GetTransformBufferForMesh( &dynamicMesh, calcVerts, calcNormals, calcSTVectors );

		inVertsArray = dynamicMesh.xyzArray;
		inNormalsArray = dynamicMesh.normalsArray;
		inSVectorsArray = dynamicMesh.sVectorsArray;

		if( calcVerts ) {
			if( framenum == oldframenum ) {
				for( i = 0; i < 3; i++ )
					frontv[i] = frame->scale[i];

				v = aliasmesh->vertexes + framenum * aliasmesh->numverts;
				for( i = 0; i < aliasmesh->numverts; i++, v++ ) {
					Vector4Set( inVertsArray[i],
								move[0] + v->point[0] * frontv[0],
								move[1] + v->point[1] * frontv[1],
								move[2] + v->point[2] * frontv[2],
								1 );

					if( calcNormals ) {
						R_LatLongToNorm4( v->latlong, inNormalsArray[i] );
					}
				}
			} else {
				for( i = 0; i < 3; i++ ) {
					backv[i] = backlerp * oldframe->scale[i];
					frontv[i] = ( 1.0f - backlerp ) * frame->scale[i];
				}

				v = aliasmesh->vertexes + framenum * aliasmesh->numverts;
				ov = aliasmesh->vertexes + oldframenum * aliasmesh->numverts;
				for( i = 0; i < aliasmesh->numverts; i++, v++, ov++ ) {
					VectorSet( inVertsArray[i],
							   move[0] + v->point[0] * frontv[0] + ov->point[0] * backv[0],
							   move[1] + v->point[1] * frontv[1] + ov->point[1] * backv[1],
							   move[2] + v->point[2] * frontv[2] + ov->point[2] * backv[2] );

					if( calcNormals ) {
						R_LatLongToNorm( v->latlong, normal );
						R_LatLongToNorm( ov->latlong, oldnormal );

						Vector4Set( inNormalsArray[i],
								   normal[0] + ( oldnormal[0] - normal[0] ) * backlerp,
								   normal[1] + ( oldnormal[1] - normal[1] ) * backlerp,
								   normal[2] + ( oldnormal[2] - normal[2] ) * backlerp, 0 );
					}
				}
			}
		}

		if( calcSTVectors ) {
			R_BuildTangentVectors( aliasmesh->numverts, inVertsArray, inNormalsArray, aliasmesh->stArray, aliasmesh->numtris, aliasmesh->elems, inSVectorsArray );
		}

		if( !calcVerts ) {
			dynamicMesh.xyzArray = aliasmesh->xyzArray;
		}
		dynamicMesh.stArray = aliasmesh->stArray;
		if( !calcNormals ) {
			dynamicMesh.normalsArray = aliasmesh->normalsArray;
		}
		if( !calcSTVectors ) {
			dynamicMesh.sVectorsArray = aliasmesh->sVectorsArray;
		}

		RB_AddDynamicMesh( e, shader, fog, portalSurface, &dynamicMesh, GL_TRIANGLES, 0.0f, 0.0f );

		RB_FlushDynamicMeshes();
	}
}

/*
* R_AliasModelFrameBounds
*/
void R_AliasModelFrameBounds( const model_t *mod, int frame, vec3_t mins, vec3_t maxs ) {
	const maliasframe_t *pframe;
	const maliasmodel_t *aliasmodel = ( const maliasmodel_t * )mod->extradata;

	if( !aliasmodel->nummeshes ) {
		ClearBounds( mins, maxs );
		return;
	}

	if( ( frame >= (int)aliasmodel->numframes ) || ( frame < 0 ) ) {
#ifndef PUBLIC_BUILD
		ri.Com_DPrintf( "R_SkeletalModelFrameBounds %s: no such frame %d\n", mod->name, frame );
#endif
		ClearBounds( mins, maxs );
		return;
	}

	pframe = aliasmodel->frames + frame;
	VectorCopy( pframe->mins, mins );
	VectorCopy( pframe->maxs, maxs );
}

/*
* R_CacheAliasModelEntity
*/
void R_CacheAliasModelEntity( const entity_t *e ) {
	const model_t *mod;
	entSceneCache_t *cache = R_ENTCACHE( e );

	mod = e->model;
	if( !mod || !mod->extradata ) {
		cache->mod_type = mod_bad;
		return;
	}
	if( mod->type != mod_alias ) {
		assert( mod->type == mod_alias );
		return;
	}

	cache->rotated = true;
	cache->radius = R_AliasModelLerpBBox( e, mod, cache->mins, cache->maxs );
	cache->fog = R_FogForSphere( e->origin, cache->radius );
	BoundsFromRadius( e->origin, cache->radius, cache->absmins, cache->absmaxs );
}

/*
* R_AddAliasModelToDrawList
*
* Returns true if the entity is added to draw list
*/
bool R_AddAliasModelToDrawList( const entity_t *e, int lod ) {
	int i, j;
	const model_t *mod = lod < e->model->numlods ? e->model->lods[lod] : e->model;
	const maliasmodel_t *aliasmodel;
	const mfog_t *fog;
	const maliasmesh_t *mesh;
	float distance;
	const entSceneCache_t *cache = R_ENTCACHE( e );
	maliasskin_t fakeskin;

	if( cache->mod_type != mod_alias ) {
		return false;
	}
	if( !( aliasmodel = ( ( const maliasmodel_t * )mod->extradata ) ) || !aliasmodel->nummeshes ) {
		return false;
	}

	// make sure weapon model is always close to the viewer
	distance = 0;
	if( !( e->renderfx & RF_WEAPONMODEL ) ) {
		distance = Distance( e->origin, rn.viewOrigin ) + 1;
	}

	fog = cache->fog;
#if 0
	if( !( e->flags & RF_WEAPONMODEL ) && fog ) {
		R_AliasModelLerpBBox( e, mod );
		if( R_FogCull( fog, e->origin, cache->radius ) ) {
			return false;
		}
	}
#endif

	fakeskin.name[0] = 0;
	fakeskin.shader = NULL;

	for( i = 0, mesh = aliasmodel->meshes; i < aliasmodel->nummeshes; i++, mesh++ ) {
		int numSkins = 1;
		maliasskin_t *skins = &fakeskin;

		if( e->customSkin ) {
			fakeskin.shader = R_FindShaderForSkinFile( e->customSkin, mesh->name );
		} else if( e->customShader ) {
			fakeskin.shader = e->customShader;
		} else if( mesh->numskins ) {
			skins = mesh->skins;
			numSkins = mesh->numskins;
		} else {
			continue;
		}

		for( j = 0; j < numSkins; j++ ) {
			int drawOrder;
			const shader_t *shader = skins[j].shader;
		
			if( !shader ) {
				continue;
			}

			if( rn.renderFlags & RF_LIGHTVIEW ) {
				if( R_ShaderNoDlight( shader ) ) {
					continue;
				}
			}

			drawOrder = R_PackOpaqueOrder( fog, shader, 0, false );
			R_AddSurfToDrawList( rn.meshlist, e, shader, fog, -1,
				MD3SURF_DISTANCE( shader, distance ), drawOrder, NULL, aliasmodel->drawSurfs + i );
		}
	}

	return true;
}
