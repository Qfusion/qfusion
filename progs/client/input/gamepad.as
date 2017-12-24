/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2015 SiPlus, Chasseur de bots
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


namespace CGame {

namespace Input {

namespace Gamepad {

Cvar cg_gamepad_moveThres( "cg_gamepad_moveThres", "0.239", CVAR_ARCHIVE );
Cvar cg_gamepad_runThres( "cg_gamepad_runThres", "0.75", CVAR_ARCHIVE );
Cvar cg_gamepad_strafeThres( "cg_gamepad_strafeThres", "0.239", CVAR_ARCHIVE );
Cvar cg_gamepad_strafeRunThres( "cg_gamepad_strafeRunThres", "0.45", CVAR_ARCHIVE );
Cvar cg_gamepad_pitchThres( "cg_gamepad_pitchThres", "0.265", CVAR_ARCHIVE );
Cvar cg_gamepad_yawThres( "cg_gamepad_yawThres", "0.265", CVAR_ARCHIVE );
Cvar cg_gamepad_pitchSpeed( "cg_gamepad_pitchSpeed", "240", CVAR_ARCHIVE );
Cvar cg_gamepad_yawSpeed( "cg_gamepad_yawSpeed", "260", CVAR_ARCHIVE );
Cvar cg_gamepad_pitchInvert( "cg_gamepad_pitchInvert", "0", CVAR_ARCHIVE );
Cvar cg_gamepad_accelMax( "cg_gamepad_accelMax", "2", CVAR_ARCHIVE );
Cvar cg_gamepad_accelSpeed( "cg_gamepad_accelSpeed", "3", CVAR_ARCHIVE );
Cvar cg_gamepad_accelThres( "cg_gamepad_accelThres", "0.9", CVAR_ARCHIVE );
Cvar cg_gamepad_swapSticks( "cg_gamepad_swapSticks", "0", CVAR_ARCHIVE );

float cg_gamepadAccelPitch = 1.0f, cg_gamepadAccelYaw = 1.0f;

/**
 * Updates time-dependent gamepad state.
 */
void Frame( void ) {
	// Add acceleration to the gamepad look above the acceleration threshold.
	Vec4 sticks = GetThumbsticks();

	int axes = ( cg_gamepad_swapSticks.integer != 0 ? 0 : 2 );

	if( cg_gamepad_accelMax.value < 0.0f ) {
		cg_gamepad_accelMax.set( 0.0f );
	}
	if( cg_gamepad_accelSpeed.value < 0.0f ) {
		cg_gamepad_accelSpeed.set( 0.0f );
	}

	float accelMax = cg_gamepad_accelMax.value + 1.0f;
	float accelSpeed = cg_gamepad_accelSpeed.value;
	float accelThres = cg_gamepad_accelThres.value;

	float value = abs( sticks[axes] );
	if( value > cg_gamepad_yawThres.value ) {
		cg_gamepadAccelYaw += ( ( value > accelThres ) ? 1.0f : -1.0f ) * frameTime * 0.001f * accelSpeed;
		if( cg_gamepadAccelYaw < 1.0f ) cg_gamepadAccelYaw = 1.0f;
		else if( cg_gamepadAccelYaw > accelMax ) cg_gamepadAccelYaw = accelMax;
	} else {
		cg_gamepadAccelYaw = 1.0f;
	}

	value = abs( sticks[axes + 1] );
	if( value > cg_gamepad_pitchThres.value ) {
		cg_gamepadAccelPitch += ( ( value > accelThres ) ? 1.0f : -1.0f ) * frameTime * 0.001f * accelSpeed;
		if( cg_gamepadAccelPitch < 1.0f ) cg_gamepadAccelPitch = 1.0f;
		else if( cg_gamepadAccelPitch > accelMax ) cg_gamepadAccelPitch = accelMax;
	} else {
		cg_gamepadAccelPitch = 1.0f;
	}
}

/*
* ClearState
*/
void ClearState( void ) {
	cg_gamepadAccelPitch = 1.0f;
	cg_gamepadAccelYaw = 1.0f;
}

/**
 * Adds view rotation from the gamepad.
 */
Vec3 GetAngularMovement() {
	Vec3 viewAngles;
	Vec4 sticks = GetThumbsticks();

	int axes = ( cg_gamepad_swapSticks.integer != 0 ? 0 : 2 );

	if( ( cg_gamepad_yawThres.value <= 0.0f ) || ( cg_gamepad_yawThres.value >= 1.0f ) ) {
		cg_gamepad_yawThres.reset();
	}
	if( ( cg_gamepad_pitchThres.value <= 0.0f ) || ( cg_gamepad_pitchThres.value >= 1.0f ) ) {
		cg_gamepad_pitchThres.reset();
	}

	float axisValue = sticks[axes];
	float threshold = cg_gamepad_yawThres.value;
	float value = ( abs( axisValue ) - threshold ) / ( 1.0f - threshold ); // Smoothly apply the dead zone.
	if( value > 0.0f ) {
		// Quadratic interpolation.
		viewAngles[YAW] -= frameTime * 0.001f *
						   value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepadAccelYaw *
						   cg_gamepad_yawSpeed.value * GetSensitivityScale( cg_gamepad_yawSpeed.value, 0.0f );
	}

	axisValue = sticks[axes + 1];
	threshold = cg_gamepad_pitchThres.value;
	value = ( abs( axisValue ) - threshold ) / ( 1.0f - threshold );
	if( value > 0.0f ) {
		viewAngles[PITCH] += frameTime * 0.001f * ( cg_gamepad_pitchInvert.integer != 0 ? -1.0f : 1.0f ) *
							 value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepadAccelPitch *
							 cg_gamepad_pitchSpeed.value * GetSensitivityScale( cg_gamepad_pitchSpeed.value, 0.0f );
	}
	
	return viewAngles;
}

/**
 * Adds movement from the gamepad.
 */
Vec3 GetMovement() {
	Vec3 movement;
	Vec4 sticks = GetThumbsticks();

	int axes = ( cg_gamepad_swapSticks.integer != 0 ? 2 : 0 );

	float value = sticks[axes];
	float threshold = cg_gamepad_strafeThres.value;
	float runThreshold = cg_gamepad_strafeRunThres.value;
	float absValue = abs( value );
	if( runThreshold > threshold ) {
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		if( absValue < 0.0f ) absValue = 0.0f;
		else if( absValue > 1.0f ) absValue = 1.0f;
		absValue *= absValue;
	} else {
		absValue = ( absValue > threshold ? 1.0 : 0.0 );
	}
	if( absValue > cg_gamepad_strafeThres.value ) {
		movement[0] += absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );
	}

	value = sticks[axes + 1];
	threshold = cg_gamepad_moveThres.value;
	runThreshold = cg_gamepad_runThres.value;
	absValue = abs( value );
	if( runThreshold > threshold ) {
		absValue = ( absValue - threshold ) / ( runThreshold - threshold );
		if( absValue < 0.0f ) absValue = 0.0f;
		else if( absValue > 1.0f ) absValue = 1.0f;
		absValue *= absValue;
	} else {
		absValue = ( absValue > threshold ? 1.0 : 0.0 );
	}
	if( absValue > cg_gamepad_moveThres.value ) {
		movement[1] -= absValue * ( ( value < 0.0f ) ? -1.0f : 1.0f );
	}
	
	return movement;
}

}

}

}
