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

#include "../client/client.h"
#include "x11.h"
#include "keysym2ucs.h"
#include <sys/time.h>
#include <unistd.h>
#include "../sdl/sdl_input_joy.h"

// Vic: transplanted the XInput2 code from jquake

// TODO: add in_mouse?
cvar_t *in_grabinconsole;

static bool focus = false;
static bool minimized = false;

static bool input_inited = false;
static bool mouse_active = false;
static bool input_active = false;

static int xi_opcode;

static int mx, my;

Atom XA_TARGETS, XA_text, XA_utf8_string;

char* clip_data;

int Sys_XTimeToSysTime( unsigned long xtime );

//============================================

/*
* Sys_SendClipboardData
* Adapted from GLFW - x11_clipboard:writeTargetToProperty
* See: https://github.com/glfw/glfw/blob/master/src/x11_clipboard.c
*
* @param e The XEvent of the request
* @returns The proterty Atom for the appropriate response
*/
static Atom Sys_ClipboardProperty( XSelectionRequestEvent* request )
{
	int i;
	const Atom formats[] = {
		XA_utf8_string,
		XA_text,
		XA_STRING };
	const int formatCount = sizeof( formats ) / sizeof( formats[0] );

	// Legacy client
	if( request->property == None )
		return None;

	// Requested list of available datatypes
	if( request->target == XA_TARGETS )
	{
		const Atom targets[] = {
			XA_TARGETS,
			XA_utf8_string,
			XA_text,
			XA_STRING };

		XChangeProperty(
			x11display.dpy,
			request->requestor,
			request->property,
			XA_ATOM,
			32,
			PropModeReplace,
			(unsigned char*)targets,
			sizeof( targets ) / sizeof( targets[0] ) );

		return request->property;
	}

	// Requested a data type
	for( i = 0; i < formatCount; i++ )
	{
		if( request->target == formats[i] )
		{
			// Requested a supported type
			XChangeProperty(
				x11display.dpy,
				request->requestor,
				request->property,
				request->target,
				8,
				PropModeReplace,
				(unsigned char*)clip_data,
				strlen( clip_data ) );

			return request->property;
		}
	}

	// Not supported
	return None;
}

/**
* XPending() actually performs a blocking read if no events available. From Fakk2, by way of
* Heretic2, by way of SDL, original idea GGI project. The benefit of this approach over the quite
* badly behaved XAutoRepeatOn/Off is that you get focus handling for free, which is a major win
* with debug and windowed mode. It rests on the assumption that the X server will use the same
* timestamp on press/release event pairs for key repeats.
*/
static bool X11_PendingInput( void )
{
	assert( x11display.dpy );

	// Flush the display connection and look to see if events are queued
	XFlush( x11display.dpy );
	if( XEventsQueued( x11display.dpy, QueuedAlready ) )
		return true;

	{ // More drastic measures are required -- see if X is ready to talk
		static struct timeval zero_time;
		int x11_fd;
		fd_set fdset;

		x11_fd = ConnectionNumber( x11display.dpy );
		FD_ZERO( &fdset );
		FD_SET( x11_fd, &fdset );
		if( select( x11_fd+1, &fdset, NULL, NULL, &zero_time ) == 1 )
			return ( XPending( x11display.dpy ) );
	}

	// Oh well, nothing is ready ..
	return false;
}

static bool repeated_press( XEvent *event )
{
	XEvent peekevent;
	bool repeated = false;

	assert( x11display.dpy );

	if( X11_PendingInput() )
	{
		XPeekEvent( x11display.dpy, &peekevent );
		if( ( peekevent.type == KeyPress ) &&
			( peekevent.xkey.keycode == event->xkey.keycode ) &&
			( peekevent.xkey.time == event->xkey.time ) )
		{
			repeated = true;
			// we only skip the KeyRelease event, so we send many key down events, but no releases, while repeating
			//XNextEvent(x11display.dpy, &peekevent);  // skip event.
		}
	}
	return repeated;
}


/*****************************************************************************/

static Cursor CreateNullCursor( Display *display, Window root )
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap( display, root, 1, 1, 1 /*depth*/ );
	xgc.function = GXclear;
	gc = XCreateGC( display, cursormask, GCFunction, &xgc );
	XFillRectangle( display, cursormask, gc, 0, 0, 1, 1 );

	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;

	cursor = XCreatePixmapCursor( display, cursormask, cursormask, &dummycolour, &dummycolour, 0, 0 );
	XFreePixmap( display, cursormask );
	XFreeGC( display, gc );

	return cursor;
}

