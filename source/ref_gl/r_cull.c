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
* R_SetupSideViewFrustum
*/
void R_SetupSideViewFrustum( const refdef_t *rd, float nearClip, float farClip, cplane_t *frustum, int side )
{
	int i;
	float sign;
	int a0, a1, a2;
	vec3_t forward, left, up;

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - nearclip
	// 5 - farclip

	sign = side & 1 ? -1 : 1;
	a0 = (AXIS_FORWARD + 3 * (side >> 1)) % 12;
	a1 = (AXIS_RIGHT + 3 * (side >> 1)) % 12;
	a2 = (AXIS_UP + 3 * (side >> 1)) % 12;

	VectorCopy( &rd->viewaxis[a0], forward );
	VectorCopy( &rd->viewaxis[a1], left );
	VectorCopy( &rd->viewaxis[a2], up );

	VectorScale( forward, sign, forward );

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
	} else {
		vec3_t right;

		VectorNegate( left, right );

		// rotate rn.vpn right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, up, forward, -( 90 - rd->fov_x / 2 ) );
		// rotate rn.vpn left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, up, forward, 90 - rd->fov_x / 2 );
		// rotate rn.vpn up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, right, forward, 90 - rd->fov_y / 2 );
		// rotate rn.vpn down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, right, forward, -( 90 - rd->fov_y / 2 ) );

		for( i = 0; i < 4; i++ ) {
			frustum[i].type = PLANE_NONAXIAL;
			frustum[i].dist = DotProduct( rd->vieworg, frustum[i].normal );
			frustum[i].signbits = SignbitsForPlane( &frustum[i] );
		}
	}

	// near clip
	VectorCopy( forward, frustum[4].normal );
	frustum[4].type = PLANE_NONAXIAL;
	frustum[4].dist = DotProduct( rd->vieworg, frustum[4].normal ) + nearClip;
	frustum[4].signbits = SignbitsForPlane( &frustum[4] );

	// farclip
	VectorNegate( forward, frustum[5].normal );
	frustum[5].type = PLANE_NONAXIAL;
	frustum[5].dist = DotProduct( rd->vieworg, frustum[5].normal ) - farClip;
	frustum[5].signbits = SignbitsForPlane( &frustum[5] );
}

/*
* R_SetupFrustum
*/
void R_SetupFrustum( const refdef_t *rd, float nearClip, float farClip, cplane_t *frustum )
{
	R_SetupSideViewFrustum( rd, nearClip, farClip, frustum, 0 );
}

/*
* R_FogCull
*/
bool R_FogCull( const mfog_t *fog, vec3_t origin, float radius ) {
	// note that fog->distanceToEye < 0 is always true if
	// globalfog is not NULL and we're inside the world boundaries
	if( fog && fog->shader && fog == rn.fog_eye ) {
		float vpnDist = ( ( rn.viewOrigin[0] - origin[0] ) * rn.viewAxis[AXIS_FORWARD + 0] +
			( rn.viewOrigin[1] - origin[1] ) * rn.viewAxis[AXIS_FORWARD + 1] +
			( rn.viewOrigin[2] - origin[2] ) * rn.viewAxis[AXIS_FORWARD + 2] );
		return ( ( vpnDist + radius ) / fog->shader->fog_dist ) < -1;
	}

	return false;
}

/*
* R_CullBox
*
* Returns true if the box is completely outside the frustum
*/
bool R_CullBox( const vec3_t mins, const vec3_t maxs, const unsigned int clipflags ) {
	unsigned int i, bit;
	const cplane_t *p;

	if( r_nocull->integer ) {
		return false;
	}

	for( i = sizeof( rn.frustum ) / sizeof( rn.frustum[0] ), bit = 1, p = rn.frustum; i > 0; i--, bit <<= 1, p++ ) {
		if( !( clipflags & bit ) ) {
			continue;
		}

		switch( p->signbits & 7 ) {
			case 0:
				if( p->normal[0] * maxs[0] + p->normal[1] * maxs[1] + p->normal[2] * maxs[2] < p->dist ) {
					return true;
				}
				break;
			case 1:
				if( p->normal[0] * mins[0] + p->normal[1] * maxs[1] + p->normal[2] * maxs[2] < p->dist ) {
					return true;
				}
				break;
			case 2:
				if( p->normal[0] * maxs[0] + p->normal[1] * mins[1] + p->normal[2] * maxs[2] < p->dist ) {
					return true;
				}
				break;
			case 3:
				if( p->normal[0] * mins[0] + p->normal[1] * mins[1] + p->normal[2] * maxs[2] < p->dist ) {
					return true;
				}
				break;
			case 4:
				if( p->normal[0] * maxs[0] + p->normal[1] * maxs[1] + p->normal[2] * mins[2] < p->dist ) {
					return true;
				}
				break;
			case 5:
				if( p->normal[0] * mins[0] + p->normal[1] * maxs[1] + p->normal[2] * mins[2] < p->dist ) {
					return true;
				}
				break;
			case 6:
				if( p->normal[0] * maxs[0] + p->normal[1] * mins[1] + p->normal[2] * mins[2] < p->dist ) {
					return true;
				}
				break;
			case 7:
				if( p->normal[0] * mins[0] + p->normal[1] * mins[1] + p->normal[2] * mins[2] < p->dist ) {
					return true;
				}
				break;
			default:
				break;
		}
	}

	return false;
}

