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

#include "../g_local.h"
#include "ai_local.h"


//ACE

nav_plink_t pLinks[MAX_NODES];      // pLinks array
nav_node_t nodes[MAX_NODES];        // nodes array


//===========================================================
//
//				NODES
//
//===========================================================

/*
* AI_FlagsForNode
* check the world and set up node flags
*/
int AI_FlagsForNode( vec3_t origin, edict_t *passent )
{
	trace_t	trace;
	int flagsmask = 0;
	int contents;

	contents = G_PointContents( origin );
	//water
	if( contents & MASK_WATER )
		flagsmask |= NODEFLAGS_WATER;

	if( contents & CONTENTS_DONOTENTER )
		flagsmask |= NODEFLAGS_DONOTENTER;

	//floor
	G_Trace( &trace, origin, tv( -15, -15, 0 ), tv( 15, 15, 0 ), tv( origin[0], origin[1], origin[2] - AI_JUMPABLE_HEIGHT ), passent, MASK_NODESOLID );
	if( trace.fraction < 1.0 )
		flagsmask &= ~NODEFLAGS_FLOAT; //ok, it wasn't set, I know...
	else
		flagsmask |= NODEFLAGS_FLOAT;

	//ladder
	//	G_Trace( &trace, origin, tv(-18, -18, -16), tv(18, 18, 16), origin, passent, MASK_ALL );
	//	if( trace.startsolid && trace.contents & CONTENTS_LADDER )
	//		flagsmask |= NODEFLAGS_LADDER;

	return flagsmask;
}

//#define SHOW_JUMPAD_GUESS
#ifdef SHOW_JUMPAD_GUESS
/*
* just a debug tool
*/
static void AI_JumpadGuess_ShowPoint( vec3_t origin, char *modelname )
{
	edict_t	*ent;

	ent = G_Spawn();
	VectorCopy( origin, ent->s.origin );
	ent->movetype = MOVETYPE_NONE;
	ent->r.clipmask = MASK_WATER;
	ent->r.solid = SOLID_NOT;
	ent->s.type = ET_GENERIC;
	ent->s.renderfx |= RF_NOSHADOW;
	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	ent->s.modelindex = trap_ModelIndex( modelname );
	ent->nextThink = level.time + 20000000;
	ent->think = G_FreeEdict;
	ent->classname = "checkent";
	ent->r.svflags &= ~SVF_NOCLIENT;

	GClip_LinkEntity( ent );
}
#endif

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


/*
* AI_AddNode_JumpPad
* Drop two nodes, one at jump pad and other
* at predicted destity
*/
static int AI_AddNode_JumpPad( edict_t *ent )
{
	vec3_t v1, v2;
	vec3_t out;
	int closest_node;

	if( nav.num_nodes + 1 > MAX_NODES )
		return NODE_INVALID;

	if( !AI_PredictJumpadDestity( ent, out ) )
		return NODE_INVALID;

	// jumpad node
	nodes[nav.num_nodes].flags = ( NODEFLAGS_JUMPPAD|NODEFLAGS_SERVERLINK|NODEFLAGS_REACHATTOUCH );

	// find the origin
	VectorCopy( ent->r.maxs, v1 );
	VectorCopy( ent->r.mins, v2 );
	nodes[nav.num_nodes].origin[0] = ( v1[0] - v2[0] ) / 2 + v2[0];
	nodes[nav.num_nodes].origin[1] = ( v1[1] - v2[1] ) / 2 + v2[1];
	nodes[nav.num_nodes].origin[2] = ent->r.maxs[2]/* + 16*/; // raise it up a bit

	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );

	closest_node = AI_FindClosestReachableNode( nodes[nav.num_nodes].origin, ent, 64, NODE_ALL );

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );
	nav.num_nodes++;

	// link jumppad to closest walkable node
	if( closest_node != -1 )
		AI_AddLink( closest_node, nav.num_nodes-1, LINK_JUMPPAD );

	// Destiny node
	nodes[nav.num_nodes].flags = ( NODEFLAGS_JUMPPAD_LAND|NODEFLAGS_SERVERLINK );
	nodes[nav.num_nodes].origin[0] = out[0];
	nodes[nav.num_nodes].origin[1] = out[1];
	nodes[nav.num_nodes].origin[2] = out[2];
	AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL );

	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );

	// link jumpad to dest
	AI_AddLink( nav.num_nodes-1, nav.num_nodes, LINK_JUMPPAD );

	closest_node = AI_FindClosestReachableNode( nodes[nav.num_nodes].origin, ent, 64, NODE_ALL );

	nav.num_nodes++;

	// link jumppad destination to closest walkable node
	if( closest_node != -1 )
		AI_AddLink( closest_node, nav.num_nodes-1, LINK_JUMPPAD );

	return nav.num_nodes - 1;
}


