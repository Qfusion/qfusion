#include "tactical_spots_detector.h"
#include "bot.h"

int TacticalSpotsDetector::FindBBoxAreas(const OriginParams &originParams, int *areas)
{
    const AiAasWorld *aasWorld = AiAasWorld::Instance();
    if (!aasWorld->IsLoaded())
        return 0;

    return aasWorld->BBoxAreas(originParams.MinBBoxBounds(minHeightAdvantage), originParams.MaxBBoxBounds(), areas, 8192);
}

int TacticalSpotsDetector::CopyResults(const TraceCheckedAreas &results, vec3_t *spots, int maxSpots)
{
    if (maxSpots == 0 || results.empty())
        return 0;

    const aas_area_t *worldAreas = AiAasWorld::Instance()->Areas();

    // Its a common case so give it an optimized branch
    if (maxSpots == 1)
    {
        VectorCopy(worldAreas[results[0].areaNum].center, spots[0]);
        spots[0][2] = 16.0f + worldAreas[results[0].areaNum].mins[2];
        return 1;
    }

    bool isAreaExcluded[results.capacity()];
    memset(isAreaExcluded, 0, sizeof(bool) * results.capacity());

    int numSpots = 0;
    unsigned keptAreaIndex = 0;
    for (;;)
    {
        if (keptAreaIndex >= results.size())
            return numSpots;
        if (numSpots >= maxSpots)
            return numSpots;

        // Areas are sorted by score.
        // So first area not marked as excluded yet has higher priority and should be kept.

        const aas_area_t &keptArea = worldAreas[results[keptAreaIndex].areaNum];
        Vec3 keptAreaPoint(keptArea.center);
        keptAreaPoint.Z() = 16.0f + keptArea.mins[2];

        VectorCopy(keptAreaPoint.Data(), spots[numSpots]);
        ++numSpots;

        // Exclude all next (i.e. lower score) areas that are too close to kept area.

        unsigned testedAreaIndex = keptAreaIndex + 1;
        keptAreaIndex = 999999;
        for (; testedAreaIndex < results.size(); testedAreaIndex++)
        {
            // Skip already excluded areas
            if (isAreaExcluded[testedAreaIndex])
                continue;

            const aas_area_t &testedArea = worldAreas[results[testedAreaIndex].areaNum];
            Vec3 testedAreaPoint(testedArea.center);
            testedAreaPoint.Z() = 16.0f + testedArea.mins[2];

            if ((testedAreaPoint - keptAreaPoint).SquaredLength() < spotProximityThreshold * spotProximityThreshold)
                isAreaExcluded[testedAreaIndex] = true;
            else if (keptAreaIndex > testedAreaIndex)
                keptAreaIndex = testedAreaIndex;
        }
    }
}

void TacticalSpotsDetector::FindReachCheckedAreas(const OriginParams &originParams, ReachCheckedAreas &result)
{
    int boundsAreas[8192];
    int numAreasInBounds = FindBBoxAreas(originParams, boundsAreas);

    CandidateAreas candidateAreas;
    SelectCandidateAreas(originParams, boundsAreas, numAreasInBounds, candidateAreas);

    if (checkToAndBackReachability)
        CheckAreasReachFromOriginAndBack(originParams, candidateAreas, result);
    else
        CheckAreasReachFromOrigin(originParams, candidateAreas, result);
}

int TacticalSpotsDetector::FindPositionalAdvantageSpots(const OriginParams &originParams,
                                                        const AdvantageProblemParams &problemParams,
                                                        vec3_t *spots, int maxSpots)
{
    ReachCheckedAreas reachCheckedAreas;
    FindReachCheckedAreas(originParams, reachCheckedAreas);

    TraceCheckedAreas traceCheckedAreas;
    CheckAreasVisibleOriginTrace(problemParams, reachCheckedAreas, traceCheckedAreas);

    SortByVisAndOtherFactors(originParams, traceCheckedAreas);

    return CopyResults(traceCheckedAreas, spots, maxSpots);
}

