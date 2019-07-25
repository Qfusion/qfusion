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

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"
#include "angelscript.h"
#include "q_angeliface.h"
#include "gs_ascript.h"

//===============================================================
//		WARSOW player AAboxes sizes

vec3_t playerbox_stand_mins = { -16, -16, -24 };
vec3_t playerbox_stand_maxs = { 16, 16, 40 };
int playerbox_stand_viewheight = 30;

vec3_t playerbox_crouch_mins = { -16, -16, -24 };
vec3_t playerbox_crouch_maxs = { 16, 16, 16 };
int playerbox_crouch_viewheight = 12;

vec3_t playerbox_gib_mins = { -16, -16, 0 };
vec3_t playerbox_gib_maxs = { 16, 16, 16 };
int playerbox_gib_viewheight = 8;

#define SPEEDKEY    500.0f

#define PM_DASHJUMP_TIMEDELAY 1000 // delay in milliseconds
#define PM_WALLJUMP_TIMEDELAY   1300
#define PM_WALLJUMP_FAILED_TIMEDELAY    700
#define PM_SPECIAL_CROUCH_INHIBIT 400
#define PM_AIRCONTROL_BOUNCE_DELAY 200
#define PM_OVERBOUNCE       1.01f
#define PM_CROUCHSLIDE_TIMEDELAY 700
#define PM_CROUCHSLIDE_CONTROL 3
#define PM_FORWARD_ACCEL_TIMEDELAY 0 // delay before the forward acceleration kicks in
#define PM_SKIM_TIME 230

//===============================================================

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
	vec3_t origin;          // full float precision
	vec3_t velocity;        // full float precision

	vec3_t forward, right, up;
	vec3_t flatforward;     // normalized forward without z component, saved here because it needs
	// special handling for looking straight up or down
	float frametime;

	int groundsurfFlags;
	cplane_t groundplane;
	int groundcontents;

	vec3_t previous_origin;
	bool ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxWalkSpeed;
	float maxCrouchedSpeed;
	float jumpPlayerSpeed;
	float dashPlayerSpeed;
} pml_t;

pmove_t *pm;
pml_t pml;

// movement parameters

#define DEFAULT_WALKSPEED 160.0f
#define DEFAULT_CROUCHEDSPEED 100.0f
#define DEFAULT_LADDERSPEED 250.0f

const float pm_friction = 8; //  ( initially 6 )
const float pm_waterfriction = 1;
const float pm_wateraccelerate = 10; // user intended acceleration when swimming ( initially 6 )

const float pm_accelerate = 12; // user intended acceleration when on ground or fly movement ( initially 10 )
const float pm_decelerate = 12; // user intended deceleration when on ground

const float pm_airaccelerate = 1; // user intended aceleration when on air
const float pm_airdecelerate = 2.0f; // air deceleration (not +strafe one, just at normal moving).

// special movement parameters

const float pm_aircontrol = 150.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
const float pm_strafebunnyaccel = 70; // forward acceleration when strafe bunny hopping
const float pm_wishspeed = 30;

const float pm_dashupspeed = ( 174.0f * GRAVITY_COMPENSATE );

#ifdef OLDWALLJUMP
const float pm_wjupspeed = 370;
const float pm_wjbouncefactor = 0.5f;
#define pm_wjminspeed pm_maxspeed
#else
const float pm_wjupspeed = ( 330.0f * GRAVITY_COMPENSATE );
const float pm_failedwjupspeed = ( 50.0f * GRAVITY_COMPENSATE );
const float pm_wjbouncefactor = 0.3f;
const float pm_failedwjbouncefactor = 0.1f;
#define pm_wjminspeed ( ( pml.maxWalkSpeed + pml.maxPlayerSpeed ) * 0.5f )
#endif

//
// Kurim : some functions/defines that can be useful to work on the horizontal movement of player :
//
#define VectorScale2D( in, scale, out ) ( ( out )[0] = ( in )[0] * ( scale ), ( out )[1] = ( in )[1] * ( scale ) )
#define DotProduct2D( x, y )           ( ( x )[0] * ( y )[0] + ( x )[1] * ( y )[1] )

static vec_t VectorNormalize2D( vec3_t v ) { // ByMiK : normalize horizontally (don't affect Z value)
	float length, ilength;
	length = v[0] * v[0] + v[1] * v[1];
	if( length ) {
		length = sqrt( length ); // FIXME
		ilength = 1.0f / length;
		v[0] *= ilength;
		v[1] *= ilength;
	}
	return length;
}

// Walljump wall availability check
// nbTestDir is the number of directions to test around the player
// maxZnormal is the max Z value of the normal of a poly to consider it a wall
// normal becomes a pointer to the normal of the most appropriate wall
static void PlayerTouchWall( int nbTestDir, float maxZnormal, vec3_t *normal ) {
	vec3_t zero, dir, mins, maxs;
	trace_t trace;
	int i;
	bool alternate;
	float r, d, dx, dy, m;

	VectorClear( zero );

	// if there is nothing at all within the checked area, we can skip the individual checks
	// this optimization must always overapproximate the combination of those checks
	mins[0] = pm->mins[0] - pm->maxs[0];
	mins[1] = pm->mins[1] - pm->maxs[0];
	maxs[0] = pm->maxs[0] + pm->maxs[0];
	maxs[1] = pm->maxs[1] + pm->maxs[0];
	if( pml.velocity[0] > 0 ) {
		maxs[0] += pml.velocity[0] * 0.015f;
	} else {
		mins[0] += pml.velocity[0] * 0.015f;
	}
	if( pml.velocity[1] > 0 ) {
		maxs[1] += pml.velocity[1] * 0.015f;
	} else {
		mins[1] += pml.velocity[1] * 0.015f;
	}
	mins[2] = maxs[2] = 0;
	gs.api.Trace( &trace, pml.origin, mins, maxs, pml.origin, pm->playerState->POVnum, pm->contentmask, 0 );
	if( !trace.allsolid && trace.fraction == 1 ) {
		return;
	}

	// determine the primary direction
	if( pml.sidePush > 0 ) {
		r = -M_PI / 2.0f;
	} else if( pml.sidePush < 0 ) {
		r = M_PI / 2.0f;
	} else if( pml.forwardPush > 0 ) {
		r = 0.0f;
	} else {
		r = M_PI;
	}
	alternate = pml.sidePush == 0 || pml.forwardPush == 0;

	d = 0.0f; // current distance from the primary direction

	for( i = 0; i < nbTestDir; i++ ) {
		if( i != 0 ) {
			if( alternate ) {
				r += M_PI; // switch front and back
			}
			if( ( !alternate && i % 2 == 0 ) || ( alternate && i % 4 == 0 ) ) { // switch from left to right
				r -= 2 * d;
			} else if( !alternate || ( alternate && i % 4 == 2 ) ) {   // switch from right to left and move further away
				r += d;
				d += M_TWOPI / nbTestDir;
				r += d;
			}
		}

		// determine the relative offsets from the origin
		dx = cos( DEG2RAD( pm->playerState->viewangles[YAW] ) + r );
		dy = sin( DEG2RAD( pm->playerState->viewangles[YAW] ) + r );

		// project onto the player box
		if( dx == 0 ) {
			m = pm->maxs[1];
		} else if( dy == 0 ) {
			m = pm->maxs[0];
		} else if( fabs( dx / pm->maxs[0] ) > fabs( dy / pm->maxs[1] ) ) {
			m = fabs( pm->maxs[0] / dx );
		} else {
			m = fabs( pm->maxs[1] / dy );
		}

		// allow a gap between the player and the wall
		m += pm->maxs[0];

		dir[0] = pml.origin[0] + dx * m + pml.velocity[0] * 0.015f;
		dir[1] = pml.origin[1] + dy * m + pml.velocity[1] * 0.015f;
		dir[2] = pml.origin[2];

		gs.api.Trace( &trace, pml.origin, zero, zero, dir, pm->playerState->POVnum, pm->contentmask, 0 );

		if( trace.allsolid ) {
			return;
		}

		if( trace.fraction == 1 ) {
			continue; // no wall in this direction

		}
		if( trace.surfFlags & ( SURF_SKY | SURF_NOWALLJUMP ) ) {
			continue;
		}

		if( trace.ent > 0 && gs.api.GetEntityState( trace.ent, 0 )->type == ET_PLAYER ) {
			continue;
		}

		if( trace.fraction > 0 && fabs( trace.plane.normal[2] ) < maxZnormal ) {
			VectorCopy( trace.plane.normal, *normal );
			return;
		}
	}
}

