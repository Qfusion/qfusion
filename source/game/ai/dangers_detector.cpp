#include "ai_shutdown_hooks_holder.h"
#include "caching_game_allocator.h"
#include "dangers_detector.h"
#include <algorithm>

inline trace_t Trace(const Vec3 &start, const Vec3 &mins, const Vec3 &maxs, const Vec3 &end, const edict_t *passedict, int contentmask = MASK_SOLID)
{
    trace_t trace;
    float *startVec = const_cast<float*>(start.Data());
    float *minsVec = const_cast<float*>(mins.Data());
    float *maxsVec = const_cast<float*>(maxs.Data());
    float *endVec = const_cast<float*>(end.Data());
    G_Trace(&trace, startVec, minsVec, maxsVec, endVec, const_cast<edict_t*>(passedict), contentmask);
    return trace;
}

constexpr float Square(float x) { return x * x; }

class EntitiesDetector {
    static constexpr float DETECT_ROCKET_SQ_RADIUS = Square(300.0f);
    static constexpr float DETECT_PLASMA_SQ_RADIUS = Square(400.0f);
    static constexpr float DETECT_GB_BLAST_SQ_RADIUS = Square(400.0f);
    static constexpr float DETECT_GRENADE_SQ_RADIUS = Square(300.0f);
    static constexpr float DETECT_LG_BEAM_SQ_RADIUS = Square(1000.0f);

    // There is a way to compute it in compile-time but it looks ugly
    static constexpr float MAX_RADIUS = 1000.0f;
    static_assert(MAX_RADIUS * MAX_RADIUS >= DETECT_ROCKET_SQ_RADIUS, "");
    static_assert(MAX_RADIUS * MAX_RADIUS >= DETECT_PLASMA_SQ_RADIUS, "");
    static_assert(MAX_RADIUS * MAX_RADIUS >= DETECT_GB_BLAST_SQ_RADIUS, "");
    static_assert(MAX_RADIUS * MAX_RADIUS >= DETECT_GRENADE_SQ_RADIUS, "");
    static_assert(MAX_RADIUS * MAX_RADIUS >= DETECT_LG_BEAM_SQ_RADIUS, "");

    void Clear()
    {
        rockets.clear();
        plasmas.clear();
        blasts.clear();
        grenades.clear();
        lasers.clear();
    }

public:
    StaticVector<const edict_t*, MAX_EDICTS> rockets;
    StaticVector<const edict_t*, MAX_EDICTS> plasmas;
    StaticVector<const edict_t*, MAX_EDICTS> blasts;
    StaticVector<const edict_t*, MAX_EDICTS> grenades;
    StaticVector<const edict_t*, MAX_EDICTS> lasers;

