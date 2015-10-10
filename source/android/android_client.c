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
#include <unistd.h>

/**
 * Client commands from the UI thread.
 */
enum
{
	CL_SYS_ANDROID_EVENT_CHAR
};

/**
 * The LOOPER_ID_USER_CLIENT looper data.
 */
static struct android_poll_source cl_sys_android_pollSource;
static int cl_sys_android_pipe[2];

/**
 * Processes client events from the UI thread.
 *
 * @param app    the Native App Glue app
 * @param source the looper poll source
 */
static void CL_Sys_Android_ProcessUserEvent( struct android_app *app, struct android_poll_source *source )
{
	int fd = cl_sys_android_pipe[0];
	uint8_t event;

	if( read( fd, &event, sizeof( event ) ) != sizeof( event ) )
		return;

	switch( event )
	{
	case CL_SYS_ANDROID_EVENT_CHAR:
		{
			uint16_t charkey;
			if( read( fd, &charkey, sizeof( charkey ) ) == sizeof( charkey ) )
				IN_Android_CharEvent( charkey );
		}
		break;
	}
}

/**
 * Sends a character from the IME to the looper.
 *
 * @param env     the JNI environment
 * @param obj     the activity Java object
 * @param charkey the character code
 */
JNIEXPORT void JNICALL Java_net_qfusion_engine_QfusionActivity_dispatchCharEvent( JNIEnv *env, jobject obj, jchar charkey )
{
	uint8_t event[1 + sizeof( uint16_t )];

	if( !cl_sys_android_pipe[1] )
		return;

	event[0] = CL_SYS_ANDROID_EVENT_CHAR;
	memcpy( event + 1, &charkey, sizeof( uint16_t ) );
	write( cl_sys_android_pipe[1], event, sizeof( event ) );
}

void CL_Sys_Init( void )
{
	struct android_app *app = sys_android_app;

	if( pipe( cl_sys_android_pipe ) )
		Sys_Error( "Failed to create the pipe for client events" );

	cl_sys_android_pollSource.id = LOOPER_ID_USER_CLIENT;
	cl_sys_android_pollSource.app = app;
	cl_sys_android_pollSource.process = CL_Sys_Android_ProcessUserEvent;
	ALooper_addFd( app->looper, cl_sys_android_pipe[0], LOOPER_ID_USER_CLIENT, ALOOPER_EVENT_INPUT, NULL, &cl_sys_android_pollSource );
}

void CL_Sys_Shutdown( void )
{
	close( cl_sys_android_pipe[0] );
	close( cl_sys_android_pipe[1] );
	cl_sys_android_pipe[0] = cl_sys_android_pipe[1] = 0;
}

void CL_Sys_Android_OnAppCmd( struct android_app *app, int32_t cmd )
{
	switch( cmd )
	{
	case APP_CMD_INIT_WINDOW:
	case APP_CMD_START:
		VID_Android_SetWindow( app->window );
		break;
	case APP_CMD_TERM_WINDOW:
	case APP_CMD_STOP:
		VID_Android_SetWindow( NULL );
		break;
	case APP_CMD_RESUME:
		CL_SoundModule_Activate( true );
		break;
	case APP_CMD_PAUSE:
		CL_SoundModule_Activate( false );
		IN_ShowSoftKeyboard( false );
		break;
	}
}
