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

cReviver[] gtRevivers( maxClients );

Vec3 playerMins( -16, -16, -24 ), playerMaxs( 16, 16, 40 );

class cReviver
{
    bool inuse;

    Entity @ent;
    Entity @decal;
    int decalindex;
    int modelindex;
    int duration;
    cPlayer @player;
    bool revived;

    void Init()
    {
        // set up with default values
        this.inuse = false;
        this.revived = false;
        this.decalindex = G_ImageIndex( "gfx/indicators/radar_decal" );
        this.modelindex = G_ModelIndex( "models/objects/reviver/reviver.md3" );
        this.duration = CTFT_RESPAWN_TIME - 250;
        @this.player = null;
    }

    void Free()
    {
    	if( @this.player != null )
			@player.reviver = null;

        if ( @this.ent!= null )
        {
            this.ent.freeEntity();
            @this.ent = null;
        }

        if ( @this.decal!= null )
        {
            this.decal.freeEntity();
            @this.decal = null;
        }

        this.Init();
    }

    cReviver()
    {
        this.Init();
    }

    ~cReviver()
    {
        this.Free();
    }

    bool Spawn( cPlayer @player )
    {
        // we copy the position and size from the player, so we asume it's a sane position
        // at least by now...

        // so we only check for nodrop contents
        if ( ( G_PointContents( player.ent.origin ) & CONTENTS_NODROP ) != 0 )
            return false;

		Entity @ent = @G_SpawnEntity( "reviver" );
        @this.ent = @ent;

		@ent.stop = reviver_stop;
		@ent.think = reviver_think;
		@ent.die = reviver_die;
		@ent.touch = reviver_touch;
        ent.team = player.ent.team;
        ent.type = ET_GENERIC;
        ent.modelindex = this.modelindex;
        ent.effects = 0;
        ent.setSize( playerMins, playerMaxs );
        ent.solid = SOLID_TRIGGER;
        ent.moveType = MOVETYPE_TOSS;
        ent.clipMask = MASK_PLAYERSOLID;
        ent.mass = player.ent.mass;
        ent.svflags &= ~SVF_NOCLIENT;
        ent.svflags |= SVF_ONLYTEAM|SVF_BROADCAST; // set for only visible from teammates
        ent.takeDamage = DAMAGE_NO; // change if enemies can kill respawners
        ent.health = 200;

        // copy client position
        ent.origin = player.ent.origin;
        ent.angles = player.ent.angles;
        ent.velocity = player.ent.velocity;

        ent.nextThink = levelTime + duration;

        ent.linkEntity();

        // the count field will be used to store the index of the cTurret object
        // int the list. If the object is part of the list, ofc. This is just for
        // quickly accessing it.
        int index = -1;
        for ( int i = 0; i < maxClients; i++ )
        {
            if ( @gtRevivers[i] == @this )
            {
                index = i;
                break;
            }
        }

        ent.count = index;
        @this.player = @player;

        this.inuse = true;

        return true;
    }

    void Stop()
    {
    	// don't move nor receive knockback anymore
		this.ent.moveType = MOVETYPE_NONE;
		this.ent.linkEntity();

		if( @this.ent.groundEntity == null )
			return; // don't spawn the decal without a ground entity

    	// drop to floor
		Trace tr;
		tr.doTrace( this.ent.origin, vec3Origin, vec3Origin, this.ent.origin - Vec3( 0.0f, 0.0f, 128.0f ), 0, MASK_DEADSOLID );

		Entity @decal = @G_SpawnEntity( "reviver_decal" );
		@this.decal = @decal;
        decal.type = ET_DECAL;
        decal.solid = SOLID_NOT;
        decal.origin = tr.endPos + Vec3( 0.0f, 0.0f, 2.0f );
		decal.origin2 = Vec3( 0.0f, 0.0f, 1.0f );
        decal.modelindex = this.decalindex;
		decal.modelindex2 = 0; // rotation angle for ET_DECAL
        decal.team = this.ent.team;
        decal.frame = 96; // radius in case of ET_DECAL
        decal.svflags = this.ent.svflags | SVF_TRANSMITORIGIN2;
        decal.linkEntity();
    }

    void die( Entity @attacker )
    {
        this.Free();
    }

    void use( Client @activator )
    {
    	Client @cl;

    	if( this.revived == true )
			return;

    	if( @this.player == null )
			return;

		if( @activator == null )
			return;

		this.player.printMessage( S_COLOR_MAGENTA + "You have been revived by " + S_COLOR_WHITE + activator.name + S_COLOR_WHITE + "!\n" );
		activator.printMessage( S_COLOR_MAGENTA + "You revived " + S_COLOR_WHITE + this.player.client.name + S_COLOR_WHITE + "!\n" );

		for( int i = 0; i < maxClients; i++ )
		{
			@cl = @G_GetClient( i );
			if( cl.state() < CS_SPAWNED )
				continue;

			if( @cl == @this.player.client || @cl == @activator )
				continue;

			cl.printMessage( this.player.client.name + S_COLOR_MAGENTA + " was revived by " + S_COLOR_WHITE + activator.name + S_COLOR_WHITE + "!\n" );
		}

		G_Print( this.player.client.name + S_COLOR_MAGENTA + " was revived by " + S_COLOR_WHITE + activator.name + S_COLOR_WHITE + "!\n" );

		this.revived = true;
		this.player.client.respawn( false );
		activator.stats.addScore( 3 );
		this.Free(); // respawning should free it already, but doesn't hurt to do it twice.
    }
}

void reviver_stop( Entity @self )
{
	gtRevivers[self.count].Stop();
}

void reviver_think( Entity @self )
{
    gtRevivers[self.count].Free();
}

void reviver_die( Entity @self, Entity @inflictor, Entity @attacker )
{
	gtRevivers[self.count].die( attacker );
}

void reviver_touch( Entity @self, Entity @other, const Vec3 planeNormal, int surfFlags )
{
    if ( @other.client == null )
        return;

	if( self.team != other.team )
		return;

	if( GetPlayer( other.client ).playerClass.tag != PLAYERCLASS_MEDIC )
		return;

	if( self.count < 0 || self.count >= maxClients )
		return;

	gtRevivers[self.count].use( other.client );
}


