/*
Copyright (C) 2007 Victor Luchits

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

FRUSTUM AND PVS CULLING

=============================================================
*/

/*
* R_SetupFrustum
*/
void R_SetupFrustum( const refdef_t *rd, float farClip, cplane_t *frustum )
{
	int i;
	vec3_t forward, left, up;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip

	VectorCopy( &rd->viewaxis[AXIS_FORWARD], forward );
	VectorCopy( &rd->viewaxis[AXIS_RIGHT], left );
	VectorCopy( &rd->viewaxis[AXIS_UP], up );

	if( rd->rdflags & RDF_USEORTHO ) {
		VectorNegate( left, frustum[0].normal );
		VectorCopy( left, frustum[1].normal );
		VectorNegate( up, frustum[2].normal );
		VectorCopy( up, frustum[3].normal );

		for( i = 0; i < 4; i++ ) {
			frustum[i].type = PLANE_NONAXIAL;
			frustum[i].dist = DotProduct( rd->vieworg, frustum[i].normal );
			frustum[i].signbits = SignbitsForPlane( &frustum[i] );
		}

		frustum[0].dist -= rd->ortho_x;
		frustum[1].dist -= rd->ortho_x;
		frustum[2].dist -= rd->ortho_y;
		frustum[3].dist -= rd->ortho_y;
	}
	else {
		vec3_t right;

		VectorNegate( left, right );
		// rotate rn.vpn right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, up, forward, -( 90-rd->fov_x / 2 ) );
		// rotate rn.vpn left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, up, forward, 90-rd->fov_x / 2 );
		// rotate rn.vpn up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, right, forward, 90-rd->fov_y / 2 );
		// rotate rn.vpn down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, right, forward, -( 90 - rd->fov_y / 2 ) );

		for( i = 0; i < 4; i++ ) {
			frustum[i].type = PLANE_NONAXIAL;
			frustum[i].dist = DotProduct( rd->vieworg, frustum[i].normal );
			frustum[i].signbits = SignbitsForPlane( &frustum[i] );
		}
	}

	// farclip
	VectorNegate( forward, frustum[4].normal );
	frustum[4].type = PLANE_NONAXIAL;
	frustum[4].dist = DotProduct( rd->vieworg, frustum[4].normal ) - farClip;
	frustum[4].signbits = SignbitsForPlane( &frustum[4] );
}

/*
* R_CullBox
* 
* Returns true if the box is completely outside the frustum
*/
bool R_CullBox( const vec3_t mins, const vec3_t maxs, const unsigned int clipflags )
{
	unsigned int i, bit;
	const cplane_t *p;

	if( r_nocull->integer )
		return false;

	for( i = sizeof( rn.frustum )/sizeof( rn.frustum[0] ), bit = 1, p = rn.frustum; i > 0; i--, bit<<=1, p++ )
	{
		if( !( clipflags & bit ) )
			continue;

		switch( p->signbits )
		{
		case 0:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 1:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 2:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 3:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist )
				return true;
			break;
		case 4:
			if( p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 5:
			if( p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 6:
			if( p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		case 7:
			if( p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist )
				return true;
			break;
		default:
			assert( 0 );
			return false;
		}
	}

	return false;
}

/*
* R_CullSphere
* 
* Returns true if the sphere is completely outside the frustum
*/
bool R_CullSphere( const vec3_t centre, const float radius, const unsigned int clipflags )
{
	unsigned int i;
	unsigned int bit;
	const cplane_t *p;

	if( r_nocull->integer )
		return false;

	for( i = sizeof( rn.frustum )/sizeof( rn.frustum[0] ), bit = 1, p = rn.frustum; i > 0; i--, bit<<=1, p++ )
	{
		if( !( clipflags & bit ) )
			continue;
		if( DotProduct( centre, p->normal ) - p->dist <= -radius )
			return true;
	}

	return false;
}

/*
* R_VisCullBox
*/
bool R_VisCullBox( const vec3_t mins, const vec3_t maxs )
{
	int s, stackdepth = 0;
	vec3_t extmins, extmaxs;
	mnode_t *node, *localstack[2048];

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return false;
	if( rn.renderFlags & RF_NOVIS )
		return false;

	for( s = 0; s < 3; s++ )
	{
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = rsh.worldBrushModel->nodes;; )
	{
		if( !node->plane )
		{
			/*if( !rf.worldLeafVis[(mleaf_t *)node - rsh.worldBrushModel->leafs] )
			{
				if( !stackdepth )
					return true;
				node = localstack[--stackdepth];
				continue;
			}*/
			return false;
		}

		s = BOX_ON_PLANE_SIDE( extmins, extmaxs, node->plane ) - 1;
		if( s < 2 )
		{
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack )/sizeof( mnode_t * ) )
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return true;
}

/*
* R_VisCullSphere
*/
bool R_VisCullSphere( const vec3_t origin, float radius )
{
	float dist;
	int stackdepth = 0;
	mnode_t *node, *localstack[2048];

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return false;
	if( rn.renderFlags & RF_NOVIS )
		return false;

	radius += 4;
	for( node = rsh.worldBrushModel->nodes;; )
	{
		if( !node->plane )
		{
			/*if( !rf.worldLeafVis[(mleaf_t *)node - rsh.worldBrushModel->leafs] )
			{
				if( !stackdepth )
					return true;
				node = localstack[--stackdepth];
				continue;
			}*/
			return false;
		}

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius )
		{
			node = node->children[0];
			continue;
		}
		else if( dist < -radius )
		{
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack )/sizeof( mnode_t * ) )
			localstack[stackdepth++] = node->children[0];
		else
			assert( 0 );
		node = node->children[1];
	}

	return true;
}

/*
* R_CullModelEntity
*/
int R_CullModelEntity( const entity_t *e, vec3_t mins, vec3_t maxs, float radius, bool sphereCull, bool pvsCull )
{
	if( e->flags & RF_NOSHADOW )
	{
		if( rn.renderFlags & RF_SHADOWMAPVIEW )
			return 3;
	}

	if( e->flags & RF_WEAPONMODEL )
	{
		if( rn.renderFlags & RF_NONVIEWERREF )
			return 1;
		return 0;
	}

	if( e->flags & RF_VIEWERMODEL )
	{
		//if( !(rn.renderFlags & RF_NONVIEWERREF) )
		if( !( rn.renderFlags & ( RF_MIRRORVIEW|RF_SHADOWMAPVIEW ) ) )
			return 1;
	}

	if( e->flags & RF_NODEPTHTEST )
		return 0;

	// account for possible outlines
	if( e->outlineHeight )
		radius += e->outlineHeight * r_outlines_scale->value * 1.73/*sqrt(3)*/;

	if( sphereCull )
	{
		if( R_CullSphere( e->origin, radius, rn.clipFlags ) )
			return 1;
	}
	else
	{
		if( R_CullBox( mins, maxs, rn.clipFlags ) )
			return 1;
	}

	if( pvsCull )
	{
		if( sphereCull )
		{
			if( R_VisCullSphere( e->origin, radius ) )
				return 2;
		}
		else
		{
			if( R_VisCullBox( mins, maxs ) )
				return 2;
		}
	}

	return 0;
}
