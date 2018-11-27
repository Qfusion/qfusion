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

void GENERIC_SetUpWarmup()
{
	int j;
	Team @team;

	gametype.shootingDisabled = false;
	gametype.readyAnnouncementEnabled = true;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;

	if ( gametype.isTeamBased )
	{
		bool anyone = false;
		int t;

		for ( t = TEAM_ALPHA; t < GS_MAX_TEAMS; t++ )
		{
			@team = @G_GetTeam( t );
			team.clearInvites();

			for ( j = 0; @team.ent( j ) != null; j++ )
				GENERIC_ClearOverlayMenu( @team.ent( j ).client );

			if ( team.unlock() )
				anyone = true;
		}

		if ( anyone )
			G_PrintMsg( null, "Teams unlocked.\n" );
	}
	else
	{
		@team = @G_GetTeam( TEAM_PLAYERS );
		team.clearInvites();

		for ( j = 0; @team.ent( j ) != null; j++ )
			GENERIC_ClearOverlayMenu( @team.ent( j ).client );

		if ( team.unlock() )
			G_PrintMsg( null, "Teams unlocked.\n" );
	}

	match.name = "";
}

void GENERIC_SetUpCountdown( bool respawnItems = true )
{
	G_RemoveAllProjectiles();

	if( respawnItems )
		G_Items_RespawnByType( 0, 0, 0 ); // respawn all items

	gametype.shootingDisabled = true;
	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = true;

	// lock teams
	bool anyone = false;
	if ( gametype.isTeamBased )
	{
		for ( int team = TEAM_ALPHA; team < GS_MAX_TEAMS; team++ )
		{
			if ( G_GetTeam( team ).lock() )
				anyone = true;
		}
	}
	else
	{
		if ( G_GetTeam( TEAM_PLAYERS ).lock() )
			anyone = true;
	}

	if ( anyone )
		G_PrintMsg( null, "Teams locked.\n" );

	// Countdowns should be made entirely client side, because we now can

	int soundIndex = G_SoundIndex( "sounds/announcer/countdown/get_ready_to_fight0" + random_uniform( 1, 3 ) );
	G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
}

void GENERIC_SetUpMatch()
{
	int i, j;
	Entity @ent;
	Team @team;

	G_RemoveAllProjectiles();
	gametype.shootingDisabled = true;  // avoid shooting before "FIGHT!"
	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = true;
	gametype.countdownEnabled = true;

	// clear player stats and scores, team scores and respawn clients in team lists

	for ( i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
	{
		@team = @G_GetTeam( i );
		team.stats.clear();

		// respawn all clients inside the playing teams
		for ( j = 0; @team.ent( j ) != null; j++ )
		{
			@ent = @team.ent( j );
			ent.client.stats.clear(); // clear player scores & stats
			ent.client.respawn( false );
		}
	}

	// set items to be spawned with a delay
	G_Items_RespawnByType( IT_POWERUP, 0, random_uniform( 20, 40 ) );
	G_Items_RespawnByType( IT_ARMOR, 0, 10 );
	G_Items_RespawnByType( IT_HEALTH, HEALTH_MEGA, 15 );
	G_Items_RespawnByType( IT_HEALTH, HEALTH_ULTRA, 15 );
	G_RemoveDeadBodies();

	// Countdowns should be made entirely client side, because we now can
	int soundindex = G_SoundIndex( "sounds/announcer/countdown/fight0" + random_uniform( 1, 3 ) );
	G_AnnouncerSound( null, soundindex, GS_MAX_TEAMS, false, null );
	G_CenterPrintMsg( null, "FIGHT!" );

	// now we can enable shooting
	gametype.shootingDisabled = false;
}

void GENERIC_SetUpEndMatch()
{
	Client @client;

	gametype.shootingDisabled = true;
	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;

	for ( int i = 0; i < maxClients; i++ )
	{
		@client = @G_GetClient( i );

		if ( client.state() >= CS_SPAWNED ) {
			client.respawn( true ); // ghost them all
			GENERIC_SetPostmatchOverlayMenu( @client );
		}
	}

	GENERIC_UpdateMatchScore();

	// print scores to console
	if ( gametype.isTeamBased )
	{
		Team @team1 = @G_GetTeam( TEAM_ALPHA );
		Team @team2 = @G_GetTeam( TEAM_BETA );

		String sC1 = (team1.stats.score < team2.stats.score ? S_COLOR_RED : S_COLOR_GREEN);
		String sC2 = (team2.stats.score < team1.stats.score ? S_COLOR_RED : S_COLOR_GREEN);

		G_PrintMsg( null, S_COLOR_YELLOW + "Final score: " + S_COLOR_WHITE + team1.name + S_COLOR_WHITE + " vs " +
			team2.name + S_COLOR_WHITE + " - " + match.getScore() + "\n" );
	}

	int soundIndex = G_SoundIndex( "sounds/announcer/postmatch/game_over0" + random_uniform( 1, 3 ) );
	G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, true, null );
}

