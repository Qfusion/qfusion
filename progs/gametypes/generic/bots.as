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

// Finds entities that might suit as a dummy goal.
array<Entity @> @GENERIC_FindBotroamEntities()
{
    array<Entity @> result;

    for ( int i = maxClients + 1; i < numEntities; ++i )
    {
        Entity @ent = G_GetEntity( i );
        // Teleports
        if ( ent.classname == "trigger_teleport" )
            result.insertLast( ent );
        // Jumppads
        else if ( ent.classname == "trigger_push" )
            result.insertLast( ent );
        // Elevators
        else if ( ent.classname == "trigger_elevator" )
            result.insertLast( ent );
    }

    return result;
}

array<Entity @> @botroamEntities = null;

// Adds dummy goals for bots, so they do not feel themselves lost on a map without items.
void GENERIC_AddBotroamGoals()
{
    if ( @botroamEntities == null )
        @botroamEntities = @GENERIC_FindBotroamEntities();

    for ( uint i = 0; i < botroamEntities.length(); ++i )
        AI::AddNavEntity( botroamEntities[ i ], AI_NAV_REACH_AT_RADIUS );
}

// Removes dummy goals for bots.
void GENERIC_RemoveBotroamGoals()
{   
    // We have to clear entities weights to prevent reusing these weights for newly spawned entities.
    for ( int clientNum = 0; clientNum < maxClients; ++clientNum )
    {
        Client @client = G_GetClient( clientNum );
        Bot @bot = client.getBot();
        if ( @bot == null )
            continue;
        
        for ( uint i = 0; i < botroamEntities.length(); ++i )
            bot.overrideEntityWeight( botroamEntities[ i ], 0.0f );
    }

    for ( uint i = 0; i < botroamEntities.length(); ++i )
        AI::RemoveNavEntity( botroamEntities[ i ] );
}

void GENERIC_UpdateBotroamGoalsWeights()
{
    for ( int i = 0; i < maxClients; ++i )
    {
        Client @client = G_GetClient( i );
        Bot @bot = client.getBot();
        if ( @bot == null )
            continue;

        for ( uint j = 0; j < botroamEntities.length(); ++j )
        {
            Entity @goal = botroamEntities[ j ];
            float distance = client.getEnt().origin.distance( goal.origin );
            if ( distance < 512.0f )
            {
                bot.overrideEntityWeight( goal, 0.0f );
            }            
            else
            {
                float factor = 1.0f - ( distance - 512.0f ) / 10000.0f;
                if ( factor < 0.0f )
                    factor = 0.0f;

                bot.overrideEntityWeight( goal, 999.0f * factor );
            }  
        }
    }
}