/*
* AI_AddNode_Door
* Drop a node at each side of the door
* and force them to link. Only typical
* doors are covered.
*/
static int AI_AddNode_Door( edict_t *ent )
{
	edict_t	*other;
	vec3_t mins, maxs;
	vec3_t door_origin, movedir, moveangles;
	mat3_t moveaxis;
	vec3_t MOVEDIR_UP = { 0, 0, 1 };
	float nodeOffset = NODE_DENSITY * 0.75f;
	int i, j;
	int dropped[4];

	if( ent->flags & FL_TEAMSLAVE )
		return NODE_INVALID; // only team master will drop the nodes

	for( i = 0; i < 4; i++ )
		dropped[i] = NODE_INVALID;

	//make box formed by all team members boxes
	VectorCopy( ent->r.absmin, mins );
	VectorCopy( ent->r.absmax, maxs );

	for( other = ent->teamchain; other; other = other->teamchain )
	{
		AddPointToBounds( other->r.absmin, mins, maxs );
		AddPointToBounds( other->r.absmax, mins, maxs );
	}

	for( i = 0; i < 3; i++ )
		door_origin[i] = ( maxs[i] + mins[i] ) * 0.5;

	VectorSubtract( ent->moveinfo.end_origin, ent->moveinfo.start_origin, movedir );
	VectorNormalizeFast( movedir );
	VecToAngles( movedir, moveangles );

	AnglesToAxis( moveangles, moveaxis );

	//add nodes in "side" direction

	nodes[nav.num_nodes].flags = 0;
	VectorMA( door_origin, nodeOffset, &moveaxis[AXIS_RIGHT], nodes[nav.num_nodes].origin );
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
	if( AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL ) )
	{
		nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );
		dropped[0] = nav.num_nodes;
		nav.num_nodes++;
	}

	nodes[nav.num_nodes].flags = 0;
	VectorMA( door_origin, -nodeOffset, &moveaxis[AXIS_RIGHT], nodes[nav.num_nodes].origin );
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
	if( AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL ) )
	{
		nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );
		dropped[1] = nav.num_nodes;
		nav.num_nodes++;
	}

	// if moving in the Y axis drop also in the other crossing direction and hope the
	// bad ones are inhibited by a solid
	if( fabs( DotProduct( MOVEDIR_UP, &moveaxis[AXIS_FORWARD] ) ) > 0.8 )
	{
		nodes[nav.num_nodes].flags = 0;
		VectorMA( door_origin, nodeOffset, &moveaxis[AXIS_UP], nodes[nav.num_nodes].origin );
#ifdef SHOW_JUMPAD_GUESS
		AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
		if( AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL ) )
		{
			nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );
			dropped[2] = nav.num_nodes;
			nav.num_nodes++;
		}

		nodes[nav.num_nodes].flags = 0;
		VectorMA( door_origin, -nodeOffset, &moveaxis[AXIS_UP], nodes[nav.num_nodes].origin );
#ifdef SHOW_JUMPAD_GUESS
		AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
		if( AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL ) )
		{
			nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );
			dropped[3] = nav.num_nodes;
			nav.num_nodes++;
		}
	}

	// link those we dropped
	for( i = 0; i < 4; i++ )
	{
		if( dropped[i] == NODE_INVALID )
			continue;

		//put into ents table
		AI_AddNavigatableEntity( ent, dropped[i] );

		for( j = 0; j < 4; j++ )
		{
			if( dropped[j] == NODE_INVALID )
				continue;

			AI_AddLink( dropped[i], dropped[j], LINK_MOVE|LINK_DOOR );
		}
	}

	return nav.num_nodes-1;
}


