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

g_teamlist_t teamlist[GS_MAX_TEAMS];

static void G_Gametype_GENERIC_Init( void ) {
	level.gametype.spawnableItemsMask = ( IT_WEAPON | IT_AMMO | IT_POWERUP | IT_HEALTH );
	level.gametype.respawnableItemsMask = ( IT_WEAPON | IT_AMMO | IT_POWERUP | IT_HEALTH );
	level.gametype.dropableItemsMask = ( IT_WEAPON | IT_AMMO | IT_POWERUP | IT_HEALTH );
	level.gametype.pickableItemsMask = ( level.gametype.spawnableItemsMask | level.gametype.dropableItemsMask );

	level.gametype.isTeamBased = false;
	level.gametype.isRace = false;
	level.gametype.hasChallengersQueue = false;
	level.gametype.hasChallengersRoulette = false;
	level.gametype.maxPlayersPerTeam = 0;

	level.gametype.ammo_respawn = 20;
	level.gametype.weapon_respawn = 5;
	level.gametype.health_respawn = 25;
	level.gametype.powerup_respawn = 90;
	level.gametype.megahealth_respawn = 20;
	level.gametype.ultrahealth_respawn = 60;

	level.gametype.countdownEnabled = false;
	level.gametype.matchAbortDisabled = false;
	level.gametype.canForceModels = true;
	level.gametype.spawnpointRadius = 256;

	level.gametype.numBots = 0;

	trap_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 164 %i 64 %l 48 %p 18 %p 18" );
	trap_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Score Ping C R" );
}

//==========================================================
//					Matches
//==========================================================

cvar_t *g_warmup_timelimit;
cvar_t *g_postmatch_timelimit;
cvar_t *g_match_extendedtime;
cvar_t *g_scorelimit;
cvar_t *g_gametype;

//==========================================================
//					Matches
//==========================================================

/*
* G_GetGameState
*/
game_state_t *G_GetGameState( void ) {
	return &gs.gameState;
}

/*
* G_Match_Tied
*/
bool G_Match_Tied( void ) {
	int team, total, numteams;

	total = 0; numteams = 0;
	for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
		if( !teamlist[team].numplayers ) {
			continue;
		}

		numteams++;
		total += teamlist[team].stats.score;
	}

	if( numteams < 2 ) {
		return false;
	} else {

		// total / numteams = averaged score
		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				continue;
			}

			if( teamlist[team].stats.score != total / numteams ) {
				return false;
			}
		}
	}

	return true;
}

/*
* G_Match_CheckExtendPlayTime
*/
bool G_Match_CheckExtendPlayTime( void ) {
	// check for extended time/sudden death
	if( GS_MatchState() != MATCH_STATE_PLAYTIME ) {
		return false;
	}

	if( GS_TeamBasedGametype() && !level.forceExit ) {
		if( G_Match_Tied() ) {
			GS_GamestatSetFlag( GAMESTAT_FLAG_MATCHEXTENDED, true );
			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_PLAYTIME;
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;

			G_AnnouncerSound( NULL, trap_SoundIndex( va( S_ANNOUNCER_OVERTIME_SUDDENDEATH_1_to_2, ( rand() & 1 ) + 1 ) ), GS_MAX_TEAMS, true, NULL );
			G_PrintMsg( NULL, "Match tied. Sudden death!\n" );
			G_CenterPrintMsg( NULL, "SUDDEN DEATH" );
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = 0;

			return true;
		}
	}

	return false;
}

/*
* G_Match_SetAutorecordState
*/
static void G_Match_SetAutorecordState( const char *state ) {
	trap_ConfigString( CS_AUTORECORDSTATE, state );
}

