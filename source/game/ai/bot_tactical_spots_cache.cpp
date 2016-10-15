#include "bot.h"
#include "tactical_spots_registry.h"

inline AiAasRouteCache *BotTacticalSpotsCache::RouteCache()
{
    return self->ai->botRef->routeCache;
}

inline float BotTacticalSpotsCache::Skill() const
{
    return self->ai->botRef->Skill();
}

inline bool BotTacticalSpotsCache::BotHasAlmostSameOrigin(const Vec3 &unpackedOrigin) const
{
    constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    return DistanceSquared(self->s.origin, unpackedOrigin.Data()) < squareDistanceError;
}

template <typename ProblemParams>
inline bool BotTacticalSpotsCache::FindForOrigin(const ProblemParams &problemParams,
                                                 const Vec3 &origin, float searchRadius, vec3_t result)
{
    vec3_t *spots = (vec3_t *)result;
    const TacticalSpotsRegistry *tacticalSpotsRegistry = TacticalSpotsRegistry::Instance();
    if (BotHasAlmostSameOrigin(origin))
    {
        // Provide a bot entity to aid trace checks
        TacticalSpotsRegistry::OriginParams originParams(self, searchRadius, RouteCache());
        return tacticalSpotsRegistry->FindPositionalAdvantageSpots(originParams, problemParams, spots, 1) > 0;
    }
    TacticalSpotsRegistry::OriginParams originParams(origin.Data(), searchRadius, RouteCache());
    return tacticalSpotsRegistry->FindPositionalAdvantageSpots(originParams, problemParams, spots, 1) > 0;
}

const short *BotTacticalSpotsCache::GetSpot(SpotsList *list, const short *origin,
                                            const short *enemyOrigin, FindMethod findMethod)
{
    CachedSpot *spots = list->spots;
    for (unsigned i = 0, end = list->numSpots; i < end; ++i)
    {
        CachedSpot &spot = spots[i];
        if (!VectorCompare(origin, spot.origin))
            continue;
        if (!VectorCompare(enemyOrigin, spot.enemyOrigin))
            continue;
        return spot.succeeded ? spot.spotData : nullptr;
    }

    CachedSpot *newSpot = list->Alloc();
    // Can't allocate a spot. It also means a limit of such tactical spots per think frame has been exceeded.
    if (!newSpot)
        return nullptr;

    VectorCopy(origin, newSpot->origin);
    VectorCopy(enemyOrigin, newSpot->enemyOrigin);

    Vec3 unpackedOrigin(4 * origin[0], 4 * origin[1], 4 * origin[2]);
    Vec3 unpackedEnemyOrigin(4 * enemyOrigin[0], 4 * enemyOrigin[1], 4 * enemyOrigin[2]);
    vec3_t foundSpotOrigin;
    if (!(this->*findMethod)(unpackedOrigin, unpackedEnemyOrigin, foundSpotOrigin))
    {
        newSpot->succeeded = false;
        return nullptr;
    }

    for (unsigned i = 0; i < 3; ++i)
        newSpot->spotData[i] = (short)(((int)foundSpotOrigin[i]) / 4);

    newSpot->succeeded = true;
    return newSpot->spotData;
}

bool BotTacticalSpotsCache::FindSniperRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result)
{
    TacticalSpotsRegistry::AdvantageProblemParams problemParams(enemyOrigin.Data());
    problemParams.SetMinSpotDistanceToEntity(WorldState::FAR_RANGE_MAX);
    problemParams.SetOriginDistanceInfluence(0.0f);
    problemParams.SetTravelTimeInfluence(0.5f);
    problemParams.SetMinHeightAdvantageOverOrigin(-1024.0f);
    problemParams.SetMinHeightAdvantageOverEntity(-1024.0f);
    problemParams.SetHeightOverOriginInfluence(0.3f);
    problemParams.SetHeightOverEntityInfluence(0.1f);
    problemParams.SetCheckToAndBackReachability(false);

    float searchRadius = 192.0f + 768.0f * Skill();
    float distanceToEnemy = (origin - enemyOrigin).LengthFast();
    // If bot is not on sniper range, increase search radius (otherwise a point in a sniper range can't be found).
    if (distanceToEnemy - searchRadius < WorldState::FAR_RANGE_MAX)
        searchRadius += WorldState::FAR_RANGE_MAX - distanceToEnemy + searchRadius;

    return FindForOrigin(problemParams, origin, searchRadius, result);
}

bool BotTacticalSpotsCache::FindFarRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result)
{
    TacticalSpotsRegistry::AdvantageProblemParams problemParams(enemyOrigin.Data());
    problemParams.SetMinSpotDistanceToEntity(WorldState::MIDDLE_RANGE_MAX);
    problemParams.SetMaxSpotDistanceToEntity(WorldState::FAR_RANGE_MAX);
    problemParams.SetOriginDistanceInfluence(0.0f);
    problemParams.SetEntityDistanceInfluence(0.3f);
    problemParams.SetEntityWeightFalloffDistanceRatio(0.25f);
    problemParams.SetTravelTimeInfluence(0.5f);
    problemParams.SetMinHeightAdvantageOverOrigin(-192.0f);
    problemParams.SetMinHeightAdvantageOverEntity(-512.0f);
    problemParams.SetHeightOverOriginInfluence(0.3f);
    problemParams.SetHeightOverEntityInfluence(0.5f);
    problemParams.SetCheckToAndBackReachability(false);

    float searchRadius = 192.0f + 768.0f * Skill();
    float distanceToEnemy = (origin - enemyOrigin).LengthFast();
    float minSearchDistanceToEnemy = distanceToEnemy - searchRadius;
    float maxSearchDistanceToEnemy = distanceToEnemy + searchRadius;
    if (minSearchDistanceToEnemy < WorldState::MIDDLE_RANGE_MAX)
        searchRadius += WorldState::MIDDLE_RANGE_MAX - minSearchDistanceToEnemy;
    else if (maxSearchDistanceToEnemy > WorldState::FAR_RANGE_MAX)
        searchRadius += maxSearchDistanceToEnemy - WorldState::FAR_RANGE_MAX;

    return FindForOrigin(problemParams, origin, searchRadius, result);
}

