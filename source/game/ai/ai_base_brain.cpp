#include "ai_local.h"
#include "aas.h"

AiBaseBrain::AiBaseBrain(edict_t *self, int allowedAasTravelFlags, int preferredAasTravelFlags)
    : self(self),
      longTermGoal(nullptr),
      shortTermGoal(nullptr),
      longTermGoalSearchTimeout(0),
      shortTermGoalSearchTimeout(0),
      longTermGoalSearchPeriod(1500),
      shortTermGoalSearchPeriod(700),
      longTermGoalReevaluationTimeout(0),
      shortTermGoalReevaluationTimeout(0),
      longTermGoalReevaluationPeriod(700),
      shortTermGoalReevaluationPeriod(350),
      weightsUpdateTimeout(0),
      allowedAasTravelFlags(allowedAasTravelFlags),
      preferredAasTravelFlags(preferredAasTravelFlags)
{
    ClearWeights();
}

void AiBaseBrain::Debug(const char *format, ...) const
{
    va_list va;
    va_start(va, format);
    AI_Debugv(self->r.client ? self->r.client->netname : self->classname, format, va);
    va_end(va);
}

void AiBaseBrain::ClearWeights()
{
    memset(entityWeights, 0, sizeof(entityWeights));
}

void AiBaseBrain::UpdateWeights()
{
    ClearWeights();

    // Call (overridden) subclass method that sets nav entities weights
    UpdatePotentialGoalsWeights();
}

// To be overridden. Its a stub that does not modify cleared weights
void AiBaseBrain::UpdatePotentialGoalsWeights() { }

void AiBaseBrain::Think()
{
    if (!currAasAreaNum)
        return;

    CheckOrCancelGoal();

    // Always update weights before goal picking, except we have updated it in this frame
    bool weightsUpdated = false;
    //update status information to feed up ai
    if (weightsUpdateTimeout <= level.time)
    {
        UpdateWeights();
        weightsUpdated = true;
    }

    if (longTermGoalSearchTimeout <= level.time || longTermGoalReevaluationTimeout <= level.time)
    {
        if (!weightsUpdated)
        {
            UpdateWeights();
            weightsUpdated = true;
        }
        PickLongTermGoal(longTermGoal);
    }

    if (shortTermGoalSearchTimeout <= level.time || shortTermGoalReevaluationTimeout <= level.time)
    {
        if (!weightsUpdated)
        {
            UpdateWeights();
            weightsUpdated = true;
        }
        PickShortTermGoal(shortTermGoal);
    }

    if (weightsUpdated)
        weightsUpdateTimeout = level.time + 500;
}

void AiBaseBrain::CheckOrCancelGoal()
{
    // Check for goal nullity in this function, not in ShouldCancelGoal()
    // (ShouldCancelGoal() return result may be confusing)

    // Check long-term goal first
    if (longTermGoal && ShouldCancelGoal(longTermGoal))
        ClearLongAndShortTermGoal(longTermGoal);
    else if (shortTermGoal && ShouldCancelGoal(shortTermGoal))
        ClearLongAndShortTermGoal(shortTermGoal);
}

bool AiBaseBrain::ShouldCancelGoal(const NavEntity *goalEnt)
{
    if (goalEnt->IsDisabled())
        return true;

    if (goalEnt->IsBasedOnSomeEntity())
    {
        unsigned spawnTime = goalEnt->SpawnTime();
        // The entity is not spawned and respawn time is unknown
        if (!spawnTime)
            return true;

        // Find milliseconds required to move to a goal
        unsigned moveTime = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, goalEnt->AasAreaNum()) * 10U;
        if (moveTime)
        {
            unsigned reachTime = level.time + moveTime;
            // A goal requires too long waiting
            if (spawnTime > reachTime && spawnTime - reachTime > 3000)
                return true;
        }

        if (MayNotBeFeasibleGoal(goalEnt))
        {
            Debug("Goal %s should be canceled as not looking like a feasible goal\n", goalEnt->Name());
            return true;
        }
    }

    return false;
}

