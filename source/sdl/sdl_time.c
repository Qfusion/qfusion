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
	return 1000000 * SDL_GetPerformanceCounter() / freq;
}