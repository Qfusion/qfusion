#include "ai_base_ai.h"
#include "ai_base_brain.h"
#include "ai_ground_trace_cache.h"

Ai::Ai(edict_t *self_,
       AiBaseBrain *aiBaseBrain_,
       AiAasRouteCache *routeCache_,
       int allowedAasTravelFlags_,
       int preferredAasTravelFlags_)
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
      blockedTimeout(level.time + 15000),
      aiYawSpeed(0.0f),
      aiPitchSpeed(0.0f),
      oldYawAbsDiff(0.0f),
      oldPitchAbsDiff(0.0f)
{
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

    if (ent->classname && !Q_stricmp(ent->classname, "trigger_push"))
    {
        TouchedJumppad(ent);
        return;
    }
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

static constexpr float ANGLE_DIFF_PRECISION_THRESHOLD = 10.0f;

static inline float ComputeAngularSpeedScale(float currAngleDiff, float oldAngleDiff, float frametime)
{
    // The angle difference has the largest influence on angular speed.
    float scale = 0.0001f + 0.15f * (currAngleDiff / ANGLE_DIFF_PRECISION_THRESHOLD);
    float absDiffAccel = (currAngleDiff - oldAngleDiff) * frametime / 16.6f;
    // If the angle difference grew up, increase angular speed proportionally
    if (absDiffAccel > 0)
        scale *= 1.0f + 0.55f * std::min(1.0f, absDiffAccel / 3.0f);
    return scale;
}

void Ai::ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier /*= 1.0f*/, bool extraPrecision /*= false*/)
{
    const float currentYaw = anglemod(self->s.angles[YAW]);
    const float currentPitch = anglemod(self->s.angles[PITCH]);

    vec3_t idealAngle;
    VecToAngles(idealDirection.Data(), idealAngle);

    const float ideal_yaw = anglemod(idealAngle[YAW]);
    const float ideal_pitch = anglemod(idealAngle[PITCH]);

    aiYawSpeed *= angularSpeedMultiplier;
    aiPitchSpeed *= angularSpeedMultiplier;

    float yawAbsDiff = fabsf(currentYaw - ideal_yaw);
    float pitchAbsDiff = fabsf(currentPitch - ideal_pitch);

    if (extraPrecision)
    {
        if (yawAbsDiff < ANGLE_DIFF_PRECISION_THRESHOLD)
            aiYawSpeed *= ComputeAngularSpeedScale(yawAbsDiff, oldYawAbsDiff, game.frametime);

        if (pitchAbsDiff < ANGLE_DIFF_PRECISION_THRESHOLD)
            aiPitchSpeed *= ComputeAngularSpeedScale(pitchAbsDiff, oldPitchAbsDiff, game.frametime);
    }
    else
    {
        if (yawAbsDiff < ANGLE_DIFF_PRECISION_THRESHOLD)
            aiYawSpeed *= 0.5f;

        if (pitchAbsDiff < ANGLE_DIFF_PRECISION_THRESHOLD)
            aiPitchSpeed *= 0.5f;
    }

    oldYawAbsDiff = yawAbsDiff;
    oldPitchAbsDiff = pitchAbsDiff;

    ChangeAxisAngle(currentYaw, ideal_yaw, self->yaw_speed, &aiYawSpeed, &self->s.angles[YAW]);
    ChangeAxisAngle(currentPitch, ideal_pitch, self->yaw_speed, &aiPitchSpeed, &self->s.angles[PITCH]);
}

#define AI_YAW_ACCEL ( 95 * FRAMETIME )

void Ai::ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle)
{
    float angleMove = idealAngle - currAngle;
    float speed = edictAngleSpeed * FRAMETIME;
    if( idealAngle > currAngle )
    {
        if (angleMove >= 180)
            angleMove -= 360;
    }
    else
    {
        if (angleMove <= -180)
            angleMove += 360;
    }
    if (angleMove > 0)
    {
        if (*aiAngleSpeed > speed)
            *aiAngleSpeed = speed;
        if (angleMove < 3)
            *aiAngleSpeed += AI_YAW_ACCEL/4.0;
        else
            *aiAngleSpeed += AI_YAW_ACCEL;
    }
    else
    {
        if (*aiAngleSpeed < -speed)
            *aiAngleSpeed = -speed;
        if (angleMove > -3)
            *aiAngleSpeed -= AI_YAW_ACCEL;
        else
            *aiAngleSpeed -= AI_YAW_ACCEL/4.0;
    }

    angleMove = *aiAngleSpeed;
    *changedAngle = anglemod(currAngle + angleMove);
}
