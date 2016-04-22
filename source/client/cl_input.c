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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t *cl_ucmdMaxResend;

cvar_t *cl_ucmdFPS;
#ifdef UCMDTIMENUDGE
cvar_t *cl_ucmdTimeNudge;
#endif

extern cvar_t *cl_maxfps;

extern unsigned	sys_frame_time;
static unsigned ucmd_frame_time;

/*
===============================================================================

MOUSE

===============================================================================
*/
extern cvar_t *in_grabinconsole;
extern cvar_t *m_filter;
extern cvar_t *m_filterStrength;
extern cvar_t *m_accel;
extern cvar_t *m_accelStyle;
extern cvar_t *m_accelOffset;
extern cvar_t *m_accelPow;
extern cvar_t *m_sensCap;

#define M_FILTER_NONE		0
#define M_FILTER_INTERPOLATE	1
#define M_FILTER_EXTRAPOLATE	2

#define M_STRINGIFY( x ) # x
#define M_DOUBLEQUOTE( x ) M_STRINGIFY( x )

static unsigned int mouse_frame_time = 0;

static const unsigned int DEFAULT_BUF_SIZE = 5;
static float *buf_x = NULL, *buf_y = NULL;
static unsigned int buf_size = 0;
static float buf_decay = 0.5;

/*
* CL_MouseFilterBufferSizeGet_f
*/
static dynvar_get_status_t CL_MouseFilterBufferSizeGet_f( void **size )
{
	static char sizeStr[16];
	sprintf( sizeStr, "%u", buf_size );
	*size = sizeStr;
	return DYNVAR_GET_OK;
}

/*
* CL_MouseFilterBufferSizeSet_f
*/
static dynvar_set_status_t CL_MouseFilterBufferSizeSet_f( void *size )
{
	static const unsigned int MIN_SIZE = 1;
	static const unsigned int MAX_SIZE = 32; // more is pointless (probably anything more than 5)
	const unsigned int desiredSize = atoi( (char *) size );
	if( desiredSize >= MIN_SIZE && desiredSize <= MAX_SIZE )
	{
		// reallocate buffer
		if( m_filter->integer != M_FILTER_EXTRAPOLATE )
			Com_Printf( "Warning: \"m_filterBufferSize\" has no effect unless \"m_filter " M_DOUBLEQUOTE( M_FILTER_EXTRAPOLATE ) "\" is set.\n" );
		Mem_ZoneFree( buf_x );
		Mem_ZoneFree( buf_y );
		buf_x = (float *) Mem_ZoneMalloc( sizeof( float ) * desiredSize );
		buf_y = (float *) Mem_ZoneMalloc( sizeof( float ) * desiredSize );
		// reset to 0
		memset( buf_x, 0, sizeof( float ) * desiredSize );
		memset( buf_y, 0, sizeof( float ) * desiredSize );
		buf_size = desiredSize;
		return DYNVAR_SET_OK;
	}
	else
	{
		Com_Printf( "\"m_filterBufferSize\" must be between \"%d\" and \"%d\".\n", MIN_SIZE, MAX_SIZE );
		return DYNVAR_SET_INVALID;
	}
}

/*
* CL_MouseFilterBufferDecayGet_f
*/
static dynvar_get_status_t CL_MouseFilterBufferDecayGet_f( void **decay )
{
	static char decayStr[16];
	sprintf( decayStr, "%f", buf_decay );
	*decay = decayStr;
	return DYNVAR_GET_OK;
}

/*
* CL_MouseFilterBufferDecaySet_f
*/
static dynvar_set_status_t CL_MouseFilterBufferDecaySet_f( void *decay )
{
	static const float MIN_DECAY = 0.0;
	static const float MAX_DECAY = 1.0;
	const float desiredDecay = atof( decay );
	if( desiredDecay >= MIN_DECAY && desiredDecay <= MAX_DECAY )
	{
		if( m_filter->integer != M_FILTER_EXTRAPOLATE )
			Com_Printf( "Warning: \"m_filterBufferDecay\" has no effect unless \"m_filter " M_DOUBLEQUOTE( M_FILTER_EXTRAPOLATE ) "\" is set.\n" );
		buf_decay = desiredDecay;
		return DYNVAR_SET_OK;
	}
	else
	{
		Com_Printf( "\"m_filterBufferDecay\" must be between \"%f\" and \"%f\".\n", MIN_DECAY, MAX_DECAY );
		return DYNVAR_SET_INVALID;
	}
}

