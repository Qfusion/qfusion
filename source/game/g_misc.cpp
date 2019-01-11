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

// g_misc.c

#include "g_local.h"


//=====================================================

static void VelocityForDamage( int damage, vec3_t v ) {
	v[0] = 10.0 * crandom();
	v[1] = 10.0 * crandom();
	v[2] = 20.0 + 10.0 * random();

	VectorNormalizeFast( v );

	if( damage < 50 ) {
		VectorScale( v, 0.7, v );
	} else {
		VectorScale( v, 1.2, v );
	}
}

void ThrowSmallPileOfGibs( edict_t *self, int damage ) {
	vec3_t origin;
	edict_t *event;
	int contents;
	int i;

	contents = G_PointContents( self->s.origin );
	if( contents & CONTENTS_NODROP ) {
		return;
	}

	for( i = 0; i < 3; i++ )
		origin[i] = self->s.origin[i];

	self->s.origin[2] += 4;

	// clamp the damage value since events do bitwise & 0xFF on the passed param
	damage = bound( 0, damage, 255 );

	event = G_SpawnEvent( EV_SPOG, damage, origin );
	event->s.team = self->s.team;
	VectorCopy( self->velocity, event->s.origin2 );
}

void ThrowClientHead( edict_t *self, int damage ) {
	vec3_t vd;

	self->s.modelindex = 1;
	self->s.modelindex2 = 0;
	self->s.skinnum = 0;

	self->s.origin[2] += 32;
	self->s.frame = 0;

	VectorSet( self->r.mins, -16, -16, 0 );
	VectorSet( self->r.maxs, 16, 16, 16 );

	self->takedamage = DAMAGE_NO;
	self->r.solid = SOLID_NOT;
	self->s.type = ET_GIB;
	self->s.sound = 0;
	self->s.effects = 0;
	self->flags |= FL_NO_KNOCKBACK;

	self->movetype = MOVETYPE_BOUNCE;
	VelocityForDamage( max( damage, 50 ), vd );
	VectorAdd( self->velocity, vd, self->velocity );

	G_AddEvent( self, EV_GIB, 0, false );
	GClip_LinkEntity( self );
}

/*
* debris
*/
static void debris_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point ) {
	G_FreeEdict( self );
}

void ThrowDebris( edict_t *self, int modelindex, float speed, vec3_t origin ) {
	edict_t *chunk;
	vec3_t v;

	chunk = G_Spawn();
	VectorCopy( origin, chunk->s.origin );
	chunk->r.svflags &= ~SVF_NOCLIENT;
	chunk->s.modelindex = modelindex;
	v[0] = 100 * crandom();
	v[1] = 100 * crandom();
	v[2] = 100 + 100 * crandom();
	VectorMA( self->velocity, speed, v, chunk->velocity );
	chunk->movetype = MOVETYPE_BOUNCE;
	chunk->r.solid = SOLID_NOT;
	chunk->avelocity[0] = random() * 600;
	chunk->avelocity[1] = random() * 600;
	chunk->avelocity[2] = random() * 600;
	chunk->think = G_FreeEdict;
	chunk->nextThink = level.time + 5000 + random() * 5000;
	chunk->s.frame = 0;
	chunk->flags = 0;
	chunk->classname = "debris";
	chunk->takedamage = DAMAGE_YES;
	chunk->die = debris_die;
	chunk->r.owner = self;
	GClip_LinkEntity( chunk );
}


void BecomeExplosion1( edict_t *self ) {
	int radius;

	// turn entity into event
	if( self->projectileInfo.radius > 255 * 8 ) {
		radius = ( self->projectileInfo.radius * 1 / 16 ) & 0xFF;
		if( radius < 1 ) {
			radius = 1;
		}

		G_MorphEntityIntoEvent( self, EV_EXPLOSION2, radius );
	} else {
		radius = ( self->projectileInfo.radius * 1 / 8 ) & 0xFF;
		if( radius < 1 ) {
			radius = 1;
		}

		G_MorphEntityIntoEvent( self, EV_EXPLOSION1, radius );
	}

	self->r.svflags &= ~SVF_NOCLIENT;
}


//QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT
//Path corner entity that func_trains can be made to follow.
//-------- KEYS --------
//target : point to next path_corner in the path.
//targetname : the train following the path or the previous path_corner in the path points to this.
//pathtarget: gets used when an entity that has this path_corner targeted touches it
//speed : speed of func_train while moving to the next path corner. This will override the speed value of the train.
//wait : number of seconds func_train will pause on path corner before moving to next path corner (default 0 - see Notes).
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- SPAWNFLAGS --------
//TELEPORT : &1 instant move to next target
//-------- NOTES --------
//Setting the wait key to -1 will not make the train stop on the path corner, it will simply default to 0.

static void path_corner_touch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	vec3_t v;
	edict_t *next;

	if( other->movetarget != self ) {
		return;
	}
	if( other->enemy ) {
		return;
	}

	if( self->pathtarget ) {
		const char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		G_UseTargets( self, other );
		self->target = savetarget;
	}

	if( self->target ) {
		next = G_PickTarget( self->target );
	} else {
		next = NULL;
	}

	if( next && ( next->spawnflags & 1 ) ) {
		VectorCopy( next->s.origin, v );
		v[2] += next->r.mins[2];
		v[2] -= other->r.mins[2];
		VectorCopy( v, other->s.origin );
		next = G_PickTarget( next->target );
		other->s.teleported = true;
	}

	other->goalentity = other->movetarget = next;

	VectorSubtract( other->goalentity->s.origin, other->s.origin, v );
}

void SP_path_corner( edict_t *self ) {
	if( !self->targetname ) {
		if( developer->integer ) {
			G_Printf( "path_corner with no targetname at %s\n", vtos( self->s.origin ) );
		}
		G_FreeEdict( self );
		return;
	}

	self->r.solid = SOLID_TRIGGER;
	self->touch = path_corner_touch;
	VectorSet( self->r.mins, -8, -8, -8 );
	VectorSet( self->r.maxs, 8, 8, 8 );
	self->r.svflags |= SVF_NOCLIENT;
	GClip_LinkEntity( self );
}

//QUAKED info_null (0 .5 0) (-8 -8 -8) (8 8 8)
//Used as a positional target for light entities to create a spotlight effect. removed during gameplay.
//-------- KEYS --------
//targetname : must match the target key of entity that uses this for pointing.
//-------- NOTES --------
//A target_position can be used instead of this but was kept in for legacy purposes.
void SP_info_null( edict_t *self ) {
	G_FreeEdict( self );
}


//QUAKED info_notnull (0 .5 0) (-8 -8 -8) (8 8 8)
//Used as a positional target for entities that can use directional pointing. Kept during gameplay.
//-------- KEYS --------
//targetname : must match the target key of entity that uses this for pointing.
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- NOTES --------
//A target_position can be used instead of this but was kept in for legacy purposes.
void SP_info_notnull( edict_t *self ) {
	VectorCopy( self->s.origin, self->r.absmin );
	VectorCopy( self->s.origin, self->r.absmax );
}

#define START_OFF   64

//========================================================
//
//	FUNC_*
//
//========================================================

//QUAKED func_group (0 0 0) ?
//Used to group brushes together just for editor convenience.

//===========================================================

//QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON - -
//This is just a solid wall if not inhibited. Can be used for conditional walls.
//-------- KEYS --------
//target : activate entities with this targetname when used
//targetname : use this name to target me
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- SPAWNFLAGS --------
//TRIGGER_SPAWN : &1 the wall will not be present until triggered it will then blink in to existance; it will kill anything that was in its way
//TOGGLE : &2 only valid for TRIGGER_SPAWN walls this allows the wall to be turned on and off
//START_ON : &4 only valid for TRIGGER_SPAWN walls the wall will initially be present
//-------- NOTES --------
//Untested entity

static void func_wall_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->r.solid == SOLID_NOT ) {
		self->r.solid = SOLID_YES;
		self->r.svflags &= ~SVF_NOCLIENT;
		KillBox( self );
	} else {
		self->r.solid = SOLID_NOT;
		self->r.svflags |= SVF_NOCLIENT;
	}
	GClip_LinkEntity( self );

	if( !( self->spawnflags & 2 ) ) {
		self->use = NULL;
	}
}

