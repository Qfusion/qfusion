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
#include "../qcommon/webdownload.h"


//===============================================================================
//
//OPERATOR CONSOLE ONLY COMMANDS
//
//These commands can only be entered from stdin or by a remote operator datagram
//===============================================================================

/*
* SV_FindPlayer
* Helper for the functions below. It finds the client_t for the given name or id
*/
static client_t *SV_FindPlayer( char *s )
{
	client_t *cl;
	client_t *player;
	int i;
	int idnum = 0;

	if( !s )
		return NULL;

	// numeric values are just slot numbers
	if( s[0] >= '0' && s[0] <= '9' )
	{
		idnum = atoi( s );
		if( idnum < 0 || idnum >= sv_maxclients->integer )
		{
			Com_Printf( "Bad client slot: %i\n", idnum );
			return NULL;
		}

		player = &svs.clients[idnum];
		goto found_player;
	}

	// check for a name match
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state )
			continue;
		if( !Q_stricmp( cl->name, s ) )
		{
			player = cl;
			goto found_player;
		}
	}

	Com_Printf( "Userid %s is not on the server\n", s );
	return NULL;

found_player:
	if( !player->state || !player->edict )
	{
		Com_Printf( "Client %s is not active\n", s );
		return NULL;
	}

	return player;
}

//=========================================================

// FIXME: rewrite this to use AsyncStream!

/*
* SV_WebDownloadProgress
* Callback function for webdownloads.
*/
static int webDownloadPercentPrint;
static qboolean webDownloadPercentStarted;

static int SV_WebDownloadProgress( float percent )
{
	int shortPercent = (int)( 100 * percent );

	//if( webDownloadPercentPrint >= 100 )
	//	webDownloadPercentPrint = 0;

	if( !webDownloadPercentStarted )
	{
		Com_Printf( "Download progress:" );
		webDownloadPercentStarted = qtrue;
	}

	if( webDownloadPercentPrint + 4 < shortPercent )
	{
		Com_Printf( " %i%c", shortPercent, '%' );
		webDownloadPercentPrint = shortPercent;
	}

	return 0;
}

