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

//==================================================================================

/*
* R_SurfPotentiallyVisible
*/
bool R_SurfPotentiallyVisible( const msurface_t *surf ) {
	const shader_t *shader = surf->shader;
	if( surf->flags & SURF_NODRAW ) {
		return false;
	}
	if( !surf->mesh.numVerts ) {
		return false;
	}
	if( !shader ) {
		return false;
	}
	return true;
}

/*
* R_SurfPotentiallyShadowed
*/
bool R_SurfPotentiallyShadowed( const msurface_t *surf ) {
	if( surf->flags & ( SURF_SKY | SURF_NODLIGHT | SURF_NODRAW ) ) {
		return false;
	}
	if( ( surf->shader->sort >= SHADER_SORT_OPAQUE ) && ( surf->shader->sort <= SHADER_SORT_ALPHATEST ) ) {
		return true;
	}
	return false;
}

/*
* R_SurfPotentiallyLit
*/
bool R_SurfPotentiallyLit( const msurface_t *surf ) {
	const shader_t *shader;

	if( surf->flags & ( SURF_SKY | SURF_NODLIGHT | SURF_NODRAW ) ) {
		return false;
	}
	shader = surf->shader;
	if( ( shader->flags & SHADER_SKY ) || !shader->numpasses ) {
		return false;
	}
	return ( surf->mesh.numVerts != 0 /* && (surf->facetype != FACETYPE_TRISURF)*/ );
}

/*
* R_CullSurface
*/
bool R_CullSurface( const entity_t *e, const msurface_t *surf, unsigned int clipflags ) {
	return ( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ) );
}

/*
* R_SurfaceClipRtLight
*/
static bool R_SurfaceClipRtLight( const msurface_t *surf, const rtlight_t *lt ) {
	float dist;

	if( !R_SurfPotentiallyLit( surf ) ) {
		return false;
	}

	switch( surf->facetype ) {
		case FACETYPE_PLANAR:
			dist = DotProduct( lt->origin, surf->plane ) - surf->plane[3];
			if( dist >= 0 && dist < lt->intensity ) {
				if( BoundsAndSphereIntersect( surf->mins, surf->maxs, lt->origin, lt->intensity ) ) {
					return true;
				}
			}
			break;
		case FACETYPE_PATCH:
		case FACETYPE_TRISURF:
		case FACETYPE_FOLIAGE:
			if( BoundsAndSphereIntersect( surf->mins, surf->maxs, lt->origin, lt->intensity ) ) {
				return true;
			}
			break;
	}

	return false;
}

/*
* R_DrawBSPSurf
*/
void R_DrawBSPSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, 
	const portalSurface_t *portalSurface, drawSurfaceBSP_t *drawSurf ) {
	const vboSlice_t *slice;
	static const vboSlice_t nullSlice = { 0 };
	int firstVert, firstElem;
	int numVerts, numElems;

	slice = R_GetDrawListVBOSlice( rn.meshlist, drawSurf - rsh.worldBrushModel->drawSurfaces );

	assert( slice != NULL );
	if( !slice ) {
		return;
	}

	numVerts = slice->numVerts;
	numElems = slice->numElems;
	firstVert = drawSurf->firstVboVert + slice->firstVert;
	firstElem = drawSurf->firstVboElem + slice->firstElem;

	if( !numVerts ) {
		return;
	}

	RB_BindVBO( drawSurf->vbo->index, GL_TRIANGLES );

	RB_SetLightstyle( drawSurf->superLightStyle );

	RB_SetRtLightParams( drawSurf->numRtLights, drawSurf->rtLights, drawSurf->numWorldSurfaces, drawSurf->surfRtlightBits );

	if( drawSurf->numInstances ) {
		RB_DrawElementsInstanced( firstVert, numVerts, firstElem, numElems,
								  0, 0, 0, 0,
								  drawSurf->numInstances, drawSurf->instances );
	} else {
		RB_DrawElements( firstVert, numVerts, firstElem, numElems,
						 0, 0, 0, 0 );
	}
}

/*
* R_AddSurfaceVBOSlice
*/
static void R_AddSurfaceVBOSlice( drawList_t *list, drawSurfaceBSP_t *drawSurf, const msurface_t *surf, int offset ) {
	R_AddDrawListVBOSlice( list, offset + drawSurf - rsh.worldBrushModel->drawSurfaces,
				   surf->mesh.numVerts, surf->mesh.numElems,
				   surf->firstDrawSurfVert, surf->firstDrawSurfElem );
}

