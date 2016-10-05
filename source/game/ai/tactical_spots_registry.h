#ifndef QFUSION_TACTICAL_SPOTS_DETECTOR_H
#define QFUSION_TACTICAL_SPOTS_DETECTOR_H

#include "ai_local.h"
#include "ai_aas_route_cache.h"
#include "static_vector.h"

class TacticalSpotsRegistry
{
    // These types need forward declaration before methods where they are used.
public:
    class OriginParams
    {
        friend class TacticalSpotsRegistry;

        const edict_t *originEntity;
        vec3_t origin;
        float searchRadius;
        AiAasRouteCache *routeCache;
        int originAreaNum;
    public:
        OriginParams(const edict_t *originEntity_, float searchRadius_, AiAasRouteCache *routeCache_)
            : originEntity(originEntity_), searchRadius(searchRadius_), routeCache(routeCache_)
        {
            VectorCopy(originEntity_->s.origin, this->origin);
            const AiAasWorld *aasWorld = AiAasWorld::Instance();
            originAreaNum = aasWorld->IsLoaded() ? aasWorld->FindAreaNum(originEntity) : 0;
        }

        OriginParams(const vec3_t origin_, float searchRadius_, AiAasRouteCache *routeCache_)
            : originEntity(nullptr), searchRadius(searchRadius_), routeCache(routeCache_)
        {
            VectorCopy(origin_, this->origin);
            const AiAasWorld *aasWorld = AiAasWorld::Instance();
            originAreaNum = aasWorld->IsLoaded() ? aasWorld->FindAreaNum(origin) : 0;
        }

        inline Vec3 MinBBoxBounds(float minHeightAdvantage = 0.0f) const
        {
            return Vec3(-searchRadius, -searchRadius, minHeightAdvantage) + origin;
        }

        inline Vec3 MaxBBoxBounds() const
        {
            return Vec3(+searchRadius, +searchRadius, +searchRadius) + origin;
        }
    };

    class CommonProblemParams
    {
        friend class TacticalSpotsRegistry;
    protected:
        float minHeightAdvantage;
        float weightFalloffDistanceRatio;
        float distanceInfluence;
        float travelTimeInfluence;
        float heightInfluence;
        int lowestWeightTravelTimeBounds;
        float spotProximityThreshold;
        bool checkToAndBackReachability;
    public:
        CommonProblemParams()
            : minHeightAdvantage(0.0f),
              weightFalloffDistanceRatio(0.0f),
              distanceInfluence(0.9f),
              travelTimeInfluence(0.9f),
              heightInfluence(0.9f),
              lowestWeightTravelTimeBounds(5000),
              spotProximityThreshold(64.0f),
              checkToAndBackReachability(false) {}

        inline void SetCheckToAndBackReachability(bool checkToAndBack)
        {
            this->checkToAndBackReachability = checkToAndBack;
        }

        inline void SetWeightFalloffDistanceRatio(float ratio)
        {
            weightFalloffDistanceRatio = Clamp(ratio);
        }

        inline void SetMinHeightAdvantage(float minHeight)
        {
            minHeightAdvantage = minHeight;
        }

        inline void SetLowestWeightTravelTimeBounds(int millis)
        {
            lowestWeightTravelTimeBounds = std::min(1, millis);
        }

        inline void SetDistanceInfluence(float influence) { distanceInfluence = Clamp(influence); }

        inline void SetTravelTimeInfluence(float influence) { travelTimeInfluence = Clamp(influence); }

        inline void SetHeightInfluence(float influence) { heightInfluence = Clamp(influence); }

        inline void SetSpotProximityThreshold(float radius) { spotProximityThreshold = std::max(0.0f, radius); }
    };

    class AdvantageProblemParams: public CommonProblemParams
    {
        friend class TacticalSpotsRegistry;

        const edict_t *keepVisibleEntity;
        vec3_t keepVisibleOrigin;
    public:
        AdvantageProblemParams(const edict_t *keepVisibleEntity_)
            : keepVisibleEntity(keepVisibleEntity_)
        {
            VectorCopy(keepVisibleEntity->s.origin, this->keepVisibleOrigin);
        }

        AdvantageProblemParams(const vec3_t keepVisibleOrigin_)
            : keepVisibleEntity(nullptr)
        {
            VectorCopy(keepVisibleOrigin_, this->keepVisibleOrigin);
        }
    };

    class CoverProblemParams: public CommonProblemParams
    {
        friend class TacticalSpotsRegistry;

        const edict_t *attackerEntity;
        vec3_t attackerOrigin;
        float harmfulRayThickness;
    public:
        CoverProblemParams(const edict_t *attackerEntity_, float harmfulRayThickness_)
            : attackerEntity(attackerEntity_), harmfulRayThickness(harmfulRayThickness_)
        {
            VectorCopy(attackerEntity_->s.origin, this->attackerOrigin);
        }

        CoverProblemParams(const vec3_t attackerOrigin_, float harmfulRayThickness_)
            : attackerEntity(nullptr), harmfulRayThickness(harmfulRayThickness_)
        {
            VectorCopy(attackerOrigin_, this->attackerOrigin);
        }
    };
private:
    static constexpr uint16_t MAX_SPOTS = 2048;
    static constexpr uint16_t MIN_GRID_CELL_SIDE = 512;
    static constexpr uint16_t MAX_GRID_DIMENSION = 32;

    struct TacticalSpot
    {
        vec3_t origin;
        vec3_t absMins;
        vec3_t absMaxs;
        int aasAreaNum;
    };

