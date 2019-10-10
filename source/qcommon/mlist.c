/*
Copyright (C) 2007 Will Franklin

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

// MAPLIST FUNCTIONS

#include "qcommon.h"
#include "../qalgo/q_trie.h"

#define MLIST_CACHE "cache/mapcache.txt"
#define MLIST_NULL  ""

#define MLIST_TRIE_CASING TRIE_CASE_INSENSITIVE

#define MLIST_UNKNOWN_MAPNAME   "@#$"

#define MLIST_CACHE_EXISTS ( FS_FOpenFile( MLIST_CACHE, NULL, FS_READ | FS_CACHE ) > 0 )

typedef struct mapinfo_s {
	char *filename, *fullname;
	struct mapinfo_s *next;
} mapinfo_t;

static mapinfo_t *maplist;
static trie_t *mlist_filenames_trie = NULL, *mlist_fullnames_trie = NULL;

static bool ml_flush = true;
static bool ml_initialized = false;

static void ML_BuildCache( void );
static void ML_InitFromCache( void );
static void ML_InitFromMaps( void );
static void ML_GetFullnameFromMap( const char *filename, char *fullname, size_t len );
static bool ML_FilenameExistsExt( const char *filename, bool quick );

/*
* ML_AddMap
* Handles assigning memory for map and adding it to the list
* in alphabetical order
*/
static void ML_AddMap( const char *filename, const char *fullname ) {
	mapinfo_t *map;
	char *buffer;
	char fullname_[MAX_CONFIGSTRING_CHARS];

	if( !ML_ValidateFilename( filename ) ) {
		return;
	}

#ifdef PUBLIC_BUILD
	if( !strcmp( filename, "ui" ) ) {
		return;
	}
#endif

	if( !fullname ) {
		ML_GetFullnameFromMap( filename, fullname_, sizeof( fullname_ ) );
		fullname = fullname_;
	}

	if( !ML_ValidateFullname( fullname ) && *fullname ) { // allow empty fullnames
		return;
	}

#ifdef PUBLIC_BUILD
	if( !strcmp( fullname, "ui" ) ) {
		return;
	}
#endif

	ml_flush = true;    // tell everyone that maplist has changed
	buffer = ( char* )Mem_ZoneMalloc( sizeof( mapinfo_t ) + strlen( filename ) + 1 + strlen( fullname ) + 1 );

	map = ( mapinfo_t * )buffer;
	buffer += sizeof( mapinfo_t );

	map->filename = buffer;
	strcpy( map->filename, filename );
	COM_StripExtension( map->filename );
	buffer += strlen( filename ) + 1;

	map->fullname = buffer;
	strcpy( map->fullname, fullname );
	COM_RemoveColorTokens( map->fullname );
	Q_strlwr( map->fullname );

	Trie_Insert( mlist_filenames_trie, map->filename, map );
	Trie_Insert( mlist_fullnames_trie, map->fullname, map );

	map->next = maplist;
	maplist = map;
}

/*
* ML_BuildCache
* Write the map data to a cache file
*/
static void ML_BuildCache( void ) {
	int filenum;
	mapinfo_t *map;
	struct trie_dump_s *dump;

	if( !ml_initialized ) {
		return;
	}

	if( FS_FOpenFile( MLIST_CACHE, &filenum, FS_WRITE | FS_CACHE ) != -1 ) {
		unsigned int i;

		Trie_Dump( mlist_filenames_trie, "", TRIE_DUMP_VALUES, &dump );
		for( i = 0; i < dump->size; ++i ) {
			map = ( mapinfo_t * )( dump->key_value_vector[i].value );
			FS_Printf( filenum, "%s\r\n%s\r\n", map->filename, map->fullname );
		}
		Trie_FreeDump( dump );

		FS_FCloseFile( filenum );
	}
}

typedef struct mapdir_s {
	char *filename;
	struct mapdir_s *prev, *next;
} mapdir_t;

