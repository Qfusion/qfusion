/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "g_local.h"

enum {
	PROJECTILE_TOUCH_NOT,
	PROJECTILE_TOUCH_DIRECTHIT,
	PROJECTILE_TOUCH_DIRECTAIRHIT,
};

int G_Projectile_HitStyle( edict_t *projectile, edict_t *target ) {
	// can't hit yourself
	if( target == projectile->r.owner && target != world ) {
		return PROJECTILE_TOUCH_NOT;
	}

	if( !target->takedamage || ISBRUSHMODEL( target->s.modelindex ) ) {
		return PROJECTILE_TOUCH_DIRECTHIT;
	}

	if( target->waterlevel > 1 ) {
		return PROJECTILE_TOUCH_DIRECTHIT;
	}

	if( !target->groundentity ) {
		const float AIRHIT_MINHEIGHT = 64.0f;

		vec3_t end;
		VectorCopy( target->s.origin, end );
		end[2] -= AIRHIT_MINHEIGHT;

		trace_t trace;
		G_Trace4D( &trace, target->s.origin, target->r.mins, target->r.maxs, end, target, MASK_DEADSOLID, 0 );
		if( ( trace.ent != -1 || trace.startsolid ) && ISWALKABLEPLANE( &trace.plane ) ) {
			return PROJECTILE_TOUCH_DIRECTAIRHIT;
		}
	}

	return PROJECTILE_TOUCH_DIRECTHIT;
}

static void ForgotToSetProjectileTouch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	assert( false );
}

/*
* W_Fire_LinearProjectile - Spawn a generic linear projectile without a model, touch func, sound nor mod
*/
static edict_t *W_Fire_LinearProjectile( edict_t *self, vec3_t start, vec3_t angles, int speed,
										 float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *projectile;
	vec3_t dir;

	projectile = G_Spawn();
	VectorCopy( start, projectile->s.origin );
	VectorCopy( start, projectile->olds.origin );

	VectorCopy( angles, projectile->s.angles );
	AngleVectors( angles, dir, NULL, NULL );
	VectorScale( dir, speed, projectile->velocity );

	projectile->movetype = MOVETYPE_LINEARPROJECTILE;

	projectile->r.solid = SOLID_YES;
	projectile->r.clipmask = ( !GS_RaceGametype() ) ? MASK_SHOT : MASK_SOLID;

	projectile->r.svflags = SVF_PROJECTILE;

	// enable me when drawing exception is added to cgame
	VectorClear( projectile->r.mins );
	VectorClear( projectile->r.maxs );
	projectile->s.modelindex = 0;
	projectile->r.owner = self;
	projectile->s.ownerNum = ENTNUM( self );
	projectile->touch = ForgotToSetProjectileTouch;
	projectile->nextThink = level.time + timeout;
	projectile->think = G_FreeEdict;
	projectile->classname = NULL; // should be replaced after calling this func.
	projectile->s.sound = 0;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;

	projectile->projectileInfo.minDamage = min( minDamage, damage );
	projectile->projectileInfo.maxDamage = damage;
	projectile->projectileInfo.minKnockback = min( minKnockback, maxKnockback );
	projectile->projectileInfo.maxKnockback = maxKnockback;
	projectile->projectileInfo.radius = radius;

	GClip_LinkEntity( projectile );

	// update some data required for the transmission
	projectile->s.linearMovement = true;
	VectorCopy( projectile->s.origin, projectile->s.linearMovementBegin );
	VectorCopy( projectile->velocity, projectile->s.linearMovementVelocity );
	projectile->s.linearMovementTimeStamp = game.serverTime;
	projectile->s.team = self->s.team;
	projectile->s.modelindex2 = ( abs( timeDelta ) > 255 ) ? 255 : (unsigned int)abs( timeDelta );
	return projectile;
}

