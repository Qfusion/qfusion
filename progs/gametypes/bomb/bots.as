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

void BOMB_UpdateBotsExtraGoals()
{
    Entity @ent;
    for ( int i = 1; i <= maxClients; ++i )
    {
        @ent = G_GetEntity( i );
        if ( ent.inuse )
            BOMB_UpdateBotExtraGoals( ent );
    }
}

void BOMB_UpdateBotExtraGoals( Entity @self )
{
    Entity @goal;
    Bot @bot;

    @bot = @self.client.getBot();
    if ( @bot == null )
        return;
   
    // TODO: Do not iterate over all non-player entities but check only needed ones
    for ( int i = maxClients + 1; i < numEntities; ++i )
    {
        @goal = @G_GetEntity( i );

		if( @bombCarrier != null && @bombCarrier == @self )
		{
			if( goal.classname == "capture_indicator_model" )
			{
				if( @BOMB_BOTS_SITE != null && @BOMB_BOTS_SITE.model == @goal )
					bot.setExternalEntityWeight( goal, 5.0f );
				else
					bot.setExternalEntityWeight( goal, 2.0f );

				continue;
			}
		}
		
		// a goal has been assigned by the round start
		if( @self.owner != null && @self.owner == @goal )
		{
			if( self.team == defendingTeam )
			{
				bot.setExternalEntityWeight( goal, 3.0f );
			}
			else
			{
				bot.setExternalEntityWeight( goal, random() );
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
					bot.setExternalEntityWeight( goal, 3.0f );
					continue;
				}
			}
			else if( bombState == BOMBSTATE_ARMED )
			{
				if( self.team == defendingTeam )
				{
					bot.setExternalEntityWeight( goal, 2.0f );
					continue;
				}
			}
		}
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
