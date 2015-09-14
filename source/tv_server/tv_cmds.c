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

#include "tv_cmds.h"

#include "tv_upstream.h"
#include "tv_upstream_demos.h"

static char *TV_ConnstateToString( connstate_t state )
{
	switch( state )
	{
	case CA_UNINITIALIZED:
		return "Unitialized";
	case CA_DISCONNECTED:
		return "Disconnected";
	case CA_CONNECTING:
		return "Connecting";
	case CA_HANDSHAKE:
		return "Handshake";
	case CA_CONNECTED:
		return "Connected";
	case CA_LOADING:
		return "Loading";
	case CA_ACTIVE:
		return "Active";
	default:
		return "Unknown";
	}
}

/*
* TV_Upstream_Status_f
*/
static void TV_Upstream_Status_f( void )
{
	int i;
	bool none;
	client_t *client;
	upstream_t *upstream;

	if( !TV_UpstreamForText( Cmd_Argv( 1 ), &upstream ) )
	{
		Com_Printf( "No such connection\n" );
		return;
	}

	if( upstream )
	{
		Com_Printf( "%s" S_COLOR_WHITE ": %s\n", upstream->name, NET_AddressToString( &upstream->serveraddress ) );
		Com_Printf( "Server name: %s\n", upstream->servername );
		Com_Printf( "Connection: %s\n", TV_ConnstateToString( upstream->state ) );
		Com_Printf( "Relay: %s\n", TV_ConnstateToString( upstream->relay.state ) );
	}
	else
	{
		Com_Printf( "lobby\n" );
	}

	Com_Printf( "Downstream connections:\n" );
	none = true;
	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		client = &tvs.clients[i];

		if( client->state == CS_FREE || client->state == CS_ZOMBIE )
			continue;
		if( !client->relay )
		{
			if( upstream )
				continue;
		}
		else
		{
			if( client->relay->upstream != upstream )
				continue;
		}

		Com_Printf( "%3i: %s" S_COLOR_WHITE " (%s)\n", i, client->name, NET_AddressToString( &client->netchan.remoteAddress ) );
		none = false;
	}
	if( none )
		Com_Printf( "- No downstream connections\n" );
}

/*
* TV_Status
*/
static void TV_Status( void )
{
	int i;
	bool none;
	client_t *client;

	Com_Printf( "Upstream connections:\n" );
	none = true;
	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		Com_Printf( "%3i: %22s: %s\n", i+1, NET_AddressToString( &tvs.upstreams[i]->serveraddress ),
			tvs.upstreams[i]->name );
		none = false;
	}
	if( none )
		Com_Printf( "- No upstream connections\n" );

	Com_Printf( "Downstream connections:\n" );
	none = true;
	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		client = &tvs.clients[i];

		if( client->state == CS_FREE || client->state == CS_ZOMBIE )
			continue;

		Com_Printf( "%3i: %s" S_COLOR_WHITE " (%s): %s" S_COLOR_WHITE " %s\n", i, client->name, NET_AddressToString( &client->netchan.remoteAddress ),
			( client->relay ? client->relay->upstream->name : "lobby" ), client->mv ? "MV" : "" );
		none = false;
	}
	if( none )
		Com_Printf( "- No downstream connections\n" );
}

/*
* TV_Status_f
*/
static void TV_Status_f( void )
{
	if( Cmd_Argc() == 1 )
		TV_Status();
	else if( Cmd_Argc() == 2 )
		TV_Upstream_Status_f();
	else
		Com_Printf( "Usage: status [upstream]\n" );
}

/*
* TV_Cmd_f
*/
static void TV_Cmd_f( void )
{
	upstream_t *upstream;

	if( Cmd_Argc() != 3 )
	{
		Com_Printf( "Usage: cmd <server> <command>\n" );
		return;
	}

	if( !TV_UpstreamForText( Cmd_Argv( 1 ), &upstream ) )
	{
		Com_Printf( "No such upstream\n" );
		return;
	}

	if( !upstream )
	{
		Com_Printf( "Can't send commands to lobby\n" );
		return;
	}

	if( !Cmd_Argv( 2 )[0] )
	{
		Com_Printf( "Empty command\n" );
		return;
	}

	TV_Upstream_AddReliableCommand( upstream, Cmd_Argv( 2 ) );
}