/*
* W_Fire_TossProjectile - Spawn a generic projectile without a model, touch func, sound nor mod
*/
static edict_t *W_Fire_TossProjectile( edict_t *self, vec3_t start, vec3_t angles, int speed,
									   float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *projectile;
	vec3_t dir;

	projectile = G_Spawn();
	VectorCopy( start, projectile->s.origin );
	VectorCopy( start, projectile->olds.origin );

	VectorCopy( angles, projectile->s.angles );
	AngleVectors( angles, dir, NULL, NULL );
	VectorScale( dir, speed, projectile->velocity );

	projectile->movetype = MOVETYPE_BOUNCEGRENADE;

	// make missile fly through players in race
	if( GS_RaceGametype() ) {
		projectile->r.clipmask = MASK_SOLID;
	} else {
		projectile->r.clipmask = MASK_SHOT;
	}

	projectile->r.solid = SOLID_YES;
	projectile->r.svflags = SVF_PROJECTILE;
	VectorClear( projectile->r.mins );
	VectorClear( projectile->r.maxs );

	//projectile->s.modelindex = trap_ModelIndex ("models/objects/projectile/plasmagun/proj_plasmagun2.md3");
	projectile->s.modelindex = 0;
	projectile->r.owner = self;
	projectile->touch = ForgotToSetProjectileTouch;
	projectile->nextThink = level.time + timeout;
	projectile->think = G_FreeEdict;
	projectile->classname = NULL; // should be replaced after calling this func.
	projectile->s.sound = 0;
	projectile->timeStamp = level.time;
	projectile->timeDelta = timeDelta;
	projectile->s.team = self->s.team;

	projectile->projectileInfo.minDamage = min( minDamage, damage );
	projectile->projectileInfo.maxDamage = damage;
	projectile->projectileInfo.minKnockback = min( minKnockback, maxKnockback );
	projectile->projectileInfo.maxKnockback = maxKnockback;
	projectile->projectileInfo.radius = radius;

	GClip_LinkEntity( projectile );

	return projectile;
}


//	------------ the actual weapons --------------


/*
* W_Fire_Blade
*/
void W_Fire_Blade( edict_t *self, int range, vec3_t start, vec3_t angles, float damage, int knockback, int timeDelta ) {
	edict_t *event, *other = NULL;
	vec3_t end;
	trace_t trace;
	int mask = MASK_SHOT;
	vec3_t dir;
	int dmgflags = 0;

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( start, range, dir, end );

	if( GS_RaceGametype() ) {
		mask = MASK_SOLID;
	}

	G_Trace4D( &trace, start, NULL, NULL, end, self, mask, timeDelta );
	if( trace.ent == -1 ) { //didn't touch anything
		return;
	}

	// find out what touched
	other = &game.edicts[trace.ent];
	if( !other->takedamage ) { // it was the world
		// wall impact
		VectorMA( trace.endpos, -0.02, dir, end );
		event = G_SpawnEvent( EV_BLADE_IMPACT, 0, end );
		event->s.ownerNum = ENTNUM( self );
		VectorScale( trace.plane.normal, 1024, event->s.origin2 );
		return;
	}

	// it was a player
	G_Damage( other, self, self, dir, dir, other->s.origin, damage, knockback, dmgflags, MOD_GUNBLADE_W );
}

/*
* W_Touch_GunbladeBlast
*/
static void W_Touch_GunbladeBlast( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	int hitType = G_Projectile_HitStyle( ent, other );
	if( hitType == PROJECTILE_TOUCH_NOT ) {
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( ENTNUM( other ), ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_GUNBLADE_S );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_GUNBLADE_S );

	// add explosion event
	if( !other->takedamage || ISBRUSHMODEL( other->s.modelindex ) ) {
		edict_t * event = G_SpawnEvent( EV_GUNBLADEBLAST_IMPACT, DirToByte( plane ? plane->normal : NULL ), ent->s.origin );
		event->s.weapon = min( ent->projectileInfo.radius / 8, 127 );
		event->s.skinnum = min( ent->projectileInfo.maxKnockback / 8, 255 );
	}

	G_FreeEdict( ent );
}