//
//  walking up a step should kill some velocity
//

/*
* PM_SlideMove
*
* Returns a new origin, velocity, and contact entity
* Does not modify any world state?
*/

#define MAX_CLIP_PLANES 5

static void PM_AddTouchEnt( int entNum ) {
	int i;

	if( pm->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( i = 0; i < pm->numtouch; i++ ) {
		if( pm->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	pm->touchents[pm->numtouch] = entNum;
	pm->numtouch++;
}


static int PM_SlideMove( void ) {
	vec3_t end, dir;
	vec3_t old_velocity, last_valid_origin;
	float value;
	vec3_t planes[MAX_CLIP_PLANES];
	int numplanes = 0;
	trace_t trace;
	int moves, i, j, k;
	int maxmoves = 4;
	float remainingTime = pml.frametime;
	int blockedmask = 0;

	if( pm->groundentity != -1 ) { // clip velocity to ground, no need to wait
		// if the ground is not horizontal (a ramp) clipping will slow the player down
		if( pml.groundplane.normal[2] == 1.0f && pml.velocity[2] < 0.0f ) {
			pml.velocity[2] = 0.0f;
		}
	}

	// Do a shortcut in this case
	if( pm->skipCollision ) {
		VectorMA( pml.origin, remainingTime, pml.velocity, pml.origin );
		return blockedmask;
	}

	VectorCopy( pml.velocity, old_velocity );
	VectorCopy( pml.origin, last_valid_origin );

	numplanes = 0; // clean up planes count for checking

	for( moves = 0; moves < maxmoves; moves++ ) {
		VectorMA( pml.origin, remainingTime, pml.velocity, end );
		gs.api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid ) { // trapped into a solid
			VectorCopy( last_valid_origin, pml.origin );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		if( trace.fraction > 0 ) { // actually covered some distance
			VectorCopy( trace.endpos, pml.origin );
			VectorCopy( trace.endpos, last_valid_origin );
		}

		if( trace.fraction == 1 ) {
			break; // move done

		}
		// save touched entity for return output
		PM_AddTouchEnt( trace.ent );

		// at this point we are blocked but not trapped.

		blockedmask |= SLIDEMOVEFLAG_BLOCKED;
		if( trace.plane.normal[2] < SLIDEMOVE_PLANEINTERACT_EPSILON ) { // is it a vertical wall?
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		remainingTime -= ( trace.fraction * remainingTime );

		// we got blocked, add the plane for sliding along it

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for( i = 0; i < numplanes; i++ ) {
			if( DotProduct( trace.plane.normal, planes[i] ) > ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) ) {
				VectorAdd( trace.plane.normal, pml.velocity, pml.velocity );
				break;
			}
		}
		if( i < numplanes ) { // found a repeated plane, so don't add it, just repeat the trace
			continue;
		}

		// security check: we can't store more planes
		if( numplanes >= MAX_CLIP_PLANES ) {
			VectorClear( pml.velocity );
			return SLIDEMOVEFLAG_TRAPPED;
		}

		// put the actual plane in the list
		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//

		for( i = 0; i < numplanes; i++ ) {
			if( DotProduct( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) { // would not touch it
				continue;
			}

			GS_ClipVelocity( pml.velocity, planes[i], pml.velocity, PM_OVERBOUNCE );
			// see if we enter a second plane
			for( j = 0; j < numplanes; j++ ) {
				if( j == i ) { // it's the same plane
					continue;
				}
				if( DotProduct( pml.velocity, planes[j] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue; // not with this one

				}
				//there was a second one. Try to slide along it too
				GS_ClipVelocity( pml.velocity, planes[j], pml.velocity, PM_OVERBOUNCE );

				// check if the slide sent it back to the first plane
				if( DotProduct( pml.velocity, planes[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
					continue;
				}

				// bad luck: slide the original velocity along the crease
				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				value = DotProduct( dir, pml.velocity );
				VectorScale( dir, value, pml.velocity );

				// check if there is a third plane, in that case we're trapped
				for( k = 0; k < numplanes; k++ ) {
					if( j == k || i == k ) { // it's the same plane
						continue;
					}
					if( DotProduct( pml.velocity, planes[k] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
						continue; // not with this one
					}
					VectorClear( pml.velocity );
					break;
				}
			}
		}
	}

	if( pm->numtouch ) {
		if( pm->playerState->pmove.pm_time || ( pm->groundentity == -1 && pm->waterlevel < 2
												&& ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CORNERSKIMMING )
												&& pm->playerState->pmove.skim_time > 0 && old_velocity[2] >= pml.velocity[2] ) ) {
			VectorCopy( old_velocity, pml.velocity );
		}
		pm->playerState->pmove.skim_time -= pm->cmd.msec;
		if( pm->playerState->pmove.skim_time < 0 ) {
			pm->playerState->pmove.skim_time = 0;
		}
	}

	return blockedmask;
}

/*
* PM_StepSlideMove
*
* Each intersection will try to step over the obstruction instead of
* sliding along it.
*/
static void PM_StepSlideMove( void ) {
	vec3_t start_o, start_v;
	vec3_t down_o, down_v;
	trace_t trace;
	float down_dist, up_dist;
	float hspeed;
	vec3_t up, down;
	int blocked;

	VectorCopy( pml.origin, start_o );
	VectorCopy( pml.velocity, start_v );

	blocked = PM_SlideMove();

	// We have modified the origin in PM_SlideMove() in this case.
	// No further computations are required.
	if( pm->skipCollision ) {
		return;
	}

	VectorCopy( pml.origin, down_o );
	VectorCopy( pml.velocity, down_v );

	VectorCopy( start_o, up );
	up[2] += STEPSIZE;

	gs.api.Trace( &trace, up, pm->mins, pm->maxs, up, pm->playerState->POVnum, pm->contentmask, 0 );
	if( trace.allsolid ) {
		return; // can't step up

	}
	// try sliding above
	VectorCopy( up, pml.origin );
	VectorCopy( start_v, pml.velocity );

	PM_SlideMove();

	// push down the final amount
	VectorCopy( pml.origin, down );
	down[2] -= STEPSIZE;
	gs.api.Trace( &trace, pml.origin, pm->mins, pm->maxs, down, pm->playerState->POVnum, pm->contentmask, 0 );
	if( !trace.allsolid ) {
		VectorCopy( trace.endpos, pml.origin );
	}

	VectorCopy( pml.origin, up );

	// decide which one went farther
	down_dist = ( down_o[0] - start_o[0] ) * ( down_o[0] - start_o[0] )
				+ ( down_o[1] - start_o[1] ) * ( down_o[1] - start_o[1] );
	up_dist = ( up[0] - start_o[0] ) * ( up[0] - start_o[0] )
			  + ( up[1] - start_o[1] ) * ( up[1] - start_o[1] );

	if( down_dist >= up_dist || trace.allsolid || ( trace.fraction != 1.0 && !ISWALKABLEPLANE( &trace.plane ) ) ) {
		VectorCopy( down_o, pml.origin );
		VectorCopy( down_v, pml.velocity );
		return;
	}

	// only add the stepping output when it was a vertical step (second case is at the exit of a ramp)
	if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) || trace.plane.normal[2] == 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
		pm->step = ( pml.origin[2] - pml.previous_origin[2] );
	}

	// Preserve speed when sliding up ramps
	hspeed = sqrt( start_v[0] * start_v[0] + start_v[1] * start_v[1] );
	if( hspeed && ISWALKABLEPLANE( &trace.plane ) ) {
		if( trace.plane.normal[2] >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
			VectorCopy( start_v, pml.velocity );
		} else {
			VectorNormalize2D( pml.velocity );
			VectorScale2D( pml.velocity, hspeed, pml.velocity );
		}
	}

	// wsw : jal : The following line is what produces the ramp sliding.

	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[2] = down_v[2];
}

/*
* PM_Friction -- Modified for wsw
*
* Handles both ground friction and water friction
*/
static void PM_Friction( void ) {
	float *vel;
	float speed, newspeed, control;
	float friction;
	float drop;

	vel = pml.velocity;

	speed = vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2];
	if( speed < 1 ) {
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	speed = sqrt( speed );
	drop = 0;

	// apply ground friction
	if( ( ( ( ( pm->groundentity != -1 ) && !( pml.groundsurfFlags & SURF_SLICK ) ) )
		  && ( pm->waterlevel < 2 ) ) || pml.ladder ) {
		if( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] <= 0 ) {
			friction = pm_friction;
			control = speed < pm_decelerate ? pm_decelerate : speed;
			if( pm->playerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) {
				if( pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] < PM_CROUCHSLIDE_FADE ) {
					friction *= 1 - sqrt( (float)pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] / PM_CROUCHSLIDE_FADE );
				} else {
					friction = 0;
				}
			}
			drop += control * friction * pml.frametime;
		}
	}

	// apply water friction
	if( ( pm->waterlevel >= 2 ) && !pml.ladder ) {
		drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;
	}

	// scale the velocity
	newspeed = speed - drop;
	if( newspeed <= 0 ) {
		newspeed = 0;
		VectorClear( vel );
	} else {
		newspeed /= speed;
		VectorScale( vel, newspeed, vel );
	}
}

/*
* PM_Accelerate
*
* Handles user intended acceleration
*/
static void PM_Accelerate( vec3_t wishdir, float wishspeed, float accel ) {
	float addspeed, accelspeed, currentspeed, realspeed, newspeed;
	bool crouchslide;

	realspeed = VectorLengthFast( pml.velocity );

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) {
		return;
	}

	accelspeed = accel * pml.frametime * wishspeed;
	if( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	crouchslide = ( pm->playerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) && pm->groundentity != -1 && !( pml.groundsurfFlags & SURF_SLICK );

	if( crouchslide ) {
		accelspeed *= PM_CROUCHSLIDE_CONTROL;
	}

	VectorMA( pml.velocity, accelspeed, wishdir, pml.velocity );

	if( crouchslide ) { // disable overacceleration while crouch sliding
		newspeed = VectorLengthFast( pml.velocity );
		if( newspeed > wishspeed && newspeed != 0 ) {
			VectorScale( pml.velocity, fmax( wishspeed, realspeed ) / newspeed, pml.velocity );
		}
	}
}

static void PM_AirAccelerate( vec3_t wishdir, float wishspeed ) {
	vec3_t heading = { pml.velocity[0], pml.velocity[1], 0 };
	float speed = VectorNormalize( heading );

	// Speed is below player walk speed
	if( speed <= pml.maxPlayerSpeed ) {
		// Apply acceleration
		VectorMA( pml.velocity, pml.maxPlayerSpeed * pml.frametime, wishdir, pml.velocity );
		return;
	}

	// Calculate a dot product between heading and wishdir
	// Looking straight results in better acceleration
	float dot = 50 * ( DotProduct( heading, wishdir ) - 0.98 );
	clamp( dot, 0, 1 );

	// Calculate resulting acceleration
	float accel = dot * pml.maxPlayerSpeed * pml.maxPlayerSpeed * pml.maxPlayerSpeed / ( speed * speed );

	// Apply acceleration
	VectorMA( pml.velocity, accel * pml.frametime, heading, pml.velocity );
}

// when using +strafe convert the inertia to forward speed.
static void PM_Aircontrol( vec3_t wishdir, float wishspeed ) {
	int i;
	float zspeed, speed, dot, k;
	float smove;

	if( !pm_aircontrol ) {
		return;
	}

	// accelerate
	smove = pml.sidePush;

	if( ( smove > 0 || smove < 0 ) || ( wishspeed == 0.0 ) ) {
		return; // can't control movement if not moving forward or backward

	}
	zspeed = pml.velocity[2];
	pml.velocity[2] = 0;
	speed = VectorNormalize( pml.velocity );


	dot = DotProduct( pml.velocity, wishdir );
	k = 32.0f * pm_aircontrol * dot * dot * pml.frametime;

	if( dot > 0 ) {
		// we can't change direction while slowing down
		for( i = 0; i < 2; i++ )
			pml.velocity[i] = pml.velocity[i] * speed + wishdir[i] * k;

		VectorNormalize( pml.velocity );
	}

	for( i = 0; i < 2; i++ )
		pml.velocity[i] *= speed;

	pml.velocity[2] = zspeed;
}

#if 0 // never used
static void PM_AirAccelerate( vec3_t wishdir, float wishspeed, float accel ) {
	int i;
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if( wishspd > 30 ) {
		wishspd = 30;
	}
	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspd - currentspeed;
	if( addspeed <= 0 ) {
		return;
	}
	accelspeed = accel * wishspeed * pml.frametime;
	if( accelspeed > addspeed ) {
		accelspeed = addspeed;
	}

	for( i = 0; i < 3; i++ )
		pml.velocity[i] += accelspeed * wishdir[i];
}
#endif


/*
* PM_AddCurrents
*/
static void PM_AddCurrents( vec3_t wishvel ) {
	//
	// account for ladders
	//

	if( pml.ladder && fabs( pml.velocity[2] ) <= DEFAULT_LADDERSPEED ) {
		if( ( pm->playerState->viewangles[PITCH] <= -15 ) && ( pml.forwardPush > 0 ) ) {
			wishvel[2] = DEFAULT_LADDERSPEED;
		} else if( ( pm->playerState->viewangles[PITCH] >= 15 ) && ( pml.forwardPush > 0 ) ) {
			wishvel[2] = -DEFAULT_LADDERSPEED;
		} else if( pml.upPush > 0 ) {
			wishvel[2] = DEFAULT_LADDERSPEED;
		} else if( pml.upPush < 0 ) {
			wishvel[2] = -DEFAULT_LADDERSPEED;
		} else {
			wishvel[2] = 0;
		}

		// limit horizontal speed when on a ladder
		clamp( wishvel[0], -25, 25 );
		clamp( wishvel[1], -25, 25 );
	}
}

/*
* PM_WaterMove
*
*/
static void PM_WaterMove( void ) {
	int i;
	vec3_t wishvel;
	float wishspeed;
	vec3_t wishdir;

	// user intentions
	for( i = 0; i < 3; i++ )
		wishvel[i] = pml.forward[i] * pml.forwardPush + pml.right[i] * pml.sidePush;

	if( !pml.forwardPush && !pml.sidePush && !pml.upPush ) {
		wishvel[2] -= 60; // drift towards bottom
	} else {
		wishvel[2] += pml.upPush;
	}

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	if( wishspeed > pml.maxPlayerSpeed ) {
		wishspeed = pml.maxPlayerSpeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = pml.maxPlayerSpeed;
	}
	wishspeed *= 0.5;

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );
	PM_StepSlideMove();
}

/*
* PM_Move -- Kurim
*
*/
static void PM_Move( void ) {
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	float maxspeed;
	float accel;
	float wishspeed2;

	fmove = pml.forwardPush;
	smove = pml.sidePush;

	for( i = 0; i < 2; i++ )
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] = 0;

	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	// clamp to server defined max speed

	if( pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] ) {
		maxspeed = pml.maxCrouchedSpeed;
	} else if( ( pm->cmd.buttons & BUTTON_WALK ) && ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WALK ) ) {
		maxspeed = pml.maxWalkSpeed;
	} else {
		maxspeed = pml.maxPlayerSpeed;
	}

	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = maxspeed;
	}

	if( pml.ladder ) {
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		if( !wishvel[2] ) {
			if( pml.velocity[2] > 0 ) {
				pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
				if( pml.velocity[2] < 0 ) {
					pml.velocity[2]  = 0;
				}
			} else {
				pml.velocity[2] += pm->playerState->pmove.gravity * pml.frametime;
				if( pml.velocity[2] > 0 ) {
					pml.velocity[2]  = 0;
				}
			}
		}

		PM_StepSlideMove();
	} else if( pm->groundentity != -1 ) {
		// walking on ground
		if( pml.velocity[2] > 0 ) {
			pml.velocity[2] = 0; //!!! this is before the accel

		}
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );

		// fix for negative trigger_gravity fields
		if( pm->playerState->pmove.gravity > 0 ) {
			if( pml.velocity[2] > 0 ) {
				pml.velocity[2] = 0;
			}
		} else {
			pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		}

		if( !pml.velocity[0] && !pml.velocity[1] ) {
			return;
		}

		PM_StepSlideMove();
	} else if( ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_AIRCONTROL )
			   && !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_FWDBUNNY ) ) {
		// Air Control
		wishspeed2 = wishspeed;
		if( DotProduct( pml.velocity, wishdir ) < 0
			&& !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING )
			&& ( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] <= 0 ) ) {
			accel = pm_airdecelerate;
		} else {
			accel = pm_airaccelerate;
		}

		// ch : remove knockback test here
		if( ( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING )
		    /* || ( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] > 0 ) */ ) {
			accel = 0; // no stopmove while walljumping

		}
		if( ( smove > 0 || smove < 0 ) && !fmove && ( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] <= 0 ) ) {
			if( wishspeed > pm_wishspeed ) {
				wishspeed = pm_wishspeed;
			}
			accel = pm_strafebunnyaccel;
		}

		// Air control
		PM_Accelerate( wishdir, wishspeed, accel );
		if( pm_aircontrol && !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) && ( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] <= 0 ) ) { // no air ctrl while wjing
			PM_Aircontrol( wishdir, wishspeed2 );
		}

		// add gravity
		pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		PM_StepSlideMove();
	} else {   // air movement (old school)
		bool inhibit = false;
		bool accelerating, decelerating;

		accelerating = ( DotProduct( pml.velocity, wishdir ) > 0.0f ) ? true : false;
		decelerating = ( DotProduct( pml.velocity, wishdir ) < -0.0f ) ? true : false;

		if( ( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) &&
			( pm->playerState->pmove.stats[PM_STAT_WJTIME] >= ( PM_WALLJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
			inhibit = true;
		}

		if( ( pm->playerState->pmove.pm_flags & PMF_DASHING ) &&
			( pm->playerState->pmove.stats[PM_STAT_DASHTIME] >= ( PM_DASHJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
			inhibit = true;
		}

		if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_FWDBUNNY ) ||
			pm->playerState->pmove.stats[PM_STAT_FWDTIME] > 0 ) {
			inhibit = true;
		}

		// ch : remove this because of the knockback 'bug'?
		/*
		if( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] > 0 )
		    inhibit = true;
		*/

		// (aka +fwdbunny) pressing forward or backward but not pressing strafe and not dashing
		if( accelerating && !inhibit && !smove && fmove ) {
			PM_AirAccelerate( wishdir, wishspeed );
			PM_Aircontrol( wishdir, wishspeed );
		} else {   // strafe running
			bool aircontrol = true;

			wishspeed2 = wishspeed;
			if( decelerating &&
				!( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
				accel = pm_airdecelerate;
			} else {
				accel = pm_airaccelerate;
			}

			// ch : knockback out
			if( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING
			    /*	|| ( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] > 0 ) */) {
				accel = 0; // no stop-move while wall-jumping
				aircontrol = false;
			}

			if( ( pm->playerState->pmove.pm_flags & PMF_DASHING ) &&
				( pm->playerState->pmove.stats[PM_STAT_DASHTIME] >= ( PM_DASHJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
				aircontrol = false;
			}

			if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_AIRCONTROL ) ) {
				aircontrol = false;
			}

			// +strafe bunnyhopping
			if( aircontrol && smove && !fmove ) {
				if( wishspeed > pm_wishspeed ) {
					wishspeed = pm_wishspeed;
				}

				PM_Accelerate( wishdir, wishspeed, pm_strafebunnyaccel );
				PM_Aircontrol( wishdir, wishspeed2 );
			} else {   // standard movement (includes strafejumping)
				PM_Accelerate( wishdir, wishspeed, accel );
			}
		}

		// add gravity
		pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		PM_StepSlideMove();
	}
}

/*
* PM_GroundTrace
*
* If the player hull point one-quarter unit down is solid, the player is on ground
*/
static void PM_GroundTrace( trace_t *trace ) {
	vec3_t point;

	if( pm->skipCollision ) {
		memset( trace, 0, sizeof( trace_t ) );
		trace->fraction = 1.0f;
		return;
	}

	// see if standing on something solid
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25;

	gs.api.Trace( trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );
}

/*
* PM_GoodPosition
*/
static bool PM_GoodPosition( vec3_t origin, trace_t *trace ) {
	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		return true;
	}

	gs.api.Trace( trace, origin, pm->mins, pm->maxs, origin, pm->playerState->POVnum, pm->contentmask, 0 );

	return !trace->allsolid;
}

/*
* PM_UnstickPosition
*/
static void PM_UnstickPosition( trace_t *trace ) {
	int j;
	vec3_t origin;

	VectorCopy( pml.origin, origin );

	// try all combinations
	for( j = 0; j < 8; j++ ) {
		VectorCopy( pml.origin, origin );

		origin[0] += ( ( j & 1 ) ? -1 : 1 );
		origin[1] += ( ( j & 2 ) ? -1 : 1 );
		origin[2] += ( ( j & 4 ) ? -1 : 1 );

		if( PM_GoodPosition( origin, trace ) ) {
			VectorCopy( origin, pml.origin );
			PM_GroundTrace( trace );
			return;
		}
	}

	// go back to the last position
	VectorCopy( pml.previous_origin, pml.origin );
}

/*
* PM_CategorizePosition
*/
static void PM_CategorizePosition( void ) {
	vec3_t point;
	int cont;
	int sample1;
	int sample2;

	if( pml.velocity[2] > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
		pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = -1;
	} else {
		trace_t trace;

		// see if standing on something solid
		PM_GroundTrace( &trace );

		if( trace.allsolid ) {
			// try to unstick position
			PM_UnstickPosition( &trace );
		}

		pml.groundplane = trace.plane;
		pml.groundsurfFlags = trace.surfFlags;
		pml.groundcontents = trace.contents;

		if( ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			pm->groundentity = -1;
			pm->playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
		} else {
			pm->groundentity = trace.ent;
			pm->groundplane = trace.plane;
			pm->groundsurfFlags = trace.surfFlags;
			pm->groundcontents = trace.contents;

			// hitting solid ground will end a waterjump
			if( pm->playerState->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
				pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
				pm->playerState->pmove.pm_time = 0;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) ) { // just hit the ground
				pm->playerState->pmove.pm_flags |= PMF_ON_GROUND;
			}
		}

		if( trace.fraction < 1.0 ) {
			PM_AddTouchEnt( trace.ent );
		}
	}

	//
	// get waterlevel, accounting for ducking
	//
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->playerState->viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] + pm->mins[2] + 1;
	cont = gs.api.PointContents( point, 0 );

	if( cont & MASK_WATER ) {
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = gs.api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = gs.api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				pm->waterlevel = 3;
			}
		}
	}
}

