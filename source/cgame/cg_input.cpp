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

/*
===============================================================================

MOUSE

===============================================================================
*/

cvar_t *sensitivity;
cvar_t *zoomsens;
cvar_t *m_accel;
cvar_t *m_accelStyle;
cvar_t *m_accelOffset;
cvar_t *m_accelPow;
cvar_t *m_filter;
cvar_t *m_sensCap;

cvar_t *m_pitch;
cvar_t *m_yaw;

static float mouse_x = 0, mouse_y = 0;

/*
* CG_MouseMove
*/
void CG_MouseMove( int frame_time, int mx, int my ) {
	static float old_mouse_x = 0, old_mouse_y = 0;
	float accelSensitivity;

	// mouse filtering
	switch( m_filter->integer ) {
	case 1:
	{
		mouse_x = ( mx + old_mouse_x ) * 0.5;
		mouse_y = ( my + old_mouse_y ) * 0.5;
	}
	break;

	default: // no filtering
		mouse_x = mx;
		mouse_y = my;
		break;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	accelSensitivity = sensitivity->value;

	if( m_accel->value != 0.0f && frame_time != 0 ) {
		float rate;

		// QuakeLive-style mouse acceleration, ported from ioquake3
		// original patch by Gabriel Schnoering and TTimo
		if( m_accelStyle->integer == 1 ) {
			float base[2];
			float power[2];

			// sensitivity remains pretty much unchanged at low speeds
			// m_accel is a power value to how the acceleration is shaped
			// m_accelOffset is the rate for which the acceleration will have doubled the non accelerated amplification
			// NOTE: decouple the config cvars for independent acceleration setup along X and Y?

			base[0] = (float) ( abs( mx ) ) / (float) frame_time;
			base[1] = (float) ( abs( my ) ) / (float) frame_time;
			power[0] = powf( base[0] / m_accelOffset->value, m_accel->value );
			power[1] = powf( base[1] / m_accelOffset->value, m_accel->value );

			mouse_x = ( mouse_x + ( ( mouse_x < 0 ) ? -power[0] : power[0] ) * m_accelOffset->value );
			mouse_y = ( mouse_y + ( ( mouse_y < 0 ) ? -power[1] : power[1] ) * m_accelOffset->value );
		} else if( m_accelStyle->integer == 2 ) {
			float accelOffset, accelPow;

			// ch : similar to normal acceleration with offset and variable pow mechanisms

			// sanitize values
			accelPow = m_accelPow->value > 1.0 ? m_accelPow->value : 2.0;
			accelOffset = m_accelOffset->value >= 0.0 ? m_accelOffset->value : 0.0;

			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)frame_time;
			rate -= accelOffset;
			if( rate < 0 ) {
				rate = 0.0;
			}
			// ch : TODO sens += pow( rate * m_accel->value, m_accelPow->value - 1.0 )
			accelSensitivity += pow( rate * m_accel->value, accelPow - 1.0 );

			// TODO : move this outside of this branch?
			if( m_sensCap->value > 0 && accelSensitivity > m_sensCap->value ) {
				accelSensitivity = m_sensCap->value;
			}
		} else {
			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)frame_time;
			accelSensitivity += rate * m_accel->value;
		}
	}

	accelSensitivity *= CG_GetSensitivityScale( sensitivity->value, zoomsens->value );

	mouse_x *= accelSensitivity;
	mouse_y *= accelSensitivity;
}

/**
* Adds view rotation from mouse.
*
* @param viewangles view angles to modify
* @param frametime  real frame time
* @param flip       horizontal flipping direction
*/
static void CG_AddMouseViewAngles( vec3_t viewangles, float frametime, bool flipped ) {
	if( !mouse_x && !mouse_y ) {
		return;
	}

	// add mouse X/Y movement to cmd
	viewangles[YAW] -= ( m_yaw->value * mouse_x ) * ( flipped ? -1.0 : 1.0 );
	viewangles[PITCH] += ( m_pitch->value * mouse_y );
}

