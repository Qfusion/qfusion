#include "../qcommon/qcommon.h"
#include <windows.h>

static bool hwtimer;
static dynvar_t *hwtimer_var;
static uint64_t hwtimer_freq;
static int milli_offset = 0;
static int64_t micro_offset = 0;

static unsigned int Sys_Milliseconds_TGT( void );
static uint64_t Sys_Microseconds_QPC( void );

// wsw: pb adapted High Res Performance Counter code from ezquake

/*
* Sys_GetHwTimer_f
*/
static dynvar_get_status_t Sys_GetHwTimer_f( void **val )
{
	static char hwtimerStr[2] = { '\0', '\0' };
	hwtimerStr[0] = '0' + hwtimer;
	assert( val );
	*val = hwtimerStr;
	return DYNVAR_GET_OK;
}

/*
* Sys_SetHwTimer_f
*/
static dynvar_set_status_t Sys_SetHwTimer_f( void *val )
{
	assert( val );
	switch( *( (char *) val ) )
	{
	case '0':
		hwtimer = 0;
		return DYNVAR_SET_OK;
	case '1':
		if( hwtimer_freq )
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

/*
* Sys_SynchronizeTimers_f
*/
static void Sys_SynchronizeTimers_f( void *val )
{
	static int hwtimer_old = -1;

	const unsigned int millis = Sys_Milliseconds_TGT();
	const int64_t micros = Sys_Microseconds_QPC();
	const int64_t drift = micros - millis * 1000;

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
* Sys_InitTimeDynvar
*/
void Sys_InitTimeDynvar( void )
{
	QueryPerformanceFrequency( (LARGE_INTEGER *) &hwtimer_freq );

	hwtimer_var = Dynvar_Create( "sys_hwtimer", 1, Sys_GetHwTimer_f, Sys_SetHwTimer_f );
	assert( hwtimer_var );
	Dynvar_AddListener( hwtimer_var, Sys_SynchronizeTimers_f );
	Dynvar_SetValue( hwtimer_var, "0" );
}

/*
* Sys_InitTime
*/
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

/*
* Sys_Milliseconds
*/

static unsigned int Sys_Milliseconds_TGT( void )
{
	static unsigned int base;
	static bool	initialized = false;
	unsigned int now;

	if( !initialized )
	{
		// let base retain 16 bits of effectively random data which is used
		//for quickly generating random numbers
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}

	now = timeGetTime();

	return now - base;
}

static uint64_t Sys_Microseconds_QPC( void )
{
	static bool first = true;
	static int64_t p_start;

	int64_t p_now;
	QueryPerformanceCounter( (LARGE_INTEGER *) &p_now );

	if( first )
	{
		first = false;
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

uint64_t Sys_Microseconds( void )
{
	if( hwtimer )
		return Sys_Microseconds_QPC() + micro_offset;
	else
		return (uint64_t)( Sys_Milliseconds_TGT() + milli_offset ) * 1000;
}