/*
* AI_AddNode_Platform_FindLowerLinkableCandidate
* helper to AI_AddNode_Platform
*/
static int AI_AddNode_Platform_FindLowerLinkableCandidate( edict_t *ent )
{
	trace_t	trace;
	float plat_dist;
	float platlip;
	int numtries = 0, maxtries = 10;
	int candidate;
	vec3_t candidate_origin, virtualorigin;
	float mindist = 0;

	if( ent->flags & FL_TEAMSLAVE )
		return NODE_INVALID;

	plat_dist = ent->moveinfo.start_origin[2] - ent->moveinfo.end_origin[2];
	platlip = ( ent->r.maxs[2] - ent->r.mins[2] ) - plat_dist;

	//find a good candidate for lower
	candidate_origin[0] = ( ent->r.maxs[0] - ent->r.mins[0] ) / 2 + ent->r.mins[0];
	candidate_origin[1] = ( ent->r.maxs[1] - ent->r.mins[1] ) / 2 + ent->r.mins[1];
	candidate_origin[2] = ent->r.mins[2] + platlip;

	//try to find the closer reachable node to the bottom of the plat
	do
	{
		candidate = AI_FindClosestNode( candidate_origin, mindist, NODE_DENSITY * 2, NODE_ALL );
		if( candidate != NODE_INVALID )
		{
			mindist = DistanceFast( candidate_origin, nodes[candidate].origin );

			//check to see if it would be valid
			if( fabs( candidate_origin[2] - nodes[candidate].origin[2] )
				< ( fabs( platlip ) + AI_JUMPABLE_HEIGHT ) )
			{
				//put at linkable candidate height
				virtualorigin[0] = candidate_origin[0];
				virtualorigin[1] = candidate_origin[1];
				virtualorigin[2] = nodes[candidate].origin[2];

				G_Trace( &trace, virtualorigin, vec3_origin, vec3_origin, nodes[candidate].origin, ent, MASK_NODESOLID );
				//trace = gi.trace( virtualorigin, vec3_origin, vec3_origin, nodes[candidate].origin, ent, MASK_NODESOLID );
				if( trace.fraction == 1.0 && !trace.startsolid )
				{
#ifdef SHOW_JUMPAD_GUESS
					AI_JumpadGuess_ShowPoint( virtualorigin, "models/objects/grenade/tris.md2" );
#endif
					return candidate;
				}
			}
		}
	}
	while( candidate != NODE_INVALID && numtries++ < maxtries );

	return NODE_INVALID;
}

/*
* AI_AddNode_Platform
* drop two nodes one at top, one at bottom
*/
static int AI_AddNode_Platform( edict_t *ent )
{
	float plat_dist;
	float platlip;
	int candidate;
	vec3_t lorg;

	if( nav.num_nodes + 2 > MAX_NODES )
		return NODE_INVALID;

	if( ent->flags & FL_TEAMSLAVE )
		return NODE_INVALID; // only team master will drop the nodes

	plat_dist = ent->moveinfo.start_origin[2] - ent->moveinfo.end_origin[2];
	platlip = ( ent->r.maxs[2] - ent->r.mins[2] ) - plat_dist;

	//make a guess on lower plat position
	candidate = AI_AddNode_Platform_FindLowerLinkableCandidate( ent );
	if( candidate != NODE_INVALID )
	{                    //base lower on cadidate's height
		lorg[0] = ( ent->r.maxs[0] - ent->r.mins[0] ) / 2 + ent->r.mins[0];
		lorg[1] = ( ent->r.maxs[1] - ent->r.mins[1] ) / 2 + ent->r.mins[1];
		lorg[2] = nodes[candidate].origin[2];

	}
	else
	{
		lorg[0] = ( ent->r.maxs[0] - ent->r.mins[0] ) / 2 + ent->r.mins[0];
		lorg[1] = ( ent->r.maxs[1] - ent->r.mins[1] ) / 2 + ent->r.mins[1];
		lorg[2] = ent->r.mins[2] + platlip + 16;
	}

	// Upper node
	nodes[nav.num_nodes].flags = ( NODEFLAGS_PLATFORM|NODEFLAGS_SERVERLINK|NODEFLAGS_FLOAT );
	nodes[nav.num_nodes].origin[0] = lorg[0];
	nodes[nav.num_nodes].origin[1] = lorg[1];
	nodes[nav.num_nodes].origin[2] = lorg[2] + plat_dist;
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );

	nav.num_nodes++;

	// Lower node
	nodes[nav.num_nodes].flags = ( NODEFLAGS_PLATFORM|NODEFLAGS_SERVERLINK|NODEFLAGS_FLOAT );
	nodes[nav.num_nodes].origin[0] = lorg[0];
	nodes[nav.num_nodes].origin[1] = lorg[1];
	nodes[nav.num_nodes].origin[2] = lorg[2] + 16;
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif
	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );

	// link lower to upper
	AI_AddLink( nav.num_nodes, nav.num_nodes-1, LINK_PLATFORM );

	//next
	nav.num_nodes++;
	return nav.num_nodes-1;
}


