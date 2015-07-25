#include <SDL.h>
#include "../client/client.h"

/*
* Sys_Milliseconds
*/
static Uint64 freq;

void Sys_InitTime( void )
{
	freq = SDL_GetPerformanceFrequency();
}

unsigned int Sys_Milliseconds( void )
{
	return Sys_Microseconds() / 1000;
}

uint64_t Sys_Microseconds( void )
{
	static Uint64 base = 0;	
	if( !base )
		base = SDL_GetPerformanceCounter();
	return 1000000ULL * ( SDL_GetPerformanceCounter() - base ) / freq;
}