/*
* G_Match_Autorecord_Start
*/
void G_Match_Autorecord_Start( void ) {
	int team, i, playerCount;

	G_Match_SetAutorecordState( "start" );

	// do not start autorecording if all playing clients are bots
	for( playerCount = 0, team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		// add our team info to the string
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			if( game.edicts[ teamlist[team].playerIndices[i] ].r.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			playerCount++;
			break; // we only need one for this check
		}
	}

	if( playerCount && g_autorecord->integer ) {
		char datetime[17], players[MAX_STRING_CHARS];
		time_t long_time;
		struct tm *newtime;

		// date & time
		time( &long_time );
		newtime = localtime( &long_time );

		Q_snprintfz( datetime, sizeof( datetime ), "%04d-%02d-%02d_%02d-%02d", newtime->tm_year + 1900,
					 newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min );

		// list of players
		Q_strncpyz( players, trap_GetConfigString( CS_MATCHNAME ), sizeof( players ) );
		if( players[0] == '\0' ) {
			if( GS_InvidualGameType() ) {
				const char *netname;
				edict_t *ent;

				for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
					if( !teamlist[team].numplayers ) {
						continue;
					}
					ent = game.edicts + teamlist[team].playerIndices[0];
					netname = ent->r.client->netname;
					Q_strncatz( players, netname, sizeof( players ) );
					if( team != GS_MAX_TEAMS - 1 ) {
						Q_strncatz( players, " vs ", sizeof( players ) );
					}
				}
			}
		}

		if( players[0] != '\0' ) {
			char *t = strstr( players, " vs " );
			if( t ) {
				memcpy( t, "_vs_", strlen( "_vs_" ) );
			}
			Q_strncpyz( players, COM_RemoveJunkChars( COM_RemoveColorTokens( players ) ), sizeof( players ) );
		}

		// combine
		Q_snprintfz( level.autorecord_name, sizeof( level.autorecord_name ), "%s_%s%s%s_auto%04i",
					 datetime, level.mapname, players[0] == '\0' ? "" : "_", players, (int)brandom( 1, 9999 ) );

		trap_Cmd_ExecuteText( EXEC_APPEND, va( "serverrecord %s\n", level.autorecord_name ) );
	}
}

/*
* G_Match_Autorecord_AltStart
*/
void G_Match_Autorecord_AltStart( void ) {
	G_Match_SetAutorecordState( "altstart" );
}

/*
* G_Match_Autorecord_Stats
*/
void G_Match_Autorecord_Stats( void ) {
	edict_t *ent;

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ ) {
		if( !ent->r.inuse || ent->s.team == TEAM_SPECTATOR || ( ent->r.svflags & SVF_FAKECLIENT ) ) {
			continue;
		}
		trap_GameCmd( ent, va( "plstats 2 \"%s\"", G_StatsMessage( ent ) ) );
	}
}

/*
* G_Match_Autorecord_Stop
*/
void G_Match_Autorecord_Stop( void ) {
	G_Match_SetAutorecordState( "stop" );

	if( g_autorecord->integer ) {
		// stop it
		trap_Cmd_ExecuteText( EXEC_APPEND, "serverrecordstop 1\n" );

		// check if we wanna delete some
		if( g_autorecord_maxdemos->integer > 0 ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "serverrecordpurge %i\n", g_autorecord_maxdemos->integer ) );
		}
	}
}

/*
* G_Match_Autorecord_Cancel
*/
void G_Match_Autorecord_Cancel( void ) {
	G_Match_SetAutorecordState( "cancel" );

	if( g_autorecord->integer ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "serverrecordcancel 1\n" );
	}
}

/*
* G_Match_CheckStateAbort
*/
static void G_Match_CheckStateAbort( void ) {
	bool any = false;
	bool enough;

	if( GS_MatchState() <= MATCH_STATE_NONE || GS_MatchState() >= MATCH_STATE_POSTMATCH
		|| level.gametype.matchAbortDisabled ) {
		GS_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
		return;
	}

	if( GS_TeamBasedGametype() ) {
		int team, emptyteams = 0;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				emptyteams++;
			} else {
				any = true;
			}
		}

		enough = ( emptyteams == 0 );
	} else {
		enough = ( teamlist[TEAM_PLAYERS].numplayers > 1 );
		any = ( teamlist[TEAM_PLAYERS].numplayers > 0 );
	}

	// if waiting, turn on match states when enough players joined
	if( GS_MatchWaiting() && enough ) {
		GS_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );
	}
	// turn off active match states if not enough players left
	else if( GS_MatchState() == MATCH_STATE_WARMUP && !enough && GS_MatchDuration() ) {
		GS_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	} else if( GS_MatchState() == MATCH_STATE_COUNTDOWN && !enough ) {
		if( any ) {
			G_PrintMsg( NULL, "Not enough players left. Countdown aborted.\n" );
			G_CenterPrintMsg( NULL, "COUNTDOWN ABORTED" );
		}
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
		GS_GamestatSetFlag( GAMESTAT_FLAG_WAITING, true );
	}
	// match running, but not enough players left
	else if( GS_MatchState() == MATCH_STATE_PLAYTIME && !enough ) {
		if( any ) {
			G_PrintMsg( NULL, "Not enough players left. Match aborted.\n" );
			G_CenterPrintMsg( NULL, "MATCH ABORTED" );
		}
		G_EndMatch();
	}
}

