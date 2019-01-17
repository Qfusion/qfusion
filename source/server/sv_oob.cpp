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
#include "qcommon/version.h"

typedef struct sv_master_s {
	netadr_t address;
} sv_master_t;

static sv_master_t sv_masters[MAX_MASTERS];

extern cvar_t *sv_masterservers;
extern cvar_t *sv_hostname;
extern cvar_t *sv_reconnectlimit;     // minimum seconds between connect messages
extern cvar_t *rcon_password;         // password for remote server commands
extern cvar_t *sv_iplimit;


//==============================================================================
//
//MASTER SERVERS MANAGEMENT
//
//==============================================================================


/*
* SV_AddMaster_f
* Add a master server to the list
*/
static void SV_AddMaster_f( char *address ) {
	int i;

	if( !address || !address[0] ) {
		return;
	}

	if( !sv_public->integer ) {
		Com_Printf( "'SV_AddMaster_f' Only public servers use masters.\n" );
		return;
	}

	//never go public when not acting as a game server
	if( sv.state > ss_game ) {
		return;
	}

	for( i = 0; i < MAX_MASTERS; i++ ) {
		sv_master_t *master = &sv_masters[i];

		if( master->address.type != NA_NOTRANSMIT ) {
			continue;
		}

		if( !NET_StringToAddress( address, &master->address ) ) {
			Com_Printf( "'SV_AddMaster_f' Bad Master server address: %s\n", address );
			return;
		}

		if( NET_GetAddressPort( &master->address ) == 0 ) {
			NET_SetAddressPort( &master->address, PORT_MASTER );
		}

		Com_Printf( "Added new master server #%i at %s\n", i, NET_AddressToString( &master->address ) );
		return;
	}

	Com_Printf( "'SV_AddMaster_f' List of master servers is already full\n" );
}

/*
* SV_ResolveMaster
*/
static void SV_ResolveMaster( void ) {
	char *master, *mlist;

	// wsw : jal : initialize masters list
	memset( sv_masters, 0, sizeof( sv_masters ) );

	//never go public when not acting as a game server
	if( sv.state > ss_game ) {
		return;
	}

	if( !sv_public->integer ) {
		return;
	}

	mlist = sv_masterservers->string;
	if( *mlist ) {
		while( mlist ) {
			master = COM_Parse( &mlist );
			if( !master[0] ) {
				break;
			}

			SV_AddMaster_f( master );
		}
	}

	svc.lastMasterResolve = Sys_Milliseconds();
}

/*
* SV_InitMaster
* Set up the main master server
*/
void SV_InitMaster( void ) {
	SV_ResolveMaster();

	svc.nextHeartbeat = Sys_Milliseconds();
}

/*
* SV_UpdateMaster
*/
void SV_UpdateMaster( void ) {
	// refresh master server IP addresses periodically
	if( svc.lastMasterResolve + TTL_MASTERS < Sys_Milliseconds() ) {
		SV_ResolveMaster();
	}
}

/*
* SV_MasterHeartbeat
* Send a message to the master every few minutes to
* let it know we are alive, and log information
*/
void SV_MasterHeartbeat( void ) {
	int64_t time = Sys_Milliseconds();
	int i;

	if( svc.nextHeartbeat > time ) {
		return;
	}

	svc.nextHeartbeat = time + HEARTBEAT_SECONDS * 1000;

	if( !sv_public->integer || ( sv_maxclients->integer == 1 ) ) {
		return;
	}

	// never go public when not acting as a game server
	if( sv.state > ss_game ) {
		return;
	}

	// send to group master
	for( i = 0; i < MAX_MASTERS; i++ ) {
		sv_master_t *master = &sv_masters[i];

		if( master->address.type != NA_NOTRANSMIT ) {
			socket_t *socket;

			Com_Printf( "Sending heartbeat to %s\n", NET_AddressToString( &master->address ) );

			socket = ( master->address.type == NA_IP6 ? &svs.socket_udp6 : &svs.socket_udp );

			// warning: "DarkPlaces" is a protocol name here, not a game name. Do not replace it.
			Netchan_OutOfBandPrint( socket, &master->address, "heartbeat DarkPlaces\n" );
		}
	}
}

