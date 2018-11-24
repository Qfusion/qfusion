/*
Copyright (C) 2009-2010 Chasseur de bots

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

/*enum eRoundStates FIXME enum
{
	ROUNDSTATE_NONE,
	ROUNDSTATE_PRE,
	ROUNDSTATE_ROUND,
	ROUNDSTATE_FINISHED,
	ROUNDSTATE_POST
}
*/

const uint ROUNDSTATE_NONE = 0;
const uint ROUNDSTATE_PRE = 1;
const uint ROUNDSTATE_ROUND = 2;
const uint ROUNDSTATE_FINISHED = 3;
const uint ROUNDSTATE_POST = 4;

//eRoundStates roundState = ROUNDSTATE_NONE; FIXME enum
uint roundState = ROUNDSTATE_NONE;

bool roundCheckEndTime; // you can check if roundStateEndTime == 0 but roundStateEndTime can overflow
int64 roundStartTime;    // roundStartTime because only spawn protection uses it
int64 roundStateEndTime; // XXX: this should be fixed in all gts

int roundCountDown;

uint roundCount;

int attackingTeam;
int defendingTeam;

uint alphaAliveAtStart;
uint betaAliveAtStart;

bool attackersHurried;
bool defendersHurried;

void playerKilled( Entity @victim, Entity @attacker, Entity @inflictor )
{
	// this happens if you kill a corpse or something...
	if ( @victim == null || @victim.client == null )
	{
		return;
	}

	// ch :
	cPlayer @pVictim = @playerFromClient( @victim.client );
	pVictim.oneVS = 0;
	
	if( match.getState() != MATCH_STATE_PLAYTIME )
		return;

	if( roundState >= ROUNDSTATE_FINISHED )
		return;

	if ( bombState == BOMBSTATE_CARRIED && @victim == @bombCarrier )
	{
		bombDrop( BOMBDROP_KILLED );

		G_CenterPrintMsg( null, S_COLOR_ORANGE + "The bomb carrier has been fragged!" );

		if ( @attacker != null && @attacker.client != null && attacker.team != victim.team )
		{
			attacker.client.addAward( S_COLOR_ORANGE + "Bomb Carrier Frag!" );
		}
	}

	if ( @attacker != null && @attacker.client != null && attacker.team != victim.team )
	{
		cPlayer @player = @playerFromClient( @attacker.client );
		
		player.killsThisRound++;

		if ( player.killsThisRound >= IMPRESSIVE_KILLS )
		{
			int required_for_bongo = attacker.team == TEAM_ALPHA ? betaAliveAtStart : alphaAliveAtStart;
			if ( player.killsThisRound == required_for_bongo )
			{
				player.client.addAward( S_COLOR_YELLOW + "King of Bongo!" );

				G_AnnouncerSound( null, sndBongo, GS_MAX_TEAMS, true, null );

				G_CenterPrintFormatMsg( null, "%s is the King of Bongo!", player.client.name );
			}
			else
			{
				// XXX: handle kills = 1?
				player.client.addAward( S_COLOR_YELLOW + "Impressive! " + player.killsThisRound + " frags!" );
			}
		}
	}
	
	// add a round for the victim
	victim.client.stats.addRound();
	
	// check if the player's team is now dead
	checkPlayersAlive( victim.team );
}

void checkPlayersAlive( int team )
{
	uint alive = playersAliveOnTeam( team );

	if ( alive == 0 )
	{
		if ( team == attackingTeam )
		{
			if ( bombState != BOMBSTATE_ARMED )
			{
				roundWonBy( defendingTeam );
			}
		}
		else
		{
			roundWonBy( attackingTeam );
		}

		return;
	}

	int teamOther   = otherTeam( team );
	uint aliveOther = playersAliveOnTeam( teamOther );

	if ( alive == 1 )
	{
		if ( aliveOther == 1 )
		{
			G_PrintMsg( null, "1v1! Good luck!\n" );

			firstAliveOnTeam( attackingTeam ).addAward( "1v1! Good luck!" );
			firstAliveOnTeam( defendingTeam ).addAward( "1v1! Good luck!" );
		}
		else if ( aliveOther != 0 )
		{
			oneVsMsg( team, aliveOther );
		}

		return;
	}

	if ( aliveOther == 1 )
	{
		// we know alive != 0 && alive != 1
		oneVsMsg( teamOther, alive );
	}
}