void SP_func_wall( edict_t *self ) {
	G_InitMover( self );
	self->s.solid = SOLID_NOT;

	// just a wall
	if( ( self->spawnflags & 7 ) == 0 ) {
		self->r.solid = SOLID_YES;
		GClip_LinkEntity( self );
		return;
	}

	// it must be TRIGGER_SPAWN
	if( !( self->spawnflags & 1 ) ) {
		//		G_Printf ("func_wall missing TRIGGER_SPAWN\n");
		self->spawnflags |= 1;
	}

	// yell if the spawnflags are odd
	if( self->spawnflags & 4 ) {
		if( !( self->spawnflags & 2 ) ) {
			if( developer->integer ) {
				G_Printf( "func_wall START_ON without TOGGLE\n" );
			}
			self->spawnflags |= 2;
		}
	}

	self->use = func_wall_use;
	if( self->spawnflags & 4 ) {
		self->r.solid = SOLID_YES;
	} else {
		self->r.solid = SOLID_NOT;
		self->r.svflags |= SVF_NOCLIENT;
	}
	GClip_LinkEntity( self );
}

//===========================================================

//QUAKED func_static (0 .5 .8) ?
//Static non-solid bspmodel. Can be used for conditional walls and models.
//-------- KEYS --------
//model2 : path/name of model to include (eg: models/mapobjects/bitch/fembotbig.md3).
//origin : alternate method of setting XYZ origin of .md3 model included with entity (See Notes).
//light : constantLight radius of .md3 model included with entity. Has no effect on the entity's brushes (default 0).
//color : constantLight color of .md3 model included with entity. Has no effect on the entity's brushes (default 1 1 1).
//targetname : NOT SUPPORTED BY RENDERER - if set, a func_button or trigger can make entity disappear from the game (See Notes).
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- NOTES --------
//When using the model2 key, the origin point of the model will correspond to the origin point defined by either the origin brush or the origin coordinate value. If a model is included with a targeted func_static, the brush(es) of the entity will be removed from the game but the .md3 model won't: it will automatically be moved to the (0 0 0) world origin so you should NOT include an .md3 model to a targeted func_static.

void SP_func_static( edict_t *ent ) {
	G_InitMover( ent );
	ent->movetype = MOVETYPE_NONE;
	ent->r.svflags = SVF_BROADCAST;
	GClip_LinkEntity( ent );
}

//===========================================================

//QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN - -
//This is solid bmodel that will fall if its support it removed.
//-------- KEYS --------
//origin : alternate method of setting XYZ origin of .md3 model included with entity (See Notes).
//light : constantLight radius of .md3 model included with entity. Has no effect on the entity's brushes (default 0).
//color : constantLight color of .md3 model included with entity. Has no effect on the entity's brushes (default 1 1 1).
//target : fire targets with this name at being used.
//targetname : name to be targeted with.
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- SPAWNFLAGS --------
//TRIGGER_SPAWN : &1 spawn this entity when target is fired
//-------- NOTES --------
//model2 is not supported in func_objects, only map brushes can be safely used as model

static void func_object_touch( edict_t *self, edict_t *other, cplane_t *plane, int surfFlags ) {
	// only squash thing we fall on top of
	if( !plane ) {
		return;
	}
	if( plane->normal[2] < 1.0 ) {
		return;
	}
	if( other->takedamage == DAMAGE_NO ) {
		return;
	}

	G_Damage( other, self, self, vec3_origin, vec3_origin, self->s.origin, self->dmg, 1, 0, MOD_CRUSH );
}

static void func_object_release( edict_t *self ) {
	self->movetype = MOVETYPE_TOSS;
	self->touch = func_object_touch;
}

static void func_object_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->r.solid = SOLID_YES;
	self->r.svflags &= ~SVF_NOCLIENT;
	self->use = NULL;
	KillBox( self );
	func_object_release( self );
}