///*****************************************************************
/// CHECK FOR CHEAT CVARS
///*****************************************************************

bool cheatVarsListInitialized = false;
int64 lastCheatVarRequestTime = levelTime + 30000;
int cheackVarChecked = 0;

class cCheatVar
{
	String name;
	String content;
	bool anyContent;
}

const int MAX_CHEATVAR_NAMES = 27;
cCheatVar[] cheatVarNames( MAX_CHEATVAR_NAMES );

void GENERIC_InitCheatVarsList()
{
	if ( cheatVarsListInitialized == true )
		return;

	//cheatVarNames[0].name = "orgy_aim_aimbot";
	//cheatVarNames[0].anyContent = true;

	cheatVarsListInitialized = true;
}

void GENERIC_RequestCheatVars()
{
	GENERIC_InitCheatVarsList();

	if( cheatVarNames.empty() )
		return;

	if ( lastCheatVarRequestTime + 15000 > levelTime )
		return;

	lastCheatVarRequestTime = levelTime;

	G_CmdExecute( "cvarcheck " + "all \"" + cheatVarNames[cheackVarChecked].name + "\"\n" );

	cheackVarChecked++;
	if ( cheackVarChecked >= MAX_CHEATVAR_NAMES || cheatVarNames[cheackVarChecked].name.len() == 0 )
		cheackVarChecked = 0;
}

void GENERIC_CheatVarResponse( Client @client, String &cmdString, String &argsString, int argc )
{
	//G_Print( S_COLOR_RED + "cvarinfo response: (argc" + argc + ") " + S_COLOR_WHITE + client.name + S_COLOR_WHITE + " " + argsString + "\n" );

	if ( argc < 2 )
		return;

	if ( @client == null )
		return;

	bool kick = false;

	String cvarName = argsString.getToken( 0 );
	String cvarContent = argsString.getToken( 1 );

	if ( cvarContent.len() > 0 && cvarContent != "not found" )
	{
		// find what was the cvar
		for ( int i = 0; i < MAX_CHEATVAR_NAMES; i++ )
		{
			if ( cheatVarNames[i].name == cvarName )
			{
				if ( cheatVarNames[i].anyContent ) // any means we kick if it exists, no matter the content
				{
					kick = true;
					break;
				}
				else if ( cheatVarNames[i].content == cvarContent )
				{
					kick = true;
					break;
				}

			}
			else if ( cheatVarNames[i].name.len() == 0 )
				break;
		}
	}

	if ( kick )
	{
		G_PrintMsg( null, S_COLOR_RED + "WARNING: " + S_COLOR_WHITE + client.name + S_COLOR_RED + " is kickbanned cause of forbidden cvar \"" + cvarName + " " + cvarContent + "\"\n" );

		G_CmdExecute( "addip \"" + client.getUserInfoKey( "ip" ) + "\" 10080\n" );
		G_CmdExecute( "kick " + client.playerNum + "\n" );
		return;
	}
}

///*****************************************************************
/// MISC UTILS (this should get its own generic file
///*****************************************************************

