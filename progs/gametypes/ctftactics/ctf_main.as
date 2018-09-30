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

// Capture the flag Tactics
// by kiki & jal :>

const float CTF_AUTORETURN_TIME = 30.0f;
const int CTF_BONUS_RECOVERY = 2;
const int CTF_BONUS_STEAL = 1;
const int CTF_BONUS_CAPTURE = 10;
const int CTF_BONUS_CARRIER_KILL = 2;
const int CTF_BONUS_CARRIER_PROTECT = 2;
const int CTF_BONUS_FLAG_DEFENSE = 1;

const float CTF_FLAG_RECOVERY_BONUS_DISTANCE = 512.0f;
const float CTF_CARRIER_KILL_BONUS_DISTANCE = 512.0f;
const float CTF_OBJECT_DEFENSE_BONUS_DISTANCE = 512.0f;

int CTFT_RESPAWN_TIME = 15000;
int CTFT_MAXTURRETS_PER_TEAM = 4;
int CTFT_MAXDISPENSERS_PER_TEAM = 3;
int CTFT_TURRET_AP_COST = 50;
int CTFT_TURRET_STRONG_AP_COST = 125;
uint CTFT_ENGINEER_BUILD_COOLDOWN_TIME = 15000;
float CTFT_RUNNER_INVISIBILITY_MINLOAD = 20;
float CTFT_RUNNER_INVISIBILITY_MAXLOAD = 100;
uint CTFT_INVISIBILITY_COOLDOWN = 1000;
int CTFT_BATTLESUIT_AP_COST = 50;
int CTFT_BATTLESUIT_RUNNER_TIME = 3; 	// in seconds
int CTFT_BATTLESUIT_GRUNT_TIME = 8;		// in seconds
int CTFT_RAGE_TIME = 5000;
int CTFT_MEDIC_COOLDOWN = 1500;
int CTFT_GRUNT_COOLDOWN = 1500;
int CTFT_GENERIC_COOLDOWN = 10000;
int CTFT_BOMB_COOLDOWN = 20000;
int CTFT_DISPENSER_COOLDOWN = 15000;
float CTFT_RESPAWN_RADIUS = 384.0f;
float CTFT_BUILD_RADIUS = 160.0f;
float CTFT_BUILD_DESTROY_RADIUS = 96.0f;
float CTF_RUNNER_DETECT_DISTANCE = 2048.0f;
int CTFT_RUNNER_DETECT_COST = 50;
int CTFT_RUNNER_DETECT_TIME = 1500;
uint CTFT_RUNNER_DETECT_DURATION = 90000;

// precache images and sounds

int prcShockIcon;
int prcShellIcon;
int prcAlphaFlagIcon;
int prcBetaFlagIcon;
int prcFlagIcon;
int prcFlagIconStolen;
int prcFlagIconLost;
int prcFlagIconCarrier;
int prcDropFlagIcon;

int prcFlagIndicatorDecal;

int prcAnnouncerRecovery01;
int prcAnnouncerRecovery02;
int prcAnnouncerRecoveryTeam;
int prcAnnouncerRecoveryEnemy;
int prcAnnouncerFlagTaken;
int prcAnnouncerFlagTakenTeam01;
int prcAnnouncerFlagTakenTeam02;
int prcAnnouncerFlagTakenEnemy01;
int prcAnnouncerFlagTakenEnemy02;
int prcAnnouncerFlagScore01;
int prcAnnouncerFlagScore02;
int prcAnnouncerFlagScoreTeam01;
int prcAnnouncerFlagScoreTeam02;
int prcAnnouncerFlagScoreEnemy01;
int prcAnnouncerFlagScoreEnemy02;

bool firstSpawn = false;

Cvar ctfAllowPowerupDrop( "ctf_powerupDrop", "0", CVAR_ARCHIVE );

///*****************************************************************
/// LOCAL FUNCTIONS
///*****************************************************************

// a player has just died. The script is warned about it so it can account scores
void CTF_playerKilled( Entity @target, Entity @attacker, Entity @inflictor )
{
    if ( @target.client == null )
        return;

    cFlagBase @flagBase = @CTF_getBaseForCarrier( target );

    // reset flag if carrying one
    if ( @flagBase != null )
    {
        if ( @attacker != null )
            flagBase.carrierKilled( attacker, target );

        CTF_PlayerDropFlag( target, false );
    }
    else if ( @attacker != null )
    {
        @flagBase = @CTF_getBaseForTeam( attacker.team );

        // if not flag carrier, check whether victim was offending our flag base or friendly flag carrier
        if ( @flagBase != null )
            flagBase.offenderKilled( attacker, target );		
    }

    if ( match.getState() != MATCH_STATE_PLAYTIME )
        return;

    // check for generic awards for the frag
    if( @attacker != null && attacker.team != target.team )
		award_playerKilled( @target, @attacker, @inflictor );	
}

