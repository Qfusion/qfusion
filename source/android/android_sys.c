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
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/window.h>
#include <linux/limits.h>

int64_t sys_frame_time;

struct android_app *sys_android_app;

char sys_android_internalDataPath[PATH_MAX];

jclass sys_android_activityClass;
jmethodID sys_android_getSystemService;

char sys_android_packageName[PATH_MAX];

static pthread_key_t sys_android_jniEnvKey;

static jobject sys_android_wakeLock, sys_android_wifiLock;

static bool sys_android_initialized = false;

JNIEnv *Sys_Android_GetJNIEnv( void ) {
	JNIEnv *env;
	if( ( *sys_android_app->activity->vm )->AttachCurrentThread( sys_android_app->activity->vm, &env, NULL ) ) {
		Sys_Error( "Failed to attach the current thread to the Java VM" );
	}
	pthread_setspecific( sys_android_jniEnvKey, env );
	return env;
}

static void Sys_Android_JNIEnvDestructor( void *env ) {
	if( !env ) {
		return;
	}
	( *sys_android_app->activity->vm )->DetachCurrentThread( sys_android_app->activity->vm );
	pthread_setspecific( sys_android_jniEnvKey, NULL );
}

static void Sys_Android_OnAppCmd( struct android_app *app, int32_t cmd ) {
#ifndef DEDICATED_ONLY
	CL_Sys_Android_OnAppCmd( app, cmd );
#endif

	switch( cmd ) {
		case APP_CMD_DESTROY:
			Cbuf_ExecuteText( EXEC_NOW, "quit" );
			break;
	}
}

static void Sys_Android_WaitOnAppCmd( struct android_app *app, int32_t cmd ) {
	switch( cmd ) {
		case APP_CMD_INIT_WINDOW:
			sys_android_initialized = true;
			break;
		case APP_CMD_TERM_WINDOW:
			sys_android_initialized = false;
			break;
	}
}

void Sys_AppActivate( void ) {
}

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char string[1024];

	va_start( argptr, format );
	Q_vsnprintfz( string, sizeof( string ), format, argptr );
	va_end( argptr );

	__android_log_write( ANDROID_LOG_ERROR, "Qfusion", string );

	_exit( 1 );
}

const char *Sys_GetPreferredLanguage( void ) {
	static char lang[6];

	AConfiguration_getLanguage( sys_android_app->config, lang );
	if( lang[0] ) {
		AConfiguration_getCountry( sys_android_app->config, lang + 3 );
		if( lang[3] ) {
			lang[2] = '_';
		}
	}

	return Q_strlwr( lang );
}

void Sys_Init( void ) {
}

static bool sys_android_browserAvailable;

static bool Sys_Android_CheckBrowserAvailability( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	jobject intent, list;
	jstring str;
	bool available;

	{
		jclass intentClass;
		jmethodID init, addCategory;
		jobject intentRef;

		intentClass = ( *env )->FindClass( env, "android/content/Intent" );
		init = ( *env )->GetMethodID( env, intentClass, "<init>", "(Ljava/lang/String;)V" );
		addCategory = ( *env )->GetMethodID( env, intentClass, "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;" );

		str = ( *env )->NewStringUTF( env, "android.intent.action.MAIN" );
		intent = ( *env )->NewObject( env, intentClass, init, str );
		( *env )->DeleteLocalRef( env, str );
		( *env )->DeleteLocalRef( env, intentClass );

		str = ( *env )->NewStringUTF( env, "android.intent.category.APP_BROWSER" );
		intentRef = ( *env )->CallObjectMethod( env, intent, addCategory, str );
		( *env )->DeleteLocalRef( env, str );
		( *env )->DeleteLocalRef( env, intentRef );
	}

	{
		jobject pm;
		jmethodID getPackageManager;
		jclass pmClass;
		jmethodID queryIntentActivities;

		getPackageManager = ( *env )->GetMethodID( env, sys_android_activityClass,
												   "getPackageManager", "()Landroid/content/pm/PackageManager;" );
		pm = ( *env )->CallObjectMethod( env, sys_android_app->activity->clazz, getPackageManager );

		pmClass = ( *env )->FindClass( env, "android/content/pm/PackageManager" );
		queryIntentActivities = ( *env )->GetMethodID( env, pmClass,
													   "queryIntentActivities", "(Landroid/content/Intent;I)Ljava/util/List;" );
		( *env )->DeleteLocalRef( env, pmClass );

		list = ( *env )->CallObjectMethod( env, pm, queryIntentActivities, intent, 0x00010000 );
		( *env )->DeleteLocalRef( env, intent );
		( *env )->DeleteLocalRef( env, pm );
	}

	{
		jclass listClass;
		jmethodID isEmpty;

		listClass = ( *env )->FindClass( env, "java/util/List" );
		isEmpty = ( *env )->GetMethodID( env, listClass, "isEmpty", "()Z" );
		( *env )->DeleteLocalRef( env, listClass );
		available = ( ( *env )->CallBooleanMethod( env, list, isEmpty ) == JNI_FALSE );
		( *env )->DeleteLocalRef( env, list );
	}

	return available;
}