/*
* G_Match_LaunchState
*/
void G_Match_LaunchState( int matchState ) {
	static bool advance_queue = false;

	if( game.asEngine != NULL ) {
		// give the gametype a chance to refuse the state change, or to set up things for it
		if( !GT_asCallMatchStateFinished( matchState ) ) {
			return;
		}
	}

	GS_GamestatSetFlag( GAMESTAT_FLAG_MATCHEXTENDED, false );
	GS_GamestatSetFlag( GAMESTAT_FLAG_WAITING, false );

	if( matchState == MATCH_STATE_POSTMATCH ) {
		level.finalMatchDuration = game.serverTime - GS_MatchStartTime();
	}

	switch( matchState ) {
		default:
		case MATCH_STATE_WARMUP:
		{
			advance_queue = false;
			level.forceStart = false;

			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_WARMUP;
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = (int64_t)( fabs( g_warmup_timelimit->value * 60 ) * 1000 );
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;

			break;
		}

		case MATCH_STATE_COUNTDOWN:
		{
			advance_queue = true;

			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_COUNTDOWN;
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = 5000;
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;

			break;
		}

		case MATCH_STATE_PLAYTIME:
		{
			// ch : should clear some statcollection memory from warmup?

			advance_queue = true; // shouldn't be needed here
			level.forceStart = false;

			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_PLAYTIME;
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = 0;
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;
		}
		break;

		case MATCH_STATE_POSTMATCH:
		{
			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_POSTMATCH;
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = (int64_t)fabs( g_postmatch_timelimit->value * 1000 ); // postmatch time in seconds
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;

			G_Timeout_Reset();
			level.teamlock = false;
			level.forceExit = false;

			G_Match_Autorecord_Stats();
		}
		break;

		case MATCH_STATE_WAITEXIT:
		{
			if( advance_queue ) {
				G_Teams_AdvanceChallengersQueue();
				advance_queue = true;
			}

			gs.gameState.stats[GAMESTAT_MATCHSTATE] = MATCH_STATE_WAITEXIT;
			gs.gameState.stats[GAMESTAT_MATCHDURATION] = 3000;
			gs.gameState.stats[GAMESTAT_MATCHSTART] = game.serverTime;

			level.exitNow = false;
		}
		break;
	}

	// give the gametype the chance to setup for the new state

	if( game.asEngine != NULL ) {
		GT_asCallMatchStateStarted();
	}
}

/*
* G_Match_ScorelimitHit
*/
bool G_Match_ScorelimitHit( void ) {
	edict_t *e;

	if( GS_MatchState() != MATCH_STATE_PLAYTIME ) {
		return false;
	}

	if( g_scorelimit->integer ) {
		if( !GS_TeamBasedGametype() ) {
			for( e = game.edicts + 1; PLAYERNUM( e ) < gs.maxclients; e++ ) {
				if( !e->r.inuse ) {
					continue;
				}

				if( e->r.client->level.stats.score >= g_scorelimit->integer ) {
					return true;
				}
			}
		} else {
			int team;

			for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
				if( teamlist[team].stats.score >= g_scorelimit->integer ) {
					return true;
				}
			}
		}
	}

	return false;
}

/*
* G_Match_SuddenDeathFinished
*/
bool G_Match_SuddenDeathFinished( void ) {
	if( GS_MatchState() != MATCH_STATE_PLAYTIME ) {
		return false;
	}

	if( !GS_MatchExtended() || GS_MatchDuration() ) {
		return false;
	}

	return G_Match_Tied() ? false : true;
}

/*
* G_Match_TimelimitHit
*/
bool G_Match_TimelimitHit( void ) {
	// check for timelimit hit
	if( !GS_MatchDuration() || game.serverTime < GS_MatchEndTime() ) {
		return false;
	}

	if( GS_MatchState() == MATCH_STATE_WARMUP ) {
		level.forceStart = true; // force match starting when timelimit is up, even if someone goes unready

	}
	if( GS_MatchState() == MATCH_STATE_WAITEXIT ) {
		level.exitNow = true;
		return false; // don't advance into next state. The match will be restarted
	}

	return true;
}


static bool score_announcement_init = false;
static int last_leaders[MAX_CLIENTS];
static int leaders[MAX_CLIENTS];
#define G_ANNOUNCER_READYUP_DELAY 20000; // milliseconds

/*
* G_IsLeading
*/
static bool G_IsLeading( edict_t *ent ) {
	int num, i;

	if( GS_TeamBasedGametype() ) {
		num = ent->s.team;
	} else {
		num = PLAYERNUM( ent ) + 1;
	}

	for( i = 0; i < MAX_CLIENTS && leaders[i] != 0; i++ ) {
		if( leaders[i] == num ) {
			return true;
		}
	}

	return false;
}

/*
* G_WasLeading
*/
static bool G_WasLeading( edict_t *ent ) {
	int num, i;

	if( GS_TeamBasedGametype() ) {
		num = ent->s.team;
	} else {
		num = PLAYERNUM( ent ) + 1;
	}

	for( i = 0; i < MAX_CLIENTS && last_leaders[i] != 0; i++ ) {
		if( last_leaders[i] == num ) {
			return true;
		}
	}

	return false;
}