void AiBaseBrain::ClearAllGoals()
{
    if (longTermGoal)
        ClearLongAndShortTermGoal(longTermGoal);
    if (shortTermGoal)
        ClearLongAndShortTermGoal(shortTermGoal);
}

bool AiBaseBrain::HandleGoalTouch(const edict_t *ent)
{
    // Handle long-term goal first (this implies short-term goal handling too)
    if (longTermGoal && longTermGoal->IsBasedOnEntity(ent))
    {
        OnLongTermGoalReached();
        return true;
    }

    if (shortTermGoal && shortTermGoal->IsBasedOnEntity(ent))
    {
        OnShortTermGoalReached();
        return true;
    }

    return false;
}

bool AiBaseBrain::IsCloseToAnyGoal() const
{
    return IsCloseToGoal(longTermGoal, 128.0f) && IsCloseToGoal(shortTermGoal, 128.0f);
}

constexpr float GOAL_PROXIMITY_SQ_THRESHOLD = 40.0f * 40.0f;

bool AiBaseBrain::TryReachGoalByProximity()
{
    // Bots do not wait for these kind of goals atm, just check goal presence, kind and proximity
    // Check long-term goal first
    if (longTermGoal && !longTermGoal->ShouldBeReachedAtTouch())
    {
        if ((longTermGoal->Origin() - self->s.origin).SquaredLength() < GOAL_PROXIMITY_SQ_THRESHOLD)
        {
            OnLongTermGoalReached();
            return true;
        }
    }

    if (shortTermGoal && !shortTermGoal->ShouldBeReachedAtTouch())
    {
        if ((shortTermGoal->Origin() - self->s.origin).SquaredLength() < GOAL_PROXIMITY_SQ_THRESHOLD)
        {
            OnShortTermGoalReached();
            return true;
        }
    }

    return false;
}

bool AiBaseBrain::ShouldWaitForGoal() const
{
    // Only long-term goals may be waited for
    if (longTermGoal && longTermGoal->ShouldBeReachedAtTouch())
    {
        if ((longTermGoal->Origin() - self->s.origin).SquaredLength() < GOAL_PROXIMITY_SQ_THRESHOLD)
        {
            if (longTermGoal->SpawnTime() > level.time)
                return true;
        }
    }
    return false;
}

Vec3 AiBaseBrain::ClosestGoalOrigin() const
{
    float minSqDist = INFINITY;
    NavEntity *chosenGoal = nullptr;
    for (NavEntity *goal: { longTermGoal, shortTermGoal })
    {
        if (!goal) continue;
        float sqDist = (goal->Origin() - self->s.origin).SquaredLength();
        if (minSqDist < sqDist)
        {
            minSqDist = sqDist;
            chosenGoal = goal;
        }
    }
#ifdef _DEBUG
    if (!chosenGoal)
    {
        Debug("AiBaseBrain::ClosestGoalOrigin(): there are no goals\n");
        abort();
    }
#endif
    return chosenGoal->Origin();
}

constexpr float COST_INFLUENCE = 0.5f;
constexpr float MOVE_TIME_WEIGHT = 1.0f;
constexpr float WAIT_TIME_WEIGHT = 3.5f;

