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
	const shader_t *shader = surf->shader;

	if( r_nocull->integer )
		return false;
	if( ( shader->flags & SHADER_ALLDETAIL ) && !r_detailtextures->integer )
		return true;

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

	if( !R_SurfPotentiallyLit( surf ) ) {
		return 0;
	}

	for( i = 0, bit = 1, lt = rsc.dlights; i < rsc.numDlights; i++, bit <<= 1, lt++ ) {
		if( !checkDlightBits ) {
			break;
		}
		if( checkDlightBits & bit ) {
			switch( surf->facetype ) {
				case FACETYPE_PLANAR:
					dist = PlaneDiff( lt->origin, surf->plane );
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

	if( !R_SurfPotentiallyShadowed( surf ) ) {
		return 0;
	}

	for( i = 0; i < rsc.numShadowGroups; i++ ) {
		if( !checkShadowBits ) {
			break;
		}

		grp = rsc.shadowGroups + i;
		bit = grp->bit;

		if( checkShadowBits & bit ) {
			switch( surf->facetype ) {
				case FACETYPE_PLANAR:
					if ( BoundsIntersect( surf->mins, surf->maxs, grp->visMins, grp->visMaxs ) ) {
						float dist = PlaneDiff( grp->visOrigin, surf->plane );
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
	unsigned int dlightBits, unsigned shadowBits, float dist )
{
	shader_t *shader;
	drawSurfaceBSP_t *drawSurf = surf->drawSurf;
	portalSurface_t *portalSurface = NULL;
	bool lightmapped;
	unsigned drawOrder;

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
					addSurf = portalSurface != NULL && surf->fog != NULL;
					addSlice = portalSurface != NULL;
				}

				if( addSurf ) {
					addSlice = R_AddSkySurfToDrawList( surf, portalSurface );
				}
				if( addSlice ) {
					R_AddSurfaceVBOSlice( surf, 0 );
				}
			}

			rn.numVisSurfaces++;
			return;
		}
	}
	
	lightmapped = surf->superLightStyle != NULL && surf->superLightStyle->lightmapNum[0] >= 0;
	drawOrder = R_PackOpaqueOrder( e, shader, lightmapped, dlightBits != 0 );

	if( drawSurf->visFrame != rf.frameCount ) {
		if( shader->flags & SHADER_PORTAL ) {
			// draw portals in front-to-back order
			dist = 1024 - dist / 100.0f; 
			if( dist < 1 ) dist = 1;

			portalSurface = R_AddPortalSurface( e, surf->mesh, surf->mins, surf->maxs, shader, drawSurf );
		}
		else {
			// just ignore the distance since we're drawing batched geometry anyway
			dist = 0;
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

		// update (OR) the dlightbit
		R_UpdateDrawListSurf( drawSurf->listSurf, drawOrder );
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

	rf.stats.c_brush_polys++;
	rn.numVisSurfaces++;
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
	float radius, distance;
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
	distance = Distance( origin, rn.refdef.vieworg );

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

	for( i = 0, surf = bmodel->firstmodelsurface; i < bmodel->nummodelsurfaces; i++, surf++ ) {
		int surfDlightBits, surfShadowBits;

		if( !surf->drawSurf ) {
			continue;
		}
		if( surf->visFrame != rf.frameCount ) {
			surf->visFrame = rf.frameCount;

			if( R_CullSurface( e, surf, 0 ) ) {
				continue;
			}

			surfDlightBits = R_SurfPotentiallyLit( surf ) ? dlightBits : 0;
			surfShadowBits = R_SurfPotentiallyShadowed( surf ) ? shadowBits : 0;

			R_AddSurfaceToDrawList( e, surf, fog, surfDlightBits, surfShadowBits, distance );
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
* R_MarkLeafSurfaces
*/
static void R_MarkLeafSurfaces( msurface_t **mark, unsigned int clipFlags, 
	unsigned int dlightBits, unsigned int shadowBits )
{
	msurface_t *surf;
	unsigned int newDlightBits;
	unsigned int newShadowBits;
	drawSurfaceBSP_t *drawSurf;
	vec3_t centre;
	float distance;

	do
	{
		surf = *mark++;
		drawSurf = surf->drawSurf;

		if( R_CullSurface( rsc.worldent, surf, clipFlags ) ) {
			continue;
		}

		// avoid double-checking dlights that have already been added to drawSurf
		newDlightBits = dlightBits;
		if( drawSurf->dlightFrame == rsc.frameCount ) {
			newDlightBits &= ~drawSurf->dlightBits;
		}
		if( newDlightBits ) {
			newDlightBits = R_SurfaceDlightBits( surf, newDlightBits );
		}

		newShadowBits = R_SurfaceShadowBits( surf, shadowBits );

		if( surf->visFrame != rf.frameCount || newDlightBits || newShadowBits ) {
			VectorAdd( surf->mins, surf->maxs, centre );
			VectorScale( centre, 0.5, centre );
			distance = Distance( rn.refdef.vieworg, centre );

			R_AddSurfaceToDrawList( rsc.worldent, surf, surf->fog, 
				newDlightBits, newShadowBits, distance );
		}

		surf->visFrame = rf.frameCount;
	} while( *mark );
}

/*
* R_RecursiveWorldNode
*/
static void R_RecursiveWorldNode( mnode_t *node, unsigned int clipFlags, 
	unsigned int dlightBits, unsigned int shadowBits )
{
	unsigned int i;
	unsigned int dlightBits1;
	unsigned int shadowBits1;
	unsigned int bit;
	const cplane_t *clipplane;
	mleaf_t	*pleaf;

	while( 1 )
	{
		if( node->pvsframe != rf.pvsframecount )
			return;

		if( clipFlags )
		{
			for( i = sizeof( rn.frustum )/sizeof( rn.frustum[0] ), bit = 1, clipplane = rn.frustum; i > 0; i--, bit<<=1, clipplane++ )
			{
				if( clipFlags & bit )
				{
					int clipped = BoxOnPlaneSide( node->mins, node->maxs, clipplane );
					if( clipped == 2 )
						return;
					else if( clipped == 1 )
						clipFlags &= ~bit; // node is entirely on screen
				}
			}
		}

		if( !node->plane )
			break;

		dlightBits1 = 0;
		if( dlightBits )
		{
			float dist;
			unsigned int checkBits = dlightBits;

			for( i = 0, bit = 1; i < rsc.numDlights; i++, bit <<= 1 )
			{
				dlight_t *dl = rsc.dlights + i;
				if( dlightBits & bit )
				{
					dist = PlaneDiff( dl->origin, node->plane );
					if( dist < -dl->intensity )
						dlightBits &= ~bit;
					if( dist < dl->intensity )
						dlightBits1 |= bit;

					checkBits &= ~bit;
					if( !checkBits )
						break;
				}
			}
		}

		shadowBits1 = 0;
		if( shadowBits )
		{
			float dist;
			unsigned int checkBits = shadowBits;

			for( i = 0; i < rsc.numShadowGroups; i++ )
			{
				shadowGroup_t *group = rsc.shadowGroups + i;
				bit = group->bit;
				if( checkBits & bit )
				{
					dist = PlaneDiff( group->visOrigin, node->plane );
					if( dist < -group->visRadius )
						shadowBits &= ~bit;
					if( dist < group->visRadius )
						shadowBits1 |= bit;

					checkBits &= ~bit;
					if( !checkBits )
						break;
				}
			}
		}

		R_RecursiveWorldNode( node->children[0], clipFlags, dlightBits, shadowBits );

		node = node->children[1];
		dlightBits = dlightBits1;
		shadowBits = shadowBits1;
	}

	// if a leaf node, draw stuff
	pleaf = ( mleaf_t * )node;
	pleaf->visframe = rf.frameCount;

	// add leaf bounds to view bounds
	for( i = 0; i < 3; i++ )
	{
		rn.visMins[i] = min( rn.visMins[i], pleaf->mins[i] );
		rn.visMaxs[i] = max( rn.visMaxs[i], pleaf->maxs[i] );
	}

	rn.dlightBits |= dlightBits;
	rn.shadowBits |= shadowBits;

	R_MarkLeafSurfaces( pleaf->firstVisSurface, clipFlags, dlightBits, shadowBits );
	rf.stats.c_world_leafs++;

	if( r_leafvis->integer && !( rn.renderFlags & RF_NONVIEWERREF ) )
	{
		const byte_vec4_t color = { 255, 0, 0, 255 };
		R_AddDebugBounds( pleaf->mins, pleaf->maxs, color );
	}
}

//==================================================================================

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

	rn.dlightBits = dlightBits;
	rn.shadowBits = shadowBits;

	if( r_speeds->integer )
		msec = ri.Sys_Milliseconds();

	R_RecursiveWorldNode( rsh.worldBrushModel->nodes, clipFlags, dlightBits, shadowBits );

	if( r_speeds->integer )
		rf.stats.t_world_node += ri.Sys_Milliseconds() - msec;
}

/*
* R_MarkLeaves
* 
* Mark the leaves and nodes that are in the PVS for the current cluster
*/
void R_MarkLeaves( void )
{
	uint8_t *pvs;
	unsigned int i;
	int rdflags;
	mleaf_t	*leaf, **pleaf;
	mnode_t *node;
	uint8_t *areabits;
	int cluster;
	uint8_t fatpvs[MAX_MAP_LEAFS/8];
	int arearowbytes, areabytes;
	bool haveareabits;

	rdflags = rn.refdef.rdflags;
	if( rdflags & RDF_NOWORLDMODEL )
		return;
	if( !rsh.worldModel )
		return;

	haveareabits = rn.refdef.areabits != NULL;
	arearowbytes = ((rsh.worldBrushModel->numareas+7)/8);
	areabytes = arearowbytes;
#ifdef AREAPORTALS_MATRIX
	areabytes *= rsh.worldBrushModel->numareas;
#endif

	if( rf.oldviewcluster == rf.viewcluster && !(rn.renderFlags & RF_NOVIS) && rf.viewcluster != -1 && rf.oldviewcluster != -1 ) {
		// compare area bits from previous frame
		if( !haveareabits && rf.haveOldAreabits == false )
			return;
		if( haveareabits && rf.haveOldAreabits == true && memcmp( rf.oldAreabits, rn.refdef.areabits, areabytes ) == 0 )
			return;
	}

	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if( r_lockpvs->integer )
		return;

	rf.pvsframecount++;
	rf.oldviewcluster = rf.viewcluster;
	rf.haveOldAreabits = haveareabits;
	if( haveareabits )
		memcpy( rf.oldAreabits, rn.refdef.areabits, areabytes );

	if( rn.renderFlags & RF_NOVIS || rf.viewcluster == -1 || !rsh.worldBrushModel->pvs )
	{
		// mark everything
		for( pleaf = rsh.worldBrushModel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
			leaf->pvsframe = rf.pvsframecount;
		for( i = 0, node = rsh.worldBrushModel->nodes; i < rsh.worldBrushModel->numnodes; i++, node++ )
			node->pvsframe = rf.pvsframecount;
		return;
	}

	pvs = Mod_ClusterPVS( rf.viewcluster, rsh.worldModel );
	if( rf.viewarea > -1 && rn.refdef.areabits )
#ifdef AREAPORTALS_MATRIX
		areabits = rn.refdef.areabits + rf.viewarea * arearowbytes;
#else
		areabits = rn.refdef.areabits;
#endif
	else
		areabits = NULL;

	// may have to combine two clusters because of solid water boundaries
	if( mapConfig.checkWaterCrossing && ( rdflags & RDF_CROSSINGWATER ) )
	{
		int i, c;
		vec3_t pvsOrigin2;
		int viewcluster2;

		VectorCopy( rn.pvsOrigin, pvsOrigin2 );
		if( rdflags & RDF_UNDERWATER )
		{
			// look up a bit
			pvsOrigin2[2] += 9;
		}
		else
		{
			// look down a bit
			pvsOrigin2[2] -= 9;
		}

		leaf = Mod_PointInLeaf( pvsOrigin2, rsh.worldModel );
		viewcluster2 = leaf->cluster;
		if( viewcluster2 > -1 && viewcluster2 != rf.viewcluster && !( pvs[viewcluster2>>3] & ( 1<<( viewcluster2&7 ) ) ) )
		{
			memcpy( fatpvs, pvs, ( rsh.worldBrushModel->pvs->numclusters + 7 ) / 8 ); // same as pvs->rowsize
			pvs = Mod_ClusterPVS( viewcluster2, rsh.worldModel );
			c = ( rsh.worldBrushModel->pvs->numclusters + 31 ) / 32;
			for( i = 0; i < c; i++ )
				(( int * )fatpvs)[i] |= (( int * )pvs)[i];
			pvs = fatpvs;
		}
	}

	for( pleaf = rsh.worldBrushModel->visleafs, leaf = *pleaf; leaf; leaf = *pleaf++ )
	{
		cluster = leaf->cluster;

		// check for door connected areas
		if( areabits )
		{
			if( leaf->area < 0 || !( areabits[leaf->area>>3] & ( 1<<( leaf->area&7 ) ) ) )
				continue; // not visible
		}

		if( pvs[cluster>>3] & ( 1<<( cluster&7 ) ) )
		{
			node = (mnode_t *)leaf;
			do
			{
				if( node->pvsframe == rf.pvsframecount )
					break;
				node->pvsframe = rf.pvsframecount;
				node = node->parent;
			}
			while( node );
		}
	}
}
