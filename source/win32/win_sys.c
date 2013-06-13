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

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <limits.h>

#include "../win32/conproc.h"

#define MINIMUM_WIN_MEMORY  0x0a00000
#define MAXIMUM_WIN_MEMORY  0x1000000

qboolean s_win95;

int starttime;
int ActiveApp;
int Minimized;
int AppFocused;

static HANDLE hinput, houtput;

unsigned sys_msg_time;
unsigned sys_frame_time;


static HANDLE qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int argc;
char *argv[MAX_NUM_ARGVS];

// dynvar forward declarations
static dynvar_get_status_t Sys_GetAffinity_f( void **affinity );
static dynvar_set_status_t Sys_SetAffinity_f( void *affinity );

static qboolean	hwtimer;
static dynvar_t	*hwtimer_var;
static int milli_offset = 0;
static qint64 micro_offset = 0;

static dynvar_get_status_t Sys_GetHwTimer_f( void **val );
static dynvar_set_status_t Sys_SetHwTimer_f( void *val );
static void Sys_SynchronizeTimers_f( void *val );

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	CL_Shutdown();

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	MessageBox( NULL, msg, "Error", 0 /* MB_OK */ );
	if( qwclsemaphore )
		CloseHandle( qwclsemaphore );

	// shut down QHOST hooks if necessary
	DeinitConProc();

	Qcommon_Shutdown();

	exit( 1 );
}

void Sys_Quit( void )
{
	timeEndPeriod( 1 );

	CL_Shutdown();

	CloseHandle( qwclsemaphore );
	if( dedicated && dedicated->integer )
		FreeConsole();

	// shut down QHOST hooks if necessary
	DeinitConProc();

	Qcommon_Shutdown();

	exit( 0 );
}

//================================================================

/*
* Sys_Milliseconds
*/
// wsw: pb adapted High Res Performance Counter code from ezquake
static qint64 freq;

static void Sys_InitTime( void )
{
	char *hwtimerStr;
	assert( hwtimer_var );
	Dynvar_GetValue( hwtimer_var, (void **)&hwtimerStr );
	assert( hwtimerStr );
	if( hwtimerStr[0] == '0' && COM_CheckParm( "-hwtimer" ) )
	{
		// hwtimer set by command line parameter (deprecated)
		Dynvar_SetValue( hwtimer_var, "1" );
	}
	// finally check whether hwtimer is activated
	if( hwtimer )
		Com_Printf( "Using High Resolution Performance Counter\n" );
	else
		Com_Printf( "Using timeGetTime\n" );
}

static unsigned int Sys_Milliseconds_TGT( void )
{
	static unsigned int base;
	static qboolean	initialized = qfalse;
	unsigned int now;

	if( !initialized )
	{
		// let base retain 16 bits of effectively random data which is used
		//for quickly generating random numbers
		base = timeGetTime() & 0xffff0000;
		initialized = qtrue;
	}

	now = timeGetTime();

	return now - base;
}

static quint64 Sys_Microseconds_QPC( void )
{
	static qboolean first = qtrue;
	static qint64 p_start;

	qint64 p_now;
	QueryPerformanceCounter( (LARGE_INTEGER *) &p_now );

	if( first )
	{
		first = qfalse;
		p_start = p_now;
	}

	return ( ( p_now - p_start ) * 1000000 ) / freq;
}

unsigned int Sys_Milliseconds( void )
{
	if( hwtimer )
		return ( Sys_Microseconds_QPC() + micro_offset ) / 1000;
	else
		return Sys_Milliseconds_TGT() + milli_offset;
}

quint64 Sys_Microseconds( void )
{
	if( hwtimer )
		return Sys_Microseconds_QPC() + micro_offset;
	else
		return (quint64)( Sys_Milliseconds_TGT() + milli_offset ) * 1000;
}

void Sys_Sleep( unsigned int millis )
{
	Sleep( millis );
}

/*
* Sys_GetSymbol
*/
#ifdef SYS_SYMBOL
void *Sys_GetSymbol( const char *moduleName, const char *symbolName )
{
	HMODULE module = GetModuleHandle( moduleName );
	return module
		? (void *) GetProcAddress( module, symbolName )
		: NULL;
}
#endif // SYS_SYMBOL

//===============================================================================

