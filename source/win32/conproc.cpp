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
// conproc.c -- support for qhost

#include <stdio.h>
#include <process.h>
#include <windows.h>
#include "conproc.h"
#include "../gameshared/q_arch.h"

#define CCOM_WRITE_TEXT     0x2
// Param1 : Text

#define CCOM_GET_TEXT       0x3
// Param1 : Begin line
// Param2 : End line

#define CCOM_GET_SCR_LINES  0x4
// No params

#define CCOM_SET_SCR_LINES  0x5
// Param1 : Number of lines


static HANDLE heventDone;
static HANDLE hfileBuffer;
static HANDLE heventChildSend;
static HANDLE heventParentSend;
static HANDLE hStdout;
static HANDLE hStdin;

static unsigned WINAPI RequestProc( void *arg );
static LPVOID GetMappedBuffer( HANDLE hBuffer );
static void ReleaseMappedBuffer( LPVOID pBuffer );
static BOOL GetScreenBufferLines( int *piLines );
static BOOL SetScreenBufferLines( int iLines );
static BOOL ReadText( LPTSTR pszText, int iBeginLine, int iEndLine );
static BOOL WriteText( LPCTSTR szText );
static int CharToCode( char c );
static BOOL SetConsoleCXCY( HANDLE hStdout, int cx, int cy );

static int ccom_argc;
static char **ccom_argv;

/*
* CCheckParm
*
* Returns the position (1 to argc-1) in the program's argument list
* where the given parameter apears, or 0 if not present
*/
static int CCheckParm( char *parm ) {
	int i;

	for( i = 1; i < ccom_argc; i++ ) {
		if( !ccom_argv[i] ) {
			continue;
		}
		if( !strcmp( parm, ccom_argv[i] ) ) {
			return i;
		}
	}

	return 0;
}


void InitConProc( int argc, char **argv ) {
	unsigned threadAddr;
	HANDLE hFile = NULL;
	HANDLE heventParent = NULL;
	HANDLE heventChild = NULL;
	int t;

	ccom_argc = argc;
	if( argv ) {
		ccom_argv = argv;
	} else {
		ccom_argv = NULL;
	}

	// give QHOST a chance to hook into the console
	if( ( t = CCheckParm( "-HFILE" ) ) > 0 ) {
		if( t < argc ) {
			hFile = (HANDLE) (intptr_t) atol( ccom_argv[t + 1] );
		}
	}

	if( ( t = CCheckParm( "-HPARENT" ) ) > 0 ) {
		if( t < argc ) {
			heventParent = (HANDLE) (intptr_t) atol( ccom_argv[t + 1] );
		}
	}

	if( ( t = CCheckParm( "-HCHILD" ) ) > 0 ) {
		if( t < argc ) {
			heventChild = (HANDLE) (intptr_t) atol( ccom_argv[t + 1] );
		}
	}


	// ignore if we don't have all the events.
	if( !hFile || !heventParent || !heventChild ) {
		printf( "Qhost not present.\n" );
		return;
	}

	printf( "Initializing for qhost.\n" );

	hfileBuffer = hFile;
	heventParentSend = heventParent;
	heventChildSend = heventChild;

	// so we'll know when to go away.
	heventDone = CreateEvent( NULL, FALSE, FALSE, NULL );

	if( !heventDone ) {
		printf( "Couldn't create heventDone\n" );
		return;
	}

	if( !_beginthreadex( NULL, 0, RequestProc, NULL, 0, &threadAddr ) ) {
		CloseHandle( heventDone );
		printf( "Couldn't create QHOST thread\n" );
		return;
	}

	// save off the input/output handles.
	hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
	hStdin = GetStdHandle( STD_INPUT_HANDLE );

	// force 80 character width, at least 25 character height
	SetConsoleCXCY( hStdout, 80, 25 );
}


void DeinitConProc( void ) {
	if( heventDone ) {
		SetEvent( heventDone );
	}
}


static unsigned WINAPI RequestProc( void *arg ) {
	int *pBuffer;
	DWORD dwRet;
	HANDLE heventWait[2];
	int iBeginLine, iEndLine;

	heventWait[0] = heventParentSend;
	heventWait[1] = heventDone;

	for(;; ) {
		dwRet = WaitForMultipleObjects( 2, heventWait, FALSE, INFINITE );

		// heventDone fired, so we're exiting.
		if( dwRet == WAIT_OBJECT_0 + 1 ) {
			break;
		}

		pBuffer = (int *) GetMappedBuffer( hfileBuffer );

		// hfileBuffer is invalid.  Just leave.
		if( !pBuffer ) {
			printf( "Invalid hfileBuffer\n" );
			break;
		}

		switch( pBuffer[0] ) {
			case CCOM_WRITE_TEXT:
				// Param1 : Text
				pBuffer[0] = WriteText( (LPCTSTR) ( pBuffer + 1 ) );
				break;

			case CCOM_GET_TEXT:
				// Param1 : Begin line
				// Param2 : End line
				iBeginLine = pBuffer[1];
				iEndLine = pBuffer[2];
				pBuffer[0] = ReadText( (LPTSTR) ( pBuffer + 1 ), iBeginLine,
									   iEndLine );
				break;

			case CCOM_GET_SCR_LINES:
				// No params
				pBuffer[0] = GetScreenBufferLines( &pBuffer[1] );
				break;

			case CCOM_SET_SCR_LINES:
				// Param1 : Number of lines
				pBuffer[0] = SetScreenBufferLines( pBuffer[1] );
				break;
		}

		ReleaseMappedBuffer( pBuffer );
		SetEvent( heventChildSend );
	}

	_endthreadex( 0 );
	arg = NULL; // wsw : aiwa : shut up compiler
	return 0;
}


