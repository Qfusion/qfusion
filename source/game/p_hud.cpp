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

char scoreboardString[MAX_STRING_CHARS];
const unsigned int scoreboardInterval = 1000;
static const char *G_PlayerStatsMessage( edict_t *ent );

//======================================================================
//
//PLAYER SCOREBOARDS
//
//======================================================================

/*
* G_ClientUpdateScoreBoardMessage
*
* Show the scoreboard messages if the scoreboards are active
*/
void G_UpdateScoreBoardMessages( void ) {
	static int nexttime = 0;
	int i;
	edict_t *ent;
	gclient_t *client;
	bool forcedUpdate = false;
	char command[MAX_STRING_CHARS];
	size_t maxlen, staticlen;

	// fixme : mess of copying
	maxlen = MAX_STRING_CHARS - ( strlen( "scb \"\"" ) + 4 );

	if( game.asEngine != NULL ) {
		GT_asCallScoreboardMessage( maxlen );
	} else {
		G_Gametype_GENERIC_ScoreboardMessage();
	}

	G_ScoreboardMessage_AddSpectators();

	staticlen = strlen( scoreboardString );

update:

	// send to players who have scoreboard visible
	for( i = 0; i < gs.maxclients; i++ ) {
		ent = game.edicts + 1 + i;
		if( !ent->r.inuse || !ent->r.client ) {
			continue;
		}

		client = ent->r.client;

		if( game.realtime <= client->level.scoreboard_time + scoreboardInterval ) {
			continue;
		}

		if( forcedUpdate || ( client->ps.stats[STAT_LAYOUTS] & STAT_LAYOUT_SCOREBOARD ) ) {
			scoreboardString[staticlen] = '\0';
			if( client->resp.chase.active ) {
				G_ScoreboardMessage_AddChasers( client->resp.chase.target, ENTNUM( ent ) );
			} else {
				G_ScoreboardMessage_AddChasers( ENTNUM( ent ), ENTNUM( ent ) );
			}
			Q_snprintfz( command, sizeof( command ), "scb \"%s\"", scoreboardString );

			client->level.scoreboard_time = game.realtime + scoreboardInterval - ( game.realtime % scoreboardInterval );
			trap_GameCmd( ent, command );
			trap_GameCmd( ent, G_PlayerStatsMessage( ent ) );
		}
	}

	if( !forcedUpdate ) {
		// every 10 seconds, send everyone the scoreboard
		nexttime -= game.snapFrameTime;
		if( nexttime > 0 ) {
			return;
		}

		do {
			nexttime += 10000;
		} while( nexttime <= 0 );

		forcedUpdate = true;
		goto update;
	}
}

/*
* G_ScoreboardMessage_AddSpectators
* generic one to add the same spectator entries to all scoreboards
*/
#define ADD_SCOREBOARD_ENTRY( scoreboardString,len,entry ) \
    \
	if( SCOREBOARD_MSG_MAXSIZE - len > strlen( entry ) ) \
	{ \
		Q_strncatz( scoreboardString, entry, sizeof( scoreboardString ) ); \
		len = strlen( scoreboardString ); \
	} \
	else \
	{ \
		return; \
	}

