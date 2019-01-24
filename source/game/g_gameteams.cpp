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


//==========================================================
//					Teams
//==========================================================

cvar_t *g_teams_maxplayers;
cvar_t *g_teams_allow_uneven;

/*
* G_Teams_Init
*/
void G_Teams_Init( void ) {
	edict_t *ent;

	// set the team names with default ones
	trap_ConfigString( CS_TEAM_SPECTATOR_NAME, GS_DefaultTeamName( TEAM_SPECTATOR ) );
	trap_ConfigString( CS_TEAM_PLAYERS_NAME, GS_DefaultTeamName( TEAM_PLAYERS ) );
	trap_ConfigString( CS_TEAM_ALPHA_NAME, GS_DefaultTeamName( TEAM_ALPHA ) );
	trap_ConfigString( CS_TEAM_BETA_NAME, GS_DefaultTeamName( TEAM_BETA ) );

	g_teams_maxplayers = trap_Cvar_Get( "g_teams_maxplayers", "0", CVAR_ARCHIVE );
	g_teams_allow_uneven = trap_Cvar_Get( "g_teams_allow_uneven", "1", CVAR_ARCHIVE );

	//unlock all teams and clear up team lists
	memset( teamlist, 0, sizeof( teamlist ) );

	for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ ) {
		if( ent->r.inuse ) {
			memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
			memset( &ent->r.client->resp, 0, sizeof( ent->r.client->resp ) );
			ent->s.team = ent->r.client->team = TEAM_SPECTATOR;
			G_GhostClient( ent );
			ent->movetype = MOVETYPE_NOCLIP; // allow freefly
			ent->r.client->teamstate.timeStamp = level.time;
			ent->r.client->resp.timeStamp = level.time;
		}
	}

}

static int G_Teams_CompareMembers( const void *a, const void *b ) {
	edict_t *edict_a = game.edicts + *(int *)a;
	edict_t *edict_b = game.edicts + *(int *)b;
	int score_a = edict_a->r.client->level.stats.score;
	int score_b = edict_b->r.client->level.stats.score;
	int result = score_b - score_a;
	if( !result ) {
		result = Q_stricmp( edict_a->r.client->netname, edict_b->r.client->netname );
	}
	if( !result ) {
		result = ENTNUM( edict_a ) - ENTNUM( edict_b );
	}
	return result;
}

/*
* G_Teams_UpdateMembersList
* It's better to count the list in detail once per fame, than
* creating a quick list each time we need it.
*/
void G_Teams_UpdateMembersList( void ) {
	edict_t *ent;
	int i, team;

	for( team = TEAM_SPECTATOR; team < GS_MAX_TEAMS; team++ ) {
		teamlist[team].numplayers = 0;
		teamlist[team].ping = 0;

		//create a temp list with the clients inside this team
		for( i = 0, ent = game.edicts + 1; i < gs.maxclients; i++, ent++ ) {
			if( !ent->r.client || ( trap_GetClientState( PLAYERNUM( ent ) ) < CS_CONNECTED ) ) {
				continue;
			}

			if( ent->s.team == team ) {
				teamlist[team].playerIndices[teamlist[team].numplayers++] = ENTNUM( ent );
			}
		}

		qsort( teamlist[team].playerIndices, teamlist[team].numplayers, sizeof( teamlist[team].playerIndices[0] ), G_Teams_CompareMembers );

		if( teamlist[team].numplayers ) {
			for( i = 0; i < teamlist[team].numplayers; i++ )
				teamlist[team].ping += game.edicts[teamlist[team].playerIndices[i]].r.client->r.ping;
			teamlist[team].ping /= teamlist[team].numplayers;
		}
	}
}

/*
* G_Teams_TeamIsLocked
*/
bool G_Teams_TeamIsLocked( int team ) {
	if( team > TEAM_SPECTATOR && team < GS_MAX_TEAMS ) {
		return teamlist[team].locked;
	} else {
		return false;
	}
}

/*
* G_Teams_LockTeam
*/
bool G_Teams_LockTeam( int team ) {
	if( team <= TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return false;
	}

	if( !level.teamlock || teamlist[team].locked ) {
		return false;
	}

	teamlist[team].locked = true;
	return true;
}

