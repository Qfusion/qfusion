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

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, bool down, int64_t time);

===============================================================================
*/

typedef struct {
	int down[2];            // key nums holding it down
	int64_t downtime;       // msec timestamp
	unsigned msec;          // msec down this frame
	int state;
} kbutton_t;

static kbutton_t in_klook;
static kbutton_t in_left, in_right, in_forward, in_back;
static kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
static kbutton_t in_strafe, in_speed, in_use, in_attack;
static kbutton_t in_up, in_down;
static kbutton_t in_special;
static kbutton_t in_zoom;

static cvar_t *cl_yawspeed;
static cvar_t *cl_pitchspeed;

static cvar_t *cl_run;

static cvar_t *cl_anglespeedkey;

/*
* CG_KeyDown
*/
static void CG_KeyDown( kbutton_t *b ) {
	int k;
	const char *c;

	c = trap_Cmd_Argv( 1 );
	if( c[0] ) {
		k = atoi( c );
	} else {
		k = -1; // typed manually at the console for continuous down

	}
	if( k == b->down[0] || k == b->down[1] ) {
		return; // repeating key

	}
	if( !b->down[0] ) {
		b->down[0] = k;
	} else if( !b->down[1] ) {
		b->down[1] = k;
	} else {
		Com_Printf( "Three keys down for a button!\n" );
		return;
	}

	if( b->state & 1 ) {
		return; // still down

	}
	// save timestamp
	c = trap_Cmd_Argv( 2 );
	b->downtime = atoi( c );
	if( !b->downtime ) {
		b->downtime = cg_inputTime - 100;
	}

	b->state |= 1 + 2; // down + impulse down
}

/*
* CG_KeyUp
*/
static void CG_KeyUp( kbutton_t *b ) {
	int k;
	const char *c;
	int uptime;

	c = trap_Cmd_Argv( 1 );
	if( c[0] ) {
		k = atoi( c );
	} else { // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4; // impulse up
		return;
	}

	if( b->down[0] == k ) {
		b->down[0] = 0;
	} else if( b->down[1] == k ) {
		b->down[1] = 0;
	} else {
		return; // key up without corresponding down (menu pass through)
	}
	if( b->down[0] || b->down[1] ) {
		return; // some other key is still holding it down

	}
	if( !( b->state & 1 ) ) {
		return; // still up (this should not happen)

	}
	// save timestamp
	c = trap_Cmd_Argv( 2 );
	uptime = atoi( c );
	if( uptime ) {
		b->msec += uptime - b->downtime;
	} else {
		b->msec += 10;
	}

	b->state &= ~1; // now up
	b->state |= 4;  // impulse up
}

static void IN_KLookDown( void ) { CG_KeyDown( &in_klook ); }
static void IN_KLookUp( void ) { CG_KeyUp( &in_klook ); }
static void IN_UpDown( void ) { CG_KeyDown( &in_up ); }
static void IN_UpUp( void ) { CG_KeyUp( &in_up ); }
static void IN_DownDown( void ) { CG_KeyDown( &in_down ); }
static void IN_DownUp( void ) { CG_KeyUp( &in_down ); }
static void IN_LeftDown( void ) { CG_KeyDown( &in_left ); }
static void IN_LeftUp( void ) { CG_KeyUp( &in_left ); }
static void IN_RightDown( void ) { CG_KeyDown( &in_right ); }
static void IN_RightUp( void ) { CG_KeyUp( &in_right ); }
static void IN_ForwardDown( void ) { CG_KeyDown( &in_forward ); }
static void IN_ForwardUp( void ) { CG_KeyUp( &in_forward ); }
static void IN_BackDown( void ) { CG_KeyDown( &in_back ); }
static void IN_BackUp( void ) { CG_KeyUp( &in_back ); }
static void IN_LookupDown( void ) { CG_KeyDown( &in_lookup ); }
static void IN_LookupUp( void ) { CG_KeyUp( &in_lookup ); }
static void IN_LookdownDown( void ) { CG_KeyDown( &in_lookdown ); }
static void IN_LookdownUp( void ) { CG_KeyUp( &in_lookdown ); }
static void IN_MoveleftDown( void ) { CG_KeyDown( &in_moveleft ); }
static void IN_MoveleftUp( void ) { CG_KeyUp( &in_moveleft ); }
static void IN_MoverightDown( void ) { CG_KeyDown( &in_moveright ); }
static void IN_MoverightUp( void ) { CG_KeyUp( &in_moveright ); }
static void IN_SpeedDown( void ) { CG_KeyDown( &in_speed ); }
static void IN_SpeedUp( void ) { CG_KeyUp( &in_speed ); }
static void IN_StrafeDown( void ) { CG_KeyDown( &in_strafe ); }
static void IN_StrafeUp( void ) { CG_KeyUp( &in_strafe ); }
static void IN_AttackDown( void ) { CG_KeyDown( &in_attack ); }
static void IN_AttackUp( void ) { CG_KeyUp( &in_attack ); }
static void IN_UseDown( void ) { CG_KeyDown( &in_use ); }
static void IN_UseUp( void ) { CG_KeyUp( &in_use ); }
static void IN_SpecialDown( void ) { CG_KeyDown( &in_special ); }
static void IN_SpecialUp( void ) { CG_KeyUp( &in_special ); }
static void IN_ZoomDown( void ) { CG_KeyDown( &in_zoom ); }
static void IN_ZoomUp( void ) { CG_KeyUp( &in_zoom ); }


