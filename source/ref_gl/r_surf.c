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

static vec3_t modelOrg;							// relative to view point

//==================================================================================

/*
* R_SurfPotentiallyVisible
*/
bool R_SurfPotentiallyVisible( const msurface_t *surf )
{
	const shader_t *shader = surf->shader;
	if( surf->flags & SURF_NODRAW )
		return false;
	if( !surf->mesh )
		return false;
	if( !shader )
		return false;
	return true;
}

/*
* R_SurfPotentiallyShadowed
*/
bool R_SurfPotentiallyShadowed( const msurface_t *surf )
{
	if( surf->flags & ( SURF_SKY|SURF_NODLIGHT|SURF_NODRAW ) )
		return false;
	if( ( surf->shader->sort >= SHADER_SORT_OPAQUE ) && ( surf->shader->sort <= SHADER_SORT_ALPHATEST ) )
		return true;
	return false;
}

/*
* R_SurfPotentiallyLit
*/
bool R_SurfPotentiallyLit( const msurface_t *surf )
{
	const shader_t *shader;

	if( surf->flags & ( SURF_SKY|SURF_NODLIGHT|SURF_NODRAW ) )
		return false;
	shader = surf->shader;
	if( ( shader->flags & SHADER_SKY ) || !shader->numpasses )
		return false;
	return ( surf->mesh != NULL /* && (surf->facetype != FACETYPE_TRISURF)*/ );
}

/*
* R_CullSurface
*/
bool R_CullSurface( const entity_t *e, const msurface_t *surf, unsigned int clipflags )
{
	return ( clipflags && R_CullBox( surf->mins, surf->maxs, clipflags ) );
}

/*
* R_SurfaceDlightBits
*/
static unsigned int R_SurfaceDlightBits( const msurface_t *surf, unsigned int checkDlightBits )
{
	unsigned int i, bit;
	dlight_t *lt;
	float dist;
	unsigned int surfDlightBits = 0;

	if( !checkDlightBits ) {
		return 0;
	}
	if( !R_SurfPotentiallyLit( surf ) ) {
		return 0;
	}

	for( i = 0, bit = 1, lt = rsc.dlights; i < rsc.numDlights; i++, bit <<= 1, lt++ ) {
		if( checkDlightBits & bit ) {
			switch( surf->facetype ) {
				case FACETYPE_PLANAR:
					dist = DotProduct( lt->origin, surf->plane ) - surf->plane[3];
					if( dist > -lt->intensity && dist < lt->intensity ) {
						surfDlightBits |= bit;
					}
					break;
				case FACETYPE_PATCH:
				case FACETYPE_TRISURF:
				case FACETYPE_FOLIAGE:
					if( BoundsAndSphereIntersect( surf->mins, surf->maxs, lt->origin, lt->intensity ) ) {
						surfDlightBits |= bit;
					}
					break;
			}
			checkDlightBits &= ~bit;
			if( !checkDlightBits ) {
				break;
			}
		}
	}

	return surfDlightBits;
}

/*
* R_SurfaceShadowBits
*/
static unsigned int R_SurfaceShadowBits( const msurface_t *surf, unsigned int checkShadowBits )
{
	unsigned int i, bit;
	shadowGroup_t *grp;
	unsigned int surfShadowBits = 0;

	if( !checkShadowBits ) {
		return 0;
	}
	if( !R_SurfPotentiallyShadowed( surf ) ) {
		return 0;
	}

	for( i = 0; i < rsc.numShadowGroups; i++ ) {
		grp = rsc.shadowGroups + i;
		bit = grp->bit;

		if( checkShadowBits & bit ) {
			switch( surf->facetype ) {
				case FACETYPE_PLANAR:
					if ( BoundsIntersect( surf->mins, surf->maxs, grp->visMins, grp->visMaxs ) ) {
						float dist = DotProduct( grp->visOrigin, surf->plane ) - surf->plane[3];
						if ( dist > -grp->visRadius && dist <= grp->visRadius ) {
							// crossed by plane
							surfShadowBits |= bit;
						}
					}
					break;
				case FACETYPE_PATCH:
				case FACETYPE_TRISURF:
				case FACETYPE_FOLIAGE:
					if( BoundsIntersect( surf->mins, surf->maxs, grp->visMins, grp->visMaxs ) ) {
						surfShadowBits |= bit;
					}
					break;
			}
			checkShadowBits &= ~bit;
			if( !checkShadowBits ) {
				break;
			}
		}
	}
	
	return surfShadowBits;
}