void SP_func_object( edict_t *self ) {
	G_InitMover( self );

	self->r.mins[0] += 1;
	self->r.mins[1] += 1;
	self->r.mins[2] += 1;
	self->r.maxs[0] -= 1;
	self->r.maxs[1] -= 1;
	self->r.maxs[2] -= 1;

	if( !self->dmg ) {
		self->dmg = 100;
	}

	if( self->spawnflags == 0 ) {
		self->r.solid = SOLID_YES;
		self->movetype = MOVETYPE_PUSH;
		self->think = func_object_release;
		self->nextThink = level.time + self->wait * 1000;
		self->r.svflags &= ~SVF_NOCLIENT;
	} else {
		self->r.solid = SOLID_NOT;
		self->movetype = MOVETYPE_PUSH;
		self->use = func_object_use;
		self->r.svflags |= SVF_NOCLIENT;
	}

	self->r.clipmask = MASK_MONSTERSOLID;

	GClip_LinkEntity( self );
}

//===========================================================

//QUAKED func_explosive (0 .5 .8) ? TRIGGER_SPAWN - -
//Any brush that you want to explode or break apart.  If you want an
//explosion, set dmg and it will do a radius explosion of that amount
//at the center of the bursh. If targeted it will not be shootable.
//-------- KEYS --------
//health : defaults to 100.
//mass : defaults to 75.  This determines how much debris is emitted when it explodes.  You get one large chunk per 100 of mass (up to 8) and one small chunk per 25 of mass (up to 16).  So 800 gives the most.
//model2 : a md3 model.
//origin : alternate method of setting XYZ origin of .md3 model included with entity (See Notes).
//light : constantLight radius of .md3 model included with entity. Has no effect on the entity's brushes (default 0).
//color : constantLight color of .md3 model included with entity. Has no effect on the entity's brushes (default 1 1 1).
//target : fire targets with this name at being used.
//targetname : name to be targeted with.
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- SPAWNFLAGS --------
//TRIGGER_SPAWN : &1 spawn this entity when target is fired
//-------- NOTES --------
//Untested: model2 models might not collide perfectly if used with a brush origin

static void func_explosive_explode( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point ) {
	vec3_t origin, bakorigin;
	vec3_t chunkorigin;
	vec3_t size;
	int count;
	int mass;

	// do not explode unless visible
	if( self->r.svflags & SVF_NOCLIENT ) {
		return;
	}

	self->takedamage = DAMAGE_NO;

	// bmodel origins are (0 0 0), we need to adjust that here
	VectorCopy( self->s.origin, bakorigin );
	VectorScale( self->r.size, 0.5, size );
	VectorAdd( self->r.absmin, size, origin );
	VectorCopy( origin, self->s.origin );

	if( self->projectileInfo.maxDamage ) {
		G_RadiusDamage( self, attacker, NULL, NULL, MOD_EXPLOSIVE );
	}

	VectorSubtract( self->s.origin, inflictor->s.origin, self->velocity );
	VectorNormalize( self->velocity );
	VectorScale( self->velocity, 150, self->velocity );

	// start chunks towards the center
	VectorScale( size, 0.5, size );
	mass = self->projectileInfo.radius * 0.75;
	if( !mass ) {
		mass = 75;
	}

	// big chunks
	if( self->count > 0 ) {
		if( mass >= 100 ) {
			count = mass / 100;
			if( count > 8 ) {
				count = 8;
			}
			while( count-- ) {
				chunkorigin[0] = origin[0] + crandom() * size[0];
				chunkorigin[1] = origin[1] + crandom() * size[1];
				chunkorigin[2] = origin[2] + crandom() * size[2];
				ThrowDebris( self, self->count, 1, chunkorigin );
			}
		}
	}

	// small chunks
	if( self->viewheight > 0 ) {
		count = mass / 25;
		if( count > 16 ) {
			count = 16;
		}
		if( count < 1 ) {
			count = 1;
		}
		while( count-- ) {
			chunkorigin[0] = origin[0] + crandom() * size[0];
			chunkorigin[1] = origin[1] + crandom() * size[1];
			chunkorigin[2] = origin[2] + crandom() * size[2];
			ThrowDebris( self, self->viewheight, 2, chunkorigin );
		}
	}

	G_UseTargets( self, attacker );

	if( self->projectileInfo.maxDamage ) {
		edict_t *explosion;

		explosion = G_Spawn();
		VectorCopy( self->s.origin, explosion->s.origin );
		explosion->projectileInfo = self->projectileInfo;
		BecomeExplosion1( explosion );
	}

	if( self->use == NULL ) {
		G_FreeEdict( self );
		return;
	}

	self->health = self->max_health;
	self->r.solid = SOLID_NOT;
	self->r.svflags |= SVF_NOCLIENT;
	VectorCopy( bakorigin, self->s.origin );
	VectorClear( self->velocity );
	GClip_LinkEntity( self );
}

