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

#include "android_sys.h"
#include <unistd.h>

static bool in_android_initialized;

static vec4_t in_android_thumbsticks;

cvar_t *in_grabinconsole;

static void IN_Android_ShowSoftKeyboard( void );
static bool IN_Android_HideSoftKeyboard( void );

static const int s_android_scantokey[] =
{
	0,              K_LEFTARROW,    K_RIGHTARROW,   0,              K_ESCAPE,       // 0
	0,              0,              '0',            '1',            '2',            // 5
	'3',            '4',            '5',            '6',            '7',            // 10
	'8',            '9',            '*',            '#',            K_DPAD_UP,      // 15
	K_DPAD_DOWN,    K_DPAD_LEFT,    K_DPAD_RIGHT,   K_DPAD_CENTER,  0,              // 20
	0,              0,              0,              0,              'a',            // 25
	'b',            'c',            'd',            'e',            'f',            // 30
	'g',            'h',            'i',            'j',            'k',            // 35
	'l',            'm',            'n',            'o',            'p',            // 40
	'q',            'r',            's',            't',            'u',            // 45
	'v',            'w',            'x',            'y',            'z',            // 50
	',',            '.',            K_LALT,         K_RALT,         K_LSHIFT,       // 55
	K_RSHIFT,       K_TAB,          K_SPACE,        0,              0,              // 60
	0,              K_ENTER,        K_BACKSPACE,    '`',            '-',            // 65
	'=',            '[',            ']',            '\\',           ';',            // 70
	'\'',           '/',            '@',            K_LALT,         0,              // 75
	0,              '+',            0,              0,              0,              // 80
	0,              0,              0,              0,              0,              // 85
	0,              0,              K_PGUP,         K_PGDN,         0,              // 90
	0,              K_A_BUTTON,     K_B_BUTTON,     K_C_BUTTON,     K_X_BUTTON,     // 95
	K_Y_BUTTON,     K_Z_BUTTON,     K_LSHOULDER,    K_RSHOULDER,    K_LTRIGGER,     // 100
	K_RTRIGGER,     K_LSTICK,       K_RSTICK,       K_ESCAPE,       K_ESCAPE,       // 105
	0,              K_ESCAPE,       K_DEL,          K_LCTRL,        K_RCTRL,        // 110
	K_CAPSLOCK,     K_SCROLLLOCK,   0,              0,              0,              // 115
	0,              K_PAUSE,        K_HOME,         K_END,          K_INS,          // 120
	0,              0,              0,              0,              0,              // 125
	0,              K_F1,           K_F2,           K_F3,           K_F4,           // 130
	K_F5,           K_F6,           K_F7,           K_F8,           K_F9,           // 135
	K_F10,          K_F11,          K_F12,          K_NUMLOCK,      KP_INS,         // 140
	KP_END,         KP_DOWNARROW,   KP_PGDN,        KP_LEFTARROW,   KP_5,           // 145
	KP_RIGHTARROW,  KP_HOME,        KP_UPARROW,     KP_PGUP,        KP_SLASH,       // 150
	KP_STAR,        KP_MINUS,       KP_PLUS,        KP_DEL,         ',',            // 155
	KP_ENTER,       KP_EQUAL,       '(',            ')'
};

/*
* IN_Android_KeyEvent2UCS
*/
static wchar_t IN_Android_KeyEvent2UCS( const AInputEvent *event ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jclass mapClass;
	static jmethodID load, get;
	jobject map;
	unsigned int ucs;

	if( !mapClass ) {
		jclass mapClassRef;

		mapClassRef = ( *env )->FindClass( env, "android/view/KeyCharacterMap" );
		mapClass = ( *env )->NewGlobalRef( env, mapClassRef );
		load = ( *env )->GetStaticMethodID( env, mapClass, "load", "(I)Landroid/view/KeyCharacterMap;" );
		get = ( *env )->GetMethodID( env, mapClass, "get", "(II)I" );
		( *env )->DeleteLocalRef( env, mapClassRef );
	}

	map = ( *env )->CallStaticObjectMethod( env, mapClass, load, AInputEvent_getDeviceId( event ) );
	ucs = ( *env )->CallIntMethod( env, map, get, AKeyEvent_getKeyCode( event ), AKeyEvent_getMetaState( event ) );
	( *env )->DeleteLocalRef( env, map );

	return ucs;
}