void CTF_SetVoicecommOverlayMenu( Client @client, int playerClass )
{
	String menuStr = '';

    if ( playerClass == PLAYERCLASS_ENGINEER ) {
		menuStr += 
			'"Spawn turret" "classaction1" ' + 
			'"Spawn ammo dispenser" "classaction2" ';
	}
    else if ( playerClass == PLAYERCLASS_GRUNT ) {
		menuStr += 
			'"Drop armor" "classaction1" ' + 
			'"Drop/detonate bomb" "classaction2" ';
	}
    else if ( playerClass == PLAYERCLASS_MEDIC ) {
		menuStr += 	
			'"Drop health" "classaction1" '
			'"Empty" "" ';
	}
    else if ( playerClass == PLAYERCLASS_RUNNER ) {
		menuStr += 
			'"Activate invisibility" "classaction1" ' + 
			'"Reveal nearby turrets "classaction2" ';
	}
		
	menuStr += 
		'"Area secured" "vsay_team areasecured" ' + 
		'"Go to quad" "vsay_team gotoquad" ' + 
		'"Go to powerup" "vsay_team gotopowerup" ' +		
		'"Need offense" "vsay_team needoffense" ' + 
		'"Need defense" "vsay_team needdefense" ' + 
		'"On offense" "vsay_team onoffense" ' + 
		'"On defense" "vsay_team ondefense" ';

	GENERIC_SetOverlayMenu( @client, menuStr );
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

            if ( token == "flag" )
            {
                if ( ( client.getEnt().effects & EF_CARRIER ) == 0 )
                    client.printMessage( "You don't have the flag\n" );
                else
                    CTF_PlayerDropFlag( client.getEnt(), true );
            }
            else
            {
                GENERIC_CommandDropItem( client, token );
            }
        }

        return true;
    }
    else if( cmdString == "cvarinfo" )
    {
    	GENERIC_CheatVarResponse( client, cmdString, argsString, argc );
    	return true;
    }
    else if ( cmdString == "classaction1" )
    {
        if ( @client != null )
        {
            cPlayer @player = @GetPlayer( client );

            if ( player.playerClass.tag == PLAYERCLASS_ENGINEER )
                CTFT_DropTurret( client, AMMO_BULLETS );
            else if ( player.playerClass.tag == PLAYERCLASS_GRUNT )
                CTFT_DropArmor( client, "Yellow Armor" );
            else if ( player.playerClass.tag == PLAYERCLASS_MEDIC )
                CTFT_DropHealth( client );
            else if ( player.playerClass.tag == PLAYERCLASS_RUNNER )
                player.activateInvisibility();
        }
    }
    else if ( cmdString == "classaction2" )
    {
        if ( @client != null )
        {
            cPlayer @player = @GetPlayer( client );

            if ( player.playerClass.tag == PLAYERCLASS_ENGINEER )
                CTFT_DropDispenser( client );
            else if ( player.playerClass.tag == PLAYERCLASS_GRUNT )
                CTFT_DropBomb( client );
            else if ( player.playerClass.tag == PLAYERCLASS_MEDIC )
                CTFT_DropHealth( client );
            else if ( player.playerClass.tag == PLAYERCLASS_RUNNER )
                CTFT_DetectTurretsDelayed( client );
        }
    }
    else if ( cmdString == "gametypemenu" )
    {
        if ( client.getEnt().team < TEAM_PLAYERS )
        {
            G_PrintMsg( client.getEnt(), "You must join a team before selecting a class\n" );
            return true;
        }

        client.execGameCommand( "mecu \"Select class\" Grunt \"class grunt\" Medic \"class medic\" Runner \"class runner\" Engineer \"class engineer\"" );
        return true;
    }
    else if ( cmdString == "class" )
    {
        if ( @client != null )
            GetPlayer( client ).setPlayerClassCommand( argsString );
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

        if ( votename == "ctf_powerup_drop" )
        {
            String voteArg = argsString.getToken( 1 );
            if ( voteArg.len() < 1 )
            {
                client.printMessage( "Callvote " + votename + " requires at least one argument\n" );
                return false;
            }

            int value = voteArg.toInt();
            if ( voteArg != "0" && voteArg != "1" )
            {
                client.printMessage( "Callvote " + votename + " expects a 1 or a 0 as argument\n" );
                return false;
            }

            if ( voteArg == "0" && !ctfAllowPowerupDrop.boolean )
            {
                client.printMessage( "Powerup drop is already disallowed\n" );
                return false;
            }

            if ( voteArg == "1" && ctfAllowPowerupDrop.boolean )
            {
                client.printMessage( "Powerup drop is already allowed\n" );
                return false;
            }

            return true;
        }

        client.printMessage( "Unknown callvote " + votename + "\n" );
        return false;
    }
    else if ( cmdString == "callvotepassed" )
    {
        String votename = argsString.getToken( 0 );

        if ( votename == "ctf_powerup_drop" )
        {
            if ( argsString.getToken( 1 ).toInt() > 0 )
                ctfAllowPowerupDrop.set( 1 );
            else
                ctfAllowPowerupDrop.set( 0 );
        }

        return true;
    }

    return false;
}

bool GT_BotWouldDropHealth( const Client @client )
{
    if ( @client == null )
        return false;

    Entity @ent = G_GetEntity( client.playerNum + 1 );

    if ( ( ent.effects & EF_CARRIER ) != 0 )
        return false;

    if ( GetPlayer( ent.client ).playerClass.tag != PLAYERCLASS_MEDIC )
        return false;

    return ent.health > 95;
}

void GT_BotDropHealth( Client @client )
{
    if ( @client != null )
        CTFT_DropHealth( client );
}

bool GT_BotWouldDropArmor( const Client @client )
{
    if ( @client == null )
        return false;

    Entity @ent = G_GetEntity( client.playerNum + 1 );

    if ( ( ent.effects & EF_CARRIER ) != 0 )
        return false;

    if ( GetPlayer( client ).playerClass.tag != PLAYERCLASS_GRUNT )
        return false;

    return client.armor > 65;
}

void GT_BotDropArmor( Client @client )
{
    if ( @client != null )
        CTFT_DropArmor( client, "Yellow Armor" );
}

float GT_PlayerOffensiveAbilitiesRating( const Client @client )
{
    if ( @client != null )
    {
        switch ( GetPlayer( client ).playerClass.tag )
        {
            case PLAYERCLASS_GRUNT:
                return 0.3f;
            case PLAYERCLASS_MEDIC:
                return 0.7f;
            case PLAYERCLASS_RUNNER:
                return 1.0f;
            case PLAYERCLASS_ENGINEER:
                return 0.0f;
        }
    }
    return 0.5f;
}

float GT_PlayerDefenciveAbilitiesRating( const Client @client )
{
    if ( @client != null )
    {
        switch ( GetPlayer( client ).playerClass.tag )
        {
            case PLAYERCLASS_GRUNT:
                return 0.9f;
            case PLAYERCLASS_MEDIC:
                return 0.5f;
            case PLAYERCLASS_RUNNER:
                return 0.0f;
            case PLAYERCLASS_ENGINEER:
                return 1.0f;
        }
    }
    return 0.5f;
}

int GT_GetScriptWeaponsNum( const Client @client )
{
    if ( @client == null )
        return 0;

    if ( GetPlayer( client ).playerClass.tag != PLAYERCLASS_GRUNT )
        return 0;

    return 1;
}

