#include "ai_base_ai.h"
#include "ai_base_brain.h"
#include "ai_ground_trace_cache.h"

Ai::Ai(edict_t *self_,
       AiBaseBrain *aiBaseBrain_,
       AiAasRouteCache *routeCache_,
       int allowedAasTravelFlags_,
       int preferredAasTravelFlags_,
       float yawSpeed,
       float pitchSpeed)
    : EdictRef(self_),
      aiBaseBrain(aiBaseBrain_),
      routeCache(routeCache_),
      aasWorld(AiAasWorld::Instance()),
      currAasAreaNum(0),
      droppedToFloorAasAreaNum(0),
      droppedToFloorOrigin(0, 0, 0),
      allowedAasTravelFlags(allowedAasTravelFlags_),
      preferredAasTravelFlags(preferredAasTravelFlags_),
      distanceToNextReachStart(std::numeric_limits<float>::infinity()),
      blockedTimeout(level.time + 15000)
{
    angularViewSpeed[YAW] = yawSpeed;
    angularViewSpeed[PITCH] = pitchSpeed;
    angularViewSpeed[ROLL] = 999999.0f;
}

void Ai::SetFrameAffinity(unsigned modulo, unsigned offset)
{
    frameAffinityModulo = modulo;
    frameAffinityOffset = offset;
    aiBaseBrain->SetFrameAffinity(modulo, offset);
}

// These getters cannot be defined in headers due to incomplete AiBaseBrain class definition

int Ai::NavTargetAasAreaNum() const
{
    return aiBaseBrain->NavTargetAasAreaNum();
}

Vec3 Ai::NavTargetOrigin() const
{
    return aiBaseBrain->NavTargetOrigin();
}

void Ai::ResetNavigation()
{
    distanceToNextReachStart = std::numeric_limits<float>::infinity();

    currAasAreaNum = aasWorld->FindAreaNum(self);
    nextReaches.clear();

    blockedTimeout = level.time + BLOCKED_TIMEOUT;
}

void Ai::UpdateReachCache(int reachedAreaNum)
{
    if (!aiBaseBrain->HasNavTarget())
        return;

    const int goalAreaNum = NavTargetAasAreaNum();
    // First skip reaches to reached area
    unsigned i = 0;
    for (i = 0; i < nextReaches.size(); ++i)
    {
        if (nextReaches[i].areanum == reachedAreaNum)
            break;
    }
    // Remove all reaches including i-th
    if (i != nextReaches.size())
        nextReaches.erase(nextReaches.begin(), nextReaches.begin() + i + 1);
    else
        nextReaches.clear();

    int areaNum = nextReaches.empty() ? reachedAreaNum : nextReaches.back().areanum;
    while (areaNum != goalAreaNum && nextReaches.size() != nextReaches.capacity())
    {
        int reachNum = routeCache->ReachabilityToGoalArea(areaNum, goalAreaNum, preferredAasTravelFlags);
        if (!reachNum)
            reachNum = routeCache->ReachabilityToGoalArea(areaNum, goalAreaNum, allowedAasTravelFlags);
        // We hope we'll be pushed in some other area during movement, and goal area will become reachable. Leave as is.
        if (!reachNum)
            break;
        if (reachNum > aasWorld->NumReachabilities())
            break;
        const aas_reachability_t &reach = aasWorld->Reachabilities()[reachNum];
        areaNum = reach.areanum;
        nextReaches.push_back(reach);
    }
}

void Ai::CheckReachedArea()
{
    const int actualAasAreaNum = aasWorld->FindAreaNum(self);

    // It deserves a separate statement (may modify droppedToFloorOrigin)
    bool droppedToFloor = AiGroundTraceCache::Instance()->TryDropToFloor(self, 96.0f, droppedToFloorOrigin.Data());
    if (droppedToFloor)
    {
        droppedToFloorAasAreaNum = aasWorld->FindAreaNum(droppedToFloorOrigin);
        if (!droppedToFloorAasAreaNum)
        {
            // Revert the dropping attempt
            VectorCopy(self->s.origin, droppedToFloorOrigin.Data());
            droppedToFloorAasAreaNum = actualAasAreaNum;
        }
    }
    else
        droppedToFloorAasAreaNum = actualAasAreaNum;

    // Current aas area num did not changed
    if (actualAasAreaNum)
    {
        if (currAasAreaNum != actualAasAreaNum)
        {
            UpdateReachCache(actualAasAreaNum);
        }
    }
    else
    {
        nextReaches.clear();
    }

    currAasAreaNum = actualAasAreaNum;

    if (!nextReaches.empty())
    {
        distanceToNextReachStart = DistanceSquared(nextReaches.front().start, self->s.origin);
        if (distanceToNextReachStart > 1)
            distanceToNextReachStart = 1.0f / Q_RSqrt(distanceToNextReachStart);
    }
}