/*
* CL_MouseExtrapolate
* asymptotic extrapolation function
*/
static void CL_MouseExtrapolate( int mx, int my, float *extra_x, float *extra_y )
{

	static unsigned int frameNo = 0;
	static float sub_x = 0, sub_y = 0;
	static int64_t lastMicros = 0;
	static int64_t avgMicros = 0;

	float add_x = 0.0, add_y = 0.0;
	float decay = 1.0;
	float decaySum = buf_size > 1 ? 0.0 : decay;
	unsigned int i;

	int64_t micros;
	if( !lastMicros )
		lastMicros = Sys_Microseconds() - 10000;    // start at 100 FPS
	micros = Sys_Microseconds();                        // get current time in us
	avgMicros = ( avgMicros + ( micros - lastMicros ) ) / 2; // calc running avg of us per frame

	assert( buf_size );
	frameNo = ( frameNo + 1 ) % buf_size;   // we use the buffer in a cyclic fashion

	// normalize mouse movement to pixels per microsecond
	buf_x[frameNo] = mx / (float) avgMicros;
	buf_y[frameNo] = my / (float) avgMicros;

	// calculate asymptotically weighted sum of movement over the last few frames
	assert( buf_decay >= 0.0 );
	for( i = 0; i < buf_size; ++i )
	{
		const unsigned int f = ( frameNo-i ) % buf_size;
		assert( f <= buf_size );
		add_x += buf_x[f] * decay;
		add_y += buf_y[f] * decay;
		decaySum += decay;
		decay *= buf_decay;
	}
	assert( decaySum >= 1.0 );
	add_x /= decaySum;
	add_y /= decaySum;

	// calculate difference to last frame and re-weigh it with avg us per frame
	// we need to extrapolate the delta, not the momentum alone, so the mouse will come to
	// rest at the same spot ultimately, regardless of extrapolation on or off
	*extra_x = ( add_x - sub_x ) * avgMicros;
	*extra_y = ( add_y - sub_y ) * avgMicros;

	sub_x = add_x;
	sub_y = add_y;
	lastMicros = micros;
}

