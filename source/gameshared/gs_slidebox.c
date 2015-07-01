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

#define	STOP_EPSILON	0.1

//#define IsGroundPlane( normal, gravityDir ) ( DotProduct( normal, gravityDir ) < -0.45f )

//==================================================
// SNAP AND CLIP ORIGIN AND VELOCITY
//==================================================

/*
* GS_GoodPosition
*/
static bool GS_GoodPosition( int snaptorigin[3], vec3_t mins, vec3_t maxs, int passent, int contentmask )
{
	trace_t	trace;
	vec3_t point;
	int i;

	if( !( contentmask & MASK_SOLID ) )
		return true;

	for( i = 0; i < 3; i++ )
		point[i] = (float)snaptorigin[i] * ( 1.0/PM_VECTOR_SNAP );

	module_Trace( &trace, point, mins, maxs, point, passent, contentmask, 0 );
	return !trace.allsolid ? true : false;
}

/*
* GS_SnapInitialPosition
*/
bool GS_SnapInitialPosition( vec3_t origin, vec3_t mins, vec3_t maxs, int passent, int contentmask )
{
	int x, y, z;
	int base[3];
	static const int offset[3] = { 0, -1, 1 };
	int originInt[3];

	VectorScale( origin, PM_VECTOR_SNAP, originInt );
	VectorCopy( originInt, base );

	for( z = 0; z < 3; z++ )
	{
		originInt[2] = base[2] + offset[z];
		for( y = 0; y < 3; y++ )
		{
			originInt[1] = base[1] + offset[y];
			for( x = 0; x < 3; x++ )
			{
				originInt[0] = base[0] + offset[x];
				if( GS_GoodPosition( originInt, mins, maxs, passent, contentmask ) )
				{
					origin[0] = originInt[0]*( 1.0/PM_VECTOR_SNAP );
					origin[1] = originInt[1]*( 1.0/PM_VECTOR_SNAP );
					origin[2] = originInt[2]*( 1.0/PM_VECTOR_SNAP );
					return true;
				}
			}
		}
	}

	return false;
}

/*
* GS_SnapPosition
*/
bool GS_SnapPosition( vec3_t origin, vec3_t mins, vec3_t maxs, int passent, int contentmask )
{
	int sign[3];
	int i, j, bits;
	int base[3];
	int originInt[3];
	// try all single bits first
	static const int jitterbits[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };

	for( i = 0; i < 3; i++ )
	{
		if( origin[i] >= 0 )
			sign[i] = 1;
		else
			sign[i] = -1;
		originInt[i] = (int)( origin[i] * PM_VECTOR_SNAP );
		if( (float)originInt[i] * ( 1.0/PM_VECTOR_SNAP ) == origin[i] )
			sign[i] = 0;
	}

	VectorCopy( originInt, base );

	// try all combinations
	for( j = 0; j < 8; j++ )
	{
		bits = jitterbits[j];
		VectorCopy( base, originInt );
		for( i = 0; i < 3; i++ )
		{
			if( bits & ( 1<<i ) )
				originInt[i] += sign[i];
		}

		if( GS_GoodPosition( originInt, mins, maxs, passent, contentmask ) )
		{
			VectorScale( originInt, ( 1.0/PM_VECTOR_SNAP ), origin );
			return true;
		}
	}

	return false;
}

/*
* GS_SnapVelocity
*/
void GS_SnapVelocity( vec3_t velocity )
{
	int i, velocityInt[3];
	// snap velocity to sixteenths
	for( i = 0; i < 3; i++ )
	{
		velocityInt[i] = (int)( velocity[i]*PM_VECTOR_SNAP );
		velocity[i] = (float)velocityInt[i] * ( 1.0/PM_VECTOR_SNAP );
	}
}

