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

const int PLAYERCLASS_GRUNT = 0;
const int PLAYERCLASS_CAMPER = 1;
const int PLAYERCLASS_SPAMMER = 2;

const int PLAYERCLASS_TOTAL = 3;

Cvar g_noclass_inventory( "g_noclass_inventory", "gb mg rg gl rl pg lg eb cells shells grens rockets plasma lasers bullets", 0 );
Cvar g_class_grunt_inventory( "g_class_grunt_inventory", "gb mg rg gl rl pg lg eb cells shells rockets", 0 );
Cvar g_class_camper_inventory( "g_class_camper_inventory", "gb mg rg gl rl pg lg eb cells grens bolts", 0 );
Cvar g_class_spammer_inventory( "g_class_spammer_inventory", "gb mg rg gl rl pg lg eb cells lasers plasma", 0 );
Cvar g_class_grunt_health( "g_class_grunt_health", "100", 0 );
Cvar g_class_camper_health( "g_class_camper_health", "100", 0 );
Cvar g_class_spammer_health( "g_class_spammer_health", "100", 0 );
Cvar g_class_grunt_armor( "g_class_grunt_armor", "150", 0 );
Cvar g_class_camper_armor( "g_class_camper_armor", "150", 0 );
Cvar g_class_spammer_armor( "g_class_spammer_armor", "150", 0 );

Cvar g_class_weak_ammo( "g_class_weak_ammo", "0 100 15 15 50 75 120 10", 0 ); // gb mg rg gl rl pg lg eb
Cvar g_class_strong_ammo( "g_class_strong_ammo", "1 0 15 20 25 75 100 10", 0 ); // GB MG RG GL RL PG LG EB

int[] playerClasses( maxClients ); // class of each player

// definition of the classes
class cPlayerClass
{
    int maxHealth;
    int maxArmor;
    int prcIcon;
    bool initialized;
    String name;
    String itemList;
    String weakAmmoCounts;
    String ammoCounts;

    cPlayerClass()
    {
        this.maxHealth = 100;
        this.maxArmor = 200;
        this.initialized = false;
        this.prcIcon = 0;
    }

    ~cPlayerClass() {}

    void setup( String &class_name, String &list, int health, int armor, String &icon )
    {
        this.name = class_name;
        this.itemList = list;
        this.weakAmmoCounts = g_class_weak_ammo.string;
        this.ammoCounts = g_class_strong_ammo.string;

        this.maxHealth = health;
        this.maxArmor = armor;

        this.prcIcon = G_ImageIndex( icon );

        // warn of unkown items in the list
        String token;
        Item @item;

        for ( int i = 0; ;i++ )
        {
            token = this.itemList.getToken( i );
            if ( token.len() == 0 )
                break; // done

            @item = @G_GetItemByName( token );
            if ( @item == null )
            {
                G_Print( "WARNING: cPlayerClass::giveInventory found an unkown item shortname '" + token + "' in the playerclass inventory list\n" );
                continue;
            }
        }

        this.initialized = true;
    }

    String @getName()
    {
        if ( !this.initialized )
        {
            G_Print( "WARNING: PlayerClass has not being setup before calling cPlayerClass::getName\n" );
            return null;
        }

        return this.name;
    }

    bool includesItem( Item @item )
    {
        if ( @item == null )
            return false;

        if ( !this.initialized )
        {
            G_Print( "WARNING: PlayerClass has not being setup before calling cPlayerClass::includesItem\n" );
            return false;
        }

        String token;

        for ( int i = 0; ; i++ )
        {
            token = this.itemList.getToken( i );

            if ( token.len() == 0 )
                break;

            if ( token == item.shortName )
                return true;
        }

        return false;
    }

    void giveHealth( Client @client )
    {
        if ( @client == null )
            return;

        if ( !this.initialized )
        {
            G_Print( "WARNING: PlayerClass has not being setup before calling cPlayerClass::giveHealth\n" );
            return;
        }

        Entity @ent= @client.getEnt();

        ent.health = this.maxHealth;
        ent.maxHealth = this.maxHealth;
    }

    void giveArmor( Client @client )
    {
        if ( @client == null )
            return;

        if ( !this.initialized )
        {
            G_Print( "WARNING: PlayerClass has not being setup before calling cPlayerClass::giveArmor\n" );
            return;
        }

        client.armor = float( this.maxArmor );
    }

