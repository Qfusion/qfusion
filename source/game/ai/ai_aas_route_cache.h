#ifndef QFUSION_AI_ROUTE_CACHE_H
#define QFUSION_AI_ROUTE_CACHE_H

#include "ai_aas_world.h"

//travel flags
#define TFL_INVALID				0x00000001	//traveling temporary not possible
#define TFL_WALK				0x00000002	//walking
#define TFL_CROUCH				0x00000004	//crouching
#define TFL_BARRIERJUMP			0x00000008	//jumping onto a barrier
#define TFL_JUMP				0x00000010	//jumping
#define TFL_LADDER				0x00000020	//climbing a ladder
#define TFL_WALKOFFLEDGE		0x00000080	//walking of a ledge
#define TFL_SWIM				0x00000100	//swimming
#define TFL_WATERJUMP			0x00000200	//jumping out of the water
#define TFL_TELEPORT			0x00000400	//teleporting
#define TFL_ELEVATOR			0x00000800	//elevator
#define TFL_ROCKETJUMP			0x00001000	//rocket jumping
#define TFL_BFGJUMP				0x00002000	//bfg jumping
#define TFL_GRAPPLEHOOK			0x00004000	//grappling hook
#define TFL_DOUBLEJUMP			0x00008000	//double jump
#define TFL_RAMPJUMP			0x00010000	//ramp jump
#define TFL_STRAFEJUMP			0x00020000	//strafe jump
#define TFL_JUMPPAD				0x00040000	//jump pad
#define TFL_AIR					0x00080000	//travel through air
#define TFL_WATER				0x00100000	//travel through water
#define TFL_SLIME				0x00200000	//travel through slime
#define TFL_LAVA				0x00400000	//travel through lava
#define TFL_DONOTENTER			0x00800000	//travel through donotenter area
#define TFL_FUNCBOB				0x01000000	//func bobbing
#define TFL_FLIGHT				0x02000000	//flight
#define TFL_BRIDGE				0x04000000	//move over a bridge
//
#define TFL_NOTTEAM1			0x08000000	//not team 1
#define TFL_NOTTEAM2			0x10000000	//not team 2

//default travel flags
#define TFL_DEFAULT	TFL_WALK|TFL_CROUCH|TFL_BARRIERJUMP|\
	TFL_JUMP|TFL_LADDER|\
	TFL_WALKOFFLEDGE|TFL_SWIM|TFL_WATERJUMP|\
	TFL_TELEPORT|TFL_ELEVATOR|\
	TFL_AIR|TFL_WATER|TFL_JUMPPAD|TFL_FUNCBOB

class AiAasRouteCache
{
    static constexpr int CACHETYPE_PORTAL = 0;
    static constexpr int CACHETYPE_AREA = 1;

    //routing cache
    typedef struct aas_routingcache_s
    {
        int type;							        //portal or area cache
        int size;									//size of the routing cache
        int cluster;								//cluster the cache is for
        int areanum;								//area the cache is created for
        vec3_t origin;								//origin within the area
        float starttraveltime;						//travel time to start with
        int travelflags;							//combinations of the travel flags
        struct aas_routingcache_s *prev, *next;
        struct aas_routingcache_s *time_prev, *time_next;
        unsigned char *reachabilities;				//reachabilities used for routing
        unsigned short int traveltimes[1];			//travel time for every area (variable sized)
    } aas_routingcache_t;

    //fields for the routing algorithm
    typedef struct aas_routingupdate_s
    {
        int cluster;
        int areanum;								//area number of the update
        unsigned short int tmptraveltime;			//temporary travel time
        unsigned short int *areatraveltimes;		//travel times within the area
        bool marked;							    //true if the update is in the list
    } aas_routingupdate_t;

    //reversed reachability link
    typedef struct aas_reversedlink_s
    {
        int linknum;								//the aas_areareachability_t
        int areanum;								//reachable from this area
        struct aas_reversedlink_s *next;			//next link
    } aas_reversedlink_t;