/*
* CL_MouseMove
*/
void CL_MouseMove( usercmd_t *cmd, int mx, int my )
{
	static unsigned int mouse_time = 0, old_mouse_time = 0xFFFFFFFF;
	static float mouse_x = 0, mouse_y = 0;
	static float old_mouse_x = 0, old_mouse_y = 0;
	float accelSensitivity;
	float	rate;
	float	accelOffset, accelPow;

	old_mouse_time = mouse_time;
	mouse_time = Sys_Milliseconds();
	if( old_mouse_time >= mouse_time )
		old_mouse_time = mouse_time - 1;

	mouse_frame_time = mouse_time - old_mouse_time;

	if( cls.key_dest == key_menu )
	{
		CL_UIModule_MouseMove( mx, my );
		return;
	}

	if( ( cls.key_dest == key_console ) && !in_grabinconsole->integer )
		return;

	if( cls.state < CA_ACTIVE )
		return;

	// mouse filtering
	switch( m_filter->integer )
	{
	case M_FILTER_INTERPOLATE:
		{
			mouse_x = ( mx + old_mouse_x ) * 0.5;
			mouse_y = ( my + old_mouse_y ) * 0.5;
		}
		break;

	case M_FILTER_EXTRAPOLATE:
		{
			float extra_x, extra_y;
			CL_MouseExtrapolate( mx, my, &extra_x, &extra_y );
			mouse_x = mx + extra_x * m_filterStrength->value;
			mouse_y = my + extra_y * m_filterStrength->value;
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

	if( m_accel->value != 0.0f && mouse_frame_time != 0.0f )
	{
		// QuakeLive-style mouse acceleration, ported from ioquake3
		// original patch by Gabriel Schnoering and TTimo
		if( m_accelStyle->integer == 1 )
		{
			float rate[2];
			float power[2];

			// sensitivity remains pretty much unchanged at low speeds
			// m_accel is a power value to how the acceleration is shaped
			// m_accelOffset is the rate for which the acceleration will have doubled the non accelerated amplification
			// NOTE: decouple the config cvars for independent acceleration setup along X and Y?

			rate[0] = (float) (abs(mx)) / (float) mouse_frame_time;
			rate[1] = (float) (abs(my)) / (float) mouse_frame_time;
			power[0] = powf(rate[0] / m_accelOffset->value, m_accel->value);
			power[1] = powf(rate[1] / m_accelOffset->value, m_accel->value);

			mouse_x = (mouse_x + ((mouse_x < 0) ? -power[0] : power[0]) * m_accelOffset->value);
			mouse_y = (mouse_y + ((mouse_y < 0) ? -power[1] : power[1]) * m_accelOffset->value);
		}
		else if( m_accelStyle->integer == 2 )
		{
			// ch : similar to normal acceleration with offset and variable pow mechanisms

			// sanitize values
			accelPow = m_accelPow->value > 1.0 ? m_accelPow->value : 2.0;
			accelOffset = m_accelOffset->value >= 0.0 ? m_accelOffset->value : 0.0;

			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)mouse_frame_time;
			rate -= accelOffset;
			if( rate < 0 )
				rate = 0.0;
			// ch : TODO sens += pow( rate * m_accel->value, m_accelPow->value - 1.0 )
			accelSensitivity += pow( rate * m_accel->value, accelPow - 1.0 );

			// TODO : move this outside of this branch?
			if( m_sensCap->value > 0 && accelSensitivity > m_sensCap->value )
				accelSensitivity = m_sensCap->value;
		}
		else
		{
			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / (float)mouse_frame_time;
			accelSensitivity += rate * m_accel->value;
		}
	}

	accelSensitivity *= CL_GameModule_GetSensitivityScale( sensitivity->value, zoomsens->value );

	mouse_x *= accelSensitivity;
	mouse_y *= accelSensitivity;

	if( !mouse_x && !mouse_y )
		return;

	// add mouse X/Y movement to cmd
	cl.viewangles[YAW] -= ( m_yaw->value * mouse_x ) * (cl_flip->integer ? -1.0 : 1.0);
	cl.viewangles[PITCH] += ( m_pitch->value * mouse_y );
}

/*
* CL_MouseSet
*
* Mouse input for systems with basic mouse support (without centering
* and possibly without toggleable cursor).
*/
void CL_MouseSet( int mx, int my, bool showCursor )
{
	if( cls.key_dest == key_menu )
		CL_UIModule_MouseSet( mx, my, showCursor );
}

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


Key_Event (int key, bool down, unsigned time);

===============================================================================
*/

kbutton_t in_klook;
kbutton_t in_left, in_right, in_forward, in_back;
kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_strafe, in_speed, in_use, in_attack;
kbutton_t in_up, in_down;
kbutton_t in_special;
kbutton_t in_zoom;

/*
* KeyDown
*/
static void KeyDown( kbutton_t *b )
{
	int k;
	char *c;

	c = Cmd_Argv( 1 );
	if( c[0] )
		k = atoi( c );
	else
		k = -1; // typed manually at the console for continuous down

	if( k == b->down[0] || k == b->down[1] )
		return; // repeating key

	if( !b->down[0] )
		b->down[0] = k;
	else if( !b->down[1] )
		b->down[1] = k;
	else
	{
		Com_Printf( "Three keys down for a button!\n" );
		return;
	}

	if( b->state & 1 )
		return; // still down

	// save timestamp
	c = Cmd_Argv( 2 );
	b->downtime = atoi( c );
	if( !b->downtime )
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2; // down + impulse down
}

/*
* KeyUp
*/
static void KeyUp( kbutton_t *b )
{
	int k;
	char *c;
	unsigned uptime;

	c = Cmd_Argv( 1 );
	if( c[0] )
		k = atoi( c );
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4; // impulse up
		return;
	}

	if( b->down[0] == k )
		b->down[0] = 0;
	else if( b->down[1] == k )
		b->down[1] = 0;
	else
		return; // key up without corresponding down (menu pass through)
	if( b->down[0] || b->down[1] )
		return; // some other key is still holding it down

	if( !( b->state & 1 ) )
		return; // still up (this should not happen)

	// save timestamp
	c = Cmd_Argv( 2 );
	uptime = atoi( c );
	if( uptime )
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1; // now up
	b->state |= 4;  // impulse up
}

