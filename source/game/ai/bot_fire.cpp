#include "bot.h"
#include "ai_shutdown_hooks_holder.h"

inline bool operator!=(const AiScriptWeaponDef &first, const AiScriptWeaponDef &second)
{
    return memcmp(&first, &second, sizeof(AiScriptWeaponDef)) != 0;
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
            AiScriptWeaponDef weaponDef;
            if (GT_asGetScriptWeaponDef(self->r.client, weaponNum, &weaponDef))
            {
                scriptWeaponDefs.emplace_back(std::move(weaponDef));
                scriptWeaponCooldown.push_back(GT_asGetScriptWeaponCooldown(self->r.client, weaponNum));
            }
        }

        selectedWeapons.Invalidate();
        botBrain.ClearGoalAndPlan();
        return;
    }

    bool hasStatusChanged = false;
    for (int weaponNum = 0; weaponNum < scriptWeaponsNum; ++weaponNum)
    {
        AiScriptWeaponDef actualWeaponDef;
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
    {
        selectedWeapons.Invalidate();
        botBrain.ClearGoalAndPlan();
    }
}

void Bot::FireWeapon(BotInput *input)
{
    if (!selectedEnemies.AreValid())
        return;

    if (!selectedWeapons.AreValid())
        return;

    const GenericFireDef *builtinFireDef = selectedWeapons.BuiltinFireDef();
    const GenericFireDef *scriptFireDef = selectedWeapons.ScriptFireDef();

    AimParams builtinWeaponAimParams;
    AimParams scriptWeaponAimParams;

    if (builtinFireDef)
        builtinFireTargetCache.AdjustAimParams(selectedEnemies, selectedWeapons, *builtinFireDef, &builtinWeaponAimParams);

    if (scriptFireDef)
        scriptFireTargetCache.AdjustAimParams(selectedEnemies, selectedWeapons, *scriptFireDef, &scriptWeaponAimParams);

    // Select a weapon that has a priority in adjusting view angles for it
    const GenericFireDef *primaryFireDef = nullptr;
    const GenericFireDef *secondaryFireDef = nullptr;
    AimParams *aimParams;
    if (selectedWeapons.PreferBuiltinWeapon())
    {
        aimParams = &builtinWeaponAimParams;
        primaryFireDef = builtinFireDef;
        if (scriptFireDef)
            secondaryFireDef = scriptFireDef;
    }
    else
    {
        aimParams = &scriptWeaponAimParams;
        primaryFireDef = scriptFireDef;
        if (builtinFireDef)
            secondaryFireDef = builtinFireDef;
    }

    // Always track enemy with a "crosshair" like a human does in each frame
    LookAtEnemy(aimParams->EffectiveCoordError(Skill()), aimParams->fireOrigin, aimParams->fireTarget, input);

    // Attack only in Think() frames unless a continuousFire is required or the bot has hard skill
    if (ShouldSkipThinkFrame() && Skill() < 0.66f)
    {
        if (!primaryFireDef->IsContinuousFire())
        {
            if (!secondaryFireDef || !secondaryFireDef->IsContinuousFire())
                return;
        }
    }

    if (CheckShot(*aimParams, input, selectedEnemies, *primaryFireDef))
        PressAttack(primaryFireDef, builtinFireDef, scriptFireDef, input);

    if (secondaryFireDef)
    {
        // Check whether view angles adjusted for the primary weapon are suitable for firing secondary weapon too
        if (CheckShot(*aimParams, input, selectedEnemies, *secondaryFireDef))
            PressAttack(secondaryFireDef, builtinFireDef, scriptFireDef, input);
    }

    if (input->fireScriptWeapon)
        GT_asFireScriptWeapon(self->r.client, scriptFireDef->WeaponNum());
}

