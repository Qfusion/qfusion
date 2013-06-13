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

static x11display_t *display;

static int VID_WndProc( void *wnd, int ev, int p1, int p2 )
{
	display = wnd;
	return 0;
}

/*
* VID_Sys_Init
*/
int VID_Sys_Init( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen, qboolean verbose )
{
	extern cvar_t *vid_parentwid;

	display = NULL;

	return R_Init( NULL, NULL, (void *)(strtol( vid_parentwid->string, NULL, 0 )), 
		x, y, width, height, fullScreen, wideScreen, verbose );
}

/*
* VID_Front_f
*/
void VID_Front_f( void )
{
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
void VID_EnableAltTab( qboolean enable )
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
void VID_EnableWinKeys( qboolean enable )
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

	wm_state = XInternAtom( display->dpy, "_NET_WM_STATE", False );
	wm_demandsAttention = XInternAtom( display->dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", False );

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = display->win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = wm_demandsAttention;

	XSendEvent( display->dpy, DefaultRootWindow( display->dpy ), False,
		SubstructureNotifyMask, &xev );
}

/*
** VID_FlashWindow
*
* Sends a flash message to inactive window
*/
void VID_FlashWindow( int count )
{
	if( display ) {
		_NET_WM_STATE_DEMANDS_ATTENTION();
	}
}

/*
** VID_GetScreenSize
*/
qboolean VID_GetScreenSize( int *width, int *height )
{
	Display *dpy = XOpenDisplay( NULL );
	if( dpy ) {
		int scr = DefaultScreen( dpy );

		*width = DisplayWidth( dpy, scr );
		*height = DisplayHeight( dpy, scr );
		
		XCloseDisplay( dpy );

		return qtrue;
	}

	return qfalse;
}