int Ai::CheckTravelTimeMillis(const Vec3& from, const Vec3 &to, bool allowUnreachable)
{
    const AiAasWorld *aasWorld = AiAasWorld::Instance();

    // We try to use the same checks the TacticalSpotsRegistry performs to find spots.
    // If a spot is not reachable, it is an bug,
    // because a reachability must have been checked by the spots registry first in a few preceeding calls.

    int fromAreaNum;
    constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((from - self->s.origin).SquaredLength() < squareDistanceError)
        fromAreaNum = aasWorld->FindAreaNum(self);
    else
        fromAreaNum = aasWorld->FindAreaNum(from);

    if (!fromAreaNum)
    {
        if (allowUnreachable)
            return 0;

        FailWith("CheckTravelTimeMillis(): Can't find `from` AAS area");
    }

    const int toAreaNum = aasWorld->FindAreaNum(to.Data());
    if (!toAreaNum)
    {
        if (allowUnreachable)
            return 0;

        FailWith("CheckTravelTimeMillis(): Can't find `to` AAS area");
    }

    for (int flags: { self->ai->aiRef->PreferredTravelFlags(), self->ai->aiRef->AllowedTravelFlags() })
    {
        if (int aasTravelTime = routeCache->TravelTimeToGoalArea(fromAreaNum, toAreaNum, flags))
            return 10U * aasTravelTime;
    }

    if (allowUnreachable)
        return 0;

    FailWith("CheckTravelTimeMillis(): Can't find travel time %d->%d\n", fromAreaNum, toAreaNum);
}

void Ai::CategorizePosition()
{
    CheckReachedArea();

    bool stepping = Ai::IsStep(self);

    self->was_swim = self->is_swim;
    self->was_step = self->is_step;

    self->is_ladder = aasWorld->AreaLadder(currAasAreaNum);

    G_CategorizePosition(self);
    if (self->waterlevel > 2 || (self->waterlevel && !stepping))
    {
        self->is_swim = true;
        self->is_step = false;
        return;
    }

    self->is_swim = false;
    self->is_step = stepping;
}

void Ai::OnNavTargetSet(NavTarget *navTarget)
{
    if (!currAasAreaNum)
    {
        currAasAreaNum = aasWorld->FindAreaNum(self);
        if (!currAasAreaNum)
        {
            Debug("Still can't find curr aas area num");
        }
    }

    nextReaches.clear();
    UpdateReachCache(currAasAreaNum);
}

void Ai::OnNavTargetReset()
{
    nextReaches.clear();
}

void Ai::TouchedEntity(edict_t *ent)
{
    if (aiBaseBrain->HandleNavTargetTouch(ent))
    {
        // Clear goal area num to ensure bot will not repeatedly try to reach that area even if he has no goals.
        // Usually it gets overwritten in this or next frame, when bot picks up next goal,
        // but sometimes there are no other goals to pick up.
        OnNavTargetReset();
        return;
    }
    TouchedOtherEntity(ent);
}

void Ai::Frame()
{
    // Call super method first
    AiFrameAwareUpdatable::Frame();

    // Call brain Update() (Frame() and, maybe Think())
    aiBaseBrain->Update();

    if (level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities)
    {
        self->nextThink = level.time + game.snapFrameTime;
        return;
    }
}

void Ai::Think()
{
    // check for being blocked
    if (!G_ISGHOSTING(self))
    {
        CategorizePosition();

        // TODO: Check whether we are camping/holding a spot
        if (VectorLengthFast(self->velocity) > 37)
            blockedTimeout = level.time + BLOCKED_TIMEOUT;

        // if completely stuck somewhere
        if (blockedTimeout < level.time)
        {
            OnBlockedTimeout();
            return;
        }
    }
}

void Ai::TestMove(MoveTestResult *moveTestResult, int currAasAreaNum_, const vec3_t forward) const
{
    // These values will be returned by default
    moveTestResult->canWalk = 0;
    moveTestResult->canFall = 0;
    moveTestResult->canJump = 0;
    moveTestResult->fallDepth = 0;

    if (!aasWorld->AreaGrounded(currAasAreaNum_))
        return;

    if (!currAasAreaNum)
        return;

    constexpr int MAX_TRACED_AREAS = 6;
    int tracedAreas[MAX_TRACED_AREAS];
    Vec3 traceEnd = 36.0f * Vec3(forward) + self->s.origin;
    int numTracedAreas = aasWorld->TraceAreas(self->s.origin, traceEnd.Data(), tracedAreas, MAX_TRACED_AREAS);

    if (!numTracedAreas)
        return;

    // Trace ends still in current area
    if (numTracedAreas == 1)
    {
        moveTestResult->canWalk = 1;
        moveTestResult->canFall = 0;
        moveTestResult->canJump = 1;
        return;
    }

    int traceFlags = TFL_WALK | TFL_WALKOFFLEDGE | TFL_BARRIERJUMP;
    float fallDepth = 0;

    for (int i = 0; i < numTracedAreas - 1; ++i)
    {
        const int nextAreaNum = tracedAreas[i + 1];
        // Trace ends in solid
        if (!nextAreaNum)
            return;

        const aas_areasettings_t &currAreaSettings = aasWorld->AreaSettings()[tracedAreas[i]];
        if (!currAreaSettings.numreachableareas)
            return; // blocked

        int reachFlags = 0;
        for (int j = 0; j < currAreaSettings.numreachableareas; ++j)
        {
            const aas_reachability_t &reach = aasWorld->Reachabilities()[currAreaSettings.firstreachablearea + j];
            if (reach.areanum == nextAreaNum)
            {
                switch (reach.traveltype)
                {
                    case TRAVEL_WALK:
                        // Bot can escape using a teleporter
                    case TRAVEL_TELEPORT:
                        reachFlags |= TFL_WALK;
                        break;
                    case TRAVEL_WALKOFFLEDGE:
                        fallDepth += reach.start[2] - reach.end[2];
                        reachFlags |= TFL_WALKOFFLEDGE;
                        break;
                    case TRAVEL_BARRIERJUMP:
                    case TRAVEL_DOUBLEJUMP:
                        reachFlags |= TFL_BARRIERJUMP;
                        break;
                }
            }
        }
        traceFlags &= reachFlags;
        // Reject early
        if (!traceFlags)
            return;
    }

    moveTestResult->canWalk = 0 != (traceFlags & TFL_WALK);
    moveTestResult->canFall = 0 != (traceFlags & TFL_WALKOFFLEDGE);
    moveTestResult->canJump = 0 != (traceFlags & TFL_BARRIERJUMP);
    moveTestResult->fallDepth = fallDepth;
};

