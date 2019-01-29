/*
Copyright (C) 2002-2003 Victor Luchits

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
// Z_zone.c

#include "qcommon.h"

//#define MEMTRASH

#define POOLNAMESIZE 128

#define MEMHEADER_SENTINEL1         0xDEADF00D
#define MEMHEADER_SENTINEL2         0xDF

#define MEMALIGNMENT_DEFAULT        16

typedef struct memheader_s {
	// address returned by malloc (may be significantly before this header to satisify alignment)
	void *baseaddress;

	// next and previous memheaders in chain belonging to pool
	struct memheader_s *next;
	struct memheader_s *prev;

	// pool this memheader belongs to
	struct mempool_s *pool;

	// size of the memory after the header (excluding header and sentinel2)
	size_t size;

	// size of the memory including the header, alignment and sentinel2
	size_t realsize;

	// file name and line where Mem_Alloc was called
	const char *filename;
	int fileline;

	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;
	// immediately followed by data, which is followed by a MEMHEADER_SENTINEL2 byte
} memheader_t;

struct mempool_s {
	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;

	// chain of individual memory allocations
	struct memheader_s *chain;

	// temporary, etc
	int flags;

	// total memory allocated in this pool (inside memheaders)
	int totalsize;

	// total memory allocated in this pool (actual malloc total)
	int realsize;

	// updated each time the pool is displayed by memlist, shows change from previous time (unless pool was freed)
	int lastchecksize;

	// name of the pool
	char name[POOLNAMESIZE];

	// linked into global mempool list or parent's children list
	struct mempool_s *next;

	struct mempool_s *parent;
	struct mempool_s *child;

	// file name and line where Mem_AllocPool was called
	const char *filename;

	int fileline;

	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel2;
};

// ============================================================================

//#define SHOW_NONFREED

cvar_t *developer_memory;

static mempool_t *poolChain = NULL;

// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this pool stays allocated during gameplay, it is
// considered a leak
mempool_t *tempMemPool;

// only for zone
mempool_t *zoneMemPool;

static qmutex_t *memMutex;

static bool memory_initialized = false;
static bool commands_initialized = false;

static void _Mem_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Sys_Error( "%s", msg );
}

ATTRIBUTE_MALLOC void *_Mem_AllocExt( mempool_t *pool, size_t size, size_t alignment, int z, int musthave, int canthave, const char *filename, int fileline ) {
	void *base;
	size_t realsize;
	memheader_t *mem;

	if( size <= 0 ) {
		return NULL;
	}

	// default to 16-bytes alignment
	if( !alignment ) {
		alignment = MEMALIGNMENT_DEFAULT;
	}

	assert( pool != NULL );

	if( pool == NULL ) {
		_Mem_Error( "Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline );
	}
	if( musthave && ( ( pool->flags & musthave ) != musthave ) ) {
		_Mem_Error( "Mem_Alloc: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if( canthave && ( pool->flags & canthave ) ) {
		_Mem_Error( "Mem_Alloc: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	if( developer_memory && developer_memory->integer ) {
		Com_DPrintf( "Mem_Alloc: pool %s, file %s:%i, size %" PRIuPTR " bytes\n", pool->name, filename, fileline, (uintptr_t)size );
	}

	QMutex_Lock( memMutex );

	pool->totalsize += size;
	realsize = sizeof( memheader_t ) + size + alignment + sizeof( int );

	pool->realsize += realsize;

	base = malloc( realsize );
	if( base == NULL ) {
		_Mem_Error( "Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline );
	}

	// calculate address that aligns the end of the memheader_t to the specified alignment
	mem = ( memheader_t * )( ( ( (size_t)base + sizeof( memheader_t ) + ( alignment - 1 ) ) & ~( alignment - 1 ) ) - sizeof( memheader_t ) );
	mem->baseaddress = base;
	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->realsize = realsize;
	mem->pool = pool;
	mem->sentinel1 = MEMHEADER_SENTINEL1;

	// we have to use only a single byte for this sentinel, because it may not be aligned, and some platforms can't use unaligned accesses
	*( (uint8_t *) mem + sizeof( memheader_t ) + mem->size ) = MEMHEADER_SENTINEL2;

	// append to head of list
	mem->next = pool->chain;
	mem->prev = NULL;
	pool->chain = mem;
	if( mem->next ) {
		mem->next->prev = mem;
	}

	QMutex_Unlock( memMutex );

	if( z ) {
		memset( (void *)( (uint8_t *) mem + sizeof( memheader_t ) ), 0, mem->size );
	}

	return (void *)( (uint8_t *) mem + sizeof( memheader_t ) );
}

ATTRIBUTE_MALLOC void *_Mem_Alloc( mempool_t *pool, size_t size, int musthave, int canthave, const char *filename, int fileline ) {
	return _Mem_AllocExt( pool, size, 0, 1, musthave, canthave, filename, fileline );
}

// FIXME: rewrite this?
void *_Mem_Realloc( void *data, size_t size, const char *filename, int fileline ) {
	void *newdata;
	memheader_t *mem;

	if( data == NULL ) {
		_Mem_Error( "Mem_Realloc: data == NULL (called at %s:%i)", filename, fileline );
	}
	if( size <= 0 ) {
		Mem_Free( data );
		return NULL;
	}

	mem = ( memheader_t * )( (uint8_t *) data - sizeof( memheader_t ) );

	assert( mem->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( *( (uint8_t *) mem + sizeof( memheader_t ) + mem->size ) == MEMHEADER_SENTINEL2 );

	if( mem->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_Realloc: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", 
			mem->filename, mem->fileline, filename, fileline );
	}
	if( *( (uint8_t *)mem + sizeof( memheader_t ) + mem->size ) != MEMHEADER_SENTINEL2 ) {
		_Mem_Error( "Mem_Realloc: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", 
			mem->filename, mem->fileline, filename, fileline );
	}

	if( size <= mem->size ) {
		return data;
	}

	newdata = Mem_AllocExt( mem->pool, size, 0 );
	memcpy( newdata, data, mem->size );
	memset( (uint8_t *)newdata + mem->size, 0, size - mem->size );
	Mem_Free( data );

	return newdata;
}

char *_Mem_CopyString( mempool_t *pool, const char *in, const char *filename, int fileline ) {
	char *out;
	size_t num_chars = strlen( in ) + 1;
	size_t str_size = sizeof( char ) * num_chars;

	out = ( char* )_Mem_Alloc( pool, str_size, 0, 0, filename, fileline );
	memcpy( out, in, str_size );

	return out;
}

void _Mem_Free( void *data, int musthave, int canthave, const char *filename, int fileline ) {
	void *base;
	memheader_t *mem;
	mempool_t *pool;

	if( data == NULL ) {
		//_Mem_Error( "Mem_Free: data == NULL (called at %s:%i)", filename, fileline );
		return;
	}

	mem = ( memheader_t * )( (uint8_t *) data - sizeof( memheader_t ) );

	assert( mem->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( *( (uint8_t *) mem + sizeof( memheader_t ) + mem->size ) == MEMHEADER_SENTINEL2 );

	if( mem->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", 
			mem->filename, mem->fileline, filename, fileline );
	}
	if( *( (uint8_t *)mem + sizeof( memheader_t ) + mem->size ) != MEMHEADER_SENTINEL2 ) {
		_Mem_Error( "Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", 
			mem->filename, mem->fileline, filename, fileline );
	}

	pool = mem->pool;
	if( musthave && ( ( pool->flags & musthave ) != musthave ) ) {
		_Mem_Error( "Mem_Free: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if( canthave && ( pool->flags & canthave ) ) {
		_Mem_Error( "Mem_Free: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	if( developer_memory && developer_memory->integer ) {
		Com_DPrintf( "Mem_Free: pool %s, alloc %s:%i, free %s:%i, size %" PRIuPTR " bytes\n", 
			pool->name, mem->filename, mem->fileline, filename, fileline, (uintptr_t)mem->size );
	}

	QMutex_Lock( memMutex );

	// unlink memheader from doubly linked list
	if( ( mem->prev ? mem->prev->next != mem : pool->chain != mem ) || ( mem->next && mem->next->prev != mem ) ) {
		_Mem_Error( "Mem_Free: not allocated or double freed (free at %s:%i)", filename, fileline );
	}

	if( mem->prev ) {
		mem->prev->next = mem->next;
	} else {
		pool->chain = mem->next;
	}
	if( mem->next ) {
		mem->next->prev = mem->prev;
	}

	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;

	base = mem->baseaddress;
	pool->realsize -= mem->realsize;

	QMutex_Unlock( memMutex );

#ifdef MEMTRASH
	memset( mem, 0xBF, sizeof( memheader_t ) + mem->size + sizeof( int ) );
#endif

	free( base );
}

mempool_t *_Mem_AllocPool( mempool_t *parent, const char *name, int flags, const char *filename, int fileline ) {
	mempool_t *pool;

	if( parent && ( parent->flags & MEMPOOL_TEMPORARY ) ) {
		_Mem_Error( "Mem_AllocPool: nested temporary pools are not allowed (allocpool at %s:%i)", filename, fileline );
	}
	if( flags & MEMPOOL_TEMPORARY ) {
		_Mem_Error( "Mem_AllocPool: tried to allocate temporary pool, use Mem_AllocTempPool instead (allocpool at %s:%i)", filename, fileline );
	}

	pool = ( mempool_t* )malloc( sizeof( mempool_t ) );
	if( pool == NULL ) {
		_Mem_Error( "Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline );
	}

	memset( pool, 0, sizeof( mempool_t ) );
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
	pool->filename = filename;
	pool->fileline = fileline;
	pool->flags = flags;
	pool->chain = NULL;
	pool->parent = parent;
	pool->child = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof( mempool_t );
	Q_strncpyz( pool->name, name, sizeof( pool->name ) );

	if( parent ) {
		pool->next = parent->child;
		parent->child = pool;
	} else {
		pool->next = poolChain;
		poolChain = pool;
	}

	return pool;
}

mempool_t *_Mem_AllocTempPool( const char *name, const char *filename, int fileline ) {
	mempool_t *pool;

	pool = _Mem_AllocPool( NULL, name, 0, filename, fileline );
	pool->flags = MEMPOOL_TEMPORARY;

	return pool;
}

void _Mem_FreePool( mempool_t **pool, int musthave, int canthave, const char *filename, int fileline ) {
	mempool_t **chainAddress;
#ifdef SHOW_NONFREED
	memheader_t *mem;
#endif

	if( !( *pool ) ) {
		return;
	}
	if( musthave && ( ( ( *pool )->flags & musthave ) != musthave ) ) {
		_Mem_Error( "Mem_FreePool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if( canthave && ( ( *pool )->flags & canthave ) ) {
		_Mem_Error( "Mem_FreePool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	// recurse into children
	// note that children will be freed no matter if their flags
	// do not match musthave\canthave pair
	while( ( *pool )->child ) {
		mempool_t *tmp = ( *pool )->child;
		_Mem_FreePool( &tmp, 0, 0, filename, fileline );
	}

	assert( ( *pool )->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( ( *pool )->sentinel2 == MEMHEADER_SENTINEL1 );

	if( ( *pool )->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", ( *pool )->filename, ( *pool )->fileline, filename, fileline );
	}
	if( ( *pool )->sentinel2 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", ( *pool )->filename, ( *pool )->fileline, filename, fileline );
	}

#ifdef SHOW_NONFREED
	if( ( *pool )->chain ) {
		Com_Printf( "Warning: Memory pool %s has resources that weren't freed:\n", ( *pool )->name );
	}
	for( mem = ( *pool )->chain; mem; mem = mem->next ) {
		Com_Printf( "%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline );
	}
#endif

	// unlink pool from chain
	if( ( *pool )->parent ) {
		for( chainAddress = &( *pool )->parent->child; *chainAddress && *chainAddress != *pool; chainAddress = &( ( *chainAddress )->next ) ) ;
	} else {
		for( chainAddress = &poolChain; *chainAddress && *chainAddress != *pool; chainAddress = &( ( *chainAddress )->next ) ) ;
	}

	if( *chainAddress != *pool ) {
		_Mem_Error( "Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline );
	}

	while( ( *pool )->chain )  // free memory owned by the pool
		Mem_Free( (void *)( (uint8_t *)( *pool )->chain + sizeof( memheader_t ) ) );

	*chainAddress = ( *pool )->next;

	// free the pool itself
#ifdef MEMTRASH
	memset( *pool, 0xBF, sizeof( mempool_t ) );
#endif
	free( *pool );
	*pool = NULL;
}

void _Mem_EmptyPool( mempool_t *pool, int musthave, int canthave, const char *filename, int fileline ) {
	mempool_t *child, *next;
#ifdef SHOW_NONFREED
	memheader_t *mem;
#endif

	if( pool == NULL ) {
		_Mem_Error( "Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline );
	}
	if( musthave && ( ( pool->flags & musthave ) != musthave ) ) {
		_Mem_Error( "Mem_EmptyPool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	}
	if( canthave && ( pool->flags & canthave ) ) {
		_Mem_Error( "Mem_EmptyPool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	}

	// recurse into children
	if( pool->child ) {
		for( child = pool->child; child; child = next ) {
			next = child->next;
			_Mem_EmptyPool( child, 0, 0, filename, fileline );
		}
	}

	assert( pool->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( pool->sentinel2 == MEMHEADER_SENTINEL1 );

	if( pool->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}
	if( pool->sentinel2 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}

#ifdef SHOW_NONFREED
	if( pool->chain ) {
		Com_Printf( "Warning: Memory pool %s has resources that weren't freed:\n", pool->name );
	}
	for( mem = pool->chain; mem; mem = mem->next ) {
		Com_Printf( "%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline );
	}
#endif
	while( pool->chain )        // free memory owned by the pool
		Mem_Free( (void *)( (uint8_t *) pool->chain + sizeof( memheader_t ) ) );
}

size_t Mem_PoolTotalSize( mempool_t *pool ) {
	assert( pool != NULL );

	return pool->totalsize;
}

void _Mem_CheckSentinels( void *data, const char *filename, int fileline ) {
	memheader_t *mem;

	if( data == NULL ) {
		_Mem_Error( "Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline );
	}

	mem = (memheader_t *)( (uint8_t *) data - sizeof( memheader_t ) );

	assert( mem->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( *( (uint8_t *) mem + sizeof( memheader_t ) + mem->size ) == MEMHEADER_SENTINEL2 );

	if( mem->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}
	if( *( (uint8_t *) mem + sizeof( memheader_t ) + mem->size ) != MEMHEADER_SENTINEL2 ) {
		_Mem_Error( "Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline );
	}
}

static void _Mem_CheckSentinelsPool( mempool_t *pool, const char *filename, int fileline ) {
	memheader_t *mem;
	mempool_t *child;

	// recurse into children
	if( pool->child ) {
		for( child = pool->child; child; child = child->next )
			_Mem_CheckSentinelsPool( child, filename, fileline );
	}

	assert( pool->sentinel1 == MEMHEADER_SENTINEL1 );
	assert( pool->sentinel2 == MEMHEADER_SENTINEL1 );

	if( pool->sentinel1 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "_Mem_CheckSentinelsPool: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}
	if( pool->sentinel2 != MEMHEADER_SENTINEL1 ) {
		_Mem_Error( "_Mem_CheckSentinelsPool: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline );
	}

	for( mem = pool->chain; mem; mem = mem->next )
		_Mem_CheckSentinels( (void *)( (uint8_t *) mem + sizeof( memheader_t ) ), filename, fileline );
}

void _Mem_CheckSentinelsGlobal( const char *filename, int fileline ) {
	mempool_t *pool;

	for( pool = poolChain; pool; pool = pool->next )
		_Mem_CheckSentinelsPool( pool, filename, fileline );
}

static void Mem_CountPoolStats( mempool_t *pool, int *count, int *size, int *realsize ) {
	mempool_t *child;

	// recurse into children
	if( pool->child ) {
		for( child = pool->child; child; child = child->next )
			Mem_CountPoolStats( child, count, size, realsize );
	}

	if( count ) {
		( *count )++;
	}
	if( size ) {
		( *size ) += pool->totalsize;
	}
	if( realsize ) {
		( *realsize ) += pool->realsize;
	}
}

static void Mem_PrintStats( void ) {
	int count, size, real;
	int total, totalsize, realsize;
	mempool_t *pool;
	memheader_t *mem;

	Mem_CheckSentinelsGlobal();

	for( total = 0, totalsize = 0, realsize = 0, pool = poolChain; pool; pool = pool->next ) {
		count = 0; size = 0; real = 0;
		Mem_CountPoolStats( pool, &count, &size, &real );
		total += count; totalsize += size; realsize += real;
	}

	Com_Printf( "%i memory pools, totalling %i bytes (%.3fMB), %i bytes (%.3fMB) actual\n", total, totalsize, totalsize / 1048576.0,
				realsize, realsize / 1048576.0 );

	// temporary pools are not nested
	for( pool = poolChain; pool; pool = pool->next ) {
		if( ( pool->flags & MEMPOOL_TEMPORARY ) && pool->chain ) {
			Com_Printf( "%i bytes (%.3fMB) (%i bytes (%.3fMB actual)) of temporary memory still allocated (Leak!)\n", pool->totalsize, pool->totalsize / 1048576.0,
						pool->realsize, pool->realsize / 1048576.0 );
			Com_Printf( "listing temporary memory allocations for %s:\n", pool->name );

			for( mem = pool->chain; mem; mem = mem->next )
				Com_Printf( "%10" PRIuPTR " bytes allocated at %s:%i\n", (uintptr_t)mem->size, mem->filename, mem->fileline );
		}
	}
}

static void Mem_PrintPoolStats( mempool_t *pool, int listchildren, int listallocations ) {
	mempool_t *child;
	memheader_t *mem;
	int totalsize = 0, realsize = 0;

	Mem_CountPoolStats( pool, NULL, &totalsize, &realsize );

	if( pool->parent ) {
		if( pool->lastchecksize != 0 && totalsize != pool->lastchecksize ) {
			Com_Printf( "%6ik (%6ik actual) %s:%s (%i byte change)\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->parent->name, pool->name, totalsize - pool->lastchecksize );
		} else {
			Com_Printf( "%6ik (%6ik actual) %s:%s\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->parent->name, pool->name );
		}
	} else {
		if( pool->lastchecksize != 0 && totalsize != pool->lastchecksize ) {
			Com_Printf( "%6ik (%6ik actual) %s (%i byte change)\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->name, totalsize - pool->lastchecksize );
		} else {
			Com_Printf( "%6ik (%6ik actual) %s\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->name );
		}
	}

	pool->lastchecksize = totalsize;

	if( listallocations ) {
		for( mem = pool->chain; mem; mem = mem->next )
			Com_Printf( "%10" PRIuPTR " bytes allocated at %s:%i\n", (uintptr_t)mem->size, mem->filename, mem->fileline );
	}

	if( listchildren ) {
		if( pool->child ) {
			for( child = pool->child; child; child = child->next )
				Mem_PrintPoolStats( child, listchildren, listallocations );
		}
	}
}

static void Mem_PrintList( int listchildren, int listallocations ) {
	mempool_t *pool;

	Mem_CheckSentinelsGlobal();

	Com_Printf( "memory pool list:\n" "size    name\n" );

	for( pool = poolChain; pool; pool = pool->next )
		Mem_PrintPoolStats( pool, listchildren, listallocations );
}

static void MemList_f( void ) {
	mempool_t *pool;
	const char *name = "";

	switch( Cmd_Argc() ) {
		case 1:
			Mem_PrintList( true, false );
			Mem_PrintStats();
			return;
		case 2:
			if( !Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
				Mem_PrintList( true, true );
				Mem_PrintStats();
				break;
			}
			name = Cmd_Argv( 1 );
			break;
		default:
			name = Cmd_Args();
			break;
	}

	for( pool = poolChain; pool; pool = pool->next ) {
		if( !Q_stricmp( pool->name, name ) ) {
			Com_Printf( "memory pool list:\n" "size    name\n" );
			Mem_PrintPoolStats( pool, true, true );
			return;
		}
	}

	Com_Printf( "MemList_f: unknown pool name '%s'. Usage: %s [all|pool]\n", name, Cmd_Argv( 0 ) );
}

static void MemStats_f( void ) {
	Mem_CheckSentinelsGlobal();
	Mem_PrintStats();
}


/*
* Memory_Init
*/
void Memory_Init( void ) {
	assert( !memory_initialized );

	memMutex = QMutex_Create();

	zoneMemPool = Mem_AllocPool( NULL, "Zone" );
	tempMemPool = Mem_AllocTempPool( "Temporary Memory" );

	memory_initialized = true;
}

