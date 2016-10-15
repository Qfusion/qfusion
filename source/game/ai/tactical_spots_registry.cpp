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

uint16_t TacticalSpotsRegistry::FindSpotsInRadius(const OriginParams &originParams, unsigned short *spotNums) const
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

    uint16_t numSpotsInRadius = 0;
    const float squareRadius = originParams.searchRadius * originParams.searchRadius;

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
                    if (DistanceSquared(spot.origin, originParams.origin) < squareRadius)
                    {
                        spotNums[numSpotsInRadius++] = spotNum;
                    }
                }
            }
        }
    }

    return numSpotsInRadius;
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

int TacticalSpotsRegistry::FindPositionalAdvantageSpots(const OriginParams &originParams,
                                                        const AdvantageProblemParams &problemParams,
                                                        vec3_t *spotOrigins, int maxSpots) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t numSpotsInBounds = FindSpotsInRadius(originParams, boundsSpots);

    CandidateSpots candidateSpots;
    SelectCandidateSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    ReachCheckedSpots reachCheckedSpots;
    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots, reachCheckedSpots);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, reachCheckedSpots);

    TraceCheckedSpots traceCheckedSpots;
    CheckSpotsVisibleOriginTrace(originParams, problemParams, reachCheckedSpots, traceCheckedSpots);

    SortByVisAndOtherFactors(originParams, problemParams, traceCheckedSpots);

    return CopyResults(traceCheckedSpots, problemParams, spotOrigins, maxSpots);
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
                                                      ReachCheckedSpots &result) const
{
    AiAasRouteCache *routeCache = originParams.routeCache;
    const int originAreaNum = originParams.originAreaNum;
    const float *origin = originParams.origin;
    const float searchRadius = originParams.searchRadius;
    const float lowestWeightTravelTimeBounds = problemParams.lowestWeightTravelTimeBounds;
    const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.originDistanceInfluence;
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
    const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
    const float distanceInfluence = problemParams.originDistanceInfluence;
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
    std::sort(result.begin(), result.end());
}

int TacticalSpotsRegistry::FindCoverSpots(const OriginParams &originParams,
                                          const CoverProblemParams &problemParams,
                                          vec3_t *spotOrigins, int maxSpots) const
{
    uint16_t boundsSpots[MAX_SPOTS];
    uint16_t numSpotsInBounds = FindSpotsInRadius(originParams, boundsSpots);

    CandidateSpots candidateSpots;
    SelectCandidateSpots(originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots);

    ReachCheckedSpots reachCheckedSpots;
    if (problemParams.checkToAndBackReachability)
        CheckSpotsReachFromOriginAndBack(originParams, problemParams, candidateSpots, reachCheckedSpots);
    else
        CheckSpotsReachFromOrigin(originParams, problemParams, candidateSpots, reachCheckedSpots);

    TraceCheckedSpots coverSpots;
    SelectSpotsForCover(originParams, problemParams, reachCheckedSpots, coverSpots);

    return CopyResults(coverSpots, problemParams, spotOrigins, maxSpots);
}

void TacticalSpotsRegistry::SelectSpotsForCover(const OriginParams &originParams,
                                                const CoverProblemParams &problemParams,
                                                ReachCheckedSpots &candidateSpots,
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