/*
===============================================================================

GAMEPAD

===============================================================================
*/

cvar_t *cg_gamepad_moveThres;
cvar_t *cg_gamepad_runThres;
cvar_t *cg_gamepad_strafeThres;
cvar_t *cg_gamepad_strafeRunThres;
cvar_t *cg_gamepad_pitchThres;
cvar_t *cg_gamepad_yawThres;
cvar_t *cg_gamepad_pitchSpeed;
cvar_t *cg_gamepad_yawSpeed;
cvar_t *cg_gamepad_pitchInvert;
cvar_t *cg_gamepad_accelMax;
cvar_t *cg_gamepad_accelSpeed;
cvar_t *cg_gamepad_accelThres;
cvar_t *cg_gamepad_swapSticks;

static float cg_gamepad_accelPitch = 1.0f, cg_gamepad_accelYaw = 1.0f;

/**
 * Updates time-dependent gamepad state.
 *
 * @param frametime real frame time
 */
static void CG_GamepadFrame( float frametime ) {
	// Add acceleration to the gamepad look above the acceleration threshold.

	vec4_t sticks;
	trap_IN_GetThumbsticks( sticks );

	int axes = ( cg_gamepad_swapSticks->integer ? 0 : 2 );

	if( cg_gamepad_accelMax->value < 0.0f ) {
		trap_Cvar_SetValue( cg_gamepad_accelMax->name, 0.0f );
	}
	if( cg_gamepad_accelSpeed->value < 0.0f ) {
		trap_Cvar_SetValue( cg_gamepad_accelSpeed->name, 0.0f );
	}

	float accelMax = cg_gamepad_accelMax->value + 1.0f;
	float accelSpeed = cg_gamepad_accelSpeed->value;
	float accelThres = cg_gamepad_accelThres->value;

	float value = fabs( sticks[axes] );
	if( value > cg_gamepad_yawThres->value ) {
		cg_gamepad_accelYaw += ( ( value > accelThres ) ? 1.0f : -1.0f ) * frametime * accelSpeed;
		clamp( cg_gamepad_accelYaw, 1.0f, accelMax );
	} else {
		cg_gamepad_accelYaw = 1.0f;
	}

	value = fabs( sticks[axes + 1] );
	if( value > cg_gamepad_pitchThres->value ) {
		cg_gamepad_accelPitch += ( ( value > accelThres ) ? 1.0f : -1.0f ) * frametime * accelSpeed;
		clamp( cg_gamepad_accelPitch, 1.0f, accelMax );
	} else {
		cg_gamepad_accelPitch = 1.0f;
	}
}

/**
 * Adds view rotation from the gamepad.
 *
 * @param viewangles view angles to modify
 * @param frametime  real frame time
 * @param flip       horizontal flipping direction
 */
static void CG_AddGamepadViewAngles( vec3_t viewangles, float frametime, float flip ) {
	vec4_t sticks;
	trap_IN_GetThumbsticks( sticks );

	int axes = ( cg_gamepad_swapSticks->integer ? 0 : 2 );

	if( ( cg_gamepad_yawThres->value <= 0.0f ) || ( cg_gamepad_yawThres->value >= 1.0f ) ) {
		trap_Cvar_Set( cg_gamepad_yawThres->name, cg_gamepad_yawThres->dvalue );
	}
	if( ( cg_gamepad_pitchThres->value <= 0.0f ) || ( cg_gamepad_pitchThres->value >= 1.0f ) ) {
		trap_Cvar_Set( cg_gamepad_pitchThres->name, cg_gamepad_pitchThres->dvalue );
	}

	float axisValue = sticks[axes];
	float threshold = cg_gamepad_yawThres->value;
	float value = ( fabs( axisValue ) - threshold ) / ( 1.0f - threshold ); // Smoothly apply the dead zone.
	if( value > 0.0f ) {
		// Quadratic interpolation.
		viewangles[YAW] -= frametime * flip *
						   value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepad_accelYaw *
						   cg_gamepad_yawSpeed->value * CG_GetSensitivityScale( cg_gamepad_yawSpeed->value, 0.0f );
	}

	axisValue = sticks[axes + 1];
	threshold = cg_gamepad_pitchThres->value;
	value = ( fabs( axisValue ) - threshold ) / ( 1.0f - threshold );
	if( value > 0.0f ) {
		viewangles[PITCH] += frametime * ( cg_gamepad_pitchInvert->integer ? -1.0f : 1.0f ) *
							 value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepad_accelPitch *
							 cg_gamepad_pitchSpeed->value * CG_GetSensitivityScale( cg_gamepad_pitchSpeed->value, 0.0f );
	}
}