/*
* AI_AddNode_Teleporter
* Drop two nodes, one at trigger and other
* at target entity
*/
static int AI_AddNode_Teleporter( edict_t *ent )
{
	vec3_t v1, v2;
	edict_t	*dest;

	if( nav.num_nodes + 1 > MAX_NODES )
		return NODE_INVALID;

	dest = G_Find( NULL, FOFS( targetname ), ent->target );
	if( !dest )
		return NODE_INVALID;

	// NODE_TELEPORTER_IN
	nodes[nav.num_nodes].flags = ( NODEFLAGS_TELEPORTER_IN|NODEFLAGS_SERVERLINK|NODEFLAGS_REACHATTOUCH );

	if( !strcmp( ent->classname, "misc_teleporter" ) ) //jabot092(2)
	{
		nodes[nav.num_nodes].origin[0] = ent->s.origin[0];
		nodes[nav.num_nodes].origin[1] = ent->s.origin[1];
		nodes[nav.num_nodes].origin[2] = ent->s.origin[2]+16;
	}
	else
	{
		VectorCopy( ent->r.maxs, v1 );
		VectorCopy( ent->r.mins, v2 );
		nodes[nav.num_nodes].origin[0] = ( v1[0] - v2[0] ) / 2 + v2[0];
		nodes[nav.num_nodes].origin[1] = ( v1[1] - v2[1] ) / 2 + v2[1];
		nodes[nav.num_nodes].origin[2] = ent->r.mins[2]+32;
	}

	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, ent );
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );

	nav.num_nodes++;

	//NODE_TELEPORTER_OUT
	nodes[nav.num_nodes].flags = ( NODEFLAGS_TELEPORTER_OUT|NODEFLAGS_SERVERLINK );
	VectorCopy( dest->s.origin, nodes[nav.num_nodes].origin );
	if( ent->spawnflags & 1 )  // droptofloor
		nodes[nav.num_nodes].flags |= NODEFLAGS_FLOAT;
	else
		AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, NULL );

	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, ent );
#ifdef SHOW_JUMPAD_GUESS
	AI_JumpadGuess_ShowPoint( nodes[nav.num_nodes].origin, PATH_AMMO_BOX_MODEL );
#endif

	//put into ents table
	AI_AddNavigatableEntity( ent, nav.num_nodes );

	// link from teleport_in
	AI_AddLink( nav.num_nodes-1, nav.num_nodes, LINK_TELEPORT );

	nav.num_nodes++;
	return nav.num_nodes -1;
}

/*
* AI_AddNode_EntityNode
* Used to add nodes from items
*/
static int AI_AddNode_GoalEntityNode( edict_t *ent )
{
	if( nav.num_nodes + 1 > MAX_NODES )
		return NODE_INVALID;

	VectorCopy( ent->s.origin, nodes[nav.num_nodes].origin );
	if( ent->spawnflags & 1 )  // floating items
		nodes[nav.num_nodes].flags |= NODEFLAGS_FLOAT;
	else if( !AI_DropNodeOriginToFloor( nodes[nav.num_nodes].origin, ent ) )
		return NODE_INVALID; // spawned inside solid

	nodes[nav.num_nodes].flags |= AI_FlagsForNode( nodes[nav.num_nodes].origin, NULL );

	nav.num_nodes++;
	return nav.num_nodes-1; // return the node added
}