/*
* G_Match_ScoreAnnouncement
*/
static void G_Match_ScoreAnnouncement( void ) {
	int i;
	edict_t *e, *chased;
	int num_leaders, team;

	if( !level.gametype.scoreAnnouncementEnabled ) {
		return;
	}

	num_leaders = 0;
	memset( leaders, 0, sizeof( leaders ) );

	if( GS_TeamBasedGametype() ) {
		int score_max = -999999999;

		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			if( !teamlist[team].numplayers ) {
				continue;
			}

			if( teamlist[team].stats.score > score_max ) {
				score_max = teamlist[team].stats.score;
				leaders[0] = team;
				num_leaders = 1;
			} else if( teamlist[team].stats.score == score_max ) {
				leaders[num_leaders++] = team;
			}
		}
		leaders[num_leaders] = 0;
	} else {
		int score_max = -999999999;

		for( i = 0; i < MAX_CLIENTS && i < teamlist[TEAM_PLAYERS].numplayers; i++ ) {
			if( game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score > score_max ) {
				score_max = game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score;
				leaders[0] = teamlist[TEAM_PLAYERS].playerIndices[i];
				num_leaders = 1;
			} else if( game.clients[teamlist[TEAM_PLAYERS].playerIndices[i] - 1].level.stats.score == score_max ) {
				leaders[num_leaders++] = teamlist[TEAM_PLAYERS].playerIndices[i];
			}
		}
		leaders[num_leaders] = 0;
	}

	if( !score_announcement_init ) {
		// copy over to last_leaders
		memcpy( last_leaders, leaders, sizeof( leaders ) );
		score_announcement_init = true;
		return;
	}

	for( e = game.edicts + 1; PLAYERNUM( e ) < gs.maxclients; e++ ) {
		if( !e->r.client || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		if( e->r.client->resp.chase.active ) {
			chased = &game.edicts[e->r.client->resp.chase.target];
		} else {
			chased = e;
		}

		// floating spectator
		if( chased->s.team == TEAM_SPECTATOR ) {
			if( !GS_TeamBasedGametype() ) {
				continue;
			}

			if( last_leaders[1] == 0 && leaders[1] != 0 ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else if( leaders[1] == 0 && ( last_leaders[0] != leaders[0] || last_leaders[1] != 0 ) ) {
				//G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_1_to_4_TAKEN_LEAD_1_to_2,
				//	leaders[0]-1, ( rand()&1 )+1 ) ), GS_MAX_TEAMS, true, NULL );
			}
			continue;
		}

		// in the game or chasing someone who is
		if( G_WasLeading( chased ) && !G_IsLeading( chased ) ) {
			if( GS_TeamBasedGametype() && !GS_InvidualGameType() ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_LOST_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_LOST_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		} else if( ( !G_WasLeading( chased ) || ( last_leaders[1] != 0 ) ) && G_IsLeading( chased ) && ( leaders[1] == 0 ) ) {
			if( GS_TeamBasedGametype() && !GS_InvidualGameType() ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TAKEN_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TAKEN_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		} else if( ( !G_WasLeading( chased ) || ( last_leaders[1] == 0 ) ) && G_IsLeading( chased ) && ( leaders[1] != 0 ) ) {
			if( GS_TeamBasedGametype() && !GS_InvidualGameType() ) {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TEAM_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			} else {
				G_AnnouncerSound( e, trap_SoundIndex( va( S_ANNOUNCER_SCORE_TIED_LEAD_1_to_2, ( rand() & 1 ) + 1 ) ),
								  GS_MAX_TEAMS, true, NULL );
			}
		}
	}

	// copy over to last_leaders
	memcpy( last_leaders, leaders, sizeof( leaders ) );
}

/*
* G_Match_ReadyAnnouncement
*/
static void G_Match_ReadyAnnouncement( void ) {
	int i;
	edict_t *e;
	int team;
	bool readyupwarnings = false;
	int START_TEAM, END_TEAM;

	if( !level.gametype.readyAnnouncementEnabled ) {
		return;
	}

	// ready up announcements

	if( GS_TeamBasedGametype() ) {
		START_TEAM = TEAM_ALPHA;
		END_TEAM = GS_MAX_TEAMS;
	} else {
		START_TEAM = TEAM_PLAYERS;
		END_TEAM = TEAM_PLAYERS + 1;
	}

	for( team = START_TEAM; team < END_TEAM; team++ ) {
		if( !teamlist[team].numplayers ) {
			continue;
		}

		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			e = game.edicts + teamlist[team].playerIndices[i];
			if( e->r.svflags & SVF_FAKECLIENT ) {
				continue;
			}

			if( level.ready[teamlist[team].playerIndices[i] - 1] ) {
				readyupwarnings = true;
				break;
			}
		}
	}

	if( !readyupwarnings ) {
		return;
	}

	// now let's repeat and warn
	for( team = START_TEAM; team < END_TEAM; team++ ) {
		if( !teamlist[team].numplayers ) {
			continue;
		}
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			if( !level.ready[teamlist[team].playerIndices[i] - 1] ) {
				e = game.edicts + teamlist[team].playerIndices[i];
				if( !e->r.client || trap_GetClientState( PLAYERNUM( e ) ) != CS_SPAWNED ) {
					continue;
				}

				if( e->r.client->teamstate.readyUpWarningNext < game.realtime ) {
					e->r.client->teamstate.readyUpWarningNext = game.realtime + G_ANNOUNCER_READYUP_DELAY;
					e->r.client->teamstate.readyUpWarningCount++;
					if( e->r.client->teamstate.readyUpWarningCount > 3 ) {
						G_AnnouncerSound( e, trap_SoundIndex( S_ANNOUNCER_READY_UP_PISSEDOFF ), GS_MAX_TEAMS, true, NULL );
						e->r.client->teamstate.readyUpWarningCount = 0;
					} else {
						G_AnnouncerSound( e, trap_SoundIndex( S_ANNOUNCER_READY_UP_POLITE ), GS_MAX_TEAMS, true, NULL );
					}
				}
			}
		}
	}
}

/*
* G_EndMatch
*/
void G_EndMatch( void ) {
	level.forceExit = true;
	G_Match_LaunchState( MATCH_STATE_POSTMATCH );
}

/*
* G_Match_CheckReadys
*/
void G_Match_CheckReadys( void ) {
	edict_t *e;
	bool allready;
	int readys, notreadys, teamsready;
	int team, i;

	if( GS_MatchState() != MATCH_STATE_WARMUP && GS_MatchState() != MATCH_STATE_COUNTDOWN ) {
		return;
	}

	if( GS_MatchState() == MATCH_STATE_COUNTDOWN && level.forceStart ) {
		return; // never stop countdown if we have run out of warmup_timelimit

	}
	teamsready = 0;
	for( team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ ) {
		readys = notreadys = 0;
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			e = game.edicts + teamlist[team].playerIndices[i];

			if( !e->r.inuse ) {
				continue;
			}
			if( e->s.team == TEAM_SPECTATOR ) { //ignore spectators
				continue;
			}

			if( level.ready[PLAYERNUM( e )] ) {
				readys++;
			} else {
				notreadys++;
			}
		}
		if( !notreadys && readys ) {
			teamsready++;
		}
	}

	// everyone has commited
	if( GS_TeamBasedGametype() ) {
		if( teamsready == GS_MAX_TEAMS - TEAM_ALPHA ) {
			allready = true;
		} else {
			allready = false;
		}
	} else {   //ffa
		if( teamsready && teamlist[TEAM_PLAYERS].numplayers > 1 ) {
			allready = true;
		} else {
			allready = false;
		}
	}

	if( allready && GS_MatchState() != MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( NULL, "All players are ready. Match starting!\n" );
		G_Match_LaunchState( MATCH_STATE_COUNTDOWN );
	} else if( !allready && GS_MatchState() == MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( NULL, "Countdown aborted.\n" );
		G_CenterPrintMsg( NULL, "COUNTDOWN ABORTED" );
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
	}
}

