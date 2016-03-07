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
static bool CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	if( ( found & _A_RDONLY ) && ( canthave & SFF_RDONLY ) )
		return false;
	if( ( found & _A_HIDDEN ) && ( canthave & SFF_HIDDEN ) )
		return false;
	if( ( found & _A_SYSTEM ) && ( canthave & SFF_SYSTEM ) )
		return false;
	if( ( found & _A_SUBDIR ) && ( canthave & SFF_SUBDIR ) )
		return false;
	if( ( found & _A_ARCH ) && ( canthave & SFF_ARCH ) )
		return false;

	if( ( musthave & SFF_RDONLY ) && !( found & _A_RDONLY ) )
		return false;
	if( ( musthave & SFF_HIDDEN ) && !( found & _A_HIDDEN ) )
		return false;
	if( ( musthave & SFF_SYSTEM ) && !( found & _A_SYSTEM ) )
		return false;
	if( ( musthave & SFF_SUBDIR ) && !( found & _A_SUBDIR ) )
		return false;
	if( ( musthave & SFF_ARCH ) && !( found & _A_ARCH ) )
		return false;

	return true;
}

/*
* Sys_FS_FindFirst
*/
const char *Sys_FS_FindFirst( const char *path, unsigned musthave, unsigned canthave )
{
	size_t size;
	struct _finddata_t findinfo;
	const char *finame;

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

	finame = findinfo.name;

	if( strcmp( finame, "." ) && strcmp( finame, ".." ) &&
		CompareAttributes( findinfo.attrib, musthave, canthave ) )
	{
		size_t finame_len = strlen( finame );
		size = sizeof( char ) * ( strlen( findbase ) + 1 + finame_len + 1 + 1 );
		if( findpath_size < size )
		{
			if( findpath )
				Mem_TempFree( findpath );
			findpath_size = size * 2; // extra space to reduce reallocs
			findpath = Mem_TempMalloc( findpath_size );
		}

		Q_snprintfz( findpath, findpath_size, "%s/%s%s", findbase, finame, 
			( findinfo.attrib & _A_SUBDIR ) && finame[finame_len-1] != '/' ? "/" : "" );
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
	const char *finame;

	assert( findhandle != -1 );
	assert( findbase );

	if( findhandle == -1 )
		return NULL;

	while( _findnext( findhandle, &findinfo ) != -1 )
	{
		finame = findinfo.name;

		if( strcmp( finame, "." ) && strcmp( finame, ".." ) &&
			CompareAttributes( findinfo.attrib, musthave, canthave ) )
		{
			size_t finame_len = strlen( finame );
			size = sizeof( char ) * ( strlen( findbase ) + 1 + finame_len + 1 + 1 );
			if( findpath_size < size )
			{
				if( findpath )
					Mem_TempFree( findpath );
				findpath_size = size * 2; // extra space to reduce reallocs
				findpath = Mem_TempMalloc( findpath_size );
			}

			Q_snprintfz( findpath, findpath_size, "%s/%s%s", findbase, finame, 
				( findinfo.attrib & _A_SUBDIR ) && finame[finame_len-1] != '/' ? "/" : "" );
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
	if( home[0] != '\0' )
		return home;

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

	if ( home[0] == '\0' )
		return NULL;
	Q_strncpyz( home, va( "%s/%s %d.%d", COM_SanitizeFilePath( home ), APPLICATION, APP_VERSION_MAJOR, APP_VERSION_MINOR ), sizeof( home ) );
	return home;
}

/*
* Sys_FS_GetCacheDirectory
*/
const char *Sys_FS_GetCacheDirectory( void )
{
	return NULL;
}

/*
* Sys_FS_GetSecureDirectory
*/
const char *Sys_FS_GetSecureDirectory( void )
{
	return NULL;
}

/*
* Sys_FS_GetMediaDirectory
*/
const char *Sys_FS_GetMediaDirectory( fs_mediatype_t type )
{
	// TODO: Libraries / My Pictures?
	return NULL;
}

/*
* Sys_FS_GetRuntimeDirectory
*/
const char *Sys_FS_GetRuntimeDirectory( void )
{
	static char temp[MAX_PATH] = { '\0' };

	if( temp[0] == 0 ) {
		DWORD res = GetTempPath( MAX_PATH, temp );
		if( res == 0 || res >= MAX_PATH ) {
			temp[0] = '\0';
			return NULL;
		}
		Q_strncpyz( temp, va( "%s/%s %d.%d", COM_SanitizeFilePath( temp ), APPLICATION, APP_VERSION_MAJOR, APP_VERSION_MINOR ), sizeof( temp ) );		
	}

	return temp[0] == '\0' ? NULL : temp;
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
bool Sys_FS_CreateDirectory( const char *path )
{
	return ( !_mkdir( path ) );
}

/*
* Sys_FS_RemoveDirectory
*/
bool Sys_FS_RemoveDirectory( const char *path )
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

/*
* Sys_FS_FileNo
*/
int Sys_FS_FileNo( FILE *fp )
{
	return _fileno( fp );
}

/*
* Sys_FS_MMapFile
*/
void *Sys_FS_MMapFile( int fileno, size_t size, size_t offset, void **mapping, size_t *mapping_offset )
{
	HANDLE h;
	size_t offsetpad;
	void *data;
	static DWORD granularitymask = 0;

	assert( mapping != NULL );

	h = CreateFileMapping( (HANDLE) _get_osfhandle( fileno ), 0, PAGE_READONLY, 0, 0, 0 );
	if( h == 0 )
		return NULL;
	
	if( granularitymask == 0 ) {
		SYSTEM_INFO sysInfo;
		GetSystemInfo( &sysInfo );
		granularitymask = ~(sysInfo.dwAllocationGranularity - 1);
	}

	offsetpad = offset - (offset & granularitymask);

	data = MapViewOfFile( h, FILE_MAP_READ, 0, offset - offsetpad, size + offsetpad );
	if( !data ) {
		CloseHandle( h );
		return NULL;
	}

	*mapping = h;
	*mapping_offset = offsetpad;
	return (char *)data + offsetpad;
}

/*
* Sys_FS_UnMMapFile
*/
void Sys_FS_UnMMapFile( void *mapping, void *data, size_t size, size_t mapping_offset )
{
	if( data )
		UnmapViewOfFile( (HANDLE)((char *)data - mapping_offset) );
	if( mapping )
		CloseHandle( (HANDLE)mapping );
}

/*
* Sys_FS_AddFileToMedia
*/
void Sys_FS_AddFileToMedia( const char *filename )
{
}
