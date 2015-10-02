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

#include "tv_downstream.h"

#include "tv_relay.h"
#include "tv_lobby.h"
#include "tv_downstream_parse.h"
#include "tv_downstream_oob.h"
#include "tv_relay_client.h"

/*
* TV_Downstream_ClientResetCommandBuffers
*/
void TV_Downstream_ClientResetCommandBuffers( client_t *client, bool resetReliable )
{
	// clear the sounds datagram
	MSG_Init( &client->soundsmsg, client->soundsmsgData, sizeof( client->soundsmsgData ) );
	MSG_Clear( &client->soundsmsg );

	if( resetReliable )
	{                   // reset the reliable commands buffer
		client->clientCommandExecuted = 0;
		client->reliableAcknowledge = 0;
		client->reliableSequence = 0;
		client->reliableSent = 0;
		memset( client->reliableCommands, 0, sizeof( client->reliableCommands ) );
	}

	// reset frames and game commands
	memset( client->gameCommands, 0, sizeof( client->gameCommands ) );
	client->gameCommandCurrent = 0;
	client->lastframe = -1;
	client->lastSentFrameNum = 0;
	memset( client->snapShots, 0, sizeof( client->snapShots ) );

	// reset the usercommands buffer(clc_move)
	client->UcmdTime = 0;
	client->UcmdExecuted = 0;
	client->UcmdReceived = 0;
	memset( client->ucmds, 0, sizeof( client->ucmds ) );
}

/*
* TV_Downstream_AddGameCommand
*/
void TV_Downstream_AddGameCommand( relay_t *relay, client_t *client, const char *cmd )
{
	int index;

	assert( client );
	assert( client->relay == relay );
	assert( cmd && cmd[0] );

	client->gameCommandCurrent++;
	index = client->gameCommandCurrent & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( client->gameCommands[index].command, cmd, sizeof( client->gameCommands[index].command ) );
	if( client->lastSentFrameNum )
	{
		client->gameCommands[index].framenum = client->lastSentFrameNum + 1;
	}
	else
	{
		if( relay )
			client->gameCommands[index].framenum = relay->framenum;
		else
			client->gameCommands[index].framenum = tvs.lobby.framenum;
	}
}

/*
* TV_Downstream_Msg
* 
* NULL sends to all the message to all clients
*/
void TV_Downstream_Msg( client_t *client, relay_t *relay, client_t *who, bool chat, const char *format, ... )
{
	int i;
	char msg[1024];
	va_list	argptr;
	char *s, *p;

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	// double quotes are bad
	p = msg;
	while( ( p = strchr( p, '\"' ) ) != NULL )
		*p = '\'';

	if( chat )
		s = va( "tvch \"%s\" \"%s\"", (who ? who->name : ""), msg );
	else
		s = va( "pr \"%s\"",  msg );

	if( !client )
	{
		// mirror at server console
		if( !relay )  // lobby
			Com_Printf( "%s", msg );

		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state < CS_SPAWNED )
				continue;
			if( client->relay != relay )
				continue;

			TV_Downstream_AddGameCommand( relay, client, s );
		}
	}
	else
	{
		if( client->state == CS_SPAWNED )
			TV_Downstream_AddGameCommand( client->relay, client, s );
	}
}

// kill all chars with code >= 127
// (127 is not exactly a highchar, but we drop it, too)
static void strip_highchars( char *in )
{
	char *out = in;
	for( ; *in; in++ )
		if( ( unsigned char )*in < 127 )
			*out++ = *in;
	*out = 0;
}

