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
// Headhunt Deathmatch 1.0 by kiki
// crizis@warsow.net
// Based on DM gt

// Scoring:
// 5 points for tagging player
// Tagged player gets 1 point for each 2 seconds staying alive
// 10 points for killing tagged player
// 5 points for headhunt defense (killing someone while being tagged)
// -25 points for killing someone else than tagged player
// 1 point for hitting someone with gunblade when tagged

// Indicators if some player has been tagged
int hhHasPlayerTagged = 0;
int hhFoundTaggedPlayer = 0;

// playerNum and name of the tagged player. 999 when no one is tagged, dummy number
int hhTaggedPlayer = 999;
String hhTaggedPlayerName;

// playerNum of the player who tagged the target.
int hhTaggerPlayer;

// Indicator if clients have been informed about target player
int hhClientsInformed = 0;

// For checking scoring timing
int hhTime = levelTime / 2000;
int hhTimeOld = 0;
int hhTagTime = levelTime / 1000;
int hhTagTimeOld = 0;

// Autotag
// if 1, player who killed the last tag, will become tag.
// Changed with callvote autotag 0/1
int autoTag = 1;

// Tag timer
int[] tagTimes( maxClients );

///*****************************************************************
/// HEADHUNT FUNCTIONS
///*****************************************************************

// Tags the player when shot
void HEADHUNT_checkTag( Entity @target, Entity @attacker, Entity @inflictor )
{
    if ( @target == null || @attacker == null )
        return;
    if ( @target.client == null || @attacker.client == null )
        return;

    if ( hhHasPlayerTagged == 0 )
    {
        HEADHUNT_tagPlayer( target , attacker, inflictor );
    }
    else
    {
        // Spam shooter to shoot only tagged player.
        // This should be printed in middle of the screen rather than award (like tdm warning).
        if ( (target.client.playerNum != hhTaggedPlayer)
                && hhTaggedPlayer != 999
                && (attacker.client.playerNum != hhTaggedPlayer) )
        {
            G_CenterPrintMsg( attacker , '^1Headhunt only tagged players!');
        }

        // Give score for the grave gunblade defender!
        if ( attacker.client.playerNum == hhTaggedPlayer )
        {
            if ( attacker.client.weapon == WEAP_GUNBLADE )
            {
                attacker.client.addAward("^4Gunblade defender bonus!");
                attacker.client.stats.addScore( 1 );
            }
        }

    }
}

// Clear headhunt state
void HEADHUNT_clearState()
{
    // Clear tag state
    hhHasPlayerTagged = 0;
    hhTaggedPlayer = 999; // dummy number =P
    hhTaggerPlayer = 999; // dummy number =P
    hhClientsInformed = 0;
    hhTaggedPlayerName = "";
}

void HEADHUNT_tagPlayer( Entity @target, Entity @attacker, Entity @inflictor )
{
    // Display award for attacker && tagged player
    attacker.client.addAward( "You tagged^7 " +  target.client.name + "!");
    attacker.client.stats.addScore( 5 );
    target.client.addAward("^3You are tagged, good luck!\n");

    // Save taggers
    hhTaggedPlayer = target.client.playerNum;
    hhTaggedPlayerName = target.client.name;
    hhTaggerPlayer = attacker.client.playerNum;

    // Reset tag's inventory
    target.client.inventoryClear();
    target.client.inventorySetCount(WEAP_GUNBLADE, 1);
    target.client.inventorySetCount(AMMO_GUNBLADE, 1);
    target.client.selectWeapon(WEAP_GUNBLADE);

    // Tagged indicator + little workaround to clean up
    // centered message from 'no on is tagged' message.
    G_CenterPrintMsg( null , '');
    hhHasPlayerTagged = 1;
}

