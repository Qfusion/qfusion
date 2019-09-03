#include <SDL.h>
#include "../client/client.h"
#include "sdl_input_joy.h"

cvar_t *in_grabinconsole;
cvar_t *in_disablemacosxmouseaccel;
cvar_t *in_mousehack;

extern cvar_t *vid_xpos;
extern cvar_t *vid_ypos;

extern SDL_Window *sdl_window;

static bool input_inited = false;
static bool input_active = false;
static bool input_focus = false;
static bool mouse_relative = false;

static int mx = 0, my = 0;

static bool bugged_rawXevents = false;

#if defined( __APPLE__ )
void IN_SetMouseScalingEnabled( bool isRestore );
#else
void IN_SetMouseScalingEnabled( bool isRestore ) {
}
#endif

void IN_Commands( void ) {
	IN_SDL_JoyCommands();
}

/**
 * Function which is called whenever the mouse is moved.
 * @param ev the SDL event object containing the mouse position et all
 */
static void mouse_motion_event( SDL_MouseMotionEvent *event ) {
	// See:
	// https://bugzilla.libsdl.org/show_bug.cgi?id=2963
	// https://bugs.freedesktop.org/show_bug.cgi?id=71609
	if( mouse_relative && bugged_rawXevents ) {
		static Uint32 last_timestamp;
		static Uint32 last_which;
		static Sint32 last_xrel, last_yrel;

		if( last_timestamp == event->timestamp && last_which == event->which
			&& last_xrel == event->xrel && last_yrel == event->yrel ) {
			return;
		}

		last_timestamp = event->timestamp;
		last_which = event->which;
		last_xrel = event->xrel;
		last_yrel = event->yrel;
	}

	mx += event->xrel;
	my += event->yrel;
}

/**
 * Function which is called whenever a mouse button is pressed or released.
 * @param ev the SDL event object containing the button number et all
 * @param state either true if it is a keydown event or false otherwise
 */
static void mouse_button_event( SDL_MouseButtonEvent *event, bool state ) {
	Uint8 button = event->button;

	if( button <= 10 ) {
		// The engine only supports up to 8 buttons plus the mousewheel.

		switch( button ) {
			case SDL_BUTTON_LEFT:
				Key_MouseEvent( K_MOUSE1, state, Sys_Milliseconds() );
				break;
			case SDL_BUTTON_MIDDLE:
				Key_MouseEvent( K_MOUSE3, state, Sys_Milliseconds() );
				break;
			case SDL_BUTTON_RIGHT:
				Key_MouseEvent( K_MOUSE2, state, Sys_Milliseconds() );
				break;
			case 4:
				if( in_mousehack->integer ) {
					Key_MouseEvent( K_MOUSE6, state, Sys_Milliseconds() );
				}
				break;
			case 5:
				if( in_mousehack->integer ) {
					Key_MouseEvent( K_MOUSE7, state, Sys_Milliseconds() );
				}
				break;
			case 6:
				Key_MouseEvent( K_MOUSE6, state, Sys_Milliseconds() );
				break;
			case 7:
				Key_MouseEvent( K_MOUSE7, state, Sys_Milliseconds() );
				break;
			case 8:
				Key_MouseEvent( K_MOUSE4, state, Sys_Milliseconds() );
				break;
			case 9:
				Key_MouseEvent( K_MOUSE5, state, Sys_Milliseconds() );
				break;
			case 10:
				Key_MouseEvent( K_MOUSE8, state, Sys_Milliseconds() );
				break;
		}
	} else {
		Com_Printf( "sdl_input.c: Unsupported mouse button (button = %u)\n", button );
	}
}

static void mouse_wheel_event( SDL_MouseWheelEvent *event ) {
	int key = event->y > 0 ? K_MWHEELUP : K_MWHEELDOWN;
	int64_t sys_msg_time = Sys_Milliseconds();

	Key_Event( key, true, sys_msg_time );
	Key_Event( key, false, sys_msg_time );
}

