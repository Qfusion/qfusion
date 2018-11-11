/*
Copyright (C) 2006-2007 Benjamin Litzelmann ("Kurim")
for Chasseur de bots association.

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

#define EBHIT_FOR_AWARD     3
#define DIRECTROCKET_FOR_AWARD  3
#define DIRECTGRENADE_FOR_AWARD 3
#define MULTIKILL_INTERVAL  3000
#define LB_TIMEOUT_FOR_COMBO    200
#define GUNBLADE_TIMEOUT_FOR_COMBO  400

void G_PlayerAward( edict_t *ent, const char *awardMsg ) {
	edict_t *other;
	char cmd[MAX_STRING_CHARS];
	gameaward_t *ga;
	int i, size;
	score_stats_t *stats;

	//asdasd
	if( !awardMsg || !awardMsg[0] || !ent->r.client ) {
		return;
	}

	Q_snprintfz( cmd, sizeof( cmd ), "aw \"%s\"", awardMsg );
	trap_GameCmd( ent, cmd );

	if( dedicated->integer ) {
		G_Printf( "%s", COM_RemoveColorTokens( va( "%s receives a '%s' award.\n", ent->r.client->netname, awardMsg ) ) );
	}

	ent->r.client->level.stats.awards++;
	teamlist[ent->s.team].stats.awards++;
	G_Gametype_ScoreEvent( ent->r.client, "award", awardMsg );

	stats = &ent->r.client->level.stats;
	if( !stats->awardAllocator ) {
		stats->awardAllocator = LinearAllocator( sizeof( gameaward_t ), 0, _G_LevelMalloc, _G_LevelFree );
	}

	// ch : this doesnt work for race right?
	if( GS_MatchState() == MATCH_STATE_PLAYTIME || GS_MatchState() == MATCH_STATE_POSTMATCH ) {
		// ch : we store this locally to send to MM
		// first check if we already have this one on the clients list
		size = LA_Size( stats->awardAllocator );
		ga = NULL;
		for( i = 0; i < size; i++ ) {
			ga = ( gameaward_t * )LA_Pointer( stats->awardAllocator, i );
			if( !strncmp( ga->name, awardMsg, sizeof( ga->name ) - 1 ) ) {
				break;
			}
		}

		if( i >= size ) {
			ga = ( gameaward_t * )LA_Alloc( stats->awardAllocator );
			memset( ga, 0, sizeof( *ga ) );
			ga->name = G_RegisterLevelString( awardMsg );
		}

		if( ga ) {
			ga->count++;
		}
	}

	// add it to every player who's chasing this player
	for( other = game.edicts + 1; PLAYERNUM( other ) < gs.maxclients; other++ ) {
		if( !other->r.client || !other->r.inuse || !other->r.client->resp.chase.active ) {
			continue;
		}

		if( other->r.client->resp.chase.target == ENTNUM( ent ) ) {
			trap_GameCmd( other, cmd );
		}
	}
}

void G_PlayerMetaAward( edict_t *ent, const char *awardMsg ) {
	int i, size;
	gameaward_t *ga;
	score_stats_t *stats;

	/*
	* ch : meta-award is an award that isn't announced but
	* it is sent to MM
	*/

	if( !awardMsg || !awardMsg[0] || !ent->r.client ) {
		return;
	}

	stats = &ent->r.client->level.stats;
	if( !stats->awardAllocator ) {
		stats->awardAllocator = LinearAllocator( sizeof( gameaward_t ), 0, _G_LevelMalloc, _G_LevelFree );
	}

	// ch : this doesnt work for race right?
	if( GS_MatchState() == MATCH_STATE_PLAYTIME ) {
		// ch : we store this locally to send to MM
		// first check if we already have this one on the clients list
		size = LA_Size( stats->awardAllocator );
		ga = NULL;
		for( i = 0; i < size; i++ ) {
			ga = ( gameaward_t * )LA_Pointer( stats->awardAllocator, i );
			if( !strncmp( ga->name, awardMsg, sizeof( ga->name ) - 1 ) ) {
				break;
			}
		}

		if( i >= size ) {
			ga = ( gameaward_t * )LA_Alloc( stats->awardAllocator );
			memset( ga, 0, sizeof( *ga ) );
			ga->name = G_RegisterLevelString( awardMsg );
		}

		if( ga ) {
			ga->count++;
		}
	}
}

void G_AwardPlayerKilled( edict_t *self, edict_t *inflictor, edict_t *attacker, int mod ) {
	score_stats_t *stats;
	loggedFrag_t *lfrag;

	if( self->r.svflags & SVF_CORPSE ) {
		return;
	}

	if( !attacker->r.client ) {
		return;
	}

	if( !self->r.client ) {
		return;
	}

	if( attacker == self ) {
		return;
	}

	if( attacker->s.team == self->s.team && attacker->s.team > TEAM_PLAYERS ) {
		return;
	}

	// Multikill
	if( game.serverTime - attacker->r.client->resp.awardInfo.multifrag_timer < MULTIKILL_INTERVAL ) {
		attacker->r.client->resp.awardInfo.multifrag_count++;
	} else {
		attacker->r.client->resp.awardInfo.multifrag_count = 1;
	}

	attacker->r.client->resp.awardInfo.multifrag_timer = game.serverTime;

	if( attacker->r.client->resp.awardInfo.multifrag_count > 1 ) {
		char s[MAX_CONFIGSTRING_CHARS];

		s[0] = 0;

		switch( attacker->r.client->resp.awardInfo.multifrag_count ) {
			case 0:
			case 1:
				break;
			case 2:
				Q_strncpyz( s, S_COLOR_GREEN "Double Frag!", sizeof( s ) );
				break;
			case 3:
				Q_strncpyz( s, S_COLOR_GREEN "Triple Frag!", sizeof( s ) );
				break;
			case 4:
				Q_strncpyz( s, S_COLOR_GREEN "Quadruple Frag!", sizeof( s ) );
				break;
			default:
				Q_snprintfz( s, sizeof( s ), S_COLOR_GREEN "Extermination! %i in a row!", attacker->r.client->resp.awardInfo.multifrag_count );
				break;
		}

		G_PlayerAward( attacker, s );
	}

	if( GS_MatchState() == MATCH_STATE_PLAYTIME /* && !strcmp( "duel", gs.gametypeName ) */ ) {
		// ch : frag log
		stats = &attacker->r.client->level.stats;
		if( !stats->fragAllocator ) {
			stats->fragAllocator = LinearAllocator( sizeof( loggedFrag_t ), 0, _G_LevelMalloc, _G_LevelFree );
		}

		lfrag = ( loggedFrag_t * )LA_Alloc( stats->fragAllocator );
		lfrag->mm_attacker = attacker->r.client->mm_session;
		lfrag->mm_victim = self->r.client->mm_session;
		lfrag->weapon = G_ModToAmmo( mod ) - AMMO_GUNBLADE;
		lfrag->time = ( game.serverTime - GS_MatchStartTime() ) / 1000;
	}
}

void G_AwardRaceRecord( edict_t *self ) {
	G_PlayerAward( self, S_COLOR_CYAN "New Record!" );
}