/*
* TV_Downstream_FixName
* 
* Make name valid, so it's not used by anyone else or so. See G_SetName
* Client can be given, so conflict with that client's name won't matter
* The returned value will be overwritten by the next call to this function
*/
char *TV_Downstream_FixName( const char *original_name, client_t *client )
{
	const char *invalid_prefixes[] = { "console", "[team]", "[spec]", "[bot]", "[coach]", "[tv]", NULL };
	client_t *other;
	static char name[MAX_NAME_BYTES];
	char colorless[MAX_NAME_BYTES];
	int i, trynum, trylen;
	int c_ascii;
	int maxchars;

	// we allow NULL to be passed for name
	if( !original_name )
		original_name = "";

	Q_strncpyz( name, original_name, sizeof( name ) );

	// life is hard, UTF-8 will have to go
	strip_highchars( name );

	COM_SanitizeColorString( va( "%s", name ), name, sizeof( name ), -1, COLOR_WHITE );

	// remove leading whitespace
	while( name[0] == ' ' )
		memmove( name, name + 1, strlen( name ) );

	// remove trailing whitespace
	while( strlen( name ) && name[strlen(name)-1] == ' ' )
		name[strlen(name)-1] = '\0';

	Q_strncpyz( colorless, COM_RemoveColorTokens( name ), sizeof( colorless ) );

	maxchars = MAX_NAME_CHARS;
	if( client && client->tv )
		maxchars = min( maxchars + 10, MAX_NAME_BYTES-1 );

	// require at least one non-whitespace ascii char in the name
	// (this will upset people who would like to have a name entirely in a non-latin
	// script, but it makes damn sure you can't get an empty name by exploiting some
	// utf-8 decoder quirk)
	c_ascii = 0;
	for( i = 0; colorless[i]; i++ )
		if( colorless[i] > 32 && colorless[i] < 127 )
			c_ascii++;

	if( !c_ascii )
	{
		Q_strncpyz( name, "Player", sizeof( name ) );
		Q_strncpyz( colorless, COM_RemoveColorTokens( name ), sizeof( colorless ) );
	}

	for( i = 0; invalid_prefixes[i] != NULL; i++ )
	{
		if( !Q_strnicmp( colorless, invalid_prefixes[i], strlen( invalid_prefixes[i] ) ) )
		{
			Q_strncpyz( name, "Player", sizeof( name ) );
			Q_strncpyz( colorless, COM_RemoveColorTokens( name ), sizeof( colorless ) );
		}
	}

	trynum = 1;
	do
	{
		for( i = 0, other = tvs.clients; i < tv_maxclients->integer; i++, other++ )
		{
			if( ( client && other == client ) || other->state == CS_FREE || other->state == CS_ZOMBIE )
				continue;

			// if nick is already in use, try with (number) appended
			if( !Q_stricmp( colorless, COM_RemoveColorTokens( other->name ) ) )
			{
				if( trynum != 1 )  // remove last try
					name[strlen( name ) - strlen( va( "%s(%i)", S_COLOR_WHITE, trynum-1 ) )] = 0;

				// make sure there is enough space for the postfix
				trylen = strlen( va( "%s(%i)", S_COLOR_WHITE, trynum ) );
				if( (int)strlen( colorless ) + trylen > maxchars )
				{
					COM_SanitizeColorString( va( "%s", name ), name, sizeof( name ),
						maxchars - trylen, COLOR_WHITE );
					Q_strncpyz( colorless, COM_RemoveColorTokens( name ), sizeof( colorless ) );
				}

				// add the postfix
				Q_strncatz( name, va( "%s(%i)", S_COLOR_WHITE, trynum ), sizeof( name ) );
				Q_strncpyz( colorless, COM_RemoveColorTokens( name ), sizeof( colorless ) );

				// go trough all clients again
				trynum++;
				break;
			}
		}
	}
	while( i != tv_maxclients->integer && trynum <= MAX_CLIENTS );

	return name;
}

/*
* TV_Downstream_UserinfoChanged
* 
* Pull specific info from a newly changed userinfo string
* into a more C friendly form.
*/
void TV_Downstream_UserinfoChanged( client_t *client )
{
	char *val;

	assert( client );
	assert( Info_Validate( client->userinfo ) );

	// force the IP key/value pair so the game can filter based on ip
	if( !Info_SetValueForKey( client->userinfo, "socket", NET_SocketTypeToString( client->netchan.socket->type ) ) )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error: Couldn't set userinfo (socket)\n" );
		return;
	}
	if( !Info_SetValueForKey( client->userinfo, "ip", NET_AddressToString( &client->netchan.remoteAddress ) ) )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error: Couldn't set userinfo (ip)\n" );
		return;
	}

	// we handle name ourselves here, since tv module doesn't know about all the players
	val = TV_Downstream_FixName( Info_ValueForKey( client->userinfo, "name" ), client );
	Q_strncpyz( client->name, val, sizeof( client->name ) );
	if( !Info_SetValueForKey( client->userinfo, "name", client->name ) )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error: Couldn't set userinfo (name)" );
		return;
	}

	if( client->relay )
		TV_Relay_ClientUserinfoChanged( client->relay, client );

	if( !Info_Validate( client->userinfo ) )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error: Invalid userinfo (after game)" );
		return;
	}
}

