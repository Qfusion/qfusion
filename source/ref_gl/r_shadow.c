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
	unsigned shaderId;

	int firstVert, lastVert;
	int numElems;
	int numInstances;
	int numWorldSurfaces;

	drawSurfaceCompiledLight_t drawSurf;

	mesh_vbo_t *vbo;
	mesh_vbo_t *elemsVbo;

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
	if( sky || portal || !Shader_DepthWrite( shader ) ) {
		return NULL;
	}

	// use a simplistic dummy opaque shader if we can
	if( shader->sort == SHADER_SORT_OPAQUE && !shader->numdeforms && Shader_CullFront( shader ) ) {
		return rsh.envShader;
	}
	return shader;
}

/*
* R_DrawCompiledLightSurf
*/
void R_DrawCompiledLightSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, 
	const portalSurface_t *portalSurface, drawSurfaceCompiledLight_t *drawSurf ) {
	RB_BindVBO( drawSurf->vbo->index, GL_TRIANGLES );

	RB_SetLightstyle( NULL );

	RB_SetRtLightParams( 0, NULL, 0, NULL );

	if( drawSurf->numInstances ) {
		RB_DrawElementsInstanced( 0, drawSurf->vbo->vertsVbo->numVerts, 
			0, drawSurf->numElems,	drawSurf->numInstances, drawSurf->instances );
		return;
	}

	RB_DrawElements( 0, drawSurf->vbo->vertsVbo->numVerts, 0, drawSurf->numElems );
}

/*
* R_BatchLightSideView
*/
static void R_BatchLightSideView( shadowSurfBatch_t *batch, const entity_t *e, const shader_t *shader,
	drawSurfaceBSP_t *drawSurf, msurface_t *surf ) {
	mesh_vbo_t *vbo;
	int vertsOffset;
	int firstVert, lastVert;
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
	firstVert = drawSurf->firstVboVert + surf->firstDrawSurfVert;
	lastVert = firstVert + numVerts - 1;
	vertsOffset = drawSurf->firstVboVert + surf->firstDrawSurfVert;

	tail = batch->tail;

	if( tail->vbo != vbo || tail->shaderId != shader->id || tail->numInstances != 0 || numInstances != 0 ) {
		if( !tail->next ) {
			tail->next = Mod_Malloc( rsh.worldModel, sizeof( shadowSurfBatch_t ) );
			tail->next->prev = tail;
		}
		batch->tail = tail->next;
		tail = tail->next;

		if( tail->elemsVbo ) {
			if( numInstances ) {
				tail->elemsVbo = vbo;
				return;
			}

			R_UploadVBOElemData( tail->elemsVbo, vertsOffset, 0, &surf->mesh );
			tail->numElems = numElems;
			return;
		}

		tail->vbo = vbo;
		tail->shaderId = shader->id;
		tail->firstVert = firstVert;
		tail->lastVert = lastVert;
		tail->numElems = numElems;
		tail->numInstances = numInstances;
		tail->instances = instances;
		tail->numWorldSurfaces = 1;
		return;
	}

	if( tail->elemsVbo ) {
		R_UploadVBOElemData( tail->elemsVbo, vertsOffset, tail->numElems, &surf->mesh );
		tail->numElems += numElems;
		return;
	}

	if( firstVert < tail->firstVert ) {
		tail->firstVert = firstVert;
	}
	if( lastVert > tail->lastVert ) {
		tail->lastVert = lastVert;
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

		if( p->numInstances )
			p->elemsVbo = p->vbo;
		else
			p->elemsVbo = R_CreateElemsVBO( l, p->vbo, p->numElems, VBO_TAG_WORLD );

		p->drawSurf.type = ST_COMPILED_LIGHT;
		p->drawSurf.firstVert = p->firstVert;
		p->drawSurf.numVerts = p->lastVert - p->firstVert + 1;
		p->drawSurf.firstElem = 0;
		p->drawSurf.numElems = p->numElems;
		p->drawSurf.vbo = p->elemsVbo;
	}
	
	if( !head.next ) {
		// create a stub batch so that the early exit check above won't fail
		head.next = Mod_Malloc( rsh.worldModel, sizeof( shadowSurfBatch_t ) );
	} else {
		// walk the list again, now uploading elems to newly created VBO's
		head.tail = &head;
		R_WalkDrawList( &r_shadowlist, R_BatchLightSideView, &head );
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
			R_TouchMeshVBO( b->elemsVbo );

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
		R_DrawWorld();
		return;
	}

	for( b = l->compiledSurf[side]; b && b->shaderId; b = b->next ) {
		R_AddSurfToDrawList( rn.meshlist, rsc.worldent, NULL, R_ShaderById( b->shaderId ), 
			0, 0, NULL, &b->drawSurf );
	}
}

