/*
Copyright (C) 2017 Victor Luchits

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

#include "r_local.h"

/*
=============================================================

OMNIDIRECTIONAL SHADOW MAPS

=============================================================
*/

typedef struct shadowSurfBatch_s {
	int pass;
	unsigned shaderId;

	int firstElem;
	int firstVert, lastVert;
	int numElems;
	int numInstances;
	int numWorldSurfaces;

	int vbo;
	int elemsVbo;
	elem_t *elemsBuffer;
	void *cacheMark;

	drawSurfaceCompiledLight_t drawSurf;

	rtlight_t *light;
	instancePoint_t *instances;

	struct shadowSurfBatch_s *prev, *next, *tail;
} shadowSurfBatch_t;

static lightmapAllocState_t shadowAtlasAlloc;

/*
* R_OpaqueShadowShader
*/
const shader_t *R_OpaqueShadowShader( const shader_t *shader ) {
	bool sky, portal;

	sky = ( shader->flags & SHADER_SKY ) != 0;
	portal = ( shader->flags & SHADER_PORTAL ) != 0;

	if( portal ) {
		return NULL;
	}
	if( sky ) {
		if( !mapConfig.writeSkyDepth ) {
			return NULL;
		}
	} else {
		if( !Shader_DepthWrite( shader ) ) {
			return NULL;
		}
	}

	// use a simplistic dummy opaque shader if we can
	if( ( shader->sort <= SHADER_SORT_SKY ) && !shader->numdeforms && Shader_CullFront( shader ) ) {
		return rsh.envShader;
	}
	return shader;
}

/*
* R_DrawCompiledLightSurf
*/
void R_DrawCompiledLightSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, 
	int lightStyleNum, const portalSurface_t *portalSurface, drawSurfaceCompiledLight_t *drawSurf ) {
	if( !drawSurf->numElems ) {
		return;
	}

	RB_BindVBO( drawSurf->vbo, GL_TRIANGLES );

	if( drawSurf->numInstances ) {
		RB_DrawElementsInstanced( drawSurf->firstVert, drawSurf->numVerts, 
			drawSurf->firstElem, drawSurf->numElems, drawSurf->numInstances, drawSurf->instances );
		return;
	}

	RB_DrawElements( drawSurf->firstVert, drawSurf->numVerts, drawSurf->firstElem, drawSurf->numElems );
}

/*
* R_BatchShadowSurfElems
*/
static void R_BatchShadowSurfElems( shadowSurfBatch_t *batch, int vertsOffset, const msurface_t *surf ) {
	elem_t *oe = batch->elemsBuffer + batch->numElems;
	const rtlight_t *l = batch->light;
	bool cull = r_shadows_culltriangles->integer != 0;

	batch->numElems += R_CullRtLightSurfaceTriangles( l, surf, cull, vertsOffset, oe, &batch->firstVert, &batch->lastVert );
}

/*
* R_UpdateBatchShadowDrawSurf
*/
static void R_UpdateBatchShadowDrawSurf( shadowSurfBatch_t *batch ) {
	drawSurfaceCompiledLight_t *drawSurf;

	drawSurf = &batch->drawSurf;
	drawSurf->type = ST_COMPILED_LIGHT;
	drawSurf->firstVert = batch->firstVert;
	drawSurf->numVerts = batch->lastVert - batch->firstVert + 1;
	drawSurf->firstElem = batch->firstElem;
	drawSurf->numElems = batch->numElems;
	drawSurf->vbo = batch->elemsVbo;
	drawSurf->numInstances = batch->numInstances;
}

/*
* R_UploadBatchShadowElems
*/
static void R_UploadBatchShadowElems( shadowSurfBatch_t *batch ) {
	if( batch->numElems ) {
		mesh_t mesh;
		mesh_vbo_t *elemsVbo;

		memset( &mesh, 0, sizeof( mesh_t ) );
		mesh.numElems = batch->numElems;
		mesh.elems = batch->elemsBuffer;

		elemsVbo = R_CreateElemsVBO( batch->light, R_GetVBOByIndex( batch->vbo ), batch->numElems, VBO_TAG_WORLD );
		R_UploadVBOElemData( elemsVbo, 0, 0, &mesh );

		batch->elemsVbo = elemsVbo->index;
	}

	R_UpdateBatchShadowDrawSurf( batch );

	R_FrameCache_FreeToMark( batch->cacheMark );

	batch->elemsBuffer = NULL;
	batch->cacheMark = NULL;
}