/*
* ML_InitFromCache
* Fills map list array from cache, much faster
*/
static void ML_InitFromCache( void ) {
	int count, i, total, len;
	size_t size = 0;
	char *buffer, *chr, *current, *curend;
	char *temp, *maps, *map;
	mapdir_t *dir, *curmap, *prev;

	if( ml_initialized ) {
		return;
	}

	total = FS_GetFileListExt( "maps", ".bsp", NULL, &size, 0, 0 );
	if( !total ) {
		return;
	}

	// load maps from directory reading into a list
	maps = temp = ( char* )Mem_TempMalloc( size + sizeof( mapdir_t ) * total );
	temp += size;
	FS_GetFileList( "maps", ".bsp", maps, size, 0, 0 );
	len = 0;
	prev = NULL;
	dir = NULL;
	for( i = 0; i < total; i++ ) {
		map = maps + len;
		len += strlen( map ) + 1;

		curmap = ( mapdir_t * )temp;
		temp += sizeof( mapdir_t );

		COM_StripExtension( map );

		if( !i ) {
			dir = curmap;
		} else {
			prev->next = curmap;
			curmap->prev = prev;
		}

		curmap->filename = map;
		prev = curmap;
	}

	FS_LoadCacheFile( MLIST_CACHE, (void **)&buffer, NULL, 0 );
	if( !buffer ) {
		Mem_TempFree( maps );
		return;
	}

	current = curend = buffer;
	count = 0;

	for( chr = buffer; *chr; chr++ ) {
		// current character is a delimiter
		if( *chr == '\n' ) {
			if( *( chr - 1 ) == '\r' ) {
				*( chr - 1 ) = '\0';    // clear the CR too
			}
			*chr = '\0';            // clear the LF

			// if we have got both params
			if( !( ++count & 1 ) ) {
				// check if its in the maps directory
				for( curmap = dir; curmap; curmap = curmap->next ) {
					if( !Q_stricmp( curmap->filename, current ) ) {
						if( curmap->prev ) {
							curmap->prev->next = curmap->next;
						} else {
							dir = curmap->next;
						}

						if( curmap->next ) {
							curmap->next->prev = curmap->prev;
						}
						break;
					}
				}

				// if we found it in the maps directory
				if( curmap ) {
					COM_SanitizeFilePath( current );

					// well, if we've got a map with an unknown fullname, load it from map
					if( !strcmp( curend + 1, MLIST_UNKNOWN_MAPNAME ) ) {
						ML_AddMap( current, NULL );
					} else {
						ML_AddMap( current, curend + 1 );
					}
				}
				current = chr + 1;
			} else {
				curend = chr;
			}
		}
	}

	// we've now loaded the mapcache, but there may be files which
	// have been added to maps directory, and the mapcache isnt aware
	// these will be left over in our directory list
	for( curmap = dir; curmap; curmap = curmap->next )
		ML_AddMap( curmap->filename, NULL );

	Mem_TempFree( maps );
	FS_FreeFile( buffer );
}

/*
* ML_InitFromMaps
* Fills map list array from each map file. Very slow
* and should only be called if cache doesnt exist
*/
static void ML_InitFromMaps( void ) {
	int i, j, total, len;
	char maps[2048];
	char *filename;

	if( ml_initialized ) {
		return;
	}

	total = FS_GetFileList( "maps", ".bsp", NULL, 0, 0, 0 );
	if( !total ) {
		return;
	}

	i = 0;
	while( i < total ) {
		memset( maps, 0, sizeof( maps ) );
		j = FS_GetFileList( "maps", ".bsp", maps, sizeof( maps ), i, total );

		// no maps returned, map name is too big or end of list?
		if( !j ) {
			i++;
			continue;
		}
		i += j;

		// split the maps up and find their fullnames
		len = 0;
		while( j-- ) {
			filename = maps + len;
			if( !*filename ) {
				continue;
			}

			len += strlen( filename ) + 1;

			COM_SanitizeFilePath( filename );
			COM_StripExtension( filename );

			ML_AddMap( filename, NULL );
		}
	}
}