/*
* GS_ClipVelocity
*/
void GS_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce )
{
	float backoff;
	float change;
	int i;

	backoff = DotProduct( in, normal );

	if( backoff <= 0 )
	{
		backoff *= overbounce;
	}
	else
	{
		backoff /= overbounce;
	}

	for( i = 0; i < 3; i++ )
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}
#ifdef GS_SLIDEMOVE_CLAMPING
	{
		float oldspeed, newspeed;
		oldspeed = VectorLength( in );
		newspeed = VectorLength( out );
		if( newspeed > oldspeed )
		{
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
int GS_LinearMovement( const entity_state_t *ent, unsigned time, vec3_t dest )
{
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
		clamp( moveFrac, 0, 1 );
		VectorMA( ent->linearMovementBegin, moveFrac, dist, dest );
	}
	else {
		moveFrac = moveTime * 0.001f;
		VectorMA( ent->linearMovementBegin, moveFrac, ent->linearMovementVelocity, dest );
	}

	return moveTime;
}

/* 
* GS_LinearMovementDelta
*/
void GS_LinearMovementDelta( const entity_state_t *ent, unsigned oldTime, unsigned curTime, vec3_t dest )
{
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
static void GS_AddTouchEnt( move_t *move, int entNum )
{
	int i;

	if( move->numtouch >= MAXTOUCH || entNum < 0 )
		return;

	// see if it is already added
	for( i = 0; i < move->numtouch; i++ )
	{
		if( move->touchents[i] == entNum )
			return;
	}

	// add it
	move->touchents[move->numtouch] = entNum;
	move->numtouch++;
}

/*
* GS_ClearClippingPlanes
*/
static void GS_ClearClippingPlanes( move_t *move )
{
	move->numClipPlanes = 0;
}

/*
* GS_ClipVelocityToClippingPlanes
*/
static void GS_ClipVelocityToClippingPlanes( move_t *move )
{
	int i;

	for( i = 0; i < move->numClipPlanes; i++ )
	{
		if( DotProduct( move->velocity, move->clipPlaneNormals[i] ) >= SLIDEMOVE_PLANEINTERACT_EPSILON )
			continue; // looking in the same direction than the velocity
#ifndef TRACEVICFIX
#ifndef TRACE_NOAXIAL
		// this is a hack, cause non axial planes can return invalid positions in trace endpos
		if( PlaneTypeForNormal( move->clipPlaneNormals[i] ) == PLANE_NONAXIAL )
		{
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
static void GS_AddClippingPlane( move_t *move, const vec3_t planeNormal )
{
	int i;

	// see if we are already clipping to this plane
	for( i = 0; i < move->numClipPlanes; i++ )
	{
		if( DotProduct( planeNormal, move->clipPlaneNormals[i] ) >= ( 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) )
		{
			return;
		}
	}

	if( move->numClipPlanes + 1 == MAX_SLIDEMOVE_CLIP_PLANES )
		module_Error( "GS_AddTouchPlane: MAX_SLIDEMOVE_CLIP_PLANES reached\n" );

	// add the plane
	VectorCopy( planeNormal, move->clipPlaneNormals[move->numClipPlanes] );
	move->numClipPlanes++;
}

/*
* GS_SlideMoveClipMove
*/
static int GS_SlideMoveClipMove( move_t *move /*, const bool stepping*/ )
{
	vec3_t endpos;
	trace_t	trace;
	int blockedmask = 0;

	VectorMA( move->origin, move->remainingTime, move->velocity, endpos );
	module_Trace( &trace, move->origin, move->mins, move->maxs, endpos, move->passent, move->contentmask, 0 );
	if( trace.allsolid )
	{
		if( trace.ent > 0 )
			GS_AddTouchEnt( move, trace.ent );
		return blockedmask|SLIDEMOVEFLAG_TRAPPED;
	}

	if( trace.fraction == 1.0f )
	{                          // was able to cleanly perform the full move
		VectorCopy( trace.endpos, move->origin );
		move->remainingTime -= ( trace.fraction * move->remainingTime );
		return blockedmask|SLIDEMOVEFLAG_MOVED;
	}

	if( trace.fraction < 1.0f )
	{                         // wasn't able to make the full move
		GS_AddTouchEnt( move, trace.ent );
		blockedmask |= SLIDEMOVEFLAG_PLANE_TOUCHED;

		// move what can be moved
		if( trace.fraction > 0.0 )
		{
			VectorCopy( trace.endpos, move->origin );
			move->remainingTime -= ( trace.fraction * move->remainingTime );
			blockedmask |= SLIDEMOVEFLAG_MOVED;
		}

		// if the plane is a wall and stepping, try to step it up
		if( !ISWALKABLEPLANE( trace.plane.normal ) )
		{
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
int GS_SlideMove( move_t *move )
{
#define MAX_SLIDEMOVE_ATTEMPTS	8
	int count;
	int blockedmask = 0;
	vec3_t lastValidOrigin, originalVelocity;

	// if the velocity is too small, just stop
	if( VectorLength( move->velocity ) < STOP_EPSILON )
	{
		VectorClear( move->velocity );
		move->remainingTime = 0;
		return 0;
	}

	VectorCopy( move->velocity, originalVelocity );
	VectorCopy( move->origin, lastValidOrigin );

	GS_ClearClippingPlanes( move );
	move->numtouch = 0;

	for( count = 0; count < MAX_SLIDEMOVE_ATTEMPTS; count++ )
	{
		// get the original velocity and clip it to all the planes we got in the list
		VectorCopy( originalVelocity, move->velocity );
		GS_ClipVelocityToClippingPlanes( move );
		blockedmask = GS_SlideMoveClipMove( move /*, stepping*/ );

#ifdef CHECK_TRAPPED
		{
			trace_t	trace;
			module_Trace( &trace, move->origin, move->mins, move->maxs, move->origin, move->passent, move->contentmask, 0 );
			if( trace.startsolid )
			{
				blockedmask |= SLIDEMOVEFLAG_TRAPPED;
			}
		}
#endif

		// can't continue
		if( blockedmask & SLIDEMOVEFLAG_TRAPPED )
		{
#ifdef CHECK_TRAPPED
			module_Printf( "GS_SlideMove SLIDEMOVEFLAG_TRAPPED\n" );
#endif
			move->remainingTime = 0.0f;
			VectorCopy( lastValidOrigin, move->origin );
			return blockedmask;
		}

		VectorCopy( move->origin, lastValidOrigin );

		// touched a plane, re-clip velocity and retry
		if( blockedmask & SLIDEMOVEFLAG_PLANE_TOUCHED )
		{
			continue;
		}

		// if it didn't touch anything the move should be completed
		if( move->remainingTime > 0.0f )
		{
			module_Printf( "slidemove finished with remaining time\n" );
			move->remainingTime = 0.0f;
		}

		break;
	}

	// snap
	GS_SnapPosition( move->origin, move->mins, move->maxs, move->passent, move->contentmask );
	GS_SnapVelocity( move->velocity );

	return blockedmask;
}