/*
* R_CullSphere
*
* Returns true if the sphere is completely outside the frustum
*/
bool R_CullSphere( const vec3_t centre, const float radius, const unsigned int clipflags ) {
	unsigned int i;
	unsigned int bit;
	const cplane_t *p;

	if( r_nocull->integer ) {
		return false;
	}

	for( i = sizeof( rn.frustum ) / sizeof( rn.frustum[0] ), bit = 1, p = rn.frustum; i > 0; i--, bit <<= 1, p++ ) {
		if( !( clipflags & bit ) ) {
			continue;
		}
		if( DotProduct( centre, p->normal ) - p->dist <= -radius ) {
			return true;
		}
	}

	return false;
}

/*
* R_VisCullBox
*/
bool R_VisCullBox( const vec3_t mins, const vec3_t maxs ) {
	int s, stackdepth = 0;
	vec3_t extmins, extmaxs;
	mnode_t *node, *localstack[2048];

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return false;
	}
	if( rn.renderFlags & RF_NOVIS ) {
		return false;
	}
	if( rn.viewcluster < 0 ) {
		return false;
	}

	for( s = 0; s < 3; s++ ) {
		extmins[s] = mins[s] - 4;
		extmaxs[s] = maxs[s] + 4;
	}

	for( node = rsh.worldBrushModel->nodes;; ) {
		if( !node->plane ) {
			unsigned j;
			mleaf_t *leaf = (mleaf_t *)node;

			for( j = 0; j < leaf->numVisSurfaces; j++ ) {
				if( rn.meshlist->worldSurfVis[leaf->visSurfaces[j]] )
					return false;
			}

			if( !stackdepth )
		        return true;
		    node = localstack[--stackdepth];
		    continue;
		}

		s = BOX_ON_PLANE_SIDE( extmins, extmaxs, node->plane ) - 1;
		if( s < 2 ) {
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack ) / sizeof( mnode_t * ) ) {
			localstack[stackdepth++] = node->children[0];
		}
		node = node->children[1];
	}

	return true;
}

/*
* R_VisCullSphere
*/
bool R_VisCullSphere( const vec3_t origin, float radius ) {
	float dist;
	int stackdepth = 0;
	mnode_t *node, *localstack[2048];

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return false;
	}
	if( rn.renderFlags & RF_NOVIS ) {
		return false;
	}
	if( rn.viewcluster < 0 ) {
		return false;
	}

	radius += 4;
	for( node = rsh.worldBrushModel->nodes;; ) {
		if( !node->plane ) {
			unsigned j;
			mleaf_t *leaf = (mleaf_t *)node;

			for( j = 0; j < leaf->numVisSurfaces; j++ ) {
				if( rn.meshlist->worldSurfVis[leaf->visSurfaces[j]] )
					return false;
			}

			if( !stackdepth )
				return true;
			node = localstack[--stackdepth];
			continue;
		}

		dist = PlaneDiff( origin, node->plane );
		if( dist > radius ) {
			node = node->children[0];
			continue;
		} else if( dist < -radius ) {
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < sizeof( localstack ) / sizeof( mnode_t * ) ) {
			localstack[stackdepth++] = node->children[0];
		}
		node = node->children[1];
	}

	return true;
}

/*
* R_CullModelEntity
*/
int R_CullModelEntity( const entity_t *e, bool pvsCull ) {
	const entSceneCache_t *cache = R_ENTCACHE( e );
	const vec_t *mins = cache->absmins;
	const vec_t *maxs = cache->absmaxs;
	float radius = cache->radius;
	bool sphereCull = cache->rotated;

	if( cache->mod_type == mod_bad ) {
		return false;
	}

	// account for possible outlines
	if( e->outlineHeight ) {
		radius += e->outlineHeight * r_outlines_scale->value * 1.733 /*sqrt(3)*/;
	}

	if( sphereCull ) {
		if( R_CullSphere( e->origin, radius, rn.clipFlags ) ) {
			return 1;
		}
	} else {
		if( R_CullBox( mins, maxs, rn.clipFlags ) ) {
			return 1;
		}
	}

	if( pvsCull ) {
		if( sphereCull ) {
			if( R_VisCullSphere( e->origin, radius ) ) {
				return 2;
			}
		} else {
			if( R_VisCullBox( mins, maxs ) ) {
				return 2;
			}
		}
	}

	return 0;
}

/*
* R_CullSpriteEntity
*/
int R_CullSpriteEntity( const entity_t *e ) {
	if( rn.renderFlags & RF_LIGHTVIEW ) {
		if( !R_ShaderNoDlight( e->customShader ) ) {
			return 1;
		}
	}
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		if( !R_ShaderNoShadow( e->customShader ) ) {
			return 1;
		}
	}
	return 0;
}
