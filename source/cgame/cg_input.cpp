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

static int64_t cg_inputTime;
static int cg_inputFrameTime;
static bool cg_inputCenterView;

/*
===============================================================================

MOUSE

===============================================================================
*/

/*
* CG_GetSensitivityScale
* Scale sensitivity for different view effects
*/
float CG_GetSensitivityScale( float sens, float zoomSens ) {
	float sensScale = 1.0f;

	if( !cgs.demoPlaying && sens && ( cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] > 0 ) ) {
		if( zoomSens ) {
			return zoomSens / sens;
		}

		return cg_zoomfov->value / cg_fov->value;
	}

	return sensScale;
}

/*
* CG_MouseMove
*/
void CG_MouseMove( int mx, int my ) {
	CG_asInputMouseMove( mx, my );
}

/*
===============================================================================

GAMEPAD

===============================================================================
*/

static cvar_t *cg_gamepad_moveThres;
static cvar_t *cg_gamepad_runThres;
static cvar_t *cg_gamepad_strafeThres;
static cvar_t *cg_gamepad_strafeRunThres;
static cvar_t *cg_gamepad_pitchThres;
static cvar_t *cg_gamepad_yawThres;
static cvar_t *cg_gamepad_pitchSpeed;
static cvar_t *cg_gamepad_yawSpeed;
static cvar_t *cg_gamepad_pitchInvert;
static cvar_t *cg_gamepad_accelMax;
static cvar_t *cg_gamepad_accelSpeed;
static cvar_t *cg_gamepad_accelThres;
static cvar_t *cg_gamepad_swapSticks;

static float cg_gamepadAccelPitch = 1.0f, cg_gamepadAccelYaw = 1.0f;

/**
 * Updates time-dependent gamepad state.
 *
 * @param frametime real frame time
 */
static void CG_GamepadFrame( void ) {
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
		cg_gamepadAccelYaw += ( ( value > accelThres ) ? 1.0f : -1.0f ) * cg_inputFrameTime * 0.001f * accelSpeed;
		clamp( cg_gamepadAccelYaw, 1.0f, accelMax );
	} else {
		cg_gamepadAccelYaw = 1.0f;
	}

	value = fabs( sticks[axes + 1] );
	if( value > cg_gamepad_pitchThres->value ) {
		cg_gamepadAccelPitch += ( ( value > accelThres ) ? 1.0f : -1.0f ) * cg_inputFrameTime * 0.001f * accelSpeed;
		clamp( cg_gamepadAccelPitch, 1.0f, accelMax );
	} else {
		cg_gamepadAccelPitch = 1.0f;
	}
}

/**
 * Adds view rotation from the gamepad.
 *
 * @param viewAngles view angles to modify
 */