/*
* AI_IsPlatformLink
* interpretation of this link type
*/
static int AI_IsPlatformLink( int n1, int n2 )
{
	int i;
	if( nodes[n1].flags & NODEFLAGS_PLATFORM && nodes[n2].flags & NODEFLAGS_PLATFORM )
	{
		//the link was added by it's dropping function or it's invalid
		return LINK_INVALID;
	}

	if( nodes[n1].flags & NODEFLAGS_DONOTENTER || nodes[n2].flags & NODEFLAGS_DONOTENTER )
		return LINK_INVALID;

	//if first is plat but not second
	if( nodes[n1].flags & NODEFLAGS_PLATFORM && !( nodes[n2].flags & NODEFLAGS_PLATFORM ) )
	{
		edict_t *n1ent = NULL;
		int othernode = -1;

		// find ent
		for( i = 0; i < nav.num_navigableEnts; i++ )
		{
			if( nav.navigableEnts[i].node == n1 )
				n1ent = nav.navigableEnts[i].ent;
		}

		// find the other node from that ent
		for( i = 0; i < nav.num_navigableEnts; i++ )
		{
			if( nav.navigableEnts[i].node != n1 && nav.navigableEnts[i].ent == n1ent )
				othernode = nav.navigableEnts[i].node;
		}

		if( othernode == -1 || !n1ent )
			return LINK_INVALID;

		//find out if n1 is the upper or the lower plat node
		if( nodes[n1].origin[2] < nodes[othernode].origin[2] )
		{
			//n1 is plat lower: it can't link TO anything but upper plat node
			return LINK_INVALID;

		}
		else
		{
			trace_t	trace;
			float heightdiff;

			//n1 is plat upper: it can link to visibles at same height
			G_Trace( &trace, nodes[n1].origin, vec3_origin, vec3_origin, nodes[n2].origin, n1ent, MASK_NODESOLID );
			if( trace.fraction == 1.0 && !trace.startsolid )
			{
				heightdiff = nodes[n1].origin[2] - nodes[n2].origin[2];
				if( heightdiff < 0 )
					heightdiff = -heightdiff;

				if( heightdiff < AI_JUMPABLE_HEIGHT )
					return LINK_MOVE;

				return LINK_INVALID;
			}
		}
	}

	//only second is plat node
	if( !( nodes[n1].flags & NODEFLAGS_PLATFORM ) && nodes[n2].flags & NODEFLAGS_PLATFORM )
	{
		edict_t *n2ent = NULL;
		int othernode = -1;

		// find ent
		for( i = 0; i < nav.num_navigableEnts; i++ )
		{
			if( nav.navigableEnts[i].node == n2 )
				n2ent = nav.navigableEnts[i].ent;
		}

		// find the other node from that ent
		for( i = 0; i < nav.num_navigableEnts; i++ )
		{
			if( nav.navigableEnts[i].node != n2 && nav.navigableEnts[i].ent == n2ent )
				othernode = nav.navigableEnts[i].node;
		}

		if( othernode == -1 || !n2ent )
			return LINK_INVALID;

		//find out if n2 is the upper or the lower plat node
		if( nodes[n2].origin[2] < nodes[othernode].origin[2] )
		{
			trace_t	trace;
			float heightdiff;

			//n2 is plat lower: other's can link to it when visible and good height
			G_Trace( &trace, nodes[n1].origin, vec3_origin, vec3_origin, nodes[n2].origin, n2ent, MASK_NODESOLID );
			if( trace.fraction == 1.0 && !trace.startsolid )
			{
				heightdiff = nodes[n1].origin[2] - nodes[n2].origin[2];
				if( heightdiff < 0 )
					heightdiff = -heightdiff;

				if( heightdiff < AI_JUMPABLE_HEIGHT )
					return LINK_MOVE;

				return LINK_INVALID;
			}

		}
		else
		{
			// n2 is plat upper: others can't link to plat upper nodes
			return LINK_INVALID;
		}
	}

	return LINK_INVALID;
}

/*
* AI_IsJumpPadLink
* interpretation of this link type
*/
static int AI_IsJumpPadLink( int n1, int n2 )
{
	if( nodes[n1].flags & NODEFLAGS_JUMPPAD )
		return LINK_INVALID; // only can link TO jumppad land, and it's linked elsewhere

	if( nodes[n2].flags & NODEFLAGS_JUMPPAD_LAND )
		return LINK_INVALID; // linked as TO only from it's jumppad. Handled elsewhere

	return AI_GravityBoxToLink( n1, n2 );
}

/*
* AI_IsTeleporterLink
* interpretation of this link type
*/
static int AI_IsTeleporterLink( int n1, int n2 )
{
	if( nodes[n1].flags & NODEFLAGS_TELEPORTER_IN )
		return LINK_INVALID;

	if( nodes[n2].flags & NODEFLAGS_TELEPORTER_OUT )
		return LINK_INVALID;

	//find out the link move type against the world
	return AI_GravityBoxToLink( n1, n2 );
}

/*
* AI_FindServerLinkType
* determine what type of link it is
*/
static int AI_FindServerLinkType( int n1, int n2 )
{
	if( AI_PlinkExists( n1, n2 ) )
		return LINK_INVALID; //already saved

	if( nodes[n1].flags & NODEFLAGS_PLATFORM || nodes[n2].flags & NODEFLAGS_PLATFORM )
	{
		return AI_IsPlatformLink( n1, n2 );
	}
	else if( nodes[n2].flags & NODEFLAGS_TELEPORTER_IN || nodes[n1].flags & NODEFLAGS_TELEPORTER_OUT )
	{
		return AI_IsTeleporterLink( n1, n2 );
	}
	else if( nodes[n2].flags & NODEFLAGS_JUMPPAD || nodes[n1].flags & NODEFLAGS_JUMPPAD_LAND )
	{
		return AI_IsJumpPadLink( n1, n2 );
	}

	return LINK_INVALID;
}