/*
* Sys_Init
*/
void Sys_Init( void )
{
	OSVERSIONINFO vinfo;

	timeBeginPeriod( 1 );
	Sys_InitTime();

	vinfo.dwOSVersionInfoSize = sizeof( vinfo );

	if( !GetVersionEx( &vinfo ) )
		Sys_Error( "Couldn't get OS info" );

	if( vinfo.dwMajorVersion < 4 )
		Sys_Error( "%s requires windows version 4 or greater", APPLICATION );
	if( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
		Sys_Error( "%s doesn't run on Win32s", APPLICATION );
	else if( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
		s_win95 = qtrue;

	if( dedicated->integer )
	{
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

		if( !AllocConsole() )
			Sys_Error( "Couldn't create dedicated server console" );
		hinput = GetStdHandle( STD_INPUT_HANDLE );
		houtput = GetStdHandle( STD_OUTPUT_HANDLE );

		// let QHOST hook in
		InitConProc( argc, argv );
	}
}

/*
* Sys_InitDynvars
*/
void Sys_InitDynvars( void )
{
	char *dummyStr;
	dynvar_t *affinity_var;

	QueryPerformanceFrequency( (LARGE_INTEGER *) &freq );

	affinity_var = Dynvar_Create( "sys_affinity", qtrue, Sys_GetAffinity_f, Sys_SetAffinity_f );
	assert( affinity_var );
	Dynvar_GetValue( affinity_var, (void **)&dummyStr );
	assert( dummyStr );
	Dynvar_SetValue( affinity_var, dummyStr );

	hwtimer_var = Dynvar_Create( "sys_hwtimer", 1, Sys_GetHwTimer_f, Sys_SetHwTimer_f );
	assert( hwtimer_var );
	Dynvar_AddListener( hwtimer_var, Sys_SynchronizeTimers_f );
	Dynvar_SetValue( hwtimer_var, "0" );
}

#define MAX_CONSOLETEXT 256
static char console_text[MAX_CONSOLETEXT];
static int console_textlen;

static char *OEM_to_utf8( const char *str )
{
	WCHAR wstr[MAX_CONSOLETEXT];
	static char utf8str[MAX_CONSOLETEXT*4]; /* longest valid utf8 sequence is 4 bytes */

	MultiByteToWideChar( CP_OEMCP, 0, str, -1, wstr, sizeof( wstr )/sizeof( WCHAR ) );
	wstr[sizeof( wstr )/sizeof( wstr[0] ) - 1] = 0;
	WideCharToMultiByte( CP_UTF8, 0, wstr, -1, utf8str, sizeof( utf8str ), NULL, NULL );
	utf8str[sizeof( utf8str ) - 1] = 0;

	return utf8str;
}

static char *utf8_to_OEM( const char *utf8str )
{
	WCHAR wstr[MAX_PRINTMSG];
	static char oemstr[MAX_PRINTMSG];

	MultiByteToWideChar( CP_UTF8, 0, utf8str, -1, wstr, sizeof( wstr )/sizeof( WCHAR ) );
	wstr[sizeof( wstr )/sizeof( wstr[0] ) - 1] = 0;
	WideCharToMultiByte( CP_OEMCP, 0, wstr, -1, oemstr, sizeof( oemstr ), "?", NULL );
	oemstr[sizeof( oemstr ) - 1] = 0;

	return oemstr;
}

/*
* Sys_ConsoleInput
*/
char *Sys_ConsoleInput( void )
{
	INPUT_RECORD rec;
	int ch;
	DWORD dummy;
	DWORD numread, numevents;

	if( !dedicated || !dedicated->integer )
		return NULL;

	for(;; )
	{
		if( !GetNumberOfConsoleInputEvents( hinput, &numevents ) )
			Sys_Error( "Error getting # of console events" );

		if( numevents <= 0 )
			break;

		if( !ReadConsoleInput( hinput, &rec, 1, &numread ) )
			Sys_Error( "Error reading console input" );

		if( numread != 1 )
			Sys_Error( "Couldn't read console input" );

		if( rec.EventType == KEY_EVENT )
		{
			if( !rec.Event.KeyEvent.bKeyDown )
			{
				ch = rec.Event.KeyEvent.uChar.AsciiChar;

				switch( ch )
				{
				case '\r':
					WriteFile( houtput, "\r\n", 2, &dummy, NULL );

					if( console_textlen )
					{
						console_text[console_textlen] = 0;
						console_textlen = 0;
						return OEM_to_utf8( console_text );
					}
					break;

				case '\b':
					if( console_textlen )
					{
						console_textlen--;
						WriteFile( houtput, "\b \b", 3, &dummy, NULL );
					}
					break;

				default:
					if( ( unsigned char )ch >= ' ' )
					{
						if( console_textlen < sizeof( console_text )-2 )
						{
							WriteFile( houtput, &ch, 1, &dummy, NULL );
							console_text[console_textlen] = ch;
							console_textlen++;
						}
					}
					break;
				}
			}
		}
	}

	return NULL;
}

static void PrintColoredText( const char *s )
{
	char c;
	int colorindex;
	DWORD dummy;

	while( *s )
	{
		int gc = Q_GrabCharFromColorString( &s, &c, &colorindex );
		if( gc == GRABCHAR_CHAR )
		{
			if( c == '\n' )
				SetConsoleTextAttribute( houtput, 7 );
			// I hope it's not too slow to output char by char
			WriteFile( houtput, &c, 1, &dummy, NULL );
		}
		else if( gc == GRABCHAR_COLOR )
		{
			switch( colorindex )
			{
			case 0: colorindex = 3; break;	// dark cyan instead of black to keep it visible
			case 1: colorindex = 12; break;
			case 2: colorindex = 10; break;
			case 3: colorindex = 14; break;
			case 4: colorindex = 9; break;
			case 5: colorindex = 11; break;	// note that cyan and magenta are 
			case 6: colorindex = 13; break;	// not where one might expect
			case 8: colorindex = 6; break;
			case 9: colorindex = 8; break;
			default:
			case 7: colorindex = 7; break;	// 15 would be bright white
			};
			SetConsoleTextAttribute( houtput, colorindex );
		}
		else if( gc == GRABCHAR_END )
			break;
		else
			assert( 0 );
	}
}

/*
* Sys_ConsoleOutput
* 
* Print text to the dedicated console
*/
void Sys_ConsoleOutput( char *string )
{
	DWORD dummy;
	char text[MAX_CONSOLETEXT+2];	/* need 2 chars for the \r's */

	if( !dedicated || !dedicated->integer )
		return;

	if( console_textlen )
	{
		text[0] = '\r';
		memset( &text[1], ' ', console_textlen );
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile( houtput, text, console_textlen+2, &dummy, NULL );
	}

	string = utf8_to_OEM( string );

#if 0
	WriteFile( houtput, string, (unsigned)strlen( string ), &dummy, NULL );
#else
	PrintColoredText( string );
#endif

	if( console_textlen )
		WriteFile( houtput, console_text, console_textlen, &dummy, NULL );
}


/*
* myTranslateMessage
* A wrapper around TranslateMessage to avoid garbage if the toggleconsole
* key happens to be a dead key (like in the German layout)
*/
#ifdef DEDICATED_ONLY
#define myTranslateMessage(msg) TranslateMessage(msg)
#else
int IN_MapKey( int key );
qboolean Key_IsNonPrintable( int key );
static BOOL myTranslateMessage (MSG *msg)
{
	if (msg->message == WM_KEYDOWN) {
		if (Key_IsNonPrintable(IN_MapKey(msg->lParam)))
			return TRUE;
		else
			return TranslateMessage(msg);
	}
	return TranslateMessage(msg);
}
#endif

/*
* Sys_SendKeyEvents
* 
* Send Key_Event calls
*/
void Sys_SendKeyEvents( void )
{
	MSG msg;

	while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) )
	{
		if( !GetMessageW( &msg, NULL, 0, 0 ) )
			Sys_Quit();
		sys_msg_time = msg.time;
		myTranslateMessage( &msg );
		DispatchMessageW( &msg );
	}

	// grab frame time
	sys_frame_time = timeGetTime(); // FIXME: should this be at start?
}