static void CG_AddGamepadViewAngles( vec3_t viewAngles ) {
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
		viewAngles[YAW] -= cg_inputFrameTime * 0.001f *
						   value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepadAccelYaw *
						   cg_gamepad_yawSpeed->value * CG_GetSensitivityScale( cg_gamepad_yawSpeed->value, 0.0f );
	}

	axisValue = sticks[axes + 1];
	threshold = cg_gamepad_pitchThres->value;
	value = ( fabs( axisValue ) - threshold ) / ( 1.0f - threshold );
	if( value > 0.0f ) {
		viewAngles[PITCH] += cg_inputFrameTime * 0.001f * ( cg_gamepad_pitchInvert->integer ? -1.0f : 1.0f ) *
							 value * value * ( ( axisValue < 0.0f ) ? -1.0f : 1.0f ) * cg_gamepadAccelPitch *
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

/*
===============================================================================

TOUCH INPUT

===============================================================================
*/

cg_touch_t cg_touches[CG_MAX_TOUCHES];

typedef struct {
	int touch;
	float x, y;
} cg_touchpad_t;

static cg_touchpad_t cg_touchpads[TOUCHPAD_COUNT];

static cvar_t *cg_touch_moveThres;
static cvar_t *cg_touch_strafeThres;
static cvar_t *cg_touch_lookThres;
static cvar_t *cg_touch_lookSens;
static cvar_t *cg_touch_lookInvert;
static cvar_t *cg_touch_lookDecel;

/*
* CG_TouchArea
*
* Touches a rectangle. Returns touch id if it's a new touch.
*/
int CG_TouchArea( int area, int x, int y, int w, int h, void ( *upfunc )( int id, int64_t time ) ) {
	if( ( w <= 0 ) || ( h <= 0 ) ) {
		return -1;
	}

	int i;
	int x2 = x + w, y2 = y + h;

	// first check if already touched
	for( i = 0; i < CG_MAX_TOUCHES; ++i ) {
		cg_touch_t &touch = cg_touches[i];

		if( touch.down && ( ( touch.area & TOUCHAREA_MASK ) == ( area & TOUCHAREA_MASK ) ) ) {
			touch.area_valid = true;
			if( ( ( touch.area >> TOUCHAREA_SUB_SHIFT ) != ( area >> TOUCHAREA_SUB_SHIFT ) ) &&
				( touch.x >= x ) && ( touch.y >= y ) && ( touch.x < x2 ) && ( touch.y < y2 ) ) {
				if( touch.upfunc ) {
					touch.upfunc( i, 0 );
				}
				touch.area = area;
				return i;
			}
			return -1;
		}
	}

	// now add a new touch
	for( i = 0; i < CG_MAX_TOUCHES; ++i ) {
		cg_touch_t &touch = cg_touches[i];

		if( touch.down && ( touch.area == TOUCHAREA_NONE ) &&
			( touch.x >= x ) && ( touch.y >= y ) && ( touch.x < x2 ) && ( touch.y < y2 ) ) {
			touch.area = area;
			touch.area_valid = true;
			touch.upfunc = upfunc;
			return i;
		}
	}

	return -1;
}

/*
* CG_GetTouch
*/
cg_touch_t *CG_GetTouch( int id ) {
	if( id < 0 || id >= CG_MAX_TOUCHES ) {
		return NULL;
	}
	return &cg_touches[id];
}

/*
* CG_TouchEvent
*/
void CG_TouchEvent( int id, touchevent_t type, int x, int y, int64_t time ) {
	if( id < 0 || id >= CG_MAX_TOUCHES ) {
		return;
	}

	cg_touch_t &touch = cg_touches[id];
	touch.x = x;
	touch.y = y;

	switch( type ) {
	case TOUCH_DOWN:
	case TOUCH_MOVE:
		if( !touch.down ) {
			touch.down = true;
			touch.time = time;
			touch.area = TOUCHAREA_NONE;
		}
		break;

	case TOUCH_UP:
		if( touch.down ) {
			touch.down = false;
			if( ( touch.area != TOUCHAREA_NONE ) && touch.upfunc ) {
				touch.upfunc( id, time );
			}
		}
		break;
	}
}

/*
* CG_IsTouchDown
*/
bool CG_IsTouchDown( int id ) {
	if( id < 0 || id >= CG_MAX_TOUCHES ) {
		return false;
	}

	return cg_touches[id].down;
}

/*
* CG_TouchFrame
*/
static void CG_TouchFrame( void ) {
	int i;
	bool touching = false;

	cg_touchpad_t &viewpad = cg_touchpads[TOUCHPAD_VIEW];
	if( viewpad.touch >= 0 ) {
		if( cg_touch_lookDecel->modified ) {
			if( cg_touch_lookDecel->value < 0.0f ) {
				trap_Cvar_Set( cg_touch_lookDecel->name, cg_touch_lookDecel->dvalue );
			}
			cg_touch_lookDecel->modified = false;
		}

		cg_touch_t &touch = cg_touches[viewpad.touch];

		float decel = cg_touch_lookDecel->value * ( float )cg_inputFrameTime * 0.001f;
		float xdist = ( float )touch.x - viewpad.x;
		float ydist = ( float )touch.y - viewpad.y;
		viewpad.x += xdist * decel;
		viewpad.y += ydist * decel;

		// Check if decelerated too much (to the opposite direction)
		if( ( ( ( float )touch.x - viewpad.x ) * xdist ) < 0.0f ) {
			viewpad.x = touch.x;
		}
		if( ( ( ( float )touch.y - viewpad.y ) * ydist ) < 0.0f ) {
			viewpad.y = touch.y;
		}
	}

	for( i = 0; i < CG_MAX_TOUCHES; ++i ) {
		cg_touches[i].area_valid = false;
		if( cg_touches[i].down ) {
			touching = true;
		}
	}

	if( touching ) {
		CG_DrawHUD( true ); // FIXME

		// cancel non-existent areas
		for( i = 0; i < CG_MAX_TOUCHES; ++i ) {
			cg_touch_t &touch = cg_touches[i];
			if( touch.down ) {
				if( ( touch.area != TOUCHAREA_NONE ) && !touch.area_valid ) {
					if( touch.upfunc ) {
						touch.upfunc( i, 0 );
					}
					touch.area = TOUCHAREA_NONE;
				}
			}
		}
	}

	CG_UpdateHUDPostTouch();
}

/*
* CG_GetTouchButtonBits
*/
static int CG_GetTouchButtonBits( void ) {
	int buttons;
	CG_GetHUDTouchButtons( &buttons, NULL );
	return buttons;
}

static void CG_AddTouchViewAngles( vec3_t viewAngles ) {
	cg_touchpad_t &viewpad = cg_touchpads[TOUCHPAD_VIEW];
	if( viewpad.touch >= 0 ) {
		if( cg_touch_lookThres->modified ) {
			if( cg_touch_lookThres->value < 0.0f ) {
				trap_Cvar_Set( cg_touch_lookThres->name, cg_touch_lookThres->dvalue );
			}
			cg_touch_lookThres->modified = false;
		}

		cg_touch_t &touch = cg_touches[viewpad.touch];

		float speed = cg_touch_lookSens->value * cg_inputFrameTime * 0.001f * CG_GetSensitivityScale( 1.0f, 0.0f );
		float scale = 1.0f / cgs.pixelRatio;

		float angle = ( ( float )touch.y - viewpad.y ) * scale;
		if( cg_touch_lookInvert->integer ) {
			angle = -angle;
		}
		float dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
		angle = fabs( angle ) - cg_touch_lookThres->value;
		if( angle > 0.0f ) {
			viewAngles[PITCH] += angle * dir * speed;
		}

		angle = ( viewpad.x - ( float )touch.x ) * scale;
		dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
		angle = fabs( angle ) - cg_touch_lookThres->value;
		if( angle > 0.0f ) {
			viewAngles[YAW] += angle * dir * speed;
		}
	}
}

void CG_GetTouchMovement( vec3_t movement ) {
	int upmove;
	cg_touchpad_t &movepad = cg_touchpads[TOUCHPAD_MOVE];

	VectorClear( movement );

	if( movepad.touch >= 0 ) {
		if( cg_touch_moveThres->modified ) {
			if( cg_touch_moveThres->value < 0.0f ) {
				trap_Cvar_Set( cg_touch_moveThres->name, cg_touch_moveThres->dvalue );
			}
			cg_touch_moveThres->modified = false;
		}
		if( cg_touch_strafeThres->modified ) {
			if( cg_touch_strafeThres->value < 0.0f ) {
				trap_Cvar_Set( cg_touch_strafeThres->name, cg_touch_strafeThres->dvalue );
			}
			cg_touch_strafeThres->modified = false;
		}

		cg_touch_t &touch = cg_touches[movepad.touch];

		float move = ( float )touch.x - movepad.x;
		if( fabs( move ) > cg_touch_strafeThres->value * cgs.pixelRatio ) {
			movement[0] += ( move < 0 ) ? -1.0f : 1.0f;
		}

		move = movepad.y - ( float )touch.y;
		if( fabs( move ) > cg_touch_moveThres->value * cgs.pixelRatio ) {
			movement[1] += ( move < 0 ) ? -1.0f : 1.0f;
		}
	}

	CG_GetHUDTouchButtons( NULL, &upmove );
	movement[2] = ( float )upmove;
}

static void CG_AddTouchMovement( vec3_t movement ) {
	vec3_t tm;

	CG_GetTouchMovement( tm );

	VectorAdd( movement, tm, movement );
}

/*
* CG_CancelTouches
*/
void CG_CancelTouches( void ) {
	int i;

	for( i = 0; i < CG_MAX_TOUCHES; ++i ) {
		cg_touch_t &touch = cg_touches[i];
		if( touch.down ) {
			if( touch.area != TOUCHAREA_NONE ) {
				if( touch.upfunc ) {
					touch.upfunc( i, 0 );
				}
				touch.area = TOUCHAREA_NONE;
			}
			touch.down = false;
		}
	}
}

/*
* CG_SetTouchpad
*/
void CG_SetTouchpad( int padID, int touchID ) {
	cg_touchpad_t &pad = cg_touchpads[padID];

	pad.touch = touchID;

	if( touchID >= 0 ) {
		cg_touch_t &touch = cg_touches[touchID];
		pad.x = ( float )touch.x;
		pad.y = ( float )touch.y;
	}
}

/*
===============================================================================

COMMON

===============================================================================
*/

/*
* CG_CenterView
*/
static void CG_CenterView( void ) {
	cg_inputCenterView = true;
}

/*
* CG_InputInit
*/
void CG_InitInput( void ) {
	CG_asLoadInputScript();

	CG_asInputInit();

	trap_Cmd_AddCommand( "centerview", CG_CenterView );

	cg_gamepad_moveThres = trap_Cvar_Get( "cg_gamepad_moveThres", "0.239", CVAR_ARCHIVE );
	cg_gamepad_runThres = trap_Cvar_Get( "cg_gamepad_runThres", "0.75", CVAR_ARCHIVE );
	cg_gamepad_strafeThres = trap_Cvar_Get( "cg_gamepad_strafeThres", "0.239", CVAR_ARCHIVE );
	cg_gamepad_strafeRunThres = trap_Cvar_Get( "cg_gamepad_strafeRunThres", "0.45", CVAR_ARCHIVE );
	cg_gamepad_pitchThres = trap_Cvar_Get( "cg_gamepad_pitchThres", "0.265", CVAR_ARCHIVE );
	cg_gamepad_yawThres = trap_Cvar_Get( "cg_gamepad_yawThres", "0.265", CVAR_ARCHIVE );
	cg_gamepad_pitchSpeed = trap_Cvar_Get( "cg_gamepad_pitchSpeed", "240", CVAR_ARCHIVE );
	cg_gamepad_yawSpeed = trap_Cvar_Get( "cg_gamepad_yawSpeed", "260", CVAR_ARCHIVE );
	cg_gamepad_pitchInvert = trap_Cvar_Get( "cg_gamepad_pitchInvert", "0", CVAR_ARCHIVE );
	cg_gamepad_accelMax = trap_Cvar_Get( "cg_gamepad_accelMax", "2", CVAR_ARCHIVE );
	cg_gamepad_accelSpeed = trap_Cvar_Get( "cg_gamepad_accelSpeed", "3", CVAR_ARCHIVE );
	cg_gamepad_accelThres = trap_Cvar_Get( "cg_gamepad_accelThres", "0.9", CVAR_ARCHIVE );
	cg_gamepad_swapSticks = trap_Cvar_Get( "cg_gamepad_swapSticks", "0", CVAR_ARCHIVE );

	cg_touch_moveThres = trap_Cvar_Get( "cg_touch_moveThres", "24", CVAR_ARCHIVE );
	cg_touch_strafeThres = trap_Cvar_Get( "cg_touch_strafeThres", "32", CVAR_ARCHIVE );
	cg_touch_lookThres = trap_Cvar_Get( "cg_touch_lookThres", "5", CVAR_ARCHIVE );
	cg_touch_lookSens = trap_Cvar_Get( "cg_touch_lookSens", "9", CVAR_ARCHIVE );
	cg_touch_lookInvert = trap_Cvar_Get( "cg_touch_lookInvert", "0", CVAR_ARCHIVE );
	cg_touch_lookDecel = trap_Cvar_Get( "cg_touch_lookDecel", "8.5", CVAR_ARCHIVE );
}

/*
* CG_ShutdownInput
*/
void CG_ShutdownInput( void ) {
	CG_asInputShutdown();

	CG_asUnloadInputScript();

	trap_Cmd_RemoveCommand( "centerview" );
}

/*
* CG_GetButtonBits
*/
unsigned int CG_GetButtonBits( void ) {
	int buttons = 0;

	buttons |= CG_GetTouchButtonBits();

	buttons |= CG_asGetButtonBits();

	return buttons;
}

/**
* Adds view rotation from all kinds of input devices.
*
* @param viewAngles view angles to modify
* @param flipped    horizontal flipping direction
*/
void CG_AddViewAngles( vec3_t viewAngles ) {
	vec3_t am;
	bool flipped = cg_flip->integer != 0;
	
	VectorClear( am );

	CG_AddGamepadViewAngles( am );
	CG_AddTouchViewAngles( am );

	if( flipped ) {
		am[YAW] = -am[YAW];
	}
	VectorAdd( viewAngles, am, viewAngles );

	if( cg_inputCenterView ) {
		viewAngles[PITCH] = -SHORT2ANGLE( cg.predictedPlayerState.pmove.delta_angles[PITCH] );
		cg_inputCenterView = false;
	}

	VectorCopy( viewAngles, am );
	CG_asAddViewAngles( am );
	VectorCopy( am, viewAngles );
}

/*
* CG_AddMovement
*/
void CG_AddMovement( vec3_t movement ) {
	vec3_t dm;
	bool flipped = cg_flip->integer != 0;

	VectorClear( dm );

	CG_AddGamepadMovement( dm );
	CG_AddTouchMovement( dm );

	if( flipped ) {
		dm[0] = dm[0] * -1.0;
	}
	VectorAdd( movement, dm, movement );

	VectorCopy( movement, dm );
	CG_asAddMovement( dm );
	VectorCopy( dm, movement );
}

/*
* CG_InputFrame
*/
void CG_InputFrame( int frameTime ) {
	CG_asInputFrame( frameTime );

	cg_inputTime = trap_Milliseconds();
	cg_inputFrameTime = frameTime;

	CG_GamepadFrame();

	CG_TouchFrame();
}

/*
* CG_ClearInputState
*/
void CG_ClearInputState( void ) {
	CG_asInputClearState();

	cg_inputFrameTime = 0;
	cg_gamepadAccelPitch = 1.0f;
	cg_gamepadAccelYaw = 1.0f;

	CG_ClearHUDInputState();
}

/*
* CG_GetBoundKeysString
*/
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
