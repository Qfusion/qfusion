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

void BOMB_SetEntityGoalWeightForTeam( int teamNum, Entity @goal, float weight )
{
    Team @team = @G_GetTeam( teamNum );
    for ( int i = 0; i < team.numPlayers; ++i )
    {
        Bot @bot = team.ent( i ).client.getBot();
        if ( @bot != null )
            bot.overrideEntityWeight( goal, weight );
    }
}

cBombSite @BOMB_PickRandomTargetSite( )
{
    String siteLetter;
    cBombSite @site;
    
    siteLetter = SITE_LETTERS[ ( ( random() < 0.5f ) ? 0 : 1 ) ];
	
	for ( @site = @siteHead; @site != null; @site = @site.next )
    {
        if ( site.letter == siteLetter )
            return @site;
    }

	return null;
}

void BOMB_SetupNewBotsRound()
{
    // Remove old defence spots
    switch ( oldBombState )
    {
        // Defence spots have not been added. Do nothing.
        case BOMBSTATE_IDLE:
            break;
        
        // Defence spots have been added and were valid.
        // Since all entities have been invalidated, 
        // we should invalidate old defence spots based on these entities.
        case BOMBSTATE_CARRIED:
        case BOMBSTATE_DROPPING:
            BOMB_RemoveDefenceSpotsForSites();
            break;

        // Defenders had a defence spot, it should be removed
        case BOMBSTATE_DROPPED:
        case BOMBSTATE_PLANTING:
        case BOMBSTATE_PLANTED:
            AI::RemoveDefenceSpot( oldDefendingTeam, 0 );
            break;

        // Attackers had a defence spot, it should be removed
        case BOMBSTATE_ARMED:
        case BOMBSTATE_EXPLODING_ANIM:
        case BOMBSTATE_EXPLODING:
            AI::RemoveDefenceSpot( oldAttackingTeam, 0 );
            break;
    }
    
    // Setup initial defence spots for current defenders
    BOMB_AddDefenceSpotsForSites();

    // Clear all external entity weights
    for ( int i = 0; i < maxClients; ++i )
    {
        Bot @bot = G_GetClient( i ).getBot();
        if ( @bot != null )
            bot.clearOverriddenEntityWeights();
    }
    
    @BOMB_BOTS_SITE = @BOMB_PickRandomTargetSite();
}

// Defence spots have these id's:
// 0 - special defence spot that corresponds to the bomb model
// 1, ... - spots that correspond to sites

void BOMB_AddDefenceSpotsForSites()
{    
    int spotId = 1;
	for ( cBombSite @site = @siteHead; @site != null; @site = @site.next )
    {
        AIDefenceSpot defenceSpot( spotId, site.indicator, 768.0f );
        // Allow to leave the spot to defend the one being attacked now.        
        defenceSpot.minDefenders = 0;
        // This value will be clamped to a maximum supported one.
        defenceSpot.maxDefenders = 999;
        defenceSpot.regularEnemyAlertScale = 1.5f;
        defenceSpot.carrierEnemyAlertScale = 5.0f;
        AI::AddDefenceSpot( defendingTeam, defenceSpot );
        // Force all defenders available for the spot to reach the spot
        AI::DefenceSpotAlert( defendingTeam, spotId, 0.5f, uint(15000) );
        spotId++;
    }
}

void BOMB_RemoveDefenceSpotsForSites()
{    
    int spotId = 1;
	for ( cBombSite @site = @siteHead; @site != null; @site = @site.next )
    {
        AI::RemoveDefenceSpot( defendingTeam, spotId );
        spotId++;
    }
}