/*
* TV_Downstream_Netchan_Transmit
*/
static bool TV_Downstream_Netchan_Transmit( netchan_t *netchan, msg_t *msg )
{
	int zerror;

	// if we got here with unsent fragments, fire them all now
	if( !Netchan_PushAllFragments( netchan ) )
		return false;

	if( tv_compresspackets->integer )
	{
		zerror = Netchan_CompressMessage( msg );
		if( zerror < 0 )
		{
			// it's compression error, just send uncompressed
			Com_Printf( "Compression error (%i), sending packet uncompressed\n", zerror );
		}
	}

	return Netchan_Transmit( netchan, msg );
}

/*
* TV_Downstream_AddServerCommand
* 
* The given command will be transmitted to the client, and is guaranteed to
* not have future snapshot_t executed before it is executed
*/
void TV_Downstream_AddServerCommand( client_t *client, const char *cmd )
{
	int index;
	unsigned int i;

	assert( client );
	assert( cmd && strlen( cmd ) );

	if( !cmd || !cmd[0] || !strlen( cmd ) )
		return;

	// ch : To avoid overflow of messages from excessive amount of configstrings
	// we batch them here. On incoming "cs" command, we'll trackback the queue
	// to find a pending "cs" command that has space in it. If we'll find one,
	// we'll batch this there, if not, we'll create a new one.
	if( !strncmp( cmd, "cs ", 3 ) )
	{
		// length of the index/value (leave room for one space and null char)
		size_t len = strlen( cmd ) - 1;
		for( i = client->reliableSequence; i > client->reliableSent; i-- )
		{
			size_t otherLen;
			char *otherCmd;

			otherCmd= client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1)];
			if( !strncmp( otherCmd, "cs ", 3 ) )
			{
				otherLen = strlen( otherCmd );
				// is there any room? (should check for sizeof client->reliableCommands[0]?)
				if( (otherLen + len) < MAX_STRING_CHARS )
				{
					// yahoo, put it in here
					Q_strncatz( otherCmd, cmd + 2, MAX_STRING_CHARS - 1 );
					return;
				}
			}
		}
	}

	client->reliableSequence++;
	// if we would be losing an old command that hasn't been acknowledged, we must drop the connection
	// we check == instead of >= so a broadcast print added by SV_DropClient() doesn't cause a recursive drop client
	if( client->reliableSequence - client->reliableAcknowledge == MAX_RELIABLE_COMMANDS + 1 )
	{
		//Com_Printf( "===== pending server commands =====\n" );
		for( i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++ )
		{
			Com_DPrintf( "cmd %5d: %s\n", i, client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS-1 )] );
		}
		Com_DPrintf( "cmd %5d: %s\n", i, cmd );
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Server command overflow" );
		return;
	}

	index = client->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( client->reliableCommands[index], cmd, sizeof( client->reliableCommands[index] ) );
}

/*
* TV_Downstream_SendServerCommand
* 
* Sends a reliable command string to be interpreted by
* the client: "cs", "changing", "disconnect", etc
* A NULL client will broadcast to all clients
*/
void TV_Downstream_SendServerCommand( client_t *cl, const char *format, ... )
{
	va_list	argptr;
	char message[MAX_MSGLEN];
	client_t *client;
	int i;

	va_start( argptr, format );
	Q_vsnprintfz( message, sizeof( message ), format, argptr );
	va_end( argptr );

	if( cl != NULL )
	{
		if( cl->state < CS_CONNECTING )
			return;
		TV_Downstream_AddServerCommand( cl, message );
		return;
	}

	// send the data to all relevant clients
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		if( client->state < CS_CONNECTING )  // wsw: Medar: or connected?
			continue;
		TV_Downstream_AddServerCommand( client, message );
	}
}

/*
* TV_Downstream_AddReliableCommandsToMessage
* 
* (re)send all server commands the client hasn't acknowledged yet
*/
void TV_Downstream_AddReliableCommandsToMessage( client_t *client, msg_t *msg )
{
	unsigned int i;

	// write any unacknowledged serverCommands
	for( i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++ )
	{
		if( !strlen( client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS-1 )] ) )
			continue;

		MSG_WriteByte( msg, svc_servercmd );
		if( !client->reliable )
			MSG_WriteLong( msg, i );
		MSG_WriteString( msg, client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS-1 )] );
	}

	client->reliableSent = client->reliableSequence;
	if( client->reliable )
		client->reliableAcknowledge = client->reliableSent;
}

