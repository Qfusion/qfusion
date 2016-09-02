#include "bot.h"
#include "ai_shutdown_hooks_holder.h"

// This is a port of public domain projectile prediction code by Kain Shin
// http://ringofblades.com/Blades/Code/PredictiveAim.cs
// This function assumes that target velocity is constant and gravity is not applied to projectile and target.
bool PredictProjectileNoClip(const Vec3 &fireOrigin, float projectileSpeed, vec3_t target, const Vec3 &targetVelocity)
{
    constexpr float EPSILON = 0.0001f;

    float projectileSpeedSq = projectileSpeed * projectileSpeed;
    float targetSpeedSq = targetVelocity.SquaredLength();
    float targetSpeed = sqrtf(targetSpeedSq);
    Vec3 targetToFire = fireOrigin - target;
    float targetToFireDistSq = targetToFire.SquaredLength();
    float targetToFireDist = sqrtf(targetToFireDistSq);
    Vec3 targetToFireDir(targetToFire);
    targetToFireDir.Normalize();

    Vec3 targetVelocityDir(targetVelocity);
    targetVelocityDir.Normalize();

    float cosTheta = targetToFireDir.Dot(targetVelocityDir);

    float t;
    if (fabsf(projectileSpeedSq - targetSpeedSq) < EPSILON)
    {
        if (cosTheta <= 0)
            return false;

        t = 0.5f * targetToFireDist / (targetSpeed * cosTheta);
    }
    else
    {
        float a = projectileSpeedSq - targetSpeedSq;
        float b = 2.0f * targetToFireDist * targetSpeed * cosTheta;
        float c = -targetToFireDistSq;
        float discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0)
            return false;

        float uglyNumber = sqrtf(discriminant);
        float t0 = 0.5f * (-b + uglyNumber) / a;
        float t1 = 0.5f * (-b - uglyNumber) / a;
        t = std::min(t0, t1);
        if (t < EPSILON)
            t = std::max(t0, t1);

        if (t < EPSILON)
            return false;
    }

    Vec3 move = targetVelocity * t;
    VectorAdd(target, move.Data(), target);
    return true;
}

void Bot::FireTargetCache::GetPredictedTargetOrigin(const CombatTask &combatTask, float projectileSpeed,
                                                      AimParams *aimParams)
{
    if (bot->ai->botRef->Skill() < 0.33f || combatTask.IsTargetAStaticSpot())
        return;

    // Check whether we are shooting the same enemy and cached predicted origin is not outdated
    if (cachedFireTarget.IsValidFor(combatTask))
    {
        VectorCopy(cachedFireTarget.origin.Data(), aimParams->fireTarget);
    }
    else
    {
        PredictProjectileShot(combatTask, projectileSpeed, aimParams, true);
        cachedFireTarget.origin = Vec3(aimParams->fireTarget);
        cachedFireTarget.combatTaskInstanceId = combatTask.instanceId;
        cachedFireTarget.invalidAt = level.time + 66;
    }
}

