#include "ai_aas_route_cache.h"
#include "static_vector.h"
#include "../g_local.h"
#undef min
#undef max
#include <stdlib.h>
#include <limits>


// Static member definition
AiAasRouteCache *AiAasRouteCache::shared = nullptr;

void AiAasRouteCache::Init(const AiAasWorld &aasWorld)
{
    if (shared)
    {
        G_Printf("AiAasRouteCache::Init(): shared instance is already present\n");
        abort();
    }
    // AiAasRouteCache is quite large, so it should be allocated on heap
    shared = (AiAasRouteCache *)G_Malloc(sizeof(AiAasRouteCache));
    new(shared) AiAasRouteCache(*AiAasWorld::Instance());
}

void AiAasRouteCache::Shutdown()
{
    // This may be called on first map load when an instance has never been instantiated
    if (shared)
    {
        shared->~AiAasRouteCache();
        G_Free(shared);
        // Allow the pointer to be reused, otherwise an assertion will fail on a next Init() call
        shared = nullptr;
    }
}

AiAasRouteCache *AiAasRouteCache::NewInstance()
{
    return new(G_Malloc(sizeof(AiAasRouteCache))) AiAasRouteCache(Shared());
}

void AiAasRouteCache::ReleaseInstance(AiAasRouteCache *instance)
{
    if (instance == Shared())
    {
        G_Printf("AiAasRouteCache::ReleaseInstance() called on shared object\n");
        abort();
    }

    instance->~AiAasRouteCache();
    G_Free(instance);
}

AiAasRouteCache::AiAasRouteCache(const AiAasWorld &aasWorld)
    : aasWorld(aasWorld), loaded(false)
{
    InitAreaFlags();
    //
    InitTravelFlagFromType();
    //
    InitAreaContentsTravelFlags();
    //initialize the routing update fields
    InitRoutingUpdate();
    //create reversed reachability links used by the routing update algorithm
    CreateReversedReachability();
    //initialize the cluster cache
    InitClusterAreaCache();
    //initialize portal cache
    InitPortalCache();
    //initialize the area travel times
    CalculateAreaTravelTimes();
    //calculate the maximum travel times through portals
    InitPortalMaxTravelTimes();
    //get the areas reachabilities go through
    InitReachabilityAreas();
    loaded = true;
}

AiAasRouteCache::AiAasRouteCache(AiAasRouteCache &&that)
    : aasWorld(that.aasWorld), loaded(true)
{
    areaflags = that.areaflags;

    memcpy(travelflagfortype, that.travelflagfortype, sizeof(travelflagfortype));

    areacontentstravelflags = that.areacontentstravelflags;

    areaupdate = that.areaupdate;
    portalupdate = that.portalupdate;

    reversedreachability = that.reversedreachability;

    clusterareacache = that.clusterareacache;
    portalcache = that.portalcache;

    areatraveltimes = that.areatraveltimes;
    portalmaxtraveltimes = that.portalmaxtraveltimes;

    reachabilityareas = that.reachabilityareas;
    reachabilityareaindex = that.reachabilityareaindex;

    that.loaded = false;
}

AiAasRouteCache::AiAasRouteCache(AiAasRouteCache *parent)
    : aasWorld(parent->aasWorld), loaded(true)
{
    InitAreaFlags();

    memcpy(travelflagfortype, parent->travelflagfortype, sizeof(travelflagfortype));

    areacontentstravelflags = parent->areacontentstravelflags;
    AddRef(areacontentstravelflags);

    InitRoutingUpdate();

    reversedreachability = parent->reversedreachability;
    AddRef(reversedreachability);

    InitClusterAreaCache();
    InitPortalCache();

    areatraveltimes = parent->areatraveltimes;
    AddRef(areatraveltimes);
    portalmaxtraveltimes = parent->portalmaxtraveltimes;
    AddRef(portalmaxtraveltimes);

    reachabilityareas = parent->reachabilityareas;
    AddRef(reachabilityareas);
    reachabilityareaindex = parent->reachabilityareaindex;
    AddRef(reachabilityareaindex);
}

AiAasRouteCache::~AiAasRouteCache()
{
    if (!loaded)
        return;

    // free all the existing cluster area cache
    FreeAllClusterAreaCache();
    // free all the existing portal cache
    FreeAllPortalCache();
    // free cached travel times within areas
    FreeRefCountedMemory(areatraveltimes);
    // free cached maximum travel time through cluster portals
    FreeRefCountedMemory(portalmaxtraveltimes);
    // free reversed reachability links
    FreeRefCountedMemory(reversedreachability);
    // free routing algorithm memory
    FreeMemory(areaupdate);
    FreeMemory(portalupdate);
    // free lists with areas the reachabilities go through
    FreeRefCountedMemory(reachabilityareas);
    // free the reachability area index
    FreeRefCountedMemory(reachabilityareaindex);
    // free area contents travel flags look up table
    FreeRefCountedMemory(areacontentstravelflags);
    // free a local copy of areasettings flags
    FreeMemory(areaflags);
}

inline int AiAasRouteCache::ClusterAreaNum(int cluster, int areanum)
{
    int areacluster = aasWorld.AreaSettings()[areanum].cluster;
    if (areacluster > 0)
        return aasWorld.AreaSettings()[areanum].clusterareanum;

    int side = aasWorld.Portals()[-areacluster].frontcluster != cluster;
    return aasWorld.Portals()[-areacluster].clusterareanum[side];
}