static void IN_KLookDown( void ) { KeyDown( &in_klook ); }
static void IN_KLookUp( void ) { KeyUp( &in_klook ); }
static void IN_UpDown( void ) { KeyDown( &in_up ); }
static void IN_UpUp( void ) { KeyUp( &in_up ); }
static void IN_DownDown( void ) { KeyDown( &in_down ); }
static void IN_DownUp( void ) { KeyUp( &in_down ); }
static void IN_LeftDown( void ) { KeyDown( &in_left ); }
static void IN_LeftUp( void ) { KeyUp( &in_left ); }
static void IN_RightDown( void ) { KeyDown( &in_right ); }
static void IN_RightUp( void ) { KeyUp( &in_right ); }
static void IN_ForwardDown( void ) { KeyDown( &in_forward ); }
static void IN_ForwardUp( void ) { KeyUp( &in_forward ); }
static void IN_BackDown( void ) { KeyDown( &in_back ); }
static void IN_BackUp( void ) { KeyUp( &in_back ); }
static void IN_LookupDown( void ) { KeyDown( &in_lookup ); }
static void IN_LookupUp( void ) { KeyUp( &in_lookup ); }
static void IN_LookdownDown( void ) { KeyDown( &in_lookdown ); }
static void IN_LookdownUp( void ) { KeyUp( &in_lookdown ); }
static void IN_MoveleftDown( void ) { KeyDown( &in_moveleft ); }
static void IN_MoveleftUp( void ) { KeyUp( &in_moveleft ); }
static void IN_MoverightDown( void ) { KeyDown( &in_moveright ); }
static void IN_MoverightUp( void ) { KeyUp( &in_moveright ); }
static void IN_SpeedDown( void ) { KeyDown( &in_speed ); }
static void IN_SpeedUp( void ) { KeyUp( &in_speed ); }
static void IN_StrafeDown( void ) { KeyDown( &in_strafe ); }
static void IN_StrafeUp( void ) { KeyUp( &in_strafe ); }
static void IN_AttackDown( void ) { KeyDown( &in_attack ); }
static void IN_AttackUp( void ) { KeyUp( &in_attack ); }
static void IN_UseDown( void ) { KeyDown( &in_use ); }
static void IN_UseUp( void ) { KeyUp( &in_use ); }
static void IN_SpecialDown( void ) { KeyDown( &in_special ); }
static void IN_SpecialUp( void ) { KeyUp( &in_special ); }
static void IN_ZoomDown( void ) { KeyDown( &in_zoom ); }
static void IN_ZoomUp( void ) { KeyUp( &in_zoom ); }


/*
* CL_KeyState
*/
static float CL_KeyState( kbutton_t *key )
{
	float val;
	int msec;

	key->state &= 1; // clear impulses

	msec = key->msec;
	key->msec = 0;

	if( key->state )
	{
		// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

	val = (float) msec / (float)ucmd_frame_time;

	return bound( 0, val, 1 );
}

/*
===============================================================================

TOUCHSCREEN

===============================================================================
*/

/*
* CL_TouchEvent
*/
void CL_TouchEvent( int id, touchevent_t type, int x, int y, unsigned int time )
{
	switch( cls.key_dest )
	{
		case key_game:
			{
				bool toQuickMenu = false;

				if( SCR_IsQuickMenuShown() && !CL_GameModule_IsTouchDown( id ) )
				{
					if( CL_UIModule_IsTouchDownQuick( id ) )
						toQuickMenu = true;

					// if the quick menu has consumed the touch event, don't send the event to the game
					toQuickMenu |= CL_UIModule_TouchEventQuick( id, type, x, y );
				}

				if( !toQuickMenu )
					CL_GameModule_TouchEvent( id, type, x, y, time );
			}
			break;

		case key_console:
		case key_message:
			if( id == 0 )
				Con_TouchEvent( ( type != TOUCH_UP ) ? true : false, x, y );
			break;
		
		case key_menu:
			CL_UIModule_TouchEvent( id, type, x, y );
			break;

		default:
			break;
	}
}

//==========================================================================

cvar_t *cl_yawspeed;
cvar_t *cl_pitchspeed;
cvar_t *cl_run;
cvar_t *cl_anglespeedkey;

/*
* CL_AddButtonBits
*/
static void CL_AddButtonBits( uint8_t *buttons )
{
	// figure button bits

	if( in_attack.state & 3 )
		*buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;

	if( in_special.state & 3 )
		*buttons |= BUTTON_SPECIAL;
	in_special.state &= ~2;

	if( in_use.state & 3 )
		*buttons |= BUTTON_USE;
	in_use.state &= ~2;

	// we use this bit for a different flag, sorry!
	if( anykeydown && cls.key_dest == key_game )
		*buttons |= BUTTON_ANY;

	if( ( in_speed.state & 1 ) ^ !cl_run->integer )
		*buttons |= BUTTON_WALK;

	// add chat/console/ui icon as a button
	if( cls.key_dest != key_game )
		*buttons |= BUTTON_BUSYICON;

	if( in_zoom.state & 3 )
		*buttons |= BUTTON_ZOOM;
	in_zoom.state &= ~2;

	if( cls.key_dest == key_game )
		*buttons |= CL_GameModule_GetButtonBits();
}

/*
* CL_AddAnglesFromKeys
*/
static void CL_AddAnglesFromKeys( int frametime )
{
	float speed;

	if( in_speed.state & 1 )
		speed = ( (float)frametime * 0.001f ) * cl_anglespeedkey->value;
	else
		speed = (float)frametime * 0.001f;

	if( !( in_strafe.state & 1 ) )
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
	}
	if( in_klook.state & 1 )
	{
		cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_forward );
		cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_back );
	}

	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_lookup );
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_lookdown );
}

