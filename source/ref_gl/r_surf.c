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
qboolean R_SurfPotentiallyVisible( const msurface_t *surf )
{
	if( surf->flags & SURF_NODRAW )
		return qfalse;
	if( !surf->mesh )
		return qfalse;
	return qtrue;
}

/*
* R_SurfPotentiallyShadowed
*/
qboolean R_SurfPotentiallyShadowed( const msurface_t *surf )
{
	if( surf->flags & ( SURF_SKY|SURF_NODLIGHT|SURF_NODRAW ) )
		return qfalse;
	if( ( surf->shader->sort >= SHADER_SORT_OPAQUE ) && ( surf->shader->sort <= SHADER_SORT_ALPHATEST ) ) {
		return qtrue;
	}
	return qfalse;
}

/*
* R_SurfPotentiallyLit
*/
qboolean R_SurfPotentiallyLit( const msurface_t *surf )
{
	const shader_t *shader;

	if( surf->flags & ( SURF_SKY|SURF_NODLIGHT|SURF_NODRAW ) )
		return qfalse;
	shader = surf->shader;
	if( ( shader->flags & SHADER_SKY ) || !shader->numpasses )
		return qfalse;
	return ( surf->mesh != NULL /* && (surf->facetype != FACETYPE_TRISURF)*/ );
}

