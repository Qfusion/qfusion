#include "bot.h"
#include "ai_local.h"
#include "../../gameshared/q_comref.h"

//==========================================
// BOT_DMclass_CheckShot
// Checks if shot is blocked (doesn't verify it would hit)
//==========================================
bool Bot::CheckShot(const vec3_t point)
{
    trace_t tr;
    vec3_t start, forward, right, offset;

    AngleVectors( self->r.client->ps.viewangles, forward, right, NULL );

    VectorSet( offset, 0, 0, self->viewheight );
    G_ProjectSource( self->s.origin, offset, forward, right, start );

    // blocked, don't shoot
    G_Trace( &tr, start, vec3_origin, vec3_origin, const_cast<vec_t*>(point), self, MASK_AISOLID );
    if( tr.fraction < 0.8f )
    {
        if( tr.ent < 1 || !game.edicts[tr.ent].takedamage || game.edicts[tr.ent].movetype == MOVETYPE_PUSH )
            return false;

        // check if the player we found is at our team
        if( game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype() )
            return false;
    }

    return true;
}

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
            nextPoint.z() += targetVelocity[2] * nextTime - GRAVITY * nextTime * nextTime;

            // We assume that target has the same velocity as currPoint on a [currPoint, nextPoint] segment
            Vec3 currTargetVelocity(targetVelocity);
            currTargetVelocity.z() -= GRAVITY * currTime;

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
        currTargetVelocity.z() -= GRAVITY * currTime;

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

    // Skip shooting in non-think frames unless continuous fire is required
    if (!continuousFire && ShouldSkipThinkFrame())
        return false;

    const firedef_t *firedef = GS_FiredefForPlayerState(&self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON]);

    int weapon = self->s.weapon;
    if (weapon < 0 || weapon >= WEAP_TOTAL)
        weapon = 0;

    if (!firedef)
        return false;

    vec3_t target, fire_origin;
    SetupCoarseFireTarget(fire_origin, target);

    bool isInFront, mayHitApriori;
    CheckEnemyInFrontAndMayBeHit(target, &isInFront, &mayHitApriori);

    if(!continuousFire && !(isInFront && mayHitApriori))
        return false;

    float wfac = AdjustTarget(weapon, firedef, fire_origin, target);
    wfac = 25 + wfac * (1.0f - 0.75f * Skill());
    if (importantShot && Skill() > 0.33f)
        wfac *= (1.13f - Skill());

    // mayHitReally means that enemy is quite close to "crosshair" to press attack,
    // the main purpose is to prevent shooting in wall when real look dir is not close to ideal one
    bool mayHitReally = LookAtEnemy(wfac, fire_origin, target);
    if (mayHitReally)
        return TryPressAttack(importantShot);

    return false;

    /*
    if (nav.debugMode && bot_showcombat->integer)
    {
        if (AimEnemy())
        {
            const edict_t *enemy = CombatTask().aimEnemy->ent;
            const char *enemyName = enemy->r.client ? enemy->r.client->netname : enemy->classname;
            G_PrintChasersf(self, "%s: attacking %s\n", self->ai->pers.netname, enemyName);
        }
    }
    */
}

void Bot::SetupCoarseFireTarget(vec_t *fire_origin, vec_t *target)
{
    VectorCopy(EnemyOrigin().data(), target);
    VectorAdd(target, (0.5f * (EnemyMins() + EnemyMaxs())).data(), target);

    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;
}

void Bot::CheckEnemyInFrontAndMayBeHit(const vec3_t target, bool *isInFront, bool *mayHit)
{
    // TODO: Inline Ai::IsInFront() contents or change Ai::IsInFront() signature
    edict_t dummyEnt;
    VectorCopy(target, dummyEnt.s.origin);
    *isInFront = Ai::IsInFront(&dummyEnt);
    *mayHit = CheckShot(target);
}

bool Bot::TryPressAttack(bool importantShot)
{
    const auto weapState = self->r.client->ps.weaponState;
    if (weapState != WEAPON_STATE_READY && weapState != WEAPON_STATE_REFIRE && weapState != WEAPON_STATE_REFIRESTRONG)
        return false;

    float firedelay;
    // in continuous fire weapons don't add delays
    if (self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN)
        firedelay = 1.0f;
    else
        firedelay = 0.95f + 0.55f * Skill() - 1.5f * random();

    if (importantShot)
        firedelay += 0.45f * Skill();

    return firedelay > 0;
}

bool Bot::LookAtEnemy(float wfac, const vec_t *fire_origin, vec_t *target)
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

float Bot::AdjustPredictionExplosiveAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target)
{
    GetPredictedTargetOrigin(fire_origin, firedef->speed, target);

    float wfac = WFAC_GENERIC_PROJECTILE * 1.3f;

    // TODO: Disabled to get rid of self->enemy until sophisticated prediction code will be introduced
    /*
    if (GetCombatTask().aimEnemy)
    {
        // aim to the feet when enemy isn't higher
        if (fire_origin[2] > (target[2] + (self->enemy->r.mins[2] * 0.8)))
        {
            vec3_t checktarget;
            VectorSet(checktarget,
                      self->enemy->s.origin[0],
                      self->enemy->s.origin[1],
                      self->enemy->s.origin[2] + self->enemy->r.mins[2] + 4);

            trace_t trace;
            G_Trace(&trace, fire_origin, vec3_origin, vec3_origin, checktarget, self, MASK_SHOT);
            if (trace.fraction == 1.0f || (trace.ent > 0 && game.edicts[trace.ent].takedamage))
                VectorCopy(checktarget, target);
        }
        else if (!IsStep(self->enemy))
            wfac *= 2.5; // more imprecise for air rockets
    }
     */

    return wfac;
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
    //jalToDo
    float wfac = WFAC_GENERIC_PROJECTILE;
    GetPredictedTargetOrigin(fire_origin, firedef->speed, target);
    return wfac;
}

float Bot::AdjustInstantAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    if( self->s.weapon == WEAP_ELECTROBOLT )
        return WFAC_GENERIC_INSTANT;
    if( self->s.weapon == WEAP_LASERGUN )
        return WFAC_GENERIC_INSTANT * 1.5f;

    return WFAC_GENERIC_INSTANT;
}