static void install_grabs_mouse( void )
{
	int i;
	int num_devices;
	XIDeviceInfo *info;
	XIEventMask mask;

	assert( x11display.dpy && x11display.win );

	if( mouse_active )
		return;

	XDefineCursor(x11display.dpy, x11display.win, CreateNullCursor(x11display.dpy, x11display.win));

	mask.deviceid = XIAllDevices;
	mask.mask_len = XIMaskLen(XI_LASTEVENT);
	mask.mask = calloc(mask.mask_len, sizeof(char));
	XISetMask(mask.mask, XI_Enter);
	XISetMask(mask.mask, XI_Leave);
	XISetMask(mask.mask, XI_ButtonPress);
	XISetMask(mask.mask, XI_ButtonRelease);

	info = XIQueryDevice(x11display.dpy, XIAllDevices, &num_devices);
	for(i = 0; i < num_devices; i++) {
		int id = info[i].deviceid;
		if(info[i].use == XISlavePointer) {
			mask.deviceid = id;
			XIGrabDevice(x11display.dpy, id, x11display.win, CurrentTime, None, GrabModeSync,
				GrabModeSync, True, &mask);
		}
		else if(info[i].use == XIMasterPointer) {
			if (x11display.features.wmStateFullscreen)
				XIWarpPointer(x11display.dpy, id, None, x11display.win, 0, 0, 0, 0, 0, 0);
			else
				XIWarpPointer(x11display.dpy, id, None, x11display.win, 0, 0, 0, 0, x11display.win_width/2, x11display.win_height/2);
		}
	}
	XIFreeDeviceInfo(info);

	mask.deviceid = XIAllDevices;
	memset(mask.mask, 0, mask.mask_len);
	XISetMask(mask.mask, XI_RawMotion);

	XISelectEvents(x11display.dpy, DefaultRootWindow(x11display.dpy), &mask, 1);

	free(mask.mask);

	XSync(x11display.dpy, True);

	mx = my = 0;
	mouse_active = true;
}

static void uninstall_grabs_mouse( void )
{
	int i;
	int num_devices;
	XIDeviceInfo *info;

	assert( x11display.dpy && x11display.win );

	if( !mouse_active )
		return;

	XUndefineCursor(x11display.dpy, x11display.win);

	info = XIQueryDevice(x11display.dpy, XIAllDevices, &num_devices);

	for(i = 0; i < num_devices; i++) {
		if(info[i].use == XIFloatingSlave) {
			XIUngrabDevice(x11display.dpy, info[i].deviceid, CurrentTime);
		}
		else if(info[i].use == XIMasterPointer) {
			XIWarpPointer(x11display.dpy, info[i].deviceid, None, x11display.win, 0, 0, 0, 0,
				x11display.win_width/2, x11display.win_height/2);
		}
	}
	XIFreeDeviceInfo(info);

	mouse_active = false;
	mx = my = 0;
}

static void install_grabs_keyboard( void )
{
	int res;
	int fevent;

	assert( x11display.dpy && x11display.win );

	if( input_active )
		return;

	if( !x11display.features.wmStateFullscreen ) {
		res = XGrabKeyboard( x11display.dpy, x11display.win, False, GrabModeAsync, GrabModeAsync, CurrentTime );
		if( res != GrabSuccess ) {
			Com_Printf( "Warning: XGrabKeyboard failed\n" );
			return;
		}
	}

	// init X Input method, needed by Xutf8LookupString
	x11display.im = XOpenIM( x11display.dpy, NULL, NULL, NULL );
	x11display.ic = XCreateIC( x11display.im,
		XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
		XNClientWindow, x11display.win,
		NULL );

	if( x11display.ic ) {
		XGetICValues( x11display.ic, XNFilterEvents, &fevent, NULL );
		XSelectInput( x11display.dpy, x11display.win, fevent | x11display.wa.event_mask );
	}

	input_active = true;
}

static void uninstall_grabs_keyboard( void )
{
	assert( x11display.dpy && x11display.win );

	if( !input_active )
		return;

	XUngrabKeyboard( x11display.dpy, CurrentTime );

	x11display.ic = 0;

	input_active = false;
}

