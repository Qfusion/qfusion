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

#include "tv_downstream_oob.h"
#include "tv_upstream.h"
#include "tv_downstream.h"
#include "tv_lobby.h"
#include "tv_relay_client.h"

/*
* TV_Downstream_Ack
*/
static void TV_Downstream_Ack( const socket_t *socket, const netadr_t *address )
{
	Com_Printf( "Ping acknowledge from %s\n", NET_AddressToString( address ) );
}

/*
* TV_Downstream_Ping
* Just responds with an acknowledgement
*/
static void TV_Downstream_Ping( const socket_t *socket, const netadr_t *address )
{
	// send any arguments back with ack
	Netchan_OutOfBandPrint( socket, address, "ack %s", Cmd_Args() );
}

/*
* TV_Downstream_LongInfoString
* Builds the string that is sent as heartbeats and status replies
*/
static char *TV_Downstream_LongInfoString( qboolean fullStatus )
{
	char tempstr[1024] = { 0 };
	const char *p;
	static char status[MAX_MSGLEN - 16];
	int i, count;
	upstream_t *upstream;
	size_t statusLength;
	size_t tempstrLength;

	Q_strncpyz( status, Cvar_Serverinfo(), sizeof( status ) );
	p = Q_strlocate( status, "tv_maxclients", 0 );
	if( p ) // master server expects a sv_maxclients cvar, so hack the name in
		status[p - status] = 's';

	statusLength = strlen( status );

	count = 0;
	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		if( tvs.clients[i].state >= CS_CONNECTED )
			count++;
	}

	Q_snprintfz( tempstr, sizeof( tempstr ), "\\tv\\%i", 1 );
	Q_snprintfz( tempstr + strlen( tempstr ), sizeof( tempstr ) - strlen( tempstr ), "\\clients\\%i%s", count, fullStatus ? "\n" : "" );
	tempstrLength = strlen( tempstr );
	if( statusLength + tempstrLength >= sizeof( status ) )
		return status; // can't hold any more
	Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
	statusLength += tempstrLength;

	if ( fullStatus )
	{
		for( i = 0; i < tvs.numupstreams; i++ )
		{
			upstream = tvs.upstreams[i];
			if( upstream && upstream->relay.state > CA_CONNECTING )
			{
				relay_t *relay = &upstream->relay;

				if( relay->state >= CA_ACTIVE )
				{
					int numplayers;

					numplayers = TV_Relay_NumPlayers( relay );

					Q_snprintfz( tempstr, sizeof( tempstr ), "%i \"%s\" %i \"%s\" %i\n",
						i+1, upstream->name, relay->delay/1000, relay->levelname, numplayers );
					tempstrLength = strlen( tempstr );
					if( statusLength + tempstrLength >= sizeof( status ) )
						break; // can't hold any more
					Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
					statusLength += tempstrLength;
				}
			}
		}
	}

	return status;
}

/*
* SV_ShortInfoString
* Generates a short info string for broadcast scan replies
*/
#define MAX_STRING_SVCINFOSTRING 160
#define MAX_SVCINFOSTRING_LEN ( MAX_STRING_SVCINFOSTRING - 4 )
static char *TV_Downstream_ShortInfoString( void )
{
	static char string[MAX_STRING_SVCINFOSTRING];
	char hostname[64];
	char entry[20];
	size_t len;
	int i, count;
	/*
	int channels;
	upstream_t *upstream;
	*/

	count = 0;
	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		if( tvs.clients[i].state >= CS_CONNECTED )
			count++;
	}

	//format:
	//" \377\377\377\377info\\n\\server_name\\m\\map name\\u\\clients/maxclients\\EOT "

	Q_strncpyz( hostname, tv_name->string, sizeof( hostname ) );
	Q_snprintfz( string, sizeof( string ),
		"\\\\n\\\\%s\\\\m\\\\%8s\\\\u\\\\%2i/%2i\\\\",
		hostname,
		"lobby",
		count > 99 ? 99 : count,
		tv_maxclients->integer > 99 ? 99 : tv_maxclients->integer
		);

	len = strlen( string );

	Q_snprintfz( entry, sizeof( entry ), "tv\\\\%i\\\\", 1 );
	if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) )
	{
		Q_strncatz( string, entry, sizeof( string ) );
		len = strlen( string );
	}

	if( tv_password->string[0] )
	{
		Q_snprintfz( entry, sizeof( entry ), "p\\\\%i\\\\", 1 );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) )
		{
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}
	/*
	channels = 0;
	for( i = 0; i < tvs.numupstreams; i++ )
	{
	upstream = tvs.upstreams[i];
	if( upstream && upstream->relay.state > CA_CONNECTING )
	channels++;
	}

	if( channels )
	{
	Q_snprintfz( entry, sizeof( entry ), "c\\\\%2i\\\\", channels > 99 ? 99 : channels );
	if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) )
	{
	Q_strncatz( string, entry, sizeof( string ) );
	len = strlen( string );
	}
	}
	*/
	// finish it
	Q_strncatz( string, "EOT", sizeof( string ) );
	return string;
}

