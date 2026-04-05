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

const int MAX_DISPENSERS = 8;

cDispenser[] gtDispensers( MAX_DISPENSERS );

Vec3 dispenserMins( -16, -16, -16 ), dispenserMaxs( 24, 24, 16 );

int64[] dispenserCooldownTimes( maxClients );

class cDispenser
{
    bool inuse;

    Entity @bodyEnt;

    void Init()
    {
        // set up with default values
        this.inuse = false;
    }

    cDispenser()
    {
        this.Init();
    }

    ~cDispenser()
    {
        this.Free();
    }

    void Free()
    {
        if ( @this.bodyEnt!= null )
        {
            this.bodyEnt.freeEntity();
            @this.bodyEnt = null;
        }

        this.Init();
    }

    bool Spawn( Vec3 origin, int team )
    {
        // try to position the dispenser in the world.
        Trace tr;

        // check the initial position is not inside solid

        if ( tr.doTrace( origin, dispenserMins , dispenserMaxs, origin, -1, MASK_PLAYERSOLID ) )
            return false;

        if ( tr.startSolid || tr.allSolid )
            return false; // initial position is inside solid, we can not spawn the dispenser

        // proceed setting up
        this.Init();

        Vec3 partOrigin = origin;

        // the body entity will be used for collision. Will be the only solid entity of
        // the three, and will have physic properties. It will not rotate.
        @this.bodyEnt = @G_SpawnEntity( "dispenser_body" );
		@this.bodyEnt.touch = dispenser_body_touch;
		@this.bodyEnt.die = dispenser_body_die;
		@this.bodyEnt.think = dispenser_body_think;
        this.bodyEnt.type = ET_GENERIC;
        this.bodyEnt.modelindex = G_ModelIndex( "models/objects/misc/ammobox.md3", true );
        this.bodyEnt.setSize( dispenserMins, dispenserMaxs );
        this.bodyEnt.team = team;
        this.bodyEnt.origin = partOrigin;
        this.bodyEnt.solid = SOLID_YES;
        this.bodyEnt.clipMask = MASK_PLAYERSOLID;
        this.bodyEnt.moveType = MOVETYPE_TOSS;
        this.bodyEnt.svflags &= ~SVF_NOCLIENT;
        this.bodyEnt.health = 400;
        this.bodyEnt.mass = 450;
        this.bodyEnt.takeDamage = DAMAGE_YES;
        this.bodyEnt.nextThink = levelTime + 1000;
        this.bodyEnt.linkEntity();

        // the count field will be used to store the index of the cdispenser object
        // int the list. If the object is part of the list, ofc. This is just for
        // quickly accessing it.
        int index = -1;
        for ( int i = 0; i < MAX_DISPENSERS; i++ )
        {
            if ( @gtDispensers[i] == @this )
            {
                index = i;
                break;
            }
        }

        this.bodyEnt.count = index;
        this.inuse = true;
        return true; // dispenser has been correctly spawned
    }

    void die( Entity @inflictor, Entity @attacker )
    {
        if ( !this.inuse )
            return;

        if ( @this.bodyEnt != null && this.bodyEnt.inuse )
        {
            this.bodyEnt.solid = SOLID_NOT;
            this.bodyEnt.linkEntity();
            this.bodyEnt.explosionEffect( 500 );
            this.bodyEnt.splashDamage( this.bodyEnt, 200, 100, 70, 0, MOD_EXPLOSIVE );
        }

        this.Free();
    }

}