void AiAasRouteCache::InitTravelFlagFromType()
{
    for (int i = 0; i < MAX_TRAVELTYPES; i++)
    {
        travelflagfortype[i] = TFL_INVALID;
    }
    travelflagfortype[TRAVEL_INVALID] = TFL_INVALID;
    travelflagfortype[TRAVEL_WALK] = TFL_WALK;
    travelflagfortype[TRAVEL_CROUCH] = TFL_CROUCH;
    travelflagfortype[TRAVEL_BARRIERJUMP] = TFL_BARRIERJUMP;
    travelflagfortype[TRAVEL_JUMP] = TFL_JUMP;
    travelflagfortype[TRAVEL_LADDER] = TFL_LADDER;
    travelflagfortype[TRAVEL_WALKOFFLEDGE] = TFL_WALKOFFLEDGE;
    travelflagfortype[TRAVEL_SWIM] = TFL_SWIM;
    travelflagfortype[TRAVEL_WATERJUMP] = TFL_WATERJUMP;
    travelflagfortype[TRAVEL_TELEPORT] = TFL_TELEPORT;
    travelflagfortype[TRAVEL_ELEVATOR] = TFL_ELEVATOR;
    travelflagfortype[TRAVEL_ROCKETJUMP] = TFL_ROCKETJUMP;
    travelflagfortype[TRAVEL_BFGJUMP] = TFL_BFGJUMP;
    travelflagfortype[TRAVEL_GRAPPLEHOOK] = TFL_GRAPPLEHOOK;
    travelflagfortype[TRAVEL_DOUBLEJUMP] = TFL_DOUBLEJUMP;
    travelflagfortype[TRAVEL_RAMPJUMP] = TFL_RAMPJUMP;
    travelflagfortype[TRAVEL_STRAFEJUMP] = TFL_STRAFEJUMP;
    travelflagfortype[TRAVEL_JUMPPAD] = TFL_JUMPPAD;
    travelflagfortype[TRAVEL_FUNCBOB] = TFL_FUNCBOB;
}

inline int AiAasRouteCache::TravelFlagForType(int traveltype)
{
    int tfl = 0;

    if (traveltype & TRAVELFLAG_NOTTEAM1)
        tfl |= TFL_NOTTEAM1;
    if (traveltype & TRAVELFLAG_NOTTEAM2)
        tfl |= TFL_NOTTEAM2;
    traveltype &= TRAVELTYPE_MASK;
    if (traveltype < 0 || traveltype >= MAX_TRAVELTYPES)
        return TFL_INVALID;
    tfl |= travelflagfortype[traveltype];
    return tfl;
}

void AiAasRouteCache::UnlinkCache(aas_routingcache_t *cache)
{
    if (cache->time_next)
        cache->time_next->time_prev = cache->time_prev;
    else
        newestcache = cache->time_prev;
    if (cache->time_prev)
        cache->time_prev->time_next = cache->time_next;
    else
        oldestcache = cache->time_next;
    cache->time_next = nullptr;
    cache->time_prev = nullptr;
}

void AiAasRouteCache::LinkCache(aas_routingcache_t *cache)
{
    if (newestcache)
    {
        newestcache->time_next = cache;
        cache->time_prev = newestcache;
    }
    else
    {
        oldestcache = cache;
        cache->time_prev = nullptr;
    }
    cache->time_next = nullptr;
    newestcache = cache;
}

void AiAasRouteCache::FreeRoutingCache(aas_routingcache_t *cache)
{
    UnlinkCache(cache);
    FreePooledChunk(cache);
}

void AiAasRouteCache::RemoveRoutingCacheInCluster(int clusternum)
{
    if (!clusterareacache)
        return;

    const aas_cluster_t *cluster = &aasWorld.Clusters()[clusternum];
    for (int i = 0; i < cluster->numareas; i++)
    {
        aas_routingcache_t *cache, *nextcache;
        for (cache = clusterareacache[clusternum][i]; cache; cache = nextcache)
        {
            nextcache = cache->next;
            FreeRoutingCache(cache);
        }
        clusterareacache[clusternum][i] = nullptr;
    }
}

void AiAasRouteCache::RemoveRoutingCacheUsingArea( int areanum )
{
    int clusternum = aasWorld.AreaSettings()[areanum].cluster;
    if (clusternum > 0)
    {
        //remove all the cache in the cluster the area is in
        RemoveRoutingCacheInCluster( clusternum );
    }
    else
    {
        // if this is a portal remove all cache in both the front and back cluster
        RemoveRoutingCacheInCluster( aasWorld.Portals()[-clusternum].frontcluster );
        RemoveRoutingCacheInCluster( aasWorld.Portals()[-clusternum].backcluster );
    }
    // remove all portal cache
    for (int i = 0; i < aasWorld.NumAreas(); i++)
    {
        aas_routingcache_t *cache, *nextcache;
        //refresh portal cache
        for (cache = portalcache[i]; cache; cache = nextcache)
        {
            nextcache = cache->next;
            FreeRoutingCache(cache);
        }
        portalcache[i] = nullptr;
    }
}

int AiAasRouteCache::EnableRoutingArea(int areanum, int enable)
{
    if (areanum <= 0 || areanum >= aasWorld.NumAreas())
    {
        return 0;
    }
    int flags = areaflags[areanum] & AREA_DISABLED;
    if (enable < 0)
        return !flags;

    // Qfusion: thats why we need a local copy of areaflags
    if (enable)
        areaflags[areanum] &= ~AREA_DISABLED;
    else
        areaflags[areanum] |= AREA_DISABLED;
    // if the status of the area changed
    if ( (flags & AREA_DISABLED) != (areaflags[areanum] & AREA_DISABLED) )
    {
        //remove all routing cache involving this area
        RemoveRoutingCacheUsingArea( areanum );
    }
    return !flags;
}

