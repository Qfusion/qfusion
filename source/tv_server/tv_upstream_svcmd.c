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

#include "tv_local.h"

#include "tv_upstream.h"
#include "tv_upstream_svcmd.h"
#include "tv_upstream_demos.h"

/*
* TV_Upstream_ParseConfigstringCommand_f
*/
static void TV_Upstream_HandleConfigstring( upstream_t *upstream, int index, const char *val )
{
	char hostname[MAX_CONFIGSTRING_CHARS];
	msg_t msg;
	uint8_t msgbuf[MAX_MSGLEN];

	if( !val || !val[0] )
		return;

	if( index < 0 || index >= MAX_CONFIGSTRINGS )
		TV_Upstream_Error( upstream, "configstring > MAX_CONFIGSTRINGS" );

	Q_strncpyz( upstream->configstrings[index], val, sizeof( upstream->configstrings[index] ) );

	if( index == CS_AUTORECORDSTATE )
	{
		// don't do a thing until we receive a "precache" command
		if( upstream->precacheDone )
			TV_Upstream_AutoRecordAction( upstream, upstream->configstrings[CS_AUTORECORDSTATE] );
		return;
	}

	if( index != CS_HOSTNAME )
		return;

	if( !upstream->demo.playing )
	{
		TV_Upstream_SetName( upstream, val );
		return;
	}

	// demos often come with generic hostnames, attempt to workaround that
	if( !Q_stricmp( val, APPLICATION " server" ) )
	{
		char *temp;
		size_t temp_size;
		const char *filebase;

		filebase = COM_FileBase( upstream->demo.filename );
		temp_size = strlen( filebase ) + strlen( APP_DEMO_EXTENSION_STR ) + 1;
		temp = Mem_TempMalloc( temp_size );
		Q_strncpyz( temp, filebase, temp_size );
		COM_ReplaceExtension( temp, APP_DEMO_EXTENSION_STR, temp_size );

		if( Com_GlobMatch( "*_auto[0-9][0-9][0-9][0-9]" APP_DEMO_EXTENSION_STR, temp, false )
			|| Com_GlobMatch( "*_mvd" APP_DEMO_EXTENSION_STR, temp, false ) )
			temp[strrchr( temp, '_' ) - temp] = '\0';
		else
			COM_StripExtension( temp );

		Q_strncpyz( hostname, va( S_COLOR_ORANGE "R: " S_COLOR_WHITE "%s", temp ), sizeof( hostname ) );

		Mem_TempFree( temp );
	}
	else
	{
		Q_strncpyz( hostname, va( S_COLOR_ORANGE "R: " S_COLOR_WHITE "%s", val ), sizeof( hostname ) );
	}

	TV_Upstream_SetName( upstream, hostname );

	// override CS_HOSTNAME in next packet
	MSG_Init( &msg, msgbuf, sizeof( msgbuf ) );
	MSG_WriteByte( &msg, svc_servercs );
	MSG_WriteString( &msg, va( "cs %i \"%s\"", CS_HOSTNAME, hostname ) );
	TV_Upstream_SavePacket( upstream, &msg, 0 );
}

/*
* TV_Upstream_ParseConfigstringCommand_f
*/
static void TV_Upstream_ParseConfigstringCommand_f( upstream_t *upstream )
{
	int i, argc, index;
	char *val;

	if( Cmd_Argc() < 3 )
		return;

	argc = Cmd_Argc();
	for( i = 1; i < argc-1; i += 2 )
	{
		index = atoi( Cmd_Argv( 1 ) );
		val = Cmd_Argv( 2 );

		TV_Upstream_HandleConfigstring( upstream, index, val );
	}
}

/*
* TV_Upstream_ForwardToServer_f
*/
static void TV_Upstream_ForwardToServer_f( upstream_t *upstream )
{
	if( upstream->state != CA_CONNECTED && upstream->state != CA_ACTIVE )
	{
		Com_Printf( "Can't \"%s\", not connected\n", Cmd_Argv( 0 ) );
		return;
	}

	// don't forward the first argument
	if( Cmd_Argc() > 1 )
		TV_Upstream_AddReliableCommand( upstream, Cmd_Args() );
}