/*
* SV_WebDownload
*/
static qboolean SV_WebDownload( const char *baseUrl, const char *filepath, qboolean overwrite, qboolean silent )
{
	qboolean success;
	int alloc_size;
	char *temppath, *writepath, *url;

	if( developer->integer )
		silent = qfalse;

	if( !baseUrl || !baseUrl[0] || !filepath )
		return qfalse;

	if( !strrchr( baseUrl, '/' ) )
	{
		if( !silent )
			Com_Printf( "SV_WebDownload: Invalid URL\n" );
		return qfalse;
	}

	if( filepath[0] == '/' ) // filepath should never begin with a slash
		filepath++;

	if( !COM_ValidateRelativeFilename( filepath ) )
	{
		if( !silent )
			Com_Printf( "SV_WebDownload: Invalid filename\n" );
		return qfalse;
	}

	if( !COM_FileExtension( filepath ) )
	{
		if( !silent )
			Com_Printf( "SV_WebDownload: no file extension\n" );
		return qfalse;
	}

	// full url (baseurl + path)
	alloc_size = strlen( baseUrl ) + 1 + strlen( filepath ) + 1;
	url = Mem_TempMalloc( alloc_size );
	if( baseUrl[ strlen( baseUrl ) - 1 ] == '/' ) // url includes last slash
		Q_snprintfz( url, alloc_size, "%s%s", baseUrl, filepath );
	else
		Q_snprintfz( url, alloc_size, "%s/%s", baseUrl, filepath );

	// add .tmp (relative + .tmp)
	alloc_size = strlen( filepath ) + strlen( ".tmp" ) + 1;
	temppath = Mem_TempMalloc( alloc_size );
	Q_snprintfz( temppath, alloc_size, "%s.tmp", filepath );

	// full write path for curl
	alloc_size = strlen( FS_WriteDirectory() ) + 1 + strlen( temppath ) + 1;
	writepath = Mem_TempMalloc( alloc_size );
	Q_snprintfz( writepath, alloc_size, "%s/%s", FS_WriteDirectory(), temppath );

	webDownloadPercentPrint = 0;
	webDownloadPercentStarted = qfalse;

	success = Web_Get( url, NULL, writepath, qtrue, 60 * 30, 60, SV_WebDownloadProgress, qfalse );

	if( webDownloadPercentStarted )
		Com_Printf( "\n" );

	if( !success )
	{
		if( !silent )
			Com_Printf( "Failed to download remote file.\n" );
		goto failed;
	}

	// rename the downloaded file
	if( !FS_MoveBaseFile( temppath, filepath ) )
	{
		if( !overwrite )
		{
			if( !silent )
				Com_Printf( "Failed to rename temporary file.\n" );
			goto failed;
		}

		// check if it failed because there already exists a file with the same name
		// and in this case remove this file
		if( FS_FOpenBaseFile( filepath, NULL, FS_READ ) != -1 )
		{
			char *backfile;

			alloc_size = strlen( filepath ) + strlen( ".bak" ) + 1;
			backfile = Mem_TempMalloc( alloc_size );
			Q_snprintfz( backfile, alloc_size, "%s.bak", filepath );

			// if there is already a .bak file, destroy it
			if( FS_FOpenBaseFile( backfile, NULL, FS_READ ) != -1 )
				FS_RemoveBaseFile( backfile );

			// move the current file into .bak file
			if( !FS_MoveBaseFile( filepath, backfile ) )
			{
				Mem_TempFree( backfile );
				if( !silent )
					Com_Printf( "Failed to backup destination file.\n" );
				goto failed;
			}

			// now try renaming the downloaded file again
			if( !FS_MoveBaseFile( temppath, filepath ) )
			{
				// didn't work, so restore the backup file
				if( FS_MoveBaseFile( backfile, filepath ) )
				{
					if( !silent )
						Com_Printf( "Failed to rename temporary file, restoring from backup.\n" );
				}
				else
				{
					if( !silent )
						Com_Printf( "Failed to rename temporary file and restore from backup.\n" );
				}

				Mem_TempFree( backfile );
				goto failed;
			}

			Mem_TempFree( backfile );
		}
	}

	Mem_TempFree( temppath );
	Mem_TempFree( writepath );
	Mem_TempFree( url );

	return qtrue;

failed:
	if( !silent )
		Com_Printf( "Removing temporary file: %s\n", writepath );
	FS_RemoveAbsoluteFile( writepath );
	Mem_TempFree( temppath );
	Mem_TempFree( writepath );
	Mem_TempFree( url );

	return qfalse;
}

