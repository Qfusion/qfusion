/*
Copyright (C) 2013 Victor Luchits

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

// r_surf.c: surface-related refresh code

#include "r_local.h"

#define WORLDSURF_DIST 1024.0f                  // hack the draw order for world surfaces

static vec3_t modelOrg;                         // relative to view point

#define R_DrawSurfLightsKey(ds) ((ds)->numLightmaps)

//==================================================================================

/*
* R_SurfNoDraw
*/
bool R_SurfNoDraw( const msurface_t *surf ) {
	const shader_t *shader = surf->shader;
	if( surf->flags & SURF_NODRAW ) {
		return true;
	}
	if( !surf->mesh.numVerts || !surf->mesh.numElems ) {
		return true;
	}
	if( !shader ) {
		return true;
	}
	return false;
}

/*
* R_SurfNoDlight
*/
bool R_SurfNoDlight( const msurface_t *surf ) {
	if( surf->flags & (SURF_NODRAW|SURF_SKY|SURF_NODLIGHT) ) {
		return true;
	}
	return R_ShaderNoDlight( surf->shader );
}

/*
* R_FlushBSPSurfBatch
*/
void R_FlushBSPSurfBatch( void ) {
	drawListBatch_t *batch = &rn.meshlist->bspBatch;
	drawSurfaceBSP_t *drawSurf = batch->lastDrawSurf;
	int lightStyleNum = batch->lightStyleNum;
	const entity_t *e = batch->entity;
	const shader_t *shader = batch->shader;
	superLightStyle_t *ls = NULL, *rls = NULL;

	if( batch->count == 0 ) {
		return;
	}

	batch->count = 0;

	if( shader->flags & SHADER_SKY ) {
		return;
	}

	if( lightStyleNum >= 0 ) {
		ls = rls = rsh.worldBrushModel->superLightStyles + lightStyleNum;
	}

	RB_BindShader( e, shader );

	RB_SetPortalSurface( batch->portalSurface );

	RB_SetLightstyle( ls, rls );

	RB_BindVBO( batch->vbo, GL_TRIANGLES );

	RB_SetSurfFlags( drawSurf->surfFlags );

	RB_DrawElements( batch->firstVert, batch->numVerts, batch->firstElem, batch->numElems );
}

/*
* R_BatchBSPSurf
*/
void R_BatchBSPSurf( const entity_t *e, const shader_t *shader,
	int lightStyleNum, const portalSurface_t *portalSurface, drawSurfaceBSP_t *drawSurf, bool mergable ) {
	unsigned i;
	drawListBatch_t *batch = &rn.meshlist->bspBatch;

	if( !mergable ) {
		R_FlushBSPSurfBatch();

		R_TransformForEntity( e );
	}

	if( drawSurf->numInstances ) {
		batch->count = 0;

		RB_DrawElementsInstanced( drawSurf->firstVboVert, drawSurf->numVerts, 
			drawSurf->firstVboElem, drawSurf->numElems, drawSurf->numInstances, drawSurf->instances );
	} else {
		for( i = 0; i < drawSurf->numWorldSurfaces; i++ ) {
			unsigned si = drawSurf->worldSurfaces[i];

			if( rn.meshlist->worldSurfVis[si] ) {
				msurface_t *surf = rsh.worldBrushModel->surfaces + si;
				unsigned surfFirstVert = drawSurf->firstVboVert + surf->firstDrawSurfVert;
				unsigned surfFirstElem = drawSurf->firstVboElem + surf->firstDrawSurfElem;

				if( batch->vbo != drawSurf->vbo || surfFirstElem != batch->firstElem+batch->numElems ) {
					R_FlushBSPSurfBatch();
				}

				if( batch->count == 0 ) {
					batch->vbo = drawSurf->vbo;
					batch->shader = ( shader_t * )shader;
					batch->portalSurface = ( portalSurface_t *)portalSurface;
					batch->entity = ( entity_t * )e;
					batch->lightStyleNum = lightStyleNum;
					batch->lastDrawSurf = drawSurf;
					batch->firstVert = surfFirstVert;
					batch->firstElem = surfFirstElem;
					batch->numVerts = 0;
					batch->numElems = 0;
				}

				batch->count++;
				batch->numVerts += surf->mesh.numVerts;
				batch->numElems += surf->mesh.numElems;
			}
		}
	}
}