/*
* G_Match_Ready
*/
void G_Match_Ready( edict_t *ent ) {
	if( ( ent->r.svflags & SVF_FAKECLIENT ) && level.ready[PLAYERNUM( ent )] ) {
		return;
	}

	if( ent->s.team == TEAM_SPECTATOR ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( GS_MatchState() != MATCH_STATE_WARMUP ) {
		if( !( ent->r.svflags & SVF_FAKECLIENT ) ) {
			G_PrintMsg( ent, "We're not in warmup.\n" );
		}
		return;
	}

	if( level.ready[PLAYERNUM( ent )] ) {
		G_PrintMsg( ent, "You are already ready.\n" );
		return;
	}

	level.ready[PLAYERNUM( ent )] = true;

	G_PrintMsg( NULL, "%s%s is ready!\n", ent->r.client->netname, S_COLOR_WHITE );

	G_Match_CheckReadys();
}

/*
* G_Match_NotReady
*/
void G_Match_NotReady( edict_t *ent ) {
	if( ent->s.team == TEAM_SPECTATOR ) {
		G_PrintMsg( ent, "Join the game first\n" );
		return;
	}

	if( GS_MatchState() != MATCH_STATE_WARMUP && GS_MatchState() != MATCH_STATE_COUNTDOWN ) {
		G_PrintMsg( ent, "A match is not being setup.\n" );
		return;
	}

	if( !level.ready[PLAYERNUM( ent )] ) {
		G_PrintMsg( ent, "You weren't ready.\n" );
		return;
	}

	level.ready[PLAYERNUM( ent )] = false;

	G_PrintMsg( NULL, "%s%s is no longer ready.\n", ent->r.client->netname, S_COLOR_WHITE );

	G_Match_CheckReadys();
}

/*
* G_Match_ToggleReady
*/
void G_Match_ToggleReady( edict_t *ent ) {
	if( !level.ready[PLAYERNUM( ent )] ) {
		G_Match_Ready( ent );
	} else {
		G_Match_NotReady( ent );
	}
}

/*
* G_Match_RemoveProjectiles
*/
void G_Match_RemoveProjectiles( edict_t *owner ) {
	edict_t *ent;

	for( ent = game.edicts + gs.maxclients; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( ent->r.inuse && !ent->r.client && ent->r.svflags & SVF_PROJECTILE && ent->r.solid != SOLID_NOT &&
			( owner == NULL || ent->r.owner->s.number == owner->s.number ) ) {
			G_FreeEdict( ent );
		}
	}
}

/*
* G_Match_FreeBodyQueue
*/
void G_Match_FreeBodyQueue( void ) {
	edict_t *ent;
	int i;

	ent = &game.edicts[gs.maxclients + 1];
	for( i = 0; i < BODY_QUEUE_SIZE; ent++, i++ ) {
		if( !ent->r.inuse ) {
			continue;
		}

		if( ent->classname && !Q_stricmp( ent->classname, "body" ) ) {
			GClip_UnlinkEntity( ent );

			ent->deadflag = DEAD_NO;
			ent->movetype = MOVETYPE_NONE;
			ent->r.solid = SOLID_NOT;
			ent->r.svflags = SVF_NOCLIENT;

			ent->s.type = ET_GENERIC;
			ent->s.skinnum = 0;
			ent->s.frame = 0;
			ent->s.modelindex = 0;
			ent->s.sound = 0;
			ent->s.effects = 0;

			ent->takedamage = DAMAGE_NO;
			ent->flags |= FL_NO_KNOCKBACK;

			GClip_LinkEntity( ent );
		}
	}

	level.body_que = 0;
}


//======================================================
//		Game types
//======================================================

/*
* G_Gametype_CanPickUpItem
*/
bool G_Gametype_CanPickUpItem( const gsitem_t *item ) {
	if( !item ) {
		return false;
	}

	return ( item->type & level.gametype.pickableItemsMask ) ? true : false;
}

/*
* G_Gametype_CanSpawnItem
*/
bool G_Gametype_CanSpawnItem( const gsitem_t *item ) {
	if( !item ) {
		return false;
	}

	return ( level.gametype.spawnableItemsMask & item->type ) ? true : false;
}

/*
* G_Gametype_CanRespawnItem
*/
bool G_Gametype_CanRespawnItem( const gsitem_t *item ) {
	int itemmask;

	if( !item ) {
		return false;
	}

	itemmask = level.gametype.respawnableItemsMask;

	return ( ( itemmask & item->type ) != 0 ) ? true : false;
}

/*
* G_Gametype_CanDropItem
*/
bool G_Gametype_CanDropItem( const gsitem_t *item, bool ignoreMatchState ) {
	int itemmask;

	if( !item ) {
		return false;
	}

	if( !ignoreMatchState ) {
		if( GS_MatchState() > MATCH_STATE_PLAYTIME ) {
			return false;
		}
	}

	itemmask = level.gametype.dropableItemsMask;

	return ( itemmask & item->type ) ? true : false;
}

/*
* G_Gametype_CanTeamDamage
*/
bool G_Gametype_CanTeamDamage( int damageflags ) {
	if( damageflags & DAMAGE_NO_PROTECTION ) {
		return true;
	}

	if( !GS_TeamBasedGametype() ) {
		return true;
	}

	return g_allow_teamdamage->integer ? true : false;
}

/*
* G_Gametype_RespawnTimeForItem
*/
int G_Gametype_RespawnTimeForItem( const gsitem_t *item ) {
	if( !item ) {
		return -1; // free the edict

	}
	if( item->type & IT_AMMO ) {
		return level.gametype.ammo_respawn * 1000;
	}

	if( item->type & IT_WEAPON ) {
		return level.gametype.weapon_respawn * 1000;
	}

	if( item->tag == HEALTH_MEGA ) {
		return level.gametype.megahealth_respawn * 1000;
	}

	if( item->tag == HEALTH_ULTRA ) {
		return level.gametype.ultrahealth_respawn * 1000;
	}

	if( item->type & IT_HEALTH ) {
		return level.gametype.health_respawn * 1000;
	}

	if( item->type & IT_POWERUP ) {
		return level.gametype.powerup_respawn * 1000;
	}

	return item->quantity * 1000;
}

/*
* G_Gametype_DroppedItemTimeout
*/
int G_Gametype_DroppedItemTimeout( const gsitem_t *item ) {
	// to do: add cvar
	return 29;
}

/*
* G_EachNewSecond
*/
static bool G_EachNewSecond( void ) {
	static int lastsecond;
	static int second;

	second = (int)( level.time * 0.001 );
	if( lastsecond == second ) {
		return false;
	}

	lastsecond = second;
	return true;
}

/*
* G_CheckNumBots
*/
static void G_CheckNumBots( void ) {
	edict_t *ent;
	int desiredNumBots;

	if( level.spawnedTimeStamp + 5000 > game.realtime ) {
		return;
	}

	// check sanity of g_numbots
	if( g_numbots->integer < 0 ) {
		trap_Cvar_Set( "g_numbots", "0" );
	}

	if( g_numbots->integer > gs.maxclients ) {
		trap_Cvar_Set( "g_numbots", va( "%i", gs.maxclients ) );
	}

	if( level.gametype.numBots > gs.maxclients ) {
		level.gametype.numBots = gs.maxclients;
	}

	desiredNumBots = level.gametype.numBots ? level.gametype.numBots : g_numbots->integer;

	if( desiredNumBots < game.numBots ) {
		// kick one bot
		for( ent = game.edicts + gs.maxclients; PLAYERNUM( ent ) >= 0; ent-- ) {
			if( !ent->r.inuse || !( ent->r.svflags & SVF_FAKECLIENT ) ) {
				continue;
			}
			AI_RemoveBot( ent->r.client->netname );
			break;
		}
		return;
	}

	if( desiredNumBots > game.numBots ) { // add a bot if there is room
		for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients && game.numBots < desiredNumBots; ent++ ) {
			if( !ent->r.inuse && trap_GetClientState( PLAYERNUM( ent ) ) == CS_FREE ) {
				AI_SpawnBot( NULL );
			}
		}
	}
}

