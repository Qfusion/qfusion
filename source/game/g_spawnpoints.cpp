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

#include "g_local.h"

//QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
//The normal starting point for a level.
void SP_info_player_start( edict_t *self ) {
	G_DropSpawnpointToFloor( self );
}

//QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
//potential spawning position for deathmatch games
void SP_info_player_deathmatch( edict_t *self ) {
	G_DropSpawnpointToFloor( self );
}

void SP_post_match_camera( edict_t *ent ) { }

//=======================================================================
//
// SelectSpawnPoints
//
//=======================================================================


/*
* PlayersRangeFromSpot
*
* Returns the distance to the nearest player from the given spot
*/
float PlayersRangeFromSpot( edict_t *spot, int ignore_team ) {
	edict_t *player;
	float bestplayerdistance;
	int n;
	float playerdistance;

	bestplayerdistance = 9999999;

	for( n = 1; n <= gs.maxclients; n++ ) {
		player = &game.edicts[n];

		if( !player->r.inuse ) {
			continue;
		}
		if( player->r.solid == SOLID_NOT ) {
			continue;
		}
		if( ( ignore_team && ignore_team == player->s.team ) || player->s.team == TEAM_SPECTATOR ) {
			continue;
		}

		playerdistance = DistanceFast( spot->s.origin, player->s.origin );

		if( playerdistance < bestplayerdistance ) {
			bestplayerdistance = playerdistance;
		}
	}

	return bestplayerdistance;
}

static edict_t *G_FindPostMatchCamera( void ) {
	edict_t * ent = G_Find( NULL, FOFS( classname ), "post_match_camera" );
	if( ent != NULL )
		return ent;

	return G_Find( NULL, FOFS( classname ), "info_player_intermission" );
}

/*
* SelectRandomDeathmatchSpawnPoint
*
* go to a random point, but NOT the two points closest
* to other players
*/
static edict_t *SelectRandomDeathmatchSpawnPoint( edict_t *ent ) {
	edict_t *spot, *spot1, *spot2;
	int count = 0;
	int selection, ignore_team = 0;
	float range, range1, range2;

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	if( ent && GS_TeamBasedGametype() ) {
		ignore_team = ent->s.team;
	}

	while( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
		count++;
		range = PlayersRangeFromSpot( spot, ignore_team );
		if( range < range1 ) {
			if( range1 < range2 ) {
				range2 = range1;
				spot2 = spot1;
			}
			range1 = range;
			spot1 = spot;
		} else if( range < range2 ) {
			range2 = range;
			spot2 = spot;
		}
	}

	if( !count ) {
		return NULL;
	}

	if( count <= 2 ) {
		spot1 = spot2 = NULL;
	} else {
		if( spot1 ) {
			count--;
		}
		if( spot2 && spot2 != spot1 ) {
			count--;
		}
	}

	selection = rand() % count;
	spot = NULL;
	do {
		spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" );
		if( spot == spot1 || spot == spot2 ) {
			selection++;
		}
	} while( selection-- );

	return spot;
}

edict_t *SelectDeathmatchSpawnPoint( edict_t *ent ) {
	return SelectRandomDeathmatchSpawnPoint( ent );
}

