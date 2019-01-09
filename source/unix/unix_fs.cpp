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

#define __USE_BSD

#include <dirent.h>

#ifdef __linux__
#include <linux/limits.h>
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Mac OS X and FreeBSD don't know the readdir64 and dirent64
#if ( defined ( __FreeBSD__ ) || !defined( _LARGEFILE64_SOURCE ) )
#define readdir64 readdir
#define dirent64 dirent
#endif

static char *findbase = NULL;
static size_t findbase_size = 0;
static const char *findpattern = NULL;
static char *findpath = NULL;
static size_t findpath_size = 0;
static DIR *fdir = NULL;
static int fdots = 0;

/*
* FS_DirentIsDir
*/
static bool FS_DirentIsDir( const struct dirent64 *d, const char *base ) {
#if defined( _DIRENT_HAVE_D_TYPE ) && defined( DT_DIR )
	return ( d->d_type == DT_DIR );
#else
	size_t pathSize;
	char *path;
	struct stat st;

	pathSize = strlen( base ) + 1 + strlen( d->d_name ) + 1;
	path = alloca( pathSize );
	Q_snprintfz( path, pathSize, "%s/%s", base, d->d_name );
	if( stat( path, &st ) ) {
		return false;
	}
	return S_ISDIR( st.st_mode ) != 0;
#endif
}

/*
* CompareAttributes
*/
static bool CompareAttributes( const struct dirent64 *d, const char *base, unsigned musthave, unsigned canthave ) {
	bool isDir;
	bool checkDir;

	assert( d );

	isDir = false;
	checkDir = ( canthave & SFF_SUBDIR ) || ( musthave & SFF_SUBDIR );
	if( checkDir ) {
		isDir = FS_DirentIsDir( d, base );
	}

	if( isDir && ( canthave & SFF_SUBDIR ) ) {
		return false;
	}
	if( ( musthave & SFF_SUBDIR ) && !isDir ) {
		return false;
	}

	return true;
}

/*
* CompareAttributesForPath
*/
static bool CompareAttributesForPath( const struct dirent64 *d, const char *path, unsigned musthave, unsigned canthave ) {
	return true;
}

/*
* Sys_FS_FindFirst
*/
const char *Sys_FS_FindFirst( const char *path, unsigned musthave, unsigned canhave ) {
	char *p;

	assert( path );
	assert( !fdir );
	assert( !findbase && !findpattern && !findpath && !findpath_size );

	if( fdir ) {
		Sys_Error( "Sys_BeginFind without close" );
	}

	findbase_size = strlen( path );
	assert( findbase_size );
	findbase_size += 1;

	findbase = ( char * ) Mem_TempMalloc( sizeof( char ) * findbase_size );
	Q_strncpyz( findbase, path, sizeof( char ) * findbase_size );

	if( ( p = strrchr( findbase, '/' ) ) ) {
		*p = 0;
		if( !strcmp( p + 1, "*.*" ) ) { // *.* to *
			*( p + 2 ) = 0;
		}
		findpattern = p + 1;
	} else {
		findpattern = "*";
	}

	if( !( fdir = opendir( findbase ) ) ) {
		return NULL;
	}

	fdots = 2; // . and ..
	return Sys_FS_FindNext( musthave, canhave );
}

/*
* Sys_FS_FindNext
*/
const char *Sys_FS_FindNext( unsigned musthave, unsigned canhave ) {
	struct dirent64 *d;

	assert( fdir );
	assert( findbase && findpattern );

	if( !fdir ) {
		return NULL;
	}

	while( ( d = readdir64( fdir ) ) != NULL ) {
		if( !CompareAttributes( d, findbase, musthave, canhave ) ) {
			continue;
		}

		if( fdots > 0 ) {
			// . and .. never match
			const char *base = COM_FileBase( d->d_name );
			if( !strcmp( base, "." ) || !strcmp( base, ".." ) ) {
				fdots--;
				continue;
			}
		}

		if( !*findpattern || Com_GlobMatch( findpattern, d->d_name, 0 ) ) {
			const char *dname = d->d_name;
			size_t dname_len = strlen( dname );
			size_t size = sizeof( char ) * ( findbase_size + dname_len + 1 + 1 );
			if( findpath_size < size ) {
				if( findpath ) {
					Mem_TempFree( findpath );
				}
				findpath_size = size * 2; // extra size to reduce reallocs
				findpath = ( char * ) Mem_TempMalloc( findpath_size );
			}

			Q_snprintfz( findpath, findpath_size, "%s/%s%s", findbase, dname,
						 dname[dname_len - 1] != '/' && FS_DirentIsDir( d, findbase ) ? "/" : "" );
			if( CompareAttributesForPath( d, findpath, musthave, canhave ) ) {
				return findpath;
			}
		}
	}

	return NULL;
}

void Sys_FS_FindClose( void ) {
	assert( findbase );

	if( fdir ) {
		closedir( fdir );
		fdir = NULL;
	}

	fdots = 0;

	Mem_TempFree( findbase );
	findbase = NULL;
	findbase_size = 0;
	findpattern = NULL;

	if( findpath ) {
		Mem_TempFree( findpath );
		findpath = NULL;
		findpath_size = 0;
	}
}

/*
* Sys_FS_GetHomeDirectory
*/
const char *Sys_FS_GetHomeDirectory( void ) {
	static char home[PATH_MAX] = { '\0' };

	if( home[0] == '\0' ) {
		const char *homeEnv = getenv( "HOME" );
		const char *base = NULL, *local = "";

#ifdef __MACOSX__
		base = homeEnv;
		local = "Library/Application Support/";
#else
		base = getenv( "XDG_DATA_HOME" );
		local = "";
		if( !base ) {
			base = homeEnv;
			local = ".local/share/";
		}
#endif

		if( base ) {
			Q_snprintfz( home, sizeof( home ), "%s/%s%s-0.0", base, local, APPLICATION );
		}
	}

	if( home[0] == '\0' ) {
		return NULL;
	}
	return home;
}

/*
* Sys_FS_GetCacheDirectory
*/
const char *Sys_FS_GetCacheDirectory( void ) {
	static char cache[PATH_MAX] = { '\0' };

	if( cache[0] == '\0' ) {
		const char *homeEnv = getenv( "HOME" );
		const char *base = NULL, *local = "";

#ifdef __MACOSX__
		base = homeEnv;
		local = "Library/Caches/";
#else
		base = getenv( "XDG_CACHE_HOME" );
		local = "";
		if( !base ) {
			base = homeEnv;
			local = ".cache/";
		}
#endif

		if( base ) {
			Q_snprintfz( cache, sizeof( cache ), "%s/%s%s-0.0", base, local, APPLICATION );
		}
	}

	if( cache[0] == '\0' ) {
		return NULL;
	}
	return cache;
}

/*
* Sys_FS_LockFile
*/
void *Sys_FS_LockFile( const char *path ) {
	return (void *)1; // return non-NULL pointer
}

/*
* Sys_FS_UnlockFile
*/
void Sys_FS_UnlockFile( void *handle ) {
}

/*
* Sys_FS_CreateDirectory
*/
bool Sys_FS_CreateDirectory( const char *path ) {
	return ( !mkdir( path, 0777 ) );
}

/*
* Sys_FS_RemoveDirectory
*/
bool Sys_FS_RemoveDirectory( const char *path ) {
	return ( !rmdir( path ) );
}

/*
* Sys_FS_FileNo
*/
int Sys_FS_FileNo( FILE *fp ) {
	return fileno( fp );
}
