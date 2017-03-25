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

static int ucmdFrameTime;

/*
===============================================================================

MOUSE

===============================================================================
*/
extern cvar_t *in_grabinconsole;

/*
* CL_AddAnglesFromMouse
*/
static void CL_AddAnglesFromMouse( usercmd_t *cmd, int frameTime ) {
	int mx, my;

	// always let the mouse refresh cl.viewangles
	IN_GetMouseMovement( &mx, &my );

	if( cls.key_dest == key_menu ) {
		CL_UIModule_MouseMove( frameTime, mx, my );
		return;
	}

	if( ( cls.key_dest == key_console ) && !in_grabinconsole->integer ) {
		return;
	}

	if( cls.state < CA_ACTIVE ) {
		return;
	}

	CL_GameModule_MouseMove( frameTime, mx, my );
}

/*
* CL_MouseSet
*
* Mouse input for systems with basic mouse support (without centering
* and possibly without toggleable cursor).
*/
void CL_MouseSet( int mx, int my, bool showCursor ) {
	if( cls.key_dest == key_menu ) {
		CL_UIModule_MouseSet( mx, my, showCursor );
	}
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
static void KeyDown( kbutton_t *b ) {
	int k;
	char *c;

	c = Cmd_Argv( 1 );
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
	c = Cmd_Argv( 2 );
	b->downtime = atoi( c );
	if( !b->downtime ) {
		b->downtime = cls.realtime - 100;
	}

	b->state |= 1 + 2; // down + impulse down
}

/*
* KeyUp
*/
static void KeyUp( kbutton_t *b ) {
	int k;
	char *c;
	unsigned uptime;

	c = Cmd_Argv( 1 );
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
	c = Cmd_Argv( 2 );
	uptime = atoi( c );
	if( uptime ) {
		b->msec += uptime - b->downtime;
	} else {
		b->msec += 10;
	}

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
static float CL_KeyState( kbutton_t *key ) {
	float val;
	int msec;

	key->state &= 1; // clear impulses

	msec = key->msec;
	key->msec = 0;

	if( key->state ) {
		// still down
		msec += cls.realtime - key->downtime;
		key->downtime = cls.realtime;
	}

	val = (float) msec / (float)ucmdFrameTime;

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
void CL_TouchEvent( int id, touchevent_t type, int x, int y, unsigned int time ) {
	switch( cls.key_dest ) {
		case key_game:
		{
			bool toQuickMenu = false;

			if( SCR_IsQuickMenuShown() && !CL_GameModule_IsTouchDown( id ) ) {
				if( CL_UIModule_IsTouchDownQuick( id ) ) {
					toQuickMenu = true;
				}

				// if the quick menu has consumed the touch event, don't send the event to the game
				toQuickMenu |= CL_UIModule_TouchEventQuick( id, type, x, y );
			}

			if( !toQuickMenu ) {
				CL_GameModule_TouchEvent( id, type, x, y, time );
			}
		}
		break;

		case key_console:
		case key_message:
			if( id == 0 ) {
				Con_TouchEvent( ( type != TOUCH_UP ) ? true : false, x, y );
			}
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
static void CL_AddButtonBits( uint8_t *buttons ) {
	// figure button bits

	if( in_attack.state & 3 ) {
		*buttons |= BUTTON_ATTACK;
	}
	in_attack.state &= ~2;

	if( in_special.state & 3 ) {
		*buttons |= BUTTON_SPECIAL;
	}
	in_special.state &= ~2;

	if( in_use.state & 3 ) {
		*buttons |= BUTTON_USE;
	}
	in_use.state &= ~2;

	// we use this bit for a different flag, sorry!
	if( anykeydown && cls.key_dest == key_game ) {
		*buttons |= BUTTON_ANY;
	}

	if( ( in_speed.state & 1 ) ^ !cl_run->integer ) {
		*buttons |= BUTTON_WALK;
	}

	// add chat/console/ui icon as a button
	if( cls.key_dest != key_game ) {
		*buttons |= BUTTON_BUSYICON;
	}

	if( in_zoom.state & 3 ) {
		*buttons |= BUTTON_ZOOM;
	}
	in_zoom.state &= ~2;

	if( cls.key_dest == key_game ) {
		*buttons |= CL_GameModule_GetButtonBits();
	}
}

/*
* CL_AddAnglesFromKeys
*/
static void CL_AddAnglesFromKeys( int frametime ) {
	float speed;

	if( in_speed.state & 1 ) {
		speed = ( (float)frametime * 0.001f ) * cl_anglespeedkey->value;
	} else {
		speed = (float)frametime * 0.001f;
	}

	if( !( in_strafe.state & 1 ) ) {
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
	}
	if( in_klook.state & 1 ) {
		cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_forward );
		cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_back );
	}

	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_lookup );
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_lookdown );
}

/*
* CL_AddMovementFromKeys
*/
static void CL_AddMovementFromKeys( vec3_t movement ) {
	float down;

	if( in_strafe.state & 1 ) {
		movement[0] += CL_KeyState( &in_right );
		movement[0] -= CL_KeyState( &in_left );
	}

	movement[0] += CL_KeyState( &in_moveright );
	movement[0] -= CL_KeyState( &in_moveleft );

	if( !( in_klook.state & 1 ) ) {
		movement[1] += CL_KeyState( &in_forward );
		movement[1] -= CL_KeyState( &in_back );
	}

	movement[2] += CL_KeyState( &in_up );
	down = CL_KeyState( &in_down );
	if( down > movement[2] ) {
		movement[2] -= down;
	}
}

/*
* CL_UpdateCommandInput
*/
static void CL_UpdateCommandInput( int frameTime ) {
	usercmd_t *cmd = &cl.cmds[cls.ucmdHead & CMD_MASK];

	ucmdFrameTime = frameTime;

	CL_AddAnglesFromMouse( cmd, frameTime );

	CL_AddButtonBits( &cmd->buttons );

	if( cls.key_dest == key_game ) {
		CL_GameModule_AddViewAngles( cl.viewangles, cls.realFrameTime * 0.001f, cl_flip->integer != 0 );
	}

	if( frameTime ) {
		cmd->msec += frameTime;
		CL_AddAnglesFromKeys( frameTime );
	}

	if( cmd->msec ) {
		vec3_t movement;

		VectorSet( movement, 0.0f, 0.0f, 0.0f );

		if( frameTime ) {
			CL_AddMovementFromKeys( movement );
		}
		if( cls.key_dest == key_game ) {
			CL_GameModule_AddMovement( movement );
		}

		cmd->sidemove = bound( -1.0f, movement[0], 1.0f ) * ( cl_flip->integer ? -1.0f : 1.0f );
		cmd->forwardmove = bound( -1.0f, movement[1], 1.0f );
		cmd->upmove = bound( -1.0f, movement[2], 1.0f );
	}

	cmd->angles[0] = ANGLE2SHORT( cl.viewangles[0] );
	cmd->angles[1] = ANGLE2SHORT( cl.viewangles[1] );
	cmd->angles[2] = ANGLE2SHORT( cl.viewangles[2] );
}

/*
* IN_CenterView
*/
void IN_CenterView( void ) {
	if( cl.currentSnapNum > 0 ) {
		player_state_t *playerState;
		playerState = &cl.snapShots[cl.currentSnapNum & UPDATE_MASK].playerState;
		cl.viewangles[PITCH] = -SHORT2ANGLE( playerState->pmove.delta_angles[PITCH] );
	}
}

/*
* IN_ClearState
*/
void IN_ClearState( void ) {
	IN_ShowSoftKeyboard( false );

	Key_ClearStates();

	switch( cls.key_dest ) {
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
void CL_InitInput( void ) {
	if( in_initialized ) {
		return;
	}

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

	cl_ucmdMaxResend =  Cvar_Get( "cl_ucmdMaxResend", "3", CVAR_ARCHIVE );
	cl_ucmdFPS =        Cvar_Get( "cl_ucmdFPS", "62", CVAR_DEVELOPER );

#ifdef UCMDTIMENUDGE
	cl_ucmdTimeNudge =  Cvar_Get( "cl_ucmdTimeNudge", "0", CVAR_USERINFO | CVAR_DEVELOPER );
	if( abs( cl_ucmdTimeNudge->integer ) > MAX_UCMD_TIMENUDGE ) {
		if( cl_ucmdTimeNudge->integer < -MAX_UCMD_TIMENUDGE ) {
			Cvar_SetValue( "cl_ucmdTimeNudge", -MAX_UCMD_TIMENUDGE );
		} else if( cl_ucmdTimeNudge->integer > MAX_UCMD_TIMENUDGE ) {
			Cvar_SetValue( "cl_ucmdTimeNudge", MAX_UCMD_TIMENUDGE );
		}
	}
#endif

	in_initialized = true;
}

/*
* CL_ShutdownInput
*/
void CL_ShutdownInput( void ) {
	if( !in_initialized ) {
		return;
	}

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
void CL_UserInputFrame( void ) {
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
void CL_WriteUcmdsToMessage( msg_t *msg ) {
	usercmd_t *cmd;
	usercmd_t *oldcmd;
	usercmd_t nullcmd;
	unsigned int resendCount;
	unsigned int i;
	unsigned int ucmdFirst;
	unsigned int ucmdHead;

	if( !msg || cls.state < CA_ACTIVE || cls.demo.playing ) {
		return;
	}

	// find out what ucmds we have to send
	ucmdFirst = cls.ucmdAcknowledged + 1;
	ucmdHead = cl.cmdNum + 1;

	if( cl_ucmdMaxResend->integer > CMD_BACKUP * 0.5 ) {
		Cvar_SetValue( "cl_ucmdMaxResend", CMD_BACKUP * 0.5 );
	} else if( cl_ucmdMaxResend->integer < 1 ) {
		Cvar_SetValue( "cl_ucmdMaxResend", 1 );
	}

	// find what is our resend count (resend doesn't include the newly generated ucmds)
	// and move the start back to the resend start
	if( ucmdFirst <= cls.ucmdSent + 1 ) {
		resendCount = 0;
	} else {
		resendCount = ( cls.ucmdSent + 1 ) - ucmdFirst;
	}
	if( resendCount > (unsigned int)cl_ucmdMaxResend->integer ) {
		resendCount = (unsigned int)cl_ucmdMaxResend->integer;
	}

	if( ucmdFirst > ucmdHead ) {
		ucmdFirst = ucmdHead;
	}

	// if this happens, the player is in a freezing lag. Send him the less possible data
	if( ( ucmdHead - ucmdFirst ) + resendCount > CMD_MASK * 0.5 ) {
		resendCount = 0;
	}

	// move the start backwards to the resend point
	ucmdFirst = ( ucmdFirst > resendCount ) ? ucmdFirst - resendCount : ucmdFirst;

	if( ( ucmdHead - ucmdFirst ) > CMD_MASK ) { // ran out of updates, seduce the send to try to recover activity
		ucmdFirst = ucmdHead - 3;
	}

	// begin a client move command
	MSG_WriteByte( msg, clc_move );

	// (acknowledge server frame snap)
	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if( cl.receivedSnapNum <= 0 ) {
		MSG_WriteLong( msg, -1 );
	} else {
		MSG_WriteLong( msg, cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverFrame );
	}

	// Write the actual ucmds

	// write the id number of first ucmd to be sent, and the count
	MSG_WriteLong( msg, ucmdHead );
	MSG_WriteByte( msg, (uint8_t)( ucmdHead - ucmdFirst ) );

	// write the ucmds
	for( i = ucmdFirst; i < ucmdHead; i++ ) {
		if( i == ucmdFirst ) { // first one isn't delta-compressed
			cmd = &cl.cmds[i & CMD_MASK];
			memset( &nullcmd, 0, sizeof( nullcmd ) );
			MSG_WriteDeltaUsercmd( msg, &nullcmd, cmd );
		} else {   // delta compress to previous written
			cmd = &cl.cmds[i & CMD_MASK];
			oldcmd = &cl.cmds[( i - 1 ) & CMD_MASK];
			MSG_WriteDeltaUsercmd( msg, oldcmd, cmd );
		}
	}

	cls.ucmdSent = i;
}

/*
* CL_NextUserCommandTimeReached
*/
static bool CL_NextUserCommandTimeReached( int realMsec ) {
	static int minMsec = 1, allMsec = 0, extraMsec = 0;
	static float roundingMsec = 0.0f;
	float maxucmds;

	if( cls.state < CA_ACTIVE ) {
		maxucmds = 10; // reduce ratio while connecting
	} else {
		maxucmds = cl_ucmdFPS->value;
	}

	// the cvar is developer only
	//clamp( maxucmds, 10, 90 ); // don't let people abuse cl_ucmdFPS

	if( cls.demo.playing ) {
		minMsec = 1;
	} else {
		minMsec = ( 1000.0f / maxucmds );
		roundingMsec += ( 1000.0f / maxucmds ) - minMsec;
	}

	if( roundingMsec >= 1.0f ) {
		minMsec += (int)roundingMsec;
		roundingMsec -= (int)roundingMsec;
	}

	if( minMsec > extraMsec ) { // remove, from min frametime, the extra time we spent in last frame
		minMsec -= extraMsec;
	}

	allMsec += realMsec;
	if( allMsec < minMsec ) {
		//if( !cls.netchan.unsentFragments ) {
		//	NET_Sleep( minMsec - allMsec );
		return false;
	}

	extraMsec = allMsec - minMsec;
	if( extraMsec > minMsec ) {
		extraMsec = minMsec - 1;
	}

	allMsec = 0;

	// send a new user command message to the server
	return true;
}

/*
* CL_NewUserCommand
*/
void CL_NewUserCommand( int realMsec ) {
	usercmd_t *ucmd;

	CL_UpdateCommandInput( realMsec );

	if( !CL_NextUserCommandTimeReached( realMsec ) ) {
		return;
	}

	if( cls.state < CA_ACTIVE ) {
		return;
	}

	cl.cmdNum = cls.ucmdHead;
	ucmd = &cl.cmds[cl.cmdNum & CMD_MASK];
	ucmd->serverTimeStamp = cl.serverTime; // return the time stamp to the server
	cl.cmd_time[cl.cmdNum & CMD_MASK] = cls.realtime;

	// snap push fracs so client and server version match
	ucmd->forwardmove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->forwardmove ) ) / UCMD_PUSHFRAC_SNAPSIZE;
	ucmd->sidemove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->sidemove ) ) / UCMD_PUSHFRAC_SNAPSIZE;
	ucmd->upmove = ( (int)( UCMD_PUSHFRAC_SNAPSIZE * ucmd->upmove ) ) / UCMD_PUSHFRAC_SNAPSIZE;

	if( cl.cmdNum > 0 ) {
		ucmd->msec = ucmd->serverTimeStamp - cl.cmds[( cl.cmdNum - 1 ) & CMD_MASK].serverTimeStamp;
	} else {
		ucmd->msec = 20;
	}

	if( ucmd->msec < 1 ) {
		ucmd->msec = 1;
	}

	// advance head and init the new command
	cls.ucmdHead++;
	ucmd = &cl.cmds[cls.ucmdHead & CMD_MASK];
	memset( ucmd, 0, sizeof( usercmd_t ) );

	// start up with the most recent viewangles
	ucmd->angles[0] = ANGLE2SHORT( cl.viewangles[0] );
	ucmd->angles[1] = ANGLE2SHORT( cl.viewangles[1] );
	ucmd->angles[2] = ANGLE2SHORT( cl.viewangles[2] );
}