// returns false if the target wasn't visible
bool GENERIC_LookAtEntity( Vec3 &in origin, Vec3 &in angles, Entity @lookTarget, int ignoreNum, bool lockPitch, int backOffset, int upOffset, Vec3 &out lookOrigin, Vec3 &out lookAngles )
{
	if ( @lookTarget == null )
		return false;

	bool visible = true;

	Vec3 start, end, mins, maxs, dir;
	Trace trace;

	start = end = origin;
	if ( upOffset != 0 )
	{
		end.z += upOffset;
		trace.doTrace( start, vec3Origin, vec3Origin, end, ignoreNum, MASK_OPAQUE );
		if ( trace.fraction < 1.0f )
		{
			start = trace.endPos + ( trace.planeNormal * 0.1f );
		}
	}

	lookTarget.getSize( mins, maxs );
	end = lookTarget.origin + ( 0.5 * ( maxs + mins ) );

	if ( !trace.doTrace( start, vec3Origin, vec3Origin, end, ignoreNum, MASK_OPAQUE ) )
	{
		if ( trace.entNum != lookTarget.entNum )
			visible = false;
	}

	if ( lockPitch )
		end.z = lookOrigin.z;

	if ( backOffset != 0 )
	{
		// trace backwards from dest to origin projected to backoffset
		dir = start - end;
		dir.normalize();
		Vec3 newStart = start + ( dir * backOffset );

		trace.doTrace( start, vec3Origin, vec3Origin, newStart, ignoreNum, MASK_OPAQUE );
		start = trace.endPos;
		if ( trace.fraction < 1.0f )
		{
			start += ( trace.planeNormal * 0.1f );
		}
	}

	dir = end - start;

	lookOrigin = start;
	lookAngles = dir.toAngles();

	return visible;
}

///*****************************************************************
/// BASIC SPAWNPOINT SELECTION
///*****************************************************************

class cSpawnPoint
{
	Entity @ent;
	int range;
	cSpawnPoint()
	{
		this.range = -1;
		@this.ent = null;
	}
	~cSpawnPoint() {}
}

Entity @GENERIC_SelectBestRandomTeamSpawnPoint( Entity @self, String &className, bool onlyTeam )
{
	Entity @spawn;
	Entity @enemy;
	int numSpawns;
	int dist;
	int closestRange;
	int numPickableSpawns;
	Team @enemyTeam;
	bool isDuel = ( gametype.maxPlayersPerTeam == 1 );

	array<Entity @> @spawnents = G_FindByClassname( className );
	numSpawns = spawnents.size();

	if ( numSpawns == 0 )
		return null;
	if ( numSpawns == 1 )
		return spawnents[0];

	cSpawnPoint[] spawns( numSpawns );

	if ( @self != null )
	{
		if ( self.team == TEAM_ALPHA )
			@enemyTeam = @G_GetTeam( TEAM_BETA );
		else if ( self.team == TEAM_BETA )
			@enemyTeam = @G_GetTeam( TEAM_ALPHA );
		else
			@enemyTeam = @G_GetTeam( TEAM_PLAYERS );
	}
	else
	{
		@enemyTeam = null;
	}

	// Get spawn points
	int pos = 0; // Current position
	for( uint i = 0; i < spawns.size(); i++ )
	{
		@spawn = spawnents[i];

		// only accept those of the same team
		if ( onlyTeam && ( self.team != TEAM_SPECTATOR ) )
		{
			if ( spawn.team != self.team )
			{
				continue;
			}
		}

		if ( @enemyTeam != null )
		{
			closestRange = 9999999;

			// find the distance to the closer enemy player from this spawn
			for ( int j = 0; @enemyTeam.ent( j ) != null; j++ )
			{
				@enemy = @enemyTeam.ent( j );

				if ( enemy.isGhosting() || @enemy == @self )
					continue;

				// Get closer distance from the enemies
				dist = int( spawn.origin.distance( enemy.origin ) );
				if ( isDuel ) {
					if( !G_InPVS( enemy.origin, spawn.origin ) ) {
						dist *= 2;
					}
				}

				if ( dist < closestRange ) {
					closestRange = dist;
				}
			}
		}

		// Save current spawn point
		@spawns[pos].ent = @spawn;
		spawns[pos].range = closestRange;

		// Go forward reading next respawn point
		pos++;
	}

	// Get spawn points in descending order by range
	// Used algorithm: insertion sort
	// Dont use it over 30 respawn points
	for( int i = 0; i < numSpawns; i++ )
	{
		int j = i;
		cSpawnPoint save;
		@save.ent = @spawns[j].ent;
		save.range = spawns[j].range;
		while( ( j > 0 ) && ( spawns[ j-1 ].range < save.range ) )
		{	
			@spawns[j].ent = @spawns[j-1].ent;
			spawns[j].range = spawns[j-1].range;

			j--;
		}
		@spawns[j].ent = @save.ent;
		spawns[j].range = save.range;
	}

	if ( numSpawns < 5 ) // always choose the clearest one
		return spawns[0].ent;
	numSpawns -= 3; // ignore the closest 3 points

	if( !isDuel ) {
		return spawns[ random_uniform( 0, numSpawns ) ].ent;
	}

	// calculate denormalized range sum
	int rangeSum = 0;
	for( int i = 0; i < numSpawns; i++ )
		rangeSum += spawns[i].range;

	// pick random denormalized range
	int testRange = 0;
	int weightedRange = random_uniform( 0, rangeSum );

	// find spot for the weighted range. distant spawn points are more probable
	for( int i = numSpawns - 1; i >= 0; i-- ) {
		testRange += spawns[i].range;
		if( testRange >= weightedRange ) {
			return spawns[i].ent;
		}
	}

	return spawns[0].ent;
}

