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

cvar_t *cg_gamepad_moveThres;
cvar_t *cg_gamepad_runThres;
cvar_t *cg_gamepad_strafeThres;
cvar_t *cg_gamepad_strafeRunThres;
cvar_t *cg_gamepad_pitchThres;
cvar_t *cg_gamepad_yawThres;
cvar_t *cg_gamepad_pitchSpeed;
cvar_t *cg_gamepad_yawSpeed;
cvar_t *cg_gamepad_pitchInvert;
cvar_t *cg_gamepad_swapSticks;

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

	int axes = ( cg_gamepad_swapSticks->integer ? 0 : 2 );

	if( ( cg_gamepad_yawThres->value <= 0.0f ) || ( cg_gamepad_yawThres->value >= 1.0f ) )
		trap_Cvar_Set( cg_gamepad_yawThres->name, cg_gamepad_yawThres->dvalue );
	if( ( cg_gamepad_pitchThres->value <= 0.0f ) || ( cg_gamepad_pitchThres->value >= 1.0f ) )
		trap_Cvar_Set( cg_gamepad_pitchThres->name, cg_gamepad_pitchThres->dvalue );

	float value = sticks[axes];
	float threshold = cg_gamepad_yawThres->value;
	float absValue = fabs( value );
	absValue = ( absValue - threshold ) / ( 1.0f - threshold ); // Smoothly apply the dead zone.
	if( absValue > 0.0f )
	{
		// Quadratic interpolation.
		viewangles[YAW] -= frametime *
			absValue * absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f ) *
			cg_gamepad_yawSpeed->value * CG_GetSensitivityScale( cg_gamepad_yawSpeed->value, 0.0f );
	}

	value = sticks[axes + 1];
	threshold = cg_gamepad_pitchThres->value;
	absValue = fabs( value );
	absValue = ( absValue - threshold ) / ( 1.0f - threshold );
	if( absValue > 0.0f )
	{
		viewangles[PITCH] += frametime * ( cg_gamepad_pitchInvert->integer ? -1.0f : 1.0f ) *
			absValue * absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f ) *
			cg_gamepad_pitchSpeed->value * CG_GetSensitivityScale( cg_gamepad_pitchSpeed->value, 0.0f );
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

	int axes = ( cg_gamepad_swapSticks->integer ? 2 : 0 );

	float value = sticks[axes];
	float threshold = cg_gamepad_strafeThres->value;
	float runThreshold = cg_gamepad_strafeRunThres->value;
	float absValue = fabs( value );
	if( runThreshold > threshold )
	{
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		clamp( absValue, 0.0f, 1.0f );
		absValue *= absValue;
	}
	else
	{
		absValue = ( float )( absValue > threshold );
	}
	if( absValue > cg_gamepad_strafeThres->value )
		movement[0] += absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );

	value = sticks[axes + 1];
	threshold = cg_gamepad_moveThres->value;
	runThreshold = cg_gamepad_runThres->value;
	absValue = fabs( value );
	if( runThreshold > threshold )
	{
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		clamp( absValue, 0.0f, 1.0f );
		absValue *= absValue;
	}
	else
	{
		absValue = ( float )( absValue > threshold );
	}
	if( absValue > cg_gamepad_moveThres->value )
		movement[1] -= absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );
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
