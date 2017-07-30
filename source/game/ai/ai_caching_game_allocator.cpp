#include "ai_caching_game_allocator.h"
#include "ai_local.h"
#include "ai_shutdown_hooks_holder.h"

UntypedCachingGameAllocator::UntypedCachingGameAllocator( size_t elemSize,
														  const char *tag,
														  size_t limit, unsigned initialCacheSize )
	: chunkSize( elemSize ), limit( limit ), tag( tag ), 
	usedChunksCount( 0 ), cachedChunksCount( initialCacheSize ),
	cache( nullptr ), isInitialized( false ), isCleared( false ) {
	if( this->tag == NULL )
		this->tag = "unknown tag";
}

void UntypedCachingGameAllocator::Init() {
	if( isInitialized ) {
		return;
	}

	cache = (void **)G_Malloc( sizeof( void * ) * limit );

	for( unsigned i = 0; i < cachedChunksCount; ++i ) {
		cache[i] = AllocDirect();
	}

	AiShutdownHooksHolder::Instance()->RegisterHook([&] { this->Clear(); } );

	isInitialized = true;
}

void UntypedCachingGameAllocator::Clear() {
	if( !isInitialized ) {
		return;
	}

	if( isCleared ) {
		AI_FailWith( "UntypedCachingGameAllocator::Clear()", "%s: Has been already cleared\n", tag );
	}
	for( unsigned i = 0; i < cachedChunksCount; ++i ) {
		G_Free( cache[i] );
	}
	G_Free( cache );
	isCleared = true;
}

UntypedCachingGameAllocator::~UntypedCachingGameAllocator() {
	if( isCleared ) {
		return;
	}
	if( !isInitialized ) {
		return;
	}
	AI_FailWith( "UntypedCachingGameAllocator::~UntypedCachingGameAllocator()", "%s: Has not been cleared\n", tag );
}

void* UntypedCachingGameAllocator::AllocDirect() {
	void *ptr = G_Malloc( chunkSize );
	knownChunks.insert( ptr );
	return ptr;
}

void *UntypedCachingGameAllocator::Alloc() {
	Init();

	if( usedChunksCount == limit ) {
		AI_FailWith( "UntypedCachingGameAllocator::Alloc()", "%s: Can't allocate more than %d chunks\n", tag, (int) limit );
	}
	usedChunksCount++;
	void *chunk = cachedChunksCount > 0 ? cache[--cachedChunksCount] : AllocDirect();
	return chunk;
}

void UntypedCachingGameAllocator::Free( void *ptr ) {
	if( !knownChunks.count( ptr ) ) {
		AI_FailWith( "UntypedCachingGameAllocator::Free()", "%s: Attempt to free chunk %p that has not been registered\n", tag, ptr );
	}
	cache[cachedChunksCount++] = ptr;
	usedChunksCount--;
}