/*
* CL_AddMovementFromKeys
*/
static void CL_AddMovementFromKeys( vec3_t movement )
{
	if( in_strafe.state & 1 )
	{
		movement[0] += ( float )CL_KeyState( &in_right );
		movement[0] -= ( float )CL_KeyState( &in_left );
	}

	movement[0] += ( float )CL_KeyState( &in_moveright );
	movement[0] -= ( float )CL_KeyState( &in_moveleft );

	movement[2] += ( float )CL_KeyState( &in_up );
	movement[2] -= ( float )CL_KeyState( &in_down );

	if( !( in_klook.state & 1 ) )
	{
		movement[1] += ( float )CL_KeyState( &in_forward );
		movement[1] -= ( float )CL_KeyState( &in_back );
	}
}

/*
* CL_UpdateCommandInput
*/
void CL_UpdateCommandInput( void )
{
	static unsigned old_ucmd_frame_time;
	usercmd_t *cmd = &cl.cmds[cls.ucmdHead & CMD_MASK];

	if( cl.inputRefreshed )
		return;

	ucmd_frame_time = sys_frame_time - old_ucmd_frame_time;

	// always let the mouse refresh cl.viewangles
	IN_MouseMove( cmd );
	CL_AddButtonBits( &cmd->buttons );
	if( cls.key_dest == key_game )
		CL_GameModule_AddViewAngles( cl.viewangles, cls.realframetime, cl_flip->integer != 0 );

	if( ucmd_frame_time )
	{
		cmd->msec += ucmd_frame_time;
		CL_AddAnglesFromKeys( ucmd_frame_time );
		old_ucmd_frame_time = sys_frame_time;
	}

	if( cmd->msec )
	{
		vec3_t movement;

		VectorSet( movement, 0.0f, 0.0f, 0.0f );

		if( ucmd_frame_time )
			CL_AddMovementFromKeys( movement );
		if( cls.key_dest == key_game )
			CL_GameModule_AddMovement( movement );

		cmd->sidemove = bound( -1.0f, movement[0], 1.0f ) * ( cl_flip->integer ? -1.0f : 1.0f );
		cmd->forwardmove = bound( -1.0f, movement[1], 1.0f );
		cmd->upmove = bound( -1.0f, movement[2], 1.0f );
	}

	cmd->angles[0] = ANGLE2SHORT( cl.viewangles[0] );
	cmd->angles[1] = ANGLE2SHORT( cl.viewangles[1] );
	cmd->angles[2] = ANGLE2SHORT( cl.viewangles[2] );

	cl.inputRefreshed = true;
}

/*
* IN_CenterView
*/
void IN_CenterView( void )
{
	if( cl.currentSnapNum > 0 )
	{
		player_state_t *playerState;
		playerState = &cl.snapShots[cl.currentSnapNum & UPDATE_MASK].playerState;
		cl.viewangles[PITCH] = -SHORT2ANGLE( playerState->pmove.delta_angles[PITCH] );
	}
}

/*
* IN_ClearState
*/
void IN_ClearState( void )
{
	IN_ShowSoftKeyboard( false );

	Key_ClearStates();

	switch( cls.key_dest )
	{
		case key_game:
			CL_GameModule_ClearInputState();
			break;
		case key_console:
		case key_message:
			Con_TouchEvent( false, -1, -1 );
			break;
		case key_menu:
			CL_UIModule_CancelTouches();
		default:
			break;
	}
}

static bool in_initialized = false;

