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

#include "g_local.h"

/*
* Cmd_ConsoleSay_f
*/
static void Cmd_ConsoleSay_f( void ) {
	G_ChatMsg( NULL, NULL, false, "%s", trap_Cmd_Args() );
}


/*
* Cmd_ConsoleKick_f
*/
static void Cmd_ConsoleKick_f( void ) {
	edict_t *ent;

	if( trap_Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: kick <id or name>\n" );
		return;
	}

	ent = G_PlayerForText( trap_Cmd_Argv( 1 ) );
	if( !ent ) {
		Com_Printf( "No such player\n" );
		return;
	}

	trap_DropClient( ent, DROP_TYPE_NORECONNECT, "Kicked" );
}


/*
* Cmd_Match_f
*/
static void Cmd_Match_f( void ) {
	const char *cmd;

	if( trap_Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: match <option: restart|advance|status>\n" );
		return;
	}

	cmd = trap_Cmd_Argv( 1 );
	if( !Q_stricmp( cmd, "restart" ) ) {
		level.exitNow = false;
		level.hardReset = false;
		Q_strncpyz( level.forcemap, level.mapname, sizeof( level.mapname ) );
		G_EndMatch();
	} else if( !Q_stricmp( cmd, "advance" ) ) {
		level.exitNow = false;
		level.hardReset = true;

		//		level.forcemap[0] = 0;
		G_EndMatch();
	} else if( !Q_stricmp( cmd, "status" ) ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "status" );
	}
}

//==============================================================================
//
//PACKET FILTERING
//
//
//You can add or remove addresses from the filter list with:
//
//addip <ip>
//removeip <ip>
//
//The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".
//
//Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.
//
//listip
//Prints the current list of filters.
//
//writeip
//Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.
//
//filterban <0 or 1>
//
//If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.
//
//If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.
//
//==============================================================================

typedef struct
{
	unsigned mask;
	unsigned compare;
	int64_t timeout;
} ipfilter_t;

#define MAX_IPFILTERS   1024

static ipfilter_t ipfilters[MAX_IPFILTERS];
static int numipfilters;