/*
* G_TickOutPowerUps
*/
static void G_TickOutPowerUps( void ) {
	edict_t *ent;
	const gsitem_t *item;
	int i;

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ ) {
		if( ent->r.inuse && trap_GetClientState( PLAYERNUM( ent ) ) >= CS_SPAWNED ) {
			for( i = POWERUP_QUAD; i < POWERUP_TOTAL; i++ ) {
				item = GS_FindItemByTag( i );
				if( item && item->quantity && ent->r.client->ps.inventory[item->tag] > 0 ) {
					ent->r.client->ps.inventory[item->tag]--;
				}
			}
		}
	}

	// also tick out dropped powerups
	for( ent = game.edicts + gs.maxclients + BODY_QUEUE_SIZE; ENTNUM( ent ) < game.numentities; ent++ ) {
		if( !ent->r.inuse || !ent->item ) {
			continue;
		}

		if( !( ent->item->type & IT_POWERUP ) ) {
			continue;
		}

		if( ent->spawnflags & DROPPED_ITEM ) {
			ent->count--;
			if( ent->count <= 0 ) {
				G_FreeEdict( ent );
				continue;
			}
		}
	}
}

/*
* G_EachNewMinute
*/
static bool G_EachNewMinute( void ) {
	static int lastminute;
	static int minute;

	minute = (int)( level.time * 0.001 / 60.0f );
	if( lastminute == minute ) {
		return false;
	}

	lastminute = minute;
	return true;
}

