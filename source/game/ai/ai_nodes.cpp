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
--------------------------------------------------------------
The ACE Bot is a product of Steve Yeager, and is available from
the ACE Bot homepage, at http://www.axionfx.com/ace.

This program is a modification of the ACE Bot, and is therefore
in NO WAY supported by Steve Yeager.
*/

#include "ai_local.h"
#include "aas.h"
#include "static_vector.h"

//===========================================================
//
//				NODES
//
//===========================================================

/*
* AI_PredictJumpadDestity
* Make a guess on where a jumpad will send the player.
*/
static bool AI_PredictJumpadDestity( edict_t *ent, vec3_t out )
{
	int i;
	edict_t *target;
	trace_t	trace;
	vec3_t pad_origin, v1, v2;
	float htime, vtime, tmpfloat, player_factor;
	vec3_t floor_target_origin, target_origin;
	vec3_t floor_dist_vec, floor_movedir;

	VectorClear( out );

	if( !ent->target )
		return false;

	// get target entity
	target = G_Find( NULL, FOFS( targetname ), ent->target );
	if( !target )
		return false;

	// find pad origin
	VectorCopy( ent->r.maxs, v1 );
	VectorCopy( ent->r.mins, v2 );
	pad_origin[0] = ( v1[0] - v2[0] ) / 2 + v2[0];
	pad_origin[1] = ( v1[1] - v2[1] ) / 2 + v2[1];
	pad_origin[2] = ent->r.maxs[2];

	//make a projection 'on floor' of target origin
	VectorCopy( target->s.origin, target_origin );
	VectorCopy( target->s.origin, floor_target_origin );
	floor_target_origin[2] = pad_origin[2]; //put at pad's height

	//make a guess on how player movement will affect the trajectory
	tmpfloat = DistanceFast( pad_origin, floor_target_origin );
	htime = sqrt( ( tmpfloat ) );
	vtime = sqrt( ( target->s.origin[2] - pad_origin[2] ) );
	if( !vtime ) return false;
	htime *= 4; vtime *= 4;
	if( htime > vtime )
		htime = vtime;
	player_factor = vtime - htime;

	// find distance vector, on floor, from pad_origin to target origin.
	for( i = 0; i < 3; i++ )
		floor_dist_vec[i] = floor_target_origin[i] - pad_origin[i];

	// movement direction on floor
	VectorCopy( floor_dist_vec, floor_movedir );
	VectorNormalize( floor_movedir );

	// move both target origin and target origin on floor by player movement factor.
	VectorMA( target_origin, player_factor, floor_movedir, target_origin );
	VectorMA( floor_target_origin, player_factor, floor_movedir, floor_target_origin );

	// move target origin on floor by floor distance, and add another player factor step to it
	VectorMA( floor_target_origin, 1, floor_dist_vec, floor_target_origin );
	VectorMA( floor_target_origin, player_factor, floor_movedir, floor_target_origin );

#ifdef SHOW_JUMPAD_GUESS
	// this is our top of the curve point, and the original target
	AI_JumpadGuess_ShowPoint( target_origin, PATH_AMMO_BOX_MODEL );
	AI_JumpadGuess_ShowPoint( target->s.origin, PATH_AMMO_BOX_MODEL );
#endif

	//trace from target origin to endPoint.
	G_Trace( &trace, target_origin, tv( -15, -15, -8 ), tv( 15, 15, 8 ), floor_target_origin, NULL, MASK_NODESOLID );
	if( ( trace.fraction == 1.0 && trace.startsolid ) || ( trace.allsolid && trace.startsolid ) )
	{
		G_Printf( "JUMPAD LAND: ERROR: trace was in solid.\n" ); //started inside solid (target should never be inside solid, this is a mapper error)
		return false;
	}
	else if( trace.fraction == 1.0 )
	{

		//didn't find solid. Extend Down (I have to improve this part)
		vec3_t target_origin2, extended_endpoint, extend_dist_vec;

		VectorCopy( floor_target_origin, target_origin2 );
		for( i = 0; i < 3; i++ )
			extend_dist_vec[i] = floor_target_origin[i] - target_origin[i];

		VectorMA( target_origin2, 1, extend_dist_vec, extended_endpoint );

		G_Trace( &trace, target_origin2, tv( -15, -15, -8 ), tv( 15, 15, 8 ), extended_endpoint, NULL, MASK_NODESOLID );
		if( trace.fraction == 1.0 )
			return false; //still didn't find solid
	}

#ifdef SHOW_JUMPAD_GUESS
	// destiny found
	AI_JumpadGuess_ShowPoint( trace.endpos, PATH_AMMO_BOX_MODEL );
#endif

	VectorCopy( trace.endpos, out );
	return true;
}

GoalEntitiesRegistry GoalEntitiesRegistry::instance;

void GoalEntitiesRegistry::Init()
{
	memset(goalEnts, 0, sizeof(goalEnts));
	memset(entGoals, 0, sizeof(entGoals));

	goalEntsFree = goalEnts;
	goalEntsHeadnode.id = -1;
	goalEntsHeadnode.ent = game.edicts;
	goalEntsHeadnode.aasAreaNum = 0;
	goalEntsHeadnode.prev = &goalEntsHeadnode;
	goalEntsHeadnode.next = &goalEntsHeadnode;
	int i;
	for (i = 0; i < sizeof(goalEnts)/sizeof(goalEnts[0]) - 1; i++)
	{
		goalEnts[i].id = i;
		goalEnts[i].next = &goalEnts[i+1];
	}
	goalEnts[i].id = i;
	goalEnts[i].next = NULL;
}