void Bot::LookAtEnemy(float coordError, const vec_t *fire_origin, vec_t *target, BotInput *input)
{
    for (int i = 0; i < 3; ++i)
        target[i] += (aimingRandomHolder.GetCoordRandom(i) - 0.5f) * coordError;

    Vec3 toTargetVec(target);
    toTargetVec -= fire_origin;
    float angularSpeedMultiplier = 0.5f + 0.5f * Skill();
    // If there is no look vec set or it can be completely overridden
    if (!input->isLookVecSet || input->canOverrideLookVec)
    {
        input->intendedLookVec = toTargetVec;
        input->alreadyComputedAngles = GetNewViewAngles(self->s.angles, toTargetVec, angularSpeedMultiplier);
    }
    // (in case when XY view movement is exactly specified and Z view movement can vary)
    else if (input->canOverridePitch)
    {
        // These angles can be intended by the already set look vec (can be = not always ideal due to limited view speed).
        Vec3 intendedAngles = GetNewViewAngles(self->s.angles, input->intendedLookVec, angularSpeedMultiplier);
        // These angles can be required to hit the target
        Vec3 targetAimAngles = GetNewViewAngles(self->s.angles, toTargetVec, angularSpeedMultiplier);
        // Override pitch in hope this will be sufficient for hitting a target
        intendedAngles.Data()[PITCH] = targetAimAngles.Data()[PITCH];
        input->hasAlreadyComputedAngles = true;
    }
    // Otherwise do not modify already set look vec (a shot can be made if it is occasionally sufficient for it).

    input->isLookVecSet = true;
    input->canOverrideLookVec = false;
    input->canOverridePitch = false;
}

void Bot::PressAttack(const GenericFireDef *fireDef,
                      const GenericFireDef *builtinFireDef,
                      const GenericFireDef *scriptFireDef,
                      BotInput *input)
{
    if (fireDef == scriptFireDef)
    {
        input->fireScriptWeapon = true;
        return;
    }

    auto weapState = self->r.client->ps.weaponState;
    if (weapState == WEAPON_STATE_READY || weapState == WEAPON_STATE_REFIRE || weapState == WEAPON_STATE_REFIRESTRONG)
        input->SetAttackButton(true);
}