bool Sys_IsBrowserAvailable( void ) {
	return sys_android_browserAvailable;
}

void Sys_OpenURLInBrowser( const char *url ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	jobject uri, intent;
	jstring str;

	{
		jclass uriClass;
		jmethodID parse;

		uriClass = ( *env )->FindClass( env, "android/net/Uri" );
		parse = ( *env )->GetStaticMethodID( env, uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;" );
		str = ( *env )->NewStringUTF( env, url );
		uri = ( *env )->CallStaticObjectMethod( env, uriClass, parse, str );
		( *env )->DeleteLocalRef( env, str );
		( *env )->DeleteLocalRef( env, uriClass );
	}

	if( !uri ) {
		return;
	}

	{
		jclass intentClass;
		jmethodID init, setData;
		jobject intentRef;

		intentClass = ( *env )->FindClass( env, "android/content/Intent" );
		init = ( *env )->GetMethodID( env, intentClass, "<init>", "(Ljava/lang/String;)V" );
		setData = ( *env )->GetMethodID( env, intentClass, "setData", "(Landroid/net/Uri;)Landroid/content/Intent;" );

		str = ( *env )->NewStringUTF( env, "android.intent.action.VIEW" );
		intent = ( *env )->NewObject( env, intentClass, init, str );
		( *env )->DeleteLocalRef( env, str );
		( *env )->DeleteLocalRef( env, intentClass );

		intentRef = ( *env )->CallObjectMethod( env, intent, setData, uri );
		( *env )->DeleteLocalRef( env, uri );
		( *env )->DeleteLocalRef( env, intentRef );
	}

	{
		jmethodID startActivity;

		startActivity = ( *env )->GetMethodID( env, sys_android_activityClass, "startActivity", "(Landroid/content/Intent;)V" );
		( *env )->CallVoidMethod( env, sys_android_app->activity->clazz, startActivity, intent );
		( *env )->DeleteLocalRef( env, intent );
	}
}

void Sys_SendKeyEvents( void ) {
	sys_frame_time = Sys_Milliseconds();
}

void Sys_Sleep( unsigned int millis ) {
	usleep( millis * 1000 );
}

void *Sys_AcquireWakeLock( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID acquireWakeLock, acquireWifiLock;
	jclass wlClass;

	if( !acquireWakeLock ) {
		wlClass = ( *env )->GetObjectClass( env, sys_android_wakeLock );
		acquireWakeLock = ( *env )->GetMethodID( env, wlClass, "acquire", "()V" );
		( *env )->DeleteLocalRef( env, wlClass );
	}
	if( !acquireWifiLock ) {
		wlClass = ( *env )->GetObjectClass( env, sys_android_wifiLock );
		acquireWifiLock = ( *env )->GetMethodID( env, wlClass, "acquire", "()V" );
		( *env )->DeleteLocalRef( env, wlClass );
	}

	( *env )->CallVoidMethod( env, sys_android_wakeLock, acquireWakeLock );
	( *env )->CallVoidMethod( env, sys_android_wifiLock, acquireWifiLock );

	return ( void * )1;
}

void Sys_ReleaseWakeLock( void *wl ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID releaseWakeLock, releaseWifiLock;
	jclass wlClass;

	if( !releaseWakeLock ) {
		wlClass = ( *env )->GetObjectClass( env, sys_android_wakeLock );
		releaseWakeLock = ( *env )->GetMethodID( env, wlClass, "release", "()V" );
		( *env )->DeleteLocalRef( env, wlClass );
	}
	if( !releaseWifiLock ) {
		wlClass = ( *env )->GetObjectClass( env, sys_android_wifiLock );
		releaseWifiLock = ( *env )->GetMethodID( env, wlClass, "release", "()V" );
		( *env )->DeleteLocalRef( env, wlClass );
	}

	( *env )->CallVoidMethod( env, sys_android_wakeLock, releaseWakeLock );
	( *env )->CallVoidMethod( env, sys_android_wifiLock, releaseWifiLock );
}

int Sys_GetCurrentProcessId( void ) {
	return getpid();
}

void Sys_Quit( void ) {
	Qcommon_Shutdown();
	exit( 0 );
}

static void Sys_Android_Init( void ) {
	struct android_app *app = sys_android_app;
	JNIEnv *env;
	jobject activity = app->activity->clazz;

	// Resolve the app's internal storage root directory.
	{
		char relativePath[PATH_MAX];
		Q_snprintfz( relativePath, sizeof( relativePath ), "%s/..", app->activity->internalDataPath );
		realpath( relativePath, sys_android_internalDataPath );
	}

	// Set working directory to external data path.
	{
		char externalDataPath[PATH_MAX];
		Q_snprintfz( externalDataPath, sizeof( externalDataPath ), "%s/%d.%d/", // The trailing slash is here to make it a directory path, not a file path
					 app->activity->externalDataPath, APP_VERSION_MAJOR, APP_VERSION_MINOR );
		FS_CreateAbsolutePath( externalDataPath );
		if( chdir( externalDataPath ) ) {
			Sys_Error( "Failed to change working directory" );
		}
	}

	// Initialize JNI.
	if( pthread_key_create( &sys_android_jniEnvKey, Sys_Android_JNIEnvDestructor ) ) {
		Sys_Error( "Failed to create JNIEnv destructor" );
	}
	env = Sys_Android_GetJNIEnv();

	// Activity class shortcut.
	{
		jclass activityClass = ( *env )->GetObjectClass( env, activity );
		sys_android_activityClass = ( *env )->NewGlobalRef( env, activityClass );
		sys_android_getSystemService = ( *env )->GetMethodID( env, activityClass,
															  "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;" );
		( *env )->DeleteLocalRef( env, activityClass );
	}

	// Get package name.
	{
		jmethodID getPackageName;
		jstring packageNameJS;
		const char *packageNameUTF;

		getPackageName = ( *env )->GetMethodID( env, sys_android_activityClass, "getPackageName", "()Ljava/lang/String;" );
		packageNameJS = ( *env )->CallObjectMethod( env, activity, getPackageName );
		packageNameUTF = ( *env )->GetStringUTFChars( env, packageNameJS, NULL );
		Q_strncpyz( sys_android_packageName, packageNameUTF, sizeof( sys_android_packageName ) );
		( *env )->ReleaseStringUTFChars( env, packageNameJS, packageNameUTF );
		( *env )->DeleteLocalRef( env, packageNameJS );
	}

	// Initialize the wake lock.
	{
		jstring name;
		jobject power;
		jclass powerClass;
		jmethodID newWakeLock;
		jstring tag;
		jobject wakeLock;

		name = ( *env )->NewStringUTF( env, "power" );
		power = ( *env )->CallObjectMethod( env, activity, sys_android_getSystemService, name );
		( *env )->DeleteLocalRef( env, name );
		powerClass = ( *env )->FindClass( env, "android/os/PowerManager" );
		newWakeLock = ( *env )->GetMethodID( env, powerClass, "newWakeLock",
											 "(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;" );
		( *env )->DeleteLocalRef( env, powerClass );
		tag = ( *env )->NewStringUTF( env, "Qfusion" );
		wakeLock = ( *env )->CallObjectMethod( env, power, newWakeLock, 1 /* PARTIAL_WAKE_LOCK */, tag );
		( *env )->DeleteLocalRef( env, tag );
		( *env )->DeleteLocalRef( env, power );
		sys_android_wakeLock = ( *env )->NewGlobalRef( env, wakeLock );
		( *env )->DeleteLocalRef( env, wakeLock );
	}

	// Initialize the high-performance Wi-Fi lock.
	{
		jstring name;
		jobject wifi;
		jclass wifiClass;
		jmethodID createWifiLock;
		jstring tag;
		jobject wifiLock;

		name = ( *env )->NewStringUTF( env, "wifi" );
		wifi = ( *env )->CallObjectMethod( env, activity, sys_android_getSystemService, name );
		( *env )->DeleteLocalRef( env, name );
		wifiClass = ( *env )->FindClass( env, "android/net/wifi/WifiManager" );
		createWifiLock = ( *env )->GetMethodID( env, wifiClass, "createWifiLock",
												"(ILjava/lang/String;)Landroid/net/wifi/WifiManager$WifiLock;" );
		( *env )->DeleteLocalRef( env, wifiClass );
		tag = ( *env )->NewStringUTF( env, "Qfusion" );
		wifiLock = ( *env )->CallObjectMethod( env, wifi, createWifiLock, 3 /* WIFI_MODE_FULL_HIGH_PERF */, tag );
		( *env )->DeleteLocalRef( env, tag );
		( *env )->DeleteLocalRef( env, wifi );
		sys_android_wifiLock = ( *env )->NewGlobalRef( env, wifiLock );
		( *env )->DeleteLocalRef( env, wifiLock );
	}

	// Set native app cmd handler to the actual one.
	app->onAppCmd = Sys_Android_OnAppCmd;

	// Check if browser links can be clicked.
	sys_android_browserAvailable = Sys_Android_CheckBrowserAvailability();
}

static void Sys_Android_ProcessInput( struct android_app *app, struct android_poll_source *source ) {
	AInputEvent *event = NULL;
	int32_t handled;

	while( AInputQueue_getEvent( app->inputQueue, &event ) >= 0 ) {
		// Instead of calling AInputQueue_preDispatchEvent which hangs the whole engine on API 17 and lower,
		// let onInputEvent hide the IME using JNI.
		handled = 0;
		if( app->onInputEvent ) {
			handled = app->onInputEvent( app, event );
		}
		if( ( AInputEvent_getType( event ) == AINPUT_EVENT_TYPE_KEY ) && ( AKeyEvent_getKeyCode( event ) == AKEYCODE_BACK ) ) {
			handled = 1;
		}
		AInputQueue_finishEvent( app->inputQueue, event, handled );
	}
}

static void Sys_Android_ExecuteIntent( void ) {
	JNIEnv *env = Sys_Android_GetJNIEnv();
	static jmethodID getIntentCommand;
	jstring cmdJS;
	const char *cmd;

	if( !getIntentCommand ) {
		getIntentCommand = ( *env )->GetMethodID( env, sys_android_activityClass, "getIntentCommand", "()Ljava/lang/String;" );
	}

	cmdJS = ( *env )->CallObjectMethod( env, sys_android_app->activity->clazz, getIntentCommand );
	if( !cmdJS ) {
		return;
	}

	cmd = ( *env )->GetStringUTFChars( env, cmdJS, NULL );
	Cbuf_AddText( cmd );
	( *env )->ReleaseStringUTFChars( env, cmdJS, cmd );
	( *env )->DeleteLocalRef( env, cmdJS );
}

void android_main( struct android_app *app ) {
	int ident, events;
	struct android_poll_source *source;
	int64_t oldtime, newtime, time;

	app_dummy();

	sys_android_app = app;
	app->inputPollSource.process = Sys_Android_ProcessInput;
	ANativeActivity_setWindowFlags( app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 ); // flags must be set before window creation

	app->onAppCmd = Sys_Android_WaitOnAppCmd;
	while( !sys_android_initialized ) {
		Sys_Sleep( 0 );
		while( ( ident = ALooper_pollAll( 0, NULL, &events, ( void ** )( &source ) ) ) >= 0 ) {
			if( source ) {
				source->process( app, source );
			}
		}
	}

	Sys_Android_Init();
	Qcommon_Init( 0, NULL );

	oldtime = Sys_Milliseconds();
	for( ;; ) {
		if( dedicated && dedicated->integer ) {
			Sys_Sleep( 1 );
		}

		while( ( ident = ALooper_pollAll( 0, NULL, &events, ( void ** )( &source ) ) ) >= 0 ) {
			if( source ) {
				source->process( app, source );
			}
		}

		for( ;; ) {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 ) {
				break;
			}
			Sys_Sleep( 0 );
		}
		oldtime = newtime;

		Sys_Android_ExecuteIntent();
		Qcommon_Frame( time );
	}
}
