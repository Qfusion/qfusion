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

void GENERIC_SpawnItem( Entity @ent, int tag )
{
	Item @item;

	@item = @G_GetItem( tag );

    ent.type = ET_ITEM;
	@ent.item = @item;
    ent.effects = 0;
}

bool GENERIC_ClientCanDropItem( Client @client )
{
    if( client.team <= TEAM_SPECTATOR || client.team >= GS_MAX_TEAMS )
        return false;
    if( client.chaseActive )
        return false;
    return true;
}

Entity @GENERIC_CommandDropItem( Client @client, String @argsString )
{
    Item @item;
    Entity @dropped = null;

    if( !GENERIC_ClientCanDropItem( client ) )
        return null;

    @item = @G_GetItemByName( argsString );
    if ( @item == null )
        return null;

    if ( item.isDropable() == false )
    {
        client.printMessage( "Item " + item.name + " is not droppable\n" );
        return null;
    }

    int count = client.inventoryCount( item.tag );

    if ( ( item.type & IT_HEALTH ) != 0 )
    {
        if ( client.getEnt().health < item.quantity + 1 )
        {
            client.printMessage( "You don't have enough health to drop a " + item.name + "\n" );
        }
        else
        {
            @dropped = @client.getEnt().dropItem( item.tag );
            if ( @dropped == null )
                client.printMessage( "Couldn't drop a " + item.name + "\n" );
            else
                client.getEnt().health -= float( item.quantity );
        }
    }
    else if ( ( item.type & IT_ARMOR ) != 0 )
    {
        if ( client.armor < item.quantity )
        {
            client.printMessage( "You don't have enough armor to drop a " + item.name + "\n" );
        }
        else
        {
            @dropped = @client.getEnt().dropItem( item.tag );
            if ( @dropped == null )
                client.printMessage( "Couldn't drop a " + item.name + "\n" );
            else
                client.armor -= float( item.quantity );
        }
    }
    else if ( ( item.type & IT_POWERUP ) != 0 )
    {
        if ( count < 4 )
        {
            client.printMessage( "Not enough powerup time left to drop the " + item.name + "\n" );
        }
        else
        {
            @dropped = @client.getEnt().dropItem( item.tag );
            if ( @dropped == null )
                client.printMessage( "Couldn't drop a " + item.name + "\n" );
            else
                client.inventorySetCount( item.tag, 0 );
        }
    }
    else if ( ( item.type & IT_AMMO ) != 0 )
    {
        if ( count < item.quantity )
        {
            client.printMessage( "You don't have enough " + item.name + " to drop\n" );
        }
        else
        {
            @dropped = @client.getEnt().dropItem( item.tag );
            if ( @dropped == null )
                client.printMessage( "Couldn't drop a " + item.name + "\n" );
            else
            {
                count -= item.quantity;
                client.inventorySetCount( item.tag, count );
            }
        }
    }
    else if ( ( item.type & IT_WEAPON ) != 0 )
    {
        if ( count < 1 )
        {
            client.printMessage( "You don't have a " + item.name + " to drop\n" );
        }
        else
        {
            @dropped = GENERIC_DropWeapon( client, item, true );
        }
    }
    else if ( count < 1 )
    {
        client.printMessage( "You don't have enough " + item.name + " to drop\n" );
    }
    else
    {
        @dropped = @client.getEnt().dropItem( item.tag );
        if ( @dropped == null )
            client.printMessage( "Couldn't drop a " + item.name + "\n" );
        else
        {
            count--;
            client.inventorySetCount( item.tag, count );
        }
    }

    if( client.inventoryCount( client.weapon ) == 0 )
		client.selectWeapon( -1 );

    return dropped;
}

Entity @GENERIC_DropCurrentAmmoStrong( Client @client )
{
    Item @item;
    Entity @dropped = null;

    if( !GENERIC_ClientCanDropItem( client ) )
        return null;

    @item = @G_GetItem( client.weapon );
    if ( @item == null )
        return null;

    Item @ammoItem = @G_GetItem( item.ammoTag );
    if ( @ammoItem == null )
        return null;

	if ( ammoItem.isDropable() == false )
    {
        client.printMessage( "Item " + item.name + " is not droppable\n" );
        return null;
    }

    int count = client.inventoryCount( ammoItem.tag );

    if ( count >= ammoItem.quantity )
    {
        @dropped = @client.getEnt().dropItem( ammoItem.tag );
        if ( @dropped != null )
        {
            dropped.count = ammoItem.quantity;
            count -= ammoItem.quantity;
            client.inventorySetCount( ammoItem.tag, count );
        }
    }

    return dropped;
}

Entity @GENERIC_DropWeapon( Client @client, Item @item, bool addAmmo )
{
    Entity @dropped;

    if( !GENERIC_ClientCanDropItem( client ) )
        return null;

    if ( @item == null )
        return null;

	if ( item.isDropable() == false )
    {
        client.printMessage( item.name + " is not droppable\n" );
        return null;
    }

    int count = client.inventoryCount( item.tag );
    if ( count <= 0 )
        return null;

    @dropped = @client.getEnt().dropItem( item.tag );
    if ( @dropped != null )
    {
        count--;
        client.inventorySetCount( item.tag, count );

        if( addAmmo )
        {
            Item @ammoItem = @G_GetItem( item.ammoTag );
            if( @ammoItem != null && ammoItem.inventoryMax > 0 )
            {
                dropped.count = client.inventoryCount( ammoItem.tag );
                client.inventorySetCount( ammoItem.tag, 0 );
            }
        }

        if( client.inventoryCount( client.weapon ) == 0 )
			client.selectWeapon( -1 );
    }

    return dropped;
}

Entity @GENERIC_DropCurrentWeapon( Client @client, bool addAmmo )
{
    return GENERIC_DropWeapon( client, @G_GetItem( client.weapon ), addAmmo );
}