bool Bot::CheckShot(const AimParams &aimParams,
                    const BotInput *input,
                    const SelectedEnemies &selectedEnemies,
                    const GenericFireDef &fireDef)
{
    // Convert modified angles to direction back (due to limited view speed it rarely will match given direction)
    Vec3 newLookDir(0, 0, 0);
    AngleVectors(input->alreadyComputedAngles.Data(), newLookDir.Data(), nullptr, nullptr);

    Vec3 toTarget(aimParams.fireTarget);
    toTarget -= aimParams.fireOrigin;
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

    // Precache this result, it is not just a value getter
    const auto aimType = fireDef.AimType();

    if (fireDef.IsContinuousFire())
    {
        if (toTargetDotLookDir < 0.8f * directionDistanceFactor)
            return false;
    }
    else if (aimType != AI_WEAPON_AIM_TYPE_DROP)
    {
        if (toTargetDotLookDir < 0.6f * directionDistanceFactor)
            return false;
    }
    else
    {
        if (toTargetDotLookDir < 0)
            return false;
    }

    // Do not shoot in enemies that are behind obstacles atm, bot may kill himself easily
    // We test directions factor first because it is cheaper to calculate

    trace_t tr;
    if (aimType != AI_WEAPON_AIM_TYPE_DROP)
    {
        Vec3 traceEnd(newLookDir);
        traceEnd *= 999999.0f;
        traceEnd += aimParams.fireOrigin;
        G_Trace(&tr, const_cast<float*>(aimParams.fireOrigin), nullptr, nullptr, traceEnd.Data(), self, MASK_AISOLID);
        if (tr.fraction == 1.0f)
            return true;
    }
    else
    {
        // For drop aim type weapons (a gravity is applied to a projectile) split projectile trajectory in segments
        vec3_t segmentStart;
        vec3_t segmentEnd;
        VectorCopy(aimParams.fireOrigin, segmentEnd);

        Vec3 projectileVelocity(newLookDir);
        projectileVelocity *= fireDef.ProjectileSpeed();

        const int numSegments = (int)(2 + 4 * Skill());
        // Predict for 1 second
        const float timeStep = 1.0f / numSegments;
        const float halfGravity = 0.5f * level.gravity;
        const float *fireOrigin = aimParams.fireOrigin;

        float currTime = timeStep;
        for (int i = 0; i < numSegments; ++i)
        {
            VectorCopy(segmentEnd, segmentStart);
            segmentEnd[0] = fireOrigin[0] + projectileVelocity.X() * currTime;
            segmentEnd[1] = fireOrigin[1] + projectileVelocity.Y() * currTime;
            segmentEnd[2] = fireOrigin[2] + projectileVelocity.Z() * currTime - halfGravity * currTime * currTime;

            G_Trace(&tr, segmentStart, nullptr, nullptr, segmentEnd, self, MASK_AISOLID);
            if (tr.fraction != 1.0f)
                break;

            currTime += timeStep;
        }
        // If hit point has not been found for predicted for 1 second trajectory
        if (tr.fraction == 1.0f)
        {
            // Check a trace from the last segment end to an infinite point
            VectorCopy(segmentEnd, segmentStart);
            currTime = 999.0f;
            segmentEnd[0] = fireOrigin[0] + projectileVelocity.X() * currTime;
            segmentEnd[1] = fireOrigin[1] + projectileVelocity.Y() * currTime;
            segmentEnd[2] = fireOrigin[2] + projectileVelocity.Z() * currTime - halfGravity * currTime * currTime;
            G_Trace(&tr, segmentStart, nullptr, nullptr, segmentEnd, self, MASK_AISOLID);
            if (tr.fraction == 1.0f)
                return true;
        }
    }

    if (game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype())
        return false;

    float hitToTargetDist = DistanceFast(selectedEnemies.LastSeenOrigin().Data(), tr.endpos);
    float hitToBotDist = DistanceFast(self->s.origin, tr.endpos);
    float proximityDistanceFactor = BoundedFraction(hitToBotDist, 2000.0f);
    float hitToTargetMissThreshold = 30.0f + 300.0f * proximityDistanceFactor;

    if (hitToBotDist < hitToTargetDist && !fireDef.IsContinuousFire())
        return false;

    if (aimType == AI_WEAPON_AIM_TYPE_PREDICTION_EXPLOSIVE)
        return hitToTargetDist < std::max(hitToTargetMissThreshold, 0.85f * fireDef.SplashRadius());

    // Trajectory prediction is not accurate, also this adds some randomization in grenade spamming.
    if (aimType == AI_WEAPON_AIM_TYPE_DROP)
    {
        // Allow shooting grenades in vertical walls
        if (DotProduct(tr.plane.normal, &axis_identity[AXIS_UP]) < -0.1f)
            return false;

        return hitToTargetDist < std::max(hitToTargetMissThreshold, 1.15f * fireDef.SplashRadius());
    }

    // For one-shot instant-hit weapons each shot is important, so check against a player bounding box
    Vec3 absMins(aimParams.fireTarget);
    Vec3 absMaxs(aimParams.fireTarget);
    absMins += playerbox_stand_mins;
    absMaxs += playerbox_stand_maxs;
    float factor = 0.33f;
    // Extra hack for EB/IG, otherwise they miss too lot due to premature firing
    if (fireDef.IsBuiltin())
    {
        if (fireDef.WeaponNum() == WEAP_ELECTROBOLT || fireDef.WeaponNum() == WEAP_INSTAGUN)
            factor *= std::max(0.0f, 0.66f - Skill());
    }
    return BoundsAndSphereIntersect(absMins.Data(), absMaxs.Data(), tr.endpos, 1.0f + factor * hitToTargetMissThreshold);
}


