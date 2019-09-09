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

static bool in_initialized = false;
static int64_t sys_frame_time;

cvar_t *cl_ucmdMaxResend;

cvar_t *cl_ucmdFPS;
#ifdef UCMDTIMENUDGE
cvar_t *cl_ucmdTimeNudge;
#endif

static void CL_CreateNewUserCommand( int realMsec );

/*
* CL_MouseSet
*/
void CL_MouseSet( int mx, int my, bool showCursor ) {
	if( cls.key_dest == key_menu ) {
		CL_UIModule_MouseSet( true, mx, my, showCursor );
	} else if( cls.key_dest == key_game ) {
		CL_UIModule_MouseSet( false, mx, my, showCursor );
	}
}

/*
* CL_TouchEvent
*/
void CL_TouchEvent( int id, touchevent_t type, int x, int y, int64_t time ) {
	switch( cls.key_dest ) {
		case key_game:
		{
			bool toOverlayMenu = false;

			if( SCR_HaveOverlay() && !CL_GameModule_IsTouchDown( id ) ) {
				if( CL_UIModule_IsTouchDownQuick( id ) ) {
					toOverlayMenu = true;
				}

				// if the quick menu has consumed the touch event, don't send the event to the game
				toOverlayMenu |= CL_UIModule_TouchEventQuick( id, type, x, y );
			}

			if( !toOverlayMenu ) {
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
			CL_UIModule_TouchEvent( false, id, type, x, y );
			break;

		default:
			break;
	}
}

/*
* CL_ClearInputState
*/
void CL_ClearInputState( void ) {
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

/*
* CL_UpdateGameInput
*
* Notifies cgame of new frame, refreshes input timings, coordinates and angles
*/
static void CL_UpdateGameInput( int frameTime ) {
	int mx, my;

	IN_GetMouseMovement( &mx, &my );

	// refresh input in cgame
	CL_GameModule_InputFrame( sys_frame_time );

	if( cls.key_dest == key_menu ) {
		CL_UIModule_MouseMove( true, frameTime, mx, my );
	} else {
		CL_GameModule_MouseMove( mx, my );
	}

	if( cls.key_dest == key_game || ( ( cls.key_dest == key_console ) && Cvar_Value( "in_grabinconsole" ) != 0 ) ) {
		CL_GameModule_AddViewAngles( cl.viewangles );
	}
}

/*
* CL_UserInputFrame
*/
void CL_UserInputFrame( int realMsec ) {
	// let the mouse activate or deactivate
	IN_Frame();

	// get new key events
	Sys_SendKeyEvents();

	// grab frame time
	sys_frame_time = Sys_Milliseconds();

	// get new key events from mice or external controllers
	IN_Commands();

	// create a new usercmd_t structure for this frame
	CL_CreateNewUserCommand( realMsec );

	// process console commands
	Cbuf_Execute();
}

/*
* CL_InitInput
*/
void CL_InitInput( void ) {
	if( in_initialized ) {
		return;
	}

	Cmd_AddCommand( "in_restart", IN_Restart );

	IN_Init();

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

	in_initialized = true;
}

//===============================================================================
//
//	UCMDS
//
//===============================================================================

/*
* CL_SetUcmdMovement
*/
static void CL_SetUcmdMovement( usercmd_t *ucmd ) {
	vec3_t movement = { 0.0f, 0.0f, 0.0f };

	if( cls.key_dest == key_game ) {
		CL_GameModule_AddMovement( movement );
	}

	ucmd->sidemove = Q_bound( -127, (int)(movement[0] * 127.0f), 127 );
	ucmd->forwardmove = Q_bound( -127, (int)(movement[1] * 127.0f), 127 );
	ucmd->upmove = Q_bound( -127, (int)(movement[2] * 127.0f), 127 );
}

/*
* CL_SetUcmdButtons
*/
static void CL_SetUcmdButtons( usercmd_t *ucmd ) {
	if( cls.key_dest == key_game ) {
		ucmd->buttons |= CL_GameModule_GetButtonBits();
		if( anykeydown ) {
			ucmd->buttons |= BUTTON_ANY;
		}
		return;
	}

	// add chat/console/ui icon as a button
	ucmd->buttons |=  BUTTON_BUSYICON;
}

/*
* CL_RefreshUcmd
*
* Updates ucmd to use the most recent viewangles.
*/
static void CL_RefreshUcmd( usercmd_t *ucmd, int msec, bool ready ) {
	static int64_t old_ucmd_frame_time;
	int ucmd_frame_time = sys_frame_time - old_ucmd_frame_time;
	old_ucmd_frame_time = sys_frame_time;

	// refresh mouse angles and movement velocity
	if( ucmd_frame_time > 0 ) {
		ucmd->msec += ucmd_frame_time;

		CL_UpdateGameInput( ucmd_frame_time );

		CL_SetUcmdMovement( ucmd );

		CL_SetUcmdButtons( ucmd );
	}

	if( ready ) {
		ucmd->serverTimeStamp = cl.serverTime; // return the time stamp to the server

		if( cl.cmdNum > 0 ) {
			ucmd->msec = ucmd->serverTimeStamp - cl.cmds[( cl.cmdNum - 1 ) & CMD_MASK].serverTimeStamp;
		} else {
			ucmd->msec = 20;
		}
		if( ucmd->msec < 1 ) {
			ucmd->msec = 1;
		}
	}

	ucmd->angles[0] = ANGLE2SHORT( cl.viewangles[0] );
	ucmd->angles[1] = ANGLE2SHORT( cl.viewangles[1] );
	ucmd->angles[2] = ANGLE2SHORT( cl.viewangles[2] );
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
	MSG_WriteUint8( msg, clc_move );

	// (acknowledge server frame snap)
	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if( cl.receivedSnapNum <= 0 ) {
		MSG_WriteInt32( msg, -1 );
	} else {
		MSG_WriteInt32( msg, cl.snapShots[cl.receivedSnapNum & UPDATE_MASK].serverFrame );
	}

	// Write the actual ucmds

	// write the id number of first ucmd to be sent, and the count
	MSG_WriteInt32( msg, ucmdHead );
	MSG_WriteUint8( msg, (uint8_t)( ucmdHead - ucmdFirst ) );

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

	if( cls.state < CA_ACTIVE ) {
		return false;
	}

	// send a new user command message to the server
	return true;
}

/*
* CL_CreateNewUserCommand
*/
static void CL_CreateNewUserCommand( int realMsec ) {
	usercmd_t *ucmd;

	if( !CL_NextUserCommandTimeReached( realMsec ) ) {
		// refresh current command with up to date data for movement prediction
		CL_RefreshUcmd( &cl.cmds[cls.ucmdHead & CMD_MASK], realMsec, false );
		return;
	}

	cl.cmdNum = cls.ucmdHead;
	cl.cmd_time[cl.cmdNum & CMD_MASK] = cls.realtime;

	ucmd = &cl.cmds[cl.cmdNum & CMD_MASK];

	CL_RefreshUcmd( ucmd, realMsec, true );

	// advance head and init the new command
	cls.ucmdHead++;
	ucmd = &cl.cmds[cls.ucmdHead & CMD_MASK];
	memset( ucmd, 0, sizeof( usercmd_t ) );

	// start up with the most recent viewangles
	CL_RefreshUcmd( ucmd, 0, false );
}
