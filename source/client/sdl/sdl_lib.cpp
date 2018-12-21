#include "sdl/SDL.h"

#include "qcommon/qcommon.h"
#include "qcommon/sys_library.h"

/*
* Sys_Library_Close
*/
bool Sys_Library_Close( void *lib ) {
	SDL_UnloadObject( lib );
	return true;
}

/*
* Sys_Library_GetFullName
*/
const char *Sys_Library_GetFullName( const char *name ) {
	return FS_AbsoluteNameForBaseFile( name );
}

/*
* Sys_Library_Open
*/
void *Sys_Library_Open( const char *name ) {
	return SDL_LoadObject( name );
}

/*
* Sys_Library_ProcAddress
*/
void *Sys_Library_ProcAddress( void *lib, const char *apifuncname ) {
	return SDL_LoadFunction( lib, apifuncname );
}

/*
* Sys_Library_ErrorString
*/
const char *Sys_Library_ErrorString( void ) {
	return (char *)SDL_GetError();
}
