#include "tactical_spots_registry.h"
#include "bot.h"

TacticalSpotsRegistry TacticalSpotsRegistry::instance;

#define	NAV_FILE_VERSION 10
#define NAV_FILE_EXTENSION "nav"
#define NAV_FILE_FOLDER "navigation"

bool TacticalSpotsRegistry::Init(const char *mapname)
{
    if (instance.IsLoaded())
    {
        G_Printf("TacticalSpotsRegistry::Init(): The instance has been already initialized\n");
        abort();
    }

    return instance.Load(mapname);
}

bool TacticalSpotsRegistry::Load(const char *mapname)
{
    char filename[MAX_QPATH];
    Q_snprintfz(filename, sizeof( filename ), "%s/%s.%s", NAV_FILE_FOLDER, mapname, NAV_FILE_EXTENSION);

    int filenum;
    int length = trap_FS_FOpenFile(filename, &filenum, FS_READ);
    if (length == -1)
    {
        G_Printf("TacticalSpotsRegistry::Load(): Cannot open file %s", filename);
        return false;
    }

    int version;
    trap_FS_Read(&version, sizeof(int), filenum);
    if (version != NAV_FILE_VERSION)
    {
        trap_FS_FCloseFile(filenum);
        G_Printf("TacticalSpotsRegistry::Load(): Invalid nav file version %i\n", version);
        return false;
    }

    int numNodes;
    trap_FS_Read(&numNodes, sizeof( int ), filenum);
    if (numNodes > MAX_SPOTS)
    {
        trap_FS_FCloseFile(filenum);
        G_Printf("TacticalSpotsRegistry::Load(): Too many nodes in file\n");
        return false;
    }

    struct nav_node_s
    {
        vec3_t origin;
        int flags;
        int area;
    } nodesBuffer[MAX_SPOTS];

    trap_FS_Read(nodesBuffer, sizeof(nav_node_s) * numNodes, filenum);
    trap_FS_FCloseFile(filenum);

    char *mem = (char *)G_LevelMalloc(sizeof(TacticalSpot) * numNodes + numNodes * numNodes);

    spots = (TacticalSpot *)mem;
    spotVisibilityTable = (unsigned char *)(mem + sizeof(TacticalSpot) * numNodes);

    const AiAasWorld *aasWorld = AiAasWorld::Instance();

    // TODO: Compute real spot bounds based on AAS info and tracing
    const vec3_t mins = { -24, -24, 0 };
    const vec3_t maxs = { +24, +24, 72 };

    for (int i = 0; i < numNodes; ++i)
    {
        if (int aasAreaNum = aasWorld->FindAreaNum(nodesBuffer[i].origin))
        {
            spots[numSpots].aasAreaNum = aasAreaNum;
            VectorCopy(nodesBuffer[i].origin, spots[numSpots].origin);
            VectorCopy(nodesBuffer[i].origin, spots[numSpots].absMins);
            VectorAdd(spots[numSpots].absMins, mins, spots[numSpots].absMins);
            VectorCopy(nodesBuffer[i].origin, spots[numSpots].absMaxs);
            VectorAdd(spots[numSpots].absMaxs, maxs, spots[numSpots].absMaxs);
            instance.numSpots++;
        }
        else
        {
            float x = nodesBuffer[i].origin[0], y = nodesBuffer[i].origin[1], z = nodesBuffer[i].origin[2];
            G_Printf(S_COLOR_YELLOW "Can't find AAS area num for spot @ %f %f %f\n", x, y, z);
        }
    }

    ComputeMutualSpotsVisibility();

    return numSpots > 0;
}

void TacticalSpotsRegistry::Shutdown()
{
    instance.numSpots = 0;
    if (instance.spots)
        G_LevelFree(instance.spots);
    instance.spots = nullptr;
    instance.spotVisibilityTable = nullptr;
}

