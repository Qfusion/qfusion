#include "sdl/SDL.h"
#include "client/client.h"

/*
* Sys_Milliseconds
*/
static Uint64 freq;
static Uint64 base;

void Sys_InitTime( void ) {
	freq = SDL_GetPerformanceFrequency();
	base = SDL_GetPerformanceCounter();
}

int64_t Sys_Milliseconds( void ) {
	return Sys_Microseconds() / 1000;
}

uint64_t Sys_Microseconds( void ) {
	MICROPROFILE_SCOPEI( "Main", "Sys_Microseconds", 0xffffffff );
	return 1000000ULL * ( SDL_GetPerformanceCounter() - base ) / freq;
}
