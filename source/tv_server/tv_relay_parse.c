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

#include "tv_relay_parse.h"

#include "tv_relay.h"
#include "tv_upstream.h"
#include "tv_relay_svcmd.h"
#include "tv_relay_client.h"
#include "tv_downstream_clcmd.h"

/*
* TV_Relay_ParseFrame
*/
static void TV_Relay_ParseFrame( relay_t *relay, msg_t *msg )
{
	snapshot_t *snap;

	snap = SNAP_ParseFrame( msg, relay->lastFrame, NULL, relay->frames, relay->baselines, 0 );

	// ignore older than already received
	if( relay->lastFrame && snap->serverFrame <= relay->lastFrame->serverFrame )
	{
		if( relay->lastFrame->serverFrame == snap->serverFrame )
			Com_Printf( "Frame %i received twice\n", relay->lastFrame->serverFrame );
		else
			Com_Printf( "Dropping older frame snap\n" );
		return;
	}

	if( !snap->valid )
	{
		Com_Printf( "Invalid frame\n" );
		return;
	}

	relay->serverTimeDelta = (snap->serverTime - relay->realtime);
	relay->serverTime = relay->realtime + relay->serverTimeDelta;

	// save the frame off in the backup array for later delta comparisons
	relay->frames[snap->serverFrame & UPDATE_MASK] = *snap;
	// update lastframe pointer
	relay->lastFrame = &relay->frames[snap->serverFrame & UPDATE_MASK];

	// update areaportals
	CM_ReadAreaBits( relay->cms, snap->areabits );

	// getting a valid frame message ends the upstream process
	if( relay->state != CA_ACTIVE )
	{
		int i;
		client_t *client;

		relay->state = CA_ACTIVE;

		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state != CS_CONNECTED )  // AWAITING?
				continue;
			if( client->relay != relay )
				continue;
			TV_Downstream_New_f( client );
		}
	}
}

/*
* TV_Relay_ParseServerData
*/
static void TV_Relay_ParseServerData( relay_t *relay, msg_t *msg )
{
	int i, numpure;

	TV_Relay_ClearState( relay );

	relay->state = CA_CONNECTED;
	relay->map_checksum = 0;

	// parse protocol version number
	i = MSG_ReadLong( msg );

	if( i != APP_PROTOCOL_VERSION && !(relay->upstream->demo.playing && i == APP_DEMO_PROTOCOL_VERSION) )
		TV_Relay_Error( relay, "Server returned version %i, not %i", i, APP_PROTOCOL_VERSION );

	relay->servercount = MSG_ReadLong( msg );
	relay->snapFrameTime = (unsigned int)MSG_ReadShort( msg );

	Q_strncpyz( relay->basegame, MSG_ReadString( msg ), sizeof( relay->basegame ) );
	Q_strncpyz( relay->game, MSG_ReadString( msg ), sizeof( relay->game ) );

	// parse player entity number
	relay->playernum = MSG_ReadShort( msg );

	// get the full level name
	Q_strncpyz( relay->levelname, MSG_ReadString( msg ), sizeof( relay->levelname ) );

	relay->sv_bitflags = MSG_ReadByte( msg );

	// using upstream->reliable won't work for TV_Relay_ParseServerMessage
	// in case of reliable demo following unreliable demo, causing "clack message while reliable" error
	relay->reliable = ( ( relay->sv_bitflags & SV_BITFLAGS_RELIABLE ) ? true : false );

	if( ( relay->sv_bitflags & SV_BITFLAGS_HTTP ) != 0 ) {
		if( ( relay->sv_bitflags & SV_BITFLAGS_HTTP_BASEURL ) != 0 ) {
			// read base upstream url
			MSG_ReadString( msg );
		}
		else {
			// http port number
			MSG_ReadShort( msg );
		}
	}

	// pure list

	// clean old, if necessary
	Com_FreePureList( &relay->purelist );

	// add new
	numpure = MSG_ReadShort( msg );
	while( numpure > 0 )
	{
		const char *pakname = MSG_ReadString( msg );
		const unsigned checksum = MSG_ReadLong( msg );

		Com_AddPakToPureList( &relay->purelist, pakname, checksum, relay->upstream->mempool );

		numpure--;
	}
}

/*
* TV_Relay_ParseBaseline
*/
static void TV_Relay_ParseBaseline( relay_t *relay, msg_t *msg )
{
	SNAP_ParseBaseline( msg, relay->baselines );
}

/*
* TV_Relay_ParseServerMessage
*/
void TV_Relay_ParseServerMessage( relay_t *relay, msg_t *msg )
{
	int cmd;

	assert( relay && relay->state >= CA_HANDSHAKE );
	assert( msg );

	// parse the message
	while( relay->state >= CA_HANDSHAKE )
	{
		if( msg->readcount > msg->cursize )
			TV_Relay_Error( relay, "Bad server message" );

		cmd = MSG_ReadByte( msg );
		/*if( cmd == -1 )
		Com_Printf( "%3i:CMD %i %s\n", msg->readcount-1, cmd, "EOF" );
		else
		Com_Printf( "%3i:CMD %i %s\n", msg->readcount-1, cmd, !svc_strings[cmd] ? "bad" : svc_strings[cmd] );*/

		if( cmd == -1 )
			break;

		// other commands
		switch( cmd )
		{
		default:
			TV_Relay_Error( relay, "Illegible server message" );

		case svc_nop:
			break;

		case svc_servercmd:
			if( !relay->reliable )
			{
				int cmdNum = MSG_ReadLong( msg );
				if( cmdNum < 0 )
					TV_Relay_Error( relay, "Invalid cmdNum value" );
				if( cmdNum <= relay->lastExecutedServerCommand )
				{
					MSG_ReadString( msg ); // read but ignore
					break;
				}
				relay->lastExecutedServerCommand = cmdNum;
			}
			// fall trough
		case svc_servercs: // configstrings from demo files. they don't have acknowledge
			TV_Relay_ParseServerCommand( relay, msg );
			break;

		case svc_serverdata:
			if( relay->upstream->demo.playing )
				TV_Relay_ReconnectClients( relay );

			if( relay->state == CA_HANDSHAKE )
			{
				Cbuf_Execute(); // make sure any stuffed commands are done
				TV_Relay_ParseServerData( relay, msg );
			}
			else
			{
				return; // ignore rest of the packet (serverdata is always sent alone)
			}
			break;

		case svc_spawnbaseline:
			TV_Relay_ParseBaseline( relay, msg );
			break;

		case svc_download:
			//CL_ParseDownload( msg );
			break;

		case svc_clcack:
			if( relay->reliable )
				TV_Relay_Error( relay, "clack message while reliable" );
			MSG_ReadLong( msg ); // reliableAcknowledge
			MSG_ReadLong( msg ); // ucmdAcknowledged
			break;

		case svc_frame:
			TV_Relay_ParseFrame( relay, msg );
			break;

		case svc_demoinfo:
			{
				int length;
				
				length = MSG_ReadLong( msg );
				MSG_SkipData( msg, length );
			}
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_match:
			TV_Relay_Error( relay, "Out of place frame data" );
			break;

		case svc_extension:
			if( 1 )
			{
				int len;

				MSG_ReadByte( msg );			// extension id
				MSG_ReadByte( msg );			// version number
				len = MSG_ReadShort( msg );		// command length
				MSG_SkipData( msg, len );		// command data
			}
			break;
		}
	}
}
