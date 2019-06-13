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

#include "tvm_cmds.h"
#include "tvm_misc.h"
#include "tvm_client.h"
#include "tvm_chase.h"

/*
* TVM_Cmd_Ignore_f
*/
static void TVM_Cmd_Ignore_f( edict_t *ent ) {
}

/*
* TVM_Cmd_PlayersExt_f
*/
static void TVM_Cmd_PlayersExt_f( edict_t *ent, bool onlyspecs ) {
	int i;
	int count = 0;
	int start = 0;
	char line[64];
	char msg[1024];
	edict_t *e;
	tvm_relay_t *relay = ent->relay;

	if( trap_Cmd_Argc() > 1 ) {
		start = atoi( trap_Cmd_Argv( 1 ) );
	}
	clamp( start, 0, relay->maxclients - 1 );

	// print information
	msg[0] = 0;

	for( i = start + 1; PLAYERNUM( ( relay->edicts + i ) ) < relay->maxclients; i++ ) {
		e = relay->edicts + i;
		if( e->r.inuse && !e->local && e->r.client ) {
			char *userinfo;

			userinfo = relay->configStrings[CS_PLAYERINFOS + PLAYERNUM( e )];
			Q_snprintfz( line, sizeof( line ), "%3i %s\n", i, Info_ValueForKey( userinfo, "name" ) );

			if( strlen( line ) + strlen( msg ) > sizeof( msg ) - 100 ) {
				// can't print all of them in one packet
				Q_strncatz( msg, "...\n", sizeof( msg ) );
				break;
			}

			if( count == 0 ) {
				Q_strncatz( msg, "num name\n", sizeof( msg ) );
				Q_strncatz( msg, "--- ---------------\n", sizeof( msg ) );
			}

			Q_strncatz( msg, line, sizeof( msg ) );
			count++;
		}
	}

	if( count ) {
		Q_strncatz( msg, "--- ---------------\n", sizeof( msg ) );
	}
	Q_strncatz( msg, va( "%3i %s\n", count, trap_Cmd_Argv( 0 ) ), sizeof( msg ) );
	TVM_PrintMsg( relay, ent, "%s", msg );

	if( i < relay->maxclients ) {
		TVM_PrintMsg( relay, ent, "Type '%s %i' for more %s\n", trap_Cmd_Argv( 0 ), i, trap_Cmd_Argv( 0 ) );
	}
}

/*
* TVM_Cmd_Players_f
*/
static void TVM_Cmd_Players_f( edict_t *ent ) {
	TVM_Cmd_PlayersExt_f( ent, false );
}
typedef struct {
	char name[MAX_QPATH];
	void ( *func )( edict_t *ent );
} g_gamecommands_t;

g_gamecommands_t g_Commands[MAX_GAMECOMMANDS];

/*
* TVM_ClientCommand
*/
bool TVM_ClientCommand( tvm_relay_t *relay, edict_t *ent ) {
	char *cmd;
	int i;

	assert( ent && ent->local && ent->r.client );

	cmd = trap_Cmd_Argv( 0 );

	for( i = 0; i < MAX_GAMECOMMANDS; i++ ) {
		if( g_Commands[i].name[0] == '\0' ) {
			continue;
		}
		if( !Q_stricmp( g_Commands[i].name, cmd ) ) {
			if( g_Commands[i].func ) {
				g_Commands[i].func( ent );
			}
			return true;
		}
	}

	// unknown command
	return false;
}

/*
* TVM_AddGameCommand
*/
static void TVM_AddGameCommand( tvm_relay_t *relay, const char *name, void ( *callback )( edict_t *ent ) ) {
	int i;

#ifndef NDEBUG
	// check for double add
	for( i = 0; i < MAX_GAMECOMMANDS; i++ ) {
		if( g_Commands[i].name[0] == '\0' ) {
			continue;
		}
		if( !Q_stricmp( g_Commands[i].name, name ) ) {
			assert( false );
		}
	}
#endif

	// find a free one and add it
	for( i = 0; i < MAX_GAMECOMMANDS; i++ ) {
		if( g_Commands[i].name[0] == '\0' ) {
			g_Commands[i].func = callback;
			Q_strncpyz( g_Commands[i].name, name, sizeof( g_Commands[i].name ) );
			trap_ConfigString( relay, CS_GAMECOMMANDS + i, name );
			return;
		}
	}

	TVM_RelayError( relay, "G_AddCommand: Couldn't find a free g_Commands spot for the new command\n" );
}

/*
* TVM_RemoveGameCommands
*/
void TVM_RemoveGameCommands( tvm_relay_t *relay ) {
	int i;

	for( i = 0; i < MAX_GAMECOMMANDS; i++ ) {
		if( g_Commands[i].name[0] != '\0' ) {
			g_Commands[i].func = NULL;
			g_Commands[i].name[0] = 0;
			trap_ConfigString( relay, CS_GAMECOMMANDS + i, "" );
		}
	}
}

/*
* List of commands
*/
typedef struct {
	char *name;
	void ( *func )( edict_t *ent );
} gamecmd_t;

static gamecmd_t gamecmdlist[] =
{
	// chase
	{ "chaseprev", TVM_Cmd_ChasePrev },
	{ "chasenext", TVM_Cmd_ChaseNext },
	{ "chase", TVM_Cmd_ChaseCam },
	{ "camswitch", TVM_Cmd_SwitchChaseCamMode },
	{ "spec", TVM_Cmd_Ignore_f },
	{ "players", TVM_Cmd_Players_f },
	{ "say", NULL },

	{ NULL, NULL }
};

/*
* TVM_AddGameCommands
*/
void TVM_AddGameCommands( tvm_relay_t *relay ) {
	gamecmd_t *cmd;

	for( cmd = gamecmdlist; cmd->name; cmd++ ) {
		TVM_AddGameCommand( relay, cmd->name, cmd->func );
	}
}
