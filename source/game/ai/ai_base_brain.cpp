#include "ai_base_brain.h"
#include "ai_gametype_brain.h"
#include "ai_base_team_brain.h"
#include "ai_base_ai.h"
#include "ai_ground_trace_cache.h"
#include "aas.h"
#include "static_vector.h"
#include "../../gameshared/q_collision.h"

AiBaseBrain::AiBaseBrain(edict_t *self, int preferredAasTravelFlags, int allowedAasTravelFlags)
    : self(self),
      localLongTermGoal(this),
      longTermGoal(nullptr),
      localShortTermGoal(this),
      shortTermGoal(nullptr),
      localSpecialGoal(this),
      specialGoal(nullptr),
      longTermGoalSearchTimeout(0),
      shortTermGoalSearchTimeout(0),
      longTermGoalSearchPeriod(1500),
      shortTermGoalSearchPeriod(700),
      longTermGoalReevaluationTimeout(0),
      shortTermGoalReevaluationTimeout(0),
      longTermGoalReevaluationPeriod(700),
      shortTermGoalReevaluationPeriod(350),
      currAasAreaNum(0),
      droppedToFloorAasAreaNum(0),
      droppedToFloorOrigin(NAN, NAN, NAN),
      preferredAasTravelFlags(preferredAasTravelFlags),
      allowedAasTravelFlags(allowedAasTravelFlags)
{
    ClearInternalEntityWeights();
    // External weights are cleared by AI code only once in this constructor.
    // Their values are completely managed by external code.
    ClearExternalEntityWeights();
}

bool AiBaseBrain::IsCloseToGoal(const Goal *goal, float proximityThreshold) const
{
    if (!goal)
        return false;
    float radius = goal->RadiusOrDefault(proximityThreshold) + 32.0f;
    return (goal->Origin() - self->s.origin).SquaredLength() <= radius * radius;
}

int AiBaseBrain::GoalAasAreaNum() const
{
    if (specialGoal)
        return specialGoal->AasAreaNum();
    if (longTermGoal)
        return longTermGoal->AasAreaNum();
    if (shortTermGoal)
        return shortTermGoal->AasAreaNum();
    return 0;
}

template<typename AASFun>
int AiBaseBrain::FindAASParamToGoalArea(AASFun aasFun, int goalAreaNum) const
{
    float *origin = const_cast<float*>(droppedToFloorOrigin.Data());
    int param = aasFun(droppedToFloorAasAreaNum, origin, goalAreaNum, preferredAasTravelFlags);
    if (param)
        return param;
    return aasFun(droppedToFloorAasAreaNum, origin, goalAreaNum, allowedAasTravelFlags);
}

int AiBaseBrain::FindAASReachabilityToGoalArea(int goalAreaNum) const
{
    return AiBaseBrain::FindAASParamToGoalArea(AAS_AreaReachabilityToGoalArea, goalAreaNum);
}

int AiBaseBrain::FindAASTravelTimeToGoalArea(int goalAreaNum) const
{
    return AiBaseBrain::FindAASParamToGoalArea(AAS_AreaTravelTimeToGoalArea, goalAreaNum);
}

void AiBaseBrain::UpdateInternalWeights()
{
    ClearInternalEntityWeights();

    // Call (overridden) subclass method that sets nav entities weights
    UpdatePotentialGoalsWeights();
}

// To be overridden. Its a stub that does not modify cleared weights
void AiBaseBrain::UpdatePotentialGoalsWeights() { }

float AiBaseBrain::GetEntityWeight(int entNum) const
{
    float externalWeight = externalEntityWeights[entNum];
    // Note: a negative value of an external weight overrides corresponding internal weight too.
    if (externalWeight != 0.0f)
        return externalWeight;
    return internalEntityWeights[entNum];
}