/*
* CL_InitInput
*/
void CL_InitInput( void )
{
	if( in_initialized )
		return;

	Cmd_AddCommand( "in_restart", IN_Restart );

	IN_Init();

	Cmd_AddCommand( "centerview", IN_CenterView );
	Cmd_AddCommand( "+moveup", IN_UpDown );
	Cmd_AddCommand( "-moveup", IN_UpUp );
	Cmd_AddCommand( "+movedown", IN_DownDown );
	Cmd_AddCommand( "-movedown", IN_DownUp );
	Cmd_AddCommand( "+left", IN_LeftDown );
	Cmd_AddCommand( "-left", IN_LeftUp );
	Cmd_AddCommand( "+right", IN_RightDown );
	Cmd_AddCommand( "-right", IN_RightUp );
	Cmd_AddCommand( "+forward", IN_ForwardDown );
	Cmd_AddCommand( "-forward", IN_ForwardUp );
	Cmd_AddCommand( "+back", IN_BackDown );
	Cmd_AddCommand( "-back", IN_BackUp );
	Cmd_AddCommand( "+lookup", IN_LookupDown );
	Cmd_AddCommand( "-lookup", IN_LookupUp );
	Cmd_AddCommand( "+lookdown", IN_LookdownDown );
	Cmd_AddCommand( "-lookdown", IN_LookdownUp );
	Cmd_AddCommand( "+strafe", IN_StrafeDown );
	Cmd_AddCommand( "-strafe", IN_StrafeUp );
	Cmd_AddCommand( "+moveleft", IN_MoveleftDown );
	Cmd_AddCommand( "-moveleft", IN_MoveleftUp );
	Cmd_AddCommand( "+moveright", IN_MoverightDown );
	Cmd_AddCommand( "-moveright", IN_MoverightUp );
	Cmd_AddCommand( "+speed", IN_SpeedDown );
	Cmd_AddCommand( "-speed", IN_SpeedUp );
	Cmd_AddCommand( "+attack", IN_AttackDown );
	Cmd_AddCommand( "-attack", IN_AttackUp );
	Cmd_AddCommand( "+use", IN_UseDown );
	Cmd_AddCommand( "-use", IN_UseUp );
	Cmd_AddCommand( "+klook", IN_KLookDown );
	Cmd_AddCommand( "-klook", IN_KLookUp );
	// wsw
	Cmd_AddCommand( "+special", IN_SpecialDown );
	Cmd_AddCommand( "-special", IN_SpecialUp );
	Cmd_AddCommand( "+zoom", IN_ZoomDown );
	Cmd_AddCommand( "-zoom", IN_ZoomUp );

	cl_ucmdMaxResend =	Cvar_Get( "cl_ucmdMaxResend", "3", CVAR_ARCHIVE );
	cl_ucmdFPS =		Cvar_Get( "cl_ucmdFPS", "62", CVAR_DEVELOPER );

#ifdef UCMDTIMENUDGE
	cl_ucmdTimeNudge =	Cvar_Get( "cl_ucmdTimeNudge", "0", CVAR_USERINFO|CVAR_DEVELOPER );
	if( abs( cl_ucmdTimeNudge->integer ) > MAX_UCMD_TIMENUDGE )
	{
		if( cl_ucmdTimeNudge->integer < -MAX_UCMD_TIMENUDGE )
			Cvar_SetValue( "cl_ucmdTimeNudge", -MAX_UCMD_TIMENUDGE );
		else if( cl_ucmdTimeNudge->integer > MAX_UCMD_TIMENUDGE )
			Cvar_SetValue( "cl_ucmdTimeNudge", MAX_UCMD_TIMENUDGE );
	}
#endif

	in_initialized = true;
}

/*
* CL_InitInputDynvars
*/
void CL_InitInputDynvars( void )
{
	Dynvar_Create( "m_filterBufferSize", true, CL_MouseFilterBufferSizeGet_f, CL_MouseFilterBufferSizeSet_f );
	Dynvar_Create( "m_filterBufferDecay", true, CL_MouseFilterBufferDecayGet_f, CL_MouseFilterBufferDecaySet_f );
	// we could simply call Dynvar_SetValue(m_filterBufferSize, "5") here, but then the user would get a warning in the console if m_filter was != M_FILTER_EXTRAPOLATE
	buf_size = DEFAULT_BUF_SIZE;
	buf_x = (float *) Mem_ZoneMalloc( sizeof( float ) * buf_size );
	buf_y = (float *) Mem_ZoneMalloc( sizeof( float ) * buf_size );
	memset( buf_x, 0, sizeof( float ) * buf_size );
	memset( buf_y, 0, sizeof( float ) * buf_size );
}

