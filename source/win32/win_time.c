#include "../qcommon/qcommon.h"
#include <windows.h>

qboolean	hwtimer;
dynvar_t	*hwtimer_var;
quint64		hwtimer_freq;
int			milli_offset = 0;
qint64		micro_offset = 0;

/*
* Sys_Milliseconds
*/
// wsw: pb adapted High Res Performance Counter code from ezquake

void Sys_InitTime( void )
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

unsigned int Sys_Milliseconds_TGT( void )
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

quint64 Sys_Microseconds_QPC( void )
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

	return ( ( p_now - p_start ) * 1000000 ) / hwtimer_freq;
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