/*
Copyright (C) 2009-2015 Chasseur de bots

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

void target_botmatch_start_use( Entity @self, Entity @other, Entity @activator )
{
	gametype.numBots = 1;
}

void target_botmatch_start( Entity @self )
{
	@self.use = target_botmatch_start_use;
}

///*****************************************************************
/// LOCAL FUNCTIONS
///*****************************************************************

// a player has just died. The script is warned about it so it can account scores
void TUTORIAL_playerKilled( Entity @target, Entity @attacker, Entity @inflictor )
{
    if ( match.getState() != MATCH_STATE_PLAYTIME )
        return;

    if ( @target == null || @target.client == null )
        return;

    // update player score based on player stats
	if ( target.classname != "bot" ) {
		target.client.stats.setScore( target.client.stats.frags - target.client.stats.suicides );

		// copy the player score into the team score
		G_GetTeam( target.team ).stats.setScore( target.client.stats.score );
	}
	if ( @attacker != null && @attacker.client != null && attacker.classname != "bot" ) {
		attacker.client.stats.setScore( attacker.client.stats.frags - attacker.client.stats.suicides );
		G_GetTeam( attacker.team ).stats.setScore( attacker.client.stats.score );
	}
	
    // drop items
    if ( ( G_PointContents( target.origin ) & CONTENTS_NODROP ) == 0 )
    {
        target.dropItem( AMMO_PACK );

        if ( target.client.inventoryCount( POWERUP_QUAD ) > 0 )
        {
            target.dropItem( POWERUP_QUAD );
            target.client.inventorySetCount( POWERUP_QUAD, 0 );
        }

        if ( target.client.inventoryCount( POWERUP_SHELL ) > 0 )
        {
            target.dropItem( POWERUP_SHELL );
            target.client.inventorySetCount( POWERUP_SHELL, 0 );
        }
    }
    
	if( attacker.classname != "bot" ) {
		award_playerKilled( @target, @attacker, @inflictor );
	}
}

void TUTORIAL_SetUpCountdown()
{
    gametype.pickableItemsMask = 0; // disallow item pickup
    gametype.dropableItemsMask = 0; // disallow item drop
	GENERIC_SetUpCountdown();
}
		
void TUTORIAL_SetUpMatch()
{
    int i, j;
    Entity @ent;
    Team @team;

    G_RemoveAllProjectiles();
    G_RemoveDeadBodies();

    gametype.pickableItemsMask = gametype.spawnableItemsMask;
    gametype.dropableItemsMask = gametype.spawnableItemsMask;

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
    G_Items_RespawnByType( IT_ARMOR, 0, 15 );
    G_Items_RespawnByType( IT_HEALTH, HEALTH_MEGA, 15 );
    G_Items_RespawnByType( IT_HEALTH, HEALTH_ULTRA, 15 );
    G_Items_RespawnByType( IT_POWERUP, 0, brandom( 20, 40 ) );

	gametype.shootingDisabled = false;

	if( match.getState() == MATCH_STATE_WARMUP )
	{
		gametype.countdownEnabled = false;
		gametype.readyAnnouncementEnabled = true;
		gametype.scoreAnnouncementEnabled = false;
	}
	else
	{
		gametype.countdownEnabled = true;
		gametype.readyAnnouncementEnabled = false;
		gametype.scoreAnnouncementEnabled = true;

		// Countdowns should be made entirely client side, because we now can
		int soundindex = G_SoundIndex( "sounds/announcer/countdown/fight0" + (1 + (rand() & 1)) );
		G_AnnouncerSound( null, soundindex, GS_MAX_TEAMS, false, null );
	}
}

void TUTORIAL_EndMatch()
{
	// find the winning player
	int bestScore = 0;
	Client @winner = null;

	for ( int i = 0; i < maxClients; i++ )
	{
		Client @client = @G_GetClient( i );
		if( client.stats.score > bestScore ) {
			@winner = client;
			bestScore = client.stats.score;
		}
	}
	
	if( @winner != null ) {
		winner.addAward( S_COLOR_YELLOW + "Tutorial completed!" );
	}
	
	GENERIC_SetUpEndMatch();

    gametype.pickableItemsMask = 0;
    gametype.dropableItemsMask = 0;
	gametype.countdownEnabled = false;	
}

///*****************************************************************
/// MODULE SCRIPT CALLS
///*****************************************************************

bool GT_Command( Client @client, const String &cmdString, const String &argsString, int argc )
{
    if ( cmdString == "drop" )
    {
        String token;

        for ( int i = 0; i < argc; i++ )
        {
            token = argsString.getToken( i );
            if ( token.len() == 0 )
                break;

            if ( token == "weapon" || token == "fullweapon" )
            {
                GENERIC_DropCurrentWeapon( client, true );
            }
            else if ( token == "strong" )
            {
                GENERIC_DropCurrentAmmoStrong( client );
            }
            else
            {
                GENERIC_CommandDropItem( client, token );
            }
        }

        return true;
    }
    else if ( cmdString == "gametype" )
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
    else if ( cmdString == "cvarinfo" )
    {
        GENERIC_CheatVarResponse( client, cmdString, argsString, argc );
        return true;
    }

    return false;
}

// When this function is called the weights of items have been reset to their default values,
// this means, the weights *are set*, and what this function does is scaling them depending
// on the current bot status.
// Player, and non-item entities don't have any weight set. So they will be ignored by the bot
// unless a weight is assigned here.
bool GT_UpdateBotStatus( Entity @self )
{
    return false; // let the default code handle it itself
}

// select a spawning point for a player
Entity @GT_SelectSpawnPoint( Entity @self )
{
	if( self.classname == "bot" || gametype.numBots > 0 )
		return GENERIC_SelectBestRandomSpawnPoint( self, "info_player_deathmatch" );
	return GENERIC_SelectBestRandomSpawnPoint( self, "info_player_start" );		
}

String @GT_ScoreboardMessage( uint maxlen )
{
    String scoreboardMessage = "";
    String entry;
    Team @team;
    Entity @ent;
    int i, t;

    for ( t = TEAM_ALPHA; t < GS_MAX_TEAMS; t++ )
    {
        @team = @G_GetTeam( t );

        // &t = team tab, team tag, team score (doesn't apply), team ping (doesn't apply)
        entry = "&t " + t + " " + team.stats.score + " " + team.ping + " ";
        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;

        for ( i = 0; @team.ent( i ) != null; i++ )
        {
            @ent = @team.ent( i );

            int playerID = ( ent.isGhosting() && ( match.getState() == MATCH_STATE_PLAYTIME ) ) ? -( ent.playerNum + 1 ) : ent.playerNum;

            // "Name Clan Score Frags Sui Ping R"
            entry = "&p " + playerID + " "
                    + ent.client.clanName + " "
                    + ent.client.stats.score + " "
                    + ent.client.stats.frags + " "
                    + ent.client.stats.suicides + " "
                    + ent.client.ping + " "
                    + ( ent.client.isReady() ? "1" : "0" ) + " ";

            if ( scoreboardMessage.len() + entry.len() < maxlen )
                scoreboardMessage += entry;
        }
    }

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
        TUTORIAL_playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
    }
    else if ( score_event == "award" )
    {
    }
    else if ( score_event == "enterGame" )
    {
    }
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
    if ( ent.isGhosting() )
        return;

	ent.client.inventorySetCount( WEAP_GUNBLADE, 1 );
	ent.client.inventorySetCount( AMMO_GUNBLADE, ( gametype.numBots > 0 ) ? 1 : 0 );

    // select rocket launcher if available
    if ( ent.client.canSelectWeapon( WEAP_ROCKETLAUNCHER ) )
        ent.client.selectWeapon( WEAP_ROCKETLAUNCHER );
    else
        ent.client.selectWeapon( -1 ); // auto-select best weapon in the inventory
		
    // add a teleportation effect
    ent.respawnEffect();
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
    if ( match.scoreLimitHit() || match.timeLimitHit() || match.suddenDeathFinished() )
        match.launchState( match.getState() + 1 );

	GENERIC_Think();

    if ( match.getState() >= MATCH_STATE_POSTMATCH )
        return;

    // check maxHealth rule and max armor rule
    for ( int i = 0; i < maxClients; i++ )
    {
        Entity @ent = @G_GetClient( i ).getEnt();
        if ( ent.client.state() >= CS_SPAWNED && ent.team != TEAM_SPECTATOR )
        {
            if ( ent.health > ent.maxHealth ) {
                ent.health -= ( frameTime * 0.001f );
				// fix possible rounding errors
				if( ent.health < ent.maxHealth ) {
					ent.health = ent.maxHealth;
				}
			}

            ent.client.inventorySetCount( AMMO_GUNBLADE, ( gametype.numBots > 0 ) ? 1 : 0 );
        }
    }
}

// The game has detected the end of the match state, but it
// doesn't advance it before calling this function.
// This function must give permission to move into the next
// state by returning true.
bool GT_MatchStateFinished( int incomingMatchState )
{
    // check maxHealth rule
    for ( int i = 0; i < maxClients; i++ )
    {
        Entity @ent = @G_GetClient( i ).getEnt();
        if ( ent.client.state() >= CS_SPAWNED && ent.team != TEAM_SPECTATOR )
        {
            if ( ent.health > ent.maxHealth ) {
                ent.health -= ( frameTime * 0.001f );
				// fix possible rounding errors
				if( ent.health < ent.maxHealth ) {
					ent.health = ent.maxHealth;
				}
			}
        }
    }
    return true;
}

// the match state has just moved into a new state. Here is the
// place to set up the new state rules
void GT_MatchStateStarted()
{
    switch ( match.getState() )
    {
    case MATCH_STATE_WARMUP:
		TUTORIAL_SetUpMatch();
        break;

    case MATCH_STATE_COUNTDOWN:
		TUTORIAL_SetUpCountdown();
        break;

    case MATCH_STATE_PLAYTIME:
        TUTORIAL_SetUpMatch();
        break;

    case MATCH_STATE_POSTMATCH:
		TUTORIAL_EndMatch();
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
    gametype.title = "Tutorial";
    gametype.version = "1.00";
    gametype.author = "Warsow Development Team";

    // if the gametype doesn't have a config file, create it
    if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
    {
        String config;

        // the config file doesn't exist or it's empty, create it
        config = "// '" + gametype.title + "' gametype configuration file\n"
                 + "// This config will be executed each time the gametype is started\n"
                 + "\n\n// map rotation\n"
                 + "set g_maplist \"\" // list of maps in automatic rotation\n"
                 + "set g_maprotation \"0\"   // 0 = same map, 1 = in order, 2 = random\n"
                 + "\n// game settings\n"
                 + "set g_scorelimit \"5\"\n"
                 + "set g_timelimit \"0\"\n"
                 + "set g_warmup_timelimit \"0\"\n"
                 + "set g_match_extendedtime \"2\"\n"
                 + "set g_allow_falldamage \"1\"\n"
                 + "set g_allow_selfdamage \"1\"\n"
                 + "set g_allow_teamdamage \"1\"\n"
                 + "set g_allow_stun \"1\"\n"
                 + "set g_teams_maxplayers \"1\"\n"
                 + "set g_teams_allow_uneven \"1\"\n"
                 + "set g_countdown_time \"5\"\n"
                 + "set g_numbots \"0\"\n"
                 + "set g_maxtimeouts \"-1\" // -1 = unlimited\n"
                 + "set g_instagib \"0\"\n"
                 + "\necho " + gametype.name + ".cfg executed\n";

        G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
        G_Print( "Created default config file for '" + gametype.name + "'\n" );
        G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
    }

    gametype.spawnableItemsMask = ( IT_AMMO | IT_WEAPON | IT_POWERUP | IT_HEALTH | IT_ARMOR );
    if ( gametype.isInstagib )
        gametype.spawnableItemsMask &= ~uint(G_INSTAGIB_NEGATE_ITEMMASK);

    gametype.respawnableItemsMask = gametype.spawnableItemsMask;
    gametype.dropableItemsMask = 0;
    gametype.pickableItemsMask = gametype.spawnableItemsMask;

    gametype.isTeamBased = true;
    gametype.isRace = false;
	gametype.isTutorial = true;
    gametype.hasChallengersQueue = true;
    gametype.maxPlayersPerTeam = 1;

    gametype.ammoRespawn = 10;
    gametype.armorRespawn = 15;
    gametype.weaponRespawn = 10;
    gametype.healthRespawn = 15;
    gametype.powerupRespawn = 90;
    gametype.megahealthRespawn = 20;
    gametype.ultrahealthRespawn = 40;

    gametype.readyAnnouncementEnabled = true;
    gametype.scoreAnnouncementEnabled = false;
    gametype.countdownEnabled = true;
    gametype.mathAbortDisabled = true;
    gametype.shootingDisabled = false;
    gametype.infiniteAmmo = false;
    gametype.canForceModels = true;
    gametype.canShowMinimap = false;
	gametype.teamOnlyMinimap = true;

    gametype.spawnpointRadius = 0;
	
	gametype.numBots = 0;

	gametype.removeInactivePlayers = false;

    if ( gametype.isInstagib )
        gametype.spawnpointRadius *= 2;

    // set spawnsystem type
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

    // define the scoreboard layout
    G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %s 52 %i 44 %i 42 %i 38 %l 38 %r l1" );
    G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Clan Score Frags Sui Ping R" );

    // add commands
    G_RegisterCommand( "drop" );
    G_RegisterCommand( "gametype" );

    G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