/*
* R_BatchLightSideView
*/
static void R_BatchLightSideView( shadowSurfBatch_t *batch, const entity_t *e, const shader_t *shader,
	int lightStyleNum, drawSurfaceBSP_t *drawSurf, msurface_t *surf ) {
	int vbo;
	int vertsOffset;
	int numVerts, numElems;
	int numInstances;
	instancePoint_t *instances;
	shadowSurfBatch_t *tail;

	if( e != rsc.worldent ) {
		return;
	}

	vbo = drawSurf->vbo;
	numVerts = surf->mesh.numVerts;
	numElems = surf->mesh.numElems;
	numInstances = drawSurf->numInstances;
	instances = drawSurf->instances;
	vertsOffset = drawSurf->firstVboVert + surf->firstDrawSurfVert;

	tail = batch->tail;

	if( tail->vbo != vbo || tail->shaderId != shader->id || 
		tail->numInstances != 0 || numInstances != 0 || 
		tail->numElems + numElems > UINT16_MAX ) {
		if( tail->pass ) {
			R_UploadBatchShadowElems( tail );
		}

		if( !tail->next ) {
			tail->next = Mod_Malloc( rsh.worldModel, sizeof( shadowSurfBatch_t ) );
			tail->next->prev = tail;
		}
		batch->tail = tail->next;
		tail = tail->next;

		if( tail->pass ) {
			if( numInstances ) {
				tail->elemsVbo = tail->vbo;
				tail->numElems = numElems;
				tail->firstVert = drawSurf->firstVboVert + surf->firstDrawSurfVert;
				tail->lastVert = tail->firstVert + numVerts - 1;
				tail->firstElem = drawSurf->firstVboElem + surf->firstDrawSurfElem;
				R_UpdateBatchShadowDrawSurf( tail );
				return;
			}

			tail->cacheMark = R_FrameCache_SetMark();
			tail->elemsBuffer = R_FrameCache_Alloc( sizeof( elem_t ) * tail->numElems );
			tail->numElems = 0;
			tail->firstVert = UINT16_MAX;
			tail->lastVert = 0;
			R_BatchShadowSurfElems( tail, vertsOffset, surf );
			return;
		}

		tail->light = rn.rtLight;
		tail->vbo = vbo;
		tail->shaderId = shader->id;
		tail->numElems = numElems;
		tail->numInstances = numInstances;
		tail->instances = instances;
		tail->numWorldSurfaces = 1;
		return;
	}

	if( tail->pass ) {
		R_BatchShadowSurfElems( tail, vertsOffset, surf );
		return;
	}

	tail->numElems += numElems;
	tail->numWorldSurfaces++;
}

/*
* R_CompileLightSideView
*/
static void R_CompileLightSideView( rtlight_t *l, int side ) {
	shadowSurfBatch_t head;
	shadowSurfBatch_t *p, *next = NULL;

	if( l->compiledSurf[side] ) {
		return;
	}

	memset( &head, 0, sizeof( head ) );
	head.tail = &head;

	// walk the sorted list, batching BSP geometry
	R_WalkDrawList( &r_shadowlist, R_BatchLightSideView, &head );

	for( p = head.next; p && p != &head; p = next ) {
		next = p->next;
		p->pass++;
	}

	if( !head.next ) {
		// create a stub batch so that the early exit check above won't fail
		head.next = Mod_Malloc( rsh.worldModel, sizeof( shadowSurfBatch_t ) );
	} else {
		// walk the list again, now uploading elems to newly created VBO's
		head.tail = &head;
		R_WalkDrawList( &r_shadowlist, R_BatchLightSideView, &head );
		R_UploadBatchShadowElems( head.tail );
	}

	l->compiledSurf[side] = head.next;
}

/*
* R_TouchCompiledRtLightShadows
*/
void R_TouchCompiledRtLightShadows( rtlight_t *l ) {
	int side;
	shadowSurfBatch_t *b;

	for( side = 0; side < 6; side++ ) {
		if( !l->compiledSurf[side] ) {
			continue;
		}

		for( b = l->compiledSurf[side]; b && b->shaderId; b = b->next ) {
			R_TouchMeshVBO( R_GetVBOByIndex( b->elemsVbo ) );

			R_TouchShader( R_ShaderById( b->shaderId ) );
		}
	}
}

