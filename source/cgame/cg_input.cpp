/*
Copyright (C) 2015 SiPlus, Chasseur de bots

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

/**
 * Warsow-specific input code.
 */

#include "cg_local.h"

cvar_t *joy_forwardthreshold;
cvar_t *joy_forwardrunthreshold;
cvar_t *joy_sidethreshold;
cvar_t *joy_siderunthreshold;
cvar_t *joy_pitchthreshold;
cvar_t *joy_yawthreshold;
cvar_t *joy_pitchspeed;
cvar_t *joy_yawspeed;
cvar_t *joy_inverty;
cvar_t *joy_movement_stick;

/**
 * Adds the view rotation from the gamepad.
 *
 * @param viewangles the target view angles
 * @param frametime  the length of the last frame
 */
static void CG_AddGamepadViewAngles( vec3_t viewangles, float frametime )
{
	vec4_t sticks;
	trap_IN_GetThumbsticks( sticks );

	int axes = ( joy_movement_stick->integer ? 0 : 2 );

	if( ( joy_yawthreshold->value <= 0.0f ) || ( joy_yawthreshold->value >= 1.0f ) )
		trap_Cvar_Set( joy_yawthreshold->name, joy_yawthreshold->dvalue );
	if( ( joy_pitchthreshold->value <= 0.0f ) || ( joy_pitchthreshold->value >= 1.0f ) )
		trap_Cvar_Set( joy_pitchthreshold->name, joy_pitchthreshold->dvalue );

	float value = sticks[axes];
	float threshold = joy_yawthreshold->value;
	float absValue = fabs( value );
	absValue = ( absValue - threshold ) / ( 1.0f - threshold ); // Smoothly apply the dead zone.
	if( absValue > 0.0f )
	{
		// Quadratic interpolation.
		viewangles[YAW] -= frametime *
			absValue * absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f ) *
			joy_yawspeed->value * CG_GetSensitivityScale( joy_yawspeed->value, 0.0f );
	}

	value = sticks[axes + 1];
	threshold = joy_pitchthreshold->value;
	absValue = fabs( value );
	absValue = ( absValue - threshold ) / ( 1.0f - threshold );
	if( absValue > 0.0f )
	{
		viewangles[PITCH] += frametime * ( joy_inverty->integer ? -1.0f : 1.0f ) *
			absValue * absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f ) *
			joy_pitchspeed->value * CG_GetSensitivityScale( joy_pitchspeed->value, 0.0f );
	}
}

/**
 * Adds the movement from the gamepad.
 *
 * @param movement the target movement vector
 */
static void CG_AddGamepadMovement( vec3_t movement )
{
	vec4_t sticks;
	trap_IN_GetThumbsticks( sticks );

	int axes = ( joy_movement_stick->integer ? 2 : 0 );

	float value = sticks[axes];
	float absValue = fabs( value );
	if( absValue > joy_sidethreshold->value )
		movement[0] += ( ( absValue > joy_siderunthreshold->value ) ? ( ( value < 0.0f ) ? -1.0f : 1.0f ) : value );

	value = sticks[axes + 1];
	absValue = fabs( value );
	if( absValue > joy_forwardthreshold->value )
		movement[1] -= ( ( absValue > joy_forwardrunthreshold->value ) ? ( ( value < 0.0f ) ? -1.0f : 1.0f ) : value );
}

void CG_UpdateInput( float frametime )
{
	CG_TouchFrame( frametime );
}

unsigned int CG_GetButtonBits( void )
{
	return CG_GetTouchButtonBits();
}

void CG_AddViewAngles( vec3_t viewangles, float frametime )
{
	CG_AddGamepadViewAngles( viewangles, frametime );
	CG_AddTouchViewAngles( viewangles, frametime );
}

void CG_AddMovement( vec3_t movement )
{
	CG_AddGamepadMovement( movement );
	CG_AddTouchMovement( movement );
}