/*
* R_AddSurfaceToDrawList
*/
static bool R_AddSurfaceToDrawList( const entity_t *e, drawSurfaceBSP_t *drawSurf ) {
	const shader_t *shader = drawSurf->shader;
	const mfog_t *fog = drawSurf->fog;
	portalSurface_t *portalSurface = NULL;
	bool sky, portal;
	unsigned drawOrder = 0;
	unsigned sliceIndex = drawSurf - rsh.worldBrushModel->drawSurfaces;

	if( drawSurf->visFrame == rf.frameCount ) {
		return true;
	}

	sky = ( shader->flags & SHADER_SKY ) != 0;
	portal = ( shader->flags & SHADER_PORTAL ) != 0;

	if( sky ) {
		if( R_FASTSKY() ) {
			return false;
		}

		if( rn.refdef.rdflags & RDF_SKYPORTALINVIEW ) {
			portalSurface = R_AddSkyportalSurface( e, shader, drawSurf );
		}

		drawSurf->visFrame = rf.frameCount;
		drawSurf->listSurf = R_AddSurfToDrawList( rn.meshlist, e, fog, shader, 0, drawOrder, portalSurface, drawSurf );

		R_AddSkySurfToDrawList( rn.meshlist, shader, portalSurface, &rn.skyDrawSurface );

		R_AddDrawListVBOSlice( rn.meshlist, sliceIndex, 0, 0, 0, 0 );
	
		rf.stats.c_world_draw_surfs++;
		return true;
	}

	if( portal ) {
		portalSurface = R_AddPortalSurface( e, shader, drawSurf );
	}

	drawOrder = R_PackOpaqueOrder( fog, shader, drawSurf->numLightmaps, false );

	drawSurf->visFrame = rf.frameCount;
	drawSurf->listSurf = R_AddSurfToDrawList( rn.meshlist, e, fog, shader, WORLDSURF_DIST, drawOrder, portalSurface, drawSurf );

	if( !drawSurf->listSurf ) {
		return false;
	}

	if( portalSurface && !( shader->flags & ( SHADER_PORTAL_CAPTURE | SHADER_PORTAL_CAPTURE2 ) ) ) {
		R_AddSurfToDrawList( rn.portalmasklist, e, NULL, rsh.skyShader, 0, 0, NULL, drawSurf );
	}

	R_AddDrawListVBOSlice( rn.meshlist, sliceIndex, 0, 0, 0, 0 );

	rf.stats.c_world_draw_surfs++;
	return true;
}

/*
* R_ClipSpecialWorldSurf
*/
static bool R_ClipSpecialWorldSurf( drawSurfaceBSP_t *drawSurf, const msurface_t *surf, const vec3_t origin, float *pdist ) {
	bool sky, portal;
	portalSurface_t *portalSurface = NULL;
	const shader_t *shader = drawSurf->shader;

	sky = ( shader->flags & SHADER_SKY ) != 0;
	portal = ( shader->flags & SHADER_PORTAL ) != 0;

	if( sky ) {
		if( R_ClipSkySurface( &rn.skyDrawSurface, surf ) ) {
			return true;
		}
		return false;
	}

	if( portal ) {
		portalSurface = R_GetDrawListSurfPortal( drawSurf->listSurf );
	}

	if( portalSurface != NULL ) {
		vec3_t centre;
		float dist = 0;

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

		R_UpdatePortalSurface( portalSurface, &surf->mesh, surf->mins, surf->maxs, shader, drawSurf );

		*pdist = dist;
	}

	return true;
}

