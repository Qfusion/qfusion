#include "sdl/SDL.h"
#include "client/client.h"

#if defined( _WIN32 )
#include "win32/resource.h"
#endif

#if defined( __APPLE__ )
#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
#endif

int64_t sys_frame_time;

void Sys_InitTime( void );

void Sys_Sleep( unsigned int millis ) {
	SDL_Delay( millis );
}

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, APPLICATION, msg, NULL );

	exit( 1 );
}

/*
* Sys_Init
*/
void Sys_Init( void ) {
	Sys_InitTime();
}

/*
* Sys_Quit
*/
void Sys_Quit( void ) {
	Qcommon_Shutdown();

	exit( 0 );
}

/*
* Sys_AppActivate
*/
void Sys_AppActivate( void ) {
}

/*
* Sys_SendKeyEvents
*/
void Sys_SendKeyEvents( void ) {
	// grab frame time
	sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/

int main( int argc, char **argv ) {
	int64_t oldtime, newtime;

#if defined( __APPLE__ ) && !defined( DEDICATED_ONLY )
	char resourcesPath[MAXPATHLEN];
	CFURLGetFileSystemRepresentation( CFBundleCopyResourcesDirectoryURL( CFBundleGetMainBundle() ), 1, (UInt8 *)resourcesPath, MAXPATHLEN );
	chdir( resourcesPath );
#endif

#if defined( __WIN32__ )
#if defined( _DEBUG )
	SDL_SetHint( SDL_HINT_ALLOW_TOPMOST, "0" );
#endif
#endif

	SDL_Init( SDL_INIT_VIDEO );

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();
	while( true ) {
		int time;
		// find time spent rendering last frame
		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 ) {
				break;
			}
		} while( 1 );
		oldtime = newtime;

		Qcommon_Frame( time );
	}

	SDL_Quit();
}
