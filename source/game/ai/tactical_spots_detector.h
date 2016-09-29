#ifndef QFUSION_TACTICAL_SPOTS_DETECTOR_H
#define QFUSION_TACTICAL_SPOTS_DETECTOR_H

#include "ai_local.h"
#include "ai_aas_route_cache.h"
#include "static_vector.h"

class TacticalSpotsDetector
{
    // These types need forward declaration before methods where they are used.
public:
    class OriginParams
    {
        friend class TacticalSpotsDetector;

        const edict_t *originEntity;
        vec3_t origin;
        float searchRadius;
        AiAasRouteCache *routeCache;
        int originAreaNum;
    public:
        OriginParams(const edict_t *originEntity, float searchRadius, AiAasRouteCache *routeCache)
            : originEntity(originEntity), searchRadius(searchRadius), routeCache(routeCache)
        {
            VectorCopy(originEntity->s.origin, this->origin);
            const AiAasWorld *aasWorld = AiAasWorld::Instance();
            originAreaNum = aasWorld->IsLoaded() ? aasWorld->FindAreaNum(originEntity) : 0;
        }

        OriginParams(const vec3_t origin, float searchRadius, AiAasRouteCache *routeCache)
            : originEntity(nullptr), searchRadius(searchRadius), routeCache(routeCache)
        {
            VectorCopy(origin, this->origin);
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

    class AdvantageProblemParams
    {
        friend class TacticalSpotsDetector;

        const edict_t *keepVisibleEntity;
        vec3_t keepVisibleOrigin;
    public:
        AdvantageProblemParams(const edict_t *keepVisibleEntity)
            : keepVisibleEntity(keepVisibleEntity)
        {
            VectorCopy(keepVisibleEntity->s.origin, this->keepVisibleOrigin);
        }

        AdvantageProblemParams(const vec3_t keepVisibleOrigin)
            : keepVisibleEntity(nullptr)
        {
            VectorCopy(keepVisibleOrigin, this->keepVisibleOrigin);
        }
    };

    class CoverProblemParams
    {
        friend class TacticalSpotsDetector;

        const edict_t *attackerEntity;
        vec3_t attackerOrigin;
        float harmfulRayThickness;
    public:
        CoverProblemParams(const edict_t *attackerEntity, float harmfulRayThickness)
            : attackerEntity(attackerEntity), harmfulRayThickness(harmfulRayThickness)
        {
            VectorCopy(attackerEntity->s.origin, this->attackerOrigin);
        }

        CoverProblemParams(const vec3_t attackerOrigin, float harmfulRayThickness)
            : attackerEntity(nullptr), harmfulRayThickness(harmfulRayThickness)
        {
            VectorCopy(attackerOrigin, this->attackerOrigin);
        }
    };
private:
    float minHeightAdvantage;
    float weightFalloffDistanceRatio;
    float distanceInfluence;
    float travelTimeInfluence;
    float heightInfluence;
    int lowestWeightTravelTimeBounds;
    float ledgePenalty;
    float wallPenalty;
    float spotProximityThreshold;
    bool checkToAndBackReachability;

    struct AreaAndScore
    {
        int areaNum;
        float score;
        AreaAndScore(int areaNum, float score): areaNum(areaNum), score(score) {}
        bool operator<(const AreaAndScore &that) const { return score > that.score; }
    };

    // TODO: Should be allocated dynamically depending of input size
    typedef StaticVector<AreaAndScore, 384> CandidateAreas;
    typedef StaticVector<AreaAndScore, 256> ReachCheckedAreas;
    typedef StaticVector<AreaAndScore, 128> TraceCheckedAreas;

    int FindBBoxAreas(const OriginParams &originParams, int *areas);

    void SelectCandidateAreas(const OriginParams &originParams, const int *areas, int numAreas, CandidateAreas &result);
    void CheckAreasReachFromOrigin(const OriginParams &originParams, const CandidateAreas &candidateAreas,
                                   ReachCheckedAreas &result);
    void CheckAreasReachFromOriginAndBack(const OriginParams &originParams, const CandidateAreas &candidateAreas,
                                          ReachCheckedAreas &result);

    void FindReachCheckedAreas(const OriginParams &originParams, ReachCheckedAreas &result);

    int CopyResults(const TraceCheckedAreas &results, vec3_t *spots, int maxSpots);

    // Specific for positional advantage spots
    void CheckAreasVisibleOriginTrace(const AdvantageProblemParams &params, const ReachCheckedAreas &candidateAreas,
                                      TraceCheckedAreas &result);
    void SortByVisAndOtherFactors(const OriginParams &params, TraceCheckedAreas &areas);

    // Specific for cover spots
    void SelectAreasForCover(const OriginParams &originParams, const CoverProblemParams &problemParams, 
                             ReachCheckedAreas &candidateAreas, TraceCheckedAreas &result);

    bool LooksLikeACoverArea(const aas_area_t &area, const OriginParams &originParams, 
                             const CoverProblemParams &problemParams);

    inline float ComputeDistanceFactor(const vec3_t v1, const vec3_t v2, float searchRadius)
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

    inline float ComputeTravelTimeFactor(int travelTimeMillis)
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
    TacticalSpotsDetector()
    {
        weightFalloffDistanceRatio = 0.0f;
        lowestWeightTravelTimeBounds = 5000;
        distanceInfluence = 0.9f;
        travelTimeInfluence = 0.9f;
        heightInfluence = 0.9f;
        minHeightAdvantage = 0.0f;
        ledgePenalty = 0.33f;
        wallPenalty = 0.33f;
        spotProximityThreshold = 64.0f;
        checkToAndBackReachability = false;
    }

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

    inline void SetLedgePenalty(float howMuchWorse) { ledgePenalty = 1.0f / howMuchWorse; }

    inline void SetWallPenalty(float howMuchWorse) { wallPenalty = 1.0f / howMuchWorse; }

    inline void SetSpotProximityThreshold(float radius) { spotProximityThreshold = std::max(0.0f, radius); }

    int FindPositionalAdvantageSpots(const OriginParams &originParams, const AdvantageProblemParams &problemParams,
                                     vec3_t *spots, int maxSpots);

    int FindCoverSpots(const OriginParams &originParams, const CoverProblemParams &problemParams,
                       vec3_t *spots, int maxSpots);
};

#endif