static void PM_ClearDash( void ) {
	pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
	pm->playerState->pmove.stats[PM_STAT_DASHTIME] = 0;
}

static void PM_ClearWallJump( void ) {
	pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	pm->playerState->pmove.stats[PM_STAT_WJTIME] = 0;
}

static void PM_ClearStun( void ) {
	pm->playerState->pmove.stats[PM_STAT_STUN] = 0;
}

/*
* PM_CheckJump
*/
static void PM_CheckJump( void ) {
	if( pml.upPush < 10 ) {
		// not holding jump
		if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) ) {
			pm->playerState->pmove.pm_flags &= ~PMF_JUMP_HELD;
		}
		return;
	}

	if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) ) {
		// must wait for jump to be released
		if( pm->playerState->pmove.pm_flags & PMF_JUMP_HELD ) {
			return;
		}
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( pm->waterlevel >= 2 ) { // swimming, not jumping
		pm->groundentity = -1;
		return;
	}

	if( pm->groundentity == -1 ) {
		return;
	}

	if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_JUMP ) ) {
		return;
	}

	if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) ) {
		pm->playerState->pmove.pm_flags |= PMF_JUMP_HELD;
	}

	pm->groundentity = -1;

	// clip against the ground when jumping if moving that direction
	if( pml.groundplane.normal[2] > 0 && pml.velocity[2] < 0 && DotProduct2D( pml.groundplane.normal, pml.velocity ) > 0 ) {
		GS_ClipVelocity( pml.velocity, pml.groundplane.normal, pml.velocity, PM_OVERBOUNCE );
	}

	pm->playerState->pmove.skim_time = PM_SKIM_TIME;

	//if( gs.module == GS_MODULE_GAME ) GS_Printf( "upvel %f\n", pml.velocity[2] );
	if( pml.velocity[2] > 100 ) {
		gs.api.PredictedEvent( pm->playerState->POVnum, EV_DOUBLEJUMP, 0 );
		pml.velocity[2] += pml.jumpPlayerSpeed;
	} else if( pml.velocity[2] > 0 ) {
		gs.api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
		pml.velocity[2] += pml.jumpPlayerSpeed;
	} else {
		gs.api.PredictedEvent( pm->playerState->POVnum, EV_JUMP, 0 );
		pml.velocity[2] = pml.jumpPlayerSpeed;
	}

	// remove wj count
	pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
	PM_ClearDash();
	PM_ClearWallJump();
}