int AiAasRouteCache::GetAreaContentsTravelFlags(int areanum)
{
    int contents = aasWorld.AreaSettings()[areanum].contents;
    int tfl = 0;
    if (contents & AREACONTENTS_WATER)
        tfl |= TFL_WATER;
    else if (contents & AREACONTENTS_SLIME)
        tfl |= TFL_SLIME;
    else if (contents & AREACONTENTS_LAVA)
        tfl |= TFL_LAVA;
    else
        tfl |= TFL_AIR;
    if (contents & AREACONTENTS_DONOTENTER)
        tfl |= TFL_DONOTENTER;
    if (contents & AREACONTENTS_NOTTEAM1)
        tfl |= TFL_NOTTEAM1;
    if (contents & AREACONTENTS_NOTTEAM2)
        tfl |= TFL_NOTTEAM2;
    if (aasWorld.AreaSettings()[areanum].areaflags & AREA_BRIDGE)
        tfl |= TFL_BRIDGE;
    return tfl;
}

void AiAasRouteCache::InitAreaFlags()
{
    areaflags = (int *)GetClearedMemory(aasWorld.NumAreas() * sizeof(int));
    for (int i = 0; i < aasWorld.NumAreas(); ++i)
    {
        areaflags[i] = aasWorld.AreaSettings()[i].areaflags;
    }
}

void AiAasRouteCache::InitAreaContentsTravelFlags(void)
{
    areacontentstravelflags = (int *) GetClearedRefCountedMemory(aasWorld.NumAreas() * sizeof(int));
    for (int i = 0; i < aasWorld.NumAreas(); i++)
    {
        areacontentstravelflags[i] = GetAreaContentsTravelFlags(i);
    }
}

void AiAasRouteCache::CreateReversedReachability(void)
{
    //allocate memory for the reversed reachability links
    char *ptr = (char *) GetClearedRefCountedMemory(aasWorld.NumAreas() * sizeof(aas_reversedreachability_t) +
                                                    aasWorld.NumReachabilities() * sizeof(aas_reversedlink_t));
    //
    reversedreachability = (aas_reversedreachability_t *) ptr;
    //pointer to the memory for the reversed links
    ptr += aasWorld.NumAreas() * sizeof(aas_reversedreachability_t);
    //check all reachabilities of all areas
    for (int i = 1; i < aasWorld.NumAreas(); i++)
    {
        //settings of the area
        const aas_areasettings_t *settings = &aasWorld.AreaSettings()[i];
        //
        if (settings->numreachableareas >= 128)
            G_Printf(S_COLOR_YELLOW "area %d has more than 128 reachabilities\n", i);
        //create reversed links for the reachabilities
        for (int n = 0; n < settings->numreachableareas && n < 128; n++)
        {
            //reachability link
            const aas_reachability_t *reach = &aasWorld.Reachabilities()[settings->firstreachablearea + n];
            //
            aas_reversedlink_t *revlink = (aas_reversedlink_t *) ptr;
            ptr += sizeof(aas_reversedlink_t);
            //
            revlink->areanum = i;
            revlink->linknum = settings->firstreachablearea + n;
            revlink->next = reversedreachability[reach->areanum].first;
            reversedreachability[reach->areanum].first = revlink;
            reversedreachability[reach->areanum].numlinks++;
        }
    }
}

//travel time in hundreths of a second = distance * 100 / speed
constexpr auto DISTANCEFACTOR_CROUCH = 1.3f; //crouch speed = 100
constexpr auto DISTANCEFACTOR_SWIM = 1.0f;	 //should be 0.66, swim speed = 150
constexpr auto DISTANCEFACTOR_WALK = 0.33f;	 //walk speed = 300

unsigned short AiAasRouteCache::AreaTravelTime(int areanum, const vec3_t start, const vec3_t end)
{
    vec3_t dir;
    VectorSubtract(start, end, dir);
    float dist = VectorLength(dir);
    //if crouch only area
    if (aasWorld.AreaCrouch(areanum))
        dist *= DISTANCEFACTOR_CROUCH;
    else if (aasWorld.AreaSwim(areanum))
        dist *= DISTANCEFACTOR_SWIM;
    else
        dist *= DISTANCEFACTOR_WALK;
    //
    int intdist = (int) dist;
    //make sure the distance isn't zero
    if (intdist <= 0) intdist = 1;
    return intdist;
}

#define PAD(base, alignment) (((base)+(alignment)-1) & ~((alignment)-1))

void AiAasRouteCache::CalculateAreaTravelTimes(void)
{
    //get the total size of all the area travel times
    int size = aasWorld.NumAreas() * sizeof(unsigned short **);
    for (int i = 0; i < aasWorld.NumAreas(); i++)
    {
        aas_reversedreachability_t *revreach = &reversedreachability[i];
        //settings of the area
        const aas_areasettings_t *settings = &aasWorld.AreaSettings()[i];
        //
        size += settings->numreachableareas * sizeof(unsigned short *);
        //
        size += settings->numreachableareas *  PAD(revreach->numlinks, sizeof(long)) * sizeof(unsigned short);
    }
    //allocate memory for the area travel times
    char *ptr = (char *) GetClearedRefCountedMemory(size);
    areatraveltimes = (unsigned short ***) ptr;
    ptr += aasWorld.NumAreas() * sizeof(unsigned short **);
    //calcluate the travel times for all the areas
    for (int i = 0; i < aasWorld.NumAreas(); i++)
    {
        //reversed reachabilities of this area
        aas_reversedreachability_s *revreach = &reversedreachability[i];
        //settings of the area
        const aas_areasettings_t *settings = &aasWorld.AreaSettings()[i];
        //
        areatraveltimes[i] = (unsigned short **) ptr;
        ptr += settings->numreachableareas * sizeof(unsigned short *);
        //
        for (int l = 0; l < settings->numreachableareas; l++)
        {
            areatraveltimes[i][l] = (unsigned short *) ptr;
            ptr += PAD(revreach->numlinks, sizeof(long)) * sizeof(unsigned short);
            //reachability link
            const aas_reachability_t *reach = &aasWorld.Reachabilities()[settings->firstreachablearea + l];
            //
            int n = 0;
            aas_reversedlink_t *revlink = revreach->first;
            for (; revlink; revlink = revlink->next, n++)
            {
                vec3_t end;
                VectorCopy(aasWorld.Reachabilities()[revlink->linknum].end, end);
                //
                areatraveltimes[i][l][n] = AreaTravelTime(i, end, reach->start);
            }
        }
    }
}

