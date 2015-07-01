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

#include "tvm_frame.h"
#include "tvm_clip.h"
#include "tvm_client.h"

#include "tvm_clip.h"

/*
* TVM_RunClients
*/
static void TVM_RunClients( tvm_relay_t *relay )
{
	int i;
	edict_t *ent;

	for( i = 0; i < relay->local_maxclients; i++ )
	{
		ent = relay->local_edicts + i;
		if( ent->s.teleported )
			ent->s.teleported = false;
		if( !ent->r.inuse || !ent->r.client )
			continue;
		if( trap_GetClientState( relay, PLAYERNUM( ent ) ) != CS_SPAWNED )
			continue;

		// the client entities may have been moved by the world, update their pmove positions
		if( ent->movetype != MOVETYPE_NOCLIP )
		{
			VectorCopy( ent->s.origin, ent->r.client->ps.pmove.origin );
			VectorCopy( ent->velocity, ent->r.client->ps.pmove.velocity );
		}
		trap_ExecuteClientThinks( relay, PLAYERNUM( ent ) );
		ent->r.client->ps.plrkeys = ent->r.client->plrkeys;
		ent->snap.buttons |= ent->r.client->buttons;
	}
}

/*
* TVM_RunLinearProjectiles
*/
void TVM_RunLinearProjectiles( tvm_relay_t *relay )
{
	int i;
	edict_t *ent;
	vec3_t old_origin;

	for( i = relay->maxclients + 1; i < relay->numentities; i++ )
	{
		ent = relay->edicts + i;
		if( !ent->s.linearMovement || !ent->r.inuse )
			continue;

		VectorCopy( ent->s.origin, old_origin );
		GClip_LinearMovement( relay, ent );

		// check if projectile has moved since TVM_NewPacketEntityState
		// before calling GClip_LinkEntity to save CPU time
		if( ent->r.linkcount && VectorCompare( ent->s.origin, old_origin ) )
			continue;

		GClip_LinkEntity( relay, ent );
	}
}

/*
* TVM_RunFrame
* Advances the world
*/
void TVM_RunFrame( tvm_relay_t *relay, unsigned int msec )
{
	tvm.realtime += msec;

	TVM_RunLinearProjectiles( relay );
	TVM_RunClients( relay );
}
