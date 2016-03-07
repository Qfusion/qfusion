#include <SDL.h>

#include "../qcommon/qcommon.h"
#include "../qcommon/sys_library.h"

/*
* Sys_Library_Close
*/
bool Sys_Library_Close( void *lib )
{
	SDL_UnloadObject( lib );
	return true;
}

/*
* Sys_Library_GetFullName
*/
const char *Sys_Library_GetFullName( const char *name )
{
	return FS_AbsoluteNameForBaseFile( name );
}

/*
* Sys_Library_GetGameLibPath
*/
const char *Sys_Library_GetGameLibPath( const char *name, int64_t time, int randomizer )
{
	static char tempname[1024 * 10];
	Q_snprintfz( tempname, sizeof( tempname ), "%s/%s/tempmodules_%lld_%d_%d/%s", FS_RuntimeDirectory(), FS_GameDirectory(), 
		time, Sys_GetCurrentProcessId(), randomizer, name );
	return tempname;
}

/*
* Sys_Library_Open
*/
void *Sys_Library_Open( const char *name )
{
	return SDL_LoadObject( name );
}

/*
* Sys_Library_ProcAddress
*/
void *Sys_Library_ProcAddress( void *lib, const char *apifuncname )
{
	return SDL_LoadFunction( lib, apifuncname );
}

/*
* Sys_Library_ErrorString
*/
const char *Sys_Library_ErrorString( void )
{
	return (char *)SDL_GetError();
}
