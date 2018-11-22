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

// gs_weapons.c	-	game shared weapons definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"


#define BULLET_WATER_REFRACTION 1.5f

/*
* GS_TraceBullet
*/
trace_t *GS_TraceBullet( trace_t *trace, vec3_t start, const vec3_t fv, const vec3_t rv, const vec3_t uv, 
	float r, float u, int range, int ignore, int timeDelta ) {
	vec3_t end;
	bool water = false;
	vec3_t water_start;
	int content_mask = MASK_SHOT | MASK_WATER;
	static trace_t water_trace;

	assert( trace );

	if( gs.api.PointContents( start, timeDelta ) & MASK_WATER ) {
		water = true;
		VectorCopy( start, water_start );
		content_mask &= ~MASK_WATER;

		// ok, this isn't randomized, but I think we can live with it
		// the effect on water has never been properly noticed anyway
		//r *= BULLET_WATER_REFRACTION;
		//u *= BULLET_WATER_REFRACTION;
	}

	VectorMA( start, range, fv, end );
	if( r ) {
		VectorMA( end, r, rv, end );
	}
	if( u ) {
		VectorMA( end, u, uv, end );
	}

	gs.api.Trace( trace, start, vec3_origin, vec3_origin, end, ignore, content_mask, timeDelta );

	// see if we hit water
	if( trace->contents & MASK_WATER ) {
		water_trace = *trace;

		VectorCopy( trace->endpos, water_start );
#if 0
		if( !VectorCompare( start, trace->endpos ) ) {
			vec3_t forward, right, up;

			// change bullet's course when it enters water
			VectorSubtract( end, start, dir );
			VecToAngles( dir, dir );
			AngleVectors( dir, forward, right, up );

			// ok, this isn't randomized, but I think we can live with it
			// the effect on water has never been properly noticed anyway
			r *= BULLET_WATER_REFRACTION;
			u *= BULLET_WATER_REFRACTION;

			VectorMA( water_start, range, forward, end );
			VectorMA( end, r, right, end );
			VectorMA( end, u, up, end );
		}
#endif

		// re-trace ignoring water this time
		gs.api.Trace( trace, water_start, vec3_origin, vec3_origin, end, ignore, MASK_SHOT, timeDelta );

		return &water_trace;
	}

	if( water ) {
		water_trace = *trace;
		VectorCopy( water_start, water_trace.endpos );
		return &water_trace;
	}

	return NULL;
}
#undef BULLET_WATER_REFRACTION

#define MAX_BEAM_HIT_ENTITIES 16

void GS_TraceLaserBeam( trace_t *trace, vec3_t origin, vec3_t dir, float range, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) ) {
	vec3_t from, end;
	int mask = MASK_SHOT;
	int passthrough = ignore;
	entity_state_t *hit;
	vec3_t mins = { -0.5, -0.5, -0.5 };
	vec3_t maxs = { 0.5, 0.5, 0.5 };
	int hits[MAX_BEAM_HIT_ENTITIES];
	int j, numhits;

	assert( trace );

	VectorCopy( origin, from );
	VectorMA( origin, range, dir, end );

	trace->ent = 0;

	numhits = 0;
	while( trace->ent != -1 ) {
		gs.api.Trace( trace, from, mins, maxs, end, passthrough, mask, timeDelta );
		if( trace->ent != -1 ) {
			// prevent endless loops by checking whether we have already impacted this entity
			for( j = 0; j < numhits; j++ ) {
				if( trace->ent == hits[j] ) {
					break;
				}
			}
			if( j < numhits ) {
				break;
			}

			// callback impact
			if( impact ) {
				impact( trace, dir );
			}

			// check for pass-through
			hit = gs.api.GetEntityState( trace->ent, timeDelta );
			if( trace->ent == 0 || !hit || hit->solid == SOLID_BMODEL ) { // can't pass through brush models
				break;
			}

			// if trapped inside solid, just forget about it
			if( trace->fraction == 0 || trace->allsolid || trace->startsolid ) {
				break;
			}

			// put a limit on number of hit entities
			if( numhits < MAX_BEAM_HIT_ENTITIES ) {
				hits[numhits++] = trace->ent;
			} else {
				break;
			}

			passthrough = trace->ent;
			VectorCopy( trace->endpos, from );
		}
	}

	if( trace->ent != -1 ) { // was interrupted by a brush model impact
	}
}

