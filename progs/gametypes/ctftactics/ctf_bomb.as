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

const int MAX_BOMBS = 16;
const uint EXPLODE_DELAY = 300;

cBomb[] gtBombs( MAX_BOMBS );

Vec3 bombMins( -8, -8, -8 ), bombMaxs( 8, 8, 8 );

class cBomb
{
    bool inuse;
	int64 explodeTime;
	int bombAlarmSoundIndex;

    Entity @bombEnt;
    cPlayer @player;

    void Init()
    {
        // set up with default values
        this.inuse = false;
        @this.player = null;
		this.explodeTime = 0;
		this.bombAlarmSoundIndex = G_SoundIndex( "sounds/misc/timer_bip_bip" );
    }

    cBomb()
    {
        this.Init();
    }

    ~cBomb()
    {
        this.Free();
    }

    void Free()
    {
        if ( @this.bombEnt!= null )
        {
            this.bombEnt.freeEntity();
            @this.bombEnt = null;
        }

        if ( @this.player != null && @this.player.bomb != null )
            @this.player.bomb = null;

        this.Init();
    }

    bool Spawn( Vec3 origin, Client @client )
    {
        if ( @client == null )
            return false;

        // try to position the bomb in the world.
        Trace tr;

        // check the initial position is not inside solid

        if ( tr.doTrace( origin, bombMins , bombMaxs, origin, -1, MASK_PLAYERSOLID ) )
            return false;

        if ( tr.startSolid || tr.allSolid )
            return false; // initial position is inside solid, we can not spawn the bomb

        // proceed setting up
        this.Init();

        @this.player = @GetPlayer( client );

        // the body entity will be used for collision. Will be the only solid entity of
        // the three, and will have physic properties. It will not rotate.
        @this.bombEnt = @G_SpawnEntity( "bomb_body" );
		@this.bombEnt.touch = bomb_body_touch;
		@this.bombEnt.die = bomb_body_die;
		@this.bombEnt.think = bomb_body_think;
        this.bombEnt.type = ET_GENERIC;
        this.bombEnt.modelindex = G_ModelIndex( "models/objects/projectile/glauncher/grenadestrong.md3", false );
        this.bombEnt.setSize( bombMins, bombMaxs );
        this.bombEnt.team = this.player.ent.team;
        this.bombEnt.ownerNum = this.player.client.playerNum;
        this.bombEnt.origin = origin;
        this.bombEnt.solid = SOLID_TRIGGER;
        this.bombEnt.clipMask = MASK_PLAYERSOLID;
        this.bombEnt.moveType = MOVETYPE_TOSS;
        this.bombEnt.svflags &= ~SVF_NOCLIENT;
        this.bombEnt.health = 150;
        this.bombEnt.mass = 250;
        this.bombEnt.takeDamage = 0;
        this.bombEnt.nextThink = levelTime + 1000;
        this.bombEnt.linkEntity();

        // the count field will be used to store the index of the cbomb object
        // int the list. If the object is part of the list, ofc. This is just for
        // quickly accessing it.
        int index = -1;
        for ( int i = 0; i < MAX_BOMBS; i++ )
        {
            if ( @gtBombs[i] == @this )
            {
                index = i;
                break;
            }
        }

        @this.player = @player;
        this.bombEnt.count = index;
        this.inuse = true;

        return true; // bomb has been correctly spawned
    }

	void setExplode()
	{
		G_Sound( this.bombEnt, CHAN_VOICE, this.bombAlarmSoundIndex, 0.25f );
		this.explodeTime = levelTime + EXPLODE_DELAY;
	}

