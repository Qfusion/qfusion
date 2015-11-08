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

#include "tvm_local.h"

#include "tvm_relay_snap.h"

#include "tvm_clip.h"
#include "tvm_relay_cmds.h"
#include "tvm_chase.h"
#include "tvm_client.h"

static void TVM_RemoveClient( edict_t *ent );

/*
* TVM_AddEntity
*/
static void TVM_AddEntity( edict_t *ent )
{
	assert( ent && !ent->r.inuse && !ent->local );

	ent->r.inuse = true;
}

/*
* TVM_NewPacketEntityState
*/
static void TVM_NewPacketEntityState( edict_t *ent, entity_state_t *state )
{
	assert( ent && ent->r.inuse && !ent->local );
	assert( state );
	assert( state->number == ENTNUM( ent ) );

	ent->s = *state;

	ent->r.svflags = ent->s.svflags;

	if( ent->s.solid == SOLID_BMODEL )
	{
		trap_CM_InlineModelBounds( ent->relay, trap_CM_InlineModel( ent->relay, ent->s.modelindex ), ent->r.mins, ent->r.maxs );
	}
	else if( ent->s.solid )
	{
		int x, zd, zu;

		x = 8 * ( ent->s.solid & 31 );
		zd = 8 * ( ( ent->s.solid>>5 ) & 31 );
		zu = 8 * ( ( ent->s.solid>>10 ) & 63 ) - 32;

		ent->r.mins[0] = ent->r.mins[1] = -x;
		ent->r.maxs[0] = ent->r.maxs[1] = x;
		ent->r.mins[2] = -zd;
		ent->r.maxs[2] = zu;
	}

	if( ent->s.linearMovement ) {
		ent->s.linearMovementTimeStamp -= ent->relay->snapFrameTime;
		GClip_LinearMovement( ent->relay, ent );
	}

	GClip_LinkEntity( ent->relay, ent );
}

/*
* TVM_RemoveEntity
*/
static void TVM_RemoveEntity( edict_t *ent )
{
	tvm_relay_t *relay;

	assert( ent && !ent->local && ent->r.inuse );

	GClip_UnlinkEntity( ent->relay, ent );

	if( ent->r.client )
	{
		TVM_RemoveClient( ent );
		if( developer->integer )
			Com_Printf( "Entity removed before client: %i\n", ENTNUM( ent ) );
	}

	relay = ent->relay;

	memset( ent, 0, sizeof( *ent ) );
	memset( &ent->s, 0, sizeof( ent->s ) );
	memset( &ent->r, 0, sizeof( ent->r ) );

	ent->local = false;
	ent->relay = relay;
	ent->s.number = ENTNUM( ent );
	ent->r.inuse = false;
}

/*
* TVM_AddClient
*/
static void TVM_AddClient( edict_t *ent )
{
	assert( ent && !ent->local && !ent->r.client );

	if( !ent->r.inuse )
		TVM_RelayError( ent->relay, "Playerstate without an entity\n" );

	ent->r.client = ent->relay->clients + PLAYERNUM( ent );
	memset( ent->r.client, 0, sizeof( *ent->r.client ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );
}

/*
* TVM_NewPlayerState
*/
static void TVM_NewPlayerState( edict_t *ent, player_state_t *ps )
{
	assert( ent && ent->r.inuse && !ent->local && ent->r.client );
	assert( ps );

	ent->r.client->ps = *ps;
}

/*
* TVM_RemoveClient
*/
static void TVM_RemoveClient( edict_t *ent )
{
	int i;
	edict_t *chaser;

	assert( ent && ent->r.inuse && !ent->local && ent->r.client );

	// normal clients
	for( i = 0; i < ent->relay->local_maxclients; i++ )
	{
		chaser = ent->relay->local_edicts + i;
		if( !chaser->r.inuse || !chaser->r.client )
			continue; // disconnected
	}

	ent->r.client = NULL;
}

/*
* TVM_NewMatchState
*/
static void TVM_NewGameState( tvm_relay_t *relay, game_state_t *gameState )
{
	relay->gameState = *gameState;
}

/*
* TVM_NewFrameSnap
* a new frame snap has been received from the server
*/
void TVM_NewFrameSnapshot( tvm_relay_t *relay, snapshot_t *frame )
{
	int i, j, num, numentities, maxclients;

	assert( relay );
	assert( frame );

	relay->frame = *frame;
	relay->serverTime = frame->serverTime;

	// add and update entities
	for( i = 0; i < frame->numEntities; i++ )
	{
		num = frame->parsedEntities[i & ( MAX_PARSE_ENTITIES-1 )].number;
		if( num < 1 || num >= relay->maxentities )
			TVM_RelayError( relay, "Invalid entity number" );
		if( !relay->edicts[num].r.inuse )
			TVM_AddEntity( &relay->edicts[num] );
		TVM_NewPacketEntityState( &relay->edicts[num], &frame->parsedEntities[i & ( MAX_PARSE_ENTITIES-1 )] );
	}

	// add, update and remove clients
	j = 0;
	maxclients = 0;
	for( i = 0; i < frame->numplayers; i++ )
	{
		num = frame->playerStates[i].playerNum + 1;
		if( num < 1 || num >= relay->maxentities || num > MAX_CLIENTS )
			TVM_RelayError( relay, "Invalid playerstate number" );
		while( j < num )
		{
			if( relay->edicts[j].r.inuse && relay->edicts[j].r.client )
				TVM_RemoveClient( &relay->edicts[j] );
			j++;
		}
		if( !relay->edicts[num].r.client )
			TVM_AddClient( &relay->edicts[num] );
		TVM_NewPlayerState( &relay->edicts[num], &frame->playerStates[i] );
		maxclients = num;
		j++;
	}
	while( j <= relay->maxclients )
	{
		if( relay->edicts[j].r.inuse && relay->edicts[j].r.client )
			TVM_RemoveClient( &relay->edicts[j] );
		j++;
	}
	relay->maxclients = maxclients;

	// remove entities
	j = 0;
	numentities = 0;
	for( i = 0; i < frame->numEntities; i++ )
	{
		num = frame->parsedEntities[i & ( MAX_PARSE_ENTITIES-1 )].number;
		if( num < 1 || num >= relay->maxentities )
			TVM_RelayError( relay, "Invalid entity number" );
		while( j < num )
		{
			if( relay->edicts[j].r.inuse )
				TVM_RemoveEntity( &relay->edicts[j] );
			j++;
		}
		numentities = num + 1;
		j++;
	}
	while( j < relay->numentities - 1 )
	{
		if( relay->edicts[j].r.inuse )
			TVM_RemoveEntity( &relay->edicts[j] );
		j++;
	}
	if( numentities != relay->numentities )
	{
		relay->numentities = numentities;
		trap_LocateEntities( relay, relay->edicts, sizeof( relay->edicts[0] ), relay->numentities,
		                     relay->maxentities );
	}

	// match state
	TVM_NewGameState( relay, &frame->gameState );

	// game commands
	for( i = 0; i < frame->numgamecommands; i++ )
		TVM_RelayCommand( relay, frame, &frame->gamecommands[i] );
}