/*
* R_UpdateSurfaceInDrawList
*
* Walk the list of visible world surfaces and prepare the final VBO slice and draw order bits.
* For sky surfaces, skybox clipping is also performed.
*/
static void R_UpdateSurfaceInDrawList( drawSurfaceBSP_t *drawSurf, const vec3_t origin ) {
	unsigned i;
	float dist = 0;
	bool special;
	unsigned rtLightFrame;
	msurface_t *firstVisSurf, *lastVisSurf;

	if( !drawSurf->listSurf ) {
		return;
	}

	firstVisSurf = lastVisSurf = NULL;

	rtLightFrame = drawSurf->rtLightFrame;

	special = ( drawSurf->shader->flags & (SHADER_SKY|SHADER_PORTAL) ) != 0;

	if( drawSurf->rtLightFrame != rsc.frameCount ) {
		unsigned l;

		drawSurf->numRtLights = 0;
		drawSurf->rtLightFrame = rsc.frameCount;

		// reset lightbits for individual surfaces
		memset( drawSurf->surfRtlightBits, 0, sizeof( *drawSurf->surfRtlightBits ) * drawSurf->numWorldSurfaces );

		for( l = 0; l < rn.numRealtimeLights; l++ ) {
			int bit = 1 << drawSurf->numRtLights;
			rtlight_t *lt = rn.rtlights[l];
			bool clipped = false;

			if( drawSurf->numRtLights == MAX_DRAWSURF_RTLIGHTS ) {
				break;
			}

			for( i = 0; i < drawSurf->numWorldSurfaces; i++ ) {
				int s = drawSurf->firstWorldSurface + i;
				msurface_t *surf = rsh.worldBrushModel->surfaces + s;

				if( !rf.worldSurfVis[s] ) {
					continue;
				}

				if( drawSurf->numRtLights == MAX_DRAWSURF_RTLIGHTS ) {
					break;
				}

				if( R_SurfaceClipRtLight( surf, lt ) ) {
					if( !clipped ) {
						drawSurf->rtLights[drawSurf->numRtLights] = lt;
						drawSurf->numRtLights++;
						clipped = true;
					}

					drawSurf->surfRtlightBits[i] |= bit;
				}
			}
		}
	}

	for( i = 0; i < drawSurf->numWorldSurfaces; i++ ) {
		int s = drawSurf->firstWorldSurface + i;
		msurface_t *surf = rsh.worldBrushModel->surfaces + s;

		if( rf.worldSurfVis[s] ) {
			float sdist = 0;

			if( special && !R_ClipSpecialWorldSurf( drawSurf, surf, origin, &sdist ) ) {
				// clipped away
				continue;
			}

			if( sdist > sdist )
				dist = sdist;

			// surfaces are sorted by their firstDrawVert index so to cut the final slice
			// we only need to note the first and the last surface
			if( firstVisSurf == NULL )
				firstVisSurf = surf;
			lastVisSurf = surf;
		}
	}

	// prepare the slice
	if( firstVisSurf ) {
		bool rtlight = drawSurf->numRtLights != 0;

		R_AddSurfaceVBOSlice( rn.meshlist, drawSurf, firstVisSurf, 0 );

		if( lastVisSurf != firstVisSurf )
			R_AddSurfaceVBOSlice( rn.meshlist, drawSurf, lastVisSurf, 0 );

		// update the distance sorting key if it's a portal surface or a normal dlit surface
		if( dist != 0 || rtlight ) {
			int drawOrder = R_PackOpaqueOrder( drawSurf->fog, drawSurf->shader, drawSurf->numLightmaps, rtlight );
			if( dist == 0 )
				dist = WORLDSURF_DIST;
			R_UpdateDrawSurfDistKey( drawSurf->listSurf, 0, drawSurf->shader, dist, drawOrder );
		}
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
		return model->radius * e->scale;
	} else {
		if( rotated ) {
			*rotated = false;
		}
		VectorMA( e->origin, e->scale, model->mins, mins );
		VectorMA( e->origin, e->scale, model->maxs, maxs );
		return RadiusFromBounds( mins, maxs );
	}
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
	cache->fog = R_FogForBounds( cache->mins, cache->maxs );
}

