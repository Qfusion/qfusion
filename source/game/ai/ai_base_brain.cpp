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
      statusUpdateTimeout(0),
      allowedAasTravelFlags(allowedAasTravelFlags),
      preferredAasTravelFlags(preferredAasTravelFlags)
{
    ClearWeights();
}

void AiBaseBrain::Debug(const char *format, ...)
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

    // Always update weights before goal picking, except we have updated it in this frame
    bool weightsUpdated = false;
    //update status information to feed up ai
    if (statusUpdateTimeout <= level.time)
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
        }
        PickShortTermGoal(shortTermGoal);
    }
}

constexpr float COST_INFLUENCE = 0.5f;

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

        // Items timing is currently disabled
        if (goalEnt->ToBeSpawnedLater())
            continue;

        if (goalEnt->Item() && !G_Gametype_CanPickUpItem(goalEnt->Item()))
            continue;

        float weight = entityWeights[goalEnt->Id()];

        if (weight <= 0.0f)
            continue;

        float cost = 0;
        if (currAasAreaNum == goalEnt->AasAreaNum())
        {
            // Traveling in a single area is cheap anyway for a player-like bot, don't bother to compute travel time.
            cost = 1;
        }
        else
        {
            // We ignore cost of traveling in goal area, since:
            // 1) to estimate it we have to retrieve reachability to goal area from last area before the goal area
            // 2) it is relative low compared to overall travel cost, and movement in areas is cheap anyway
            cost = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, goalEnt->AasAreaNum());
        }

        if (cost == 0)
            continue;

        clamp_low(cost, 1);
        weight = (1000 * weight) / (cost * COST_INFLUENCE); // Check against cost of getting there

        if (currLongTermGoalEnt == goalEnt)
            currGoalEntWeight = weight;

        if (weight > bestWeight)
        {
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

        if (currShortTermGoalEnt == goalEnt)
            currGoalEntWeight = weight;

        if (weight > 0)
        {
            if (weight > bestWeight)
            {
                if (IsShortRangeReachable(goalEnt->Origin()))
                {
                    bestWeight = weight;
                    bestGoalEnt = goalEnt;
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

void AiBaseBrain::ClearLongTermGoal()
{
    longTermGoal = nullptr;
    // Request immediate long-term goal update
    longTermGoalSearchTimeout = 0;
    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    // Clear short-term goal too
    shortTermGoal = nullptr;
    shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
    // Request immediate status update
    statusUpdateTimeout = 0;
}

void AiBaseBrain::ClearShortTermGoal()
{
    shortTermGoal = nullptr;
    shortTermGoalSearchTimeout = level.time + shortTermGoalReevaluationPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
}

void AiBaseBrain::OnLongTermGoalReached()
{
    NavEntity *goalEnt = longTermGoal;
    Debug("reached long-term goal %s\n", goalEnt->Name());
    ClearLongTermGoal();
    AiGametypeBrain::Instance()->ClearGoals(goalEnt, self->ai->aiRef);
}

void AiBaseBrain::OnShortTermGoalReached()
{
    NavEntity *goalEnt = shortTermGoal;
    Debug("reached short-term goal %s\n", goalEnt->Name());
    ClearShortTermGoal();
    AiGametypeBrain::Instance()->ClearGoals(goalEnt, self->ai->aiRef);
    // Clear long-term goal too if it is based on the goalEnt
    if (longTermGoal == goalEnt)
    {
        ClearLongTermGoal();
    }
    // Restore old long-term goal overridden by short-term one if it is still present
    else if (longTermGoal)
    {
        SetLongTermGoal(longTermGoal);
    }
}

bool AiBaseBrain::IsShortRangeReachable(const Vec3 &targetOrigin) const
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
        return false;

    if (areaNum == currAasAreaNum)
        return true;

    // AAS functions return time in seconds^-2
    int toTravelTimeMillis = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, areaNum) * 10;
    if (!toTravelTimeMillis)
        return false;
    int backTravelTimeMillis = FindAASTravelTimeToGoalArea(areaNum, testedOrigin, currAasAreaNum) * 10;
    if (!backTravelTimeMillis)
        return false;

    return (toTravelTimeMillis + backTravelTimeMillis) / 2 < AI_GOAL_SR_MILLIS;
}