    void die( Entity @inflictor, Entity @attacker )
    {
        if ( !this.inuse )
            return;

        if ( @this.bombEnt == null || !this.bombEnt.inuse )
        {
            this.Free();
            return;
        }

        Vec3 bombOrigin = this.bombEnt.origin;
        cPlayer @player = @this.player;

        this.bombEnt.solid = SOLID_NOT;
        this.bombEnt.linkEntity();
        this.bombEnt.explosionEffect( 500 );
        this.bombEnt.splashDamage( this.player.ent, 225, 180, 125, 0, MOD_EXPLOSIVE );

        for (int i = 0; i < 6; i++)
        {
            Vec3 dir;
            dir.set(-1 + random()*2, -1 + random()*2, random());
            dir = dir.toAngles();
            Entity @nade = @G_FireGrenade( bombOrigin, dir, 350, 225, 70, 100, 1, player.client.getEnt() );
            if ( @nade != null )
            {
                nade.modelindex = G_ModelIndex( "models/objects/projectile/glauncher/grenadeweak.md3", false );
                // make nades explode after 1sec
                nade.nextThink = levelTime + 1000;
            }
        }

        this.Free();
    }
}

void bomb_body_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
    if ( @other == null )
        return;

    if ( other.classname == "worldspawn" )
    {
        // Make it sticky =P
        ent.moveType = MOVETYPE_NONE;
        ent.linkEntity();
    }

    // Pick up the bomb if owner is touching
    if ( @other.client != null )
    {
        cPlayer @player = @GetPlayer( other.client );

        if ( @player.bomb != null )
        {
            if ( @ent == @player.bomb.bombEnt )
            {
                player.bomb.Free();
                player.bombCooldownTime = levelTime;

                other.client.printMessage("You picked up your cluster bomb.\n");
                int spawnSoundIndex = G_SoundIndex( "sounds/items/ammo_pickup" );
                G_Sound( other, CHAN_AUTO, spawnSoundIndex, 0.4f );
            }
        }
    }

}

void bomb_body_die( Entity @self, Entity @inflictor, Entity @attacker )
{
    if ( self.count >= 0 && self.count < MAX_BOMBS )
        gtBombs[self.count].die( inflictor, attacker );
}

void bomb_body_think( Entity @self )
{
    // if for some reason the bomb moved to inside a solid, kill it
    if ( ( G_PointContents( self.origin ) & (CONTENTS_SOLID|CONTENTS_NODROP) ) != 0 )
    {
        bomb_body_die( self, @G_GetEntity(0), @G_GetEntity(0) );
        return;
    }

	if ( self.count >= 0 && self.count < MAX_BOMBS )
	{
		if ( gtBombs[self.count].explodeTime > 0 && levelTime > gtBombs[self.count].explodeTime ) 
		{
	        bomb_body_die( self, @G_GetEntity(0), @G_GetEntity(0) );
	        return;		
		}
	}

    self.nextThink = levelTime + 25; // no need to check this every frame
}

cBomb @ClientDropBomb( Client @client )
{
    if ( @client == null )
        return null;

    cBomb @bomb = null;

    // find an unused bomb slot
    for ( int i = 0; i < MAX_BOMBS; i++ )
    {
        if ( gtBombs[i].inuse == false )
        {
            @bomb = @gtBombs[i];
            break;
        }
    }

    if ( @bomb == null )
    {
        // let the clients read this, so they complaint to us in case it's happening
        client.printMessage( "GT ERROR: ClientDropbomb: MAX_BOMBS reached. Can't spawn bomb.\n" );
        return null;
    }

    // nodrop area
    if ( ( G_PointContents( client.getEnt().origin ) & CONTENTS_NODROP ) != 0 )
        return null;

    // first check that there's space for spawning the bomb in front of us
    Vec3 dir, start, end, r, u;

    client.getEnt().angles.angleVectors( dir, r, u );
    start = client.getEnt().origin + Vec3( 0, 0, client.getEnt().viewHeight );
    end = ( start + ( 0.5 * ( bombMaxs + bombMins) ) ) + ( dir * 64 );

    Trace tr;

    tr.doTrace( start, bombMins, bombMaxs, end, client.getEnt().entNum, MASK_PLAYERSOLID );

    // try spawning the bomb
    if ( !bomb.Spawn( tr.endPos, client ) ) // can't spawn bomb in that position. Blocked by something
        return null;

    // assign some frontal velocity to the bomb, as for being dropped by the player
    dir.normalize();
    dir *= 750;
    dir.z += 100;

    bomb.bombEnt.velocity = dir;
    bomb.bombEnt.linkEntity();

    return @bomb;
}
