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
    VectorAdd(target, move.data(), target);
    return true;
}

void Bot::GetPredictedTargetOrigin(const vec3_t fireOrigin, float projSpeed, vec_t *target)
{
    if (Skill() < 0.33f || IsEnemyStatic())
        return;

    // Check whether we are shooting the same enemy and cached predicted origin is not outdated
    if (!HasCachedTargetOrigin())
    {
        PredictProjectileShot(fireOrigin, projSpeed, target, EnemyVelocity().data(), true);
        cachedPredictedTargetInstanceId = EnemyInstanceId();
        cachedPredictedTargetValidUntil = level.time + 66;
    }
}

//==========================================
// BOT_DMclass_PredictProjectileShot
// predict target movement
//==========================================
void Bot::PredictProjectileShot(
    const vec3_t fire_origin, float projectileSpeed, vec3_t target, const vec3_t targetVelocity, bool applyTargetGravity)
{
    if (projectileSpeed <= 0.0f)
        return;

    // Copy for convenience
    Vec3 fireOrigin(fire_origin);

    if (applyTargetGravity)
    {
        // Aside from solving quite complicated system of equations that involve acceleration,
        // we have to predict target collision with map environment.
        // To solve it, we approximate target trajectory as a polyline
        // that consists of linear segments plus an end ray from last segment point to infinity.
        // We assume that target velocity is constant withing bounds of a segment.

        constexpr float TIME_STEP = 0.15f; // Time is in seconds

        Vec3 currPoint(target);
        float currTime = 0.0f;
        float nextTime = TIME_STEP;

        trace_t trace;
        edict_t *traceKey = const_cast<edict_t*>(EnemyTraceKey());

        const int maxSegments = 2 + (int)(2.1 * Skill());

        for (int i = 0; i < maxSegments; ++i)
        {
            Vec3 nextPoint(target);
            nextPoint.x() += targetVelocity[0] * nextTime;
            nextPoint.y() += targetVelocity[1] * nextTime;
            nextPoint.z() += targetVelocity[2] * nextTime - 0.5f * level.gravity * nextTime * nextTime;

            // We assume that target has the same velocity as currPoint on a [currPoint, nextPoint] segment
            Vec3 currTargetVelocity(targetVelocity);
            currTargetVelocity.z() -= level.gravity * currTime;

            // TODO: Projectile speed used in PredictProjectileNoClip() needs correction
            // We can't offset fire origin since we do not know direction to target yet
            // Instead, increase projectile speed used in calculations according to currTime
            // Exact formula is to be proven yet
            Vec3 predictedTarget(currPoint);
            if (!PredictProjectileNoClip(fireOrigin, projectileSpeed, predictedTarget.data(), currTargetVelocity))
                return;

            // Check whether predictedTarget is within [currPoint, nextPoint]
            // where extrapolation that use currTargetVelocity is assumed to be valid.
            Vec3 currToNextVec = nextPoint - currPoint;
            Vec3 predictedTargetToNextVec = nextPoint - predictedTarget;
            Vec3 predictedTargetToCurrVec = currPoint - predictedTarget;

            if (currToNextVec.Dot(predictedTargetToNextVec) >= 0 && currToNextVec.Dot(predictedTargetToCurrVec) <= 0)
            {
                // Trace from the segment start (currPoint) to the predictedTarget
                G_Trace(&trace, currPoint.data(), nullptr, nullptr, predictedTarget.data(), traceKey, MASK_AISOLID);
                if (trace.fraction == 1.0f)
                {
                    // Target may be hit in air
                    VectorCopy(predictedTarget.data(), target);
                }
                else
                {
                    // Segment from currPoint to predictedTarget hits solid, use trace end as a predicted target
                    VectorCopy(trace.endpos, target);
                }
                return;
            }
            else
            {
                // Trace from the segment start (currPoint) to the segment end (nextPoint)
                G_Trace(&trace, currPoint.data(), nullptr, nullptr, nextPoint.data(), traceKey, MASK_AISOLID);
                if (trace.fraction != 1.0f)
                {
                    // Trajectory segment hits solid, use trace end as a predicted target point and return
                    VectorCopy(trace.endpos, target);
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
        currTargetVelocity.z() -= level.gravity * currTime;

        Vec3 predictedTarget(currPoint);
        if (!PredictProjectileNoClip(fireOrigin, projectileSpeed, predictedTarget.data(), currTargetVelocity))
            return;

        G_Trace(&trace, currPoint.data(), nullptr, nullptr, predictedTarget.data(), traceKey, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            VectorCopy(predictedTarget.data(), target);
        else
            VectorCopy(trace.endpos, target);
    }
    else
    {
        Vec3 predictedTarget(target);
        if (!PredictProjectileNoClip(Vec3(fireOrigin), projectileSpeed, predictedTarget.data(), Vec3(targetVelocity)))
            return;

        trace_t trace;
        edict_t *traceKey = const_cast<edict_t *>(EnemyTraceKey());
        G_Trace(&trace, target, nullptr, nullptr, predictedTarget.data(), traceKey, MASK_AISOLID);
        if (trace.fraction == 1.0f)
            VectorCopy(predictedTarget.data(), target);
        else
            VectorCopy(trace.endpos, target);
    }
}

constexpr float WFAC_GENERIC_PROJECTILE = 300.0f;
constexpr float WFAC_GENERIC_INSTANT = 150.0f;

//==========================================
// BOT_DMclass_FireWeapon
// Fire if needed
//==========================================
bool Bot::FireWeapon()
{
    const bool importantShot = botBrain.combatTask.importantShot;
    // Reset shot importance, it is for a single flick shot and the task is for many frames
    botBrain.combatTask.importantShot = false;

    if (botBrain.combatTask.Empty())
        return false;

    bool continuousFire = false;
    if (self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN || self->s.weapon == WEAP_MACHINEGUN)
        continuousFire = true;

    const firedef_t *firedef = GS_FiredefForPlayerState(&self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON]);

    int weapon = self->s.weapon;
    if (weapon < 0 || weapon >= WEAP_TOTAL)
        weapon = 0;

    if (!firedef)
        return false;

    // Always try to to track enemy with a "crosshair" like a human does

    vec3_t target, fire_origin;
    SetupCoarseFireTarget(fire_origin, target);

    // AdjustTarget() uses caching, so it is not expensive to call it each frame
    float wfac = AdjustTarget(weapon, firedef, fire_origin, target);
    wfac = 25 + wfac * (1.0f - 0.75f * Skill());
    if (importantShot && Skill() > 0.33f)
        wfac *= (1.13f - Skill());

    // Always track enemy with a "crosshair" like a human does in each frame
    LookAtEnemy(wfac, fire_origin, target);

    // Attack only in Think() frames unless a continuousFire is required
    if (!continuousFire && ShouldSkipThinkFrame())
        return false;

    if (CheckShot(fire_origin, target))
        return TryPressAttack();

    return false;
}

void Bot::SetupCoarseFireTarget(vec_t *fire_origin, vec_t *target)
{
    VectorCopy(EnemyOrigin().data(), target);
    VectorAdd(target, (0.5f * (EnemyMins() + EnemyMaxs())).data(), target);

    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;
}

bool Bot::TryPressAttack()
{
    const auto weapState = self->r.client->ps.weaponState;
    return weapState == WEAPON_STATE_READY || weapState == WEAPON_STATE_REFIRE || weapState == WEAPON_STATE_REFIRESTRONG;
}

void Bot::LookAtEnemy(float wfac, const vec_t *fire_origin, vec_t *target)
{
    target[0] += (random() - 0.5f) * wfac;
    target[1] += (random() - 0.5f) * wfac;

    // TODO: Cancel pending turn?
    if (!hasPendingLookAtPoint)
    {
        Vec3 lookAtVector(target);
        lookAtVector -= fire_origin;
        float angularSpeedMultiplier = 0.5f + 0.5f * Skill();
        ChangeAngle(lookAtVector, angularSpeedMultiplier);
    }
}

bool Bot::CheckShot(const vec3_t fire_origin, const vec3_t target)
{
    // Do not shoot enemies that are far from "crosshair" except they are very close
    Vec3 newLookDir(0, 0, 0);
    AngleVectors(self->s.angles, newLookDir.data(), nullptr, nullptr);

    Vec3 toTarget(target);
    toTarget -= fire_origin;
    float toTargetDotLookDir = toTarget.Dot(newLookDir);

    // 0 on zero range, 1 on distanceFactorBound range
    float directionDistanceFactor = 0.0001f;
    float squareDistanceToTarget = toTarget.SquaredLength();
    if (squareDistanceToTarget > 1)
    {
        directionDistanceFactor += BoundedFraction(Q_RSqrt(squareDistanceToTarget), 450.0f);
    }

    if (toTargetDotLookDir < 0.8f * directionDistanceFactor)
        return false;

    // Do not shoot in enemies that are behind obstacles atm, bot may kill himself easily
    // We test directions factor first because it is cheaper to calculate

    trace_t tr;
    G_Trace(&tr, const_cast<float*>(fire_origin), nullptr, nullptr, (99999.0f * newLookDir + fire_origin).data(), self, MASK_AISOLID);
    if (tr.fraction == 1.0f)
        return true;

    // Same as in CheckShot()
    if (tr.ent < 1 || !game.edicts[tr.ent].takedamage || game.edicts[tr.ent].movetype == MOVETYPE_PUSH)
        return false;
    if (game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype())
        return false;

    // CheckShot() checks whether we are looking straight on the target, we test just proximity of hit point to the target
    float hitToTargetDist = (Vec3(target) - tr.endpos).LengthFast();
    float proximityDistanceFactor = BoundedFraction(Q_RSqrt(squareDistanceToTarget), 2000.0f);
    return hitToTargetDist < 30.0f + 500.0f * proximityDistanceFactor;
}

float Bot::AdjustTarget(int weapon, const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{

    switch (AIWeapons[weapon].aimType)
    {
        case AI_AIMSTYLE_PREDICTION_EXPLOSIVE:
            return AdjustPredictionExplosiveAimStyleTarget(firedef, fire_origin, target);
        case AI_AIMSTYLE_PREDICTION:
            return AdjustPredictionAimStyleTarget(firedef, fire_origin, target);
        case AI_AIMSTYLE_DROP:
            return AdjustDropAimStyleTarget(firedef, fire_origin, target);
        default:
            return AdjustInstantAimStyleTarget(firedef, fire_origin, target);
    }
}

bool Bot::AdjustTargetByEnvironmentTracing(float splashRadius, const vec3_t fire_origin, vec3_t target)
{
    trace_t trace;

    float minSqDistance = 999999.0f;
    vec_t nearestPoint[3] = { NAN, NAN, NAN };  // Avoid an "uninitialized" compiler/inspection warning

    edict_t *traceKey = const_cast<edict_t *>(EnemyTraceKey());
    float *firePoint = const_cast<float*>(fire_origin);

    if (IsEnemyOnGround())
    {
        Vec3 groundPoint(target);
        groundPoint.z() += playerbox_stand_maxs[2];
        // Check whether shot to this point is not blocked
        G_Trace(&trace, firePoint, nullptr, nullptr, groundPoint.data(), self, MASK_AISOLID);
        if (trace.fraction > 0.999f || EnemyTraceKey() == game.edicts + trace.ent)
        {
            // For mid-skill bots it may be enough. Do not waste cycles.
            if (Skill() < 0.66f && random() < (1.0f - Skill()))
            {
                target[2] += playerbox_stand_mins[2];
                return true;
            }

            VectorCopy(groundPoint.data(), nearestPoint);
            minSqDistance = playerbox_stand_mins[2] * playerbox_stand_mins[2];
        }
    }

    Vec3 toTargetDir(target);
    toTargetDir -= fire_origin;
    float sqDistanceToTarget = toTargetDir.SquaredLength();
    // Not only prevent division by zero, but keep target as-is when it is colliding with bot
    if (sqDistanceToTarget < 16 * 16)
        return false;

    // Normalize to target dir
    toTargetDir *= Q_RSqrt(sqDistanceToTarget);
    Vec3 traceEnd = Vec3(target) + splashRadius * toTargetDir;

    // We hope this function will be called rarely only when somebody wants to load a stripped Q3 AAS.
    // Just trace an environment behind the bot, it is better than it used to be anyway.
    G_Trace(&trace, target, nullptr, nullptr, traceEnd.data(), traceKey, MASK_AISOLID);
    if (trace.fraction != 1.0f)
    {
        // First check whether an explosion in the point behind may damage the target to cut a trace quickly
        float sqDistance = DistanceSquared(target, trace.endpos);
        if (sqDistance < minSqDistance)
        {
            // trace.endpos will be overwritten
            Vec3 pointBehind(trace.endpos);
            // Check whether shot to this point is not blocked
            G_Trace(&trace, firePoint, nullptr, nullptr, pointBehind.data(), self, MASK_AISOLID);
            if (trace.fraction > 0.999f || EnemyTraceKey() == game.edicts + trace.ent)
            {
                minSqDistance = sqDistance;
                VectorCopy(pointBehind.data(), nearestPoint);
            }
        }
    }

    // Modify `target` if we have found some close solid point
    if (minSqDistance <= splashRadius)
    {
        VectorCopy(nearestPoint, target);
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
    FixedBitVector &visitedFaces = visitedFacesHolder.Get((unsigned)aasworld.numfaces);
    FixedBitVector &visitedAreas = visitedAreasHolder.Get((unsigned)aasworld.numareas);

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

        const aas_area_t *area = aasworld.areas + areaNum;

        for (int faceIndexNum = area->firstface; faceIndexNum < area->firstface + area->numfaces; ++faceIndexNum)
        {
            int faceIndex = aasworld.faceindex[faceIndexNum];

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
                face = aasworld.faces + faceIndex;
                areaBehindFace = face->backarea;
            }
            else
            {
                face = aasworld.faces - faceIndex;
                areaBehindFace = face->frontarea;
            }

            // Determine a distance from the target to the face
            const aas_plane_t *plane = aasworld.planes + face->planenum;
            const aas_edge_t *anyFaceEdge = aasworld.edges + abs(aasworld.edgeindex[face->firstedge]);

            Vec3 anyPlanePointToTarget(target);
            anyPlanePointToTarget -= aasworld.vertexes[anyFaceEdge->v[0]];
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

bool Bot::AdjustTargetByEnvironmentWithAAS(float splashRadius, const vec3_t fire_origin, vec3_t target, int areaNum)
{
    // We can't just get a closest point from AAS world, it may be blocked for shooting.
    // Also we can't check each potential point for being blocked, tracing is very expensive.
    // Instead we get MAX_CLOSEST_FACE_POINTS best points and for each check whether it is blocked.
    // We hope at least a single point will not be blocked.

    StaticVector<PointAndDistance, MAX_CLOSEST_FACE_POINTS + 1> closestAreaFacePoints;
    FindClosestAreasFacesPoints(splashRadius, target, areaNum, closestAreaFacePoints);

    trace_t trace;

    // On each step get a best point left and check it for being blocked for shooting
    // We assume that FindClosestAreasFacesPoints() returns a sorted array where closest points are first
    for (const PointAndDistance &pointAndDistance: closestAreaFacePoints)
    {
        float *traceEnd = const_cast<float*>(pointAndDistance.point.data());
        float *traceStart = const_cast<float*>(fire_origin);
        G_Trace(&trace, traceStart, nullptr, nullptr, traceEnd, self, MASK_AISOLID);

        if (trace.fraction > 0.999f || EnemyTraceKey() == game.edicts + trace.ent)
        {
            VectorCopy(traceEnd, target);
            return true;
        }
    }

    return false;
}

bool Bot::AdjustTargetByEnvironment(const firedef_t *firedef, const vec3_t fire_origin, vec3_t target)
{
    int targetAreaNum = 0;
    // Reject AAS worlds that look like stripped
    if (aasworld.numfaces > 512)
    {
        targetAreaNum = AAS_PointAreaNum(target);
        if (!targetAreaNum)
        {
            // Try some close random point (usually Z should be greater)
            Vec3 offsetTarget(target);
            offsetTarget += Vec3(-4.0f + 8.0f * random(),  -4.0f + 8.0f * random(), 4.0f + 4.0f * random());
            targetAreaNum = AAS_PointAreaNum(offsetTarget.data());
        }
    }

    if (targetAreaNum)
        return AdjustTargetByEnvironmentWithAAS(firedef->splash_radius, fire_origin, target, targetAreaNum);

    return AdjustTargetByEnvironmentTracing(firedef->splash_radius, fire_origin, target);
}

float Bot::AdjustPredictionExplosiveAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target)
{
    bool wasCached = HasCachedTargetOrigin();
    GetPredictedTargetOrigin(fire_origin, firedef->speed, target);
    // If new generic predicted target origin has been computed, adjust it for target environment
    if (!wasCached)
    {
        // First, modify temporary `target` value
        AdjustTargetByEnvironment(firedef, fire_origin, target);
        // Copy modified `target` value to cached value
        cachedPredictedTargetOrigin = Vec3(target);
    }
    // Accuracy for air rockets is worse anyway (movement prediction in gravity field is approximate)
    return 1.3f * (1.01f - Skill()) * WFAC_GENERIC_PROJECTILE;
}

float Bot::AdjustPredictionAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    float wfac;
    if (self->s.weapon == WEAP_PLASMAGUN)
        wfac = WFAC_GENERIC_PROJECTILE * 0.5f;
    else
        wfac = WFAC_GENERIC_PROJECTILE;

    GetPredictedTargetOrigin(fire_origin, firedef->speed, target);
    return wfac;
}

float Bot::AdjustDropAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    bool wasCached = HasCachedTargetOrigin();
    GetPredictedTargetOrigin(fire_origin, firedef->speed, target);
    // If new generic predicted target origin has been computed, adjust it for gravity (changes will be cached)
    if (!wasCached)
    {
        // It is not very accurate but satisfactory
        Vec3 fireOriginToTarget = Vec3(target) - fire_origin;
        Vec3 fireOriginToTarget2D(fireOriginToTarget.x(), fireOriginToTarget.y(), 0);
        float squareDistance2D = fireOriginToTarget2D.SquaredLength();
        if (squareDistance2D > 0)
        {
            Vec3 velocity2DVec(fireOriginToTarget);
            velocity2DVec.NormalizeFast();
            velocity2DVec *= firedef->speed;
            velocity2DVec.z() = 0;
            float squareVelocity2D = velocity2DVec.SquaredLength();
            if (squareVelocity2D > 0)
            {
                float distance2D = 1.0f / Q_RSqrt(squareDistance2D);
                float velocity2D = 1.0f / Q_RSqrt(squareVelocity2D);
                float time = distance2D / velocity2D;
                float height = std::max(0.0f, 0.5f * level.gravity * time * time - 32.0f);
                // Modify both cached and temporary values
                cachedPredictedTargetOrigin.z() += height;
                target[2] += height;
            }
        }
    }

    // This kind of weapons is not precise by its nature, do not add any more noise.
    return 0.3f * (1.01f - Skill()) * WFAC_GENERIC_PROJECTILE;
}

float Bot::AdjustInstantAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    if( self->s.weapon == WEAP_ELECTROBOLT )
        return WFAC_GENERIC_INSTANT;
    if( self->s.weapon == WEAP_LASERGUN )
        return WFAC_GENERIC_INSTANT * 1.5f;

    return WFAC_GENERIC_INSTANT;
}
