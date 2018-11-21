/*
Copyright (C) 2007 Daniel Lindenfelser

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

#include "cg_local.h"

/*
* CG_DamageIndicatorAdd
*/
void CG_DamageIndicatorAdd( int damage, const vec3_t dir ) {
	int i;
	int64_t damageTime;
	vec3_t playerAngles;
	mat3_t playerAxis;

// epsilons are 30 degrees
#define INDICATOR_EPSILON 0.5f
#define INDICATOR_EPSILON_UP 0.85f
#define TOP_BLEND 0
#define RIGHT_BLEND 1
#define BOTTOM_BLEND 2
#define LEFT_BLEND 3
	float blends[4];
	float forward, side;

	if( !cg_damage_indicator->integer ) {
		return;
	}

	playerAngles[PITCH] = 0;
	playerAngles[YAW] = cg.predictedPlayerState.viewangles[YAW];
	playerAngles[ROLL] = 0;

	Matrix3_FromAngles( playerAngles, playerAxis );

	if( cg_damage_indicator_time->value < 0 ) {
		trap_Cvar_SetValue( "cg_damage_indicator_time", 0 );
	}

	Vector4Set( blends, 0, 0, 0, 0 );
	damageTime = damage * cg_damage_indicator_time->value;

	// up and down go distributed equally to all blends and assumed when no dir is given
	if( !dir || VectorCompare( dir, vec3_origin ) || cg_damage_indicator->integer == 2 ||
		( fabs( DotProduct( dir, &playerAxis[AXIS_UP] ) ) > INDICATOR_EPSILON_UP ) ) {
		blends[RIGHT_BLEND] += damageTime;
		blends[LEFT_BLEND] += damageTime;
		blends[TOP_BLEND] += damageTime;
		blends[BOTTOM_BLEND] += damageTime;
	} else {
		side = DotProduct( dir, &playerAxis[AXIS_RIGHT] );
		if( side > INDICATOR_EPSILON ) {
			blends[LEFT_BLEND] += damageTime;
		} else if( side < -INDICATOR_EPSILON ) {
			blends[RIGHT_BLEND] += damageTime;
		}

		forward = DotProduct( dir, &playerAxis[AXIS_FORWARD] );
		if( forward > INDICATOR_EPSILON ) {
			blends[BOTTOM_BLEND] += damageTime;
		} else if( forward < -INDICATOR_EPSILON ) {
			blends[TOP_BLEND] += damageTime;
		}
	}

	for( i = 0; i < 4; i++ ) {
		if( cg.damageBlends[i] < cg.time + blends[i] ) {
			cg.damageBlends[i] = cg.time + blends[i];
		}
	}
#undef TOP_BLEND
#undef RIGHT_BLEND
#undef BOTTOM_BLEND
#undef LEFT_BLEND
#undef INDICATOR_EPSILON
#undef INDICATOR_EPSILON_UP
}

/*
* CG_ResetDamageIndicator
*/
void CG_ResetDamageIndicator( void ) {
	int i;

	for( i = 0; i < 4; i++ )
		cg.damageBlends[i] = 0;
}
