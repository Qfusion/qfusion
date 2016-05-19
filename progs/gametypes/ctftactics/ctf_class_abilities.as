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

void CTFT_DropHealth( Client @client )
{
    Item @item;
    Entity @dropped = null;

    @item = @G_GetItemByName( "25 Health" );

    if ( @item == null )
        return;

    if ( CTFT_TeamHasTooMany( client.team, "25 Health", 16 ) )
    {
        client.printMessage( "You have too many unclaimed health bubbles spawned.\n" );
        return;
    }

    if ( client.getEnt().health < 26 )
    {
        client.printMessage( "You don't have enough health to drop a " + item.name + "\n" );
    }
    else
    {
        @dropped = @client.getEnt().dropItem( item.tag );
        if ( @dropped == null )
        {
            client.printMessage( "Couldn't drop a " + item.name + "\n" );
        }
        else
        {
            client.getEnt().health -= float( item.quantity );

            // Change velocity a bit so it's easier to throw health bubbles at team mates
            Vec3 velocity;
            velocity = dropped.velocity;
            velocity.z = 0;
            velocity.normalize();
            velocity *= 700;
            velocity.z = 225;
            dropped.velocity = velocity;
        }

    }
}

void CTFT_DropArmor( Client @client, String armorType )
{
    Item @item;
    Entity @dropped = null;

    @item = @G_GetItemByName( armorType );

    if ( CTFT_TeamHasTooMany( client.team, armorType, 8 ) )
    {
        client.printMessage( "You have too many unclaimed armors spawned.\n" );
        return;
    }

    if ( @item == null )
        return;

    float maxArmor = G_GetItem( item.tag ).quantity;

    if ( client.armor < maxArmor )
    {
        client.printMessage( "You don't have enough armor to drop a " + item.name + "\n" );
    }
    else
    {
        @dropped = @client.getEnt().dropItem( item.tag );
        if ( @dropped == null )
        {
            client.printMessage( "Couldn't drop a " + item.name + "\n" );
        }
        else
        {
            client.armor -= float( item.quantity );

            // Change velocity a bit so it's easier to throw armor at team mates,
            // but not as much as health
            Vec3 velocity;
            velocity = dropped.velocity;
            velocity.z = 0;
            velocity.normalize();
            velocity *= 300;
            velocity.z = 225;
            dropped.velocity = velocity;
        }
    }
}

void CTFT_DropTurret( Client @client, int ammoType )
{
	if( @client == null )
		return;

    if ( client.getEnt().isGhosting() )
        return;

	cPlayer @player = @GetPlayer( client );

    if ( CTFT_IsMaxTurretsReached ( client.team ) )
    {
        client.printMessage( "Your team already has reached maximum turret count of " + CTFT_MAXTURRETS_PER_TEAM + " turrets\n" );
        return;
    }

    if ( player.isEngineerCooldown() )
    {
        client.printMessage( "You can't build yet\n" );
        return;
    }

	for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
	{
		if( flagBase.owner.origin.distance( client.getEnt().origin ) < CTFT_BUILD_RADIUS )
		{
			client.printMessage( "Too close to the flag, cannot build.\n" );
			return;
		}
	}

    if ( ammoType == AMMO_BULLETS )
    {
        if ( client.armor < CTFT_TURRET_AP_COST )
        {
            client.printMessage( "You don't have enough armor to spawn a turret\n" );
        }
        else
        {
            cTurret @turret = ClientDropTurret( client );
            if ( @turret != null )
            {
                turret.fireMode = AMMO_BULLETS;
                turret.refireDelay = 100;
                turret.damage = 10;
                turret.yawSpeed = 270.0f;
                turret.pitchSpeed = 170.0f;
                turret.gunOffset = 24;
				turret.knockback = 5;
                turret.bodyEnt.health = 300;
                @turret.client = @client;
                client.armor = client.armor - CTFT_TURRET_AP_COST;

                // have a delay before being able to build again
                player.setEngineerCooldown();
				
				client.stats.addScore( 2 );
            }
        }
    }

}