void TacticalSpotsRegistry::ComputeMutualSpotsVisibility()
{
    trace_t trace;
    for (unsigned i = 0; i < numSpots; ++i)
    {
        TacticalSpot &currSpot = spots[i];
        vec3_t currSpotBounds[2];
        VectorCopy(currSpot.absMins, currSpotBounds[0]);
        VectorCopy(currSpot.absMaxs, currSpotBounds[1]);

        for (unsigned j = 0; j < numSpots / 2; ++j)
        {
            unsigned char visibility = 0;

            TacticalSpot &testedSpot = spots[j];
            vec3_t testedSpotBounds[2];
            VectorCopy(testedSpot.absMins, testedSpotBounds[0]);
            VectorCopy(testedSpot.absMaxs, testedSpotBounds[1]);

            G_Trace(&trace, currSpot.origin, nullptr, nullptr, testedSpot.origin, nullptr, MASK_AISOLID);
            bool areOriginsMutualVisible = (trace.fraction == 1.0f);

            for (unsigned n = 0; n < 8; ++n)
            {
                float from[] =
                {
                    currSpotBounds[(n >> 2) & 1][0],
                    currSpotBounds[(n >> 1) & 1][1],
                    currSpotBounds[(n >> 0) & 1][2]
                };
                for (unsigned m = 0; m < 8; ++m)
                {
                    float to[] =
                    {
                        testedSpotBounds[(m >> 2) & 1][0],
                        testedSpotBounds[(m >> 1) & 1][1],
                        testedSpotBounds[(m >> 0) & 1][2]
                    };
                    G_Trace(&trace, from, nullptr, nullptr, to, nullptr, MASK_AISOLID);
                    // If all 64 traces succeed, the visibility is a half of the maximal score
                    visibility += 2 * (unsigned char)trace.fraction;
                }
            }

            // Prevent marking of the most significant bit
            if (visibility == 128)
                visibility = 127;

            // Mutual origins visibility counts is a half of the maximal score.
            // Also, if the most significant bit of visibility bits is set, spot origins are mutually visible
            if (areOriginsMutualVisible)
                visibility |= 128;

            spotVisibilityTable[i * numSpots + j] = visibility;
            spotVisibilityTable[j * numSpots + i] = visibility;
            // Consider the spot visible to itself?
            spotVisibilityTable[i * numSpots + i] = 255;
        }
    }
}

uint16_t TacticalSpotsRegistry::FindBBoxSpots(const OriginParams &originParams, unsigned short *spotNums) const
{
    if (!numSpots) abort();

    // TODO: Use octree

    uint16_t numBBoxSpots = 0;
    for (uint16_t spotNum = 0; spotNum < MAX_SPOTS; ++spotNum)
    {
        const auto &spot = spots[spotNum];
        if (originParams.origin[0] < spot.absMins[0] || originParams.origin[0] > spot.absMaxs[0])
            continue;
        if (originParams.origin[1] < spot.absMins[1] || originParams.origin[1] > spot.absMaxs[1])
            continue;
        if (originParams.origin[2] < spot.absMins[2] || originParams.origin[2] > spot.absMaxs[2])
            continue;

        spotNums[numBBoxSpots++] = spotNum;
    }

    return numBBoxSpots;
}

int TacticalSpotsRegistry::CopyResults(const TraceCheckedSpots &results,
                                       const CommonProblemParams &problemParams,
                                       vec3_t *spotOrigins, int maxSpots) const
{
    if (maxSpots == 0 || results.empty())
        return 0;

    // Its a common case so give it an optimized branch
    if (maxSpots == 1)
    {
        VectorCopy(spots[results[0].spotNum].origin, spotOrigins[0]);
        return 1;
    }

    const float spotProximityThreshold = problemParams.spotProximityThreshold;

    bool isSpotExcluded[results.capacity()];
    memset(isSpotExcluded, 0, sizeof(bool) * results.capacity());

    int numSpots = 0;
    unsigned keptSpotIndex = 0;
    for (;;)
    {
        if (keptSpotIndex >= results.size())
            return numSpots;
        if (numSpots >= maxSpots)
            return numSpots;

        // Spots are sorted by score.
        // So first spot not marked as excluded yet has higher priority and should be kept.

        const TacticalSpot &keptSpot = spots[results[keptSpotIndex].spotNum];
        VectorCopy(keptSpot.origin, spotOrigins[numSpots]);
        ++numSpots;

        // Exclude all next (i.e. lower score) spots that are too close to the kept spot.

        unsigned testedSpotIndex = keptSpotIndex + 1;
        keptSpotIndex = 999999;
        for (; testedSpotIndex < results.size(); testedSpotIndex++)
        {
            // Skip already excluded areas
            if (isSpotExcluded[testedSpotIndex])
                continue;

            const TacticalSpot &testedSpot = spots[results[testedSpotIndex].spotNum];
            if (DistanceSquared(keptSpot.origin, testedSpot.origin) < spotProximityThreshold * spotProximityThreshold)
                isSpotExcluded[testedSpotIndex] = true;
            else if (keptSpotIndex > testedSpotIndex)
                keptSpotIndex = testedSpotIndex;
        }
    }
}

void TacticalSpotsRegistry::FindReachCheckedSpots(const OriginParams &originParams,
                                                  const CommonProblemParams &problemParams,
                                                  ReachCheckedSpots &result) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t numSpotsInBounds = FindBBoxSpots(originParams, boundsSpots);

    CandidateSpots candidateSpots;
    SelectCandidateSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots, result);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, result);
}