// a player has just died. The script is warned about it so it can account scores
void HEADHUNT_playerKilled( Entity @target, Entity @attacker, Entity @inflictor )
{
    if ( match.getState() != MATCH_STATE_PLAYTIME )
        return;

    if ( @target.client == null )
        return;

    // update player score

    if ( @attacker != null && @attacker.client != null )
    {
        if (target.client.playerNum == hhTaggedPlayer && attacker.client.playerNum != target.client.playerNum)
        {
            attacker.client.stats.addScore( 10 );
            attacker.client.addAward('You headhunted^7 ' + target.client.name + ' !\n');
            // Clear headhunt status
            HEADHUNT_clearState();

            // Mark attacker as tag if autotag is on
            if ( autoTag == 1 )
            {
                attacker.client.addAward("^3You are tagged, good luck!\n");

                // Save taggers
                hhTaggedPlayer = attacker.client.playerNum;
                hhTaggedPlayerName = attacker.client.name;
                hhTaggerPlayer = 999;

                // Reset tag's inventory
                attacker.client.inventoryClear();
                attacker.client.inventorySetCount(WEAP_GUNBLADE, 1);
                attacker.client.inventorySetCount(AMMO_GUNBLADE, 1);
                attacker.client.selectWeapon(WEAP_GUNBLADE);

                // Tagged indicator + little workaround to clean up
                // centered message from 'no on is tagged' message.
                G_CenterPrintMsg( null , '');
                hhHasPlayerTagged = 1;
            }

        }
        else
        {
            if ( attacker.client.playerNum == hhTaggedPlayer && target.client.playerNum != hhTaggedPlayer )
            {
                // Headhunt defense bonus
                attacker.client.stats.addScore( 5 );
                attacker.client.addAward('^4Headhunt defense!\n');
            }
            else
            {
                // Miskill punishment
                attacker.client.stats.addScore( -25 );
                attacker.client.addAward('^1headhunt miskill punishment!\n');
            }
        }
    }
    else
    {
        if (target.client.playerNum == hhTaggedPlayer )
        {
            // Selfkill punishment
            target.client.stats.addScore( -15 );
            target.client.addAward('^1headhunt selfkill punishment!\n');

            // Clear headhunt status
            HEADHUNT_clearState();
        }
    }

    // drop items
    if ( ( G_PointContents( target.origin ) & CONTENTS_NODROP ) == 0 )
    {
        // drop the weapon
        if ( target.client.weapon > WEAP_GUNBLADE )
        {
            GENERIC_DropCurrentWeapon( target.client, true );
        }

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
}

String HEADHUNT_TimeToString( uint time )
{
    // convert times to printable form
    String minsString, secsString;
    uint min, sec;

    min = time / 60;
    sec = time - ( min * 60 ) ;

    if ( min == 0 )
        minsString = "00";
    else if ( min < 10 )
        minsString = "0" + min;
    else
        minsString = min;

    if ( sec == 0 )
        secsString = "00";
    else if ( sec < 10 )
        secsString = "0" + sec;
    else
        secsString = sec;

    return minsString + ":" + secsString;
}

// minimap
void tagged_player_minimap_think ( Entity @ent )
{
    // locate tagged player
    if ( hhTaggedPlayer != 999 )
    {
        Entity @taggedPlayer = @G_GetClient( hhTaggedPlayer ).getEnt();
        ent.origin = taggedPlayer.origin;
        ent.svflags &= ~ SVF_NOCLIENT;
    }
    else
    {
        // Hide entity from minimap minimap
        ent.svflags |= SVF_NOCLIENT;
    }
    ent.nextThink = levelTime + 1; // set to think again next frame
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
    else if ( cmdString == "cvarinfo" )
    {
        GENERIC_CheatVarResponse( client, cmdString, argsString, argc );
        return true;
    }
    // example of registered command
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
    else if ( cmdString == "callvotevalidate" )
    {
        String votename = argsString.getToken( 0 );
        if ( votename == "autotag" )
        {
            String voteArg = argsString.getToken( 1 );
            if ( voteArg.len() < 1 )
            {
                client.printMessage( "Callvote " + votename + " requires at least one argument\n" );
                return false;
            }

            int value = voteArg.toInt();
            if ( value == 0 )
            {
                return true;
            }
            if ( value == 1 )
            {
                return true;
            }

            return false;
        }

        client.printMessage( "Unknown callvote " + votename + "\n" );
        return false;
    }
    else if ( cmdString == "callvotepassed" )
    {
        String votename = argsString.getToken( 0 );
        if ( votename == "autotag" )
        {
            autoTag = argsString.getToken( 1 ).toInt();
        }

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
            // Someone is tag so assign him as priority
            if ( hhTaggedPlayer != 999 )
            {
                if ( goal.client.playerNum == hhTaggedPlayer )
                {
                    bot.setGoalWeight( i, GENERIC_PlayerWeight( ent, goal ) * offensiveStatus );
                }
                else
                {
                    bot.setGoalWeight( i, 0 );
                }
            }

            // No one is tagged, default attack mode
            if ( hhTaggedPlayer == 999 )
            {
                bot.setGoalWeight( i, GENERIC_PlayerWeight( ent, goal ) * offensiveStatus );
            }
        }
    }

    return true;
}

