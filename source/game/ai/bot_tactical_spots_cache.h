#ifndef QFUSION_BOT_TACTICAL_SPOTS_CACHE_H
#define QFUSION_BOT_TACTICAL_SPOTS_CACHE_H

#include "ai_local.h"

class BotTacticalSpotsCache
{
    struct CachedSpot
    {
        short origin[3];
        short enemyOrigin[3];
        short spotData[3];
        bool succeeded;
    };

    // Use just a plain array for caching spots.
    // High number of distinct (and thus searched for) spots will kill TacticalSpotsRegistry performance first.
    struct SpotsList
    {
        static constexpr auto MAX_SPOTS = 3;
        CachedSpot spots[MAX_SPOTS];
        unsigned numSpots;

        inline SpotsList() { Clear(); }

        inline void Clear()
        {
            numSpots = 0;
            memset(spots, 0, sizeof(spots));
        }

        inline CachedSpot *Alloc()
        {
            if (numSpots == MAX_SPOTS)
                return nullptr;
            return &spots[numSpots++];
        }
    };

    SpotsList sniperRangeTacticalSpotsList;
    SpotsList farRangeTacticalSpotsList;
    SpotsList middleRangeTacticalSpotsList;
    SpotsList closeRangeTacticalSpotsList;
    SpotsList coverSpotsTacticalSpotsList;

    edict_t *self;

    bool FindSniperRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result);
    bool FindFarRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result);
    bool FindMiddleRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result);
    bool FindCloseRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result);
    bool FindCoverSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result);

    // We can't(?) refer to a nested class in a forward declaration, so declare the parameter as a template one
    template <typename ProblemParams>
    inline bool FindForOrigin(const ProblemParams &problemParams, const Vec3 &origin, float searchRadius, vec3_t result);

    // These functions are extracted to be able to mock a bot entity
    // by a player entity easily for testing and tweaking the cache
    inline class AiAasRouteCache *RouteCache();
    inline float Skill() const;
    inline bool BotHasAlmostSameOrigin(const Vec3 &unpackedOrigin) const;

    typedef bool (BotTacticalSpotsCache::*FindMethod)(const Vec3 &, const Vec3 &, vec3_t);
    const short *GetSpot(SpotsList *list, const short *origin, const short *enemyOrigin, FindMethod findMethod);
public:
    inline BotTacticalSpotsCache(edict_t *self_): self(self_) {}

    inline void Clear()
    {
        sniperRangeTacticalSpotsList.Clear();
        farRangeTacticalSpotsList.Clear();
        middleRangeTacticalSpotsList.Clear();
        closeRangeTacticalSpotsList.Clear();
        coverSpotsTacticalSpotsList.Clear();
    }

    inline const short *GetSniperRangeTacticalSpot(const short *origin, const short *enemyOrigin)
    {
        return GetSpot(&sniperRangeTacticalSpotsList, origin, enemyOrigin,
                       &BotTacticalSpotsCache::FindSniperRangeTacticalSpot);
    }
    inline const short *GetFarRangeTacticalSpot(const short *origin, const short *enemyOrigin)
    {
        return GetSpot(&farRangeTacticalSpotsList, origin, enemyOrigin,
                       &BotTacticalSpotsCache::FindFarRangeTacticalSpot);
    }
    inline const short *GetMiddleRangeTacticalSpot(const short *origin, const short *enemyOrigin)
    {
        return GetSpot(&middleRangeTacticalSpotsList, origin, enemyOrigin,
                       &BotTacticalSpotsCache::FindMiddleRangeTacticalSpot);
    }
    inline const short *GetCloseRangeTacticalSpot(const short *origin, const short *enemyOrigin)
    {
        return GetSpot(&closeRangeTacticalSpotsList, origin, enemyOrigin,
                       &BotTacticalSpotsCache::FindCloseRangeTacticalSpot);
    }
    inline const short *GetCoverSpot(const short *origin, const short *enemyOrigin)
    {
        return GetSpot(&coverSpotsTacticalSpotsList, origin, enemyOrigin, &BotTacticalSpotsCache::FindCoverSpot);
    }
};

#endif