/*
* W_Fire_GunbladeBlast
*/
edict_t *W_Fire_GunbladeBlast( edict_t *self, vec3_t start, vec3_t angles, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int speed, int timeout, int timeDelta ) {
	edict_t *blast;

	blast = W_Fire_LinearProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );
	blast->s.modelindex = trap_ModelIndex( PATH_GUNBLADEBLAST_MODEL );
	blast->s.type = ET_BLASTER;
	blast->touch = W_Touch_GunbladeBlast;
	blast->classname = "gunblade_blast";

	blast->s.sound = trap_SoundIndex( S_WEAPON_PLASMAGUN_FLY );
	blast->s.attenuation = ATTN_STATIC;

	return blast;
}
/*
* W_Bullet_Touch
*/
static void W_Bullet_Touch( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	int hitType = G_Projectile_HitStyle( ent, other );
	if( hitType == PROJECTILE_TOUCH_NOT ) {
		return;
	}

	if( other->takedamage ) {
		G_Damage( other, ent, ent->r.owner, ent->velocity, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_GRENADE );
	}

	G_FreeEdict( ent );
}

/*
* W_Fire_Bullet
*/
void W_Fire_Bullet( edict_t *self, vec3_t start, vec3_t angles, int seed, int range, int hspread, int vspread,
					float damage, int knockback, int timeDelta ) {
	edict_t *bullet = W_Fire_TossProjectile( self, start, angles, 3000, 12, 0, 0, 0, 0, 9000, timeDelta );

	bullet->s.type = ET_PLASMA;
	bullet->movetype = MOVETYPE_TOSS;
	bullet->touch = W_Bullet_Touch;
	bullet->use = NULL;
	bullet->think = NULL;
	bullet->classname = "bullet";
	bullet->enemy = NULL;

	bullet->s.modelindex = trap_ModelIndex( PATH_BULLET_MODEL );

	GClip_LinkEntity( bullet );
}

// Sunflower spiral with Fibonacci numbers
static void G_Fire_SunflowerPattern( edict_t *self, vec3_t start, vec3_t dir, int count,
									 int hspread, int vspread, int range, float damage, int kick, int dflags, int timeDelta ) {
	vec3_t right, up;
	ViewVectors( dir, right, up );

	for( int i = 0; i < count; i++ ) {
		float fi = i * 2.4f; //magic value creating Fibonacci numbers
		float r = cosf( fi ) * hspread * sqrtf( fi );
		float u = sinf( fi ) * vspread * sqrtf( fi );

		trace_t trace;
		GS_TraceBullet( &trace, start, dir, right, up, r, u, range, ENTNUM( self ), timeDelta );
		if( trace.ent != -1 && game.edicts[trace.ent].takedamage ) {
			G_Damage( &game.edicts[trace.ent], self, self, dir, dir, trace.endpos, damage, kick, dflags, MOD_RIOTGUN );
		}
	}
}

void W_Fire_Riotgun( edict_t *self, vec3_t start, vec3_t angles, int range, int hspread, int vspread,
					 int count, float damage, int knockback, int timeDelta ) {
	vec3_t dir;
	edict_t *event;
	int dmgflags = 0;

	AngleVectors( angles, dir, NULL, NULL );

	// send the event
	event = G_SpawnEvent( EV_FIRE_RIOTGUN, 0, start );
	event->s.ownerNum = ENTNUM( self );
	VectorScale( dir, 4096, event->s.origin2 ); // DirToByte is too inaccurate
	event->s.weapon = WEAP_RIOTGUN;

	G_Fire_SunflowerPattern( self, start, dir, count, hspread, vspread,
		range, damage, knockback, dmgflags, timeDelta );
}