Entity @GENERIC_SelectBestRandomSpawnPoint( Entity @self, String &className )
{
	return GENERIC_SelectBestRandomTeamSpawnPoint( self, className, false );
}

///*****************************************************************
/// SET TEAM NAMES IN TEAM BASED GAMETYPES
///*****************************************************************

void GENERIC_UpdateMatchScore()
{
	if ( gametype.isTeamBased )
	{
		Team @team1 = @G_GetTeam( TEAM_ALPHA );
		Team @team2 = @G_GetTeam( TEAM_BETA );

		String score = team1.stats.score + " : " + team2.stats.score;

		match.setScore( score );
		return;
	}

	match.setScore( "" );
}

void GENERIC_DetectTeamsAndMatchNames()
{
	if ( !gametype.isTeamBased )
		return;

	String matchName = "";
	bool matchNameOk = true;

	for ( int teamNo = TEAM_ALPHA; teamNo <= TEAM_BETA; teamNo++ )
	{
		Team @team;
		String teamName, defaultTeamName;
		bool multiPlayerTeams = ( gametype.maxPlayersPerTeam == 0 || gametype.maxPlayersPerTeam > 1 );

		@team = @G_GetTeam( teamNo );
		teamName = defaultTeamName = team.defaultName;
		if ( team.numPlayers > 0 )
		{
			// use first player's clan name (with color chars intact)
			String clanName = team.ent( 0 ).client.clanName;
			String clanNameColorless = clanName.removeColorTokens();

			if ( multiPlayerTeams && ( team.numPlayers > 1 ) )
			{
				for ( int i = 1; @team.ent( i ) != null; i++ )
				{
					if ( team.ent( i ).client.clanName.removeColorTokens() != clanNameColorless )
					{
						clanName = clanNameColorless = "";
						break;
					}
				}
			}

			if ( multiPlayerTeams )
			{
				// set clan name as team name
				if ( ( team.numPlayers > 1 ) && ( clanNameColorless.len() > 0 ) )
					teamName = clanName + S_COLOR_WHITE;
			}
			else
			{
				// for individual gametypes, append clan name to player's name
				String lastClanNameChar = "";
				if ( clanNameColorless.len() > 0 )
					lastClanNameChar = clanNameColorless.substr( clanNameColorless.length() - 1, 1 );
				teamName = (lastClanNameChar.length() > 0 ? clanName + (lastClanNameChar.isAlphaNumerical() ? "/" : "") : "") + team.ent( 0 ).client.name;
			}
		}

		if ( teamName != team.name )
			team.name = teamName;

		// match name
		if ( matchNameOk )
		{
			if ( teamName != defaultTeamName )
			{
				matchName += (matchName.len() > 0 ? " vs " : "") + teamName;
			}
			else
			{
				matchName = "";
				matchNameOk = false;
			}
		}
	}

	if ( matchName != match.name )
		match.name = matchName;
}

void GENERIC_Think()
{
	GENERIC_DetectTeamsAndMatchNames();
	GENERIC_UpdateMatchScore();
	GENERIC_RequestCheatVars();
}