static LPVOID GetMappedBuffer( HANDLE hBuffer ) {
	LPVOID pBuffer;

	pBuffer = MapViewOfFile( hBuffer,
							 FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0 );

	return pBuffer;
}


static void ReleaseMappedBuffer( LPVOID pBuffer ) {
	UnmapViewOfFile( pBuffer );
}


static BOOL GetScreenBufferLines( int *piLines ) {
	CONSOLE_SCREEN_BUFFER_INFO info;
	BOOL bRet;

	bRet = GetConsoleScreenBufferInfo( hStdout, &info );

	if( bRet ) {
		*piLines = info.dwSize.Y;
	}

	return bRet;
}


static BOOL SetScreenBufferLines( int iLines ) {

	return SetConsoleCXCY( hStdout, 80, iLines );
}


static BOOL ReadText( LPTSTR pszText, int iBeginLine, int iEndLine ) {
	COORD coord;
	DWORD dwRead;
	BOOL bRet;

	coord.X = 0;
	coord.Y = (short)iBeginLine;

	bRet = ReadConsoleOutputCharacter(
		hStdout,
		pszText,
		80 * ( iEndLine - iBeginLine + 1 ),
		coord,
		&dwRead );

	// Make sure it's null terminated.
	if( bRet ) {
		pszText[dwRead] = '\0';
	}

	return bRet;
}


static BOOL WriteText( LPCTSTR szText ) {
	DWORD dwWritten;
	INPUT_RECORD rec;
	char upper, *sz;

	sz = (LPTSTR) szText;

	while( *sz ) {
		// 13 is the code for a carriage return (\n) instead of 10.
		if( *sz == 10 ) {
			*sz = 13;
		}

		upper = (char) toupper( *sz );

		rec.EventType = KEY_EVENT;
		rec.Event.KeyEvent.bKeyDown = TRUE;
		rec.Event.KeyEvent.wRepeatCount = 1;
		rec.Event.KeyEvent.wVirtualKeyCode = upper;
		rec.Event.KeyEvent.wVirtualScanCode = (WORD) CharToCode( *sz );
		rec.Event.KeyEvent.uChar.AsciiChar = *sz;
		rec.Event.KeyEvent.uChar.UnicodeChar = *sz;
		rec.Event.KeyEvent.dwControlKeyState = isupper( *sz ) ? 0x80 : 0x0;

		WriteConsoleInput(
			hStdin,
			&rec,
			1,
			&dwWritten );

		rec.Event.KeyEvent.bKeyDown = FALSE;

		WriteConsoleInput(
			hStdin,
			&rec,
			1,
			&dwWritten );

		sz++;
	}

	return TRUE;
}


static int CharToCode( char c ) {
	char upper;

	upper = (char) toupper( c );

	switch( c ) {
		case 13:
			return 28;

		default:
			break;
	}

	if( isalpha( c ) ) {
		return ( 30 + upper - 65 );
	}

	if( isdigit( c ) ) {
		return ( 1 + upper - 47 );
	}

	return c;
}


static BOOL SetConsoleCXCY( HANDLE hStdout_, int cx, int cy ) {
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD coordMax;

	coordMax = GetLargestConsoleWindowSize( hStdout_ );

	if( cy > coordMax.Y ) {
		cy = coordMax.Y;
	}

	if( cx > coordMax.X ) {
		cx = coordMax.X;
	}

	if( !GetConsoleScreenBufferInfo( hStdout_, &info ) ) {
		return FALSE;
	}

	// height
	info.srWindow.Left = 0;
	info.srWindow.Right = info.dwSize.X - 1;
	info.srWindow.Top = 0;
	info.srWindow.Bottom = (SHORT) ( cy - 1 );

	if( cy < info.dwSize.Y ) {
		if( !SetConsoleWindowInfo( hStdout_, TRUE, &info.srWindow ) ) {
			return FALSE;
		}

		info.dwSize.Y = (SHORT) cy;

		if( !SetConsoleScreenBufferSize( hStdout_, info.dwSize ) ) {
			return FALSE;
		}
	} else if( cy > info.dwSize.Y ) {
		info.dwSize.Y = (SHORT) cy;

		if( !SetConsoleScreenBufferSize( hStdout_, info.dwSize ) ) {
			return FALSE;
		}

		if( !SetConsoleWindowInfo( hStdout_, TRUE, &info.srWindow ) ) {
			return FALSE;
		}
	}

	if( !GetConsoleScreenBufferInfo( hStdout_, &info ) ) {
		return FALSE;
	}

	// width
	info.srWindow.Left = 0;
	info.srWindow.Right = (SHORT) ( cx - 1 );
	info.srWindow.Top = 0;
	info.srWindow.Bottom = info.dwSize.Y - 1;

	if( cx < info.dwSize.X ) {
		if( !SetConsoleWindowInfo( hStdout_, TRUE, &info.srWindow ) ) {
			return FALSE;
		}

		info.dwSize.X = (SHORT) cx;

		if( !SetConsoleScreenBufferSize( hStdout_, info.dwSize ) ) {
			return FALSE;
		}
	} else if( cx > info.dwSize.X ) {
		info.dwSize.X = (SHORT) cx;

		if( !SetConsoleScreenBufferSize( hStdout_, info.dwSize ) ) {
			return FALSE;
		}

		if( !SetConsoleWindowInfo( hStdout_, TRUE, &info.srWindow ) ) {
			return FALSE;
		}
	}

	return TRUE;
}