    void DetectEntities(const edict_t *self)
    {
        Clear();

        // Copy to locals for faster access
        const edict_t *gameEdicts = game.edicts;
        vec3_t origin;
        VectorCopy(self->s.origin, origin);
        const int selfTeam = self->s.team;
        const int selfPlayerNum = ENTNUM(const_cast<edict_t *>(self)) + 1;
        const bool hasSelfDamage = GS_SelfDamage();
        const unsigned levelTime = level.time;
        const unsigned grenadeTimeout = GS_GetWeaponDef(WEAP_GRENADELAUNCHER)->firedef.timeout;

        int entNums[MAX_EDICTS];
        int numEntsInRadius = GClip_FindInRadius(origin, MAX_RADIUS, entNums, MAX_EDICTS);

        // Conditions are put inside the switch body for optimization of cases.
        // Note that we always skip own rockets, plasma, etc.
        // Otherwise all own bot shot events yield a danger.
        // There are some cases when an own rocket can hurt but they are either extremely rare or handled by bot fire code.
        // Own grenades are the only exception. We check grenade think time to skip grenades just fired by bot.
        // If a grenade is about to explode and is close to bot, its likely it has bounced of the world and can hurt.

        if (g_allow_teamdamage->integer)
        {
            for (int i = 0; i < numEntsInRadius; ++i)
            {
                const edict_t *ent = gameEdicts + entNums[i];
                switch (ent->s.type)
                {
                    case ET_ROCKET:
                        if (selfPlayerNum != ent->s.ownerNum)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_ROCKET_SQ_RADIUS)
                            {
                                rockets.push_back(ent);
                            }
                        }
                        break;
                    case ET_PLASMA:
                        if (selfPlayerNum != ent->s.ownerNum)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_PLASMA_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_BLASTER:
                        if (selfPlayerNum != ent->s.ownerNum)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GB_BLAST_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_GRENADE:
                        if (selfPlayerNum != ent->s.ownerNum)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GRENADE_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        else if (hasSelfDamage && ent->nextThink - levelTime < grenadeTimeout - 500)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GRENADE_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_LASERBEAM:
                        if (selfPlayerNum != ent->s.ownerNum)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_LG_BEAM_SQ_RADIUS)
                                lasers.push_back(ent);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        else
        {
            for (int i = 0; i < numEntsInRadius; ++i)
            {
                const edict_t *ent = gameEdicts + entNums[i];
                switch (ent->s.type)
                {
                    case ET_ROCKET:
                        if (self->team != ent->team)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_ROCKET_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_PLASMA:
                        if (selfTeam != ent->s.team)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_PLASMA_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_BLASTER:
                        if (selfTeam != ent->s.team)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GB_BLAST_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_GRENADE:
                        if (selfTeam != ent->s.team)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GRENADE_SQ_RADIUS)
                            {
                                rockets.push_back(ent);
                            }
                        }
                        else if (selfPlayerNum == ent->s.ownerNum && ent->nextThink - levelTime < grenadeTimeout - 500)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_GRENADE_SQ_RADIUS)
                                rockets.push_back(ent);
                        }
                        break;
                    case ET_LASERBEAM:
                        if (selfTeam != ent->s.team)
                        {
                            if (DistanceSquared(origin, ent->s.origin) < DETECT_LG_BEAM_SQ_RADIUS)
                                lasers.push_back(ent);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
};

class PlasmaBeam
{
    friend class PlasmaBeamsBuilder;

    PlasmaBeam() {}
public:
    PlasmaBeam(const edict_t *firstProjectile)
        : startProjectile(firstProjectile),
          endProjectile(firstProjectile),
          owner(firstProjectile->r.owner),
          projectilesCount(1) {}

    const edict_t *startProjectile;
    const edict_t *endProjectile;
    const edict_t *owner; // May be null if this beam contains of projectiles of many players

    inline Vec3 start() { return Vec3(startProjectile->s.origin); }
    inline Vec3 end() { return Vec3(endProjectile->s.origin); }

    int projectilesCount;

    inline void AddProjectile(const edict_t *nextProjectile)
    {
        endProjectile = nextProjectile;
        // If beam is combined from projectiles of many players, a beam owner is unknown
        if (owner != nextProjectile->r.owner)
        {
            owner = nullptr;
        }
        projectilesCount++;
    }
};

struct EntAndLineParam
{
    const edict_t *ent;
    float t;

    inline EntAndLineParam(const edict_t *ent_, float t_): ent(ent_), t(t_) {}
    inline bool operator<(const EntAndLineParam &that) const { return t < that.t; }
};

class SameDirBeamsList
{
    friend class PlasmaBeamsBuilder;
    // All projectiles in this list belong to this line defined as a (point, direction) pair
    Vec3 lineEqnPoint;

    EntAndLineParam *sortedProjectiles;
    unsigned projectilesCount;

    static constexpr float DIST_TO_RAY_THRESHOLD = 200.0f;
    static constexpr float DIR_DOT_THRESHOLD = 0.995f;
    static constexpr float PRJ_PROXIMITY_THRESHOLD = 300.0f;

public:
    bool isAprioriSkipped;
    Vec3 avgDirection;
    PlasmaBeam *plasmaBeams;
    unsigned plasmaBeamsCount;

    SameDirBeamsList(const edict_t *firstEntity, const edict_t *bot);

    inline SameDirBeamsList(SameDirBeamsList &&that)
        : lineEqnPoint(that.lineEqnPoint),
          sortedProjectiles(that.sortedProjectiles),
          projectilesCount(that.projectilesCount),
          isAprioriSkipped(that.isAprioriSkipped),
          avgDirection(that.avgDirection),
          plasmaBeams(that.plasmaBeams),
          plasmaBeamsCount(that.plasmaBeamsCount)
    {
        that.sortedProjectiles = nullptr;
        that.plasmaBeams = nullptr;
    }