int AiAasRouteCache::PortalMaxTravelTime(int portalnum)
{
    const aas_portal_t *portal = &aasWorld.Portals()[portalnum];
    //reversed reachabilities of this portal area
    const aas_reversedreachability_t *revreach = &reversedreachability[portal->areanum];
    //settings of the portal area
    const aas_areasettings_t *settings = &aasWorld.AreaSettings()[portal->areanum];
    //
    int maxt = 0;
    for (int l = 0; l < settings->numreachableareas; l++)
    {
        int n = 0;
        aas_reversedlink_t *revlink = revreach->first;
        for (; revlink; revlink = revlink->next, n++)
        {
            int t = areatraveltimes[portal->areanum][l][n];
            if (t > maxt)
                maxt = t;
        }
    }
    return maxt;
}

void AiAasRouteCache::InitPortalMaxTravelTimes(void)
{
    portalmaxtraveltimes = (int *) GetClearedRefCountedMemory(aasWorld.NumPortals() * sizeof(int));
    for (int i = 0; i < aasWorld.NumPortals(); i++)
    {
        portalmaxtraveltimes[i] = PortalMaxTravelTime(i);
    }
}


AiAasRouteCache::FreelistPool::FreelistPool(void *buffer, unsigned bufferSize, unsigned chunkSize)
    : buffer((char *)buffer), chunkSize(chunkSize), maxChunks(bufferSize / (chunkSize + sizeof(ChunkHeader)))
{
#ifdef _DEBUG
    if (((uintptr_t)buffer) & 7)
    {
        G_Printf("Illegal buffer pointer (should be at least 8-byte-aligned)\n");
        abort();
    }
#endif

    freeChunk = nullptr;
    if (maxChunks)
    {
        // We can't use array access on Chunk * pointer since real chunk size is not a sizeof(Chunk).
        // Next chunk has this offset in bytes from previous one:
        unsigned stride = chunkSize + sizeof(ChunkHeader);
        char *nextChunkPtr = this->buffer + stride;
        Chunk *currChunk = (Chunk *)this->buffer;
        freeChunk = &currChunk->header;
        for (unsigned i = 0; i < maxChunks - 1; ++i)
        {
            Chunk *nextChunk = (Chunk *)(nextChunkPtr);
            currChunk->header.prev = nullptr;
            currChunk->header.next = &nextChunk->header;
            currChunk = nextChunk;
            nextChunkPtr += stride;
        }
        currChunk->header.prev = nullptr;
        currChunk->header.next = nullptr;
    }
    headChunk.next = &headChunk;
    headChunk.prev = &headChunk;
}

void *AiAasRouteCache::FreelistPool::Alloc(int size)
{
#ifdef _DEBUG
    if ((unsigned)size > chunkSize)
        abort();

    if (!freeChunk)
        abort();
#endif

    ChunkHeader *newChunk = freeChunk;
    freeChunk = newChunk->next;

    newChunk->prev = &headChunk;
    newChunk->next = headChunk.next;
    newChunk->next->prev = newChunk;
    newChunk->prev->next = newChunk;

    return newChunk + 1;
}

void AiAasRouteCache::FreelistPool::Free(void *ptr)
{
#ifdef _DEBUG
    if (!MayOwn(ptr))
        abort();
#endif

    ChunkHeader *oldChunk = ((ChunkHeader *)ptr) - 1;
    oldChunk->prev->next = oldChunk->next;
    oldChunk->next->prev = oldChunk->prev;
    oldChunk->next = freeChunk;
    freeChunk = oldChunk;
}

AiAasRouteCache::ChunksCache::ChunksCache()
    : pooledChunks(buffer, sizeof(buffer), CHUNK_SIZE), heapMemoryUsed(0)
{
}

void *AiAasRouteCache::ChunksCache::Alloc(int size)
{
    // Likely case first
    if ((unsigned)size <= CHUNK_SIZE && !pooledChunks.IsFull())
        return pooledChunks.Alloc(size);

    unsigned realSize = size + sizeof(Envelope);
    Envelope *envelope = (Envelope *)G_Malloc(realSize);
    envelope->realSize = realSize;
    heapMemoryUsed += realSize;
    return envelope + 1;
}

void AiAasRouteCache::ChunksCache::Free(void *ptr)
{
    // Likely case first
    if (pooledChunks.MayOwn(ptr))
    {
        pooledChunks.Free(ptr);
        return;
    }

    Envelope *envelope = ((Envelope *)ptr) - 1;
    heapMemoryUsed -= envelope->realSize;
    G_Free(envelope);
}

void *AiAasRouteCache::GetClearedMemory(int size)
{
    return G_Malloc(size);
}

void AiAasRouteCache::FreeMemory(void *ptr)
{
    G_Free(ptr);
}