void G_ScoreboardMessage_AddSpectators( void ) {
	char entry[MAX_TOKEN_CHARS];
	int i, clstate;
	edict_t *e;
	edict_t **challengers;
	size_t len;

	len = strlen( scoreboardString );
	if( !len ) {
		return;
	}

	if( GS_HasChallengers() && ( challengers = G_Teams_ChallengersQueue() ) != NULL ) {
		// add the challengers
		Q_strncpyz( entry, "&w ", sizeof( entry ) );
		ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );

		for( i = 0; challengers[i]; i++ ) {
			e = challengers[i];

			//spectator tab entry
			if( !( e->r.client->connecting == true || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) ) {
				Q_snprintfz( entry, sizeof( entry ), "%i %i ",
							 PLAYERNUM( e ),
							 e->r.client->r.ping > 999 ? 999 : e->r.client->r.ping );
				ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );
			}
		}
	}

	// add spectator team
	Q_strncpyz( entry, "&s ", sizeof( entry ) );
	ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );

	for( i = 0; i < teamlist[TEAM_SPECTATOR].numplayers; i++ ) {
		e = game.edicts + teamlist[TEAM_SPECTATOR].playerIndices[i];

		if( e->r.client->connecting == true || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		if( !e->r.client->queueTimeStamp ) {
			// not in challenger queue
			Q_snprintfz( entry, sizeof( entry ), "%i %i ",
						 PLAYERNUM( e ),
						 e->r.client->r.ping > 999 ? 999 : e->r.client->r.ping );
			ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );
		}
	}

	// add connecting spectators
	for( i = 0; i < teamlist[TEAM_SPECTATOR].numplayers; i++ ) {
		e = game.edicts + teamlist[TEAM_SPECTATOR].playerIndices[i];

		// spectator tab entry
		clstate = trap_GetClientState( PLAYERNUM( e ) );
		if( e->r.client->connecting == true || ( clstate >= CS_CONNECTED && clstate < CS_SPAWNED ) ) {
			Q_snprintfz( entry, sizeof( entry ), "%i %i ", PLAYERNUM( e ), -1 );
			ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );
		}
	}
}

void G_ScoreboardMessage_AddChasers( int entnum, int entnum_self ) {
	char entry[MAX_TOKEN_CHARS];
	int i;
	edict_t *e;
	size_t len;

	len = strlen( scoreboardString );
	if( !len ) {
		return;
	}

	// add personal spectators
	Q_strncpyz( entry, "&y ", sizeof( entry ) );
	ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );

	for( i = 0; i < teamlist[TEAM_SPECTATOR].numplayers; i++ ) {
		e = game.edicts + teamlist[TEAM_SPECTATOR].playerIndices[i];

		if( ENTNUM( e ) == entnum_self ) {
			continue;
		}

		if( e->r.client->connecting || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		if( !e->r.client->resp.chase.active || e->r.client->resp.chase.target != entnum ) {
			continue;
		}

		Q_snprintfz( entry, sizeof( entry ), "%i ", PLAYERNUM( e ) );
		ADD_SCOREBOARD_ENTRY( scoreboardString, len, entry );
	}
}

/*
* G_PlayerStatsMessage
* generic one to add the stats of the current player into the scoreboard message at cgame
*/
static const char *G_PlayerStatsMessage( edict_t *ent ) {
	gsitem_t *it;
	int i;
	int weakhit, weakshot;
	int hit, shot;
	edict_t *target;
	gclient_t *client;
	static char entry[MAX_TOKEN_CHARS];

	// when chasing generate from target
	target = ent;
	client = ent->r.client;

	if( client->resp.chase.active && game.edicts[client->resp.chase.target].r.client ) {
		target = &game.edicts[client->resp.chase.target];
		client = target->r.client;
	}

	// message header
	entry[0] = '\0';
	Q_snprintfz( entry, sizeof( entry ), "plstats 0 \"" );
	Q_strncatz( entry, va( " %d", PLAYERNUM( target ) ), sizeof( entry ) );

	// weapon loop
	for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ ) {
		it = GS_FindItemByTag( i );
		assert( it );

		weakhit = hit = 0;
		weakshot = shot = 0;

		if( it->weakammo_tag != AMMO_NONE ) {
			weakhit = client->level.stats.accuracy_hits[it->weakammo_tag - AMMO_GUNBLADE];
			weakshot = client->level.stats.accuracy_shots[it->weakammo_tag - AMMO_GUNBLADE];
		}

		if( it->ammo_tag != AMMO_NONE ) {
			hit = client->level.stats.accuracy_hits[it->ammo_tag - AMMO_GUNBLADE];
			shot = client->level.stats.accuracy_shots[it->ammo_tag - AMMO_GUNBLADE];
		}

		// both in one
		Q_strncatz( entry, va( " %d", weakshot + shot ), sizeof( entry ) );
		if( weakshot + shot > 0 ) {
			Q_strncatz( entry, va( " %d", weakhit + hit ), sizeof( entry ) );

			if( i == WEAP_LASERGUN || i == WEAP_ELECTROBOLT ) {
				// strong
				Q_strncatz( entry, va( " %d", shot ), sizeof( entry ) );
				if( shot != ( weakshot + shot ) ) {
					Q_strncatz( entry, va( " %d", hit ), sizeof( entry ) );
				}
			}
		}
	}

	// add enclosing quote
	Q_strncatz( entry, "\"", sizeof( entry ) );

	return entry;
}