/*
* G_CheckEvenTeam
*/
static void G_CheckEvenTeam( void ) {
	int max = 0;
	int min = gs.maxclients + 1;
	int uneven_team = TEAM_SPECTATOR;
	int i;

	if( GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		return;
	}

	if( !GS_TeamBasedGametype() ) {
		return;
	}

	if( g_teams_allow_uneven->integer ) {
		return;
	}

	for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		if( max < teamlist[i].numplayers ) {
			max = teamlist[i].numplayers;
			uneven_team = i;
		}
		if( min > teamlist[i].numplayers ) {
			min = teamlist[i].numplayers;
		}
	}

	if( max - min > 1 ) {
		for( i = 0; i < teamlist[uneven_team].numplayers; i++ ) {
			edict_t *e = game.edicts + teamlist[uneven_team].playerIndices[i];
			if( !e->r.inuse ) {
				continue;
			}
			G_CenterPrintMsg( e, "Teams are uneven. Please switch into another team." ); // FIXME: need more suitable message :P
			G_PrintMsg( e, "%sTeams are uneven. Please switch into another team.\n", S_COLOR_CYAN ); // FIXME: need more suitable message :P
		}

		// FIXME: switch team forcibly?
	}
}

/*
* G_Gametype_ScoreEvent
*/
void G_Gametype_ScoreEvent( gclient_t *client, const char *score_event, const char *args ) {
	if( !score_event || !score_event[0] ) {
		return;
	}

	if( game.asEngine != NULL ) {
		GT_asCallScoreEvent( client, score_event, args );
	}
}

