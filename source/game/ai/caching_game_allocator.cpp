#include "caching_game_allocator.h"
#include "ai_local.h"
#include "ai_shutdown_hooks_holder.h"

#define FANCY_TAG "CachedGameAllocator\"%s\"::"

UntypedCachingGameAllocator::UntypedCachingGameAllocator(size_t elemSize, const char *tag, unsigned limit, unsigned initialCacheSize)
    : chunkSize(elemSize), limit(limit), tag(tag ? tag : "unknown tag")
{
    isCleared = false;
    isInitialized = false;

    cachedChunksCount = initialCacheSize;
    usedChunksCount = 0;
}

void UntypedCachingGameAllocator::Init()
{
    if (isInitialized)
        return;

    cache = (void **)G_Malloc(sizeof(void *) * limit);

    for (unsigned i = 0; i < cachedChunksCount; ++i)
    {
        cache[i] = AllocDirect();
    }

    AiShutdownHooksHolder::Instance()->RegisterHook([&]{ this->Clear(); });

    isInitialized = true;
}

void UntypedCachingGameAllocator::Clear()
{
    if (!isInitialized)
        return;

    if (isCleared)
    {
        printf(FANCY_TAG "Clear(): has been already cleared\n", tag);
        abort();
    }
    for (unsigned i = 0; i < cachedChunksCount; ++i)
    {
        G_Free(cache[i]);
    }
    G_Free(cache);
    isCleared = true;
}

UntypedCachingGameAllocator::~UntypedCachingGameAllocator()
{
    if (isCleared)
        return;
    if (!isInitialized)
        return;
    printf(FANCY_TAG "~CachingGameAllocator(): has not been cleared\n", tag);
    abort();
}

void* UntypedCachingGameAllocator::AllocDirect()
{
    void *ptr = G_Malloc(chunkSize);
    knownChunks.insert(ptr);
    return ptr;
}

void *UntypedCachingGameAllocator::Alloc()
{
    Init();

    if (usedChunksCount == limit)
    {
        printf(FANCY_TAG "Alloc(): Can't allocate more than %d chunks\n", tag, (int) limit);
        abort();
    }
    usedChunksCount++;
    void *chunk = cachedChunksCount > 0 ? cache[--cachedChunksCount] : AllocDirect();
    return chunk;
}

void UntypedCachingGameAllocator::Free(void *ptr)
{
    if (!knownChunks.count(ptr))
    {
        printf(FANCY_TAG "Free(): Attempt to free chunk %p that has not been registered\n", tag, ptr);
        abort();
    }
    cache[cachedChunksCount++] = ptr;
    usedChunksCount--;
}