/*
* AI_LinkServerNodes
* link the new nodes to&from those loaded from disk
*/
static int AI_LinkServerNodes( int start )
{
	int n1, n2;
	int count = 0;
	float pLinkRadius = NODE_DENSITY * 1.5f;
	bool ignoreHeight = true;

	if( start >= nav.num_nodes )
		return 0;

	for( n1 = start; n1 < nav.num_nodes; n1++ )
	{
		n2 = 0;

		while( ( n2 = AI_findNodeInRadius( n2, nodes[n1].origin, pLinkRadius, ignoreHeight ) ) != NODE_INVALID )
		{
			if( nodes[n1].flags & NODEFLAGS_SERVERLINK || nodes[n2].flags & NODEFLAGS_SERVERLINK )
			{
				if( AI_AddLink( n1, n2, AI_FindServerLinkType( n1, n2 ) ) )
					count++;

				if( AI_AddLink( n2, n1, AI_FindServerLinkType( n2, n1 ) ) )
					count++;
			}
			else
			{
				if( AI_AddLink( n1, n2, AI_FindLinkType( n1, n2 ) ) )
					count++;

				if( AI_AddLink( n2, n1, AI_FindLinkType( n2, n1 ) ) )
					count++;
			}
		}
	}
	return count;
}

/*
* AI_AddNavigableEntity
*/
static bool AI_AddNavigableEntity( edict_t *ent )
{
	bool handled = false;

	if( !ent->r.inuse || !ent->classname || ent->r.client )
		return handled;

	// can't add navigable entities after the spawn process (at least by now)
	if( nav.loaded )
		return handled;

	//
	// Navigable entities
	//

	// platforms
	if( !Q_stricmp( ent->classname, "func_plat" ) )
	{
		AI_AddNode_Platform( ent );
		handled = true;
	}

	// teleporters
	else if( !Q_stricmp( ent->classname, "trigger_teleport" ) || !Q_stricmp( ent->classname, "misc_teleporter" ) )
	{
		AI_AddNode_Teleporter( ent );
		handled = true;
	}

	// jump pads
	else if( !Q_stricmp( ent->classname, "trigger_push" ) )
	{
		AI_AddNode_JumpPad( ent );
		handled = true;
	}

	// doors
	else if( !Q_stricmp( ent->classname, "func_door" ) )
	{
		AI_AddNode_Door( ent );
		handled = true;
	}

	return handled;
}

/*
* AI_AllocGoalEntity
*/
static nav_ents_t *AI_AllocGoalEntity( void )
{
	nav_ents_t *goalEnt;

	if( !nav.goalEntsFree ) {
		return NULL;
	}

	// take a free decal if possible
	goalEnt = nav.goalEntsFree;
	nav.goalEntsFree = goalEnt->next;

	// put the decal at the start of the list
	goalEnt->prev = &nav.goalEntsHeadnode;
	goalEnt->next = nav.goalEntsHeadnode.next;
	goalEnt->next->prev = goalEnt;
	goalEnt->prev->next = goalEnt;

	return goalEnt;
}

/*
* AI_FreeGoalEntity
*/
static void AI_FreeGoalEntity( nav_ents_t *goalEnt )
{
	// remove from linked active list
	goalEnt->prev->next = goalEnt->next;
	goalEnt->next->prev = goalEnt->prev;

	// insert into linked free list
	goalEnt->next = nav.goalEntsFree;
	nav.goalEntsFree = goalEnt;
}

