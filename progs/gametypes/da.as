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

const int DA_ROUNDSTATE_NONE = 0;
const int DA_ROUNDSTATE_PREROUND = 1;
const int DA_ROUNDSTATE_ROUND = 2;
const int DA_ROUNDSTATE_ROUNDFINISHED = 3;
const int DA_ROUNDSTATE_POSTROUND = 4;

class cDARound
{
    int state;
    int numRounds;
    int64 roundStateStartTime;
    int64 roundStateEndTime;
    int countDown;
    int[] daChallengersQueue;
    Entity @alphaSpawn;
    Entity @betaSpawn;
    Client @roundWinner;
    Client @roundChallenger;

    cDARound()
    {
        this.state = DA_ROUNDSTATE_NONE;
        this.numRounds = 0;
        this.roundStateStartTime = 0;
        this.countDown = 0;
        @this.alphaSpawn = null;
        @this.betaSpawn = null;
        @this.roundWinner = null;
        @this.roundChallenger = null;
    }

    ~cDARound() {}

    void init()
    {
        this.clearChallengersQueue();
    }

    void clearChallengersQueue()
    {
        if ( this.daChallengersQueue.length() != uint( maxClients ) )
            this.daChallengersQueue.resize( maxClients );

        for ( int i = 0; i < maxClients; i++ )
            this.daChallengersQueue[i] = -1;
    }

    void challengersQueueAddPlayer( Client @client )
    {
        if ( @client == null )
            return;

        // check for already added
        for ( int i = 0; i < maxClients; i++ )
        {
            if ( this.daChallengersQueue[i] == client.playerNum )
                return;
        }

        for ( int i = 0; i < maxClients; i++ )
        {
            if ( this.daChallengersQueue[i] < 0 || this.daChallengersQueue[i] >= maxClients )
            {
                this.daChallengersQueue[i] = client.playerNum;
                break;
            }
        }
    }

    bool challengersQueueRemovePlayer( Client @client )
    {
        if ( @client == null )
            return false;

        for ( int i = 0; i < maxClients; i++ )
        {
            if ( this.daChallengersQueue[i] == client.playerNum )
            {
                int j;
                for ( j = i + 1; j < maxClients; j++ )
                {
                    this.daChallengersQueue[j - 1] = this.daChallengersQueue[j];
                    if ( daChallengersQueue[j] == -1 )
                        break;
                }

                this.daChallengersQueue[j] = -1;
                return true;
            }
        }

        return false;
    }

    Client @challengersQueueGetNextPlayer()
    {
        Client @client = @G_GetClient( this.daChallengersQueue[0] );

        if ( @client != null )
        {
            this.challengersQueueRemovePlayer( client );
        }

        return client;
    }

    void playerTeamChanged( Client @client, int new_team )
    {
        if ( new_team != TEAM_PLAYERS )
        {
            this.challengersQueueRemovePlayer( client );

            if ( this.state != DA_ROUNDSTATE_NONE )
            {
                if ( @client == @this.roundWinner )
                {
                    @this.roundWinner = null;
                    this.newRoundState( DA_ROUNDSTATE_ROUNDFINISHED );
                }

                if ( @client == @this.roundChallenger )
                {
                    @this.roundChallenger = null;
                    this.newRoundState( DA_ROUNDSTATE_ROUNDFINISHED );
                }
            }
        }
        else if ( new_team == TEAM_PLAYERS )
        {
            this.challengersQueueAddPlayer( client );
        }
    }

    void roundAnnouncementPrint( String &string )
    {
        if ( string.len() <= 0 )
            return;

        if ( @this.roundWinner != null )
            G_CenterPrintMsg( this.roundWinner.getEnt(), string );

        if ( @this.roundChallenger != null )
            G_CenterPrintMsg( this.roundChallenger.getEnt(), string );

        // also add it to spectators who are not in chasecam

        Team @team = @G_GetTeam( TEAM_SPECTATOR );
        Entity @ent;

        // respawn all clients inside the playing teams
        for ( int i = 0; @team.ent( i ) != null; i++ )
        {
            @ent = @team.ent( i );
            if ( !ent.isGhosting() )
                G_CenterPrintMsg( ent, string );
        }
    }