/*
* TV_Downstream_InitClientMessage
*/
void TV_Downstream_InitClientMessage( client_t *client, msg_t *msg, uint8_t *data, size_t size )
{
	assert( client );

	if( client->edict && ( client->edict->r.svflags & SVF_FAKECLIENT ) )
		return;

	if( data && size )
		MSG_Init( msg, data, size );
	MSG_Clear( msg );

	// write the last client-command we received so it's acknowledged
	if( !client->reliable )
	{
		MSG_WriteByte( msg, svc_clcack );
		MSG_WriteLong( msg, client->clientCommandExecuted );
		MSG_WriteLong( msg, client->UcmdReceived ); // acknowledge the last ucmd
	}
}

/*
* TV_Downstream_SendMessageToClient
*/
bool TV_Downstream_SendMessageToClient( client_t *client, msg_t *msg )
{
	assert( client );

	client->lastPacketSentTime = tvs.realtime;
	return TV_Downstream_Netchan_Transmit( &client->netchan, msg );
}


/*
* TV_Downstream_DropClient
*/
void TV_Downstream_DropClient( client_t *drop, int type, const char *format, ... )
{
	va_list	argptr;
	char string[1024];
	msg_t Message;
	uint8_t MessageData[MAX_MSGLEN];

	va_start( argptr, format );
	Q_vsnprintfz( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Com_Printf( "%s" S_COLOR_WHITE " dropped: %s\n", drop->name, string );

	TV_Downstream_InitClientMessage( drop, &Message, MessageData, sizeof( MessageData ) );

	TV_Downstream_SendServerCommand( drop, "disconnect %i \"%s\"", type, string );
	TV_Downstream_AddReliableCommandsToMessage( drop, &Message );

	TV_Downstream_SendMessageToClient( drop, &Message );
	Netchan_PushAllFragments( &drop->netchan );

	if( drop->relay && /*drop->relay->state == CA_ACTIVE && */drop->state >= CS_CONNECTING )
		TV_Relay_ClientDisconnect( drop->relay, drop );

	// make sure everything is clean
	TV_Downstream_ClientResetCommandBuffers( drop, true );

	SNAP_FreeClientFrames( drop );

	if( drop->download.name )
	{
		if( drop->download.data )
		{
			FS_FreeBaseFile( drop->download.data );
			drop->download.data = NULL;
		}

		Mem_ZoneFree( drop->download.name );
		drop->download.name = NULL;

		drop->download.size = 0;
		drop->download.timeout = 0;
	}

	if( drop->individual_socket )
		NET_CloseSocket( &drop->socket );

	if( drop->mv )
	{
		tvs.nummvclients--;
		drop->mv = false;
	}

	memset( &drop->flood, 0, sizeof( drop->flood ) );

	drop->edict = NULL;
	drop->relay = NULL;
	drop->tv = false;
	drop->state = CS_ZOMBIE;    // become free in a few seconds
	drop->name[0] = 0;
}

/*
* TV_Downstream_ChangeStream
*/
bool TV_Downstream_ChangeStream( client_t *client, relay_t *relay )
{
	relay_t *oldrelay;

	assert( client );

	oldrelay = client->relay;

	if( relay )
	{
		if( !TV_Relay_CanConnect( relay, client, client->userinfo ) )
			return false;
	}
	else
	{
		if( !TV_Lobby_CanConnect( client, client->userinfo ) )
			return false;
	}

	if( oldrelay )
		TV_Relay_ClientDisconnect( oldrelay, client );
	else
		TV_Lobby_ClientDisconnect( client );

	TV_Downstream_ClientResetCommandBuffers( client, false );

	if( relay )
		TV_Relay_ClientConnect( relay, client );
	else
		TV_Lobby_ClientConnect( client );

	TV_Downstream_SendServerCommand( client, "changing" );
	TV_Downstream_SendServerCommand( client, "reconnect" );
	client->state = CS_CONNECTED;

	// let upstream servers know how many clients are connected
	userinfo_modified = true;

	return true;
}

/*
* TV_Downstream_ProcessPacket
*/
static bool TV_Downstream_ProcessPacket( netchan_t *netchan, msg_t *msg )
{
	/*int sequence, sequence_ack;
	int game_port = -1;*/
	int zerror;

	if( !Netchan_Process( netchan, msg ) )
		return false; // wasn't accepted for some reason

	// now if compressed, expand it
	MSG_BeginReading( msg );
	/*sequence = */MSG_ReadLong( msg );
	/*sequence_ack = */MSG_ReadLong( msg );
	/*game_port = */MSG_ReadShort( msg );
	if( msg->compressed )
	{
		zerror = Netchan_DecompressMessage( msg );
		if( zerror < 0 )
		{          // compression error. Drop the packet
			Com_DPrintf( "TV_Downstream_ProcessPacket: Compression error %i. Dropping packet\n", zerror );
			return false;
		}
	}

	return true;
}

/*
* TV_Downstream_ReadPackets
*/
void TV_Downstream_ReadPackets( void )
{
	int i, socketind, ret, game_port;
	client_t *cl;
#ifdef TCP_ALLOW_TVCONNECT
	socket_t newsocket;
#endif
	socket_t *socket;
	netadr_t address;
	msg_t msg;
	uint8_t msgData[MAX_MSGLEN];

#ifdef TCP_ALLOW_TVCONNECT
	socket_t* tcpsockets [] =
	{
		&tvs.socket_tcp,
		&tvs.socket_tcp6,
	};
#endif

	socket_t* sockets [] =
	{
		&tvs.socket_udp,
		&tvs.socket_udp6,
	};

	MSG_Init( &msg, msgData, sizeof( msgData ) );

#ifdef TCP_ALLOW_TVCONNECT
	for( socketind = 0; socketind < sizeof( tcpsockets ) / sizeof( tcpsockets[0] ); socketind++ )
	{
		socket = tcpsockets[socketind];

		if( socket->open )
		{
			while( true )
			{
				// find a free slot
				for( i = 0; i < MAX_INCOMING_CONNECTIONS; i++ )
				{
					if( !tvs.incoming[i].active )
						break;
				}
				if( i == MAX_INCOMING_CONNECTIONS )
					break;

				if( ( ret = NET_Accept( socket, &newsocket, &address ) ) == 0 )
					break;
				if( ret == -1 )
				{
					Com_Printf( "NET_Accept: Error: %s\n", NET_ErrorString() );
					continue;
				}

				tvs.incoming[i].active = true;
				tvs.incoming[i].socket = newsocket;
				tvs.incoming[i].address = address;
				tvs.incoming[i].time = tvs.realtime;
			}
		}

		for( i = 0; i < MAX_INCOMING_CONNECTIONS; i++ )
		{
			if( !tvs.incoming[i].active )
				continue;

			ret = NET_GetPacket( &tvs.incoming[i].socket, &address, &msg );
			if( ret == -1 )
			{
				NET_CloseSocket( &tvs.incoming[i].socket );
				tvs.incoming[i].active = false;
			}
			else if( ret == 1 )
			{
				if( *(int *)msg.data != -1 )
				{
					// sequence packet without upstreams
					NET_CloseSocket( &tvs.incoming[i].socket );
					tvs.incoming[i].active = false;
					continue;
				}

				TV_Downstream_UpstreamlessPacket( &tvs.incoming[i].socket, &address, &msg );
			}
		}
	}
#endif

	for( socketind = 0; socketind < sizeof( sockets ) / sizeof( sockets[0] ); socketind++ )
	{
		socket = sockets[socketind];

		while( socket->open && ( ret = NET_GetPacket( socket, &address, &msg ) ) != 0 )
		{
			if( ret == -1 )
			{
				Com_Printf( "NET_GetPacket: Error: %s\n", NET_ErrorString() );
				continue;
			}

			// check for upstreamless packet (0xffffffff) first
			if( *(int *)msg.data == -1 )
			{
				TV_Downstream_UpstreamlessPacket( socket, &address, &msg );
				continue;
			}

			// read the game port out of the message so we can fix up
			// stupid address translating routers
			MSG_BeginReading( &msg );
			MSG_ReadLong( &msg ); // sequence number
			MSG_ReadLong( &msg ); // sequence number
			game_port = MSG_ReadShort( &msg ) & 0xffff;
			// data follows

			// check for packets from connected clients
			for( i = 0, cl = tvs.clients; i < tv_maxclients->integer; i++, cl++ )
			{
				unsigned short remoteaddr_port, addr_port;

				if( cl->state == CS_FREE || cl->state == CS_ZOMBIE )
					continue;
				if( !NET_CompareBaseAddress( &address, &cl->netchan.remoteAddress ) )
					continue;
				if( cl->netchan.game_port != game_port )
					continue;

				remoteaddr_port = NET_GetAddressPort( &cl->netchan.remoteAddress );
				addr_port = NET_GetAddressPort( &address );
				if( remoteaddr_port != addr_port )
				{
					Com_DPrintf( "%s" S_COLOR_WHITE ": Fixing up a translated port from %i to %i\n", cl->name,
						remoteaddr_port, addr_port );
					NET_SetAddressPort( &cl->netchan.remoteAddress, addr_port );
				}

				if( TV_Downstream_ProcessPacket( &cl->netchan, &msg ) )
				{                                           // this is a valid, sequenced packet, so process it
					cl->lastPacketReceivedTime = tvs.realtime;
					TV_Downstream_ParseClientMessage( cl, &msg );
				}
				break;
			}
		}
	}

	// handle clients with individual sockets
	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		cl = &tvs.clients[i];

		if( cl->state == CS_ZOMBIE || cl->state == CS_FREE )
			continue;

		if( !cl->individual_socket )
			continue;

		// not while, we only handle one packet per client at a time here
		if( ( ret = NET_GetPacket( cl->netchan.socket, &address, &msg ) ) != 0 )
		{
			if( ret == -1 )
			{
				Com_Printf( "%s" S_COLOR_WHITE ": Error receiving packet: %s\n", cl->name, NET_ErrorString() );
				if( cl->reliable )
					TV_Downstream_DropClient( cl, DROP_TYPE_GENERAL, "Error receiving packet: %s", NET_ErrorString() );
			}
			else
			{
				if( *(int *)msg.data == -1 )
				{
					TV_Downstream_UpstreamlessPacket( cl->netchan.socket, &address, &msg );
				}
				else
				{
					if( TV_Downstream_ProcessPacket( &cl->netchan, &msg ) )
					{
						cl->lastPacketReceivedTime = tvs.realtime;
						TV_Downstream_ParseClientMessage( cl, &msg );
					}
				}
			}
		}
	}
}