void AiBaseBrain::PreThink()
{
    // Copy these values for faster access and (mainly) backward compatibility
    // TODO: Make these values read-only properties to avoid confusion?
    currAasAreaNum = self->ai->aiRef->currAasAreaNum;
    droppedToFloorAasAreaNum = self->ai->aiRef->droppedToFloorAasAreaNum;
    droppedToFloorOrigin = self->ai->aiRef->droppedToFloorOrigin;
}

void AiBaseBrain::Think()
{
    if (!currAasAreaNum)
        return;

    CheckOrCancelGoal();

    // Do not bother of picking a goal while in air (many areas are not reachable from air areas).
    // Otherwise bot will spam lots of messages "Can't find any goal candidates".
    trace_t trace;
    AiGroundTraceCache::Instance()->GetGroundTrace(self, 96.0f, &trace);
    if (trace.fraction == 1.0f)
        return;

    // Always update weights before goal picking, except we have updated it in this frame
    bool weightsUpdated = false;

    if (longTermGoalSearchTimeout <= level.time || longTermGoalReevaluationTimeout <= level.time)
    {
        if (!weightsUpdated)
        {
            UpdateInternalWeights();
            weightsUpdated = true;
        }
        PickLongTermGoal(longTermGoal);
    }

    if (shortTermGoalSearchTimeout <= level.time || shortTermGoalReevaluationTimeout <= level.time)
    {
        if (!weightsUpdated)
        {
            UpdateInternalWeights();
            weightsUpdated = true;
        }
        PickShortTermGoal(shortTermGoal);
    }
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

    if (specialGoal && ShouldCancelGoal(specialGoal))
        OnClearSpecialGoalRequested();
}

bool AiBaseBrain::ShouldCancelGoal(const Goal *goal)
{
    if (goal->IsDisabled())
        return true;

    unsigned spawnTime = goal->SpawnTime();
    // The entity is not spawned and respawn time is unknown
    if (!spawnTime)
        return true;

    unsigned timeout = goal->Timeout();
    if (timeout <= level.time)
        return true;

    if (goal->IsBasedOnSomeEntity())
    {
        // Find milliseconds required to move to a goal
        unsigned moveTime = FindAASTravelTimeToGoalArea(goal->AasAreaNum()) * 10U;
        if (moveTime)
        {
            unsigned reachTime = level.time + moveTime;
            // A goal requires too long waiting
            if (spawnTime > reachTime && spawnTime - reachTime > goal->MaxWaitDuration())
                return true;
        }

        if (MayNotBeFeasibleGoal(goal))
        {
            Debug("Goal %s should be canceled as not looking like a feasible goal\n", goal->Name());
            return true;
        }
    }

    if (goal == specialGoal)
        return ShouldCancelSpecialGoalBySpecificReasons();

    return false;
}

void AiBaseBrain::ClearAllGoals()
{
    if (longTermGoal)
        ClearLongAndShortTermGoal(longTermGoal);
    if (shortTermGoal)
        ClearLongAndShortTermGoal(shortTermGoal);
    // Do not clear directly but delegate it
    if (specialGoal)
        OnClearSpecialGoalRequested();
}

void AiBaseBrain::OnClearSpecialGoalRequested()
{
    OnGoalCleanedUp(specialGoal);
    specialGoal = nullptr;
}

bool AiBaseBrain::HandleGoalTouch(const edict_t *ent)
{
    // Handle long-term goal first (this implies short-term goal handling too)
    if (longTermGoal && longTermGoal->ShouldBeReachedAtTouch() && longTermGoal->IsBasedOnEntity(ent))
    {
        OnLongTermGoalReached();
        return true;
    }

    if (shortTermGoal && longTermGoal->ShouldBeReachedAtTouch() && shortTermGoal->IsBasedOnEntity(ent))
    {
        OnShortTermGoalReached();
        return true;
    }

    return HandleSpecialGoalTouch(ent);
}

