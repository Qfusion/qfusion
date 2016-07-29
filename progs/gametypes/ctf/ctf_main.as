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

float CTF_UNLOCK_TIME = 2.0f;
float CTF_CAPTURE_TIME = 3.5f;
float CTF_UNLOCK_RADIUS = 150.0f;
float CTF_CAPTURE_RADIUS = 40.0f;

const float CTF_AUTORETURN_TIME = 30.0f;
const int CTF_BONUS_RECOVERY = 2;
const int CTF_BONUS_STEAL = 1;
const int CTF_BONUS_CAPTURE = 10;
const int CTF_BONUS_CAPTURE_ASSISTANCE = 4;
const int CTF_BONUS_CARRIER_KILL = 2;
const int CTF_BONUS_CARRIER_PROTECT = 2;
const int CTF_BONUS_FLAG_DEFENSE = 1;

const float CTF_FLAG_RECOVERY_BONUS_DISTANCE = 512.0f;
const float CTF_CARRIER_KILL_BONUS_DISTANCE = 512.0f;
const float CTF_OBJECT_DEFENSE_BONUS_DISTANCE = 512.0f;

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
Cvar ctfInstantFlag( "ctf_instantFlag", "0", CVAR_ARCHIVE );

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
		if( @flagBase != null )
			flagBase.offenderKilled( attacker, target );
	}

    if ( match.getState() != MATCH_STATE_PLAYTIME )
        return;

    // drop items
    if ( ( G_PointContents( target.origin ) & CONTENTS_NODROP ) == 0 )
    {
        // drop the weapon
        if ( target.client.weapon > WEAP_GUNBLADE )
        {
            GENERIC_DropCurrentWeapon( target.client, true );
        }

        // drop ammo pack (won't drop anything if player doesn't have any ammo)
        target.dropItem( AMMO_PACK );

        if ( ctfAllowPowerupDrop.boolean )
        {
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
	
    // check for generic awards for the frag
    if( @attacker != null && attacker.team != target.team )
		award_playerKilled( @target, @attacker, @inflictor );
}

void CTF_SetVoicecommQuickMenu( Client @client )
{
	String menuStr = '';
	
	menuStr += 
		'"Attack!" "vsay_team attack" ' + 
		'"Defend!" "vsay_team defend" ' +
		'"Area secured" "vsay_team areasecured" ' + 
		'"Go to quad" "vsay_team gotoquad" ' + 
		'"Go to powerup" "vsay_team gotopowerup" ' +		
		'"Need offense" "vsay_team needoffense" ' + 
		'"Need defense" "vsay_team needdefense" ' + 
		'"On offense" "vsay_team onoffense" ' + 
		'"On defense" "vsay_team ondefense" ';

	GENERIC_SetQuickMenu( @client, menuStr );
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
            else if ( token == "weapon" || token == "fullweapon" )
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
        else if ( votename == "ctf_flag_instant" )
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
			
            if ( voteArg == "0" && !ctfInstantFlag.boolean )
            {
                client.printMessage( "Instant flags are already disallowed\n" );
                return false;
            }

            if ( voteArg == "1" && ctfInstantFlag.boolean )
            {
                client.printMessage( "Instant flags are already allowed\n" );
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
        else if ( votename == "ctf_flag_instant" )
        {
            if ( argsString.getToken( 1 ).toInt() > 0 )
                ctfInstantFlag.set( 1 );
            else
                ctfInstantFlag.set( 0 );
        }

        return true;
    }

    return false;
}

void CTF_UpdateBotsExtraGoals()
{
    Entity @ent;
    for ( int i = 1; i <= maxClients; ++i )
    {
        @ent = @G_GetEntity( i );
        CTF_UpdateBotExtraGoals( ent );
    }
}


void CTF_UpdateBotExtraGoals( Entity @ent )
{
    Entity @goal;
    Bot @bot;
    float baseFactor;
    float alphaDist, betaDist, homeDist;

    @bot = @ent.client.getBot();
    if ( @bot == null )
        return;

    float offensiveStatus = 1.0f;

    // play defensive when being a flag carrier
    if ( ( ent.effects & EF_CARRIER ) != 0 )
        offensiveStatus = 0.33f;

    cFlagBase @alphaBase = @CTF_getBaseForTeam( TEAM_ALPHA );
    cFlagBase @betaBase = @CTF_getBaseForTeam( TEAM_BETA );

    // for carriers, find the raw distance to base
    if ( ( ( ent.effects & EF_CARRIER ) != 0 ) && @alphaBase != null && @betaBase != null )
    {
        if ( ent.team == TEAM_ALPHA )
            homeDist = ent.origin.distance( alphaBase.owner.origin );
        else
            homeDist = ent.origin.distance( betaBase.owner.origin );
    }

    // TODO: Do not iterate over all entities but check only bases and flags
    for ( int i = maxClients + 1; i < numEntities; ++i )
    {
        @goal = @G_GetEntity(i);

        // when being a flag carrier have a tendency to stay around your own base
        baseFactor = 1.0f;

        if ( ( ( ent.effects & EF_CARRIER ) != 0 ) && @alphaBase != null && @betaBase != null )
        {
            alphaDist = goal.origin.distance( alphaBase.owner.origin );
            betaDist = goal.origin.distance( betaBase.owner.origin );

            if ( ( ent.team == TEAM_ALPHA ) && ( alphaDist + 64 < betaDist || alphaDist < homeDist + 128 ) )
                baseFactor = 5.0f;
            else if ( ( ent.team == TEAM_BETA ) && ( betaDist + 64 < alphaDist || betaDist < homeDist + 128 ) )
                baseFactor = 5.0f;
            else
                baseFactor = 0.5f;
        }

        // the entities spawned from scripts never have linked items,
        // so the flags are weighted here

        cFlagBase @flagBase = @CTF_getBaseForOwner( goal );

        if ( @flagBase != null && @flagBase.owner != null )
        {
            // enemy or team?

            if ( flagBase.owner.team != ent.team ) // enemy base
            {
                if ( @flagBase.owner == @flagBase.carrier ) // enemy flag is at base
                {
                    bot.setExternalEntityWeight( goal, 12.0f * offensiveStatus );
                }
                else
                {
                    bot.setExternalEntityWeight( goal, 0 );
                }
            }
            else // team
            {
                // flag is at base and this bot has the enemy flag
                if ( ( ent.effects & EF_CARRIER ) != 0 && ( goal.effects & EF_CARRIER ) != 0 )
                {
                    bot.setExternalEntityWeight( goal, 3.5f * baseFactor );
                }
                else
                {
                    bot.setExternalEntityWeight( goal, 0 );
                }
            }

            continue;
        }

        if ( goal.classname == "ctf_flag" )
        {
            // ** please, note, no item has a weight above 1.0 **
            // ** these are really huge weights **

            // it's my flag, dropped somewhere
            if ( goal.team == ent.team )
            {
                bot.setExternalEntityWeight( goal, 5.0f * baseFactor );
            }
            // it's enemy flag, dropped somewhere
            else if ( goal.team != ent.team )
            {
                bot.setExternalEntityWeight( goal, 3.5f * offensiveStatus * baseFactor );
            }

            continue;
        }
    }
}

// select a spawning point for a player
Entity @GT_SelectSpawnPoint( Entity @self )
{
	Entity @spot;
	
    if ( firstSpawn )
    {
        if ( self.team == TEAM_ALPHA )
            @spot = @GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_alphaplayer" );
        else
			@spot = @GENERIC_SelectBestRandomSpawnPoint( self, "team_CTF_betaplayer" );
			
		if( @spot != null )
			return @spot;
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
    int i, t, carrierIcon;

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

            if ( ( ent.effects & EF_CARRIER ) != 0 )
                carrierIcon = ( ent.team == TEAM_BETA ) ? prcAlphaFlagIcon : prcBetaFlagIcon;
            else if ( ent.client.inventoryCount( POWERUP_QUAD ) > 0 )
                carrierIcon = prcShockIcon;
            else if ( ent.client.inventoryCount( POWERUP_SHELL ) > 0 )
                carrierIcon = prcShellIcon;
            else
                carrierIcon = 0;

            int playerID = ( ent.isGhosting() && ( match.getState() == MATCH_STATE_PLAYTIME ) ) ? -( ent.playerNum + 1 ) : ent.playerNum;

            // "Name Score Ping C R"
            entry = "&p " + playerID + " "
                    + ent.client.clanName + " "
                    + ent.client.stats.score + " "
                    + ent.client.ping + " "
                    + carrierIcon + " "
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
    }
    else if ( score_event == "kill" )
    {
        Entity @attacker = null;

        if ( @client != null )
            @attacker = @client.getEnt();

        int arg1 = args.getToken( 0 ).toInt();
        int arg2 = args.getToken( 1 ).toInt();

        CTF_playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
    }
    else if ( score_event == "award" )
    {
    }
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
	Client @client = @ent.client;

    if ( old_team != new_team )
    {
        // ** MISSING CLEAR SCORES **
    }

    if ( ent.isGhosting() )
	{
		GENERIC_ClearQuickMenu( @client );
		ent.svflags &= ~SVF_FORCETEAM;
        return;
	}

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

        if ( match.getState() <= MATCH_STATE_WARMUP )
        {
            for ( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ )
            {
                if ( i == WEAP_INSTAGUN ) // dont add instagun...
                    continue;

                client.inventoryGiveItem( i );

                @item = @G_GetItem( i );

                @ammoItem = @G_GetItem( item.ammoTag );
                if ( @ammoItem != null )
                    client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );

                @ammoItem = item.weakAmmoTag == AMMO_NONE ? null : @G_GetItem( item.weakAmmoTag );
                if ( @ammoItem != null )
                    client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );
            }

            client.inventoryGiveItem( ARMOR_RA );
        }
    }

    // select rocket launcher if available
    if ( client.canSelectWeapon( WEAP_ROCKETLAUNCHER ) )
        client.selectWeapon( WEAP_ROCKETLAUNCHER );
    else
        client.selectWeapon( -1 ); // auto-select best weapon in the inventory

	ent.svflags |= SVF_FORCETEAM;

	CTF_SetVoicecommQuickMenu( @client );

    // add a teleportation effect
    ent.respawnEffect();
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

    if ( match.getState() >= MATCH_STATE_POSTMATCH )
        return;

    // do a rules thinking for all flags
    // and set all players' team tart indicators

    int alphaStatUnlock = 0, alphaStatCap = 0, betaStatUnlock = 0, betaStatCap = 0;
    int alphaCount = 0, betaCount = 0;
    float val = 0;
    int alphaState = 0, betaState = 0; // 0 at base, 1 stolen, 2 dropped

    for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
    {
        flagBase.thinkRules();

        if ( flagBase.owner.team == TEAM_ALPHA )
        {
            if ( CTF_CAPTURE_TIME > 0 )
                alphaStatCap += flagBase.captureTime;
            if ( CTF_UNLOCK_TIME > 0 )
                alphaStatUnlock += flagBase.unlockTime;
            alphaCount++;

            if ( @flagBase.owner == @flagBase.carrier )
                alphaState = 0;
            else if ( @flagBase.carrier.client != null )
                alphaState = 1;
            else if ( @flagBase.carrier != null )
                alphaState = 2;
        }

        if ( flagBase.owner.team == TEAM_BETA )
        {
            if ( CTF_CAPTURE_TIME > 0 )
                betaStatCap += flagBase.captureTime;
            if ( CTF_UNLOCK_TIME > 0 )
                betaStatUnlock += flagBase.unlockTime;
            betaCount++;

            if ( @flagBase.owner == @flagBase.carrier )
                betaState = 0;
            else if ( @flagBase.carrier.client != null )
                betaState = 1;
            else if ( @flagBase.carrier != null )
                betaState = 2;
        }
    }

    if ( alphaCount != 0 )
    {
        if ( CTF_CAPTURE_TIME <= 0 )
            alphaStatCap = 0;
        else
        {
            val = ( float(alphaStatCap) / float(alphaCount) ) / ( CTF_CAPTURE_TIME * 1000 );
            alphaStatCap = int( val * 100 );
        }

        if ( CTF_UNLOCK_TIME <= 0 )
            alphaStatUnlock = 0;
        else
        {
            val = ( float(alphaStatUnlock) / float(alphaCount) ) / ( CTF_UNLOCK_TIME * 1000 );
            alphaStatUnlock = int( val * 100 );
        }
    }

    if ( betaCount != 0 )
    {
        if ( CTF_CAPTURE_TIME <= 0 )
            betaStatCap = 0;
        else
        {
            val = ( float(betaStatCap) / float(betaCount) ) / ( CTF_CAPTURE_TIME * 1000 );
            betaStatCap = int( val * 100 );
        }

        if ( CTF_UNLOCK_TIME <= 0 )
            betaStatUnlock = 0;
        else
        {
            val = ( float(betaStatUnlock) / float(betaCount) ) / ( CTF_UNLOCK_TIME * 1000 );
            betaStatUnlock = int( val * 100 );
        }
    }

    for ( int i = 0; i < maxClients; i++ )
    {
        Entity @ent = @G_GetClient( i ).getEnt();

        if( ent.client.state() < CS_SPAWNED )
            continue;

        // check maxHealth rule
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

        // always clear all before setting
        ent.client.setHUDStat( STAT_PROGRESS_SELF, 0 );
        ent.client.setHUDStat( STAT_PROGRESS_OTHER, 0 );
        ent.client.setHUDStat( STAT_IMAGE_SELF, 0 );
        ent.client.setHUDStat( STAT_IMAGE_OTHER, 0 );
        ent.client.setHUDStat( STAT_PROGRESS_ALPHA, 0 );
        ent.client.setHUDStat( STAT_PROGRESS_BETA, 0 );
        ent.client.setHUDStat( STAT_IMAGE_ALPHA, 0 );
        ent.client.setHUDStat( STAT_IMAGE_BETA, 0 );
        ent.client.setHUDStat( STAT_MESSAGE_SELF, 0 );
        ent.client.setHUDStat( STAT_MESSAGE_OTHER, 0 );
        ent.client.setHUDStat( STAT_MESSAGE_ALPHA, 0 );
        ent.client.setHUDStat( STAT_MESSAGE_BETA, 0 );
        ent.client.setHUDStat( STAT_IMAGE_DROP_ITEM, 0 );

        if ( ent.team == TEAM_ALPHA )
        {
            // if our flag is being stolen
            if ( alphaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_SELF, -( alphaStatUnlock ) );
            // we are capturing the enemy's flag
            else if ( alphaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_SELF, alphaStatCap );

            // we are unlocking enemy's flag
            if ( betaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_OTHER, betaStatUnlock );
            // the enemy is capturing our flag
            else if ( betaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_OTHER, -( betaStatCap ) );

            if ( @CTF_getBaseForCarrier( ent ) != null )
            {
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconCarrier );
                ent.client.setHUDStat( STAT_IMAGE_DROP_ITEM, prcDropFlagIcon );
            }
            else if ( betaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconLost );
            else if ( betaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconStolen );

            if ( alphaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_SELF, prcFlagIconLost );
            else if ( alphaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_SELF, prcFlagIconStolen );
        }
        else if ( ent.team == TEAM_BETA )
        {
            // if our flag is being stolen
            if ( betaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_SELF, -( betaStatUnlock ) );
            // we are capturing the enemy's flag
            else if ( betaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_SELF, betaStatCap );

            // we are unlocking enemy's flag
            if ( alphaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_OTHER, alphaStatUnlock );
            // the enemy is capturing our flag
            else if ( alphaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_OTHER, -( alphaStatCap ) );

            if ( @CTF_getBaseForCarrier( ent ) != null )
            {
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconCarrier );
                ent.client.setHUDStat( STAT_IMAGE_DROP_ITEM, prcDropFlagIcon );
            }
            else if ( alphaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconLost );
            else if ( alphaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_OTHER, prcFlagIconStolen );

            if ( betaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_SELF, prcFlagIconLost );
            else if ( betaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_SELF, prcFlagIconStolen );
        }
        else if ( ent.client.chaseActive == false ) // don't bother with people in chasecam, they will get a copy of their chase target stat
        {
            if ( alphaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_ALPHA, prcFlagIconLost );
            else if ( alphaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_ALPHA, prcFlagIconStolen );
            else
                ent.client.setHUDStat( STAT_IMAGE_ALPHA, prcFlagIcon );

            if ( betaState == 2 )
                ent.client.setHUDStat( STAT_IMAGE_BETA, prcFlagIconLost );
            else if ( betaState == 1 )
                ent.client.setHUDStat( STAT_IMAGE_BETA, prcFlagIconStolen );
            else
                ent.client.setHUDStat( STAT_IMAGE_BETA, prcFlagIcon );

            // alpha flag is being unlocked
            if ( alphaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_ALPHA, -( alphaStatUnlock ) );
            // alpha is capturing the enemy's flag
            else if ( alphaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_ALPHA, alphaStatCap );

            // beta flag is being unlocked
            if ( betaStatUnlock != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_BETA, -( betaStatUnlock ) );
            // beta is capturing the enemy's flag
            else if ( betaStatCap != 0 )
                ent.client.setHUDStat( STAT_PROGRESS_BETA, betaStatCap );
        }
    }

    CTF_UpdateBotsExtraGoals();
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
        firstSpawn = false;
        GENERIC_SetUpWarmup();
		SpawnIndicators::Create( "team_CTF_alphaplayer", TEAM_ALPHA );
		SpawnIndicators::Create( "team_CTF_alphaspawn", TEAM_ALPHA );
		SpawnIndicators::Create( "team_CTF_betaplayer", TEAM_BETA );
		SpawnIndicators::Create( "team_CTF_betaspawn", TEAM_BETA );
        break;

    case MATCH_STATE_COUNTDOWN:
        GENERIC_SetUpCountdown();
		SpawnIndicators::Delete();	
        firstSpawn = true;
        break;

    case MATCH_STATE_PLAYTIME:
        GENERIC_SetUpMatch();
        CTF_ResetFlags();
        firstSpawn = false;
        break;

    case MATCH_STATE_POSTMATCH:
        firstSpawn = false;
        GENERIC_SetUpEndMatch();
        CTF_ResetFlags();
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
    gametype.title = "Capture the Flag";
    gametype.version = "1.04";
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

    gametype.spawnableItemsMask = ( IT_WEAPON | IT_AMMO | IT_ARMOR | IT_POWERUP | IT_HEALTH );
    if ( gametype.isInstagib )
        gametype.spawnableItemsMask &= ~uint(G_INSTAGIB_NEGATE_ITEMMASK);

    gametype.respawnableItemsMask = gametype.spawnableItemsMask ;
    gametype.dropableItemsMask = gametype.spawnableItemsMask ;
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
    gametype.canForceModels = true;
    gametype.canShowMinimap = false;
    gametype.teamOnlyMinimap = true;

	gametype.mmCompatible = true;
	
    gametype.spawnpointRadius = 256;

    if ( gametype.isInstagib )
    {
        gametype.spawnpointRadius *= 2;
        CTF_UNLOCK_TIME = 0;
        CTF_CAPTURE_TIME = 0;
        CTF_UNLOCK_RADIUS = 0;
        CTF_CAPTURE_RADIUS = 0;
    }

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

    G_RegisterCallvote( "ctf_powerup_drop", "1 or 0", "bool", "Enables or disables the dropping of powerups at dying" );
    G_RegisterCallvote( "ctf_flag_instant", "1 or 0", "bool", "Enables or disables instant flag captures and unlocks" );


    G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
