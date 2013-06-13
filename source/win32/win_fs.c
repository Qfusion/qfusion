/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../qcommon/sys_fs.h"

#include "winquake.h"
#include <direct.h>
#include <shlobj.h>

#ifndef CSIDL_APPDATA
# define CSIDL_APPDATA					0x001A
#endif

static char *findbase = NULL;
static char *findpath = NULL;
static size_t findpath_size = 0;
static int findhandle = -1;

/*
* CompareAttributes
*/
static qboolean CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	if( ( found & _A_RDONLY ) && ( canthave & SFF_RDONLY ) )
		return qfalse;
	if( ( found & _A_HIDDEN ) && ( canthave & SFF_HIDDEN ) )
		return qfalse;
	if( ( found & _A_SYSTEM ) && ( canthave & SFF_SYSTEM ) )
		return qfalse;
	if( ( found & _A_SUBDIR ) && ( canthave & SFF_SUBDIR ) )
		return qfalse;
	if( ( found & _A_ARCH ) && ( canthave & SFF_ARCH ) )
		return qfalse;

	if( ( musthave & SFF_RDONLY ) && !( found & _A_RDONLY ) )
		return qfalse;
	if( ( musthave & SFF_HIDDEN ) && !( found & _A_HIDDEN ) )
		return qfalse;
	if( ( musthave & SFF_SYSTEM ) && !( found & _A_SYSTEM ) )
		return qfalse;
	if( ( musthave & SFF_SUBDIR ) && !( found & _A_SUBDIR ) )
		return qfalse;
	if( ( musthave & SFF_ARCH ) && !( found & _A_ARCH ) )
		return qfalse;

	return qtrue;
}

/*
* Sys_FS_FindFirst
*/
const char *Sys_FS_FindFirst( const char *path, unsigned musthave, unsigned canthave )
{
	size_t size;
	struct _finddata_t findinfo;

	assert( path );
	assert( findhandle == -1 );
	assert( !findbase && !findpath && !findpath_size );

	if( findhandle != -1 )
		Sys_Error( "Sys_FindFirst without close" );

	findbase = Mem_TempMalloc( sizeof( char ) * ( strlen( path ) + 1 ) );
	Q_strncpyz( findbase, path, ( strlen( path ) + 1 ) );
	COM_StripFilename( findbase );

	findhandle = _findfirst( path, &findinfo );

	if( findhandle == -1 )
		return NULL;

	if( strcmp( findinfo.name, "." ) && strcmp( findinfo.name, ".." ) &&
		CompareAttributes( findinfo.attrib, musthave, canthave ) )
	{
		size = sizeof( char ) * ( strlen( findbase ) + 1 + strlen( findinfo.name ) + 1 );
		if( findpath_size < size )
		{
			if( findpath )
				Mem_TempFree( findpath );
			findpath_size = size * 2; // extra space to reduce reallocs
			findpath = Mem_TempMalloc( findpath_size );
		}

		Q_snprintfz( findpath, findpath_size, "%s/%s", findbase, findinfo.name );
		return findpath;
	}

	return Sys_FS_FindNext( musthave, canthave );
}

/*
* Sys_FS_FindNext
*/
const char *Sys_FS_FindNext( unsigned musthave, unsigned canthave )
{
	size_t size;
	struct _finddata_t findinfo;

	assert( findhandle != -1 );
	assert( findbase );

	if( findhandle == -1 )
		return NULL;

	while( _findnext( findhandle, &findinfo ) != -1 )
	{
		if( strcmp( findinfo.name, "." ) && strcmp( findinfo.name, ".." ) &&
			CompareAttributes( findinfo.attrib, musthave, canthave ) )
		{
			size = sizeof( char ) * ( strlen( findbase ) + 1 + strlen( findinfo.name ) + 1 );
			if( findpath_size < size )
			{
				if( findpath )
					Mem_TempFree( findpath );
				findpath_size = size * 2; // extra space to reduce reallocs
				findpath = Mem_TempMalloc( findpath_size );
			}

			Q_snprintfz( findpath, findpath_size, "%s/%s", findbase, findinfo.name );
			return findpath;
		}
	}

	return NULL;
}

/*
* Sys_FS_FindClose
*/
void Sys_FS_FindClose( void )
{
	assert( findbase );

	if( findhandle != -1 )
	{
		_findclose( findhandle );
		findhandle = -1;
	}

	Mem_TempFree( findbase );
	findbase = NULL;

	if( findpath )
	{
		Mem_TempFree( findpath );
		findpath = NULL;
		findpath_size = 0;
	}
}

/*
* Sys_FS_GetHomeDirectory
*/
const char *Sys_FS_GetHomeDirectory( void )
{
	static char home[MAX_PATH] = { '\0' };
#ifndef SHGetFolderPath
	HINSTANCE shFolderDll = LoadLibrary( "shfolder.dll" );

	if( !shFolderDll )
		shFolderDll = LoadLibrary( "shell32.dll" );

	SHGetFolderPath = GetProcAddress( shFolderDll, "SHGetFolderPathA" );
	if( SHGetFolderPath )
		SHGetFolderPath( NULL, CSIDL_APPDATA, 0, 0, home );

	FreeLibrary( shFolderDll );
#else
	SHGetFolderPath( 0, CSIDL_APPDATA, 0, 0, home );
#endif
	return (home[0] == '\0' ? NULL : COM_SanitizeFilePath( home ) );
}

/*
* Sys_FS_LockFile
*/
void *Sys_FS_LockFile( const char *path )
{
	HANDLE handle;

	handle = CreateFile( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if( handle == INVALID_HANDLE_VALUE )
		return NULL;
	return (void *)handle;
}

/*
* Sys_FS_UnlockFile
*/
void Sys_FS_UnlockFile( void *handle )
{
	CloseHandle( (HANDLE)handle );
}

/*
* Sys_FS_CreateDirectory
*/
qboolean Sys_FS_CreateDirectory( const char *path )
{
	return ( !_mkdir( path ) );
}

/*
* Sys_FS_RemoveDirectory
*/
qboolean Sys_FS_RemoveDirectory( const char *path )
{
	return ( !_rmdir( path ) );
}

/*
* Sys_FS_FileMTime
*/
time_t Sys_FS_FileMTime( const char *filename )
{
	HANDLE hFile;
	FILETIME ft;
	time_t time = 0;

	hFile = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if( hFile == INVALID_HANDLE_VALUE ) {
		// the file doesn't exist
		return 0;
	}

    // retrieve the last-write file time for the file
	if( GetFileTime( hFile, NULL, NULL, &ft ) ) {
		ULARGE_INTEGER ull;

		// FILETIME is the number of 100-nanosecond intervals since January 1, 1601.
		// time_t is the number of 1-second intervals since January 1, 1970.

		ull.LowPart = ft.dwLowDateTime;
		ull.HighPart = ft.dwHighDateTime;
		time = ull.QuadPart / 10000000ULL - 11644473600ULL;
	}

	CloseHandle( hFile );

	return time;
}