/*
* CG_KeyState
*/
static float CG_KeyState( kbutton_t *key ) {
	float val;
	int msec;

	key->state &= 1; // clear impulses

	msec = key->msec;
	key->msec = 0;

	if( key->state ) {
		// still down
		msec += cg_inputTime - key->downtime;
		key->downtime = cg_inputTime;
	}

	if( !cg_inputFrameTime )
		return 0;

	val = (float) msec / (float)cg_inputFrameTime;

	return bound( 0, val, 1 );
}

/*
* CG_AddKeysViewAngles
*/
static void CG_AddKeysViewAngles( vec3_t viewAngles ) {
	float speed;

	if( in_speed.state & 1 ) {
		speed = ( (float)cg_inputFrameTime * 0.001f ) * cl_anglespeedkey->value;
	} else {
		speed = (float)cg_inputFrameTime * 0.001f;
	}

	if( !( in_strafe.state & 1 ) ) {
		viewAngles[YAW] -= speed * cl_yawspeed->value * CG_KeyState( &in_right );
		viewAngles[YAW] += speed * cl_yawspeed->value * CG_KeyState( &in_left );
	}
	if( in_klook.state & 1 ) {
		viewAngles[PITCH] -= speed * cl_pitchspeed->value * CG_KeyState( &in_forward );
		viewAngles[PITCH] += speed * cl_pitchspeed->value * CG_KeyState( &in_back );
	}

	viewAngles[PITCH] -= speed * cl_pitchspeed->value * CG_KeyState( &in_lookup );
	viewAngles[PITCH] += speed * cl_pitchspeed->value * CG_KeyState( &in_lookdown );
}

/*
* CG_AddKeysMovement
*/
static void CG_AddKeysMovement( vec3_t movement ) {
	float down;

	if( in_strafe.state & 1 ) {
		movement[0] += CG_KeyState( &in_right );
		movement[0] -= CG_KeyState( &in_left );
	}

	movement[0] += CG_KeyState( &in_moveright );
	movement[0] -= CG_KeyState( &in_moveleft );

	if( !( in_klook.state & 1 ) ) {
		movement[1] += CG_KeyState( &in_forward );
		movement[1] -= CG_KeyState( &in_back );
	}

	movement[2] += CG_KeyState( &in_up );
	down = CG_KeyState( &in_down );
	if( down > movement[2] ) {
		movement[2] -= down;
	}
}

/*
* CG_GetButtonBitsFromKeys
*/
unsigned int CG_GetButtonBitsFromKeys( void ) {
	int buttons = 0;

	// figure button bits

	if( in_attack.state & 3 ) {
		buttons |= BUTTON_ATTACK;
	}
	in_attack.state &= ~2;

	if( in_special.state & 3 ) {
		buttons |= BUTTON_SPECIAL;
	}
	in_special.state &= ~2;

	if( in_use.state & 3 ) {
		buttons |= BUTTON_USE;
	}
	in_use.state &= ~2;

	if( ( in_speed.state & 1 ) ^ !cl_run->integer ) {
		buttons |= BUTTON_WALK;
	}

	if( in_zoom.state & 3 ) {
		buttons |= BUTTON_ZOOM;
	}
	in_zoom.state &= ~2;

	return buttons;
}

/*
===============================================================================

MOUSE

===============================================================================
*/

