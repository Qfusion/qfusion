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

x11display_t x11display;

static int VID_WndProc( x11display_t *wnd, int ev, int p1, int p2 )
{
	x11display = *wnd;
	return 0;
}

/*
* VID_Sys_Init
*/
rserr_t VID_Sys_Init( const char *applicationName, const char *screenshotsPrefix, int startupColor, 
	const int *iconXPM, void *parentWindow, bool verbose )
{
	x11display.dpy = NULL;

	return re.Init( applicationName, screenshotsPrefix, startupColor, 0, iconXPM,
		NULL, &VID_WndProc, parentWindow, verbose );
}

/*
* VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize( int x, int y )
{
}

/*
* VID_EnableAltTab
*/
void VID_EnableAltTab( bool enable )
{
}

/*
* VID_GetWindowHandle - The sound module may require the handle when using Window's directsound
*/
void *VID_GetWindowHandle( void )
{
	return ( void * )NULL;
}

/*
* VID_EnableWinKeys
*/
void VID_EnableWinKeys( bool enable )
{
}

/*
* _NET_WM_STATE_DEMANDS_ATTENTION
*
* Tell Window-Manager that application demands user attention
*/
static void _NET_WM_STATE_DEMANDS_ATTENTION( void )
{
	XEvent xev;
	Atom wm_state;
	Atom wm_demandsAttention;

	if( !x11display.dpy ) {
		return;
	}

	wm_state = XInternAtom( x11display.dpy, "_NET_WM_STATE", False );
	wm_demandsAttention = XInternAtom( x11display.dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", False );

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = x11display.win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = wm_demandsAttention;

	XSendEvent( x11display.dpy, DefaultRootWindow( x11display.dpy ), False,
		SubstructureNotifyMask, &xev );
}

/*
** VID_FlashWindow
*
* Sends a flash message to inactive window
*/
void VID_FlashWindow( int count )
{
	if( x11display.dpy ) {
		_NET_WM_STATE_DEMANDS_ATTENTION();
	}
}

/*
** VID_GetSysModes
*/
unsigned int VID_GetSysModes( vidmode_t *modes )
{
	XRRScreenConfiguration *xrrConfig;
	XRRScreenSize *xrrSizes;
	Display *dpy;
	Window root;
	int num_sizes = 0, i;

	dpy = XOpenDisplay( NULL );
	if( dpy )
	{
		root = DefaultRootWindow( dpy );
		xrrConfig = XRRGetScreenInfo( dpy, root );
		xrrSizes = XRRConfigSizes( xrrConfig, &num_sizes );

		if( modes )
		{
			for( i = 0; i < num_sizes; i++ )
			{
				modes[i].width = xrrSizes[i].width;
				modes[i].height = xrrSizes[i].height;
			}
		}

		XCloseDisplay( dpy );
	}

	return max( num_sizes, 0 );
}

/*
** VID_GetDefaultMode
*/
bool VID_GetDefaultMode( int *width, int *height )
{
	XRRScreenConfiguration *xrrConfig;
	XRRScreenSize *xrrSizes;
	Display *dpy;
	Window root;
	Rotation rotation;
	SizeID size_id;
	int num_sizes;

	dpy = XOpenDisplay( NULL );
	if( dpy )
	{
		root = DefaultRootWindow( dpy );
		xrrConfig = XRRGetScreenInfo( dpy, root );
		xrrSizes = XRRConfigSizes( xrrConfig, &num_sizes );
		size_id = XRRConfigCurrentConfiguration( xrrConfig, &rotation );

		*width = xrrSizes[size_id].width;
		*height = xrrSizes[size_id].height;

		XCloseDisplay( dpy );
		return true;
	}

	return false;
}

/*
** VID_GetPixelRatio
*/
float VID_GetPixelRatio( void )
{
	return 1.0f;
}