/*
* SV_AutoUpdateFromWeb
*/
void SV_AutoUpdateFromWeb( qboolean checkOnly )
{
	static const char *autoUpdateBaseUrl = APP_UPDATE_URL APP_SERVER_UPDATE_DIRECTORY;
	char checksumString1[32], checksumString2[32];
	unsigned int checksum;
	qboolean success;
	int length, filenum;
	qbyte *data;
	const char *token, *ptr;
	char path[MAX_QPATH];
	int downloadCount = 0, downloadFailed = 0;
	char newVersionTag[MAX_QPATH];
	qboolean newVersion = qfalse;

	if( !dedicated->integer )
		return;

	assert( svs.mapcmd[0] );

	if( !checkOnly )
		SV_UpdateActivity();

	Com_Printf( "\n" );
	Com_Printf( "========== Starting Auto Update ===========\n" );

	Com_Printf( "Checking for updates\n" );

	// download the update file list
	success = SV_WebDownload( autoUpdateBaseUrl, APP_SERVER_UPDATE_FILE, qtrue, qtrue );

	// set as last updated today
	if( !checkOnly )
		Cvar_ForceSet( "sv_lastAutoUpdate", va( "%i", (int)Com_DaysSince1900() ) );

	if( !success ) // no update to do
		goto done;

	// read the file list
	if( ( length = FS_FOpenBaseFile( APP_SERVER_UPDATE_FILE, &filenum, FS_READ ) ) == -1 )
	{
		Com_Printf( "WARNING: Couldn't find %s\n", path );
		goto done;
	}

	if( !length )
	{
		FS_FCloseFile( filenum );
		goto done;
	}

	data = Mem_TempMalloc( length + 1 );
	FS_Read( data, length, filenum );
	FS_FCloseFile( filenum );
	FS_RemoveBaseFile( APP_SERVER_UPDATE_FILE );

	ptr = (const char *)data;

	// first token is always the current release version
	token = COM_ParseExt( &ptr, qtrue );
	if( !token[0] )
		goto cancel;

	// compare versions
	Q_strncpyz( newVersionTag, token, sizeof( newVersionTag ) );
	if( atof( newVersionTag ) > atof( va( "%4.3f", APP_VERSION ) ) )
		newVersion = qtrue;

	while( ptr )
	{
		// we got what should be a checksum
		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] )
			goto cancel;

		// copy checksum reported by server
		Q_strncpyz( checksumString1, token, sizeof( checksumString1 ) );

		// get filename
		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] )
			goto cancel;

		// filename should never begin with a slash
		if( token[0] == '/' )
			token++;

		Q_strncpyz( path, token, sizeof( path ) );

		// we got what should be a file path
		if( !COM_ValidateRelativeFilename( path ) )
		{
			Com_Printf( "WARNING: Invalid filename %s\n", path );
			continue;
		}

		checksum = FS_ChecksumBaseFile( path );
		Q_snprintfz( checksumString2, sizeof( checksumString2 ), "%u", checksum );

		// if same checksum no need to update
		if( !strcmp( checksumString1, checksumString2 ) )
			continue;

		// if it's a pack file and the file exists it can't be replaced, so skip
		if( FS_CheckPakExtension( path ) && checksum )
		{
			Com_Printf( "WARNING: Purity check failed for: %s\n", path );
			Com_Printf( "WARNING: This file has been locally modified. It is highly \n" );
			Com_Printf( "WARNING: recommended to restore the original file.\n" );
			Com_Printf( "WARNING: Reinstalling \""APPLICATION"\" might be convenient.\n" );
			continue;	
		}

		if( checkOnly )
		{
			Com_Printf( "File update available : %s\n", path );
			continue;
		}

		if( developer->integer )
			Com_Printf( "Downloading update of %s (checksum %s local checksum %s)\n", path, checksumString1, checksumString2 );
		else
			Com_Printf( "Updating %s\n", path );

		if( !SV_WebDownload( autoUpdateBaseUrl, path, qtrue, qtrue ) )
		{
			Com_Printf( "Failed to update %s\n", path );
			downloadFailed++;
		}

		downloadCount++;
	}

cancel:
	Mem_TempFree( data );
done:
	if( newVersion )
	{
		if( downloadCount )
		{
			if( downloadFailed )
				Com_Printf( "This version of "APPLICATION" was updated incompletely\n" );
			else
				Com_Printf( "This version of "APPLICATION" was updated successfully\n\n" );
		}

		Com_Printf( "****** Version %s of "APPLICATION" is available. ******\n", newVersionTag );
		Com_Printf( "****** Please download the new version at "APP_URL" ******\n" );
	}
	else if( downloadCount )
	{
		if( downloadFailed )
			Com_Printf( APPLICATION" was updated incompletely\n" );
		else
			Com_Printf( APPLICATION" was updated successfully\n" );
	}
	else if( !checkOnly )
	{
		if( downloadFailed )
			Com_Printf( "At least one file failed to update\n" );
		else
			Com_Printf( APPLICATION" is up to date\n" );
	}

	Com_Printf( "========== Auto Update Finished ===========\n" );
	Com_Printf( "\n" );

	// update the map list, which also does a filesystem rescan
	ML_Update();

	// if there are any new filesystem entries, restart
	if( FS_GetNotifications() & FS_NOTIFT_NEWPAKS )
	{
		if( sv.state != ss_dead )
		{
			// restart the current map, SV_Map also rescans the filesystem
			Com_Printf( "The server will now restart...\n\n" );

			// start the default map if current map isn't available
			Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", svs.mapcmd[0] ? svs.mapcmd : sv_defaultmap->string ) );
		}
	}
}

