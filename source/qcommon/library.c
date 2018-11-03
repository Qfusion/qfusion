/*
Copyright (C) 2008 Chasseur de bots

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

#include "qcommon.h"
#include "sys_library.h"

/*
* Com_UnloadLibrary
*/
void Com_UnloadLibrary( void **lib ) {
	if( lib && *lib ) {
		if( !Sys_Library_Close( *lib ) ) {
			Com_Error( ERR_FATAL, "Sys_CloseLibrary failed" );
		}
		*lib = NULL;
	}
}

/*
* Com_LoadLibraryExt
*/
void *Com_LoadLibraryExt( const char *name, dllfunc_t *funcs, bool sys ) {
	void *lib;
	dllfunc_t *func;
	const char *fullname;

	if( !name || !name[0] || !funcs ) {
		return NULL;
	}

	Com_DPrintf( "LoadLibrary (%s)\n", name );

	if( sys ) {
		fullname = name;
	} else {
		fullname = Sys_Library_GetFullName( name );
	}
	if( !fullname ) {
		Com_DPrintf( "LoadLibrary (%s):(Not found)\n", name );
		return NULL;
	}

	lib = Sys_Library_Open( fullname );
	if( !lib ) {
		if( !sys ) {
			Com_Printf( "LoadLibrary (%s):(%s)\n", fullname, Sys_Library_ErrorString() );
		} else {
			Com_DPrintf( "LoadLibrary (%s):(%s)\n", fullname, Sys_Library_ErrorString() );
		}
		return NULL;
	}

	for( func = funcs; func->name; func++ ) {
		*( func->funcPointer ) = Sys_Library_ProcAddress( lib, func->name );

		if( !( *( func->funcPointer ) ) ) {
			Com_UnloadLibrary( &lib );
			if( sys ) {
				Com_DPrintf( "%s: Sys_GetProcAddress failed for %s\n", fullname, func->name );
				return NULL;
			}
			Com_Error( ERR_FATAL, "%s: Sys_GetProcAddress failed for %s", fullname, func->name );
		}
	}

	return lib;
}

/*
* Com_LoadSysLibrary
*/
void *Com_LoadSysLibrary( const char *name, dllfunc_t *funcs ) {
	char *names;
	size_t names_size;
	char *s, *saveptr;
	void *lib = NULL;

	names_size = strlen( name ) + 1;
	names = Q_malloc( names_size );
	memcpy( names, name, names_size );

	s = strtok_r( names, "|", &saveptr );
	while( s != NULL ) {
		lib = Com_LoadLibraryExt( s, funcs, true );
		if( lib ) {
			Com_Printf( "Loaded %s\n", s );
			break;
		}
		s = strtok_r( NULL, "|", &saveptr );
	}

	free( names );
	return lib;
}

/*
* Com_LoadLibrary
*/
void *Com_LoadLibrary( const char *name, dllfunc_t *funcs ) {
	return Com_LoadLibraryExt( name, funcs, false );
}

/*
* Com_LibraryProcAddress
*/
void *Com_LibraryProcAddress( void *lib, const char *name ) {
	return Sys_Library_ProcAddress( lib, name );
}
