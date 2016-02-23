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

#include "tv_relay.h"

#include "tv_upstream.h"
#include "tv_relay_parse.h"
#include "tv_relay_module.h"
#include "tv_relay_client.h"
#include "tv_downstream.h"

#include <setjmp.h>

// for jumping over relay handling when it's disconnected
jmp_buf relay_abortframe;

/*
* TV_Relay_RunSnap
*/
static bool TV_Relay_RunSnap( relay_t *relay )
{
	int start, i;

	if( relay->state != CA_ACTIVE )
		return false;

	if( !relay->lastFrame || !relay->lastFrame->valid )
		return false;

	if( relay->curFrame == relay->lastFrame )
		return false;

	if( !relay->map_checksum )
		return false; // not fully loaded yet

	if( relay->curFrame && relay->curFrame->valid )
	{
		start = relay->curFrame->serverFrame + 1;
		if( start < relay->lastFrame->serverFrame - UPDATE_BACKUP + 1 )
			start = relay->lastFrame->serverFrame - UPDATE_BACKUP + 1;
	}
	else
	{
		start = relay->lastFrame->serverFrame - UPDATE_BACKUP + 1;
	}

	// find the frame
	for( i = start; i <= relay->lastFrame->serverFrame; i++ )
	{
		if( relay->frames[i & UPDATE_MASK].valid && relay->frames[i & UPDATE_MASK].serverFrame == i )
		{
			// we buffer server snaps and launch them with slight delay to add smoothness
			if( relay->serverTime < relay->frames[i & UPDATE_MASK].serverTime + relay->snapFrameTime )
				return false;

			relay->curFrame = &relay->frames[i & UPDATE_MASK];
			relay->framenum = relay->curFrame->serverFrame;

			return true;
		}
	}
	assert( false ); // lastFrame has to match atleast

	return false;
}

/*
* TV_Relay_InitFramesAreabits
*/
static void TV_Relay_InitFramesAreabits( relay_t *relay )
{
	int i;
	int areas;

	if( relay->frames_areabits )
	{
		Mem_Free( relay->frames_areabits );
		relay->frames_areabits = NULL;
	}

	areas = CM_NumAreas( relay->cms );
	if( areas )
	{
		areas *= CM_AreaRowSize( relay->cms );

		relay->frames_areabits = Mem_Alloc( relay->upstream->mempool, UPDATE_BACKUP * areas );
		for( i = 0; i < UPDATE_BACKUP; i++ )
		{
			relay->frames[i].areabytes = areas;
			relay->frames[i].areabits = relay->frames_areabits + i * areas;
		}
	}
	else
	{
		for( i = 0; i < UPDATE_BACKUP; i++ )
		{
			relay->frames[i].areabytes = 0;
			relay->frames[i].areabits = NULL;
		}
	}
}

/*
* TV_Relay_InitMap
*/
void TV_Relay_InitMap( relay_t *relay )
{
	int i;

	// load the map
	CM_LoadMap( relay->cms, relay->configstrings[CS_WORLDMODEL], false, &relay->map_checksum );

	// allocate areabits for frames
	TV_Relay_InitFramesAreabits( relay );

	// allow different map checksums for demos
	if( !relay->upstream->demo.playing )
	{
		if( (unsigned)atoi( relay->configstrings[CS_MAPCHECKSUM] ) != relay->map_checksum )
			TV_Relay_Error( relay, "Local map version differs from server: %u != '%u'",
			relay->map_checksum, (unsigned)atoi( relay->configstrings[CS_MAPCHECKSUM] ) );
	}
	else
	{
		// hack-update the map checksum for demos
		Q_snprintfz( relay->configstrings[CS_MAPCHECKSUM], sizeof( relay->configstrings[CS_MAPCHECKSUM] ), "%u", relay->map_checksum );
	}

	// load and spawn all other entities
	relay->module_export->SpawnEntities( relay->module, relay->configstrings[CS_WORLDMODEL],
		CM_EntityString( relay->cms ), CM_EntityStringLen( relay->cms ) );

	TV_Relay_SetAudioTrack( relay, relay->upstream->audiotrack );

	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
		relay->module_export->ConfigString( relay->module, i, relay->configstrings[i] );
}

