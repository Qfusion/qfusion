/*
Copyright (C) 2014 SiPlus, Chasseur de bots

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
#include <sys/system_properties.h>

void VID_EnableAltTab( bool enable )
{
}

void VID_EnableWinKeys( bool enable )
{
}

void VID_FlashWindow( int count )
{
}

void VID_Front_f( void )
{
}

bool VID_GetDisplaySize( int *width, int *height )
{
	ANativeWindow *window = sys_android_app->window;
	if( !window )
		return false;

	*width = ANativeWindow_getWidth( window );
	*height = ANativeWindow_getHeight( window );
	return true;
}

void *VID_GetWindowHandle( void )
{
	return NULL;
}

void VID_UpdateWindowPosAndSize( int x, int y )
{
}

float VID_GetPixelRatio( void )
{
	static float density;
	int width = 0, height = 0;

	if( !density )
	{
		const prop_info *densityPropInfo = __system_property_find( "ro.sf.lcd_density" );
		if( densityPropInfo )
		{
			char densityProp[PROP_VALUE_MAX];
			__system_property_read( densityPropInfo, NULL, densityProp );
			density = atoi( densityProp ) * ( 1.0f / 160.0f );
		}
	}

	if( density <= 0.0f )
		return 1.0f;

	VID_GetDisplaySize( &width, &height );

	if( !viddef.height || !height )
		return 1.0f;

	height /= density;
	clamp_low( height, 576 );

	return viddef.height / ( float )height;
}

rserr_t VID_Sys_Init( int x, int y, int width, int height, int displayFrequency,
	void *parentWindow, bool fullScreen, bool wideScreen, bool verbose )
{
	return re.Init( APPLICATION, APP_SCREENSHOTS_PREFIX, APP_STARTUP_COLOR,
		sys_android_app->activity, NULL, sys_android_app->window,
		x, y, width, height, displayFrequency,
		fullScreen, wideScreen, verbose );
}