/*
* Memory_InitCommands
*/
void Memory_InitCommands( void ) {
	assert( !commands_initialized );

	developer_memory = Cvar_Get( "developer_memory", "0", 0 );

	Cmd_AddCommand( "memlist", MemList_f );
	Cmd_AddCommand( "memstats", MemStats_f );

	commands_initialized = true;
}

/*
* Memory_Shutdown
*
* NOTE: Should be the last called function before shutdown!
*/
void Memory_Shutdown( void ) {
	mempool_t *pool, *next;

	if( !memory_initialized ) {
		return;
	}

	// set the cvar to NULL so nothing is printed to non-existing console
	developer_memory = NULL;

	Mem_CheckSentinelsGlobal();

	Mem_FreePool( &zoneMemPool );
	Mem_FreePool( &tempMemPool );

	for( pool = poolChain; pool; pool = next ) {
		// do it here, because pool is to be freed
		// and the chain will be broken
		next = pool->next;
#ifdef SHOW_NONFREED
		Com_Printf( "Warning: Memory pool %s was never freed\n", pool->name );
#endif
		Mem_FreePool( &pool );
	}

	QMutex_Destroy( &memMutex );

	memory_initialized = false;
}

/*
* Memory_ShutdownCommands
*/
void Memory_ShutdownCommands( void ) {
	if( !commands_initialized ) {
		return;
	}

	Cmd_RemoveCommand( "memlist" );
	Cmd_RemoveCommand( "memstats" );
}