/*
* PM_CheckDash -- by Kurim
*/
static void PM_CheckDash( void ) {
	float actual_velocity;
	float upspeed;
	vec3_t dashdir;

	if( !( pm->cmd.buttons & BUTTON_SPECIAL ) ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	if( pm->playerState->pmove.stats[PM_STAT_DASHTIME] > 0 ) {
		return;
	}

	if( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] > 0 ) { // can not start a new dash during knockback time
		return;
	}

	if( ( pm->cmd.buttons & BUTTON_SPECIAL ) && pm->groundentity != -1
		&& ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_DASH ) ) {
		if( pm->playerState->pmove.pm_flags & PMF_SPECIAL_HELD ) {
			return;
		}

		pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
		PM_ClearWallJump();

		pm->playerState->pmove.pm_flags |= PMF_DASHING;
		pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;
		pm->groundentity = -1;

		// clip against the ground when jumping if moving that direction
		if( pml.groundplane.normal[2] > 0 && pml.velocity[2] < 0 && DotProduct2D( pml.groundplane.normal, pml.velocity ) > 0 ) {
			GS_ClipVelocity( pml.velocity, pml.groundplane.normal, pml.velocity, PM_OVERBOUNCE );
		}

		if( pml.velocity[2] <= 0.0f ) {
			upspeed = pm_dashupspeed;
		} else {
			upspeed = pm_dashupspeed + pml.velocity[2];
		}

		// ch : we should do explicit forwardPush here, and ignore sidePush ?
		VectorMA( vec3_origin, pml.forwardPush, pml.flatforward, dashdir );
		VectorMA( dashdir, pml.sidePush, pml.right, dashdir );
		dashdir[2] = 0.0;

		if( VectorLength( dashdir ) < 0.01f ) { // if not moving, dash like a "forward dash"
			VectorCopy( pml.flatforward, dashdir );
		}

		VectorNormalizeFast( dashdir );

		actual_velocity = VectorNormalize2D( pml.velocity );
		if( actual_velocity <= pml.dashPlayerSpeed ) {
			VectorScale( dashdir, pml.dashPlayerSpeed, dashdir );
		} else {
			VectorScale( dashdir, actual_velocity, dashdir );
		}

		VectorCopy( dashdir, pml.velocity );
		pml.velocity[2] = upspeed;

		pm->playerState->pmove.stats[PM_STAT_DASHTIME] = PM_DASHJUMP_TIMEDELAY;

		pm->playerState->pmove.skim_time = PM_SKIM_TIME;

		// return sound events
		if( fabs( pml.sidePush ) > 10 && fabs( pml.sidePush ) >= fabs( pml.forwardPush ) ) {
			if( pml.sidePush > 0 ) {
				gs.api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 2 );
			} else {
				gs.api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 1 );
			}
		} else if( pml.forwardPush < -10 ) {
			gs.api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 3 );
		} else {
			gs.api.PredictedEvent( pm->playerState->POVnum, EV_DASH, 0 );
		}
	} else if( pm->groundentity == -1 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
	}
}