void GS_TraceCurveLaserBeam( trace_t *trace, vec3_t origin, vec3_t angles, vec3_t blendPoint, int ignore, int timeDelta, void ( *impact )( trace_t *tr, vec3_t dir ) ) {
	float frac, subdivisions = CURVELASERBEAM_SUBDIVISIONS;
	float range = (float)GS_GetWeaponDef( WEAP_LASERGUN )->firedef_weak.timeout;
	vec3_t from, dir, end;
	int passthrough = ignore;

	int i, j;
	vec3_t tmpangles, blendAngles;

	assert( trace );

	VectorCopy( origin, from );
	VectorSubtract( blendPoint, origin, dir );
	VecToAngles( dir, blendAngles );

	for( i = 1; i <= (int)subdivisions; i++ ) {
		frac = ( ( range / subdivisions ) * (float)i ) / (float)range;

		for( j = 0; j < 3; j++ )
			tmpangles[j] = LerpAngle( angles[j], blendAngles[j], frac );

		AngleVectors( tmpangles, dir, NULL, NULL );
		VectorMA( origin, range * frac, dir, end );

		GS_TraceLaserBeam( trace, from, dir, DistanceFast( from, end ), passthrough, timeDelta, impact );
		if( trace->fraction != 1.0f ) {
			break;
		}

		passthrough = trace->ent;
		VectorCopy( end, from );
	}
}

void GS_AddLaserbeamPoint( gs_laserbeamtrail_t *trail, player_state_t *playerState, int64_t timeStamp ) {
	vec3_t origin, dir;
	int range = GS_GetWeaponDef( WEAP_LASERGUN )->firedef_weak.timeout;

	if( !timeStamp ) {
		return;
	}

	origin[0] = playerState->pmove.origin[0];
	origin[1] = playerState->pmove.origin[1];
	origin[2] = playerState->pmove.origin[2] + playerState->viewheight;
	AngleVectors( playerState->viewangles, dir, NULL, NULL );

	VectorMA( origin, range, dir, trail->origins[trail->head & LASERGUN_WEAK_TRAIL_MASK] );
	trail->timeStamps[trail->head & LASERGUN_WEAK_TRAIL_MASK] = timeStamp;
	trail->teleported[trail->head & LASERGUN_WEAK_TRAIL_MASK] = ( playerState->pmove.pm_flags & PMF_TIME_TELEPORT ) ? true : false;
	trail->head++;
}

bool G_GetLaserbeamPoint( gs_laserbeamtrail_t *trail, player_state_t *playerState, int64_t curtime, vec3_t out ) {
	int older;
	int current;
	int64_t timeStamp;

	if( curtime <= CURVELASERBEAM_BACKTIME ) {
		return false;
	}

	timeStamp = curtime - CURVELASERBEAM_BACKTIME;

	current = trail->head - 1;

	// add current if doesn't exist
	if( trail->timeStamps[current & LASERGUN_WEAK_TRAIL_MASK] == 0 ) {
		return false;
	}

	if( timeStamp >= trail->timeStamps[current & LASERGUN_WEAK_TRAIL_MASK] ) {
		timeStamp = trail->timeStamps[current & LASERGUN_WEAK_TRAIL_MASK];
	}

	older = current;
	while( ( older > 0 ) && ( trail->timeStamps[older & LASERGUN_WEAK_TRAIL_MASK] > timeStamp )
		   && ( trail->timeStamps[( older - 1 ) & LASERGUN_WEAK_TRAIL_MASK] )
		   && ( !trail->teleported[older & LASERGUN_WEAK_TRAIL_MASK] )
		   ) {
		older--;
	}

	// todo: add interpolation?
	VectorCopy( trail->origins[older & LASERGUN_WEAK_TRAIL_MASK], out );
	return true;
}
#undef  LASERGUN_WEAK_TRAIL_BACKUP
#undef  LASERGUN_WEAK_TRAIL_MASK

