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

void VID_EnableAltTab( bool enable ) {
}

void VID_EnableWinKeys( bool enable ) {
}

void VID_FlashWindow( int count ) {
}

bool VID_GetDefaultMode( int *width, int *height ) {
	int i, modeWidth, modeHeight, lastWidth = 0, lastHeight = 0;
	static int maxWidth;

	if( !maxWidth ) {
		JNIEnv *env = Sys_Android_GetJNIEnv();

		jstring name;
		jobject activityManager;
		jclass activityManagerClass;
		jmethodID getDeviceConfigurationInfo;

		jobject configurationInfo;
		jclass configurationInfoClass;
		jfieldID reqGlEsVersion;

		name = ( *env )->NewStringUTF( env, "activity" );
		activityManager = ( *env )->CallObjectMethod( env, sys_android_app->activity->clazz, sys_android_getSystemService, name );
		( *env )->DeleteLocalRef( env, name );
		activityManagerClass = ( *env )->FindClass( env, "android/app/ActivityManager" );
		getDeviceConfigurationInfo = ( *env )->GetMethodID( env, activityManagerClass,
															"getDeviceConfigurationInfo", "()Landroid/content/pm/ConfigurationInfo;" );
		( *env )->DeleteLocalRef( env, activityManagerClass );

		configurationInfo = ( *env )->CallObjectMethod( env, activityManager, getDeviceConfigurationInfo );
		( *env )->DeleteLocalRef( env, activityManager );
		configurationInfoClass = ( *env )->FindClass( env, "android/content/pm/ConfigurationInfo" );
		reqGlEsVersion = ( *env )->GetFieldID( env, configurationInfoClass, "reqGlEsVersion", "I" );
		( *env )->DeleteLocalRef( env, configurationInfoClass );

		// Use full HD as the default on GPUs that support OpenGL ES 3.1, created in 2014 or later, such as NVIDIA Tegra K1.
		maxWidth = ( ( ( *env )->GetIntField( env, configurationInfo, reqGlEsVersion ) >= 0x30001 ) ? 1920 : 1280 );
		( *env )->DeleteLocalRef( env, configurationInfo );
	}

	for( i = 0; VID_GetModeInfo( &modeWidth, &modeHeight, i ); i++ ) {
		if( modeWidth > maxWidth ) {
			break;
		}

		lastWidth = modeWidth;
		lastHeight = modeHeight;
	}

	assert( lastWidth && lastHeight ); // this function may be called only after vid mode initialization
	*width = lastWidth;
	*height = lastHeight;

	return true;
}

unsigned int VID_GetSysModes( vidmode_t *modes ) {
	ANativeWindow *window = sys_android_app->window;
	int windowWidth, windowHeight;
	int density;
	float densityStep;
	int i;

	if( !window ) {
		return 0;
	}

	windowWidth = ANativeWindow_getWidth( window );
	windowHeight = ANativeWindow_getHeight( window );
	if( windowHeight > windowWidth ) {
		// The window may be created in portrait orientation sometimes, for example, when launching in sleep mode.
		int tempWidth = windowWidth;
		windowWidth = windowHeight;
		windowHeight = tempWidth;
	}

	if( windowHeight <= 480 ) {
		if( modes ) {
			modes[0].width = windowWidth;
			modes[0].height = windowHeight;
		}
		return 1;
	}

	density = AConfiguration_getDensity( sys_android_app->config );
	if( !density ) {
		density = ACONFIGURATION_DENSITY_MEDIUM;
	}
	densityStep = 1.0f - ( float )( density - 40 ) / ( float )density;

	for( i = 0; ; i++ ) {
		float scale = 1.0f - densityStep * ( float )i;
		int width = windowWidth * scale;
		int height = windowHeight * scale;

		if( ( width <= 0 ) || ( height < 480 ) ) {
			break;
		}

		if( modes ) {
			modes[i].width = width;
			modes[i].height = height;
		}
	}

	return i;
}

void *VID_GetWindowHandle( void ) {
	return NULL;
}

void VID_UpdateWindowPosAndSize( int x, int y ) {
}

float VID_GetPixelRatio( void ) {
	static float invDensity;
	int height;
	int windowWidth, windowHeight;

	if( !sys_android_app->window || !viddef.height ) {
		return 1.0f;
	}

	if( !invDensity ) {
		float density = AConfiguration_getDensity( sys_android_app->config ) * ( 1.0f / ( float )ACONFIGURATION_DENSITY_MEDIUM );
		if( !density ) {
			density = 1.0f / ( float )ACONFIGURATION_DENSITY_MEDIUM;
		}
		invDensity = 1.0f / density;
	}

	windowWidth = ANativeWindow_getWidth( sys_android_app->window );
	windowHeight = ANativeWindow_getHeight( sys_android_app->window );
	return ( float )viddef.height / ( ( float )( min( windowWidth, windowHeight ) ) * invDensity );
}

rserr_t VID_Sys_Init( const char *applicationName, const char *screenshotsPrefix, int startupColor,
					  const int *iconXPM, bool verbose ) {
	ANativeWindow *window = sys_android_app->window;
	rserr_t res;

	res = re.Init( applicationName, screenshotsPrefix, startupColor,
				   0, iconXPM, NULL, NULL, window, VID_GetPixelRatio(),
				   verbose );

	if( res == rserr_ok ) {
		VID_AppActivate( window != NULL, window == NULL, false );
	}

	return res;
}

void VID_Android_SetWindow( ANativeWindow *window ) {
	re.SetWindow( NULL, NULL, window );
	VID_AppActivate( window != NULL, window == NULL, false );
}
