/*
Copyright (C) 2015 SiPlus, Chasseur de bots

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

#include <time.h>
#include "android_sys.h"

/*
* Sys_Android_Microseconds
*/
uint64_t Sys_Android_Microseconds( void )
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return now.tv_sec * ( ( uint64_t )1000000 ) + now.tv_nsec / ( ( uint64_t )1000 );
}

/*
* Sys_Microseconds
*/
uint64_t Sys_Microseconds( void )
{
	static uint64_t base;
	uint64_t now;

	now = Sys_Android_Microseconds();

	if( !base )
		base = now;

	return now - base;
}

/*
* Sys_Milliseconds
*/
unsigned int Sys_Milliseconds( void )
{
	return Sys_Microseconds() / ( ( uint64_t )1000 );
}
