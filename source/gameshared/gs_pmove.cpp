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

static void PM_AddTouchEnt( pmove_t *pmove, int entNum ) {
	int i;

	if( pmove->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( i = 0; i < pmove->numtouch; i++ ) {
		if( pmove->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	pmove->touchents[pmove->numtouch] = entNum;
	pmove->numtouch++;
}

int PM_SlideMove( pmove_t *pmove ) {
	vec3_t end, dir;
	vec3_t old_velocity, last_valid_origin;
	float value;
	vec3_t planes[MAX_CLIP_PLANES];
	int numplanes = 0;
	trace_t trace;
	int moves, i, j, k;
	int maxmoves = 4;
	float remainingTime = pmove->remainingTime;
	int blockedmask = 0;
	vec_t *pml_velocity = pmove->velocity;
	vec_t *pml_origin = pmove->origin;

	VectorCopy(pml_velocity, old_velocity);
	VectorCopy(pml_origin, last_valid_origin);

	// Do a shortcut in this case
	if( pmove->skipCollision ) {
		VectorMA( pml_origin, remainingTime, pml_velocity, pml_origin );
		return blockedmask;
	}

	numplanes = 0; // clean up planes count for checking

	for (moves = 0; moves < maxmoves; moves++) {
		VectorMA(pml_origin, remainingTime, pml_velocity, end);
		gs.api.Trace(&trace, pml_origin, pmove->mins, pmove->maxs, end, pmove->passEnt, pmove->contentmask, 0);
		if (trace.allsolid) { // trapped into a solid
			VectorCopy(last_valid_origin, pml_origin);
			return SLIDEMOVEFLAG_TRAPPED;
		}

		if (trace.fraction > 0) { // actually covered some distance
			VectorCopy(trace.endpos, pml_origin);
			VectorCopy(trace.endpos, last_valid_origin);
		}

		if (trace.fraction == 1) {
			break; // move done

		}
		// save touched entity for return output
		PM_AddTouchEnt(pmove, trace.ent);

		// at this point we are blocked but not trapped.

		blockedmask |= SLIDEMOVEFLAG_BLOCKED;
		if (trace.plane.normal[2] < SLIDEMOVE_PLANEINTERACT_EPSILON) { // is it a vertical wall?
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
		}

		remainingTime -= (trace.fraction * remainingTime);
		pmove->remainingTime = remainingTime;

		// we got blocked, add the plane for sliding along it

		// if this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for (i = 0; i < numplanes; i++) {
			if (DotProduct(trace.plane.normal, planes[i]) > (1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON)) {
				VectorAdd(trace.plane.normal, pml_velocity, pml_velocity);
				break;
			}
		}
		if (i < numplanes) { // found a repeated plane, so don't add it, just repeat the trace
			continue;
		}

		// security check: we can't store more planes
		if (numplanes >= MAX_CLIP_PLANES) {
			VectorClear(pml_velocity);
			return SLIDEMOVEFLAG_TRAPPED;
		}

		// put the actual plane in the list
		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify original_velocity so it parallels all of the clip planes
		//

		for (i = 0; i < numplanes; i++) {
			if (DotProduct(pml_velocity, planes[i]) >= SLIDEMOVE_PLANEINTERACT_EPSILON) { // would not touch it
				continue;
			}

			GS_ClipVelocity(pml_velocity, planes[i], pml_velocity, pmove->slideBounce);
			// see if we enter a second plane
			for (j = 0; j < numplanes; j++) {
				if (j == i) { // it's the same plane
					continue;
				}
				if (DotProduct(pml_velocity, planes[j]) >= SLIDEMOVE_PLANEINTERACT_EPSILON) {
					continue; // not with this one

				}
				//there was a second one. Try to slide along it too
				GS_ClipVelocity(pml_velocity, planes[j], pml_velocity, pmove->slideBounce);

				// check if the slide sent it back to the first plane
				if (DotProduct(pml_velocity, planes[i]) >= SLIDEMOVE_PLANEINTERACT_EPSILON) {
					continue;
				}

				// bad luck: slide the original velocity along the crease
				CrossProduct(planes[i], planes[j], dir);
				VectorNormalize(dir);
				value = DotProduct(dir, pml_velocity);
				VectorScale(dir, value, pml_velocity);

				// check if there is a third plane, in that case we're trapped
				for (k = 0; k < numplanes; k++) {
					if (j == k || i == k) { // it's the same plane
						continue;
					}
					if (DotProduct(pml_velocity, planes[k]) >= SLIDEMOVE_PLANEINTERACT_EPSILON) {
						continue; // not with this one
					}
					VectorClear(pml_velocity);
					break;
				}
			}
		}
	}

	return blockedmask;
}

/*
* PM_ClampAngles
*
*/
static void PM_ClampAngles( player_state_t *ps, usercmd_t *cmd, vec3_t vaClamp ) {
	int i;
	short temp;

	// don't let the player look up or down more than 'vaClamp' degrees on each axis

	for( i = 0; i < 3; i++ ) {
		temp = cmd->angles[i] + ps->pmove.delta_angles[i];

		float c = fabs( vaClamp[i] );
		if( c >= 1.0f ) {
			Q_clamp( c, 1, 90 );

			int clampPos = ANGLE2SHORT( c );
			int clampNeg = ANGLE2SHORT( c * -1.0f );

			if( temp > (short)clampPos - 1 ) {
				ps->pmove.delta_angles[i] = ( clampPos - 1 ) - cmd->angles[i];
				temp = ( short )clampPos - 1;
			} else if( temp < (short)clampNeg + 1 ) {
				ps->pmove.delta_angles[i] = ( clampNeg + 1 ) - cmd->angles[i];
				temp = ( short )clampNeg + 1;
			}
		}

		ps->viewangles[i] = SHORT2ANGLE( temp );
	}
}
 
/*
 * PM_Pmove
 */
void PM_Pmove( pmove_t *pmove, player_state_t *ps, usercmd_t *cmd, void (*vaClampFn)( const player_state_t *, vec3_t ), void (*PmoveFn)( pmove_t *, player_state_t *, usercmd_t * ) ) {
	vec3_t vaClamp = { 0, 0, 0 };

	if( !pmove || !ps || !cmd ) {
		return;
	}

	vaClamp[PITCH] = 90;
	if( vaClampFn != nullptr ) {
		vaClampFn( ps, vaClamp );
	}

	PM_ClampAngles( ps, cmd, vaClamp );

	if( PmoveFn != nullptr ) {
		PmoveFn( pmove, ps, cmd );
	}
}