/*
* R_DrawBSPSurf
*/
void R_DrawBSPSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, const portalSurface_t *portalSurface, unsigned int entShadowBits, drawSurfaceBSP_t *drawSurf )
{
	const vboSlice_t *slice;
	const vboSlice_t *shadowSlice;
	static const vboSlice_t nullSlice = { 0 };
	int firstVert, firstElem;
	int numVerts, numElems;
	int firstShadowVert, firstShadowElem;
	int numShadowVerts, numShadowElems;
	unsigned shadowBits, dlightBits;

	slice = R_GetVBOSlice( drawSurf - rsh.worldBrushModel->drawSurfaces );
	shadowSlice = R_GetVBOSlice( rsh.worldBrushModel->numDrawSurfaces + ( drawSurf - rsh.worldBrushModel->drawSurfaces ) );
	if( !shadowSlice ) {
		shadowSlice = &nullSlice;
	}

	assert( slice != NULL );

	if( drawSurf->dlightFrame == rsc.frameCount ) {
		dlightBits = drawSurf->dlightBits & rn.dlightBits;
	}
	else {
		dlightBits = 0;
	}

	if( drawSurf->shadowFrame == rsc.frameCount ) {
		shadowBits = (drawSurf->shadowBits & rn.shadowBits) & rsc.renderedShadowBits;
	}
	else {
		shadowBits = 0;
	}

	// shadowBits are shared for all rendering instances (normal view, portals, etc)
	// if either shadow slice is empty or shadowBits is 0, then we must pass the surface unshadowed
	numVerts = slice->numVerts;
	numElems = slice->numElems;
	firstVert = drawSurf->firstVboVert + slice->firstVert;
	firstElem = drawSurf->firstVboElem + slice->firstElem;
	if( shadowBits && shadowSlice->numElems ) {
		numShadowVerts = shadowSlice->numVerts;
		numShadowElems = shadowSlice->numElems;
		firstShadowVert = drawSurf->firstVboVert + shadowSlice->firstVert;
		firstShadowElem = drawSurf->firstVboElem + shadowSlice->firstElem;
	}
	else {
		shadowBits = 0;
		numShadowVerts = 0;
		numShadowElems = 0;
		firstShadowVert = 0;
		firstShadowElem = 0;
	}

	RB_BindVBO( drawSurf->vbo->index, GL_TRIANGLES );

	RB_SetDlightBits( dlightBits );

	RB_SetShadowBits( shadowBits );

	RB_SetLightstyle( drawSurf->superLightStyle );

	if( drawSurf->numInstances ) {
		RB_DrawElementsInstanced( firstVert, numVerts, firstElem, numElems, 
			firstShadowVert, numShadowVerts, firstShadowElem, numShadowElems,
			drawSurf->numInstances, drawSurf->instances );
	}
	else {
		RB_DrawElements( firstVert, numVerts, firstElem, numElems, 
			firstShadowVert, numShadowVerts, firstShadowElem, numShadowElems );
	}
}

/*
* R_AddSurfaceVBOSlice
*/
static void R_AddSurfaceVBOSlice( const msurface_t *surf, int offset )
{
	drawSurfaceBSP_t *drawSurf = surf->drawSurf;
	R_AddVBOSlice( offset + drawSurf - rsh.worldBrushModel->drawSurfaces, 
		surf->numVerts, surf->numElems,
		surf->firstDrawSurfVert, surf->firstDrawSurfElem );
}