int TacticalSpotsRegistry::FindPositionalAdvantageSpots(const OriginParams &originParams,
                                                        const AdvantageProblemParams &problemParams,
                                                        vec3_t *spotOrigins, int maxSpots) const
{
    ReachCheckedSpots reachCheckedSpots;
    FindReachCheckedSpots(originParams, problemParams, reachCheckedSpots);

    TraceCheckedSpots traceCheckedAreas;
    CheckSpotsVisibleOriginTrace(problemParams, reachCheckedSpots, traceCheckedAreas);

    SortByVisAndOtherFactors(originParams, problemParams, traceCheckedAreas);

    return CopyResults(traceCheckedAreas, problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectCandidateSpots(const OriginParams &originParams,
                                                 const CommonProblemParams &problemParams,
                                                 const uint16_t *spotNums,
                                                 uint16_t numSpots, CandidateSpots &result) const
{
    const float minHeightAdvantage = problemParams.minHeightAdvantage;
    const float heightInfluence = problemParams.heightInfluence;

    for (unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i)
    {
        const TacticalSpot &spot = spots[spotNums[i]];

        float height = spot.absMins[2] - originParams.origin[2];
        if (height < minHeightAdvantage)
            continue;

        float score = 1.0f;
        // Increase score for higher areas
        float heightFactor = BoundedFraction(height - minHeightAdvantage, originParams.searchRadius);
        score = ApplyFactor(score, heightFactor, heightInfluence);

        result.push_back(SpotAndScore(spotNums[i], score));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsReachFromOrigin(const OriginParams &originParams,
                                                      const CommonProblemParams &problemParams,
                                                      const CandidateSpots &candidateSpots,
                                                      ReachCheckedSpots &result) const
{
    AiAasRouteCache *routeCache = originParams.routeCache;
    const int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    const float searchRadius = originParams.searchRadius;
    const float lowestWeightTravelTimeBounds = problemParams.lowestWeightTravelTimeBounds;
    const float weightFalloffDistanceRatio = problemParams.weightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.distanceInfluence;
    const float travelTimeInfluence = problemParams.travelTimeInfluence;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but this is intended to reduce performance drops (do not more than result.capacity() pathfinder calls).
    for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateSpots[i];
        const TacticalSpot &spot = spots[spotAndScore.spotNum];
        int travelTime = routeCache->TravelTimeToGoalArea(originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!travelTime)
            continue;

        // AAS time is in seconds^-2
        float travelTimeFactor = 1.0f - ComputeTravelTimeFactor(travelTime * 10, lowestWeightTravelTimeBounds);
        float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
        float newScore = spotAndScore.score;
        newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
        newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
        result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsReachFromOriginAndBack(const OriginParams &originParams,
                                                             const CommonProblemParams &problemParams,
                                                             const CandidateSpots &candidateSpots,
                                                             ReachCheckedSpots &result) const
{
    AiAasRouteCache *routeCache = originParams.routeCache;
    const int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    const float searchRadius = originParams.searchRadius;
    const float lowestWeightTravelTimeBounds = problemParams.lowestWeightTravelTimeBounds;
    const float weightFalloffDistanceRatio = problemParams.weightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.distanceInfluence;
    const float travelTimeInfluence = problemParams.travelTimeInfluence;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but it is intended to reduce performance drops (do not more than 2 * result.capacity() pathfinder calls).
    for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateSpots[i];
        const TacticalSpot &spot = spots[spotAndScore.spotNum];
        int toSpotTime = routeCache->TravelTimeToGoalArea(originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!toSpotTime)
            continue;
        int toEntityTime = routeCache->TravelTimeToGoalArea(spot.aasAreaNum, originAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
        if (!toEntityTime)
            continue;

        // AAS time is in seconds^-2
        int totalTravelTimeMillis = 10 * (toSpotTime + toEntityTime);
        float travelTimeFactor = ComputeTravelTimeFactor(totalTravelTimeMillis, lowestWeightTravelTimeBounds);
        float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
        float newScore = spotAndScore.score;
        newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
        newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
        result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsVisibleOriginTrace(const AdvantageProblemParams &problemParams,
                                                         const ReachCheckedSpots &candidateSpots,
                                                         TraceCheckedSpots &result) const
{
    edict_t *passent = const_cast<edict_t*>(problemParams.keepVisibleEntity);
    float *origin = const_cast<float *>(problemParams.keepVisibleOrigin);
    trace_t trace;

    // Do not more than result.capacity() iterations
    // (do not do more than result.capacity() traces even if it may cause loose of feasible areas).
    for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateSpots[i];
        G_Trace(&trace, spots[spotAndScore.spotNum].origin, nullptr, nullptr, origin, passent, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            result.push_back(spotAndScore);
    }
}

void TacticalSpotsRegistry::SortByVisAndOtherFactors(const OriginParams &originParams,
                                                     const AdvantageProblemParams &problemParams,
                                                     TraceCheckedSpots &result) const
{
    const float originZ = originParams.origin[2];
    const float searchRadius = originParams.searchRadius;
    const float minHeightAdvantage = problemParams.minHeightAdvantage;
    const float heightInfluence = problemParams.heightInfluence;

    const unsigned resultSpotsSize = result.size();

    for (unsigned i = 0; i < resultSpotsSize; ++i)
    {
        unsigned visibilitySum = 0;
        unsigned testedSpotNum = result[i].spotNum;
        // Get address of the visibility table row
        unsigned char *spotVisibilityForSpotNum = spotVisibilityTable + testedSpotNum * numSpots;

        for (unsigned j = 0; j < i; ++j)
            visibilitySum += spotVisibilityForSpotNum[j];

        // Skip i-th index

        for (unsigned j = i + 1; j < resultSpotsSize; ++j)
            visibilitySum += spotVisibilityForSpotNum[j];

        // The maximum possible visibility score for a pair of spots is 255
        float visFactor = visibilitySum / ((result.size() - 1) * 255);
        visFactor = 1.0f / Q_RSqrt(visFactor);
        result[i].score *= visFactor;

        const TacticalSpot &testedSpot = spots[testedSpotNum];
        float height = testedSpot.absMins[2] - originZ - minHeightAdvantage;
        float heightFactor = BoundedFraction(height, searchRadius - minHeightAdvantage);
        result[i].score = ApplyFactor(result[i].score, heightFactor, heightInfluence);
    }

    // Sort results so best score spots are first
    std::sort(result.begin(), result.end());
}

int TacticalSpotsRegistry::FindCoverSpots(const OriginParams &originParams,
                                          const CoverProblemParams &problemParams,
                                          vec3_t *spotOrigins, int maxSpots) const
{
    ReachCheckedSpots reachCheckedAreas;
    FindReachCheckedSpots(originParams, problemParams, reachCheckedAreas);

    TraceCheckedSpots coverAreas;
    SelectSpotsForCover(originParams, problemParams, reachCheckedAreas, coverAreas);

    return CopyResults(coverAreas, problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectSpotsForCover(const OriginParams &originParams,
                                                const CoverProblemParams &problemParams,
                                                ReachCheckedSpots &candidateAreas,
                                                TraceCheckedSpots &result) const
{
    // Do not do more than result.capacity() iterations
    for (unsigned i = 0, end = std::min(candidateAreas.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateAreas[i];
        if (!LooksLikeACoverSpot(spotAndScore.spotNum, originParams, problemParams))
            continue;

        result.push_back(spotAndScore);
    };
}

bool TacticalSpotsRegistry::LooksLikeACoverSpot(uint16_t spotNum, const OriginParams &originParams,
                                                const CoverProblemParams &problemParams) const
{
    const TacticalSpot &spot = spots[spotNum];

    edict_t *passent = const_cast<edict_t *>(problemParams.attackerEntity);
    float *attackerOrigin = const_cast<float *>(problemParams.attackerOrigin);
    float *spotOrigin = const_cast<float *>(spot.origin);
    const edict_t *doNotHitEntity = originParams.originEntity;

    trace_t trace;
    G_Trace(&trace, attackerOrigin, nullptr, nullptr, spotOrigin, passent, MASK_AISOLID);
    if (trace.fraction == 1.0f)
        return false;

    float harmfulRayThickness = problemParams.harmfulRayThickness;

    vec3_t bounds[2] =
    {
        { -harmfulRayThickness, -harmfulRayThickness, -harmfulRayThickness },
        { +harmfulRayThickness, +harmfulRayThickness, +harmfulRayThickness }
    };

    // Convert bounds from relative to absolute
    VectorAdd(bounds[0], spot.origin, bounds[0]);
    VectorAdd(bounds[1], spot.origin, bounds[1]);

    for (int i = 0; i < 8; ++i)
    {
        vec3_t traceEnd;
        traceEnd[0] = bounds[(i >> 2) & 1][0];
        traceEnd[1] = bounds[(i >> 1) & 1][1];
        traceEnd[2] = bounds[(i >> 0) & 1][2];
        G_Trace(&trace, attackerOrigin, nullptr, nullptr, traceEnd, passent, MASK_AISOLID);
        if (trace.fraction == 1.0f || game.edicts + trace.ent == doNotHitEntity)
            return false;
    }

    return true;
}