//============================================================================

/*
* SV_LongInfoString
* Builds the string that is sent as heartbeats and status replies
*/
static char *SV_LongInfoString( bool fullStatus ) {
	char tempstr[1024] = { 0 };
	const char *gametype;
	static char status[MAX_MSGLEN - 16];
	int i, bots, count;
	client_t *cl;
	size_t statusLength;
	size_t tempstrLength;

	Q_strncpyz( status, Cvar_Serverinfo(), sizeof( status ) );

	// convert "g_gametype" to "gametype"
	gametype = Info_ValueForKey( status, "g_gametype" );
	if( gametype ) {
		Info_RemoveKey( status, "g_gametype" );
		Info_SetValueForKey( status, "gametype", gametype );
	}

	statusLength = strlen( status );

	bots = 0;
	count = 0;
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if( cl->state >= CS_CONNECTED ) {
			if( cl->edict->r.svflags & SVF_FAKECLIENT ) {
				bots++;
			}
			count++;
		}
	}

	if( bots ) {
		Q_snprintfz( tempstr, sizeof( tempstr ), "\\bots\\%i", bots );
	}
	Q_snprintfz( tempstr + strlen( tempstr ), sizeof( tempstr ) - strlen( tempstr ), "\\clients\\%i%s", count, fullStatus ? "\n" : "" );
	tempstrLength = strlen( tempstr );
	if( statusLength + tempstrLength >= sizeof( status ) ) {
		return status; // can't hold any more
	}
	Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
	statusLength += tempstrLength;

	if( fullStatus ) {
		for( i = 0; i < sv_maxclients->integer; i++ ) {
			cl = &svs.clients[i];
			if( cl->state >= CS_CONNECTED ) {
				Q_snprintfz( tempstr, sizeof( tempstr ), "%i %i \"%s\" %i\n",
							 cl->edict->r.client->r.frags, cl->ping, cl->name, cl->edict->s.team );
				tempstrLength = strlen( tempstr );
				if( statusLength + tempstrLength >= sizeof( status ) ) {
					break; // can't hold any more
				}
				Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
				statusLength += tempstrLength;
			}
		}
	}

	return status;
}

