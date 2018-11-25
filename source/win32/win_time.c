#include "qcommon/qcommon.h"
#include <windows.h>

static uint64_t hwtimer_freq;

void Sys_InitTime( void ) {
	QueryPerformanceFrequency( (LARGE_INTEGER *) &hwtimer_freq );
}

uint64_t Sys_Microseconds( void ) {
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
	return Sys_Microseconds() / 1000;
}
