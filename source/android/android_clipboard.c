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

// Implemented in Java because ClipboardManager is obtained in the UI thread.

#include <stdlib.h>
#include <string.h>
#include "android_sys.h"

char *Sys_GetClipboardData( bool primary )
{
	JNIEnv *env = Sys_Android_GetJNIEnv();
	jmethodID getClipboardData;
	jstring textJS;
	const char *textUTF;
	char *text;

	getClipboardData = (*env)->GetMethodID( env, sys_android_activityClass, "getClipboardData", "()Ljava/lang/String;" );
	textJS = (*env)->CallObjectMethod( env, sys_android_app->activity->clazz, getClipboardData );
	if( !textJS )
		return NULL;

	textUTF = (*env)->GetStringUTFChars( env, textJS, NULL );
	text = Q_malloc( strlen( textUTF ) + 1 );
	strcpy( text, textUTF );
	(*env)->ReleaseStringUTFChars( env, textJS, textUTF );
	(*env)->DeleteLocalRef( env, textJS );
	return text;
}

bool Sys_SetClipboardData( const char *data )
{
	JNIEnv *env = Sys_Android_GetJNIEnv();
	jmethodID setClipboardData;
	jstring textJS;

	setClipboardData = (*env)->GetMethodID( env, sys_android_activityClass, "setClipboardData", "(Ljava/lang/CharSequence;)V" );
	textJS = (*env)->NewStringUTF( env, data );
	(*env)->CallVoidMethod( env, sys_android_app->activity->clazz, setClipboardData, textJS );
	(*env)->DeleteLocalRef( env, textJS );

	return true;
}

void Sys_FreeClipboardData( char *data )
{
	Q_free( data );
}