/*
* TV_Downstream_CheckTimeouts
*/
void TV_Downstream_CheckTimeouts( void )
{
	client_t *client;
	int i;

#ifdef TCP_ALLOW_TVCONNECT
	// timeout incoming upstreams
	for( i = 0; i < MAX_INCOMING_CONNECTIONS; i++ )
	{
		if( tvs.incoming[i].active && tvs.incoming[i].time + 1000 * 15 < tvs.realtime )
		{
			Com_Printf( "Incoming TCP upstream from %s timed out\n", NET_AddressToString( &tvs.incoming[i].address ) );
			NET_CloseSocket( &tvs.incoming[i].socket );
			tvs.incoming[i].active = false;
		}
	}
#endif

	// timeout clients
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		// message times may be wrong across a changelevel
		if( client->lastPacketReceivedTime > tvs.realtime )
			client->lastPacketReceivedTime = tvs.realtime;

		if( client->state == CS_ZOMBIE && client->lastPacketReceivedTime + 1000 * tv_zombietime->value < tvs.realtime )
		{
			userinfo_modified = true;
			client->state = CS_FREE; // can now be reused
			if( client->individual_socket )
				NET_CloseSocket( &client->socket );
			continue;
		}

		if( ( client->state != CS_FREE && client->state != CS_ZOMBIE ) &&
			( client->lastPacketReceivedTime + 1000 * tv_timeout->value < tvs.realtime ) )
		{
			userinfo_modified = true;
			TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Upstream timed out" );
			client->state = CS_FREE; // don't bother with zombie state
			if( client->socket.open )
				NET_CloseSocket( &client->socket );
		}

		// timeout downloads left open
		if( ( client->state != CS_FREE && client->state != CS_ZOMBIE ) &&
			( client->download.name && client->download.timeout < tvs.realtime ) )
		{
			Com_Printf( "Download of %s to %s" S_COLOR_WHITE " timed out\n", client->download.name, client->name );

			if( client->download.data )
			{
				FS_FreeBaseFile( client->download.data );
				client->download.data = NULL;
			}

			Mem_ZoneFree( client->download.name );
			client->download.name = NULL;

			client->download.size = 0;
			client->download.timeout = 0;
		}
	}
}