/*
* PM_CheckWallJump -- By Kurim
*/
static void PM_CheckWallJump( void ) {
	vec3_t normal;
	float hspeed;

	if( !( pm->cmd.buttons & BUTTON_SPECIAL ) ) {
		pm->playerState->pmove.pm_flags &= ~PMF_SPECIAL_HELD;
	}

	if( pm->groundentity != -1 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	}

	if( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING && pml.velocity[2] < 0.0 ) {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}

	if( pm->playerState->pmove.stats[PM_STAT_WJTIME] <= 0 ) { // reset the wj count after wj delay
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	// don't walljump in the first 100 milliseconds of a dash jump
	if( pm->playerState->pmove.pm_flags & PMF_DASHING
		&& ( pm->playerState->pmove.stats[PM_STAT_DASHTIME] > ( PM_DASHJUMP_TIMEDELAY - 100 ) ) ) {
		return;
	}


	// markthis

	if( pm->groundentity == -1 && ( pm->cmd.buttons & BUTTON_SPECIAL )
		&& ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WALLJUMP ) &&
		( !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPCOUNT ) )
		&& pm->playerState->pmove.stats[PM_STAT_WJTIME] <= 0
		&& !pm->skipCollision
		) {
		trace_t trace;
		vec3_t point;

		point[0] = pml.origin[0];
		point[1] = pml.origin[1];
		point[2] = pml.origin[2] - STEPSIZE;

		// don't walljump if our height is smaller than a step
		// unless jump is pressed or the player is moving faster than dash speed and upwards
		hspeed = VectorLengthFast( tv( pml.velocity[0], pml.velocity[1], 0 ) );
		gs.api.Trace( &trace, pml.origin, pm->mins, pm->maxs, point, pm->playerState->POVnum, pm->contentmask, 0 );

		if( pml.upPush >= 10
			|| ( hspeed > pm->playerState->pmove.stats[PM_STAT_DASHSPEED] && pml.velocity[2] > 8 )
			|| ( trace.fraction == 1 ) || ( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
			VectorClear( normal );
			PlayerTouchWall( 20, 0.3f, &normal );
			if( !VectorLength( normal ) ) {
				return;
			}

			if( !( pm->playerState->pmove.pm_flags & PMF_SPECIAL_HELD )
				&& !( pm->playerState->pmove.pm_flags & PMF_WALLJUMPING ) ) {
				float oldupvelocity = pml.velocity[2];
				pml.velocity[2] = 0.0;

				hspeed = VectorNormalize2D( pml.velocity );

				// if stunned almost do nothing
				if( pm->playerState->pmove.stats[PM_STAT_STUN] > 0 ) {
					GS_ClipVelocity( pml.velocity, normal, pml.velocity, 1.0f );
					VectorMA( pml.velocity, pm_failedwjbouncefactor, normal, pml.velocity );

					VectorNormalize( pml.velocity );

					VectorScale( pml.velocity, hspeed, pml.velocity );
					pml.velocity[2] = ( oldupvelocity + pm_failedwjupspeed > pm_failedwjupspeed ) ? oldupvelocity : oldupvelocity + pm_failedwjupspeed;
				} else {
					GS_ClipVelocity( pml.velocity, normal, pml.velocity, 1.0005f );
					VectorMA( pml.velocity, pm_wjbouncefactor, normal, pml.velocity );

					if( hspeed < pm_wjminspeed ) {
						hspeed = pm_wjminspeed;
					}

					VectorNormalize( pml.velocity );

					VectorScale( pml.velocity, hspeed, pml.velocity );
					pml.velocity[2] = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it
				}

				// set the walljumping state
				PM_ClearDash();
				pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPING;
				pm->playerState->pmove.pm_flags |= PMF_SPECIAL_HELD;

				pm->playerState->pmove.pm_flags |= PMF_WALLJUMPCOUNT;

				if( pm->playerState->pmove.stats[PM_STAT_STUN] > 0 ) {
					pm->playerState->pmove.stats[PM_STAT_WJTIME] = PM_WALLJUMP_FAILED_TIMEDELAY;

					// Create the event
					gs.api.PredictedEvent( pm->playerState->POVnum, EV_WALLJUMP_FAILED, DirToByte( normal ) );
				} else {
					pm->playerState->pmove.stats[PM_STAT_WJTIME] = PM_WALLJUMP_TIMEDELAY;
					pm->playerState->pmove.skim_time = PM_SKIM_TIME;

					// Create the event
					gs.api.PredictedEvent( pm->playerState->POVnum, EV_WALLJUMP, DirToByte( normal ) );
				}
			}
		}
	} else {
		pm->playerState->pmove.pm_flags &= ~PMF_WALLJUMPING;
	}
}