/*
* R_DrawRtLightWorld
*/
void R_DrawRtLightWorld( void ) {
	int side;
	rtlight_t *l;
	shadowSurfBatch_t *b;

	l = rn.rtLight;
	side = rn.rtLightSide;

	assert( l != NULL );
	if( !l ) {
		return;
	}

	if( !l->compiledSurf[side] || !r_shadows_usecompiled->integer ) {
		R_DrawWorldShadowNode();
		return;
	}

	for( b = l->compiledSurf[side]; b && b->shaderId; b = b->next ) {
		R_AddSurfToDrawList( rn.meshlist, rsc.worldent, R_ShaderById( b->shaderId ), NULL, 
			-1, 0, 0, NULL, &b->drawSurf );
	}
}

/*
* R_DrawRtLightShadow
*/
static void R_DrawRtLightShadow( rtlight_t *l, image_t *target, int sideMask, bool compile, bool novis, refinst_t *prevrn ) {
	int x, y, size, border;
	int side;
	refdef_t *fd;
	refinst_t *rnp = &rn;

	if( !l->shadow ) {
		return;
	}

	size = l->shadowSize;
	border = l->shadowBorder;
	x = l->shadowOffset[0];
	y = l->shadowOffset[1];

	rnp->renderTarget = target->fbo;
	rnp->nearClip = r_shadows_nearclip->value;
	rnp->farClip = l->intensity;
	rnp->clipFlags = 63; // clip by near and far planes too
	rnp->polygonFactor = r_shadows_polygonoffset_factor->value;
	rnp->polygonUnits = r_shadows_polygonoffset_units->value;
	rnp->meshlist = &r_shadowlist;
	rnp->parent = prevrn;
	rnp->portalmasklist = NULL;
	rnp->lodBias = r_shadows_lodbias->integer;
	rnp->lodScale = 1;
	rnp->numDepthPortalSurfaces = 0;
	rnp->numDeformedFrustumPlanes = 0;
	rnp->numRtLightEntities = l->numShadowEnts;
	rnp->rtLightEntities = l->shadowEnts;
	rnp->rtLightSurfaceInfo = l->surfaceInfo;
	rnp->numRtLightVisLeafs = l->numVisLeafs;
	rnp->rtLightVisLeafs = l->visLeafs;
	VectorCopy( l->origin, rnp->lodOrigin );
	VectorCopy( l->origin, rnp->pvsOrigin );

	fd = &rnp->refdef;
	fd->rdflags = 0;
	fd->fov_x = fd->fov_y = RAD2DEG( 2 * atan2( size, ((float)size - border) ) );
	fd->width = size;
	fd->height = size;
	VectorCopy( l->origin, fd->vieworg );
	Matrix3_Copy( l->axis, fd->viewaxis );

	// ignore current frame's area vis when compiling shadow geometry
	if( compile ) {
		fd->areabits = NULL;
	}

	if( prevrn != NULL && !compile ) {
		unsigned i;

		// generate a deformed frustum that includes the light origin, this is
		// used to cull shadow casting surfaces that can not possibly cast a
		// shadow onto the visible light-receiving surfaces, which can be a
		// performance gain
		rnp->numDeformedFrustumPlanes = R_DeformFrustum( prevrn->frustum, prevrn->frustumCorners, 
			prevrn->viewOrigin, l->origin, rnp->deformedFrustum  );

		// cull entities by the deformed frustum
		rnp->numRtLightEntities = 0;
		rnp->rtLightEntities = R_FrameCache_Alloc( sizeof( *(rnp->rtLightEntities) ) * l->numShadowEnts );

		for( i = 0; i < l->numShadowEnts; i++ ) {
			int entNum = l->shadowEnts[i];
			entSceneCache_t *cache = R_ENTNUMCACHE( entNum );
			if( !R_DeformedCullBox( cache->absmins, cache->absmaxs ) ) {
				rnp->rtLightEntities[rnp->numRtLightEntities++] = entNum;
			}
		}

		rnp->numRtLightVisLeafs = 0;
		rnp->rtLightVisLeafs = R_FrameCache_Alloc( sizeof( *(rnp->rtLightVisLeafs) ) * l->numVisLeafs );

		for( i = 0; i < l->numVisLeafs; i++ ) {
			int leafNum = l->visLeafs[i];
			const mleaf_t *leaf = rsh.worldBrushModel->leafs + leafNum;
			if( !R_DeformedCullBox( leaf->mins, leaf->maxs ) ) {
				rnp->rtLightVisLeafs[rnp->numRtLightVisLeafs++] = leafNum;
			}
		}

#if 0
		if( 0 ) {
			unsigned j;
			unsigned dscount, scount;
			unsigned *pin, *pout;

			pin = l->surfaceInfo;
			dscount = *pin;
			scount = l->numSurfaces;

			pout = R_FrameCache_Alloc( sizeof( unsigned ) * (1 + dscount*2 + scount*3) );
			rnp->rtLightSurfaceInfo = pout;

			*pout = *pin;
			pout++, pin++;

			for( i = 0; i < dscount; i++ ) {
				unsigned ds = *pin++;
				unsigned numSurfaces = *pin++;

				*pout++ = ds;
				*pout++ = numSurfaces;

				for( j = 0; j < numSurfaces; j++, pin += 3, pout += 3 ) {
					const msurface_t *surf = rsh.worldBrushModel->surfaces + pin[0];

					pout[0] = pin[0], pout[1] = pin[1];
					if( !R_DeformedCullBox( surf->mins, surf->maxs ) ) {
						pout[2] = pin[2];
					} else {
						pout[2] = 0;
					}
				}
			}
		}
#endif
	}

	for( side = 0; side < 6; side++ ) {
		if( !(sideMask & (1<<side)) ) {
			continue;
		}

		rnp->rtLight = l;
		rnp->rtLightSide = side;

		rnp->renderFlags = RF_SHADOWMAPVIEW;
		if( (side & 1) ^ (side >> 2) ) {
			rnp->renderFlags |= RF_FLIPFRONTFACE;
		}
		if( !( target->flags & IT_DEPTH ) ) {
			rnp->renderFlags |= RF_SHADOWMAPVIEW_RGB;
		}
		if( novis ) {
			rnp->renderFlags |= RF_NOVIS;
		}
		if( compile ) {
			rnp->renderFlags |= RF_NOENTS;
		}

		fd->x = x + (side & 1) * size;
		fd->y = y + (side >> 1) * size;
		Vector4Set( rnp->viewport, fd->x, -fd->y + target->upload_height - fd->height, fd->width, fd->height );
		Vector4Set( rnp->scissor, fd->x, -fd->y + target->upload_height - fd->height, fd->width, fd->height );

		R_SetupPVSFromCluster( l->cluster, l->area );

		R_SetupSideViewMatrices( fd, side );

		R_SetupSideViewFrustum( fd, side, rnp->nearClip, rnp->farClip, rnp->frustum, rn.frustumCorners );

		R_RenderView( fd );

		if( compile && l->world ) {
			R_CompileLightSideView( l, side );
		}
	}
}