static void func_explosive_think( edict_t *self ) {
	func_explosive_explode( self, self, self->enemy, self->count, vec3_origin );
}

static void func_explosive_use( edict_t *self, edict_t *other, edict_t *activator ) {
	self->enemy = other;
	self->count = ceil( self->health );

	if( self->delay ) {
		self->think = func_explosive_think;
		self->nextThink = level.time + self->delay * 1000;
		return;
	}

	func_explosive_explode( self, self, other, self->count, vec3_origin );
}

static void func_explosive_spawn( edict_t *self, edict_t *other, edict_t *activator ) {
	self->r.solid = SOLID_YES;
	self->r.svflags &= ~SVF_NOCLIENT;
	self->use = NULL;
	KillBox( self );
	GClip_LinkEntity( self );
}

void SP_func_explosive( edict_t *self ) {
	G_InitMover( self );

	self->projectileInfo.maxDamage = max( self->dmg, 1 );
	self->projectileInfo.minDamage = min( self->dmg, 1 );
	self->projectileInfo.maxKnockback = self->projectileInfo.maxDamage;
	self->projectileInfo.minKnockback = self->projectileInfo.minDamage;
	self->projectileInfo.radius = st.radius;
	if( !self->projectileInfo.radius ) {
		self->projectileInfo.radius = self->dmg + 100;
	}

	if( self->spawnflags & 1 ) {
		self->r.svflags |= SVF_NOCLIENT;
		self->r.solid = SOLID_NOT;
		self->use = func_explosive_spawn;
	} else {
		if( self->targetname ) {
			self->use = func_explosive_use;
		}
	}

	if( self->use != func_explosive_use ) {
		if( !self->health ) {
			self->health = 100;
		}
		self->die = func_explosive_explode;
		self->takedamage = DAMAGE_YES;
	}
	self->max_health = self->health;

	// HACK HACK HACK
	if( st.debris1 && st.debris1[0] ) {
		self->count = trap_ModelIndex( st.debris1 );
	}
	if( st.debris2 && st.debris2[0] ) {
		self->viewheight = trap_ModelIndex( st.debris2 );
	}

	GClip_LinkEntity( self );
}

//========================================================
//
//	MISC_*
//
//========================================================

//QUAKED misc_teleporter_dest (1 .5 .25) (-32 -32 -24) (32 32 -16)
//Teleport destination location point for trigger_teleporter entities.
//-------- KEYS --------
//angle : direction in which player will look when teleported.
//targetname : make the trigger_teleporter point to this.
//notsingle : when set to 1, entity will not spawn in Single Player mode
//notfree : when set to 1, entity will not spawn in "Free for all" and "Tournament" modes.
//notduel : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes. (jaltodo)
//notteam : when set to 1, entity will not spawn in "Teamplay" and "CTF" modes.
//-------- NOTES --------
//Does the same thing as info_teleport_destination
void SP_misc_teleporter_dest( edict_t *ent ) {
	//ent->s.origin[2] += 16;
}

//=====================================================

//QUAKED misc_model (1 .5 .25) (-16 -16 -16) (16 16 16)
//Generic placeholder for inserting .md3 models in game. Requires compilation of map geometry to be added to level.
//-------- KEYS --------
//angle: direction in which model will be oriented.
//model : path/name of model to use (eg: models/mapobjects/teleporter/teleporter.md3).
void SP_misc_model( edict_t *ent ) {
	G_FreeEdict( ent );
}