/*
* IN_Android_EventToWindowCoordinates
*/
static bool IN_Android_EventToWindowCoordinates( const AInputEvent *event, size_t p, int *x, int *y ) {
	ANativeWindow *window = sys_android_app->window;

	if( !window ) {
		return false;
	}

	*x = ( AMotionEvent_getX( event, p ) / ( float )ANativeWindow_getWidth( window ) ) * viddef.width;
	*y = ( AMotionEvent_getY( event, p ) / ( float )ANativeWindow_getHeight( window ) ) * viddef.height;
	return true;
}

/*
* IN_Android_OnInputEvent
*/
static int32_t IN_Android_OnInputEvent( struct android_app *app, AInputEvent *event ) {
	int32_t type = AInputEvent_getType( event );
	int64_t time;

	if( type == AINPUT_EVENT_TYPE_KEY ) {
		int32_t keycode = AKeyEvent_getKeyCode( event );
		int key;

		if( keycode >= ( sizeof( s_android_scantokey ) / sizeof( s_android_scantokey[0] ) ) ) {
			return 0;
		}

		if( ( keycode >= AKEYCODE_DPAD_UP ) && ( keycode <= AKEYCODE_DPAD_RIGHT ) &&
			( AInputEvent_getSource( event ) == AINPUT_SOURCE_KEYBOARD ) ) {
			key = keycode + ( K_UPARROW - AKEYCODE_DPAD_UP );
		} else {
			key = s_android_scantokey[keycode];
		}
		if( !key ) {
			return 0;
		}

		time = AKeyEvent_getEventTime( event ) / ( ( int64_t )1000000 );

		switch( AKeyEvent_getAction( event ) ) {
			case AKEY_EVENT_ACTION_DOWN:
			case AKEY_EVENT_ACTION_MULTIPLE:
				if( ( key == K_ESCAPE ) && IN_Android_HideSoftKeyboard() ) { // Instead of broken AInputQueue_preDispatchEvent.
					return 1;
				}

				Key_Event( key, true, time );

				if( Key_IsDown( K_LCTRL ) || Key_IsDown( K_RCTRL ) ) {
					if( key == 'v' ) {
						Key_CharEvent( KC_CTRLV, KC_CTRLV );
					} else if( key == 'c' ) {
						Key_CharEvent( KC_CTRLC, KC_CTRLC );
					} else {
						Key_CharEvent( key, IN_Android_KeyEvent2UCS( event ) );
					}
				} else {
					Key_CharEvent( key, IN_Android_KeyEvent2UCS( event ) );
				}
				break;

			case AKEY_EVENT_ACTION_UP:
				Key_Event( key, false, time );
				break;
		}

		return 1;
	}

	if( type == AINPUT_EVENT_TYPE_MOTION ) {
		int32_t action = AMotionEvent_getAction( event );
		int32_t source = AInputEvent_getSource( event );
		int x, y;

		time = AMotionEvent_getEventTime( event ) / ( ( int64_t )1000000 );

		switch( source ) {
			case AINPUT_SOURCE_TOUCHSCREEN:
			{
				touchevent_t type;
				size_t i, pointerCount = 0, p;

				switch( action & AMOTION_EVENT_ACTION_MASK ) {
					case AMOTION_EVENT_ACTION_DOWN:
					case AMOTION_EVENT_ACTION_POINTER_DOWN:
						type = TOUCH_DOWN;
						pointerCount = 1;
						break;
					case AMOTION_EVENT_ACTION_POINTER_UP:
						type = TOUCH_UP;
						pointerCount = 1;
						break;
					case AMOTION_EVENT_ACTION_MOVE:
						type = TOUCH_MOVE;
						pointerCount = AMotionEvent_getPointerCount( event );
						break;
					case AMOTION_EVENT_ACTION_UP:
					case AMOTION_EVENT_ACTION_CANCEL:
					case AMOTION_EVENT_ACTION_OUTSIDE:
						type = TOUCH_UP;
						pointerCount = AMotionEvent_getPointerCount( event );
						break;
				}

				p = action >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
				for( i = 0; i < pointerCount; ++i, ++p ) {
					if( IN_Android_EventToWindowCoordinates( event, p, &x, &y ) ) {
						CL_TouchEvent( AMotionEvent_getPointerId( event, p ), type, x, y, time );
					}
				}
			}
			break;

			case AINPUT_SOURCE_JOYSTICK:
			{
				float hatXValue = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_HAT_X, 0 );
				float hatYValue = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_HAT_Y, 0 );
				int hatX = 0, hatY = 0;
				static int oldHatX = 0, oldHatY = 0;
				bool leftTrigger, rightTrigger;
				static bool oldLeftTrigger = false, oldRightTrigger = false;

				in_android_thumbsticks[0] = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_X, 0 );
				in_android_thumbsticks[1] = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_Y, 0 );
				in_android_thumbsticks[2] = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_Z, 0 );
				in_android_thumbsticks[3] = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_RZ, 0 );

				hatX = ( hatXValue > 0.5f ) - ( hatXValue < -0.5f );
				hatY = ( hatYValue > 0.5f ) - ( hatYValue < -0.5f );
				if( hatX != oldHatX ) {
					if( oldHatX ) {
						Key_Event( K_DPAD_LEFT + ( oldHatX > 0 ), false, time );
					}
					if( hatX ) {
						Key_Event( K_DPAD_LEFT + ( hatX > 0 ), true, time );
					}
					oldHatX = hatX;
				}
				if( hatY != oldHatY ) {
					if( oldHatY ) {
						Key_Event( K_DPAD_UP + ( oldHatY > 0 ), false, time );
					}
					if( hatY ) {
						Key_Event( K_DPAD_UP + ( hatY > 0 ), true, time );
					}
					oldHatY = hatY;
				}

				leftTrigger = ( AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_BRAKE, 0 ) > ( 30.0f / 255.0f ) )
							  || ( AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_LTRIGGER, 0 ) > ( 30.0f / 255.0f ) );
				rightTrigger = ( AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_GAS, 0 ) > ( 30.0f / 255.0f ) )
							   || ( AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_RTRIGGER, 0 ) > ( 30.0f / 255.0f ) );
				if( leftTrigger != oldLeftTrigger ) {
					Key_Event( K_LTRIGGER, leftTrigger, time );
					oldLeftTrigger = leftTrigger;
				}
				if( rightTrigger != oldRightTrigger ) {
					Key_Event( K_RTRIGGER, rightTrigger, time );
					oldRightTrigger = rightTrigger;
				}
			}
			break;

			case AINPUT_SOURCE_MOUSE:
			{
				switch( action & AMOTION_EVENT_ACTION_MASK ) {
					case AMOTION_EVENT_ACTION_DOWN:
					case AMOTION_EVENT_ACTION_UP:
					{
						static int32_t oldButtonState = 0;
						int32_t buttonState = AMotionEvent_getButtonState( event );
						int32_t buttonsDown = buttonState & ~oldButtonState, buttonsUp = oldButtonState & ~buttonState;
						int32_t buttonsChanged = buttonsDown | buttonsUp;
						int button;
						oldButtonState = buttonState;
						for( button = 0; buttonsChanged >> button; button++ ) {
							if( buttonsChanged & ( 1 << button ) ) {
								Key_MouseEvent( K_MOUSE1 + button, ( buttonsDown & ( 1 << button ) ) ? true : false, time );
							}
						}
					}
					break;
					case AMOTION_EVENT_ACTION_HOVER_MOVE:
					case AMOTION_EVENT_ACTION_MOVE:
						if( IN_Android_EventToWindowCoordinates( event, 0, &x, &y ) ) {
							CL_MouseSet( x, y, false );
						}
						break;
					case AMOTION_EVENT_ACTION_SCROLL:
					{
						float scroll = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_VSCROLL, 0 );
						if( scroll > 0.0f ) {
							Key_Event( K_MWHEELUP, true, time );
							Key_Event( K_MWHEELUP, false, time );
						} else if( scroll < 0.0f ) {
							Key_Event( K_MWHEELDOWN, true, time );
							Key_Event( K_MWHEELDOWN, false, time );
						}
					}
					break;
				}
			}
			break;
		}

		return 1;
	}

	return 0;
}

