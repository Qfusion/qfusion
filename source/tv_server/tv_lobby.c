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

#include "tv_lobby.h"

#include "tv_relay.h"
#include "tv_downstream.h"

/*
* TV_Lobby_WriteFrameSnapToClient
*/
static void TV_Lobby_WriteFrameSnapToClient( client_t *client, msg_t *msg )
{
	ginfo_t gi;

	memset( &gi, 0, sizeof( ginfo_t ) );

	SNAP_WriteFrameSnapToClient( &gi, client, msg, tvs.lobby.framenum, tvs.realtime, NULL, NULL, 0, NULL, NULL );
}

/*
* TV_Lobby_SendClientDatagram
*/
static bool TV_Lobby_SendClientDatagram( client_t *client )
{
	uint8_t msg_buf[MAX_MSGLEN];
	msg_t msg;

	assert( client );
	assert( !client->relay );

	TV_Downstream_InitClientMessage( client, &msg, msg_buf, sizeof( msg_buf ) );

	TV_Downstream_AddReliableCommandsToMessage( client, &msg );
	TV_Lobby_WriteFrameSnapToClient( client, &msg );

	return TV_Downstream_SendMessageToClient( client, &msg );
}

/*
* TV_Lobby_SendClientMessages
*/
static void TV_Lobby_SendClientMessages( void )
{
	int i;
	client_t *client;

	// send a message to each connected client
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		if( client->state != CS_SPAWNED )
			continue;

		if( client->relay )
			continue;

		if( !TV_Lobby_SendClientDatagram( client ) )
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

/*
* TV_Lobby_RunSnap
*/
static bool TV_Lobby_RunSnap( void )
{
	if( tvs.lobby.lastrun + tvs.lobby.snapFrameTime > tvs.realtime )
		return false;

	tvs.lobby.framenum++;
	return true;
}

/*
* TV_Lobby_ClientBegin
*/
void TV_Lobby_ClientBegin( client_t *client )
{
	assert( client );
	assert( !client->relay );
}

/*
* TV_Lobby_ClientDisconnect
*/
void TV_Lobby_ClientDisconnect( client_t *client )
{
	assert( client );
}

/*
* TV_Lobby_CanConnect
*/
bool TV_Lobby_CanConnect( client_t *client, char *userinfo )
{
	char *value;

	assert( client );

	// check for a password
	value = Info_ValueForKey( userinfo, "password" );
	if( ( *tv_password->string && ( !value || strcmp( tv_password->string, value ) ) ) )
	{
		Info_SetValueForKey( userinfo, "rejtype", va( "%i", DROP_TYPE_PASSWORD ) );
		Info_SetValueForKey( userinfo, "rejflag", va( "%i", 0 ) );
		if( value && value[0] )
		{
			Info_SetValueForKey( userinfo, "rejmsg", "Incorrect password" );
		}
		else
		{
			Info_SetValueForKey( userinfo, "rejmsg", "Password required" );
		}
		return false;
	}

	return true;
}

/*
* TV_Lobby_ClientConnect
*/
void TV_Lobby_ClientConnect( client_t *client )
{
	assert( client );

	client->edict = NULL;
	client->relay = NULL;
}

/*
* TV_Lobby_Run
*/
void TV_Lobby_Run( void )
{
	if( TV_Lobby_RunSnap() )
	{
		tvs.lobby.lastrun = tvs.realtime;
		TV_Lobby_SendClientMessages();
	}
}