/*
* SV_ShortInfoString
* Generates a short info string for broadcast scan replies
*/
#define MAX_STRING_SVCINFOSTRING 180
#define MAX_SVCINFOSTRING_LEN ( MAX_STRING_SVCINFOSTRING - 4 )
static char *SV_ShortInfoString( void ) {
	static char string[MAX_STRING_SVCINFOSTRING];
	char hostname[64];
	char entry[20];
	size_t len;
	int i, count, bots;
	int maxcount;
	const char *password;

	bots = 0;
	count = 0;
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_CONNECTED ) {
			if( svs.clients[i].edict->r.svflags & SVF_FAKECLIENT ) {
				bots++;
			} else {
				count++;
			}
		}
	}
	maxcount = sv_maxclients->integer - bots;

	//format:
	//" \377\377\377\377info\\n\\server_name\\m\\map name\\u\\clients/maxclients\\g\\gametype\\s\\skill\\EOT "

	Q_strncpyz( hostname, sv_hostname->string, sizeof( hostname ) );
	Q_snprintfz( string, sizeof( string ),
				 "\\\\n\\\\%s\\\\m\\\\%8s\\\\u\\\\%2i/%2i\\\\",
				 hostname,
				 sv.mapname,
				 count > 99 ? 99 : count,
				 maxcount > 99 ? 99 : maxcount
				 );

	len = strlen( string );
	Q_snprintfz( entry, sizeof( entry ), "g\\\\%6s\\\\", Cvar_String( "g_gametype" ) );
	if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
		Q_strncatz( string, entry, sizeof( string ) );
		len = strlen( string );
	}

	if( Q_stricmp( FS_GameDirectory(), FS_BaseGameDirectory() ) ) {
		Q_snprintfz( entry, sizeof( entry ), "mo\\\\%8s\\\\", FS_GameDirectory() );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	password = Cvar_String( "password" );
	if( password[0] != '\0' ) {
		Q_snprintfz( entry, sizeof( entry ), "p\\\\1\\\\" );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	if( bots ) {
		Q_snprintfz( entry, sizeof( entry ), "b\\\\%2i\\\\", bots > 99 ? 99 : bots );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	if( Cvar_Value( "g_race_gametype" ) ) {
		Q_snprintfz( entry, sizeof( entry ), "r\\\\1\\\\" );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	// finish it
	Q_strncatz( string, "EOT", sizeof( string ) );
	return string;
}



//==============================================================================
//
//OUT OF BAND COMMANDS
//
//==============================================================================


/*
* SVC_Ack
*/
static void SVC_Ack( const socket_t *socket, const netadr_t *address ) {
	Com_Printf( "Ping acknowledge from %s\n", NET_AddressToString( address ) );
}

/*
* SVC_Ping
* Just responds with an acknowledgement
*/
static void SVC_Ping( const socket_t *socket, const netadr_t *address ) {
	// send any arguments back with ack
	Netchan_OutOfBandPrint( socket, address, "ack %s", Cmd_Args() );
}

/*
* SVC_InfoResponse
*
* Responds with short info for broadcast scans
* The second parameter should be the current protocol version number.
*/
static void SVC_InfoResponse( const socket_t *socket, const netadr_t *address ) {
	int i, count;
	char *string;
	bool allow_empty = false, allow_full = false;

	if( sv_showInfoQueries->integer ) {
		Com_Printf( "Info Packet %s\n", NET_AddressToString( address ) );
	}

	// KoFFiE: When not public and coming from a LAN address
	//         assume broadcast and respond anyway, otherwise ignore
	if( ( ( !sv_public->integer ) && ( !NET_IsLANAddress( address ) ) ) ||
		( sv_maxclients->integer == 1 ) ) {
		return;
	}

	// ignore when in invalid server state
	if( sv.state < ss_loading || sv.state > ss_game ) {
		return;
	}

	// different protocol version
	if( atoi( Cmd_Argv( 1 ) ) != APP_PROTOCOL_VERSION ) {
		return;
	}

	// check for full/empty filtered states
	for( i = 0; i < Cmd_Argc(); i++ ) {
		if( !Q_stricmp( Cmd_Argv( i ), "full" ) ) {
			allow_full = true;
		}

		if( !Q_stricmp( Cmd_Argv( i ), "empty" ) ) {
			allow_empty = true;
		}
	}

	count = 0;
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
		}
	}

	if( ( count == sv_maxclients->integer ) && !allow_full ) {
		return;
	}

	if( ( count == 0 ) && !allow_empty ) {
		return;
	}

	string = SV_ShortInfoString();
	if( string ) {
		Netchan_OutOfBandPrint( socket, address, "info\n%s", string );
	}
}

/*
* SVC_SendInfoString
*/
static void SVC_SendInfoString( const socket_t *socket, const netadr_t *address, const char *requestType, const char *responseType, bool fullStatus ) {
	char *string;

	if( sv_showInfoQueries->integer ) {
		Com_Printf( "%s Packet %s\n", requestType, NET_AddressToString( address ) );
	}

	// KoFFiE: When not public and coming from a LAN address
	//         assume broadcast and respond anyway, otherwise ignore
	if( ( ( !sv_public->integer ) && ( !NET_IsLANAddress( address ) ) ) ||
		( sv_maxclients->integer == 1 ) ) {
		return;
	}

	// ignore when in invalid server state
	if( sv.state < ss_loading || sv.state > ss_game ) {
		return;
	}

	// send the same string that we would give for a status OOB command
	string = SV_LongInfoString( fullStatus );
	if( string ) {
		Netchan_OutOfBandPrint( socket, address, "%s\n\\challenge\\%s%s", responseType, Cmd_Argv( 1 ), string );
	}
}

/*
* SVC_GetInfoResponse
*/
static void SVC_GetInfoResponse( const socket_t *socket, const netadr_t *address ) {
	SVC_SendInfoString( socket, address, "GetInfo", "infoResponse", false );
}

/*
* SVC_GetStatusResponse
*/
static void SVC_GetStatusResponse( const socket_t *socket, const netadr_t *address ) {
	SVC_SendInfoString( socket, address, "GetStatus", "statusResponse", true );
}


/*
* SVC_GetChallenge
*
* Returns a challenge number that can be used
* in a subsequent client_connect command.
* We do this to prevent denial of service attacks that
* flood the server with invalid connection IPs.  With a
* challenge, they must give a valid IP address.
*/
static void SVC_GetChallenge( const socket_t *socket, const netadr_t *address ) {
	int i;
	int oldest;
	int oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	if( sv_showChallenge->integer ) {
		Com_Printf( "Challenge Packet %s\n", NET_AddressToString( address ) );
	}

	// see if we already have a challenge for this ip
	for( i = 0; i < MAX_CHALLENGES; i++ ) {
		if( NET_CompareBaseAddress( address, &svs.challenges[i].adr ) ) {
			break;
		}
		if( svs.challenges[i].time < oldestTime ) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if( i == MAX_CHALLENGES ) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].adr = *address;
		svs.challenges[oldest].time = Sys_Milliseconds();
		i = oldest;
	}

	Netchan_OutOfBandPrint( socket, address, "challenge %i", svs.challenges[i].challenge );
}