/*
* TV_Relay_Error
* Must only be called from inside TV_Relay_Run
*/
void TV_Relay_Error( relay_t *relay, const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	assert( relay );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	TV_Relay_Shutdown( relay, "%s", msg );
	longjmp( relay_abortframe, -1 );
}

/*
* TV_Relay_Shutdown
*/
void TV_Relay_Shutdown( relay_t *relay, const char *format, ... )
{
	va_list	argptr;
	char msg[1024], cmd[256];
	int i;
	client_t *client;

	assert( relay );
	assert( format );

	if( relay->state == CA_UNINITIALIZED )
		return;

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Com_Printf( "%s" S_COLOR_WHITE ": Relay shutdown: %s\n", relay->upstream->name, msg );

	// send a message to each connected client
	for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
	{
		if( client->relay != relay )
			continue;
		TV_Downstream_ChangeStream( client, NULL );
	}

	if( relay->state > CA_CONNECTING )
	{
		for( i = 0; i < tvs.numupstreams; i++ )
		{
			if( tvs.upstreams[i] == relay->upstream )
				break;
		}
		assert( i < tvs.numupstreams );

		Q_snprintfz( cmd, sizeof( cmd ), "chr %i", i+1 );

		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state != CS_SPAWNED )
				continue;

			TV_Downstream_AddGameCommand( client->relay, client, cmd );
		}
	}

	if( relay->module_export )
		TV_Relay_ShutdownModule( relay );

	Com_FreePureList( &relay->purelist );

	if( relay->client_entities.entities )
	{
		Mem_Free( relay->client_entities.entities );
		memset( &relay->client_entities, 0, sizeof( relay->client_entities ) );
	}

	CM_ReleaseReference( relay->cms );
	relay->cms = NULL;

	relay->state = CA_UNINITIALIZED;
}

/*
* TV_Relay_ClearState
*/
void TV_Relay_ClearState( relay_t *relay )
{
	memset( relay->configstrings, 0, sizeof( relay->configstrings ) );
	memset( relay->baselines, 0, sizeof( relay->baselines ) );

	relay->realtime = 0;
	relay->lastrun = 0;

	relay->lastFrame = NULL;
	relay->curFrame = NULL;
	memset( relay->frames, 0, sizeof( relay->frames ) );
	relay->framenum = 0;
	relay->lastExecutedServerCommand = 0;
}

/*
* TV_Relay_GetPacket
*/
static msg_t *TV_Relay_GetPacket( relay_t *relay )
{
	packet_t *packet;

	assert( relay && relay->state > CA_UNINITIALIZED );

	if( !relay->packetqueue_pos )
		packet = relay->upstream->packetqueue;
	else
		packet = relay->packetqueue_pos->next;

	if( packet && ( tvs.realtime >= relay->delay ) && ( packet->time + relay->delay < tvs.realtime ) )
	{
		relay->packetqueue_pos = packet;
		return &packet->msg;
	}

	return NULL;
}

/*
* TV_Relay_NumPlayers
*/
int TV_Relay_NumPlayers( relay_t *relay )
{
	int j, numplayers = 0;
	edict_t	*ent;

	if( relay->state >= CA_ACTIVE )
	{
		for( j = 0; j < relay->gi.max_clients; j++ )
		{
			ent = EDICT_NUM( relay, j );
			if( ent->r.inuse && ent->r.client && !( ent->r.svflags & SVF_NOCLIENT ) )
				numplayers++;
		}
	}

	return numplayers;
}