bool BotTacticalSpotsCache::FindMiddleRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result)
{
    TacticalSpotsRegistry::AdvantageProblemParams problemParams(enemyOrigin.Data());
    problemParams.SetMinSpotDistanceToEntity(WorldState::CLOSE_RANGE_MAX);
    problemParams.SetMaxSpotDistanceToEntity(WorldState::MIDDLE_RANGE_MAX);
    problemParams.SetOriginDistanceInfluence(0.3f);
    problemParams.SetEntityDistanceInfluence(0.4f);
    problemParams.SetEntityWeightFalloffDistanceRatio(0.5f);
    problemParams.SetTravelTimeInfluence(0.7f);
    problemParams.SetMinHeightAdvantageOverOrigin(-16.0f);
    problemParams.SetMinHeightAdvantageOverEntity(-64.0f);
    problemParams.SetHeightOverOriginInfluence(0.6f);
    problemParams.SetHeightOverEntityInfluence(0.8f);
    problemParams.SetCheckToAndBackReachability(false);

    float searchRadius = WorldState::MIDDLE_RANGE_MAX;
    float distanceToEnemy = (origin - enemyOrigin).LengthFast();
    if (distanceToEnemy < WorldState::CLOSE_RANGE_MAX)
        searchRadius += WorldState::CLOSE_RANGE_MAX;
    else if (distanceToEnemy > WorldState::MIDDLE_RANGE_MAX)
        searchRadius += distanceToEnemy - WorldState::MIDDLE_RANGE_MAX;
    else
        searchRadius *= 1.0f + 0.5f * Skill();

    return FindForOrigin(problemParams, origin, searchRadius, result);
}

bool BotTacticalSpotsCache::FindCloseRangeTacticalSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result)
{
    TacticalSpotsRegistry::AdvantageProblemParams problemParams(enemyOrigin.Data());
    float meleeRange = GS_GetWeaponDef(WEAP_GUNBLADE)->firedef_weak.timeout;
    problemParams.SetMinSpotDistanceToEntity(meleeRange);
    problemParams.SetMaxSpotDistanceToEntity(WorldState::CLOSE_RANGE_MAX);
    problemParams.SetOriginDistanceInfluence(0.0f);
    problemParams.SetEntityDistanceInfluence(0.7f);
    problemParams.SetEntityWeightFalloffDistanceRatio(0.8f);
    problemParams.SetTravelTimeInfluence(0.0f);
    problemParams.SetMinHeightAdvantageOverOrigin(-16.0f);
    problemParams.SetMinHeightAdvantageOverEntity(-16.0f);
    problemParams.SetHeightOverOriginInfluence(0.4f);
    problemParams.SetHeightOverEntityInfluence(0.9f);
    // Bot should be able to retreat from close combat
    problemParams.SetCheckToAndBackReachability(true);

    float searchRadius = WorldState::CLOSE_RANGE_MAX * 2;
    float distanceToEnemy = (origin - enemyOrigin).LengthFast();
    if (distanceToEnemy > WorldState::CLOSE_RANGE_MAX)
    {
        searchRadius += distanceToEnemy - WorldState::CLOSE_RANGE_MAX;
        // On this range retreating to an old position makes little sense
        if (distanceToEnemy > 0.5f * WorldState::MIDDLE_RANGE_MAX)
            problemParams.SetCheckToAndBackReachability(false);
    }

    return FindForOrigin(problemParams, origin, searchRadius, result);
}

bool BotTacticalSpotsCache::FindCoverSpot(const Vec3 &origin, const Vec3 &enemyOrigin, vec3_t result)
{
    const float searchRadius = 192.0f + 512.0f * Skill();
    TacticalSpotsRegistry::CoverProblemParams problemParams(enemyOrigin.Data(), 32.0f);
    problemParams.SetOriginDistanceInfluence(0.0f);
    problemParams.SetTravelTimeInfluence(0.3f);
    problemParams.SetMinHeightAdvantageOverOrigin(-searchRadius);
    problemParams.SetHeightOverOriginInfluence(0.3f);
    problemParams.SetCheckToAndBackReachability(false);

    auto *tacticalSpotsRegistry = TacticalSpotsRegistry::Instance();
    if (BotHasAlmostSameOrigin(origin))
    {
        TacticalSpotsRegistry::OriginParams originParams(self, searchRadius, RouteCache());
        return tacticalSpotsRegistry->FindCoverSpots(originParams, problemParams, (vec3_t *)result, 1) != 0;
    }
    TacticalSpotsRegistry::OriginParams originParams(origin.Data(), searchRadius, RouteCache());
    return tacticalSpotsRegistry->FindCoverSpots(originParams, problemParams, (vec3_t *)result, 1) != 0;
}
