#include "../client/client.h"
#include "x11.h"

extern Atom XA_utf8_string;
extern char* clip_data;

/*
* Sys_GetClipboardData
*
* Orginally from EzQuake
*/
char *Sys_GetClipboardData( bool primary )
{
	Window win;
	Atom type;
	int format, ret;
	unsigned long nitems, bytes_after, bytes_left;
	unsigned char *data;
	char *buffer;
	Atom atom;

	if( !x11display.dpy )
		return NULL;

	if( primary )
	{
		atom = XInternAtom( x11display.dpy, "PRIMARY", True );
	}
	else
	{
		atom = XInternAtom( x11display.dpy, "CLIPBOARD", True );
	}
	if( atom == None )
		return NULL;

	win = XGetSelectionOwner( x11display.dpy, atom );
	if( win == None )
		return NULL;

	XConvertSelection( x11display.dpy, atom, XA_utf8_string, atom, win, CurrentTime );
	XFlush( x11display.dpy );

	XGetWindowProperty( x11display.dpy, win, atom, 0, 0, False, AnyPropertyType, &type, &format, &nitems, &bytes_left,
		&data );
	if( bytes_left <= 0 )
		return NULL;

	ret = XGetWindowProperty( x11display.dpy, win, atom, 0, bytes_left, False, AnyPropertyType, &type,
		&format, &nitems, &bytes_after, &data );
	if( ret == Success )
	{
		buffer = Q_malloc( bytes_left + 1 );
		memcpy( buffer, data, bytes_left + 1 );
	}
	else
	{
		buffer = NULL;
	}

	XFree( data );

	return buffer;
}

/*
* Sys_SetClipboardData
* Adapted from GLFW - x11_clipboard:_glfwPlatformSetClipboardString
* See: https://github.com/glfw/glfw/blob/master/src/x11_clipboard.c
*
* @param e The XEvent of the request
* @returns The proterty Atom for the appropriate response
*/
bool Sys_SetClipboardData( const char *data )
{
	// Save the message
	Q_free( clip_data );
	clip_data = Q_malloc( strlen( data ) - 1 );
	memcpy( clip_data, data, strlen( data ) - 1 );
	
	// Requesting clipboard ownership
	Atom XA_CLIPBOARD = XInternAtom( x11display.dpy, "CLIPBOARD", True );
	if( XA_CLIPBOARD == None )
		return false;

	XSetSelectionOwner( x11display.dpy, XA_CLIPBOARD, x11display.win, CurrentTime );

	// Check if we got ownership
	if( XGetSelectionOwner( x11display.dpy, XA_CLIPBOARD ) == x11display.win )
		return true;

	return false;
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	Q_free( data );
}