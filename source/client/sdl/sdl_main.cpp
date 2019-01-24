#include "sdl/SDL.h"
#include "client/client.h"

#if defined( __APPLE__ )
#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
#endif

const bool is_dedicated_server = false;

void Sys_InitTime();

void Sys_Sleep( unsigned int millis ) {
	SDL_Delay( millis );
}

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	printf( "%s\n", msg );

	// SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, APPLICATION, msg, NULL );

	exit( 1 );
}

void Sys_Init() {
	Sys_InitTime();
}

void Sys_Quit() {
	Qcommon_Shutdown();
	exit( 0 );
}

int main( int argc, char **argv ) {
	int64_t oldtime, newtime;

#if defined( __APPLE__ )
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
		CL_Profiler_Flip();

		int time;
		{
			MICROPROFILE_SCOPEI( "Main", "Interframe", 0xffff0000 );
			// find time spent rendering last frame
			do {
				newtime = Sys_Milliseconds();
				time = newtime - oldtime;
			} while( time == 0 );
			oldtime = newtime;
		}

		Qcommon_Frame( time );
	}

	SDL_Quit();

	return 0;
}