/*
* TV_Downstream_InfoResponse
* 
* Responds with short info for broadcast scans
* The second parameter should be the current protocol version number.
*/
static void TV_Downstream_InfoResponse( const socket_t *socket, const netadr_t *address )
{
	int i, count;
	char *string;
	qboolean allow_empty = qfalse, allow_full = qfalse;

	// KoFFiE: When not public and coming from a LAN address
	//         assume broadcast and respond anyway, otherwise ignore
	if( ( ( !tv_public->integer ) && ( !NET_IsLANAddress( address ) ) ) ||
		( tv_maxclients->integer == 1 ) )
		return;

	// different protocol version
	if( atoi( Cmd_Argv( 1 ) ) != APP_PROTOCOL_VERSION )
		return;

	// check for full/empty filtered states
	for( i = 0; i < Cmd_Argc(); i++ )
	{
		if( !Q_stricmp( Cmd_Argv( i ), "full" ) )
			allow_full = qtrue;
		if( !Q_stricmp( Cmd_Argv( i ), "empty" ) )
			allow_empty = qtrue;
	}

	count = 0;
	for( i = 0; i < tv_maxclients->integer; i++ )
		if( tvs.clients[i].state >= CS_CONNECTED )
			count++;

	if( ( count == tv_maxclients->integer ) && !allow_full )
		return;
	if( ( count == 0 ) && !allow_empty )
		return;

	string = TV_Downstream_ShortInfoString();
	if( string )
		Netchan_OutOfBandPrint( socket, address, "info\n%s", string );
}

/*
* TV_Downstream_SendInfoString
*/
static void TV_Downstream_SendInfoString( const socket_t *socket, const netadr_t *address, const char *responseType, qboolean fullStatus )
{
	char *string;

	// KoFFiE: When not public and coming from a LAN address
	//         assume broadcast and respond anyway, otherwise ignore
	if( ( ( !tv_public->integer ) && ( !NET_IsLANAddress( address ) ) ) ||
		( tv_maxclients->integer == 1 ) )
		return;

	// send the same string that we would give for a status OOB command
	string = TV_Downstream_LongInfoString( fullStatus );
	if( string )
		Netchan_OutOfBandPrint( socket, address, "%s\n\\challenge\\%s%s", responseType, Cmd_Argv( 1 ), string );
}

/*
* TV_Downstream_GetInfoResponse
*/
static void TV_Downstream_GetInfoResponse( const socket_t *socket, const netadr_t *address )
{
	TV_Downstream_SendInfoString( socket, address, "infoResponse", qfalse );
}

/*
* TV_Downstream_GetStatusResponse
*/
static void TV_Downstream_GetStatusResponse( const socket_t *socket, const netadr_t *address )
{
	TV_Downstream_SendInfoString( socket, address, "statusResponse", qtrue );
}

