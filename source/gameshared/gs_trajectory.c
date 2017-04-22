/*
Copyright (C) 2017 Victor Luchits

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

//==================================================

/*
* GS_EvaluatePosition
*/

void GS_EvaluatePosition( const trajectory_t *tr, int64_t curTime, vec3_t position )
{
	int64_t moveTime;
	double moveFrac;

	moveTime = curTime - tr->startTime;

	switch( tr->type ) {
		case TR_STATIC:
			VectorCopy( tr->position, position );
			break;
		case TR_INTERPOLATE:
			VectorCopy( tr->position, position );
			break;
		case TR_LINEAR:
			if( moveTime < 0 ) {
				moveTime = 0;
			}
			moveFrac = moveTime * 0.001f;

			if( tr->duration ) {
				if( moveTime > (int64_t)tr->duration ) {
					moveTime = tr->duration;
				}

				moveFrac = (double)moveTime / (double)tr->duration;
				clamp( moveFrac, 0.0, 1.0 );

				VectorMA( tr->position, moveFrac, tr->velocity, position );
				break;
			}

			VectorMA( tr->position, moveFrac, tr->velocity, position );
			break;
		case TR_SINE:
			moveFrac = (double)moveTime / (double)tr->duration;
			moveFrac = sin( moveFrac * M_TWOPI );
			VectorMA( tr->position, moveFrac, tr->velocity, position );
			break;
	}
}

/*
* GS_EvaluateVelocity
*/
void GS_EvaluateVelocity( const trajectory_t *tr, int64_t curTime, vec3_t velocity )
{
	int64_t moveTime;
	double moveFrac;

	moveTime = curTime - tr->startTime;

	switch( tr->type ) {
	case TR_STATIC:
		VectorClear( velocity );
		break;
	case TR_INTERPOLATE:
		VectorClear( velocity );
		break;
	case TR_LINEAR:
		VectorCopy( tr->velocity, velocity );
		break;
	case TR_SINE:
		moveFrac = (double)moveTime / (double)tr->duration;
		moveFrac = cos( moveFrac * M_TWOPI );
		VectorScale( tr->velocity, moveFrac, velocity );
		break;
	}
}