bool AiBaseBrain::HandleSpecialGoalTouch(const edict_t *ent)
{
    if (specialGoal && specialGoal->ShouldBeReachedAtTouch() && specialGoal->IsBasedOnEntity(ent))
    {
        OnSpecialGoalReached();
        return true;
    }
    return false;
}

bool AiBaseBrain::IsCloseToAnyGoal() const
{
    return
        IsCloseToGoal(longTermGoal, 96.0f) ||
        IsCloseToGoal(shortTermGoal, 96.0f) ||
        IsCloseToGoal(specialGoal, 96.0f);
}

constexpr float GOAL_PROXIMITY_THRESHOLD = 40.0f * 40.0f;

bool AiBaseBrain::TryReachGoalByProximity()
{
    // Bots do not wait for these kind of goals atm, just check goal presence, kind and proximity
    // Check long-term goal first
    if (longTermGoal && longTermGoal->ShouldBeReachedAtRadius())
    {
        float radius = longTermGoal->RadiusOrDefault(GOAL_PROXIMITY_THRESHOLD);
        if ((longTermGoal->Origin() - self->s.origin).SquaredLength() < radius * radius)
        {
            OnLongTermGoalReached();
            return true;
        }
    }

    if (shortTermGoal && shortTermGoal->ShouldBeReachedAtRadius())
    {
        float radius = shortTermGoal->RadiusOrDefault(GOAL_PROXIMITY_THRESHOLD);
        if ((shortTermGoal->Origin() - self->s.origin).SquaredLength() < radius * radius)
        {
            OnShortTermGoalReached();
            return true;
        }
    }

    return TryReachSpecialGoalByProximity();
}

bool AiBaseBrain::TryReachSpecialGoalByProximity()
{
    if (specialGoal && specialGoal->ShouldBeReachedAtRadius())
    {
        float radius = specialGoal->RadiusOrDefault(GOAL_PROXIMITY_THRESHOLD);
        if ((specialGoal->Origin() - self->s.origin).SquaredLength() < radius * radius)
        {
            OnSpecialGoalReached();
            return true;
        }
    }
    return false;
}

bool AiBaseBrain::ShouldWaitForGoal() const
{
    if (longTermGoal && !longTermGoal->ShouldBeReachedAtRadius())
    {
        float radius = GOAL_PROXIMITY_THRESHOLD;
        if ((longTermGoal->Origin() - self->s.origin).SquaredLength() < radius * radius)
        {
            if (longTermGoal->ShouldBeReachedOnEvent())
                return true;
            if (longTermGoal->SpawnTime() > level.time)
                return true;
        }
    }

    return ShouldWaitForSpecialGoal();
}

bool AiBaseBrain::ShouldWaitForSpecialGoal() const
{
    if (specialGoal && !specialGoal->ShouldBeReachedAtRadius())
    {
        float radius = GOAL_PROXIMITY_THRESHOLD;
        if ((specialGoal->Origin() - self->s.origin).SquaredLength() < radius * radius)
        {
            if (specialGoal->ShouldBeReachedOnEvent())
                return true;
            if (specialGoal->SpawnTime() > level.time)
                return true;
        }
    }
    return false;
}

Vec3 AiBaseBrain::ClosestGoalOrigin() const
{
    float minSqDist = std::numeric_limits<float>::max();
    Goal *chosenGoal = nullptr;
    for (Goal *goal: { longTermGoal, shortTermGoal, specialGoal })
    {
        if (!goal)
            continue;
        float sqDist = (goal->Origin() - self->s.origin).SquaredLength();
        if (minSqDist > sqDist)
        {
            minSqDist = sqDist;
            chosenGoal = goal;
        }
    }
    if (!chosenGoal)
    {
        FailWith("ClosestGoalOrigin(): there are no goals\n");
    }
    return chosenGoal->Origin();
}

constexpr float COST_INFLUENCE = 0.5f;
constexpr float MOVE_TIME_WEIGHT = 1.0f;
constexpr float WAIT_TIME_WEIGHT = 3.5f;

