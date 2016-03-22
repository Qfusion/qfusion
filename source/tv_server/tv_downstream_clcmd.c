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

#include "tv_downstream_clcmd.h"

#include "tv_relay.h"
#include "tv_upstream.h"
#include "tv_downstream.h"
#include "tv_lobby.h"
#include "tv_relay_client.h"
#include "tv_cmds.h"

/*
* TV_Downstream_SendChannelList
*/
void TV_Downstream_SendChannelList( client_t *client )
{
	int i;

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] || tvs.upstreams[i]->relay.state == CA_CONNECTING )
			continue;
		TV_Relay_NameNotify( &tvs.upstreams[i]->relay, client );
	}
}

/*
* TV_Downstream_DelayNew
* 
* new command is delayed, because of a connect attempt while relay
* wasn't ready
*/
static void TV_Relay_DelayNew( client_t *client )
{
	assert( client );
	assert( client->relay && client->relay->state < CA_ACTIVE );

	// tell client we missed this "new" command, this way it'll be resent
	client->clientCommandExecuted--;
}

/*
* TV_Downstream_New_f
* 
* Sends the first message from the server to a connected client.
* This will be sent on the initial upstream and upon each server load.
*/
void TV_Downstream_New_f( client_t *client )
{
	int playernum, numpure;
	int tv_bitflags;
	purelist_t *iter;
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];

	// if in CS_AWAITING we have sended the response packet the new once already,
	// but client might have not got it so we send it again
	if( client->state >= CS_SPAWNED )
	{
		Com_DPrintf( "New not valid -- already spawned\n" );
		return;
	}

	// relay is not ready yet
	if( client->relay && client->relay->state < CA_ACTIVE )
	{
		TV_Relay_DelayNew( client );
		return;
	}

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	TV_Downstream_InitClientMessage( client, &message, messageData, sizeof( messageData ) );

	// send the serverdata
	MSG_WriteByte( &message, svc_serverdata );
	MSG_WriteLong( &message, APP_PROTOCOL_VERSION );
	if( !client->relay )
	{
		MSG_WriteLong( &message, tvs.lobby.spawncount );
		MSG_WriteShort( &message, tvs.lobby.snapFrameTime );
		MSG_WriteString( &message, FS_BaseGameDirectory() );
		MSG_WriteString( &message, FS_GameDirectory() );
	}
	else
	{
		MSG_WriteLong( &message, client->relay->servercount );
		MSG_WriteShort( &message, client->relay->snapFrameTime );
		MSG_WriteString( &message, client->relay->basegame );
		MSG_WriteString( &message, client->relay->game );
	}

	if( client->relay )
	{
		// we use our own playernum on the relay server
		MSG_WriteShort( &message, client->relay->playernum );
	}
	else
	{
		playernum = client - tvs.clients;
		MSG_WriteShort( &message, playernum );
	}

	// send full levelname
	if( !client->relay )
		MSG_WriteString( &message, tv_name->string );
	else
		MSG_WriteString( &message, client->relay->levelname );

	memset( &client->lastcmd, 0, sizeof( client->lastcmd ) );

	tv_bitflags = SV_BITFLAGS_TVSERVER;
	if( client->reliable )
		tv_bitflags |= SV_BITFLAGS_RELIABLE;

	MSG_WriteByte( &message, tv_bitflags ); // sv_bitflags

	// purelist
	if( !client->relay )
	{
		MSG_WriteShort( &message, 0 );
	}
	else
	{
		numpure = Com_CountPureListFiles( client->relay->purelist );

		MSG_WriteShort( &message, numpure );
		iter = client->relay->purelist;
		while( iter )
		{
			MSG_WriteString( &message, iter->filename );
			MSG_WriteLong( &message, iter->checksum );
			iter = iter->next;
		}
	}

	TV_Downstream_ClientResetCommandBuffers( client, true );

	TV_Downstream_SendMessageToClient( client, &message );

	Netchan_PushAllFragments( &client->netchan );

	// don't let it send reliable commands until we get the first configstring request
	client->state = CS_CONNECTING;
}

