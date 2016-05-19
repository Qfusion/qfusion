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

/***********************************************************************

HOW TO USE THIS TURRET CLASS

----------------------------

Include the turret class file in your gametype project. If you want
turrets to be spawned from maps, all it requires is to place misc_turret
entities in the map. If you want players to be able to build turrets, add
to your gametype a command ( classAction1 is the recommended name ) and,
at the reception of this command in GT_Command,
call the function: "cTurret @ClientDropTurret( Client @client )".

This function call will return a cTurret object for you to configure it
( you can change the models, sounds, damage, fire ratio, projectile, etc)
or null in the case the turret couldn't be dropped (blocked by the world, etc).

************************************************************************/


const int MAX_TURRETS = 64;

cTurret[] gtTurrets( MAX_TURRETS );

Vec3 turretMins( -16, -16, -16 ), turretMaxs( 16, 16, 40 );

class cTurret
{
    bool inuse;

    Entity @bodyEnt; 	// main entity and tripod
    Entity @gunEnt; 	// full aiming gun model
    Entity @flashEnt; // muzzleflash

    Client @client;

    Entity @enemy;	// located target
    Entity @invisibleEnemy;
    uint invisibleEnemySince;
    uint invisibleEnemyReactionTime;
    int invisibilityAlarmSoundIndex;
    uint lastInvisibilityAlarmTime;
    uint invisibilityAlarmRepeatDelay;
	
    Entity @minimap;   // Minimap
    Entity @sprite;    // On-screen sprite	

    float gunOffset;	// height of the gun relative to the rotator
    int flashTime;		// duration of the muzzleflash
    int fireSoundIndex;	// sound index of the sound it makes at firing
    int moveSoundIndex;	// sound index of the sound it makes at moving
    float yawSpeed; 	// horizontal rotation speed in degrees per second
    float pitchSpeed; 	// vertical rotation speed in degrees per second
    float minPITCH; 	// maximum looking-down angle (negative value)
    uint scanDelay; 	// time between scans for enemies in milliseconds
    uint refireDelay;	// time between shooting
    bool returnToIdle; 	// go back to initial position when no enemies
    int fireMode;		// ammo item values define the projectile it will shoot
    int range;		    // distance at which enemies are located
	bool detected;      // Is this turret detected? aka draw on map/wh?
    int spread;
    int damage;
    int knockback;
    int stun;
	uint detectTime;
    int projectileSpeed;	// only for projectiles (rockets, grenades, plasma)
    int splashRadius;		// only for projectiles (rockets, grenades, plasma)

    int painSoundIndex;
    uint lastPainTime;
    uint painDelay;

    Vec3 idleAngles;		// initial angles. Do not modify.
    uint firedTime;		// last time it shot. Do not modify
    uint targetLocationTime; 	// last time it scanned for enemies. Do not modify

    void Init()
    {
        // set up with default values
        this.inuse = false;
		this.detected = false;
        @this.client = null;
        @this.enemy = null;
        @this.invisibleEnemy = null;
        this.invisibleEnemySince = 0;
        this.invisibleEnemyReactionTime = 125;
        this.invisibilityAlarmSoundIndex = G_SoundIndex( "sounds/misc/timer_bip_bip" );
        this.lastInvisibilityAlarmTime = 0;
        this.invisibilityAlarmRepeatDelay = 1500;
        this.gunOffset = 8;
        this.flashTime = 100;
        this.yawSpeed = 100.0f;
        this.pitchSpeed = 24.0f;
        this.scanDelay = 500;
        this.range = 750;
        this.spread = 75;
        this.damage = 10;
        this.knockback = 7;
        this.stun = 0;
        this.refireDelay = 200;
        this.minPITCH = -45;
        this.fireMode = AMMO_BULLETS;
        this.fireSoundIndex = G_SoundIndex( "sounds/weapons/machinegun_fire" );
        this.moveSoundIndex = G_SoundIndex( "sounds/movers/elevator_move" );
        this.projectileSpeed = 1175; //  set up as for AMMO_ROCKETS
        this.splashRadius = 150; // set up as for AMMO_ROCKETS
        this.returnToIdle = false;
        this.lastPainTime = 0;
        this.painDelay = 300;
        this.painSoundIndex = G_SoundIndex( "sounds/ctftactics/turret_pain01" );
    }