//=======================================================================

static unsigned int G_FindPointedPlayer( edict_t *self ) {
	trace_t trace;
	int i, j, bestNum = 0;
	vec3_t boxpoints[8];
	float value, dist, value_best = 0.90f;   // if nothing better is found, print nothing
	edict_t *other;
	vec3_t vieworg, dir, viewforward;

	if( G_IsDead( self ) ) {
		return 0;
	}

	// we can't handle the thirdperson modifications in server side :/
	VectorSet( vieworg, self->r.client->ps.pmove.origin[0], self->r.client->ps.pmove.origin[1], self->r.client->ps.pmove.origin[2] + self->r.client->ps.viewheight );
	AngleVectors( self->r.client->ps.viewangles, viewforward, NULL, NULL );

	for( i = 0; i < gs.maxclients; i++ ) {
		other = PLAYERENT( i );
		if( !other->r.inuse ) {
			continue;
		}
		if( !other->r.client ) {
			continue;
		}
		if( other == self ) {
			continue;
		}
		if( !other->r.solid || ( other->r.svflags & SVF_NOCLIENT ) ) {
			continue;
		}

		VectorSubtract( other->s.origin, self->s.origin, dir );
		dist = VectorNormalize2( dir, dir );
		if( dist > 1000 ) {
			continue;
		}

		value = DotProduct( dir, viewforward );

		if( value > value_best ) {
			BuildBoxPoints( boxpoints, other->s.origin, tv( 4, 4, 4 ), tv( 4, 4, 4 ) );
			for( j = 0; j < 8; j++ ) {
				G_Trace( &trace, vieworg, vec3_origin, vec3_origin, boxpoints[j], self, MASK_SHOT | MASK_OPAQUE );
				if( trace.ent && trace.ent == ENTNUM( other ) ) {
					value_best = value;
					bestNum = ENTNUM( other );
				}
			}
		}
	}

	return bestNum;
}