/*
* Sys_GetClipboardData
*/
char *Sys_GetClipboardData( qboolean primary )
{
	char *utf8text = NULL;
	int utf8size;
	WCHAR *cliptext;

	if( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if( ( hClipboardData = GetClipboardData( CF_UNICODETEXT ) ) != 0 )
		{
			if( ( cliptext = GlobalLock( hClipboardData ) ) != 0 )
			{
				utf8size = WideCharToMultiByte( CP_UTF8, 0, cliptext, -1, NULL, 0, NULL, NULL );
				utf8text = Q_malloc( utf8size );
				WideCharToMultiByte( CP_UTF8, 0, cliptext, -1, utf8text, utf8size, NULL, NULL );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return utf8text;
}

/*
* Sys_SetClipboardData
*/
qboolean Sys_SetClipboardData( char *data )
{
	size_t size;
	HGLOBAL hglbCopy;
	UINT uFormat;
	LPCSTR cliptext = data;
	LPWSTR lptstrCopy;

	// open the clipboard, and empty it
	if( !OpenClipboard( NULL ) ) 
		return qfalse;

	EmptyClipboard();

	size = MultiByteToWideChar( CP_UTF8, 0, cliptext, -1, NULL, 0 );

	// allocate a global memory object for the text
	hglbCopy = GlobalAlloc( GMEM_MOVEABLE, (size + 1) * sizeof( *lptstrCopy ) ); 
	if( hglbCopy == NULL )
	{
		CloseClipboard(); 
		return qfalse; 
	} 

	// lock the handle and copy the text to the buffer
	lptstrCopy = GlobalLock( hglbCopy ); 

	uFormat = CF_UNICODETEXT;
	MultiByteToWideChar( CP_UTF8, 0, cliptext, -1, lptstrCopy, size );
	lptstrCopy[size] = 0;

	GlobalUnlock( hglbCopy ); 

	// place the handle on the clipboard
	SetClipboardData( uFormat, hglbCopy );

	// close the clipboard
	CloseClipboard();

	return qtrue;
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	Q_free( data );
}

/*
* Sys_OpenURLInBrowser
*/
void Sys_OpenURLInBrowser( const char *url )
{
	ShellExecute( NULL, "open", url, NULL, NULL, SW_SHOWNORMAL );
}

/*
==============================================================================

WINDOWS CRAP

==============================================================================
*/

/*
* Sys_AppActivate
*/
void Sys_AppActivate( void )
{
#ifndef DEDICATED_ONLY
	ShowWindow( cl_hwnd, SW_RESTORE );
	SetForegroundWindow( cl_hwnd );
#endif
}

//========================================================================

/*
* ParseCommandLine
*/
static void ParseCommandLine( LPSTR lpCmdLine )
{
	argc = 1;
	argv[0] = "exe";

	while( *lpCmdLine && ( argc < MAX_NUM_ARGVS ) )
	{
		while( *lpCmdLine && ( *lpCmdLine <= 32 || *lpCmdLine > 126 ) )
			lpCmdLine++;

		if( *lpCmdLine )
		{
			char quote = ( ( '"' == *lpCmdLine || '\'' == *lpCmdLine ) ? *lpCmdLine++ : 0 );

			argv[argc++] = lpCmdLine;
			if( quote )
			{
				while( *lpCmdLine && *lpCmdLine != quote && *lpCmdLine >= 32 && *lpCmdLine <= 126 )
					lpCmdLine++;
			}
			else
			{
				while( *lpCmdLine && *lpCmdLine > 32 && *lpCmdLine <= 126 )
					lpCmdLine++;
			}

			if( *lpCmdLine )
				*lpCmdLine++ = 0;
		}
	}
}

static dynvar_get_status_t Sys_GetAffinity_f( void **affinity )
{
	static qboolean affinityAutoSet = qfalse;
	static char affinityString[33];
	DWORD_PTR procAffinity, sysAffinity;
	HANDLE proc = GetCurrentProcess();

	if( GetProcessAffinityMask( proc, &procAffinity, &sysAffinity ) )
	{
		SYSTEM_INFO sysInfo;
		DWORD i;

		CloseHandle( proc );

		assert( affinity );

		GetSystemInfo( &sysInfo );
		for( i = 0; i < sysInfo.dwNumberOfProcessors && i < 33; ++i )
		{
			affinityString[i] = '0' + ( ( procAffinity & sysAffinity ) & 1 );
			procAffinity >>= 1;
			sysAffinity >>= 1;
		}
		affinityString[i] = '\0';

		if( !affinityAutoSet )
		{
			// set the affinity string to something like 0001
			const char *lastBit = strrchr( affinityString, '1' );
			if( lastBit )
			{   // Vic: FIXME??
				for( i = 0; i < (DWORD)( lastBit - affinityString ); i++ )
					affinityString[i] = '0';
			}
			affinityAutoSet = qtrue;
		}

		*affinity = affinityString;
		return DYNVAR_GET_OK;
	}

	CloseHandle( proc );
	*affinity = NULL;
	return DYNVAR_GET_TRANSIENT;
}

static dynvar_set_status_t Sys_SetAffinity_f( void *affinity )
{
	dynvar_set_status_t result = DYNVAR_SET_INVALID;
	SYSTEM_INFO sysInfo;
	DWORD_PTR procAffinity = 0, i;
	HANDLE proc = GetCurrentProcess();
	char minValid[33], maxValid[33];
	const size_t len = strlen( (char *) affinity );

	// create range of valid values for error printing
	GetSystemInfo( &sysInfo );
	for( i = 0; i < sysInfo.dwNumberOfProcessors; ++i )
	{
		minValid[i] = '0';
		maxValid[i] = '1';
	}
	minValid[i] = '\0';
	maxValid[i] = '\0';

	if( len == sysInfo.dwNumberOfProcessors )
	{
		// string is of valid length, parse in reverse direction
		const char *c;
		for( c = ( (char *) affinity ) + len - 1; c >= (char *) affinity; --c )
		{
			// parse binary digit
			procAffinity <<= 1;
			switch( *c )
			{
			case '0':
				// nothing to do
				break;
			case '1':
				// at least one digit must be 1
				result = DYNVAR_SET_OK;
				procAffinity |= 1;
				break;
			default:
				// invalid character found
				result = DYNVAR_SET_INVALID;
				goto abort;
			}
		}

		SetProcessAffinityMask( proc, procAffinity );
		//if (len > 1)
		//SetPriorityClass(proc, HIGH_PRIORITY_CLASS);
	}

abort:
	if( result != DYNVAR_SET_OK )
		Com_Printf( "\"sys_affinity\" must be a non-zero bitmask between \"%s\" and \"%s\".\n", minValid, maxValid );

	CloseHandle( proc );
	return result;
}

static dynvar_get_status_t Sys_GetHwTimer_f( void **val )
{
	static char hwtimerStr[2] = { '\0', '\0' };
	hwtimerStr[0] = '0' + hwtimer;
	assert( val );
	*val = hwtimerStr;
	return DYNVAR_GET_OK;
}

static dynvar_set_status_t Sys_SetHwTimer_f( void *val )
{
	assert( val );
	switch( *( (char *) val ) )
	{
	case '0':
		hwtimer = 0;
		return DYNVAR_SET_OK;
	case '1':
		if( freq )
		{
			hwtimer = 1;
			return DYNVAR_SET_OK;
		}
		else
			return DYNVAR_SET_TRANSIENT;
	default:
		return DYNVAR_SET_INVALID;
	}
}

static void Sys_SynchronizeTimers_f( void *val )
{
	static int hwtimer_old = -1;

	const unsigned int millis = Sys_Milliseconds_TGT();
	const qint64 micros = Sys_Microseconds_QPC();
	const qint64 drift = micros - millis * 1000;

	const int hwtimer_new = ( *(char *) val ) - '0';

	if( hwtimer_new != hwtimer_old )
	{
		switch( hwtimer_new )
		{
		case 0:
			// switched from micro to milli precision
			milli_offset = max( milli_offset, drift / 1000 );
			break;
		case 1:
			// switched from milli to micro precision
			micro_offset = max( micro_offset, -drift );
			break;
		default:
			assert( 0 );
		}
		hwtimer_old = hwtimer_new;
	}
}

/*
* WinMain
*/
HINSTANCE global_hInstance;
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	MSG msg;
	unsigned int oldtime, newtime, time;

	/* previous instances do not exist in Win32 */
	if( hPrevInstance )
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine( lpCmdLine );

	Qcommon_Init( argc, argv );

	oldtime = Sys_Milliseconds();

	/* main window message loop */
	while( 1 )
	{
		// if at a full screen console, don't update unless needed
		if( Minimized || ( dedicated && dedicated->integer ) )
		{
			Sleep( 1 );
		}

		while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) )
		{
			if( !GetMessageW( &msg, NULL, 0, 0 ) )
				Com_Quit();
			sys_msg_time = msg.time;
			myTranslateMessage( &msg );
			DispatchMessageW( &msg );
		}

		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime; // no warp problem as unsigned
			if( time > 0 )
				break;
#ifdef PUTCPU2SLEEP
			Sys_Sleep( 0 );
#endif
		}
		while( 1 );
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
