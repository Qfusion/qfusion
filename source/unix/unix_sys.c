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

/* we need __APPLE__ here because __MACOSX__ is defined in ../game/q_shared.h from ../qcommon/qcommon.h
which defines HAVE_STRCASECMP if SDL.h isn't called first, causing a bunch of warnings
FIXME:  This will be remidied once a native Mac port is complete
*/
#if defined ( __APPLE__ ) && !defined ( DEDICATED_ONLY )
#include <SDL/SDL.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#if defined ( __FreeBSD__ )
#include <machine/param.h>
#endif

#include "../qcommon/qcommon.h"
#include "glob.h"

cvar_t *nostdout;
static qboolean nostdout_backup_val = qfalse;

unsigned sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = qtrue;

// =======================================================================
// General routines
// =======================================================================

#ifndef DEDICATED_ONLY
extern void GLimp_Shutdown( void );
extern void IN_Shutdown( void );
#endif

static void signal_handler( int sig )
{
	static int try = 0;

	switch( try++ )
	{
	case 0:
		if( sig == SIGINT || sig == SIGTERM )
		{
			Com_Printf( "Received signal %d, exiting...\n", sig );
			Com_Quit();
		}
		else
		{
			Com_Error( ERR_FATAL, "Received signal %d\n", sig );
		}
		break;
	case 1:
#ifndef DEDICATED_ONLY
		printf( "Received signal %d, exiting...\n", sig );
		IN_Shutdown();
		GLimp_Shutdown();
		_exit( 1 );
		break;
	case 2:
#endif
		printf( "Received signal %d, exiting...\n", sig );
		_exit( 1 );
		break;

	default:
		_exit( 1 );
		break;
	}
}

static void InitSig( void )
{
	signal( SIGHUP, signal_handler );
	signal( SIGQUIT, signal_handler );
	signal( SIGILL, signal_handler );
	signal( SIGTRAP, signal_handler );
	signal( SIGIOT, signal_handler );
	signal( SIGBUS, signal_handler );
	signal( SIGFPE, signal_handler );
	signal( SIGSEGV, signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGINT, signal_handler );
}

static void Sys_AnsiColorPrint( const char *msg )
{
	static char buffer[2096];
	int         length = 0;
	static int  q3ToAnsi[ 8 ] =
	{
		30, // COLOR_BLACK
		31, // COLOR_RED
		32, // COLOR_GREEN
		33, // COLOR_YELLOW
		34, // COLOR_BLUE
		36, // COLOR_CYAN
		35, // COLOR_MAGENTA
		0   // COLOR_WHITE
	};

	while( *msg )
	{
		char c = *msg;
		int colorindex;

		int gc = Q_GrabCharFromColorString( &msg, &c, &colorindex );
		if( gc == GRABCHAR_COLOR || (gc == GRABCHAR_CHAR && c == '\n') )
		{
			// First empty the buffer
			if( length > 0 )
			{
				buffer[length] = '\0';
				fputs( buffer, stdout );
				length = 0;
			}

			if( c == '\n' )
			{
				// Issue a reset and then the newline
				fputs( "\033[0m\n", stdout );
			}
			else
			{
				// Print the color code
				Q_snprintfz( buffer, sizeof( buffer ), "\033[%dm", q3ToAnsi[ colorindex ] );
				fputs( buffer, stdout );
			}
		}
		else if( gc == GRABCHAR_END )
			break;
		else
		{
			if( length >= sizeof( buffer ) - 1 )
				break;
			buffer[length++] = c;
		}
	}

	// Empty anything still left in the buffer
	if( length > 0 )
	{
		buffer[length] = '\0';
		fputs( buffer, stdout );
	}
}

void Sys_ConsoleOutput( char *string )
{
	if( nostdout && nostdout->integer )
		return;
	if( nostdout_backup_val )
		return;

#if 0
	fputs( string, stdout );
#else
	Sys_AnsiColorPrint( string );
#endif
}

/*
* Sys_Quit
*/
void Sys_Quit( void )
{
	// Qcommon_Shutdown is going destroy the cvar, so backup its value now
	// and invalidate the pointer
	nostdout_backup_val = (nostdout && nostdout->integer ? qtrue : qfalse);
	nostdout = NULL;

	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) & ~FNDELAY );

	Qcommon_Shutdown();

	exit( 0 );
}

/*
* Sys_Init
*/
void Sys_Init( void )
{
}

/*
* Sys_InitDynvars
*/
void Sys_InitDynvars( void )
{
}

