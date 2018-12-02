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
	CG_asInputMouseMove( mx, my );
}

/*
* CG_KeyEvent
*/
bool CG_KeyEvent( int key, bool down ) {
	return CG_asInputKeyEvent( key, down );
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
	CG_asInputInit();
}

/*
* CG_ShutdownInput
*/
void CG_ShutdownInput( void ) {
	CG_asInputShutdown();
}

/*
* CG_GetButtonBits
*/
unsigned int CG_GetButtonBits( void ) {
	int buttons = 0;

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
	CG_GetAngularMovement( am );
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
	CG_GetMovement( dm );
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
	CG_asGetMovement( movement );
}

/*
* CG_InputFrame
*/
void CG_InputFrame( int frameTime ) {
	CG_asInputFrame( frameTime );
}

/*
* CG_ClearInputState
*/
void CG_ClearInputState( void ) {
	CG_asInputClearState();
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

		if( key >= 'a' && key <= 'z' ) {
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
		keyNames[0] = "UNBOUND";
	}

	if( numKeys == 2 ) {
		Q_snprintfz( keys, keysSize, "%s or %s", keyNames[0], keyNames[1] );
	} else {
		Q_strncpyz( keys, keyNames[0], keysSize );
	}
}
