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
#include "../client/client.h"
#include <android_native_app_glue.h>

extern struct android_app *sys_android_app;

extern jclass sys_android_activityClass;

extern char sys_android_packageName[];

JNIEnv *Sys_Android_GetJNIEnv( void );

uint64_t Sys_Android_Microseconds( void );

enum
{
	LOOPER_ID_QFUSION = LOOPER_ID_USER
};

enum
{
	SYS_ANDROID_EVENT_INPUT = 0x80,
	SYS_ANDROID_EVENT_INPUT_CHAR = SYS_ANDROID_EVENT_INPUT
};

#ifndef DEDICATED_ONLY
void IN_Android_OnQfusionEvent( uint8_t event, int readfd );
#endif

#endif // ANDROID_SYS_H