/*
* R_AddBrushModelToDrawList
*/
bool R_AddBrushModelToDrawList( const entity_t *e ) {
	unsigned int i;
	vec3_t origin;
	model_t *model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
	mfog_t *fog;
	int numRtLights;
	unsigned rtLightBits;
	rtlight_t *rtLights[MAX_DRAWSURF_RTLIGHTS];
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

	fog = cache->fog;

	R_TransformPointToModelSpace( e, cache->rotated, rn.refdef.vieworg, modelOrg );

	numVisSurfaces = 0;

	for( i = 0; i < bmodel->numModelSurfaces; i++ ) {
		unsigned s = bmodel->firstModelSurface + i;
		msurface_t *surf = rsh.worldBrushModel->surfaces + s;

		if( !surf->drawSurf ) {
			continue;
		}
		if( R_CullSurface( e, surf, 0 ) ) {
			continue;
		}

		rf.worldSurfVis[s] = 1;
		rf.worldDrawSurfVis[surf->drawSurf - 1] = 1;

		numVisSurfaces++;
	}

	if( !numVisSurfaces ) {
		return false;
	}

	// check dynamic lights that matter in the instance against the model
	numRtLights = 0;
	for( i = 0; i < rn.numRealtimeLights; i++ ) {
		rtlight_t *l = rn.rtlights[i];
		if( numRtLights == MAX_DRAWSURF_RTLIGHTS ) {
			break;
		}
		if( !BoundsAndSphereIntersect( cache->mins, cache->maxs, l->origin, l->intensity ) ) {
			continue;
		}
		rtLights[numRtLights++] = l;
	}
	rtLightBits = (1<<numRtLights) - 1;

	for( i = 0; i < bmodel->numModelDrawSurfaces; i++ ) {
		unsigned s = bmodel->firstModelDrawSurface + i;
		drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + s;

		if( rf.worldDrawSurfVis[s] ) {
			unsigned j;

			drawSurf->numRtLights = numRtLights;
			drawSurf->rtLightFrame = rsc.frameCount;
			memcpy( drawSurf->rtLights, rtLights, numRtLights * sizeof( *rtLights ) );

			for( j = 0; j < drawSurf->numWorldSurfaces; j++ ) {
				drawSurf->surfRtlightBits[j] = rtLightBits;
			}

			R_AddSurfaceToDrawList( e, drawSurf );

			R_UpdateSurfaceInDrawList( drawSurf, origin );
		}
	}

	return true;
}

/*
=============================================================

WORLD MODEL

=============================================================
*/

/*
* R_PostCullVisLeaves
*/
static void R_PostCullVisLeaves( void ) {
	unsigned i, j;
	mleaf_t *leaf;
	
	for( i = 0; i < rsh.worldBrushModel->numvisleafs; i++ ) {
		if( !rf.worldLeafVis[i] ) {
			continue;
		}

		leaf = rsh.worldBrushModel->visleafs[i];
		if( r_leafvis->integer && !( rn.renderFlags & RF_NONVIEWERREF ) ) {
			R_AddDebugBounds( leaf->mins, leaf->maxs, colorRed );
		}

		// add leaf bounds to view bounds
		for( j = 0; j < 3; j++ ) {
			rn.visMins[j] = min( rn.visMins[j], leaf->mins[j] );
			rn.visMaxs[j] = max( rn.visMaxs[j], leaf->maxs[j] );
		}

		rf.stats.c_world_leafs++;
	}
}

/*
* R_CullVisLeaves
*/
static void R_CullVisLeaves( unsigned firstLeaf, unsigned numLeaves, unsigned clipFlags ) {
	unsigned i, j;
	mleaf_t *leaf;
	uint8_t *pvs;
	uint8_t *areabits;
	int arearowbytes, areabytes;
	bool novis;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return;
	}

	novis = rn.renderFlags & RF_NOVIS || rf.viewcluster == -1 || !rsh.worldBrushModel->pvs;
	arearowbytes = ( ( rsh.worldBrushModel->numareas + 7 ) / 8 );
	areabytes = arearowbytes;
#ifdef AREAPORTALS_MATRIX
	areabytes *= rsh.worldBrushModel->numareas;
#endif

	pvs = Mod_ClusterPVS( rf.viewcluster, rsh.worldModel );
	if( rf.viewarea > -1 && rn.refdef.areabits )
#ifdef AREAPORTALS_MATRIX
	{ areabits = rn.refdef.areabits + rf.viewarea * arearowbytes;}
#else
	{ areabits = rn.refdef.areabits;}
