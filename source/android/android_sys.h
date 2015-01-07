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

#ifndef ANDROID_SYS_H
#define ANDROID_SYS_H

#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "android_native_app_glue.h"

extern struct android_app *sys_android_app;

extern jclass sys_android_activityClass;

extern char sys_android_packageName[];

JNIEnv *Sys_Android_GetJNIEnv( void );

#endif // ANDROID_SYS_H