/*
* IN_Android_CharEvent
*/
void IN_Android_CharEvent( int charkey ) {
	int key = 0;

	if( !in_android_initialized ) {
		return;
	}

	switch( charkey ) {
		case 8:
			key = K_BACKSPACE;
			break;
		case 13:
			key = K_ENTER;
			break;
		case 127:
			key = K_DEL;
			break;
		default:
			Key_CharEvent( -1, charkey );
			break;
	}

	if( key ) {
		int64_t time = Sys_Android_Microseconds() / ( ( uint64_t )1000 );
		Key_Event( key, true, time );
		Key_Event( key, false, time );
	}
}

/*
* IN_Commands
*/
void IN_Commands( void ) {
}

/*
* IN_Frame
*/
void IN_Frame( void ) {
}

/*
* IN_Init
*/
void IN_Init( void ) {
	in_grabinconsole = Cvar_Get( "in_grabinconsole", "0", CVAR_ARCHIVE );
	sys_android_app->onInputEvent = IN_Android_OnInputEvent;
	in_android_initialized = true;
}

/*
* IN_JoyMove
*/
void IN_GetThumbsticks( vec4_t sticks ) {
	Vector4Copy( in_android_thumbsticks, sticks );
}

/*
* IN_GetMouseMovement
*/
void IN_GetMouseMovement( int *dx, int *dy ) {
}