/*
* R_WalkBSPSurf
*/
void R_WalkBSPSurf( const entity_t *e, const shader_t *shader, int lightStyleNum,
	drawSurfaceBSP_t *drawSurf, walkDrawSurf_cb_cb cb, void *ptr ) {
	unsigned i;

	for( i = 0; i < drawSurf->numWorldSurfaces; i++ ) {
		int s = drawSurf->worldSurfaces[i];
		msurface_t *surf = rsh.worldBrushModel->surfaces + s;

		assert( rn.meshlist->worldDrawSurfVis[surf->drawSurf - 1] );

		if( rn.meshlist->worldSurfVis[s] ) {
			cb( ptr, e, shader, lightStyleNum, drawSurf, surf );
		}
	}
}

/*
* R_AddWorldDrawSurfaceToDrawList
*/
static bool R_AddWorldDrawSurfaceToDrawList( const entity_t *e, unsigned ds ) {
	drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + ds;
	const shader_t *shader = drawSurf->shader;
	int lightStyleNum = drawSurf->superLightStyle;
	portalSurface_t *portalSurface = NULL;
	bool sky, portal;
	unsigned drawOrder = 0;

	if( !drawSurf->vbo ) {
		return false;
	}
	if( drawSurf->visFrame == rf.frameCount ) {
		return true;
	}

	sky = ( shader->flags & SHADER_SKY ) != 0;
	portal = ( shader->flags & SHADER_PORTAL ) != 0;

	if( sky ) {
		bool writeDepth = mapConfig.writeSkyDepth;

		if( R_FASTSKY() ) {
			return false;
		}

		drawSurf->visFrame = rf.frameCount;

		if( rn.refdef.rdflags & RDF_SKYPORTALINVIEW ) {
			// this will later trigger the skyportal view to be rendered in R_DrawPortals
			portalSurface = R_AddSkyportalSurface( e, shader, drawSurf );
		}

		if( writeDepth ) {
			if( !portalSurface ) {
				// add the sky surface to depth mask
				R_AddSurfToDrawList( rn.portalmasklist, e, rsh.depthOnlyShader, -1, 0, 0, NULL, drawSurf );
			}

			drawSurf->listSurf = R_AddSurfToDrawList( rn.meshlist, e, rsh.depthOnlyShader, 
				lightStyleNum, 0, drawOrder, portalSurface, drawSurf );
		}

		// the actual skydome surface
		R_AddSkySurfToDrawList( rn.meshlist, shader, portalSurface, &rn.skyDrawSurface );

		rf.stats.c_world_draw_surfs++;
		return true;
	}

	if( portal ) {
		portalSurface = R_AddPortalSurface( e, shader, drawSurf );
	}

	drawOrder = R_PackOpaqueOrder( shader, R_DrawSurfLightsKey( drawSurf ), false );

	drawSurf->visFrame = rf.frameCount;
	drawSurf->listSurf = R_AddSurfToDrawList( rn.meshlist, e, shader, lightStyleNum, 
		WORLDSURF_DIST, drawOrder, portalSurface, drawSurf );

	if( !drawSurf->listSurf ) {
		return false;
	}

	if( portalSurface && !( shader->flags & ( SHADER_PORTAL_CAPTURE | SHADER_PORTAL_CAPTURE2 ) ) ) {
		R_AddSurfToDrawList( rn.portalmasklist, e, rsh.depthOnlyShader, -1, 0, 0, NULL, drawSurf );
	}

	rf.stats.c_world_draw_surfs++;
	return true;
}