float AiBaseBrain::SelectLongTermGoalCandidates(const Goal *currLongTermGoal, GoalCandidates &result)
{
    result.clear();
    float currGoalEntWeight = 0.0f;

    FOREACH_NAVENT(navEnt)
    {
        if (navEnt->IsDisabled())
            continue;

        if (navEnt->Item() && !G_Gametype_CanPickUpItem(navEnt->Item()))
            continue;

        float weight = GetEntityWeight(navEnt->Id());
        if (weight <= 0.0f)
            continue;

        unsigned moveDuration = 1;
        unsigned waitDuration = 1;
        if (currAasAreaNum != navEnt->AasAreaNum())
        {
            // We ignore cost of traveling in goal area, since:
            // 1) to estimate it we have to retrieve reachability to goal area from last area before the goal area
            // 2) it is relative low compared to overall travel cost, and movement in areas is cheap anyway
            moveDuration = FindAASTravelTimeToGoalArea(navEnt->AasAreaNum()) * 10U;
            // AAS functions return 0 as a "none" value, 1 as a lowest feasible value
            if (!moveDuration)
                continue;

            if (navEnt->IsDroppedEntity())
            {
                // Do not pick an entity that is likely to dispose before it may be reached
                if (navEnt->Timeout() <= level.time + moveDuration)
                    continue;
            }
        }

        unsigned spawnTime = navEnt->SpawnTime();
        // The entity is not spawned and respawn time is unknown
        if (!spawnTime)
            continue;

        // Entity origin may be reached at this time
        unsigned reachTime = level.time + moveDuration;
        if (reachTime < spawnTime)
            waitDuration = spawnTime - reachTime;

        if (waitDuration > navEnt->MaxWaitDuration())
            continue;

        float cost = 0.0001f + MOVE_TIME_WEIGHT * moveDuration + WAIT_TIME_WEIGHT * waitDuration;

        weight = (1000 * weight) / (cost * COST_INFLUENCE); // Check against cost of getting there

        // Store current weight of the current goal entity
        if (currLongTermGoal && currLongTermGoal->IsBasedOnNavEntity(navEnt))
            currGoalEntWeight = weight;

        result.emplace_back(NavEntityAndWeight(navEnt, weight));
    }

    std::sort(result.begin(), result.end());

    return currGoalEntWeight;
}

