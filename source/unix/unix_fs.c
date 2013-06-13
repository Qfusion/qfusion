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

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

// Mac OS X and FreeBSD don't know the readdir64 and dirent64
#if ( defined (__FreeBSD__) || !defined(_LARGEFILE64_SOURCE) )
#define readdir64 readdir
#define dirent64 dirent
#endif

static char *findbase = NULL;
static size_t findbase_size = 0;
static const char *findpattern = NULL;
static char *findpath = NULL;
static size_t findpath_size = 0;
static DIR *fdir = NULL;
static int fdfd = -1;
static int fdots = 0;

/*
* CompareAttributes
*/
static qboolean CompareAttributes( const struct dirent64 *d, unsigned musthave, unsigned canthave )
{
	qboolean isDir;

	assert( d );

	isDir = ( d->d_type == DT_DIR /* || d->d_type == DT_LINK*/ );
	if( isDir && ( canthave & SFF_SUBDIR ) )
		return qfalse;
	if( ( musthave & SFF_SUBDIR ) && !isDir )
		return qfalse;

	return qtrue;
}

/*
* CompareAttributesForPath
*/
static qboolean CompareAttributesForPath( const struct dirent64 *d, const char *path, unsigned musthave, unsigned canthave )
{
	return qtrue;
}

/*
* Sys_FS_FindFirst
*/
const char *Sys_FS_FindFirst( const char *path, unsigned musthave, unsigned canhave )
{
	char *p;

	assert( path );
	assert( !fdir );
	assert( fdfd == -1 );
	assert( !findbase && !findpattern && !findpath && !findpath_size );

	if( fdir )
		Sys_Error( "Sys_BeginFind without close" );

	findbase_size = strlen( path );
	assert( findbase_size );
	findbase_size += 1;

	findbase = Mem_TempMalloc( sizeof( char ) * findbase_size );
	Q_strncpyz( findbase, path, sizeof( char ) * findbase_size );

	if( ( p = strrchr( findbase, '/' ) ) )
	{
		*p = 0;
		if( !strcmp( p+1, "*.*" ) )  // *.* to *
			*( p+2 ) = 0;
		findpattern = p+1;
	}
	else
	{
		findpattern = "*";
	}

	if( !( fdir = opendir( findbase ) ) )
		return NULL;

	fdfd = dirfd( fdir );
	if( fdfd == -1 )
		return NULL;

	fdots = 2; // . and ..

	return Sys_FS_FindNext( musthave, canhave );
}

/*
* Sys_FS_FindNext
*/
const char *Sys_FS_FindNext( unsigned musthave, unsigned canhave )
{
	struct dirent64 *d;

	assert( fdir );
	assert( findbase && findpattern );

	if( !fdir )
		return NULL;

	while( ( d = readdir64( fdir ) ) != NULL )
	{
		if( !CompareAttributes( d, musthave, canhave ) )
			continue;

		if( d->d_type == DT_DIR && fdots > 0 )
		{                                   // . and .. never match
			const char *base = COM_FileBase( d->d_name );
			if( !strcmp( base, "." ) || !strcmp( base, ".." ) )
			{
				fdots--;
				continue;
			}
		}

		if( !*findpattern || Com_GlobMatch( findpattern, d->d_name, 0 ) )
		{
			size_t size = sizeof( char ) * ( findbase_size + strlen( d->d_name ) + 1 );
			if( findpath_size < size )
			{
				if( findpath )
					Mem_TempFree( findpath );
				findpath_size = size * 2; // extra size to reduce reallocs
				findpath = Mem_TempMalloc( findpath_size );
			}

			Q_snprintfz( findpath, findpath_size, "%s/%s", findbase, d->d_name );
			if( CompareAttributesForPath( d, findpath, musthave, canhave ) )
				return findpath;
		}
	}

	return NULL;
}

void Sys_FS_FindClose( void )
{
	assert( findbase );

	if( fdir )
	{
		closedir( fdir );
		fdir = NULL;
	}

	fdfd = -1;
	fdots = 0;

	Mem_TempFree( findbase );
	findbase = NULL;
	findbase_size = 0;
	findpattern = NULL;

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
	return getenv( "HOME" );
}

/*
* Sys_FS_LockFile
*/
void *Sys_FS_LockFile( const char *path )
{
	return (void *)1; // return non-NULL pointer
}

/*
* Sys_FS_UnlockFile
*/
void Sys_FS_UnlockFile( void *handle )
{
}

/*
* Sys_FS_CreateDirectory
*/
qboolean Sys_FS_CreateDirectory( const char *path )
{
	return ( !mkdir( path, 0777 ) );
}

/*
* Sys_FS_RemoveDirectory
*/
qboolean Sys_FS_RemoveDirectory( const char *path )
{
	return ( !rmdir( path ) );
}

/*
* Sys_FS_FileMTime
*/
time_t Sys_FS_FileMTime( const char *filename )
{
	struct stat buffer;
	int         status;

	status = stat( filename, &buffer );
	if( status ) {
		return -1;
	}
	return buffer.st_mtime;
}