/*
* G_Teams_UnLockTeam
*/
bool G_Teams_UnLockTeam( int team ) {
	if( team <= TEAM_SPECTATOR || team >= GS_MAX_TEAMS ) {
		return false;
	}

	if( !teamlist[team].locked ) {
		return false;
	}

	teamlist[team].locked = false;
	return true;
}

/*
* G_Teams_SetTeam - sets clients team without any checking
*/
void G_Teams_SetTeam( edict_t *ent, int team ) {
	assert( ent && ent->r.inuse && ent->r.client );
	assert( team >= TEAM_SPECTATOR && team < GS_MAX_TEAMS );

	if( ent->r.client->team != TEAM_SPECTATOR && team != TEAM_SPECTATOR ) {
		// keep scores when switching between non-spectating teams
		int64_t timeStamp = ent->r.client->teamstate.timeStamp;
		memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
		ent->r.client->teamstate.timeStamp = timeStamp;
	} else {
		// clear scores at changing team
		memset( &ent->r.client->level.stats, 0, sizeof( ent->r.client->level.stats ) );
		memset( &ent->r.client->teamstate, 0, sizeof( ent->r.client->teamstate ) );
		ent->r.client->teamstate.timeStamp = level.time;
	}

	ent->r.client->team = team;

	G_ClientRespawn( ent, true ); // make ghost using G_ClientRespawn so team is updated at ghosting
	G_SpawnQueue_AddClient( ent );

	level.ready[PLAYERNUM( ent )] = false;

	G_Match_CheckReadys();
}

enum
{
	ER_TEAM_OK,
	ER_TEAM_INVALID,
	ER_TEAM_FULL,
	ER_TEAM_LOCKED,
	ER_TEAM_MATCHSTATE,
	ER_TEAM_CHALLENGERS,
	ER_TEAM_UNEVEN
};

static bool G_Teams_CanKeepEvenTeam( int leaving, int joining ) {
	int max = 0;
	int min = gs.maxclients + 1;
	int numplayers;
	int i;

	for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
		numplayers = teamlist[i].numplayers;
		if( i == leaving ) {
			numplayers--;
		}
		if( i == joining ) {
			numplayers++;
		}

		if( max < numplayers ) {
			max = numplayers;
		}
		if( min > numplayers ) {
			min = numplayers;
		}
	}

	return teamlist[joining].numplayers + 1 == min || abs( max - min ) <= 1;
}

/*
* G_GameTypes_DenyJoinTeam
*/
static int G_GameTypes_DenyJoinTeam( edict_t *ent, int team ) {
	if( team < 0 || team >= GS_MAX_TEAMS ) {
		G_Printf( "WARNING: 'G_GameTypes_CanJoinTeam' parsing a unrecognized team value\n" );
		return ER_TEAM_INVALID;
	}

	if( team == TEAM_SPECTATOR ) {
		return ER_TEAM_OK;
	}

	if( GS_MatchState() > MATCH_STATE_PLAYTIME ) {
		return ER_TEAM_MATCHSTATE;
	}

	// waiting for chanllengers queue to be executed
	if( GS_HasChallengers() && game.realtime < level.spawnedTimeStamp + (int64_t)( G_CHALLENGERS_MIN_JOINTEAM_MAPTIME + game.snapFrameTime ) ) {
		return ER_TEAM_CHALLENGERS;
	}

	// force eveyone to go through queue so things work on map change
	if( GS_HasChallengers() && !ent->r.client->queueTimeStamp ) {
		return ER_TEAM_CHALLENGERS;
	}

	// see if team is locked
	if( G_Teams_TeamIsLocked( team ) ) {
		return ER_TEAM_LOCKED;
	}

	if( !GS_TeamBasedGametype() ) {
		return team == TEAM_PLAYERS ? ER_TEAM_OK : ER_TEAM_INVALID;
	}

	if( team != TEAM_ALPHA && team != TEAM_BETA )
		return ER_TEAM_INVALID;

	// see if team is full
	int count = teamlist[team].numplayers;

	if( ( count + 1 > level.gametype.maxPlayersPerTeam &&
		  level.gametype.maxPlayersPerTeam > 0 ) ||
		( count + 1 > g_teams_maxplayers->integer &&
		  g_teams_maxplayers->integer > 0 ) ) {
		return ER_TEAM_FULL;
	}

	if( !g_teams_allow_uneven->integer && !G_Teams_CanKeepEvenTeam( ent->s.team, team ) ) {
		return ER_TEAM_UNEVEN;
	}

	return ER_TEAM_OK;
}