/**
 * Adds movement from the gamepad.
 *
 * @param movement movement vector to modify
 */
static void CG_AddGamepadMovement( vec3_t movement ) {
	vec4_t sticks;
	trap_IN_GetThumbsticks( sticks );

	int axes = ( cg_gamepad_swapSticks->integer ? 2 : 0 );

	float value = sticks[axes];
	float threshold = cg_gamepad_strafeThres->value;
	float runThreshold = cg_gamepad_strafeRunThres->value;
	float absValue = fabs( value );
	if( runThreshold > threshold ) {
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		clamp( absValue, 0.0f, 1.0f );
		absValue *= absValue;
	} else {
		absValue = ( float )( absValue > threshold );
	}
	if( absValue > cg_gamepad_strafeThres->value ) {
		movement[0] += absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );
	}

	value = sticks[axes + 1];
	threshold = cg_gamepad_moveThres->value;
	runThreshold = cg_gamepad_runThres->value;
	absValue = fabs( value );
	if( runThreshold > threshold ) {
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		clamp( absValue, 0.0f, 1.0f );
		absValue *= absValue;
	} else {
		absValue = ( float )( absValue > threshold );
	}
	if( absValue > cg_gamepad_moveThres->value ) {
		movement[1] -= absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );
	}
}

void CG_UpdateInput( float frametime ) {
	CG_GamepadFrame( frametime );
	CG_TouchFrame( frametime );
}

void CG_ClearInputState( void ) {
	cg_gamepad_accelPitch = cg_gamepad_accelYaw = 1.0f;

	CG_ClearHUDInputState();
}

unsigned int CG_GetButtonBits( void ) {
	return CG_GetTouchButtonBits();
}

void CG_AddViewAngles( vec3_t viewangles, float frametime, bool flipped ) {
	float flip = ( flipped ? -1.0f : 1.0f );
	CG_AddMouseViewAngles( viewangles, frametime, flipped );
	CG_AddGamepadViewAngles( viewangles, frametime, flip );
	CG_AddTouchViewAngles( viewangles, frametime, flip );
}

void CG_AddMovement( vec3_t movement ) {
	CG_AddGamepadMovement( movement );
	CG_AddTouchMovement( movement );
}

void CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize ) {
	int key;
	const char *bind;
	int numKeys = 0;
	const char *keyNames[2];
	char charKeys[2][2];

	memset( charKeys, 0, sizeof( charKeys ) );

	for( key = 0; key < 256; key++ ) {
		bind = trap_Key_GetBindingBuf( key );
		if( !bind || Q_stricmp( bind, cmd ) ) {
			continue;
		}

		if( ( key >= 'a' ) && ( key <= 'z' ) ) {
			charKeys[numKeys][0] = key - ( 'a' - 'A' );
			keyNames[numKeys] = charKeys[numKeys];
		} else {
			keyNames[numKeys] = trap_Key_KeynumToString( key );
		}

		numKeys++;
		if( numKeys == 2 ) {
			break;
		}
	}

	if( !numKeys ) {
		keyNames[0] = CG_TranslateString( "UNBOUND" );
	}

	if( numKeys == 2 ) {
		Q_snprintfz( keys, keysSize, CG_TranslateString( "%s or %s" ), keyNames[0], keyNames[1] );
	} else {
		Q_strncpyz( keys, keyNames[0], keysSize );
	}
}
