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
#include <SDL.h>

static int VID_WndProc( void *wnd, int ev, int p1, int p2 )
{
	return 0;
}

/*
 * VID_Sys_Init
 */
int VID_Sys_Init( int x, int y, int width, int height, int displayFrequency, void *parentWindow, bool fullScreen, bool wideScreen, bool verbose )
{
	return re.Init( APPLICATION, APP_SCREENSHOTS_PREFIX, APP_STARTUP_COLOR, NULL, NULL, parentWindow, x, y, width, height, displayFrequency, fullScreen, wideScreen, verbose );
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
void VID_EnableAltTab( bool enable )
{
}

/*
 * VID_GetWindowHandle - The sound module may require the handle when using Window's directsound
 */
void *VID_GetWindowHandle( void )
{
	return (void *)NULL;
}

/*
 * VID_EnableWinKeys
 */
void VID_EnableWinKeys( bool enable )
{
}

/*
 * VID_FlashWindow
 *
 * Sends a flash message to inactive window
 */
void VID_FlashWindow( int count )
{
}

/*
 * VID_GetDisplaySize
 */
bool VID_GetDisplaySize( int *width, int *height )
{
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( 0, &mode );

	*width = mode.w;
	*height = mode.h;

	return true;
}

/*
 * VID_GetPixelRatio
 */
float VID_GetPixelRatio( void )
{
	return 1.0f; // TODO: check if retina?
}