/*
* IN_GetMousePosition
*/
void IN_GetMousePosition( int *x, int *y ) {
}

/*
* IN_Android_TouchscreenAvailable
*/
static bool IN_Android_TouchscreenAvailable( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	jobject inputManager;
	jmethodID getInputDeviceIds, getInputDevice, getSources;
	jintArray idsJA;
	jint *ids;
	int i, num;

	{
		jstring name = ( *env )->NewStringUTF( env, "input" );
		inputManager = ( *env )->CallObjectMethod( env, sys_android_app->activity->clazz, sys_android_getSystemService, name );
		( *env )->DeleteLocalRef( env, name );
	}

	{
		jclass c;
		c = ( *env )->FindClass( env, "android/hardware/input/InputManager" );
		getInputDeviceIds = ( *env )->GetMethodID( env, c, "getInputDeviceIds", "()[I" );
		getInputDevice = ( *env )->GetMethodID( env, c, "getInputDevice", "(I)Landroid/view/InputDevice;" );
		( *env )->DeleteLocalRef( env, c );
		c = ( *env )->FindClass( env, "android/view/InputDevice" );
		getSources = ( *env )->GetMethodID( env, c, "getSources", "()I" );
		( *env )->DeleteLocalRef( env, c );
	}

	idsJA = ( *env )->CallObjectMethod( env, inputManager, getInputDeviceIds );
	num = ( *env )->GetArrayLength( env, idsJA );
	ids = ( *env )->GetIntArrayElements( env, idsJA, NULL );
	for( i = 0; i < num; i++ ) {
		jobject device = ( *env )->CallObjectMethod( env, inputManager, getInputDevice, ids[i] );
		int sources;
		if( !device ) {
			continue;
		}
		sources = ( *env )->CallIntMethod( env, device, getSources );
		( *env )->DeleteLocalRef( env, device );
		if( ( sources & AINPUT_SOURCE_TOUCHSCREEN ) == AINPUT_SOURCE_TOUCHSCREEN ) {
			break;
		}
	}
	( *env )->ReleaseIntArrayElements( env, idsJA, ids, JNI_ABORT );
	( *env )->DeleteLocalRef( env, idsJA );

	( *env )->DeleteLocalRef( env, inputManager );

	return i < num;
}

/*
* IN_SupportedDevices
*/
unsigned int IN_SupportedDevices( void ) {
	static unsigned int devices = IN_DEVICE_JOYSTICK | IN_DEVICE_SOFTKEYBOARD;
	static bool touchscreenChecked;

	if( !touchscreenChecked ) {
		touchscreenChecked = true;
		if( IN_Android_TouchscreenAvailable() ) {
			devices |= IN_DEVICE_TOUCHSCREEN;
		}
	}

	return devices;
}