/*
* W_Grenade_ExplodeDir
*/
static void W_Grenade_ExplodeDir( edict_t *ent, vec3_t normal ) {
	vec3_t origin;
	int radius;
	edict_t *event;
	vec3_t up = { 0, 0, 1 };
	vec_t *dir = normal ? normal : up;

	G_RadiusDamage( ent, ent->r.owner, NULL, ent->enemy, MOD_GRENADE_SPLASH );

	radius = ( ( ent->projectileInfo.radius * 1 / 8 ) > 127 ) ? 127 : ( ent->projectileInfo.radius * 1 / 8 );
	VectorMA( ent->s.origin, -0.02, ent->velocity, origin );
	event = G_SpawnEvent( EV_GRENADE_EXPLOSION, ( dir ? DirToByte( dir ) : 0 ), ent->s.origin );
	event->s.weapon = radius;

	G_FreeEdict( ent );
}

/*
* W_Grenade_Explode
*/
static void W_Grenade_Explode( edict_t *ent ) {
	W_Grenade_ExplodeDir( ent, NULL );
}

/*
* W_Touch_Grenade
*/
static void W_Touch_Grenade( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	int hitType = G_Projectile_HitStyle( ent, other );
	if( hitType == PROJECTILE_TOUCH_NOT ) {
		return;
	}

	// don't explode on doors and plats that take damage
	if( !other->takedamage || ISBRUSHMODEL( other->s.modelindex ) ) {
		G_AddEvent( ent, EV_GRENADE_BOUNCE, 0, true );
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( ENTNUM( other ), ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_GRENADE );
	}

	ent->enemy = other;
	W_Grenade_ExplodeDir( ent, plane ? plane->normal : NULL );
}

/*
* W_Fire_Grenade
*/
edict_t *W_Fire_Grenade( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage,
						 int minKnockback, int maxKnockback, int minDamage, float radius,
						 int timeout, int timeDelta, bool aim_up ) {
	edict_t *grenade;

	if( aim_up ) {
		angles[PITCH] -= 5.0f * cos( DEG2RAD( angles[PITCH] ) ); // aim some degrees upwards from view dir

	}
	grenade = W_Fire_TossProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );
	VectorClear( grenade->s.angles );
	grenade->s.type = ET_GRENADE;
	grenade->movetype = MOVETYPE_BOUNCEGRENADE;
	grenade->touch = W_Touch_Grenade;
	grenade->use = NULL;
	grenade->think = W_Grenade_Explode;
	grenade->classname = "grenade";
	grenade->enemy = NULL;
	VectorSet( grenade->avelocity, 300, 300, 300 );

	grenade->s.modelindex = trap_ModelIndex( PATH_GRENADE_MODEL );

	GClip_LinkEntity( grenade );

	return grenade;
}

/*
* W_Touch_Rocket
*/
static void W_Touch_Rocket( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	int hitType = G_Projectile_HitStyle( ent, other );
	if( hitType == PROJECTILE_TOUCH_NOT ) {
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( ENTNUM( other ), ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, 0, MOD_ROCKET );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_ROCKET_SPLASH );

	// spawn the explosion
	vec3_t explosion_origin;
	VectorMA( ent->s.origin, -0.02, ent->velocity, explosion_origin );

	edict_t *event = G_SpawnEvent( EV_ROCKET_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), explosion_origin );
	event->s.weapon = min( ent->projectileInfo.radius / 8, 255 );

	G_FreeEdict( ent );
}

/*
* W_Fire_Rocket
*/
edict_t *W_Fire_Rocket( edict_t *self, vec3_t start, vec3_t angles, int speed, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int timeout, int timeDelta ) {
	edict_t *rocket;

	rocket = W_Fire_LinearProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );

	rocket->s.type = ET_ROCKET; //rocket trail sfx
	rocket->s.modelindex = trap_ModelIndex( PATH_ROCKET_MODEL );
	rocket->s.sound = trap_SoundIndex( S_WEAPON_ROCKET_FLY );
	rocket->s.attenuation = ATTN_STATIC;
	rocket->touch = W_Touch_Rocket;
	rocket->think = G_FreeEdict;
	rocket->classname = "rocket";

	return rocket;
}

