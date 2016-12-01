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

// Cached tactical spot origins are rounded up to 4 units.
// We want tactical spot origins to match spot origins exactly.
// Otherwise an original tactical spot may pass reachability check
// and one restored from packed values may not,
// and it happens quite often (blame AAS for it).
inline void CopyVec3RoundedForPacking(const float *from, float *to)
{
    for (int i = 0; i < 3; ++i)
        to[i] = 4.0f * ((short)(((int)from[i]) / 4));
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

    spots = (TacticalSpot *)G_LevelMalloc(sizeof(TacticalSpot) * numNodes);

    const AiAasWorld *aasWorld = AiAasWorld::Instance();

    // Its good idea to make mins/maxs rounded too as was mentioned above
    constexpr int mins[] = { -24, -24, 0 };
    constexpr int maxs[] = { +24, +24, 72 };
    static_assert(mins[0] % 4 == 0 && mins[1] % 4 == 0 && mins[2] % 4 == 0, "");
    static_assert(maxs[0] % 4 == 0 && maxs[1] % 4 == 0 && maxs[2] % 4 == 0, "");

    for (int i = 0; i < numNodes; ++i)
    {
        const float *fileOrigin = nodesBuffer[i].origin;
        vec3_t roundedOrigin;
        CopyVec3RoundedForPacking(fileOrigin, roundedOrigin);
        if (int aasAreaNum = aasWorld->FindAreaNum(roundedOrigin))
        {
            TacticalSpot &spot = spots[numSpots];
            spot.aasAreaNum = aasAreaNum;
            VectorCopy(roundedOrigin, spot.origin);
            VectorCopy(roundedOrigin, spot.absMins);
            VectorCopy(roundedOrigin, spot.absMaxs);
            VectorAdd(spot.absMins, mins, spot.absMins);
            VectorAdd(spot.absMaxs, maxs, spot.absMaxs);
            numSpots++;
        }
        else
        {
            const char *format = S_COLOR_YELLOW "Can't find AAS area num for spot @ %f %f %f (rounded to 4 units)\n";
            G_Printf(format, fileOrigin[0], fileOrigin[1], fileOrigin[2]);
        }
    }

    SetupMutualSpotsVisibility();
    SetupMutualSpotsReachability();
    SetupSpotsGrid();

    return numSpots > 0;
}

void TacticalSpotsRegistry::Shutdown()
{
    instance.numSpots = 0;
    if (instance.spots)
    {
        G_LevelFree(instance.spots);
        instance.spots = nullptr;
    }
    if (instance.spotVisibilityTable)
    {
        G_LevelFree(instance.spotVisibilityTable);
        instance.spotVisibilityTable = nullptr;
    }
    if (instance.spotTravelTimeTable)
    {
        G_LevelFree(instance.spotTravelTimeTable);
        instance.spotTravelTimeTable = nullptr;
    }
    if (instance.gridListOffsets)
    {
        G_LevelFree(instance.gridListOffsets);
        instance.gridListOffsets = nullptr;
    }
    if (instance.gridSpotsLists)
    {
        G_LevelFree(instance.gridSpotsLists);
        instance.gridSpotsLists = nullptr;
    }
}