void TacticalSpotsDetector::SelectCandidateAreas(const OriginParams &originParams, const int *areas, int numAreas,
                                                 CandidateAreas &result)
{
    const aas_area_t *worldAreas = AiAasWorld::Instance()->Areas();
    const aas_areasettings_t *worldAreaSettings = AiAasWorld::Instance()->AreaSettings();

    int badAreaContents = 0;
    badAreaContents |= AREACONTENTS_LAVA | AREACONTENTS_SLIME | AREACONTENTS_DONOTENTER;
    badAreaContents |= AREACONTENTS_JUMPPAD | AREACONTENTS_TELEPORTER | AREACONTENTS_MOVER | AREACONTENTS_WATER;

    for (int i = 0; i < numAreas && result.size() < result.capacity(); ++i)
    {
        const aas_areasettings_t &areaSettings = worldAreaSettings[areas[i]];
        if (!(areaSettings.areaflags & AREA_GROUNDED))
            continue;
        if (areaSettings.areaflags & (AREA_DISABLED|AREA_JUNK))
            continue;
        if (areaSettings.contents & badAreaContents)
            continue;
        if (!areaSettings.numreachableareas)
            continue;

        const aas_area_t &area = worldAreas[areas[i]];

        float height = area.mins[2] - originParams.origin[2];
        if (height < minHeightAdvantage)
            continue;

        float dx = area.maxs[0] - area.mins[0];
        float dy = area.maxs[1] - area.mins[1];
        // Skip small areas. If an area did not qualify as "junk" its not enough.
        if (dx < 24.0f || dy < 24.0f)
            continue;

        float score = 1.0f;

        // Increase score for higher areas
        float heightFactor = BoundedFraction(height - minHeightAdvantage, originParams.searchRadius);
        score = ApplyFactor(score, heightFactor, heightInfluence);
        // Increase scores for larger areas
        score *= 1.0f + 2.0f * BoundedFraction(dx - 24.0f, 96.0f);
        score *= 1.0f + 2.0f * BoundedFraction(dy - 24.0f, 96.0f);

        if ((areaSettings.areaflags & AREA_LEDGE))
            score *= ledgePenalty;
        if ((areaSettings.areaflags & AREA_WALL))
            score *= wallPenalty;

        result.push_back(AreaAndScore(areas[i], score));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsDetector::CheckAreasReachFromOrigin(const OriginParams &originParams,
                                                      const CandidateAreas &candidateAreas, ReachCheckedAreas &result)
{
    AiAasRouteCache *routeCache = AiAasRouteCache::Shared();
    const aas_area_t *areas = AiAasWorld::Instance()->Areas();

    int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    float searchRadius = originParams.searchRadius;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but this is intended to reduce performance drops (do not more than result.capacity() pathfinder calls).
    for (unsigned i = 0, end = std::min(candidateAreas.size(), result.capacity()); i < end; ++i)
    {
        int areaNum = candidateAreas[i].areaNum;
        int travelTime = routeCache->TravelTimeToGoalArea(originAreaNum, areaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!travelTime)
            continue;

        // AAS time is in seconds^-2
        float travelTimeFactor = 1.0f - ComputeTravelTimeFactor(travelTime * 10);
        float distanceFactor = ComputeDistanceFactor(areas[areaNum].center, origin, searchRadius);
        float newScore = candidateAreas[i].score;
        newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
        newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
        result.push_back(AreaAndScore(areaNum, newScore));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsDetector::CheckAreasReachFromOriginAndBack(const OriginParams &originParams,
                                                             const CandidateAreas &candidateAreas,
                                                             ReachCheckedAreas &result)
{
    AiAasRouteCache *routeCache = AiAasRouteCache::Shared();
    const aas_area_t *areas = AiAasWorld::Instance()->Areas();

    int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    float searchRadius = originParams.searchRadius;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but it is intended to reduce performance drops (do not more than 2 * result.capacity() pathfinder calls).
    for (unsigned i = 0, end = std::min(candidateAreas.size(), result.capacity()); i < end; ++i)
    {
        int areaNum = candidateAreas[i].areaNum;
        int toSpotTime = routeCache->TravelTimeToGoalArea(originAreaNum, areaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!toSpotTime)
            continue;
        int toEntityTime = routeCache->TravelTimeToGoalArea(areaNum, originAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!toEntityTime)
            continue;

        // AAS time is in seconds^-2
        int totalTravelTimeMillis = 10 * (toSpotTime + toEntityTime);
        float travelTimeFactor = ComputeTravelTimeFactor(totalTravelTimeMillis);
        float distanceFactor = ComputeDistanceFactor(areas[areaNum].center, origin, searchRadius);
        float newScore = candidateAreas[i].score;
        newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
        newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
        result.push_back(AreaAndScore(areaNum, newScore));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

const float PLAYER_VIEW_GROUND_OFFSET = -playerbox_stand_mins[2] + playerbox_stand_viewheight;

void TacticalSpotsDetector::CheckAreasVisibleOriginTrace(const AdvantageProblemParams &params,
                                                         const ReachCheckedAreas &candidateAreas,
                                                         TraceCheckedAreas &result)
{
    const aas_area_t *areas = AiAasWorld::Instance()->Areas();
    edict_t *passent = const_cast<edict_t*>(params.keepVisibleEntity);
    float *origin = const_cast<float *>(params.keepVisibleOrigin);
    trace_t trace;

    // Do not more than result.capacity() iterations
    // (do not do more than result.capacity() traces even if it may cause loose of feasible areas).
    for (unsigned i = 0, end = std::min(candidateAreas.size(), result.capacity()); i < end; ++i)
    {
        const aas_area_t &testedArea = areas[candidateAreas[i].areaNum];
        Vec3 areaPoint(testedArea.center);
        areaPoint.Z() = testedArea.mins[2] + PLAYER_VIEW_GROUND_OFFSET;

        G_Trace(&trace, areaPoint.Data(), nullptr, nullptr, origin, passent, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            result.push_back(candidateAreas[i]);
    }
}

void TacticalSpotsDetector::SortByVisAndOtherFactors(const OriginParams &params, TraceCheckedAreas &areas)
{
    const aas_area_t *worldAreas = AiAasWorld::Instance()->Areas();
    const aas_areasettings_t *worldAreaSettings = AiAasWorld::Instance()->AreaSettings();

    const float originZ = params.origin[2];
    const float searchRadius = params.searchRadius;

    // A matrix of trace results:
    // -1 means that trace has not been computed
    // 0 means that trace.fraction < 1.0f
    // 1 means that trace.fraction == 1.0f
    signed char traceResultCache[areas.capacity() * areas.capacity()];
    std::fill(traceResultCache, traceResultCache + areas.capacity() * areas.capacity(), -1);

    // Compute area points to avoid doing it in the trace loop.
    vec3_t areaPoints[areas.capacity()];
    for (unsigned i = 0; i < areas.size(); ++i)
    {
        const aas_area_t &area = worldAreas[areas[i].areaNum];
        VectorCopy(area.center, areaPoints[i]);
        areaPoints[i][2] = area.mins[2] + PLAYER_VIEW_GROUND_OFFSET;
    }

    trace_t trace;
    for (unsigned i = 0; i < areas.size(); ++i)
    {
        // Avoid fp <-> int conversions in a loop
        int numVisAreas = 0;
        for (unsigned j = 0; j < i; ++j)
        {
            int cachedTraceResult = traceResultCache[areas.capacity() * i + j];
            if (cachedTraceResult >= 0)
            {
                numVisAreas += cachedTraceResult;
                continue;
            }
            G_Trace(&trace, areaPoints[i], nullptr, nullptr, areaPoints[j], nullptr, MASK_AISOLID);
            // Omit fractional part by intention
            signed char visibility = (signed char)trace.fraction;
            traceResultCache[areas.capacity() * i + j] = visibility;
            traceResultCache[areas.capacity() * j + i] = visibility;
            numVisAreas += visibility;
        }

        // Skip a trace from an area to itself

        for (unsigned j = i + 1; j < areas.size(); ++j)
        {
            int cachedTraceResult = traceResultCache[areas.capacity() * i + j];
            if (cachedTraceResult >= 0)
            {
                numVisAreas += cachedTraceResult;
                continue;
            }
            G_Trace(&trace, areaPoints[i], nullptr, nullptr, areaPoints[j], nullptr, MASK_AISOLID);
            // Omit fractional part by intention
            signed char visibility = (signed char)trace.fraction;
            traceResultCache[areas.capacity() * i + j] = visibility;
            traceResultCache[areas.capacity() * j + i] = visibility;
            numVisAreas += visibility;
        }

        float visFactor = numVisAreas / (float)areas.size();
        visFactor = 1.0f / Q_RSqrt(visFactor);
        areas[i].score *= 0.1f + 0.9f * visFactor;

        // We should modify the final score by following factors.
        // These factors should be checked in earlier calls but mainly for early rejection of non-suitable areas.

        const int areaNum = areas[i].areaNum;

        float height = (worldAreas[areaNum].mins[2] - originZ - minHeightAdvantage);
        float heightFactor = BoundedFraction(height, searchRadius - minHeightAdvantage);
        areas[i].score = ApplyFactor(areas[i].score, heightFactor, heightInfluence);

        const aas_areasettings_t &areaSettings = worldAreaSettings[areaNum];
        if (areaSettings.areaflags & AREA_WALL)
            areas[i].score *= wallPenalty;
        if (areaSettings.areaflags & AREA_LEDGE)
            areas[i].score *= ledgePenalty;
    }

    // Sort results so best score areas are first
    std::sort(areas.begin(), areas.end());
}

int TacticalSpotsDetector::FindCoverSpots(const OriginParams &originParams, const CoverProblemParams &problemParams,
                                          vec3_t *spots, int maxSpots)
{
    ReachCheckedAreas reachCheckedAreas;
    FindReachCheckedAreas(originParams, reachCheckedAreas);

    TraceCheckedAreas coverAreas;
    SelectAreasForCover(problemParams, reachCheckedAreas, coverAreas);

    return CopyResults(coverAreas, spots, maxSpots);
}

void TacticalSpotsDetector::SelectAreasForCover(const CoverProblemParams &problemParams,
                                                ReachCheckedAreas &candidateAreas, TraceCheckedAreas &result)
{
    const aas_area_t *worldAreas = AiAasWorld::Instance()->Areas();

    // Do not more result.capacity() iterations
    for (unsigned i = 0, end = std::min(candidateAreas.size(), result.capacity()); i < end; ++i)
    {
        const aas_area_t &area = worldAreas[candidateAreas[i].areaNum];
        if (!LooksLikeACoverArea(area, problemParams))
            continue;

        // Prefer larger areas
        float dimensionFactor = 1.0f;
        dimensionFactor *= BoundedFraction(area.maxs[0] - area.mins[0], 64);
        dimensionFactor *= BoundedFraction(area.maxs[1] - area.mins[1], 64);
        result.push_back(AreaAndScore(candidateAreas[i].areaNum, candidateAreas[i].score * dimensionFactor));
    };

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

bool TacticalSpotsDetector::LooksLikeACoverArea(const aas_area_t &area, const CoverProblemParams &problemParams)
{
    edict_t *passent = const_cast<edict_t *>(problemParams.attackerEntity);
    float *attackerOrigin = const_cast<float *>(problemParams.attackerOrigin);
    float *areaCenter = const_cast<float *>(area.center);

    trace_t trace;
    G_Trace(&trace, attackerOrigin, nullptr, nullptr, areaCenter, passent, MASK_AISOLID);
    if (trace.fraction == 1.0f)
        return false;

    float harmfulRayThickness = problemParams.harmfulRayThickness;

    vec3_t bounds[2] =
    {
        { -harmfulRayThickness, -harmfulRayThickness, -harmfulRayThickness },
        { +harmfulRayThickness, +harmfulRayThickness, +harmfulRayThickness }
    };

    // Convert bounds from relative to absolute
    VectorAdd(bounds[0], area.center, bounds[0]);
    VectorAdd(bounds[1], area.center, bounds[1]);

    for (int i = 0; i < 8; ++i)
    {
        vec3_t traceEnd;
        traceEnd[0] = bounds[(i >> 2) & 1][0];
        traceEnd[1] = bounds[(i >> 1) & 1][1];
        traceEnd[2] = bounds[(i >> 0) & 1][2];
        G_Trace(&trace, attackerOrigin, nullptr, nullptr, traceEnd, passent, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            return false;
    }

    return true;
}
