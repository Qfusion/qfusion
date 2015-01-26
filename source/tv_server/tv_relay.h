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

#ifndef __TV_RELAY_H
#define __TV_RELAY_H

#include "tv_local.h"

#define EDICT_NUM( u, n ) ( (edict_t *)( (uint8_t *)u->gi.edicts + u->gi.edict_size*( n ) ) )
#define NUM_FOR_EDICT( u, e ) ( ( (uint8_t *)( e )-(uint8_t *)u->gi.edicts ) / u->gi.edict_size )

#define LOCAL_EDICT_NUM( u, n ) ( (edict_t *)( (uint8_t *)u->gi.local_edicts + u->gi.local_edict_size*( n ) ) )
#define NUM_FOR_LOCAL_EDICT( u, e ) ( ( (uint8_t *)( e )-(uint8_t *)u->gi.local_edicts ) / u->gi.local_edict_size )

typedef struct packet_s packet_t;

struct packet_s
{
	unsigned int time;
	msg_t msg;
	packet_t *next;
};

#define RELAY_MIN_DELAY			3*1000		// 3 seconds

#ifdef PUBLIC_BUILD
#define RELAY_GLOBAL_DELAY		30*1000		// 30 seconds
#else
#define RELAY_GLOBAL_DELAY		RELAY_MIN_DELAY
#endif

#define MAX_FRAME_SOUNDS    256
#define MAX_TIME_DELTAS	    8

typedef struct fatvis_s
{
	vec_t *skyorg;
	uint8_t pvs[MAX_MAP_LEAFS/8];
	uint8_t phs[MAX_MAP_LEAFS/8];
} fatvis_t;

typedef struct client_entities_s
{
	unsigned num_entities;				// maxclients->integer*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	unsigned next_entities;				// next client_entity to use
	entity_state_t *entities;			// [num_entities]
} client_entities_t;

struct relay_s
{
	connstate_t state;

	upstream_t *upstream;

	tvm_relay_t *module;            // link to the module's relay struct
	tv_module_export_t *module_export;
	mempool_t *module_mempool;

	unsigned int realtime;
	unsigned int lastrun;       // last RunFrame time

	int lastExecutedServerCommand;
	bool multiview;

	packet_t *packetqueue_pos;
	unsigned int delay;
	bool reliable;

	cmodel_state_t *cms;
	fatvis_t fatvis;

	ginfo_t gi;
	int num_active_specs;
	int serverTimeDelta;         // the time difference with the server time, or at least our best guess about it
	unsigned int serverTime;    // the best match we can guess about current time in the server
	unsigned int snapFrameTime;

	// initial server state
	char configstrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];
	entity_state_t baselines[MAX_EDICTS];

	// current server state
	snapshot_t	*lastFrame;             // latest snap received from the server
	snapshot_t	*curFrame;              // latest snap handled
	snapshot_t	frames[UPDATE_BACKUP];
	uint8_t *frames_areabits;
	unsigned int framenum;

	client_entities_t client_entities;

	// serverdata
	int playernum;
	int servercount;
	char game[MAX_QPATH];
	char basegame[MAX_QPATH];
	char levelname[MAX_QPATH];
	unsigned map_checksum;
	int sv_bitflags;
	purelist_t *purelist;
};

void TV_Relay_Init( relay_t *relay, upstream_t *upstream, int delay );
void TV_Relay_ClearState( relay_t *relay );
void TV_Relay_InitMap( relay_t *relay );
void TV_Relay_Error( relay_t *relay, const char *format, ... );
void TV_Relay_Shutdown( relay_t *relay, const char *format, ... );
void TV_Relay_Run( relay_t *relay, int msec );
void TV_Relay_UpstreamUserinfoChanged( relay_t *relay );
int TV_Relay_NumPlayers( relay_t *relay );
void TV_Relay_NameNotify( relay_t *relay, client_t *client );
void TV_Relay_SetAudioTrack( relay_t *relay, const char *track );

#endif // __TV_RELAY_H
