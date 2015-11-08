/*
Copyright (C) 1997-2001 Id Software, Inc.

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

//
// tvm_clip.c - entity contact detection. (high level object sorting to reduce interaction tests)
//

#include "tvm_local.h"

#include "tvm_clip.h"

/*
* GClip_ClearWorld
* called after the world model has been loaded, before linking any entities
*/
void GClip_ClearWorld( tvm_relay_t *relay )
{
}

/*
* GClip_UnlinkEntity
* call before removing an entity, and before trying to move one,
* so it doesn't clip against itself
*/
void GClip_UnlinkEntity( tvm_relay_t *relay, edict_t *ent )
{
}

/*
* GClip_LinearMovement_
*
* FIXME: this is a copy&paste of GS_LinearMovement
*/
static int GClip_LinearMovement_( const entity_state_t *ent, unsigned time, vec3_t dest )
{
    vec3_t dist;
    int moveTime;
    float moveFrac;

    moveTime = time - ent->linearMovementTimeStamp;
    if( moveTime < 0 ) {
        moveTime = 0;
    }

    if( ent->linearMovementDuration ) {
        if( moveTime > (int)ent->linearMovementDuration ) {
            moveTime = ent->linearMovementDuration;
        }

        VectorSubtract( ent->linearMovementEnd, ent->linearMovementBegin, dist );
        moveFrac = (float)moveTime / (float)ent->linearMovementDuration;
        clamp( moveFrac, 0, 1 );
        VectorMA( ent->linearMovementBegin, moveFrac, dist, dest );
    }
    else {
        moveFrac = moveTime * 0.001f;
        VectorMA( ent->linearMovementBegin, moveFrac, ent->linearMovementVelocity, dest );
    }

    return moveTime;
}

/*
* GClip_LinearMovement
*/
void GClip_LinearMovement( tvm_relay_t *relay, edict_t *ent )
{
	vec3_t origin;
	GClip_LinearMovement_( &ent->s, relay->serverTime, origin );
	VectorCopy( origin, ent->s.origin );
}

/*
* GClip_LinkEntity
* Needs to be called any time an entity changes origin, mins, maxs,
* or solid. Automatically unlinks if needed.
* sets ent->v.absmin and ent->v.absmax
* sets ent->leafnums[] for pvs determination even if the entity
* is not solid
*/
#define MAX_TOTAL_ENT_LEAFS	128
void GClip_LinkEntity( tvm_relay_t *relay, edict_t *ent )
{
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int clusters[MAX_TOTAL_ENT_LEAFS];
	int num_leafs;
	int i, j;
	int area;
	int topnode;

	if( ent->r.area.prev )
		GClip_UnlinkEntity( relay, ent ); // unlink from old position

	if( ent == ent->relay->edicts )
		return; // don't add the world

	if( !ent->r.inuse )
		return;

	// set the size
	VectorSubtract( ent->r.maxs, ent->r.mins, ent->r.size );

	// set the abs box
	if( ent->s.solid == SOLID_BMODEL &&
		( ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2] ) )
	{ // expand for rotation
		float radius;

		radius = RadiusFromBounds( ent->r.mins, ent->r.maxs );

		for( i = 0; i < 3; i++ )
		{
			ent->r.absmin[i] = ent->s.origin[i] - radius;
			ent->r.absmax[i] = ent->s.origin[i] + radius;
		}
	}
	else
	{ // normal
		VectorAdd( ent->s.origin, ent->r.mins, ent->r.absmin );
		VectorAdd( ent->s.origin, ent->r.maxs, ent->r.absmax );
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->r.absmin[0] -= 1;
	ent->r.absmin[1] -= 1;
	ent->r.absmin[2] -= 1;
	ent->r.absmax[0] += 1;
	ent->r.absmax[1] += 1;
	ent->r.absmax[2] += 1;

	// link to PVS leafs
	ent->r.num_clusters = 0;
	ent->r.areanum = ent->r.areanum2 = -1;

	// get all leafs, including solids
	num_leafs = trap_CM_BoxLeafnums( relay, ent->r.absmin, ent->r.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// set areas
	for( i = 0; i < num_leafs; i++ )
	{
		clusters[i] = trap_CM_LeafCluster( relay, leafs[i] );
		area = trap_CM_LeafArea( relay, leafs[i] );
		if( area > -1 )
		{ // doors may legally straggle two areas,
			// but nothing should ever need more than that
			if( ent->r.areanum > -1 && ent->r.areanum != area )
			{
				if( ent->r.areanum2 > -1 && ent->r.areanum2 != area )
				{
					TVM_Printf( "Object touching 3 areas at %f %f %f\n", ent->r.absmin[0], ent->r.absmin[1],
						ent->r.absmin[2] );
				}
				ent->r.areanum2 = area;
			}
			else
				ent->r.areanum = area;
		}
	}

	if( num_leafs >= MAX_TOTAL_ENT_LEAFS )
	{ // assume we missed some leafs, and mark by headnode
		ent->r.num_clusters = -1;
		ent->r.headnode = topnode;
	}
	else
	{
		ent->r.num_clusters = 0;
		for( i = 0; i < num_leafs; i++ )
		{
			if( clusters[i] == -1 )
				continue; // not a visible leaf
			for( j = 0; j < i; j++ )
				if( clusters[j] == clusters[i] )
					break;
			if( j == i )
			{
				if( ent->r.num_clusters == MAX_ENT_CLUSTERS )
				{ // assume we missed some leafs, and mark by headnode
					ent->r.num_clusters = -1;
					ent->r.headnode = topnode;
					break;
				}

				ent->r.clusternums[ent->r.num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if( !ent->r.linkcount && !( ent->r.svflags & SVF_TRANSMITORIGIN2 ) )
	{
		VectorCopy( ent->s.origin, ent->s.old_origin );
		//ent->olds = ent->s;
	}
	ent->r.linkcount++;
}