// select a spawning point for a player
Entity @GT_SelectSpawnPoint( Entity @self )
{
    return GENERIC_SelectBestRandomSpawnPoint( self, "info_player_deathmatch" );
}

String @GT_ScoreboardMessage( uint maxlen )
{
    String scoreboardMessage = "";
    String entry;
    Team @team;
    Entity @ent;
    int i;

    @team = @G_GetTeam( TEAM_PLAYERS );

    // &t = team tab, team tag, team score (doesn't apply), team ping (doesn't apply)
    entry = "&t " + int( TEAM_PLAYERS ) + " " + team.stats.score + " 0 ";
    if ( scoreboardMessage.len() + entry.len() < maxlen )
        scoreboardMessage += entry;

    for ( i = 0; @team.ent( i ) != null; i++ )
    {
        @ent = @team.ent( i );

        int playerID = ( ent.isGhosting() && ( match.getState() == MATCH_STATE_PLAYTIME ) ) ? -( ent.playerNum + 1 ) : ent.playerNum;

        entry = "&p " + playerID + " "
                + ent.client.clanName + " "
                + ent.client.stats.score + " "
                + HEADHUNT_TimeToString( tagTimes [ ent.playerNum ] ) + " "
                + ent.client.ping + " "
                + ( ent.client.isReady() ? "1" : "0" ) + " ";

        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;
    }

    return scoreboardMessage;
}