bool GT_GetScriptWeaponDef( const Client @client, int weaponNum, AIScriptWeaponDef &out weaponDef )
{
    if ( @client == null )
        return false;

    if ( GetPlayer( client ).playerClass.tag != PLAYERCLASS_GRUNT )
        return false;

    if ( weaponNum != 0 )
        return false;

    weaponDef.weaponNum = 0;
    weaponDef.tier = 4;
    weaponDef.minRange = 250.0f;
    weaponDef.maxRange = 1200.0f;
    weaponDef.bestRange = 600.0f;
    weaponDef.projectileSpeed = 750.0f;
    weaponDef.splashRadius = 250.0f;
    weaponDef.maxSelfDamage = 400.0f;
    weaponDef.aimType = AI_WEAPON_AIM_TYPE_DROP;
    weaponDef.isContinuousFire = false;

    return true;
}

int GT_GetScriptWeaponCooldown( const Client @client, int weaponNum )
{
    if ( @client == null )
        return 999999;

    if ( client.getEnt().isGhosting() )
        return 999999;
    
    cPlayer @player = GetPlayer( client );
    if ( player.playerClass.tag != PLAYERCLASS_GRUNT )
        return 999999;

    if ( weaponNum != 0 )
        return 999999;

    // If a bomb has been already thrown
    if ( @player.bomb != null && @player.bomb.bombEnt != null )
        return 999999;

    if ( client.armor < CTFT_TURRET_AP_COST )
        return 999999;

    if ( levelTime >= player.bombCooldownTime )
        return 0;

    return player.bombCooldownTime - levelTime;
}

bool GT_SelectScriptWeapon( Client @client, int weaponNum )
{
    if ( @client == null )
        return false;

    if ( client.getEnt().isGhosting() )
        return false;

    cPlayer @player = GetPlayer( client );
    if ( player.playerClass.tag != PLAYERCLASS_GRUNT )
        return false;

    if ( weaponNum != 0 )
        return false;

    return !player.isBombCooldown();
}

bool GT_FireScriptWeapon( Client @client, int weaponNum )
{
    if ( @client == null )
        return false;

    if ( client.getEnt().isGhosting() )
        return false;

    // Do not fire in countdown state
    
    cPlayer @player = GetPlayer( client );
    if ( player.playerClass.tag != PLAYERCLASS_GRUNT )
        return false;

    if ( weaponNum != 0 )
        return false;

    // If a bomb has been already thrown
    if ( @player.bomb != null && @player.bomb.bombEnt != null )
        return false;

    CTFT_DropBomb( client );

    return @player.bomb != null && @player.bomb.bombEnt != null;
}

int lastBotGoalUpdateClientNum = 0;

void CTFT_UpdateBotsExtraGoals() 
{   
    // Limit bot extra goals updates to a single client per frame. (is it a bot or not), its notoriously slow.
    int clientNum = lastBotGoalUpdateClientNum + 1;
    // maxClients might be modified since last update
    if ( clientNum > maxClients )
        clientNum = 1;

    Entity @ent = G_GetEntity( clientNum );
    if ( @ent != null )
        CTFT_UpdateBotExtraGoals( ent );
    
    lastBotGoalUpdateClientNum = clientNum;
}

bool CTFT_ShouldNotPlantItem( const Entity @originEntity, float radius )
{
    array<Entity @> @nearbyEntities = @G_FindInRadius( originEntity.origin, radius );

    for ( uint i = 0; i < nearbyEntities.size(); ++i )
    {
        Entity @nearby = nearbyEntities[i];

        // Do this cheap test first

        if ( @nearby.client != null )
            continue;     

        // Do not plant a turret or a dispenser if there are nearby entities of this class (entity team is ignored)

        // Do cheap solid tests first

        if ( nearby.solid == SOLID_YES )
        {
        
            if ( nearby.classname == "turret_body" )
                return true;

            if ( nearby.classname == "dispenser_body" )
                return true;
        }
        else if ( nearby.solid == SOLID_TRIGGER )
        {
            if ( nearby.classname == "bomb_body" )
                return true;

            if ( nearby.classname == "team_CTF_alphaflag" )
                return true;

            if ( nearby.classname == "team_CTF_betaflag" )
                return true;
        }
    }
    return false;
}

void CTFT_UpdateEngineerExtraGoal( Entity @ent, Bot @bot, cPlayer @player, Entity @goal )
{
    // Skip entities that are not plant spots quickly

    if ( goal.team != ent.team )
        return;

    if ( ( goal.svflags & SVF_NOCLIENT ) == 0 )
        return; 

    if ( goal.solid != SOLID_TRIGGER )
        return;

    if ( goal.classname != "ai_planting_spot" )
        return;

    // Set zero weight for the common case.
    // Even if an entity is a planting spot, its weight should be reset unless it needs planting and the bot should do it.
    bot.overrideEntityWeight( goal, 0.0f );

    // Do not try to plant turrets carrying a flag 
    if ( ( ent.effects & EF_CARRIER ) != 0 )
        return;

    // Both turret and dispencer have this cost
    if ( ent.client.armor < CTFT_TURRET_AP_COST )
        return;

    if ( player.isEngineerCooldown() )
        return;

    // If the bot is not assigned as a defender
    if ( bot.defenceSpotId < 0 )
    {
        // Bother about planting an item only if the bot is occasionally close to a planting spot
        if ( ent.origin.distance( goal.origin ) > 200 )
            return;
    }

    // Plant quickly only a minimal number of enities sufficient for defense purposes.
    // Humans in a team (if any) should finish planting.

    // Bots should not plant more than 3 turrets
    if ( CTFT_TeamTurretsNum( ent.team ) > 3 )
        return;

    // Bots should plant only a single dispenser
    if ( CTFT_TeamDispensersNum( ent.team ) > 1 )
        return;

    if ( CTFT_ShouldNotPlantItem( goal, 192.0f ) )
        return;

    // Set a huge value to override the value of defence spot entity
    bot.overrideEntityWeight( goal, 999.0f );
}