    void giveInventory( Client @client )
    {
        if ( @client == null )
            return;

        if ( !this.initialized )
        {
            G_Print( "WARNING: PlayerClass has not being setup before calling cPlayerClass::giveInventory\n" );
            return;
        }

        client.inventoryClear();

        if ( gametype.isInstagib )
            return;

        String token, weakammotoken, ammotoken;
        Item @item;

        for ( int i = 0; ;i++ )
        {
            token = this.itemList.getToken( i );
            if ( token.len() == 0 )
                return; // done

            @item = @G_GetItemByName( token );
            if ( @item == null )
                continue;

            client.inventoryGiveItem( item.tag );

            // if it's a weapon, set the weak ammo count as defined in the cvar
            if ( ( item.type & IT_WEAPON ) != 0 )
            {
                Item @ammoItem = item.weakAmmoTag == AMMO_NONE ? null : @G_GetItem( item.weakAmmoTag );
                if ( @ammoItem != null )
                {
                    int pos;

                    pos = ammoItem.tag - AMMO_WEAK_GUNBLADE;
                    token = this.weakAmmoCounts.getToken( pos );
                    if ( token.len() > 0 )
                    {
                        client.inventorySetCount( ammoItem.tag, token.toInt() );
                    }
                }
            }

            // if it's ammo, set the ammo count as defined in the cvar
            if ( ( item.type & IT_AMMO ) != 0 )
            {
                if ( item.tag < AMMO_GUNBLADE )
                    token = this.weakAmmoCounts.getToken( item.tag - AMMO_WEAK_GUNBLADE );
                else
                    token = this.ammoCounts.getToken( item.tag - AMMO_GUNBLADE );

                if ( token.len() > 0 )
                {
                    client.inventorySetCount( item.tag, token.toInt() );
                }
            }
        }
    }
}

cPlayerClass[] cPlayerClassInfos( PLAYERCLASS_TOTAL );

// Initialize player classes

void GENERIC_InitPlayerClasses()
{
    for ( int i = 0; i < maxClients; i++ )
        playerClasses[ i ] = PLAYERCLASS_GRUNT;

    cPlayerClassInfos[ PLAYERCLASS_GRUNT ].setup( "Grunt",
            g_class_grunt_inventory.string,
            g_class_grunt_health.integer,
            g_class_grunt_armor.integer,
            "gfx/hud/icons/weapon/rocket" );

    cPlayerClassInfos[ PLAYERCLASS_CAMPER ].setup( "Camper",
            g_class_camper_inventory.string,
            g_class_camper_health.integer,
            g_class_camper_armor.integer,
            "gfx/hud/icons/weapon/electro" );

    cPlayerClassInfos[ PLAYERCLASS_SPAMMER ].setup( "Spammer",
            g_class_spammer_inventory.string,
            g_class_spammer_health.integer,
            g_class_spammer_armor.integer,
            "gfx/hud/icons/weapon/laser" );
}

int GENERIC_GetClientClass( Client @client )
{
    if ( @client == null || client.playerNum < 0 || client.playerNum >= maxClients )
        return PLAYERCLASS_GRUNT;

    if ( gametype.isInstagib )
        return 0;

    return playerClasses[ client.playerNum ];
}

cPlayerClass @GENERIC_GetClientClassInfo( Client @client )
{
    return @cPlayerClassInfos[ GENERIC_GetClientClass( client ) ];
}

int GENERIC_GetClientClassIcon( Client @client )
{
    if ( gametype.isInstagib )
        return 0;

    return cPlayerClassInfos[ GENERIC_GetClientClass( client ) ].prcIcon;
}

void GENERIC_SetClientClassIndex( Client @client, int classIndex )
{
    if ( @client == null || client.playerNum < 0 || client.playerNum >= maxClients )
        return;

    if ( classIndex < PLAYERCLASS_GRUNT || classIndex >= PLAYERCLASS_TOTAL )
        classIndex = PLAYERCLASS_GRUNT;

    playerClasses[ client.playerNum ] = classIndex;
}

bool GENERIC_SetClientClass( Client @client, String @className )
{
    if ( @client == null || client.playerNum < 0 || client.playerNum >= maxClients )
        return false;

    if ( @className == null )
        return false;

    for ( int i = 0; i < PLAYERCLASS_TOTAL; i++ )
    {
        if ( cPlayerClassInfos[i].name == className )
        {
            playerClasses[ client.playerNum ] = i;
            return true;
        }
    }

    return false;
}

void GENERIC_GiveClientClassInventory( Client @client, bool giveItems, bool giveHealth, bool giveArmor )
{
    int c = GENERIC_GetClientClass( client );

    if ( giveItems )
        cPlayerClassInfos[ c ].giveInventory( client );

    if ( giveHealth )
        cPlayerClassInfos[ c ].giveHealth( client );

    if ( giveArmor )
        cPlayerClassInfos[ c ].giveArmor( client );
}

void GENERIC_PlayerclassCommand( Client @client, String &argsString )
{
    String token = argsString.getToken( 0 );

    if ( token.len() == 0 )
    {
        G_PrintMsg( client.getEnt(), "Usage: class <name>\n" );
        return;
    }

    if ( client.getEnt().team < TEAM_PLAYERS )
    {
        G_PrintMsg( client.getEnt(), "You must join a team before selecting a class\n" );
        return;
    }

    if ( GENERIC_SetClientClass( client, token ) == false )
    {
        G_PrintMsg( client.getEnt(), "Unknown playerClass '" + token + "'\n" );
        return;
    }

    if ( match.getState() < MATCH_STATE_COUNTDOWN )
        client.respawn( false );
    else
        G_PrintMsg( client.getEnt(), "You will respawn as " + token + "\n" );
}

