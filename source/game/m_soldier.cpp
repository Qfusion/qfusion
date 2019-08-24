/*
Copyright (C) 2018 Victor Luchits

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
==============================================================================

soldier

==============================================================================
*/

#include "g_local.h"

void soldier_sight( edict_t *self, edict_t *other ) {
	G_AddEvent( self, EV_WEAPONACTIVATE, self->s.weapon<<1, true );
}

void soldier_search( edict_t *self ) {
}

mframe_t soldier_frames_stand [] = {
	{ ai_stand, 0, NULL, },
};

mmove_t soldier_move_stand = { 0, 0, soldier_frames_stand, NULL };

void soldier_stand( edict_t *self ) {
	self->monsterinfo.currentmove = &soldier_move_stand;
}

mframe_t soldier_frames_walk [] =
{
	{ ai_walk, 25, NULL, },
};
mmove_t soldier_move_walk = {0, 0, soldier_frames_walk, NULL};

void soldier_walk( edict_t *self ) {
	self->monsterinfo.currentmove = &soldier_move_walk;
}

mframe_t soldier_frames_run [] =
{
	{ ai_run, 120, NULL, },
};
mmove_t soldier_move_run = {0, 0, soldier_frames_run, NULL};

void soldier_run( edict_t *self ) {
	if( self->monsterinfo.aiflags & AI_STAND_GROUND ) {
		self->monsterinfo.currentmove = &soldier_move_stand;
	} else {
		self->monsterinfo.currentmove = &soldier_move_run;
	}
}

void soldier_attack( edict_t *self ) {
	int parm = self->s.weapon << 1;
	
	AttackFinished( self, 500 );

	G_FireWeapon( self, parm );

	G_AddEvent( self, EV_FIREWEAPON, parm, true );
}

static void soldier_pain_think( edict_t *self ) {
	if( self->snap.damage_taken > 0 || level.time < self->pain_debounce_time ) {
		// freeze if in pain
		return;
	}
	soldier_run( self );
}

mframe_t soldier_frames_pain[] =
{
	{ ai_walk, 0, soldier_pain_think, },
};
mmove_t soldier_move_pain = {0, 0, soldier_frames_pain, NULL};

void soldier_pain( edict_t *self, edict_t *other, float kick, int damage ) {
	self->monsterinfo.currentmove = &soldier_move_pain;
}

void soldier_dead( edict_t *self ) {
	const int deathanim[3] = { BOTH_DEAD1, BOTH_DEAD2, BOTH_DEAD3 };
	int i = rand() % (sizeof(deathanim) / sizeof(deathanim[0]));

	self->deadflag = DEAD_NO;

	if( self->s.type == ET_MONSTER_CORPSE ) {
		return;
	}
	
	VectorSet( self->r.mins, -16, -16, -24 );
	VectorSet( self->r.maxs, 16, 16, -8 );
	self->movetype = MOVETYPE_TOSS;
	self->s.type = ET_MONSTER_CORPSE;
	self->r.svflags |= SVF_CORPSE;
	self->r.solid = SOLID_NOT;
	self->nextThink = 0;
	
	// launch the death animation
	G_AddEvent( self, EV_DIE, i, true );
	self->s.frame = GS_EncodeAnimState( deathanim[i], deathanim[i], 0 );

	GClip_LinkEntity( self );
}

mframe_t soldier_frames_death[] =
{
	{ ai_move, 0, NULL, },
};
mmove_t soldier_move_death = {0, 0, soldier_frames_death, soldier_dead};

void soldier_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t point )
{
	soldier_dead( self );

	if( self->health <= self->gib_health ) {
		ThrowSmallPileOfGibs( self, damage );
		VectorClear( self->velocity );
		ThrowClientHead( self, damage ); // sets ET_GIB
		return;
	}

	self->takedamage = DAMAGE_YES;
}


/*QUAKED monster_soldier (1 .5 0) (-16 -16 -24) (16 16 40) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier( edict_t *self )
{
	VectorCopy( playerbox_stand_mins, self->r.mins );
	VectorCopy( playerbox_stand_maxs, self->r.maxs );
	self->s.modelindex = trap_ModelIndex( "$models/players/bigvic" );
	self->r.solid = SOLID_YES;
	self->movetype = MOVETYPE_STEP;
	self->s.type = ET_MONSTER_PLAYER;
	self->r.svflags &= ~SVF_NOCLIENT;
	self->s.weapon = WEAP_MACHINEGUN;
	self->s.team = TEAM_MONSTERS;

	self->health = 30;
	self->gib_health = -40;
	self->mass = 250;
	self->yaw_speed = 45;

	self->pain = soldier_pain;
	self->die = soldier_die;

	self->monsterinfo.stand = soldier_stand;
	self->monsterinfo.walk = soldier_walk;
	self->monsterinfo.run = soldier_run;
	self->monsterinfo.dodge = NULL;
	self->monsterinfo.attack = soldier_attack;
	self->monsterinfo.sight = soldier_sight;
	self->monsterinfo.search = soldier_search;
	
	soldier_stand( self );

	GClip_LinkEntity( self );

	walkmonster_start( self );
}