void CTFT_DropDispenser( Client @client )
{
	if( @client == null )
		return;

    if ( client.getEnt().isGhosting() )
        return;

    if ( CTFT_IsMaxDispensersReached ( client.team ) )
    {
        client.printMessage( "Your team already has reached maximum dispenser count of " + CTFT_MAXDISPENSERS_PER_TEAM + " dispensers\n" );
        return;
    }

    cPlayer @player = @GetPlayer( client );

    if ( player.isEngineerCooldown() )
    {
        client.printMessage( "You can't build yet\n" );
        return;
    }

	for ( cFlagBase @flagBase = @fbHead; @flagBase != null; @flagBase = @flagBase.next )
	{
		if( flagBase.owner.origin.distance( client.getEnt().origin ) < CTFT_BUILD_RADIUS )
		{
			client.printMessage( "Too close to the flag, cannot build.\n" );
			return;
		}
	}

    if ( client.armor < CTFT_TURRET_AP_COST )
    {
        client.printMessage( "You don't have enough armor to spawn an ammo dispenser\n" );
    }
    else
    {
        cDispenser @dispenser = ClientDropDispenser( client );
        if ( @dispenser != null )
        {
            client.armor = client.armor - CTFT_TURRET_AP_COST; // Costs the same as turret

			player.setEngineerCooldown();
			
			client.stats.addScore( 2 );
        }
    }
}

void CTFT_DetectTurretsDelayed ( Client @client ) 
{
	cPlayer @player = @GetPlayer( client );
	
	if ( player.detectEnabled == true )
		return;
		
    if ( client.armor < ( CTFT_RUNNER_DETECT_COST ) )
    {
        client.printMessage( "You don't have enough armor to detect turrets\n" );
		return;
    }		
	
	client.armor = client.armor - CTFT_RUNNER_DETECT_COST; 
	player.setDetectTime( );		
	client.printMessage( "Scanning for turrets...\n" );
} 	

void CTFT_DetectTurrets ( Client @client )
{	
	int detected = 0;
	
	for ( int i = 0; i < MAX_TURRETS; i++ )
	{
		if ( gtTurrets[i].inuse == true )
		{
			if( gtTurrets[i].bodyEnt.origin.distance( client.getEnt().origin ) < CTF_RUNNER_DETECT_DISTANCE )
			{
				if ( gtTurrets[i].detected == false && ( gtTurrets[i].bodyEnt.team != client.getEnt().team ) ) 
				{
					gtTurrets[i].detect();
					detected++;
				}
			}
		}
	}
	
	if ( detected > 0 )
		client.stats.addScore( detected );
	
	client.printMessage( "Detected " + detected + " turrets\n" );
}

void CTFT_SpawnRage ( Client @client, uint level )
{
    Item @item;
    if ( client.getEnt().isGhosting() )
        return;

	cPlayer @player = @GetPlayer( client );
	
	if ( player.rageEnabled == true )
		return;
	
	player.setRageTime( level );		

	@item = @G_GetItemByName( "Quad Damage" );
	client.inventoryGiveItem( item.tag );

	int spawnSoundIndex = G_SoundIndex( "sounds/items/quad_pickup" );
	G_Sound( client.getEnt(), CHAN_MUZZLEFLASH, spawnSoundIndex, 1.0f );
	
	Client @cl;
	
	for( int i = 0; i < maxClients; i++ )
	{
		@cl = @G_GetClient( i );
		if( cl.state() < CS_SPAWNED )
			continue;

		cl.printMessage( client.name + S_COLOR_GREEN + " gained " + S_COLOR_YELLOW + "Rage!\n" );
	}	
}

void CTFT_RemoveRage ( Client @client )
{
    Item @item;
    if ( client.getEnt().isGhosting() )
        return;

	@item = @G_GetItemByName( "Quad Damage" );
	client.inventorySetCount( item.tag, 0 );
}

// KIKI WAnts BIG BOOM!!
void CTFT_DropBomb( Client @client )
{
	if( @client == null )
		return;

    if ( client.getEnt().isGhosting() )
        return;

    cPlayer @player = @GetPlayer( client );

	if( @player.bomb != null )
	{
		if ( player.bomb.explodeTime > 0 )
			return;
			
		player.bomb.setExplode();
		return;
	}

    if ( player.isBombCooldown() )
    {
        client.printMessage( "You can't throw a bomb yet\n" );
        return;
    }

    if ( client.armor < ( CTFT_TURRET_AP_COST ) )
    {
        client.printMessage( "You don't have enough armor to spawn a bomb\n" );
    }
    else
    {
        cBomb @bomb = ClientDropBomb( client );
        if ( @bomb != null )
        {
            client.armor = client.armor - ( CTFT_TURRET_AP_COST ); // Costs the same as turret
			player.setBombCooldown();
			@player.bomb = bomb;
        }
    }
}
