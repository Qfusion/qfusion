#ifndef CACHING_GAME_ALLOCATOR_H
#define CACHING_GAME_ALLOCATOR_H

#include <unordered_set>

class UntypedCachingGameAllocator
{
    const size_t chunkSize;
    const size_t limit;
    const char *tag;
    unsigned usedChunksCount;
    unsigned cachedChunksCount;
    void **cache;

    std::unordered_set<void *> knownChunks;

    bool isInitialized;
    bool isCleared;

    void Init();
    void Clear();

    void *AllocDirect();

public:
    UntypedCachingGameAllocator(size_t chunkSize, const char *tag = nullptr, unsigned limit = 32, unsigned initialCacheSize = 8);
    ~UntypedCachingGameAllocator();

    void *Alloc();
    void Free(void *ptr);
};

template<typename T, unsigned N>
class CachingGameBufferAllocator: UntypedCachingGameAllocator
{
    static constexpr unsigned alignedElemSize()
    {
        return (sizeof(T) % alignof(T)) ? sizeof(T) + alignof(T) - (sizeof(T) % alignof(T)) : sizeof(T);
    }
public:
    CachingGameBufferAllocator(const char *tag, size_t limit = 32, int initialCacheSize = 8)
        : UntypedCachingGameAllocator(alignedElemSize() * N, tag, limit, initialCacheSize) {}

    inline T *Alloc()
    {
        return (T*)UntypedCachingGameAllocator::Alloc();
    }
    inline void Free(T *ptr)
    {
        UntypedCachingGameAllocator::Free(ptr);
    }
};

#endif