/*
* TV_Downstream_Configstrings_f
*/
static void TV_Downstream_Configstrings_f( client_t *client )
{
	int start;

	// relay is not ready yet
	if( client->relay && client->relay->state < CA_ACTIVE )
	{
		TV_Downstream_SendServerCommand( client, "reconnect" );
		return;
	}

	if( client->state == CS_CONNECTING )
	{
		Com_DPrintf( "Start Configstrings() from %s\n", client->name );
		client->state = CS_CONNECTED;
	}
	else
	{
		Com_DPrintf( "Configstrings() from %s\n", client->name );
	}

	if( client->state != CS_CONNECTED )
	{
		Com_DPrintf( "configstrings not valid -- already spawned\n" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if( atoi( Cmd_Argv( 1 ) ) != ( client->relay ? client->relay->servercount : tvs.lobby.spawncount ) )
	{
		TV_Downstream_SendServerCommand( client, "reconnect" );
		return;
	}

	if( !client->relay )
	{
		TV_Downstream_SendServerCommand( client, "cs %i \"%s\"", CS_TVSERVER, "1" );
		TV_Downstream_SendServerCommand( client, "cs %i \"%s\"", CS_AUDIOTRACK, tv_lobbymusic->string );
		TV_Downstream_SendServerCommand( client, "cmd baselines %i 0", tvs.lobby.spawncount );
		return;
	}

	start = atoi( Cmd_Argv( 2 ) );
	if( start < 0 )
	{
		start = 0;
	}

	// write a packet full of data
	while( start < MAX_CONFIGSTRINGS &&
		client->reliableSequence - client->reliableAcknowledge < MAX_RELIABLE_COMMANDS - 8 )
	{
		//		if( start == CS_HOSTNAME )
		//			Com_Printf( "cs %s\n", client->relay->configstrings[start] );

		if( client->relay->configstrings[start][0] )
		{
			TV_Downstream_SendServerCommand( client, "cs %i \"%s\"", start, client->relay->configstrings[start] );
		}
		start++;
	}

	// send next command
	if( start == MAX_CONFIGSTRINGS )
	{
		TV_Downstream_SendServerCommand( client, "cmd baselines %i 0", client->relay->servercount );
	}
	else
	{
		TV_Downstream_SendServerCommand( client, "cmd configstrings %i %i", client->relay->servercount, start );
	}
}

/*
* TV_Downstream_Baselines_f
*/
static void TV_Downstream_Baselines_f( client_t *client )
{
	int start;
	entity_state_t nullstate;
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];

	if( client->state != CS_CONNECTED )
		return;

	// relay is not ready yet
	if( client->relay && client->relay->state < CA_ACTIVE )
	{
		TV_Downstream_SendServerCommand( client, "reconnect" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if( atoi( Cmd_Argv( 1 ) ) != ( client->relay ? client->relay->servercount : tvs.lobby.spawncount ) )
	{
		TV_Downstream_New_f( client );
		return;
	}

	if( !client->relay )
	{
		TV_Downstream_SendServerCommand( client, "precache %i", tvs.lobby.spawncount );
		return;
	}

	start = atoi( Cmd_Argv( 2 ) );
	if( start < 0 )
		start = 0;

	memset( &nullstate, 0, sizeof( nullstate ) );

	// write a packet full of data
	TV_Downstream_InitClientMessage( client, &message, messageData, sizeof( messageData ) );

	while( message.cursize < FRAGMENT_SIZE * 3 && start < MAX_EDICTS )
	{
		if( client->relay->baselines[start].number )
		{
			MSG_WriteByte( &message, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, &client->relay->baselines[start], &message, true, true );
		}
		start++;
	}

	// send next command
	if( start == MAX_EDICTS )
	{
		TV_Downstream_SendServerCommand( client, "precache %i", client->relay->servercount );
	}
	else
	{
		TV_Downstream_SendServerCommand( client, "cmd baselines %i %i", client->relay->servercount, start );
	}

	TV_Downstream_AddReliableCommandsToMessage( client, &message );
	TV_Downstream_SendMessageToClient( client, &message );
}

/*
* TV_Downstream_Begin_f
*/
static void TV_Downstream_Begin_f( client_t *client )
{
	if( client->state != CS_CONNECTED )
		return;

	// relay is not ready yet
	if( client->relay && client->relay->state < CA_ACTIVE )
		return;

	// handle the case of a level changing while a client was connecting
	if( atoi( Cmd_Argv( 1 ) ) != ( client->relay ? client->relay->servercount : tvs.lobby.spawncount ) )
	{
		TV_Downstream_SendServerCommand( client, "changing" );
		TV_Downstream_SendServerCommand( client, "reconnect" );
		return;
	}

	client->state = CS_SPAWNED;

	if( client->relay )
		TV_Relay_ClientBegin( client->relay, client );
	else
		TV_Lobby_ClientBegin( client );

	//TV_Downstream_SendChannelList( client );
}

/*
* TV_Downstream_Disconnect_f
* The client is going to disconnect, so remove the upstream immediately
*/
static void TV_Downstream_Disconnect_f( client_t *client )
{
	TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "User disconnected" );
}

/*
* TV_Downstream_UserinfoCommand_f
*/
static void TV_Downstream_UserinfoCommand_f( client_t *client )
{
	char *info;

	info = Cmd_Argv( 1 );

	if( !Info_Validate( info ) )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error: Invalid userinfo" );
		return;
	}

	Q_strncpyz( client->userinfo, info, sizeof( client->userinfo ) );
	TV_Downstream_UserinfoChanged( client );
}

/*
* TV_Downstream_NoDelta_f
*/
static void TV_Downstream_NoDelta_f( client_t *client )
{
	client->nodelta = true;
	client->nodelta_frame = 0;
}

/*
* TV_Downstream_Multiview_f
*/
static void TV_Downstream_Multiview_f( client_t *client )
{
	bool mv;

	mv = ( atoi( Cmd_Argv( 1 ) ) != 0 );

	if( client->mv == mv )
		return;
	if( !client->tv )
		return;		// allow MV connections only for TV
	if( mv && (tvs.nummvclients == tv_maxmvclients->integer) )
		return;

	client->mv = mv;
	tvs.nummvclients = tvs.nummvclients + (mv ? 1 : -1);
}

/*
* TV_Downstream_Watch_f
*/
static void TV_Downstream_Watch_f( client_t *client )
{
	upstream_t *upstream;

	if( !TV_UpstreamForText( Cmd_Argv( 1 ), &upstream ) )
	{
		//TV_Dowstream_Printf( client, "Invalid stream\n" );
		return;
	}
	if( ( !client->relay && !upstream ) || ( client->relay && client->relay->upstream == upstream ) )
	{
		//TV_Dowstream_Printf( client, "You are already on that stream\n" );
		return;
	}

	if( TV_Downstream_ChangeStream( client, ( upstream ? &upstream->relay : NULL ) ) )
		Com_Printf( "%s" S_COLOR_WHITE " now watches %s" S_COLOR_WHITE " (%i)\n", client->name, upstream ? upstream->name : "lobby", upstream ? upstream->number + 1 : 0 );
}

/*
* TV_Downstream_Channels_f
*/
static void TV_Downstream_Channels_f( client_t *client )
{
	int i;
	upstream_t *upstream;

	if( client->state != CS_SPAWNED )
		return;

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		upstream = tvs.upstreams[i];
		if( upstream && upstream->relay.state > CA_CONNECTING )
			TV_Relay_NameNotify( &upstream->relay, client );
	}
}