/*
* R_CheckSpecialWorldSurfaces
*/
static void R_CheckSpecialWorldSurfaces( const entity_t *e, unsigned ds, const vec3_t origin ) {
	unsigned i;
	bool portal;
	drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + ds;
	const shader_t *shader = drawSurf->shader;

	if( !drawSurf->listSurf ) {
		return;
	}

	portal = ( shader->flags & SHADER_PORTAL ) != 0;

	if( portal ) {
		vec3_t centre;
		float dist;
		portalSurface_t *portalSurface = R_GetDrawListSurfPortal( drawSurf->listSurf );

		if( !portalSurface ) {
			return;
		}

		for( i = 0; i < drawSurf->numWorldSurfaces; i++ ) {
			int s = drawSurf->worldSurfaces[i];
			msurface_t *surf = rsh.worldBrushModel->surfaces + s;

			if( !rn.meshlist->worldSurfVis[s] ) {
				continue;			
			}

			if( origin ) {
				VectorCopy( origin, centre );
			} else {
				VectorAdd( surf->mins, surf->maxs, centre );
				VectorScale( centre, 0.5, centre );
			}
			dist = Distance( rn.refdef.vieworg, centre );

			// draw portals in front-to-back order
			dist = 1024 - dist / 100.0f;
			if( dist < 1 ) {
				dist = 1;
			}

			R_UpdatePortalSurface( portalSurface, &surf->mesh, surf->mins, surf->maxs, shader );
		}

		return;
	}
}

/*
=============================================================

BRUSH MODELS

=============================================================
*/

/*
* R_BrushModelBBox
*/
float R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, bool *rotated ) {
	int i;
	const model_t   *model = e->model;

	if( !Matrix3_Compare( e->axis, axis_identity ) ) {
		if( rotated ) {
			*rotated = true;
		}
		for( i = 0; i < 3; i++ ) {
			mins[i] = e->origin[i] - model->radius * e->scale;
			maxs[i] = e->origin[i] + model->radius * e->scale;
		}
	} else {
		if( rotated ) {
			*rotated = false;
		}
		VectorMA( e->origin, e->scale, model->mins, mins );
		VectorMA( e->origin, e->scale, model->maxs, maxs );
	}
	return model->radius * e->scale;
}

#define R_TransformPointToModelSpace( e,rotated,in,out ) \
	VectorSubtract( in, ( e )->origin, out ); \
	if( rotated ) { \
		vec3_t temp; \
		VectorCopy( out, temp ); \
		Matrix3_TransformVector( ( e )->axis, temp, out ); \
	}

/*
* R_CacheBrushModelEntity
*/
void R_CacheBrushModelEntity( const entity_t *e ) {
	const model_t *mod;
	entSceneCache_t *cache = R_ENTCACHE( e );

	mod = e->model;
	if( mod->type != mod_brush ) {
		assert( mod->type == mod_brush );
		return;
	}

	cache->radius = R_BrushModelBBox( e, cache->mins, cache->maxs, &cache->rotated );
	VectorCopy( cache->mins, cache->absmins );
	VectorCopy( cache->maxs, cache->absmaxs );
}