/*
* IN_ShowSoftKeyboard
*/
void IN_ShowSoftKeyboard( bool show ) {
	if( show ) {
		IN_Android_ShowSoftKeyboard();
	} else {
		IN_Android_HideSoftKeyboard();
	}
}

/*
* IN_Restart
*/
void IN_Restart( void ) {
	IN_Shutdown();
	IN_Init();
}

/*
* IN_Shutdown
*/
void IN_Shutdown( void ) {
	if( !in_android_initialized ) {
		return;
	}

	sys_android_app->onInputEvent = NULL;

	Vector4Set( in_android_thumbsticks, 0.0f, 0.0f, 0.0f, 0.0f );

	in_android_initialized = false;
}

/*
* IN_GetInputLanguage
*
* This is not needed because the language is visible on soft keyboard itself.
*/
void IN_GetInputLanguage( char *dest, size_t size ) {
	if( size ) {
		dest[0] = '\0';
	}
}

/*
=========================================================================

INPUT METHOD EDITORS

=========================================================================
*/

static bool in_android_ime_enabled;

void IN_IME_Enable( bool enable ) {
	if( in_android_ime_enabled == enable ) {
		return;
	}

	in_android_ime_enabled = enable;

	// Reset the keyboard to switch its mode if it's active.
	if( IN_Android_HideSoftKeyboard() ) {
		IN_Android_ShowSoftKeyboard();
	}
}

size_t IN_IME_GetComposition( char *str, size_t strSize, size_t *cursorPos, size_t *convStart, size_t *convLen ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID getComposingText, getComposingCursor;
	jstring textJS;
	const char *textUTF;
	size_t len;
	const char *cursorStr;
	int cursor;

	if( convStart ) {
		*convStart = 0;
	}
	if( convLen ) {
		*convLen = 0;
	}

	if( !strSize ) {
		str = NULL;
	}

	if( !getComposingText ) {
		getComposingText = ( *env )->GetMethodID( env, sys_android_activityClass, "getComposingText", "()Ljava/lang/String;" );
		getComposingCursor = ( *env )->GetMethodID( env, sys_android_activityClass, "getComposingCursor", "()I" );
	}

	textJS = ( *env )->CallObjectMethod( env, sys_android_app->activity->clazz, getComposingText );
	if( !textJS ) {
		if( str ) {
			str[0] = '\0';
		}
		if( cursorPos ) {
			*cursorPos = 0;
		}
		return 0;
	}

	textUTF = ( *env )->GetStringUTFChars( env, textJS, NULL );
	if( str ) {
		Q_strncpyz( str, textUTF, strSize );
		Q_FixTruncatedUtf8( str );
		len = strlen( str );
		cursorStr = str;
	} else {
		len = strlen( textUTF );
		cursorStr = textUTF;
	}

	if( cursorPos ) {
		int cursor = ( *env )->CallIntMethod( env, sys_android_app->activity->clazz, getComposingCursor );
		const char *oldCursorStr = cursorStr;
		while( ( cursor > 0 ) && *cursorStr ) {
			cursor--;
			cursorStr += Q_Utf8SyncPos( cursorStr, 1, UTF8SYNC_RIGHT );
		}
		*cursorPos = cursorStr - oldCursorStr;
	}

	( *env )->ReleaseStringUTFChars( env, textJS, textUTF );
	( *env )->DeleteLocalRef( env, textJS );

	return len;
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

/*
* IN_Android_ShowSoftKeyboard
*/
static void IN_Android_ShowSoftKeyboard( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID showSoftKeyboard;

	if( !showSoftKeyboard ) {
		showSoftKeyboard = ( *env )->GetMethodID( env, sys_android_activityClass, "showSoftKeyboard", "(Z)V" );
	}

	return ( *env )->CallVoidMethod( env, sys_android_app->activity->clazz, showSoftKeyboard, in_android_ime_enabled );
}

/*
* IN_Android_HideSoftKeyboard
*/
static bool IN_Android_HideSoftKeyboard( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID hideSoftKeyboard;

	if( !hideSoftKeyboard ) {
		hideSoftKeyboard = ( *env )->GetMethodID( env, sys_android_activityClass, "hideSoftKeyboard", "()Z" );
	}

	return ( *env )->CallBooleanMethod( env, sys_android_app->activity->clazz, hideSoftKeyboard );
}