    ~SameDirBeamsList();

    bool TryAddProjectile(const edict_t *projectile);

    void BuildBeams();

    inline float ComputeLineEqnParam(const edict_t *projectile)
    {
        const float *origin = projectile->s.origin;
        if (fabs(avgDirection.X()) > 0.1)
            return (origin[0] - lineEqnPoint.X()) / avgDirection.X();
        if (fabs(avgDirection.Y()) > 0.1)
            return (origin[1] - lineEqnPoint.Y()) / avgDirection.Y();
        return (origin[2] - lineEqnPoint.Z()) / avgDirection.Z();
    }
};

class PlasmaBeamsBuilder
{
    StaticVector<SameDirBeamsList, 1024> sameDirLists;

    static constexpr float SQ_DANGER_RADIUS = 300.0f * 300.0f;

    const edict_t *bot;
public:
    PlasmaBeamsBuilder(const edict_t *bot_): bot(bot_) {}

    void AddProjectile(const edict_t *projectile)
    {
        for (unsigned i = 0; i < sameDirLists.size(); ++i)
        {
            if (sameDirLists[i].TryAddProjectile(projectile))
            {
                return;
            }
        }
        sameDirLists.emplace_back(SameDirBeamsList(projectile, bot));
    }

    template<unsigned N>
    bool FindMostDangerousBeams(StaticVector<Danger, N> &beamDangers, float plasmaSplashRadius);
};

CachingGameBufferAllocator<EntAndLineParam, MAX_EDICTS> sortedProjectilesBufferAllocator("prj");
CachingGameBufferAllocator<PlasmaBeam, MAX_EDICTS> plasmaBeamsBufferAllocator("beams");

SameDirBeamsList::SameDirBeamsList(const edict_t *firstEntity, const edict_t *bot)
    : lineEqnPoint(firstEntity->s.origin),
      sortedProjectiles(nullptr),
      projectilesCount(0),
      avgDirection(firstEntity->velocity),
      plasmaBeams(nullptr),
      plasmaBeamsCount(0)
{
    avgDirection.NormalizeFast();

    // If distance from an infinite line of beam to bot is greater than threshold, skip;
    // Let's compute distance from bot to the beam infinite line;
    Vec3 botOrigin(bot->s.origin);
    float squaredDistanceToBeamLine = (botOrigin - lineEqnPoint).Cross(avgDirection).SquaredLength();
    if (squaredDistanceToBeamLine > DIST_TO_RAY_THRESHOLD * DIST_TO_RAY_THRESHOLD)
    {
        isAprioriSkipped = true;
    }
    else
    {
        sortedProjectiles = sortedProjectilesBufferAllocator.Alloc();
        plasmaBeams = plasmaBeamsBufferAllocator.Alloc();

        isAprioriSkipped = false;

        sortedProjectiles[projectilesCount++] = EntAndLineParam(firstEntity, ComputeLineEqnParam(firstEntity));
    }
}

SameDirBeamsList::~SameDirBeamsList()
{
    if (isAprioriSkipped)
        return;
    // (Do not spam log by messages unless we have allocated memory chunks)
    if (sortedProjectiles)
        sortedProjectilesBufferAllocator.Free(sortedProjectiles);
    if (plasmaBeams)
        plasmaBeamsBufferAllocator.Free(plasmaBeams);
    sortedProjectiles = nullptr;
    plasmaBeams = nullptr;
}

bool SameDirBeamsList::TryAddProjectile(const edict_t *projectile)
{
    Vec3 direction(projectile->velocity);
    direction.NormalizeFast();

    if (direction.Dot(avgDirection) < DIR_DOT_THRESHOLD)
        return false;

    // Do not process a projectile, but "consume" it anyway...
    if (isAprioriSkipped)
        return true;

    // Update average direction
    avgDirection += direction;
    avgDirection.NormalizeFast();

    float t = ComputeLineEqnParam(projectile);

    sortedProjectiles[projectilesCount++] = EntAndLineParam(projectile, t);
    std::push_heap(sortedProjectiles, sortedProjectiles + projectilesCount);

    return true;
}

void SameDirBeamsList::BuildBeams()
{
    if (isAprioriSkipped)
        return;

    if (projectilesCount == 0)
        AI_FailWith("SameDirBeamsList::BuildBeams()", "Projectiles count: %d\n", projectilesCount);

    // Get the projectile that has a maximal `t`
    std::pop_heap(sortedProjectiles, sortedProjectiles + projectilesCount);
    const edict_t *prevProjectile = sortedProjectiles[--projectilesCount].ent;

    plasmaBeams[plasmaBeamsCount++] = PlasmaBeam(prevProjectile);

    while (projectilesCount > 0)
    {
        // Get the projectile that has a maximal `t` atm
        std::pop_heap(sortedProjectiles, sortedProjectiles + projectilesCount);
        const edict_t *currProjectile = sortedProjectiles[--projectilesCount].ent;

        float prevToCurrLen = (Vec3(prevProjectile->s.origin) - currProjectile->s.origin).SquaredLength();
        if (prevToCurrLen < PRJ_PROXIMITY_THRESHOLD * PRJ_PROXIMITY_THRESHOLD)
        {
            // Add the projectile to the last beam
            plasmaBeams[plasmaBeamsCount - 1].AddProjectile(currProjectile);
        }
        else
        {
            // Construct new plasma beam at the end of beams array
            plasmaBeams[plasmaBeamsCount++] = PlasmaBeam(currProjectile);
        }
    }
}

template <unsigned N>
void PushDanger(StaticVector<Danger, N> &dangers, const Danger &danger, float minDamageLike)
{
    if (dangers.empty())
    {
        dangers.push_back(danger);
    }
    else
    {
        if (dangers.size() < dangers.capacity())
        {
            // There are a free room for the new danger, just add the new one
            dangers.push_back(danger);
        }
        else
        {
            // Replace previous min danger with the current one
            auto predicate = [=](const Danger &a) { return a.damage == minDamageLike; };
            auto at = std::find_if(dangers.begin(), dangers.end(), predicate) - dangers.begin();
            dangers[at] = danger;
        }
    }
}

template <unsigned N>
bool PlasmaBeamsBuilder::FindMostDangerousBeams(StaticVector<Danger, N> &dangers, float plasmaSplashRadius)
{
    Vec3 botOrigin(bot->s.origin);

    for (unsigned i = 0; i < sameDirLists.size(); ++i)
        sameDirLists[i].BuildBeams();

    Vec3 beamMins(vec3_origin);
    Vec3 beamMaxs(vec3_origin);

    float minDamageLike = 0.0f;

    for (const SameDirBeamsList &beamsList: sameDirLists)
    {
        if (beamsList.isAprioriSkipped)
            continue;

        for (unsigned i = 0; i < beamsList.plasmaBeamsCount; ++i)
        {
            PlasmaBeam *beam = beamsList.plasmaBeams + i;

            Vec3 botToBeamStart = beam->start() - botOrigin;
            Vec3 botToBeamEnd = beam->end() - botOrigin;

            if (botToBeamStart.SquaredLength() > SQ_DANGER_RADIUS && botToBeamEnd.SquaredLength() > SQ_DANGER_RADIUS)
                continue;

            Vec3 beamStartToEnd = beam->end() - beam->start();

            float dotBotToStartWithDir = botToBeamStart.Dot(beamStartToEnd);
            float dotBotToEndWithDir = botToBeamEnd.Dot(beamStartToEnd);

            // If the beam has entirely passed the bot and is flying away, skip it
            if (dotBotToStartWithDir > 0 && dotBotToEndWithDir > 0)
                continue;

            Vec3 tracedBeamStart = beam->start();
            Vec3 tracedBeamEnd = beam->end();

            // It works for single-projectile beams too
            Vec3 beamDir(beam->startProjectile->velocity);
            beamDir.NormalizeFast();
            tracedBeamEnd += 108.0f * beamDir;

            trace_t trace = Trace(tracedBeamStart, beamMins, beamMaxs, tracedBeamEnd, beam->owner);
            if (trace.fraction < 1.0f)
            {
                Vec3 hitPoint(trace.endpos);

                // Direct hit
                if (bot == game.edicts + trace.ent)
                {
                    float damageLike = beam->projectilesCount;
                    if (damageLike > minDamageLike)
                    {
                        PushDanger(dangers, Danger(hitPoint, beamsList.avgDirection, damageLike, beam->owner, false), minDamageLike);
                        minDamageLike = damageLike;
                    }
                }
                // Splash hit
                else
                {
                    float hitVecLen = (hitPoint - botOrigin).LengthFast();
                    if (hitVecLen < plasmaSplashRadius)
                    {
                        // We treat up to 3 projectiles as a single explosion cluster (other projectiles are still flying)
                        float damageLike = std::max(3, beam->projectilesCount) * (1.0f - hitVecLen / plasmaSplashRadius);
                        if (damageLike > minDamageLike)
                        {
                            PushDanger(dangers, Danger(hitPoint, beamsList.avgDirection, damageLike, beam->owner, true), minDamageLike);
                            minDamageLike = damageLike;
                        }
                    }
                }
            }
        }
    }

    return !dangers.empty();
}

void DangersDetector::Clear()
{
    rocketDangers.clear();
    plasmaDangers.clear();
    grenadeDangers.clear();
    blastsDangers.clear();
    laserDangers.clear();

    primaryDanger = nullptr;
}

// TODO: Add secondary danger, try to clusterize dangers?
void DangersDetector::RegisterDanger(const Danger &danger)
{
    if (primaryDanger == nullptr || primaryDanger->damage < danger.damage)
        primaryDanger = &danger;
}

template<unsigned N>
void DangersDetector::ScaleAndRegisterDangers(StaticVector<Danger, N> &container, float damageScale)
{
    for (Danger &danger: container)
    {
        danger.damage *= damageScale;
        RegisterDanger(danger);
    }
}

// TODO: Do not detect dangers that may not be seen by bot, but make bot aware if it can hear the danger
bool DangersDetector::FindDangers()
{
    Clear();

    auto defOf_RL = GS_GetWeaponDef(WEAP_ROCKETLAUNCHER);
    auto defOf_GB = GS_GetWeaponDef(WEAP_GUNBLADE);
    auto defOf_GL = GS_GetWeaponDef(WEAP_GRENADELAUNCHER);
    auto defOf_PG = GS_GetWeaponDef(WEAP_PLASMAGUN);
    auto defOf_LG = GS_GetWeaponDef(WEAP_LASERGUN);

    float dangerRadius_RL = 1.75f * defOf_RL->firedef.splash_radius;
    float dangerRadius_GB = 1.50f * defOf_GB->firedef.splash_radius;
    float dangerRadius_GL = 2.00f * defOf_GL->firedef.splash_radius;

    EntitiesDetector entitiesDetector;
    entitiesDetector.DetectEntities(self);

    bool result = false;

    if (!entitiesDetector.rockets.empty())
    {
        if (FindProjectileDangers(rocketDangers, entitiesDetector.rockets, dangerRadius_RL))
        {
            ScaleAndRegisterDangers(rocketDangers, defOf_RL->firedef.damage);
            result = true;
        }
    }
    if (!entitiesDetector.blasts.empty())
    {
        if (FindProjectileDangers(blastsDangers, entitiesDetector.blasts, dangerRadius_GB))
        {
            ScaleAndRegisterDangers(blastsDangers, defOf_GB->firedef.damage);
            result = true;
        }
    }
    if (!entitiesDetector.grenades.empty())
    {
        if (FindProjectileDangers(grenadeDangers, entitiesDetector.grenades, dangerRadius_GL))
        {
            ScaleAndRegisterDangers(grenadeDangers, defOf_GL->firedef.damage);
            result = true;
        }
    }
    if (!entitiesDetector.plasmas.empty())
    {
        if (FindPlasmaDangers(plasmaDangers, entitiesDetector.plasmas, 1.5f * defOf_PG->firedef.splash_radius))
        {
            ScaleAndRegisterDangers(plasmaDangers, defOf_PG->firedef.damage);
            result = true;
        }
    }
    if (!entitiesDetector.lasers.empty())
    {
        if (FindLaserDangers(laserDangers, entitiesDetector.lasers))
        {
            // Let us assume possible damage as one second of being shot with 33% accuracy
            float beamDamage = 0.33f * defOf_LG->firedef.damage * (1000.0f / defOf_LG->firedef.reload_time);
            ScaleAndRegisterDangers(laserDangers, beamDamage);
            result = true;
        }
    }

    return result;
}

template <unsigned N, unsigned M>
bool DangersDetector::FindPlasmaDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t*, M> &plasmas, float plasmaSplashRadius)
{
    PlasmaBeamsBuilder plasmaBeamsBuilder(self);
    for (unsigned i = 0; i < plasmas.size(); ++i)
    {
        plasmaBeamsBuilder.AddProjectile(plasmas[i]);
    }
    return plasmaBeamsBuilder.FindMostDangerousBeams(dangers, plasmaSplashRadius);
}

