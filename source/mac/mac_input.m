#import <AppKit/AppKit.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hidsystem/IOHIDParameter.h>
#import <IOKit/hidsystem/event_status_driver.h>

#include <SDL/SDL.h>
#include "../client/client.h"

cvar_t *in_grabinconsole;

static qboolean input_inited = qfalse;
static qboolean mouse_active = qfalse;
static qboolean input_active = qfalse;

static int mx, my;

#pragma mark Mouse Accel Fix

cvar_t *in_disablemacosxmouseaccel;

io_connect_t IN_GetIOHandle (void)
{
    io_connect_t 	myHandle = MACH_PORT_NULL;
    kern_return_t	myStatus;
    io_service_t	myService = MACH_PORT_NULL;
    mach_port_t		myMasterPort;
	
    myStatus = IOMasterPort (MACH_PORT_NULL, &myMasterPort );
	
    if (myStatus != KERN_SUCCESS)
    {
        return (0);
    }
	
    myService = IORegistryEntryFromPath (myMasterPort, kIOServicePlane ":/IOResources/IOHIDSystem");
	
    if (myService == 0)
    {
        return (0);
    }
	
    myStatus = IOServiceOpen (myService, mach_task_self (), kIOHIDParamConnectType, &myHandle);
    IOObjectRelease (myService);
	
    return (myHandle);
}

