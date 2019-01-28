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

void Com_UnloadGameLibrary( void **handle ) {
	Sys_Library_Close( *handle );
	*handle = NULL;
}

void *Com_LoadGameLibrary( const char *basename, const char *apifuncname, void **handle, void *parms ) {
	*handle = NULL;

	int libname_size = strlen( LIB_PREFIX ) + strlen( basename ) + strlen( LIB_SUFFIX ) + 1;
	char *libname = ( char* )Mem_TempMalloc( libname_size );
	Q_snprintfz( libname, libname_size, LIB_PREFIX "%s" LIB_SUFFIX, basename );

	const char *abspath = FS_BaseNameForFile( libname );
	if( abspath == NULL ) {
		Com_Printf( "LoadLibrary (%s):(File not found)\n", libname );
		Mem_TempFree( libname );
		return NULL;
	}

	*handle = Sys_Library_Open( abspath );

	if( *handle == NULL ) {
		Com_Printf( "LoadLibrary (%s):(%s)\n", abspath, Sys_Library_ErrorString() );
		Mem_TempFree( libname );
		return NULL;
	}

	void *( *APIfunc )( void * ) = ( void* ( * )( void * ) )Sys_Library_ProcAddress( *handle, apifuncname );
	if( !APIfunc ) {
		Com_Printf( "LoadLibrary (%s):(%s)\n", abspath, Sys_Library_ErrorString() );
		Sys_Library_Close( *handle );
		return NULL;
	}

	Mem_TempFree( libname );

	return APIfunc( parms );
}