/*
* R_AddBrushModelToDrawList
*/
bool R_AddBrushModelToDrawList( const entity_t *e ) {
	unsigned int i, j;
	vec3_t origin;
	model_t *model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
	unsigned numVisSurfaces;
	const entSceneCache_t *cache = R_ENTCACHE( e );

	if( cache->mod_type != mod_brush ) {
		return false;
	}

	if( bmodel->numModelDrawSurfaces == 0 ) {
		return false;
	}

	VectorAdd( e->model->mins, e->model->maxs, origin );
	VectorMA( e->origin, 0.5, origin, origin );

	R_TransformPointToModelSpace( e, cache->rotated, rn.refdef.vieworg, modelOrg );

	numVisSurfaces = 0;

	for( i = 0; i < bmodel->numModelSurfaces; i++ ) {
		unsigned s = bmodel->firstModelSurface + i;
		msurface_t *surf = rsh.worldBrushModel->surfaces + s;

		if( !surf->drawSurf ) {
			continue;
		}

		if( rn.renderFlags & RF_LIGHTMAP ) {
			if( R_SurfNoDlight( surf ) ) {
				continue;
			}
		}

		rn.meshlist->worldSurfVis[s] = 1;
		rn.meshlist->worldDrawSurfVis[surf->drawSurf - 1] = 1;

		numVisSurfaces++;
	}

	if( !numVisSurfaces ) {
		return false;
	}

	for( i = 0; i < bmodel->numModelDrawSurfaces; i++ ) {
		unsigned ds = bmodel->firstModelDrawSurface + i;

		if( rn.meshlist->worldDrawSurfVis[ds] ) {
			R_AddWorldDrawSurfaceToDrawList( e, ds );

			R_CheckSpecialWorldSurfaces( e, ds, origin );
		}
	}

	for( j = 0; j < 3; j++ ) {
		rn.visMins[j] = min( rn.visMins[j], cache->mins[j] );
		rn.visMaxs[j] = max( rn.visMaxs[j], cache->maxs[j] );
	}

	return true;
}

/*
=============================================================

WORLD MODEL

=============================================================
*/

/*
* R_CullVisLeaves
*/
static void R_CullVisLeaves( unsigned firstLeaf, unsigned numLeaves, unsigned clipFlags ) {
	unsigned i, j;
	mleaf_t *leaf;
	const uint8_t *pvs = rn.pvs;
	const uint8_t *areabits = rn.areabits;

	for( i = 0; i < numLeaves; i++ ) {
		int clipped;
		unsigned bit, testFlags;
		cplane_t *clipplane;
		unsigned l = firstLeaf + i;

		leaf = &rsh.worldBrushModel->leafs[l];
		if( leaf->cluster < 0 || !leaf->numVisSurfaces ) {
			continue;
		}

		// check for door connected areas
		if( areabits ) {
			if( leaf->area < 0 || !( areabits[leaf->area >> 3] & ( 1 << ( leaf->area & 7 ) ) ) ) {
				continue; // not visible
			}
		}

		if( pvs ) {
			if( !( pvs[leaf->cluster >> 3] & ( 1 << ( leaf->cluster & 7 ) ) ) ) {
				continue; // not visible
			}
		}

		// add leaf bounds to pvs bounds
		for( j = 0; j < 3; j++ ) {
			rn.pvsMins[j] = min( rn.pvsMins[j], leaf->mins[j] );
			rn.pvsMaxs[j] = max( rn.pvsMaxs[j], leaf->maxs[j] );
		}

		// track leaves, which are entirely inside the frustum
		clipped = 0;
		testFlags = clipFlags;
		for( j = sizeof( rn.frustum ) / sizeof( rn.frustum[0] ), bit = 1, clipplane = rn.frustum; j > 0; j--, bit <<= 1, clipplane++ ) {
			if( testFlags & bit ) {
				clipped = BoxOnPlaneSide( leaf->mins, leaf->maxs, clipplane );
				if( clipped == 2 ) {
					break;
				} else if( clipped == 1 ) {
					testFlags &= ~bit; // node is entirely on screen
				}
			}
		}

		if( clipped == 2 ) {
			continue; // fully clipped
		}

		if( testFlags == 0 ) {
			// fully visible
			for( j = 0; j < leaf->numVisSurfaces; j++ ) {
				assert( leaf->visSurfaces[j] < rn.meshlist->numWorldSurfVis );
				rn.meshlist->worldSurfFullVis[leaf->visSurfaces[j]] = 1;
			}
		} else {
			// partly visible
			for( j = 0; j < leaf->numVisSurfaces; j++ ) {
				assert( leaf->visSurfaces[j] < rn.meshlist->numWorldSurfVis );
				rn.meshlist->worldSurfVis[leaf->visSurfaces[j]] = 1;
			}
		}

		rn.meshlist->worldLeafVis[l] = 1;
	}
}

