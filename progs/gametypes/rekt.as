/*
Copyright (C) 2016 Victor Luchits

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

enum eRoundState
{
	ROUNDSTATE_NONE,
	ROUNDSTATE_PREROUND,
	ROUNDSTATE_ROUND,
	ROUNDSTATE_ROUNDFINISHED,
	ROUNDSTATE_POSTROUND
}

const String FORCED_MODEL_BETA = "$models/players/bobot";

const uint BASE_BOT_HEALTH = 40.0f;
const uint MAXIMUM_BOT_COUNT = 7;
const uint FATTER_BOTS_ROUND_NUM = 8;

const int DEFAULT_ROUND_DURATION = 40000;
const int MAXIMUM_ROUND_DURATION = 90000;

class EnduranceMatch
{
	private eRoundState roundState;
	private uint numRounds;
	private uint roundStateStartTime;
	private uint roundStateEndTime;

	private int countDown;

	private Team @humans;
	private Team @bots;
	private Team @specs;

	private Entity @warshellBot;

	private uint humansAlive;
	private uint botsAlive;

	EnduranceMatch()
	{
	}

	~EnduranceMatch() {}

	void init()
	{
		this.roundState = ROUNDSTATE_NONE;
		this.numRounds = 0;
		this.roundStateStartTime = 0;
		this.countDown = 0;

		this.humansAlive = 0;
		this.botsAlive = 0;

		@this.humans = G_GetTeam( TEAM_ALPHA );
		@this.bots = G_GetTeam( TEAM_BETA );
		@this.specs = G_GetTeam( TEAM_SPECTATOR );
	}

	private void announce()
	{
		String str = S_COLOR_CYAN + "Round " + this.numRounds;
		String cmd = 'aw "' + str + '"';

		for ( int i = 0; @this.humans.ent( i ) != null; i++ )
		{
			Entity @ent = @this.humans.ent( i );
			ent.client.execGameCommand( cmd );
		}

		// also add it to spectators
		for ( int i = 0; @this.specs.ent( i ) != null; i++ )
		{
			Entity @ent = @this.specs.ent( i );
			ent.client.execGameCommand( cmd );
		}
	}

	void warmup()
	{
		// clear scores
		for ( int i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
		{
			Team @team = @G_GetTeam( i );
			team.stats.clear();
			for ( int j = 0; @team.ent( j ) != null; j++ )
			{
				Entity @ent = @team.ent( j );
				ent.client.stats.clear();
			}
		}

		this.numRounds = 0;
		this.newRoundState( ROUNDSTATE_NONE );
	}

	void countdown()
	{
		
	}

	void begin()
	{
		this.newRound();
	}
	
	void end()
	{

	}

	private void newRound()
	{
		this.numRounds++;
		this.newRoundState( ROUNDSTATE_PREROUND );
	}
	
	private void newRoundState( eRoundState newState )
	{
		int duration;
		int soundIndex;
		int numBots;

		if ( newState > ROUNDSTATE_POSTROUND )
		{
			this.newRound();
			return;
		}

		this.roundState = newState;
		this.roundStateStartTime = levelTime;

		switch ( this.roundState )
		{
		case ROUNDSTATE_NONE:
			this.roundStateEndTime = 0;
			this.countDown = 0;
			break;

		case ROUNDSTATE_PREROUND:
			this.roundStateEndTime = levelTime + 4000;
			this.countDown = 4;
			this.announce();
			break;

		case ROUNDSTATE_ROUND:
			// respawn only dead humans
			for ( int j = 0; @this.humans.ent( j ) != null; j++ )
			{
				Entity @ent = @this.humans.ent( j );
				if( ent.isGhosting() )
					ent.client.respawn( false );
				ent.client.stats.addRound();
			}
		
			// respawn bots
			numBots = 0;
			@this.warshellBot = null;
			for ( uint j = 0; @this.bots.ent( j ) != null; j++ )
			{
				Entity @ent = @this.bots.ent( j );
				if( j < this.numRounds )
				{
					numBots++;
					ent.client.respawn( false );
					ent.health = BASE_BOT_HEALTH;
					if( this.numRounds >= FATTER_BOTS_ROUND_NUM )
						ent.health += (this.numRounds - FATTER_BOTS_ROUND_NUM + 1) * 10;
					if( this.humans.numPlayers > 2 )
						ent.health += (this.humans.numPlayers - 2) * 5;
					if( this.numRounds > 0 && ( this.numRounds % 5 ) == 0 ) {
						if( @this.warshellBot == null ) {
							// here's our warshell guy
							ent.health = BASE_BOT_HEALTH;
							ent.client.inventoryGiveItem( POWERUP_SHELL );
							@this.warshellBot = @ent;
						}
					}
				}
				else
				{
					ent.client.respawn( false );
					ent.client.chaseCam( null, true );
				}
			}
			this.bots.stats.setScore( numBots );

			duration = (this.numRounds - 4) * 10000;
			if( duration < 0 )
				duration = 0;
			duration += DEFAULT_ROUND_DURATION;
			if( duration > MAXIMUM_ROUND_DURATION )
				duration = MAXIMUM_ROUND_DURATION;

			this.countDown = 0;
			this.roundStateEndTime = levelTime + uint( duration );
			
			soundIndex = G_SoundIndex( "sounds/announcer/countdown/fight0" + (1 + (rand() & 1)) );
			G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
			break;

		case ROUNDSTATE_ROUNDFINISHED:
			if ( this.humansAlive == 0 || this.botsAlive > 0 )
			{
				// game over
				match.launchState( MATCH_STATE_POSTMATCH );
			}
			else
			{
				soundIndex = G_SoundIndex( "sounds/announcer/ctf/score0" + (1 + (rand() & 1)) );
				G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
			}
			this.roundStateEndTime = levelTime + 500;
			this.countDown = 0;
			break;

		case ROUNDSTATE_POSTROUND:
			// respawn dead humans
			if ( this.humansAlive > 0 && this.humans.numPlayers > 1 )
			{
				for ( int j = 0; @this.humans.ent( j ) != null; j++ )
				{
					Entity @ent = @this.humans.ent( j );
					if( ent.isGhosting() )
						ent.client.respawn( false );
				}
			}
			this.roundStateEndTime = levelTime + 1000;
			break;

		default:
			break;
		}
	}

	void think()
	{
		Entity @ent;
		Client @client;

		GENERIC_Think();

		this.humansAlive = 0;
		this.botsAlive = 0;
		
		if ( match.getState() != MATCH_STATE_PLAYTIME )
		{
			if( this.humans.numPlayers > 0 && uint( this.bots.numPlayers ) == gametype.numBots )
			{
				if ( match.getState() != MATCH_STATE_COUNTDOWN )
				{
					// respawn humans
					for ( int j = 0; @this.humans.ent( j ) != null; j++ )
					{
						@ent = @this.humans.ent( j );
						if( ent.isGhosting() )
							ent.client.respawn( false );
					}
					match.launchState( MATCH_STATE_COUNTDOWN );
				}
				return;
			}

			return;
		}
		
		if ( this.roundState == ROUNDSTATE_NONE )
			return;

		for ( int j = 0; @this.humans.ent( j ) != null; j++ )
		{
			@ent = this.humans.ent( j );
			if ( !ent.isGhosting() )
				this.humansAlive++;
		}

		for ( int j = 0; @this.bots.ent( j ) != null; j++ )
		{
			@ent = this.bots.ent( j );
			if ( !ent.isGhosting() )
				this.botsAlive++;
		}

		if ( this.roundStateEndTime != 0 )
		{
			if ( this.roundStateEndTime < levelTime )
			{
				this.newRoundState( eRoundState( this.roundState + 1 ) );
				return;
			}

			if ( this.countDown > 0 )
			{
				// we can't use the authomatic countdown announces because their are based on the
				// matchstate timelimit, and prerounds don't use it. So, fire the announces "by hand".
				int remainingSeconds = int( ( this.roundStateEndTime - levelTime ) * 0.001f ) + 1;
				if ( remainingSeconds < 0 )
					remainingSeconds = 0;

				if ( remainingSeconds < this.countDown )
				{
					this.countDown = remainingSeconds;

					if ( this.countDown == 4 )
					{
						int soundIndex = G_SoundIndex( "sounds/announcer/countdown/ready0" + (1 + (rand() & 1)) );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
					else if ( this.countDown <= 3 )
					{
						int soundIndex = G_SoundIndex( "sounds/announcer/countdown/" + this.countDown + "_0" + (1 + (rand() & 1)) );
						G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
					}
				}
			}
		}

		// if one of the teams has no humans alive move from ROUNDSTATE_ROUND
		if ( this.roundState == ROUNDSTATE_ROUND )
		{
			if ( this.humansAlive == 0 || this.botsAlive == 0 )
				this.newRoundState( eRoundState( this.roundState + 1 ) );
			else
				match.setClockOverride( this.roundStateEndTime - levelTime );
		}		
	}

	private void playerTeamChanged( Client @client, int new_team )
	{
		if ( new_team == TEAM_ALPHA )
		{
		}		
		else if( !client.isBot() )
		{
		}
		else
		{
			client.getEnt().health = 0;
			client.chaseCam( null, false );
		}
	}

	void playerKilled( Entity @target, Entity @attacker, Entity @inflictor )
	{
		if( @target.client == null )
			return;
			
		if( target.client.isBot() ) {
			this.humans.stats.addScore( 1 );
			this.bots.stats.addScore( -1 );

			if( @attacker.client != null )
				attacker.client.stats.addScore( 1 );

			if( this.numRounds >= 11 )
				target.dropItem( HEALTH_MEDIUM );
			else
				target.dropItem( HEALTH_SMALL );
			
			// drop full ammo pack
			if( @target == @this.warshellBot ) {
				// an ugly hack to drop full ammo pack
				Entity @temp = G_SpawnEntity( "temp" );
				temp.origin = target.origin;
				temp.dropItem( AMMO_PACK );
				temp.freeEntity();
			}
		}
	}
	
	void playerRespawn( Entity @ent, int old_team, int new_team )
	{
		Client @client = ent.client;

		if ( old_team != new_team )
		{
			this.playerTeamChanged( ent.client, new_team );
		}

		if ( ent.isGhosting() )
			return;

		if ( gametype.isInstagib )
		{
			client.inventoryGiveItem( WEAP_INSTAGUN );
			client.inventorySetCount( AMMO_INSTAS, 1 );
			client.inventorySetCount( AMMO_WEAK_INSTAS, 1 );
		}
		else
		{
			Item @item;
			Item @ammoItem;

			// the gunblade can't be given (because it can't be dropped)
			client.inventorySetCount( WEAP_GUNBLADE, 1 );
			client.inventorySetCount( AMMO_GUNBLADE, 1 ); // enable gunblade blast

			if( client.isBot() )
			{
				int w = WEAP_MACHINEGUN + rand() % (WEAP_LASERGUN - WEAP_MACHINEGUN + 1);
				client.inventoryGiveItem( w );

				@item = @G_GetItem( w );
				@ammoItem = @G_GetItem( item.ammoTag );
				client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );
			}
			else
			{
				for ( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ )
				{
					if ( i == WEAP_INSTAGUN ) // dont add instagun...
						continue;

					client.inventoryGiveItem( i );

					@item = @G_GetItem( i );
					@ammoItem = @G_GetItem( item.ammoTag );
					client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );
				}
			}
		}

		if( client.isBot() && !FORCED_MODEL_BETA.empty() )
			ent.setupModel( FORCED_MODEL_BETA );

		// auto-select best weapon in the inventory
		client.selectWeapon( -1 );

		// add a teleportation effect
		ent.respawnEffect();
	}
}

EnduranceMatch enduranceMatch;

///*****************************************************************
/// MODULE SCRIPT CALLS
///*****************************************************************

bool GT_Command( Client @client, const String &cmdString, const String &argsString, int argc )
{
	if ( cmdString == "gametype" )
	{
		String response = "";
		Cvar fs_game( "fs_game", "", 0 );
		String manifest = gametype.manifest;

		response += "\n";
		response += "Gametype " + gametype.name + " : " + gametype.title + "\n";
		response += "----------------\n";
		response += "Version: " + gametype.version + "\n";
		response += "Author: " + gametype.author + "\n";
		response += "Mod: " + fs_game.string + (!manifest.empty() ? " (manifest: " + manifest + ")" : "") + "\n";
		response += "----------------\n";

		G_PrintMsg( client.getEnt(), response );
		return true;
	}
	else if( cmdString == "cvarinfo" )
	{
		GENERIC_CheatVarResponse( client, cmdString, argsString, argc );
		return true;
	}
	else if( cmdString == "kill" )
	{
		return true;
	}

	return false;
}

// When this function is called the weights of items have been reset to their default values,
// this means, the weights *are set*, and what this function does is scaling them depending
// on the current bot status.
// Player, and non-item entities don't have any weight set. So they will be ignored by the bot
// unless a weight is assigned here.
bool GT_UpdateBotStatus( Entity @ent )
{
	Entity @goal;
	Bot @bot;

	@bot = @ent.client.getBot();
	if ( @bot == null )
		return false;

	float offensiveStatus = GENERIC_OffensiveStatus( ent );

	// loop all the goal entities
	for ( int i = AI::GetNextGoal( AI::GetRootGoal() ); i != AI::GetRootGoal(); i = AI::GetNextGoal( i ) )
	{
		@goal = @AI::GetGoalEntity( i );

		// by now, always full-ignore not solid entities
		if ( goal.solid == SOLID_NOT )
		{
			bot.setGoalWeight( i, 0 );
			continue;
		}

		if ( @goal.client != null )
		{
			if( !goal.client.isBot() )
				bot.setGoalWeight( i, GENERIC_PlayerWeight( ent, goal ) * 2.5 * offensiveStatus );
			continue;
		}

		// ignore it
		bot.setGoalWeight( i, 0 );
	}

	return true; // handled by the script
}

// select a spawning point for a player
Entity @GT_SelectSpawnPoint( Entity @self )
{
	return GENERIC_SelectBestRandomSpawnPoint( null, "info_player_deathmatch" );
}

String @GT_ScoreboardMessage( uint maxlen )
{
	String scoreboardMessage = "";
	String entry;

	Team @alpha = @G_GetTeam( TEAM_ALPHA );
	Team @beta = @G_GetTeam( TEAM_BETA );

	// &t = team tab, team tag, team score (doesn't apply)
	entry = "&t " + int( TEAM_ALPHA ) + " " + alpha.stats.score + " " + alpha.ping + " ";
	if ( scoreboardMessage.len() + entry.len() < maxlen )
		scoreboardMessage += entry;

	for ( int j = 0; @alpha.ent( j ) != null; j++ )
	{
		int playerID;
		Entity @ent = @alpha.ent( j );
		Client @client = ent.client;

		if ( match.getState() != MATCH_STATE_PLAYTIME )
			playerID = client.playerNum;
		else
			playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

		entry = "&p " + playerID + " " + client.stats.score + " " + client.ping + " ";
		if ( scoreboardMessage.len() + entry.len() < maxlen )
			scoreboardMessage += entry;
	}

	// &t = team tab, team tag, team score
	entry = "&t " + int( TEAM_BETA ) + " " + beta.stats.score + " " + beta.ping + " ";
	if ( scoreboardMessage.len() + entry.len() < maxlen )
		scoreboardMessage += entry;
			
	return scoreboardMessage;
}

// Some game actions trigger score events. These are events not related to killing
// oponents, like capturing a flag
// Warning: client can be null
void GT_ScoreEvent( Client @client, const String &score_event, const String &args )
{
	if ( score_event == "dmg" )
	{
	}
	else if ( score_event == "kill" )
	{
		Entity @attacker = null;

		if ( @client != null )
			@attacker = @client.getEnt();

		int arg1 = args.getToken( 0 ).toInt();
		int arg2 = args.getToken( 1 ).toInt();

		// target, attacker, inflictor
		enduranceMatch.playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
	}
	else if ( score_event == "award" )
	{
	}
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
	enduranceMatch.playerRespawn( ent, old_team, new_team );
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
	if ( match.scoreLimitHit() || match.timeLimitHit() || match.suddenDeathFinished() )
		match.launchState( match.getState() + 1 );

	if ( match.getState() >= MATCH_STATE_POSTMATCH )
		return;

	enduranceMatch.think();
}

// The game has detected the end of the match state, but it
// doesn't advance it before calling this function.
// This function must give permission to move into the next
// state by returning true.
bool GT_MatchStateFinished( int incomingMatchState )
{
	// ** MISSING EXTEND PLAYTIME CHECK **

	if ( match.getState() <= MATCH_STATE_WARMUP && incomingMatchState > MATCH_STATE_WARMUP
			&& incomingMatchState < MATCH_STATE_POSTMATCH )
		match.startAutorecord();

	if ( match.getState() == MATCH_STATE_POSTMATCH )
		match.stopAutorecord();

	return true;
}

// the match state has just moved into a new state. Here is the
// place to set up the new state rules
void GT_MatchStateStarted()
{
	switch ( match.getState() )
	{
	case MATCH_STATE_WARMUP:
		GENERIC_SetUpWarmup();
	
		G_RemoveDeadBodies();
		G_RemoveAllProjectiles();
	
		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = false;
		gametype.countdownEnabled = false;

		// set spawnsystem type to not respawn the humans when they die
		for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
			gametype.setTeamSpawnsystem( team, team == TEAM_ALPHA ? SPAWNSYSTEM_INSTANT : SPAWNSYSTEM_HOLD, 0, 0, false );
	
		enduranceMatch.warmup();
		break;

	case MATCH_STATE_COUNTDOWN:
		GENERIC_SetUpCountdown( false );

		gametype.pickableItemsMask |= IT_POWERUP;
		G_Items_RespawnByType( IT_POWERUP, 0, brandom( 20, 40 ) );

		gametype.shootingDisabled = false;
		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = false;
		gametype.countdownEnabled = false;

		enduranceMatch.countdown();
		break;

	case MATCH_STATE_PLAYTIME:
		gametype.setTeamSpawnsystem( TEAM_ALPHA, SPAWNSYSTEM_HOLD, 0, 0, true );
	
		enduranceMatch.begin();
		break;

	case MATCH_STATE_POSTMATCH:
		GENERIC_SetUpEndMatch();
		enduranceMatch.end();
		break;

	default:
		break;
	}
}

// the gametype is shutting down cause of a match restart or map change
void GT_Shutdown()
{
}

// The map entities have just been spawned. The level is initialized for
// playing, but nothing has yet started.
void GT_SpawnGametype()
{
}

// Important: This function is called before any entity is spawned, and
// spawning entities from it is forbidden. If you want to make any entity
// spawning at initialization do it in GT_SpawnGametype, which is called
// right after the map entities spawning.

void GT_InitGametype()
{
	gametype.title = "Rekt";
	gametype.version = "1.00";
	gametype.author = "Warsow Development Team";

	enduranceMatch.init();

	// if the gametype doesn't have a config file, create it
	if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
	{
		String config;

		// the config file doesn't exist or it's empty, create it
		config = "// '" + gametype.title + "' gametype configuration file\n"
				+ "// This config will be executed each time the gametype is started\n"
				+ "\n\n// map rotation\n"
				+ "set g_maplist \"wda5\" // list of maps in automatic rotation\n"
				+ "set g_maprotation \"0\"   // 0 = same map, 1 = in order, 2 = random\n"
				+ "\n// game settings\n"
				+ "set g_scorelimit \"0\"\n"
				+ "set g_timelimit \"1.5\"\n"
				+ "set g_warmup_timelimit \"1\"\n"
				+ "set g_match_extendedtime \"0\"\n"
				+ "set g_allow_falldamage \"0\"\n"
				+ "set g_allow_selfdamage \"0\"\n"
				+ "set g_allow_teamdamage \"0\"\n"
				+ "set g_allow_stun \"1\"\n"
				+ "set g_teams_maxplayers \"0\"\n"
				+ "set g_teams_allow_uneven \"1\"\n"
				+ "set g_countdown_time \"3\"\n"
				+ "set g_maxtimeouts \"3\" // -1 = unlimited\n"
				+ "set g_numbots \"0\"\n"				 
				+ "\necho \"" + gametype.name + ".cfg executed\"\n";

		G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
		G_Print( "Created default config file for '" + gametype.name + "'\n" );
		G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
	}

	gametype.spawnableItemsMask = ( IT_AMMO | IT_ARMOR | IT_POWERUP );
	gametype.respawnableItemsMask = gametype.spawnableItemsMask;
	gametype.dropableItemsMask = gametype.spawnableItemsMask | IT_HEALTH;
	gametype.pickableItemsMask = gametype.dropableItemsMask & ~IT_POWERUP;

	gametype.isTeamBased = true;
	gametype.isRace = false;
	gametype.hasChallengersQueue = true;
	gametype.hasChallengersRoulette = true;
	gametype.maxPlayersPerTeam = 0;

	gametype.ammoRespawn = 20;
	gametype.armorRespawn = 25;
	gametype.weaponRespawn = 5;
	gametype.healthRespawn = 15;
	gametype.powerupRespawn = 90;
	gametype.megahealthRespawn = 20;
	gametype.ultrahealthRespawn = 40;

	gametype.readyAnnouncementEnabled = false;
	gametype.scoreAnnouncementEnabled = false;
	gametype.countdownEnabled = false;
	gametype.mathAbortDisabled = false;
	gametype.shootingDisabled = false;
	gametype.infiniteAmmo = false;
	gametype.canForceModels = FORCED_MODEL_BETA.empty();
	gametype.canShowMinimap = false;
	gametype.teamOnlyMinimap = false;
	gametype.removeInactivePlayers = true;

	gametype.mmCompatible = true;
	gametype.disableObituaries = true;
	
	gametype.spawnpointRadius = 256;
	
	gametype.numBots = MAXIMUM_BOT_COUNT;
	gametype.forceTeamHumans = TEAM_ALPHA;
	gametype.forceTeamBots = TEAM_BETA;

	if ( gametype.isInstagib )
		gametype.spawnpointRadius *= 2;

	// set spawnsystem type to instant while players join
	for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
		gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

	// define the scoreboard layout
	G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %i 52 %l 48" );
	G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Score Ping" );

	// add commands
	G_RegisterCommand( "gametype" );
	G_RegisterCommand( "kill" );
	
	if( !FORCED_MODEL_BETA.empty() )
		G_ModelIndex( FORCED_MODEL_BETA, true );

	G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