/*
* TV_Downstream_DenyDownload
* Helper function for generating initdownload packets for denying download
*/
static void TV_Downstream_DenyDownload( client_t *client, const char *reason )
{
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];

	assert( client );
	assert( reason && reason[0] );

	// size -1 is used to signal that it's refused
	// URL field is used for deny reason
	TV_Downstream_InitClientMessage( client, &message, messageData, sizeof( messageData ) );
	TV_Downstream_SendServerCommand( client, "initdownload \"%s\" %i %u %i \"%s\"", "", -1, 0, false, reason );
	TV_Downstream_AddReliableCommandsToMessage( client, &message );
	TV_Downstream_SendMessageToClient( client, &message );
}

/*
* TV_Downstream_BeginDownload_f
* Responds to reliable download packet with reliable initdownload packet
*/
static void TV_Downstream_BeginDownload_f( client_t *client )
{
	TV_Downstream_DenyDownload( client, "Downloading is not allowed on this server" );
}


static bool CheckFlood( client_t *client )
{
	int i;

	assert( client );

	if( tv_floodprotection_messages->modified )
	{
		if( tv_floodprotection_messages->integer < 0 )
			Cvar_Set( "tv_floodprotection_messages", "0" );
		if( tv_floodprotection_messages->integer > MAX_FLOOD_MESSAGES )
			Cvar_Set( "tv_floodprotection_messages", va( "%i", MAX_FLOOD_MESSAGES ) );
		tv_floodprotection_messages->modified = false;
	}

	if( tv_floodprotection_seconds->modified )
	{
		if( tv_floodprotection_seconds->value <= 0 )
			Cvar_Set( "tv_floodprotection_seconds", "4" );
		tv_floodprotection_seconds->modified = false;
	}

	if( tv_floodprotection_penalty->modified )
	{
		if( tv_floodprotection_penalty->value < 0 )
			Cvar_Set( "tv_floodprotection_penalty", "20" );
		tv_floodprotection_penalty->modified = false;
	}

	// old protection still active
	if( tvs.realtime < client->flood.locktill )
	{
		TV_Downstream_Msg( client, NULL, NULL, false, "You can't talk for %d more seconds.\n",
			(int)( ( client->flood.locktill - tvs.realtime ) / 1000.0f ) + 1 );
		return true;
	}

	if( tv_floodprotection_messages->integer && tv_floodprotection_penalty->value > 0 )
	{
		i = client->flood.whenhead - tv_floodprotection_messages->integer + 1;
		if( i < 0 )
			i = MAX_FLOOD_MESSAGES + i;

		if( client->flood.when[i] && client->flood.when[i] <= tvs.realtime &&
			( tvs.realtime < client->flood.when[i] + tv_floodprotection_seconds->integer * 1000 ) )
		{
			client->flood.locktill = tvs.realtime + tv_floodprotection_penalty->value * 1000;
			TV_Downstream_Msg( client, NULL, NULL, false, "Flood protection: You can't talk for %d seconds.\n", tv_floodprotection_penalty->integer );
			return true;
		}

		client->flood.whenhead = ( client->flood.whenhead + 1 ) % MAX_FLOOD_MESSAGES;
		client->flood.when[client->flood.whenhead] = tvs.realtime;
	}

	return false;
}