void Ai::TestClosePlace()
{
    if (!currAasAreaNum)
    {
        closeAreaProps.frontTest.Clear();
        closeAreaProps.backTest.Clear();
        closeAreaProps.rightTest.Clear();
        closeAreaProps.leftTest.Clear();
        return;
    }
    // TODO: Try to shortcut using area boundaries

    vec3_t angles, forward;
    VectorCopy(self->s.angles, angles);

    AngleVectors(angles, forward, nullptr, nullptr);
    TestMove(&closeAreaProps.frontTest, currAasAreaNum, forward);

    angles[1] = self->s.angles[1] + 90;
    AngleVectors(angles, forward, nullptr, nullptr);
    TestMove(&closeAreaProps.leftTest, currAasAreaNum, forward);

    angles[1] = self->s.angles[1] - 90;
    AngleVectors(angles, forward, nullptr, nullptr);
    TestMove(&closeAreaProps.rightTest, currAasAreaNum, forward);

    angles[1] = self->s.angles[1] - 180;
    AngleVectors(angles, forward, nullptr, nullptr);
    TestMove(&closeAreaProps.backTest, currAasAreaNum, forward);
}

//===================
//  AI_IsStep
//  Checks the floor one step below the player. Used to detect
//  if the player is really falling or just walking down a stair.
//===================
bool Ai::IsStep(edict_t *ent)
{
    vec3_t point;
    trace_t	trace;

    //determine a point below
    point[0] = ent->s.origin[0];
    point[1] = ent->s.origin[1];
    point[2] = ent->s.origin[2] - ( 1.6*AI_STEPSIZE );

    //trace to point
    G_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, MASK_PLAYERSOLID );

    if( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid )
        return false;

    //found solid.
    return true;
}

float Ai::GetChangedAngle(float oldAngle, float desiredAngle, float angulasSpeedMultiplier, int angleIndex)
{
    if (oldAngle == desiredAngle)
        return oldAngle;

    float maxAngularMove = angulasSpeedMultiplier * angularViewSpeed[angleIndex] * FRAMETIME;
    float angularMove = AngleNormalize180(desiredAngle - oldAngle);
    if (angularMove < -maxAngularMove)
        angularMove = -maxAngularMove;
    else if (angularMove > maxAngularMove)
        angularMove = maxAngularMove;

    return AngleNormalize180(oldAngle + angularMove);
}

Vec3 Ai::GetNewViewAngles(const vec3_t oldAngles, const Vec3 &desiredDirection, float angularSpeedMultiplier)
{
    // Based on turret script code

    // For those trying to learn working with angles
    // Vec3.x is the PITCH angle (up and down rotation)
    // Vec3.y is the YAW angle (left and right rotation)
    // Vec3.z is the ROLL angle (left and right inclination)

    vec3_t newAngles, desiredAngles;
    VecToAngles(desiredDirection.Data(), desiredAngles);

    // Normalize180 all angles so they can be compared
    for (int i = 0; i < 3; ++i)
    {
        newAngles[i] = AngleNormalize180(oldAngles[i]);
        desiredAngles[i] = AngleNormalize180(desiredAngles[i]);
    }

    // Rotate the entity angles to the desired angles
    if (!VectorCompare(newAngles, desiredAngles))
    {
        newAngles[YAW] = GetChangedAngle(newAngles[YAW], desiredAngles[YAW], angularSpeedMultiplier, YAW);
        newAngles[PITCH] = GetChangedAngle(newAngles[PITCH], desiredAngles[PITCH], angularSpeedMultiplier, PITCH);
    }

    return Vec3(newAngles);
}