/*
* SVC_DirectConnect
* A connection request that did not come from the master
*/
static void SVC_DirectConnect( const socket_t *socket, const netadr_t *address ) {
	char userinfo[MAX_INFO_STRING];
	client_t *cl, *newcl;
	int i, version, game_port, challenge;
	int previousclients;
	int64_t time;

	Com_DPrintf( "SVC_DirectConnect (%s)\n", Cmd_Args() );

	version = atoi( Cmd_Argv( 1 ) );
	if( version != APP_PROTOCOL_VERSION ) {
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nServer and client don't have the same version\n", DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "    rejected connect from protocol %i\n", version );
		return;
	}

	game_port = atoi( Cmd_Argv( 2 ) );
	challenge = atoi( Cmd_Argv( 3 ) );

	if( !Info_Validate( Cmd_Argv( 4 ) ) ) {
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nInvalid userinfo string\n", DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Connection from %s refused: invalid userinfo string\n", NET_AddressToString( address ) );
		return;
	}

	Q_strncpyz( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) );

	// force the IP key/value pair so the game can filter based on ip
	if( !Info_SetValueForKey( userinfo, "socket", NET_SocketTypeToString( socket->type ) ) ) {
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nError: Couldn't set userinfo (socket)\n",
								DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Connection from %s refused: couldn't set userinfo (socket)\n", NET_AddressToString( address ) );
		return;
	}
	if( !Info_SetValueForKey( userinfo, "ip", NET_AddressToString( address ) ) ) {
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nError: Couldn't set userinfo (ip)\n",
								DROP_TYPE_GENERAL, 0 );
		Com_DPrintf( "Connection from %s refused: couldn't set userinfo (ip)\n", NET_AddressToString( address ) );
		return;
	}

	// see if the challenge is valid
	for( i = 0; i < MAX_CHALLENGES; i++ ) {
		if( NET_CompareBaseAddress( address, &svs.challenges[i].adr ) ) {
			if( challenge == svs.challenges[i].challenge ) {
				svs.challenges[i].challenge = 0; // wsw : r1q2 : reset challenge
				svs.challenges[i].time = 0;
				NET_InitAddress( &svs.challenges[i].adr, NA_NOTRANSMIT );
				break; // good
			}
			Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nBad challenge\n",
									DROP_TYPE_GENERAL, DROP_FLAG_AUTORECONNECT );
			return;
		}
	}
	if( i == MAX_CHALLENGES ) {
		Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nNo challenge for address\n",
								DROP_TYPE_GENERAL, DROP_FLAG_AUTORECONNECT );
		return;
	}

	//r1: limit connections from a single IP
	if( sv_iplimit->integer ) {
		previousclients = 0;
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
			if( cl->state == CS_FREE ) {
				continue;
			}
			if( NET_CompareBaseAddress( address, &cl->netchan.remoteAddress ) ) {
				//r1: zombies are less dangerous
				if( cl->state == CS_ZOMBIE ) {
					previousclients++;
				} else {
					previousclients += 2;
				}
			}
		}

		if( previousclients >= sv_iplimit->integer * 2 ) {
			Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nToo many connections from your host\n", DROP_TYPE_GENERAL,
									DROP_FLAG_AUTORECONNECT );
			Com_DPrintf( "%s:connect rejected : too many connections\n", NET_AddressToString( address ) );
			return;
		}
	}

	newcl = NULL;

	// if there is already a slot for this ip, reuse it
	time = Sys_Milliseconds();
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		if( cl->state == CS_FREE ) {
			continue;
		}
		if( NET_CompareAddress( address, &cl->netchan.remoteAddress ) ||
			( NET_CompareBaseAddress( address, &cl->netchan.remoteAddress ) && cl->netchan.game_port == game_port ) ) {
			if( !NET_IsLocalAddress( address ) &&
				( time - cl->lastconnect ) < (unsigned)( sv_reconnectlimit->integer * 1000 ) ) {
				Com_DPrintf( "%s:reconnect rejected : too soon\n", NET_AddressToString( address ) );
				return;
			}
			Com_Printf( "%s:reconnect\n", NET_AddressToString( address ) );
			newcl = cl;
			break;
		}
	}

	// find a client slot
	if( !newcl ) {
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
			if( cl->state == CS_FREE ) {
				newcl = cl;
				break;
			}
			// overwrite fakeclient if no free spots found
			if( cl->state && cl->edict && ( cl->edict->r.svflags & SVF_FAKECLIENT ) ) {
				newcl = cl;
			}
		}
		if( !newcl ) {
			Netchan_OutOfBandPrint( socket, address, "reject\n%i\n%i\nServer is full\n", DROP_TYPE_GENERAL,
									DROP_FLAG_AUTORECONNECT );
			Com_DPrintf( "Server is full. Rejected a connection.\n" );
			return;
		}
		if( newcl->state && newcl->edict && ( newcl->edict->r.svflags & SVF_FAKECLIENT ) ) {
			SV_DropClient( newcl, DROP_TYPE_GENERAL, "%s", "Need room for a real player" );
		}
	}

	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( socket, address, newcl, userinfo, game_port, challenge, false ) ) {
		const char *rejtype, *rejflag, *rejtypeflag, *rejmsg;

		rejtype = Info_ValueForKey( userinfo, "rejtype" );
		if( !rejtype ) {
			rejtype = "0";
		}
		rejflag = Info_ValueForKey( userinfo, "rejflag" );
		if( !rejflag ) {
			rejflag = "0";
		}
		// hax because Info_ValueForKey can only be called twice in a row
		rejtypeflag = va( "%s\n%s", rejtype, rejflag );

		rejmsg = Info_ValueForKey( userinfo, "rejmsg" );
		if( !rejmsg ) {
			rejmsg = "Game module rejected connection";
		}

		Netchan_OutOfBandPrint( socket, address, "reject\n%s\n%s\n", rejtypeflag, rejmsg );

		Com_DPrintf( "Game rejected a connection.\n" );
		return;
	}

	// send the connect packet to the client
	Netchan_OutOfBandPrint( socket, address, "client_connect\n%s", newcl->session );
}