/*
* TV_Downstream_SendClientsFragments
*/
bool TV_Downstream_SendClientsFragments( void )
{
	client_t *client;
	int i;
	bool remaining = false;

	// send a message to each connected client
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		if( client->state == CS_FREE || client->state == CS_ZOMBIE )
			continue;

		if( !client->netchan.unsentFragments )
			continue;

		if( !Netchan_TransmitNextFragment( &client->netchan ) )
		{
			Com_Printf( "%s" S_COLOR_WHITE ": Error sending fragment: %s\n", client->name, NET_ErrorString() );
			if( client->reliable )
			{
				TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error sending fragment: %s\n",
					NET_ErrorString() );
			}
			continue;
		}

		if( client->netchan.unsentFragments )
			remaining = true;
	}

	return remaining;
}

/*
* TV_Downstream_SendClientMessages
*/
void TV_Downstream_SendClientMessages( void )
{
	int i;
	client_t *client;
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];

	// send a message to each connected client
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		if( client->state == CS_FREE || client->state == CS_ZOMBIE )
			continue;

		if( client->state < CS_SPAWNED )
		{
			// send pending reliable commands, or send heartbeats for not timing out
			/*			if( client->reliableSequence > client->reliableSent ||
			(client->reliableSequence > client->reliableAcknowledge &&
			tvs.realtime - client->lastPacketSentTime > 50) ||
			tvs.realtime - client->lastPacketSentTime > 500 ) */
			if( client->reliableSequence > client->reliableAcknowledge ||
				tvs.realtime - client->lastPacketSentTime > 1000 )
			{
				TV_Downstream_InitClientMessage( client, &message, messageData, sizeof( messageData ) );

				TV_Downstream_AddReliableCommandsToMessage( client, &message );
				if( !TV_Downstream_SendMessageToClient( client, &message ) )
				{
					Com_Printf( "%s" S_COLOR_WHITE ": Error sending message: %s\n", client->name, NET_ErrorString() );
					if( client->reliable )
					{
						TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Error sending message: %s\n",
							NET_ErrorString() );
					}
				}
			}
		}
	}
}