/*
* StringToFilter
*/
static bool StringToFilter( char *s, ipfilter_t *f ) {
	char num[128];
	int i, j;
	uint8_t b[4];
	uint8_t m[4];

	for( i = 0; i < 4; i++ ) {
		b[i] = 0;
		m[i] = 0;
	}

	for( i = 0; i < 4; i++ ) {
		if( *s < '0' || *s > '9' ) {
			G_Printf( "Bad filter address: %s\n", s );
			return false;
		}

		j = 0;
		while( *s >= '0' && *s <= '9' ) {
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi( num );
		if( b[i] != 0 ) {
			m[i] = 255;
		}

		if( !*s || *s == ':' ) {
			break;
		}
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return true;
}

/*
* SV_ResetPacketFiltersTimeouts
*/
void SV_ResetPacketFiltersTimeouts( void ) {
	int i;

	for( i = 0; i < MAX_IPFILTERS; i++ )
		ipfilters[i].timeout = 0;
}

/*
* SV_FilterPacket
*/
bool SV_FilterPacket( char *from ) {
	int i;
	unsigned in;
	uint8_t m[4];
	char *p;

	if( !filterban->integer ) {
		return false;
	}

	i = 0;
	p = from;
	while( *p && i < 4 ) {
		m[i] = 0;
		while( *p >= '0' && *p <= '9' ) {
			m[i] = m[i] * 10 + ( *p - '0' );
			p++;
		}
		if( !*p || *p == ':' ) {
			break;
		}
		i++, p++;
	}

	in = *(unsigned *)m;

	for( i = 0; i < numipfilters; i++ )
		if( ( in & ipfilters[i].mask ) == ipfilters[i].compare
			&& ( !ipfilters[i].timeout || ipfilters[i].timeout > game.serverTime ) ) {
			return true;
		}

	return false;
}

/*
* SV_ReadIPList
*/
void SV_ReadIPList( void ) {
	SV_ResetPacketFiltersTimeouts();

	trap_Cmd_ExecuteText( EXEC_APPEND, "exec listip.cfg silent\n" );
}

/*
* SV_WriteIPList
*/
void SV_WriteIPList( void ) {
	int file;
	char name[MAX_QPATH];
	char string[MAX_STRING_CHARS];
	uint8_t b[4] = { 0, 0, 0, 0 };
	int i;

	Q_strncpyz( name, "listip.cfg", sizeof( name ) );

	//G_Printf( "Writing %s.\n", name );

	if( trap_FS_FOpenFile( name, &file, FS_WRITE ) == -1 ) {
		G_Printf( "Couldn't open %s\n", name );
		return;
	}

	Q_snprintfz( string, sizeof( string ), "set filterban %d\r\n", filterban->integer );
	trap_FS_Write( string, strlen( string ), file );

	for( i = 0; i < numipfilters; i++ ) {
		if( ipfilters[i].timeout && ipfilters[i].timeout <= game.serverTime ) {
			continue;
		}
		*(unsigned *)b = ipfilters[i].compare;
		if( ipfilters[i].timeout ) {
			Q_snprintfz( string, sizeof( string ), "addip %i.%i.%i.%i %.2f\r\n", b[0], b[1], b[2], b[3], ( ipfilters[i].timeout - game.serverTime ) / ( 1000.0f * 60.0f ) );
		} else {
			Q_snprintfz( string, sizeof( string ), "addip %i.%i.%i.%i\r\n", b[0], b[1], b[2], b[3] );
		}
		trap_FS_Write( string, strlen( string ), file );
	}

	trap_FS_FCloseFile( file );
}

/*
* Cmd_AddIP_f
*/
static void Cmd_AddIP_f( void ) {
	int i;

	if( trap_Cmd_Argc() < 2 ) {
		G_Printf( "Usage: addip <ip-mask> [time-mins]\n" );
		return;
	}

	for( i = 0; i < numipfilters; i++ )
		if( ipfilters[i].compare == 0xffffffff || ( ipfilters[i].timeout && ipfilters[i].timeout <= game.serverTime ) ) {
			break;
		}          // free spot
	if( i == numipfilters ) {
		if( numipfilters == MAX_IPFILTERS ) {
			G_Printf( "IP filter list is full\n" );
			return;
		}
		numipfilters++;
	}

	ipfilters[i].timeout = 0;
	if( !StringToFilter( trap_Cmd_Argv( 1 ), &ipfilters[i] ) ) {
		ipfilters[i].compare = 0xffffffff;
	} else if( trap_Cmd_Argc() == 3 ) {
		ipfilters[i].timeout = game.serverTime + atof( trap_Cmd_Argv( 2 ) ) * 60 * 1000;
	}
}

/*
* Cmd_RemoveIP_f
*/
static void Cmd_RemoveIP_f( void ) {
	ipfilter_t f;
	int i, j;

	if( trap_Cmd_Argc() < 2 ) {
		G_Printf( "Usage: removeip <ip-mask>\n" );
		return;
	}

	if( !StringToFilter( trap_Cmd_Argv( 1 ), &f ) ) {
		return;
	}

	for( i = 0; i < numipfilters; i++ )
		if( ipfilters[i].mask == f.mask
			&& ipfilters[i].compare == f.compare ) {
			for( j = i + 1; j < numipfilters; j++ )
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			G_Printf( "Removed.\n" );
			return;
		}
	G_Printf( "Didn't find %s.\n", trap_Cmd_Argv( 1 ) );
}

/*
* Cmd_ListIP_f
*/
static void Cmd_ListIP_f( void ) {
	int i;
	uint8_t b[4];

	G_Printf( "Filter list:\n" );
	for( i = 0; i < numipfilters; i++ ) {
		*(unsigned *)b = ipfilters[i].compare;
		if( ipfilters[i].timeout && ipfilters[i].timeout > game.serverTime ) {
			G_Printf( "%3i.%3i.%3i.%3i %.2f\n", b[0], b[1], b[2], b[3],
					  (float)( ipfilters[i].timeout - game.serverTime ) / ( 60 * 1000.0f ) );
		} else if( !ipfilters[i].timeout ) {
			G_Printf( "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3] );
		}
	}
}

/*
* Cmd_WriteIP_f
*/
static void Cmd_WriteIP_f( void ) {
	SV_WriteIPList();
}

/*
* G_AddCommands
*/
void G_AddServerCommands( void ) {
	if( dedicated->integer ) {
		trap_Cmd_AddCommand( "say", Cmd_ConsoleSay_f );
	}
	trap_Cmd_AddCommand( "kick", Cmd_ConsoleKick_f );

	// match controls
	trap_Cmd_AddCommand( "match", Cmd_Match_f );

	// banning
	trap_Cmd_AddCommand( "addip", Cmd_AddIP_f );
	trap_Cmd_AddCommand( "removeip", Cmd_RemoveIP_f );
	trap_Cmd_AddCommand( "listip", Cmd_ListIP_f );
	trap_Cmd_AddCommand( "writeip", Cmd_WriteIP_f );

	trap_Cmd_AddCommand( "dumpASapi", G_asDumpAPI_f );
}

/*
* G_RemoveCommands
*/
void G_RemoveCommands( void ) {
	if( dedicated->integer ) {
		trap_Cmd_RemoveCommand( "say" );
	}
	trap_Cmd_RemoveCommand( "kick" );

	// match controls
	trap_Cmd_RemoveCommand( "match" );

	// banning
	trap_Cmd_RemoveCommand( "addip" );
	trap_Cmd_RemoveCommand( "removeip" );
	trap_Cmd_RemoveCommand( "listip" );
	trap_Cmd_RemoveCommand( "writeip" );

	trap_Cmd_RemoveCommand( "dumpASapi" );
}