    void newGame()
    {
        gametype.readyAnnouncementEnabled = false;
        gametype.scoreAnnouncementEnabled = true;
        gametype.countdownEnabled = false;

        // set spawnsystem type to not respawn the players when they die
        for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
            gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_HOLD, 0, 0, true );

        // clear scores

        Entity @ent;
        Team @team;

        for ( int i = TEAM_PLAYERS; i < GS_MAX_TEAMS; i++ )
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

        this.numRounds = 0;
        this.newRound();
    }

    void endGame()
    {
        this.newRoundState( DA_ROUNDSTATE_NONE );

        if ( @this.roundWinner != null )
        {
            Cvar scoreLimit( "g_scorelimit", "", 0 );
            if ( this.roundWinner.stats.score == scoreLimit.integer )
            {
                this.roundAnnouncementPrint( S_COLOR_WHITE + this.roundWinner.name + S_COLOR_GREEN + " wins the game!" );
            }
        }

        GENERIC_SetUpEndMatch();
    }

    void newRound()
    {
        G_RemoveDeadBodies();
        G_RemoveAllProjectiles();

        this.newRoundState( DA_ROUNDSTATE_PREROUND );
        this.numRounds++;
    }

    void newRoundState( int newState )
    {
        if ( newState > DA_ROUNDSTATE_POSTROUND )
        {
            this.newRound();
            return;
        }

        this.state = newState;
        this.roundStateStartTime = levelTime;

        switch ( this.state )
        {
        case DA_ROUNDSTATE_NONE:

            this.roundStateEndTime = 0;
            this.countDown = 0;
            break;

        case DA_ROUNDSTATE_PREROUND:
        {
            this.roundStateEndTime = levelTime + 7000;
            this.countDown = 5;

            // respawn everyone and disable shooting
            gametype.shootingDisabled = true;
            gametype.removeInactivePlayers = false;

            // pick the last round winner and the first in queue,
            // or, if no round winner, the 2 first in queue
            if ( @this.roundWinner == null )
            {
                @this.roundWinner = @this.challengersQueueGetNextPlayer();
                @this.roundChallenger = @this.challengersQueueGetNextPlayer();
            }
            else
            {
                @this.roundChallenger = @this.challengersQueueGetNextPlayer();
            }

            Entity @ent;
            Team @team;

            @team = @G_GetTeam( TEAM_PLAYERS );

            // respawn all clients inside the playing teams
            for ( int j = 0; @team.ent( j ) != null; j++ )
            {
                @ent = @team.ent( j );

                if ( @ent.client == @this.roundWinner || @ent.client == @this.roundChallenger )
                {
                    ent.client.respawn( false );
                }
                else
                {
                    ent.client.respawn( true );
                    ent.client.chaseCam( null, true );
                }
            }

            this.roundAnnouncementPrint( S_COLOR_GREEN + "New Round:" );
            this.roundAnnouncementPrint( S_COLOR_WHITE + this.roundWinner.name
                                         + S_COLOR_GREEN + " vs. "
                                         + S_COLOR_WHITE + this.roundChallenger.name );
        }
        break;

        case DA_ROUNDSTATE_ROUND:
        {
            gametype.shootingDisabled = false;
            gametype.removeInactivePlayers = true;
            this.countDown = 0;
            this.roundStateEndTime = 0;
            int soundIndex = G_SoundIndex( "sounds/announcer/countdown/fight0" + (1 + (rand() & 1)) );
            G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
            G_CenterPrintMsg( null, 'Fight!');
        }
        break;

        case DA_ROUNDSTATE_ROUNDFINISHED:

            gametype.shootingDisabled = true;
            this.roundStateEndTime = levelTime + 1500;
            this.countDown = 0;
            break;

        case DA_ROUNDSTATE_POSTROUND:
        {
            this.roundStateEndTime = levelTime + 3000;

            // add score to round-winning player
            Client @winner = null;
            Client @loser = null;

            // watch for one of the players removing from the game
            if ( @this.roundWinner == null || @this.roundChallenger == null )
            {
                if ( @this.roundWinner != null )
                    @winner = @this.roundWinner;
                else if ( @this.roundChallenger != null )
                    @winner = @this.roundChallenger;
            }
            else if ( !this.roundWinner.getEnt().isGhosting() && this.roundChallenger.getEnt().isGhosting() )
            {
                @winner = @this.roundWinner;
                @loser = @this.roundChallenger;
            }
            else if ( this.roundWinner.getEnt().isGhosting() && !this.roundChallenger.getEnt().isGhosting() )
            {
                @winner = @this.roundChallenger;
                @loser = @this.roundWinner;
            }

            // if we didn't find a winner, it was a draw round
            if ( @winner == null )
            {
                this.roundAnnouncementPrint( S_COLOR_ORANGE + "Draw Round!" );

				this.challengersQueueAddPlayer( this.roundWinner );
				this.challengersQueueAddPlayer( this.roundChallenger );
            }
            else
            {
                int soundIndex;

                soundIndex = G_SoundIndex( "sounds/announcer/ctf/score0" + (1 + (rand() & 1)) );
                G_AnnouncerSound( winner, soundIndex, GS_MAX_TEAMS, false, null );

                if ( @loser != null )
                {
                    winner.stats.addScore( 1 );

                    soundIndex = G_SoundIndex( "sounds/announcer/ctf/score_enemy0" + (1 + (rand() & 1)) );
                    G_AnnouncerSound( loser, soundIndex, GS_MAX_TEAMS, false, null );
                    this.challengersQueueAddPlayer( loser );
                }

                this.roundAnnouncementPrint( S_COLOR_WHITE + winner.name + S_COLOR_GREEN + " wins the round!" );
            }

            @this.roundWinner = @winner;
            @this.roundChallenger = null;
        }
        break;

        default:
            break;
        }
    }

    void think()
    {
        if ( this.state == DA_ROUNDSTATE_NONE )
            return;

        if ( match.getState() != MATCH_STATE_PLAYTIME )
        {
            this.endGame();
            return;
        }

        if ( this.roundStateEndTime != 0 )
        {
            if ( this.roundStateEndTime < levelTime )
            {
                this.newRoundState( this.state + 1 );
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
                    G_CenterPrintMsg( null, String( this.countDown ) );
                }
            }
        }

        // if one of the teams has no players alive move from DA_ROUNDSTATE_ROUND
        if ( this.state == DA_ROUNDSTATE_ROUND )
        {
            Entity @ent;
            Team @team;
            int count;

            @team = @G_GetTeam( TEAM_PLAYERS );
            count = 0;

            for ( int j = 0; @team.ent( j ) != null; j++ )
            {
                @ent = @team.ent( j );
                if ( !ent.isGhosting() )
                    count++;

				// do not let the challengers be moved to specs due to inactivity
				if ( ent.client.chaseActive ) {
					ent.client.lastActivity = levelTime;
				}
            }

            if ( count < 2 )
                this.newRoundState( this.state + 1 );
        }
    }

    void playerKilled( Entity @target, Entity @attacker, Entity @inflictor )
    {
        if ( this.state != DA_ROUNDSTATE_ROUND )
            return;

        if ( @target == null || @target.client == null )
            return;

        if ( @attacker == null || @attacker.client == null )
            return;

        target.client.printMessage( "You were fragged by " + attacker.client.name + " (health: " + rint( attacker.health ) + ", armor: " + rint( attacker.client.armor ) + ")\n" );

        for ( int i = 0; i < maxClients; i++ )
        {
            Client @client = @G_GetClient( i );
            if ( client.state() < CS_SPAWNED )
                continue;

            if ( @client == @this.roundWinner || @client == @this.roundChallenger )
                continue;

            client.printMessage( target.client.name + " was fragged by " + attacker.client.name + " (health: " + int( attacker.health ) + ", armor: " + int( attacker.client.armor ) + ")\n" );
        }
        
        // check for generic awards for the frag
		award_playerKilled( @target, @attacker, @inflictor );
    }
}