/*
* TV_Downstream_FindNextUserCommand - Returns the next valid usercmd_t in execution list
*/
usercmd_t *TV_Downstream_FindNextUserCommand( client_t *client )
{
	usercmd_t *ucmd;
	unsigned int higherTime = 0xFFFFFFFF;
	unsigned int i;

	ucmd = NULL;
	if( client )
	{
		higherTime = client->relay ? client->relay->serverTime : tvs.realtime; // ucmds can never have a higher timestamp than server time, unless cheating

		for( i = client->UcmdExecuted + 1; i <= client->UcmdReceived; i++ )
		{
			// skip backups if already executed
			if( client->UcmdTime >= client->ucmds[i & CMD_MASK].serverTimeStamp )
				continue;

			if( client->ucmds[i & CMD_MASK].serverTimeStamp < higherTime )
			{
				higherTime = client->ucmds[i & CMD_MASK].serverTimeStamp;
				ucmd = &client->ucmds[i & CMD_MASK];
			}
		}
	}

	return ucmd;
}

/*
* TV_Downstream_ExecuteClientThinks
*/
void TV_Downstream_ExecuteClientThinks( relay_t *relay, client_t *client )
{
	unsigned int msec, higherTime;
	unsigned int minUcmdTime;
	int timeDelta;
	usercmd_t *ucmd;

	// don't let client command time delay too far away in the past
	higherTime = client->relay ? client->relay->serverTime : tvs.realtime;
	minUcmdTime = ( higherTime > 999 ) ? ( higherTime - 999 ) : 0;
	if( client->UcmdTime < minUcmdTime )
		client->UcmdTime = minUcmdTime;

	while( ( ucmd = TV_Downstream_FindNextUserCommand( client ) ) != NULL )
	{
		msec = ucmd->serverTimeStamp - client->UcmdTime;
		clamp( msec, 1, 200 );
		ucmd->msec = msec;
		timeDelta = 0;
		if( client->lastframe > 0 )
			timeDelta = -(int)( higherTime - ucmd->serverTimeStamp );

		relay->module_export->ClientThink( relay->module, client->edict, ucmd, timeDelta );

		client->UcmdTime = ucmd->serverTimeStamp;
	}

	// we did the entire update
	client->UcmdExecuted = client->UcmdReceived;
}


//==============================================================================
//
//MASTER SERVERS MANAGEMENT
//
//==============================================================================

typedef struct sv_master_s
{
	netadr_t address;
	bool steam;
} tv_master_t;

static tv_master_t tv_masters[MAX_MASTERS];    // address of group servers