    //reversed area reachability
    typedef struct aas_reversedreachability_s
    {
        int numlinks;
        aas_reversedlink_t *first;
    } aas_reversedreachability_t;

    //areas a reachability goes through
    typedef struct aas_reachabilityareas_s
    {
        int firstarea, numareas;
    } aas_reachabilityareas_t;

    const AiAasWorld &aasWorld;

    bool loaded;

    // These three following buffers are allocated at once, and only the first one should be released.
    // Total size of compound allocated buffer is aasWorld.NumAreas() * (2 * sizeof(int) + 2 * sizeof(bool))
    // A scratchpad for SetDisabledRegions() that is capable to store aasWorld.NumAreas() values
    int *currDisabledAreaNums;
    // A scratchpad for SetDisabledRegions() that is capable to store aasWorld.NumAreas() values
    int *cleanCacheAreaNums;
    // Has (almost) direct mapping to area num indices:
    // for an index i value at i * 2 + 0 is a new status
    // for an index i value at i * 2 + 1 is an old status
    // We do not use bitsets since variable shifts are required in used access patterns,
    // and variable shift instructions are usually microcoded.
    // We store adjacent pair of statuses according to the memory access pattern used.
    bool *oldAndCurrAreaDisabledStatus;

    //index to retrieve travel flag for a travel type
    // Note this is not shared for faster local acccess
    int travelflagfortype[MAX_TRAVELTYPES];
    //travel flags for each area based on contents
    int *areacontentstravelflags;
    //routing update
    aas_routingupdate_t *areaupdate;
    aas_routingupdate_t *portalupdate;
    //reversed reachability links
    aas_reversedreachability_t *reversedreachability;
    //travel times within the areas
    unsigned short ***areatraveltimes;
    //array of size numclusters with cluster cache
    aas_routingcache_t ***clusterareacache;
    aas_routingcache_t **portalcache;
    //cache list sorted on time
    aas_routingcache_t *oldestcache;		// start of cache list sorted on time
    aas_routingcache_t *newestcache;		// end of cache list sorted on time
    //maximum travel time through portal areas
    int *portalmaxtraveltimes;
    //areas the reachabilities go through
    int *reachabilityareaindex;
    aas_reachabilityareas_t *reachabilityareas;

    // We have to waste 8 bytes for the ref count since blocks should be at least 8-byte aligned
    inline static const int64_t RefCountOf(const void *chunk) { return *(((int64_t *)chunk) - 1); }
    inline static int64_t &RefCountOf(void *chunk) { return *(((int64_t *)chunk) - 1); }

    inline static void AddRef(void *chunk) { RefCountOf(chunk)++; }

    inline void *GetClearedRefCountedMemory(int size)
    {
        void *mem = ((int64_t *)GetClearedMemory(size + 8)) + 1;
        RefCountOf(mem) = 1;
        return mem;
    }

    inline void FreeRefCountedMemory(void *ptr)
    {
        --RefCountOf(ptr);
        if (!RefCountOf(ptr))
            FreeMemory(((int64_t *)ptr) - 1);
    }

    class FreelistPool
    {
    public:
        struct ChunkHeader
        {
            ChunkHeader *prev;
            ChunkHeader *next;
        };
    private:
        // Freelist head
        ChunkHeader headChunk;
        // Freelist free item
        ChunkHeader *freeChunk;
        // Actual chunks data
        char *buffer;

        // An actual chunk data size and a maximal count of chunks.
        const unsigned chunkSize, maxChunks;
        unsigned chunksInUse;
    public:

        FreelistPool(void *buffer_, unsigned bufferSize, unsigned chunkSize_);
        virtual ~FreelistPool() {}

        void *Alloc(int size);
        void Free(void *ptr);

