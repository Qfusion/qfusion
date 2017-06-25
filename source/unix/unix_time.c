#include <sys/time.h>
#include "../qcommon/qcommon.h"

/*
* Sys_Microseconds
*/
static unsigned long sys_secbase;
uint64_t Sys_Microseconds( void ) {
	struct timeval tp;

	gettimeofday( &tp, NULL );

	if( !sys_secbase ) {
		sys_secbase = tp.tv_sec;
		return tp.tv_usec;
	}

	// TODO handle the wrap
	return (uint64_t)( tp.tv_sec - sys_secbase ) * 1000000 + tp.tv_usec;
}

/*
* Sys_Milliseconds
*/
int64_t Sys_Milliseconds( void ) {
	return Sys_Microseconds() / 1000;
}

/*
* Sys_XTimeToSysTime
*
* Sub-frame timing of events returned by X
* Ported from Quake III Arena source code.
*/
int Sys_XTimeToSysTime( unsigned long xtime ) {
	int ret, time, test;

	// some X servers (like suse 8.1's) report weird event times
	// if the game is loading, resolving DNS, etc. we are also getting old events
	// so we only deal with subframe corrections that look 'normal'
	ret = xtime - (unsigned long)( sys_secbase * 1000 );
	time = Sys_Milliseconds();
	test = time - ret;

	if( test < 0 || test > 30 ) { // in normal conditions I've never seen this go above
		return time;
	}
	return ret;
}