/*
* G_SetClientStats
*/
void G_SetClientStats( edict_t *ent ) {
	gclient_t *client = ent->r.client;
	int team, i;

	if( ent->r.client->resp.chase.active ) { // in chasecam it copies the other player stats
		return;
	}

	//
	// layouts
	//
	client->ps.stats[STAT_LAYOUTS] = 0;

	// don't force scoreboard when dead during timeout
	if( ent->r.client->level.showscores || GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		client->ps.stats[STAT_LAYOUTS] |= STAT_LAYOUT_SCOREBOARD;
	}
	if( GS_TeamBasedGametype() && !GS_InvidualGameType() ) {
		client->ps.stats[STAT_LAYOUTS] |= STAT_LAYOUT_TEAMTAB;
	}
	if( GS_HasChallengers() && ent->r.client->queueTimeStamp ) {
		client->ps.stats[STAT_LAYOUTS] |= STAT_LAYOUT_CHALLENGER;
	}
	if( GS_MatchState() <= MATCH_STATE_WARMUP && level.ready[PLAYERNUM( ent )] ) {
		client->ps.stats[STAT_LAYOUTS] |= STAT_LAYOUT_READY;
	}
	if( G_SpawnQueue_GetSystem( ent->s.team ) == SPAWNSYSTEM_INSTANT ) {
		client->ps.stats[STAT_LAYOUTS] |= STAT_LAYOUT_INSTANTRESPAWN;
	}

	//
	// team
	//
	client->ps.stats[STAT_TEAM] = client->ps.stats[STAT_REALTEAM] = ent->s.team;

	//
	// health
	//
	if( ent->s.team == TEAM_SPECTATOR ) {
		client->ps.stats[STAT_HEALTH] = STAT_NOTSET; // no health for spectator
	} else {
		client->ps.stats[STAT_HEALTH] = HEALTH_TO_INT( ent->health );
	}
	client->r.frags = client->ps.stats[STAT_SCORE];

	//
	// armor
	//
	if( GS_Instagib() ) {
		if( g_instashield->integer ) {
			client->ps.stats[STAT_ARMOR] = ARMOR_TO_INT( 100.0f * ( client->resp.instashieldCharge / INSTA_SHIELD_MAX ) );
		} else {
			client->ps.stats[STAT_ARMOR] = 0;
		}
	} else {
		client->ps.stats[STAT_ARMOR] = ARMOR_TO_INT( client->resp.armor );
	}

	//
	// pickup message
	//
	if( level.time > client->resp.pickup_msg_time ) {
		client->ps.stats[STAT_PICKUP_ITEM] = 0;
	}

	//
	// frags
	//
	if( ent->s.team == TEAM_SPECTATOR ) {
		client->ps.stats[STAT_SCORE] = STAT_NOTSET; // no frags for spectators
	} else {
		client->ps.stats[STAT_SCORE] = ent->r.client->level.stats.score;
	}

	//
	// Team scores
	//
	if( GS_TeamBasedGametype() ) {
		// team based
		i = 0;
		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			client->ps.stats[STAT_TEAM_ALPHA_SCORE + i] = teamlist[team].stats.score;
			i++;
		}
	} else {
		// not team based
		i = 0;
		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			client->ps.stats[STAT_TEAM_ALPHA_SCORE + i] = STAT_NOTSET;
			i++;
		}
	}

	// spawn system
	client->ps.stats[STAT_NEXT_RESPAWN] = ceil( G_SpawnQueue_NextRespawnTime( client->team ) * 0.001f );

	// pointed player
	client->ps.stats[STAT_POINTED_TEAMPLAYER] = 0;
	client->ps.stats[STAT_POINTED_PLAYER] = G_FindPointedPlayer( ent );
	if( client->ps.stats[STAT_POINTED_PLAYER] && GS_TeamBasedGametype() ) {
		edict_t *e = &game.edicts[client->ps.stats[STAT_POINTED_PLAYER]];
		if( e->s.team == ent->s.team ) {
			int pointedhealth = HEALTH_TO_INT( e->health );
			int pointedarmor = 0;
			int available_bits = 0;
			bool mega = false;

			if( pointedhealth < 0 ) {
				pointedhealth = 0;
			}
			if( pointedhealth > 100 ) {
				pointedhealth -= 100;
				mega = true;
				if( pointedhealth > 100 ) {
					pointedhealth = 100;
				}
			}
			pointedhealth /= 3.2;

			if( GS_Armor_TagForCount( e->r.client->resp.armor ) ) {
				pointedarmor = ARMOR_TO_INT( e->r.client->resp.armor );
			}
			if( pointedarmor > 150 ) {
				pointedarmor = 150;
			}
			pointedarmor /= 5;

			client->ps.stats[STAT_POINTED_TEAMPLAYER] = ( ( pointedhealth & 0x1F ) | ( pointedarmor & 0x3F ) << 6 | ( available_bits & 0xF ) << 12 );
			if( mega ) {
				client->ps.stats[STAT_POINTED_TEAMPLAYER] |= 0x20;
			}
		}
	}

	// last killer. ignore world and team kills
	if( client->teamstate.last_killer ) {
		edict_t *targ = ent, *attacker = client->teamstate.last_killer;
		client->ps.stats[STAT_LAST_KILLER] = ( attacker->r.client && !GS_IsTeamDamage( &targ->s, &attacker->s ) ?
											   ENTNUM( attacker ) : 0 );
	} else {
		client->ps.stats[STAT_LAST_KILLER] = 0;
	}
}