void oneVsMsg( int teamNum, uint enemies )
{
	Client @survivor = @firstAliveOnTeam( teamNum );

	if ( @survivor == null )
	{
		assert( false, "round.as oneVsMsg: @survivor == null" );

		return;
	}

	survivor.addAward( "1v" + enemies + "! You're on your own!" );

	if ( enemies == 1 )
	{
		G_PrintMsg( null, "1v1! Good luck!" );
	}
	else
	{
		Team @team = @G_GetTeam( teamNum );

		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			G_PrintMsg( @team.ent( i ), "1v" + enemies + "! " + survivor.name + " is on their own!\n" );
		}
		
		// ch :
		cPlayer @pSurvivor = @playerFromClient( @survivor );
		pSurvivor.oneVS = enemies;
	}
}

void setTeams()
{
	uint limit = cvarScoreLimit.integer;

	bool even = roundCount % 2 == 0;

	if( limit == 0 || roundCount >= ( limit - 1 ) * 2 ) {
		attackingTeam = even ? INITIAL_ATTACKERS : INITIAL_DEFENDERS;
		defendingTeam = even ? INITIAL_DEFENDERS : INITIAL_ATTACKERS;
		return;
	}

	bool first_half = roundCount < limit - 1;
	attackingTeam = first_half ? INITIAL_ATTACKERS : INITIAL_DEFENDERS;
	defendingTeam = first_half ? INITIAL_DEFENDERS : INITIAL_ATTACKERS;
}

void newGame()
{
	roundCount = 0;
	setTeams();

	for ( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ )
	{
		gametype.setTeamSpawnsystem( t, SPAWNSYSTEM_HOLD, 0, 0, true );

		Team @team = @G_GetTeam( t );

		team.stats.clear();

		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			team.ent( i ).client.stats.clear();
		}
	}

	newRound();
}

// this function doesn't care how the round was won
void roundWonBy( int winner )
{
	int loser = winner == attackingTeam ? defendingTeam : attackingTeam;

	// ololo
	G_CenterPrintMsg( null, S_COLOR_CYAN + ( winner == attackingTeam ? "OFF" : "DEF" ) + "ENSE WINS!");

	int soundIndex = G_SoundIndex( "sounds/announcer/bomb/score_team0" + (1 + (rand() & 1)) );
	G_AnnouncerSound( null, soundIndex, winner, true, null );

	soundIndex = G_SoundIndex( "sounds/announcer/bomb/score_enemy0" + (1 + (rand() & 1)) );
	G_AnnouncerSound( null, soundIndex, loser, true, null );

	Team @teamWinner = @G_GetTeam( winner );

	teamWinner.stats.addScore( 1 );

	for ( int i = 0; @teamWinner.ent( i ) != null; i++ )
	{
		Entity @ent = @teamWinner.ent( i );

		if ( !ent.isGhosting() )
		{
			ent.client.addAward( S_COLOR_GREEN + "Victory!" );
			
			// ch :
			cPlayer @player = @playerFromClient( @ent.client );
			if( player.oneVS > 0 )
				// ent.client.addMetaAward( "Clean The House!" );
				ent.client.addAward( "Clean The House!" );
			
			// ch : add a round for alive players on this team
			ent.client.stats.addRound();
		}
	}

	// ch : add a round for the losing team's alive players
	Team @teamLoser = @G_GetTeam( loser );
	for( int i = 0; @teamLoser.ent( i ) != null; i++ )
	{
		Entity @ent = @teamLoser.ent( i );
		if( !ent.isGhosting() )
			ent.client.stats.addRound();
	}
	
	roundNewState( ROUNDSTATE_FINISHED );
}

void newRound()
{	
	roundNewState( ROUNDSTATE_PRE );
}

void endGame()
{
	roundNewState( ROUNDSTATE_NONE );

	GENERIC_SetUpEndMatch();
}