/*
* CL_ShutdownInput
*/
void CL_ShutdownInput( void )
{
	if( !in_initialized )
		return;

	Cmd_RemoveCommand( "in_restart" );

	IN_Shutdown();

	Cmd_RemoveCommand( "centerview" );
	Cmd_RemoveCommand( "+moveup" );
	Cmd_RemoveCommand( "-moveup" );
	Cmd_RemoveCommand( "+movedown" );
	Cmd_RemoveCommand( "-movedown" );
	Cmd_RemoveCommand( "+left" );
	Cmd_RemoveCommand( "-left" );
	Cmd_RemoveCommand( "+right" );
	Cmd_RemoveCommand( "-right" );
	Cmd_RemoveCommand( "+forward" );
	Cmd_RemoveCommand( "-forward" );
	Cmd_RemoveCommand( "+back" );
	Cmd_RemoveCommand( "-back" );
	Cmd_RemoveCommand( "+lookup" );
	Cmd_RemoveCommand( "-lookup" );
	Cmd_RemoveCommand( "+lookdown" );
	Cmd_RemoveCommand( "-lookdown" );
	Cmd_RemoveCommand( "+strafe" );
	Cmd_RemoveCommand( "-strafe" );
	Cmd_RemoveCommand( "+moveleft" );
	Cmd_RemoveCommand( "-moveleft" );
	Cmd_RemoveCommand( "+moveright" );
	Cmd_RemoveCommand( "-moveright" );
	Cmd_RemoveCommand( "+speed" );
	Cmd_RemoveCommand( "-speed" );
	Cmd_RemoveCommand( "+attack" );
	Cmd_RemoveCommand( "-attack" );
	Cmd_RemoveCommand( "+use" );
	Cmd_RemoveCommand( "-use" );
	Cmd_RemoveCommand( "+klook" );
	Cmd_RemoveCommand( "-klook" );
	// wsw
	Cmd_RemoveCommand( "+special" );
	Cmd_RemoveCommand( "-special" );
	Cmd_RemoveCommand( "+zoom" );
	Cmd_RemoveCommand( "-zoom" );
	Dynvar_Destroy( Dynvar_Lookup( "m_filterBufferDecay" ) );
	Dynvar_Destroy( Dynvar_Lookup( "m_filterBufferSize" ) );
	Mem_ZoneFree( buf_x );
	Mem_ZoneFree( buf_y );

	in_initialized = true;
}

//===============================================================================
//
//	UCMDS
//
//===============================================================================

/*
* CL_UserInputFrame
*/
void CL_UserInputFrame( void )
{
	// let the mouse activate or deactivate
	IN_Frame();

	// get new key events
	Sys_SendKeyEvents();

	// get new key events from mice or external controllers
	IN_Commands();

	// process console commands
	Cbuf_Execute();
}

/*
* CL_WriteUcmdsToMessage
*/
void CL_WriteUcmdsToMessage( msg_t *msg )
{
	usercmd_t *cmd;
	usercmd_t *oldcmd;
	usercmd_t nullcmd;
	unsigned int resendCount;
	unsigned int i;
	unsigned int ucmdFirst;
	unsigned int ucmdHead;

	if( !msg || cls.state < CA_ACTIVE || cls.demo.playing )
		return;

	// find out what ucmds we have to send
	ucmdFirst = cls.ucmdAcknowledged + 1;
	ucmdHead = cl.cmdNum + 1;

	if( cl_ucmdMaxResend->integer > CMD_BACKUP * 0.5 )
		Cvar_SetValue( "cl_ucmdMaxResend", CMD_BACKUP * 0.5 );
	else if( cl_ucmdMaxResend->integer < 1 )
		Cvar_SetValue( "cl_ucmdMaxResend", 1 );

	// find what is our resend count (resend doesn't include the newly generated ucmds)
	// and move the start back to the resend start
	if( ucmdFirst <= cls.ucmdSent + 1 )
		resendCount = 0;
	else
		resendCount = ( cls.ucmdSent + 1 ) - ucmdFirst;
	if( resendCount > (unsigned int)cl_ucmdMaxResend->integer )
		resendCount = (unsigned int)cl_ucmdMaxResend->integer;

	if( ucmdFirst > ucmdHead )
		ucmdFirst = ucmdHead;

	// if this happens, the player is in a freezing lag. Send him the less possible data
	if( ( ucmdHead - ucmdFirst ) + resendCount > CMD_MASK * 0.5 )
		resendCount = 0;

	// move the start backwards to the resend point
	ucmdFirst = ( ucmdFirst > resendCount ) ? ucmdFirst - resendCount : ucmdFirst;

	if( ( ucmdHead - ucmdFirst ) > CMD_MASK ) // ran out of updates, seduce the send to try to recover activity
		ucmdFirst = ucmdHead - 3;

	// begin a client move command
	MSG_WriteByte( msg, clc_move );

	// (acknowledge server frame snap)
	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if( cl.receivedSnapNum <= 0 )
		MSG_WriteLong( msg, -1 );
	else
		MSG_WriteLong( msg, cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverFrame );

	// Write the actual ucmds

	// write the id number of first ucmd to be sent, and the count
	MSG_WriteLong( msg, ucmdHead );
	MSG_WriteByte( msg, (uint8_t)( ucmdHead - ucmdFirst ) );

	// write the ucmds
	for( i = ucmdFirst; i < ucmdHead; i++ )
	{
		if( i == ucmdFirst ) // first one isn't delta-compressed
		{
			cmd = &cl.cmds[i & CMD_MASK];
			memset( &nullcmd, 0, sizeof( nullcmd ) );
			MSG_WriteDeltaUsercmd( msg, &nullcmd, cmd );
		}
		else // delta compress to previous written
		{
			cmd = &cl.cmds[i & CMD_MASK];
			oldcmd = &cl.cmds[( i-1 ) & CMD_MASK];
			MSG_WriteDeltaUsercmd( msg, oldcmd, cmd );
		}
	}

	cls.ucmdSent = i;
}

