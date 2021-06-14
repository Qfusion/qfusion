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

bool GENERIC_BotInRageMode( Entity @self )
{
    if ( self.health < 50 )
        return false;

    if ( ( self.effects & EF_QUAD ) != 0 || ( self.effects & EF_SHELL ) != 0 )
        return true;

    return false;
}

float GENERIC_WeaponStatus( Entity @self )
{
    float weaponStatus;
    int numWeapons = 0;

    if ( gametype.isInstagib )
        return 1.0f;

    for ( int i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ )
    {
        if ( self.client.canSelectWeapon( i ) )
            numWeapons++;
    }

    if ( numWeapons < 1 )
        weaponStatus = 0;
    else
        weaponStatus = float( numWeapons ) / float( WEAP_TOTAL - 1 );

    return weaponStatus * weaponStatus;
}

float GENERIC_HealthStatus( Entity @self )
{
    float healthStatus;

    if ( gametype.isInstagib )
        return 1.0f;

    if( self.health <= 0 )
        return 0.0f;

    healthStatus = self.health / float( self.maxHealth ); // can be bigger than 1 if using MH

    if ( healthStatus > 1.0f )
        healthStatus = 1.0f;

    return healthStatus * healthStatus * healthStatus;
}

float GENERIC_ArmorStatus( Entity @self )
{
    float armorStatus = self.client.armor / float( G_GetItem( ARMOR_RA ).inventoryMax );

    if ( gametype.isInstagib )
        return 1.0f;

    if ( armorStatus > 1.0f )
        armorStatus = 1.0f;

    return armorStatus * armorStatus;
}

float GENERIC_OffensiveStatus( Entity @self )
{
    float weaponStatus = GENERIC_WeaponStatus( self );
    float healthStatus = GENERIC_HealthStatus( self );
    float armorStatus = GENERIC_ArmorStatus( self );
    float frac = ( ( weaponStatus + healthStatus + armorStatus ) / 3 );

    if ( gametype.isInstagib )
        return 0.75f;

    if ( GENERIC_BotInRageMode( self ) && frac > 0.5f )
        frac = 1.0f;

    // never let it be 0 or will skip important items
    if ( frac > 1.0f )
        frac = 1.0f;
    else if ( frac < 0.0f )
        frac = 0.0f;

    // keep it in a 0.25 / 0.75 ratio so it's never absolute defense nor absolute attack
    frac = 0.25f + ( frac * 0.5f );

    frac = ( frac * frac ) * 1.5f;

    //G_Print( "OFFENSIVESTATUS: " + frac + "\n" );

    return frac;
}

float GENERIC_PlayerWeight( Entity @self, Entity @enemy )
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

    weight = 0.5f;

    // don't fight against powerups.
    if ( ( enemy.effects & EF_QUAD ) != 0 || ( enemy.effects & EF_SHELL ) != 0 )
        weight *= 0.5f;

    // if enemy has EF_CARRIER we can assume it's someone important
    if ( ( enemy.effects & EF_CARRIER ) != 0 )
        weight *= 1.5f;

    return weight * self.client.getBot().offensiveness;
}

float GENERIC_WeaponWeight( Entity @self, Entity @goal )
{
    float thisWeight;

    if ( @goal.item == null )
        return 0.0f;

    if ( ( goal.item.type & IT_WEAPON ) == 0 )
        return 0.0f;

    if ( gametype.isInstagib )
        return 0.0f;

    if( !self.client.isBot() )
        return 0.0f;

    thisWeight = self.client.getBot().getItemWeight( goal.item );
	int weakAmmoMax = goal.item.weakAmmoTag != AMMO_NONE ? G_GetItem( goal.item.weakAmmoTag ).inventoryMax : 0;

    if ( self.client.inventoryCount( goal.item.tag ) > 0 && weakAmmoMax > 0 )
    {
		int weakAmmoCount = self.client.inventoryCount( goal.item.weakAmmoTag );
        if ( weakAmmoCount >= weakAmmoMax )
            thisWeight = 0;
        else if ( weakAmmoCount >= weakAmmoMax * 0.5 )
            thisWeight *= 0.33f;
        else if ( weakAmmoCount <= weakAmmoMax * 0.33 )
            thisWeight *= ( 2 * ( 1.0f - GENERIC_WeaponStatus( self ) ) );
    }
    else
    {
        thisWeight *= ( 2 * ( 1.0f - GENERIC_WeaponStatus( self ) ) );
    }

    return thisWeight;
}

