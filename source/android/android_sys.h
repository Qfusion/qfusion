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

#ifndef ANDROID_SYS_H
#define ANDROID_SYS_H

#include "../qcommon/qcommon.h"
#ifndef DEDICATED_ONLY
#include "../client/client.h"
#endif
#include <android_native_app_glue.h>

/**
 * Custom looper data IDs.
 */
enum
{
	LOOPER_ID_USER_CLIENT = LOOPER_ID_USER
};

/**
 * A pointer to the Native App Glue app.
 */
extern struct android_app *sys_android_app;

/**
 * The root of the app's internal storage.
 */
extern char sys_android_internalDataPath[];

/**
 * A global reference to the class of the engine activity.
 */
extern jclass sys_android_activityClass;

/**
 * Commonly used JNI methods.
 */
extern jmethodID sys_android_getSystemService;

/**
 * The package name of the game.
 */
extern char sys_android_packageName[];

/**
 * Returns the JNI environment for the current thread.
 * Attaches the Java VM to the current thread if needed.
 * The VM is detached when the thread is finished.
 *
 * @return the JNI environment
 */
JNIEnv *Sys_Android_GetJNIEnv( void );

/**
 * Returns the time since the boot, in the SystemClock.uptimeMillis time base.
 *
 * @return milliseconds since the boot
 */
uint64_t Sys_Android_Microseconds( void );

#ifndef DEDICATED_ONLY

/**
 * Handles Native App Glue commands on the client side.
 *
 * @param app the Native App Glue app
 * @param cmd the command
 */
void CL_Sys_Android_OnAppCmd( struct android_app *app, int32_t cmd );

/**
 * Sends a character from the event looper to the engine.
 *
 * @param charkey the character
 */
void IN_Android_CharEvent( int charkey );

/**
 * Attaches the refresh module to an Android window.
 *
 * @param window the Android native window
 */
void VID_Android_SetWindow( ANativeWindow *window );

#endif

#endif // ANDROID_SYS_H