/*
* W_Touch_Plasma
*/
static void W_Touch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	if( surfFlags & SURF_NOIMPACT ) {
		G_FreeEdict( ent );
		return;
	}

	int hitType = G_Projectile_HitStyle( ent, other );
	if( hitType == PROJECTILE_TOUCH_NOT ) {
		return;
	}

	if( other->takedamage ) {
		vec3_t push_dir;
		G_SplashFrac4D( ENTNUM( other ), ent->s.origin, ent->projectileInfo.radius, push_dir, NULL, ent->timeDelta );
		G_Damage( other, ent, ent->r.owner, push_dir, ent->velocity, ent->s.origin, ent->projectileInfo.maxDamage, ent->projectileInfo.maxKnockback, DAMAGE_KNOCKBACK_SOFT, MOD_PLASMA );
	}

	G_RadiusDamage( ent, ent->r.owner, plane, other, MOD_PLASMA );

	edict_t *event = G_SpawnEvent( EV_PLASMA_EXPLOSION, DirToByte( plane ? plane->normal : NULL ), ent->s.origin );
	event->s.weapon = min( ent->projectileInfo.radius / 8, 127 );
	event->s.team = ent->s.team;

	G_FreeEdict( ent );
}

/*
* W_Plasma_Backtrace
*/
void W_Plasma_Backtrace( edict_t *ent, const vec3_t start ) {
	trace_t tr;
	vec3_t oldorigin;
	vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };

	if( GS_RaceGametype() ) {
		return;
	}

	VectorCopy( ent->s.origin, oldorigin );
	VectorCopy( start, ent->s.origin );

	do {
		G_Trace4D( &tr, ent->s.origin, mins, maxs, oldorigin, ent, ( CONTENTS_BODY | CONTENTS_CORPSE ), ent->timeDelta );

		VectorCopy( tr.endpos, ent->s.origin );

		if( tr.ent == -1 ) {
			break;
		}
		if( tr.allsolid || tr.startsolid ) {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], NULL, 0 );
		} else if( tr.fraction != 1.0 ) {
			W_Touch_Plasma( ent, &game.edicts[tr.ent], &tr.plane, tr.surfFlags );
		} else {
			break;
		}
	} while( ent->r.inuse && ent->s.type == ET_PLASMA && !VectorCompare( ent->s.origin, oldorigin ) );

	if( ent->r.inuse && ent->s.type == ET_PLASMA ) {
		VectorCopy( oldorigin, ent->s.origin );
	}
}

/*
* W_Think_Plasma
*/
static void W_Think_Plasma( edict_t *ent ) {
	vec3_t start;

	if( ent->timeout < level.time ) {
		G_FreeEdict( ent );
		return;
	}

	if( ent->r.inuse ) {
		ent->nextThink = level.time + 1;
	}

	VectorMA( ent->s.origin, -( game.frametime * 0.001 ), ent->velocity, start );

	W_Plasma_Backtrace( ent, start );
}

/*
* W_AutoTouch_Plasma
*/
static void W_AutoTouch_Plasma( edict_t *ent, edict_t *other, cplane_t *plane, int surfFlags ) {
	W_Think_Plasma( ent );
	if( !ent->r.inuse || ent->s.type != ET_PLASMA ) {
		return;
	}

	W_Touch_Plasma( ent, other, plane, surfFlags );
}

/*
* W_Fire_Plasma
*/
edict_t *W_Fire_Plasma( edict_t *self, vec3_t start, vec3_t angles, float damage, int minKnockback, int maxKnockback, int minDamage, int radius, int speed, int timeout, int timeDelta ) {
	edict_t *plasma;

	plasma = W_Fire_LinearProjectile( self, start, angles, speed, damage, minKnockback, maxKnockback, minDamage, radius, timeout, timeDelta );
	plasma->s.type = ET_PLASMA;
	plasma->classname = "plasma";

	plasma->think = W_Think_Plasma;
	plasma->touch = W_AutoTouch_Plasma;
	plasma->nextThink = level.time + 1;
	plasma->timeout = level.time + timeout;

	plasma->s.modelindex = trap_ModelIndex( PATH_PLASMA_MODEL );
	plasma->s.sound = trap_SoundIndex( S_WEAPON_PLASMAGUN_FLY );
	plasma->s.attenuation = ATTN_STATIC;
	return plasma;
}