//=====================================================

void SP_misc_particles_finish( edict_t *ent ) {
	// if it has a target, look towards it
	if( ent->target ) {
		vec3_t dir;
		edict_t *target = G_PickTarget( ent->target );
		if( target ) {
			VectorSubtract( target->s.origin, ent->s.origin, dir );
			VecToAngles( dir, ent->s.angles );
		}
	}

	ent->think = NULL;
}

void SP_misc_particles_use( edict_t *self, edict_t *other, edict_t *activator ) {
	if( self->r.svflags & SVF_NOCLIENT ) {
		self->r.svflags &= ~SVF_NOCLIENT;
	} else {
		self->r.svflags |= SVF_NOCLIENT;
	}

}

//QUAKED misc_particles (.6 .7 .7) (-8 -8 -8) (8 8 8) SPHERICAL SOLID GRAVITY LIGHT EXPAND_EFFECT SHRINK_EFFECT START_OFF
//-------- KEYS --------
//angles: direction in which particles will be thrown.
//shader : particleShader
void SP_misc_particles( edict_t *ent ) {
	ent->r.svflags &= ~SVF_NOCLIENT;
	ent->r.svflags |= SVF_BROADCAST;
	ent->r.solid = SOLID_NOT;
	ent->s.type = ET_PARTICLES;

	if( st.noise ) {
		ent->s.sound = trap_SoundIndex( st.noise );
		G_PureSound( st.noise );
	}

	if( st.gameteam >= TEAM_ALPHA && st.gameteam < GS_MAX_TEAMS ) {
		ent->s.team = st.gameteam;
	} else {
		ent->s.team = 0;
	}

	if( ent->speed > 0 ) {
		ent->particlesInfo.speed = ( (int)ent->speed ) & 255;
	}

	if( ent->count > 0 ) {
		ent->particlesInfo.frequency = ent->count & 255;
	}

	if( st.shaderName ) {
		ent->particlesInfo.shaderIndex = trap_ImageIndex( st.shaderName );
	} else {
		ent->particlesInfo.shaderIndex = trap_ImageIndex( "particle" );
	}

	if( st.size ) {
		ent->particlesInfo.size = st.size & 255;
	} else {
		ent->particlesInfo.size = 16;
	}

	ent->particlesInfo.time = ent->delay;
	if( !ent->particlesInfo.time ) {
		ent->particlesInfo.time = 4;
	}

	if( ent->spawnflags & 1 ) { // SPHERICAL
		ent->particlesInfo.spherical = true;
	}

	if( ent->spawnflags & 2 ) { // BOUNCE
		ent->particlesInfo.bounce = true;
	}

	if( ent->spawnflags & 4 ) { // GRAVITY
		ent->particlesInfo.gravity = true;
	}

	if( ent->spawnflags & 8 ) { // LIGHT
		ent->s.light = COLOR_RGB( (uint8_t)( ent->color[0] * 255 ), (uint8_t)( ent->color[1] * 255 ), (uint8_t)( ent->color[2] * 255 ) );
		if( !ent->s.light ) {
			ent->s.light = COLOR_RGB( 255, 255, 255 );
		}
	}

	if( ent->spawnflags & 16 ) { // EXPAND_EFFECT
		ent->particlesInfo.expandEffect = true;
	}

	if( ent->spawnflags & 32 ) { // SHRINK_EFFECT
		ent->particlesInfo.shrinkEffect = true;
	}

	if( ent->spawnflags & START_OFF ) { // START_OFF
		ent->r.svflags |= SVF_NOCLIENT;
	}

	if( st.radius > 0 ) {
		ent->particlesInfo.spread = st.radius;
		clamp( ent->particlesInfo.spread, 0, 255 );
	}

	ent->think = SP_misc_particles_finish;
	ent->nextThink = level.time + 1;
	ent->use = SP_misc_particles_use;

	GClip_LinkEntity( ent );
}