// Warning: client can be null
void GT_ScoreEvent( Client @client, const String &score_event, const String &args )
{
    Entity @attacker = null;

    if ( @client != null )
        @attacker = @client.getEnt();

    int arg1 = args.getToken( 0 ).toInt();
    int arg2 = args.getToken( 1 ).toInt();

    if ( score_event == "dmg" )
    {
        HEADHUNT_checkTag( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
    }
    else if ( score_event == "kill" )
    {
        HEADHUNT_playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
    }
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
    // Clear headhunt state if tagged player disconnects
    if ( hhTaggedPlayer == ent.client.playerNum )
    {
        if ( (old_team != new_team) || new_team == TEAM_SPECTATOR )
        {
            HEADHUNT_clearState();
            int tmp = ent.client.playerNum;
            tagTimes [ tmp ] = 0;
        }
    }

    if ( ent.isGhosting() )
        return;

    if ( gametype.isInstagib )
    {
        ent.client.inventoryGiveItem( WEAP_INSTAGUN );
        ent.client.inventorySetCount( AMMO_INSTAS, 1 );
        ent.client.inventorySetCount( AMMO_WEAK_INSTAS, 1 );
    }
    else
    {
        Item @item;
        Item @ammoItem;

        // the gunblade can't be given (because it can't be dropped)
        ent.client.inventorySetCount( WEAP_GUNBLADE, 1 );
        ent.client.inventorySetCount( AMMO_GUNBLADE, 1 ); // enable gunblade blast

        if ( match.getState() <= MATCH_STATE_WARMUP )
        {
            ent.client.inventoryGiveItem( ARMOR_YA );
            ent.client.inventoryGiveItem( ARMOR_YA );

            // give all weapons
            for ( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ )
            {
                if ( i == WEAP_INSTAGUN ) // dont add instagun...
                    continue;

                ent.client.inventoryGiveItem( i );

                @item = @G_GetItem( i );

                @ammoItem = item.weakAmmoTag == AMMO_NONE ? null : @G_GetItem( item.weakAmmoTag );
                if ( @ammoItem != null )
                    ent.client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );

                @ammoItem = @G_GetItem( item.ammoTag );
                if ( @ammoItem != null )
                    ent.client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );
            }

            // Clear headhunt in warmup
            HEADHUNT_clearState();
            int tmp = ent.client.playerNum;
            tagTimes [ tmp ] = 0;
        }
        else
        {
            ent.client.inventoryGiveItem( ARMOR_GA );
        }
    }

    // select rocket launcher if available
    ent.client.selectWeapon( -1 ); // auto-select best weapon in the inventory

    // add a teleportation effect
    ent.respawnEffect();
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
    if ( match.scoreLimitHit() || match.timeLimitHit() || match.suddenDeathFinished() )
        match.launchState( match.getState() + 1 );

    if ( match.getState() >= MATCH_STATE_POSTMATCH )
        return;

	GENERIC_Think();

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

        // Draw tagged player with Quad frame + add score for staying alive for every 2 sec
        if ( (ent.client.playerNum == hhTaggedPlayer) && hhTaggedPlayer != 999 && ent.team != TEAM_SPECTATOR )
        {
            ent.effects |= EF_SHELL;

            // Score timer (2 sec)
            if ( hhTimeOld == 0 )
                hhTimeOld = levelTime / 2000;

            hhTime=levelTime/2000;
            if ( hhTime > hhTimeOld )
            {
                hhTimeOld = levelTime / 2000;
                ent.client.stats.addScore( 1 );
            }

            // Tag timer (1 sec)
            if ( hhTagTimeOld == 0 )
                hhTagTimeOld = levelTime / 1000;

            hhTagTime=levelTime/1000;
            if ( hhTagTime > hhTagTimeOld )
            {
                hhTagTimeOld = levelTime / 1000;

                int tmpplayerNum = ent.client.playerNum;

                tagTimes [ tmpplayerNum ] += 1;
            }

        }

        // Inform players about tag target
        if ( hhTaggedPlayer != 999 && ent.team != TEAM_SPECTATOR )
        {
            if ( hhClientsInformed == 0 && ent.client.playerNum != hhTaggedPlayer )
            {
                ent.client.addAward("Hunt down " + hhTaggedPlayerName + "!");
            }
        }

        // Draws target to the hud
        if ( hhTaggedPlayer != 999 && ent.team != TEAM_SPECTATOR )
        {
            if ( ent.client.playerNum != hhTaggedPlayer )
            {
                G_ConfigString( CS_GENERAL, "^7Headhunt " + hhTaggedPlayerName + "!" );
                ent.client.setHUDStat( STAT_MESSAGE_ALPHA, CS_GENERAL );
            }

            if ( ent.client.playerNum == hhTaggedPlayer )
            {
                G_ConfigString( CS_GENERAL + 1, "^7You are tagged! Good luck!" );
                ent.client.setHUDStat( STAT_MESSAGE_ALPHA, CS_GENERAL +1 );
            }
        }

        // Clear headhunt state
        if ( hhTaggedPlayer == 999 && ent.team != TEAM_SPECTATOR )
        {
            // Inform players that no one is being headhunted
            G_CenterPrintMsg( ent.client.getEnt() , '^7No one is being hunted, shoot someone to tag!');
            G_ConfigString( CS_GENERAL, "^7No one is being hunted" );
            ent.client.setHUDStat( STAT_MESSAGE_ALPHA, CS_GENERAL );
        }
    }

    // Notice that awards have been drawed to players
    if ( hhTaggedPlayer != 999 )
    {
        hhClientsInformed = 1;
    }

    // Clear headhunt state
    if ( hhTaggedPlayer == 999 )
    {
        HEADHUNT_clearState();
    }
}

// The game has detected the end of the match state, but it
// doesn't advance it before calling this function.
// This function must give permission to move into the next
// state by returning true.
bool GT_MatchStateFinished( int incomingMatchState )
{
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
        HEADHUNT_clearState();
        gametype.pickableItemsMask = gametype.spawnableItemsMask;
        gametype.dropableItemsMask = gametype.spawnableItemsMask;
        GENERIC_SetUpWarmup();
		SpawnIndicators::Create( "info_player_deathmatch", TEAM_PLAYERS );
        break;

    case MATCH_STATE_COUNTDOWN:
        HEADHUNT_clearState();
        gametype.pickableItemsMask = 0; // disallow item pickup
        gametype.dropableItemsMask = 0; // disallow item drop
        GENERIC_SetUpCountdown();
		SpawnIndicators::DeleteAll();
        break;

    case MATCH_STATE_PLAYTIME:
        gametype.pickableItemsMask = gametype.spawnableItemsMask;
        gametype.dropableItemsMask = gametype.spawnableItemsMask;
        GENERIC_SetUpMatch();
        break;

    case MATCH_STATE_POSTMATCH:
        gametype.pickableItemsMask = 0; // disallow item pickup
        gametype.dropableItemsMask = 0; // disallow item drop
        GENERIC_SetUpEndMatch();
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
    // Initialize minimap. Player location is automatically
    // updated at tagged_player_minimap_think
    Entity @minimap = @G_SpawnEntity( "tagged_player_minimap" );
    minimap.type = ET_MINIMAP_ICON;
    minimap.solid = SOLID_NOT;
    minimap.modelindex = G_ImageIndex( "gfx/indicators/radar_1" );
    minimap.frame = 32; // size in case of a ET_MINIMAP_ICON
    minimap.svflags |= SVF_BROADCAST;
    minimap.svflags |= SVF_NOCLIENT;
    minimap.linkEntity();
    minimap.nextThink = levelTime + 1;
}

