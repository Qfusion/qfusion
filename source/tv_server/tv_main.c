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

#include "tv_main.h"

#include "tv_upstream.h"
#include "tv_cmds.h"
#include "tv_downstream.h"
#include "tv_lobby.h"

tv_t tvs;

mempool_t *tv_mempool;

cvar_t *tv_password;

cvar_t *tv_rcon_password;

cvar_t *tv_ip;
cvar_t *tv_port;
cvar_t *tv_udp;
cvar_t *tv_ip6;
cvar_t *tv_port6;
#ifdef TCP_ALLOW_TVCONNECT
cvar_t *tv_tcp;
#endif
cvar_t *tv_public;
cvar_t *tv_autorecord;
cvar_t *tv_lobbymusic;

cvar_t *tv_timeout;
cvar_t *tv_zombietime;

cvar_t *tv_maxclients;
cvar_t *tv_maxmvclients;
cvar_t *tv_compresspackets;
cvar_t *tv_name;
cvar_t *tv_reconnectlimit; // minimum seconds between connect messages

cvar_t *tv_masterservers;
cvar_t *tv_masterservers_steam;

cvar_t *tv_floodprotection_messages;
cvar_t *tv_floodprotection_seconds;
cvar_t *tv_floodprotection_penalty;

/*
* TV_Init
* 
* Only called at plat.exe startup, not for each game
*/
void TV_Init( void )
{
	Com_Printf( "Initializing " APPLICATION " TV server\n" );

	tv_mempool = Mem_AllocPool( NULL, "TV" );

	TV_AddCommands();

	Cvar_Get( "protocol", va( "%i", APP_PROTOCOL_VERSION ), CVAR_SERVERINFO | CVAR_NOSET );
	Cvar_Get( "gamename", Cvar_String( "gamename" ), CVAR_SERVERINFO | CVAR_NOSET );

	tv_password = Cvar_Get( "tv_password", "", 0 );

	tv_ip = Cvar_Get( "tv_ip", "", CVAR_ARCHIVE | CVAR_NOSET );
	tv_port = Cvar_Get( "tv_port", va( "%i", PORT_TV_SERVER ), CVAR_ARCHIVE | CVAR_NOSET );
	tv_ip6 = Cvar_Get( "tv_ip6", "::", CVAR_ARCHIVE | CVAR_NOSET );
	tv_port6 = Cvar_Get( "tv_port6", va( "%i", PORT_TV_SERVER ), CVAR_ARCHIVE | CVAR_NOSET );
#ifdef TCP_ALLOW_TVCONNECT
	tv_udp = Cvar_Get( "tv_udp", "1", CVAR_SERVERINFO | CVAR_NOSET );
	tv_tcp = Cvar_Get( "tv_tcp", "1", CVAR_SERVERINFO | CVAR_NOSET );
#else
	tv_udp = Cvar_Get( "tv_udp", "1", CVAR_NOSET );
#endif

#ifndef TCP_ALLOW_TVCONNECT
	Cvar_FullSet( "tv_tcp", "0", CVAR_READONLY, true );
#endif

	tv_reconnectlimit = Cvar_Get( "tv_reconnectlimit", "3", CVAR_ARCHIVE );
	tv_timeout = Cvar_Get( "tv_timeout", "125", 0 );
	tv_zombietime = Cvar_Get( "tv_zombietime", "2", 0 );
	tv_name = Cvar_Get( "tv_name", APPLICATION "[TV]", CVAR_SERVERINFO | CVAR_ARCHIVE );
	tv_compresspackets = Cvar_Get( "tv_compresspackets", "1", 0 );
	tv_maxclients = Cvar_Get( "tv_maxclients", "64", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOSET );
	tv_maxmvclients = Cvar_Get( "tv_maxmvclients", "4", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_NOSET );
	tv_public = Cvar_Get( "tv_public", "1", CVAR_ARCHIVE | CVAR_SERVERINFO );
	tv_rcon_password = Cvar_Get( "tv_rcon_password", "", 0 );
	tv_autorecord = Cvar_Get( "tv_autorecord", "", CVAR_ARCHIVE );
	tv_lobbymusic = Cvar_Get( "tv_lobbymusic", "", CVAR_ARCHIVE );

	tv_masterservers = Cvar_Get( "tv_masterservers", DEFAULT_MASTER_SERVERS_IPS, CVAR_LATCH );
	tv_masterservers_steam = Cvar_Get( "tv_masterservers_steam", DEFAULT_MASTER_SERVERS_STEAM_IPS, CVAR_LATCH );

	// flood control
	tv_floodprotection_messages = Cvar_Get( "tv_floodprotection_messages", "10", 0 );
	tv_floodprotection_messages->modified = true;
	tv_floodprotection_seconds = Cvar_Get( "tv_floodprotection_seconds", "4", 0 );
	tv_floodprotection_seconds->modified = true;
	tv_floodprotection_penalty = Cvar_Get( "tv_floodprotection_delay", "20", 0 );
	tv_floodprotection_penalty->modified = true;

	if( tv_maxclients->integer < 0 )
		Cvar_ForceSet( "tv_maxclients", "0" );

	if( tv_maxclients->integer )
	{
		tvs.clients = ( client_t * )Mem_Alloc( tv_mempool, sizeof( client_t ) * tv_maxclients->integer );
	}
	else
	{
		tvs.clients = NULL;
	}
	tvs.lobby.spawncount = rand();
	tvs.lobby.snapFrameTime = 100;

	// IPv4
	if( !NET_StringToAddress( tv_ip->string, &tvs.address ) )
		Com_Error( ERR_FATAL, "Couldn't understand address of tv_ip cvar: %s\n", NET_ErrorString() );
	NET_SetAddressPort( &tvs.address, tv_port->integer );

	if( tv_udp->integer )
	{
		if( !NET_OpenSocket( &tvs.socket_udp, SOCKET_UDP, &tvs.address, true ) )
		{
			Com_Printf( "Error: Couldn't open UDP socket: %s\n", NET_ErrorString() );
			Cvar_ForceSet( tv_udp->name, "0" );
		}
	}

	// IPv6
	if( !NET_StringToAddress( tv_ip6->string, &tvs.addressIPv6 ) )
		Com_Error( ERR_FATAL, "Couldn't understand address of tv_ip6 cvar: %s\n", NET_ErrorString() );
	NET_SetAddressPort( &tvs.addressIPv6, tv_port6->integer );

	if( tvs.addressIPv6.type != NA_NOTRANSMIT )
	{
		if( !NET_OpenSocket( &tvs.socket_udp6, SOCKET_UDP, &tvs.addressIPv6, true ) )
		{
			Com_Printf( "Error: Couldn't open UDP6 socket: %s\n", NET_ErrorString() );
		}
	}

#ifdef TCP_ALLOW_TVCONNECT
	if( tv_tcp->integer )
	{
		bool err = true;

		if( !NET_OpenSocket( &tvs.socket_tcp, SOCKET_TCP, &tvs.address, true ) )
		{
			Com_Printf( "Error: Couldn't open TCP socket: %s\n", NET_ErrorString() );
		}
		else
		{
			NET_SetSocketNoDelay( &tvs.socket_tcp, 1 );
			if( !NET_Listen( &tvs.socket_tcp ) )
			{
				Com_Printf( "Error: Couldn't listen to TCP socket: %s\n", NET_ErrorString() );
			}
			else
			{
				err = false;
			}
		}

		if( tvs.addressIPv6.type != NA_NOTRANSMIT )
		{
			if( !NET_OpenSocket( &tvs.socket_tcp6, SOCKET_TCP, &tvs.addressIPv6, true ) )
			{
				Com_Printf( "Error: Couldn't open TCP6 socket: %s\n", NET_ErrorString() );
			}
			else
			{
				NET_SetSocketNoDelay( &tvs.socket_tcp6, 1 );
				if( !NET_Listen( &tvs.socket_tcp6 ) )
				{
					Com_Printf( "Error: Couldn't listen to TCP6 socket: %s\n", NET_ErrorString() );
				}
				else
				{
					err = false;
				}
			}
		}

		if( err ) {
			Cvar_ForceSet( tv_tcp->name, "0" );
		}
	}
#endif

	TV_Downstream_InitMaster();
}