// Q3 version
static int XLateKey( KeySym keysym )
{
	switch(keysym) {
		case XK_Scroll_Lock: return K_SCROLLLOCK;
		case XK_Caps_Lock: return K_CAPSLOCK;
		case XK_Num_Lock: return K_NUMLOCK;
		case XK_KP_Page_Up: case XK_KP_9: return KP_PGUP;
		case XK_Page_Up: return K_PGUP;
		case XK_KP_Page_Down: case XK_KP_3: return KP_PGDN;
		case XK_Page_Down: return K_PGDN;
		case XK_KP_Home: case XK_KP_7: return KP_HOME;
		case XK_Home: return K_HOME;
		case XK_KP_End: case XK_KP_1: return KP_END;
		case XK_End: return K_END;
		case XK_KP_Left: case XK_KP_4: return KP_LEFTARROW;
		case XK_Left: return K_LEFTARROW;
		case XK_KP_Right: case XK_KP_6: return KP_RIGHTARROW;
		case XK_Right: return K_RIGHTARROW;
		case XK_KP_Down: case XK_KP_2: return KP_DOWNARROW;
		case XK_Down: return K_DOWNARROW;
		case XK_KP_Up: case XK_KP_8: return KP_UPARROW;
		case XK_Up: return K_UPARROW;
		case XK_Escape: return K_ESCAPE;
		case XK_KP_Enter: return KP_ENTER;
		case XK_Return: return K_ENTER;
		case XK_Tab: return K_TAB;
		case XK_F1: return K_F1;
		case XK_F2: return K_F2;
		case XK_F3: return K_F3;
		case XK_F4: return K_F4;
		case XK_F5: return K_F5;
		case XK_F6: return K_F6;
		case XK_F7: return K_F7;
		case XK_F8: return K_F8;
		case XK_F9: return K_F9;
		case XK_F10: return K_F10;
		case XK_F11: return K_F11;
		case XK_F12: return K_F12;
		case XK_BackSpace: return K_BACKSPACE;
		case XK_KP_Delete: case XK_KP_Decimal: return KP_DEL;
		case XK_Delete: return K_DEL;
		case XK_Pause: return K_PAUSE;
		case XK_Shift_L: return K_LSHIFT;
		case XK_Shift_R: return K_RSHIFT;
		case XK_Execute:
		case XK_Control_L: return K_LCTRL;
		case XK_Control_R: return K_RCTRL;
		case XK_Alt_L:
		case XK_Meta_L: return K_LALT;
		case XK_ISO_Level3_Shift:
		case XK_Alt_R:
		case XK_Meta_R: return K_RALT;
		case XK_Super_L: return K_WIN;
		case XK_Super_R: return K_WIN;
		case XK_Multi_key: return K_WIN;
		case XK_Menu: return K_MENU;
		case XK_KP_Begin: case XK_KP_5: return KP_5;
		case XK_KP_Insert: case XK_KP_0: return KP_INS;
		case XK_Insert: return K_INS;
		case XK_KP_Multiply: return KP_STAR;
		case XK_KP_Add: return KP_PLUS;
		case XK_KP_Subtract: return KP_MINUS;
		case XK_KP_Divide: return KP_SLASH;
		default:
			if (keysym >= 32 && keysym <= 126) {
				return keysym;
			}
			break;
	}

	return 0;
}

/*
*_X11_CheckWMSTATE
*/
static void _X11_CheckWMSTATE( void )
{
#define WM_STATE_ELEMENTS 1
	unsigned long *property = NULL;
	unsigned long nitems;
	unsigned long leftover;
	Atom xa_WM_STATE, actual_type;
	int actual_format;
	int status;

	minimized = false;
	xa_WM_STATE = x11display.wmState;

	status = XGetWindowProperty ( x11display.dpy, x11display.win,
		xa_WM_STATE, 0L, WM_STATE_ELEMENTS,
		False, xa_WM_STATE, &actual_type, &actual_format,
		&nitems, &leftover, (unsigned char **)&property );

	if ( ! ( ( status == Success ) &&
		( actual_type == xa_WM_STATE ) &&
		( nitems == WM_STATE_ELEMENTS ) ) )
	{
		if ( property )
		{
			XFree ( (char *)property );
			property = NULL;
			return;
		}
	}

	if( ( *property == IconicState ) || ( *property == WithdrawnState ) )
		minimized = true;

	XFree( (char *)property );
}

static void handle_button(XGenericEventCookie *cookie)
{
	XIDeviceEvent *ev = (XIDeviceEvent *)cookie->data;
	bool down = cookie->evtype == XI_ButtonPress;
	int button = ev->detail;
	unsigned time = Sys_XTimeToSysTime(ev->time);
	int k_button;

	if(!mouse_active)
		return;

	switch(button) {
		case 1: k_button = K_MOUSE1; break;
		case 2: k_button = K_MOUSE3; break;
		case 3: k_button = K_MOUSE2; break;
		case 4: k_button = K_MWHEELUP; break;
		case 5: k_button = K_MWHEELDOWN; break;
			/* Switch place of MOUSE4-5 with MOUSE6-7 */
		case 6: k_button = K_MOUSE6; break;
		case 7: k_button = K_MOUSE7; break;
		case 8: k_button = K_MOUSE4; break;
		case 9: k_button = K_MOUSE5; break;
			/* End switch */
		case 10: k_button = K_MOUSE8; break;
		default: return;
	}

	Key_Event(k_button, down, time);
}

