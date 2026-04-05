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


float BOMB_PlayerWeight( Entity @self, Entity @enemy )
{
    float weight;

    if ( @enemy == null || @enemy == @self )
        return 0;

    if ( enemy.isGhosting() )
        return 0;

    //if not team based give some weight to every one
    if ( gametype.isTeamBased && ( enemy.team == self.team ) )
        return 0;

    if( !self.client.isBot() )
        return 0.0f;

    weight = 0.35f;

	// bomb carrier is someone you want to kill
	cPlayer @player = @playerFromClient( @enemy.client );
	if( player.isCarrier )
		weight *= 1.5f;

    return weight;
}



// When this function is called the weights of items have been reset to their default values,
// this means, the weights *are set*, and what this function does is scaling them depending
// on the current bot status.
// Player, and non-item entities don't have any weight set. So they will be ignored by the bot
// unless a weight is assigned here.
bool BOMB_UpdateBotStatus( Entity @self )
{
    Entity @goal;
    Bot @bot;

    @bot = @self.client.getBot();
    if ( @bot == null )
        return false;

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
            bot.setGoalWeight( i, BOMB_PlayerWeight( self, goal ) );
            continue;
        }

		if( @bombCarrier != null && @bombCarrier == @self )
		{
			if( goal.classname == "capture_indicator_model" )
			{
				if( @BOMB_BOTS_SITE != null && @BOMB_BOTS_SITE.model == @goal )
					bot.setGoalWeight( i, 5.0f );
				else
					bot.setGoalWeight( i, 2.0f );

				continue;
			}
		}
		
		// a goal has been assigned by the round start
		if( @self.owner != null && @self.owner == @goal )
		{
			if( self.team == defendingTeam )
			{
				bot.setGoalWeight( i, 3.0f );
			}
			else
			{
				bot.setGoalWeight( i, random() );
			}
			
			@self.owner = null;
			continue;
		}

		if( @bombModel != null && @bombModel == @goal )
		{
			if( ( bombState == BOMBSTATE_PLANTED ) || ( bombState == BOMBSTATE_DROPPED ) )
			{
				if( self.team == attackingTeam )
				{
					bot.setGoalWeight( i, 3.0f );
					continue;
				}
			}
			else if( bombState == BOMBSTATE_ARMED )
			{
				if( self.team == defendingTeam )
				{
					bot.setGoalWeight( i, 2.0f );
					continue;
				}
			}
		}

        // we don't know what entity is this, so ignore it
        bot.setGoalWeight( i, 0 );
    }

    return true; // handled by the script
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

// assign a random site as initial moving goal
void BOMB_assignRandomDenfenseStart( )
{
	cBombSite @site;
	Entity @ent;
	Team @team = @G_GetTeam( defendingTeam );
	
	for ( int i = 0; @team.ent( i ) != null; i++ )
	{
		@ent = @team.ent( i );
		@ent.owner = null;

		if ( !ent.client.isBot() )
			continue;

		@site = @BOMB_PickRandomTargetSite();
		
		if( @site != null )
			@ent.owner = @site.model;
	}
	
	@team = @G_GetTeam( attackingTeam );
	
	for ( int i = 0; @team.ent( i ) != null; i++ )
	{
		@ent = @team.ent( i );
		@ent.owner = null;

		if ( !ent.client.isBot() )
			continue;

		@site = @BOMB_PickRandomTargetSite();
		
		if( @site != null )
			@ent.owner = @site.model;
	}
}