/*
* TV_Disconnect_f
*/
static void TV_Disconnect_f( void )
{
	upstream_t *upstream;

	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "Usage: disconnect <server>\n" );
		return;
	}

	if( !TV_UpstreamForText( Cmd_Argv( 1 ), &upstream ) )
	{
		Com_Printf( "No such upstream\n" );
		return;
	}

	if( !upstream )
	{
		Com_Printf( "Can't disconnect lobby\n" );
		return;
	}

	//	TV_Upstream_Disconnect( upstream, "Disconnected by adminstrator" );
	TV_Upstream_Shutdown( upstream, "Disconnected by adminstrator" );
}

/*
* TV_GenericConnect_f
*/
static void TV_GenericConnect_f( socket_type_t socket )
{
	netadr_t serveraddress;
	char *servername, *password, *name;
	upstream_t *upstream;
	unsigned int delay;

	if( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: %s <server> [password] [name] [delay]\n", Cmd_Argv( 0 ) );
		return;
	}

	if( !NET_StringToAddress( Cmd_Argv( 1 ), &serveraddress ) )
	{
		Com_Printf( "Bad server address: %s\n", Cmd_Argv( 1 ) );
		return;
	}

	servername = TempCopyString( Cmd_Argv( 1 ) );
	password = ( Cmd_Argc() >= 3 ? TempCopyString( Cmd_Argv( 2 ) ) : NULL );
	name = ( Cmd_Argc() >= 4 ? Cmd_Argv( 3 ) : "" );
	delay = ( Cmd_Argc() >= 5 ? (unsigned)atoi( Cmd_Argv( 4 ) )*1000 : RELAY_GLOBAL_DELAY );

	if( TV_UpstreamForText( servername, &upstream ) )
	{
		if( upstream->state >= CA_CONNECTED && upstream->relay.delay == delay
			&& upstream->socket && upstream->socket->type == socket )
		{
			Com_Printf( "Already connected to %s\n", servername );
			goto exit;
		}

		TV_Upstream_Shutdown( upstream, "Disconnected by adminstrator" );
		upstream = NULL;
	}

	upstream = TV_Upstream_New( servername, name, delay );
	assert( upstream );
	TV_Upstream_Connect( upstream, servername, password, socket, &serveraddress );

exit:
	Mem_TempFree( servername );
	if( password )
		Mem_TempFree( password );
}

/*
* TV_Connect_f
*/
static void TV_Connect_f( void )
{
	TV_GenericConnect_f( SOCKET_UDP );
}

/*
* TV_TCPConnect_f
*/
#ifdef TCP_ALLOW_TVCONNECT
static void TV_TCPConnect_f( void )
{
	TV_GenericConnect_f( SOCKET_TCP );
}
#endif

/*
* TV_Demo_f
*/
static void TV_Demo_f( void )
{
	char *servername, *name, *mode;
	upstream_t *upstream;
	unsigned int delay;
	bool randomize = true;

	if( Cmd_Argc() < 2 )
	{
		Com_Printf( "Usage: %s <pattern|playlist> [name] [ordered|random] [delay]\n", Cmd_Argv( 0 ) );
		return;
	}

	servername = TempCopyString( Cmd_Argv( 1 ) );
	name = ( Cmd_Argc() >= 3 ? Cmd_Argv( 2 ) : "" );
	mode = ( Cmd_Argc() >= 4 ? Cmd_Argv( 3 ) : "" );
	delay = ( Cmd_Argc() >= 5 ? (unsigned)atoi( Cmd_Argv( 4 ) )*1000 : RELAY_MIN_DELAY );
	if( !Q_stricmp( mode, "ordered" ) )
		randomize = false;

	if( TV_UpstreamForText( servername, &upstream ) )
	{
		if( upstream->state >= CA_CONNECTED && upstream->relay.delay == delay )
		{
			Com_Printf( "Already connected to %s\n", servername );
			goto exit;
		}

		TV_Upstream_Shutdown( upstream, "Disconnected by adminstrator" );
		upstream = NULL;
	}

	upstream = TV_Upstream_New( servername, name, delay );
	assert( upstream );
	TV_Upstream_StartDemo( upstream, servername, randomize );

exit:
	Mem_TempFree( servername );
}