void W_Fire_Electrobolt_FullInstant( edict_t *self, vec3_t start, vec3_t angles, float maxdamage, float mindamage, int maxknockback, int minknockback, int range, int minDamageRange, int timeDelta ) {
	vec3_t from, end, dir;
	trace_t tr;
	edict_t *ignore, *event, *hit;
	int hit_movetype;
	int mask;
	int dmgflags = 0;

#define FULL_DAMAGE_RANGE g_projectile_prestep->value

	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( start, range, dir, end );
	VectorCopy( start, from );

	ignore = self;
	hit = NULL;

	mask = MASK_SHOT;
	if( GS_RaceGametype() ) {
		mask = MASK_SOLID;
	}

	clamp_high( mindamage, maxdamage );
	clamp_high( minknockback, maxknockback );
	clamp_high( minDamageRange, range );

	if( minDamageRange <= FULL_DAMAGE_RANGE ) {
		minDamageRange = FULL_DAMAGE_RANGE + 1;
	}

	if( range <= FULL_DAMAGE_RANGE + 1 ) {
		range = FULL_DAMAGE_RANGE + 1;
	}

	tr.ent = -1;
	while( ignore ) {
		G_Trace4D( &tr, from, NULL, NULL, end, ignore, mask, timeDelta );

		VectorCopy( tr.endpos, from );
		ignore = NULL;

		if( tr.ent == -1 ) {
			break;
		}

		// some entity was touched
		hit = &game.edicts[tr.ent];
		hit_movetype = hit->movetype; // backup the original movetype as the entity may "die"
		if( hit == world ) { // stop dead if hit the world
			break;
		}

		// allow trail to go through BBOX entities (players, gibs, etc)
		if( !ISBRUSHMODEL( hit->s.modelindex ) ) {
			ignore = hit;
		}

		if( ( hit != self ) && ( hit->takedamage ) ) {
			float frac, damage, knockback, dist;

			dist = DistanceFast( tr.endpos, start );
			if( dist <= FULL_DAMAGE_RANGE ) {
				frac = 0.0f;
			} else {
				frac = ( dist - FULL_DAMAGE_RANGE ) / (float)( minDamageRange - FULL_DAMAGE_RANGE );
				clamp( frac, 0.0f, 1.0f );
			}

			damage = maxdamage - ( ( maxdamage - mindamage ) * frac );
			knockback = maxknockback - ( ( maxknockback - minknockback ) * frac );

			G_Damage( hit, self, self, dir, dir, tr.endpos, damage, knockback, dmgflags, MOD_ELECTROBOLT );

			// spawn a impact event on each damaged ent
			event = G_SpawnEvent( EV_BOLT_EXPLOSION, DirToByte( tr.plane.normal ), tr.endpos );
		}

		if( hit_movetype == MOVETYPE_NONE || hit_movetype == MOVETYPE_PUSH ) {
			break;
		}
	}

	// send the weapon fire effect
	event = G_SpawnEvent( EV_ELECTROTRAIL, ENTNUM( self ), start );
	VectorScale( dir, 1024, event->s.origin2 );

#undef FULL_DAMAGE_RANGE
}

/*
* G_HideLaser
*/
void G_HideLaser( edict_t *ent ) {
	ent->s.modelindex = 0;
	ent->s.sound = 0;
	ent->r.svflags = SVF_NOCLIENT;

	// give it 100 msecs before freeing itself, so we can relink it if we start firing again
	ent->think = G_FreeEdict;
	ent->nextThink = level.time + 100;
}

