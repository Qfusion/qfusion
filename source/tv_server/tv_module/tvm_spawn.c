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

#include "tvm_spawn.h"

#include "tvm_cmds.h"
#include "tvm_clip.h"

/*
* TVM_SpawnEntities
* 
* Creates a server's entity / program execution context by
* parsing textual entity definitions out of an ent file.
*/
void TVM_SpawnEntities( tvm_relay_t *relay, const char *mapname, const char *entities, int entstrlen )
{
	edict_t	*ent;
	int i;

	assert( mapname );

	Q_strncpyz( relay->mapname, mapname, sizeof( relay->mapname ) );

	GClip_ClearWorld( relay ); // clear areas links

	memset( relay->edicts, 0, relay->maxentities * sizeof( relay->edicts[0] ) );
	for( i = 0, ent = &relay->edicts[0]; i < relay->maxentities; i++, ent++ )
	{
		ent->relay = relay;
		ent->s.number = i;
	}
	relay->numentities = 0;

	// overwrite gamecommands from the relay
	for( i = 0; i < MAX_GAMECOMMANDS; i++ )
	{
		assert( CS_GAMECOMMANDS + i < MAX_CONFIGSTRINGS );
		relay->configStringsOverwritten[CS_GAMECOMMANDS + i] = true;
		trap_ConfigString( relay, CS_GAMECOMMANDS + i, "" );
	}

	TVM_RemoveGameCommands( relay );
	TVM_AddGameCommands( relay );
}
