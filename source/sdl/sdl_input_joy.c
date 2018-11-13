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

#include "sdl/SDL.h"
#include "../client/client.h"
#include "sdl_input_joy.h"

static bool in_sdl_joyInitialized, in_sdl_joyActive;
static SDL_GameController *in_sdl_joyController;

/*
* IN_SDL_JoyInit
*
* SDL game controller code called in IN_Init.
*/
void IN_SDL_JoyInit( bool active ) {
	in_sdl_joyActive = active;

	if( SDL_InitSubSystem( SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER ) ) {
		return;
	}

	in_sdl_joyInitialized = true;
}

/*
* IN_SDL_JoyActivate
*/
void IN_SDL_JoyActivate( bool active ) {
	in_sdl_joyActive = active;
}

/*
* IN_SDL_JoyCommands
*
* SDL game controller code called in IN_Commands.
*/
void IN_SDL_JoyCommands( void ) {
	int i, buttons = 0, buttonsDiff;
	static int buttonsOld;
	const int keys[] =
	{
		K_A_BUTTON, K_B_BUTTON, K_X_BUTTON, K_Y_BUTTON, K_ESCAPE, 0, 0,
		K_LSTICK, K_RSTICK, K_LSHOULDER, K_RSHOULDER,
		K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT,
		K_LTRIGGER, K_RTRIGGER
	};

	if( in_sdl_joyInitialized ) {
		SDL_GameControllerUpdate();

		if( in_sdl_joyController && !SDL_GameControllerGetAttached( in_sdl_joyController ) ) {
			SDL_GameControllerClose( in_sdl_joyController );
			in_sdl_joyController = NULL;
		}

		if( !in_sdl_joyController ) {
			int num = SDL_NumJoysticks();

			for( i = 0; i < num; i++ ) {
				in_sdl_joyController = SDL_GameControllerOpen( i );
				if( in_sdl_joyController ) {
					break;
				}
			}
		}
	}

	if( in_sdl_joyActive ) {
		SDL_GameController *controller = in_sdl_joyController;
		if( controller ) {
			for( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
				if( keys[i] && SDL_GameControllerGetButton( controller, i ) ) {
					buttons |= 1 << i;
				}
			}

			if( SDL_GameControllerGetButton( controller, SDL_CONTROLLER_BUTTON_START ) ) {
				buttons |= 1 << SDL_CONTROLLER_BUTTON_BACK;
			}
			if( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT ) > ( 30 * 128 ) ) {
				buttons |= 1 << SDL_CONTROLLER_BUTTON_MAX;
			}
			if( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT ) > ( 30 * 128 ) ) {
				buttons |= 1 << ( SDL_CONTROLLER_BUTTON_MAX + 1 );
			}
		}
	}

	buttonsDiff = buttons ^ buttonsOld;
	if( buttonsDiff ) {
		int64_t time = Sys_Milliseconds();

		for( i = 0; i < ( sizeof( keys ) / sizeof( keys[0] ) ); i++ ) {
			if( buttonsDiff & ( 1 << i ) ) {
				Key_Event( keys[i], ( buttons & ( 1 << i ) ) ? true : false, time );
			}
		}

		buttonsOld = buttons;
	}
}

/*
* IN_SDL_JoyThumbstickValue
*/
static float IN_SDL_JoyThumbstickValue( int value ) {
	return value * ( ( value >= 0 ) ? ( 1.0f / 32767.0f ) : ( 1.0f / 32768.0f ) );
}

/*
* IN_GetThumbsticks
*/
void IN_GetThumbsticks( vec4_t sticks ) {
	SDL_GameController *controller = in_sdl_joyController;

	if( !controller || !in_sdl_joyActive ) {
		Vector4Set( sticks, 0.0f, 0.0f, 0.0f, 0.0f );
		return;
	}

	sticks[0] = IN_SDL_JoyThumbstickValue( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTX ) );
	sticks[1] = IN_SDL_JoyThumbstickValue( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_LEFTY ) );
	sticks[2] = IN_SDL_JoyThumbstickValue( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_RIGHTX ) );
	sticks[3] = IN_SDL_JoyThumbstickValue( SDL_GameControllerGetAxis( controller, SDL_CONTROLLER_AXIS_RIGHTY ) );
}

/*
* IN_SDL_JoyShutdown
*
* SDL game controller code called in IN_Shutdown.
*/
void IN_SDL_JoyShutdown( void ) {
	if( !in_sdl_joyInitialized ) {
		return;
	}

	in_sdl_joyController = NULL;
	SDL_QuitSubSystem( SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER );
	in_sdl_joyInitialized = false;
}