static void handle_key(XEvent *event)
{
	bool down = event->type == KeyPress;
	XKeyEvent *kevent = &event->xkey;
	unsigned time = Sys_XTimeToSysTime(event->xkey.time);
	KeySym keysym;
	int key;
	int XLookupRet;
	char buf[64];

	if( !down && repeated_press( event ) ) {
		return; // don't send release events when repeating
	}

	memset( buf, 0, sizeof buf ); // XLookupString doesn't zero-terminate the buffer
	XLookupRet = 0;
#ifdef X_HAVE_UTF8_STRING
	if( x11display.ic )
		XLookupRet = Xutf8LookupString( x11display.ic, kevent, buf, sizeof buf, &keysym, 0 );
#endif
	if( !XLookupRet )
		XLookupRet = XLookupString( kevent, buf, sizeof buf, &keysym, 0 );

	// get keysym without modifiers, so that movement works when e.g. a cyrillic layout is selected
	kevent->state = 0;
	keysym = XLookupKeysym( kevent, 0 );
	key = XLateKey( keysym );

	Key_Event( key, down, time );

	if( down )
	{
		const char *p;
		for( p = buf; *p; ) {
			wchar_t wc = Q_GrabWCharFromUtf8String( (const char **)&p );
			Key_CharEvent( key, wc );
		}
	}
}

static void handle_raw_motion(XIRawEvent *ev)
{
	double *raw_valuator = ev->raw_values;

	if(!mouse_active)
		return;

	if(XIMaskIsSet(ev->valuators.mask, 0)) {
		mx += *raw_valuator++;
	}

	if(XIMaskIsSet(ev->valuators.mask, 1)) {
		my += *raw_valuator++;
	}

}

static void handle_cookie(XGenericEventCookie *cookie)
{
	switch(cookie->evtype) {
	case XI_RawMotion:
		handle_raw_motion(cookie->data);
		break;
	case XI_Enter:
	case XI_Leave:
		break;
	case XI_ButtonPress:
	case XI_ButtonRelease:
		handle_button(cookie);
		break;
	default:
		break;
	}
}

static void HandleEvents( void )
{
	XEvent event, response;
	XSelectionRequestEvent* request;

	assert( x11display.dpy && x11display.win );

	while( XPending( x11display.dpy ) )
	{
		XGenericEventCookie *cookie = &event.xcookie;
		XNextEvent( x11display.dpy, &event );

		if( cookie->type == GenericEvent && cookie->extension == xi_opcode 
			&& XGetEventData( x11display.dpy, cookie ) ) {
				handle_cookie( cookie );
				XFreeEventData( x11display.dpy, cookie );
				continue;
		}

		switch( event.type )
		{
		case KeyPress:
		case KeyRelease:
			handle_key( &event );
			break;
		case FocusIn:
			if( event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ) {
				// Someone is handling a global hotkey, ignore it
				continue;
			}
			if( !focus )
			{
				focus = true;
				install_grabs_keyboard();
			}
			break;

		case FocusOut:
			if( event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab ) {
				// Someone is handling a global hotkey, ignore it
				continue;
			}
			if( focus )
			{
				if ( Cvar_Value( "vid_fullscreen" ) ) {
					XIconifyWindow( x11display.dpy, x11display.win, x11display.scr );
				}
				uninstall_grabs_keyboard();
				IN_ClearState();
				focus = false;
			}
			break;

		case ClientMessage:
			if( event.xclient.data.l[0] == x11display.wmDeleteWindow )
				Cbuf_ExecuteText( EXEC_NOW, "quit" );
			break;

		case ConfigureNotify:
			VID_AppActivate( true, false );
			break;

		case PropertyNotify:
			if( event.xproperty.window == x11display.win )
			{
				if ( event.xproperty.atom == x11display.wmState )
				{
					bool was_minimized = minimized;

					_X11_CheckWMSTATE();

					if( minimized != was_minimized )
					{
						// FIXME: find a better place for this?..
						SCR_PauseCinematic( minimized );
						CL_SoundModule_Activate( !minimized );
					}
				}
			}
			break;

		case SelectionClear:
			// Another app took clipboard ownership away
			// There's not actually anything we need to do here
			break;

		case SelectionRequest:
			// Another app is requesting clipboard information
			request = &event.xselectionrequest;

			memset( &response, 0, sizeof( response ) );
			response.xselection.type = SelectionNotify;
			response.xselection.display = request->display;
			response.xselection.requestor = request->requestor;
			response.xselection.selection = request->selection;
			response.xselection.target = request->target;
			response.xselection.time = request->time;
			response.xselection.property = Sys_ClipboardProperty( request );

			// Send the response
			XSendEvent( x11display.dpy, request->requestor, 0, 0, &response );
			break;
		}
	}
}

