#include <SDL.h>
#include "../client/client.h"

/*
* Sys_GetClipboardData
*/
char *Sys_GetClipboardData( void ) {
	if( SDL_HasClipboardText() == SDL_TRUE ) {
		return SDL_GetClipboardText();
	}
	return NULL;
}

/*
* Sys_SetClipboardData
*/
bool Sys_SetClipboardData( const char *data ) {
	return SDL_SetClipboardText( data );
}

/*
* Sys_FreeClipboardData
*/
void Sys_FreeClipboardData( char *data ) {
	SDL_free( data );
}