/*
* CL_NextUserCommandTimeReached
*/
static bool CL_NextUserCommandTimeReached( int realmsec )
{
	static int minMsec = 1, allMsec = 0, extraMsec = 0;
	static float roundingMsec = 0.0f;
	float maxucmds;

	if( cls.state < CA_ACTIVE )
		maxucmds = 10; // reduce ratio while connecting
	else
		maxucmds = cl_ucmdFPS->value;

	// the cvar is developer only
	//clamp( maxucmds, 10, 90 ); // don't let people abuse cl_ucmdFPS

	if( !cl_timedemo->integer && !cls.demo.playing )
	{
		minMsec = ( 1000.0f / maxucmds );
		roundingMsec += ( 1000.0f / maxucmds ) - minMsec;
	}
	else
		minMsec = 1;

	if( roundingMsec >= 1.0f )
	{
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( minMsec > extraMsec )  // remove, from min frametime, the extra time we spent in last frame
		minMsec -= extraMsec;

	allMsec += realmsec;
	if( allMsec < minMsec )
	{
		//if( !cls.netchan.unsentFragments ) {
		//	NET_Sleep( minMsec - allMsec );
		return false;
	}

	extraMsec = allMsec - minMsec;
	if( extraMsec > minMsec )
		extraMsec = minMsec - 1;

	allMsec = 0;

	// send a new user command message to the server
	return true;
}

/*
* CL_NewUserCommand
*/
void CL_NewUserCommand( int realmsec )
{
	usercmd_t *ucmd;

	if( !CL_NextUserCommandTimeReached( realmsec ) )
		return;

	if( cls.state < CA_ACTIVE )
		return;

	cl.cmdNum = cls.ucmdHead;
	ucmd = &cl.cmds[cl.cmdNum & CMD_MASK];
	ucmd->serverTimeStamp = cl.serverTime; // return the time stamp to the server
	cl.cmd_time[cl.cmdNum & CMD_MASK] = cls.realtime;

	// snap push fracs so client and server version match
	ucmd->forwardmove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->forwardmove ) ) / UCMD_PUSHFRAC_SNAPSIZE;
	ucmd->sidemove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->sidemove ) ) / UCMD_PUSHFRAC_SNAPSIZE;
	ucmd->upmove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->upmove ) ) / UCMD_PUSHFRAC_SNAPSIZE;

	if( cl.cmdNum > 0 )
		ucmd->msec = ucmd->serverTimeStamp - cl.cmds[( cl.cmdNum-1 ) & CMD_MASK].serverTimeStamp;
	else
		ucmd->msec = 20;

	if( ucmd->msec < 1 )
		ucmd->msec = 1;

	// advance head and init the new command
	cls.ucmdHead++;
	ucmd = &cl.cmds[cls.ucmdHead & CMD_MASK];
	memset( ucmd, 0, sizeof( usercmd_t ) );

	// start up with the most recent viewangles
	ucmd->angles[0] = ANGLE2SHORT( cl.viewangles[0] );
	ucmd->angles[1] = ANGLE2SHORT( cl.viewangles[1] );
	ucmd->angles[2] = ANGLE2SHORT( cl.viewangles[2] );
}
