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

/*
** TRIGGERS
*/
void trigger_changelevel_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
	if( @other.client == null )
		return;

	G_Print( other.client.name + " exited the level\n" );

	match.launchState( MATCH_STATE_POSTMATCH );
}

void trigger_changelevel( Entity @ent )
{
	// sample trigger entity that advances to the next map
	if( true )
	{
		ent.freeEntity();
		return;
	}

	@ent.touch = trigger_changelevel_touch;
	ent.setupModel( ent.model ); // set up the brush model
    ent.solid = SOLID_TRIGGER;
    ent.linkEntity();

	Entity @target = G_SpawnEntity( "target_changelevel" );
	target.map = ent.map;

	Cvar mapRotation( "g_maprotation", "1", 0 );
	mapRotation.set( -1 );
}

void trigger_onlyregistered_think( Entity @ent )
{
	if ( ent.style <= 0 )
		return;
	ent.freeEntity();
}

void trigger_onlyregistered_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
	if( ent.style > 0 )
		return;

	ent.style = 1;
	ent.useTargets( other );
	ent.nextThink = levelTime + int( ent.delay * 1000.0 ) + 100;
}

void trigger_onlyregistered( Entity @ent )
{
	@ent.think = trigger_onlyregistered_think;
	@ent.touch = trigger_onlyregistered_touch;
	ent.style = 0;
	ent.setupModel( ent.model ); // set up the brush model
    ent.solid = SOLID_TRIGGER;
    ent.linkEntity();
}

/*
** LIGHT ENTITIES
*/
const uint START_OFF = 1;

void light_fluoro_use( Entity @ent, Entity @other, Entity @activator )
{
	if( ent.style < 0 )
		return;

	if( ( ent.spawnFlags & START_OFF ) == START_OFF )
	{
		G_ConfigString( CS_LIGHTS + ent.style, "m" );
		ent.spawnFlags &= ~START_OFF;
	}
	else
	{
		G_ConfigString( CS_LIGHTS + ent.style, "a" );
		ent.spawnFlags |= START_OFF;
	}
}

/*QUAKED light_fluoro (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default style is 0
If targeted, it will toggle between on or off.
Makes steady fluorescent humming sound
*/
void light_fluoro( Entity @ent )
{
	@ent.use = light_fluoro_use;
	ent.style -= 32;
	if( ent.style >= 0 )
	{
		if( ( ent.spawnFlags & START_OFF ) == START_OFF )
			G_ConfigString( CS_LIGHTS + ent.style, "a" );
		else
			G_ConfigString( CS_LIGHTS + ent.style, "m" );
	}

	ent.svflags &= ~uint( SVF_NOCLIENT );
	ent.sound = G_SoundIndex( "sound/ambience/fl_hum1.wav" );

	ent.linkEntity();
}

/*QUAKED light_fluorospark (0 1 0) (-8 -8 -8) (8 8 8)
Non-displayed light.
Makes sparking, broken fluorescent sound
*/
void light_fluorospark( Entity @ent )
{
	ent.svflags &= ~uint( SVF_NOCLIENT );
	ent.sound = G_SoundIndex( "sound/ambience/buzz1.wav" );
	ent.linkEntity();
}


void FireAmbient( Entity @ent )
{
	ent.svflags &= ~uint( SVF_NOCLIENT );
	ent.sound = G_SoundIndex( "sound/ambience/fire1.wav" );
	ent.linkEntity();
};

/*QUAKED light_torch_small_walltorch (0 .5 0) (-10 -10 -20) (10 10 20)
Short wall torch
*/
void light_torch_small_walltorch( Entity @ent )
{
	FireAmbient( ent );
}

/*QUAKED light_flame_large_yellow (0 1 0) (-10 -10 -12) (12 12 18)
Large yellow flame ball
*/
void light_flame_large_yellow( Entity @ent )
{
	FireAmbient( ent );
}

/*QUAKED light_flame_small_yellow (0 1 0) (-8 -8 -8) (8 8 8)
Small yellow flame ball
*/
void light_flame_small_yellow( Entity @ent )
{
	FireAmbient( ent );
}

/*QUAKED light_flame_small_white (0 1 0) (-10 -10 -40) (10 10 40)
Small white flame ball
*/
void light_flame_small_white( Entity @ent )
{
	FireAmbient( ent );
}

/*
** MISC
*/
void func_illusionary( Entity @ent )
{
    ent.setupModel( ent.model ); // set up the brush model
    ent.solid = SOLID_NOT;
    ent.linkEntity();
}

void info_intermission( Entity @ent )
{
	ent.classname = "info_player_intermission";
}

const uint SPAWNFLAG_SUPERSPIKE	= 1;
const uint SPAWNFLAG_LASER = 2;