/*
* G_Teams_JoinTeam - checks that client can join the given team and then joins it
*/
bool G_Teams_JoinTeam( edict_t *ent, int team ) {
	int error;

	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	if( !ent->r.client ) {
		return false;
	}

	if( ( error = G_GameTypes_DenyJoinTeam( ent, team ) ) ) {
		if( error == ER_TEAM_INVALID ) {
			G_PrintMsg( ent, "Can't join %s\n", GS_TeamName( team ) );
		} else if( error == ER_TEAM_CHALLENGERS ) {
			G_Teams_JoinChallengersQueue( ent );
		} else if( error == ER_TEAM_FULL ) {
			G_PrintMsg( ent, "Team %s is FULL\n", GS_TeamName( team ) );
			G_Teams_JoinChallengersQueue( ent );
		} else if( error == ER_TEAM_LOCKED ) {
			G_PrintMsg( ent, "Team %s is LOCKED\n", GS_TeamName( team ) );
			G_Teams_JoinChallengersQueue( ent );
		} else if( error == ER_TEAM_MATCHSTATE ) {
			G_PrintMsg( ent, "Can't join %s at this moment\n", GS_TeamName( team ) );
		} else if( error == ER_TEAM_UNEVEN ) {
			G_PrintMsg( ent, "Can't join %s because of uneven teams\n", GS_TeamName( team ) ); // FIXME: need more suitable message :P
			G_Teams_JoinChallengersQueue( ent );
		}
		return false;
	}

	//ok, can join, proceed
	G_Teams_SetTeam( ent, team );
	return true;
}

/*
* G_Teams_JoinAnyTeam - find us a team since we are too lazy to do ourselves
*/
bool G_Teams_JoinAnyTeam( edict_t *ent, bool silent ) {
	int best_numplayers = gs.maxclients + 1, best_score = 999999;
	int i, team = -1;
	bool wasinqueue = ( ent->r.client->queueTimeStamp != 0 );

	G_Teams_UpdateMembersList(); // make sure we have up-to-date data

	//depending on the gametype, of course
	if( !GS_TeamBasedGametype() ) {
		if( ent->s.team == TEAM_PLAYERS ) {
			if( !silent ) {
				G_PrintMsg( ent, "You are already in %s team\n", GS_TeamName( TEAM_PLAYERS ) );
			}
			return false;
		}
		if( G_Teams_JoinTeam( ent, TEAM_PLAYERS ) ) {
			if( !silent ) {
				G_PrintMsg( NULL, "%s%s joined the %s team.\n",
							ent->r.client->netname, S_COLOR_WHITE, GS_TeamName( ent->s.team ) );
			}
		}
		return true;

	} else {   //team based

		//find the available team with smaller player count or worse score
		for( i = TEAM_ALPHA; i < GS_MAX_TEAMS; i++ ) {
			if( G_GameTypes_DenyJoinTeam( ent, i ) ) {
				continue;
			}

			if( team == -1 || teamlist[i].numplayers < best_numplayers
				|| ( teamlist[i].numplayers == best_numplayers && teamlist[i].stats.score < best_score ) ) {
				best_numplayers = teamlist[i].numplayers;
				best_score = teamlist[i].stats.score;
				team = i;
			}
		}

		if( team == ent->s.team ) { // he is at the right team
			if( !silent ) {
				G_PrintMsg( ent, "%sCouldn't find a better team than team %s.\n",
							S_COLOR_WHITE, GS_TeamName( ent->s.team ) );
			}
			return false;
		}

		if( team != -1 ) {
			if( G_Teams_JoinTeam( ent, team ) ) {
				if( !silent ) {
					G_PrintMsg( NULL, "%s%s joined the %s team.\n",
								ent->r.client->netname, S_COLOR_WHITE, GS_TeamName( ent->s.team ) );
				}
				return true;
			}
		}
		if( GS_MatchState() <= MATCH_STATE_PLAYTIME && !silent ) {
			G_Teams_JoinChallengersQueue( ent );
		}
	}

	// don't print message if we joined the queue
	if( !silent && ( !GS_HasChallengers() || wasinqueue || !ent->r.client->queueTimeStamp ) ) {
		G_PrintMsg( ent, "You can't join the game now\n" );
	}

	return false;
}