template <unsigned N, unsigned M>
bool DangersDetector::FindLaserDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t*, M> &lasers)
{
    trace_t trace;

    for (unsigned i = 0; i < lasers.size(); ++i)
    {
        edict_t *beam = const_cast<edict_t *>(lasers[i]);
        G_Trace(&trace, beam->s.origin, vec3_origin, vec3_origin, beam->s.origin2, beam, MASK_AISOLID);
        if (trace.fraction < 1.0f)
        {
            if (self != game.edicts + trace.ent)
                continue;

            Vec3 hitPoint(trace.endpos);
            edict_t *owner = game.edicts + beam->s.ownerNum;

            Vec3 direction(beam->s.origin2);
            direction -= beam->s.origin;
            float squareLen = direction.SquaredLength();
            if (squareLen > 1)
            {
                direction *= Q_RSqrt(squareLen);
            }
            else
            {
                // Very rare but really seen case - beam has zero length
                vec3_t forward, right, up;
                AngleVectors(owner->s.angles, forward, right, up);
                direction += forward;
                direction += right;
                direction += up;
                direction.NormalizeFast();
            }
            if (dangers.empty())
            {
                dangers.emplace_back(Danger(hitPoint, direction, 1.0f, owner, false));
            }
            else
            {
                // If max beams count has been reached, overwrite first beam
                // TODO: We need a-priori info to elevate priority of best aimers beams
                // From the other hand, the bot will be dead anyway if it is hit by many beams
                if (dangers.size() < MAX_LASER_BEAMS)
                    dangers.push_back(Danger(hitPoint, direction, 1.0f, owner, false));
                else
                    dangers[0] = Danger(hitPoint, direction, 1.0f, owner, false);
            }
        }
    }

    return !dangers.empty();
}

