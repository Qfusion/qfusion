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

#include "tvm_snap.h"

#include "tvm_clip.h"
#include "tvm_client.h"
#include "tvm_chase.h"

/*
* TVM_SnapClients
* It's time to send a new snap, so set the world up for sending
*/
static void TVM_SnapClients( tvm_relay_t *relay )
{
	int i;
	edict_t	*ent;

	// normal clients
	for( i = 0; i < relay->local_maxclients; i++ )
	{
		ent = relay->local_edicts + i;
		if( !ent->r.inuse || !ent->r.client )
			continue;
		if( trap_GetClientState( relay, PLAYERNUM( ent ) ) != CS_SPAWNED )
			continue;
		if( ent->r.client->chase.active )
			continue;

		TVM_ClientEndSnapFrame( ent );
	}

	// chasers
	for( i = 0; i < relay->local_maxclients; i++ )
	{
		ent = relay->local_edicts + i;
		if( !ent->r.inuse || !ent->r.client )
			continue;
		if( trap_GetClientState( relay, PLAYERNUM( ent ) ) != CS_SPAWNED )
			continue;
		if( !ent->r.client->chase.active )
			continue;

		TVM_ChaseClientEndSnapFrame( ent );
	}
}

/*
* TVM_GetGameState
* 
* The server asks for the match state data
*/
game_state_t *TVM_GetGameState( tvm_relay_t *relay )
{
	return &relay->gameState;
}

/*
* TVM_SnapFrame
* It's time to send a new snap, so set the world up for sending
*/
void TVM_SnapFrame( tvm_relay_t *relay )
{
	edict_t	*ent;

	// finish snap
	TVM_SnapClients( relay ); // build the playerstate_t structures for all players

	// set entity bits (prepare entities for being sent in the snap)
	for( ent = &relay->edicts[0]; ENTNUM( ent ) < relay->numentities; ent++ )
	{
		assert( ent->s.number == ENTNUM( ent ) );

		if( !ent->r.inuse )
		{
			ent->r.svflags |= SVF_NOCLIENT;
			continue;
		}
	}
}

/*
* TVM_ClearSnap
* We just run SnapFrame, the server just sent the snap to the clients,
* it's now time to clean up snap specific data to start the next snap from clean.
*/
void TVM_ClearSnap( tvm_relay_t *relay )
{
	edict_t *ent;

	for( ent = &relay->local_edicts[0]; ENTNUM( ent ) < relay->local_numentities; ent++ )
	{
		if( !ent->r.inuse )
			continue;

		// clear the snap temp info
		memset( &ent->snap, 0, sizeof( ent->snap ) );
	}
}