static wchar_t TranslateSDLScancode( SDL_Scancode scancode ) {
	wchar_t charkey = 0;

	switch( scancode ) {
		case SDL_SCANCODE_TAB:          charkey = K_TAB;        break;
		case SDL_SCANCODE_RETURN:       charkey = K_ENTER;      break;
		case SDL_SCANCODE_ESCAPE:       charkey = K_ESCAPE;     break;
		case SDL_SCANCODE_SPACE:        charkey = K_SPACE;      break;
		case SDL_SCANCODE_CAPSLOCK:     charkey = K_CAPSLOCK;   break;
		case SDL_SCANCODE_SCROLLLOCK:   charkey = K_SCROLLLOCK; break;
		case SDL_SCANCODE_NUMLOCKCLEAR: charkey = K_NUMLOCK;    break;
		case SDL_SCANCODE_BACKSPACE:    charkey = K_BACKSPACE;  break;
		case SDL_SCANCODE_UP:           charkey = K_UPARROW;    break;
		case SDL_SCANCODE_DOWN:         charkey = K_DOWNARROW;  break;
		case SDL_SCANCODE_LEFT:         charkey = K_LEFTARROW;  break;
		case SDL_SCANCODE_RIGHT:        charkey = K_RIGHTARROW; break;
#if defined( __APPLE__ )
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:         charkey = K_OPTION;     break;
#else
		case SDL_SCANCODE_LALT:         charkey = K_LALT;       break;
		case SDL_SCANCODE_RALT:         charkey = K_RALT;       break;
#endif
		case SDL_SCANCODE_LCTRL:        charkey = K_LCTRL;      break;
		case SDL_SCANCODE_RCTRL:        charkey = K_RCTRL;      break;
		case SDL_SCANCODE_LSHIFT:       charkey = K_LSHIFT;     break;
		case SDL_SCANCODE_RSHIFT:       charkey = K_RSHIFT;     break;
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
		case SDL_SCANCODE_DELETE:       charkey = K_DEL;        break;
		case SDL_SCANCODE_PAGEUP:       charkey = K_PGUP;       break;
		case SDL_SCANCODE_PAGEDOWN:     charkey = K_PGDN;       break;
		case SDL_SCANCODE_HOME:         charkey = K_HOME;       break;
		case SDL_SCANCODE_END:          charkey = K_END;        break;
		case SDL_SCANCODE_GRAVE:        charkey = '~';          break;
		case SDL_SCANCODE_NONUSBACKSLASH: charkey = '<';          break;
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:         charkey = K_COMMAND;    break;


		case SDL_SCANCODE_A:            charkey = 'a';          break;
		case SDL_SCANCODE_B:            charkey = 'b';          break;
		case SDL_SCANCODE_C:            charkey = 'c';          break;
		case SDL_SCANCODE_D:            charkey = 'd';          break;
		case SDL_SCANCODE_E:            charkey = 'e';          break;
		case SDL_SCANCODE_F:            charkey = 'f';          break;
		case SDL_SCANCODE_G:            charkey = 'g';          break;
		case SDL_SCANCODE_H:            charkey = 'h';          break;
		case SDL_SCANCODE_I:            charkey = 'i';          break;
		case SDL_SCANCODE_J:            charkey = 'j';          break;
		case SDL_SCANCODE_K:            charkey = 'k';          break;
		case SDL_SCANCODE_L:            charkey = 'l';          break;
		case SDL_SCANCODE_M:            charkey = 'm';          break;
		case SDL_SCANCODE_N:            charkey = 'n';          break;
		case SDL_SCANCODE_O:            charkey = 'o';          break;
		case SDL_SCANCODE_P:            charkey = 'p';          break;
		case SDL_SCANCODE_Q:            charkey = 'q';          break;
		case SDL_SCANCODE_R:            charkey = 'r';          break;
		case SDL_SCANCODE_S:            charkey = 's';          break;
		case SDL_SCANCODE_T:            charkey = 't';          break;
		case SDL_SCANCODE_U:            charkey = 'u';          break;
		case SDL_SCANCODE_V:            charkey = 'v';          break;
		case SDL_SCANCODE_W:            charkey = 'w';          break;
		case SDL_SCANCODE_X:            charkey = 'x';          break;
		case SDL_SCANCODE_Y:            charkey = 'y';          break;
		case SDL_SCANCODE_Z:            charkey = 'z';          break;

		case SDL_SCANCODE_1:            charkey = '1';          break;
		case SDL_SCANCODE_2:            charkey = '2';          break;
		case SDL_SCANCODE_3:            charkey = '3';          break;
		case SDL_SCANCODE_4:            charkey = '4';          break;
		case SDL_SCANCODE_5:            charkey = '5';          break;
		case SDL_SCANCODE_6:            charkey = '6';          break;
		case SDL_SCANCODE_7:            charkey = '7';          break;
		case SDL_SCANCODE_8:            charkey = '8';          break;
		case SDL_SCANCODE_9:            charkey = '9';          break;
		case SDL_SCANCODE_0:            charkey = '0';          break;

		case SDL_SCANCODE_MINUS:        charkey = '-';          break;
		case SDL_SCANCODE_EQUALS:       charkey = '=';          break;
		case SDL_SCANCODE_BACKSLASH:        charkey = '\\';         break;
		case SDL_SCANCODE_COMMA:        charkey = ',';          break;
		case SDL_SCANCODE_PERIOD:       charkey = '.';          break;
		case SDL_SCANCODE_SLASH:        charkey = '/';          break;
		case SDL_SCANCODE_LEFTBRACKET:      charkey = '[';          break;
		case SDL_SCANCODE_RIGHTBRACKET:     charkey = ']';          break;
		case SDL_SCANCODE_SEMICOLON:        charkey = ';';          break;
		case SDL_SCANCODE_APOSTROPHE:       charkey = '\'';         break;

		case SDL_SCANCODE_KP_0:         charkey = KP_INS;       break;
		case SDL_SCANCODE_KP_1:         charkey = KP_END;       break;
		case SDL_SCANCODE_KP_2:         charkey = KP_DOWNARROW;     break;
		case SDL_SCANCODE_KP_3:         charkey = KP_PGDN;      break;
		case SDL_SCANCODE_KP_4:         charkey = KP_LEFTARROW;     break;
		case SDL_SCANCODE_KP_5:         charkey = KP_5;         break;
		case SDL_SCANCODE_KP_6:         charkey = KP_RIGHTARROW;    break;
		case SDL_SCANCODE_KP_7:         charkey = KP_HOME;      break;
		case SDL_SCANCODE_KP_8:         charkey = KP_UPARROW;       break;
		case SDL_SCANCODE_KP_9:         charkey = KP_PGUP;      break;
		case SDL_SCANCODE_KP_ENTER:     charkey = KP_ENTER;     break;
		case SDL_SCANCODE_KP_PERIOD:        charkey = KP_DEL;       break;
		case SDL_SCANCODE_KP_PLUS:      charkey = KP_PLUS;      break;
		case SDL_SCANCODE_KP_MINUS:     charkey = KP_MINUS;     break;
		case SDL_SCANCODE_KP_DIVIDE:        charkey = KP_SLASH;     break;
		case SDL_SCANCODE_KP_MULTIPLY:      charkey = KP_STAR;      break;
		case SDL_SCANCODE_KP_EQUALS:        charkey = KP_EQUAL;     break;

		default: break;
	}
	return charkey;
}