/*
* R_AddSurfaceToDrawList
*
* Note that dlit may be true even if dlightBits is 0, indicating there's at least potentially one
* dynamically light surface for the drawSurf.
*/
static void R_AddSurfaceToDrawList( const entity_t *e, const msurface_t *surf, const mfog_t *fog,
	unsigned int dlightBits, unsigned shadowBits, const vec3_t origin )
{
	int i;
	shader_t *shader;
	drawSurfaceBSP_t *drawSurf = surf->drawSurf;
	portalSurface_t *portalSurface = NULL;

	if( r_drawworld->integer == 2 ) {
		shader = rsh.envShader;
	} else {
		shader = surf->shader;

		if( shader->flags & SHADER_SKY ) {
			bool addSurf = true, addSlice = false;

			if( R_FASTSKY() ) {
				return;
			}

			if( R_ClipSkySurface( surf ) ) {
				if( rn.refdef.rdflags & RDF_SKYPORTALINVIEW ) {
					// for skyportals, generate portal surface and
					// also add BSP surface to skybox if it's fogged to render
					// the fog hull later
					portalSurface = R_AddSkyportalSurface( e, shader, drawSurf );
					addSurf = portalSurface != NULL && fog != NULL;
					addSlice = portalSurface != NULL;
				}

				if( addSurf ) {
					addSlice = R_AddSkySurfToDrawList( surf, portalSurface );
				}
				if( addSlice ) {
					R_AddSurfaceVBOSlice( surf, 0 );
				}
			}

			goto done;
		}
	}
	

	if( drawSurf->visFrame != rf.frameCount ) {
		float dist = 0;
		bool lightmapped = (surf->flags & SURF_NOLIGHTMAP) == 0;
		unsigned drawOrder = R_PackOpaqueOrder( e, shader, lightmapped, dlightBits != 0 );

		if( shader->flags & SHADER_PORTAL ) {
			vec3_t centre;

			if( origin ) {
				VectorCopy( origin, centre );
			}
			else {
				VectorAdd( surf->mins, surf->maxs, centre );
				VectorScale( centre, 0.5, centre );
			}
			dist = Distance( rn.refdef.vieworg, centre );

			// draw portals in front-to-back order
			dist = 1024 - dist / 100.0f; 
			if( dist < 1 ) dist = 1;

			portalSurface = R_AddPortalSurface( e, surf->mesh, surf->mins, surf->maxs, shader, drawSurf );
		}

		drawSurf->visFrame = rf.frameCount;
		drawSurf->listSurf = R_AddSurfToDrawList( rn.meshlist, e, fog, shader, dist, drawOrder, portalSurface, drawSurf );
		if( !drawSurf->listSurf ) {
			return;
		}

		if( portalSurface && !( shader->flags & (SHADER_PORTAL_CAPTURE|SHADER_PORTAL_CAPTURE2) ) ) {
			R_AddSurfToDrawList( rn.portalmasklist, e, NULL, rsh.skyShader, 0, 0, NULL, drawSurf );
		}
	}
	else {
		if( !drawSurf->listSurf ) {
			return;
		}

		if( dlightBits != 0 ) {
			// update (OR) the dlightbit
			unsigned drawOrder = R_PackOpaqueOrder( e, NULL, false, dlightBits != 0 );
			R_UpdateDrawListSurf( drawSurf->listSurf, drawOrder );
		}
	}

	// keep track of the actual vbo chunk we need to render
	R_AddSurfaceVBOSlice( surf, 0 );

	// dynamic lights that affect the surface
	if( dlightBits ) {
		// ignore dlights that have already been marked as affectors
		if( drawSurf->dlightFrame == rsc.frameCount ) {
			drawSurf->dlightBits |= dlightBits;
		} else {
			drawSurf->dlightBits = dlightBits;
			drawSurf->dlightFrame = rsc.frameCount;
		}
	}

	// shadows that are projected onto the surface
	if( shadowBits ) {
		R_AddSurfaceVBOSlice( surf, rsh.worldBrushModel->numDrawSurfaces );

		// ignore shadows that have already been marked as affectors
		if( drawSurf->shadowFrame == rsc.frameCount ) {
			drawSurf->shadowBits |= shadowBits;
		} else {
			drawSurf->shadowBits = shadowBits;
			drawSurf->shadowFrame = rsc.frameCount;
		}
	}

done:
	// add surface bounds to view bounds
	for( i = 0; i < 3; i++ )
	{
		rn.visMins[i] = min( rn.visMins[i], surf->mins[i] );
		rn.visMaxs[i] = max( rn.visMaxs[i], surf->maxs[i] );
	}

	rn.dlightBits |= dlightBits;
	rn.shadowBits |= shadowBits;
	rn.numVisSurfaces++;

	rf.stats.c_brush_polys++;
}

