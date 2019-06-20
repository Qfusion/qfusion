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