NavEntity *GoalEntitiesRegistry::AllocGoalEntity()
{
	if (!goalEntsFree)
		return nullptr;

	// take a free decal if possible
	NavEntity *goalEnt = goalEntsFree;
	goalEntsFree = goalEnt->next;

	// put the decal at the start of the list
	goalEnt->prev = &goalEntsHeadnode;
	goalEnt->next = goalEntsHeadnode.next;
	goalEnt->next->prev = goalEnt;
	goalEnt->prev->next = goalEnt;

	return goalEnt;
}

NavEntity *GoalEntitiesRegistry::AddGoalEntity(edict_t *ent, int aasAreaNum, int aasAreaNodeFlags/*=0*/)
{
	if (aasAreaNum == 0) abort();
	NavEntity *goalEnt = AllocGoalEntity();
	if (goalEnt)
	{
		goalEnt->ent = ent;
		goalEnt->aasAreaNum = aasAreaNum;
		goalEnt->aasAreaNodeFlags = aasAreaNodeFlags;
		entGoals[ENTNUM(ent)] = goalEnt;
	}
	return goalEnt;
}

void GoalEntitiesRegistry::RemoveGoalEntity(NavEntity *navEntity)
{
	edict_t *ent = navEntity->ent;
	FreeGoalEntity(navEntity);
	entGoals[ENTNUM(ent)] = nullptr;
}

void GoalEntitiesRegistry::FreeGoalEntity(NavEntity *goalEnt)
{
	// remove from linked active list
	goalEnt->prev->next = goalEnt->next;
	goalEnt->next->prev = goalEnt->prev;

	// insert into linked free list
	goalEnt->next = goalEntsFree;
	goalEntsFree = goalEnt;
}

static int EntAASAreaNum(edict_t *ent)
{
	if (!ent || ent->r.client)
		return 0;

	int areaNum = AAS_PointAreaNum(ent->s.origin);
	if (!areaNum)
	{
		vec3_t point;
		VectorCopy(ent->s.origin, point);
		point[2] += 8;
		areaNum = AAS_PointAreaNum(point);
		if (!areaNum)
		{
			point[2] +=  8;
			areaNum = AAS_PointAreaNum(point);
		}
	}
	return areaNum;
}

/*
* AI_AddGoalEntityNode
*/
static void AI_AddGoalEntityNode(edict_t *ent, bool customReach)
{
	int aasAreaNum = 0;

	if (!ent->r.inuse || !ent->classname)
		return;

	if (Ai::GetGoalentForEnt(ent) != NULL)
		return;

	if (ent->r.client)
	{
		// do not even add clients
		//GoalEntitiesRegistry::Instance()->AddGoalEntity(ent, 0);
		return;
	}

	//
	// Goal entities
	//

	// if we have a available node close enough to the entity, use it
	aasAreaNum = EntAASAreaNum(ent);
	if (aasAreaNum != 0)
	{
		//TODO:
		/*
		if (nodes[node].flags & NODE_MASK_NOREUSE)
			node = NODE_INVALID;
		else
		{
			float heightdiff = fabs( ent->s.origin[2] - nodes[node].origin[2] );

			if( heightdiff > AI_STEPSIZE + 8 )  // not near enough
				node = NODE_INVALID;
		}*/
	}

	// link the node is spawned after the load process is done
	if (AAS_Loaded())
	{
		if (aasAreaNum == 0) // (BY NOW) can not create new nodes after the initialization was done
			return;
	}
	else
	{
		// didn't find a node we could reuse
		if (aasAreaNum == 0)
			; // TODO: wtf?
	}

	if (aasAreaNum != 0)
	{
		int aasAreaNodeFlags = 0;
		if (customReach)
		{
			aasAreaNodeFlags |= NODEFLAGS_ENTITYREACH;
		}
		GoalEntitiesRegistry::Instance()->AddGoalEntity(ent, aasAreaNum, aasAreaNodeFlags);
	}
}

/*
* AI_AddGoalEntity
*/
void AI_AddGoalEntity( edict_t *ent )
{
	AI_AddGoalEntityNode( ent, false );
}

/*
* AI_AddGoalEntityCustom
*/
void AI_AddGoalEntityCustom( edict_t *ent )
{
	AI_AddGoalEntityNode( ent, true );
}

/*
* AI_RemoveGoalEntity
*/
void AI_RemoveGoalEntity( edict_t *ent )
{
	NavEntity *goalEnt = Ai::GetGoalentForEnt( ent );
	if (!goalEnt)
		return;

	GoalEntitiesRegistry::Instance()->RemoveGoalEntity(goalEnt);
}

/*************************************************************************/

/*
* AI_InitEntitiesData
*/
void AI_InitEntitiesData( void )
{
	if (!AAS_Loaded())
	{
		if (g_numbots->integer)
		{
			trap_Cvar_Set("g_numbots", "0");
		}
		return;
	}

	GoalEntitiesRegistry::Instance()->Init();

	for (int i = 0; i < MAX_EDICTS; ++i)
	{
		edict_t *ent = game.edicts + i;
		if (!ent->r.inuse || ent->r.client)
			continue;
		int aasAreaNum = EntAASAreaNum(ent);
		if (aasAreaNum)
			AI_AddGoalEntityNode(ent, ent->item != nullptr);
	}
}