void TacticalSpotsRegistry::SetupMutualSpotsVisibility()
{
    spotVisibilityTable = (unsigned char *)G_LevelMalloc(numSpots * numSpots);

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

void TacticalSpotsRegistry::SetupMutualSpotsReachability()
{
    spotTravelTimeTable = (int *)G_LevelMalloc(sizeof(int) * numSpots * numSpots);
    const int flags = Bot::ALLOWED_TRAVEL_FLAGS;
    AiAasRouteCache *routeCache = AiAasRouteCache::Shared();
    for (unsigned i = 0; i < numSpots; ++i)
    {
        const int currAreaNum = spots[i].aasAreaNum;
        for (unsigned j = 0; j < i; ++j)
        {
            const int testedAreaNum = spots[j].aasAreaNum;
            spotTravelTimeTable[i * numSpots + j] = routeCache->TravelTimeToGoalArea(currAreaNum, testedAreaNum, flags);
        }
        // Set the lowest feasible travel time value for traveling from the curr spot to the curr spot itself.
        spotTravelTimeTable[i * numSpots + i] = 1;
        for (unsigned j = i + 1; j < numSpots; ++j)
        {
            const int testedAreaNum = spots[j].aasAreaNum;
            spotTravelTimeTable[i * numSpots + j] = routeCache->TravelTimeToGoalArea(currAreaNum, testedAreaNum, flags);
        }
    }
}

void TacticalSpotsRegistry::SetupSpotsGrid()
{
    SetupGridParams();

    unsigned totalNumCells = gridNumCells[0] * gridNumCells[1] * gridNumCells[2];
    gridListOffsets = (unsigned *)G_LevelMalloc(sizeof(unsigned) * totalNumCells);
    // For each cell at least 1 short value is used to store spots count.
    // Also totalNumCells short values are required to store spot nums
    // assuming that each spot belongs to a single cell.
    gridSpotsLists = (uint16_t *)G_LevelMalloc(sizeof(uint16_t) * (totalNumCells + numSpots));

    uint16_t *listPtr = gridSpotsLists;
    // For each cell of all possible cells
    for (unsigned cellNum = 0; cellNum < totalNumCells; ++cellNum)
    {
        // Store offset of the cell spots list
        gridListOffsets[cellNum] = (unsigned)(listPtr - gridSpotsLists);
        // Use a reference to cell spots list head that contains number of spots in the cell
        uint16_t &listSize = listPtr[0];
        listSize = 0;
        // Skip list head
        ++listPtr;
        // For each loaded spot
        for (uint16_t spotNum = 0; spotNum < numSpots; ++spotNum)
        {
            auto pointCellNum = PointGridCellNum(spots[spotNum].origin);
            // If the spot belongs to the cell
            if (pointCellNum == cellNum)
            {
                *listPtr = spotNum;
                ++listPtr;
                listSize++;
            }
        }
    }
}

void TacticalSpotsRegistry::SetupGridParams()
{
    // Get world bounds
    trap_CM_InlineModelBounds(trap_CM_InlineModel(0), worldMins, worldMaxs);

    vec3_t worldDims;
    VectorSubtract(worldMaxs, worldMins, worldDims);

    for (int i = 0; i < 3; ++i)
    {
        unsigned roundedDimension = (unsigned)worldDims[i];
        if (roundedDimension > MIN_GRID_CELL_SIDE * MAX_GRID_DIMENSION)
        {
            gridCellSize[i] = roundedDimension / MAX_GRID_DIMENSION;
            gridNumCells[i] = MAX_GRID_DIMENSION;
        }
        else
        {
            gridCellSize[i] = MIN_GRID_CELL_SIDE;
            gridNumCells[i] = (roundedDimension / MIN_GRID_CELL_SIDE) + 1;
        }
    }
}

uint16_t TacticalSpotsRegistry::FindSpotsInRadius(const OriginParams &originParams,
                                                  unsigned short *spotNums,
                                                  unsigned short *insideSpotNum) const
{
    if (!IsLoaded()) abort();

    vec3_t boxMins, boxMaxs;
    VectorCopy(originParams.origin, boxMins);
    VectorCopy(originParams.origin, boxMaxs);
    const float radius = originParams.searchRadius;
    vec3_t radiusBounds = { radius, radius, radius };
    VectorSubtract(boxMins, radiusBounds, boxMins);
    VectorAdd(boxMaxs, radiusBounds, boxMaxs);

    // Find loop bounds for each dimension
    unsigned minCellDimIndex[3];
    unsigned maxCellDimIndex[3];
    for (int i = 0; i < 3; ++i)
    {
        // Clamp box bounds by world bounds
        clamp(boxMins[i], worldMins[i], worldMaxs[i]);
        clamp(boxMaxs[i], worldMins[i], worldMaxs[i]);

        // Convert box bounds to relative
        boxMins[i] -= worldMins[i];
        boxMaxs[i] -= worldMins[i];

        minCellDimIndex[i] = (unsigned)(boxMins[i] / gridCellSize[i]);
        maxCellDimIndex[i] = (unsigned)(boxMaxs[i] / gridCellSize[i]);
    }

    // Avoid unsigned wrapping
    static_assert(MAX_SPOTS < std::numeric_limits<uint16_t>::max(), "");
    *insideSpotNum = MAX_SPOTS + 1;

    // Copy to locals for faster access
    const Vec3 searchOrigin(originParams.origin);
    const float squareRadius = originParams.searchRadius * originParams.searchRadius;
    uint16_t numSpotsInRadius = 0;
    // For each index for X dimension in the query bounding box
    for (unsigned i = minCellDimIndex[0]; i <= maxCellDimIndex[0]; ++i)
    {
        unsigned indexIOffset = i * (gridNumCells[1] * gridNumCells[2]);
        // For each index for Y dimension in the query bounding box
        for (unsigned j = minCellDimIndex[1]; j <= maxCellDimIndex[1]; ++j)
        {
            unsigned indexJOffset = j * gridNumCells[2];
            // For each index for Z dimension in the query bounding box
            for (unsigned k = minCellDimIndex[2]; k <= maxCellDimIndex[2]; ++k)
            {
                // The cell is at this offset from the beginning of a linear cells array
                unsigned cellIndex = indexIOffset + indexJOffset + k;
                // Get the offset of the list of spot nums for the cell
                unsigned gridListOffset = gridListOffsets[cellIndex];
                uint16_t *spotsList = gridSpotsLists + gridListOffset;
                // List head contains the count of spots (spot numbers)
                uint16_t numGridSpots = spotsList[0];
                // Skip list head
                spotsList++;
                // For each spot number fetch a spot and test against the problem params
                for (uint16_t spotNumIndex = 0; spotNumIndex < numGridSpots; ++spotNumIndex)
                {
                    uint16_t spotNum = spotsList[spotNumIndex];
                    const TacticalSpot &spot = spots[spotNum];
                    if (DistanceSquared(spot.origin, searchOrigin.Data()) < squareRadius)
                    {
                        spotNums[numSpotsInRadius++] = spotNum;
                        // Test whether search origin is inside the spot
                        if (searchOrigin.X() < spot.absMins[0] || searchOrigin.X() > spot.absMaxs[0])
                            continue;
                        if (searchOrigin.Y() < spot.absMins[1] || searchOrigin.Y() > spot.absMaxs[1])
                            continue;
                        if (searchOrigin.Z() < spot.absMins[2] || searchOrigin.Z() > spot.absMaxs[2])
                            continue;
                        // Spots should not overlap. But if spots overlap, last matching spot will be returned
                        *insideSpotNum = spotNum;
                    }
                }
            }
        }
    }

    return numSpotsInRadius;
}

int TacticalSpotsRegistry::CopyResults(const SpotAndScore *spotsBegin,
                                       const SpotAndScore *spotsEnd,
                                       const CommonProblemParams &problemParams,
                                       vec3_t *spotOrigins, int maxSpots) const
{
    const unsigned resultsSize = (unsigned)(spotsEnd - spotsBegin);
    if (maxSpots == 0 || resultsSize == 0)
        return 0;

    // Its a common case so give it an optimized branch
    if (maxSpots == 1)
    {
        VectorCopy(spots[spotsBegin->spotNum].origin, spotOrigins[0]);
        return 1;
    }

    const float spotProximityThreshold = problemParams.spotProximityThreshold;

    bool isSpotExcluded[CandidateSpots::capacity()];
    memset(isSpotExcluded, 0, sizeof(bool) * CandidateSpots::capacity());

    int numSpots = 0;
    unsigned keptSpotIndex = 0;
    for (;;)
    {
        if (keptSpotIndex >= resultsSize)
            return numSpots;
        if (numSpots >= maxSpots)
            return numSpots;

        // Spots are sorted by score.
        // So first spot not marked as excluded yet has higher priority and should be kept.

        const TacticalSpot &keptSpot = spots[spotsBegin[keptSpotIndex].spotNum];
        VectorCopy(keptSpot.origin, spotOrigins[numSpots]);
        ++numSpots;

        // Exclude all next (i.e. lower score) spots that are too close to the kept spot.

        unsigned testedSpotIndex = keptSpotIndex + 1;
        keptSpotIndex = 999999;
        for (; testedSpotIndex < resultsSize; testedSpotIndex++)
        {
            // Skip already excluded areas
            if (isSpotExcluded[testedSpotIndex])
                continue;

            const TacticalSpot &testedSpot = spots[spotsBegin[testedSpotIndex].spotNum];
            if (DistanceSquared(keptSpot.origin, testedSpot.origin) < spotProximityThreshold * spotProximityThreshold)
                isSpotExcluded[testedSpotIndex] = true;
            else if (keptSpotIndex > testedSpotIndex)
                keptSpotIndex = testedSpotIndex;
        }
    }
}

inline float ComputeDistanceFactor(float distance, float weightFalloffDistanceRatio, float searchRadius)
{
    float weightFalloffRadius = weightFalloffDistanceRatio * searchRadius;
    if (distance < weightFalloffRadius)
        return distance / weightFalloffRadius;

    return 1.0f - ((distance - weightFalloffRadius) / (0.000001f + searchRadius - weightFalloffRadius));
}

inline float ComputeDistanceFactor(const vec3_t v1, const vec3_t v2, float weightFalloffDistanceRatio, float searchRadius)
{
    float squareDistance = DistanceSquared(v1, v2);
    float distance = 1.0f;
    if (squareDistance >= 1.0f)
        distance = 1.0f / Q_RSqrt(squareDistance);

    return ComputeDistanceFactor(distance, weightFalloffDistanceRatio, searchRadius);
}

// Units of travelTime and maxFeasibleTravelTime must match!
inline float ComputeTravelTimeFactor(int travelTime, float maxFeasibleTravelTime)
{
    float factor = 1.0f - BoundedFraction(travelTime, maxFeasibleTravelTime);
    return 1.0f / Q_RSqrt(0.0001f + factor);
}

inline float ApplyFactor(float value, float factor, float factorInfluence)
{
    float keptPart = value * (1.0f - factorInfluence);
    float modifiedPart = value * factor * factorInfluence;
    return keptPart + modifiedPart;
}

int TacticalSpotsRegistry::FindPositionalAdvantageSpots(const OriginParams &originParams,
                                                        const AdvantageProblemParams &problemParams,
                                                        vec3_t *spotOrigins, int maxSpots) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t insideSpotNum;
    uint16_t numSpotsInBounds = FindSpotsInRadius(originParams, boundsSpots, &insideSpotNum);

    CandidateSpots candidateSpots;
    SelectCandidateSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    ReachCheckedSpots reachCheckedSpots;
    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots,  insideSpotNum, reachCheckedSpots);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots);

    TraceCheckedSpots traceCheckedSpots;
    CheckSpotsVisibleOriginTrace(originParams, problemParams, reachCheckedSpots, traceCheckedSpots);

    SortByVisAndOtherFactors(originParams, problemParams, traceCheckedSpots);

    return CopyResults(traceCheckedSpots.begin(), traceCheckedSpots.end(), problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectCandidateSpots(const OriginParams &originParams,
                                                 const CommonProblemParams &problemParams,
                                                 const uint16_t *spotNums,
                                                 uint16_t numSpots, CandidateSpots &result) const
{
    const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
    const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
    const float searchRadius = originParams.searchRadius;
    const float originZ = originParams.origin[2];
    // Copy to stack for faster access
    Vec3 origin(originParams.origin);

    for (unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i)
    {
        const TacticalSpot &spot = spots[spotNums[i]];

        float heightOverOrigin = spot.absMins[2] - originZ;
        if (heightOverOrigin < minHeightAdvantageOverOrigin)
            continue;

        float squareDistanceToOrigin = DistanceSquared(origin.Data(), spot.origin);
        if (squareDistanceToOrigin > searchRadius * searchRadius)
            continue;

        float score = 1.0f;
        float factor = BoundedFraction(heightOverOrigin - minHeightAdvantageOverOrigin, searchRadius);
        score = ApplyFactor(score, factor, heightOverOriginInfluence);

        result.push_back(SpotAndScore(spotNums[i], score));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::SelectCandidateSpots(const OriginParams &originParams,
                                                 const AdvantageProblemParams &problemParams,
                                                 const uint16_t *spotNums,
                                                 uint16_t numSpots, CandidateSpots &result) const
{
    const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
    const float minHeightAdvantageOverEntity = problemParams.minHeightAdvantageOverEntity;
    const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
    const float heightOverEntityInfluence = problemParams.heightOverEntityInfluence;
    const float minSquareDistanceToEntity = problemParams.minSpotDistanceToEntity * problemParams.minSpotDistanceToEntity;
    const float maxSquareDistanceToEntity = problemParams.maxSpotDistanceToEntity * problemParams.maxSpotDistanceToEntity;
    const float searchRadius = originParams.searchRadius;
    const float originZ = originParams.origin[2];
    const float entityZ = problemParams.keepVisibleOrigin[2];
    // Copy to stack for faster access
    Vec3 origin(originParams.origin);
    Vec3 entityOrigin(problemParams.keepVisibleOrigin);

    for (unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i)
    {
        const TacticalSpot &spot = spots[spotNums[i]];

        float heightOverOrigin = spot.absMins[2] - originZ;
        if (heightOverOrigin < minHeightAdvantageOverOrigin)
            continue;

        float heightOverEntity = spot.absMins[2] - entityZ;
        if (heightOverEntity < minHeightAdvantageOverEntity)
            continue;

        float squareDistanceToOrigin = DistanceSquared(origin.Data(), spot.origin);
        if (squareDistanceToOrigin > searchRadius * searchRadius)
            continue;

        float squareDistanceToEntity = DistanceSquared(entityOrigin.Data(), spot.origin);
        if (squareDistanceToEntity < minSquareDistanceToEntity)
            continue;
        if (squareDistanceToEntity > maxSquareDistanceToEntity)
            continue;

        float score = 1.0f;
        float factor;
        factor = BoundedFraction(heightOverOrigin - minHeightAdvantageOverOrigin, searchRadius);
        score = ApplyFactor(score, factor, heightOverOriginInfluence);
        factor = BoundedFraction(heightOverEntity - minHeightAdvantageOverEntity, searchRadius);
        score = ApplyFactor(score, factor, heightOverEntityInfluence);

        result.push_back(SpotAndScore(spotNums[i], score));
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsReachFromOrigin(const OriginParams &originParams,
                                                      const CommonProblemParams &problemParams,
                                                      const CandidateSpots &candidateSpots,
                                                      uint16_t insideSpotNum,
                                                      ReachCheckedSpots &result) const
{
    AiAasRouteCache *routeCache = originParams.routeCache;
    const int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    const float searchRadius = originParams.searchRadius;
    // AAS uses travel time in centiseconds
    const int maxFeasibleTravelTimeCentis = problemParams.maxFeasibleTravelTimeMillis / 10;
    const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.originDistanceInfluence;
    const float travelTimeInfluence = problemParams.travelTimeInfluence;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but this is intended to reduce performance drops (do not more than result.capacity() pathfinder calls).
    if (insideSpotNum < MAX_SPOTS)
    {
        const int *travelTimeTable = this->spotTravelTimeTable;
        const auto tableRowOffset = insideSpotNum * this->numSpots;
        for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
        {
            const SpotAndScore &spotAndScore = candidateSpots[i];
            const TacticalSpot &spot = spots[spotAndScore.spotNum];
            // If zero, the spotNum spot is not reachable from insideSpotNum
            int tableTravelTime = travelTimeTable[tableRowOffset + spotAndScore.spotNum];
            if (!tableTravelTime || tableTravelTime > maxFeasibleTravelTimeCentis)
                continue;

            // Get an actual travel time (non-zero table value does not guarantee reachability)
            int travelTime = routeCache->TravelTimeToGoalArea(originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
            if (!travelTime || travelTime > maxFeasibleTravelTimeCentis)
                continue;

            float travelTimeFactor = 1.0f - ComputeTravelTimeFactor(travelTime, maxFeasibleTravelTimeCentis);
            float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
            float newScore = spotAndScore.score;
            newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
            newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
            result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
        }
    }
    else
    {
        for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
        {
            const SpotAndScore &spotAndScore = candidateSpots[i];
            const TacticalSpot &spot = spots[spotAndScore.spotNum];
            int travelTime = routeCache->TravelTimeToGoalArea(originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
            if (!travelTime || travelTime > maxFeasibleTravelTimeCentis)
                continue;

            float travelTimeFactor = 1.0f - ComputeTravelTimeFactor(travelTime, maxFeasibleTravelTimeCentis);
            float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
            float newScore = spotAndScore.score;
            newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
            newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
            result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
        }
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsReachFromOriginAndBack(const OriginParams &originParams,
                                                             const CommonProblemParams &problemParams,
                                                             const CandidateSpots &candidateSpots,
                                                             uint16_t insideSpotNum,
                                                             ReachCheckedSpots &result) const
{
    AiAasRouteCache *routeCache = originParams.routeCache;
    const int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    const float searchRadius = originParams.searchRadius;
    // AAS uses time in centiseconds
    const int maxFeasibleTravelTimeCentis = problemParams.maxFeasibleTravelTimeMillis / 10;
    const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.originDistanceInfluence;
    const float travelTimeInfluence = problemParams.travelTimeInfluence;

    // Do not more than result.capacity() iterations.
    // Some feasible areas in candidateAreas tai that pass test may be skipped,
    // but it is intended to reduce performance drops (do not more than 2 * result.capacity() pathfinder calls).
    if (insideSpotNum < MAX_SPOTS)
    {
        const int *travelTimeTable = this->spotTravelTimeTable;
        const auto numSpots = this->numSpots;
        for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
        {
            const SpotAndScore &spotAndScore = candidateSpots[i];
            const TacticalSpot &spot = spots[spotAndScore.spotNum];

            // If the table element i * numSpots + j is zero, j-th spot is not reachable from i-th one.
            int tableToTravelTime = travelTimeTable[insideSpotNum * numSpots + spotAndScore.spotNum];
            if (!tableToTravelTime)
                continue;
            int tableBackTravelTime = travelTimeTable[spotAndScore.spotNum * numSpots + insideSpotNum];
            if (!tableBackTravelTime)
                continue;
            if (tableToTravelTime + tableBackTravelTime > maxFeasibleTravelTimeCentis)
                continue;

            // Get an actual travel time (non-zero table values do not guarantee reachability)
            int toTravelTime = routeCache->TravelTimeToGoalArea(originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
            // If `to` travel time is apriori greater than maximum allowed one (and thus the sum would be), reject early.
            if (!toTravelTime || toTravelTime > maxFeasibleTravelTimeCentis)
                continue;
            int backTimeTravelTime = routeCache->TravelTimeToGoalArea(spot.aasAreaNum, originAreaNum, Bot::ALLOWED_TRAVEL_FLAGS);
            if (!backTimeTravelTime || toTravelTime + backTimeTravelTime > maxFeasibleTravelTimeCentis)
                continue;

            int totalTravelTimeCentis = toTravelTime + backTimeTravelTime;
            float travelTimeFactor = ComputeTravelTimeFactor(totalTravelTimeCentis, maxFeasibleTravelTimeCentis);
            float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
            float newScore = spotAndScore.score;
            newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
            newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
            result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
        }
    }
    else
    {
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

            int totalTravelTimeCentis = 10 * (toSpotTime + toEntityTime);
            float travelTimeFactor = ComputeTravelTimeFactor(totalTravelTimeCentis, maxFeasibleTravelTimeCentis);
            float distanceFactor = ComputeDistanceFactor(spot.origin, origin, weightFalloffDistanceRatio, searchRadius);
            float newScore = spotAndScore.score;
            newScore = ApplyFactor(newScore, distanceFactor, distanceInfluence);
            newScore = ApplyFactor(newScore, travelTimeFactor, travelTimeInfluence);
            result.push_back(SpotAndScore(spotAndScore.spotNum, newScore));
        }
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

void TacticalSpotsRegistry::CheckSpotsVisibleOriginTrace(const OriginParams &originParams,
                                                         const AdvantageProblemParams &problemParams,
                                                         const ReachCheckedSpots &candidateSpots,
                                                         TraceCheckedSpots &result) const
{
    edict_t *passent = const_cast<edict_t*>(originParams.originEntity);
    edict_t *keepVisibleEntity = const_cast<edict_t *>(problemParams.keepVisibleEntity);
    Vec3 entityOrigin(problemParams.keepVisibleOrigin);
    // If not only origin but an entity too is supplied
    if (keepVisibleEntity)
    {
        // Its a good idea to add some offset from the ground
        entityOrigin.Z() += 0.66f * keepVisibleEntity->r.maxs[2];
    }
    // Copy to locals for faster access
    const edict_t *gameEdicts = game.edicts;

    trace_t trace;
    // Do not more than result.capacity() iterations
    // (do not do more than result.capacity() traces even if it may cause loose of feasible areas).
    for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateSpots[i];
        G_Trace(&trace, spots[spotAndScore.spotNum].origin, nullptr, nullptr, entityOrigin.Data(), passent, MASK_AISOLID);
        if (trace.fraction == 1.0f || gameEdicts + trace.ent == keepVisibleEntity)
            result.push_back(spotAndScore);
    }
}

void TacticalSpotsRegistry::SortByVisAndOtherFactors(const OriginParams &originParams,
                                                     const AdvantageProblemParams &problemParams,
                                                     TraceCheckedSpots &result) const
{
    const Vec3 origin(originParams.origin);
    const Vec3 entityOrigin(problemParams.keepVisibleOrigin);
    const float originZ = originParams.origin[2];
    const float entityZ = problemParams.keepVisibleOrigin[2];
    const float searchRadius = originParams.searchRadius;
    const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
    const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
    const float minHeightAdvantageOverEntity = problemParams.minHeightAdvantageOverEntity;
    const float heightOverEntityInfluence = problemParams.heightOverEntityInfluence;
    const float originWeightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
    const float originDistanceInfluence = problemParams.originDistanceInfluence;
    const float entityWeightFalloffDistanceRatio = problemParams.entityWeightFalloffDistanceRatio;
    const float entityDistanceInfluence = problemParams.entityDistanceInfluence;
    const float minSpotDistanceToEntity = problemParams.minSpotDistanceToEntity;
    const float entityDistanceRange = problemParams.maxSpotDistanceToEntity - problemParams.minSpotDistanceToEntity;

    const unsigned resultSpotsSize = result.size();
    if (resultSpotsSize <= 1)
        return;

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

        const TacticalSpot &testedSpot = spots[testedSpotNum];
        float score = result[i].score;

        // The maximum possible visibility score for a pair of spots is 255
        float visFactor = visibilitySum / ((result.size() - 1) * 255);
        visFactor = 1.0f / Q_RSqrt(visFactor);
        score *= visFactor;

        float heightOverOrigin = testedSpot.absMins[2] - originZ - minHeightAdvantageOverOrigin;
        float heightOverOriginFactor = BoundedFraction(heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin);
        score = ApplyFactor(score, heightOverOriginFactor, heightOverOriginInfluence);

        float heightOverEntity = testedSpot.absMins[2] - entityZ - minHeightAdvantageOverEntity;
        float heightOverEntityFactor = BoundedFraction(heightOverEntity, searchRadius - minHeightAdvantageOverEntity);
        score = ApplyFactor(score, heightOverEntityFactor, heightOverEntityInfluence);

        float originDistance = 1.0f / Q_RSqrt(0.001f + DistanceSquared(testedSpot.origin, origin.Data()));
        float originDistanceFactor = ComputeDistanceFactor(originDistance, originWeightFalloffDistanceRatio, searchRadius);
        score = ApplyFactor(score, originDistanceFactor, originDistanceInfluence);

        float entityDistance = 1.0f / Q_RSqrt(0.001f + DistanceSquared(testedSpot.origin, entityOrigin.Data()));
        entityDistance -= minSpotDistanceToEntity;
        float entityDistanceFactor = ComputeDistanceFactor(entityDistance,
                                                           entityWeightFalloffDistanceRatio,
                                                           entityDistanceRange);
        score = ApplyFactor(score, entityDistanceFactor, entityDistanceInfluence);

        result[i].score = score;
    }

    // Sort results so best score spots are first
    std::stable_sort(result.begin(), result.end());
}

int TacticalSpotsRegistry::FindCoverSpots(const OriginParams &originParams,
                                          const CoverProblemParams &problemParams,
                                          vec3_t *spotOrigins, int maxSpots) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t insideSpotNum;
    uint16_t numSpotsInBounds = FindSpotsInRadius(originParams, boundsSpots, &insideSpotNum);

    CandidateSpots candidateSpots;
    SelectCandidateSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    ReachCheckedSpots reachCheckedSpots;
    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots);

    TraceCheckedSpots coverSpots;
    SelectSpotsForCover(originParams, problemParams, reachCheckedSpots, coverSpots);

    return CopyResults(coverSpots.begin(), coverSpots.end(), problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectSpotsForCover(const OriginParams &originParams,
                                                const CoverProblemParams &problemParams,
                                                const ReachCheckedSpots &candidateSpots,
                                                TraceCheckedSpots &result) const
{
    // Do not do more than result.capacity() iterations
    for (unsigned i = 0, end = std::min(candidateSpots.size(), result.capacity()); i < end; ++i)
    {
        const SpotAndScore &spotAndScore = candidateSpots[i];
        if (LooksLikeACoverSpot(spotAndScore.spotNum, originParams, problemParams))
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

int TacticalSpotsRegistry::FindEvadeDangerSpots(const OriginParams &originParams,
                                                const DodgeDangerProblemParams &problemParams,
                                                vec3_t *spotOrigins, int maxSpots) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t insideSpotNum;
    uint16_t numSpotsInBounds = FindSpotsInRadius(originParams, boundsSpots, &insideSpotNum);

    CandidateSpots candidateSpots;
    SelectPotentialDodgeSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    ReachCheckedSpots reachCheckedSpots;
    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots);

    return CopyResults(reachCheckedSpots.begin(), reachCheckedSpots.end(), problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectPotentialDodgeSpots(const OriginParams &originParams,
                                                      const DodgeDangerProblemParams &problemParams,
                                                      const uint16_t *spotNums,
                                                      uint16_t numSpots,
                                                      CandidateSpots &result) const
{
    bool mightNegateDodgeDir = false;
    Vec3 dodgeDir = MakeDodgeDangerDir(originParams, problemParams, &mightNegateDodgeDir);

    const float searchRadius = originParams.searchRadius;
    const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
    const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
    const float originZ = originParams.origin[2];
    // Copy to stack for faster access
    Vec3 origin(originParams.origin);

    if (mightNegateDodgeDir)
    {
        for (unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i)
        {
            const TacticalSpot &spot = spots[spotNums[i]];

            float heightOverOrigin = spot.absMins[2] - originZ;
            if (heightOverOrigin < minHeightAdvantageOverOrigin)
                continue;

            Vec3 toSpotDir(spot.origin);
            toSpotDir -= origin;
            float squaredDistanceToSpot = toSpotDir.SquaredLength();
            if (squaredDistanceToSpot < 1)
                continue;

            toSpotDir *= Q_RSqrt(squaredDistanceToSpot);
            float dot = toSpotDir.Dot(dodgeDir);
            if (dot < 0.2f)
                continue;

            heightOverOrigin -= minHeightAdvantageOverOrigin;
            float heightOverOriginFactor = BoundedFraction(heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin);
            float score = ApplyFactor(dot, heightOverOriginFactor, heightOverOriginInfluence);

            result.push_back(SpotAndScore(spotNums[i], score));
        }
    }
    else
    {
        for (unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i)
        {
            const TacticalSpot &spot = spots[spotNums[i]];

            float heightOverOrigin = spot.absMins[2] - originZ;
            if (heightOverOrigin < minHeightAdvantageOverOrigin)
                continue;

            Vec3 toSpotDir(spot.origin);
            toSpotDir -= origin;
            float squaredDistanceToSpot = toSpotDir.SquaredLength();
            if (squaredDistanceToSpot < 1)
                continue;

            toSpotDir *= Q_RSqrt(squaredDistanceToSpot);
            float absDot = fabsf(toSpotDir.Dot(dodgeDir));
            if (absDot < 0.2f)
                continue;

            heightOverOrigin -= minHeightAdvantageOverOrigin;
            float heightOverOriginFactor = BoundedFraction(heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin);
            float score = ApplyFactor(absDot, heightOverOriginFactor, heightOverOriginInfluence);

            result.push_back(SpotAndScore(spotNums[i], score));
        }
    }

    // Sort result so best score areas are first
    std::sort(result.begin(), result.end());
}

Vec3 TacticalSpotsRegistry::MakeDodgeDangerDir(const OriginParams &originParams,
                                               const DodgeDangerProblemParams &problemParams,
                                               bool *mightNegateDodgeDir) const
{
    *mightNegateDodgeDir = false;
    if (problemParams.avoidSplashDamage)
    {
        Vec3 result(0, 0, 0);
        Vec3 originToHitDir = problemParams.dangerHitPoint - originParams.origin;
        float degrees = originParams.originEntity ? -originParams.originEntity->s.angles[YAW] : -90;
        RotatePointAroundVector(result.Data(), &axis_identity[AXIS_UP], originToHitDir.Data(), degrees);
        result.NormalizeFast();

        if (fabs(result.X()) < 0.3)
            result.X() = 0;
        if (fabs(result.Y()) < 0.3)
            result.Y() = 0;
        result.Z() = 0;
        result.X() *= -1.0f;
        result.Y() *= -1.0f;
        return result;
    }

    Vec3 selfToHitPoint = problemParams.dangerHitPoint - originParams.origin;
    selfToHitPoint.Z() = 0;
    // If bot is not hit in its center, try pick a direction that is opposite to a vector from bot center to hit point
    if (selfToHitPoint.SquaredLength() > 4 * 4)
    {
        selfToHitPoint.NormalizeFast();
        // Check whether this direction really helps to dodge the danger
        // (the less is the abs. value of the dot product, the closer is the chosen direction to a perpendicular one)
        if (fabs(selfToHitPoint.Dot(originParams.origin)) < 0.5f)
        {
            if (fabs(selfToHitPoint.X()) < 0.3)
                selfToHitPoint.X() = 0;
            if (fabs(selfToHitPoint.Y()) < 0.3)
                selfToHitPoint.Y() = 0;
            return -selfToHitPoint;
        }
    }

    *mightNegateDodgeDir = true;
    // Otherwise just pick a direction that is perpendicular to the danger direction
    float maxCrossSqLen = 0.0f;
    Vec3 result(0, 1, 0);
    for (int i = 0; i < 3; ++i)
    {
        Vec3 cross = problemParams.dangerDirection.Cross(&axis_identity[i * 3]);
        cross.Z() = 0;
        float crossSqLen = cross.SquaredLength();
        if (crossSqLen > maxCrossSqLen)
        {
            maxCrossSqLen = crossSqLen;
            float invLen = Q_RSqrt(crossSqLen);
            result.X() = cross.X() * invLen;
            result.Y() = cross.Y() * invLen;
        }
    }
    return result;
}