/*
* PM_CheckCrouchSlide
*/
static void PM_CheckCrouchSlide( void ) {
	if( !( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CROUCHSLIDING ) ) {
		return;
	}

	if( pml.upPush < 0 && VectorLengthFast( tv( pml.velocity[0], pml.velocity[1], 0 ) ) > pml.maxWalkSpeed ) {
		if( pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] > 0 ) {
			return; // cooldown or already sliding

		}
		if( pm->groundentity != -1 ) {
			return; // already on the ground

		}
		// start sliding when we land
		pm->playerState->pmove.pm_flags |= PMF_CROUCH_SLIDING;
		pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] = PM_CROUCHSLIDE + PM_CROUCHSLIDE_FADE;
	} else if( pm->playerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) {
		pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] = min( pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME], PM_CROUCHSLIDE_FADE );
	}
}

/*
* PM_CheckSpecialMovement
*/
static void PM_CheckSpecialMovement( void ) {
	vec3_t spot;
	int cont;
	trace_t trace;

	pm->ladder = false;

	if( pm->playerState->pmove.pm_time ) {
		return;
	}

	pml.ladder = false;

	// check for ladder
	if( !pm->skipCollision ) {
		VectorMA( pml.origin, 1, pml.flatforward, spot );
		gs.api.Trace( &trace, pml.origin, pm->mins, pm->maxs, spot, pm->playerState->POVnum, pm->contentmask, 0 );
		if( ( trace.fraction < 1 ) && ( trace.surfFlags & SURF_LADDER ) ) {
			pml.ladder = true;
			pm->ladder = true;
		}
	}

	// check for water jump
	if( pm->waterlevel != 2 ) {
		return;
	}

	VectorMA( pml.origin, 30, pml.flatforward, spot );
	spot[2] += 4;
	cont = gs.api.PointContents( spot, 0 );
	if( !( cont & CONTENTS_SOLID ) ) {
		return;
	}

	spot[2] += 16;
	cont = gs.api.PointContents( spot, 0 );
	if( cont ) {
		return;
	}
	// jump out of water
	VectorScale( pml.flatforward, 50, pml.velocity );
	pml.velocity[2] = 350;

	pm->playerState->pmove.pm_flags |= PMF_TIME_WATERJUMP;
	pm->playerState->pmove.pm_time = 255;
}

/*
* PM_FlyMove
*/
static void PM_FlyMove( bool doclip ) {
	float speed, drop, friction, control, newspeed;
	float currentspeed, addspeed, accelspeed, maxspeed;
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	vec3_t end;
	trace_t trace;

	maxspeed = pml.maxPlayerSpeed * 1.5;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		maxspeed *= 2;
	}

	// friction
	speed = VectorLength( pml.velocity );
	if( speed < 1 ) {
		VectorClear( pml.velocity );
	} else {
		drop = 0;

		friction = pm_friction * 1.5; // extra friction
		control = speed < pm_decelerate ? pm_decelerate : speed;
		drop += control * friction * pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if( newspeed < 0 ) {
			newspeed = 0;
		}
		newspeed /= speed;

		VectorScale( pml.velocity, newspeed, pml.velocity );
	}

	// accelerate
	fmove = pml.forwardPush;
	smove = pml.sidePush;

	if( pm->cmd.buttons & BUTTON_SPECIAL ) {
		fmove *= 2;
		smove *= 2;
	}

	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pml.upPush;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );


	// clamp to server defined max speed
	//
	if( wishspeed > maxspeed ) {
		wishspeed = maxspeed / wishspeed;
		VectorScale( wishvel, wishspeed, wishvel );
		wishspeed = maxspeed;
	}

	currentspeed = DotProduct( pml.velocity, wishdir );
	addspeed = wishspeed - currentspeed;
	if( addspeed > 0 ) {
		accelspeed = pm_accelerate * pml.frametime * wishspeed;
		if( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}

		for( i = 0; i < 3; i++ )
			pml.velocity[i] += accelspeed * wishdir[i];
	}

	if( doclip ) {
		for( i = 0; i < 3; i++ )
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];

		gs.api.Trace( &trace, pml.origin, pm->mins, pm->maxs, end, pm->playerState->POVnum, pm->contentmask, 0 );

		VectorCopy( trace.endpos, pml.origin );
	} else {
		// move
		VectorMA( pml.origin, pml.frametime, pml.velocity, pml.origin );
	}
}

static void PM_CheckZoom( void ) {
	if( pm->playerState->pmove.pm_type != PM_NORMAL ) {
		pm->playerState->pmove.stats[PM_STAT_ZOOMTIME] = 0;
		return;
	}

	if( ( pm->cmd.buttons & BUTTON_ZOOM ) && ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_ZOOM ) ) {
		pm->playerState->pmove.stats[PM_STAT_ZOOMTIME] += pm->cmd.msec;
		clamp( pm->playerState->pmove.stats[PM_STAT_ZOOMTIME], 0, ZOOMTIME );
	} else if( pm->playerState->pmove.stats[PM_STAT_ZOOMTIME] > 0 ) {
		pm->playerState->pmove.stats[PM_STAT_ZOOMTIME] -= pm->cmd.msec;
		clamp( pm->playerState->pmove.stats[PM_STAT_ZOOMTIME], 0, ZOOMTIME );
	}
}