void CTFT_UpdateMedicExtraGoal( Entity @ent, Bot @bot, cPlayer @player, Entity @goal )
{
    if ( goal.team != ent.team )
        return;

    if ( goal.classname != "reviver" )
        return;

    if ( ( ent.effects & EF_CARRIER ) == 0 )
        bot.overrideEntityWeight( goal, 5.0f );
    else
        bot.overrideEntityWeight( goal, 1.5f );
}

void CTFT_UpdateGruntExtraGoal( Entity @ent, Bot @bot, cPlayer @player, Entity @goal )
{
    if ( @player.bomb == null )
        return;

    if ( @player.bomb.bombEnt != @goal )
        return;

    if ( !player.bomb.botShouldPickup )
    {
        bot.overrideEntityWeight( goal, 0.0f );
        return;
    }    

    if ( ( ent.effects & EF_CARRIER ) == 0 )
        bot.overrideEntityWeight( goal, 999.0f );
    else
        bot.overrideEntityWeight( goal, 0.5f );
}

void CTFT_UpdateBotExtraGoals( Entity @ent )
{
    Bot @bot = @ent.client.getBot();
    if ( @bot == null )
        return;

    int enemyTeam;
    if ( ent.team == TEAM_ALPHA )
        enemyTeam = TEAM_BETA;
    else if ( ent.team == TEAM_BETA )
        enemyTeam = TEAM_ALPHA;
    else 
        return;

    cPlayer @player = GetPlayer( ent.client );

    // Flags defence/offence/carrier support is managed by native code
    // using provided status of defence/offence spots.

    // The only thing should be managed in scripts is returning to a base.
    // (Carrier behaviour differs for custom gametypes).

    // Also dropped flags are managed here too.

    cFlagBase @teamBase = @CTF_getBaseForTeam( ent.team );

    if ( @teamBase != null )
    {
        // The bot is a carrier, so it should return to the team base
        if ( ( ent.effects & EF_CARRIER ) != 0 )
        {
            if ( @teamBase.owner != null )
            {
                // the flag is at our base, return to the base
                if ( ( teamBase.owner.effects & EF_CARRIER ) != 0 )
                {
                    bot.overrideEntityWeight( teamBase.owner, 9.0f );
                }
                // our flag is stolen    
                else
                {
                    float botToHomeBaseDist = teamBase.owner.origin.distance( ent.origin );
                    // return to our base
                    if ( botToHomeBaseDist > 768.0f )
                    {
                        bot.overrideEntityWeight( teamBase.owner, 9.0f );
                    }
                    // do not camp at our flag spot, hide somewhere else
                    else 
                    {      
                        bot.overrideEntityWeight( teamBase.owner, 9.0f * ( botToHomeBaseDist / 768.0f ) );
                    }
                }
            }
        }

        // it's team flag, dropped somewhere
        if ( @teamBase.carrier != @teamBase.owner && @teamBase.carrier.client == null )
            bot.overrideEntityWeight( teamBase.carrier, 9.0f );
    } 

    // CTF:T addition: 
    // Usually there are 1 defender (if flag spot is not in alert state) and 5 attackers in team.
    // These roles are assigned by native code based on status of defence/offense spots.
    // Other bots has no order spots and are roaming (grabbing items on the map and fragging enemies).
    // There are rarely any items in CTF:T except flags, so roaming bots feel lost.
    // Thus roaming bots should attack enemy base (even if enemy flag is stolen). 
        
    cFlagBase @enemyBase = @CTF_getBaseForTeam( enemyTeam );
    if ( @enemyBase != null )
    { 
        if ( @enemyBase.carrier != @enemyBase.owner && @enemyBase.carrier.client == null )
            bot.overrideEntityWeight( enemyBase.carrier, 9.0f );

        if ( @enemyBase.carrier != @enemyBase.owner )
        {
            // it's team flag, dropped somewhere
            if ( @enemyBase.carrier.client == null )
            {
                bot.overrideEntityWeight( enemyBase.carrier, 9.0f );
            }
            // If the bot is roaming
            else if ( bot.defenceSpotId == -1 && bot.offenseSpotId == -1 )
            {
                // Attack the enemy base (but no flag spot itself).
                float botToEnemyBaseDist = enemyBase.owner.origin.distance( ent.origin );
                if ( botToEnemyBaseDist < 384.0f )
                    bot.overrideEntityWeight( enemyBase.owner, 0.0f );
                else
                    bot.overrideEntityWeight( enemyBase.owner, 9.0f );
            }
        }
        else if ( bot.defenceSpotId == -1 && bot.offenseSpotId == -1 )
        {
            // Roaming bots should get the enemy flag, but its weight is lower than for attackers
            bot.overrideEntityWeight( enemyBase.owner, 3.5f );
        }
    }

    for ( int i = maxClients + 1; i < numEntities; ++i )
    {
        Entity @goal = @G_GetEntity(i);

        if ( goal.classname == "dispenser_body" )
        {
            if ( goal.team != ent.team )
                continue;
            
            if ( CTFT_isDispenserCooldown( ent ) )
                bot.overrideEntityWeight( goal, 0.0f );
            else
                bot.overrideEntityWeight( goal, 1.0f );
        }

        if ( player.playerClass.tag == PLAYERCLASS_ENGINEER )
        {
            CTFT_UpdateEngineerExtraGoal( ent, bot, player, goal );
        }
        else if ( player.playerClass.tag == PLAYERCLASS_MEDIC )
        {
            CTFT_UpdateMedicExtraGoal( ent, bot, player, goal );
        } 
        else if ( player.playerClass.tag == PLAYERCLASS_GRUNT )
        {
            CTFT_UpdateGruntExtraGoal( ent, bot, player, goal );
        }
    }
}

uint lastClassesUpdate = 0;
bool lastClassesUpdateTeamIsAlpha = false;

void CTFT_UpdateBotsClasses()
{
    if ( levelTime - lastClassesUpdate < 64 )
        return;

    CTFT_UpdateTeamBotsClasses( lastClassesUpdateTeamIsAlpha ? TEAM_BETA : TEAM_ALPHA );
    
    lastClassesUpdateTeamIsAlpha = !lastClassesUpdateTeamIsAlpha;
    lastClassesUpdate = levelTime;
}

