#include <SDL.h>
#include "../client/client.h"

/*
* Sys_GetClipboardData
*/
char *Sys_GetClipboardData( bool primary )
{
	return SDL_GetClipboardText();
}

/*
* Sys_SetClipboardData
*/
bool Sys_SetClipboardData( const char *data )
{
	return SDL_SetClipboardText( data );
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data )
{
	SDL_free( data );
}