void Bot::FireTargetCache::PredictProjectileShot(const CombatTask &combatTask, float projectileSpeed,
                                                   AimParams *aimParams, bool applyTargetGravity)
{
    if (projectileSpeed <= 0.0f)
        return;

    // Copy for convenience
    Vec3 fireOrigin(aimParams->fireOrigin);

    if (applyTargetGravity)
    {
        // Aside from solving quite complicated system of equations that involve acceleration,
        // we have to predict target collision with map environment.
        // To solve it, we approximate target trajectory as a polyline
        // that consists of linear segments plus an end ray from last segment point to infinity.
        // We assume that target velocity is constant withing bounds of a segment.

        constexpr float TIME_STEP = 0.15f; // Time is in seconds

        Vec3 currPoint(aimParams->fireTarget);
        float currTime = 0.0f;
        float nextTime = TIME_STEP;

        trace_t trace;
        edict_t *traceKey = const_cast<edict_t*>(combatTask.TraceKey());

        const int maxSegments = 2 + (int)(2.1 * bot->ai->botRef->Skill());

        const float *targetVelocity = combatTask.EnemyVelocity().Data();

        for (int i = 0; i < maxSegments; ++i)
        {
            Vec3 nextPoint(aimParams->fireTarget);
            nextPoint.X() += targetVelocity[0] * nextTime;
            nextPoint.Y() += targetVelocity[1] * nextTime;
            nextPoint.Z() += targetVelocity[2] * nextTime - 0.5f * level.gravity * nextTime * nextTime;

            // We assume that target has the same velocity as currPoint on a [currPoint, nextPoint] segment
            Vec3 currTargetVelocity(targetVelocity);
            currTargetVelocity.Z() -= level.gravity * currTime;

            // TODO: Projectile speed used in PredictProjectileNoClip() needs correction
            // We can't offset fire origin since we do not know direction to target yet
            // Instead, increase projectile speed used in calculations according to currTime
            // Exact formula is to be proven yet
            Vec3 predictedTarget(currPoint);
            if (!PredictProjectileNoClip(fireOrigin, projectileSpeed, predictedTarget.Data(), currTargetVelocity))
                return;

            // Check whether predictedTarget is within [currPoint, nextPoint]
            // where extrapolation that use currTargetVelocity is assumed to be valid.
            Vec3 currToNextVec = nextPoint - currPoint;
            Vec3 predictedTargetToNextVec = nextPoint - predictedTarget;
            Vec3 predictedTargetToCurrVec = currPoint - predictedTarget;

            if (currToNextVec.Dot(predictedTargetToNextVec) >= 0 && currToNextVec.Dot(predictedTargetToCurrVec) <= 0)
            {
                // Trace from the segment start (currPoint) to the predictedTarget
                G_Trace(&trace, currPoint.Data(), nullptr, nullptr, predictedTarget.Data(), traceKey, MASK_AISOLID);
                if (trace.fraction == 1.0f)
                {
                    // Target may be hit in air
                    VectorCopy(predictedTarget.Data(), aimParams->fireTarget);
                }
                else
                {
                    // Segment from currPoint to predictedTarget hits solid, use trace end as a predicted target
                    VectorCopy(trace.endpos, aimParams->fireTarget);
                }
                return;
            }
            else
            {
                // Trace from the segment start (currPoint) to the segment end (nextPoint)
                G_Trace(&trace, currPoint.Data(), nullptr, nullptr, nextPoint.Data(), traceKey, MASK_AISOLID);
                if (trace.fraction != 1.0f)
                {
                    // Trajectory segment hits solid, use trace end as a predicted target point and return
                    VectorCopy(trace.endpos, aimParams->fireTarget);
                    return;
                }
            }

            // Test next segment
            currTime = nextTime;
            nextTime += TIME_STEP;
            currPoint = nextPoint;
        }

        // We have tested all segments up to maxSegments and have not found an impact point yet.
        // Approximate the rest of the trajectory as a ray.

        Vec3 currTargetVelocity(targetVelocity);
        currTargetVelocity.Z() -= level.gravity * currTime;

        Vec3 predictedTarget(currPoint);
        if (!PredictProjectileNoClip(fireOrigin, projectileSpeed, predictedTarget.Data(), currTargetVelocity))
            return;

        G_Trace(&trace, currPoint.Data(), nullptr, nullptr, predictedTarget.Data(), traceKey, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            VectorCopy(predictedTarget.Data(), aimParams->fireTarget);
        else
            VectorCopy(trace.endpos, aimParams->fireTarget);
    }
    else
    {
        Vec3 predictedTarget(aimParams->fireTarget);
        if (!PredictProjectileNoClip(Vec3(fireOrigin), projectileSpeed, predictedTarget.Data(), combatTask.EnemyVelocity()))
            return;

        trace_t trace;
        edict_t *traceKey = const_cast<edict_t *>(combatTask.TraceKey());
        G_Trace(&trace, aimParams->fireTarget, nullptr, nullptr, predictedTarget.Data(), traceKey, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            VectorCopy(predictedTarget.Data(), aimParams->fireTarget);
        else
            VectorCopy(trace.endpos, aimParams->fireTarget);
    }
}

inline bool operator!=(const ai_script_weapon_def_t &first, const ai_script_weapon_def_t &second)
{
    return memcmp(&first, &second, sizeof(ai_script_weapon_def_t)) != 0;
}

void Bot::UpdateScriptWeaponsStatus()
{
    int scriptWeaponsNum = GT_asGetScriptWeaponsNum(self->r.client);

    if ((int)scriptWeaponDefs.size() != scriptWeaponsNum)
    {
        scriptWeaponDefs.clear();
        scriptWeaponCooldown.clear();

        for (int weaponNum = 0; weaponNum < scriptWeaponsNum; ++weaponNum)
        {
            ai_script_weapon_def_t weaponDef;
            if (GT_asGetScriptWeaponDef(self->r.client, weaponNum, &weaponDef))
            {
                scriptWeaponDefs.emplace_back(std::move(weaponDef));
                scriptWeaponCooldown.push_back(GT_asGetScriptWeaponCooldown(self->r.client, weaponNum));
            }
        }

        botBrain.ResetCombatTask();
        return;
    }

    bool hasStatusChanged = false;
    for (int weaponNum = 0; weaponNum < scriptWeaponsNum; ++weaponNum)
    {
        ai_script_weapon_def_t actualWeaponDef;
        // Try to retrieve the weapon def
        if (!GT_asGetScriptWeaponDef(self->r.client, weaponNum, &actualWeaponDef))
        {
            // If weapon def retrieval failed, treat the weapon as unavailable by setting a huge cooldown
            scriptWeaponCooldown[weaponNum] = std::numeric_limits<int>::max();
            hasStatusChanged = true;
            continue;
        }

        if (actualWeaponDef != scriptWeaponDefs[weaponNum])
        {
            scriptWeaponDefs[weaponNum] = actualWeaponDef;
            hasStatusChanged = true;
        }

        int cooldown = GT_asGetScriptWeaponCooldown(self->r.client, weaponNum);
        // A weapon became unavailable
        if (cooldown > scriptWeaponCooldown[weaponNum])
        {
            hasStatusChanged = true;
        }
        else
        {
            for (int thresholdMillis = 1000; thresholdMillis >= 0; thresholdMillis -= 500)
            {
                if (scriptWeaponCooldown[weaponNum] > thresholdMillis && cooldown <= thresholdMillis)
                    hasStatusChanged = true;
            }
        }
        scriptWeaponCooldown[weaponNum] = cooldown;
    }

    if (hasStatusChanged)
        botBrain.ResetCombatTask();
}

constexpr float WFAC_GENERIC_PROJECTILE = 300.0f;
constexpr float WFAC_GENERIC_INSTANT = 150.0f;

bool Bot::FireWeapon(bool *didBuiltinAttack)
{
    CombatTask &combatTask = botBrain.combatTask;

    const bool importantShot = combatTask.importantShot;
    // Reset shot importance, it is for a single flick shot and the task is for many frames
    combatTask.importantShot = false;

    int builtinWeapon = self->s.weapon;
    if (builtinWeapon < 0 || builtinWeapon >= WEAP_TOTAL)
        builtinWeapon = 0;

    firedef_t *playerStateFireDef = GS_FiredefForPlayerState(&self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON]);

    StaticVector<GenericFireDef, 2> fireDefHolder;

    const GenericFireDef *builtinFireDef = nullptr;
    const GenericFireDef *scriptFireDef = nullptr;

    if (botBrain.combatTask.CanUseBuiltinWeapon())
    {
        fireDefHolder.emplace_back(GenericFireDef(builtinWeapon, playerStateFireDef, nullptr));
        builtinFireDef = &fireDefHolder.back();
    }

    if (botBrain.combatTask.CanUseScriptWeapon())
    {
        int scriptWeapon = combatTask.ScriptWeapon();
        fireDefHolder.emplace_back(GenericFireDef(scriptWeapon, nullptr, &scriptWeaponDefs[scriptWeapon]));
        scriptFireDef = &fireDefHolder.back();
    }

    AimParams builtinWeaponAimParams;
    AimParams scriptWeaponAimParams;

    if (builtinFireDef)
        builtinFireTargetCache.AdjustAimParams(combatTask, *builtinFireDef, &builtinWeaponAimParams);

    if (scriptFireDef)
        scriptFireTargetCache.AdjustAimParams(combatTask, *scriptFireDef, &scriptWeaponAimParams);

    // Select a weapon that has a priority in adjusting view angles for it
    float accuracy;
    float *fireOrigin, *fireTarget;
    const GenericFireDef *primaryFireDef = nullptr;
    const GenericFireDef *secondaryFireDef = nullptr;
    if (combatTask.ShouldPreferBuiltinWeapon())
    {
        accuracy = builtinWeaponAimParams.EffectiveAccuracy(Skill(), importantShot);
        fireOrigin = builtinWeaponAimParams.fireOrigin;
        fireTarget = builtinWeaponAimParams.fireTarget;
        primaryFireDef = builtinFireDef;
        if (scriptFireDef)
            secondaryFireDef = scriptFireDef;
    }
    else
    {
        accuracy = scriptWeaponAimParams.EffectiveAccuracy(Skill(), importantShot);
        fireOrigin = scriptWeaponAimParams.fireOrigin;
        fireTarget = scriptWeaponAimParams.fireTarget;
        primaryFireDef = scriptFireDef;
        if (builtinFireDef)
            secondaryFireDef = builtinFireDef;
    }

    // Always track enemy with a "crosshair" like a human does in each frame
    LookAtEnemy(accuracy, fireOrigin, fireTarget);

    // Attack only in Think() frames unless a continuousFire is required
    if (ShouldSkipThinkFrame())
    {
        if (!primaryFireDef->IsContinuousFire())
        {
            if (!secondaryFireDef || !secondaryFireDef->IsContinuousFire())
                return false;
        }
    }

    bool didPrimaryAttack = false;
    bool didSecondaryAttack = false;

    if (CheckShot(fireOrigin, fireTarget, primaryFireDef))
        didPrimaryAttack = TryPressAttack(primaryFireDef, builtinFireDef, scriptFireDef, didBuiltinAttack);

    if (secondaryFireDef)
    {
        // Check whether view angles adjusted for the primary weapon are suitable for firing secondary weapon too
        if (CheckShot(fireOrigin, fireTarget, secondaryFireDef))
            didSecondaryAttack = TryPressAttack(secondaryFireDef, builtinFireDef, scriptFireDef, didBuiltinAttack);
    }

    return didPrimaryAttack || didSecondaryAttack;
}

void Bot::FireTargetCache::SetupCoarseFireTarget(const CombatTask &combatTask, vec_t *fire_origin, vec_t *target)
{
    VectorCopy(combatTask.EnemyOrigin().Data(), target);
    VectorAdd(target, (0.5f * (combatTask.EnemyMins() + combatTask.EnemyMaxs())).Data(), target);

    fire_origin[0] = bot->s.origin[0];
    fire_origin[1] = bot->s.origin[1];
    fire_origin[2] = bot->s.origin[2] + bot->viewheight;
}

void Bot::LookAtEnemy(float accuracy, const vec_t *fire_origin, vec_t *target)
{
    target[0] += (random() - 0.5f) * accuracy;
    target[1] += (random() - 0.5f) * accuracy;

    // TODO: Cancel pending turn?
    if (!hasPendingLookAtPoint)
    {
        Vec3 lookAtVector(target);
        lookAtVector -= fire_origin;
        float angularSpeedMultiplier = 0.5f + 0.5f * Skill();
        ChangeAngle(lookAtVector, angularSpeedMultiplier);
    }
}

bool Bot::TryPressAttack(const GenericFireDef *fireDef, const GenericFireDef *builtinFireDef,
                         const GenericFireDef *scriptFireDef, bool *didBuiltinAttack)
{
    if (fireDef == scriptFireDef)
        return GT_asFireScriptWeapon(self->r.client, fireDef->WeaponNum());

    auto weapState = self->r.client->ps.weaponState;
    *didBuiltinAttack = false;
    *didBuiltinAttack |= weapState == WEAPON_STATE_READY;
    *didBuiltinAttack |= weapState == WEAPON_STATE_REFIRE;
    *didBuiltinAttack |= weapState == WEAPON_STATE_REFIRESTRONG;

    return *didBuiltinAttack;
}

bool Bot::CheckShot(const vec3_t fire_origin, const vec3_t target, const GenericFireDef *fireDef)
{
    // Do not shoot enemies that are far from "crosshair" except they are very close
    Vec3 newLookDir(0, 0, 0);
    AngleVectors(self->s.angles, newLookDir.Data(), nullptr, nullptr);

    Vec3 toTarget(target);
    toTarget -= fire_origin;
    toTarget.NormalizeFast();
    float toTargetDotLookDir = toTarget.Dot(newLookDir);

    // 0 on zero range, 1 on distanceFactorBound range
    float directionDistanceFactor = 0.0001f;
    float squareDistanceToTarget = toTarget.SquaredLength();
    if (squareDistanceToTarget > 1)
    {
        float distance = 1.0f / Q_RSqrt(squareDistanceToTarget);
        directionDistanceFactor += BoundedFraction(distance, 450.0f);
    }

    if (fireDef->IsContinuousFire())
    {
        if (toTargetDotLookDir < 0.8f * directionDistanceFactor)
            return false;
    }
    else
    {
        if (toTargetDotLookDir < 0.6f * directionDistanceFactor)
            return false;
    }

    // Do not shoot in enemies that are behind obstacles atm, bot may kill himself easily
    // We test directions factor first because it is cheaper to calculate

    trace_t tr;
    G_Trace(&tr, const_cast<float*>(fire_origin), nullptr, nullptr, (99999.0f * newLookDir + fire_origin).Data(), self, MASK_AISOLID);
    if (tr.fraction == 1.0f)
        return true;

    if (!fireDef->IsContinuousFire())
    {
        if (tr.ent < 1 || !game.edicts[tr.ent].takedamage || game.edicts[tr.ent].movetype == MOVETYPE_PUSH)
            return false;
    }
    if (game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype())
        return false;

    // CheckShot() checks whether we are looking straight on the target, we test just proximity of hit point to the target
    float hitToTargetDist = (Vec3(target) - tr.endpos).LengthFast();
    float proximityDistanceFactor = BoundedFraction(Q_RSqrt(squareDistanceToTarget), 2000.0f);
    return hitToTargetDist < 30.0f + 500.0f * proximityDistanceFactor;
}

void Bot::FireTargetCache::AdjustAimParams(const CombatTask &combatTask, const GenericFireDef &fireDef,
                                             AimParams *aimParams)
{
    SetupCoarseFireTarget(combatTask, aimParams->fireOrigin, aimParams->fireTarget);

    switch (fireDef.AimType())
    {
        case AI_WEAPON_AIM_TYPE_PREDICTION_EXPLOSIVE:
            AdjustPredictionExplosiveAimTypeParams(combatTask, fireDef, aimParams);
        case AI_WEAPON_AIM_TYPE_PREDICTION:
            AdjustPredictionAimTypeParams(combatTask, fireDef, aimParams);
        case AI_WEAPON_AIM_TYPE_DROP:
            AdjustDropAimTypeParams(combatTask, fireDef, aimParams);
        default:
            AdjustInstantAimTypeParams(combatTask, fireDef, aimParams);
    }
}

bool Bot::FireTargetCache::AdjustTargetByEnvironmentTracing(const CombatTask &combatTask, float splashRadius,
                                                              AimParams *aimParams)
{
    trace_t trace;

    float minSqDistance = 999999.0f;
    vec_t nearestPoint[3] = { NAN, NAN, NAN };  // Avoid an "uninitialized" compiler/inspection warning

    edict_t *traceKey = const_cast<edict_t *>(combatTask.TraceKey());
    float *firePoint = const_cast<float*>(aimParams->fireOrigin);

    if (combatTask.IsOnGround())
    {
        Vec3 groundPoint(aimParams->fireTarget);
        groundPoint.Z() += playerbox_stand_maxs[2];
        // Check whether shot to this point is not blocked
        G_Trace(&trace, firePoint, nullptr, nullptr, groundPoint.Data(), const_cast<edict_t*>(bot), MASK_AISOLID);
        if (trace.fraction > 0.999f || combatTask.TraceKey() == game.edicts + trace.ent)
        {
            float skill = bot->ai->botRef->Skill();
            // For mid-skill bots it may be enough. Do not waste cycles.
            if (skill < 0.66f && random() < (1.0f - skill))
            {
                aimParams->fireTarget[2] += playerbox_stand_mins[2];
                return true;
            }

            VectorCopy(groundPoint.Data(), nearestPoint);
            minSqDistance = playerbox_stand_mins[2] * playerbox_stand_mins[2];
        }
    }

    Vec3 toTargetDir(aimParams->fireTarget);
    toTargetDir -= aimParams->fireOrigin;
    float sqDistanceToTarget = toTargetDir.SquaredLength();
    // Not only prevent division by zero, but keep target as-is when it is colliding with bot
    if (sqDistanceToTarget < 16 * 16)
        return false;

    // Normalize to target dir
    toTargetDir *= Q_RSqrt(sqDistanceToTarget);
    Vec3 traceEnd = Vec3(aimParams->fireTarget) + splashRadius * toTargetDir;

    // We hope this function will be called rarely only when somebody wants to load a stripped Q3 AAS.
    // Just trace an environment behind the bot, it is better than it used to be anyway.
    G_Trace(&trace, aimParams->fireTarget, nullptr, nullptr, traceEnd.Data(), traceKey, MASK_AISOLID);
    if (trace.fraction != 1.0f)
    {
        // First check whether an explosion in the point behind may damage the target to cut a trace quickly
        float sqDistance = DistanceSquared(aimParams->fireTarget, trace.endpos);
        if (sqDistance < minSqDistance)
        {
            // trace.endpos will be overwritten
            Vec3 pointBehind(trace.endpos);
            // Check whether shot to this point is not blocked
            G_Trace(&trace, firePoint, nullptr, nullptr, pointBehind.Data(), const_cast<edict_t*>(bot), MASK_AISOLID);
            if (trace.fraction > 0.999f || combatTask.TraceKey() == game.edicts + trace.ent)
            {
                minSqDistance = sqDistance;
                VectorCopy(pointBehind.Data(), nearestPoint);
            }
        }
    }

    // Modify `target` if we have found some close solid point
    if (minSqDistance <= splashRadius)
    {
        VectorCopy(nearestPoint, aimParams->fireTarget);
        return true;
    }
    return false;
}

class FixedBitVector
{
private:
    unsigned size;   // Count of bits this vector is capable to contain
    uint32_t *words; // Actual bits data. We are limited 32-bit words to work fast on 32-bit processors.

public:
    FixedBitVector(unsigned size): size(size)
    {
        words = (uint32_t *)(G_Malloc(size / 8 + 4));
        Clear();
    }

    // These following move-related members are mandatory for intended BitVectorHolder behavior

    FixedBitVector(FixedBitVector &&that)
    {
        words = that.words;
        that.words = nullptr;
    }

    FixedBitVector &operator=(FixedBitVector &&that)
    {
        if (words) G_Free(words);
        words = that.words;
        that.words = nullptr;
        return *this;
    }

    ~FixedBitVector()
    {
        // If not moved
        if (words) G_Free(words);
    }

    void Clear() { memset(words, 0, size / 8 + 4); }

    // TODO: Shift by a variable may be an interpreted instruction on some CPUs

    inline bool IsSet(int bitIndex) const
    {
        unsigned wordIndex = (unsigned)bitIndex / 32;
        unsigned bitOffset = (unsigned)bitIndex - wordIndex * 32;

        return (words[wordIndex] & (1 << bitOffset)) != 0;
    }

    inline void Set(int bitIndex, bool value) const
    {
        unsigned wordIndex = (unsigned)bitIndex / 32;
        unsigned bitOffset = (unsigned)bitIndex - wordIndex * 32;
        if (value)
            words[wordIndex] |= ((unsigned)value << bitOffset);
        else
            words[wordIndex] &= ~((unsigned)value << bitOffset);
    }
};

class BitVectorHolder
{
private:
    StaticVector<FixedBitVector, 1> vectorHolder;
    unsigned size;
    bool hasRegisteredShutdownHook;

    BitVectorHolder(BitVectorHolder &&that) = delete;
public:
    BitVectorHolder(): size(std::numeric_limits<unsigned>::max()), hasRegisteredShutdownHook(false) {}

    FixedBitVector &Get(unsigned size)
    {
        if (this->size != size)
            vectorHolder.clear();

        this->size = size;

        if (vectorHolder.empty())
        {
            vectorHolder.emplace_back(FixedBitVector(size));
        }

        if (!hasRegisteredShutdownHook)
        {
            // Clean up the held bit vector on shutdown
            const auto hook = [&]() { this->vectorHolder.clear(); };
            AiShutdownHooksHolder::Instance()->RegisterHook(hook);
            hasRegisteredShutdownHook = true;
        }

        return vectorHolder[0];
    }
};

static BitVectorHolder visitedFacesHolder;
static BitVectorHolder visitedAreasHolder;

struct PointAndDistance
{
    Vec3 point;
    float distance;
    inline PointAndDistance(const Vec3 &point, float distance): point(point), distance(distance) {}
    inline bool operator<(const PointAndDistance &that) const { return distance < that.distance; }
};

constexpr int MAX_CLOSEST_FACE_POINTS = 8;

static void FindClosestAreasFacesPoints(float splashRadius, const vec3_t target, int startAreaNum,
                                        StaticVector<PointAndDistance, MAX_CLOSEST_FACE_POINTS + 1> &closestPoints)
{
    // Retrieve these instances before the loop
    const AiAasWorld *aasWorld = AiAasWorld::Instance();
    FixedBitVector &visitedFaces = visitedFacesHolder.Get((unsigned)aasWorld->NumFaces());
    FixedBitVector &visitedAreas = visitedAreasHolder.Get((unsigned)aasWorld->NumFaces());

    visitedFaces.Clear();
    visitedAreas.Clear();

    // Actually it is not a limit of a queue capacity but a limit of processed areas number
    constexpr int MAX_FRINGE_AREAS = 16;
    // This is a breadth-first search fringe queue for a BFS through areas
    int areasFringe[MAX_FRINGE_AREAS];

    // Points to a head (front) of the fringe queue.
    int areasFringeHead = 0;
    // Points after a tail (back) of the fringe queue.
    int areasFringeTail = 0;
    // Push the start area to the queue
    areasFringe[areasFringeTail++] = startAreaNum;

    while (areasFringeHead < areasFringeTail)
    {
        const int areaNum = areasFringe[areasFringeHead++];
        visitedAreas.Set(areaNum, true);

        const aas_area_t *area = aasWorld->Areas() + areaNum;

        for (int faceIndexNum = area->firstface; faceIndexNum < area->firstface + area->numfaces; ++faceIndexNum)
        {
            int faceIndex = aasWorld->FaceIndex()[faceIndexNum];

            // If the face has been already processed, skip it
            if (visitedFaces.IsSet(abs(faceIndex)))
                continue;

            // Mark the face as processed
            visitedFaces.Set(abs(faceIndex), true);

            // Get actual face and area behind it by a sign of the faceIndex
            const aas_face_t *face;
            int areaBehindFace;
            if (faceIndex >= 0)
            {
                face = aasWorld->Faces() + faceIndex;
                areaBehindFace = face->backarea;
            }
            else
            {
                face = aasWorld->Faces() - faceIndex;
                areaBehindFace = face->frontarea;
            }

            // Determine a distance from the target to the face
            const aas_plane_t *plane = aasWorld->Planes() + face->planenum;
            const aas_edge_t *anyFaceEdge = aasWorld->Edges() + abs(aasWorld->EdgeIndex()[face->firstedge]);

            Vec3 anyPlanePointToTarget(target);
            anyPlanePointToTarget -= aasWorld->Vertexes()[anyFaceEdge->v[0]];
            const float pointToFaceDistance = anyPlanePointToTarget.Dot(plane->normal);

            // This is the actual loop stop condition.
            // This means that `areaBehindFace` will not be pushed to the fringe queue, and the queue will shrink.
            if (pointToFaceDistance > splashRadius)
                continue;

            // If the area borders with a solid
            if (areaBehindFace == 0)
            {
                Vec3 projectedPoint = Vec3(target) - pointToFaceDistance * Vec3(plane->normal);
                // We are sure we always have a free slot (closestPoints.capacity() == MAX_CLOSEST_FACE_POINTS + 1)
                closestPoints.push_back(PointAndDistance(projectedPoint, pointToFaceDistance));
                std::push_heap(closestPoints.begin(), closestPoints.end());
                // Ensure that we have a free slot by evicting a largest distance point
                // Do this afterward the addition to allow a newly added point win over some old one
                if (closestPoints.size() == closestPoints.capacity())
                {
                    std::pop_heap(closestPoints.begin(), closestPoints.end());
                    closestPoints.pop_back();
                }
            }
            // If the area behind face is not checked yet and areas BFS limit is not reached
            else if (!visitedAreas.IsSet(areaBehindFace) && areasFringeTail != MAX_FRINGE_AREAS)
            {
                // Enqueue `areaBehindFace` to the fringe queue
                areasFringe[areasFringeTail++] = areaBehindFace;
            }
        }
    }

    // `closestPoints` is a heap arranged for quick eviction of the largest value.
    // We have to sort it in ascending order
    std::sort(closestPoints.begin(), closestPoints.end());
}

bool Bot::FireTargetCache::AdjustTargetByEnvironmentWithAAS(const CombatTask &combatTask, float splashRadius,
                                                              int areaNum, AimParams *aimParams)
{
    // We can't just get a closest point from AAS world, it may be blocked for shooting.
    // Also we can't check each potential point for being blocked, tracing is very expensive.
    // Instead we get MAX_CLOSEST_FACE_POINTS best points and for each check whether it is blocked.
    // We hope at least a single point will not be blocked.

    StaticVector<PointAndDistance, MAX_CLOSEST_FACE_POINTS + 1> closestAreaFacePoints;
    FindClosestAreasFacesPoints(splashRadius, aimParams->fireTarget, areaNum, closestAreaFacePoints);

    trace_t trace;

    // On each step get a best point left and check it for being blocked for shooting
    // We assume that FindClosestAreasFacesPoints() returns a sorted array where closest points are first
    for (const PointAndDistance &pointAndDistance: closestAreaFacePoints)
    {
        float *traceEnd = const_cast<float*>(pointAndDistance.point.Data());
        edict_t *passent = const_cast<edict_t*>(bot);
        G_Trace(&trace, aimParams->fireOrigin, nullptr, nullptr, traceEnd, passent, MASK_AISOLID);

        if (trace.fraction > 0.999f || combatTask.TraceKey() == game.edicts + trace.ent)
        {
            VectorCopy(traceEnd, aimParams->fireTarget);
            return true;
        }
    }

    return false;
}

bool Bot::FireTargetCache::AdjustTargetByEnvironment(const CombatTask &combatTask, float splashRaidus,
                                                       AimParams *aimParams)
{
    int targetAreaNum = 0;
    // Reject AAS worlds that look like stripped
    if (AiAasWorld::Instance()->NumFaces() > 512)
        targetAreaNum = AiAasWorld::Instance()->FindAreaNum(aimParams->fireTarget);

    if (targetAreaNum)
        return AdjustTargetByEnvironmentWithAAS(combatTask, splashRaidus, targetAreaNum, aimParams);

    return AdjustTargetByEnvironmentTracing(combatTask, splashRaidus, aimParams);
}

void Bot::FireTargetCache::AdjustPredictionExplosiveAimTypeParams(const CombatTask &combatTask,
                                                                    const GenericFireDef &fireDef,
                                                                    AimParams *aimParams)
{
    bool wasCached = cachedFireTarget.IsValidFor(combatTask);
    GetPredictedTargetOrigin(combatTask, fireDef.ProjectileSpeed(), aimParams);
    // If new generic predicted target origin has been computed, adjust it for target environment
    if (!wasCached)
    {
        // First, modify temporary `target` value
        AdjustTargetByEnvironment(combatTask, fireDef.SplashRadius(), aimParams);
        // Copy modified `target` value to cached value
        cachedFireTarget.origin = Vec3(aimParams->fireTarget);
    }
    // Accuracy for air rockets is worse anyway (movement prediction in gravity field is approximate)
    aimParams->suggestedBaseAccuracy = 1.3f * (1.01f - bot->ai->botRef->Skill()) * WFAC_GENERIC_PROJECTILE;
}

void Bot::FireTargetCache::AdjustPredictionAimTypeParams(const CombatTask &combatTask,
                                                           const GenericFireDef &fireDef, AimParams *aimParams)
{
    if (fireDef.IsBuiltin() && fireDef.WeaponNum() == WEAP_PLASMAGUN)
        aimParams->suggestedBaseAccuracy = 0.5f * WFAC_GENERIC_PROJECTILE * (1.0f - bot->ai->botRef->Skill());
    else
        aimParams->suggestedBaseAccuracy = WFAC_GENERIC_PROJECTILE;

    GetPredictedTargetOrigin(combatTask, fireDef.ProjectileSpeed(), aimParams);
}

void Bot::FireTargetCache::AdjustDropAimTypeParams(const CombatTask &combatTask,
                                                     const GenericFireDef &fireDef, AimParams *aimParams)
{
    bool wasCached = cachedFireTarget.IsValidFor(combatTask);
    GetPredictedTargetOrigin(combatTask, fireDef.ProjectileSpeed(), aimParams);
    // If new generic predicted target origin has been computed, adjust it for gravity (changes will be cached)
    if (!wasCached)
    {
        // It is not very accurate but satisfactory
        Vec3 fireOriginToTarget = Vec3(aimParams->fireTarget) - aimParams->fireOrigin;
        Vec3 fireOriginToTarget2D(fireOriginToTarget.X(), fireOriginToTarget.Y(), 0);
        float squareDistance2D = fireOriginToTarget2D.SquaredLength();
        if (squareDistance2D > 0)
        {
            Vec3 velocity2DVec(fireOriginToTarget);
            velocity2DVec.NormalizeFast();
            velocity2DVec *= fireDef.ProjectileSpeed();
            velocity2DVec.Z() = 0;
            float squareVelocity2D = velocity2DVec.SquaredLength();
            if (squareVelocity2D > 0)
            {
                float distance2D = 1.0f / Q_RSqrt(squareDistance2D);
                float velocity2D = 1.0f / Q_RSqrt(squareVelocity2D);
                float time = distance2D / velocity2D;
                float height = std::max(0.0f, 0.5f * level.gravity * time * time - 32.0f);
                // Modify both cached and temporary values
                cachedFireTarget.origin.Z() += height;
                aimParams->fireTarget[2] += height;
            }
        }
    }

    // This kind of weapons is not precise by its nature, do not add any more noise.
    aimParams->suggestedBaseAccuracy = 0.3f * (1.01f - bot->ai->botRef->Skill()) * WFAC_GENERIC_PROJECTILE;
}

void Bot::FireTargetCache::AdjustInstantAimTypeParams(const CombatTask &combatTask,
                                                      const GenericFireDef &fireDef, AimParams *aimParams)
{
    if (fireDef.IsBuiltin())
    {
        float skill = bot->ai->botRef->Skill();
        // It is affected by bot view latency (lastSeenPosition() + finite yaw/pitch speed) enough, decrease aim error
        if (fireDef.WeaponNum() == WEAP_ELECTROBOLT)
            aimParams->suggestedBaseAccuracy = (1.0f - skill) * WFAC_GENERIC_INSTANT;
        else if (fireDef.WeaponNum() == WEAP_LASERGUN)
            aimParams->suggestedBaseAccuracy = 0.33f * WFAC_GENERIC_INSTANT * (1.0f - skill);
    }

    aimParams->suggestedBaseAccuracy = WFAC_GENERIC_INSTANT;
}