/*
* TV_Upstream_Precache_f
*/
static void TV_Upstream_Precache_f( upstream_t *upstream )
{
	upstream->precacheDone = true;

	TV_Upstream_AutoRecordAction( upstream, upstream->configstrings[CS_AUTORECORDSTATE] );

	// TODO actually check that we have the stuff
	TV_Upstream_AddReliableCommand( upstream, va( "begin %i\n", atoi( Cmd_Argv( 1 ) ) ) );
	TV_Upstream_AddReliableCommand( upstream, "multiview 1\n" );
}

/*
* TV_Upstream_Multiview_f
*/
static void TV_Upstream_Multiview_f( upstream_t *upstream )
{
	upstream->multiview = ( atoi( Cmd_Argv( 1 ) ) != 0 );
}

/*
* TV_Upstream_Changing_f
*/
static void TV_Upstream_Changing_f( upstream_t *upstream )
{
	if( upstream->demo.recording )
		TV_Upstream_StopDemoRecord( upstream, upstream->demo.autorecording, false );
}

/*
* TV_Upstream_ServerReconnect_f
* 
* The server is changing levels
*/
static void TV_Upstream_ServerReconnect_f( upstream_t *upstream )
{
	if( upstream->demo.playing )
		return;

	if( upstream->state < CA_CONNECTED )
	{
		Com_Printf( "%s: reconnect request while not connected\n", NET_AddressToString( &upstream->serveraddress ) );
		return;
	}

	if( upstream->demo.recording )
		TV_Upstream_StopDemoRecord( upstream, upstream->demo.autorecording, false );

	Com_Printf( "%s: reconnecting...\n", NET_AddressToString( &upstream->serveraddress ) );

	upstream->connect_count = 0;
	upstream->rejected = 0;
#ifdef TCP_ALLOW_TVCONNECT
	upstream->connect_time = tvs.realtime;
#else
	upstream->connect_time = tvs.realtime - 1500;
#endif
	upstream->state = CA_HANDSHAKE;
	TV_Upstream_AddReliableCommand( upstream, "new" );
}

/*
* TV_Upstream_ServerDisconnect_f
* 
* The server is changing levels
*/
static void TV_Upstream_ServerDisconnect_f( upstream_t *upstream )
{
	int type;

	type = atoi( Cmd_Argv( 1 ) );
	if( type < 0 || type >= DROP_TYPE_TOTAL )
		type = DROP_TYPE_GENERAL;

	TV_Upstream_Error( upstream, "Server disconnected: %s", Cmd_Argv( 2 ) );
}

// ---

typedef struct
{
	char *name;
	void ( *func )( upstream_t *upstream );
} svcmd_t;

static svcmd_t svcmds[] =
{
	{ "forcereconnect", TV_Upstream_Reconnect_f },
	{ "reconnect", TV_Upstream_ServerReconnect_f },
	{ "changing", TV_Upstream_Changing_f },
	{ "precache", TV_Upstream_Precache_f },
	{ "cmd", TV_Upstream_ForwardToServer_f },
	{ "cs", TV_Upstream_ParseConfigstringCommand_f },
	{ "disconnect", TV_Upstream_ServerDisconnect_f },
	//{ "initdownload", CL_InitDownload_f },
	{ "multiview", TV_Upstream_Multiview_f },
	{ "cvarinfo", NULL },

	{ NULL, NULL }
};

/*
* TV_Upstream_ParseServerCommand
*/
void TV_Upstream_ParseServerCommand( upstream_t *upstream, msg_t *msg )
{
	const char *s;
	char *text;
	svcmd_t *cmd;

	text = MSG_ReadString( msg );
	Cmd_TokenizeString( text );
	s = Cmd_Argv( 0 );

	// filter out these server commands to be called from the client
	for( cmd = svcmds; cmd->name; cmd++ )
	{
		if( !strcmp( s, cmd->name ) )
		{
			if( cmd->func )
				cmd->func( upstream );
			return;
		}
	}

	Com_Printf( "Unknown server command: %s\n", s );
}