/*
* TV_Downstream_AddMaster_f
* Add a master server to the list
*/
static void TV_Downstream_AddMaster_f( const char *address, bool steam )
{
	int i;

	if( !address || !address[0] )
		return;

	if( !tv_public->integer )
	{
		Com_Printf( "'TV_Downstream_AddMaster_f' Only public servers use masters.\n" );
		return;
	}

	for( i = 0; i < MAX_MASTERS; i++ )
	{
		tv_master_t *master = &tv_masters[i];

		if( master->address.type != NA_NOTRANSMIT )
			continue;

		if( !NET_StringToAddress( address, &master->address ) )
		{
			Com_Printf( "'TV_Downstream_AddMaster_f' Bad Master server address: %s\n", address );
			return;
		}
		if( NET_GetAddressPort( &master->address ) == 0 )
			NET_SetAddressPort( &master->address, steam ? PORT_MASTER_STEAM : PORT_MASTER );

		master->steam = steam;

		Com_Printf( "Added new master server #%i at %s\n", i, NET_AddressToString( &master->address ) );
		return;
	}

	Com_Printf( "'TV_Downstream_AddMaster_f' List of master servers is already full\n" );
}

/*
* TV_Downstream_InitMaster
* Set up the main master server
*/
void TV_Downstream_InitMaster( void )
{
	const char *master, *mlist;

	// wsw : jal : initialize masters list
	memset( tv_masters, 0, sizeof( tv_masters ) );

	if( !tv_public->integer )
		return;

	mlist = tv_masterservers->string;
	if( *mlist )
	{
		while( mlist )
		{
			master = COM_Parse( &mlist );
			if( !master[0] )
				break;

			TV_Downstream_AddMaster_f( master, false );
		}
	}

#if APP_STEAMID
	mlist = tv_masterservers_steam->string;
	if( *mlist )
	{
		while( mlist )
		{
			master = COM_Parse( &mlist );
			if( !master[0] )
				break;

			TV_Downstream_AddMaster_f( master, true );
		}
	}
#endif

	tvs.lobby.next_heartbeat = Sys_Milliseconds() + HEARTBEAT_SECONDS * 1000; // wait a while before sending first heartbeat
}

/*
* TV_Downstream_MasterHeartbeat
* Send a message to the master every few minutes to
* let it know we are alive, and log information
*/
void TV_Downstream_MasterHeartbeat( void )
{
	unsigned int time = Sys_Milliseconds();
	int i;
	const socket_t *socket;

	if( tvs.lobby.next_heartbeat > time )
		return;

	tvs.lobby.next_heartbeat = time + HEARTBEAT_SECONDS * 1000;

	if( !tv_public->integer )
		return;

	// send to group master
	for( i = 0; i < MAX_MASTERS; i++ )
	{
		tv_master_t *master = &tv_masters[i];

		if( master->address.type != NA_NOTRANSMIT )
		{
			Com_Printf( "Sending heartbeat to %s\n", NET_AddressToString( &master->address ) );

			socket = ( master->address.type == NA_IP6 ? &tvs.socket_udp6 : &tvs.socket_udp );

			if( master->steam )
			{
				uint8_t steamHeartbeat = 'q';
				NET_SendPacket( socket, &steamHeartbeat, sizeof( steamHeartbeat ), &master->address );
			}
			else
			{
				// warning: "DarkPlaces" is a protocol name here, not a game name. Do not replace it.
				Netchan_OutOfBandPrint( socket, &master->address, "heartbeat DarkPlaces\n" );
			}
		}
	}
}

/*
* TV_Downstream_MasterSendQuit
* Notifies Steam master servers that the server is shutting down.
*/
void TV_Downstream_MasterSendQuit( void )
{
	int i;
	const char quitMessage[] = "b\n";

	if( !tv_public->integer || ( tv_maxclients->integer == 1 ) )
		return;

	// send to group master
	for( i = 0; i < MAX_MASTERS; i++ )
	{
		tv_master_t *master = &tv_masters[i];

		if( master->steam && ( master->address.type != NA_NOTRANSMIT ) )
		{
			socket_t *socket = ( master->address.type == NA_IP6 ? &tvs.socket_udp6 : &tvs.socket_udp );
			Com_Printf( "Sending quit to %s\n", NET_AddressToString( &master->address ) );
			NET_SendPacket( socket, ( const uint8_t * )quitMessage, sizeof( quitMessage ), &master->address );
		}
	}
}

/*
* TV_Downstream_IsMaster
* Check whether the address belongs to a master servers.
* Also may return whether it's a Steam master server.
*/
bool TV_Downstream_IsMaster( const netadr_t *address, bool *isSteam )
{
	int i;
	for( i = 0; i < MAX_MASTERS; i++ )
	{
		if( NET_CompareAddress( address, &tv_masters[i].address ) )
		{
			if( isSteam )
				*isSteam = tv_masters[i].steam;
			return true;
		}
	}
	return false;
}