/*
* R_CullVisSurfaces
*/
static void R_CullVisSurfaces( unsigned firstSurf, unsigned numSurfs, unsigned clipFlags ) {
	unsigned i;
	unsigned end;

	end = firstSurf + numSurfs;

	for( i = firstSurf; i < end; i++ ) {
		msurface_t *surf = rsh.worldBrushModel->surfaces + i;

		if( !surf->drawSurf ) {
			rn.meshlist->worldSurfVis[i] = 0;
			rn.meshlist->worldSurfFullVis[i] = 0;
			continue;
		}

		if( rn.meshlist->worldSurfVis[i] ) {
			// the surface is at partly visible in at least one leaf, frustum cull it
			if( R_CullBox( surf->mins, surf->maxs, clipFlags ) ) {
				rn.meshlist->worldSurfVis[i] = 0;
			}
			rn.meshlist->worldSurfFullVis[i] = 0;
		}
		else {
			if( rn.meshlist->worldSurfFullVis[i] ) {
				// a fully visible surface, mark as visible
				rn.meshlist->worldSurfVis[i] = 1;
			}
		}

		if( rn.meshlist->worldSurfVis[i] ) {
			rn.meshlist->worldDrawSurfVis[surf->drawSurf - 1] = 1;

			if( surf->flags & SURF_SKY ) {
				R_ClipSkySurface( &rn.skyDrawSurface, surf );
			}

			rf.stats.c_brush_polys++;
		}
	}
}

/*
* R_AddVisSurfaces
*/
static void R_AddVisSurfaces( void ) {
	unsigned i;

	for( i = 0; i < rsh.worldBrushModel->numModelDrawSurfaces; i++ ) {
		if( !rn.meshlist->worldDrawSurfVis[i] ) {
			continue;
		}
		R_AddWorldDrawSurfaceToDrawList( rsc.worldent, i );
	}
}

/*
* R_AddWorldDrawSurfaces
*/
static void R_AddWorldDrawSurfaces( unsigned firstDrawSurf, unsigned numDrawSurfs ) {
	unsigned i;

	for( i = 0; i < numDrawSurfs; i++ ) {
		unsigned ds = firstDrawSurf + i;

		if( !rn.meshlist->worldDrawSurfVis[ds] ) {
			continue;
		}
		R_CheckSpecialWorldSurfaces( rsc.worldent, ds, NULL );
	}
}

/*
* R_GetVisFarClip
*/
static float R_GetVisFarClip( void ) {
	int i;
	float dist;
	vec3_t tmp;
	float farclip_dist;

	farclip_dist = 0;
	for( i = 0; i < 8; i++ ) {
		tmp[0] = ( ( i & 1 ) ? rn.visMins[0] : rn.visMaxs[0] );
		tmp[1] = ( ( i & 2 ) ? rn.visMins[1] : rn.visMaxs[1] );
		tmp[2] = ( ( i & 4 ) ? rn.visMins[2] : rn.visMaxs[2] );

		dist = DistanceSquared( tmp, rn.viewOrigin );
		farclip_dist = max( farclip_dist, dist );
	}

	return sqrt( farclip_dist );
}

/*
* R_PostCullVisLeaves
*/
static void R_PostCullVisLeaves( void ) {
	unsigned i, j;
	mleaf_t *leaf;
	float farclip;

	for( i = 0; i < rsh.worldBrushModel->numleafs; i++ ) {
		if( !rn.meshlist->worldLeafVis[i] ) {
			continue;
		}

		rf.stats.c_world_leafs++;

		leaf = &rsh.worldBrushModel->leafs[i];
		if( r_leafvis->integer && !( rn.renderFlags & RF_NONVIEWERREF ) ) {
			R_AddDebugBounds( leaf->mins, leaf->maxs, colorRed );
		}

		// add leaf bounds to view bounds
		for( j = 0; j < 3; j++ ) {
			rn.visMins[j] = min( rn.visMins[j], leaf->mins[j] );
			rn.visMaxs[j] = max( rn.visMaxs[j], leaf->maxs[j] );
		}
	}

	// now set  the real far clip value and reload view matrices
	farclip = R_GetVisFarClip();

	rn.farClip = max( Z_NEAR, farclip ) + Z_BIAS;

	R_SetupViewMatrices( &rn.refdef );
}

