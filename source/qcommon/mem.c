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

// ---------------------------------------------------------------------------------------------------------------------------------
//                                     _
//                                    | |
//  _ __ ___  _ __ ___   __ _ _ __    | |___
// | '_ ` _ \| '_ ` _ \ / _` | '__|   | '_  |
// | | | | | | | | | | | (_| | |    _ | | | |
// |_| |_| |_|_| |_| |_|\__, |_|   (_)|_| |_|
//                       __/ |
//                      |___/
//
// Memory manager & tracking software
//
// Best viewed with 8-character tabs and (at least) 132 columns
//
// ---------------------------------------------------------------------------------------------------------------------------------
//
// Restrictions & freedoms pertaining to usage and redistribution of this software:
//
//  * This software is 100% free
//  * If you use this software (in part or in whole) you must credit the author.
//  * This software may not be re-distributed (in part or in whole) in a modified
//    form without clear documentation on how to obtain a copy of the original work.
//  * You may not use this software to directly or indirectly cause harm to others.
//  * This software is provided as-is and without warrantee. Use at your own risk.
//
// For more information, visit HTTP://www.FluidStudios.com
//
// ---------------------------------------------------------------------------------------------------------------------------------
// Originally created on 12/22/2000 by Paul Nettle
//
// Copyright 2000, Fluid Studios, Inc., all rights reserved.
// ---------------------------------------------------------------------------------------------------------------------------------

*/
// Z_zone.c

#include "qcommon.h"
#include "qthreads.h"
#include "mod_mem.h"
#include "mem.h"

#define POOLNAMESIZE 128
#define MEMALIGNMENT_DEFAULT		16
#define MIN_MEM_ALIGNMENT sizeof(void*) 

#define AllocHashSize ( 1u << 12u )

static const unsigned int prefixPattern = 0xbaadf00d;      // Fill pattern for bytes preceeding allocated blocks
static const unsigned int postfixPattern = 0xdeadc0de;     // Fill pattern for bytes following allocated blocks
static const unsigned int unusedPattern = 0xfeedface;      // Fill pattern for freshly allocated blocks
static const unsigned int releasedPattern = 0xdeadbeef;    // Fill pattern for deallocated blocks

static bool memory_initialized = false;
static bool commands_initialized = false;

mempool_t* Q_ParentPool() {
	return NULL; 
}

#if defined(STRESS_TEST)
	static const bool AlwaysWipeAll = true; 
	static const size_t PaddingSize = 16; // 256 bytes of extra allocation
	static const bool alwaysValidateAll = true;
#elif defined( _DEBUG )
	static const bool AlwaysWipeAll = true; 
	static const size_t PaddingSize = 8;
	static const bool alwaysValidateAll = false;
#else
	static const size_t PaddingSize = 1;
	static const bool AlwaysWipeAll = false; 
	static const bool alwaysValidateAll = false;
#endif

static const bool RandomWipe = false; 
#define CANARY_SIZE (PaddingSize * sizeof(uint32_t))

struct {
	uint_fast32_t memTrackCount; // tracks the number of linked units if this is mismatched then the table is corrupted
} stats;

typedef struct memheader_s
{
	// address returned by malloc (may be significantly before this header to satisfy alignment)
	void *baseAddress;
	void *reportedAddress;
	
	struct memheader_s *hnext;
	struct memheader_s *hprev;

	// next and previous memheaders in chain belonging to pool
	struct memheader_s *next;
	struct memheader_s *prev;

	// pool this memheader belongs to
	struct mempool_s *pool;

	size_t size; // size of the memory 
	size_t alignment;
	size_t realsize; // size of the memory alignment and sentinel2

	// file name and line where Mem_Alloc was called
	char sourceFile[140];
	char sourceFunc[140];
	int sourceLine;


} memheader_t;

struct mempool_s
{
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

};

cvar_t *developerMemory;


// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this pool stays allocated during gameplay, it is
// considered a leak
mempool_t *tempMemPool;
// only for zone
mempool_t *zoneMemPool;
static mempool_t *rootChain = NULL; // root chain of mem pool without parents

static qmutex_t *memMutex;

static struct memheader_s *hashTable[AllocHashSize];
static struct memheader_s *reservoirHeaders;
static struct memheader_s **reservoirAllocHeaderBuffer;
static size_t reservoirIndex = 0;

static inline size_t __resolveUnitHashIndex(const void *reportedAddress ) {
	return ( ( (size_t)reportedAddress ) >> 4 ) & ( AllocHashSize - 1 );
}

static inline struct memheader_s *__pullMemHeaderFromReserve()
{
	if(reservoirHeaders == NULL) {
		reservoirHeaders = calloc( 256, sizeof( struct memheader_s ) );
		for( unsigned int i = 0; i < 256 - 1; i++ ) {
			reservoirHeaders[i].next = &reservoirHeaders[i + 1];
		}

		reservoirAllocHeaderBuffer = realloc( reservoirAllocHeaderBuffer, ( reservoirIndex + 1 ) * sizeof( struct memheader_s * ) );
		reservoirAllocHeaderBuffer[reservoirIndex++] = reservoirHeaders;
	}
	struct memheader_s *unit = reservoirHeaders;
	reservoirHeaders = unit->next;
	memset( unit, 0, sizeof( struct memheader_s ) );

	return unit;
}