/*
=============================================================

BRUSH MODELS

=============================================================
*/

/*
* R_BrushModelBBox
*/
float R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, bool *rotated )
{
	int i;
	const model_t	*model = e->model;

	if( !Matrix3_Compare( e->axis, axis_identity ) )
	{
		if( rotated )
			*rotated = true;
		for( i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - model->radius * e->scale;
			maxs[i] = e->origin[i] + model->radius * e->scale;
		}
		return model->radius * e->scale;
	}
	else
	{
		if( rotated )
			*rotated = false;
		VectorMA( e->origin, e->scale, model->mins, mins );
		VectorMA( e->origin, e->scale, model->maxs, maxs );
		return RadiusFromBounds( mins, maxs );
	}
}

#define R_TransformPointToModelSpace(e,rotate,in,out) \
	VectorSubtract( in, (e)->origin, out ); \
	if( rotated ) { \
		vec3_t temp; \
		VectorCopy( out, temp ); \
		Matrix3_TransformVector( (e)->axis, temp, out ); \
	}

/*
* R_AddBrushModelToDrawList
*/
bool R_AddBrushModelToDrawList( const entity_t *e )
{
	unsigned int i;
	vec3_t origin;
	vec3_t bmins, bmaxs;
	bool rotated;
	model_t	*model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
	msurface_t *surf;
	mfog_t *fog;
	float radius;
	unsigned int bit, fullBits;
	unsigned int dlightBits, shadowBits;

	if( bmodel->nummodelsurfaces == 0 ) {
		return false;
	}

	radius = R_BrushModelBBox( e, bmins, bmaxs, &rotated );

	if( R_CullModelEntity( e, bmins, bmaxs, radius, rotated, false ) ) {
		return false;
	}

	// never render weapon models or non-occluders into shadowmaps
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		if( rsc.entShadowGroups[R_ENT2NUM(e)] != rn.shadowGroup->id ) {
			return true;
		}
	}

	VectorAdd( e->model->mins, e->model->maxs, origin );
	VectorMA( e->origin, 0.5, origin, origin );

	fog = R_FogForBounds( bmins, bmaxs );

	R_TransformPointToModelSpace( e, rotated, rn.refdef.vieworg, modelOrg );

	// check dynamic lights that matter in the instance against the model
	dlightBits = 0;
	for( i = 0, fullBits = rn.dlightBits, bit = 1; fullBits; i++, fullBits &= ~bit, bit <<= 1 ) {
		if( !( fullBits & bit ) ) {
			continue;
		}
		if( !BoundsAndSphereIntersect( bmins, bmaxs, rsc.dlights[i].origin, rsc.dlights[i].intensity ) ) {
			continue;
		}
		dlightBits |= bit;
	}

	// check shadowmaps that matter in the instance against the model
	shadowBits = 0;
	for( i = 0, fullBits = rn.shadowBits; fullBits; i++, fullBits &= ~bit ) {
		shadowGroup_t *grp = rsc.shadowGroups + i;
		bit = grp->bit;
		if( !( fullBits & bit ) ) {
			continue;
		}
		if( !BoundsIntersect( bmins, bmaxs, grp->visMins, grp->visMaxs ) ) {
			continue;
		}
		shadowBits |= bit;
	}

	for( i = 0; i < bmodel->nummodelsurfaces; i++ ) {
		int surfDlightBits, surfShadowBits;

		surf = bmodel->firstmodelsurface + i;
		if( !surf->drawSurf ) {
			continue;
		}
		if( R_CullSurface( e, surf, 0 ) ) {
			continue;
		}

		surfDlightBits = R_SurfPotentiallyLit( surf ) ? dlightBits : 0;
		surfShadowBits = R_SurfPotentiallyShadowed( surf ) ? shadowBits : 0;

		R_AddSurfaceToDrawList( e, surf, fog, surfDlightBits, surfShadowBits, origin );
	}

	return true;
}

/*
=============================================================

WORLD MODEL

=============================================================
*/