/*
* SV_AutoUpdate_f
*/
static void SV_AutoUpdate_f( void )
{
	if( !sv_pure->integer )
	{
		Com_Printf( "Autoupdate is only available for pure servers\n" );
		return;
	}

	SV_AutoUpdateFromWeb( qfalse );
}

/*
* SV_AutoUpdateCheck_f
*/
static void SV_AutoUpdateCheck_f( void )
{
	if( !sv_pure->integer )
	{
		Com_Printf( "Autoupdate is only available for pure servers\n" );
		return;
	}

	SV_AutoUpdateFromWeb( qtrue );
}

/*
* SV_Download_f
* Download command issued from server
*/
static void SV_Download_f( void )
{
	qboolean success;
	char *s;
	char url[MAX_STRING_CHARS], filepath[MAX_QPATH], writepath[MAX_QPATH];

	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "Usage: %s <url>\n", Cmd_Argv( 0 ) );
		Com_Printf( "Downloads .pk3 or .pak from URL to gamedir and adds it to the server\n" );
		Com_Printf( "Note, server will not function properly while downloading\n" );
		return;
	}

	s = Cmd_Argv( 1 );
	if( !Com_GlobMatch( "*://*", s, qfalse ) )
		Q_strncpyz( url, "http://", sizeof( url ) );
	else
		url[0] = 0;
	Q_strncatz( url, s, sizeof( url ) );

	s = strrchr( url, '/' );
	if( !s )
	{
		Com_Printf( "%s: invalid URL\n", Cmd_Argv( 0 ) );
		return;
	}

	Q_strncpyz( filepath, va( "%s/%s", FS_GameDirectory(), s + 1 ), sizeof( filepath ) );
	Q_strncpyz( writepath, va( "%s.tmp", filepath ), sizeof( writepath ) );

	if( !FS_CheckPakExtension( writepath ) )
	{
		Com_Printf( "Missing or invalid archive extension. Only download of pack files is supported\n" );
		return;
	}

	Com_Printf( "download url: %s\n", url );

	webDownloadPercentPrint = 0;

	success = Web_Get( url, NULL, writepath, qtrue, 60 * 30, 60, SV_WebDownloadProgress, qfalse );

	if( !success )
	{
		Com_Printf( "Server web download failed\n" );
		return;
	}

	if( !FS_MoveBaseFile( writepath, filepath ) )
	{
		Com_Printf( "Couldn't rename the downloaded file. Download failed\n" );
		return;
	}

	Com_Printf( "Download successful\n" );

	// update the map list, which also does a filesystem rescan
	ML_Update();
}

/*
* SV_Map_f
* 
* User command to change the map
* map: restart game, and start map
* devmap: restart game, enable cheats, and start map
* gamemap: just start the map
*/
static void SV_Map_f( void )
{
	char *map;
	char mapname[MAX_CONFIGSTRING_CHARS];
	qboolean found = qfalse;

	if( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: %s <map>\n", Cmd_Argv( 0 ) );
		return;
	}

	// if map "<map>" is used Cmd_Args() will return the "" as well.
	if( Cmd_Argc() == 2 )
		map = Cmd_Argv( 1 );
	else
		map = Cmd_Args();

	Com_DPrintf( "SV_GameMap(%s)\n", map );

	// applies to fullnames and filenames (whereas + strlen( "maps/" ) wouldnt)
	if( strlen( map ) >= MAX_CONFIGSTRING_CHARS )
	{
		Com_Printf( "Map name too long.\n" );
		return;
	}

	Q_strncpyz( mapname, map, sizeof( mapname ) );
	if( ML_ValidateFilename( mapname ) )
	{
		COM_StripExtension( mapname );
		if( ML_FilenameExists( mapname ) )
		{
			found = qtrue;
		}
		else
		{
			ML_Update();
			if( ML_FilenameExists( mapname ) )
				found = qtrue;
		}
	}

	if( !found )
	{
		if( ML_ValidateFullname( map ) )
		{
			Q_strncpyz( mapname, ML_GetFilename( map ), sizeof( mapname ) );
			if( *mapname )
				found = qtrue;
		}

		if( !found )
		{
			Com_Printf( "Couldn't find map: %s\n", map );
			return;
		}
	}

	if( FS_GetNotifications() & FS_NOTIFT_NEWPAKS )
	{
		FS_RemoveNotifications( FS_NOTIFT_NEWPAKS );
		sv.state = ss_dead; // don't save current level when changing
	}
	else if( !Q_stricmp( Cmd_Argv( 0 ), "map" ) || !Q_stricmp( Cmd_Argv( 0 ), "devmap" ) )
	{
		sv.state = ss_dead; // don't save current level when changing
	}

	// start up the next map
	SV_Map( mapname, !Q_stricmp( Cmd_Argv( 0 ), "devmap" ) );

	// archive server state
	Q_strncpyz( svs.mapcmd, mapname, sizeof( svs.mapcmd ) );
}