void AiBaseBrain::PickLongTermGoal(const NavEntity *currLongTermGoalEnt)
{
    // Clear short-term goal too
    longTermGoal = nullptr;
    shortTermGoal = nullptr;

    if (G_ISGHOSTING(self))
        return;

    if (longTermGoalSearchTimeout > level.time && longTermGoalReevaluationTimeout > level.time)
        return;

    if (!self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED])
        return;

    float bestWeight = 0.000001f;
    float currGoalEntWeight = 0.0f;
    NavEntity *bestGoalEnt = NULL;

    // Run the list of potential goal entities
    FOREACH_GOALENT(goalEnt)
    {
        if (goalEnt->IsDisabled())
            continue;

        if (goalEnt->Item() && !G_Gametype_CanPickUpItem(goalEnt->Item()))
            continue;

        float weight = entityWeights[goalEnt->Id()];

        if (weight <= 0.0f)
            continue;

        unsigned moveDuration = 1;
        unsigned waitDuration = 1;
        if (currAasAreaNum != goalEnt->AasAreaNum())
        {
            // We ignore cost of traveling in goal area, since:
            // 1) to estimate it we have to retrieve reachability to goal area from last area before the goal area
            // 2) it is relative low compared to overall travel cost, and movement in areas is cheap anyway
            moveDuration = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, goalEnt->AasAreaNum()) * 10U;
            // AAS functions return 0 as a "none" value, 1 as a lowest feasible value
            if (!moveDuration)
                continue;

            if (goalEnt->IsDroppedEntity())
            {
                // Do not pick an entity that is likely to dispose before it may be reached
                if (goalEnt->DroppedEntityTimeout() <= level.time + moveDuration)
                    continue;
            }
        }

        if (goalEnt->IsBasedOnSomeEntity())
        {
            unsigned spawnTime = goalEnt->SpawnTime();
            // The entity is not spawned and respawn time is unknown
            if (!spawnTime)
                continue;

            // Entity origin may be reached at this time
            unsigned reachTime = level.time + moveDuration;
            if (reachTime < spawnTime)
                waitDuration = spawnTime - reachTime;

            // Waiting time is too large
            if (waitDuration > 3000)
                continue;
        }

        float cost = 0.0001f + MOVE_TIME_WEIGHT * moveDuration + WAIT_TIME_WEIGHT * waitDuration;

        weight = (1000 * weight) / (cost * COST_INFLUENCE); // Check against cost of getting there

        // MayNotBeFeasibleGoal() calls are very expensive.
        // Try to cut off these calls when it is possible and cache results.
        bool feasibilityChecked = false;
        if (currLongTermGoalEnt == goalEnt)
        {
            if (MayNotBeFeasibleGoal(goalEnt))
                continue;
            feasibilityChecked = true;
            currGoalEntWeight = weight;
        }

        if (weight > bestWeight)
        {
            if (!feasibilityChecked && MayNotBeFeasibleGoal(goalEnt))
                continue;
            bestWeight = weight;
            bestGoalEnt = goalEnt;
        }
    }

    // If it is time to pick a new goal (not just re-evaluate current one), do not be too sticky to the current goal
    const float currToBestWeightThreshold = longTermGoalSearchTimeout > level.time ? 0.6f : 0.8f;

    if (bestGoalEnt)
    {
        if (currLongTermGoalEnt == bestGoalEnt)
        {
            Debug("current long-term goal %s is kept as still having best weight %.3f\n", bestGoalEnt->Name(), bestWeight);
        }
        else if (currGoalEntWeight > 0 && currGoalEntWeight / bestWeight > currToBestWeightThreshold)
        {
            const char *format =
                "current long-term goal %s is kept as having weight %.3f good enough to not consider picking another one\n";
            // If currGoalEntWeight > 0, currLongTermGoalEnt is guaranteed to be non-null
            Debug(format, currLongTermGoalEnt->Name(), currGoalEntWeight);
        }
        else
        {
            if (currLongTermGoalEnt)
            {
                const char *format = "chose %s weighted %.3f as a long-term goal instead of %s weighted now as %.3f\n";
                Debug(format, bestGoalEnt->Name(), bestWeight, currLongTermGoalEnt->Name(), currGoalEntWeight);
            }
            else
            {
                Debug("chose %s weighted %.3f as a new long-term goal\n", bestGoalEnt->Name(), bestWeight);
            }
            SetLongTermGoal(bestGoalEnt);
        }
    }

    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
}