cDARound daRound;

///*****************************************************************
/// NEW MAP ENTITY DEFINITIONS
///*****************************************************************


///*****************************************************************
/// LOCAL FUNCTIONS
///*****************************************************************

void DA_SetUpWarmup()
{
    GENERIC_SetUpWarmup();

    // set spawnsystem type to instant while players join
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

    gametype.readyAnnouncementEnabled = true;
}

void DA_SetUpCountdown()
{
    gametype.shootingDisabled = true;
    gametype.readyAnnouncementEnabled = false;
    gametype.scoreAnnouncementEnabled = false;
    gametype.countdownEnabled = false;
    G_RemoveAllProjectiles();

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

    int soundIndex = G_SoundIndex( "sounds/announcer/countdown/get_ready_to_fight0" + (1 + (rand() & 1)) );
    G_AnnouncerSound( null, soundIndex, GS_MAX_TEAMS, false, null );
}

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
    return GENERIC_SelectBestRandomSpawnPoint( self, "info_player_deathmatch" );
}

String @GT_ScoreboardMessage( uint maxlen )
{
    String scoreboardMessage = "";
    String entry;
    Team @team;
    Client @client;
    int i, playerID;

    @team = @G_GetTeam( TEAM_PLAYERS );

    // &t = team tab, team tag, team score (doesn't apply), team ping (doesn't apply)
    entry = "&t " + int( TEAM_PLAYERS ) + " " + team.stats.score + " 0 ";
    if ( scoreboardMessage.len() + entry.len() < maxlen )
        scoreboardMessage += entry;

    // add first the two players in the duel
    @client = @daRound.roundWinner;
    if ( @client != null )
    {
        if ( match.getState() != MATCH_STATE_PLAYTIME )
            playerID = client.playerNum;
        else
            playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

        entry = "&p " + playerID + " "
                + client.clanName + " "
                + client.stats.score + " "
                + client.ping + " "
                + ( client.isReady() ? "1" : "0" ) + " ";

        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;
    }

    @client = @daRound.roundChallenger;
    if ( @client != null )
    {
        if ( match.getState() != MATCH_STATE_PLAYTIME )
            playerID = client.playerNum;
        else
            playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

        entry = "&p " + playerID + " "
                + client.clanName + " "
                + client.stats.score + " "
                + client.ping + " "
                + ( client.isReady() ? "1" : "0" ) + " ";

        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;
    }

    // then add all the players in the queue
    for ( i = 0; i < maxClients; i++ )
    {
        if ( daRound.daChallengersQueue[i] < 0 || daRound.daChallengersQueue[i] >= maxClients )
            break;

        @client = @G_GetClient( daRound.daChallengersQueue[i] );
        if ( @client == null )
            break;

        if ( match.getState() != MATCH_STATE_PLAYTIME )
            playerID = client.playerNum;
        else
            playerID = client.getEnt().isGhosting() ? -( client.playerNum + 1 ) : client.playerNum;

        entry = "&p " + playerID + " "
                + client.clanName + " "
                + client.stats.score + " "
                + client.ping + " "
                + ( client.isReady() ? "1" : "0" ) + " ";

        if ( scoreboardMessage.len() + entry.len() < maxlen )
            scoreboardMessage += entry;

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
        daRound.playerKilled( G_GetEntity( arg1 ), attacker, G_GetEntity( arg2 ) );
    }
    else if ( score_event == "award" )
    {
    }
}