/*
* R_DrawRtLightShadow
*/
void R_DrawRtLightShadow( rtlight_t *l, image_t *target, int sideMask, bool compile ) {
	int side;
	refdef_t *fd;
	int x, y, size, border;
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
	rnp->portalmasklist = NULL;
	rnp->lodBias = 0;
	rnp->lodScale = 1;
	VectorCopy( l->origin, rnp->lodOrigin );
	VectorCopy( l->origin, rnp->pvsOrigin );

	fd = &rnp->refdef;
	fd->rdflags = 0;
	fd->fov_x = fd->fov_y = RAD2DEG( 2 * atan2( size, ((float)size - border) ) );
	fd->width = size;
	fd->height = size;
	VectorCopy( l->origin, fd->vieworg );
	Matrix3_Copy( axis_identity, fd->viewaxis );

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

		fd->x = x + (side & 1) * size;
		fd->y = y + (side >> 1) * size;
		Vector4Set( rnp->viewport, fd->x, -fd->y + target->upload_height - fd->height, fd->width, fd->height );
		Vector4Set( rnp->scissor, fd->x, -fd->y + target->upload_height - fd->height, fd->width, fd->height );

		R_SetupPVSFromCluster( l->cluster, l->area );

		R_SetupSideViewMatrices( fd, side );

		R_SetupSideViewFrustum( fd, rnp->nearClip, rnp->farClip, rnp->frustum, side );

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

	R_PushRefInst();

	R_DrawRtLightShadow( l, atlas, 0x3F, true );

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
	float scale = (size - 2*border) / size, len;
	const vec_t *o = l->origin;

	// check if cone enclosing side would cross frustum plane
	scale = 2 / ( scale * scale + 2 );

	for( i = 0; i < 5; i++ ) {
		const vec_t *n = r->frustum[i].normal;
		if( PlaneDiff( o, &r->frustum[i] ) > -ON_EPSILON ) {
			continue;
		}
		len = scale;
		if( n[0]*n[0] > len ) sides &= n[0] < 0 ? ~(1<<0) : ~(2 << 0);
		if( n[1]*n[1] > len ) sides &= n[1] < 0 ? ~(1<<2) : ~(2 << 2);
		if( n[2]*n[2] > len ) sides &= n[2] < 0 ? ~(1<<4) : ~(2 << 4);
	}

	if( PlaneDiff( o, &r->frustum[4] ) >= r->farClip - r->nearClip + ON_EPSILON ) {
		const vec_t *n = r->frustum[4].normal;
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
	refdef_t refdef;
	unsigned numRtLights;
	rtlight_t **rtLights;
	vec3_t viewOrigin;
	int border = max( r_shadows_bordersize->integer, SHADOWMAP_MIN_BORDER );
	int minsize = max( r_shadows_minsize->integer, SHADOWMAP_MIN_SIZE );
	int maxsize = max( min( r_shadows_maxsize->integer, r_shadows_texturesize->integer ), minsize );
	refinst_t *prevrn;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return;
	}

	if( !r_lighting_realtime_world->integer && !r_lighting_realtime_dlight->integer ) {
		return;
	}
	if( !r_lighting_realtime_world_shadows->integer && !r_lighting_realtime_dlight_shadows->integer ) {
		return;
	}

	refdef = rn.refdef;
	rtLights = rn.rtlights;
	numRtLights = rn.numRealtimeLights;
	VectorCopy( rn.viewOrigin, viewOrigin );

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
		if( !l->receiveMask ) {
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
		size = bound( minsize, size & ~1, maxsize );

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

		sideMask = R_CullRtLightFrumSides( prevrn, l, size, border );
		sideMask &= l->receiveMask;
		if( !sideMask ) {
			continue;
		}

		l->shadowSize = size;
		l->shadowBorder = border;
		l->shadowOffset[0] = x;
		l->shadowOffset[1] = y;

		R_DrawRtLightShadow( l, atlas, sideMask, true );
	}

	R_PopRefInst();
}