/*
* G_Teams_Join_Cmd
*/
void G_Teams_Join_Cmd( edict_t *ent ) {
	char *t;
	int team;

	if( !ent->r.client || trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	t = trap_Cmd_Argv( 1 );
	if( !t || *t == 0 ) {
		G_Teams_JoinAnyTeam( ent, false );
		return;
	}

	team = GS_TeamFromName( t );
	if( team != -1 ) {
		if( team == TEAM_SPECTATOR ) { // special handling for spectator team
			Cmd_Spec_f( ent );
			return;
		}
		if( team == ent->s.team ) {
			G_PrintMsg( ent, "You are already in %s team\n", GS_TeamName( team ) );
			return;
		}
		if( G_Teams_JoinTeam( ent, team ) ) {
			G_PrintMsg( NULL, "%s%s joined the %s%s team.\n", ent->r.client->netname, S_COLOR_WHITE,
						GS_TeamName( ent->s.team ), S_COLOR_WHITE );
			return;
		}
	} else {
		G_PrintMsg( ent, "No such team.\n" );
		return;
	}
}

//======================================================================
//
// CHALLENGERS QUEUE
//
//======================================================================

static int G_Teams_ChallengersQueueCmp( const edict_t **pe1, const edict_t **pe2 ) {
	const edict_t *e1 = *pe1, *e2 = *pe2;

	if( e1->r.client->queueTimeStamp > e2->r.client->queueTimeStamp ) {
		return 1;
	}
	if( e2->r.client->queueTimeStamp > e1->r.client->queueTimeStamp ) {
		return -1;
	}
	return rand() & 1 ? -1 : 1;
}

/*
* G_Teams_ChallengersQueue
*
* Returns a NULL-terminated list of challengers or NULL if
* there are no challengers.
*/
edict_t **G_Teams_ChallengersQueue( void ) {
	int num_challengers = 0;
	static edict_t *challengers[MAX_CLIENTS + 1];
	edict_t *e;
	gclient_t *cl;

	// fill the challengers into array, then sort
	for( e = game.edicts + 1; PLAYERNUM( e ) < gs.maxclients; e++ ) {
		if( !e->r.inuse || !e->r.client || e->s.team != TEAM_SPECTATOR ) {
			continue;
		}
		if( trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
			continue;
		}

		cl = e->r.client;
		if( cl->connecting || !cl->queueTimeStamp ) {
			continue;
		}

		challengers[num_challengers++] = e;
	}

	if( !num_challengers ) {
		return NULL;
	}

	// NULL terminator
	challengers[num_challengers] = NULL;

	// sort challengers by the queue time in ascending order
	if( num_challengers > 1 ) {
		qsort( challengers, num_challengers, sizeof( *challengers ), ( int ( * )( const void *, const void * ) )G_Teams_ChallengersQueueCmp );
	}

	return challengers;
}

/*
* G_Teams_ExecuteChallengersQueue
*/
void G_Teams_ExecuteChallengersQueue( void ) {
	edict_t *ent;
	edict_t **challengers;
	bool restartmatch = false;

	// Medar fixme: this is only really makes sense, if playerlimit per team is one
	if( GS_MatchState() == MATCH_STATE_PLAYTIME ) {
		return;
	}

	if( !GS_HasChallengers() ) {
		return;
	}

	if( game.realtime < level.spawnedTimeStamp + G_CHALLENGERS_MIN_JOINTEAM_MAPTIME ) {
		static int time, lasttime;
		time = (int)( ( G_CHALLENGERS_MIN_JOINTEAM_MAPTIME - ( game.realtime - level.spawnedTimeStamp ) ) * 0.001 );
		if( lasttime && time == lasttime ) {
			return;
		}
		lasttime = time;
		if( lasttime ) {
			G_CenterPrintFormatMsg( NULL, 1, "Waiting... %s", va( "%i", lasttime ) );
		}
		return;
	}

	// pick players in join order and try to put them in the
	// game until we get the first refused one.
	challengers = G_Teams_ChallengersQueue();
	if( challengers ) {
		int i;

		for( i = 0; challengers[i]; i++ ) {
			ent = challengers[i];
			if( !G_Teams_JoinAnyTeam( ent, true ) ) {
				break;
			}

			// if we successfully execute the challengers queue during the countdown, revert to warmup
			if( GS_MatchState() == MATCH_STATE_COUNTDOWN ) {
				restartmatch = true;
			}
		}
	}

	if( restartmatch == true ) {
		G_Match_Autorecord_Cancel();
		G_Match_LaunchState( MATCH_STATE_WARMUP );
	}
}

/*
*
* G_Teams_BestScoreBelow
*/
static edict_t *G_Teams_BestScoreBelow( int maxscore ) {
	int team, i;
	edict_t *e, *best = NULL;
	int bestScore = -9999999;

	if( GS_TeamBasedGametype() ) {
		for( team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ ) {
			for( i = 0; i < teamlist[team].numplayers; i++ ) {
				e = game.edicts + teamlist[team].playerIndices[i];
				if( e->r.client->level.stats.score > bestScore &&
					e->r.client->level.stats.score <= maxscore
					&& !e->r.client->queueTimeStamp ) {
					bestScore = e->r.client->level.stats.score;
					best = e;
				}
			}
		}
	} else {
		for( i = 0; i < teamlist[TEAM_PLAYERS].numplayers; i++ ) {
			e = game.edicts + teamlist[TEAM_PLAYERS].playerIndices[i];
			if( e->r.client->level.stats.score > bestScore &&
				e->r.client->level.stats.score <= maxscore
				&& !e->r.client->queueTimeStamp ) {
				bestScore = e->r.client->level.stats.score;
				best = e;
			}
		}
	}

	return best;
}

/*
* G_Teams_AdvanceChallengersQueue
*/
void G_Teams_AdvanceChallengersQueue( void ) {
	int i, team, loserscount, winnerscount, playerscount = 0;
	int maxscore = 999999;
	edict_t *won, *e;
	int START_TEAM = TEAM_PLAYERS, END_TEAM = TEAM_PLAYERS + 1;

	if( !GS_HasChallengers() ) {
		return;
	}

	G_Teams_UpdateMembersList();

	if( GS_TeamBasedGametype() ) {
		START_TEAM = TEAM_ALPHA;
		END_TEAM = GS_MAX_TEAMS;
	}

	// assign new timestamps to all the players inside teams
	for( team = START_TEAM; team < END_TEAM; team++ ) {
		playerscount += teamlist[team].numplayers;
	}

	if( !playerscount ) {
		return;
	}

	loserscount = 0;
	if( playerscount > 1 ) {
		loserscount = (int)( playerscount / 2 );
	}
	winnerscount = playerscount - loserscount;

	// put everyone who just played out of the challengers queue
	for( team = START_TEAM; team < END_TEAM; team++ ) {
		for( i = 0; i < teamlist[team].numplayers; i++ ) {
			e = game.edicts + teamlist[team].playerIndices[i];
			e->r.client->queueTimeStamp = 0;
		}
	}

	if( !level.gametype.hasChallengersRoulette ) {
		// put (back) the best scoring players in first positions of challengers queue
		for( i = 0; i < winnerscount; i++ ) {
			won = G_Teams_BestScoreBelow( maxscore );
			if( won ) {
				maxscore = won->r.client->level.stats.score;
				won->r.client->queueTimeStamp = 1 + ( winnerscount - i ); // never have 2 players with the same timestamp
			}
		}
	}
}

/*
* G_Teams_LeaveChallengersQueue
*/
void G_Teams_LeaveChallengersQueue( edict_t *ent ) {
	if( !GS_HasChallengers() ) {
		ent->r.client->queueTimeStamp = 0;
		return;
	}

	if( ent->s.team != TEAM_SPECTATOR ) {
		return;
	}

	// exit the challengers queue
	if( ent->r.client->queueTimeStamp ) {
		ent->r.client->queueTimeStamp = 0;
		G_PrintMsg( ent, "%sYou left the challengers queue\n", S_COLOR_CYAN );
	}
}

/*
* G_Teams_JoinChallengersQueue
*/
void G_Teams_JoinChallengersQueue( edict_t *ent ) {
	int pos = 0;
	edict_t *e;

	if( !GS_HasChallengers() ) {
		ent->r.client->queueTimeStamp = 0;
		return;
	}

	if( ent->s.team != TEAM_SPECTATOR ) {
		return;
	}

	// enter the challengers queue
	if( !ent->r.client->queueTimeStamp ) {  // enter the line
		ent->r.client->queueTimeStamp = game.realtime;
		for( e = game.edicts + 1; PLAYERNUM( e ) < gs.maxclients; e++ ) {
			if( !e->r.inuse || !e->r.client || trap_GetClientState( PLAYERNUM( e ) ) < CS_SPAWNED ) {
				continue;
			}
			if( !e->r.client->queueTimeStamp || e->s.team != TEAM_SPECTATOR ) {
				continue;
			}

			// if there are other players with the same timestamp, increase ours
			if( e->r.client->queueTimeStamp >= ent->r.client->queueTimeStamp ) {
				ent->r.client->queueTimeStamp = e->r.client->queueTimeStamp + 1;
			}
			if( e->r.client->queueTimeStamp < ent->r.client->queueTimeStamp ) {
				pos++;
			}
		}

		G_PrintMsg( ent, "%sYou entered the challengers queue in position %i\n", S_COLOR_CYAN, pos + 1 );
	}
}

void G_InitChallengersQueue( void ) {
	int i;

	for( i = 0; i < gs.maxclients; i++ )
		game.clients[i].queueTimeStamp = 0;
}

//======================================================================
//
//TEAM COMMUNICATIONS
//
//======================================================================

void G_Say_Team( edict_t *who, const char *inmsg, bool checkflood ) {
	char *msg;
	char msgbuf[256];
	char outmsg[256];
	char *p;
	char current_color[3];

	if( who->s.team != TEAM_SPECTATOR && ( !GS_TeamBasedGametype() || GS_InvidualGameType() ) ) {
		Cmd_Say_f( who, false, true );
		return;
	}

	if( checkflood ) {
		if( CheckFlood( who, true ) ) {
			return;
		}
	}

	Q_strncpyz( msgbuf, inmsg, sizeof( msgbuf ) );

	msg = msgbuf;
	if( *msg == '\"' ) {
		msg[strlen( msg ) - 1] = 0;
		msg++;
	}

	if( who->s.team == TEAM_SPECTATOR ) {
		// if speccing, also check for non-team flood
		if( checkflood ) {
			if( CheckFlood( who, false ) ) {
				return;
			}
		}

		G_ChatMsg( NULL, who, true, "%s", msg );
		return;
	}

	Q_strncpyz( current_color, S_COLOR_WHITE, sizeof( current_color ) );

	memset( outmsg, 0, sizeof( outmsg ) );

	for( p = outmsg; *msg && (size_t)( p - outmsg ) < sizeof( outmsg ) - 3; msg++ ) {
		if( *msg == '^' ) {
			*p++ = *msg++;
			*p++ = *msg;
			Q_strncpyz( current_color, p - 2, sizeof( current_color ) );
		} else {
			*p++ = *msg;
		}
	}
	*p = 0;

	G_ChatMsg( NULL, who, true, "%s", outmsg );
}