/*
* TV_Downstream_ClientConnect
*/
static qboolean TV_Downstream_ClientConnect( const socket_t *socket, const netadr_t *address, client_t *client,
											char *userinfo, int game_port, int challenge, qboolean tv_client )
{
	assert( socket );
	assert( address );
	assert( client );
	assert( userinfo );

	// it may actually happen that we reuse a client slot (same IP, same port),
	// which is "attached" to an active relay, so we need to notify the relay that client
	// isn't active anymore, otherwise it'll get confused after we set the client's state 
	// to CS_CONNECTING down below
	if( client->relay )
		TV_Relay_ClientDisconnect( client->relay, client );

	if( !TV_Lobby_CanConnect( client, userinfo ) )
		return qfalse;

	TV_Lobby_ClientConnect( client );

	// the upstream is accepted, set up the client slot
	client->challenge = challenge; // save challenge for checksumming
	client->tv = (tv_client ? qtrue : qfalse);

	switch( socket->type )
	{
#ifdef TCP_ALLOW_CONNECT
	case SOCKET_TCP:
		client->reliable = qtrue;
		client->individual_socket = qtrue;
		client->socket = *socket;
		break;
#endif

	case SOCKET_UDP:
	case SOCKET_LOOPBACK:
		client->reliable = qfalse;
		client->individual_socket = qfalse;
		client->socket.open = qfalse;
		break;

	default:
		assert( qfalse );
	}

	TV_Downstream_ClientResetCommandBuffers( client, qtrue );

	// reset timeouts
	client->lastPacketReceivedTime = tvs.realtime;
	client->lastconnect = tvs.realtime;

	// init the upstream
	client->state = CS_CONNECTING;

	if( client->individual_socket )
		Netchan_Setup( &client->netchan, &client->socket, address, game_port );
	else
		Netchan_Setup( &client->netchan, socket, address, game_port );

	// parse some info from the info strings
	Q_strncpyz( client->userinfo, userinfo, sizeof( client->userinfo ) );
	TV_Downstream_UserinfoChanged( client );

	Com_Printf( "%s" S_COLOR_WHITE " connected\n", client->name );

	return qtrue;
}

/*
* TV_Downstream_DirectConnect
* A upstream request that did not come from the master
*/
static void TV_Downstream_DirectConnect( const socket_t *socket, const netadr_t *address )
{
#ifdef TCP_ALLOW_CONNECT
	int incoming = 0;
#endif
	char userinfo[MAX_INFO_STRING], *name;
	client_t *cl, *newcl;
	int i, version, game_port, challenge;
	qboolean tv_client;

	version = atoi( Cmd_Argv( 1 ) );
	if( version != APP_PROTOCOL_VERSION )
	{
		if( version <= 6 )
		{            // before reject packet was added
			Netchan_OutOfBandPrint( socket, address, "print\nServer is version %4.2f. Protocol %3i\n",
				APP_VERSION, APP_PROTOCOL_VERSION );
		}
		else
		{
			Netchan_OutOfBandPrint( socket, address,
				"reject\n%i\n%i\nServer and client don't have the same version\n", DROP_TYPE_GENERAL, 0 );
		}
		return;
	}

	game_port = atoi( Cmd_Argv( 2 ) );
	challenge = atoi( Cmd_Argv( 3 ) );
	tv_client = ( atoi( Cmd_Argv( 5 ) ) & 1 ? qtrue : qfalse );

	if( !Info_Validate( Cmd_Argv( 4 ) ) )
	{
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nInvalid userinfo string\n", DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Upstream from %s refused: invalid userinfo string\n", NET_AddressToString( address ) );
		return;
	}

	Q_strncpyz( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) );

	// force the IP key/value pair so the game can filter based on ip
	if( !Info_SetValueForKey( userinfo, "socket", NET_SocketTypeToString( socket->type ) ) )
	{
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nError: Couldn't set userinfo (socket)\n",
			DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Upstream from %s refused: couldn't set userinfo (socket)\n", NET_AddressToString( address ) );
		return;
	}
	if( !Info_SetValueForKey( userinfo, "ip", NET_AddressToString( address ) ) )
	{
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nError: Couldn't set userinfo (ip)\n",
			DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Upstream from %s refused: couldn't set userinfo (ip)\n", NET_AddressToString( address ) );
		return;
	}

	// we handle name ourselves here, since tv module doesn't know about all the players
	name = TV_Downstream_FixName( Info_ValueForKey( userinfo, "name" ), NULL );
	if( !Info_SetValueForKey( userinfo, "name", name ) )
	{
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nError: Couldn't set userinfo (name)\n",
			DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Upstream from %s refused: couldn't set userinfo (name)\n", NET_AddressToString( address ) );
		return;
	}