/*
* G_OffsetSpawnPoint - use a grid of player boxes to offset the spawn point
*/
bool G_OffsetSpawnPoint( vec3_t origin, const vec3_t box_mins, const vec3_t box_maxs, float radius, bool checkground ) {
	trace_t trace;
	vec3_t virtualorigin;
	vec3_t absmins, absmaxs;
	float playerbox_rowwidth;
	float playerbox_columnwidth;
	int rows, columns;
	int i, j, row = 0, column = 0;
	int rowseed = rand() & 255;
	int columnseed = rand() & 255;
	int mask_spawn = MASK_PLAYERSOLID | ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_TELEPORTER | CONTENTS_JUMPPAD | CONTENTS_BODY | CONTENTS_NODROP );
	int playersFound = 0, worldfound = 0, nofloorfound = 0, badclusterfound = 0;

	// check box clusters
	int cluster;
	int num_leafs;
	int leafs[8];

	if( radius <= box_maxs[0] - box_mins[0] ) {
		return true;
	}

	if( checkground ) { // drop the point to ground (scripts can accept any entity now)
		VectorCopy( origin, virtualorigin );
		virtualorigin[2] -= 1024;

		G_Trace( &trace, origin, box_mins, box_maxs, virtualorigin, NULL, MASK_PLAYERSOLID );
		if( trace.fraction == 1.0f ) {
			checkground = false;
		} else if( trace.endpos[2] + 8.0f < origin[2] ) {
			VectorCopy( trace.endpos, origin );
			origin[2] += 8.0f;
		}
	}

	playerbox_rowwidth = 2 + box_maxs[0] - box_mins[0];
	playerbox_columnwidth = 2 + box_maxs[1] - box_mins[1];

	rows = radius / playerbox_rowwidth;
	columns = radius / playerbox_columnwidth;

	// no, we won't just do a while, let's go safe and just check as many times as
	// positions in the grid. If we didn't found a spawnpoint by then, we let it telefrag.
	for( i = 0; i < ( rows * columns ); i++ ) {
		row = Q_brandom( &rowseed, -rows, rows );
		column = Q_brandom( &columnseed, -columns, columns );

		VectorSet( virtualorigin, origin[0] + ( row * playerbox_rowwidth ),
				   origin[1] + ( column * playerbox_columnwidth ),
				   origin[2] );

		VectorAdd( virtualorigin, box_mins, absmins );
		VectorAdd( virtualorigin, box_maxs, absmaxs );
		for( j = 0; j < 2; j++ ) {
			absmaxs[j] += 1;
			absmins[j] -= 1;
		}

		//check if position is inside world

		// check if valid cluster
		cluster = -1; // fix a warning
		num_leafs = trap_CM_BoxLeafnums( absmins, absmaxs, leafs, 8, NULL );
		for( j = 0; j < num_leafs; j++ ) {
			cluster = trap_CM_LeafCluster( leafs[j] );
			if( cluster == -1 ) {
				break;
			}
		}

		if( cluster == -1 ) {
			badclusterfound++;
			continue;
		}

		// one more trace is needed, only checking if some part of the world is on the
		// way from spawnpoint to the virtual position
		trap_CM_TransformedBoxTrace( &trace, origin, virtualorigin, box_mins, box_maxs, NULL, MASK_PLAYERSOLID, NULL, NULL );
		if( trace.fraction != 1.0f ) {
			continue;
		}

		// check if anything solid is on player's way

		G_Trace( &trace, vec3_origin, absmins, absmaxs, vec3_origin, world, mask_spawn );
		if( trace.startsolid || trace.allsolid || trace.ent != -1 ) {
			if( trace.ent == 0 ) {
				worldfound++;
			} else if( trace.ent < gs.maxclients ) {
				playersFound++;
			}
			continue;
		}

		// one more check before accepting this spawn: there's ground at our feet?
		if( checkground ) { // if floating item flag is not set
			vec3_t origin_from, origin_to;
			VectorCopy( virtualorigin, origin_from );
			origin_from[2] += box_mins[2] + 1;
			VectorCopy( origin_from, origin_to );
			origin_to[2] -= 32;

			// use point trace instead of box trace to avoid small glitches that can't support the player but will stop the trace
			G_Trace( &trace, origin_from, vec3_origin, vec3_origin, origin_to, NULL, MASK_PLAYERSOLID );
			if( trace.startsolid || trace.allsolid || trace.fraction == 1.0f ) { // full run means no ground
				nofloorfound++;
				continue;
			}
		}

		VectorCopy( virtualorigin, origin );
		return true;
	}

	//G_Printf( "Warning: couldn't find a safe spawnpoint (blocked by players:%i world:%i nofloor:%i badcluster:%i)\n", playersFound, worldfound, nofloorfound, badclusterfound );
	return false;
}


/*
* SelectSpawnPoint
*
* Chooses a player start, deathmatch start, etc
*/
void SelectSpawnPoint( edict_t *ent, edict_t **spawnpoint, vec3_t origin, vec3_t angles ) {
	edict_t *spot = NULL;

	if( GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		spot = G_FindPostMatchCamera();
	} else {
		if( game.asEngine != NULL ) {
			spot = GT_asCallSelectSpawnPoint( ent );
		}

		if( !spot ) {
			spot = SelectDeathmatchSpawnPoint( ent );
		}
	}

	// find a single player start spot
	if( !spot ) {
		spot = G_Find( spot, FOFS( classname ), "info_player_start" );
		if( !spot ) {
			spot = G_Find( spot, FOFS( classname ), "team_CTF_alphaspawn" );
			if( !spot ) {
				spot = G_Find( spot, FOFS( classname ), "team_CTF_betaspawn" );
			}
			if( !spot ) {
				spot = world;
			}
		}
	}

	*spawnpoint = spot;
	VectorCopy( spot->s.origin, origin );
	VectorCopy( spot->s.angles, angles );

	if( !Q_stricmp( spot->classname, "info_player_intermission" ) ) {
		// if it has a target, look towards it
		if( spot->target ) {
			vec3_t dir;
			edict_t *target;

			target = G_PickTarget( spot->target );
			if( target ) {
				VectorSubtract( target->s.origin, origin, dir );
				VecToAngles( dir, angles );
			}
		}
	}

	// SPAWN TELEFRAGGING PROTECTION.
	if( ent->r.solid == SOLID_YES && ( level.gametype.spawnpointRadius > ( playerbox_stand_maxs[0] - playerbox_stand_mins[0] ) ) ) {
		G_OffsetSpawnPoint( origin, playerbox_stand_mins, playerbox_stand_maxs, level.gametype.spawnpointRadius, ( spot->spawnflags & 1 ) == 0 );
	}
}