/*
* TV_Frame
*/
void TV_Frame( int realmsec, int gamemsec )
{
	int i;

	tvs.realtime += realmsec;

	TV_Lobby_Run();

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		if( userinfo_modified )
			tvs.upstreams[i]->userinfo_modified = true;

		TV_Upstream_Run( tvs.upstreams[i], realmsec );
	}
	userinfo_modified = false;

	TV_Downstream_ReadPackets();
	TV_Downstream_SendClientMessages();
	TV_Downstream_CheckTimeouts();

	// FIXME
	TV_Downstream_SendClientsFragments();

	TV_Downstream_MasterHeartbeat();

	Sys_Sleep( 5 );
}

/*
* TV_Shutdown
*/
void TV_Shutdown( const char *finalmsg )
{
	int i;

	TV_Downstream_MasterSendQuit();

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		TV_Upstream_Shutdown( tvs.upstreams[i], "Quit: %s", finalmsg );
	}
	Mem_Free( tvs.upstreams );
	tvs.upstreams = NULL;
	tvs.numupstreams = 0;

	TV_RemoveCommands();
}

/*
* TV_ShutdownGame
* 
* ERR_DROP thrown, we will upgrade it to ERR_FATAL
*/
void TV_ShutdownGame( const char *finalmsg, bool reconnect )
{
	Com_Error( ERR_FATAL, "%s", finalmsg );
}

/*
* Just some renaming so we can call the functions above TV not SV
*/

void SV_Init( void )
{
	TV_Init();
}

void SV_Shutdown( const char *finalmsg )
{
	TV_Shutdown( finalmsg );
}

void SV_ShutdownGame( const char *finalmsg, bool reconnect )
{
}

void SV_Frame( int realmsec, int gamemsec )
{
	TV_Frame( realmsec, gamemsec );
}

//=============================================================================
//
//Com_Printf redirection
//
//=============================================================================

char tv_outputbuf[TV_OUTPUTBUF_LENGTH];
void TV_FlushRedirect( int tv_redirected, const char *outputbuf, const void *extra )
{
	const flush_params_t *params = ( flush_params_t * )extra;

	if( tv_redirected == RD_PACKET )
	{
		Netchan_OutOfBandPrint( params->socket, params->address, "print\n%s", outputbuf );
	}
}

//============================================================================

char *_TVCopyString_Pool( mempool_t *pool, const char *in, const char *filename, int fileline )
{
	char *out;

	out = _Mem_Alloc( pool, sizeof( char ) * ( strlen( in ) + 1 ), 0, 0, filename, fileline );
	//out = Mem_ZoneMalloc( sizeof(char) * (strlen(in) + 1) );
	//Q_strncpyz( out, in, sizeof(char) * (strlen(in) + 1) );
	strcpy( out, in );

	return out;
}