#ifdef TCP_ALLOW_CONNECT
	if( socket->type == SOCKET_TCP )
	{
		// find the upstream
		for( i = 0; i < MAX_INCOMING_CONNECTIONS; i++ )
		{
			if( !tvs.incoming[i].active )
				continue;

			if( NET_CompareAddress( &tvs.incoming[i].address, address ) && socket == &tvs.incoming[i].socket )
				break;
		}
		if( i == MAX_INCOMING_CONNECTIONS )
		{
			Com_Error( ERR_FATAL, "Incoming upstream not found.\n" );
			return;
		}
		incoming = i;
	}
#endif

	// see if the challenge is valid
	for( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if( NET_CompareBaseAddress( address, &tvs.challenges[i].adr ) )
		{
			if( challenge == tvs.challenges[i].challenge )
			{
				tvs.challenges[i].challenge = 0; // wsw : r1q2 : reset challenge
				tvs.challenges[i].time = 0;
				NET_InitAddress( &tvs.challenges[i].adr, NA_NOTRANSMIT );
				break; // good
			}
			Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nBad challenge\n",
				DROP_TYPE_GENERAL, DROP_FLAG_AUTORECONNECT );
			return;
		}
	}
	if( i == MAX_CHALLENGES )
	{
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nNo challenge for address\n",
			DROP_TYPE_GENERAL, DROP_FLAG_AUTORECONNECT );
		return;
	}

	newcl = NULL;

	// if there is already a slot for this ip, reuse it
	for( i = 0, cl = tvs.clients; i < tv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == CS_FREE )
			continue;

		if( NET_CompareAddress( address, &cl->netchan.remoteAddress ) ||
			( NET_CompareBaseAddress( address, &cl->netchan.remoteAddress ) && cl->netchan.game_port == game_port ) )
		{
			if( !NET_IsLocalAddress( address ) &&
				( tvs.realtime - cl->lastconnect ) < (unsigned)( tv_reconnectlimit->integer * 1000 ) )
			{
				return;
			}
			newcl = cl;
			break;
		}
	}

	// find a client slot
	if( !newcl )
	{
		for( i = 0, cl = tvs.clients; i < tv_maxclients->integer; i++, cl++ )
		{
			if( cl->state == CS_FREE )
			{
				newcl = cl;
				break;
			}
		}
		if( !newcl )
		{
			Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nServer is full\n", DROP_TYPE_GENERAL,
				DROP_FLAG_AUTORECONNECT );
			return;
		}
	}

	// get the game a chance to reject this upstream or modify the userinfo
	if( !TV_Downstream_ClientConnect( socket, address, newcl, userinfo, game_port, challenge, tv_client ) )
	{
		char *rejtypeflag, *rejmsg;

		// hax because Info_ValueForKey can only be called twice in a row
		rejtypeflag = va( "%s\n%s", Info_ValueForKey( userinfo, "rejtype" ), Info_ValueForKey( userinfo, "rejflag" ) );
		rejmsg = Info_ValueForKey( userinfo, "rejmsg" );

		Netchan_OutOfBandPrint( socket, address, "reject\n%s\n%s\n", rejtypeflag, rejmsg );
		return;
	}

	// send the connect packet to the client
	Netchan_OutOfBandPrint( socket, address, "client_connect" );

	// free the incoming entry
#ifdef TCP_ALLOW_CONNECT
	if( socket->type == SOCKET_TCP )
	{
		tvs.incoming[incoming].active = qfalse;
		tvs.incoming[incoming].socket.open = qfalse;
	}
#endif
}