/*
* R_AddOrUpdateDrawSurface
*/
static void R_AddOrUpdateDrawSurface( msurface_t *surf, unsigned int dlightBits, unsigned int shadowBits )
{
	unsigned int newDlightBits = dlightBits;
	unsigned int newShadowBits = shadowBits;
	drawSurfaceBSP_t *drawSurf = surf->drawSurf;

	// avoid double-checking dlights that have already been added to drawSurf
	if( drawSurf->dlightFrame == rsc.frameCount ) {
		newDlightBits &= ~drawSurf->dlightBits;
	}

	newDlightBits = R_SurfaceDlightBits( surf, newDlightBits );
	newShadowBits = R_SurfaceShadowBits( surf, newShadowBits );

	R_AddSurfaceToDrawList( rsc.worldent, surf, surf->fog, newDlightBits, newShadowBits, NULL );
}

/*
* R_CountVisLeaves
*/
static void R_CountVisLeaves( void )
{
	unsigned i;
	mleaf_t *leaf;

	for( i = 0; i < rsh.worldBrushModel->numvisleafs; i++ )
	{
		if( !rf.worldLeafVis[i] ) {
			continue;
		}

		leaf = rsh.worldBrushModel->visleafs[i];
		if( r_leafvis->integer && !( rn.renderFlags & RF_NONVIEWERREF ) )
		{
			const byte_vec4_t color = { 255, 0, 0, 255 };
			R_AddDebugBounds( leaf->mins, leaf->maxs, color );
		}

		rf.stats.c_world_leafs++;
	}
}

/*
* R_CullVisLeaves
*/
static void R_CullVisLeaves( unsigned firstLeaf, unsigned items, unsigned clipFlags )
{
	unsigned i, j;
	mleaf_t	*leaf;
	uint8_t *pvs;
	int rdflags;
	uint8_t *areabits;
	int arearowbytes, areabytes;
	bool haveareabits, novis;

	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;

	novis = rn.renderFlags & RF_NOVIS || rf.viewcluster == -1 || !rsh.worldBrushModel->pvs;
	rdflags = rn.refdef.rdflags;

	haveareabits = rn.refdef.areabits != NULL;
	arearowbytes = ((rsh.worldBrushModel->numareas+7)/8);
	areabytes = arearowbytes;
#ifdef AREAPORTALS_MATRIX
	areabytes *= rsh.worldBrushModel->numareas;
#endif

	pvs = Mod_ClusterPVS( rf.viewcluster, rsh.worldModel );
	if( rf.viewarea > -1 && rn.refdef.areabits )
#ifdef AREAPORTALS_MATRIX
		areabits = rn.refdef.areabits + rf.viewarea * arearowbytes;
#else
		areabits = rn.refdef.areabits;
#endif
	else
		areabits = NULL;

	for( i = 0; i < items; i++ )
	{
		unsigned l = firstLeaf + i;

		leaf = rsh.worldBrushModel->visleafs[l];
		if( !novis )
		{
			// check for door connected areas
			if( areabits )
			{
				if( leaf->area < 0 || !( areabits[leaf->area>>3] & ( 1<<( leaf->area&7 ) ) ) )
					continue; // not visible
			}

			if( !( pvs[leaf->cluster>>3] & ( 1<<( leaf->cluster&7 ) ) ) )
				continue; // not visible
		}

		if( R_CullBox( leaf->mins, leaf->maxs, clipFlags ) )
			continue;

		for( j = 0; j < leaf->numVisSurfaces; j++ ) {
			assert( leaf->visSurfaces[j] < rf.numWorldSurfVis );
			rf.worldSurfVis[leaf->visSurfaces[j]] = 1;
		}

		rf.worldLeafVis[l] = 1;
	}
}

/*
* R_CullVisSurfaces
*/
static void R_CullVisSurfaces( unsigned firstSurf, unsigned items, unsigned clipFlags )
{
	unsigned i;

	for( i = 0; i < items; i++ ) {
		unsigned s = firstSurf + i;
		if( !rf.worldSurfVis[s] ) {
			continue;
		}
		if( R_CullSurface( rsc.worldent, rsh.worldBrushModel->surfaces + s, clipFlags ) ) {
			rf.worldSurfVis[s] = 0;
		}
	}
}