/*
* TV_Record_f
*
* record <upstream> <demoname>
* Begins recording a demo from the current position
*/
static void TV_Record_f( void )
{
	const char *name;
	bool res, silent;
	upstream_t *upstream;

	if( Cmd_Argc() < 3 )
	{
		Com_Printf( "%s <upstream> <demoname>\n", Cmd_Argv( 0 ) );
		return;
	}

	name = Cmd_Argv( 1 );
	res = TV_UpstreamForText( name, &upstream );

	if( !res || !upstream )
	{
		Com_Printf( "No such upstream: %s\n", name );
		return;
	}

	if( Cmd_Argc() > 3 && !Q_stricmp( Cmd_Argv( 3 ), "silent" ) )
		silent = true;
	else
		silent = false;

	TV_Upstream_StartDemoRecord( upstream, Cmd_Argv( 2 ), silent );
}

/*
* TV_Stop_f
*
* stop <upstream> [silent] [cancel]
*/
static void TV_Stop_f( void )
{
	int arg;
	const char *name;
	bool res, silent, cancel;
	upstream_t *upstream;

	if( Cmd_Argc() < 2 )
	{
		Com_Printf( "%s <upstream> [silent] [cancel]\n", Cmd_Argv( 0 ) );
		return;
	}

	name = Cmd_Argv( 1 );
	res = TV_UpstreamForText( name, &upstream );

	if( !res || !upstream )
	{
		Com_Printf( "No such upstream: %s\n", name );
		return;
	}

	// look through all the args
	silent = cancel = false;
	for( arg = 2; arg < Cmd_Argc(); arg++ )
	{
		if( !Q_stricmp( Cmd_Argv( arg ), "silent" ) )
			silent = true;
		else if( !Q_stricmp( Cmd_Argv( arg ), "cancel" ) )
			cancel = true;
	}

	TV_Upstream_StopDemoRecord( upstream, silent, cancel );
}

/*
* TV_Rename_f
*
* rename <upstream> <newname>
*/
void TV_Rename_f( void )
{
	const char *text, *newname;
	bool res;
	upstream_t *upstream;

	if( Cmd_Argc() < 3 )
	{
		Com_Printf( "%s <upstream> <newname>\n", Cmd_Argv( 0 ) );
		return;
	}

	text = Cmd_Argv( 1 );
	newname = Cmd_Argv( 2 );

	res = TV_UpstreamForText( text, &upstream );
	if( !res || !upstream )
	{
		Com_Printf( "No such upstream: %s\n", text );
		return;
	}

	if( upstream->customname )
		Mem_Free( upstream->customname );
	upstream->customname = TV_Upstream_CopyString( upstream, newname );

	TV_Upstream_SetName( upstream, upstream->backupname );
}

/*
* TV_Heartbeat_f
*/
static void TV_Heartbeat_f( void )
{
	tvs.lobby.next_heartbeat = Sys_Milliseconds();
}

/*
* TV_Music_f
*/
static void TV_Music_f( void )
{
	const char *text, *music;
	bool res;
	upstream_t *upstream;

	if( Cmd_Argc() < 3 )
	{
		Com_Printf( "%s <upstream> <music>\n", Cmd_Argv( 0 ) );
		return;
	}

	text = Cmd_Argv( 1 );
	music = Cmd_Argv( 2 );

	res = TV_UpstreamForText( text, &upstream );
	if( !res || !upstream )
	{
		Com_Printf( "No such upstream: %s\n", text );
		return;
	}

	TV_Upstream_SetAudioTrack( upstream, music );
}

// List of commands
typedef struct
{
	char *name;
	void ( *func )( void );
} cmd_function_t;

static cmd_function_t cmdlist[] =
{
	{ "connect", TV_Connect_f },
#if defined(TCP_ALLOW_TVCONNECT)
	{ "tcpconnect", TV_TCPConnect_f },
#endif
	{ "disconnect", TV_Disconnect_f },

	{ "demo", TV_Demo_f },
	{ "record", TV_Record_f },
	{ "stop", TV_Stop_f },

	{ "status", TV_Status_f },
	{ "cmd", TV_Cmd_f },

	{ "rename", TV_Rename_f },

	{ "heartbeat", TV_Heartbeat_f },

	{ "music", TV_Music_f },

	{ NULL, NULL }
};

/*
* TV_AddCommands
*/
void TV_AddCommands( void )
{
	cmd_function_t *cmd;

	for( cmd = cmdlist; cmd->name; cmd++ )
		Cmd_AddCommand( cmd->name, cmd->func );
}

/*
* TV_RemoveCommands
*/
void TV_RemoveCommands( void )
{
	cmd_function_t *cmd;

	for( cmd = cmdlist; cmd->name; cmd++ )
		Cmd_RemoveCommand( cmd->name );
}