/*
* Sys_Error
*/
void Sys_Error( const char *format, ... )
{
	static qboolean	recursive = qfalse;
	va_list	argptr;
	char string[1024];

	// change stdin to non blocking
	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) & ~FNDELAY );

	va_start( argptr, format );
	Q_vsnprintfz( string, sizeof( string ), format, argptr );
	va_end( argptr );

	if( recursive )
	{
		fprintf( stderr, "Recursive Sys_Error: %s\n", string );
		_exit( 1 );
	}

	recursive = qtrue;

	fprintf( stderr, "Error: %s\n", string );

	CL_Shutdown();
	Qcommon_Shutdown();

	_exit( 1 );
}

/*
* Sys_Microseconds
*/
static unsigned long sys_secbase;
quint64 Sys_Microseconds( void )
{
	struct timeval tp;
	struct timezone tzp;

	gettimeofday( &tp, &tzp );

	if( !sys_secbase )
	{
		sys_secbase = tp.tv_sec;
		return tp.tv_usec;
	}

	// TODO handle the wrap
	return (quint64)( tp.tv_sec - sys_secbase )*1000000 + tp.tv_usec;
}

/*
* Sys_Milliseconds
*/
unsigned int Sys_Milliseconds( void )
{
	return Sys_Microseconds() / 1000;
}

/*
* Sys_XTimeToSysTime
* 
* Sub-frame timing of events returned by X
* Ported from Quake III Arena source code.
*/
int Sys_XTimeToSysTime( unsigned long xtime )
{
	int ret, time, test;

	// some X servers (like suse 8.1's) report weird event times
	// if the game is loading, resolving DNS, etc. we are also getting old events
	// so we only deal with subframe corrections that look 'normal'
	ret = xtime - (unsigned long)(sys_secbase * 1000);
	time = Sys_Milliseconds();
	test = time - ret;

	if( test < 0 || test > 30 ) // in normal conditions I've never seen this go above
		return time;
	return ret;
}

/*
* Sys_EvdevTimeToSysTime
* 
* Sub-frame timing of events returned by evdev.
*/
int Sys_EvdevTimeToSysTime( struct timeval *tp )
{
	return ( ( tp->tv_sec - sys_secbase )*1000000 + tp->tv_usec ) / 1000;
}
/*
* Sys_Sleep
*/
void Sys_Sleep( unsigned int millis )
{
	usleep( millis * 1000 );
}

static void floating_point_exception_handler( int whatever )
{
	signal( SIGFPE, floating_point_exception_handler );
}

char *Sys_ConsoleInput( void )
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if( !dedicated || !dedicated->integer )
		return NULL;

	if( !stdin_active )
		return NULL;

	FD_ZERO( &fdset );
	FD_SET( 0, &fdset ); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if( select( 1, &fdset, NULL, NULL, &timeout ) == -1 || !FD_ISSET( 0, &fdset ) )
		return NULL;

	len = read( 0, text, sizeof( text ) );
	if( len == 0 )
	{           // eof!
		Com_Printf( "EOF from stdin, console input disabled...\n" );
		stdin_active = qfalse;
		return NULL;
	}

	if( len < 1 )
		return NULL;

	text[len-1] = 0; // rip off the /n and terminate

	return text;
}

/*
* Sys_GetSymbol
*/
#ifdef SYS_SYMBOL
void *Sys_GetSymbol( const char *moduleName, const char *symbolName )
{
	// FIXME: Does not work on Debian64 for unknown reasons (dlsym always returns NULL)
	void *const module = dlopen( moduleName, RTLD_NOW );
	if( module )
	{
		void *const symbol = dlsym( module, symbolName );
		dlclose( module );
		return symbol;
	}
	else
		return NULL;
}
#endif // SYS_SYMBOL

//===============================================================================

/*
* Sys_AppActivate
*/
void Sys_AppActivate( void )
{
}

/*
* Sys_SendKeyEvents
*/
void Sys_SendKeyEvents( void )
{
	// grab frame time
	sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/

int main( int argc, char **argv )
{
	unsigned int oldtime, newtime, time;

	InitSig();

#if defined ( __MACOSX__ ) && !defined (DEDICATED_ONLY)
	SDL_Init( 0 );
	SDL_EnableUNICODE( SDL_ENABLE );
#endif

	Qcommon_Init( argc, argv );

	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | FNDELAY );

	nostdout = Cvar_Get( "nostdout", "0", 0 );
	if( !nostdout->integer )
	{
		fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | FNDELAY );
	}

	oldtime = Sys_Milliseconds();
	while( qtrue )
	{
		// find time spent rendering last frame
		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
			if( time > 0 )
				break;
#ifdef PUTCPU2SLEEP
			Sys_Sleep( 0 );
#endif
		}
		while( 1 );
		oldtime = newtime;

		Qcommon_Frame( time );
	}
#if defined ( __MACOSX__ ) && !defined (DEDICATED_ONLY)
	SDL_Quit();
#endif
}
