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
#import <AppKit/NSScreen.h>

static int VID_WndProc( void *wnd, int ev, int p1, int p2 )
{
	return 0;
}

/*
 * VID_Sys_Init
 */
int VID_Sys_Init( int x, int y, int width, int height, int displayFrequency, 
	void *parentWindow, qboolean fullScreen, qboolean wideScreen, qboolean verbose )
{
	return R_Init( APPLICATION, APP_SCREENSHOTS_PREFIX,
				NULL, NULL, parentWindow, 
                x, y, width, height, displayFrequency,
				fullScreen, wideScreen, verbose );
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
 ** VID_FlashWindow
 *
 * Sends a flash message to inactive window
 */
void VID_FlashWindow( int count )
{
}

/*
 ** VID_GetScreenSize
 */
qboolean VID_GetScreenSize( int *width, int *height )
{
  NSScreen* screen = [NSScreen mainScreen];
  if (screen)
  {
    NSRect rect = [screen frame];
    *width = floor(rect.size.width);
    *height = floor(rect.size.height);
    return qtrue;
  }

	return qfalse;
}

/*
 ** VID_NewWindow
 */
void VID_NewWindow( int width, int height )
{
	viddef.width  = width;
	viddef.height = height;
}