    cTurret()
    {
        this.Init();
    }

    ~cTurret()
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

        if ( @this.gunEnt != null )
        {
            this.gunEnt.freeEntity();
            @this.gunEnt = null;
        }

        if ( @this.flashEnt != null )
        {
            this.flashEnt.freeEntity();
            @this.flashEnt = null;
        }
		
        if( @this.minimap != null )
        {
            this.minimap.freeEntity();
            @this.minimap = null;    
        }
        
        if( @this.sprite != null )
        {
            this.sprite.freeEntity();
            @this.sprite = null;    
        }        		

        this.Init();
    }

    bool Spawn( Vec3 origin, float yawAngle, int team )
    {
        // try to position the turret in the world.
        Trace tr;

        // check the initial position is not inside solid

        if ( tr.doTrace( origin, turretMins , turretMaxs, origin, -1, MASK_PLAYERSOLID ) )
            return false;

        if ( tr.startSolid || tr.allSolid )
            return false; // initial position is inside solid, we can not spawn the turret

        // proceed setting up
        this.Init();

        // start aiming to the same angles the turret spawns
        this.idleAngles = Vec3( 0, yawAngle, 0 );
        this.targetLocationTime = levelTime + 5000; // wait some time before start tracking
        this.firedTime = this.targetLocationTime;
        @this.enemy = null;

        Vec3 partOrigin = origin;

        // the body entity will be used for collision. Will be the only solid entity of
        // the three, and will have physic properties. It will not rotate.
        @this.bodyEnt = @G_SpawnEntity( "turret_body" );
		@this.bodyEnt.think = turret_body_think;
		@this.bodyEnt.die = turret_body_die;
		@this.bodyEnt.pain = turret_body_pain;
        this.bodyEnt.type = ET_GENERIC;
        this.bodyEnt.modelindex = G_ModelIndex( "models/objects/turret/base.md3", true );
        this.bodyEnt.setSize( turretMins, turretMaxs );
        this.bodyEnt.team = team;
        this.bodyEnt.origin = partOrigin;
        this.bodyEnt.angles = this.idleAngles;
        this.bodyEnt.solid = SOLID_YES;
        this.bodyEnt.clipMask = MASK_PLAYERSOLID;
        this.bodyEnt.moveType = MOVETYPE_TOSS;
        this.bodyEnt.svflags &= ~SVF_NOCLIENT;
        this.bodyEnt.health = 250;
        this.bodyEnt.mass = 450;
        this.bodyEnt.takeDamage = 1;
        this.bodyEnt.nextThink = levelTime + 1;
        this.bodyEnt.linkEntity();

        // the count field will be used to store the index of the cTurret object
        // int the list. If the object is part of the list, ofc. This is just for
        // quickly accessing it.
        int index = -1;
        for ( int i = 0; i < MAX_TURRETS; i++ )
        {
            if ( @gtTurrets[i] == @this )
            {
                index = i;
                break;
            }
        }

        this.bodyEnt.count = index;

        // gun entity will not be solid and only be used to draw the gun model
        @this.gunEnt = @G_SpawnEntity( "turret_gun" );
        this.gunEnt.type = ET_GENERIC;
        this.gunEnt.modelindex = G_ModelIndex( "models/objects/turret/gun.md3", true );
        this.gunEnt.team = team;
        this.gunEnt.solid = SOLID_NOT;
        this.gunEnt.svflags &= ~SVF_NOCLIENT;
        partOrigin.z += this.gunOffset;
        this.gunEnt.origin = partOrigin;
        this.gunEnt.angles = this.idleAngles;
        this.gunEnt.takeDamage = DAMAGE_NO;
        this.gunEnt.linkEntity();

        // flash entity will not be solid and only be used to draw the muzzleflash
        @this.flashEnt = @G_SpawnEntity( "turret_flash" );
		@this.flashEnt.think = turret_flash_think;
        this.flashEnt.type = ET_GENERIC;
        this.flashEnt.modelindex = G_ModelIndex( "models/objects/turret/flash.md3", true );
        this.flashEnt.team = team;
        this.flashEnt.solid = SOLID_NOT;
        this.flashEnt.origin = partOrigin;
        this.flashEnt.angles = this.idleAngles;
        this.flashEnt.takeDamage = DAMAGE_NO;
        this.flashEnt.linkEntity();

        this.inuse = true;

        return true; // turret has been correctly spawned
    }
	
	void detect()
	{
		this.detected = true;
		
		Vec3 partOrigin = this.bodyEnt.origin;
		
        // another entity to represent it in the minimap
        @this.minimap = @G_SpawnEntity( "capture_indicator_minimap" );
        this.minimap.type = ET_MINIMAP_ICON;
        this.minimap.solid = SOLID_NOT;
        this.minimap.team = this.bodyEnt.team;
        this.minimap.origin = partOrigin;
        this.minimap.modelindex = G_ImageIndex( "gfx/indicators/radar_1" );
        this.minimap.frame = 22; // size in case of a ET_MINIMAP_ICON
        this.minimap.svflags = (this.minimap.svflags & ~uint(SVF_NOCLIENT)) | uint(SVF_BROADCAST);
        this.minimap.linkEntity();        
        
        // the sprite entity is also placed upwards and sent as broadcast
        @this.sprite = @G_SpawnEntity( "capture_indicator_sprite" );
        this.sprite.type = ET_RADAR;
        this.sprite.solid = SOLID_NOT;
        this.sprite.origin = partOrigin;
        this.sprite.team = this.bodyEnt.team;
        this.sprite.modelindex = G_ImageIndex( "gfx/indicators/radar" );
        this.sprite.frame = 132; // radius in case of a ET_SPRITE
        this.sprite.svflags = (this.sprite.svflags & ~uint(SVF_NOCLIENT)) | uint(SVF_BROADCAST);
        this.sprite.linkEntity();			
		
		this.detectTime = levelTime;
	}
	
	void hide()
	{
        if( @this.minimap != null )
        {
            this.minimap.freeEntity();
            @this.minimap = null;    
        }
        
        if( @this.sprite != null )
        {
            this.sprite.freeEntity();
            @this.sprite = null;    
        }	
		
		this.detected = false;
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

    void scan()
    {
        if ( !this.inuse )
            return;

        // see if it's time for a new scan
        if ( this.targetLocationTime > levelTime )
            return;

        this.targetLocationTime = levelTime + this.scanDelay;

        // do the scan
        // find players around
        Trace tr;
        Vec3 center, mins, maxs;
        Vec3 origin = this.gunEnt.origin;
        Entity @target = null;
        Entity @bestTarget = null;
        float bestRange = this.range + 1000;

		array<Entity @> @inradius = G_FindInRadius( origin, this.range );
        for( uint i = 0; i < inradius.size(); i++ )
		{
            @target = inradius[i];
			if( @target.client == null )
				continue;

            if ( target.client.state() < CS_SPAWNED )
                continue;

            if ( target.isGhosting() )
                continue;

            if ( gametype.isTeamBased && target.team == this.bodyEnt.team )
                continue;

            // check if the player is visible from the turret
            target.getSize( mins, maxs );
            center = target.origin + ( 0.5 * ( maxs + mins ) );
            mins = 0;
            maxs = 0;

            if ( !tr.doTrace( origin, mins, maxs, center, target.entNum, MASK_SOLID ) )
            {
                // found a visible enemy, compare ranges
                float range = origin.distance( tr.endPos );
                if ( range < bestRange )
                {
                    bestRange = range;
                    @bestTarget = @target;
                }
            }
        }

        @this.enemy = @bestTarget;

        if ( @this.enemy != @this.invisibleEnemy )
        {
            this.invisibleEnemySince = 0;
            @this.invisibleEnemy = null;

            if ( @this.enemy != null && @this.enemy.client != null )
            {
                if ( GetPlayer( this.enemy.client ).invisibilityEnabled )
                {
                    this.invisibleEnemySince = levelTime;
                    @this.invisibleEnemy = @this.enemy;
                }
            }
        }
    }

    void think()
    {
        if ( !this.inuse )
            return;

        // refresh all turret parts origins based on the body part origin
        Vec3 gunOrigin = this.bodyEnt.origin;

        // if for some reason the turret moved to inside a solid, kill it
        if ( ( G_PointContents( gunOrigin ) & (CONTENTS_SOLID|CONTENTS_NODROP) ) != 0 )
        {
            this.die( @G_GetEntity(0), @G_GetEntity(0) );
            return;
        }
		
        // Update minimap and sprite location
		if ( this.detected == true ) 
		{
			this.sprite.origin = this.bodyEnt.origin;
			this.minimap.origin = this.bodyEnt.origin;
			
			if ( levelTime > ( this.detectTime + CTFT_RUNNER_DETECT_DURATION ) )
				this.hide();
		}

        gunOrigin.z += this.gunOffset;

        this.gunEnt.origin = gunOrigin;
        this.gunEnt.linkEntity();

        this.flashEnt.origin = gunOrigin;
        this.flashEnt.linkEntity();

        // delete moving sound (may be restored later)
        this.bodyEnt.sound = 0;

        // scan for targets
        this.scan();

        if( @this.invisibleEnemy != null )
        {
        	if( this.lastInvisibilityAlarmTime + this.invisibilityAlarmRepeatDelay <= levelTime )
        	{
				this.lastInvisibilityAlarmTime = levelTime;
				G_Sound( this.bodyEnt, CHAN_VOICE, this.invisibilityAlarmSoundIndex, 0.8f );
        	}
        }

        // for those trying to learn working with angles
        // Vec3.x is the PITCH angle (up and down rotation)
        // Vec3.y is the YAW angle (left and right rotation)
        // Vec3.z is the ROLL angle (left and right inclination)
        Vec3 currentAngles, desiredAngles;

        currentAngles = this.gunEnt.angles;

        // find desired aiming angles
        if ( @this.enemy != null )
        {
            Vec3 dir = this.enemy.origin - this.gunEnt.origin;
            desiredAngles = dir.toAngles();
        }
        else
        {
            if ( this.returnToIdle )
                desiredAngles = this.idleAngles;
            else
                desiredAngles = currentAngles;
        }

        // normalize180 all angles so they can be compared
        currentAngles.x = turretAngleNormalize180( currentAngles.x );
        currentAngles.y = turretAngleNormalize180( currentAngles.y );
        currentAngles.z = turretAngleNormalize180( currentAngles.z );
        desiredAngles.x = turretAngleNormalize180( desiredAngles.x );
        desiredAngles.y = turretAngleNormalize180( desiredAngles.y );
        desiredAngles.z = turretAngleNormalize180( desiredAngles.z );

        // rotate the turret to the desired angles
        if ( currentAngles != desiredAngles )
        {
            float maxYAWmove = this.yawSpeed * ( float(frameTime) * 0.001f );
            float YAWmove = 0;

            if ( currentAngles.y != desiredAngles.y )
            {
                YAWmove = turretAngleNormalize180( desiredAngles.y - currentAngles.y );

                // move at defined speed
                if ( YAWmove < -maxYAWmove )
                    YAWmove = -maxYAWmove;
                if ( YAWmove > maxYAWmove )
                    YAWmove = maxYAWmove;
            }

            float maxPITCHmove = this.pitchSpeed * ( float(frameTime) * 0.001f );
            float PITCHmove = 0;

            if ( currentAngles.x != desiredAngles.x )
            {
                PITCHmove = turretAngleNormalize180( desiredAngles.x - currentAngles.x );

                // move at defined speed
                if ( PITCHmove < -maxPITCHmove )
                    PITCHmove = -maxPITCHmove;
                if ( PITCHmove > maxPITCHmove )
                    PITCHmove = maxPITCHmove;
            }

            // Set up the new angles
            currentAngles.x += PITCHmove;
            currentAngles.y += YAWmove;

            // PITCH has limits on how much it can rotate.
            if ( currentAngles.x < this.minPITCH )
                currentAngles.x = this.minPITCH;

            currentAngles.z = 0;

            // re-normalize 180 for comparison safety
            currentAngles.x = turretAngleNormalize180( currentAngles.x );
            currentAngles.y = turretAngleNormalize180( currentAngles.y );

            // set up entities with the new angles
            this.gunEnt.angles = currentAngles;
            this.flashEnt.angles = currentAngles;

            this.bodyEnt.sound = this.moveSoundIndex;
        }

        // and, finally, if the turret is locked on the target, shoot
        if ( ( this.firedTime + this.refireDelay <= levelTime )
                && ( @this.enemy != null )
                && ( levelTime > this.invisibleEnemySince + invisibleEnemyReactionTime )
                && ( currentAngles == desiredAngles ) )
        {
            switch ( this.fireMode )
            {
            case AMMO_BULLETS:
                G_FireBullet( this.gunEnt.origin, currentAngles, this.range, this.spread, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_INSTAS:
                G_FireInstaShot( this.gunEnt.origin, currentAngles, this.range, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_BOLTS:
                G_FireStrongBolt( this.gunEnt.origin, currentAngles, this.range, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_PLASMA:
                G_FirePlasma( this.gunEnt.origin, currentAngles, this.projectileSpeed, this.splashRadius, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_ROCKETS:
                G_FireRocket( this.gunEnt.origin, currentAngles, this.projectileSpeed, this.splashRadius, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_GRENADES:
                G_FireGrenade( this.gunEnt.origin, currentAngles, this.projectileSpeed, this.splashRadius, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            case AMMO_GUNBLADE:
                G_FireBlast( this.gunEnt.origin, currentAngles, this.projectileSpeed, this.splashRadius, this.damage, this.knockback, this.stun, this.bodyEnt );
                break;

            default:
                G_Print( "Gametype script: cTurret::think(): Invalid fire mode\n" );
                break;
            }

            if ( !this.inuse )
                return;

            // shooting noise
            if ( this.fireSoundIndex > 0 )
                G_Sound( this.bodyEnt, CHAN_MUZZLEFLASH, this.fireSoundIndex, 0.5f );

            // enable the flash
            this.flashEnt.svflags &= ~SVF_NOCLIENT;
            this.flashEnt.nextThink = levelTime + this.flashTime;

            this.firedTime = levelTime;
        }

        this.bodyEnt.nextThink = levelTime + 1;
    }

    void pain( Entity @attacker, float kick, float damage )
    {
        if ( !this.inuse || @this.bodyEnt == null )
            return;

        if ( this.lastPainTime + this.painDelay > levelTime )
            return;

        this.lastPainTime = levelTime;

        G_Sound( this.bodyEnt, CHAN_PAIN, this.painSoundIndex, 1.0f );

        if ( @this.client != null )
        {
            if ( GetPlayer( this.client ).playerClass.tag == PLAYERCLASS_ENGINEER
                    && this.client.getEnt().team == this.bodyEnt.team )
            {
                G_CenterPrintMsg( client.getEnt(), S_COLOR_RED + "Your turrets are being damaged" );
            }
        }
    }
}

// helper function.
float turretAngleNormalize180( float angle )
{
    angle = ( 360.0 / 65536 ) * ( int( angle * float( 65536 / 360.0 ) ) & 65535 );
    if ( angle > 180.0f )
        angle -= 360.0f;

    return angle;
}

void turret_flash_think( Entity @self )
{
    self.svflags |= SVF_NOCLIENT; // hide the muzzleflash
}

void turret_body_think( Entity @self )
{
    if ( self.count >= 0 && self.count < MAX_TURRETS )
        gtTurrets[self.count].think();
}

void turret_body_die( Entity @self, Entity @inflictor, Entity @attacker )
{
    if ( self.count >= 0 && self.count < MAX_TURRETS )
        gtTurrets[self.count].die( inflictor, attacker );
}

void turret_body_pain( Entity @self, Entity @other, float kick, float damage )
{
    if ( self.count >= 0 && self.count < MAX_TURRETS )
        gtTurrets[self.count].pain( other, kick, damage );
}

cTurret @ClientDropTurret( Client @client )
{
    if ( @client == null )
        return null;

    cTurret @turret = null;

    // find an unused turret slot
    for ( int i = 0; i < MAX_TURRETS; i++ )
    {
        if ( gtTurrets[i].inuse == false )
        {
            @turret = @gtTurrets[i];
            break;
        }
    }

    if ( @turret == null )
    {
        G_Print( "GT: ClientDropTurret: MAX_TURRETS reached. Can't spawn turret.\n" );
        return null;
    }

    // nodrop area
    if ( ( G_PointContents( client.getEnt().origin ) & CONTENTS_NODROP ) != 0 )
        return null;

    // first check that there's space for spawning the turret in front of us
    Vec3 dir, start, end, r, u;

    client.getEnt().angles.angleVectors( dir, r, u );
    start = client.getEnt().origin;
    start.z += 6;
    end = ( start + ( 0.5 * ( turretMaxs + turretMins) ) ) + ( dir * 64 );

    Trace tr;

    tr.doTrace( start, turretMins, turretMaxs, end, client.getEnt().entNum, MASK_PLAYERSOLID );

    // try spawning the turret
    if ( !turret.Spawn( tr.endPos, client.getEnt().angles.y, client.getEnt().team ) ) // can't spawn turret in that position. Blocked by something
        return null;

    // assign some frontal velocity to the turret, as for being dropped by the player
    float speed = client.getEnt().velocity.length();
    dir *= speed + 40;
    dir.z = 50;
    turret.bodyEnt.velocity = dir;
    turret.bodyEnt.linkEntity();

    return @turret;
}

// spawn a turret from the map
void misc_turret( Entity @ent )
{
    // drop to floor

    Trace tr;
    Vec3 start, end;

    end = start = ent.origin;
    end.z -= 1024;
    start.z += 16;

    // check for starting inside solid
    tr.doTrace( start, turretMins, turretMaxs, start, ent.entNum, MASK_DEADSOLID );
    if ( tr.startSolid || tr.allSolid )
    {
        G_Print( ent.classname + " starts inside solid. Inhibited\n" );
        ent.freeEntity();
        return;
    }

    if ( ( ent.spawnFlags & 1 ) == 0 ) // do not drop if having the float flag
    {
        if ( tr.doTrace( start, turretMins, turretMaxs, end, ent.entNum, MASK_DEADSOLID ) )
        {
            start = tr.endPos + tr.planeNormal;
            ent.origin = start;
            ent.origin2 = start;
        }
    }

    // once positioned, spawn a turret object, and release the originating entity
    cTurret @turret = null;

    // find an unused turret slot
    for ( int i = 0; i < MAX_TURRETS; i++ )
    {
        if ( gtTurrets[i].inuse == false )
        {
            @turret = @gtTurrets[i];
            break;
        }
    }

    if ( @turret == null )
        G_Print( "GT: misc_turret: MAX_TURRETS reached. Can't spawn turret.\n" );
    else
    {
        int team = G_SpawnTempValue( "gameteam" ).toInt();

        // try spawning the turret
        if ( turret.Spawn( ent.origin, ent.angles.y, team ) )
        {
            // set up with some values assigned from the map
            if ( ent.health > 0 )
                turret.bodyEnt.health = ent.health;

            if ( ent.mass > 0 )
                turret.bodyEnt.mass = ent.mass;

            if ( ent.damage > 0 )
                turret.damage = ent.damage;

            if ( ent.delay > 0.0f )
                turret.refireDelay = uint( ent.delay * 1000 );

            if ( ent.style > 0 )
                turret.fireMode = ent.style;
        }
    }

    // free the original entity. It won't be used anymore
    ent.freeEntity();
}