static bool GS_CheckBladeAutoAttack( player_state_t *playerState, int timeDelta ) {
	vec3_t origin, dir, end;
	trace_t trace;
	entity_state_t *targ, *player;
	gs_weapon_definition_t *weapondef = GS_GetWeaponDef( WEAP_GUNBLADE );

	if( playerState->POVnum <= 0 || (int)playerState->POVnum > gs.maxclients ) {
		return false;
	}

	if( !( playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_GUNBLADEAUTOATTACK ) ) {
		return false;
	}

	VectorCopy( playerState->pmove.origin, origin );
	origin[2] += playerState->viewheight;
	AngleVectors( playerState->viewangles, dir, NULL, NULL );
	VectorMA( origin, weapondef->firedef_weak.timeout, dir, end );

	// check for a player to touch
	gs.api.Trace( &trace, origin, vec3_origin, vec3_origin, end, playerState->POVnum, CONTENTS_BODY, timeDelta );
	if( trace.ent <= 0 || trace.ent > gs.maxclients ) {
		return false;
	}

	player = gs.api.GetEntityState( playerState->POVnum, 0 );
	targ = gs.api.GetEntityState( trace.ent, 0 );
	if( !( targ->effects & EF_TAKEDAMAGE ) || targ->type != ET_PLAYER ) {
		return false;
	}

	if( GS_TeamBasedGametype() && ( targ->team == player->team ) ) {
		return false;
	}

	return true;
}


//============================================================
//
//		PLAYER WEAPON THINKING
//
//============================================================


char *gs_weaponStateNames[] =
{
	"WEAPON_STATE_READY",
	"WEAPON_STATE_ACTIVATING",
	"WEAPON_STATE_DROPPING",
	"WEAPON_STATE_POWERING",
	"WEAPON_STATE_COOLDOWN",
	"WEAPON_STATE_FIRING",
	"WEAPON_STATE_RELOADING",       // clip loading
	"WEAPON_STATE_NOAMMOCLICK",
	"WEAPON_STATE_REFIRE",      // projectile loading
	"WEAPON_STATE_REFIRESTRONG"
};

#define NOAMMOCLICK_PENALTY 100
#define NOAMMOCLICK_AUTOSWITCH 50

/*
* GS_SelectBestWeapon
*/
int GS_SelectBestWeapon( player_state_t *playerState ) {
	int weap, weap_chosen = WEAP_NONE;
	gs_weapon_definition_t *weapondef;

	//find with strong ammos
	for( weap = WEAP_TOTAL - 1; weap > WEAP_GUNBLADE; weap-- ) {
		if( !playerState->inventory[weap] ) {
			continue;
		}

		weapondef = GS_GetWeaponDef( weap );

		if( !weapondef->firedef.usage_count ||
			playerState->inventory[weapondef->firedef.ammo_id] >= weapondef->firedef.usage_count ) {
			weap_chosen = weap;
			goto found;
		}
	}

	//repeat find with weak ammos
	for( weap = WEAP_TOTAL - 1; weap >= WEAP_NONE; weap-- ) {
		if( !playerState->inventory[weap] ) {
			continue;
		}

		weapondef = GS_GetWeaponDef( weap );

		if( !weapondef->firedef_weak.usage_count ||
			playerState->inventory[weapondef->firedef_weak.ammo_id] >= weapondef->firedef_weak.usage_count ) {
			weap_chosen = weap;
			goto found;
		}
	}
found:
	return weap_chosen;
}

