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

//#define CHECK_TRAPPED
#define GS_SLIDEMOVE_CLAMPING

#define STOP_EPSILON    0.1

//#define IsGroundPlane( normal, gravityDir ) ( DotProduct( normal, gravityDir ) < -0.45f )

//==================================================
// SNAP AND CLIP ORIGIN AND VELOCITY
//==================================================

/*
* GS_ClipVelocity
*/
void GS_ClipVelocity( const vec3_t in, const vec3_t normal, vec3_t out, float overbounce ) {
	float backoff;
	float change;
	int i;

	backoff = DotProduct( in, normal );

	if( backoff <= 0 ) {
		backoff *= overbounce;
	} else {
		backoff /= overbounce;
	}

	for( i = 0; i < 3; i++ ) {
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
#ifdef GS_SLIDEMOVE_CLAMPING
	{
		float oldspeed, newspeed;
		oldspeed = VectorLength( in );
		newspeed = VectorLength( out );
		if( newspeed > oldspeed ) {
			VectorNormalize( out );
			VectorScale( out, oldspeed, out );
		}
	}
#endif
}

//==================================================

/*
* GS_LinearMovement
*/
int GS_LinearMovement( const entity_state_t *ent, int64_t time, vec3_t dest ) {
	vec3_t dist;
	int moveTime;
	float moveFrac;

	moveTime = time - ent->linearMovementTimeStamp;
	if( moveTime < 0 ) {
		moveTime = 0;
	}

	if( ent->linearMovementDuration ) {
		if( moveTime > (int)ent->linearMovementDuration ) {
			moveTime = ent->linearMovementDuration;
		}

		VectorSubtract( ent->linearMovementEnd, ent->linearMovementBegin, dist );
		moveFrac = (float)moveTime / (float)ent->linearMovementDuration;
		Q_clamp( moveFrac, 0, 1 );
		VectorMA( ent->linearMovementBegin, moveFrac, dist, dest );
	} else {
		moveFrac = moveTime * 0.001f;
		VectorMA( ent->linearMovementBegin, moveFrac, ent->linearMovementVelocity, dest );
	}

	return moveTime;
}

/*
* GS_LinearMovementDelta
*/
void GS_LinearMovementDelta( const entity_state_t *ent, int64_t oldTime, int64_t curTime, vec3_t dest ) {
	vec3_t p1, p2;
	GS_LinearMovement( ent, oldTime, p1 );
	GS_LinearMovement( ent, curTime, p2 );
	VectorSubtract( p2, p1, dest );
}

//==================================================
// SLIDE MOVE
//
// Note: groundentity info should be up to date when calling any slide move function
//==================================================

/*
* GS_AddTouchEnt
*/
static void GS_AddTouchEnt( move_t *move, int entNum ) {
	int i;

	if( move->numtouch >= MAXTOUCH || entNum < 0 ) {
		return;
	}

	// see if it is already added
	for( i = 0; i < move->numtouch; i++ ) {
		if( move->touchents[i] == entNum ) {
			return;
		}
	}

	// add it
	move->touchents[move->numtouch] = entNum;
	move->numtouch++;
}

/*
* GS_ClearClippingPlanes
*/
static void GS_ClearClippingPlanes( move_t *move ) {
	move->numClipPlanes = 0;
}

/*
* GS_ClipVelocityToClippingPlanes
*/
static void GS_ClipVelocityToClippingPlanes( move_t *move ) {
	int i;

	for( i = 0; i < move->numClipPlanes; i++ ) {
		if( DotProduct( move->velocity, move->clipPlaneNormals[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON ) {
			continue; // looking in the same direction than the velocity
		}
#ifndef TRACEVICFIX
#ifndef TRACE_NOAXIAL
		// this is a hack, cause non axial planes can return invalid positions in trace endpos
		if( PlaneTypeForNormal( move->clipPlaneNormals[i] ) == PLANE_NONAXIAL ) {
			// offset the origin a little bit along the plane normal
			VectorMA( move->origin, 0.05, move->clipPlaneNormals[i], move->origin );
		}
#endif
#endif

		GS_ClipVelocity( move->velocity, move->clipPlaneNormals[i], move->velocity, move->slideBounce );
	}
}

/*
* GS_AddClippingPlane
*/
static void GS_AddClippingPlane( move_t *move, const vec3_t planeNormal ) {
	int i;

	// see if we are already clipping to this plane
	for( i = 0; i < move->numClipPlanes; i++ ) {
		if( DotProduct( planeNormal, move->clipPlaneNormals[i] ) >= ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) ) {
			return;
		}
	}

	if( move->numClipPlanes + 1 == MAX_SLIDEMOVE_CLIP_PLANES ) {
		gs.api.Error( "GS_AddTouchPlane: MAX_SLIDEMOVE_CLIP_PLANES reached\n" );
	}

	// add the plane
	VectorCopy( planeNormal, move->clipPlaneNormals[move->numClipPlanes] );
	move->numClipPlanes++;
}

/*
* GS_SlideMoveClipMove
*/
static int GS_SlideMoveClipMove( move_t *move /*, const bool stepping*/ ) {
	vec3_t endpos;
	trace_t trace;
	int blockedmask = 0;

	VectorMA( move->origin, move->remainingTime, move->velocity, endpos );
	gs.api.Trace( &trace, move->origin, move->mins, move->maxs, endpos, move->passent, move->contentmask, 0 );
	if( trace.allsolid ) {
		if( trace.ent > 0 ) {
			GS_AddTouchEnt( move, trace.ent );
		}
		return blockedmask | SLIDEMOVEFLAG_TRAPPED;
	}

	if( trace.fraction == 1.0f ) { // was able to cleanly perform the full move
		VectorCopy( trace.endpos, move->origin );
		move->remainingTime -= ( trace.fraction * move->remainingTime );
		return blockedmask | SLIDEMOVEFLAG_MOVED;
	}

	if( trace.fraction < 1.0f ) { // wasn't able to make the full move
		GS_AddTouchEnt( move, trace.ent );
		blockedmask |= SLIDEMOVEFLAG_PLANE_TOUCHED;

		// move what can be moved
		if( trace.fraction > 0.0 ) {
			VectorCopy( trace.endpos, move->origin );
			move->remainingTime -= ( trace.fraction * move->remainingTime );
			blockedmask |= SLIDEMOVEFLAG_MOVED;
		}

		// if the plane is a wall and stepping, try to step it up
		if( !ISWALKABLEPLANE( trace.plane.normal ) ) {
			//if( stepping && GS_StepUp( move ) ) {
			//	return blockedmask;  // solved : don't add the clipping plane
			//}
			//else {
			blockedmask |= SLIDEMOVEFLAG_WALL_BLOCKED;
			//}
		}

		GS_AddClippingPlane( move, trace.plane.normal );
	}

	return blockedmask;
}

/*
* GS_SlideMove
*/
int GS_SlideMove( move_t *move ) {
#define MAX_SLIDEMOVE_ATTEMPTS  8
	int count;
	int blockedmask = 0;
	vec3_t lastValidOrigin, originalVelocity;

	// if the velocity is too small, just stop
	if( VectorLength( move->velocity ) < STOP_EPSILON ) {
		VectorClear( move->velocity );
		move->remainingTime = 0;
		return 0;
	}

	VectorCopy( move->velocity, originalVelocity );
	VectorCopy( move->origin, lastValidOrigin );

	GS_ClearClippingPlanes( move );
	move->numtouch = 0;

	for( count = 0; count < MAX_SLIDEMOVE_ATTEMPTS; count++ ) {
		// get the original velocity and clip it to all the planes we got in the list
		VectorCopy( originalVelocity, move->velocity );
		GS_ClipVelocityToClippingPlanes( move );
		blockedmask = GS_SlideMoveClipMove( move /*, stepping*/ );

#ifdef CHECK_TRAPPED
		{
			trace_t trace;
			gs.api.Trace( &trace, move->origin, move->mins, move->maxs, move->origin, move->passent, move->contentmask, 0 );
			if( trace.startsolid ) {
				blockedmask |= SLIDEMOVEFLAG_TRAPPED;
			}
		}
#endif

		// can't continue
		if( blockedmask & SLIDEMOVEFLAG_TRAPPED ) {
#ifdef CHECK_TRAPPED
			gs.api.Printf( "GS_SlideMove SLIDEMOVEFLAG_TRAPPED\n" );
#endif
			move->remainingTime = 0.0f;
			VectorCopy( lastValidOrigin, move->origin );
			return blockedmask;
		}

		VectorCopy( move->origin, lastValidOrigin );

		// touched a plane, re-clip velocity and retry
		if( blockedmask & SLIDEMOVEFLAG_PLANE_TOUCHED ) {
			continue;
		}

		// if it didn't touch anything the move should be completed
		if( move->remainingTime > 0.0f ) {
			gs.api.Printf( "slidemove finished with remaining time\n" );
			move->remainingTime = 0.0f;
		}

		break;
	}

	return blockedmask;
}