bool scoreLimitHit()
{
	return match.scoreLimitHit() && abs( G_GetTeam( TEAM_ALPHA ).stats.score - G_GetTeam( TEAM_BETA ).stats.score ) > 1;
}

//void roundNewState( eRoundStates state ) FIXME enum
void roundNewState( uint state )
{
	if ( state > ROUNDSTATE_POST )
	{
		state = ROUNDSTATE_PRE;
	}

	roundState = state;

	switch ( int( state ) )
	{
		case ROUNDSTATE_NONE:
			break;

		case ROUNDSTATE_PRE:
		{
			roundCountDown = COUNTDOWN_MAX;

			setTeams();

			roundCount++;

			roundCheckEndTime = true;
			roundStateEndTime = levelTime + 5000;

			gametype.shootingDisabled = true;
			gametype.removeInactivePlayers = false;

			attackersHurried = defendersHurried = false;

			resetBombSites();

			G_ResetLevel();

			resetBomb();

			resetKillCounters();
			respawnAllPlayers();
			disableMovement();

			bombGiveToRandom();

			break;
		}

		case ROUNDSTATE_ROUND:
		{
			roundCheckEndTime = true;
			roundStartTime = levelTime;
			roundStateEndTime = levelTime + int( cvarRoundTime.value * 1000.0f );

			gametype.shootingDisabled = false;
			gametype.removeInactivePlayers = true;

			enableMovement();

			Team @team = @G_GetTeam( defendingTeam );

			for ( int i = 0; @team.ent( i ) != null; i++ )
			{
				G_CenterPrintMsg( team.ent( i ), S_COLOR_ORANGE + "PROTECT THE BOMB SITES!" );
			}

			@team = @G_GetTeam( attackingTeam );

			for ( int i = 0; @team.ent( i ) != null; i++ )
			{
				G_CenterPrintMsg( team.ent( i ), S_COLOR_ORANGE + "DESTROY THE TARGETS!" );
			}

			announce( ANNOUNCEMENT_STARTED );

			break;
		}

		case ROUNDSTATE_FINISHED:
			roundCheckEndTime = true;
			roundStateEndTime = levelTime + 1500; // magic numbers are awesome

			gametype.shootingDisabled = true;

			// ch : nullify these up
			@fastPlanter = null;
			@lastCallPlanter = null;

			break;

		case ROUNDSTATE_POST:
			if ( scoreLimitHit() && !match.checkExtendPlayTime() )
			{
				match.launchState( match.getState() + 1 );

				return;
			}

			roundCheckEndTime = true;
			roundStateEndTime = levelTime + 3000; // XXX: old bomb did +5s but i don't see the point

			break;

		default:
			assert( false, "round.as roundNewState: bad state" );

			break;
	}
}

int last_time = 0;