static int ML_PatternMatchesMap( void *map, void *pattern ) {
	assert( map );
	return !pattern || Com_GlobMatch( (const char *) pattern, ( (mapinfo_t *) map )->filename, false );
}

/*
* ML_MapListCmd
* Handler for console command "maplist"
*/
static void ML_MapListCmd( void ) {
	char *pattern;
	mapinfo_t *map;
	int argc = Cmd_Argc();
	unsigned int i;
	struct trie_dump_s *dump = NULL;

	if( argc > 2 ) {
		Com_Printf( "Usage: %s [rebuild]\n", Cmd_Argv( 0 ) );
		return;
	}

	pattern = ( argc == 2 ? Cmd_Argv( 1 ) : NULL );
	if( pattern && !*pattern ) {
		pattern = NULL;
	}

	if( pattern ) {
		if( !strcmp( pattern, "rebuild" ) ) {
			Com_Printf( "Rebuilding map list...\n" );
			ML_Restart( true );
			return;
		}

		if( !strcmp( pattern, "update" ) ) {
			Com_Printf( "Updating map list...\n" );
			ML_Update();
			return;
		}
	}

	Trie_DumpIf( mlist_filenames_trie, "", TRIE_DUMP_VALUES, ML_PatternMatchesMap, pattern, &dump );
	for( i = 0; i < dump->size; i++ ) {
		map = ( mapinfo_t * )( dump->key_value_vector[i].value );
		Com_Printf( "%s: %s\n", map->filename, map->fullname );
	}
	Trie_FreeDump( dump );

	Com_Printf( "%d map(s) %s\n", i, pattern ? "matching" : "total" );
}

/*
* ML_CompleteBuildList
*/
char **ML_CompleteBuildList( const char *partial ) {
	struct trie_dump_s *dump;
	char **buf;
	unsigned int i;

	assert( mlist_filenames_trie );
	assert( partial );

	Trie_Dump( mlist_filenames_trie, partial, TRIE_DUMP_VALUES, &dump );
	buf = (char **) Mem_TempMalloc( sizeof( char * ) * ( dump->size + 1 ) );
	for( i = 0; i < dump->size; ++i )
		buf[i] = ( (mapinfo_t *) ( dump->key_value_vector[i].value ) )->filename;
	buf[dump->size] = NULL;

	Trie_FreeDump( dump );

	return buf;
}

/*
* ML_Init
* Initialize map list. Check if cache file exists, if not create it
*/
void ML_Init( void ) {
	if( ml_initialized ) {
		return;
	}

	Trie_Create( MLIST_TRIE_CASING, &mlist_filenames_trie );
	Trie_Create( MLIST_TRIE_CASING, &mlist_fullnames_trie );

	Cmd_AddCommand( "maplist", ML_MapListCmd );

	if( MLIST_CACHE_EXISTS ) {
		ML_InitFromCache();
	} else {
		ML_InitFromMaps();
	}

	ml_initialized = true;
	ml_flush = true;
}

/*
* ML_Shutdown
* Free map list memory
*/
void ML_Shutdown( void ) {
	mapinfo_t *map;

	if( !ml_initialized ) {
		return;
	}

	ML_BuildCache();

	ml_initialized = false;

	Cmd_RemoveCommand( "maplist" );

	Trie_Destroy( mlist_filenames_trie );
	Trie_Destroy( mlist_fullnames_trie );

	while( maplist ) {
		map = maplist;
		maplist = map->next;
		Mem_ZoneFree( map );
	}

	ml_flush = true;
}

