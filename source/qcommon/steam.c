/*
Copyright (C) 2014 Victor Luchits

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
#include "../qcommon/qcommon.h"
#include "../steamlib/steamlib_public.h"

static steamlib_export_t *steamlib_export;
static void *steamlib_libhandle = NULL;
static qboolean steamlib_initialized = qfalse;

void Steam_LoadLibrary( void );
void Steam_UnloadLibrary( void );

typedef void ( *com_error_t )( int code, const char *format, ... );

/*
* Steam_LoadLibrary
*/
void Steam_LoadLibrary( void )
{
	static steamlib_import_t import;
	dllfunc_t funcs[2];
	void *( *GetSteamLibAPI )(void *);

	assert( !steamlib_libhandle );

	import.Com_Error = (com_error_t)Com_Error;
	import.Com_Printf = Com_Printf;
	import.Com_DPrintf = Com_DPrintf;

	// load dynamic library
	Com_Printf( "Loading Steam module... " );
	funcs[0].name = "GetSteamLibAPI";
	funcs[0].funcPointer = (void **) &GetSteamLibAPI;
	funcs[1].name = NULL;
	steamlib_libhandle = Com_LoadLibrary( LIB_DIRECTORY "/steamlib_" ARCH LIB_SUFFIX, funcs );

	if( steamlib_libhandle )
	{
		// load succeeded
		int api_version;

		steamlib_export = GetSteamLibAPI( &import );
		api_version = steamlib_export->API();

		if( api_version != STEAMLIB_API_VERSION )
		{
			// wrong version
			Com_Printf( "Wrong version: %i, not %i.\n", api_version, STEAMLIB_API_VERSION );
			Steam_UnloadLibrary();
		}
		else
		{
			Com_Printf( "Success.\n" );
		}
	}
	else
	{
		Com_Printf( "Not found.\n" );
	}
}

/*
* Steam_UnloadLibrary
*/
void Steam_UnloadLibrary( void )
{
	if( steamlib_libhandle ) {
		steamlib_export->Shutdown();

		Com_UnloadLibrary( &steamlib_libhandle );
		assert( !steamlib_libhandle );

		Com_Printf( "Steam module unloaded.\n" );
	}

	steamlib_export = NULL;
	steamlib_initialized = qfalse;
}

/*
* Steam_Init
*/
void Steam_Init( void )
{
	if( steamlib_export ) {
		if( !steamlib_export->Init() ) {
			Com_Printf( "Steam initialization failed.\n" );
			return;
		}
		steamlib_initialized = qtrue;
	}
}

/*
* Steam_RunFrame
*/
void Steam_RunFrame( void )
{
	if( steamlib_initialized ) {
		steamlib_export->RunFrame();
	}
}

/*
* Steam_Shutdown
*/
void Steam_Shutdown( void )
{
	if( steamlib_initialized ) {
		steamlib_export->Shutdown();
	}
}