        // True result does not guarantee that the pool owns the pointer.
        // False result guarantees that the pool does not own the pointer.
        inline bool MayOwn(const void *ptr)
        {
            return ptr >= buffer && ptr < buffer + maxChunks * (chunkSize + sizeof(ChunkHeader));
        }
        inline bool IsFull() const { return freeChunk == nullptr; }
        inline unsigned Size() const { return chunksInUse; }
        inline unsigned Capacity() const { return maxChunks; }
    };

    // The enclosing class is either allocated via G_Malloc() that should be at least 8-byte aligned,
    // or stored in a StaticVector that has 16-byte alignment.
    class alignas(8) ChunksCache
    {
        static constexpr unsigned CHUNK_SIZE = 8192 - sizeof(FreelistPool::ChunkHeader);
        static constexpr unsigned MAX_CHUNKS = 640; // 512+128

        alignas(8) char buffer[MAX_CHUNKS * (CHUNK_SIZE + sizeof(FreelistPool::ChunkHeader))];

        FreelistPool pooledChunks;
        unsigned heapMemoryUsed;

        // An envelope for heap-allocated chunks. Has at least 8-bit intrinsic alignment.
        struct Envelope
        {
            // For alignment purposes this should be an 8-byte item
            uint64_t realSize;
        };
    public:
        ChunksCache();

        void *Alloc(int size);
        void Free(void *ptr);

        bool NeedsCleanup()
        {
            if (pooledChunks.Size() / (float)pooledChunks.Capacity() > 0.66f)
                return true;
            return heapMemoryUsed > sizeof(buffer) / 3;
        }
    };

    ChunksCache chunksCache;

    inline int ClusterAreaNum(int cluster, int areanum);
    void InitTravelFlagFromType();

    void LinkCache(aas_routingcache_t *cache);
    void UnlinkCache(aas_routingcache_t *cache);

    void FreeRoutingCache(aas_routingcache_t *cache);

    void RemoveRoutingCacheInClusterForArea(int areaNum);
    void RemoveRoutingCacheInCluster(int clusternum);
    void RemoveAllPortalsCache();

    int GetAreaContentsTravelFlags(int areanum);

    inline void *AllocPooledChunk(int size)
    {
        return chunksCache.Alloc(size);
    }
    inline void FreePooledChunk(void *ptr)
    {
        return chunksCache.Free(ptr);
    }
    inline bool ShouldDrainCache()
    {
        return chunksCache.NeedsCleanup();
    }

    void *GetClearedMemory(int size);
    void FreeMemory(void *ptr);

    bool FreeOldestCache();
    aas_routingcache_t *AllocRoutingCache(int numtraveltimes);
    void UnlinkAndFreeRoutingCache(aas_routingcache_t *cache);
    void UnlinkAreaRoutingCache(aas_routingcache_t *cache);
    void UnlinkPortalRoutingCache(aas_routingcache_t *cache);

    void UpdateAreaRoutingCache(aas_routingcache_t *areaCache);
    aas_routingcache_t *GetAreaRoutingCache(int clusternum, int areanum, int travelflags);
    void UpdatePortalRoutingCache(aas_routingcache_t *portalCache);
    aas_routingcache_t *GetPortalRoutingCache(int clusternum, int areanum, int travelflags);
    unsigned short AreaTravelTime(int areanum, const vec3_t start, const vec3_t end);

    struct RoutingRequest
    {
        int areanum;
        const float *origin;
        int goalareanum;
        int travelflags;
    };

    struct RoutingResult
    {
        int reachnum;
        int traveltime;
    };

    bool RoutingResultToGoalArea(int fromAreaNum, const vec3_t origin, int toAreaNum, int travelFlags, RoutingResult *result) const;

    bool RouteToGoalArea(const RoutingRequest &request, RoutingResult *result);
    bool RouteToGoalPortal(const RoutingRequest &request, aas_routingcache_t *portalCache, RoutingResult *result);

    int PortalMaxTravelTime(int portalnum);

