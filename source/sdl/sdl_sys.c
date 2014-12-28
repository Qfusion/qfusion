#include <SDL.h>
#include "../client/client.h"

/*
* Sys_GetClipboardData
*/
char *Sys_GetClipboardData( qboolean primary )
{
	return SDL_GetClipboardText();
}

/*
* Sys_SetClipboardData
*/
qboolean Sys_SetClipboardData( char *data )
{
	return qtrue;
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	SDL_free( data );
}
