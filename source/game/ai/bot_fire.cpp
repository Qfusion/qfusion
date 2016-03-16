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

    if( random() > self->ai->pers.cha.firerate )
        return false;

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

//==========================================
// BOT_DMclass_PredictProjectileShot
// predict target movement
//==========================================
void Bot::PredictProjectileShot(const vec3_t fire_origin, float projectile_speed, vec3_t target, const vec3_t target_velocity)
{
    vec3_t predictedTarget;
    vec3_t targetMovedir;
    float targetSpeed;
    float predictionTime;
    float distance;
    trace_t	trace;
    int contents;

    if( projectile_speed <= 0.0f )
        return;

    targetSpeed = VectorNormalize2( target_velocity, targetMovedir );

    // ok, this is not going to be 100% precise, since we will find the
    // time our projectile will take to travel to enemy's CURRENT position,
    // and them find enemy's position given his CURRENT velocity and his CURRENT dir
    // after prediction time. The result will be much better if the player
    // is moving to the sides (relative to us) than in depth (relative to us).
    // And, of course, when the player moves in a curve upwards it will totally miss (ie, jumping).

    // but in general it does a great job, much better than any human player :)

    distance = DistanceFast( fire_origin, target );
    predictionTime = distance/projectile_speed;
    VectorMA( target, predictionTime*targetSpeed, targetMovedir, predictedTarget );

    // if this position is inside solid, try finding a position at half of the prediction time
    contents = G_PointContents( predictedTarget );
    if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
    {
        VectorMA( target, ( predictionTime * 0.5f )*targetSpeed, targetMovedir, predictedTarget );
        contents = G_PointContents( predictedTarget );
        if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
            return; // INVALID
    }

    // if we can see this point, we use it, otherwise we keep the current position
    G_Trace( &trace, const_cast<float*>(fire_origin), vec3_origin, vec3_origin, predictedTarget, self, MASK_SHOT );
    if( trace.fraction == 1.0f || ( trace.ent && game.edicts[trace.ent].takedamage ) )
        VectorCopy( predictedTarget, target );
}

constexpr float WFAC_GENERIC_PROJECTILE = 300.0f;
constexpr float WFAC_GENERIC_INSTANT = 150.0f;

//==========================================
// BOT_DMclass_FireWeapon
// Fire if needed
//==========================================
bool Bot::FireWeapon(usercmd_t *ucmd)
{
    const bool importantShot = enemyPool.combatTask.importantShot;
    // Reset shot importance, it is for a single flick shot and the task is for many frames
    enemyPool.combatTask.importantShot = false;

    if (enemyPool.combatTask.Empty())
        return false;

    const firedef_t *firedef = GS_FiredefForPlayerState(&self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON]);

    int weapon = self->s.weapon;
    if (weapon < 0 || weapon >= WEAP_TOTAL)
        weapon = 0;

    if (!firedef)
        return false;

    vec3_t target, fire_origin;
    SetupCoarseFireTarget(fire_origin, target);

    bool continuous_fire = false;
    if (self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN)
        continuous_fire = true;

    bool isInFront, mayHitApriori;
    CheckEnemyInFrontAndMayBeHit(target, &isInFront, &mayHitApriori);

    if(!continuous_fire && !(isInFront && mayHitApriori))
        return false;

    float wfac = AdjustTarget(weapon, firedef, fire_origin, target);
    wfac = 25 + wfac * (1.0f - 0.75f * Skill());
    if (importantShot && Skill() > 0.33f)
        wfac *= (1.13f - Skill());

    // mayHitReally means that enemy is quite close to "crosshair" to press attack,
    // the main purpose is to prevent shooting in wall when real look dir is not close to ideal one
    bool mayHitReally = LookAtEnemy(wfac, fire_origin, target);
    if (mayHitReally)
        TryPressAttack(ucmd, importantShot);

    if (nav.debugMode && bot_showcombat->integer)
    {
        if (AimEnemy())
        {
            const edict_t *enemy = CombatTask().aimEnemy->ent;
            const char *enemyName = enemy->r.client ? enemy->r.client->netname : enemy->classname;
            G_PrintChasersf(self, "%s: attacking %s\n", self->ai->pers.netname, enemyName);
        }
    }
    return true;
}

void Bot::SetupCoarseFireTarget(vec_t *fire_origin, vec_t *target)
{
    if (AimEnemy())
    {
        const edict_t *enemy = AimEnemy()->ent;
        for (int i = 0; i < 3; i++)
        {
            target[i] = enemy->s.origin[i] + (0.5f * (enemy->r.maxs[i] + enemy->r.mins[i]));
        }
    }
    else
    {
        VectorCopy(SpamSpot().data(), target);
    }

    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;
}

void Bot::CheckEnemyInFrontAndMayBeHit(const vec3_t target, bool *isInFront, bool *mayHit)
{
    edict_t *targetEnt;
    edict_t dummyEnt;
    if (CombatTask().aimEnemy)
    {
        targetEnt = const_cast<edict_t *>(AimEnemy()->ent);
    }
    else
    {
        // To reuse Ai::IsInFront() atm we have to provide a dummy edict
        // Ai::IsInFront uses only edict_t::s.origin
        VectorCopy(CombatTask().spamSpot.data(), dummyEnt.s.origin);
        targetEnt = &dummyEnt;
    }
    *isInFront = Ai::IsInFront(targetEnt);
    *mayHit = CheckShot(target);
}

void Bot::TryPressAttack(usercmd_t *ucmd, bool importantShot)
{
    const auto weapState = self->r.client->ps.weaponState;
    if (weapState != WEAPON_STATE_READY && weapState != WEAPON_STATE_REFIRE && weapState != WEAPON_STATE_REFIRESTRONG)
        return;

    float firedelay;
    // in continuous fire weapons don't add delays
    if (self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN)
        firedelay = 1.0f;
    else
        firedelay = 0.95f + 0.55f * Skill() - 1.5f * random();

    if (importantShot)
        firedelay += 0.45f * Skill();

    if (firedelay <= 0.0f)
        return;

    ucmd->buttons |= BUTTON_ATTACK;
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
    // in the lowest skill level, don't predict projectiles
    if (Skill() >= 0.33f)
    {
        if (CombatTask().aimEnemy)
            PredictProjectileShot(fire_origin, firedef->speed, target, CombatTask().aimEnemy->ent->velocity);
        else
            PredictProjectileShot(fire_origin, firedef->speed, target, vec3_origin);
    }

    float wfac = WFAC_GENERIC_PROJECTILE * 1.3f;

    if (CombatTask().aimEnemy)
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

    return wfac;
}

float Bot::AdjustPredictionAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    float wfac;
    if (self->s.weapon == WEAP_PLASMAGUN)
        wfac = WFAC_GENERIC_PROJECTILE * 0.5f;
    else
        wfac = WFAC_GENERIC_PROJECTILE;

    // in the lowest skill level, don't predict projectiles
    if (Skill() >= 0.33f)
    {
        if (CombatTask().aimEnemy)
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);
        else
            PredictProjectileShot(fire_origin, firedef->speed, target, vec3_origin);
    }

    return wfac;
}

float Bot::AdjustDropAimStyleTarget(const firedef_t *firedef, vec_t *fire_origin, vec_t *target)
{
    //jalToDo
    float wfac = WFAC_GENERIC_PROJECTILE;
    // in the lowest skill level, don't predict projectiles
    if (Skill() >= 0.33f)
    {
        const float *velocity = AimEnemy() ? AimEnemy()->ent->velocity : vec3_origin;
        PredictProjectileShot(fire_origin, firedef->speed, target, velocity);
    }
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