/*
* SVC_FakeConnect
* (Not a real out of band command)
* A connection request that came from the game module
*/
int SVC_FakeConnect( const char *fakeUserinfo, const char *fakeSocketType, const char *fakeIP ) {
	int i;
	char userinfo[MAX_INFO_STRING];
	client_t *cl, *newcl;
	netadr_t address;

	Com_DPrintf( "SVC_FakeConnect ()\n" );

	if( !fakeUserinfo ) {
		fakeUserinfo = "";
	}
	if( !fakeIP ) {
		fakeIP = "127.0.0.1";
	}
	if( !fakeSocketType ) {
		fakeIP = "loopback";
	}

	Q_strncpyz( userinfo, fakeUserinfo, sizeof( userinfo ) );

	// force the IP key/value pair so the game can filter based on ip
	if( !Info_SetValueForKey( userinfo, "socket", fakeSocketType ) ) {
		return -1;
	}
	if( !Info_SetValueForKey( userinfo, "ip", fakeIP ) ) {
		return -1;
	}

	// find a client slot
	newcl = NULL;
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		if( cl->state == CS_FREE ) {
			newcl = cl;
			break;
		}
	}
	if( !newcl ) {
		Com_DPrintf( "Rejected a connection.\n" );
		return -1;
	}

	NET_InitAddress( &address, NA_NOTRANSMIT );
	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( NULL, &address, newcl, userinfo, -1, -1, true ) ) {
		Com_DPrintf( "Game rejected a connection.\n" );
		return -1;
	}

	// directly call the game begin function
	newcl->state = CS_SPAWNED;
	ge->ClientBegin( newcl->edict );

	return NUM_FOR_EDICT( newcl->edict );
}