//==================================================
// CLIENTS SPAWN QUEUE
//==================================================

#define REINFORCEMENT_WAVE_DELAY        15  // seconds
#define REINFORCEMENT_WAVE_MAXCOUNT     16

typedef struct
{
	int list[MAX_CLIENTS];
	int head;
	int start;
	int system;
	int wave_time;
	int wave_maxcount;
	bool spectate_team;
	int64_t nextWaveTime;
} g_teamspawnqueue_t;

g_teamspawnqueue_t g_spawnQueues[GS_MAX_TEAMS];

/*
* G_SpawnQueue_SetTeamSpawnsystem
*/
void G_SpawnQueue_SetTeamSpawnsystem( int team, int spawnsystem, int wave_time, int wave_maxcount, bool spectate_team ) {
	g_teamspawnqueue_t *queue;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];
	if( wave_time && wave_time != queue->wave_time ) {
		queue->nextWaveTime = level.time + brandom( 0, wave_time * 1000 );
	}

	queue->system = spawnsystem;
	queue->wave_time = wave_time;
	queue->wave_maxcount = wave_maxcount;
	if( spawnsystem != SPAWNSYSTEM_INSTANT ) {
		queue->spectate_team = spectate_team;
	} else {
		queue->spectate_team = false;
	}
}

/*
* G_SpawnQueue_Init
*/
void G_SpawnQueue_Init( void ) {
	int spawnsystem, team;
	cvar_t *g_spawnsystem;
	cvar_t *g_spawnsystem_wave_time;
	cvar_t *g_spawnsystem_wave_maxcount;

	g_spawnsystem = trap_Cvar_Get( "g_spawnsystem", va( "%i", SPAWNSYSTEM_INSTANT ), CVAR_DEVELOPER );
	g_spawnsystem_wave_time = trap_Cvar_Get( "g_spawnsystem_wave_time", va( "%i", REINFORCEMENT_WAVE_DELAY ), CVAR_ARCHIVE );
	g_spawnsystem_wave_maxcount = trap_Cvar_Get( "g_spawnsystem_wave_maxcount", va( "%i", REINFORCEMENT_WAVE_MAXCOUNT ), CVAR_ARCHIVE );

	memset( g_spawnQueues, 0, sizeof( g_spawnQueues ) );
	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ )
		memset( &g_spawnQueues[team].list, -1, sizeof( g_spawnQueues[team].list ) );

	spawnsystem = g_spawnsystem->integer;
	clamp( spawnsystem, SPAWNSYSTEM_INSTANT, SPAWNSYSTEM_HOLD );
	if( spawnsystem != g_spawnsystem->integer ) {
		trap_Cvar_ForceSet( "g_spawnsystem", va( "%i", spawnsystem ) );
	}

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		if( team == TEAM_SPECTATOR ) {
			G_SpawnQueue_SetTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
		} else {
			G_SpawnQueue_SetTeamSpawnsystem( team, spawnsystem, g_spawnsystem_wave_time->integer, g_spawnsystem_wave_maxcount->integer, true );
		}
	}
}

/*
* G_RespawnTime
*/
int G_SpawnQueue_NextRespawnTime( int team ) {
	int time;

	if( g_spawnQueues[team].system != SPAWNSYSTEM_WAVES ) {
		return 0;
	}

	if( g_spawnQueues[team].nextWaveTime < level.time ) {
		return 0;
	}

	time = (int)( g_spawnQueues[team].nextWaveTime - level.time );
	return ( time > 0 ) ? time : 0;
}

/*
* G_SpawnQueue_RemoveClient - Check all queues for this client and remove it
*/
void G_SpawnQueue_RemoveClient( edict_t *ent ) {
	g_teamspawnqueue_t *queue;
	int i, team;

	if( !ent->r.client ) {
		return;
	}

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		queue = &g_spawnQueues[team];
		for( i = queue->start; i < queue->head; i++ ) {
			if( queue->list[i % MAX_CLIENTS] == ENTNUM( ent ) ) {
				queue->list[i % MAX_CLIENTS] = -1;
			}
		}
	}
}