template <unsigned N, unsigned M>
bool DangersDetector::FindProjectileDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t*, M> &entities, float dangerRadius)
{
    trace_t trace;
    float minPrjTime = 1.0f;
    float minDamageLike = 0.0f;

    Vec3 botOrigin(self->s.origin);

    for (unsigned i = 0; i < entities.size(); ++i)
    {
        edict_t *target = const_cast<edict_t *>(entities[i]);
        Vec3 end = Vec3(target->s.origin) + 2.0f * Vec3(target->velocity);
        G_Trace(&trace, target->s.origin, target->r.mins, target->r.maxs, end.Data(), target, MASK_AISOLID);
        if (trace.fraction < minPrjTime)
        {
            minPrjTime = trace.fraction;
            Vec3 hitPoint(trace.endpos);
            Vec3 botToHitPoint = hitPoint - botOrigin;
            float hitVecLen = botToHitPoint.LengthFast();
            if (hitVecLen < dangerRadius)
            {
                float damageLike = 1.0f - hitVecLen / dangerRadius;
                if (damageLike > minDamageLike)
                {
                    // Velocity may be zero for some projectiles (e.g. grenades)
                    Vec3 direction(target->velocity);
                    float squaredLen = direction.SquaredLength();
                    if (squaredLen > 0.1f)
                    {
                        direction *= Q_RSqrt(squaredLen);
                    }
                    else
                    {
                        direction = Vec3(&axis_identity[AXIS_UP]);
                    }
                    PushDanger(dangers, Danger(hitPoint, direction, damageLike, target->r.owner, true), minDamageLike);
                    minDamageLike = damageLike;
                }
            }
        }
    }

    return !dangers.empty();
}