float GENERIC_AmmoWeight( Entity @self, Entity @goal )
{
    float thisWeight;
    float lowNeed = 0.5f;

    if ( @goal.item == null )
        return 0.0f;

    if ( ( goal.item.type & IT_AMMO ) == 0 )
        return 0.0f;

    if ( gametype.isInstagib )
        return 0.0f;

    if( !self.client.isBot() )
        return 0.0f;

    thisWeight = self.client.getBot().getItemWeight( goal.item );

    if ( ( self.client.inventoryCount( goal.item.tag ) >= goal.item.inventoryMax )
            && goal.item.inventoryMax != 0 )
    {
        thisWeight = 0;
    }
    else
    {
        for ( int weapon = WEAP_GUNBLADE; weapon < WEAP_TOTAL; weapon++ )
        {
            Item @weaponItem = @G_GetItem( weapon );
            if ( weaponItem.ammoTag == goal.item.tag || weaponItem.weakAmmoTag == goal.item.tag )
            {
                float needFrac;

                if ( self.client.inventoryCount( weaponItem.tag ) == 0 )
                {
                    needFrac = 0.25f;
                }
                else
                {
                    if ( goal.item.inventoryMax > 0 )
                        needFrac = 1.0f - ( float(self.client.inventoryCount( goal.item.tag )) / float(goal.item.inventoryMax) );
                    else
                        needFrac = 0.33f;
                }

                if ( needFrac < 0.01f )
                    needFrac = 0;

                thisWeight *= needFrac;

                break;
            }
        }
    }

    return thisWeight;
}

float GENERIC_ArmorWeight( Entity @self, Entity @goal )
{
    float thisWeight;

    if ( @goal.item == null )
        return 0.0f;

    if ( ( goal.item.type & IT_ARMOR ) == 0 )
        return 0.0f;

    if ( gametype.isInstagib )
        return 0.0f;

    if( !self.client.isBot() )
        return 0.0f;

    thisWeight = self.client.getBot().getItemWeight( goal.item );

    if ( int(self.client.armor) + 1 < goal.item.inventoryMax || goal.item.inventoryMax == 0 )
    {
        thisWeight *= ( 2 * ( 1.0f - GENERIC_ArmorStatus( self ) ) );
        if ( self.client.armor < 25 && GENERIC_HealthStatus( self ) > 0.5f )
            thisWeight *= 1.5;
    }
    else
    {
        thisWeight = 0;
    }

    return thisWeight;
}

float GENERIC_HealthWeight( Entity @self, Entity @goal )
{
    float thisWeight;

    if ( @goal.item == null )
        return 0.0f;

    if ( ( goal.item.type & IT_HEALTH ) == 0 )
        return 0.0f;

    if ( gametype.isInstagib )
        return 0.0f;

    if( !self.client.isBot() )
        return 0.0f;

    thisWeight = self.client.getBot().getItemWeight( goal.item );

    thisWeight *= ( 3 * ( 1.0f - GENERIC_HealthStatus( self ) ) );
    if ( goal.item.tag == HEALTH_MEGA || goal.item.tag == HEALTH_ULTRA )
    {
        thisWeight += self.client.getBot().getItemWeight( goal.item );
    }

    return thisWeight;
}

// When this function is called the weights of items have been reset to their default values,
// this means, the weights *are set*, and what this function does is scaling them depending
// on the current bot status.
// Player, and non-item entities don't have any weight set. So they will be ignored by the bot
// unless a weight is assigned here.
bool GENERIC_UpdateBotStatus( Entity @self )
{
    Entity @goal;
    Bot @bot;

    @bot = @self.client.getBot();
    if ( @bot == null )
        return false;

    float offensiveStatus = GENERIC_OffensiveStatus( self );

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
            bot.setGoalWeight( i, GENERIC_PlayerWeight( self, goal ) * offensiveStatus );
            continue;
        }

        if ( @goal.item != null )
        {
            // all the following entities are items
            if ( ( goal.item.type & IT_WEAPON ) != 0 )
            {
                bot.setGoalWeight( i, GENERIC_WeaponWeight( self, goal ) );
            }
            else if ( ( goal.item.type & IT_AMMO ) != 0 )
            {
                bot.setGoalWeight( i, GENERIC_AmmoWeight( self, goal ) );
            }
            else if ( ( goal.item.type & IT_ARMOR ) != 0 )
            {
                bot.setGoalWeight( i, GENERIC_ArmorWeight( self, goal ) );
            }
            else if ( ( goal.item.type & IT_HEALTH ) != 0 )
            {
                bot.setGoalWeight( i, GENERIC_HealthWeight( self, goal ) );
            }
            else if ( ( goal.item.type & IT_POWERUP ) != 0 )
            {
                bot.setGoalWeight( i, bot.getItemWeight( goal.item ) * offensiveStatus );
            }

            continue;
        }

        // we don't know what entity is this, so ignore it
        bot.setGoalWeight( i, 0 );
    }

    return true; // handled by the script
}
