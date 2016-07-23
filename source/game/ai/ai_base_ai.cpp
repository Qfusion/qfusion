#include "ai_base_ai.h"
#include "ai_base_brain.h"

Ai::Ai(edict_t *self, int allowedAasTravelFlags, int preferredAasTravelFlags)
    : EdictRef(self),
      aiBaseBrain(nullptr), // Must be set in a subclass constructor
      currAasAreaNum(0),
      goalAasAreaNum(0),
      goalTargetPoint(0, 0, 0),
      allowedAasTravelFlags(allowedAasTravelFlags),
      preferredAasTravelFlags(preferredAasTravelFlags),
      distanceToNextReachStart(std::numeric_limits<float>::infinity()),
      blockedTimeout(level.time + 15000),
      aiYawSpeed(0.0f),
      aiPitchSpeed(0.0f)
{
    // TODO: Modify preferred aas travel flags if there is no selfdamage for excessive rocketjumping
}

void Ai::Debug(const char *format, ...) const
{
    va_list va;
    va_start(va, format);
    AI_Debugv(Nick(), format, va);
    va_end(va);
}

void Ai::FailWith(const char *format, ...) const
{
    va_list va;
    va_start(va, format);
    AI_Debugv(Nick(), format, va);
    va_end(va);
    abort();
}

void Ai::SetFrameAffinity(unsigned modulo, unsigned offset)
{
    frameAffinityModulo = modulo;
    frameAffinityOffset = offset;
    aiBaseBrain->SetFrameAffinity(modulo, offset);
}

void Ai::ResetNavigation()
{
    distanceToNextReachStart = std::numeric_limits<float>::infinity();

    currAasAreaNum = FindAASAreaNum(self);
    nextReaches.clear();
    goalAasAreaNum = 0;

    aiBaseBrain->ClearAllGoals();

    blockedTimeout = level.time + BLOCKED_TIMEOUT;
}

void Ai::UpdateReachCache(int reachedAreaNum)
{
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

    int areaNum;
    float *origin;
    if (nextReaches.empty())
    {
        areaNum = reachedAreaNum;
        origin = self->s.origin;
    }
    else
    {
        areaNum = nextReaches.back().areanum;
        origin = nextReaches.back().end;
    }
    while (areaNum != goalAasAreaNum && nextReaches.size() != nextReaches.capacity())
    {
        int reachNum = FindAASReachabilityToGoalArea(areaNum, origin, goalAasAreaNum);
        // We hope we'll be pushed in some other area during movement, and goal area will become reachable. Leave as is.
        if (!reachNum)
            break;
        aas_reachability_t reach;
        AAS_ReachabilityFromNum(reachNum, &reach);
        areaNum = reach.areanum;
        origin = reach.end;
        nextReaches.push_back(reach);
    }
}