/*
* AI_AddGoalEntityNode
*/
static void AI_AddGoalEntityNode( edict_t *ent, bool customReach )
{
	int node = NODE_INVALID;
	int range = NODE_DENSITY * 0.75;
	nav_ents_t *goalEnt;

	if( !ent->r.inuse || !ent->classname )
		return;

	if( AI_GetGoalentForEnt( ent ) != NULL )
		return;

	if( ent->r.client )
	{
		node = NODE_INVALID;
		goto add;
	}

	//
	// Goal entities
	//

	if( nav.loaded )
		range = AI_GOAL_SR_RADIUS;

	// if we have a available node close enough to the entity, use it
	node = AI_FindClosestReachableNode( ent->s.origin, ent, range, NODE_ALL );
	if( node != NODE_INVALID )
	{
		if( nodes[node].flags & NODE_MASK_NOREUSE )
			node = NODE_INVALID;
		else
		{
			float heightdiff = fabs( ent->s.origin[2] - nodes[node].origin[2] );

			if( heightdiff > AI_STEPSIZE + 8 )  // not near enough
				node = NODE_INVALID;
		}
	}

	// link the node is spawned after the load process is done
	if( nav.loaded )
	{
		if( node == NODE_INVALID ) // (BY NOW) can not create new nodes after the initialization was done
			return;

		if( nav.debugMode && bot_showlrgoal->integer > 2 )
			G_Printf( "New Goal Entity added: %s\n", ent->classname );
	}
	else
	{
		// didn't find a node we could reuse
		if( node == NODE_INVALID )
			node = AI_AddNode_GoalEntityNode( ent );
	}

	if( node != NODE_INVALID )
	{
		if( customReach )
			nodes[node].flags |= NODEFLAGS_ENTITYREACH;

add:
		goalEnt = AI_AllocGoalEntity();
		if( !goalEnt ) {
			return;
		}
		goalEnt->node = node;
		goalEnt->ent = ent;
		nav.entsGoals[ENTNUM(ent)] = goalEnt;
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
* AI_AddNavigatableEntity
*/
void AI_AddNavigatableEntity( edict_t *ent, int node )
{
	if( nav.num_navigableEnts == sizeof( nav.navigableEnts ) / sizeof( nav.navigableEnts[0] ) ) {
		return;
	}
	nav.navigableEnts[nav.num_navigableEnts].ent = ent;
	nav.navigableEnts[nav.num_navigableEnts].node = node;
	nav.num_navigableEnts++;
}

/*
* AI_RemoveGoalEntity
*/
void AI_RemoveGoalEntity( edict_t *ent )
{
	nav_ents_t *goalEnt;

	goalEnt = AI_GetGoalentForEnt( ent );
	if( !goalEnt ) {
		return;
	}

	AI_FreeGoalEntity( goalEnt );
	nav.entsGoals[ENTNUM( ent )] = NULL;

	if( nav.debugMode && bot_showlrgoal->integer > 2 )
		G_Printf( "Goal Entity removed: %s\n", ent->classname );
}

/*************************************************************************/

/*
* AI_SavePLKFile
* save nodes and plinks to file.
* Only navigation nodes are saved. Item nodes aren't
*/
static bool AI_SavePLKFile( char *mapname )
{
	char filename[MAX_QPATH];
	int version = NAV_FILE_VERSION;
	int filenum;
	int length;
	int i;
	int numNodes;

	Q_snprintfz( filename, sizeof( filename ), "%s/%s.%s", NAV_FILE_FOLDER, mapname, NAV_FILE_EXTENSION );

	length = trap_FS_FOpenFile( filename, &filenum, FS_WRITE );
	if( length == -1 )
		return false;

	if( nav.serverNodesStart && nav.serverNodesStart < nav.num_nodes )
		numNodes = nav.serverNodesStart;
	else
		numNodes = nav.num_nodes;

	trap_FS_Write( &version, sizeof( int ), filenum );
	trap_FS_Write( &numNodes, sizeof( int ), filenum );

	// write out nodes
	for( i = 0; i < numNodes; i++ )
	{
		trap_FS_Write( &nodes[i], sizeof( nav_node_t ), filenum );
	}

	// write out plinks array
	for( i = 0; i < numNodes; i++ )
	{
		trap_FS_Write( &pLinks[i], sizeof( nav_plink_t ), filenum );
	}

	trap_FS_FCloseFile( filenum );

	return true;
}

/*
* AI_LoadPLKFile
* load nodes and plinks from file
*/
bool AI_LoadPLKFile( char *mapname )
{
	char filename[MAX_QPATH];
	int version;
	int length;
	int filenum;

	Q_snprintfz( filename, sizeof( filename ), "%s/%s.%s", NAV_FILE_FOLDER, mapname, NAV_FILE_EXTENSION );

	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length == -1 )
		return false;

	trap_FS_Read( &version, sizeof( int ), filenum );
	if( version != NAV_FILE_VERSION )
	{
		trap_FS_FCloseFile( filenum );
		G_Printf( "AI_LoadPLKFile: Invalid version %i\n", version );
		return false;
	}

	trap_FS_Read( &nav.num_nodes, sizeof( int ), filenum );
	if( nav.num_nodes > MAX_NODES )
	{
		trap_FS_FCloseFile( filenum );
		G_Printf( "AI_LoadPLKFile: Too many nodes\n" );
		return false;
	}

	//read nodes
	trap_FS_Read( nodes, sizeof( nav_node_t ) * nav.num_nodes, filenum );

	//read plinks
	trap_FS_Read( pLinks, sizeof( nav_plink_t ) * nav.num_nodes, filenum );

	trap_FS_FCloseFile( filenum );

	return true;
}

/*
* AI_SaveNavigation
*/
void AI_SaveNavigation( void )
{
	if( !nav.editmode )
	{
		Com_Printf( "       : Can't Save nodes when not being in editing mode.\n" );
		return;
	}

	if( !nav.num_nodes )
	{
		Com_Printf( "       : No nodes to save\n" );
		return;
	}

	AI_LinkNavigationFile( false );

	if( !AI_SavePLKFile( level.mapname ) )
		Com_Printf( "       : Failed: Couldn't create the nodes file\n" );
	else
		Com_Printf( "       : Nodes files saved\n" );

	// restart the level so it's reloaded
	G_RestartLevel();
}

/*
* AI_InitEntitiesData
*/
void AI_InitEntitiesData( void )
{
	int newlinks, newjumplinks;
	edict_t *ent;

	if( !nav.num_nodes )
	{
		if( g_numbots->integer ) trap_Cvar_Set( "g_numbots", "0" );
		return;
	}

	// create nodes for navigable map entities ( must happen after finding teams )
	for( ent = game.edicts + 1 + gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ )
		AI_AddNavigableEntity( ent );

	// add all clients to goalEntities so they can be tracked as enemies
	for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ )
		AI_AddGoalEntity( ent );

	// link all newly added nodes
	newlinks = AI_LinkServerNodes( nav.serverNodesStart );
	newjumplinks = AI_LinkCloseNodes_JumpPass( nav.serverNodesStart );

	if( developer->integer )
	{
		G_Printf( "       : added nodes:%i.\n", nav.num_nodes - nav.serverNodesStart );
		G_Printf( "       : total nodes:%i.\n", nav.num_nodes );
		G_Printf( "       : added links:%i.\n", newlinks );
		G_Printf( "       : added jump links:%i.\n", newjumplinks );
	}

	G_Printf( "       : AI Navigation Initialized.\n" );

	nav.loaded = true;
}

