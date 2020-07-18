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
#include "../gameshared//q_keycodes.h"

static bool cg_inputCenterView;
static float cg_inputCenterViewPitch;

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
	CG_Overlay_MouseMove( &cg_overlay, mx, my );

	if( CG_Overlay_Hover( &cg_overlay ) ) {
		CG_asInputMouseMove( 0, 0 );
	} else {
		CG_asInputMouseMove( mx, my );
	}
}

/*
===============================================================================

TOUCH INPUT

===============================================================================
*/

static cg_touch_t cg_touches[CG_MAX_TOUCHES];
static cg_touchpad_t cg_touchpads[TOUCHPAD_COUNT];

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
* CG_KeyEvent
*/
bool CG_KeyEvent( int key, bool down ) {
	if( CG_Overlay_Hover( &cg_overlay ) && ( key >= K_MOUSE1 && key <= K_MOUSE1DBLCLK ) ) {
		CG_Overlay_KeyEvent( &cg_overlay, key, down );
		return true;
	}
	return CG_asInputKeyEvent( key, down );
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
	cg_touchpad_t *pad = CG_GetTouchpad( padID );
	if( !pad ) {
		return;
	}

	pad->touch = touchID;
	if( touchID >= 0 ) {
		cg_touch_t &touch = cg_touches[touchID];
		pad->x = ( float )touch.x;
		pad->y = ( float )touch.y;
	}
}

/*
* CG_GetTouchpad
*/
cg_touchpad_t *CG_GetTouchpad( int padID ) {
	if( padID < 0 || padID >= TOUCHPAD_COUNT ) {
		return NULL;
	}
	return &cg_touchpads[padID];
}

/*
===============================================================================

COMMON

===============================================================================
*/

/*
* CG_CenterView
*/
void CG_CenterView( float pitch ) {
	cg_inputCenterView = true;
	cg_inputCenterViewPitch = pitch;
}

/*
* CG_InputInit
*/
void CG_InitInput( void ) {
}

/*
* CG_ShutdownInput
*/
void CG_ShutdownInput( void ) {
}

/*
* CG_GetButtonBits
*/
unsigned int CG_GetButtonBits( void ) {
	int buttons = 0;

	buttons |= CG_asGetButtonBits();
	buttons |= CG_GetTouchButtonBits();

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
	
	CG_GetAngularMovement( am );

	if( flipped ) {
		am[YAW] = -am[YAW];
	}

	VectorAdd( viewAngles, am, viewAngles );

	if( cg_inputCenterView ) {
		viewAngles[PITCH] = cg_inputCenterViewPitch;
		cg_inputCenterView = false;
	}
}

/*
* CG_AddMovement
*/
void CG_AddMovement( vec3_t movement ) {
	vec3_t dm;
	bool flipped = cg_flip->integer != 0;

	CG_GetMovement( dm );

	if( flipped ) {
		dm[0] = dm[0] * -1.0;
	}

	VectorAdd( movement, dm, movement );
}

/*
* CG_GetAngularMovement
*/
void CG_GetAngularMovement( vec3_t movement ) {
	CG_asGetAngularMovement( movement );
}

/*
* CG_GetMovement
*/
void CG_GetMovement( vec3_t movement ) {
	int upmove;

	CG_asGetMovement( movement );

	CG_GetHUDTouchButtons( NULL, &upmove );
	movement[2] += ( float )upmove;
}

/*
* CG_InputFrame
*/
void CG_InputFrame( int64_t inputTime ) {
	CG_asInputFrame( inputTime );

	CG_TouchFrame();
}

/*
* CG_ClearInputState
*/
void CG_ClearInputState( void ) {
	CG_asInputClearState();

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