/*
* R_DrawWorldNode
*/
void R_DrawWorldNode( void ) {
	int clipFlags = r_nocull->integer ? 0 : rn.clipFlags;
	int64_t msec = 0, msec2 = 0;
	bool worldOutlines;
	bool speeds = r_speeds->integer != 0;
	mbrushmodel_t *bm = rsh.worldBrushModel;

	R_ReserveDrawListWorldSurfaces( rn.meshlist );

	if( !r_drawworld->integer || !bm ) {
		return;
	}
	if( rn.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	VectorCopy( rn.refdef.vieworg, modelOrg );

	worldOutlines = mapConfig.forceWorldOutlines || ( rn.refdef.rdflags & RDF_WORLDOUTLINES );

	if( worldOutlines && ( rn.viewcluster != -1 ) && r_outlines_scale->value > 0 ) {
		rsc.worldent->outlineHeight = max( 0.0f, r_outlines_world->value );
	} else {
		rsc.worldent->outlineHeight = 0;
	}
	Vector4Copy( mapConfig.outlineColor, rsc.worldent->outlineColor );

	// BEGIN t_world_node
	if( speeds ) {
		msec = ri.Sys_Milliseconds();
	}

	VectorCopy( rn.viewOrigin, rn.visMins );
	VectorCopy( rn.viewOrigin, rn.visMaxs );

	VectorCopy( rn.viewOrigin, rn.pvsMins );
	VectorCopy( rn.viewOrigin, rn.pvsMaxs );

	//
	// cull leafs
	//
	if( r_speeds->integer ) {
		msec2 = ri.Sys_Milliseconds();
	}

	if( bm->numleafs <= bm->numsurfaces ) {
		R_CullVisLeaves( 0, bm->numleafs, clipFlags );
	} else {
		memset( (void *)rn.meshlist->worldSurfVis, 1, bm->numsurfaces * sizeof( *rn.meshlist->worldSurfVis ) );
		memset( (void *)rn.meshlist->worldSurfFullVis, 0, bm->numsurfaces * sizeof( *rn.meshlist->worldSurfFullVis ) );
		memset( (void *)rn.meshlist->worldLeafVis, 1, bm->numleafs * sizeof( *rn.meshlist->worldLeafVis ) );
		memset( (void *)rn.meshlist->worldDrawSurfVis, 0, bm->numDrawSurfaces * sizeof( *rn.meshlist->worldDrawSurfVis ) );
	}

	if( speeds ) {
		rf.stats.t_cull_world_nodes += ri.Sys_Milliseconds() - msec2;
	}

	//
	// cull surfaces and do some background work on computed vis leafs
	// 
	if( speeds ) {
		msec2 = ri.Sys_Milliseconds();
	}

	R_CullVisSurfaces( 0, bm->numModelSurfaces, clipFlags );

	R_PostCullVisLeaves();

	if( speeds ) {
		rf.stats.t_cull_world_surfs += ri.Sys_Milliseconds() - msec2;
	}

	//
	// add visible surfaces to draw list
	//
	if( speeds ) {
		msec2 = ri.Sys_Milliseconds();
	}

	R_AddVisSurfaces();

	R_AddWorldDrawSurfaces( 0, bm->numModelDrawSurfaces );

	// END t_world_node
	if( speeds ) {
		rf.stats.t_world_node += ri.Sys_Milliseconds() - msec;
	}
}