/*
* R_DrawVisSurfaces
*/
static void R_DrawVisSurfaces( unsigned dlightBits, unsigned shadowBits )
{
	unsigned i;

	for( i = 0; i < rsh.worldBrushModel->numsurfaces; i++ ) {
		if( !rf.worldSurfVis[i] ) {
			continue;
		}
		R_AddOrUpdateDrawSurface( rsh.worldBrushModel->surfaces + i, dlightBits, shadowBits );
	}
}

/*
* R_CullVisLeavesJob
*/
static void R_CullVisLeavesJob( unsigned first, unsigned items, jobarg_t *j )
{
	R_CullVisLeaves( first, items, j->uarg );
}

/*
* R_CullVisSurfacesJob
*/
static void R_CullVisSurfacesJob( unsigned first, unsigned items, jobarg_t *j )
{
	R_CullVisSurfaces( first, items, j->uarg );
}

/*
* R_DrawWorld
*/
void R_DrawWorld( void )
{
	unsigned int i;
	int clipFlags, msec = 0;
	unsigned int dlightBits;
	unsigned int shadowBits;
	bool worldOutlines;
	jobarg_t ja = { 0 };

	assert( rf.numWorldSurfVis >= rsh.worldBrushModel->numsurfaces );
	assert( rf.numWorldLeafVis >= rsh.worldBrushModel->numvisleafs );

	if( !r_drawworld->integer )
		return;
	if( !rsh.worldModel )
		return;
	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;

	VectorCopy( rn.refdef.vieworg, modelOrg );

	worldOutlines = mapConfig.forceWorldOutlines || ( rn.refdef.rdflags & RDF_WORLDOUTLINES );

	if( worldOutlines && (rf.viewcluster != -1) && r_outlines_scale->value > 0 )
		rsc.worldent->outlineHeight = max( 0.0f, r_outlines_world->value );
	else
		rsc.worldent->outlineHeight = 0;
	Vector4Copy( mapConfig.outlineColor, rsc.worldent->outlineColor );

	clipFlags = rn.clipFlags;
	dlightBits = 0;
	shadowBits = 0;

	if( r_nocull->integer )
		clipFlags = 0;

	// cull dynamic lights
	if( !( rn.renderFlags & RF_ENVVIEW ) ) {
		if( r_dynamiclight->integer == 1 && !r_fullbright->integer ) {
			for( i = 0; i < rsc.numDlights; i++ ) {
				if( R_CullSphere( rsc.dlights[i].origin, rsc.dlights[i].intensity, clipFlags ) ) {
					continue;
				}
				dlightBits |= 1<<i;
			}
		}
	}

	// cull shadowmaps
	if( !( rn.renderFlags & RF_ENVVIEW ) ) {
		for( i = 0; i < rsc.numShadowGroups; i++ ) {
			shadowGroup_t *grp = rsc.shadowGroups + i;
			if( R_CullBox( grp->visMins, grp->visMaxs, clipFlags ) ) {
				continue;
			}
			shadowBits |= grp->bit;
		}
	}

	rn.dlightBits = 0;
	rn.shadowBits = 0;

	if( r_speeds->integer )
		msec = ri.Sys_Milliseconds();

	ja.uarg = clipFlags;

	if( rsh.worldBrushModel->numvisleafs > rsh.worldBrushModel->numsurfaces )
	{
		memset( (void *)rf.worldSurfVis, 1, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldLeafVis, 1, rsh.worldBrushModel->numvisleafs * sizeof( *rf.worldLeafVis ) );
	}
	else
	{
		memset( (void *)rf.worldSurfVis, 0, rsh.worldBrushModel->numsurfaces * sizeof( *rf.worldSurfVis ) );
		memset( (void *)rf.worldLeafVis, 0, rsh.worldBrushModel->numvisleafs * sizeof( *rf.worldLeafVis ) );

		RJ_ScheduleJob( &R_CullVisLeavesJob, &ja, rsh.worldBrushModel->numvisleafs );
		RJ_CompleteJobs();
	}

	RJ_ScheduleJob( &R_CullVisSurfacesJob, &ja, rsh.worldBrushModel->numsurfaces );

	R_CountVisLeaves();

	RJ_CompleteJobs();

	R_DrawVisSurfaces( dlightBits, shadowBits );

	if( r_speeds->integer )
		rf.stats.t_world_node += ri.Sys_Milliseconds() - msec;
}