void dispenser_body_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
    if ( @other == null )
        return;

    if ( @other.client == null )
        return;

    if ( ent.team != other.team )
        return;

    if ( CTFT_isDispenserCooldown ( other ) == true )
    {
        return;
    }

    CTFT_setDispenserCooldown( other );

    int spawnSoundIndex = G_SoundIndex( "sounds/items/weapon_pickup" );
    G_Sound( ent, CHAN_AUTO, spawnSoundIndex, 0.4f );

    // Reload ammo
    cPlayer @player = @GetPlayer( other.client );

    // Runner
    if ( player.playerClass.tag == PLAYERCLASS_RUNNER )
    {
        other.client.inventoryGiveItem( WEAP_RIOTGUN );
        other.client.inventoryGiveItem( WEAP_LASERGUN );
        other.client.inventoryGiveItem( WEAP_MACHINEGUN );

        other.client.inventoryGiveItem( AMMO_SHELLS );
    }
    // Medic
    else if ( player.playerClass.tag == PLAYERCLASS_MEDIC )
    {
        other.client.inventoryGiveItem( WEAP_PLASMAGUN );
        other.client.inventoryGiveItem( WEAP_ELECTROBOLT );
        other.client.inventoryGiveItem( WEAP_LASERGUN );
        other.client.inventoryGiveItem( WEAP_MACHINEGUN );

        other.client.inventoryGiveItem( AMMO_LASERS );
    }
    // Grunt
    else if ( player.playerClass.tag == PLAYERCLASS_GRUNT )
    {
        other.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
        other.client.inventoryGiveItem( WEAP_PLASMAGUN );
        other.client.inventoryGiveItem( WEAP_GRENADELAUNCHER );

        other.client.inventoryGiveItem( AMMO_ROCKETS );
        other.client.inventoryGiveItem( AMMO_PLASMA );
    }
    // Engineer
    else if ( player.playerClass.tag == PLAYERCLASS_ENGINEER )
    {
        other.client.inventoryGiveItem( WEAP_ROCKETLAUNCHER );
        other.client.inventoryGiveItem( WEAP_RIOTGUN );
        other.client.inventoryGiveItem( WEAP_ELECTROBOLT );

        other.client.inventoryGiveItem( AMMO_BOLTS );
    }
	
	// Give 25 health too
	if ( other.client.getEnt().health <= 75 ) 
	{
		other.client.getEnt().health = other.client.getEnt().health + 25;
	}
	else if ( other.client.getEnt().health > 75 && other.client.getEnt().health < 100 )
	{
		other.client.getEnt().health = 100;
	}
}

void dispenser_body_die( Entity @self, Entity @inflictor, Entity @attacker )
{
    if ( self.count >= 0 && self.count < MAX_DISPENSERS )
        gtDispensers[self.count].die( inflictor, attacker );
}

void dispenser_body_think( Entity @self )
{
    // if for some reason the dispenser moved to inside a solid, kill it
    if ( ( G_PointContents( self.origin ) & (CONTENTS_SOLID|CONTENTS_NODROP) ) != 0 )
    {
    	dispenser_body_die( self, @G_GetEntity(0), @G_GetEntity(0) );
    	return;
    }

    self.nextThink = levelTime + 25; // no need to check this every frame
}

cDispenser @ClientDropDispenser( Client @client )
{
    if ( @client == null )
        return null;

    cDispenser @dispenser = null;

    // find an unused dispenser slot
    for ( int i = 0; i < MAX_DISPENSERS; i++ )
    {
        if ( gtDispensers[i].inuse == false )
        {
            @dispenser = @gtDispensers[i];
            break;
        }
    }

    if ( @dispenser == null )
    {
        G_Print( "GT: ClientDropdispenser: MAX_DISPENSERS reached. Can't spawn dispenser.\n" );
        return null;
    }

    // nodrop area
    if ( ( G_PointContents( client.getEnt().origin ) & CONTENTS_NODROP ) != 0 )
        return null;

    // first check that there's space for spawning the dispenser in front of us
    Vec3 dir, start, end, r, u;

    client.getEnt().angles.angleVectors( dir, r, u );
    start = client.getEnt().origin;
    start.z += 6;
    end = ( start + ( 0.5 * ( dispenserMaxs + dispenserMins) ) ) + ( dir * 64 );

    Trace tr;

    tr.doTrace( start, dispenserMins, dispenserMaxs, end, client.getEnt().entNum, MASK_PLAYERSOLID );

    // try spawning the dispenser
    if ( !dispenser.Spawn( tr.endPos, client.getEnt().team ) ) // can't spawn dispenser in that position. Blocked by something
        return null;

    // assign some frontal velocity to the dispenser, as for being dropped by the player
    float speed = client.getEnt().velocity.length();
    dir *= speed + 40;
    dir.z = 50;
    dispenser.bodyEnt.velocity = dir;
    dispenser.bodyEnt.linkEntity();

    return @dispenser;
}

void CTFT_setDispenserCooldown( Entity @target )
{
    if ( @target.client == null )
        return;

    int player = target.client.playerNum;

    dispenserCooldownTimes[ player ] = levelTime + CTFT_DISPENSER_COOLDOWN;
}

bool CTFT_isDispenserCooldown( Entity @target )
{
    if ( @target.client == null )
        return false;

    int player = target.client.playerNum;

    if ( dispenserCooldownTimes[ player ] == 0 )
        return false;

    if ( dispenserCooldownTimes[ player ] > levelTime )
        return true;
    else
        return false;
}