/*
* G_Laser_Think
*/
static void G_Laser_Think( edict_t *ent ) {
	edict_t *owner;

	if( ent->s.ownerNum < 1 || ent->s.ownerNum > gs.maxclients ) {
		G_FreeEdict( ent );
		return;
	}

	owner = &game.edicts[ent->s.ownerNum];

	if( G_ISGHOSTING( owner ) || owner->s.weapon != WEAP_LASERGUN ||
		trap_GetClientState( PLAYERNUM( owner ) ) < CS_SPAWNED ||
		( owner->r.client->ps.weaponState != WEAPON_STATE_REFIRESTRONG
		  && owner->r.client->ps.weaponState != WEAPON_STATE_REFIRE ) ) {
		G_HideLaser( ent );
		return;
	}

	ent->nextThink = level.time + 1;
}

static float laser_damage;
static int laser_knockback;
static int laser_attackerNum;

static void _LaserImpact( trace_t *trace, vec3_t dir ) {
	edict_t *attacker;

	if( !trace || trace->ent <= 0 ) {
		return;
	}
	if( trace->ent == laser_attackerNum ) {
		return; // should not be possible theoretically but happened at least once in practice

	}
	attacker = &game.edicts[laser_attackerNum];

	if( game.edicts[trace->ent].takedamage ) {
		G_Damage( &game.edicts[trace->ent], attacker, attacker, dir, dir, trace->endpos, laser_damage, laser_knockback, DAMAGE_KNOCKBACK_SOFT, MOD_LASERGUN );
	}
}

static edict_t *_FindOrSpawnLaser( edict_t *owner, int entType, bool *newLaser ) {
	int i, ownerNum;
	edict_t *e, *laser;

	// first of all, see if we already have a beam entity for this laser
	*newLaser = false;
	laser = NULL;
	ownerNum = ENTNUM( owner );
	for( i = gs.maxclients + 1; i < game.maxentities; i++ ) {
		e = &game.edicts[i];
		if( !e->r.inuse ) {
			continue;
		}

		if( e->s.ownerNum == ownerNum && e->s.type == ET_LASERBEAM ) {
			laser = e;
			break;
		}
	}

	// if no ent was found we have to create one
	if( !laser || laser->s.type != entType || !laser->s.modelindex ) {
		if( !laser ) {
			*newLaser = true;
			laser = G_Spawn();
		}

		laser->s.type = entType;
		laser->s.ownerNum = ownerNum;
		laser->movetype = MOVETYPE_NONE;
		laser->r.solid = SOLID_NOT;
		laser->s.modelindex = 255; // needs to have some value so it isn't filtered by the server culling
		laser->r.svflags &= ~SVF_NOCLIENT;
	}

	return laser;
}

/*
* W_Fire_Lasergun
*/
edict_t *W_Fire_Lasergun( edict_t *self, vec3_t start, vec3_t angles, float damage, int knockback, int range, int timeDelta ) {
	edict_t *laser;
	bool newLaser;
	trace_t tr;
	vec3_t dir;

	laser = _FindOrSpawnLaser( self, ET_LASERBEAM, &newLaser );
	if( newLaser ) {
		// the quad start sound is added from the server
		if( self->r.client && self->r.client->ps.inventory[POWERUP_QUAD] > 0 ) {
			G_Sound( self, CHAN_AUTO, trap_SoundIndex( S_QUAD_FIRE ), ATTN_NORM );
		}
	}

	laser_damage = damage;
	laser_knockback = knockback;
	laser_attackerNum = ENTNUM( self );

	GS_TraceLaserBeam( &tr, start, angles, range, ENTNUM( self ), timeDelta, _LaserImpact );

	laser->r.svflags |= SVF_FORCEOWNER;
	VectorCopy( start, laser->s.origin );
	AngleVectors( angles, dir, NULL, NULL );
	VectorMA( laser->s.origin, range, dir, laser->s.origin2 );

	laser->think = G_Laser_Think;
	laser->nextThink = level.time + 100;

	// calculate laser's mins and maxs for linkEntity
	G_SetBoundsForSpanEntity( laser, 8 );

	GClip_LinkEntity( laser );

	return laser;
}