/*
* PM_AdjustBBox
*
* Sets mins, maxs, and pm->viewheight
*/
static void PM_AdjustBBox( void ) {
	float crouchFrac;
	trace_t trace;

	if( pm->playerState->pmove.pm_type == PM_GIB ) {
		pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] = 0;
		VectorCopy( playerbox_gib_maxs, pm->maxs );
		VectorCopy( playerbox_gib_mins, pm->mins );
		pm->playerState->viewheight = playerbox_gib_viewheight;
		return;
	}

	if( pm->playerState->pmove.pm_type >= PM_FREEZE ) {
		pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] = 0;
		pm->playerState->viewheight = 0;
		return;
	}

	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] = 0;
		pm->playerState->viewheight = playerbox_stand_viewheight;
	}

	if( pml.upPush < 0 && ( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CROUCH ) &&
		pm->playerState->pmove.stats[PM_STAT_WJTIME] < ( PM_WALLJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) &&
		pm->playerState->pmove.stats[PM_STAT_DASHTIME] < ( PM_DASHJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) ) {
		pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] += pm->cmd.msec;
		clamp( pm->playerState->pmove.stats[PM_STAT_CROUCHTIME], 0, CROUCHTIME );

		crouchFrac = (float)pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, pm->mins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, pm->maxs );
		pm->playerState->viewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		// it's going down, so, no need of checking for head-chomping
		return;
	}

	// it's crouched, but not pressing the crouch button anymore, try to stand up
	if( pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] != 0 ) {
		vec3_t curmins, curmaxs, wishmins, wishmaxs;
		float curviewheight, wishviewheight;
		int newcrouchtime;

		// find the current size
		crouchFrac = (float)pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, curmins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, curmaxs );
		curviewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		if( !pm->cmd.msec ) { // no need to continue
			VectorCopy( curmins, pm->mins );
			VectorCopy( curmaxs, pm->maxs );
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// find the desired size
		newcrouchtime = pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] - pm->cmd.msec;
		clamp( newcrouchtime, 0, CROUCHTIME );
		crouchFrac = (float)newcrouchtime / (float)CROUCHTIME;
		VectorLerp( playerbox_stand_mins, crouchFrac, playerbox_crouch_mins, wishmins );
		VectorLerp( playerbox_stand_maxs, crouchFrac, playerbox_crouch_maxs, wishmaxs );
		wishviewheight = playerbox_stand_viewheight - ( crouchFrac * ( playerbox_stand_viewheight - playerbox_crouch_viewheight ) );

		// check that the head is not blocked
		gs.api.Trace( &trace, pml.origin, wishmins, wishmaxs, pml.origin, pm->playerState->POVnum, pm->contentmask, 0 );
		if( trace.allsolid || trace.startsolid ) {
			// can't do the uncrouching, let the time alone and use old position
			VectorCopy( curmins, pm->mins );
			VectorCopy( curmaxs, pm->maxs );
			pm->playerState->viewheight = curviewheight;
			return;
		}

		// can do the uncrouching, use new position and update the time
		pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] = newcrouchtime;
		VectorCopy( wishmins, pm->mins );
		VectorCopy( wishmaxs, pm->maxs );
		pm->playerState->viewheight = wishviewheight;
		return;
	}

	// the player is not crouching at all
	VectorCopy( playerbox_stand_mins, pm->mins );
	VectorCopy( playerbox_stand_maxs, pm->maxs );
	pm->playerState->viewheight = playerbox_stand_viewheight;
}

/*
* PM_AdjustViewheight
*/
void PM_AdjustViewheight( void ) {
	float height;
	vec3_t pm_maxs, mins, maxs;

	if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		VectorCopy( playerbox_stand_mins, mins );
		VectorCopy( playerbox_stand_maxs, maxs );
	} else {
		VectorCopy( pm->mins, mins );
		VectorCopy( pm->maxs, maxs );
	}

	VectorCopy( maxs, pm_maxs );
	gs.api.RoundUpToHullSize( mins, maxs );

	height = pm_maxs[2] - maxs[2];
	if( height > 0 ) {
		pm->playerState->viewheight -= height;
	}
}

/*
* PM_ClampAngles
*
*/
#if defined ( _WIN32 ) && ( _MSC_VER >= 1400 )
#pragma warning( push )
#pragma warning( disable : 4310 )   // cast truncates constant value
#endif
static void PM_ClampAngles( pmove_t *pmove ) {
	int i;
	short temp;

	for( i = 0; i < 3; i++ ) {
		temp = pmove->cmd.angles[i] + pmove->playerState->pmove.delta_angles[i];
		if( i == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if( temp > (short)ANGLE2SHORT( 90 ) - 1 ) {
				pmove->playerState->pmove.delta_angles[i] = ( ANGLE2SHORT( 90 ) - 1 ) - pmove->cmd.angles[i];
				temp = (short)ANGLE2SHORT( 90 ) - 1;
			} else if( temp < (short)ANGLE2SHORT( -90 ) + 1 ) {
				pmove->playerState->pmove.delta_angles[i] = ( ANGLE2SHORT( -90 ) + 1 ) - pmove->cmd.angles[i];
				temp = (short)ANGLE2SHORT( -90 ) + 1;
			}
		}

		pmove->playerState->viewangles[i] = SHORT2ANGLE( (short)temp );
	}
}
#if defined ( _WIN32 ) && ( _MSC_VER >= 1400 )
#pragma warning( pop )
#endif

/*
* PM_BeginMove
*/
static void PM_BeginMove( void ) {
	// clear results
	pm->numtouch = 0;
	pm->groundentity = -1;
	pm->watertype = 0;
	pm->waterlevel = 0;
	pm->step = false;

	// clear all pmove local vars
	memset( &pml, 0, sizeof( pml ) );

	VectorCopy( pm->playerState->pmove.origin, pml.origin );
	VectorCopy( pm->playerState->pmove.velocity, pml.velocity );

	// save old org in case we get stuck
	VectorCopy( pm->playerState->pmove.origin, pml.previous_origin );

	AngleVectors( pm->playerState->viewangles, pml.forward, pml.right, pml.up );

	VectorCopy( pml.forward, pml.flatforward );
	pml.flatforward[2] = 0.0f;
	VectorNormalize( pml.flatforward );
}

/*
* PM_EndMove
*/
static void PM_EndMove( void ) {
	VectorCopy( pml.origin, pm->playerState->pmove.origin );
	VectorCopy( pml.velocity, pm->playerState->pmove.velocity );
}

