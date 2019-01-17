#include <windows.h>

#include "qcommon/qcommon.h"

#define MAX_NUM_ARGVS   128
static int argc;
static char *argv[MAX_NUM_ARGVS];

void Sys_InitTime();

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	MessageBox( NULL, msg, "Error", 0 /* MB_OK */ );

	exit( 1 );
}

void Sys_Quit() {
	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();

	FreeConsole();

	Qcommon_Shutdown();

	exit( 0 );
}

void Sys_Sleep( unsigned int millis ) {
	Sleep( millis );
}

void Sys_Init() {
	Sys_InitTime();

	SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

	if( !AllocConsole() ) {
		Sys_Error( "Couldn't create dedicated server console" );
	}
}

static void ParseCommandLine( LPSTR lpCmdLine ) {
	argc = 1;
	argv[0] = "exe";

	while( *lpCmdLine && ( argc < MAX_NUM_ARGVS ) ) {
		while( *lpCmdLine && ( *lpCmdLine <= 32 || *lpCmdLine > 126 ) )
			lpCmdLine++;

		if( *lpCmdLine ) {
			char quote = ( ( '"' == *lpCmdLine || '\'' == *lpCmdLine ) ? *lpCmdLine++ : 0 );

			argv[argc++] = lpCmdLine;
			if( quote ) {
				while( *lpCmdLine && *lpCmdLine != quote && *lpCmdLine >= 32 && *lpCmdLine <= 126 )
					lpCmdLine++;
			} else {
				while( *lpCmdLine && *lpCmdLine > 32 && *lpCmdLine <= 126 )
					lpCmdLine++;
			}

			if( *lpCmdLine ) {
				*lpCmdLine++ = 0;
			}
		}
	}
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	MSG msg;
	int64_t oldtime, newtime, time;

	ParseCommandLine( lpCmdLine );

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();

	/* main window message loop */
	while( 1 ) {
		Sleep( 1 );

		while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) ) {
			if( !GetMessageW( &msg, NULL, 0, 0 ) ) {
				Com_Quit();
			}
			TranslateMessage( &msg );
			DispatchMessageW( &msg );
		}

		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 ) {
				break;
			}
			Sys_Sleep( 0 );
		} while( 1 );
		oldtime = newtime;

		Qcommon_Frame( time );
	}

	return TRUE;
}