#endif
	else {
		areabits = NULL;
	}

	for( i = 0; i < numLeaves; i++ ) {
		int clipped;
		unsigned bit, testFlags;
		cplane_t *clipplane;
		unsigned l = firstLeaf + i;

		leaf = rsh.worldBrushModel->visleafs[l];

		if( leaf->cluster < 0 ) {
			// we shouldn't really be here...
			continue;
		}

		if( !novis ) {
			// check for door connected areas
			if( areabits ) {
				if( leaf->area < 0 || !( areabits[leaf->area >> 3] & ( 1 << ( leaf->area & 7 ) ) ) ) {
					continue; // not visible
				}
			}

			if( !( pvs[leaf->cluster >> 3] & ( 1 << ( leaf->cluster & 7 ) ) ) ) {
				continue; // not visible
			}
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
				assert( leaf->visSurfaces[j] < rf.numWorldSurfVis );
				rf.worldSurfFullVis[leaf->visSurfaces[j]] = 1;
			}
		} else {
			// partly visible
			for( j = 0; j < leaf->numVisSurfaces; j++ ) {
				assert( leaf->visSurfaces[j] < rf.numWorldSurfVis );
				rf.worldSurfVis[leaf->visSurfaces[j]] = 1;
			}
		}

		assert( l < rf.numWorldLeafVis );
		rf.worldLeafVis[l] = 1;
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
			rf.worldSurfVis[i] = 0;
			rf.worldSurfFullVis[i] = 0;
			continue;
		}

		if( rf.worldSurfVis[i] ) {
			// the surface is at partly visible in at least one leaf, frustum cull it
			if( R_CullSurface( rsc.worldent, surf, clipFlags ) ) {
				rf.worldSurfVis[i] = 0;
			}
			rf.worldSurfFullVis[i] = 0;
		}
		else {
			if( rf.worldSurfFullVis[i] ) {
				// a fully visible surface, mark as visible
				rf.worldSurfVis[i] = 1;
			}
		}

		if( rf.worldSurfVis[i] ) {
			rf.worldDrawSurfVis[surf->drawSurf - 1] = 1;
		}
	}
}

/*
* R_AddVisSurfaces
*/
static void R_AddVisSurfaces( void ) {

	unsigned i;

	for( i = 0; i < rsh.worldBrushModel->numModelDrawSurfaces; i++ ) {
		drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + i;

		if( !rf.worldDrawSurfVis[i] ) {
			continue;
		}

		R_AddSurfaceToDrawList( rsc.worldent, drawSurf );
	}
}

/*
* R_AddWorldDrawSurfaces
*/
static void R_AddWorldDrawSurfaces( unsigned firstDrawSurf, unsigned numDrawSurfs ) {
	unsigned i;

	for( i = 0; i < numDrawSurfs; i++ ) {
		unsigned s = firstDrawSurf + i;
		drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + s;

		if( !rf.worldDrawSurfVis[s] ) {
			continue;
		}

		R_UpdateSurfaceInDrawList( drawSurf, NULL );
	}
}

/*
* R_AddWorldDrawSurfacesJob
*/
static void R_AddWorldDrawSurfacesJob( unsigned first, unsigned items, jobarg_t *j ) {
	R_AddWorldDrawSurfaces( first, items );
}

/*
* R_CullVisLeavesJob
*/
static void R_CullVisLeavesJob( unsigned first, unsigned items, jobarg_t *j ) {
	R_CullVisLeaves( first, items, j->uarg );
}

/*
* R_CullVisSurfacesJob
*/
static void R_CullVisSurfacesJob( unsigned first, unsigned items, jobarg_t *j ) {
	R_CullVisSurfaces( first, items, j->uarg );
}

/*
* R_AddRealtimeLights
*/
static void R_AddRealtimeLights( unsigned numLights, rtlight_t *lights, unsigned clipFlags ) {
	unsigned i;

	for( i = 0; i < numLights; i++ ) {
		rtlight_t *l = lights + i;

		if( rn.numRealtimeLights == MAX_SCENE_RTLIGHTS ) {
			break;
		}
		if( !(l->flags & LIGHTFLAG_REALTIMEMODE ) ) {
			break;
		}
		if( R_CullBox( l->cullmins, l->cullmaxs, clipFlags ) ) {
			continue;
		}
		if( R_VisCullSphere( l->origin, l->intensity ) ) {
			continue;
		}

		rn.rtlights[rn.numRealtimeLights] = l;
		rn.numRealtimeLights++;
	}
}