/*
* GS_FiredefForPlayerState
*/
firedef_t *GS_FiredefForPlayerState( player_state_t *playerState, int checkweapon ) {
	gs_weapon_definition_t *weapondef;

	weapondef = GS_GetWeaponDef( checkweapon );

	//find out our current fire mode
	if( playerState->inventory[weapondef->firedef.ammo_id] >= weapondef->firedef.usage_count ) {
		return &weapondef->firedef;
	}

	return &weapondef->firedef_weak;
}

/*
* GS_CheckAmmoInWeapon
*/
bool GS_CheckAmmoInWeapon( player_state_t *playerState, int checkweapon ) {
	firedef_t *firedef = GS_FiredefForPlayerState( playerState, checkweapon );

	if( checkweapon != WEAP_NONE && !playerState->inventory[checkweapon] ) {
		return false;
	}

	if( !firedef->usage_count || firedef->ammo_id == AMMO_NONE ) {
		return true;
	}

	return ( playerState->inventory[firedef->ammo_id] >= firedef->usage_count ) ? true : false;
}

/*
* GS_ThinkPlayerWeapon
*/
int GS_ThinkPlayerWeapon( player_state_t *playerState, int buttons, int msecs, int timeDelta ) {
	firedef_t *firedef;
	bool refire = false;

	assert( playerState->stats[STAT_PENDING_WEAPON] >= 0 && playerState->stats[STAT_PENDING_WEAPON] < WEAP_TOTAL );

	if( GS_MatchPaused() ) {
		return playerState->stats[STAT_WEAPON];
	}

	if( playerState->pmove.pm_type != PM_NORMAL ) {
		playerState->weaponState = WEAPON_STATE_READY;
		playerState->stats[STAT_PENDING_WEAPON] = playerState->stats[STAT_WEAPON] = WEAP_NONE;
		playerState->stats[STAT_WEAPON_TIME] = 0;
		return playerState->stats[STAT_WEAPON];
	}

	if( playerState->pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
		buttons = 0;
	}

	if( !msecs ) {
		goto done;
	}

	if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
		playerState->stats[STAT_WEAPON_TIME] -= msecs;
	} else {
		playerState->stats[STAT_WEAPON_TIME] = 0;
	}

	firedef = GS_FiredefForPlayerState( playerState, playerState->stats[STAT_WEAPON] );

	// during cool-down time it can shoot again or go into reload time
	if( playerState->weaponState == WEAPON_STATE_REFIRE || playerState->weaponState == WEAPON_STATE_REFIRESTRONG ) {
		int last_firemode;

		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		last_firemode = ( playerState->weaponState == WEAPON_STATE_REFIRESTRONG ) ? FIRE_MODE_STRONG : FIRE_MODE_WEAK;
		if( last_firemode == firedef->fire_mode ) {
			refire = true;
		}

		playerState->weaponState = WEAPON_STATE_READY;
	}

	// nothing can be done during reload time
	if( playerState->weaponState == WEAPON_STATE_RELOADING ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		playerState->weaponState = WEAPON_STATE_READY;
	}

	if( playerState->weaponState == WEAPON_STATE_NOAMMOCLICK ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		if( playerState->stats[STAT_WEAPON] != playerState->stats[STAT_PENDING_WEAPON] ) {
			playerState->weaponState = WEAPON_STATE_READY;
		}
	}

	// there is a weapon to be changed
	if( playerState->stats[STAT_WEAPON] != playerState->stats[STAT_PENDING_WEAPON] ) {
		if( ( playerState->weaponState == WEAPON_STATE_READY ) ||
			( playerState->weaponState == WEAPON_STATE_DROPPING ) ||
			( playerState->weaponState == WEAPON_STATE_ACTIVATING ) ) {
			if( playerState->weaponState != WEAPON_STATE_DROPPING ) {
				playerState->weaponState = WEAPON_STATE_DROPPING;
				playerState->stats[STAT_WEAPON_TIME] += firedef->weapondown_time;

				if( firedef->weapondown_time ) {
					gs.api.PredictedEvent( playerState->POVnum, EV_WEAPONDROP, 0 );
				}
			}
		}
	}

	// do the change
	if( playerState->weaponState == WEAPON_STATE_DROPPING ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		playerState->stats[STAT_WEAPON] = playerState->stats[STAT_PENDING_WEAPON];

		// update the firedef
		firedef = GS_FiredefForPlayerState( playerState, playerState->stats[STAT_WEAPON] );
		playerState->weaponState = WEAPON_STATE_ACTIVATING;
		playerState->stats[STAT_WEAPON_TIME] += firedef->weaponup_time;
		gs.api.PredictedEvent( playerState->POVnum, EV_WEAPONACTIVATE, playerState->stats[STAT_WEAPON]<<1 );
	}

	if( playerState->weaponState == WEAPON_STATE_ACTIVATING ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		playerState->weaponState = WEAPON_STATE_READY;
	}

	if( ( playerState->weaponState == WEAPON_STATE_READY ) || ( playerState->weaponState == WEAPON_STATE_NOAMMOCLICK ) ) {
		if( playerState->stats[STAT_WEAPON_TIME] > 0 ) {
			goto done;
		}

		if( !GS_ShootingDisabled() ) {
			if( buttons & BUTTON_ATTACK ) {
				if( GS_CheckAmmoInWeapon( playerState, playerState->stats[STAT_WEAPON] ) ) {
					playerState->weaponState = WEAPON_STATE_FIRING;
				} else {
					// player has no ammo nor clips
					if( playerState->weaponState == WEAPON_STATE_NOAMMOCLICK ) {
						playerState->weaponState = WEAPON_STATE_RELOADING;
						playerState->stats[STAT_WEAPON_TIME] += NOAMMOCLICK_AUTOSWITCH;
						if( playerState->stats[STAT_PENDING_WEAPON] == playerState->stats[STAT_WEAPON] ) {
							playerState->stats[STAT_PENDING_WEAPON] = GS_SelectBestWeapon( playerState );
						}
					} else {
						playerState->weaponState = WEAPON_STATE_NOAMMOCLICK;
						playerState->stats[STAT_WEAPON_TIME] += NOAMMOCLICK_PENALTY;
						gs.api.PredictedEvent( playerState->POVnum, EV_NOAMMOCLICK, 0 );
						goto done;
					}
				}
			}
			// gunblade auto attack is special
			else if( playerState->stats[STAT_WEAPON] == WEAP_GUNBLADE &&
					 playerState->pmove.stats[PM_STAT_NOUSERCONTROL] <= 0 &&
					 playerState->pmove.stats[PM_STAT_NOAUTOATTACK] <= 0 &&
					 GS_CheckBladeAutoAttack( playerState, timeDelta ) ) {
				firedef = &GS_GetWeaponDef( WEAP_GUNBLADE )->firedef_weak;
				playerState->weaponState = WEAPON_STATE_FIRING;
			}
		}
	}

	if( playerState->weaponState == WEAPON_STATE_FIRING ) {
		int parm = playerState->stats[STAT_WEAPON] << 1;
		if( firedef->fire_mode == FIRE_MODE_STRONG ) {
			parm |= 0x1;
		}

		playerState->stats[STAT_WEAPON_TIME] += firedef->reload_time;
		if( firedef->fire_mode == FIRE_MODE_STRONG ) {
			playerState->weaponState = WEAPON_STATE_REFIRESTRONG;
		} else {
			playerState->weaponState = WEAPON_STATE_REFIRE;
		}

		if( refire && firedef->smooth_refire ) {
			gs.api.PredictedEvent( playerState->POVnum, EV_SMOOTHREFIREWEAPON, parm );
		} else {
			gs.api.PredictedEvent( playerState->POVnum, EV_FIREWEAPON, parm );
		}

		// waste ammo
		if( !GS_InfiniteAmmo() && playerState->stats[STAT_WEAPON] != WEAP_GUNBLADE ) {
			if( firedef->ammo_id != AMMO_NONE && firedef->usage_count ) {
				playerState->inventory[firedef->ammo_id] -= firedef->usage_count;
			}
		}
	}
done:
	return playerState->stats[STAT_WEAPON];
}