void AiBaseBrain::PickShortTermGoal(const NavEntity *currShortTermGoalEnt)
{
    NavEntity *bestGoalEnt = nullptr;
    float bestWeight = 0.000001f;
    float currGoalEntWeight = 0.0f;

    if (!self->r.client || G_ISGHOSTING(self))
        return;

    if (shortTermGoalSearchTimeout > level.time && shortTermGoalReevaluationTimeout > level.time)
        return;

    bool canPickupItems = self->r.client->ps.pmove.stats[PM_STAT_FEATURES] & PMFEAT_ITEMPICK;

    vec3_t forward;
    AngleVectors(self->s.angles, forward, nullptr, nullptr);

    FOREACH_GOALENT(goalEnt)
    {
        if (goalEnt->IsDisabled())
            continue;

        // Do not predict short-term goal spawn (looks weird)
        if (goalEnt->ToBeSpawnedLater())
            continue;

        if (goalEnt->IsClient())
            continue;

        if (entityWeights[goalEnt->Id()] <= 0.0f)
            continue;

        if (canPickupItems && goalEnt->Item())
        {
            if(!G_Gametype_CanPickUpItem(goalEnt->Item()) || !(goalEnt->Item()->flags & ITFLAG_PICKABLE))
                continue;
        }

        // First cut off items by distance for performance reasons since this function is called quite frequently.
        // It is not very accurate in terms of level connectivity, but short-term goals are not critical.
        float dist = (goalEnt->Origin() - self->s.origin).LengthFast();
        if (goalEnt == longTermGoal)
        {
            if (dist > AI_GOAL_SR_LR_RADIUS)
                continue;
        }
        else
        {
            if (dist > AI_GOAL_SR_RADIUS)
                continue;
        }

        clamp_low(dist, 0.01f);

        bool inFront = true;
        if (dist > 1)
        {
            Vec3 botToTarget = goalEnt->Origin() - self->s.origin;
            botToTarget *= 1.0f / dist;
            if (botToTarget.Dot(forward) < 0.7)
                inFront = false;
        }

        // Cut items by weight first, IsShortRangeReachable() is quite expensive
        float weight = entityWeights[goalEnt->Id()] / dist * (inFront ? 1.0f : 0.5f);

        // MayNotBeFeasibleGoal() calls are very expensive.
        // Try to cut off these calls when it is possible and cache results.
        bool feasibilityChecked = false;
        if (currShortTermGoalEnt == goalEnt)
        {
            if (MayNotBeFeasibleGoal(goalEnt))
                continue;
            feasibilityChecked = true;
            currGoalEntWeight = weight;
        }

        if (weight > 0)
        {
            if (weight > bestWeight)
            {
                std::pair<unsigned, unsigned> toAndBackAasTravelTimes = FindToAndBackTravelTimes(goalEnt->Origin());
                if (toAndBackAasTravelTimes.first > 0 && toAndBackAasTravelTimes.second > 0)
                {
                    // Convert from AAS centiseconds
                    unsigned toTravelMillis = 10 * toAndBackAasTravelTimes.first;
                    unsigned backTravelMillis = 10 * toAndBackAasTravelTimes.second;
                    if (goalEnt->IsDroppedEntity())
                    {
                        // Ensure it will not dispose before the bot may reach it
                        if (goalEnt->DroppedEntityTimeout() > level.time + toTravelMillis)
                        {
                            if ((toTravelMillis + backTravelMillis) / 2 < AI_GOAL_SR_MILLIS)
                            {
                                // We hope this call will be cut for most entities
                                if (!feasibilityChecked && MayNotBeFeasibleGoal(goalEnt))
                                    continue;

                                bestWeight = weight;
                                bestGoalEnt = goalEnt;
                            }
                        }
                    }
                    else if ((toTravelMillis + backTravelMillis) / 2 < AI_GOAL_SR_MILLIS)
                    {
                        // We hope this call will be cut for most entities
                        if (!feasibilityChecked && MayNotBeFeasibleGoal(goalEnt))
                            continue;

                        bestWeight = weight;
                        bestGoalEnt = goalEnt;
                    }
                }
            }
                // Long-term goal just need some positive weight and be in front to be chosen as a short-term goal too
            else if (inFront && goalEnt == longTermGoal)
            {
                bestGoalEnt = goalEnt;
                break;
            }
        }
    }

    const bool isDoingSearch = level.time <= shortTermGoalReevaluationTimeout;
    const float currToBestWeightThreshold = isDoingSearch ? 0.9f : 0.7f;

    if (bestGoalEnt)
    {
        if (currShortTermGoalEnt == bestGoalEnt)
        {
            Debug("current short-term goal %s is kept as still having best weight %.3f\n", bestGoalEnt->Name(), bestWeight);
        }
        else if (currGoalEntWeight > 0 && currGoalEntWeight / bestWeight > currToBestWeightThreshold)
        {
            const char *format =
                "current short-term goal %s is kept as having weight %.3f good enough to not consider picking another one\n";
            // If currGoalEntWeight > 0, currShortTermGoal is guaranteed to be non-null
            Debug(format, currShortTermGoalEnt->Name(), currGoalEntWeight);
        }
        else
        {
            if (currShortTermGoalEnt)
            {
                const char *format = "chose %s weighted %.3f as a short-term goal instead of %s weighted now as %.3f\n";
                Debug(format, bestGoalEnt->Name(), bestWeight, currShortTermGoalEnt->Name(), currGoalEntWeight);
            }
            else
            {
                Debug("chose %s weighted %.3f as a new short-term goal\n", bestGoalEnt->Name(), bestWeight);
            }
            SetShortTermGoal(bestGoalEnt);
        }
    }
    if (isDoingSearch)
    {
        shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    }
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
}

