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

#include "server.h"

/*
* SV_MOTD_SetMOTD
*
* Helper to set svs.motd
*/
void SV_MOTD_SetMOTD( const char *motd ) {
	char *pos;
	size_t l = min( strlen( motd ), MAX_MOTD_LEN );

	if( l == 0 ) {
		if( svs.motd ) {
			Mem_Free( svs.motd );
		}
		svs.motd = NULL;
	} else {
		if( svs.motd ) {
			Mem_Free( svs.motd );
		}

		svs.motd = ( char * ) Mem_Alloc( sv_mempool, ( l + 1 ) * sizeof( char ) );
		Q_strncpyz( svs.motd, motd, l + 1 );

		// MOTD may come from a CRLF file so strip off \r's (we waste a few bytes here)
		while( ( pos = strchr( svs.motd, '\r' ) ) != NULL ) {
			memmove( pos, pos + 1, strlen( pos + 1 ) + 1 );
		}
	}
}

/*
* SV_MOTD_LoadFromFile
*
* Attempts to load the MOTD from sv_MOTDFile, on success sets
* sv_MOTDString.
*/
void SV_MOTD_LoadFromFile( void ) {
	char *f;

	FS_LoadFile( sv_MOTDFile->string, (void **)&f, NULL, 0 );
	if( !f ) {
		Com_Printf( "Couldn't load MOTD file: %s\n", sv_MOTDFile->string );
		Cvar_ForceSet( "sv_MOTDFile", "" );
		SV_MOTD_SetMOTD( "" );
		return;
	}

	if( strchr( f, '"' ) ) { // FIXME: others?
		Com_Printf( "Warning: MOTD file contains illegal characters.\n" );
		Cvar_ForceSet( "sv_MOTDFile", "" );
		SV_MOTD_SetMOTD( "" );
	} else {
		SV_MOTD_SetMOTD( f );
	}

	FS_FreeFile( f );
}

/*
* SV_MOTD_Update
*
* set the motd to the correct value depending on sv_MOTDString and sv_MOTDFile
*/
void SV_MOTD_Update( void ) {
	if( !sv_MOTD->integer ) {
		return;
	}

	if( sv_MOTDString->string[0] ) {
		SV_MOTD_SetMOTD( sv_MOTDString->string );
	} else if( sv_MOTDFile->string[0] ) {
		SV_MOTD_LoadFromFile();
	} else {
		SV_MOTD_SetMOTD( "" );
	}
}

/*
* SV_MOTD_Get_f
*
* Comand to return MOTD
*/
void SV_MOTD_Get_f( client_t *client ) {
	int flag = ( Cmd_Argc() > 1 ? 1 : 0 );

	if( sv_MOTD->integer && svs.motd && svs.motd[0] ) {
		SV_AddGameCommand( client, va( "motd %d \"%s\n\"", flag, svs.motd ) );
	}
}