// Important: This function is called before any entity is spawned, and
// spawning entities from it is forbidden. If you want to make any entity
// spawning at initialization do it in GT_SpawnGametype, which is called
// right after the map entities spawning.

void GT_InitGametype()
{
    gametype.title = "Headhunt";
    gametype.version = "1.04";
    gametype.author = "Warsow Development Team";

    // if the gametype doesn't have a config file, create it
    if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
    {
        String config;

        // the config file doesn't exist or it's empty, create it
        config = "// '" + gametype.title + "' gametype configuration file\n"
                 + "// Thi§s config will be executed each time the gametype is started\n"
                 + "\n\n// map rotation\n"
                 + "set g_maplist \"wdm1 wdm2 wdm4 wdm5 wdm6 wdm7 wdm9 wdm10 wdm11 wdm12 wdm13 wdm14 wdm15 wdm16 wdm17\" // list of maps in automatic rotation\n"
                 + "set g_maprotation \"1\"   // 0 = same map, 1 = in order, 2 = random\n"
                 + "\n// game settings\n"
                 + "set g_scorelimit \"0\"\n"
                 + "set g_timelimit \"15\"\n"
                 + "set g_warmup_timelimit \"1\"\n"
                 + "set g_match_extendedtime \"0\"\n"
                 + "set g_allow_falldamage \"0\"\n"
                 + "set g_allow_selfdamage \"0\"\n"
                 + "set g_allow_teamdamage \"0\"\n"
                 + "set g_allow_stun \"1\"\n"
                 + "set g_teams_maxplayers \"0\"\n"
                 + "set g_teams_allow_uneven \"0\"\n"
                 + "set g_countdown_time \"5\"\n"
                 + "set g_maxtimeouts \"3\" // -1 = unlimited\n"
                 + "\necho \"" + gametype.name + ".cfg executed\"\n";

        G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
        G_Print( "Created default config file for '" + gametype.name + "'\n" );
        G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
    }

    gametype.spawnableItemsMask = ( IT_WEAPON | IT_AMMO | IT_ARMOR | IT_POWERUP | IT_HEALTH );
    if ( gametype.isInstagib )
        gametype.spawnableItemsMask &= ~uint(G_INSTAGIB_NEGATE_ITEMMASK);

    HEADHUNT_clearState();
    gametype.respawnableItemsMask = gametype.spawnableItemsMask;
    gametype.dropableItemsMask = gametype.spawnableItemsMask;
    gametype.pickableItemsMask = gametype.spawnableItemsMask;

    gametype.isTeamBased = false;
    gametype.isRace = false;
    gametype.hasChallengersQueue = false;
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
    gametype.canForceModels = false;
    gametype.canShowMinimap = true;
    gametype.teamOnlyMinimap = false;

	gametype.mmCompatible = true;
	
    gametype.spawnpointRadius = 256;

    if ( gametype.isInstagib )
        gametype.spawnpointRadius *= 2;

    // set spawnsystem type
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

    // define the scoreboard layout
    G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %s 52 %i 52 %s 52 %l 48 %r l1" );
    G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Clan Score Tagtime Ping R" );

    // add commands
    G_RegisterCommand( "drop" );
    G_RegisterCommand( "gametype" );
    G_RegisterCallvote( "autotag", "<1 or 0>", "bool", "If enabled, tag fragger will become new tag automatically" );

    G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