bool AiAasRouteCache::FreeOldestCache()
{
    aas_routingcache_t *cache;
    for (cache = oldestcache; cache; cache = cache->time_next)
    {
        // never free area cache leading towards a portal
        if (cache->type == CACHETYPE_AREA && aasWorld.AreaSettings()[cache->areanum].cluster < 0)
        {
            continue;
        }
        break;
    }
    if (!cache)
        return false;

    // unlink the cache
    if (cache->type == CACHETYPE_AREA)
    {
        //number of the area in the cluster
        int clusterareanum = ClusterAreaNum(cache->cluster, cache->areanum);
        // unlink from cluster area cache
        if (cache->prev)
            cache->prev->next = cache->next;
        else
            clusterareacache[cache->cluster][clusterareanum] = cache->next;
        if (cache->next)
            cache->next->prev = cache->prev;
    }
    else
    {
        // unlink from portal cache
        if (cache->prev)
            cache->prev->next = cache->next;
        else
            portalcache[cache->areanum] = cache->next;
        if (cache->next)
            cache->next->prev = cache->prev;
    }
    FreeRoutingCache(cache);
    return true;
}

AiAasRouteCache::aas_routingcache_t *AiAasRouteCache::AllocRoutingCache(int numtraveltimes)
{
    int size = sizeof(aas_routingcache_t)
           + numtraveltimes * sizeof(unsigned short int)
           + numtraveltimes * sizeof(unsigned char);

    aas_routingcache_t *cache = (aas_routingcache_t *) AllocPooledChunk(size);
    cache->reachabilities = (unsigned char *) cache + sizeof(aas_routingcache_t)
                            + numtraveltimes * sizeof(unsigned short int);
    cache->size = size;
    return cache;
}

void AiAasRouteCache::FreeAllClusterAreaCache()
{
    //free all cluster cache if existing
    if (!clusterareacache)
        return;

    //free caches
    for (int i = 0; i < aasWorld.NumClusters(); i++)
    {
        const aas_cluster_t *cluster = &aasWorld.Clusters()[i];
        for (int j = 0; j < cluster->numareas; j++)
        {
            aas_routingcache_t *cache, *nextcache;
            for (cache = clusterareacache[i][j]; cache; cache = nextcache)
            {
                nextcache = cache->next;
                FreeRoutingCache(cache);
            }
            clusterareacache[i][j] = nullptr;
        }
    }
    //free the cluster cache array
    FreeMemory(clusterareacache);
    clusterareacache = nullptr;
}

void AiAasRouteCache::InitClusterAreaCache(void)
{
    int i, size;
    for (size = 0, i = 0; i < aasWorld.NumClusters(); i++)
    {
        size += aasWorld.Clusters()[i].numareas;
    }
    //two dimensional array with pointers for every cluster to routing cache
    //for every area in that cluster
    int numBytes = aasWorld.NumClusters() * sizeof(aas_routingcache_t **) + size * sizeof(aas_routingcache_t *);
    char *ptr = (char *) GetClearedMemory(numBytes);
    clusterareacache = (aas_routingcache_t ***) ptr;
    ptr += aasWorld.NumClusters() * sizeof(aas_routingcache_t **);
    for (i = 0; i < aasWorld.NumClusters(); i++)
    {
        clusterareacache[i] = (aas_routingcache_t **) ptr;
        ptr += aasWorld.Clusters()[i].numareas * sizeof(aas_routingcache_t *);
    }
}

void AiAasRouteCache::FreeAllPortalCache(void)
{
    //free all portal cache if existing
    if (!portalcache)
        return;
    //free portal caches
    for (int i = 0; i < aasWorld.NumAreas(); i++)
    {
        aas_routingcache_t *cache, *nextcache;
        for (cache = portalcache[i]; cache; cache = nextcache)
        {
            nextcache = cache->next;
            FreeRoutingCache(cache);
        }
        portalcache[i] = nullptr;
    }
    FreeMemory(portalcache);
    portalcache = nullptr;
}

void AiAasRouteCache::InitPortalCache(void)
{
    portalcache = (aas_routingcache_t **) GetClearedMemory(aasWorld.NumAreas() * sizeof(aas_routingcache_t *));
}

void AiAasRouteCache::InitRoutingUpdate(void)
{
    int maxreachabilityareas = 0;
    for (int i = 0; i < aasWorld.NumClusters(); i++)
    {
        if (aasWorld.Clusters()[i].numreachabilityareas > maxreachabilityareas)
        {
            maxreachabilityareas = aasWorld.Clusters()[i].numreachabilityareas;
        }
    }
    //allocate memory for the routing update fields
    areaupdate = (aas_routingupdate_t *) GetClearedMemory(maxreachabilityareas * sizeof(aas_routingupdate_t));
    //allocate memory for the portal update fields
    portalupdate = (aas_routingupdate_t *) GetClearedMemory((aasWorld.NumPortals() + 1) * sizeof(aas_routingupdate_t));
}

constexpr auto MAX_REACHABILITYPASSAREAS = 32;