/*
* AI_InitNavigationData
* Setup nodes & links for this map
*/
void AI_InitNavigationData( bool silent )
{
	int i;
	int linkscount;
	const int maxgoalEnts = sizeof( nav.goalEnts ) / sizeof( nav.goalEnts[0] );

	memset( &nav, 0, sizeof( nav ) );
	memset( nodes, 0, sizeof( nav_node_t ) * MAX_NODES );
	memset( pLinks, 0, sizeof( nav_plink_t ) * MAX_NODES );

	nav.goalEntsFree = nav.goalEnts;
	nav.goalEntsHeadnode.id = -1;
	nav.goalEntsHeadnode.ent = game.edicts;
	nav.goalEntsHeadnode.node = NODE_INVALID;
	nav.goalEntsHeadnode.prev = &nav.goalEntsHeadnode;
	nav.goalEntsHeadnode.next = &nav.goalEntsHeadnode;
	for( i = 0; i < maxgoalEnts - 1; i++ ) {
		nav.goalEnts[i].id = i;
		nav.goalEnts[i].next = &nav.goalEnts[i+1];
	}
	nav.goalEnts[i].id = i;
	nav.goalEnts[i].next = NULL;

	if( developer->integer && !silent )
	{
		G_Printf( "-------------------------------------\n" );
		G_Printf( "       : AI version: %s\n", AI_VERSION_STRING );
	}

	// Load nodes from file
	if( !AI_LoadPLKFile( level.mapname ) )
	{
		if( !silent )
			G_Printf( "       : AI FAILED to load navigation file.\n" );
		return;
	}

	nav.serverNodesStart = nav.num_nodes;

	if( developer->integer && !silent )
	{
		G_Printf( "       : \n" );
		G_Printf( "       : loaded nodes:%i.\n", nav.num_nodes );

		for( linkscount = 0, i = 0; i < nav.num_nodes; i++ )
			linkscount += pLinks[i].numLinks;

		G_Printf( "       : loaded links:%i.\n", linkscount );
	}
}