/*
* R_CompileRtLightShadow
*/
void R_CompileRtLightShadow( rtlight_t *l ) {
	image_t *atlas;
	refinst_t *prevrn;

	if( !l->world || !l->shadow ) {
		return;
	}
	if( !r_lighting_realtime_world->integer || !r_lighting_realtime_world_shadows->integer ) {
		return;
	}

	if( l->compiledSurf[0] ) {
		return;
	}

	atlas = R_GetShadowmapAtlasTexture();
	if( !atlas || !atlas->fbo ) {
		return;
	}

	l->shadowSize = SHADOWMAP_MIN_SIZE;
	l->shadowBorder = SHADOWMAP_MIN_BORDER;
	l->shadowOffset[0] = 0;
	l->shadowOffset[1] = 0;

	prevrn = R_PushRefInst();

	R_DrawRtLightShadow( l, atlas, 0x3F, true, false, prevrn );

	R_PopRefInst();
}

/*
* R_CullRtLightFrumSides
*
* Based on R_Shadow_CullFrustumSides from Darkplaces, by Forest "LordHavoc" Hale
*/
static int R_CullRtLightFrumSides( const refinst_t *r, const rtlight_t *l, float size, float border ) {
	int i;
	int sides = 0x3F;
	vec3_t n;
	float scale = (size - 2*border) / size, len;

	// check if cone enclosing side would cross frustum plane
	scale = 2 / ( scale * scale + 2 );

	for( i = 0; i < 5; i++ ) {
		Matrix4_Multiply_Vector3( l->worldToLightMatrix, r->frustum[i].normal, n );
		if( PlaneDiff( l->origin, &r->frustum[i] ) > -ON_EPSILON ) {
			continue;
		}

		len = scale;
		if( n[0]*n[0] > len ) sides &= n[0] < 0 ? ~(1<<0) : ~(2 << 0);
		if( n[1]*n[1] > len ) sides &= n[1] < 0 ? ~(1<<2) : ~(2 << 2);
		if( n[2]*n[2] > len ) sides &= n[2] < 0 ? ~(1<<4) : ~(2 << 4);
	}

	if( PlaneDiff( l->origin, &r->frustum[4] ) >= r->farClip - r->nearClip + ON_EPSILON ) {
		Matrix4_Multiply_Vector3( l->worldToLightMatrix, r->frustum[4].normal, n );

		len = scale;
		if( n[0]*n[0] > len ) sides &= n[0] >= 0 ? ~(1<<0) : ~(2 << 0);
		if( n[1]*n[1] > len ) sides &= n[1] >= 0 ? ~(1<<2) : ~(2 << 2);
		if( n[2]*n[2] > len ) sides &= n[2] >= 0 ? ~(1<<4) : ~(2 << 4);
	}

	return sides;
}