static inline void __returnMemHeaderToReserve(struct memheader_s* mem) {
	memset( mem, 0, sizeof( struct memheader_s ) );
	mem->next = reservoirHeaders;
	reservoirHeaders = mem;
}

static void __wipeWithPattern( void *reportedAddress, size_t reportedSize, size_t originalReportedSize, uint32_t pattern )
{
	// For a serious test run, we use wipes of random a random value. However, if this causes a crash, we don't want it to
	// crash in a differnt place each time, so we specifically DO NOT call srand. If, by chance your program calls srand(),
	// you may wish to disable that when running with a random wipe test. This will make any crashes more consistent so they
	// can be tracked down easier.

	if( RandomWipe ) {
		pattern = ( ( rand() & 0xff ) << 24 ) | ( ( rand() & 0xff ) << 16 ) | ( ( rand() & 0xff ) << 8 ) | ( rand() & 0xff );
	}

	// -DOC- We should wipe with 0's if we're not in debug mode, so we can help hide bugs if possible when we release the
	// product. So uncomment the following line for releases.
	//
	// Note that the "alwaysWipeAll" should be turned on for this to have effect, otherwise it won't do much good. But we'll
	// leave it this way (as an option) because this does slow things down.
	//	pattern = 0;

	// This part of the operation is optional
  if( AlwaysWipeAll && reportedSize > originalReportedSize ) {
  	// Fill the bulk

  	uint32_t *lptr = (uint32_t *)( ( (char *)reportedAddress ) + originalReportedSize );
  	size_t length = reportedSize - originalReportedSize;
  	for( size_t i = 0; i < ( length >> 2 ); i++, lptr++ ) {
  		*lptr = pattern;
  	}

  	// Fill the remainder

  	unsigned int shiftCount = 0;
  	char *cptr = (char *)( lptr );
  	for( size_t i = 0; i < ( length & 0x3 ); i++, cptr++, shiftCount += 8 ) {
  		*cptr = (char)( ( pattern & ( 0xff << shiftCount ) ) >> shiftCount );
  	}
  }

	// Write in the prefix/postfix bytes

	// Calculate the correct start addresses for pre and post patterns relative to
	// allocUnit->reportedAddress, since it may have been offset due to alignment requirements
	uint8_t *pre = (uint8_t *)reportedAddress - PaddingSize * sizeof( uint32_t );
	uint8_t* post = (uint8_t*)reportedAddress + reportedSize;

	const size_t paddingBytes = PaddingSize * sizeof(uint32_t);
	for (size_t i = 0; i < paddingBytes; i++, pre++, post++)
	{
		*pre = (prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
		*post = (postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
	}
}

static const char* _insertCommas(unsigned int value)
{
	static char str[30];
	memset(str, 0, sizeof(str));

	sprintf(str, "%u", value);
	if (strlen(str) > 3)
	{
		memmove(&str[strlen(str) - 3], &str[strlen(str) - 4], 4);
		str[strlen(str) - 4] = ',';
	}
	if (strlen(str) > 7)
	{
		memmove(&str[strlen(str) - 7], &str[strlen(str) - 8], 8);
		str[strlen(str) - 8] = ',';
	}
	if (strlen(str) > 11)
	{
		memmove(&str[strlen(str) - 11], &str[strlen(str) - 12], 12);
		str[strlen(str) - 12] = ',';
	}

	return str;
}

static const char* __memorySizeString(uint32_t size)
{
	static char str[128];
	if (size > (1024 * 1024))
		sprintf(str, "%10s (%7.2fM)", _insertCommas(size), ((float)size) / (1024.0f * 1024.0f));
	else if (size > 1024)
		sprintf(str, "%10s (%7.2fK)", _insertCommas(size), ((float)size) / 1024.0f);
	else
		sprintf(str, "%10s bytes     ", _insertCommas(size));
	return str;
}

/**
* Links a memory block to the hash table.
*/
static inline void __linkMemory( struct memheader_s *mem )
{
	assert( mem );
	assert( mem->reportedAddress );
	assert(mem->hnext == NULL);
	assert(mem->hprev == NULL);
	
	const size_t hashIndex = __resolveUnitHashIndex( mem->reportedAddress );
	if( hashTable[hashIndex] )
		hashTable[hashIndex]->hprev = mem;
	mem->hnext = hashTable[hashIndex];
	mem->hprev = NULL;
	hashTable[hashIndex] = mem;
	stats.memTrackCount++;
}
/**
* Unlinks a memory block from any pool it may be linked to.
* free from the hash table
*/
static inline void __unlinkMemory( struct memheader_s *mem )
{
	assert( mem );
	assert( mem->reportedAddress );
	const size_t hashIndex = __resolveUnitHashIndex( mem->reportedAddress );
	if( hashTable[hashIndex] == mem ) {
		hashTable[hashIndex] = mem->hnext;
	} else {
		if( mem->hprev ) {
			mem->hprev->hnext = mem->hnext;
		}
		if( mem->hnext ) {
			mem->hnext->hprev = mem->hprev;
		}
	}
	mem->hprev = NULL;
	mem->hnext = NULL;
	stats.memTrackCount--;

}

static inline void __unlinkPool(struct memheader_s* mem) {
	assert(mem);
	if( mem->pool ) {
		if( mem->prev )
			mem->prev->next = mem->next;
		else
			mem->pool->chain = mem->next;
		if( mem->next )
			mem->next->prev = mem->prev;

		mem->pool->realsize -= mem->realsize;
		mem->pool->totalsize -= mem->size;
		mem->pool = NULL;
		mem->prev = NULL;
		mem->next = NULL;
	}
}


/**
* Links a memory block to a pool and unlinks it from any other pool it may be linked to.
*/
static inline void __linkPool(struct memheader_s* mem, struct mempool_s* pool) {
	assert(mem);
	assert(pool);

	// we are linked to a pool so unlink
	__unlinkPool(mem);
	// these should be unlinked
	assert(mem->next == NULL); 
	assert(mem->prev == NULL);

	mem->next = pool->chain;
	mem->prev = NULL;
	pool->chain = mem;
	if( mem->next )
		mem->next->prev = mem;
	mem->pool = pool;

	pool->realsize += mem->realsize;
	pool->totalsize += mem->size;
}

static struct memheader_s *__findLinkMemory( const void *reportedAddress )
{
	// Just in case...
	assert( reportedAddress != NULL );

	// Use the address to locate the hash index. Note that we shift off the lower four bits. This is because most allocated
	// addresses will be on four-, eight- or even sixteen-byte boundaries. If we didn't do this, the hash index would not have
	// very good coverage.

	const size_t hashIndex = __resolveUnitHashIndex(reportedAddress);
	struct memheader_s *ptr = hashTable[hashIndex];
	while( ptr ) {
		if( ptr->reportedAddress == reportedAddress )
			return ptr;
		ptr = ptr->hnext;
	}
	return NULL;
}

static void __dumpMemHeader(const struct memheader_s* allocUnit)
{
	if( allocUnit->pool ) {
		Com_Printf( "[I] Pool: %s\n", allocUnit->pool->name);
	}
	Com_Printf("[I] Address (reported): %010p\n", allocUnit->reportedAddress);
	Com_Printf("[I] Address (actual)  : %010p\n", allocUnit->baseAddress);
	Com_Printf("[I] Size (reported)   : 0x%08X (%s)\n", (unsigned int)(allocUnit->size), __memorySizeString((unsigned int)(allocUnit->size)));
	Com_Printf("[I] Size (actual)     : 0x%08X (%s)\n", (unsigned int)(allocUnit->realsize), __memorySizeString((unsigned int)(allocUnit->realsize)));
	Com_Printf("[I] Owner             : %s:%s(%d)\n", allocUnit->sourceFile, allocUnit->sourceFunc, allocUnit->sourceLine);
}

bool __validateAllocationHeader(const struct memheader_s* header)
{
	// Make sure the padding is untouched

	uint8_t* pre = ((uint8_t*)header->reportedAddress - PaddingSize * sizeof(uint32_t));
	uint8_t* post = ((uint8_t*)header->reportedAddress + header->size);
	bool      errorFlag = false;
	const size_t paddingBytes = PaddingSize * sizeof(uint32_t);
	for (size_t i = 0; i < paddingBytes; i++, pre++, post++)
	{
		const uint8_t expectedPrefixByte = (prefixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
		if (*pre != expectedPrefixByte)
		{
			Com_Printf("[!] A memory allocation unit was corrupt because of an underrun: offset: %d \n", i);
			__dumpMemHeader(header);
			errorFlag = true;
		}

		// If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
		// owner?) has underrun the allocation unit (modified a few bytes prior to the start). You can interrogate the
		// variable 'allocUnit' to see statistics and information about this damaged allocation unit.
		assert(*pre == expectedPrefixByte);

		const uint8_t expectedPostfixByte = (postfixPattern >> ((i % sizeof(uint32_t)) * 8)) & 0xFF;
		if (*post != expectedPostfixByte)
		{
			Com_Printf("[!] A memory allocation unit was corrupt because of an overrun: offset: %d \n", i);
			__dumpMemHeader(header);
			errorFlag = true;
		}

		// If you hit this assert, then you should know that this allocation unit has been damaged. Something (possibly the
		// owner?) has overrun the allocation unit (modified a few bytes after the end). You can interrogate the variable
		// 'allocUnit' to see statistics and information about this damaged allocation unit.
		assert(*post == expectedPostfixByte);
	}

	// Return the error status (we invert it, because a return of 'false' means error)

	return !errorFlag;
}

static void _Mem_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Sys_Error( msg );
}
struct mempool_stats_s Q_PoolStats(mempool_t *pool ) {
	struct mempool_stats_s stats = {
		.size = pool->totalsize,
		.realSize = pool->realsize
	};
	return stats;
}
void *__Q_Calloc( size_t count, size_t size, const char *sourceFilename, const char *functionName, int sourceLine ) {
	const size_t allocSize = count * size;
	void* result = __Q_MallocAligned(0, allocSize, sourceFilename, functionName, sourceLine);
	memset(result, 0, allocSize);
	return result;
}

void *__Q_CallocAligned( size_t count, size_t alignment, size_t size, const char *sourceFilename, const char *functionName, int sourceLine ) {
	const size_t allocSize = count * size;
	void* result = __Q_MallocAligned(alignment, allocSize, sourceFilename, functionName, sourceLine);
	memset(result, 0, allocSize);
	return result;
}

void *__Q_MallocAligned(size_t align, size_t size, const char* sourceFilename, const char* functionName, int sourceLine) {
	const size_t alignment = align < MIN_MEM_ALIGNMENT ? MIN_MEM_ALIGNMENT : align;
	const size_t realsize = size + ( CANARY_SIZE * 2 ) + alignment;
	void *baseAddress = malloc( realsize );
	if( baseAddress == NULL ) 
		return NULL;

	void *reportedAddress = ( (uint8_t*)baseAddress + CANARY_SIZE );
	size_t offset = ( (size_t)reportedAddress ) % alignment;
	if( offset ) {
		reportedAddress = (uint8_t *)reportedAddress + ( alignment - offset );
	}
	
	QMutex_Lock( memMutex );
	
	__wipeWithPattern(reportedAddress, size, 0, unusedPattern);

	struct memheader_s *mem = __pullMemHeaderFromReserve();
	mem->alignment = alignment;
	mem->reportedAddress = reportedAddress; 
	mem->baseAddress = baseAddress;
	mem->sourceLine = sourceLine;
	mem->size = size;
	mem->realsize = realsize;
	if(sourceFilename) {
		strncpy(mem->sourceFile, sourceFilename, sizeof(mem->sourceFile)); 	
	} else {
		strcpy(mem->sourceFile, "??");
	}
	if(functionName) {
		strncpy(mem->sourceFunc, functionName, sizeof(mem->sourceFunc)); 	
	} else {
		strcpy(mem->sourceFunc, "??");
	}

	__linkMemory(mem);
	QMutex_Unlock(memMutex);

	return reportedAddress;
}

mempool_t *Q_CreatePool( mempool_t *parent, const char *name )
{
	mempool_t *pool = (mempool_t *)malloc( sizeof( mempool_t ) );
	if( pool == NULL )
		_Mem_Error( "Mem_AllocPool: out of memory" );

	memset( pool, 0, sizeof( mempool_t ) );
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
		pool->next = rootChain;
		rootChain = pool;
	}

	return pool;
}

void *__Q_Malloc(size_t size, const char* sourceFilename, const char* functionName, int sourceline) {
	return __Q_MallocAligned(0,size, sourceFilename, functionName, sourceline);
}

void *__Q_Realloc( void *ptr, size_t size, const char *sourceFilename, const char *functionName, int sourceLine )
{
	if( ptr == NULL ) 
		return __Q_Malloc( size, sourceFilename, functionName, sourceLine );

	QMutex_Lock( memMutex );
	struct memheader_s *mem = __findLinkMemory( ptr );
	if( mem == NULL ) {
		assert( false );
		_Mem_Error( "Mem_Free: Request to deallocate RAM that was naver allocated (alloc at %s:%i)", mem->sourceFile, mem->sourceLine );
	}
	__unlinkMemory(mem); // unlink the memory because the address may change

	// realloc can't guarantee alignment so will just use min platform alignment 
	const size_t alignmentReq = MIN_MEM_ALIGNMENT; 
	const ptrdiff_t oldPtrDiff = ( (uint8_t*)mem->reportedAddress - (uint8_t*)mem->baseAddress ) - CANARY_SIZE;
	const size_t realsize = size + ( CANARY_SIZE * 2 ) + alignmentReq;
	const size_t oldReportedSize = mem->size;
	void *baseAddress = realloc( mem->baseAddress, realsize );
	void *reportedAddress = ( (uint8_t*)baseAddress + CANARY_SIZE );
	const size_t offset = ( (size_t)reportedAddress ) % alignmentReq;
	if( offset ) {
		reportedAddress = (uint8_t *)reportedAddress + ( alignmentReq - offset );
	}
	const ptrdiff_t newPtrDiff = ( (uint8_t*)reportedAddress - (uint8_t*)baseAddress ) - CANARY_SIZE;
	if(mem->pool) {
		mem->pool->realsize -= mem->realsize;
		mem->pool->totalsize -= mem->size;
		mem->pool->realsize += realsize;
		mem->pool->totalsize += size;
	}
	
	// the offset from the base address is different so we need to adjust the memory to be re-aligned when the memory was allocated
	if( newPtrDiff != oldPtrDiff ) {
		memmove( (uint8_t*)reportedAddress - CANARY_SIZE, (uint8_t*)reportedAddress - CANARY_SIZE + ( oldPtrDiff  - newPtrDiff ), mem->size + ( CANARY_SIZE * 2 ) );
	}

	// wipe the pattern and wipe in unused porition 
	__wipeWithPattern( reportedAddress, size, oldReportedSize, unusedPattern );

	mem->realsize = realsize;
	mem->size = size;
	mem->alignment = alignmentReq;
	mem->reportedAddress = reportedAddress;
	mem->baseAddress = baseAddress;
	mem->sourceLine = sourceLine;
	if(sourceFilename) {
		strncpy(mem->sourceFile, sourceFilename, sizeof(mem->sourceFile)); 	
	} else {
		strcpy(mem->sourceFile, "??");
	}
	if(functionName) {
		strncpy(mem->sourceFunc, functionName, sizeof(mem->sourceFunc)); 	
	} else {
		strcpy(mem->sourceFunc, "??");
	}
	__linkMemory(mem);
	__validateAllocationHeader( mem );

	QMutex_Unlock( memMutex );
	if( alwaysValidateAll ) {
		Mem_ValidationAllAllocations();
	}

	return mem->reportedAddress;
}

void Q_LinkToPool( void *ptr, mempool_t *pool )
{
	assert( ptr );
	assert( pool );
	QMutex_Lock( memMutex );
	struct memheader_s *mem = __findLinkMemory( ptr );
	if( mem == NULL ) {
		assert( false );
		_Mem_Error( "Mem_Free: Request to deallocate RAM that was naver allocated (alloc at %s:%i)", mem->sourceFile, mem->sourceLine );
	}
	__linkPool( mem, pool );
	QMutex_Unlock( memMutex );
}

void Q_EmptyPool( struct mempool_s *pool )
{
	assert( pool );
	QMutex_Lock( memMutex );
	size_t capacity = 16;
	size_t len = 0;
	struct mempool_s **process = malloc( sizeof( struct mempool_s * ) * capacity );
	process[len++] = pool;
	while( len > 0 ) {
		struct mempool_s *const pool = process[--len];
		pool->totalsize = 0;
		pool->realsize = 0;

		struct memheader_s *chain = pool->chain;
		while( chain ) {
			struct memheader_s *current = chain;
			chain = current->next;
			__validateAllocationHeader( current );
			__wipeWithPattern( current->reportedAddress, current->size, 0, releasedPattern );
			__unlinkMemory( current );
			__unlinkPool( current );
			__returnMemHeaderToReserve( current );
			free( current->baseAddress );
		}

		struct mempool_s *child = pool->child;
		while( child ) {
			if( len >= capacity ) {
				capacity = ( capacity >> 1 ) + capacity; // grow 1.5
				process = realloc( process, capacity );
			}
			process[len++] = child;
			child = child->next;
		}
	}
	free( process );
	QMutex_Unlock( memMutex );
}

void Q_FreePool( struct mempool_s *pool )
{
	assert( pool );
	QMutex_Lock( memMutex );
	struct mempool_s **current = pool->parent ? &( pool->parent->child ) : &rootChain;
	while( *current && *current != pool ) {
		current = &( *current )->next;
	}
	assert(*current == pool); // pool was already freed 
	assert(current != NULL);
	*current = pool->next;

	size_t capacity = 16;
	size_t len = 0;
	struct mempool_s **process = malloc( sizeof( struct mempool_s * ) * capacity );
	process[len++] = pool;
	while( len > 0 ) {
		struct mempool_s *const pool = process[--len];
		struct memheader_s *chain = pool->chain;
		while( chain ) {
			struct memheader_s *current = chain;
			chain = current->next;
			__validateAllocationHeader( current );
			__wipeWithPattern( current->reportedAddress, current->size, 0, releasedPattern );
			__unlinkMemory( current );
			__unlinkPool(current);
			free( current->baseAddress );
			__returnMemHeaderToReserve( current );
		}
		struct mempool_s *child = pool->child;
		while( child ) {
			if( len >= capacity ) {
				capacity = ( capacity >> 1 ) + capacity; // grow 1.5
				process = realloc( process, capacity );
			}
			process[len++] = child;
			child = child->next;
		}
		free( pool );
	}
	free( process );
	QMutex_Unlock( memMutex );
}

void Q_Free( void *ptr )
{
	if(!ptr) {
		return;
	}

	QMutex_Lock( memMutex );
	struct memheader_s *mem = __findLinkMemory( ptr );
	if( mem == NULL ) {
		assert( false );
		_Mem_Error( "Mem_Free: Request to deallocate RAM that was naver allocated (alloc at %s:%i)", mem->sourceFile, mem->sourceLine );
	}
	// unlink the memory 
	__unlinkMemory( mem );
	__unlinkPool(mem);

	// wipe with closing pattern
	__validateAllocationHeader( mem);
	__wipeWithPattern( mem->reportedAddress, mem->size, 0, releasedPattern );
	
	free( mem->baseAddress );
	__returnMemHeaderToReserve( mem );
	QMutex_Unlock( memMutex );
	if( alwaysValidateAll ) {
		Mem_ValidationAllAllocations();
	}
}

void *_Mem_AllocExt( mempool_t *pool, size_t size, size_t alignment, int z, int musthave, int canthave, const char *filename, int fileline )
{

	if( size <= 0 )
		return NULL;

	// default to 16-bytes alignment
	alignment = alignment < MIN_MEM_ALIGNMENT ? MIN_MEM_ALIGNMENT  : alignment;
	assert( pool != NULL );

 // if( pool == NULL )
 // 	_Mem_Error( "Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline );
 // if( musthave && ( ( pool->flags & musthave ) != musthave ) )
 // 	_Mem_Error( "Mem_Alloc: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
 // if( canthave && ( pool->flags & canthave ) )
 // 	_Mem_Error( "Mem_Alloc: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );

	if( developerMemory && developerMemory->integer )
		Com_DPrintf( "Mem_Alloc: pool %s, file %s:%i, size %i bytes\n", pool->name, filename, fileline, size );

	QMutex_Lock( memMutex );

	const size_t realsize = size + ( CANARY_SIZE * 2 ) + alignment;
	void *baseAddress = malloc( realsize );
	void *reportedAddress = ( (uint8_t*)baseAddress + CANARY_SIZE );
	struct memheader_s *mem = __pullMemHeaderFromReserve();
	
	if( baseAddress == NULL )
		_Mem_Error( "Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline );
	
	const size_t offset = ( (size_t)reportedAddress ) % alignment;
	mem->alignment = alignment;
	if( offset ) {
		reportedAddress = (uint8_t*)reportedAddress + ( alignment - offset );
	}

	// calculate address that aligns the end of the memheader_t to the specified alignment
	mem->reportedAddress = reportedAddress; 
	mem->baseAddress = baseAddress;
	mem->size = size;
	mem->realsize = realsize;
	mem->sourceLine = fileline;
	if(filename) {
		strncpy(mem->sourceFile, filename, sizeof(mem->sourceFile)); 	
	} else {
		strcpy(mem->sourceFile, "??");
	}
	strcpy(mem->sourceFunc, "??");
	
	// append to head of list
	__linkMemory(mem);
	__linkPool(mem, pool);

	__wipeWithPattern(reportedAddress, size, 0, unusedPattern);
	QMutex_Unlock( memMutex );


	if( z )
		memset(reportedAddress, 0, mem->size );

	return reportedAddress; 
}

void *_Mem_Alloc( mempool_t *pool, size_t size, int musthave, int canthave, const char *filename, int fileline )
{
	return _Mem_AllocExt( pool, size, 0, 1, musthave, canthave, filename, fileline );
}

// FIXME: rewrite this?
void *_Mem_Realloc( void *data, size_t size, const char *filename, int fileline )
{

	if( data == NULL )
		_Mem_Error( "Mem_Realloc: data == NULL (called at %s:%i)", filename, fileline );
	if( size <= 0 )
	{
		Mem_Free( data );
		return NULL;
	}
	QMutex_Lock( memMutex );
	struct memheader_s *mem = __findLinkMemory( data );
	if( mem == NULL ) {
		assert( false);
		_Mem_Error( "Mem_Free: Request to deallocate RAM that was naver allocated (alloc at %s:%i)", filename, fileline );
	}
	if( size <= mem->size ) {
		QMutex_Unlock( memMutex );
		return data;
	}

	void *newdata = Mem_AllocExt( mem->pool, size, 0 );
	memcpy( newdata, data, mem->size );
	memset( (uint8_t *)newdata + mem->size, 0, size - mem->size );
	QMutex_Unlock( memMutex );
	Mem_Free( data );
	return newdata;
}

char *_Mem_CopyString( mempool_t *pool, const char *in, const char *filename, int fileline )
{
	char *out;
	size_t num_chars = strlen( in ) + 1;
	size_t str_size = sizeof( char ) * num_chars;

	out = ( char* )_Mem_Alloc( pool, str_size, 0, 0, filename, fileline );
	memcpy( out, in, str_size );

	return out;
}

void _Mem_Free( void *data, int musthave, int canthave, const char *filename, int fileline )
{
	if( data == NULL )
		//_Mem_Error( "Mem_Free: data == NULL (called at %s:%i)", filename, fileline );
		return;

	QMutex_Lock( memMutex );

	struct memheader_s* mem = __findLinkMemory(data);
	if( mem == NULL ) {
		assert( false);
		_Mem_Error( "Mem_Free: Request to deallocate RAM that was never allocated (alloc at %s:%i)", filename, fileline );
	}
	__validateAllocationHeader(mem);

	mempool_t *pool = mem->pool;
	//if( musthave && ( ( pool->flags & musthave ) != musthave ) )
	//	_Mem_Error( "Mem_Free: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
	//if( canthave && ( pool->flags & canthave ) )
	//	_Mem_Error( "Mem_Free: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );
	if( developerMemory && developerMemory->integer )
		Com_DPrintf( "Mem_Free: pool %s, alloc %s:%i, free %s:%i, size %i bytes\n", pool->name, mem->sourceFile, mem->sourceLine, filename, fileline, mem->size );
	
	// unlink the memory 
	__unlinkMemory(mem);
	__unlinkPool(mem);

	// wipe with closing pattern
	__validateAllocationHeader( mem);
	__wipeWithPattern(mem->reportedAddress, mem->size, 0, releasedPattern);
	
	free(mem->baseAddress);
	
	// return the header to the reservoir
	__returnMemHeaderToReserve(mem);
	QMutex_Unlock( memMutex );
}

mempool_t *_Mem_AllocPool( mempool_t *parent, const char *name, int flags, const char *filename, int fileline )
{
	mempool_t *pool;

	if( parent && ( parent->flags & MEMPOOL_TEMPORARY ) )
		_Mem_Error( "Mem_AllocPool: nested temporary pools are not allowed (allocpool at %s:%i)", filename, fileline );
	if( flags & MEMPOOL_TEMPORARY )
		_Mem_Error( "Mem_AllocPool: tried to allocate temporary pool, use Mem_AllocTempPool instead (allocpool at %s:%i)", filename, fileline );

	pool = ( mempool_t* )malloc( sizeof( mempool_t ) );
	if( pool == NULL )
		_Mem_Error( "Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline );

	memset( pool, 0, sizeof( mempool_t ) );
	pool->flags = flags;
	pool->chain = NULL;
	pool->parent = parent;
	pool->child = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof( mempool_t );
	Q_strncpyz( pool->name, name, sizeof( pool->name ) );

	if( parent )
	{
		pool->next = parent->child;
		parent->child = pool;
	}
	else
	{
		pool->next = rootChain;
		rootChain = pool;
	}

	return pool;
}

mempool_t *_Mem_AllocTempPool( const char *name, const char *filename, int fileline )
{
	mempool_t *pool;

	pool = _Mem_AllocPool( NULL, name, 0, filename, fileline );
	pool->flags = MEMPOOL_TEMPORARY;

	return pool;
}

void _Mem_FreePool( mempool_t **pool, int musthave, int canthave, const char *filename, int fileline )
{
	mempool_t **chainAddress;
#ifdef SHOW_NONFREED
	memheader_t *mem;
#endif

	if( !( *pool ) )
		return;
 // if( musthave && ( ( ( *pool )->flags & musthave ) != musthave ) )
 // 	_Mem_Error( "Mem_FreePool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
 // if( canthave && ( ( *pool )->flags & canthave ) )
 // 	_Mem_Error( "Mem_FreePool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );

	Q_FreePool(*pool);
	*pool = NULL;
}

void Mem_ValidationAllAllocations() {
	QMutex_Lock( memMutex );
	uint_fast32_t memTrackCount = 0; 
	int numberErrors = 0;
	for( size_t i = 0; i < AllocHashSize; i++ ) {
		struct memheader_s *ptr = hashTable[i];
		while( ptr ) {
			if(__validateAllocationHeader( ptr ) == false) {
				numberErrors++;		
				assert(false); // this is bad means means something has gone wrong 
			}
			memTrackCount++; 
			ptr = ptr->hnext;
		}
	}
	if(memTrackCount != stats.memTrackCount) 
		Com_Printf("[!] number of tracked units is mismatched table is corrupted (found: %d expected: %d)", memTrackCount, stats.memTrackCount);

	assert(memTrackCount == stats.memTrackCount);
	if (numberErrors > 0)
		Com_Printf("[!] While validting header, %d allocation headers(s) were found to have problems", numberErrors);
	QMutex_Unlock( memMutex );
}

void _Mem_EmptyPool( mempool_t *pool, int musthave, int canthave, const char *filename, int fileline )
{
	assert(pool);
 // if( pool == NULL )
 // 	_Mem_Error( "Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline );
 // if( musthave && ( ( pool->flags & musthave ) != musthave ) )
 // 	_Mem_Error( "Mem_EmptyPool: bad pool flags (musthave) (alloc at %s:%i)", filename, fileline );
 // if( canthave && ( pool->flags & canthave ) )
 // 	_Mem_Error( "Mem_EmptyPool: bad pool flags (canthave) (alloc at %s:%i)", filename, fileline );

	Q_EmptyPool(pool);
}

size_t Mem_PoolTotalSize( mempool_t *pool )
{
	assert( pool != NULL );

	return pool->totalsize;
}

static void _Mem_CheckSentinelsPool( mempool_t *pool, const char *filename, int fileline )
{
	// recurse into children
	if( pool->child )
	{
		for( mempool_t *child = pool->child; child != NULL; child = child->next ) { 
			_Mem_CheckSentinelsPool( child, filename, fileline );
		}
	}

	for( memheader_t *mem = pool->chain; mem != NULL; mem = mem->next ) {
		__validateAllocationHeader(mem);
	}
}

void _Mem_CheckSentinelsGlobal( const char *filename, int fileline )
{
	mempool_t *pool;

	for( pool = rootChain; pool; pool = pool->next )
		_Mem_CheckSentinelsPool( pool, filename, fileline );
}

static void Mem_CountPoolStats( mempool_t *pool, int *count, int *size, int *realsize )
{
	mempool_t *child;

	// recurse into children
	if( pool->child )
	{
		for( child = pool->child; child; child = child->next )
			Mem_CountPoolStats( child, count, size, realsize );
	}

	if( count )
		( *count )++;
	if( size )
		( *size ) += pool->totalsize;
	if( realsize )
		( *realsize ) += pool->realsize;
}

static void Mem_PrintStats( void )
{
	int count, size, real;
	int total, totalsize, realsize;
	mempool_t *pool;
	memheader_t *mem;

	Mem_CheckSentinelsGlobal();

	for( total = 0, totalsize = 0, realsize = 0, pool = rootChain; pool; pool = pool->next )
	{
		count = 0; size = 0; real = 0;
		Mem_CountPoolStats( pool, &count, &size, &real );
		total += count; totalsize += size; realsize += real;
	}

	Com_Printf( "%i memory pools, totalling %i bytes (%.3fMB), %i bytes (%.3fMB) actual\n", total, totalsize, totalsize / 1048576.0,
		realsize, realsize / 1048576.0 );

	// temporary pools are not nested
	for( pool = rootChain; pool; pool = pool->next )
	{
		if( ( pool->flags & MEMPOOL_TEMPORARY ) && pool->chain )
		{
			Com_Printf( "%i bytes (%.3fMB) (%i bytes (%.3fMB actual)) of temporary memory still allocated (Leak!)\n", pool->totalsize, pool->totalsize / 1048576.0,
				pool->realsize, pool->realsize / 1048576.0 );
			Com_Printf( "listing temporary memory allocations for %s:\n", pool->name );

			for( mem = tempMemPool->chain; mem; mem = mem->next )
				Com_Printf( "%10i bytes allocated at %s:%i\n", mem->size, mem->sourceFile, mem->sourceLine );
		}
	}
}

static void Mem_PrintPoolStats( mempool_t *pool, int listchildren, int listallocations )
{
	mempool_t *child;
	memheader_t *mem;
	int totalsize = 0, realsize = 0;

	Mem_CountPoolStats( pool, NULL, &totalsize, &realsize );

	if( pool->parent )
	{
		if( pool->lastchecksize != 0 && totalsize != pool->lastchecksize )
			Com_Printf( "%6ik (%6ik actual) %s:%s (%i byte change)\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->parent->name, pool->name, totalsize - pool->lastchecksize );
		else
			Com_Printf( "%6ik (%6ik actual) %s:%s\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->parent->name, pool->name );
	}
	else
	{
		if( pool->lastchecksize != 0 && totalsize != pool->lastchecksize )
			Com_Printf( "%6ik (%6ik actual) %s (%i byte change)\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->name, totalsize - pool->lastchecksize );
		else
			Com_Printf( "%6ik (%6ik actual) %s\n", ( totalsize + 1023 ) / 1024, ( realsize + 1023 ) / 1024, pool->name );
	}

	pool->lastchecksize = totalsize;

	if( listallocations )
	{
		for( mem = pool->chain; mem; mem = mem->next )
			Com_Printf( "%10i bytes allocated at %s:%i\n", mem->size, mem->sourceFile, mem->sourceLine );
	}

	if( listchildren )
	{
		if( pool->child )
		{
			for( child = pool->child; child; child = child->next )
				Mem_PrintPoolStats( child, listchildren, listallocations );
		}
	}
}

static void Mem_PrintList( int listchildren, int listallocations )
{
	mempool_t *pool;

	Mem_CheckSentinelsGlobal();

	Com_Printf( "memory pool list:\n" "size    name\n" );

	for( pool = rootChain; pool; pool = pool->next )
		Mem_PrintPoolStats( pool, listchildren, listallocations );
}

static void MemList_f( void )
{
	mempool_t *pool;
	const char *name = "";

	switch( Cmd_Argc() )
	{
	case 1:
		Mem_PrintList( true, false );
		Mem_PrintStats();
		return;
	case 2:
		if( !Q_stricmp( Cmd_Argv( 1 ), "all" ) )
		{
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

	for( pool = rootChain; pool; pool = pool->next )
	{
		if( !Q_stricmp( pool->name, name ) )
		{
			Com_Printf( "memory pool list:\n" "size    name\n" );
			Mem_PrintPoolStats( pool, true, true );
			return;
		}
	}

	Com_Printf( "MemList_f: unknown pool name '%s'. Usage: [all|pool]\n", name, Cmd_Argv( 0 ) );
}

static void MemStats_f( void )
{
	Mem_CheckSentinelsGlobal();
	Mem_PrintStats();
}

void Mem_DumpMemoryReport() {
	QMutex_Lock( memMutex );
	Com_Printf("----------------------- Tracked Allocations ----------------------------\n");
	uint_fast32_t memTrackCount = 0; 
	int numberErrors = 0;
	for( size_t i = 0; i < AllocHashSize; i++ ) {
		struct memheader_s *ptr = hashTable[i];
		while( ptr ) {
			if(!__validateAllocationHeader( ptr )) {
				numberErrors++;		
			}
			memTrackCount++; 
			__dumpMemHeader(ptr);	
			ptr = ptr->hnext;
		}
	}
	if(memTrackCount != stats.memTrackCount) 
		Com_Printf("[!] number of tracked units is mismatched table is corrupted (found: %d expected: %d)", memTrackCount, stats.memTrackCount);

	assert(memTrackCount == stats.memTrackCount);
	if (numberErrors > 0)
		Com_Printf("[!] While validting header, %d allocation headers(s) were found to have problems", numberErrors);
	
	QMutex_Unlock( memMutex );

}

void Memory_Init( void )
{
	assert( !memory_initialized );

	memMutex = QMutex_Create();

	zoneMemPool = Mem_AllocPool( NULL, "Zone" );
	tempMemPool = Mem_AllocTempPool( "Temporary Memory" );

	memory_initialized = true;
}

/*
* Memory_InitCommands
*/
void Memory_InitCommands( void )
{
	assert( !commands_initialized );

	developerMemory = Cvar_Get( "developerMemory", "0", 0 );

	Cmd_AddCommand( "memdump", Mem_DumpMemoryReport );
	Cmd_AddCommand( "memlist", MemList_f );
	Cmd_AddCommand( "memstats", MemStats_f );

	commands_initialized = true;
}

/*
* Memory_Shutdown
* 
* NOTE: Should be the last called function before shutdown!
*/
void Memory_Shutdown( void )
{
	mempool_t *pool, *next;

	if( !memory_initialized )
		return;
	

	// set the cvar to NULL so nothing is printed to non-existing console
	developerMemory = NULL;

	Mem_CheckSentinelsGlobal();



	Mem_FreePool( &zoneMemPool );
	Mem_FreePool( &tempMemPool );

	for( pool = rootChain; pool; pool = next )
	{
		// do it here, because pool is to be freed
		// and the chain will be broken
		next = pool->next;
#ifdef SHOW_NONFREED
		Com_Printf( "Warning: Memory pool %s was never freed\n", pool->name );
#endif
		Mem_FreePool( &pool );
	}

	QMutex_Destroy( &memMutex );
	
	for(size_t i = 0; i < reservoirIndex; i++) {
		free(reservoirAllocHeaderBuffer[i]);
	}
	free(reservoirAllocHeaderBuffer);
	reservoirIndex = 0;
	reservoirAllocHeaderBuffer = NULL;
	reservoirHeaders = NULL;

	memory_initialized = false;
}

/*
* Memory_ShutdownCommands
*/
void Memory_ShutdownCommands( void )
{
	if( !commands_initialized )
		return;
	
	Cmd_RemoveCommand( "memdump");
	Cmd_RemoveCommand( "memlist" );
	Cmd_RemoveCommand( "memstats" );
}