/*
* SV_MapComplete_f
*/
static char **SV_MapComplete_f( const char *partial )
{
	return ML_CompleteBuildList( partial );
}

//===============================================================

/*
* SV_Status_f
*/
void SV_Status_f( void )
{
	int i, j, l;
	client_t *cl;
	const char *s;
	int ping;
	if( !svs.clients )
	{
		Com_Printf( "No server running.\n" );
		return;
	}
	Com_Printf( "map              : %s\n", sv.mapname );

	Com_Printf( "num score ping name            lastmsg address               port   rate  \n" );
	Com_Printf( "--- ----- ---- --------------- ------- --------------------- ------ ------\n" );
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( !cl->state )
			continue;
		Com_Printf( "%3i ", i );
		Com_Printf( "%5i ", cl->edict->r.client->r.frags );

		if( cl->state == CS_CONNECTED )
			Com_Printf( "CNCT " );
		else if( cl->state == CS_ZOMBIE )
			Com_Printf( "ZMBI " );
		else if( cl->state == CS_CONNECTING )
			Com_Printf( "AWAI " );
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf( "%4i ", ping );
		}

		s = COM_RemoveColorTokens( cl->name );
		Com_Printf( "%s", s );
		l = 16 - (int)strlen( s );
		for( j = 0; j < l; j++ )
			Com_Printf( " " );

		Com_Printf( "%7i ", svs.realtime - cl->lastPacketReceivedTime );

		s = NET_AddressToString( &cl->netchan.remoteAddress );
		Com_Printf( "%s", s );
		l = 22 - (int)strlen( s );
		for( j = 0; j < l; j++ )
			Com_Printf( " " );

		Com_Printf( "%5i", cl->netchan.game_port );
#ifndef RATEKILLED
		// wsw : jal : print real rate in use
		Com_Printf( "  " );
		if( cl->edict && ( cl->edict->r.svflags & SVF_FAKECLIENT ) )
			Com_Printf( "BOT" );
		else if( cl->rate == 99999 )
			Com_Printf( "LAN" );
		else
			Com_Printf( "%5i", cl->rate );
#endif
		Com_Printf( " " );
		if( cl->mv )
			Com_Printf( "MV" );
		Com_Printf( "\n" );
	}
	Com_Printf( "\n" );
}

/*
* SV_Heartbeat_f
*/
static void SV_Heartbeat_f( void )
{
	svc.last_heartbeat = 0;
	svc.last_mmheartbeat = 0;
}

/*
* SV_Serverinfo_f
* Examine or change the serverinfo string
*/
static void SV_Serverinfo_f( void )
{
	Com_Printf( "Server info settings:\n" );
	Info_Print( Cvar_Serverinfo() );
}

/*
* SV_DumpUser_f
* Examine all a users info strings
*/
static void SV_DumpUser_f( void )
{
	client_t *client;
	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "Usage: info <userid>\n" );
		return;
	}

	client = SV_FindPlayer( Cmd_Argv( 1 ) );
	if( !client )
		return;

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( client->userinfo );
}