/*
* R_DrawShadows
*/
void R_DrawShadows( void ) {
	unsigned i;
	rtlight_t *l;
	image_t *atlas;
	lightmapAllocState_t *salloc = &shadowAtlasAlloc;
	unsigned numRtLights;
	rtlight_t **rtLights;
	int border = max( r_shadows_bordersize->integer, SHADOWMAP_MIN_BORDER );
	int minsize = max( r_shadows_minsize->integer, SHADOWMAP_MIN_SIZE );
	int maxsize = bound( minsize + 2, r_shadows_maxsize->integer, r_shadows_texturesize->integer / 8 );
	refinst_t *prevrn;

	if( rn.renderFlags & (RF_LIGHTVIEW|RF_SHADOWMAPVIEW) ) {
		return;
	}

	if( !r_lighting_realtime_world->integer && !r_lighting_realtime_dlight->integer ) {
		return;
	}
	if( !r_lighting_realtime_world_shadows->integer && !r_lighting_realtime_dlight_shadows->integer ) {
		return;
	}

	rtLights = rn.rtlights;
	numRtLights = rn.numRealtimeLights;

	if( !numRtLights ) {
		return;
	}

	atlas = R_GetShadowmapAtlasTexture();
	if( !atlas || !atlas->fbo ) {
		return;
	}

	if( atlas->upload_width != salloc->width || atlas->upload_height != salloc->height ) {
		R_AllocLightmap_Free( salloc );
	}

	if( salloc->width ) {
		R_AllocLightmap_Reset( salloc );
	} else {
		R_AllocLightmap_Init( salloc, atlas->upload_width, atlas->upload_height );
	}

	prevrn = R_PushRefInst();
	if( !prevrn ) {
		return;
	}

	for( i = 0; i < numRtLights; i++ ) {
		int x, y;
		int size, width, height;
		int sideMask;
		bool haveBlock;

		l = rtLights[i];

		if( !l->shadow ) {
			continue;
		}

		sideMask = l->casterMask;
		sideMask &= l->receiverMask;
		if( !sideMask ) {
			continue;
		}

		if( l->world ) {
			if( !r_lighting_realtime_world_shadows->integer ) {
				continue;
			}
		} else {
			if( !r_lighting_realtime_dlight_shadows->integer ) {
				continue;
			}
		}

		size = l->lod;
		size = l->intensity * r_shadows_precision->value / (size + 1.0);
		size = bound( minsize, size, maxsize );

		x = y = 0;
		haveBlock = false;
		while( !haveBlock && size >= minsize ) {
			width = size * 2;
			height = size * 3;

			haveBlock = R_AllocLightmap_Block( salloc, width, height, &x, &y );
			if( haveBlock ) {
				break;
			}
			size >>= 1;
		}

		if( !haveBlock ) {
			continue;
		}

		sideMask &= R_CullRtLightFrumSides( prevrn, l, size, border );
		if( !sideMask ) {
			continue;
		}

		l->shadowSize = size;
		l->shadowBorder = border;
		l->shadowOffset[0] = x;
		l->shadowOffset[1] = y;

		R_DrawRtLightShadow( l, atlas, sideMask, false, false, prevrn );
	}

	R_PopRefInst();
}