/*
* R_CullSurface
*/
qboolean R_CullSurface( const entity_t *e, const msurface_t *surf, unsigned int clipflags )
{
	const shader_t *shader = surf->shader;

	if( r_nocull->integer )
		return qfalse;
	if( ( shader->flags & SHADER_ALLDETAIL ) && !r_detailtextures->integer )
		return qtrue;

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
					if( BOX_ON_PLANE_SIDE( grp->visMins, grp->visMaxs, surf->plane ) == 3 ) {
						// crossed by plane
						surfShadowBits |= bit;
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
qboolean R_DrawBSPSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceBSP_t *drawSurf )
{
	vboSlice_t *slice;

	slice = R_GetVBOSlice( drawSurf - rsh.worldBrushModel->drawSurfaces );
	assert( slice != NULL );

	RB_BindVBO( drawSurf->vbo->index, GL_TRIANGLES );

	if( drawSurf->dlightFrame == rsc.frameCount ) {
		RB_SetDlightBits( drawSurf->dlightBits & rn.dlightBits );
	}
	else {
		RB_SetDlightBits( 0 );
	}

	if( drawSurf->shadowFrame == rsc.frameCount ) {
		RB_SetShadowBits( drawSurf->shadowBits & rn.shadowBits );
	}
	else {
		RB_SetShadowBits( 0 );
	}

	RB_SetLightstyle( drawSurf->superLightStyle );

	if( drawSurf->numInstances ) {
		RB_DrawElementsInstanced( slice->firstVert, slice->numVerts, slice->firstElem, slice->numElems, 
			drawSurf->numInstances, drawSurf->instances );
	}
	else {
		RB_DrawElements( slice->firstVert, slice->numVerts, slice->firstElem, slice->numElems );
	}

	return qfalse;
}

/*
* R_AddSurfaceToDrawList
*/
static void R_AddSurfaceToDrawList( const entity_t *e, const msurface_t *surf, const mfog_t *fog,
	unsigned int clipFlags, unsigned int dlightBits, unsigned shadowBits, float dist )
{
	int order = 0;
	shader_t *shader;
	drawSurfaceBSP_t *drawSurf;

	if( R_CullSurface( e, surf, clipFlags ) ) {
		return;
	}

	if( r_drawworld->integer == 2 ) {
		shader = rsh.envShader;
	} else {
		shader = surf->shader;

		if( shader->flags & SHADER_SKY ) {
			if( !R_FASTSKY() ) {
				R_AddSkyToDrawList( surf );
				rn.numVisSurfaces++;
			}
			return;
		}
	}

	drawSurf = surf->drawSurf;
	if( drawSurf->visFrame != rf.frameCount ) {
		portalSurface_t *portalSurface = NULL;

		if( shader->flags & SHADER_PORTAL ) {
			// draw portals in front-to-back order
			dist = 1024 - dist / 100.0f; 
			if( dist < 1 ) dist = 1;

			portalSurface = R_AddPortalSurface( e, surf->mesh, surf->mins, surf->maxs, shader );
		}

		drawSurf->visFrame = rf.frameCount;

		if( !R_AddDSurfToDrawList( e, fog, shader, dist, order, portalSurface, drawSurf ) ) {
			return;
		}
	}

	// keep track of the actual vbo chunk we need to render
	R_AddVBOSlice( drawSurf - rsh.worldBrushModel->drawSurfaces, 
		surf->mesh->numVerts, surf->mesh->numElems,
		surf->firstDrawSurfVert, surf->firstDrawSurfElem );

	// dynamic lights that affect the surface
	if( dlightBits && R_SurfPotentiallyLit( surf ) ) {
		// ignore dlights that have already been marked as affectors
		if( drawSurf->dlightFrame == rsc.frameCount ) {
			drawSurf->dlightBits |= dlightBits;
		} else {
			drawSurf->dlightBits = dlightBits;
			drawSurf->dlightFrame = rsc.frameCount;
		}
	}

	// shadows that are projected onto the surface
	if( shadowBits && R_SurfPotentiallyShadowed( surf ) ) {
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
float R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, qboolean *rotated )
{
	int i;
	const model_t	*model = e->model;

	if( !Matrix3_Compare( e->axis, axis_identity ) )
	{
		if( rotated )
			*rotated = qtrue;
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
			*rotated = qfalse;
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
qboolean R_AddBrushModelToDrawList( const entity_t *e )
{
	unsigned int i;
	vec3_t origin;
	vec3_t bmins, bmaxs;
	qboolean rotated;
	model_t	*model = e->model;
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
	msurface_t *surf;
	mfog_t *fog;
	float radius, distance;
	unsigned int bit, fullBits;
	unsigned int dlightBits, shadowBits;

	if( bmodel->nummodelsurfaces == 0 ) {
		return qfalse;
	}

	radius = R_BrushModelBBox( e, bmins, bmaxs, &rotated );

	if( R_CullModelEntity( e, bmins, bmaxs, radius, rotated ) ) {
		return qfalse;
	}

	// never render weapon models or non-occluders into shadowmaps
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		if( rsc.entShadowGroups[R_ENT2NUM(e)] != rn.shadowGroup->id ) {
			return qtrue;
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
		if( !surf->drawSurf ) {
			continue;
		}
		if( surf->visFrame != rf.frameCount ) {
			surf->visFrame = rf.frameCount;
			R_AddSurfaceToDrawList( e, surf, fog, 0, dlightBits, shadowBits, distance );
		}
	}

	return qtrue;
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
	unsigned int newDlightBits, newShadowBits;
	drawSurfaceBSP_t *drawSurf;
	vec3_t centre;
	float distance;

	do
	{
		surf = *mark++;
		drawSurf = surf->drawSurf;

		// avoid double-checking dlights that have already been added to drawSurf
		newDlightBits = dlightBits;
		if( drawSurf->dlightFrame == rsc.frameCount ) {
			newDlightBits &= ~drawSurf->dlightBits;
		}
		if( newDlightBits ) {
			newDlightBits = R_SurfaceDlightBits( surf, newDlightBits );
		}

		// avoid double-checking shadows that have already been added to drawSurf
		newShadowBits = shadowBits;
		if( drawSurf->shadowFrame == rsc.frameCount ) {
			newShadowBits &= ~drawSurf->shadowBits;
		}
		if( newShadowBits ) {
			newShadowBits = R_SurfaceShadowBits( surf, newShadowBits );
		}

		if( surf->visFrame != rf.frameCount || newDlightBits || newShadowBits ) {
			VectorAdd( surf->mins, surf->maxs, centre );
			VectorScale( centre, 0.5, centre );
			distance = Distance( rn.refdef.vieworg, centre );

			R_AddSurfaceToDrawList( rsc.worldent, surf, surf->fog, clipFlags, 
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
			unsigned int checkBits = shadowBits;

			for( i = 0; i < rsc.numShadowGroups; i++ )
			{
				shadowGroup_t *group = rsc.shadowGroups + i;
				bit = group->bit;
				if( checkBits & bit )
				{
					int clipped = BOX_ON_PLANE_SIDE( group->visMins, group->visMaxs, node->plane );
					if( !(clipped & 1) )
						shadowBits &= ~bit;
					if( clipped & 2 )
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
}

//==================================================================================

/*
* R_DrawWorld
*/
void R_DrawWorld( void )
{
	int clipFlags, msec = 0;
	unsigned int dlightBits;
	unsigned int shadowBits;

	if( !r_drawworld->integer )
		return;
	if( !rsh.worldModel )
		return;
	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;

	VectorCopy( rn.refdef.vieworg, modelOrg );

	if( (rn.refdef.rdflags & RDF_WORLDOUTLINES) && (rf.viewcluster != -1) && r_outlines_scale->value > 0 )
		rsc.worldent->outlineHeight = max( 0.0f, r_outlines_world->value );
	else
		rsc.worldent->outlineHeight = 0;
	Vector4Copy( mapConfig.outlineColor, rsc.worldent->outlineColor );

	if( r_nocull->integer )
		clipFlags = 0;
	else
		clipFlags = rn.clipFlags;

	// dynamic lights
	if( r_dynamiclight->integer != 1 || r_fullbright->integer || rn.renderFlags & RF_ENVVIEW ) {
		dlightBits = 0;
	} else {
		dlightBits = rsc.numDlights < 32 ? ( 1 << rsc.numDlights ) - 1 : ~0;
	}

	// shadowmaps
	if( rn.renderFlags & RF_ENVVIEW ) {
		shadowBits = 0;
	}
	else {
		shadowBits = rsc.numShadowGroups < 32 ? ( 1 << rsc.numShadowGroups ) - 1 : ~0;
	}

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
	qbyte *pvs;
	unsigned int i;
	int rdflags;
	mleaf_t	*leaf, **pleaf;
	mnode_t *node;
	qbyte *areabits;
	int cluster;
	qbyte fatpvs[MAX_MAP_LEAFS/8];

	rdflags = rn.refdef.rdflags;
	if( rdflags & RDF_NOWORLDMODEL )
		return;
	if( rf.oldviewcluster == rf.viewcluster && ( rdflags & RDF_OLDAREABITS ) 
		&& !(rn.renderFlags & RF_NOVIS) && rf.viewcluster != -1 && rf.oldviewcluster != -1 )
		return;
	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;
	if( !rsh.worldModel )
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if( r_lockpvs->integer )
		return;

	rf.pvsframecount++;
	rf.oldviewcluster = rf.viewcluster;

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
		areabits = rn.refdef.areabits + rf.viewarea * ((rsh.worldBrushModel->numareas+7)/8);
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