void AiBaseBrain::PickLongTermGoal(const Goal *currLongTermGoal)
{
    shortTermGoal = nullptr;

    if (G_ISGHOSTING(self))
        return;

    // Present special goal blocks other goals selection
    if (specialGoal)
        return;

    // Should defer long-term goal pickup
    if (longTermGoalSearchTimeout > level.time && longTermGoalReevaluationTimeout > level.time)
        return;

    // Can't pickup items if can't move
    if (!self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED])
        return;

    StaticVector<NavEntityAndWeight, MAX_NAVENTS> goalCandidates;
    float currGoalEntWeight = SelectLongTermGoalCandidates(currLongTermGoal, goalCandidates);

    // Always check current goal feasibility if this goal has non-zero current weight
    if (currGoalEntWeight && MayNotBeFeasibleGoal(currLongTermGoal))
        currGoalEntWeight = 0;

    if (goalCandidates.empty())
    {
        Debug("Can't find any long-term goal nav. entity candidates\n");
        return;
    }

    NavEntity *bestNavEnt = nullptr;
    float bestWeight;

    for (auto &goalAndWeight: goalCandidates)
    {
        if (!MayNotBeFeasibleGoal(goalAndWeight.goal))
        {
            // Goals are sorted by weight in descending order,
            // so the first feasible goal has largest weight among other feasible ones
            bestNavEnt = goalAndWeight.goal;
            bestWeight = goalAndWeight.weight;
            break;
        }
    }

    if (!bestNavEnt)
    {
        Debug("Can't find a feasible long-term goal nav. entity\n");
        return;
    }

    // If it is time to pick a new goal (not just re-evaluate current one), do not be too sticky to the current goal
    const float currToBestWeightThreshold = longTermGoalSearchTimeout > level.time ? 0.6f : 0.8f;

    if (currLongTermGoal && currLongTermGoal->IsBasedOnNavEntity(bestNavEnt))
    {
        Debug("current long-term goal %s is kept as still having best weight %.3f\n", currLongTermGoal->Name(), bestWeight);
    }
    else if (currGoalEntWeight > 0 && currGoalEntWeight / bestWeight > currToBestWeightThreshold)
    {
        const char *format =
            "current long-term goal %s is kept as having weight %.3f good enough to not consider picking another one\n";
        // If currGoalEntWeight > 0, currLongTermGoalEnt is guaranteed to be non-null
        Debug(format, currLongTermGoal->Name(), currGoalEntWeight);
    }
    else
    {
        if (currLongTermGoal)
        {
            const char *format = "chose %s weighted %.3f as a long-term goal instead of %s weighted now as %.3f\n";
            Debug(format, bestNavEnt->Name(), bestWeight, currLongTermGoal->Name(), currGoalEntWeight);
        }
        else
        {
            Debug("chose %s weighted %.3f as a new long-term goal\n", bestNavEnt->Name(), bestWeight);
        }
    }

    if (!currLongTermGoal || !currLongTermGoal->IsBasedOnNavEntity(bestNavEnt))
        SetLongTermGoal(bestNavEnt);

    // Was doing search
    if (longTermGoalSearchTimeout <= level.time)
    {
        longTermGoalSearchTimeout = level.time + longTermGoalSearchPeriod;
        longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
        shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
        shortTermGoalReevaluationTimeout = level.time + shortTermGoalSearchPeriod + shortTermGoalReevaluationTimeout;
    }
    else
    {
        longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    }
}

float AiBaseBrain::SelectShortTermGoalCandidates(const Goal *currShortTermGoal, GoalCandidates &result)
{
    result.clear();
    float currGoalEntWeight = 0.0f;

    bool canPickupItems = (self->r.client->ps.pmove.stats[PM_STAT_FEATURES] & PMFEAT_ITEMPICK) != 0;

    vec3_t forward;
    AngleVectors(self->s.angles, forward, nullptr, nullptr);

    FOREACH_NAVENT(navEnt)
    {
        if (navEnt->IsDisabled())
            continue;

        // Do not predict short-term goal spawn (looks weird)
        if (navEnt->ToBeSpawnedLater())
            continue;

        if (navEnt->IsClient())
            continue;

        float weight = GetEntityWeight(navEnt->Id());
        if (weight <= 0.0f)
            continue;

        if (canPickupItems && navEnt->Item())
        {
            if (!G_Gametype_CanPickUpItem(navEnt->Item()) || !(navEnt->Item()->flags & ITFLAG_PICKABLE))
                continue;
        }

        // First cut off items by distance for performance reasons since this function is called quite frequently.
        // It is not very accurate in terms of level connectivity, but short-term goals are not critical.
        float dist = (navEnt->Origin() - self->s.origin).LengthFast();
        if (longTermGoal && longTermGoal->IsBasedOnNavEntity(navEnt))
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
            Vec3 botToTarget = navEnt->Origin() - self->s.origin;
            botToTarget *= 1.0f / dist;
            if (botToTarget.Dot(forward) < 0.7)
                inFront = false;
        }

        weight = weight / dist * (inFront ? 1.0f : 0.5f);

        // Store current short-term goal current weight
        if (currShortTermGoal && currShortTermGoal->IsBasedOnNavEntity(navEnt))
            currGoalEntWeight = weight;

        result.emplace_back(NavEntityAndWeight(navEnt, weight));
    }

    std::sort(result.begin(), result.end());

    return currGoalEntWeight;
}