/*
* G_RunGametype
*/
void G_RunGametype( void ) {
	G_Teams_ExecuteChallengersQueue();
	G_Teams_UpdateMembersList();
	G_Match_CheckStateAbort();

	G_UpdateScoreBoardMessages();

	//check gametype specific rules
	if( game.asEngine != NULL ) {
		GT_asCallThinkRules();
	}

	if( G_EachNewSecond() ) {
		G_CheckNumBots();
		G_TickOutPowerUps();
	}

	if( G_EachNewMinute() ) {
		G_CheckEvenTeam();
	}

	G_Match_ScoreAnnouncement();
	G_Match_ReadyAnnouncement();

	G_asGarbageCollect( false );
}

//======================================================
//		Game type registration
//======================================================

/*
* G_Gametype_SetDefaults
*/
void G_Gametype_SetDefaults( void ) {
	level.gametype.spawnableItemsMask = ( IT_WEAPON | IT_AMMO | IT_POWERUP | IT_HEALTH );
	level.gametype.respawnableItemsMask = level.gametype.spawnableItemsMask;
	level.gametype.dropableItemsMask = level.gametype.spawnableItemsMask;
	level.gametype.pickableItemsMask = level.gametype.spawnableItemsMask;

	level.gametype.isTeamBased = false;
	level.gametype.isRace = false;
	level.gametype.hasChallengersQueue = false;
	level.gametype.hasChallengersRoulette = false;
	level.gametype.maxPlayersPerTeam = 0;

	level.gametype.ammo_respawn = 20;
	level.gametype.weapon_respawn = 5;
	level.gametype.health_respawn = 15;
	level.gametype.powerup_respawn = 90;
	level.gametype.megahealth_respawn = 20;
	level.gametype.ultrahealth_respawn = 40;

	level.gametype.readyAnnouncementEnabled = false;
	level.gametype.scoreAnnouncementEnabled = false;
	level.gametype.countdownEnabled = false;
	level.gametype.matchAbortDisabled = false;
	level.gametype.shootingDisabled = false;
	level.gametype.infiniteAmmo = false;
	level.gametype.canForceModels = true;
	level.gametype.customDeadBodyCam = false;
	level.gametype.removeInactivePlayers = true;
	level.gametype.disableObituaries = false;

	level.gametype.spawnpointRadius = 64;

	level.gametype.numBots = 0;
}

/*
* G_Gametype_Init
*/
void G_Gametype_Init( void ) {
	bool changed = false;

	if( !g_gametype ) { // first time initialized
		changed = true;
	}

	g_gametype = trap_Cvar_Get( "g_gametype", "bomb", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH | CVAR_READONLY );

	// get the match cvars too
	g_warmup_timelimit = trap_Cvar_Get( "g_warmup_timelimit", "5", CVAR_ARCHIVE );
	g_postmatch_timelimit = trap_Cvar_Get( "g_postmatch_timelimit", "4", CVAR_ARCHIVE );
	g_match_extendedtime = trap_Cvar_Get( "g_match_extendedtime", "2", CVAR_ARCHIVE );

	// game settings
	g_scorelimit = trap_Cvar_Get( "g_scorelimit", "10", CVAR_ARCHIVE );
	g_allow_teamdamage = trap_Cvar_Get( "g_allow_teamdamage", "0", CVAR_ARCHIVE | CVAR_READONLY );

	G_Printf( "-------------------------------------\n" );
	G_Printf( "Initalizing '%s' gametype\n", g_gametype->string );

	if( changed ) {
		const char *configs_path = "configs/server/gametypes/";

		G_InitChallengersQueue();

		// print a hint for admins so they know there's a chance to execute a
		// config here, but don't show it as an error, because it isn't
		G_Printf( "loading %s%s.cfg\n", configs_path, g_gametype->string );
		trap_Cmd_ExecuteText( EXEC_NOW, va( "exec %s%s.cfg silent\n", configs_path, g_gametype->string ) );
		trap_Cbuf_Execute();

		// on a listen server, override gametype-specific settings in config
		trap_Cmd_ExecuteText( EXEC_NOW, "vstr ui_startservercmd\n" );
		trap_Cbuf_Execute();
	}

	G_CheckCvars();

	G_Gametype_SetDefaults();

	// Init the current gametype
	if( !GT_asLoadScript( g_gametype->string ) ) {
		G_Gametype_GENERIC_Init();
	}

	trap_ConfigString( CS_GAMETYPENAME, g_gametype->string );
}