void AiBaseBrain::SetLongTermGoal(NavEntity *goalEnt)
{
    longTermGoal = goalEnt;
    longTermGoalSearchTimeout = level.time + longTermGoalSearchPeriod;
    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    self->ai->aiRef->OnGoalSet(goalEnt);
}

void AiBaseBrain::SetShortTermGoal(NavEntity *goalEnt)
{
    shortTermGoal = goalEnt;
    shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
    self->ai->aiRef->OnGoalSet(goalEnt);
}

void AiBaseBrain::ClearLongAndShortTermGoal(const NavEntity *pickedGoal)
{
    longTermGoal = nullptr;
    // Request long-term goal update in next frame
    longTermGoalSearchTimeout = level.time + 1;
    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    // Clear short-term goal too
    shortTermGoal = nullptr;
    shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalSearchPeriod + shortTermGoalReevaluationPeriod;
    // Request immediate status update
    weightsUpdateTimeout = 0;
    // Call possible overridden child callback method
    OnGoalCleanedUp(pickedGoal);
    // Notify other AI's about the goal pickup
    AiGametypeBrain::Instance()->ClearGoals(pickedGoal, self->ai->aiRef);
}

void AiBaseBrain::OnLongTermGoalReached()
{
    Debug("reached long-term goal %s\n", longTermGoal->Name());
    ClearLongAndShortTermGoal(longTermGoal);
}

void AiBaseBrain::OnShortTermGoalReached()
{
    Debug("reached short-term goal %s\n", shortTermGoal->Name());
    ClearLongAndShortTermGoal(shortTermGoal);
}

std::pair<unsigned, unsigned> AiBaseBrain::FindToAndBackTravelTimes(const Vec3 &targetOrigin) const
{
    vec3_t testedOrigin;
    VectorCopy(targetOrigin.data(), testedOrigin);
    int areaNum = AAS_PointAreaNum(testedOrigin);
    if (!areaNum)
    {
        testedOrigin[2] += 8.0f;
        areaNum = AAS_PointAreaNum(testedOrigin);
        if (!areaNum)
        {
            testedOrigin[2] -= 16.0f;
            areaNum = AAS_PointAreaNum(testedOrigin);
        }
    }
    if (!areaNum)
        return std::make_pair(0, 0);

    if (areaNum == currAasAreaNum)
        return std::make_pair(1, 1);

    int toAasTravelTime = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, areaNum);
    if (!toAasTravelTime)
        return std::make_pair(0, 0);
    int backAasTravelTime = FindAASTravelTimeToGoalArea(areaNum, testedOrigin, currAasAreaNum);
    return std::make_pair(toAasTravelTime, backAasTravelTime);
}