/*
* TV_Cmd_Say_f
*/
void TV_Cmd_Say_f( client_t *client, bool arg0 )
{
	char *p;
	char text[256];

	if( CheckFlood( client ) )
		return;
	if( Cmd_Argc() < 2 && !arg0 )
		return;

	text[0] = 0;

	if( arg0 )
	{
		Q_strncatz( text, Cmd_Argv( 0 ), sizeof( text ) );
		Q_strncatz( text, " ", sizeof( text ) );
		Q_strncatz( text, Cmd_Args(), sizeof( text ) );
	}
	else
	{
		p = Cmd_Args();

		if( *p == '"' )
		{
			if( p[strlen( p )-1] == '"' )
				p[strlen( p )-1] = 0;
			p++;
		}
		Q_strncatz( text, p, sizeof( text ) );
	}

	// don't let text be too long for malicious reasons
	text[MAX_CHAT_BYTES - 1] = 0;

	Q_strncatz( text, "\n", sizeof( text ) );

	TV_Downstream_Msg( NULL, client->relay, client, true, "%s", text );
}

/*
* TV_Cmd_SayCmd_f
*/
void TV_Cmd_SayCmd_f( client_t *client )
{
	TV_Cmd_Say_f( client, false );
}

/*
* TV_Cmd_Spectators_f
*/
static void TV_Cmd_Spectators_f( client_t *client )
{
	int i;
	int count = 0;
	int start = 0;
	char line[64];
	char msg[1024];
	client_t *cl;
	relay_t *relay = client->relay;
	int maxclients = tv_maxclients->integer;

	if( Cmd_Argc() > 1 )
		start = atoi( Cmd_Argv( 1 ) );
	clamp( start, 0, maxclients - 1 );

	// print information
	msg[0] = 0;

	for( i = start; i < maxclients; i++ )
	{
		cl = &tvs.clients[i];
		if( cl->state >= CS_SPAWNED )
		{
			if( cl->relay != client->relay )
				continue;

			Q_snprintfz( line, sizeof( line ), "%s%s%s\n", count ? " " : "", S_COLOR_WHITE, cl->name );
			if( strlen( line ) + strlen( msg ) > sizeof( msg ) - 100 )
			{
				// can't print all of them in one packet
				Q_strncatz( msg, " ...\n", sizeof( msg ) );
				break;
			}

			if( !count )
				Q_strncatz( msg, "---------------\n", sizeof( msg ) );
			Q_strncatz( msg, line, sizeof( msg ) );
			count++;
		}
	}

	if( count )
		Q_strncatz( msg, "---------------\n", sizeof( msg ) );
	Q_strncatz( msg, va( "%i %s\n", count, Cmd_Argv( 0 ) ), sizeof( msg ) );
	TV_Downstream_Msg( client, relay, NULL, false, "%s", msg );

	if( i < maxclients )
		TV_Downstream_Msg( client, relay, NULL, false, "Type '%s %i' for more %s\n", Cmd_Argv( 0 ), i, Cmd_Argv( 0 ) );

}

