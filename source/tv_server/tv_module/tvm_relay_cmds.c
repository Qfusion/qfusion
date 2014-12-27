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
#include "tvm_relay_cmds.h"
#include "tvm_client.h"

/*
* TVM_RelayCommand_Pass
*/
static void TVM_RelayCommand_Pass( tvm_relay_t *relay, snapshot_t *frame, gcommand_t *gcmd )
{
	int i;
	int target;
	edict_t *ent;

	assert( gcmd );

	for( i = 0; i < relay->local_maxclients; i++ )
	{
		ent = relay->local_edicts + i;
		if( !ent->r.inuse || !ent->r.client )
			continue;
		if( trap_GetClientState( relay, PLAYERNUM( ent ) ) != CS_SPAWNED )
			continue;

		target = (ent->r.client->chase.active ? ent->r.client->chase.target-1 : relay->playernum);
		if( gcmd->all || !frame->multipov || ( target >= 0 && gcmd->targets[target>>3] & ( 1<<( target&7 ) ) ) )
			trap_GameCmd( relay, PLAYERNUM( ent ), frame->gamecommandsData + gcmd->commandOffset );
	}
}

/*
* TVM_RelayCommand
*/
void TVM_RelayCommand( tvm_relay_t *relay, snapshot_t *frame, gcommand_t *gcmd )
{
	TVM_RelayCommand_Pass( relay, frame, gcmd );
}