void shooter_fire( Entity @ent )
{
	Entity @attacker;

	// world is used as an attacker entity insted of spike shooter
	// because otherwise a suicide obituary would be printed
	@attacker = G_GetEntity( 0 );

	if ( (ent.spawnFlags & SPAWNFLAG_LASER) == SPAWNFLAG_LASER )
	{
		G_FireBlast( ent.origin, ent.angles, 600, 1, 15, 0, 0, attacker );
	}
	else if ( (ent.spawnFlags & SPAWNFLAG_SUPERSPIKE) == SPAWNFLAG_SUPERSPIKE )
	{
		G_FirePlasma( ent.origin, ent.angles, 600, 1, 18, 0, 0, attacker );
	}
	else
	{
		G_FirePlasma( ent.origin, ent.angles, 600, 1, 9, 0, 0, attacker );
	}
}

void trap_spikeshooter_use( Entity @ent, Entity @other, Entity @activator )
{
	shooter_fire( @ent );
}

/*QUAKED trap_spikeshooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser
When triggered, fires a spike in the direction set in QuakeEd.
*/
void trap_spikeshooter( Entity @ent )
{
	@ent.use = trap_spikeshooter_use;
}

void trap_shooter_think( Entity @self )
{
	shooter_fire( self );
	self.nextThink = levelTime + int( self.wait * 1000.0 );
}

void trap_shooter_use( Entity @ent, Entity @other, Entity @activator )
{
	trap_spikeshooter_use( ent, other, activator );
}

/*QUAKED trap_shooter (0 .5 .8) (-8 -8 -8) (8 8 8) superspike laser
*/
void trap_shooter( Entity @ent )
{
	@ent.think = trap_shooter_think;
	@ent.use = trap_shooter_use;
	
	trap_spikeshooter( ent );

	if( ent.wait == 0 )
		ent.wait = 1;
	ent.nextThink = ent.nextThink + int( ent.wait * 1000.0 );
}

//------------------------------------------
// Not Q1, but I don't want to add a new file
//------------------------------------------

/*QUAKED shooter_rocket (0 .5 .8) (-8 -8 -8) (8 8 8)
*/
void shooter_rocket_use( Entity @ent, Entity @other, Entity @activator )
{
    if ( @activator == null || @activator.client == null )
        return;

	Vec3 aimAngles;

    // find shooting angles
	array<Entity @> @targets = ent.findTargets();
    if ( !targets.empty() ) {
		Entity @target = targets[0];
		Vec3 dir(target.origin - ent.origin);
        aimAngles = dir.toAngles();
	}
    else // it isn't pointing anywhere, aim to angles value
        aimAngles = ent.angles;

	G_FireRocket( ent.origin, aimAngles,
	 1150, 145, 80, 100, 0, activator );
}

void shooter_rocket_think( Entity @ent )
{
    // see if the point is targeted elsewhere
	array<Entity @> @triggers = @ent.findTargeting();
	
    // if the shooter is triggered it will be fired from
    // elsewhere and doesn't need to think again	
	if( !triggers.empty() )
		return;

    ent.nextThink = ent.nextThink + int( ent.wait * 1000.0 );
    shooter_rocket_use( ent, null, null );
}

void shooter_rocket( Entity @ent )
{
	@ent.use = shooter_rocket_use;
	@ent.think = shooter_rocket_think;

    if ( ent.wait == 0 )
        ent.wait = 1;
    ent.nextThink = ent.nextThink + int( ent.wait * 1000.0 );
}


//============================================================================

void AmbientSound( Entity @ent, String &str )
{
	ent.svflags &= ~uint( SVF_NOCLIENT );
	ent.sound = G_SoundIndex( str );
	ent.linkEntity();
}

/*QUAKED ambient_suck_wind (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_suck_wind( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/suck1.wav" );
}

/*QUAKED ambient_drone (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_drone( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/drone6.wav" );
}

/*QUAKED ambient_flouro_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_flouro_buzz( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/buzz1.wav" );
}

/*QUAKED ambient_drip (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_drip( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/drip1.wav" );
}

/*QUAKED ambient_comp_hum (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_comp_hum( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/comp1.wav" );
}

/*QUAKED ambient_thunder (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_thunder( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/thunder1.wav" );
}

/*QUAKED ambient_light_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_light_buzz( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/fl_hum1.wav" );
}

/*QUAKED ambient_swamp1 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_swamp1( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/swamp1.wav" );
}

/*QUAKED ambient_swamp2 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
*/
void ambient_swamp2( Entity @ent )
{
	AmbientSound( ent, "sound/ambience/swamp2.wav" );
}