void AiAasRouteCache::InitReachabilityAreas()
{
    int areas[MAX_REACHABILITYPASSAREAS];
    vec3_t start, end;

    reachabilityareas = (aas_reachabilityareas_t *)
        GetClearedRefCountedMemory(aasWorld.NumReachabilities() * sizeof(aas_reachabilityareas_t));
    reachabilityareaindex = (int *)
        GetClearedRefCountedMemory(aasWorld.NumReachabilities() * MAX_REACHABILITYPASSAREAS * sizeof(int));

    int numreachareas = 0;
    for (int i = 0; i < aasWorld.NumReachabilities(); i++)
    {
        const aas_reachability_t *reach = &aasWorld.Reachabilities()[i];
        int numareas = 0;
        switch(reach->traveltype & TRAVELTYPE_MASK)
        {
            //trace areas from start to end
            case TRAVEL_BARRIERJUMP:
            case TRAVEL_WATERJUMP:
                VectorCopy(reach->start, end);
                end[2] = reach->end[2];
                numareas = aasWorld.TraceAreas(reach->start, end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
                break;
            case TRAVEL_WALKOFFLEDGE:
                VectorCopy(reach->end, start);
                start[2] = reach->start[2];
                numareas = aasWorld.TraceAreas(start, reach->end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
                break;
            case TRAVEL_GRAPPLEHOOK:
                numareas = aasWorld.TraceAreas(reach->start, reach->end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
                break;

                //trace arch
            case TRAVEL_JUMP: break;
            case TRAVEL_ROCKETJUMP: break;
            case TRAVEL_BFGJUMP: break;
            case TRAVEL_JUMPPAD: break;

                //trace from reach->start to entity center, along entity movement
                //and from entity center to reach->end
            case TRAVEL_ELEVATOR: break;
            case TRAVEL_FUNCBOB: break;

                //no areas in between
            case TRAVEL_WALK: break;
            case TRAVEL_CROUCH: break;
            case TRAVEL_LADDER: break;
            case TRAVEL_SWIM: break;
            case TRAVEL_TELEPORT: break;
            default: break;
        } //end switch
        reachabilityareas[i].firstarea = numreachareas;
        reachabilityareas[i].numareas = numareas;
        for (int j = 0; j < numareas; j++)
        {
            reachabilityareaindex[numreachareas++] = areas[j];
        }
    }
}

void AiAasRouteCache::UpdateAreaRoutingCache(aas_routingcache_t *areacache)
{
    unsigned short startareatraveltimes[128]; //NOTE: not more than 128 reachabilities per area allowed

    //number of reachability areas within this cluster
    int numreachabilityareas = aasWorld.Clusters()[areacache->cluster].numreachabilityareas;
    //clear the routing update fields
//	Com_Memset(aasworld.areaupdate, 0, aasworld.numareas * sizeof(aas_routingupdate_t));
    //
    int badtravelflags = ~areacache->travelflags;
    //
    int clusterareanum = ClusterAreaNum(areacache->cluster, areacache->areanum);
    if (clusterareanum >= numreachabilityareas) return;
    //
    memset(startareatraveltimes, 0, sizeof(startareatraveltimes));
    //
    aas_routingupdate_t *curupdate = &areaupdate[clusterareanum];
    curupdate->areanum = areacache->areanum;
    //VectorCopy(areacache->origin, curupdate->start);
    curupdate->areatraveltimes = startareatraveltimes;
    curupdate->tmptraveltime = areacache->starttraveltime;
    //
    areacache->traveltimes[clusterareanum] = areacache->starttraveltime;
    //put the area to start with in the current read list
    curupdate->next = nullptr;
    curupdate->prev = nullptr;
    aas_routingupdate_t *updateliststart = curupdate;
    aas_routingupdate_t *updatelistend = curupdate;
    //while there are updates in the current list
    while (updateliststart)
    {
        curupdate = updateliststart;
        //
        if (curupdate->next)
            curupdate->next->prev = nullptr;
        else
            updatelistend = nullptr;
        updateliststart = curupdate->next;
        //
        curupdate->inlist = false;
        //check all reversed reachability links
        aas_reversedreachability_t *revreach = &reversedreachability[curupdate->areanum];
        //
        int i = 0;
        aas_reversedlink_t *revlink = revreach->first;
        for (; revlink; revlink = revlink->next, i++)
        {
            int linknum = revlink->linknum;
            const aas_reachability_t *reach = &aasWorld.Reachabilities()[linknum];
            //if there is used an undesired travel type
            if (TravelFlagForType(reach->traveltype) & badtravelflags)
                continue;
            //if not allowed to enter the next area
            // Qfusion note: we use areaflags of this AiAasRouteCache instance
            if (areaflags[reach->areanum] & AREA_DISABLED)
                continue;
            //if the next area has a not allowed travel flag
            if (AreaContentsTravelFlags(reach->areanum) & badtravelflags)
                continue;
            //number of the area the reversed reachability leads to
            int nextareanum = revlink->areanum;
            //get the cluster number of the area
            int cluster = aasWorld.AreaSettings()[nextareanum].cluster;
            //don't leave the cluster
            if (cluster > 0 && cluster != areacache->cluster)
                continue;
            //get the number of the area in the cluster
            clusterareanum = ClusterAreaNum(areacache->cluster, nextareanum);
            if (clusterareanum >= numreachabilityareas)
                continue;
            //time already travelled plus the traveltime through
            //the current area plus the travel time from the reachability
            unsigned short t = curupdate->tmptraveltime + curupdate->areatraveltimes[i] + reach->traveltime;
            //
            if (!areacache->traveltimes[clusterareanum] || areacache->traveltimes[clusterareanum] > t)
            {
                int firstnextareareach = aasWorld.AreaSettings()[nextareanum].firstreachablearea;
                areacache->traveltimes[clusterareanum] = t;
                areacache->reachabilities[clusterareanum] = linknum - firstnextareareach;
                aas_routingupdate_t *nextupdate = &areaupdate[clusterareanum];
                nextupdate->areanum = nextareanum;
                nextupdate->tmptraveltime = t;
                //VectorCopy(reach->start, nextupdate->start);
                nextupdate->areatraveltimes = areatraveltimes[nextareanum][linknum - firstnextareareach];
                if (!nextupdate->inlist)
                {
                    // we add the update to the end of the list
                    // we could also use a B+ tree to have a real sorted list
                    // on travel time which makes for faster routing updates
                    nextupdate->next = nullptr;
                    nextupdate->prev = updatelistend;
                    if (updatelistend)
                        updatelistend->next = nextupdate;
                    else
                        updateliststart = nextupdate;
                    updatelistend = nextupdate;
                    nextupdate->inlist = true;
                }
            }
        }
    }
}

AiAasRouteCache::aas_routingcache_t *AiAasRouteCache::GetAreaRoutingCache(int clusternum, int areanum, int travelflags)
{
    //number of the area in the cluster
    int clusterareanum = ClusterAreaNum(clusternum, areanum);
    //pointer to the cache for the area in the cluster
    aas_routingcache_t *clustercache = clusterareacache[clusternum][clusterareanum];
    //find the cache without undesired travel flags
    aas_routingcache_t *cache = clustercache;
    for (; cache; cache = cache->next)
    {
        //if there aren't used any undesired travel types for the cache
        if (cache->travelflags == travelflags)
            break;
    }
    //if there was no cache
    if (!cache)
    {
        cache = AllocRoutingCache(aasWorld.Clusters()[clusternum].numreachabilityareas);
        cache->cluster = clusternum;
        cache->areanum = areanum;
        VectorCopy(aasWorld.Areas()[areanum].center, cache->origin);
        cache->starttraveltime = 1;
        cache->travelflags = travelflags;
        cache->prev = nullptr;
        cache->next = clustercache;
        if (clustercache)
            clustercache->prev = cache;
        clusterareacache[clusternum][clusterareanum] = cache;
        UpdateAreaRoutingCache(cache);
    }
    else
    {
        UnlinkCache(cache);
    }
    //the cache has been accessed
    cache->type = CACHETYPE_AREA;
    LinkCache(cache);
    return cache;
}

void AiAasRouteCache::UpdatePortalRoutingCache(aas_routingcache_t *portalcache)
{
    aas_routingupdate_t *curupdate = &portalupdate[aasWorld.NumPortals()];
    curupdate->cluster = portalcache->cluster;
    curupdate->areanum = portalcache->areanum;
    curupdate->tmptraveltime = portalcache->starttraveltime;
    //if the start area is a cluster portal, store the travel time for that portal
    int clusternum = aasWorld.AreaSettings()[portalcache->areanum].cluster;
    if (clusternum < 0)
    {
        portalcache->traveltimes[-clusternum] = portalcache->starttraveltime;
    }
    //put the area to start with in the current read list
    curupdate->next = nullptr;
    curupdate->prev = nullptr;
    aas_routingupdate_t *updateliststart = curupdate;
    aas_routingupdate_t *updatelistend = curupdate;
    //while there are updates in the current list
    while (updateliststart)
    {
        curupdate = updateliststart;
        //remove the current update from the list
        if (curupdate->next)
            curupdate->next->prev = nullptr;
        else
            updatelistend = nullptr;
        updateliststart = curupdate->next;
        //current update is removed from the list
        curupdate->inlist = false;
        //
        const aas_cluster_t *cluster = &aasWorld.Clusters()[curupdate->cluster];
        //
        aas_routingcache_t *cache = GetAreaRoutingCache(curupdate->cluster, curupdate->areanum, portalcache->travelflags);
        //take all portals of the cluster
        for (int i = 0; i < cluster->numportals; i++)
        {
            int portalnum = aasWorld.PortalIndex()[cluster->firstportal + i];
            const aas_portal_t *portal = &aasWorld.Portals()[portalnum];
            //if this is the portal of the current update continue
            if (portal->areanum == curupdate->areanum)
                continue;
            //
            int clusterareanum = ClusterAreaNum(curupdate->cluster, portal->areanum);
            if (clusterareanum >= cluster->numreachabilityareas)
                continue;
            //
            unsigned short t = cache->traveltimes[clusterareanum];
            if (!t)
                continue;
            t += curupdate->tmptraveltime;

            if (!portalcache->traveltimes[portalnum] || portalcache->traveltimes[portalnum] > t)
            {
                portalcache->traveltimes[portalnum] = t;
                aas_routingupdate_t *nextupdate = &portalupdate[portalnum];
                if (portal->frontcluster == curupdate->cluster)
                {
                    nextupdate->cluster = portal->backcluster;
                }
                else
                {
                    nextupdate->cluster = portal->frontcluster;
                }
                nextupdate->areanum = portal->areanum;
                //add travel time through the actual portal area for the next update
                nextupdate->tmptraveltime = t + portalmaxtraveltimes[portalnum];
                if (!nextupdate->inlist)
                {
                    // we add the update to the end of the list
                    // we could also use a B+ tree to have a real sorted list
                    // on travel time which makes for faster routing updates
                    nextupdate->next = nullptr;
                    nextupdate->prev = updatelistend;
                    if (updatelistend)
                        updatelistend->next = nextupdate;
                    else
                        updateliststart = nextupdate;
                    updatelistend = nextupdate;
                    nextupdate->inlist = true;
                }
            }
        }
    }
}

AiAasRouteCache::aas_routingcache_t *AiAasRouteCache::GetPortalRoutingCache(int clusternum, int areanum, int travelflags)
{
    aas_routingcache_t *cache;
    //find the cached portal routing if existing
    for (cache = portalcache[areanum]; cache; cache = cache->next)
    {
        if (cache->travelflags == travelflags)
            break;
    }
    //if the portal routing isn't cached
    if (!cache)
    {
        cache = AllocRoutingCache(aasWorld.NumPortals());
        cache->cluster = clusternum;
        cache->areanum = areanum;
        VectorCopy(aasWorld.Areas()[areanum].center, cache->origin);
        cache->starttraveltime = 1;
        cache->travelflags = travelflags;
        //add the cache to the cache list
        cache->prev = nullptr;
        cache->next = portalcache[areanum];
        if (portalcache[areanum])
            portalcache[areanum]->prev = cache;
        portalcache[areanum] = cache;
        //update the cache
        UpdatePortalRoutingCache(cache);
    }
    else
    {
        UnlinkCache(cache);
    }
    //the cache has been accessed
    cache->type = CACHETYPE_PORTAL;
    LinkCache(cache);
    return cache;
}

bool AiAasRouteCache::RouteToGoalArea(int areanum, const vec3_t origin, int goalareanum, int travelflags, int *traveltime, int *reachnum)
{
    if (areanum == goalareanum)
    {
        *traveltime = 1;
        *reachnum = 0;
        return true;
    }
    //
    if (areanum <= 0 || areanum >= aasWorld.NumAreas())
    {
        return false;
    }
    if (goalareanum <= 0 || goalareanum >= aasWorld.NumAreas())
    {
        return false;
    }
    if (aasWorld.AreaDoNotEnter(areanum) || aasWorld.AreaDoNotEnter(goalareanum))
    {
        travelflags |= TFL_DONOTENTER;
    }

    while (ShouldDrainCache())
    {
        if (FreeOldestCache())
            break;
    }

    int clusternum = aasWorld.AreaSettings()[areanum].cluster;
    int goalclusternum = aasWorld.AreaSettings()[goalareanum].cluster;
    //check if the area is a portal of the goal area cluster
    if (clusternum < 0 && goalclusternum > 0)
    {
        const aas_portal_t *portal = &aasWorld.Portals()[-clusternum];
        if (portal->frontcluster == goalclusternum || portal->backcluster == goalclusternum)
        {
            clusternum = goalclusternum;
        }
    }
    //check if the goalarea is a portal of the area cluster
    else if (clusternum > 0 && goalclusternum < 0)
    {
        const aas_portal_t *portal = &aasWorld.Portals()[-goalclusternum];
        if (portal->frontcluster == clusternum || portal->backcluster == clusternum)
        {
            goalclusternum = clusternum;
        }
    }

    //if both areas are in the same cluster
    //NOTE: there might be a shorter route via another cluster!!! but we don't care
    if (clusternum > 0 && goalclusternum > 0 && clusternum == goalclusternum)
    {
        aas_routingcache_t *areacache = GetAreaRoutingCache(clusternum, goalareanum, travelflags);
        //the number of the area in the cluster
        int clusterareanum = ClusterAreaNum(clusternum, areanum);
        //the cluster the area is in
        const aas_cluster_t *cluster = &aasWorld.Clusters()[clusternum];
        //if the area is NOT a reachability area
        if (clusterareanum >= cluster->numreachabilityareas)
            return 0;
        //if it is possible to travel to the goal area through this cluster
        if (areacache->traveltimes[clusterareanum] != 0)
        {
            *reachnum = aasWorld.AreaSettings()[areanum].firstreachablearea + areacache->reachabilities[clusterareanum];
            if (!origin)
            {
                *traveltime = areacache->traveltimes[clusterareanum];
                return true;
            }
            const aas_reachability_t *reach = &aasWorld.Reachabilities()[*reachnum];
            *traveltime = areacache->traveltimes[clusterareanum] + AreaTravelTime(areanum, origin, reach->start);
            return true;
        }
    }
    //
    clusternum = aasWorld.AreaSettings()[areanum].cluster;
    goalclusternum = aasWorld.AreaSettings()[goalareanum].cluster;
    //if the goal area is a portal
    if (goalclusternum < 0)
    {
        //just assume the goal area is part of the front cluster
        const aas_portal_t *portal = &aasWorld.Portals()[-goalclusternum];
        goalclusternum = portal->frontcluster;
    }
    //get the portal routing cache
    aas_routingcache_t *portalcache = GetPortalRoutingCache(goalclusternum, goalareanum, travelflags);
    //if the area is a cluster portal, read directly from the portal cache
    if (clusternum < 0)
    {
        *traveltime = portalcache->traveltimes[-clusternum];
        *reachnum = aasWorld.AreaSettings()[areanum].firstreachablearea + portalcache->reachabilities[-clusternum];
        return true;
    }
    //
    unsigned short besttime = 0;
    int bestreachnum = -1;
    //the cluster the area is in
    const aas_cluster_t *cluster = &aasWorld.Clusters()[clusternum];
    //find the portal of the area cluster leading towards the goal area
    for (int i = 0; i < cluster->numportals; i++)
    {
        int portalnum = aasWorld.PortalIndex()[cluster->firstportal + i];
        //if the goal area isn't reachable from the portal
        if (!portalcache->traveltimes[portalnum])
            continue;
        //
        const aas_portal_t *portal = &aasWorld.Portals()[portalnum];
        //get the cache of the portal area
        aas_routingcache_t *areacache = GetAreaRoutingCache(clusternum, portal->areanum, travelflags);
        //current area inside the current cluster
        int clusterareanum = ClusterAreaNum(clusternum, areanum);
        //if the area is NOT a reachability area
        if (clusterareanum >= cluster->numreachabilityareas)
            continue;
        //if the portal is NOT reachable from this area
        if (!areacache->traveltimes[clusterareanum])
            continue;
        //total travel time is the travel time the portal area is from
        //the goal area plus the travel time towards the portal area
        unsigned short t = portalcache->traveltimes[portalnum] + areacache->traveltimes[clusterareanum];
        //FIXME: add the exact travel time through the actual portal area
        //NOTE: for now we just add the largest travel time through the portal area
        //		because we can't directly calculate the exact travel time
        //		to be more specific we don't know which reachability was used to travel
        //		into the portal area
        t += portalmaxtraveltimes[portalnum];
        //
        if (origin)
        {
            *reachnum = aasWorld.AreaSettings()[areanum].firstreachablearea +
                        areacache->reachabilities[clusterareanum];
            const aas_reachability_t *reach = aasWorld.Reachabilities() + *reachnum;
            t += AreaTravelTime(areanum, origin, reach->start);
        }
        //if the time is better than the one already found
        if (!besttime || t < besttime)
        {
            bestreachnum = *reachnum;
            besttime = t;
        }
    }
    if (bestreachnum < 0)
        return false;

    *reachnum = bestreachnum;
    *traveltime = besttime;
    return true;
}
