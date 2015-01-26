#include <SDL.h>
#include "../client/client.h"

cvar_t *in_grabinconsole;

static bool input_inited = false;
static bool mouse_active = false;
static bool input_active = false;

cvar_t *in_disablemacosxmouseaccel;

static int mx, my;

#if defined(__APPLE__)
void IN_SetMouseScalingEnabled (bool isRestore);
#else
void IN_SetMouseScalingEnabled (bool isRestore) {}
#endif


void IN_Commands( void )
{
}
void IN_Activate( bool active )
{
}


/**
 * Function which is called whenever the mouse is moved.
 * @param ev the SDL event object containing the mouse position et all
 */
static void mouse_motion_event( SDL_MouseMotionEvent *event )
{
	mx += event->xrel;
	my += event->yrel;
}


/**
 * Function which is called whenever a mouse button is pressed or released.
 * @param ev the SDL event object containing the button number et all
 * @param state either true if it is a keydown event or false otherwise
 */
static void mouse_button_event( SDL_MouseButtonEvent *event, bool state )
{
	Uint8 button = event->button;
	if( button <= 5 )
	{
		switch( button )
		{
		case SDL_BUTTON_LEFT: Key_MouseEvent( K_MOUSE1, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_MIDDLE: Key_MouseEvent( K_MOUSE3, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_RIGHT: Key_MouseEvent( K_MOUSE2, state, Sys_Milliseconds() ); break;
		}
	}
	else if( button <= 10 )
	{
		// The engine only supports up to 8 buttons plus the mousewheel.
		Key_MouseEvent( K_MOUSE1 + button - 3, state, Sys_Milliseconds() );
	}
	else
		Com_Printf( "sdl_input.c: Unsupported mouse button (button = %u)\n", button );
}

static void mouse_wheel_event( SDL_MouseWheelEvent *event )
{
	int key = event->y > 0 ? K_MWHEELUP : K_MWHEELDOWN;
	unsigned sys_msg_time = Sys_Milliseconds();
	
	Key_Event( key, true, sys_msg_time );
	Key_Event( key, false, sys_msg_time );
}

static qwchar TranslateSDLScancode(SDL_Scancode scancode)
{
	qwchar charkey;
	
	switch(scancode)
	{
		case SDL_SCANCODE_TAB:          charkey = K_TAB;		break;
		case SDL_SCANCODE_RETURN:       charkey = K_ENTER;		break;
		case SDL_SCANCODE_ESCAPE:       charkey = K_ESCAPE;		break;
		case SDL_SCANCODE_SPACE:        charkey = K_SPACE;		break;
		case SDL_SCANCODE_CAPSLOCK:		charkey = K_CAPSLOCK;	break;
		case SDL_SCANCODE_SCROLLLOCK:   charkey = K_SCROLLLOCK; break;
		case SDL_SCANCODE_NUMLOCKCLEAR: charkey = K_NUMLOCK;    break;
		case SDL_SCANCODE_BACKSPACE:    charkey = K_BACKSPACE;  break;
		case SDL_SCANCODE_UP:           charkey = K_UPARROW;    break;
		case SDL_SCANCODE_DOWN:         charkey = K_DOWNARROW;  break;
		case SDL_SCANCODE_LEFT:         charkey = K_LEFTARROW;  break;
		case SDL_SCANCODE_RIGHT:        charkey = K_RIGHTARROW; break;
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:         charkey = K_OPTION;     break;
		case SDL_SCANCODE_LCTRL:        charkey = K_LCTRL;      break;
		case SDL_SCANCODE_RCTRL:        charkey = K_RCTRL;      break;
		case SDL_SCANCODE_LSHIFT:       charkey = K_LSHIFT;     break;
		case SDL_SCANCODE_RSHIFT:       charkey = K_LSHIFT;     break;
		case SDL_SCANCODE_F1:           charkey = K_F1;         break;
		case SDL_SCANCODE_F2:           charkey = K_F2;         break;
		case SDL_SCANCODE_F3:           charkey = K_F3;         break;
		case SDL_SCANCODE_F4:           charkey = K_F4;         break;
		case SDL_SCANCODE_F5:           charkey = K_F5;         break;
		case SDL_SCANCODE_F6:           charkey = K_F6;         break;
		case SDL_SCANCODE_F7:           charkey = K_F7;         break;
		case SDL_SCANCODE_F8:           charkey = K_F8;         break;
		case SDL_SCANCODE_F9:           charkey = K_F9;         break;
		case SDL_SCANCODE_F10:          charkey = K_F10;        break;
		case SDL_SCANCODE_F11:          charkey = K_F11;        break;
		case SDL_SCANCODE_F12:          charkey = K_F12;        break;
		case SDL_SCANCODE_F13:          charkey = K_F13;        break;
		case SDL_SCANCODE_F14:          charkey = K_F14;        break;
		case SDL_SCANCODE_F15:          charkey = K_F15;        break;
		case SDL_SCANCODE_INSERT:       charkey = K_INS;        break;
		case SDL_SCANCODE_DELETE:       charkey = K_BACKSPACE;  break;
		case SDL_SCANCODE_PAGEUP:       charkey = K_PGUP;       break;
		case SDL_SCANCODE_PAGEDOWN:     charkey = K_PGDN;       break;
		case SDL_SCANCODE_HOME:         charkey = K_HOME;       break;
		case SDL_SCANCODE_END:          charkey = K_END;        break;
		case SDL_SCANCODE_GRAVE:        charkey = '~';          break;
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:         charkey = K_COMMAND;    break;
			
			
		case SDL_SCANCODE_A:			charkey = 'a';			break;
		case SDL_SCANCODE_B:			charkey = 'b';			break;
		case SDL_SCANCODE_C:			charkey = 'c';			break;
		case SDL_SCANCODE_D:			charkey = 'd';			break;
		case SDL_SCANCODE_E:			charkey = 'e';			break;
		case SDL_SCANCODE_F:			charkey = 'f';			break;
		case SDL_SCANCODE_G:			charkey = 'g';			break;
		case SDL_SCANCODE_H:			charkey = 'h';			break;
		case SDL_SCANCODE_I:			charkey = 'i';			break;
		case SDL_SCANCODE_J:			charkey = 'j';			break;
		case SDL_SCANCODE_K:			charkey = 'k';			break;
		case SDL_SCANCODE_L:			charkey = 'l';			break;
		case SDL_SCANCODE_M:			charkey = 'm';			break;
		case SDL_SCANCODE_N:			charkey = 'n';			break;
		case SDL_SCANCODE_O:			charkey = 'o';			break;
		case SDL_SCANCODE_P:			charkey = 'p';			break;
		case SDL_SCANCODE_Q:			charkey = 'q';			break;
		case SDL_SCANCODE_R:			charkey = 'r';			break;
		case SDL_SCANCODE_S:			charkey = 's';			break;
		case SDL_SCANCODE_T:			charkey = 't';			break;
		case SDL_SCANCODE_U:			charkey = 'u';			break;
		case SDL_SCANCODE_V:			charkey = 'v';			break;
		case SDL_SCANCODE_W:			charkey = 'w';			break;
		case SDL_SCANCODE_X:			charkey = 'x';			break;
		case SDL_SCANCODE_Y:			charkey = 'y';			break;
		case SDL_SCANCODE_Z:			charkey = 'z';			break;
			
		case SDL_SCANCODE_1:			charkey = '1';			break;
		case SDL_SCANCODE_2:			charkey = '2';			break;
		case SDL_SCANCODE_3:			charkey = '3';			break;
		case SDL_SCANCODE_4:			charkey = '4';			break;
		case SDL_SCANCODE_5:			charkey = '5';			break;
		case SDL_SCANCODE_6:			charkey = '6';			break;
		case SDL_SCANCODE_7:			charkey = '7';			break;
		case SDL_SCANCODE_8:			charkey = '8';			break;
		case SDL_SCANCODE_9:			charkey = '9';			break;
		case SDL_SCANCODE_0:			charkey = '0';			break;
	}
	return charkey;
}

/**
 * Function which is called whenever a key is pressed or released.
 * @param event the SDL event object containing the keysym et all
 * @param state either true if it is a keydown event or false otherwise
 */
static void key_event( const SDL_KeyboardEvent *event, const bool state )
{
	qwchar charkey = TranslateSDLScancode(event->keysym.scancode);
	
	if(charkey >= 0 && charkey <= 255) {
		Key_Event(charkey, state, Sys_Milliseconds());
	}
}

/*****************************************************************************/

static void HandleEvents( void )
{
	Uint16* wtext = NULL;
	SDL_PumpEvents();
	
	SDL_Event event;

	while( SDL_PollEvent( &event ) )
	{
		//printf("Event: %u\n", event.type);
		switch( event.type )
		{
		case SDL_KEYDOWN:
			key_event( &event.key, true );
				
				// Emulate Ctrl+V
				if (event.key.keysym.sym == SDLK_v)
				{
					if (event.key.keysym.mod & KMOD_CTRL)
					{
						Key_CharEvent(22, 22);
					}
				}
				
			break;

		case SDL_KEYUP:
			key_event( &event.key, false );
			break;
				
		case SDL_TEXTINPUT:
			// SDL_iconv_utf8_ucs2 uses "UCS-2-INTERNAL" as tocode and fails to convert text on Linux
			// where SDL_iconv uses system iconv. So we force needed encoding directly

			#if SDL_BYTEORDER == SDL_LIL_ENDIAN
				#define UCS_2_INTERNAL "UCS-2LE"
			#else
				#define UCS_2_INTERNAL "UCS-2BE"
			#endif
				
			wtext = (Uint16*)SDL_iconv_string(UCS_2_INTERNAL, "UTF-8", event.text.text, SDL_strlen(event.text.text) + 1);
			if (wtext) {
				qwchar charkey = wtext[0];
				Key_CharEvent(charkey, charkey);
				SDL_free(wtext);
			}
			break;

		case SDL_MOUSEMOTION:
			mouse_motion_event( &event.motion );
			break;

		case SDL_MOUSEBUTTONDOWN:
			mouse_button_event( &event.button, true );
			break;

		case SDL_MOUSEBUTTONUP:
			mouse_button_event( &event.button, false );
			break;
				
		case SDL_MOUSEWHEEL:
			mouse_wheel_event( &event.wheel );
			break;
				
		case SDL_QUIT:
			Sys_Quit();
			break;
		}
	}
}

void IN_MouseMove( usercmd_t *cmd )
{
	if( ( mx || my ) && mouse_active )
	{
		CL_MouseMove( cmd, mx, my );
		mx = my = 0;
	}
}

void IN_JoyMove( usercmd_t *cmd, int frametime )
{
}

void IN_Init()
{
	if( input_inited )
		return;
	
	in_grabinconsole = Cvar_Get( "in_grabinconsole", "0", CVAR_ARCHIVE );
	in_disablemacosxmouseaccel = Cvar_Get( "in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE );
	
	Com_Printf("Initializing SDL Input\n");
	
	// SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL ); // Not available in SDL2
	Com_Printf("SDL_ShowCursor = %i\n", SDL_ShowCursor( SDL_QUERY ));
	SDL_SetRelativeMouseMode( SDL_TRUE );
	SDL_SetCursor( NULL );
	
	IN_SetMouseScalingEnabled( false );

	input_inited = true;
	input_active = true; // will be activated by IN_Frame if necessary
}

/**
 * Shutdown input subsystem.
 */
void IN_Shutdown()
{
	if( !input_inited )
		return;

	Com_Printf("Shutdown SDL Input\n");
	
	IN_Activate( false );
	input_inited = false;
	SDL_SetRelativeMouseMode( SDL_FALSE );
	IN_SetMouseScalingEnabled( true );
}

/**
 * Restart the input subsystem.
 */
void IN_Restart( void )
{
	IN_Shutdown();
	IN_Init();
}

/**
 * This function is called for every frame and gives us some time to poll
 * for events that occured at our input devices.
 */
void IN_Frame()
{
	if( !input_inited )
		return;

	if( !Cvar_Value( "vid_fullscreen" ) && cls.key_dest == key_console && !in_grabinconsole->integer )
	{
		mouse_active = false;
		input_active = true;
		if(SDL_GetRelativeMouseMode())
		{
			IN_SetMouseScalingEnabled( true );
			SDL_SetRelativeMouseMode( SDL_FALSE );
		}
	}
	else
	{
		mouse_active = true;
		input_active = true;
		if(!SDL_GetRelativeMouseMode())
		{
			IN_SetMouseScalingEnabled( false );
			SDL_SetRelativeMouseMode( SDL_TRUE );	
		}
	}

	mouse_active = true;
	input_active = true;
	
	HandleEvents();
}

/**
 * Stub for showing an on-screen keyboard.
 */
void IN_ShowIME( bool show )
{
}

/**
 * Display the mouse cursor in the UI.
 */
bool IN_ShowUICursor( void )
{
	return true;
}