// a player is being respawned. This can happen from several ways, as dying, changing team,
// being moved to ghost state, be placed in respawn queue, being spawned from spawn queue, etc
void GT_PlayerRespawn( Entity @ent, int old_team, int new_team )
{
    if ( old_team != new_team )
    {
        daRound.playerTeamChanged( ent.client, new_team );
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

        for ( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ )
        {
            if ( i == WEAP_INSTAGUN ) // dont add instagun...
                continue;

            ent.client.inventoryGiveItem( i );

            @item = @G_GetItem( i );

            @ammoItem = @G_GetItem( item.ammoTag );
            if ( @ammoItem != null )
                ent.client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );

            @ammoItem = item.weakAmmoTag == AMMO_NONE ? null : @G_GetItem( item.weakAmmoTag );
            if ( @ammoItem != null )
                ent.client.inventorySetCount( ammoItem.tag, ammoItem.inventoryMax );
        }

        // remove strong EB ammo
        ent.client.inventorySetCount( AMMO_BOLTS, 5 );

        // give inventory
        ent.client.armor = 150;
    }

    // auto-select best weapon in the inventory
    ent.client.selectWeapon( -1 );

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

    daRound.think();
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
        DA_SetUpWarmup();
        break;

    case MATCH_STATE_COUNTDOWN:
        DA_SetUpCountdown();
        break;

    case MATCH_STATE_PLAYTIME:
        daRound.newGame();
        break;

    case MATCH_STATE_POSTMATCH:
        daRound.endGame();
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
    gametype.title = "Duel Arena";
    gametype.version = "1.02";
    gametype.author = "Warsow Development Team";

    daRound.init();

    // if the gametype doesn't have a config file, create it
    if ( !G_FileExists( "configs/server/gametypes/" + gametype.name + ".cfg" ) )
    {
        String config;

        // the config file doesn't exist or it's empty, create it
        config = "// '" + gametype.title + "' gametype configuration file\n"
                 + "// This config will be executed each time the gametype is started\n"
                 + "\n\n// map rotation\n"
                 + "set g_maplist \"wda1 wda2 wda3 wda4 wda5\" // list of maps in automatic rotation\n"
                 + "set g_maprotation \"0\"   // 0 = same map, 1 = in order, 2 = random\n"
                 + "\n// game settings\n"
                 + "set g_scorelimit \"11\"\n"
                 + "set g_timelimit \"0\"\n"
                 + "set g_warmup_timelimit \"1\"\n"
                 + "set g_match_extendedtime \"0\"\n"
                 + "set g_allow_falldamage \"0\"\n"
                 + "set g_allow_selfdamage \"0\"\n"
                 + "set g_allow_teamdamage \"0\"\n"
                 + "set g_allow_stun \"1\"\n"
                 + "set g_teams_maxplayers \"0\"\n"
                 + "set g_teams_allow_uneven \"0\"\n"
                 + "set g_countdown_time \"3\"\n"
                 + "set g_maxtimeouts \"3\" // -1 = unlimited\n"
                 + "\necho \"" + gametype.name + ".cfg executed\"\n";

        G_WriteFile( "configs/server/gametypes/" + gametype.name + ".cfg", config );
        G_Print( "Created default config file for '" + gametype.name + "'\n" );
        G_CmdExecute( "exec configs/server/gametypes/" + gametype.name + ".cfg silent" );
    }

    gametype.spawnableItemsMask = 0;
    gametype.respawnableItemsMask = 0;
    gametype.dropableItemsMask = 0;
    gametype.pickableItemsMask = 0;

    gametype.isTeamBased = false;
    gametype.isRace = false;
    gametype.hasChallengersQueue = false;
    gametype.maxPlayersPerTeam = 0;

    gametype.ammoRespawn = 0;
    gametype.armorRespawn = 0;
    gametype.weaponRespawn = 0;
    gametype.healthRespawn = 0;
    gametype.powerupRespawn = 0;
    gametype.megahealthRespawn = 0;
    gametype.ultrahealthRespawn = 0;

    gametype.readyAnnouncementEnabled = false;
    gametype.scoreAnnouncementEnabled = false;
    gametype.countdownEnabled = false;
    gametype.mathAbortDisabled = false;
    gametype.shootingDisabled = false;
    gametype.infiniteAmmo = false;
    gametype.canForceModels = true;
    gametype.canShowMinimap = false;
    gametype.teamOnlyMinimap = true;
    gametype.removeInactivePlayers = true;

	gametype.mmCompatible = true;
	
    gametype.spawnpointRadius = 64;

    if ( gametype.isInstagib )
        gametype.spawnpointRadius *= 2;

    // set spawnsystem type to instant while players join
    for ( int team = TEAM_PLAYERS; team < GS_MAX_TEAMS; team++ )
        gametype.setTeamSpawnsystem( team, SPAWNSYSTEM_INSTANT, 0, 0, false );

    // define the scoreboard layout
    G_ConfigString( CS_SCB_PLAYERTAB_LAYOUT, "%n 112 %s 52 %i 52 %l 48 %r l1" );
    G_ConfigString( CS_SCB_PLAYERTAB_TITLES, "Name Clan Score Ping R" );

    // add commands
    G_RegisterCommand( "gametype" );

    G_Print( "Gametype '" + gametype.title + "' initialized\n" );
}
