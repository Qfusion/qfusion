/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// sys_win.h

#include "qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <limits.h>
#include <shellapi.h>

#include "conproc.h"

#if !defined( DEDICATED_ONLY )
QF_DLL_EXPORT DWORD NvOptimusEnablement = 0x00000001;
QF_DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;
#endif

#if defined( DEDICATED_ONLY )

int starttime;
int ActiveApp;
int Minimized;
int AppFocused;

int64_t sys_msg_time;

#define MAX_NUM_ARGVS   128
int argc;
char *argv[MAX_NUM_ARGVS];

void Sys_InitTime( void );

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	MessageBox( NULL, msg, "Error", 0 /* MB_OK */ );

	// shut down QHOST hooks if necessary
	DeinitConProc();

	exit( 1 );
}

void Sys_Quit( void ) {
	SV_Shutdown( "Server quit\n" );
	CL_Shutdown();

	if( dedicated && dedicated->integer ) {
		FreeConsole();
	}

	// shut down QHOST hooks if necessary
	DeinitConProc();

	Qcommon_Shutdown();

	exit( 0 );
}

//================================================================

void Sys_Sleep( unsigned int millis ) {
	Sleep( millis );
}

//===============================================================================

/*
* Sys_Init
*/
void Sys_Init( void ) {
	Sys_InitTime();

	if( dedicated->integer ) {
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

		if( !AllocConsole() ) {
			Sys_Error( "Couldn't create dedicated server console" );
		}

		// let QHOST hook in
		InitConProc( argc, argv );
	}
}

/*
* Sys_SendKeyEvents
*
* Send Key_Event calls
*/
void Sys_SendKeyEvents( void ) {
	MSG msg;

	while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) ) {
		if( !GetMessageW( &msg, NULL, 0, 0 ) ) {
			Sys_Quit();
		}
		sys_msg_time = msg.time;
		myTranslateMessage( &msg );
		DispatchMessageW( &msg );
	}
}

#endif // defined(DEDICATED_ONLY)

/*
* Sys_IsBrowserAvailable
*/
bool Sys_IsBrowserAvailable( void ) {
	return true;
}

/*
* Sys_OpenURLInBrowser
*/
void Sys_OpenURLInBrowser( const char *url ) {
	ShellExecute( NULL, "open", url, NULL, NULL, SW_SHOWNORMAL );
}

/*
* Sys_GetCurrentProcessId
*/
int Sys_GetCurrentProcessId( void ) {
	return GetCurrentProcessId();
}

/*
* Sys_GetPreferredLanguage
* Get the preferred language through the MUI API. Works on Vista and newer.
*/
const char *Sys_GetPreferredLanguage( void ) {
	return APP_DEFAULT_LANGUAGE;
}

#if defined( DEDICATED_ONLY )

/*
* Sys_AppActivate
*/
void Sys_AppActivate( void ) {
#ifndef DEDICATED_ONLY
	ShowWindow( cl_hwnd, SW_RESTORE );
	SetForegroundWindow( cl_hwnd );
#endif
}

//========================================================================

/*
* ParseCommandLine
*/
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

/*
* WinMain
*/
HINSTANCE global_hInstance;
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	MSG msg;
	int64_t oldtime, newtime, time;

	/* previous instances do not exist in Win32 */
	if( hPrevInstance ) {
		return 0;
	}

	global_hInstance = hInstance;

	ParseCommandLine( lpCmdLine );

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();

	/* main window message loop */
	while( 1 ) {
		// if at a full screen console, don't update unless needed
		if( Minimized || ( dedicated && dedicated->integer ) ) {
			Sleep( 1 );
		}

		while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) ) {
			if( !GetMessageW( &msg, NULL, 0, 0 ) ) {
				Com_Quit();
			}
			sys_msg_time = msg.time;
			myTranslateMessage( &msg );
			DispatchMessageW( &msg );
		}

		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime; // no warp problem as unsigned
			if( time > 0 ) {
				break;
			}
			Sys_Sleep( 0 );
		} while( 1 );
		//Com_Printf ("time:%5.2u - %5.2u = %5.2u\n", newtime, oldtime, time);
		oldtime = newtime;

		// do as q3 (use the default floating point precision)
		//	_controlfp( ~( _EM_ZERODIVIDE /*| _EM_INVALID*/ ), _MCW_EM );
		//_controlfp( _PC_24, _MCW_PC );
		Qcommon_Frame( time );
	}

	// never gets here
	return TRUE;
}

#endif // defined(DEDICATED_ONLY)