/*
* SV_KillServer_f
* Kick everyone off, possibly in preparation for a new game
*/
static void SV_KillServer_f( void )
{
	if( !svs.initialized )
		return;

	SV_ShutdownGame( "Server was killed", qfalse );
}

/*
* SV_CvarCheck_f
* Ask the client to inform us of the current value of a cvar
*/
static void SV_CvarCheck_f( void )
{
	client_t *client;
	int i;

	if( !svs.initialized )
		return;

	if( Cmd_Argc() != 3 )
	{
		Com_Printf( "Usage: cvarcheck <userid> <cvar name>\n" );
		return;
	}

	if( !Q_stricmp( Cmd_Argv( 1 ), "all" ) )
	{
		for( i = 0, client = svs.clients; i < sv_maxclients->integer; i++, client++ )
		{
			if( !client->state )
				continue;

			if( client->tvclient )
				continue;

			SV_SendServerCommand( client, "cvarinfo \"%s\"", Cmd_Argv( 2 ) );
		}

		return;
	}

	client = SV_FindPlayer( Cmd_Argv( 1 ) );
	if( !client )
	{
		Com_Printf( "%s is not valid client id\n", Cmd_Argv( 1 ) );
		return;
	}

	SV_SendServerCommand( client, "cvarinfo \"%s\"", Cmd_Argv( 2 ) );
}

//===========================================================

/*
* SV_InitOperatorCommands
*/
void SV_InitOperatorCommands( void )
{
	Cmd_AddCommand( "heartbeat", SV_Heartbeat_f );
	Cmd_AddCommand( "status", SV_Status_f );
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
	Cmd_AddCommand( "dumpuser", SV_DumpUser_f );

	Cmd_AddCommand( "map", SV_Map_f );
	Cmd_AddCommand( "devmap", SV_Map_f );
	Cmd_AddCommand( "gamemap", SV_Map_f );
	Cmd_AddCommand( "killserver", SV_KillServer_f );

	Cmd_AddCommand( "serverrecord", SV_Demo_Start_f );
	Cmd_AddCommand( "serverrecordstop", SV_Demo_Stop_f );
	Cmd_AddCommand( "serverrecordcancel", SV_Demo_Cancel_f );
	Cmd_AddCommand( "serverrecordpurge", SV_Demo_Purge_f );

	Cmd_AddCommand( "purelist", SV_PureList_f );

	if( dedicated->integer )
	{
		Cmd_AddCommand( "download", SV_Download_f );
		Cmd_AddCommand( "autoupdate", SV_AutoUpdate_f );
		Cmd_AddCommand( "autoupdatecheck", SV_AutoUpdateCheck_f );
	}

	Cmd_AddCommand( "cvarcheck", SV_CvarCheck_f );

	Cmd_SetCompletionFunc( "map", SV_MapComplete_f );
	Cmd_SetCompletionFunc( "devmap", SV_MapComplete_f );
	Cmd_SetCompletionFunc( "gamemap", SV_MapComplete_f );
}

/*
* SV_ShutdownOperatorCommands
*/
void SV_ShutdownOperatorCommands( void )
{
	Cmd_RemoveCommand( "heartbeat" );
	Cmd_RemoveCommand( "status" );
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "dumpuser" );

	Cmd_RemoveCommand( "map" );
	Cmd_RemoveCommand( "devmap" );
	Cmd_RemoveCommand( "gamemap" );
	Cmd_RemoveCommand( "killserver" );

	Cmd_RemoveCommand( "serverrecord" );
	Cmd_RemoveCommand( "serverrecordstop" );
	Cmd_RemoveCommand( "serverrecordcancel" );
	Cmd_RemoveCommand( "serverrecordpurge" );

	Cmd_RemoveCommand( "purelist" );

	if( dedicated->integer )
	{
		Cmd_RemoveCommand( "download" );
		Cmd_RemoveCommand( "autoupdate" );
		Cmd_RemoveCommand( "autoupdatecheck" );
	}

	Cmd_RemoveCommand( "cvarcheck" );
}