bool AiBaseBrain::SelectShortTermReachableGoals(const Goal *currShortTermGoal, const GoalCandidates &candidates,
                                                GoalCandidates &result)
{
    result.clear();
    bool isCurrGoalReachable = false;

    for (const NavEntityAndWeight &goalAndWeight: candidates)
    {
        std::pair<unsigned, unsigned> toAndBackAasTravelTimes = FindToAndBackTravelTimes(goalAndWeight.goal->Origin());
        bool shortTermReachable = false;
        // If current and goal points are mutually reachable
        if (toAndBackAasTravelTimes.first > 0 && toAndBackAasTravelTimes.second > 0)
        {
            // Convert from AAS centiseconds
            unsigned toTravelMillis = 10 * toAndBackAasTravelTimes.first;
            unsigned backTravelMillis = 10 * toAndBackAasTravelTimes.second;
            if (goalAndWeight.goal->IsDroppedEntity())
            {
                // Ensure it will not dispose before the bot may reach it
                if (goalAndWeight.goal->Timeout() > level.time + toTravelMillis)
                {
                    if ((toTravelMillis + backTravelMillis) / 2 < AI_GOAL_SR_MILLIS)
                    {
                        result.push_back(goalAndWeight);
                        shortTermReachable = true;
                    }
                }
            }
            else if ((toTravelMillis + backTravelMillis) / 2 < AI_GOAL_SR_MILLIS)
            {
                result.push_back(goalAndWeight);
                shortTermReachable = true;
            }
        }
        if (shortTermReachable && currShortTermGoal && currShortTermGoal->IsBasedOnNavEntity(goalAndWeight.goal))
            isCurrGoalReachable = true;
    }

    return isCurrGoalReachable;
}

void AiBaseBrain::PickShortTermGoal(const Goal *currShortTermGoal)
{
    // Present special goal blocks other goals selection
    if (specialGoal)
        return;

    if (G_ISGHOSTING(self))
        return;

    if (shortTermGoalSearchTimeout > level.time && shortTermGoalReevaluationTimeout > level.time)
        return;

    // First, filter all goals by non-zero weight to choose best goals for further checks
    StaticVector<NavEntityAndWeight, MAX_NAVENTS> goalCandidates;
    float currGoalEntWeight = SelectShortTermGoalCandidates(currShortTermGoal, goalCandidates);

    if (goalCandidates.empty())
        return;

    // Always check feasibility for current short-term goal
    if (currGoalEntWeight > 0 && MayNotBeFeasibleGoal(currShortTermGoal))
        currGoalEntWeight = 0;

    // Then, filter non-zero weight goals by short-term reachability to choose best goals for feasibilty checks
    StaticVector<NavEntityAndWeight, MAX_NAVENTS> shortTermReachableGoals;
    if (!SelectShortTermReachableGoals(currShortTermGoal, goalCandidates, shortTermReachableGoals))
        currGoalEntWeight = 0;

    if (shortTermReachableGoals.empty())
        return;

    NavEntity *bestGoalEnt = nullptr;
    float bestWeight = 0.000001f;

    // Since `goalCandidates` is sorted and the filter was sequential, `shortTermReachableGoals` is sorted too
    for (auto &goalAndWeight: shortTermReachableGoals)
    {
        if (!MayNotBeFeasibleGoal(goalAndWeight.goal))
        {
            bestGoalEnt = goalAndWeight.goal;
            bestWeight = goalAndWeight.weight;
            break;
        }
    }

    if (!bestGoalEnt)
        return;

    const bool isDoingSearch = level.time <= shortTermGoalReevaluationTimeout;
    const float currToBestWeightThreshold = isDoingSearch ? 0.9f : 0.7f;

    if (currShortTermGoal && currShortTermGoal->IsBasedOnNavEntity(bestGoalEnt))
    {
        Debug("current short-term goal %s is kept as still having best weight %.3f\n", bestGoalEnt->Name(), bestWeight);
    }
    else if (currGoalEntWeight > 0 && currGoalEntWeight / bestWeight > currToBestWeightThreshold)
    {
        const char *format =
            "current short-term goal %s is kept as having weight %.3f good enough to not consider picking another one\n";
        // If currGoalEntWeight > 0, currShortTermGoal is guaranteed to be non-null
        Debug(format, currShortTermGoal->Name(), currGoalEntWeight);
    }
    else
    {
        if (currShortTermGoal)
        {
            const char *format = "chose %s weighted %.3f as a short-term goal instead of %s weighted now as %.3f\n";
            Debug(format, bestGoalEnt->Name(), bestWeight, currShortTermGoal->Name(), currGoalEntWeight);
        }
        else
        {
            Debug("chose %s weighted %.3f as a new short-term goal\n", bestGoalEnt->Name(), bestWeight);
        }
    }

    if (!currShortTermGoal || !currShortTermGoal->IsBasedOnNavEntity(bestGoalEnt))
        SetShortTermGoal(bestGoalEnt);

    if (isDoingSearch)
    {
        shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    }
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
}