void CTFT_UpdateTeamBotsClasses( int teamNum )
{
    // Get number of player for each class in team.
    // Also, reset bot pending classes.
    Team @team = G_GetTeam( teamNum );
    int[] numClassPlayers( PLAYERCLASS_TOTAL );
    for ( int i = 0; i < team.numPlayers; ++i )
    {
        Client @client = team.ent( i ).client;
        int classTag = playerClasses[client.playerNum];
        numClassPlayers[classTag]++;
        if ( @client.getBot() != null )
            gtPlayers[client.playerNum].botPendingClass = -1;
    }

    int numTeamTurrets = CTFT_TeamTurretsNum( teamNum );

    // Now we have released bot player class slots.
    // Try to assign a pending class if there are no enough players of this class.

    for ( int classTag = 0; classTag < PLAYERCLASS_TOTAL; ++classTag )
    {
        int numPlayersToAssign = 1;
        if ( classTag == PLAYERCLASS_ENGINEER && numTeamTurrets < 2 )
            numPlayersToAssign += 2 - numTeamTurrets;

        if ( numClassPlayers[classTag] < numPlayersToAssign )
        {
            for ( int i = 0; i < team.numPlayers && numPlayersToAssign > 0; ++i )
            {
                Client @client = team.ent( i ).client;
                cPlayer @player = @gtPlayers[client.playerNum];
                
                if ( @client.getBot() == null )
                {
                    if ( player.playerClass.tag == classTag )
                        numPlayersToAssign--;

                    continue;
                }                
   
                // Player already has this pending class
                if ( player.botPendingClass == classTag )
                {
                    numPlayersToAssign--;
                    // Touch bot pending class
                    player.botPendingClassAssignedAt = levelTime;
                    continue;
                }

                // Skip just assigned players
                if ( player.botPendingClassAssignedAt == levelTime )
                    continue;
                
                player.botPendingClass = classTag;
                player.botPendingClassAssignedAt = levelTime;
                numPlayersToAssign--;
                continue;
            }
        }
    }
    
    // For each class a single mandatory player might have been assigned.
    // (for engineers class up to 3 mandatory players might have been assigned for fast turrets planting).
    // Other bots (that have pendingClass == -1) will spawn with random class.
}

// select a spawning point for a player
Entity @GT_SelectSpawnPoint( Entity @self )
{
    bool spawnFromReviver = true;

    if ( @self.client == null )
        return null;
	
    if ( firstSpawn )
    {
		Entity @spot;
		
        if ( self.team == TEAM_ALPHA )
            @spot = @GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_alphaplayer" );
        else
			@spot = @GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_betaplayer" );
			
		if( @spot != null )
			return @spot;
    }

    if ( spawnFromReviver )
    {
        if ( @self.client != null )
        {
            cPlayer @player = GetPlayer( self.client );

            // see if this guy has a reviver
            if ( @player.reviver != null )
            {
                if ( player.reviver.revived == true )
                    return player.reviver.ent;
            }
        }
    }

    if ( self.team == TEAM_ALPHA )
        return GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_alphaspawn" );

    return GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_betaspawn" );
}

String @GT_ScoreboardMessage( uint maxlen )
{
    String scoreboardMessage = "";
    String entry;
    Team @team;
    Entity @ent;
    int i, t, classIcon;

    for ( t = TEAM_ALPHA; t < GS_MAX_TEAMS; t++ )
    {
        @team = @G_GetTeam( t );

        // &t = team tab, team tag, team score, team ping
        entry = "&t " + t + " " + team.stats.score + " " + team.ping + " ";
        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;

        for ( i = 0; @team.ent( i ) != null; i++ )
        {
            @ent = @team.ent( i );

            classIcon = 0;
            if ( @ent.client != null && ent.client.state() >= CS_SPAWNED )
                classIcon = GetPlayer( ent.client ).playerClass.iconIndex;

            int playerID = ( ent.isGhosting() && ( match.getState() == MATCH_STATE_PLAYTIME ) ) ? -( ent.playerNum + 1 ) : ent.playerNum;

            //G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %s 52 %i 52 %l 48 %p l1 %r l1" );
            //G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Clan Score Ping C R" );

			int score = ent.client.stats.score + int( ent.client.stats.totalDamageGiven * 0.01 );

            // "Name Score Ping C R"
            entry = "&p " + playerID + " "
                    + ent.client.clanName + " "
                    + score + " "
                    + ent.client.ping + " "
                    + classIcon + " "
                    + ( ent.client.isReady() ? "1" : "0" ) + " ";

            if ( scoreboardMessage.len() + entry.len() < maxlen )
                scoreboardMessage += entry;
        }
    }

    return scoreboardMessage;
}