/*
* TV_Downstream_GetChallenge
* 
* Returns a challenge number that can be used
* in a subsequent client_connect command.
* We do this to prevent denial of service attacks that
* flood the server with invalid upstream IPs.  With a
* challenge, they must give a valid IP address.
*/
static void TV_Downstream_GetChallenge( const socket_t *socket, const netadr_t *address )
{
	int i;
	int oldest;
	int oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if( NET_CompareBaseAddress( address, &tvs.challenges[i].adr ) )
			break;
		if( tvs.challenges[i].time < oldestTime )
		{
			oldestTime = tvs.challenges[i].time;
			oldest = i;
		}
	}

	if( i == MAX_CHALLENGES )
	{
		// overwrite the oldest
		tvs.challenges[oldest].challenge = rand() & 0x7fff;
		tvs.challenges[oldest].adr = *address;
		tvs.challenges[oldest].time = tvs.realtime;
		i = oldest;
	}

	Netchan_OutOfBandPrint( socket, address, "challenge %i", tvs.challenges[i].challenge );
}

/*
* Rcon_Validate
*/
static int Rcon_Validate( void )
{
	if( !strlen( tv_rcon_password->string ) )
		return 0;

	if( strcmp( Cmd_Argv( 1 ), tv_rcon_password->string ) )
		return 0;

	return 1;
}

/*
* TV_Downstream_RemoteCommand
* 
* A client issued an rcon command.
* Shift down the remaining args
* Redirect all printfs
*/
static void TV_Downstream_RemoteCommand( const socket_t *socket, const netadr_t *address )
{
	int i;
	char remaining[1024];
	flush_params_t extra;

	i = Rcon_Validate();

	if( i == 0 )
		Com_Printf( "Bad rcon from %s:\n%s\n", NET_AddressToString( address ), Cmd_Args() );
	else
		Com_Printf( "Rcon from %s:\n%s\n", NET_AddressToString( address ), Cmd_Args() );

	extra.socket = socket;
	extra.address = address;
	Com_BeginRedirect( RD_PACKET, tv_outputbuf, TV_OUTPUTBUF_LENGTH, TV_FlushRedirect, ( const void * )&extra );

	if( !Rcon_Validate() )
	{
		Com_Printf( "Bad rcon_password.\n" );
	}
	else
	{
		remaining[0] = 0;

		for( i = 2; i < Cmd_Argc(); i++ )
		{
			Q_strncatz( remaining, "\"", sizeof( remaining ) );
			Q_strncatz( remaining, Cmd_Argv( i ), sizeof( remaining ) );
			Q_strncatz( remaining, "\" ", sizeof( remaining ) );
		}

		Cmd_ExecuteString( remaining );
	}

	Com_EndRedirect();
}

// ---

typedef struct
{
	char *name;
	void ( *func )( const socket_t *socket, const netadr_t *address );
} upstreamless_cmd_t;

static upstreamless_cmd_t upstreamless_cmds[] =
{
	{ "ping", TV_Downstream_Ping },
	{ "ack", TV_Downstream_Ack },
	{ "info", TV_Downstream_InfoResponse },
	{ "getinfo", TV_Downstream_GetInfoResponse },
	{ "getstatus", TV_Downstream_GetStatusResponse },
	{ "getchallenge", TV_Downstream_GetChallenge },
	{ "connect", TV_Downstream_DirectConnect },
	{ "rcon", TV_Downstream_RemoteCommand },

	{ NULL, NULL }
};

/*
* TV_Downstream_UpstreamlessPacket
*/
void TV_Downstream_UpstreamlessPacket( const socket_t *socket, const netadr_t *address, msg_t *msg )
{
	upstreamless_cmd_t *cmd;
	char *s, *c;

	MSG_BeginReading( msg );
	MSG_ReadLong( msg );    // skip the -1 marker

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );
	c = Cmd_Argv( 0 );

	for( cmd = upstreamless_cmds; cmd->name; cmd++ )
	{
		if( !strcmp( c, cmd->name ) )
		{
			cmd->func( socket, address );
			return;
		}
	}

	Com_DPrintf( "Bad downstream connectionless packet from %s:\n%s\n", NET_AddressToString( address ), s );
}