void AiBaseBrain::SetLongTermGoal(NavEntity *goalEnt)
{
    longTermGoal = &localLongTermGoal;
    longTermGoal->SetToNavEntity(goalEnt, this);
    longTermGoalSearchTimeout = level.time + longTermGoalSearchPeriod;
    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    self->ai->aiRef->OnGoalSet(&localLongTermGoal);
}

void AiBaseBrain::SetShortTermGoal(NavEntity *goalEnt)
{
    shortTermGoal = &localShortTermGoal;
    shortTermGoal->SetToNavEntity(goalEnt, this);
    shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalReevaluationPeriod;
    self->ai->aiRef->OnGoalSet(&localShortTermGoal);
}

void AiBaseBrain::SetSpecialGoal(Goal *goal)
{
    specialGoal = goal;
    self->ai->aiRef->OnGoalSet(goal);
}

void AiBaseBrain::ClearLongAndShortTermGoal(const Goal *pickedGoal)
{
    longTermGoal = nullptr;
    // Request long-term goal update in next frame
    longTermGoalSearchTimeout = level.time + 1;
    longTermGoalReevaluationTimeout = level.time + longTermGoalReevaluationPeriod;
    // Clear short-term goal too
    shortTermGoal = nullptr;
    shortTermGoalSearchTimeout = level.time + shortTermGoalSearchPeriod;
    shortTermGoalReevaluationTimeout = level.time + shortTermGoalSearchPeriod + shortTermGoalReevaluationPeriod;
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

void AiBaseBrain::OnSpecialGoalReached()
{
    Debug("reached special goal %s\n", specialGoal->Name());
    OnClearSpecialGoalRequested();
}

std::pair<unsigned, unsigned> AiBaseBrain::FindToAndBackTravelTimes(const Vec3 &targetOrigin) const
{
    // We hope that target origin has been already dropped to floor, so no adjustment is required
    float *targetOriginRef = const_cast<float*>(targetOrigin.Data());
    int areaNum = FindAASAreaNum(targetOriginRef);

    if (!areaNum)
        return std::make_pair(0, 0);
    if (areaNum == currAasAreaNum)
        return std::make_pair(1, 1);

    int toAasTravelTime = FindAASTravelTimeToGoalArea(areaNum);
    if (!toAasTravelTime)
        return std::make_pair(0, 0);
    int backAasTravelTime = AAS_AreaTravelTimeToGoalArea(areaNum, targetOriginRef, currAasAreaNum, preferredAasTravelFlags);
    if (!backAasTravelTime)
        backAasTravelTime = AAS_AreaTravelTimeToGoalArea(areaNum, targetOriginRef, currAasAreaNum, allowedAasTravelFlags);
    return std::make_pair(toAasTravelTime, backAasTravelTime);
}