/*
* Rcon_Validate
*/
static int Rcon_Validate( void ) {
	if( !strlen( rcon_password->string ) ) {
		return 0;
	}

	if( strcmp( Cmd_Argv( 1 ), rcon_password->string ) ) {
		return 0;
	}

	return 1;
}

/*
* SVC_RemoteCommand
*
* A client issued an rcon command.
* Shift down the remaining args
* Redirect all printfs
*/
static void SVC_RemoteCommand( const socket_t *socket, const netadr_t *address ) {
	int i;
	char remaining[1024];
	flush_params_t extra;

	i = Rcon_Validate();

	if( i == 0 ) {
		Com_Printf( "Bad rcon from %s:\n%s\n", NET_AddressToString( address ), Cmd_Args() );
	} else {
		Com_Printf( "Rcon from %s:\n%s\n", NET_AddressToString( address ), Cmd_Args() );
	}

	extra.socket = socket;
	extra.address = address;
	Com_BeginRedirect( RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect, ( const void * )&extra );

	if( sv_showRcon->integer ) {
		Com_Printf( "Rcon Packet %s\n", NET_AddressToString( address ) );
	}

	if( !Rcon_Validate() ) {
		Com_Printf( "Bad rcon_password.\n" );
	} else {
		remaining[0] = 0;

		for( i = 2; i < Cmd_Argc(); i++ ) {
			Q_strncatz( remaining, "\"", sizeof( remaining ) );
			Q_strncatz( remaining, Cmd_Argv( i ), sizeof( remaining ) );
			Q_strncatz( remaining, "\" ", sizeof( remaining ) );
		}

		Cmd_ExecuteString( remaining );
	}

	Com_EndRedirect();
}

typedef struct {
	const char *name;
	void ( *func )( const socket_t *socket, const netadr_t *address );
} connectionless_cmd_t;

connectionless_cmd_t connectionless_cmds[] =
{
	{ "ping", SVC_Ping },
	{ "ack", SVC_Ack },
	{ "info", SVC_InfoResponse },
	{ "getinfo", SVC_GetInfoResponse },
	{ "getstatus", SVC_GetStatusResponse },
	{ "getchallenge", SVC_GetChallenge },
	{ "connect", SVC_DirectConnect },
	{ "rcon", SVC_RemoteCommand },

	{ NULL, NULL }
};

/*
* SV_ConnectionlessPacket
*
* A connectionless packet has four leading 0xff
* characters to distinguish it from a game channel.
* Clients that are in the game can still send
* connectionless packets.
*/
void SV_ConnectionlessPacket( const socket_t *socket, const netadr_t *address, msg_t *msg ) {
	connectionless_cmd_t *cmd;
	char *s, *c;

	MSG_BeginReading( msg );
	MSG_ReadInt32( msg );    // skip the -1 marker

	s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );

	c = Cmd_Argv( 0 );
	Com_DPrintf( "Packet %s : %s\n", NET_AddressToString( address ), c );

	for( cmd = connectionless_cmds; cmd->name; cmd++ ) {
		if( !strcmp( c, cmd->name ) ) {
			cmd->func( socket, address );
			return;
		}
	}

	Com_DPrintf( "Bad connectionless packet from %s:\n%s\n", NET_AddressToString( address ), s );
}