// Some game actions get reported to the script as score events.
// Warning: client can be null
void GT_ScoreEvent( Client @client, const String &score_event, const String &args )
{
    if ( score_event == "dmg" )
    {
        int arg1 = args.getToken( 0 ).toInt();
        float arg2 = args.getToken( 1 ).toFloat();
        int arg3 = args.getToken( 2 ).toInt();

        Entity @target = @G_GetEntity( arg1 );

        if ( @target != null && @target.client != null )
        {
            /* will not work without latest bins*/
            GetPlayer( target.client ).tookDamage( arg3, arg2 );
        }
    }
    else if ( score_event == "kill" )
    {
        Entity @attacker = null;
        if ( @client != null )
            @attacker = @client.getEnt();

        int arg1 = args.getToken( 0 ).toInt();
        int arg2 = args.getToken( 1 ).toInt();
        Entity @ent = G_GetEntity( arg1 );

        // Important - if turret ends up here without this, crash =P
        if ( @ent == null || @ent.client == null )
            return;

        // target, attacker, inflictor
        CTF_playerKilled( ent, attacker, G_GetEntity( arg2 ) );

        // Class-specific death stuff
        cPlayer @targetPlayer = @GetPlayer( ent.client );
        targetPlayer.respawnTime = levelTime + CTFT_RESPAWN_TIME;

        // Spawn respawn indicator
        if ( match.getState() == MATCH_STATE_PLAYTIME )
            targetPlayer.spawnReviver();

        if ( targetPlayer.playerClass.tag == PLAYERCLASS_MEDIC )
            CTFT_DeathDrop( ent.client, "5 Health" );

        if ( targetPlayer.playerClass.tag == PLAYERCLASS_GRUNT )
        {
            CTFT_DeathDrop( ent.client, "Armor Shard" );

            // Explode all cluster bombs belonging to this grunt when dying
            cBomb @bomb = null;
            Entity @tmp = null;
            for ( int i = 0; i < MAX_BOMBS; i++ )
            {
                if ( gtBombs[i].inuse == true )
                {
                    @bomb = @gtBombs[i];

                    if ( @targetPlayer == @bomb.player )
                        bomb.die(tmp, tmp);

                }
            }
        }
    }
    else if ( score_event == "award" )
    {
		if ( args == "^3On fire!" || args == "^3Raging!" || args == "^3Fraglord!" || args == "^3Extermination!" || args == "^3God Mode!" )
		{
			if ( @client == null )
				return;

			cPlayer @targetPlayer = @GetPlayer( client );
			if ( targetPlayer.playerClass.tag == PLAYERCLASS_GRUNT )
			{
				uint level = 0;
				if ( args == "^3On fire!" )
					level = 1;
				else if ( args == "^3Raging!" )
					level = 2;
				else if ( args == "^3Fraglord!" )
					level = 3;
				else if ( args == "^3Extermination!" )
					level = 4;
				else if ( args == "^3God Mode!" )
					level = 5;

				 CTFT_SpawnRage ( client, level );
			}

		}

    }
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
	Client @client = @ent.client;

    if ( @client == null )
        return;

    cPlayer @player = @GetPlayer( client );

    if ( old_team != new_team )
    {
        player.removeReviver();

        // show the class selection menu
        if ( old_team == TEAM_SPECTATOR )
        {
            if ( @client.getBot() == null )
                client.execGameCommand( "mecu \"Select class\" Grunt \"class grunt\" Medic \"class medic\" Runner \"class runner\" Engineer \"class engineer\"" );
        }

        // Set newly joined players to respawn queue
        if ( new_team == TEAM_ALPHA || new_team == TEAM_BETA )
            player.respawnTime = levelTime + CTFT_RESPAWN_TIME;
    }

    if ( @client.getBot() != null )
    {
        // If bot class might have been reset, weights are invalid.
        // Since external entity weights are not managed by bot code,
        // they should be cleaned up manually.
        client.getBot().clearOverriddenEntityWeights();
        if ( player.botPendingClass != -1 )
        {
            player.setPlayerClass( player.botPendingClass );
        }        
        else 
        {
            // Never spawn engineers randomly.
            // This means there are no engineer bots at match start and humans are free to use engineer abilities
            // (bots will not exhaust team turrets limit by planting turrets in wrong place).
            // Also, slightly prefer runners or medics (these classes suit a bot better) over grunts.
            float random01 = random();
            if ( random01 < 0.35f )
                player.setPlayerClass( PLAYERCLASS_MEDIC );
            else if ( random01 < 0.70f )
                player.setPlayerClass( PLAYERCLASS_RUNNER );
            else
                player.setPlayerClass( PLAYERCLASS_GRUNT );
        }
    }

    if ( ent.isGhosting() )
    {
		ent.svflags &= ~SVF_FORCETEAM;
	
        if ( match.getState() == MATCH_STATE_PLAYTIME )
        {
            Entity @chaseTarget = @G_GetEntity( client.chaseTarget );
            if ( @chaseTarget != null && @chaseTarget.client != null )
                client.chaseCam( chaseTarget.client.name, true );
            else
                client.chaseCam( null, true );
        }
        return;
    }

    // Reset health and armor before setting abilities.
    ent.health = 100;
    ent.client.armor = 0;

    // Assign movement abilities for classes

    // fixme: move methods to player
    player.refreshModel();
    player.refreshMovement();

    // Runner
    if ( player.playerClass.tag == PLAYERCLASS_RUNNER )
    {
        // Weapons
        client.inventoryGiveItem( WEAP_RIOTGUN );
        client.inventoryGiveItem( WEAP_LASERGUN );
        client.inventoryGiveItem( WEAP_MACHINEGUN );
        client.inventorySetCount( WEAP_GUNBLADE, 1 );

        // Strong ammo etc
        client.inventoryGiveItem( AMMO_SHELLS );
        client.inventoryGiveItem( ARMOR_GA );

        G_PrintMsg( ent , "You're spawned as ^5RUNNER^7. This is a the fastest class with weak health and armor\n");
        G_PrintMsg( ent , "^2Class action 1: ^7Activate ^5Invisibility\n");
        G_PrintMsg( ent , "^1Class action 2: ^5Reveal nearby turrets ^7for 30 seconds.\n");
    }
    // Medic
    else if ( player.playerClass.tag == PLAYERCLASS_MEDIC )
    {
        // Weapons
        client.inventoryGiveItem( WEAP_PLASMAGUN );
        client.inventoryGiveItem( WEAP_ELECTROBOLT );
        client.inventoryGiveItem( WEAP_LASERGUN );
        client.inventoryGiveItem( WEAP_MACHINEGUN );

        // Strong ammo etc
        client.inventoryGiveItem( AMMO_LASERS );

        G_PrintMsg( ent , "You're spawned as ^3MEDIC^7. This is a healer class with regeneration and ability to drop health. You can also revive dead team mates by walking over their respawner marker.\n");
        G_PrintMsg( ent , "^2Class action 1: ^7Drop ^225 Health^7 for your team mates\n");
    }
    // Grunt
    else if ( player.playerClass.tag == PLAYERCLASS_GRUNT )
    {
        // Weapons
        client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
        client.inventoryGiveItem( WEAP_PLASMAGUN );
        client.inventoryGiveItem( WEAP_GRENADELAUNCHER );
        client.inventorySetCount( WEAP_GUNBLADE, 1 );
        // Strong ammo etc
        client.inventoryGiveItem( ARMOR_YA );
        client.inventoryGiveItem( AMMO_ROCKETS );
        client.inventoryGiveItem( AMMO_PLASMA );

        G_PrintMsg( ent , "You're spawned as ^1GRUNT^7. This is a tank class with slow movement, strong armor and strong weapons\n");
        G_PrintMsg( ent , "^2Class action 1: ^7Drop ^3Yellow Armor ^7for your team mates\n");
        G_PrintMsg( ent , "^1Class action 2: ^7Throw a ^5Cluster Bomb^7. Press ^1Class action 2^7 again to detonate it.\n");
    }
    // Engineer
    else if ( player.playerClass.tag == PLAYERCLASS_ENGINEER )
    {
        // Weapons
        client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
        client.inventoryGiveItem( WEAP_RIOTGUN );
        client.inventoryGiveItem( WEAP_ELECTROBOLT );
        client.inventorySetCount( WEAP_GUNBLADE, 1 );

        // Strong ammo etc
        client.inventoryGiveItem( ARMOR_GA );
        client.inventoryGiveItem( AMMO_BOLTS );

        G_PrintMsg( ent , "You're spawned as ^8ENGINEER^7. This is a supportive class with slow movement and ability to build turrets\n");
        G_PrintMsg( ent , "^2Class action 1:^7 Spawn a turret with ^9Machine Gun\n");
        G_PrintMsg( ent , "^1Class action 2:^7 Spawn an ^8Ammo dispenser\n");
    }

    // enable gunblade blast
    client.inventorySetCount( AMMO_GUNBLADE, 1 );

    // select rocket launcher if available
    client.selectWeapon( -1 ); // auto-select best weapon in the inventory

	ent.svflags |= SVF_FORCETEAM;

	CTF_SetVoicecommOverlayMenu( @client, player.playerClass.tag );
	
    // add a teleportation effect
    ent.respawnEffect();

    if ( @player.reviver != null && player.reviver.revived )
    {
        // when revived do not clear all timers
        player.genericCooldownTime = 0;
        player.dispenserCooldownTime = 0;
        player.respawnTime = 0;
        player.invisibilityEnabled = false;
        player.invisibilityLoad = 0;
        player.invisibilityCooldownTime = 0;
        player.hudMessageTimeout = 0;

        player.invisibilityWasUsingWeapon = -1;
    }
    else
        player.resetTimers();

    player.removeReviver();
}