/**
 * Function which is called whenever a key is pressed or released.
 * @param event the SDL event object containing the keysym et all
 * @param state either true if it is a keydown event or false otherwise
 */
static void key_event( const SDL_KeyboardEvent *event, const bool state ) {
	wchar_t charkey = TranslateSDLScancode( event->keysym.scancode );

	if( charkey >= 0 && charkey <= 255 ) {
		Key_Event( charkey, state, Sys_Milliseconds() );
	}
}

/*****************************************************************************/

static void AppActivate( SDL_Window *window, bool active ) {
	bool minimized = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_MINIMIZED ) != 0;

	SCR_PauseCinematic( !active );
	CL_SoundModule_Activate( active );
	VID_AppActivate( active, minimized, false );
}

static void IN_HandleEvents( void ) {
	Uint16 *wtext = NULL;
	SDL_PumpEvents();
	SDL_Event event;

	while( SDL_PollEvent( &event ) ) {
		switch( event.type ) {
			case SDL_KEYDOWN:
				key_event( &event.key, true );

				// Emulate copy/paste
				#if defined( __APPLE__ )
					#define KEYBOARD_COPY_PASTE_MODIFIER KMOD_GUI
				#else
					#define KEYBOARD_COPY_PASTE_MODIFIER KMOD_CTRL
				#endif

				if( event.key.keysym.sym == SDLK_c ) {
					if( event.key.keysym.mod & KEYBOARD_COPY_PASTE_MODIFIER ) {
						Key_CharEvent( KC_CTRLC, KC_CTRLC );
					}
				} else if( event.key.keysym.sym == SDLK_v ) {
					if( event.key.keysym.mod & KEYBOARD_COPY_PASTE_MODIFIER ) {
						Key_CharEvent( KC_CTRLV, KC_CTRLV );
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

				wtext = (Uint16 *)SDL_iconv_string( UCS_2_INTERNAL, "UTF-8", event.text.text, SDL_strlen( event.text.text ) + 1 );
				if( wtext ) {
					wchar_t charkey = wtext[0];
					int key = ( charkey <= 255 ) ? charkey : 0;
					Key_CharEvent( key, charkey );
					SDL_free( wtext );
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
				Cbuf_ExecuteText( EXEC_NOW, "quit" );
				break;

			case SDL_WINDOWEVENT:
				switch( event.window.event ) {
					case SDL_WINDOWEVENT_SHOWN:
						AppActivate( SDL_GetWindowFromID( event.window.windowID ), true );
						break;
					case SDL_WINDOWEVENT_HIDDEN:
						AppActivate( SDL_GetWindowFromID( event.window.windowID ), false );
						break;
					case SDL_WINDOWEVENT_CLOSE:
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						input_focus = true;
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						input_focus = false;
						break;
					case SDL_WINDOWEVENT_MOVED:
						// FIXME: move this somewhere else
						if( !Cvar_Value( "vid_fullscreen" ) ) {
							Cvar_SetValue( "vid_xpos", event.window.data1 );
							Cvar_SetValue( "vid_ypos", event.window.data2 );
							vid_xpos->modified = false;
							vid_ypos->modified = false;
						}
						break;
				}
				break;
		}
	}
}

/**
 * Skips relative mouse movement for current frame.
 * We need to ignore the movement event generated when
 * mouse cursor is warped to window centre for the first time.
 */
static void IN_SkipRelativeMouseMove( void ) {
	if( mouse_relative ) {
		SDL_GetRelativeMouseState( &mx, &my );
		mx = my = 0;
	}
}

static void IN_WarpMouseToCenter( int *pcenter_x, int *pcenter_y ) {
	int center_x, center_y;

	SDL_GetWindowSize( sdl_window, &center_x, &center_y );

	center_x /= 2;
	center_y /= 2;

	SDL_WarpMouseInWindow( sdl_window, center_x, center_y );

	if( pcenter_x ) {
		*pcenter_x = center_x;
	}
	if( pcenter_y ) {
		*pcenter_y = center_y;
	}
}

void IN_GetMouseMovement( int *dx, int *dy ) {
	if( !mouse_relative && cls.key_dest == key_game ) {
		if( mx || my ) {
			int center_x, center_y;

			SDL_GetMouseState( &mx, &my );

			IN_WarpMouseToCenter( &center_x, &center_y );

			mx -= center_x;
			my -= center_y;
		}
	}

	*dx = mx;
	*dy = my;

	mx = my = 0;
}

void IN_GetMousePosition( int *x, int *y ) {
	SDL_GetMouseState( x, y );
}

void IN_Init() {
	SDL_version linked;

	if( input_inited ) {
		return;
	}

	in_grabinconsole = Cvar_Get( "in_grabinconsole", "0", CVAR_ARCHIVE );
	in_disablemacosxmouseaccel = Cvar_Get( "in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE );
	in_mousehack = Cvar_Get( "in_mousehack", "0", CVAR_ARCHIVE );

	SDL_GetVersion( &linked );

#if SDL_VERSION_ATLEAST( 2, 0, 2 )

	{
		cvar_t *m_raw = Cvar_Get( "m_raw", "1", CVAR_ARCHIVE );
		SDL_SetHint( SDL_HINT_MOUSE_RELATIVE_MODE_WARP, m_raw->integer ? "0" : "1" );
	}
#endif

	IN_SDL_JoyInit( true );

	input_focus = true;
	input_inited = true;
	input_active = true; // will be activated by IN_Frame if necessary
	bugged_rawXevents = linked.major == 2 && linked.minor == 0 && linked.patch < 4;
}

void IN_Shutdown() {
	if( !input_inited ) {
		return;
	}

	input_inited = false;
	SDL_SetRelativeMouseMode( SDL_FALSE );
	IN_SetMouseScalingEnabled( true );
	IN_SDL_JoyShutdown();
}

void IN_Restart( void ) {
	IN_Shutdown();
	IN_Init();
}

static void IN_SetMouseState()
{
	bool show_cursor;
	bool want_active;
	bool want_relative;
	bool was_relative;

	if( !input_inited ) {
		return;
	}

	show_cursor = !input_focus;
	show_cursor = show_cursor || ( !Cvar_Value( "vid_fullscreen" ) && cls.key_dest == key_console && !in_grabinconsole->integer );
	show_cursor = show_cursor || ( cls.key_dest == key_menu && cls.show_cursor );

	want_active = input_focus && ( show_cursor || cls.key_dest == key_game );
	want_relative = input_focus && ( cls.key_dest == key_game );

	mouse_relative = SDL_GetRelativeMouseMode() == SDL_TRUE;
	was_relative = mouse_relative;

	if( want_relative != mouse_relative ) {
		bool success = SDL_SetRelativeMouseMode( want_relative ) == 0;
		mouse_relative = want_relative == success;

		if( success ) {
			IN_SetMouseScalingEnabled( !mouse_relative );

			if( mouse_relative ) {
				// skip the first relative mouse movement, which may be
				// an artifact from centering the cursor
				IN_SkipRelativeMouseMove();
			}
		}
	}

	if( !mouse_relative ) {
		bool cur_show_cursor = SDL_ShowCursor( -1 ) == 1 && !was_relative;
		if( show_cursor != cur_show_cursor ) {
			SDL_ShowCursor( show_cursor ? 1 : 0 );

			IN_WarpMouseToCenter( NULL, NULL );
		}
	}
}

/**
 * This function is called for every frame and gives us some time to poll
 * for events that occured at our input devices.
 */
void IN_Frame()
{
	// update the cursor/mouse state
	IN_SetMouseState();

	IN_HandleEvents();

	// update the state again, as IN_HandleEvents might have changed cls.key_dest
	IN_SetMouseState();
}

/**
 * The input devices supported by the system.
 */
unsigned int IN_SupportedDevices( void ) {
	return IN_DEVICE_KEYBOARD | IN_DEVICE_MOUSE | IN_DEVICE_JOYSTICK;
}

/**
 * Stub for showing an on-screen keyboard.
 */
void IN_ShowSoftKeyboard( bool show ) {
}

/**
 * Stubs for the IME until it's implemented through SDL and/or Cocoa.
 */
void IN_IME_Enable( bool enable ) {
}

size_t IN_IME_GetComposition( char *str, size_t strSize, size_t *cursorPos, size_t *convStart, size_t *convLen ) {
	if( str && strSize ) {
		str[0] = '\0';
	}
	if( cursorPos ) {
		*cursorPos = 0;
	}
	if( convStart ) {
		*convStart = 0;
	}
	if( convLen ) {
		*convLen = 0;
	}
	return 0;
}

unsigned int IN_IME_GetCandidates( char * const *cands, size_t candSize, unsigned int maxCands, int *selected, int *firstKey ) {
	if( selected ) {
		*selected = -1;
	}
	if( firstKey ) {
		*firstKey = 1;
	}
	return 0;
}