/*
* TV_Relay_NameNotify
*/
void TV_Relay_NameNotify( relay_t *relay, client_t *client )
{
	char cmd[MAX_STRING_CHARS];
	const char *addr = NET_AddressToString( &relay->upstream->serveraddress );
	int numplayers = 0, numspecs = 0;

	numplayers = TV_Relay_NumPlayers( relay );
	numspecs = relay->num_active_specs;

	Q_snprintfz( cmd, sizeof( cmd ), "cha %i \"%s\" \"%s\" \"%s\" %i %i \"%s\" \"%s\" \"%s\"", relay->upstream->number + 1, relay->upstream->name,
		relay->configstrings[CS_HOSTNAME], addr, 
		numplayers, numspecs,
		relay->configstrings[CS_GAMETYPENAME], relay->configstrings[CS_MAPNAME], relay->configstrings[CS_MATCHNAME] );

	if( client )
	{
		TV_Downstream_AddGameCommand( client->relay, client, cmd );
	}
	else
	{
		int i;

		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state != CS_SPAWNED )
				continue;
			TV_Downstream_AddGameCommand( client->relay, client, cmd );
		}
	}
}

/*
* TV_Relay_ReadPackets
*/
static void TV_Relay_ReadPackets( relay_t *relay )
{
	msg_t *msg;

	assert( relay && relay->state > CA_UNINITIALIZED );

	while( ( msg = TV_Relay_GetPacket( relay ) ) != NULL )
	{
		if( relay->state == CA_CONNECTING )
		{
			int i;

			for( i = 0; i < tvs.numupstreams; i++ )
			{
				if( tvs.upstreams[i] == relay->upstream )
					break;
			}
			assert( i < tvs.numupstreams );

			relay->state = CA_HANDSHAKE;
		}

		/*if( *(int *)msg->data == -1 ) // remote command packet
		{
		TV_Relay_UpstreamlessPacket( relay, msg );
		}
		else
		{*/
		// skip header
		MSG_BeginReading( msg );
		if( !relay->upstream->demo.playing )
		{
			MSG_ReadLong( msg );
			MSG_ReadLong( msg );
		}
		if( relay->state >= CA_HANDSHAKE )
			TV_Relay_ParseServerMessage( relay, msg );
		//}
	}
}

/*
* TV_Relay_Run
*/
void TV_Relay_Run( relay_t *relay, int msec )
{
	relay->realtime += msec;

	if( setjmp( relay_abortframe ) )  // disconnect while running
		return;

	relay->serverTime = relay->realtime + relay->serverTimeDelta;

	TV_Relay_ReadPackets( relay );
	if( relay->state <= CA_DISCONNECTED )
		return;

	if( TV_Relay_RunSnap( relay ) )
	{
		relay->module_export->RunFrame( relay->module, relay->realtime - relay->lastrun );
		relay->lastrun = relay->realtime;

		relay->module_export->NewFrameSnapshot( relay->module, relay->curFrame );
		relay->module_export->SnapFrame( relay->module );

		TV_Relay_SendClientMessages( relay );

		relay->module_export->ClearSnap( relay->module );
	}

	if( relay->upstream->state == CA_DISCONNECTED && relay->packetqueue_pos == relay->upstream->packetqueue_head )
		TV_Relay_Shutdown( relay, "Out of data" );
}

/*
* TV_Relay_UpstreamUserinfoChanged
*/
void TV_Relay_UpstreamUserinfoChanged( relay_t *relay )
{
	relay->upstream->userinfo_modified = true;
}

/*
* TV_Relay_Init
*/
void TV_Relay_Init( relay_t *relay, upstream_t *upstream, int delay )
{
	assert( relay );
	assert( upstream );

	Com_Printf( "%s" S_COLOR_WHITE ": Relay init\n", upstream->name );

	memset( relay, 0, sizeof( *relay ) );

	relay->state = CA_CONNECTING;
	relay->upstream = upstream;

	relay->gi.max_clients = MAX_CLIENTS;

	relay->client_entities.num_entities = tv_maxclients->integer * UPDATE_BACKUP * MAX_SNAP_ENTITIES;
	relay->client_entities.entities = Mem_Alloc( upstream->mempool, sizeof( entity_state_t ) * relay->client_entities.num_entities );

	relay->cms = CM_New( upstream->mempool );
	CM_AddReference( relay->cms );

	relay->delay = max( delay, RELAY_MIN_DELAY );
}

/*
* TV_Relay_Init
*/
void TV_Relay_SetAudioTrack( relay_t *relay, const char *track )
{
	if( relay->state < CA_ACTIVE )
		return;
	relay->module_export->SetAudoTrack( relay->module, track );
}