// Thinking function. Called each frame
void GT_ThinkRules()
{
    if ( match.scoreLimitHit() || match.timeLimitHit() || match.suddenDeathFinished() )
    {
        if ( !match.checkExtendPlayTime() )
            match.launchState( match.getState() + 1 );
    }

    GENERIC_Think();
    CTFT_RespawnQueuedPlayers();

    if ( match.getState() >= MATCH_STATE_POSTMATCH )
    {
        return;
    }

    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
    {
        flagBase.thinkRules();
    }

    for ( int i = 0; i < maxClients; i++ )
    {
        // update model and movement
        cPlayer @player = @GetPlayer( i );

        if ( player.client.state() < CS_SPAWNED )
            continue;

        // fixme : move methods to player
        player.refreshChasecam();
        player.refreshModel();
        player.refreshMovement();
        player.refreshRegeneration();
        player.watchWarshell();
        player.updateHUDstats();
    }

    CTFT_UpdateBotsExtraGoals();

    CTFT_UpdateBotsClasses();
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
        CTFT_SetUpWarmup();
		SpawnIndicators::Create( "team_CTF_alphaplayer", TEAM_ALPHA );
		SpawnIndicators::Create( "team_CTF_alphaspawn", TEAM_ALPHA );
		SpawnIndicators::Create( "team_CTF_betaplayer", TEAM_BETA );
		SpawnIndicators::Create( "team_CTF_betaspawn", TEAM_BETA );		
        break;

    case MATCH_STATE_COUNTDOWN:
        GENERIC_SetUpCountdown();
		SpawnIndicators::Delete();
        break;

    case MATCH_STATE_PLAYTIME:
        GENERIC_SetUpMatch();
        CTFT_SetUpMatch();
        break;

    case MATCH_STATE_POSTMATCH:
        GENERIC_SetUpEndMatch();
        CTFT_RemoveTurrets();
        CTFT_RemoveDispensers();
        CTFT_RemoveBombs();
        CTFT_RemoveRevivers();
        break;

    default:
        break;
    }
}

void CTFT_SetUpWarmup()
{
    GENERIC_SetUpWarmup();

    // set spawnsystem type to instant while players join
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );
}

