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

#include "tv_upstream_parse.h"

#include "tv_upstream.h"
#include "tv_upstream_svcmd.h"
#include "tv_upstream_demos.h"
#include "tv_downstream_clcmd.h"

/*
* TV_Upstream_ParseFrame
*/
static void TV_Upstream_ParseFrame( upstream_t *upstream, msg_t *msg )
{
	static snapshot_t snap;

	SNAP_SkipFrame( msg, &snap );

	if( upstream->demo.recording )
	{
		if( upstream->demo.waiting && !snap.delta && snap.multipov )
		{
			upstream->demo.waiting = false; // we can start recording now
			upstream->demo.basetime = snap.serverTime;
			upstream->demo.localtime = time( NULL );

			// clear demo meta data, we'll write some keys later
			upstream->demo.meta_data_realsize = SNAP_ClearDemoMeta( upstream->demo.meta_data, sizeof( upstream->demo.meta_data ) );

			// write out messages to hold the startup information
			SNAP_BeginDemoRecording( upstream->demo.filehandle, 0x10000 + upstream->servercount, 
				upstream->snapFrameTime, upstream->levelname, upstream->reliable ? SV_BITFLAGS_RELIABLE : 0, 
				upstream->purelist, upstream->configstrings[0], upstream->baselines );
		}

		if( !upstream->demo.waiting )
			upstream->demo.duration = snap.serverTime - upstream->demo.basetime;
	}

	upstream->serverTime = snap.serverTime;
	upstream->serverFrame = snap.serverFrame;

	// getting a valid frame message ends the upstream process
	if( upstream->state != CA_ACTIVE )
		upstream->state = CA_ACTIVE;
}

/*
* TV_Upstream_ParseServerData
*/
static void TV_Upstream_ParseServerData( upstream_t *upstream, msg_t *msg )
{
	int i, numpure;

	TV_Upstream_ClearState( upstream );

	upstream->state = CA_CONNECTED;

	// parse protocol version number
	i = MSG_ReadLong( msg );

	if( i != APP_PROTOCOL_VERSION && !(upstream->demo.playing && i == APP_DEMO_PROTOCOL_VERSION) )
		TV_Upstream_Error( upstream, "Server returned version %i, not %i", i, APP_PROTOCOL_VERSION );

	upstream->servercount = MSG_ReadLong( msg );
	upstream->snapFrameTime = (unsigned int) MSG_ReadShort( msg );

	Q_strncpyz( upstream->basegame, MSG_ReadString( msg ), sizeof( upstream->basegame ) );
	Q_strncpyz( upstream->game, MSG_ReadString( msg ), sizeof( upstream->game ) );

	// parse player entity number
	upstream->playernum = MSG_ReadShort( msg );

	// get the full level name
	Q_strncpyz( upstream->levelname, MSG_ReadString( msg ), sizeof( upstream->levelname ) );

	upstream->sv_bitflags = MSG_ReadByte( msg );
	upstream->reliable = ( ( upstream->sv_bitflags & SV_BITFLAGS_RELIABLE ) ? true : false );

	if( ( upstream->sv_bitflags & SV_BITFLAGS_HTTP ) != 0 ) {
		if( ( upstream->sv_bitflags & SV_BITFLAGS_HTTP_BASEURL ) != 0 ) {
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
	Com_FreePureList( &upstream->purelist );

	// add new
	numpure = MSG_ReadShort( msg );
	while( numpure > 0 )
	{
		const char *pakname = MSG_ReadString( msg );
		const unsigned checksum = MSG_ReadLong( msg );

		Com_AddPakToPureList( &upstream->purelist, pakname, checksum, upstream->mempool );

		numpure--;
	}

	TV_Upstream_AddReliableCommand( upstream, va( "configstrings %i 0", upstream->servercount ) );
}

/*
* TV_Upstream_ParseBaseline
*/
static void TV_Upstream_ParseBaseline( upstream_t *upstream, msg_t *msg )
{
	SNAP_ParseBaseline( msg, upstream->baselines );
}

/*
* TV_Upstream_ParseServerMessage
*/
void TV_Upstream_ParseServerMessage( upstream_t *upstream, msg_t *msg )
{
	int cmd;

	assert( upstream && upstream->state >= CA_HANDSHAKE );
	assert( msg );

	// parse the message
	while( upstream->state >= CA_HANDSHAKE )
	{
		if( msg->readcount > msg->cursize )
			TV_Upstream_Error( upstream, "Bad server message" );

		cmd = MSG_ReadByte( msg );

		if( cmd == -1 )
			break;

		// other commands
		switch( cmd )
		{
		default:
			TV_Upstream_Error( upstream, "Illegible server message" );

		case svc_nop:
			break;

		case svc_servercmd:
			if( !upstream->reliable )
			{
				int cmdNum = MSG_ReadLong( msg );
				if( cmdNum < 0 )
					TV_Upstream_Error( upstream, "Invalid cmdNum value" );
				if( cmdNum <= upstream->lastExecutedServerCommand )
				{
					MSG_ReadString( msg ); // read but ignore
					break;
				}
				upstream->lastExecutedServerCommand = cmdNum;
			}
			// fall trough
		case svc_servercs: // configstrings from demo files. they don't have acknowledge
			TV_Upstream_ParseServerCommand( upstream, msg );
			break;

		case svc_serverdata:
			if( upstream->state == CA_HANDSHAKE )
			{
				Cbuf_Execute(); // make sure any stuffed commands are done

				FS_Rescan();	// FIXME?

				TV_Upstream_ParseServerData( upstream, msg );
			}
			else
			{
				return; // ignore rest of the packet (serverdata is always sent alone)
			}
			break;

		case svc_spawnbaseline:
			TV_Upstream_ParseBaseline( upstream, msg );
			break;

		case svc_download:
			//CL_ParseDownload( msg );
			break;

		case svc_clcack:
			if( upstream->reliable )
				TV_Upstream_Error( upstream, "clack message while reliable" );
			upstream->reliableAcknowledge = (unsigned)MSG_ReadLong( msg );
			MSG_ReadLong( msg ); // ucmdAcknowledged
			break;

		case svc_frame:
			TV_Upstream_ParseFrame( upstream, msg );
			break;

		case svc_demoinfo:
			{
				int length;

				assert( upstream->demo.playing );

				length = MSG_ReadLong( msg );
				MSG_SkipData( msg, length );
			}
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_match:
			TV_Upstream_Error( upstream, "Out of place frame data" );
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

	// if recording demos, copy the message out
	if( upstream->demo.recording && !upstream->demo.waiting )
		TV_Upstream_WriteDemoMessage( upstream, msg );
}