/*
* _Pmove
*/
static void _Pmove( pmove_t *pmove ) {
	float fallvelocity, falldelta, damage;
	int oldGroundEntity;

	pm = pmove;

	// clear all pmove local vars
	PM_BeginMove();

	fallvelocity = ( ( pml.velocity[2] < 0.0f ) ? fabs( pml.velocity[2] ) : 0.0f );

	pml.frametime = pm->cmd.msec * 0.001;

	pml.maxPlayerSpeed = pm->playerState->pmove.stats[PM_STAT_MAXSPEED];
	if( pml.maxPlayerSpeed < 0 ) {
		pml.maxPlayerSpeed = DEFAULT_PLAYERSPEED;
	}

	pml.jumpPlayerSpeed = (float)pm->playerState->pmove.stats[PM_STAT_JUMPSPEED] * GRAVITY_COMPENSATE;
	if( pml.jumpPlayerSpeed < 0 ) {
		pml.jumpPlayerSpeed = DEFAULT_JUMPSPEED * GRAVITY_COMPENSATE;
	}

	pml.dashPlayerSpeed = pm->playerState->pmove.stats[PM_STAT_DASHSPEED];
	if( pml.dashPlayerSpeed < 0 ) {
		pml.dashPlayerSpeed = DEFAULT_DASHSPEED;
	}

	pml.maxWalkSpeed = DEFAULT_WALKSPEED;
	if( pml.maxWalkSpeed > pml.maxPlayerSpeed * 0.66f ) {
		pml.maxWalkSpeed = pml.maxPlayerSpeed * 0.66f;
	}

	pml.maxCrouchedSpeed = DEFAULT_CROUCHEDSPEED;
	if( pml.maxCrouchedSpeed > pml.maxPlayerSpeed * 0.5f ) {
		pml.maxCrouchedSpeed = pml.maxPlayerSpeed * 0.5f;
	}

	// assign a contentmask for the movement type
	switch( pm->playerState->pmove.pm_type ) {
		case PM_FREEZE:
		case PM_CHASECAM:
			if( gs.module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags |= PMF_NO_PREDICTION;
			}
			pm->contentmask = 0;
			break;

		case PM_GIB:
			if( gs.module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags |= PMF_NO_PREDICTION;
			}
			pm->contentmask = MASK_DEADSOLID;
			break;

		case PM_SPECTATOR:
			if( gs.module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			pm->contentmask = MASK_DEADSOLID;
			break;

		default:
		case PM_NORMAL:
			if( gs.module == GS_MODULE_GAME ) {
				pm->playerState->pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			if( pm->playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_GHOSTMOVE ) {
				pm->contentmask = MASK_DEADSOLID;
			} else {
				pm->contentmask = MASK_PLAYERSOLID;
			}
			break;
	}

	if( !GS_MatchPaused() ) {
		// drop timing counters
		if( pm->playerState->pmove.pm_time ) {
			int msec;

			msec = pm->cmd.msec >> 3;
			if( !msec ) {
				msec = 1;
			}
			if( msec >= pm->playerState->pmove.pm_time ) {
				pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
				pm->playerState->pmove.pm_time = 0;
			} else {
				pm->playerState->pmove.pm_time -= msec;
			}
		}

		if( pm->playerState->pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_NOUSERCONTROL] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_NOUSERCONTROL] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_NOUSERCONTROL] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] = 0;
		}

		// PM_STAT_CROUCHTIME is handled at PM_AdjustBBox
		// PM_STAT_ZOOMTIME is handled at PM_CheckZoom

		if( pm->playerState->pmove.stats[PM_STAT_DASHTIME] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_DASHTIME] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_DASHTIME] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_DASHTIME] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_WJTIME] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_WJTIME] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_WJTIME] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_WJTIME] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_NOAUTOATTACK] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_NOAUTOATTACK] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_NOAUTOATTACK] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_NOAUTOATTACK] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_STUN] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_STUN] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_STUN] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_STUN] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_FWDTIME] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_FWDTIME] -= pm->cmd.msec;
		} else if( pm->playerState->pmove.stats[PM_STAT_FWDTIME] < 0 ) {
			pm->playerState->pmove.stats[PM_STAT_FWDTIME] = 0;
		}

		if( pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] > 0 ) {
			pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] -= pm->cmd.msec;
			if( pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] <= 0 ) {
				if( pm->playerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) {
					pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] = PM_CROUCHSLIDE_TIMEDELAY;
				} else {
					pm->playerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] = 0;
				}
				pm->playerState->pmove.pm_flags &= ~PMF_CROUCH_SLIDING;
			}
		}
	}

	pml.forwardPush = pm->cmd.forwardmove * SPEEDKEY / 127.0f;
	pml.sidePush = pm->cmd.sidemove * SPEEDKEY / 127.0f;
	pml.upPush = pm->cmd.upmove * SPEEDKEY / 127.0f;

	if( pm->playerState->pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
		pml.forwardPush = 0;
		pml.sidePush = 0;
		pml.upPush = 0;
		pm->cmd.buttons = 0;
	}

	// in order the forward accelt to kick in, one has to keep +fwd pressed
	// for some time without strafing
	if( pml.forwardPush <= 0 || pml.sidePush ) {
		pm->playerState->pmove.stats[PM_STAT_FWDTIME] = PM_FORWARD_ACCEL_TIMEDELAY;
	}

	if( pm->playerState->pmove.pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( !GS_MatchPaused() ) {
			PM_ClearDash();

			PM_ClearWallJump();

			PM_ClearStun();

			pm->playerState->pmove.stats[PM_STAT_KNOCKBACK] = 0;
			pm->playerState->pmove.stats[PM_STAT_CROUCHTIME] = 0;
			pm->playerState->pmove.stats[PM_STAT_ZOOMTIME] = 0;
			pm->playerState->pmove.pm_flags &= ~( PMF_JUMPPAD_TIME | PMF_DOUBLEJUMPED | PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_SPECIAL_HELD );

			PM_AdjustBBox();
		}

		PM_AdjustViewheight();

		if( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
			PM_FlyMove( false );
		} else {
			pml.forwardPush = pml.sidePush = pml.upPush = 0;
		}

		PM_EndMove();
		return;
	}

	// set mins, maxs, viewheight amd fov
	PM_AdjustBBox();

	PM_CheckZoom();

	// round up mins/maxs to hull size and adjust the viewheight, if needed
	PM_AdjustViewheight();

	// set groundentity, watertype, and waterlevel
	PM_CategorizePosition();

	oldGroundEntity = pm->groundentity;

	PM_CheckSpecialMovement();

	if( pm->playerState->pmove.pm_flags & PMF_TIME_TELEPORT ) {
		// teleport pause stays exactly in place
	} else if( pm->playerState->pmove.pm_flags & PMF_TIME_WATERJUMP ) {
		// waterjump has no control, but falls
		pml.velocity[2] -= pm->playerState->pmove.gravity * pml.frametime;
		if( pml.velocity[2] < 0 ) {
			// cancel as soon as we are falling down again
			pm->playerState->pmove.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
			pm->playerState->pmove.pm_time = 0;
		}

		PM_StepSlideMove();
	} else {
		// Kurim
		// Keep this order !
		PM_CheckJump();

		PM_CheckDash();

		PM_CheckWallJump();

		PM_CheckCrouchSlide();

		PM_Friction();

		if( pm->waterlevel >= 2 ) {
			PM_WaterMove();
		} else {
			vec3_t angles;

			VectorCopy( pm->playerState->viewangles, angles );
			if( angles[PITCH] > 180 ) {
				angles[PITCH] = angles[PITCH] - 360;
			}
			angles[PITCH] /= 3;

			AngleVectors( angles, pml.forward, pml.right, pml.up );

			// hack to work when looking straight up and straight down
			if( pml.forward[2] == -1.0f ) {
				VectorCopy( pml.up, pml.flatforward );
			} else if( pml.forward[2] == 1.0f ) {
				VectorCopy( pml.up, pml.flatforward );
				VectorNegate( pml.flatforward, pml.flatforward );
			} else {
				VectorCopy( pml.forward, pml.flatforward );
			}
			pml.flatforward[2] = 0.0f;
			VectorNormalize( pml.flatforward );

			PM_Move();
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	PM_CategorizePosition();

	PM_EndMove();

	// falling event

#define FALL_DAMAGE_MIN_DELTA 675
#define FALL_STEP_MIN_DELTA 400
#define MAX_FALLING_DAMAGE 15
#define FALL_DAMAGE_SCALE 1.0

	// Execute the triggers that are touched.
	// We check the entire path between the origin before the pmove and the
	// current origin to ensure no triggers are missed at high velocity.
	// Note that this method assumes the movement has been linear.
	gs.api.PMoveTouchTriggers( pm, pml.previous_origin );

	// touching triggers may force groundentity off
	if( !( pm->playerState->pmove.pm_flags & PMF_ON_GROUND ) && pm->groundentity != -1 ) {
		pm->groundentity = -1;
		pml.velocity[2] = 0;
	}

	if( pm->groundentity != -1 ) { // remove wall-jump and dash bits when touching ground
		// always keep the dash flag 50 msecs at least (to prevent being removed at the start of the dash)
		if( pm->playerState->pmove.stats[PM_STAT_DASHTIME] < ( PM_DASHJUMP_TIMEDELAY - 50 ) ) {
			pm->playerState->pmove.pm_flags &= ~PMF_DASHING;
		}

		if( pm->playerState->pmove.stats[PM_STAT_WJTIME] < ( PM_WALLJUMP_TIMEDELAY - 50 ) ) {
			PM_ClearWallJump();
		}
	}

	if( oldGroundEntity == -1 ) {
		falldelta = fallvelocity - ( ( pml.velocity[2] < 0.0f ) ? fabs( pml.velocity[2] ) : 0.0f );

		// scale delta if in water
		if( pm->waterlevel == 3 ) {
			falldelta = 0;
		}
		if( pm->waterlevel == 2 ) {
			falldelta *= 0.25;
		}
		if( pm->waterlevel == 1 ) {
			falldelta *= 0.5;
		}

		if( falldelta > FALL_STEP_MIN_DELTA ) {
			if( !GS_FallDamage() || ( pml.groundsurfFlags & SURF_NODAMAGE ) || ( pm->playerState->pmove.pm_flags & PMF_JUMPPAD_TIME ) ) {
				damage = 0;
			} else {
				damage = ( ( falldelta - FALL_DAMAGE_MIN_DELTA ) / 10 ) * FALL_DAMAGE_SCALE;
				clamp( damage, 0.0f, MAX_FALLING_DAMAGE );
			}

			gs.api.PredictedEvent( pm->playerState->POVnum, EV_FALL, damage );
		}

		pm->playerState->pmove.pm_flags &= ~PMF_JUMPPAD_TIME;
	}
}

/*
 * Pmove
 *
 * Can be called by either the server or the client
 */
void Pmove( pmove_t *pmove ) {
	if( !pmove->playerState ) {
		return;
	}

	PM_ClampAngles( pmove );

	_Pmove( pmove );
}

/*
 * PmoveExt
 */
void PmoveExt( pmove_t *pmove, void *PmoveFn( pmove_t * ) ) {
	if( !pmove->playerState ) {
		return;
	}

	PM_ClampAngles( pmove );

	PmoveFn( pmove );
}
