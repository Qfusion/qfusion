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
        float minHeightAdvantageOverOrigin;
        float originWeightFalloffDistanceRatio;
        float originDistanceInfluence;
        float travelTimeInfluence;
        float heightOverOriginInfluence;
        int lowestWeightTravelTimeBounds;
        float spotProximityThreshold;
        bool checkToAndBackReachability;
    public:
        CommonProblemParams()
            : minHeightAdvantageOverOrigin(0.0f),
              originWeightFalloffDistanceRatio(0.0f),
              originDistanceInfluence(0.9f),
              travelTimeInfluence(0.9f),
              heightOverOriginInfluence(0.9f),
              lowestWeightTravelTimeBounds(5000),
              spotProximityThreshold(64.0f),
              checkToAndBackReachability(false) {}

        inline void SetCheckToAndBackReachability(bool checkToAndBack)
        {
            this->checkToAndBackReachability = checkToAndBack;
        }

        inline void SetOriginWeightFalloffDistanceRatio(float ratio)
        {
            originWeightFalloffDistanceRatio = Clamp(ratio);
        }

        inline void SetMinHeightAdvantageOverOrigin(float minHeight)
        {
            minHeightAdvantageOverOrigin = minHeight;
        }

        inline void SetLowestWeightTravelTimeBounds(int millis)
        {
            lowestWeightTravelTimeBounds = std::min(1, millis);
        }

        inline void SetOriginDistanceInfluence(float influence) { originDistanceInfluence = Clamp(influence); }

        inline void SetTravelTimeInfluence(float influence) { travelTimeInfluence = Clamp(influence); }

        inline void SetHeightOverOriginInfluence(float influence) { heightOverOriginInfluence = Clamp(influence); }

        inline void SetSpotProximityThreshold(float radius) { spotProximityThreshold = std::max(0.0f, radius); }
    };

    class AdvantageProblemParams: public CommonProblemParams
    {
        friend class TacticalSpotsRegistry;

        const edict_t *keepVisibleEntity;
        vec3_t keepVisibleOrigin;
        float minSpotDistanceToEntity;
        float maxSpotDistanceToEntity;
        float entityDistanceInfluence;
        float entityWeightFalloffDistanceRatio;
        float minHeightAdvantageOverEntity;
        float heightOverEntityInfluence;
    public:
        AdvantageProblemParams(const edict_t *keepVisibleEntity_)
            : keepVisibleEntity(keepVisibleEntity_),
              minSpotDistanceToEntity(0),
              maxSpotDistanceToEntity(999999.0f),
              entityDistanceInfluence(0.5f),
              entityWeightFalloffDistanceRatio(0),
              minHeightAdvantageOverEntity(-999999.0f),
              heightOverEntityInfluence(0.5f)
        {
            VectorCopy(keepVisibleEntity->s.origin, this->keepVisibleOrigin);
        }

        AdvantageProblemParams(const vec3_t keepVisibleOrigin_)
            : keepVisibleEntity(nullptr),
              minSpotDistanceToEntity(0),
              maxSpotDistanceToEntity(999999.0f),
              minHeightAdvantageOverEntity(-999999.0f),
              heightOverEntityInfluence(0.5f)
        {
            VectorCopy(keepVisibleOrigin_, this->keepVisibleOrigin);
        }

        inline void SetMinSpotDistanceToEntity(float distance) { minSpotDistanceToEntity = distance; }
        inline void SetMaxSpotDistanceToEntity(float distance) { maxSpotDistanceToEntity = distance; }
        inline void SetEntityDistanceInfluence(float influence) { entityDistanceInfluence = influence; }
        inline void SetEntityWeightFalloffDistanceRatio(float ratio) { entityWeightFalloffDistanceRatio = Clamp(ratio); }
        inline void SetMinHeightAdvantageOverEntity(float height) { minHeightAdvantageOverEntity = height; }
        inline void SetHeightOverEntityInfluence(float influence) { heightOverEntityInfluence = Clamp(influence); }
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
    // For i-th spot element # i * numSpots + j contains a mutual visibility between spots i-th and j-th spot:
    // 0 if spot origins and bounds are completely invisible for each other
    // ...
    // 255 if spot origins and bounds are completely visible for each other
    unsigned char *spotVisibilityTable;
    // For i-th spot element # i * numSpots + j contains AAS travel time to j-th spot.
    // If the value is zero, j-th spot is not reachable from i-th one (we conform to AAS time encoding).
    // Non-zero value is a travel time in seconds^-2 (we conform to AAS time encoding).
    // Non-zero does not guarantee the spot is reachable for some picked bot
    // (these values are calculated using shared AI route cache and bots have individual one for blocked paths handling).
    int *spotTravelTimeTable;
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
    void SetupMutualSpotsReachability();
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

    uint16_t FindSpotsInRadius(const OriginParams &originParams, uint16_t *spotNums, uint16_t *insideSpotNum) const;

    void SelectCandidateSpots(const OriginParams &originParams, const CommonProblemParams &problemParams,
                              const uint16_t *spotNums, uint16_t numSpots, CandidateSpots &result) const;

    void SelectCandidateSpots(const OriginParams &originParams, const AdvantageProblemParams &problemParams,
                              const uint16_t *spotNums, uint16_t numSpots, CandidateSpots &result) const;

    void CheckSpotsReachFromOrigin(const OriginParams &originParams,
                                   const CommonProblemParams &problemParams,
                                   const CandidateSpots &candidateSpots,
                                   uint16_t insideSpotNum,
                                   ReachCheckedSpots &result) const;

    void CheckSpotsReachFromOriginAndBack(const OriginParams &originParams,
                                          const CommonProblemParams &problemParams,
                                          const CandidateSpots &candidateSpots,
                                          uint16_t insideSpotNum,
                                          ReachCheckedSpots &result) const;

    int CopyResults(const TraceCheckedSpots &results, const CommonProblemParams &problemParams,
                    vec3_t *spotOrigins, int maxSpots) const;

    // Specific for positional advantage spots
    void CheckSpotsVisibleOriginTrace(const OriginParams &originParams,
                                      const AdvantageProblemParams &params,
                                      const ReachCheckedSpots &candidateSpots,
                                      TraceCheckedSpots &result) const;

    void SortByVisAndOtherFactors(const OriginParams &originParams, const AdvantageProblemParams &problemParams,
                                  TraceCheckedSpots &spots) const;

    // Specific for cover spots
    void SelectSpotsForCover(const OriginParams &originParams,
                             const CoverProblemParams &problemParams,
                             const ReachCheckedSpots &candidateSpots,
                             TraceCheckedSpots &result) const;

    bool LooksLikeACoverSpot(uint16_t spotNum, const OriginParams &originParams,
                             const CoverProblemParams &problemParams) const;
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
