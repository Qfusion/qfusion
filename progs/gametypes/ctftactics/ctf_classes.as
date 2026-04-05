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
const int PLAYERCLASS_MEDIC = 1;
const int PLAYERCLASS_RUNNER = 2;
const int PLAYERCLASS_ENGINEER = 3;

const int PLAYERCLASS_TOTAL = 4;

int[] playerClasses( maxClients ); // class of each player

// definition of the classes
class cPlayerClass
{
    int tag;
    int maxHealth;
    int maxSpeed;
    int dashSpeed;
    int jumpSpeed;
    bool takeStun;
    int iconIndex;
    bool initialized;
    String name;
    String playerModel;
    int action1IconIndex;
    int action2IconIndex;

    cPlayerClass()
    {
        this.maxSpeed = -1;
        this.dashSpeed = -1;
        this.jumpSpeed = -1;
        this.maxHealth = 100;
        this.takeStun = true;
        this.initialized = false;
        this.iconIndex = 0;
        this.tag = 0;
        this.action1IconIndex = 0;
        this.action2IconIndex = 0;
    }

    ~cPlayerClass() {}

    void setup( String &class_name, int tag, String &model, int health, int armor, int maxSpeed, int dashSpeed, bool stun,
        const String &icon, const String @action1Icon, const String @action2Icon )
    {
        this.name = class_name;
        this.playerModel = model;
        this.maxHealth = health;
        this.dashSpeed = dashSpeed;
        this.maxSpeed = maxSpeed;
        this.takeStun = stun;

        if ( tag < 0 || tag >= PLAYERCLASS_TOTAL )
            G_Print( "WARNING: cPlayerClass::setup with a invalid tag " + tag + "\n" );
        else
            this.tag = tag;

        // precache
        G_ModelIndex( this.playerModel, true );
        this.iconIndex = G_ImageIndex( icon );
        if( @action1Icon != null )
            this.action1IconIndex = G_ImageIndex( action1Icon );
        if( @action2Icon != null )
            this.action2IconIndex = G_ImageIndex( action2Icon );

        this.initialized = true;
    }
}

cPlayerClass[] cPlayerClassInfos( PLAYERCLASS_TOTAL );

// Initialize player classes

void GENERIC_InitPlayerClasses()
{
    // precache the runner invisibility skin
    G_SkinIndex( "models/players/silverclaw/invisibility.skin" );

    for ( int i = 0; i < maxClients; i++ )
        playerClasses[ i ] = PLAYERCLASS_GRUNT;

    cPlayerClassInfos[ PLAYERCLASS_GRUNT ].setup(
        "Grunt",					// name
        PLAYERCLASS_GRUNT,
        "$models/players/bigvic",			// player model
        100,						// initial health
        0,						// initial armor
        250,						// speed
        350,						// dash speed
        true,						// can be stunned
        "gfx/hud/icons/playerclass/grunt",
        "gfx/hud/icons/classactions/grunt1",
        "gfx/hud/icons/classactions/grunt2"
    );

    cPlayerClassInfos[ PLAYERCLASS_MEDIC ].setup(
        "Medic",					// name
        PLAYERCLASS_MEDIC,
        "$models/players/monada",			// player model
        100,						// initial health
        0,						// initial armor
        300,						// speed
        400,						// dash speed
        true,						// can be stunned
        "gfx/hud/icons/playerclass/medic",
        "gfx/hud/icons/classactions/medic1",
        null
    );

    cPlayerClassInfos[ PLAYERCLASS_RUNNER ].setup(
        "Runner",					// name
        PLAYERCLASS_RUNNER,
        "$models/players/silverclaw",			// player model
        100,						// initial health
        0,						// initial armor
        350,						// speed
        450,						// dash speed
        false,						// can be stunned
        "gfx/hud/icons/playerclass/runner",
        "gfx/hud/icons/classactions/runner1",
        "gfx/hud/icons/classactions/runner2"
    );

    cPlayerClassInfos[ PLAYERCLASS_ENGINEER ].setup(
        "Engineer",					// name
        PLAYERCLASS_ENGINEER,
        "$models/players/bobot",			// player model
        100,						// initial health
        0,						// initial armor
        300,						// speed
        350,						// dash speed
        true,						// can be stunned
        "gfx/hud/icons/playerclass/engi",
        "gfx/hud/icons/classactions/engineer1",
        "gfx/hud/icons/classactions/engineer2"
    );
}