    // i-th element contains a spot for i=spotNum
    TacticalSpot *spots;
    // i-th element contains a visibility for i=spotNum
    unsigned char *spotVisibilityTable;
    // i-th element contains an offset of a grid cell spot nums list for i=cellNum
    unsigned *gridListOffsets;
    // Contains packed lists of grid cell spot nums.
    // Each list starts by number of spot nums followed by spot nums.
    uint16_t *gridSpotsLists;

    vec3_t worldMins;
    vec3_t worldMaxs;

    unsigned gridCellSize[3];
    unsigned gridNumCells[3];

    unsigned numSpots;

    struct SpotAndScore
    {
        float score;
        uint16_t spotNum;

        SpotAndScore(uint16_t spotNum_, float score_): score(score_), spotNum(spotNum_) {}
        bool operator<(const SpotAndScore &that) const { return score > that.score; }
    };

    typedef StaticVector<SpotAndScore, 384> CandidateSpots;
    typedef StaticVector<SpotAndScore, 256> ReachCheckedSpots;
    typedef StaticVector<SpotAndScore, 128> TraceCheckedSpots;

    static TacticalSpotsRegistry instance;

    TacticalSpotsRegistry()
        : spots(nullptr),
          spotVisibilityTable(nullptr),
          gridListOffsets(nullptr),
          gridSpotsLists(nullptr),
          numSpots(0) {}

    bool Load(const char *mapname);

    void SetupMutualSpotsVisibility();
    void SetupSpotsGrid();
    void SetupGridParams();

    inline unsigned PointGridCellNum(const vec3_t point)
    {
        vec3_t offset;
        VectorSubtract(point, worldMins, offset);

        unsigned i = (unsigned)(offset[0] / gridCellSize[0]);
        unsigned j = (unsigned)(offset[1] / gridCellSize[1]);
        unsigned k = (unsigned)(offset[2] / gridCellSize[2]);

        return i * (gridNumCells[1] * gridNumCells[2]) + j * gridNumCells[2] + k;
    }

    unsigned short FindSpotsInRadius(const OriginParams &originParams, uint16_t *spotNums) const;

    void SelectCandidateSpots(const OriginParams &originParams, const CommonProblemParams &problemParams,
                              const uint16_t *spotNums, uint16_t numSpots, CandidateSpots &result) const;

    void CheckSpotsReachFromOrigin(const OriginParams &originParams, const CommonProblemParams &problemParams,
                                   const CandidateSpots &candidateSpots, ReachCheckedSpots &result) const;

    void CheckSpotsReachFromOriginAndBack(const OriginParams &originParams, const CommonProblemParams &problemParams,
                                          const CandidateSpots &candidateSpots, ReachCheckedSpots &result) const;

    void FindReachCheckedSpots(const OriginParams &originParams, const CommonProblemParams &problemParams,
                               ReachCheckedSpots &result) const;

    int CopyResults(const TraceCheckedSpots &results, const CommonProblemParams &problemParams,
                    vec3_t *spotOrigins, int maxSpots) const;

    // Specific for positional advantage spots
    void CheckSpotsVisibleOriginTrace(const AdvantageProblemParams &params, const ReachCheckedSpots &candidateSpots,
                                      TraceCheckedSpots &result) const;
    void SortByVisAndOtherFactors(const OriginParams &originParams, const AdvantageProblemParams &problemParams,
                                  TraceCheckedSpots &spots) const;

    // Specific for cover spots
    void SelectSpotsForCover(const OriginParams &originParams, const CoverProblemParams &problemParams,
                             ReachCheckedSpots &candidateSpots, TraceCheckedSpots &result) const;

    bool LooksLikeACoverSpot(uint16_t spotNum, const OriginParams &originParams,
                             const CoverProblemParams &problemParams) const;

    inline static float ComputeDistanceFactor(const vec3_t v1, const vec3_t v2,
                                              float weightFalloffDistanceRatio, float searchRadius)
    {
        float weightFalloffRadius = weightFalloffDistanceRatio * searchRadius;

        float squareDistance = DistanceSquared(v1, v2);
        float distance = 1.0f;
        if (squareDistance >= 1.0f)
            distance = 1.0f / Q_RSqrt(squareDistance);

        if (distance < weightFalloffRadius)
            return distance / weightFalloffRadius;

        return 1.0f - ((distance - weightFalloffRadius) / (0.000001f + searchRadius - weightFalloffRadius));
    }

    inline static float ComputeTravelTimeFactor(int travelTimeMillis, float lowestWeightTravelTimeBounds)
    {
        float factor = 1.0f - BoundedFraction(travelTimeMillis, lowestWeightTravelTimeBounds);
        return 1.0f / Q_RSqrt(0.0001f + factor);
    }

    inline static float ApplyFactor(float value, float factor, float factorInfluence)
    {
        float keptPart = value * (1.0f - factorInfluence);
        float modifiedPart = value * factor * factorInfluence;
        return keptPart + modifiedPart;
    }
public:
    // TacticalSpotsRegistry should be init and shut down explicitly
    // (a game library is not unloaded when a map changes)
    static bool Init(const char *mapname);
    static void Shutdown();

    inline bool IsLoaded() const { return numSpots > 0; }

    static inline const TacticalSpotsRegistry *Instance()
    {
        return instance.numSpots ? &instance : nullptr;
    }

    int FindPositionalAdvantageSpots(const OriginParams &originParams, const AdvantageProblemParams &problemParams,
                                     vec3_t *spotOrigins, int maxSpots) const;

    int FindCoverSpots(const OriginParams &originParams, const CoverProblemParams &problemParams,
                       vec3_t *spotOrigins, int maxSpots) const;
};

#endif