    void InitDisabledAreasStatusAndHelpers();
    void InitAreaContentsTravelFlags();
    void InitRoutingUpdate();
    void CreateReversedReachability();
    void InitClusterAreaCache();
    void InitPortalCache();
    void CalculateAreaTravelTimes();
    void InitPortalMaxTravelTimes();
    void InitReachabilityAreas();

    void FreeAllClusterAreaCache();
    void FreeAllPortalCache();

    // Should be used only for shared route cache initialization
    AiAasRouteCache(const AiAasWorld &aasWorld_);
    // Should be used for creation of new instances based on shared one
    AiAasRouteCache(AiAasRouteCache *parent);

    static AiAasRouteCache *shared;
public:
    // AiRoutingCache should be init and shutdown explicitly
    // (a game library is not unloaded when a map changes)
    static void Init(const AiAasWorld &aasWorld);
    static void Shutdown();

    static AiAasRouteCache *Shared() { return shared; }
    static AiAasRouteCache *NewInstance();
    static void ReleaseInstance(AiAasRouteCache *instance);

    // A helper for emplace_back() calls on instances of this class
    AiAasRouteCache(AiAasRouteCache &&that);
    ~AiAasRouteCache();

    inline int ReachabilityToGoalArea(int fromAreaNum, int toAreaNum, int travelFlags) const
    {
        return ReachabilityToGoalArea(fromAreaNum, nullptr, toAreaNum, travelFlags);
    }

    inline int ReachabilityToGoalArea(int fromAreaNum, const Vec3 &fromOrigin, int toAreaNum, int travelFlags) const
    {
        return ReachabilityToGoalArea(fromAreaNum, fromOrigin.Data(), toAreaNum, travelFlags);
    }

    inline int ReachabilityToGoalArea(int fromAreaNum, const vec3_t fromOrigin, int toAreaNum, int travelFlags) const
    {
        RoutingResult result;
        if (RoutingResultToGoalArea(fromAreaNum, fromOrigin, toAreaNum, travelFlags, &result))
        {
            return result.reachnum;
        }
        return 0;
    }

    inline int TravelTimeToGoalArea(int fromAreaNum, int toAreaNum, int travelFlags) const
    {
        return TravelTimeToGoalArea(fromAreaNum, nullptr, toAreaNum, travelFlags);
    }

    inline int TravelTimeToGoalArea(int fromAreaNum, const Vec3 &fromOrigin, int toAreaNum, int travelFlags) const
    {
        return TravelTimeToGoalArea(fromAreaNum, fromOrigin.Data(), toAreaNum, travelFlags);
    }

    inline int TravelTimeToGoalArea(int fromAreaNum, const vec3_t fromOrigin, int toAreaNum, int travelFlags) const
    {
        RoutingResult result;
        if (RoutingResultToGoalArea(fromAreaNum, fromOrigin, toAreaNum, travelFlags, &result))
        {
            return result.traveltime;
        }
        return 0;
    }

    inline bool AreaDisabled(int areaNum)
    {
        return oldAndCurrAreaDisabledStatus[areaNum * 2] || (aasWorld.AreaSettings()[areaNum].areaflags & AREA_DISABLED);
    }

    // For numSpots regions disables all areas in region for routing.
    // An i-th region is defined by absolute bounds mins[i], maxs[i].
    // Note than bounding box of areas disabled for region is wider than the defined region itself.
    // In order to prevent blocking important areas
    // regions will not be disabled if any of a region areas contains noBlockPoint.
    inline void SetDisabledRegions(const Vec3 *mins, const Vec3 *maxs, int numRegions, const vec3_t noBlockPoint)
    {
        SetDisabledRegions(mins, maxs, numRegions, aasWorld.PointAreaNum(noBlockPoint));
    }

    void SetDisabledRegions(const Vec3 *mins, const Vec3 *maxs, int numRegions, int noBlockAreaNum);
};

#endif