void CTFT_SetUpMatch()
{
    // Reset flags
    CTF_ResetFlags();
    CTFT_ResetRespawnQueue();
    CTFT_RemoveTurrets();
    CTFT_RemoveDispensers();
    CTFT_RemoveBombs();
    CTFT_RemoveItemsByName("25 Health");
    CTFT_RemoveItemsByName("Yellow Armor");
    CTFT_RemoveItemsByName("5 Health");
    CTFT_RemoveItemsByName("Armor Shard");

    // set spawnsystem type to not respawn the players when they die
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_HOLD, 0, 0, false );

    // clear scores

    Entity @ent;
    Team @team;
    int i;

    for ( i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
    {
        @team = @G_GetTeam( i );
        team.stats.clear();

        // respawn all clients inside the playing teams
        for ( int j = 0; @team.ent( j ) != null; j++ )
        {
            @ent = @team.ent( j );
            ent.client.stats.clear(); // clear player scores & stats
        }
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
    gametype.title = "CTF: Tactics";
    gametype.version = "1.0";
    gametype.author = "Warsow Development Team";

    // if the gametype doesn't have a config file, create it
    if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
    {
        String config;

        // the config file doesn't exist or it's empty, create it
        config = "// '" + gametype.title + "' gametype configuration file\n"
                 + "// This config will be executed each time the gametype is started\n"
                 + "\n\n// map rotation\n"
                 + "set g_maplist \"wctf1 wctf3 wctf4 wctf5 wctf6\" // list of maps in automatic rotation\n"
                 + "set g_maprotation \"1\"   // 0 = same map, 1 = in order, 2 = random\n"
                 + "\n// game settings\n"
                 + "set g_scorelimit \"0\"\n"
                 + "set g_timelimit \"20\"\n"
                 + "set g_warmup_timelimit \"1\"\n"
                 + "set g_match_extendedtime \"5\"\n"
                 + "set g_allow_falldamage \"1\"\n"
                 + "set g_allow_selfdamage \"1\"\n"
                 + "set g_allow_teamdamage \"0\"\n"
                 + "set g_allow_stun \"1\"\n"
                 + "set g_teams_maxplayers \"5\"\n"
                 + "set g_teams_allow_uneven \"0\"\n"
                 + "set g_countdown_time \"5\"\n"
                 + "set g_maxtimeouts \"3\" // -1 = unlimited\n"
                 + "set ctf_powerupDrop \"0\"\n"
                 + "\necho \"" + gametype.name + ".cfg executed\"\n";

        G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
        G_Print( "Created default config file for '" + gametype.name + "'\n" );
        G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
    }

    gametype.spawnableItemsMask = 0;
    if ( gametype.isInstagib )
        gametype.spawnableItemsMask &= ~uint(G_INSTAGIB_NEGATE_ITEMMASK);

    gametype.respawnableItemsMask = gametype.spawnableItemsMask ;
    gametype.dropableItemsMask = ( gametype.spawnableItemsMask | IT_HEALTH | IT_ARMOR ) ;
    gametype.pickableItemsMask = ( gametype.spawnableItemsMask | gametype.dropableItemsMask );


    gametype.isTeamBased = true;
    gametype.isRace = false;
    gametype.hasChallengersQueue = false;
    gametype.maxPlayersPerTeam = 0;

    gametype.ammoRespawn = 20;
    gametype.armorRespawn = 25;
    gametype.weaponRespawn = 5;
    gametype.healthRespawn = 25;
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
    gametype.teamOnlyMinimap = true;
    gametype.customDeadBodyCam = true; // needs new bins!

	gametype.mmCompatible = true;
	
    gametype.spawnpointRadius = 256;

    if ( gametype.isInstagib )
        gametype.spawnpointRadius *= 2;

    // set spawnsystem type
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

    // define the scoreboard layout
    G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %s 52 %i 52 %l 48 %p l1 %r l1" );
    G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Clan Score Ping C R" );

    // precache images and sounds
    prcShockIcon = G_ImageIndex( "gfx/hud/icons/powerup/quad" );
    prcShellIcon = G_ImageIndex( "gfx/hud/icons/powerup/warshell" );
    prcAlphaFlagIcon = G_ImageIndex( "gfx/hud/icons/flags/iconflag_alpha" );
    prcBetaFlagIcon = G_ImageIndex( "gfx/hud/icons/flags/iconflag_beta" );
    prcFlagIcon = G_ImageIndex( "gfx/hud/icons/flags/iconflag" );
    prcFlagIconStolen = G_ImageIndex( "gfx/hud/icons/flags/iconflag_stolen" );
    prcFlagIconLost = G_ImageIndex( "gfx/hud/icons/flags/iconflag_lost" );
    prcFlagIconCarrier = G_ImageIndex( "gfx/hud/icons/flags/iconflag_carrier" );
    prcDropFlagIcon = G_ImageIndex( "gfx/hud/icons/drop/flag" );

    prcFlagIndicatorDecal = G_ImageIndex( "gfx/indicators/radar_decal" );

    prcAnnouncerRecovery01 = G_SoundIndex( "sounds/announcer/ctf/recovery01" );
    prcAnnouncerRecovery02 = G_SoundIndex( "sounds/announcer/ctf/recovery02" );
    prcAnnouncerRecoveryTeam = G_SoundIndex( "sounds/announcer/ctf/recovery_team" );
    prcAnnouncerRecoveryEnemy = G_SoundIndex( "sounds/announcer/ctf/recovery_enemy" );
    prcAnnouncerFlagTaken = G_SoundIndex( "sounds/announcer/ctf/flag_taken" );
    prcAnnouncerFlagTakenTeam01 = G_SoundIndex( "sounds/announcer/ctf/flag_taken_team01" );
    prcAnnouncerFlagTakenTeam02 = G_SoundIndex( "sounds/announcer/ctf/flag_taken_team02" );
    prcAnnouncerFlagTakenEnemy01 = G_SoundIndex( "sounds/announcer/ctf/flag_taken_enemy_01" );
    prcAnnouncerFlagTakenEnemy02 = G_SoundIndex( "sounds/announcer/ctf/flag_taken_enemy_02" );
    prcAnnouncerFlagScore01 = G_SoundIndex( "sounds/announcer/ctf/score01" );
    prcAnnouncerFlagScore02 = G_SoundIndex( "sounds/announcer/ctf/score02" );
    prcAnnouncerFlagScoreTeam01 = G_SoundIndex( "sounds/announcer/ctf/score_team01" );
    prcAnnouncerFlagScoreTeam02 = G_SoundIndex( "sounds/announcer/ctf/score_team02" );
    prcAnnouncerFlagScoreEnemy01 = G_SoundIndex( "sounds/announcer/ctf/score_enemy01" );
    prcAnnouncerFlagScoreEnemy02 = G_SoundIndex( "sounds/announcer/ctf/score_enemy02" );

    // add commands
    G_RegisterCommand( "drop" );
    G_RegisterCommand( "gametype" );
    G_RegisterCommand( "gametypemenu" );
    G_RegisterCommand( "class" );
    G_RegisterCommand( "classAction1" );
    G_RegisterCommand( "classAction2" );

    // Make turret models pure
    G_ModelIndex( "models/objects/turret/base.md3", true );
    G_ModelIndex( "models/objects/turret/gun.md3", true );
    G_ModelIndex( "models/objects/turret/flash.md3", true );

    InitPlayers();
    G_RegisterCallvote( "ctf_powerup_drop", "1 or 0", "bool", "Enables or disables the dropping of powerups at dying" );

    G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