/*****************************************************************************/

void IN_Commands( void )
{
	IN_SDL_JoyCommands();
}

void IN_MouseMove( usercmd_t *cmd )
{
	if( mouse_active )
	{
		CL_MouseMove( cmd, mx, my );
		mx = my = 0;
	}
}

static void IN_Activate( bool active )
{
	if( !input_inited )
		return;
	if( mouse_active == active )
		return;

	assert( x11display.dpy && x11display.win );

	if( active )
	{
		install_grabs_mouse();
		install_grabs_keyboard();
	}
	else
	{
		uninstall_grabs_mouse();
		uninstall_grabs_keyboard();
	}

	IN_SDL_JoyActivate( active );
}


void IN_Init( void )
{
	int event, error;
	int xi2_major = 2;
	int xi2_minor = 0;

	if( input_inited )
		return;

	in_grabinconsole = Cvar_Get( "in_grabinconsole", "0", CVAR_ARCHIVE );

	input_active = false;
	mouse_active = false;

	if( !XQueryExtension( x11display.dpy, "XInputExtension", &xi_opcode, &event, &error ) ) {
		Com_Printf( "ERROR: XInput Extension not available.\n" );
		return;
	}
	if( XIQueryVersion( x11display.dpy, &xi2_major, &xi2_minor ) == BadRequest ) {
		Com_Printf( "ERROR: Can't initialize XInput2. Server supports %d.%d\n", xi2_major, xi2_minor );
		return;
	}

	Com_Printf( "Successfully initialized XInput2 %d.%d\n", xi2_major, xi2_minor );

	focus = true;
	input_inited = true;
	install_grabs_keyboard();
	install_grabs_mouse();
	IN_SDL_JoyInit( mouse_active );

	XA_TARGETS = XInternAtom( x11display.dpy, "TARGETS", 0 );
	XA_text = XInternAtom( x11display.dpy, "TEXT", 0 );
	XA_utf8_string = XInternAtom( x11display.dpy, "UTF8_STRING", 0 );
}

void IN_Shutdown( void )
{
	if( !input_inited )
		return;

	uninstall_grabs_keyboard();
	uninstall_grabs_mouse();
	IN_SDL_JoyShutdown();
	input_inited = false;
	focus = false;
}

void IN_Restart( void )
{
	IN_Shutdown();
	IN_Init();
}

void IN_Frame( void )
{
	bool m_active = false;

	if( !input_inited )
		return;

	HandleEvents();

	if( focus ) {
		if( !Cvar_Value( "vid_fullscreen" ) && ( ( cls.key_dest == key_console ) && !in_grabinconsole->integer ) )
		{
			m_active = false;
		}
		else
		{
			m_active = true;
		}
	}

	IN_Activate( m_active );
}

unsigned int IN_SupportedDevices( void )
{
	return IN_DEVICE_KEYBOARD | IN_DEVICE_MOUSE | IN_DEVICE_JOYSTICK;
}

void IN_ShowSoftKeyboard( bool show )
{
}

void IN_GetInputLanguage( char *dest, size_t size )
{
	if( size )
		dest[0] = '\0';
	// TODO: Implement using Xkb.
}

// TODO: IBus IME.

void IN_IME_Enable( bool enable )
{
}

size_t IN_IME_GetComposition( char *str, size_t strSize, size_t *cursorPos, size_t *convStart, size_t *convLen )
{
	if( str && strSize )
		str[0] = '\0';
	if( cursorPos )
		*cursorPos = 0;
	if( convStart )
		*convStart = 0;
	if( convLen )
		*convLen = 0;
	return 0;
}

unsigned int IN_IME_GetCandidates( char * const *cands, size_t candSize, unsigned int maxCands, int *selected, int *firstKey )
{
	if( selected )
		*selected = -1;
	if( firstKey )
		*firstKey = 1;
	return 0;
}