/*
* G_SpawnQueue_RemoveAllClients
*/
void G_SpawnQueue_ResetTeamQueue( int team ) {
	g_teamspawnqueue_t *queue;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];
	memset( &queue->list, -1, sizeof( queue->list ) );
	queue->head = queue->start = 0;
	queue->nextWaveTime = 0;
}

/*
* G_SpawnQueue_GetSystem
*/
int G_SpawnQueue_GetSystem( int team ) {
	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return SPAWNSYSTEM_INSTANT;
	}

	return g_spawnQueues[team].system;
}

/*
* G_SpawnQueue_ReleaseTeamQueue
*/
void G_SpawnQueue_ReleaseTeamQueue( int team ) {
	g_teamspawnqueue_t *queue;
	edict_t *ent;
	int count;

	if( team < TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[team];

	if( queue->start >= queue->head ) {
		return;
	}

	bool ghost = team == TEAM_SPECTATOR;

	// try to spawn them
	for( count = 0; ( queue->start < queue->head ) && ( count < gs.maxclients ); queue->start++, count++ ) {
		if( queue->list[queue->start % MAX_CLIENTS] <= 0 || queue->list[queue->start % MAX_CLIENTS] > gs.maxclients ) {
			continue;
		}

		ent = &game.edicts[queue->list[queue->start % MAX_CLIENTS]];

		G_ClientRespawn( ent, ghost );

		// when spawning inside spectator team bring up the chase camera
		if( team == TEAM_SPECTATOR && !ent->r.client->resp.chase.active ) {
			G_ChasePlayer( ent, NULL, false, 0 );
		}
	}
}

/*
* G_SpawnQueue_AddClient
*/
void G_SpawnQueue_AddClient( edict_t *ent ) {
	g_teamspawnqueue_t *queue;
	int i;

	if( !ent || !ent->r.client ) {
		return;
	}

	if( ENTNUM( ent ) <= 0 || ENTNUM( ent ) > gs.maxclients ) {
		return;
	}

	if( ent->r.client->team < TEAM_SPECTATOR || ent->r.client->team >= GS_MAX_TEAMS ) {
		return;
	}

	queue = &g_spawnQueues[ent->r.client->team];

	for( i = queue->start; i < queue->head; i++ ) {
		if( queue->list[i % MAX_CLIENTS] == ENTNUM( ent ) ) {
			return;
		}
	}

	G_SpawnQueue_RemoveClient( ent );
	queue->list[queue->head % MAX_CLIENTS] = ENTNUM( ent );
	queue->head++;

	if( queue->spectate_team ) {
		G_ChasePlayer( ent, NULL, true, 0 );
	}
}

/*
* G_SpawnQueue_Think
*/
void G_SpawnQueue_Think( void ) {
	int team, maxCount, count, spawnSystem;
	g_teamspawnqueue_t *queue;
	edict_t *ent;

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		queue = &g_spawnQueues[team];

		// if the system is limited, set limits
		maxCount = MAX_CLIENTS;

		spawnSystem = queue->system;
		clamp( spawnSystem, SPAWNSYSTEM_INSTANT, SPAWNSYSTEM_HOLD );

		switch( spawnSystem ) {
			case SPAWNSYSTEM_INSTANT:
			default:
				break;

			case SPAWNSYSTEM_WAVES:
				if( queue->nextWaveTime > level.time ) {
					maxCount = 0;
				} else {
					maxCount = ( queue->wave_maxcount < 1 ) ? gs.maxclients : queue->wave_maxcount; // max count per reinforcement wave
					queue->nextWaveTime = level.time + ( queue->wave_time * 1000 );
				}
				break;

			case SPAWNSYSTEM_HOLD:
				maxCount = 0; // players wait to be spawned elsewhere
				break;
		}

		if( maxCount <= 0 ) {
			continue;
		}

		if( queue->start >= queue->head ) {
			continue;
		}

		bool ghost = team == TEAM_SPECTATOR;

		// try to spawn them
		for( count = 0; ( queue->start < queue->head ) && ( count < maxCount ); queue->start++, count++ ) {
			if( queue->list[queue->start % MAX_CLIENTS] <= 0 || queue->list[queue->start % MAX_CLIENTS] > gs.maxclients ) {
				continue;
			}

			ent = &game.edicts[queue->list[queue->start % MAX_CLIENTS]];

			G_ClientRespawn( ent, ghost );

			// when spawning inside spectator team bring up the chase camera
			if( team == TEAM_SPECTATOR && !ent->r.client->resp.chase.active ) {
				G_ChasePlayer( ent, NULL, false, 0 );
			}
		}
	}
}
