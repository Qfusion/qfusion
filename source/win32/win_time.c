#include "../qcommon/qcommon.h"
#include <windows.h>

static bool hwtimer;
static uint64_t hwtimer_freq;
static int64_t milli_offset = 0;
static int64_t micro_offset = 0;

static int64_t Sys_Milliseconds_TGT( void );
static uint64_t Sys_Microseconds_QPC( void );

// wsw: pb adapted High Res Performance Counter code from ezquake

/*
* Sys_SynchronizeTimers
*/
static void Sys_SynchronizeTimers() {
	const int64_t millis = Sys_Milliseconds_TGT();
	const int64_t micros = Sys_Microseconds_QPC();
	const int64_t drift = micros - millis * 1000;

	if( hwtimer ) {
		micro_offset = max( micro_offset, -drift );
	} else {
		milli_offset = max( milli_offset, drift / 1000 );
	}
}

/*
* Sys_InitTime
*/
void Sys_InitTime( void ) {
	hwtimer = QueryPerformanceFrequency( (LARGE_INTEGER *)&hwtimer_freq ) == TRUE;

	if( hwtimer ) {
		Com_Printf( "Using High Resolution Performance Counter\n" );
	}
	else {
		Com_Printf( "Using timeGetTime\n" );
	}

	Sys_SynchronizeTimers();
}

/*
* Sys_Milliseconds
*/

static int64_t Sys_Milliseconds_TGT( void ) {
	static int64_t base;
	static bool initialized = false;
	unsigned int now;

	if( !initialized ) {
		// let base retain 16 bits of effectively random data which is used
		// for quickly generating random numbers
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}

	now = timeGetTime();

	return now - base;
}

static uint64_t Sys_Microseconds_QPC( void ) {
	static bool first = true;
	static int64_t p_start;

	int64_t p_now;
	QueryPerformanceCounter( (LARGE_INTEGER *) &p_now );

	if( first ) {
		first = false;
		p_start = p_now;
	}

	return ( ( p_now - p_start ) * 1000000 ) / hwtimer_freq;
}

int64_t Sys_Milliseconds( void ) {
	if( hwtimer ) {
		return ( Sys_Microseconds_QPC() + micro_offset ) / 1000;
	} else {
		return Sys_Milliseconds_TGT() + milli_offset;
	}
}

uint64_t Sys_Microseconds( void ) {
	if( hwtimer ) {
		return Sys_Microseconds_QPC() + micro_offset;
	} else {
		return (uint64_t)( Sys_Milliseconds_TGT() + milli_offset ) * 1000;
	}
}