/*
* R_DrawWorld
*/
void R_DrawWorld( void ) {
	unsigned i;
	int clipFlags;
	int64_t msec = 0, msec2 = 0;
	bool worldOutlines;
	jobarg_t ja = { 0 };
	bool speeds = r_speeds->integer != 0;

	assert( rf.numWorldSurfVis >= rsh.worldBrushModel->numsurfaces );
	assert( rf.numWorldLeafVis >= rsh.worldBrushModel->numvisleafs );

	if( !r_drawworld->integer ) {
		return;
	}
	if( !rsh.worldModel ) {
		return;
	}
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return;
	}

	VectorCopy( rn.refdef.vieworg, modelOrg );

	worldOutlines = mapConfig.forceWorldOutlines || ( rn.refdef.rdflags & RDF_WORLDOUTLINES );

	if( worldOutlines && ( rf.viewcluster != -1 ) && r_outlines_scale->value > 0 ) {
		rsc.worldent->outlineHeight = max( 0.0f, r_outlines_world->value );
	} else {
		rsc.worldent->outlineHeight = 0;
	}
	Vector4Copy( mapConfig.outlineColor, rsc.worldent->outlineColor );

	clipFlags = rn.clipFlags;

	if( r_nocull->integer ) {
		clipFlags = 0;
	}

	// BEGIN t_world_node
	if( speeds ) {
		msec = ri.Sys_Milliseconds();
	}

	ja.uarg = clipFlags;

	if( rsh.worldBrushModel->numvisleafs > rsh.worldBrushModel->numsurfaces ) {
		memset( (void *)rf.worldSurfVis, 1, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldSurfFullVis, 0, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldLeafVis, 1, rsh.worldBrushModel->numvisleafs * sizeof( *rf.worldLeafVis ) );
		memset( (void *)rf.worldDrawSurfVis, 0, rsh.worldBrushModel->numDrawSurfaces * sizeof( *rf.worldDrawSurfVis ) );
	} else {
		memset( (void *)rf.worldSurfVis, 0, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldSurfFullVis, 0, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldLeafVis, 0, rsh.worldBrushModel->numvisleafs * sizeof( *rf.worldLeafVis ) );
		memset( (void *)rf.worldDrawSurfVis, 0, rsh.worldBrushModel->numDrawSurfaces * sizeof( *rf.worldDrawSurfVis ) );

		if( r_speeds->integer ) {
			msec2 = ri.Sys_Milliseconds();
		}

		//
		// cull leafs
		//
		RJ_ScheduleJob( &R_CullVisLeavesJob, &ja, rsh.worldBrushModel->numvisleafs );

		RJ_FinishJobs();

		if( speeds ) {
			rf.stats.t_cull_world_nodes += ri.Sys_Milliseconds() - msec2;
		}
	}

	//
	// cull surfaces and do some background work on computed vis leafs
	// 
	if( speeds ) {
		msec2 = ri.Sys_Milliseconds();
	}

	RJ_ScheduleJob( &R_CullVisSurfacesJob, &ja, rsh.worldBrushModel->numModelSurfaces );

	R_PostCullVisLeaves();

	RJ_FinishJobs();

	if( speeds ) {
		rf.stats.t_cull_world_surfs += ri.Sys_Milliseconds() - msec2;
	}

	// cull lights
	if( !(rn.renderFlags & RF_SHADOWMAPVIEW) && !r_fullbright->integer ) {
		if( r_lighting_realtime_world->integer == 1 ) {
			R_AddRealtimeLights( rsh.worldBrushModel->numRtLights, rsh.worldBrushModel->rtLights, clipFlags );
		}

		if( r_lighting_realtime_dynamic->integer > 0 ) {
			if( !( rn.renderFlags & RF_ENVVIEW ) && r_dynamiclight->integer == 1 ) {
				R_AddRealtimeLights( rsc.numDlights, rsc.dlights, clipFlags );
			}
		}
	}

	//
	// add visible surfaces to draw list
	//
	if( speeds ) {
		msec2 = ri.Sys_Milliseconds();
	}

	R_AddVisSurfaces();

	RJ_ScheduleJob( &R_AddWorldDrawSurfacesJob, &ja, rsh.worldBrushModel->numModelDrawSurfaces );

	if( speeds ) {
		for( i = 0; i < rsh.worldBrushModel->numsurfaces; i++ ) {
			if( rf.worldSurfVis[i] ) {
				rf.stats.c_brush_polys++;
			}
		}
	}

	// END t_world_node
	if( speeds ) {
		rf.stats.t_world_node += ri.Sys_Milliseconds() - msec;
	}
}