/*
* ML_Restart
* Restart map list stuff
*/
void ML_Restart( bool forcemaps ) {
	ML_Shutdown();
	if( forcemaps ) {
		int filenum;
		if( FS_FOpenFile( MLIST_CACHE, &filenum, FS_WRITE | FS_CACHE ) != -1 ) {
			FS_FCloseFile( filenum );
		}
	}
	FS_Rescan();
	ML_Init();
}

/*
* ML_Update
*/
bool ML_Update( void ) {
	int i, len, total, newpaks;
	size_t size;
	char *map, *maps, *filename;

	newpaks = FS_Rescan();
	if( !newpaks ) {
		return false;
	}

	total = FS_GetFileListExt( "maps", ".bsp", NULL, &size, 0, 0 );
	if( size ) {
		maps = ( char* )Mem_TempMalloc( size );
		FS_GetFileList( "maps", ".bsp", maps, size, 0, 0 );
		for( i = 0, len = 0; i < total; i++ ) {
			map = maps + len;
			len += strlen( map ) + 1;

			filename = ( char * )COM_FileBase( map );
			COM_StripExtension( filename );

			// don't check for existance of each file itself, as we've just got the fresh list
			if( !ML_FilenameExistsExt( filename, true ) ) {
				ML_AddMap( filename, MLIST_UNKNOWN_MAPNAME );
			}
		}
		Mem_TempFree( maps );
	}

	return true;
}

/*
* ML_GetFilename
* Returns the filename for the map with the corresponding fullname
*/
const char *ML_GetFilenameExt( const char *fullname, bool recursive ) {
	mapinfo_t *map;
	trie_error_t err;
	char *fullname2;

	if( !ml_initialized ) {
		return MLIST_NULL;
	}

	if( !ML_ValidateFullname( fullname ) ) {
		return MLIST_NULL;
	}

	fullname2 = ( char* )Mem_TempMalloc( strlen( fullname ) + 1 );
	strcpy( fullname2, fullname );
	Q_strlwr( fullname2 );

	err = Trie_Find( mlist_fullnames_trie, fullname2, TRIE_EXACT_MATCH, (void **)&map );
	Mem_Free( fullname2 );

	if( err == TRIE_OK ) {
		return map->filename;
	}

	// we should technically never get here, but
	// maybe the mapper has changed the fullname of the map
	// or the user has tampered with the mapcache
	// we need to reload the whole cache from file if we get here
	/*
	if( !recursive )
	{
	ML_Restart( true );
	return ML_GetFilenameExt( fullname, true );
	}
	*/
	return MLIST_NULL;
}

/*
* ML_GetFilename
* Returns the filename for the map with the corresponding fullname
*/
const char *ML_GetFilename( const char *fullname ) {
	return ML_GetFilenameExt( fullname, false );
}

/*
* ML_FilenameExists
* Checks to see if a filename is present in the map list
*/
static bool ML_FilenameExistsExt( const char *filename, bool quick ) {
	mapinfo_t *map;
	char *filepath;

	if( !ml_initialized ) {
		return false;
	}

	filepath = va( "maps/%s.bsp", filename );
	COM_SanitizeFilePath( filepath );

	if( !ML_ValidateFilename( filename ) ) {
		return false;
	}

	if( Trie_Find( mlist_filenames_trie, filename, TRIE_EXACT_MATCH, (void **)&map ) == TRIE_OK ) {
		if( quick || FS_FOpenFile( filepath, NULL, FS_READ ) != -1 ) {
			return true;
		}
	}

	return false;
}

/*
* ML_FilenameExists
* Checks to see if a filename is present in the map list
*/
bool ML_FilenameExists( const char *filename ) {
	return ML_FilenameExistsExt( filename, false );
}