void roundThink()
{
	if ( roundState == ROUNDSTATE_NONE )
	{
		return;
	}

	if ( roundState == ROUNDSTATE_PRE )
	{
		int remainingSeconds = int( ( roundStateEndTime - levelTime ) * 0.001f ) + 1;

		if ( remainingSeconds < 0 )
		{
			remainingSeconds = 0;
		}

		if ( remainingSeconds < roundCountDown )
		{
			roundCountDown = remainingSeconds;

			if ( roundCountDown == COUNTDOWN_MAX )
			{
				int soundIndex = G_SoundIndex( "sounds/announcer/countdown/ready0" + (1 + (rand() & 1)) );

				G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
			}
			else
			{
				if( roundCountDown < 4 )
				{
					int soundIndex = G_SoundIndex( "sounds/announcer/countdown/" + roundCountDown + "_0" + (1 + (rand() & 1)) );

					G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
				}
			}
		}

		alphaAliveAtStart = playersAliveOnTeam( TEAM_ALPHA );
		betaAliveAtStart = playersAliveOnTeam( TEAM_BETA );

		last_time = roundStateEndTime - levelTime + int( cvarRoundTime.value * 1000.0f );
		match.setClockOverride( last_time );
	}

	// i suppose the following blocks could be merged to save an if or 2
	if ( roundCheckEndTime && levelTime > roundStateEndTime )
	{
		if ( roundState == ROUNDSTATE_ROUND )
		{
			if ( bombState != BOMBSTATE_ARMED )
			{
				roundWonBy( defendingTeam );
				last_time = 1; // kinda hacky, this shows at 0:00

				// put this after roundWonBy or it gets overwritten
				G_CenterPrintMsg( null, S_COLOR_RED + "Timelimit hit!" );

				return;
			}
		}
		else
		{
			//roundNewState( eRoundStates( roundState + 1 ) ); FIXME enum
			roundNewState( roundState + 1 );

			return;
		}
	}

	if ( roundState == ROUNDSTATE_ROUND )
	{
		// monitor the bomb's health
		if ( @bombModel == null || bombModel.classname != "dynamite" ) {
			bombModelCreate();

			roundWonBy( defendingTeam );

			// put this after roundWonBy or it gets overwritten
			G_CenterPrintMsg( null, S_COLOR_RED + "The attacking team has lost the bomb!!!" );

			return;
		}

		// warn defs if bomb will explode soon
		// warn offs if the round ends soon and they haven't planted
		if ( bombState == BOMBSTATE_ARMED )
		{
			last_time = -1;

			if ( !defendersHurried && levelTime + BOMB_HURRYUP_TIME >= bombActionTime )
			{
				announceDef( ANNOUNCEMENT_HURRY );

				defendersHurried = true;
			}
		}
		else {
			last_time = roundStateEndTime - levelTime;

			if ( !attackersHurried && levelTime + BOMB_HURRYUP_TIME >= roundStateEndTime )
			{
				announceOff( ANNOUNCEMENT_HURRY );

				attackersHurried = true;
			}
		}

		match.setClockOverride( last_time );

		bombThink();
	}
	else
	{
		match.setClockOverride( last_time );

		if ( roundState > ROUNDSTATE_ROUND )
		{
			bombAltThink();
		}
	}
}

uint playersAliveOnTeam( int teamNum )
{
	uint alive = 0;

	Team @team = @G_GetTeam( teamNum );

	for ( int i = 0; @team.ent( i ) != null; i++ )
	{
		Entity @ent = @team.ent( i );

		// check health incase they died this frame

		if ( !ent.isGhosting() && ent.health > 0 )
		{
			alive++;
		}
	}

	return alive;
}

// loops through players on teamNum and returns Entity of first alive player
// this is only used when playersAliveOnTeam returns 1
// hence the assert
Client @firstAliveOnTeam( int teamNum )
{
	Team @team = @G_GetTeam( teamNum );

	for ( int i = 0; @team.ent( i ) != null; i++ )
	{
		Entity @ent = @team.ent( i );

		// check health incase they died this frame

		if ( !ent.isGhosting() && ent.health > 0 )
		{
			return @ent.client;
		}
	}

	assert( false, "round.as firstAliveOnTeam: found nobody" );

	return null; // shut up compiler
}

void respawnAllPlayers()
{
	for ( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ )
	{
		Team @team = @G_GetTeam( t );

		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			team.ent( i ).client.respawn( false );
		}
	}
}

int otherTeam( int team )
{
	return team == attackingTeam ? defendingTeam : attackingTeam;
}

void enableMovement()
{
	for ( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ )
	{
		Team @team = @G_GetTeam( t );

		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			Client @client = @team.ent( i ).client;

			client.pmoveMaxSpeed = -1;
			client.pmoveDashSpeed = -1;
			client.pmoveFeatures = client.pmoveFeatures | PMFEAT_JUMP | PMFEAT_DASH | PMFEAT_WALLJUMP;
		}
	}
}

void disableMovementFor( Client @client )
{
	client.pmoveMaxSpeed = 100;
	client.pmoveDashSpeed = 0;
	client.pmoveFeatures = client.pmoveFeatures & ~( PMFEAT_JUMP | PMFEAT_DASH | PMFEAT_WALLJUMP );
}

void disableMovement()
{
	for ( int t = TEAM_PLAYERS; t < GS_MAX_TEAMS; t++ )
	{
		Team @team = @G_GetTeam( t );

		for ( int i = 0; @team.ent( i ) != null; i++ )
		{
			Client @client = @team.ent( i ).client;

			disableMovementFor( @client );
		}
	}
}