// --

typedef struct
{
	const char *name;
	void ( *func )( client_t *client );
} ucmd_t;

static ucmd_t ucmds[] =
{
	// auto issued
	{ "new", TV_Downstream_New_f },
	{ "configstrings", TV_Downstream_Configstrings_f },
	{ "baselines", TV_Downstream_Baselines_f },
	{ "begin", TV_Downstream_Begin_f },
	{ "disconnect", TV_Downstream_Disconnect_f },
	{ "usri", TV_Downstream_UserinfoCommand_f },
	{ "nodelta", TV_Downstream_NoDelta_f },

	{ "multiview", TV_Downstream_Multiview_f },
	{ "watch", TV_Downstream_Watch_f },
	{ "channels", TV_Downstream_Channels_f },

	// issued by hand at client consoles
	//{ "info", SV_ShowServerinfo_f },

	{ "download", TV_Downstream_BeginDownload_f },
	//{ "nextdl", SV_NextDownload_f },

	{ "say", TV_Cmd_SayCmd_f },

	{ "spectators", TV_Cmd_Spectators_f },

	{ NULL, NULL }
};

/*
* TV_Downstream_ExecuteUserCommand
*/
void TV_Downstream_ExecuteUserCommand( client_t *client, char *s )
{
	ucmd_t *u;

	Cmd_TokenizeString( s );

	for( u = ucmds; u->name; u++ )
	{
		if( !strcmp( Cmd_Argv( 0 ), u->name ) )
		{
			u->func( client );
			return;
		}
	}

	if( client->relay && client->state >= CS_SPAWNED )
	{
		if( TV_Relay_ClientCommand_f( client->relay, client ) )
			return;
	}

	// unknown command, chat
	TV_Cmd_Say_f( client, true );
}