static cvar_t *sensitivity;
static cvar_t *zoomsens;
static cvar_t *m_accel;
static cvar_t *m_accelStyle;
static cvar_t *m_accelOffset;
static cvar_t *m_accelPow;
static cvar_t *m_filter;
static cvar_t *m_sensCap;

static cvar_t *m_pitch;
static cvar_t *m_yaw;

static float mouse_x = 0, mouse_y = 0;

/*
* CG_MouseMove
*/
void CG_MouseMove( int mx, int my ) {
	static float old_mouse_x = 0, old_mouse_y = 0;
	float accelSensitivity;

	CG_asInputMouseMove( mx, my );

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

	if( m_accel->value != 0.0f && cg_inputFrameTime != 0 ) {
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

			base[0] = (float) ( abs( mx ) ) / (float) cg_inputFrameTime;
			base[1] = (float) ( abs( my ) ) / (float) cg_inputFrameTime;
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

			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)cg_inputFrameTime;
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
			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)cg_inputFrameTime;
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
* @param viewAngles view angles to modify
*/
static void CG_AddMouseViewAngles( vec3_t viewAngles ) {
	if( !mouse_x && !mouse_y ) {
		return;
	}

	// add mouse X/Y movement to cmd
	viewAngles[YAW] -= m_yaw->value * mouse_x;
	viewAngles[PITCH] += m_pitch->value * mouse_y;
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

	trap_Cmd_AddCommand( "+moveup", IN_UpDown );
	trap_Cmd_AddCommand( "-moveup", IN_UpUp );
	trap_Cmd_AddCommand( "+movedown", IN_DownDown );
	trap_Cmd_AddCommand( "-movedown", IN_DownUp );
	trap_Cmd_AddCommand( "+left", IN_LeftDown );
	trap_Cmd_AddCommand( "-left", IN_LeftUp );
	trap_Cmd_AddCommand( "+right", IN_RightDown );
	trap_Cmd_AddCommand( "-right", IN_RightUp );
	trap_Cmd_AddCommand( "+forward", IN_ForwardDown );
	trap_Cmd_AddCommand( "-forward", IN_ForwardUp );
	trap_Cmd_AddCommand( "+back", IN_BackDown );
	trap_Cmd_AddCommand( "-back", IN_BackUp );
	trap_Cmd_AddCommand( "+lookup", IN_LookupDown );
	trap_Cmd_AddCommand( "-lookup", IN_LookupUp );
	trap_Cmd_AddCommand( "+lookdown", IN_LookdownDown );
	trap_Cmd_AddCommand( "-lookdown", IN_LookdownUp );
	trap_Cmd_AddCommand( "+strafe", IN_StrafeDown );
	trap_Cmd_AddCommand( "-strafe", IN_StrafeUp );
	trap_Cmd_AddCommand( "+moveleft", IN_MoveleftDown );
	trap_Cmd_AddCommand( "-moveleft", IN_MoveleftUp );
	trap_Cmd_AddCommand( "+moveright", IN_MoverightDown );
	trap_Cmd_AddCommand( "-moveright", IN_MoverightUp );
	trap_Cmd_AddCommand( "+speed", IN_SpeedDown );
	trap_Cmd_AddCommand( "-speed", IN_SpeedUp );
	trap_Cmd_AddCommand( "+attack", IN_AttackDown );
	trap_Cmd_AddCommand( "-attack", IN_AttackUp );
	trap_Cmd_AddCommand( "+use", IN_UseDown );
	trap_Cmd_AddCommand( "-use", IN_UseUp );
	trap_Cmd_AddCommand( "+klook", IN_KLookDown );
	trap_Cmd_AddCommand( "-klook", IN_KLookUp );
	// wsw
	trap_Cmd_AddCommand( "+special", IN_SpecialDown );
	trap_Cmd_AddCommand( "-special", IN_SpecialUp );
	trap_Cmd_AddCommand( "+zoom", IN_ZoomDown );
	trap_Cmd_AddCommand( "-zoom", IN_ZoomUp );

	trap_Cmd_AddCommand( "centerview", CG_CenterView );

	cl_yawspeed =  trap_Cvar_Get( "cl_yawspeed", "140", 0 );
	cl_pitchspeed = trap_Cvar_Get( "cl_pitchspeed", "150", 0 );
	cl_anglespeedkey = trap_Cvar_Get( "cl_anglespeedkey", "1.5", 0 );

	cl_run = trap_Cvar_Get( "cl_run", "1", CVAR_ARCHIVE );

	sensitivity = trap_Cvar_Get( "sensitivity", "3", CVAR_ARCHIVE );
	zoomsens = trap_Cvar_Get( "zoomsens", "0", CVAR_ARCHIVE );
	m_accel = trap_Cvar_Get( "m_accel", "0", CVAR_ARCHIVE );
	m_accelStyle = trap_Cvar_Get( "m_accelStyle", "0", CVAR_ARCHIVE );
	m_accelOffset = trap_Cvar_Get( "m_accelOffset", "0", CVAR_ARCHIVE );
	m_accelPow = trap_Cvar_Get( "m_accelPow", "2", CVAR_ARCHIVE );
	m_filter = trap_Cvar_Get( "m_filter", "0", CVAR_ARCHIVE );
	m_pitch = trap_Cvar_Get( "m_pitch", "0.022", CVAR_ARCHIVE );
	m_yaw = trap_Cvar_Get( "m_yaw", "0.022", CVAR_ARCHIVE );
	m_sensCap = trap_Cvar_Get( "m_sensCap", "0", CVAR_ARCHIVE );

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

	trap_Cmd_RemoveCommand( "+moveup" );
	trap_Cmd_RemoveCommand( "-moveup" );
	trap_Cmd_RemoveCommand( "+movedown" );
	trap_Cmd_RemoveCommand( "-movedown" );
	trap_Cmd_RemoveCommand( "+left" );
	trap_Cmd_RemoveCommand( "-left" );
	trap_Cmd_RemoveCommand( "+right" );
	trap_Cmd_RemoveCommand( "-right" );
	trap_Cmd_RemoveCommand( "+forward" );
	trap_Cmd_RemoveCommand( "-forward" );
	trap_Cmd_RemoveCommand( "+back" );
	trap_Cmd_RemoveCommand( "-back" );
	trap_Cmd_RemoveCommand( "+lookup" );
	trap_Cmd_RemoveCommand( "-lookup" );
	trap_Cmd_RemoveCommand( "+lookdown" );
	trap_Cmd_RemoveCommand( "-lookdown" );
	trap_Cmd_RemoveCommand( "+strafe" );
	trap_Cmd_RemoveCommand( "-strafe" );
	trap_Cmd_RemoveCommand( "+moveleft" );
	trap_Cmd_RemoveCommand( "-moveleft" );
	trap_Cmd_RemoveCommand( "+moveright" );
	trap_Cmd_RemoveCommand( "-moveright" );
	trap_Cmd_RemoveCommand( "+speed" );
	trap_Cmd_RemoveCommand( "-speed" );
	trap_Cmd_RemoveCommand( "+attack" );
	trap_Cmd_RemoveCommand( "-attack" );
	trap_Cmd_RemoveCommand( "+use" );
	trap_Cmd_RemoveCommand( "-use" );
	trap_Cmd_RemoveCommand( "+klook" );
	trap_Cmd_RemoveCommand( "-klook" );
	// wsw
	trap_Cmd_RemoveCommand( "+special" );
	trap_Cmd_RemoveCommand( "-special" );
	trap_Cmd_RemoveCommand( "+zoom" );
	trap_Cmd_RemoveCommand( "-zoom" );

	trap_Cmd_RemoveCommand( "centerview" );
}

/*
* CG_GetButtonBits
*/
unsigned int CG_GetButtonBits( void ) {
	int buttons = 0;

	// figure button bits
	buttons |= CG_GetButtonBitsFromKeys();

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

	CG_AddKeysViewAngles( am );
	CG_AddGamepadViewAngles( am );
	CG_AddTouchViewAngles( am );
	CG_AddMouseViewAngles( am );

	if( flipped ) {
		am[YAW] = -am[YAW];
	}
	VectorAdd( viewAngles, am, viewAngles );

	if( cg_inputCenterView ) {
		viewAngles[PITCH] = -SHORT2ANGLE( cg.predictedPlayerState.pmove.delta_angles[PITCH] );
		cg_inputCenterView = false;
	}
}

/*
* CG_AddMovement
*/
void CG_AddMovement( vec3_t movement ) {
	vec3_t dm;
	bool flipped = cg_flip->integer != 0;

	VectorClear( dm );

	CG_AddKeysMovement( dm );
	CG_AddGamepadMovement( dm );
	CG_AddTouchMovement( dm );

	if( flipped ) {
		dm[0] = dm[0] * -1.0;
	}
	VectorAdd( movement, dm, movement );
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
