#include <sys/time.h>
#include "../qcommon/qcommon.h"

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
# define HAVE_CLOCKGETTIME 1
#endif

/*
* Sys_Microseconds
*/
uint64_t Sys_Microseconds( void ) {
	static time_t sys_secbase;

#ifdef HAVE_CLOCKGETTIME
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );

	if( !sys_secbase ) {
		sys_secbase = ts.tv_sec;
		return ts.tv_nsec / 1000;
	}

	// TODO handle the wrap
	return (uint64_t)( ts.tv_sec - sys_secbase ) * 1000000 + ts.ts_nsec / 1000;
#else
	struct timeval tp;
	gettimeofday( &tp, NULL );

	if( !sys_secbase ) {
		sys_secbase = tp.tv_sec;
		return tp.tv_usec;
	}

	// TODO handle the wrap
	return (uint64_t)( tp.tv_sec - sys_secbase ) * 1000000 + tp.tv_usec;
#endif
}

/*
* Sys_Milliseconds
*/
int64_t Sys_Milliseconds( void ) {
	return Sys_Microseconds() / 1000;
}