void Ai::CheckReachedArea()
{
    const int actualAasAreaNum = AAS_PointAreaNum(self->s.origin);
    // Current aas area num did not changed
    if (actualAasAreaNum)
    {
        if (currAasAreaNum != actualAasAreaNum)
        {
            UpdateReachCache(actualAasAreaNum);
        }
        currAasAreaTravelFlags = AAS_AreaContentsTravelFlags(actualAasAreaNum);
    }
    else
    {
        nextReaches.clear();
        currAasAreaTravelFlags = TFL_INVALID;
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

    self->is_ladder = currAasAreaNum ? AAS_AreaLadder(currAasAreaNum) != 0 : false;

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

void Ai::ClearAllGoals()
{
    // This clears short-term goal too
    aiBaseBrain->ClearAllGoals();
    nextReaches.clear();
}

void Ai::OnGoalSet(Goal *goalEnt)
{
    if (!currAasAreaNum)
    {
        currAasAreaNum = FindAASAreaNum(self);
        if (!currAasAreaNum)
        {
            Debug("Still can't find curr aas area num");
        }
    }

    goalAasAreaNum = goalEnt->AasAreaNum();
    goalTargetPoint = goalEnt->Origin();
    goalTargetPoint.Z() += playerbox_stand_viewheight;

    nextReaches.clear();
    UpdateReachCache(currAasAreaNum);
}

void Ai::TouchedEntity(edict_t *ent)
{
    if (aiBaseBrain->HandleGoalTouch(ent))
    {
        TouchedGoal(ent);
        // Clear goal area num to ensure bot will not repeatedly try to reach that area even if he has no goals.
        // Usually it gets overwritten in this or next frame, when bot picks up next goal,
        // but sometimes there are no other goals to pick up.
        nextReaches.clear();
        goalAasAreaNum = 0;
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

        // Update currAasAreaNum value of AiBaseBrain
        // Ai::Think() returns to Ai::Frame()
        // Ai::Frame() calls AiBaseBrain::Frame()
        // AiBaseBrain::Frame() calls AiBaseBrain::Think() in this frame
        aiBaseBrain->currAasAreaNum = currAasAreaNum;

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

void Ai::TestMove(MoveTestResult *moveTestResult, int currAasAreaNum, const vec3_t forward) const
{
    // These values will be returned by default
    moveTestResult->canWalk = 0;
    moveTestResult->canFall = 0;
    moveTestResult->canJump = 0;
    moveTestResult->fallDepth = 0;

    if (!AAS_AreaGrounded(currAasAreaNum))
        return;

    if (!currAasAreaNum)
        return;

    constexpr int MAX_TRACED_AREAS = 6;
    int tracedAreas[MAX_TRACED_AREAS];
    Vec3 traceEnd = 36.0f * Vec3(forward) + self->s.origin;
    int numTracedAreas = AAS_TraceAreas(self->s.origin, traceEnd.Data(), tracedAreas, nullptr, MAX_TRACED_AREAS);

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

        const aas_areasettings_t &currAreaSettings = aasworld.areasettings[tracedAreas[i]];
        if (!currAreaSettings.numreachableareas)
            return; // blocked

        int reachFlags = 0;
        for (int j = 0; j < currAreaSettings.numreachableareas; ++j)
        {
            const aas_reachability_t &reach = aasworld.reachability[currAreaSettings.firstreachablearea + j];
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

void Ai::ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier /*= 1.0f*/)
{
    const float currentYaw = anglemod(self->s.angles[YAW]);
    const float currentPitch = anglemod(self->s.angles[PITCH]);

    vec3_t idealAngle;
    VecToAngles(idealDirection.Data(), idealAngle);

    const float ideal_yaw = anglemod(idealAngle[YAW]);
    const float ideal_pitch = anglemod(idealAngle[PITCH]);

    aiYawSpeed *= angularSpeedMultiplier;
    aiPitchSpeed *= angularSpeedMultiplier;

    if (fabsf(currentYaw - ideal_yaw) < 10)
    {
        aiYawSpeed *= 0.5;
    }
    if (fabsf(currentPitch - ideal_pitch) < 10)
    {
        aiPitchSpeed *= 0.5;
    }

    ChangeAxisAngle(currentYaw, ideal_yaw, self->yaw_speed, &aiYawSpeed, &self->s.angles[YAW]);
    ChangeAxisAngle(currentPitch, ideal_pitch, self->yaw_speed, &aiPitchSpeed, &self->s.angles[PITCH]);
}

#define AI_YAW_ACCEL ( 95 * FRAMETIME )

void Ai::ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle)
{
    float angleMove, speed;
    if (fabsf(currAngle - idealAngle) > 1)
    {
        angleMove = idealAngle - currAngle;
        speed = edictAngleSpeed * FRAMETIME;
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
                *aiAngleSpeed -= AI_YAW_ACCEL/4.0;
            else
                *aiAngleSpeed -= AI_YAW_ACCEL;
        }

        angleMove = *aiAngleSpeed;
        *changedAngle = anglemod(currAngle + angleMove);
    }
}