void IN_SetMouseScalingEnabled (BOOL isRestore)
{
	static double	myOldAcceleration		= 0.0;

	if(in_disablemacosxmouseaccel->integer)
	{
		io_connect_t mouseDev = IN_GetIOHandle();
		if(mouseDev != 0)
		{
			// if isRestore YES, restore old (set by system control panel) acceleration.
			if (isRestore == YES)
			{
				IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), myOldAcceleration);
			}
			else // otherwise, disable mouse acceleration. we won't disable trackpad acceleration.
			{
				if(IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &myOldAcceleration) == kIOReturnSuccess)
				{
//					Com_Printf("previous mouse acceleration: %f\n", myOldAcceleration);
					if(IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
					{
						Com_Printf("Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
						Cvar_Set ("in_disablemacosxmouseaccel", "0");
					}
				}
				else
				{
					Com_Printf("Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
					Cvar_Set ("in_disablemacosxmouseaccel", "0");
				}
			}
			IOServiceClose(mouseDev);
		}
		else
		{
			Com_Printf("Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
			Cvar_Set ("in_disablemacosxmouseaccel", "0");
		}
	}
}


void IN_Commands( void )
{
}
void IN_Activate( qboolean active )
{
}


/**
 * Function which is called whenever the mouse is moved.
 * @param ev the SDL event object containing the mouse position et all
 */
static void _mouse_motion_event( SDL_MouseMotionEvent *event )
{
	mx += event->xrel;
	my += event->yrel;
}


/**
 * Function which is called whenever a mouse button is pressed or released.
 * @param ev the SDL event object containing the button number et all
 * @param state either qtrue if it is a keydown event or qfalse otherwise
 */
static void _mouse_button_event( SDL_MouseButtonEvent *event, qboolean state )
{
	Uint8 button = event->button;
	if( button <= 5 )
	{
		switch( button )
		{
		case SDL_BUTTON_LEFT: Key_MouseEvent( K_MOUSE1, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_MIDDLE: Key_MouseEvent( K_MOUSE3, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_RIGHT: Key_MouseEvent( K_MOUSE2, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_WHEELUP: Key_Event( K_MWHEELUP, state, Sys_Milliseconds() ); break;
		case SDL_BUTTON_WHEELDOWN: Key_Event( K_MWHEELDOWN, state, Sys_Milliseconds() ); break;
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

static qwchar TranslateSDLKey(qwchar charkey)
{
	switch(charkey)
	{
		case SDLK_TAB:			charkey = K_TAB;		break;
		case SDLK_RETURN:		charkey = K_ENTER;		break;
		case SDLK_ESCAPE:		charkey = K_ESCAPE;		break;
		case SDLK_SPACE:		charkey = K_SPACE;		break;
		case SDLK_CAPSLOCK:		charkey = K_CAPSLOCK;	break;
		case SDLK_SCROLLOCK:	charkey = K_SCROLLLOCK;	break;
		case SDLK_NUMLOCK:		charkey = K_NUMLOCK;	break;
		case SDLK_BACKSPACE:	charkey = K_BACKSPACE;	break;
		case SDLK_UP:			charkey = K_UPARROW;	break;
		case SDLK_DOWN:			charkey = K_DOWNARROW;	break;
		case SDLK_LEFT:			charkey = K_LEFTARROW;	break;
		case SDLK_RIGHT:		charkey = K_RIGHTARROW;	break;
		case SDLK_LALT:
		case SDLK_RALT:			charkey = K_OPTION;		break;
		case SDLK_LCTRL:		charkey = K_LCTRL;		break;
		case SDLK_RCTRL:		charkey = K_RCTRL;		break;
		case SDLK_LSHIFT:		charkey = K_LSHIFT;		break;
		case SDLK_RSHIFT:		charkey = K_RSHIFT;		break;
		case SDLK_F1:			charkey = K_F1;			break;
		case SDLK_F2:			charkey = K_F2;			break;
		case SDLK_F3:			charkey = K_F3;			break;
		case SDLK_F4:			charkey = K_F4;			break;
		case SDLK_F5:			charkey = K_F5;			break;
		case SDLK_F6:			charkey = K_F6;			break;
		case SDLK_F7:			charkey = K_F7;			break;
		case SDLK_F8:			charkey = K_F8;			break;
		case SDLK_F9:			charkey = K_F9;			break;
		case SDLK_F10:			charkey = K_F10;		break;
		case SDLK_F11:			charkey = K_F11;		break;
		case SDLK_F12:			charkey = K_F12;		break;
		case SDLK_F13:			charkey = K_F13;		break;
		case SDLK_F14:			charkey = K_F14;		break;
		case SDLK_F15:			charkey = K_F15;		break;
		case SDLK_INSERT:		charkey = K_INS;		break;
		case SDLK_DELETE:		charkey = K_BACKSPACE;	break;
		case SDLK_PAGEUP:		charkey = K_PGDN;		break;			
		case SDLK_PAGEDOWN:		charkey = K_PGUP;		break;
		case SDLK_HOME:			charkey = K_HOME;		break;
		case SDLK_END:			charkey = K_END;		break;	
		case SDLK_WORLD_0:		charkey = '~';			break;
		case SDLK_LMETA:
		case SDLK_RMETA:		charkey = K_COMMAND;	break;
	}
	return charkey;
}

/**
 * Function which is called whenever a key is pressed or released.
 * @param event the SDL event object containing the keysym et all
 * @param state either qtrue if it is a keydown event or qfalse otherwise
 */
static void _key_event( const SDL_KeyboardEvent *event, const qboolean state )
{	
	qwchar charkey = event->keysym.sym;
	charkey = TranslateSDLKey(charkey);
	
	if(charkey >= 0 && charkey <= 255) {
		Key_Event(charkey, state, Sys_Milliseconds());
		if(state == qtrue) {
			if( event->keysym.unicode > 0 && event->keysym.unicode <= 255) {
			charkey = event->keysym.unicode;
			charkey = TranslateSDLKey(charkey);
			if(charkey >= 33 && charkey == 127)
				Key_CharEvent(charkey, 0);
			else
				Key_CharEvent(charkey, charkey);
			}
		}
	}
	
}

/*****************************************************************************/

static void HandleEvents( void )
{
	SDL_Event event;

	while( SDL_PollEvent( &event ) )
	{
		//printf("Event: %u\n", event.type);
		switch( event.type )
		{
		case SDL_KEYDOWN:
			_key_event( &event.key, qtrue );
			break;

		case SDL_KEYUP:
			_key_event( &event.key, qfalse );
			break;

		case SDL_MOUSEMOTION:
			_mouse_motion_event( &event.motion );
			break;

		case SDL_MOUSEBUTTONDOWN:
			_mouse_button_event( &event.button, qtrue );
			break;

		case SDL_MOUSEBUTTONUP:
			_mouse_button_event( &event.button, qfalse );
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

void IN_JoyMove( usercmd_t *cmd )
{
}

void IN_Init()
{
	if( input_inited )
		return;
	
	in_grabinconsole = Cvar_Get( "in_grabinconsole", "0", CVAR_ARCHIVE );
	in_disablemacosxmouseaccel = Cvar_Get( "in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE );
	
	Com_Printf("Initializing SDL Input\n");
	
	SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
	Com_Printf("SDL_ShowCursor = %i", SDL_ShowCursor( SDL_QUERY ));
	SDL_ShowCursor( SDL_DISABLE );
	SDL_EnableUNICODE( SDL_ENABLE );
	SDL_WM_GrabInput( SDL_GRAB_ON );
	SDL_SetCursor( NULL );
	
	IN_SetMouseScalingEnabled(NO);

	input_inited = qtrue;
	input_active = qtrue; // will be activated by IN_Frame if necessary
}

/**
 * Shutdown input subsystem.
 */
void IN_Shutdown()
{
	if( !input_inited )
		return;

	Com_Printf("Shutdown SDL Input\n");
	
	IN_Activate( qfalse );
	input_inited = qfalse;
	SDL_EnableUNICODE( SDL_ENABLE );
	SDL_WM_GrabInput( SDL_GRAB_OFF );
	IN_SetMouseScalingEnabled(YES);
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
		mouse_active = qfalse;
		input_active = qtrue;
		if(SDL_ShowCursor( SDL_QUERY ) == SDL_DISABLE)
		{
			IN_SetMouseScalingEnabled(YES);
			SDL_WM_GrabInput( SDL_GRAB_OFF );
			SDL_ShowCursor( SDL_ENABLE );
		}
	}
	else
	{
		mouse_active = qtrue;
		input_active = qtrue;
		if(SDL_ShowCursor( SDL_QUERY ) == SDL_ENABLE)
		{
			IN_SetMouseScalingEnabled(NO);
			SDL_WM_GrabInput( SDL_GRAB_ON );
			SDL_ShowCursor( SDL_DISABLE );
		}
	}

	HandleEvents();
}