/*
* ML_GetFullname
* Returns the fullname for the map with the corresponding filename
*/
const char *ML_GetFullname( const char *filename ) {
	mapinfo_t *map;
	//char *filepath;

	if( !ml_initialized ) {
		return MLIST_NULL;
	}

	if( !ML_ValidateFilename( filename ) ) {
		return MLIST_NULL;
	}

	/*
	filepath = va( "maps/%s.bsp", filename );
	COM_SanitizeFilePath( filepath );

	if( FS_FOpenFile( filepath, NULL, FS_READ ) == -1 )
	    return MLIST_NULL;
	*/

	if( Trie_Find( mlist_filenames_trie, filename, TRIE_EXACT_MATCH, (void **)&map ) == TRIE_OK ) {
		return map->fullname;
	}

	// we should never get down here!
	assert( false );

	return MLIST_NULL;
}

/*
* ML_GetFullnameFromMap
* Get fullname of map from file or worldspawn (slow)
*/
static void ML_GetFullnameFromMap( const char *filename, char *fullname, size_t len ) {
	char *buffer;

	*fullname = '\0';

	// Try and load fullname from a file
	FS_LoadFile( va( "maps/%s.txt", filename ), ( void ** )&buffer, NULL, 0 );
	if( buffer ) {
		char *line = buffer;
		Q_strncpyz( fullname, COM_Parse( &line ), len );
		FS_FreeFile( buffer );
		return;
	}

	// Try and load fullname from worldspawn
	CM_LoadMapMessage( va( "maps/%s.bsp", filename ), fullname, len );
	COM_RemoveColorTokens( fullname );
}

/*
* ML_ValidateFilename
* Checks that the filename provided is valid
*/
bool ML_ValidateFilename( const char *filename ) {
	const char *extension;

	if( !filename || !*filename ) {
		return false;
	}

	extension = COM_FileExtension( filename );
	if( !extension ) {
		if( strlen( "maps/" ) + strlen( filename ) + strlen( ".bsp" ) >= MAX_CONFIGSTRING_CHARS ) {
			return false;
		}
	} else {
		if( Q_stricmp( extension, ".bsp" ) ) {
			return false;
		}
		if( strlen( "maps/" ) + strlen( filename ) >= MAX_CONFIGSTRING_CHARS ) {
			return false;
		}
	}

	if( !COM_ValidateRelativeFilename( filename ) || strchr( filename, '/' ) ) {
		return false;
	}

	return true;
}

/*
* ML_ValidateFullname
* Checks that the fullname provided is valid
*/
bool ML_ValidateFullname( const char *fullname ) {
	if( !fullname || !*fullname ) {
		return false;
	}

	if( strlen( fullname ) >= MAX_CONFIGSTRING_CHARS ) {
		return false;
	}

	return true;
}

/*
* ML_GetMapByNum
* Prints map infostring in "mapname\0fullname" format into "out" string,
* returns fullsize (so that out can be reallocated if there's not enough space)
*/
size_t ML_GetMapByNum( int num, char *out, size_t size ) {
	static int i = 0;
	static struct trie_dump_s *dump = NULL;
	size_t fsize;
	mapinfo_t *map;

	if( !ml_initialized ) {
		return 0;
	}

	if( ml_flush || i > num ) {
		if( dump ) {
			Trie_FreeDump( dump );
			dump = NULL;
		}
		ml_flush = false;
	}

	if( !dump ) {
		i = 0;
		Trie_Dump( mlist_filenames_trie, "", TRIE_DUMP_VALUES, &dump );
	}

	for( ; i < num && i < (int)dump->size; i++ )
		;
	if( i == (int)dump->size ) {
		return 0;
	}

	map = ( mapinfo_t * )( dump->key_value_vector[i].value );
	fsize = strlen( map->filename ) + 1 + strlen( map->fullname ) + 1;
	if( out && ( fsize <= size ) ) {
		Q_strncpyz( out, map->filename, size );
		Q_strncpyz( out + strlen( out ) + 1,
					strcmp( map->fullname, MLIST_UNKNOWN_MAPNAME ) ? map->fullname : map->filename, size - ( strlen( out ) + 1 ) );
	}

	return fsize;
}
