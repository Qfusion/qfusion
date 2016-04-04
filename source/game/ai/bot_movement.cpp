#include "bot.h"
#include "aas.h"

void Bot::Move(usercmd_t *ucmd)
{
    if (currAasAreaNum == 0 || goalAasAreaNum == 0)
        return;

    if (hasTriggeredRj && rjTimeout <= level.time)
        hasTriggeredRj = false;

    CheckPendingLandingDashTimedOut();

    isOnGroundThisFrame = self->groundentity != nullptr;

    aas_areainfo_t currAreaInfo;
    AAS_AreaInfo(currAasAreaNum, &currAreaInfo);
    const int currAreaContents = currAreaInfo.contents;

    Vec3 moveVec(self->velocity);  // Use as a default one
    if (currAasAreaNum != goalAasAreaNum)
    {
        if (!nextReaches.empty())
        {
            const auto &nextReach = nextReaches.front();
            if (!IsCloseToReachStart())
                moveVec = Vec3(nextReach.start) - self->s.origin;
            else
            {
                Vec3 linkVec(nextReach.end);
                linkVec -= nextReach.start;
                linkVec.NormalizeFast();
                moveVec = (16.0f * linkVec + nextReach.end) - self->s.origin;
            }
        }
    }
    else
    {
        moveVec = Vec3(self->s.origin) - goalTargetPoint;
    }

    if (self->is_ladder)
    {
        MoveOnLadder(&moveVec, ucmd);
    }
    else if (currAreaContents & AREACONTENTS_JUMPPAD)
    {
        MoveEnteringJumppad(&moveVec, ucmd);
    }
    else if (hasTriggeredJumppad)
    {
        MoveRidingJummpad(&moveVec, ucmd);
    }
    // Platform riding - No move, riding elevator
    else if (currAreaContents & AREACONTENTS_MOVER)
    {
        if (IsCloseToReachStart())
            MoveEnteringPlatform(&moveVec, ucmd);
        else
            MoveRidingPlatform(&moveVec, ucmd);
    }
    // Falling off ledge or jumping
    else if (!self->groundentity && !self->is_step && !self->is_swim && !isBunnyHopping)
    {
        MoveFallingOrJumping(&moveVec, ucmd);
    }
    else // standard movement
    {
        // starting a rocket jump
        if (!nextReaches.empty() && IsCloseToReachStart() && nextReaches.front().traveltype == TRAVEL_ROCKETJUMP)
        {
            MoveStartingARocketjump(&moveVec, ucmd);
        }
        else if (self->is_swim)
        {
            MoveSwimming(&moveVec, ucmd);
        }
        else
        {
            MoveGenericRunning(&moveVec, ucmd);
        }
    }

    TryMoveAwayIfBlocked(ucmd);

    CheckTargetReached();

    if (!hasPendingLookAtPoint)
    {
        float turnSpeedMultiplier = requestedViewTurnSpeedMultiplier;
        if (AimEnemy())
        {
            moveVec.NormalizeFast();
            Vec3 toEnemy(AimEnemy()->ent->s.origin);
            toEnemy -= self->s.origin;
            float squareDistanceToEnemy = toEnemy.SquaredLength();
            if (squareDistanceToEnemy > 1)
            {
                float invDistanceToEnemy = Q_RSqrt(squareDistanceToEnemy);
                toEnemy *= invDistanceToEnemy;
                if (moveVec.Dot(toEnemy) < -0.3f)
                {
                    // Check whether we should center view to prevent looking at the sky or a floor while spinning
                    float factor = fabsf(self->s.origin[2] - AimEnemy()->ent->s.origin[2]) * invDistanceToEnemy;
                    // If distance to enemy is 4x more than height difference, center view
                    if (factor < 0.25f)
                    {
                        moveVec.z() *= 0.0001f;
                    }
                    ucmd->forwardmove *= -1;
                    moveVec *= -1;
                    turnSpeedMultiplier = 1.35f;
                }
            }
        }
        ChangeAngle(moveVec, turnSpeedMultiplier);
    }

    wasOnGroundPrevFrame = isOnGroundThisFrame;
}

void Bot::TryMoveAwayIfBlocked(usercmd_t *ucmd)
{
    // Make sure that blocked timeout start counting down and the bot is blocked for at least 1000 millis
    if (blockedTimeout - level.time > BLOCKED_TIMEOUT - 1000)
        return;

    // Already turning
    if (hasPendingLookAtPoint)
        return;

    // Try to get current aas area again (we may did a little move, and AAS area may become available)
    if (currAasAreaNum == 0)
        currAasAreaNum = FindCurrAASAreaNum();

    // Still can't find current area or still is blocked, try to move in a random direction
    if (currAasAreaNum == 0 || blockedTimeout - level.time < BLOCKED_TIMEOUT - 3000)
    {
        // We use different randoms to make moves independent
        if (random() > 0.8f)
        {
            // Try either forwardmove or sidemove
            if (random() > 0.5f)
                ucmd->forwardmove = random() > 0.5f ? -1 : 1;
            else
                ucmd->sidemove = random() > 0.5f ? -1 : 1;
        }
        else
        {
            // Try both forwardmove and sidemove
            ucmd->forwardmove = random() > 0.5f ? -1 : 1;
            ucmd->sidemove = random() > 0.5f ? -1 : 1;
        }
        // These moves are mutual-exclusive
        float r = random();
        if (r > 0.8f)
            ucmd->buttons |= BUTTON_SPECIAL;
        else if (r > 0.6f)
            ucmd->upmove = 1;
        else if (r > 0.4f)
            ucmd->upmove = -1;

        return;
    }

    aas_areainfo_t currAreaInfo;
    AAS_AreaInfo(currAasAreaNum, &currAreaInfo);

    // Way to this point should not be blocked
    SetPendingLookAtPoint(Vec3(currAreaInfo.center), 1.5f);
    ucmd->forwardmove = 1;
    ucmd->buttons |= BUTTON_SPECIAL;
}

void Bot::MoveOnLadder(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;
}

void Bot::MoveEnteringJumppad(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (!hasTriggeredJumppad)
    {
        if (!nextReaches.empty())
        {
            jumppadDestAreaNum = nextReaches.front().areanum;
            VectorCopy(nextReaches.front().end, jumppadReachEndPoint.data());
            // Look at the destination point
            SetPendingLookAtPoint(jumppadReachEndPoint);
            unsigned approxFlightTime = 2 * (unsigned) DistanceFast(nextReaches.front().start, nextReaches.front().end);
            jumppadMoveTimeout = level.time + approxFlightTime;
        }
        else
        {
            jumppadDestAreaNum = 0;
            VectorSet(jumppadReachEndPoint.data(), INFINITY, INFINITY, INFINITY);
            jumppadMoveTimeout = level.time + 1000;
        }

        ucmd->forwardmove = 1;
        hasTriggeredJumppad = true;
    }
}

void Bot::MoveRidingJummpad(Vec3 *moveVec, usercmd_t *ucmd)
{
    // First check whether bot finally landed to some area
    if (self->groundentity)
    {
        hasTriggeredJumppad = false;
        ucmd->forwardmove = 1;
        return;
    }

    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (jumppadMoveTimeout <= level.time)
    {
        if (jumppadDestAreaNum)
        {
            TryLandOnArea(jumppadDestAreaNum, moveVec, ucmd);
            return;
        }
        // TryLandOnNearbyAreas() is expensive. TODO: Use timeout between calls based on current speed
        TryLandOnNearbyAreas(moveVec, ucmd);
    }
}

static inline float CoordDistance(float coord, float coordMins, float coordMaxs)
{
    if (coord < coordMins)
        return coordMins - coord;
    if (coord > coordMaxs)
        return coord - coordMaxs;
    return 0;
}

static float SquareDistanceToBoxBottom(const vec3_t origin, const vec3_t boxMins, const vec3_t boxMaxs)
{
    float xDist = CoordDistance(origin[0], boxMins[0], boxMaxs[0]);
    float yDist = CoordDistance(origin[1], boxMins[1], boxMaxs[2]);
    float zDist = origin[2] - boxMins[2];
    return xDist * xDist + yDist * yDist + zDist * zDist;
}

void Bot::TryLandOnNearbyAreas(Vec3 *moveVec, usercmd_t *ucmd)
{
    Vec3 bboxMins(self->s.origin), bboxMaxs(self->s.origin);
    bboxMins += Vec3(-128, -128, -128);
    bboxMaxs += Vec3(+128, +128, +128);

    constexpr int MAX_LANDING_AREAS = 16;
    int areas[MAX_LANDING_AREAS];
    int groundedAreas[MAX_LANDING_AREAS];
    float distanceToAreas[MAX_LANDING_AREAS];

    int numAllAreas = AAS_BBoxAreas(bboxMins.data(), bboxMaxs.data(), areas, MAX_LANDING_AREAS);
    if (!numAllAreas)
        return;

    int numGroundedAreas = 0;
    for (int i = 0; i < numAllAreas; ++i)
    {
        if (AAS_AreaGrounded(areas[i]))
            groundedAreas[numGroundedAreas++] = areas[i];
    }

    // Sort areas by distance from bot to area bottom

    for (int i = 0; i < numGroundedAreas; ++i)
    {
        const aas_area_t &area = aasworld.areas[groundedAreas[i]];
        distanceToAreas[i] = SquareDistanceToBoxBottom(self->s.origin, area.mins, area.maxs);
    }

    for (int i = 1; i < numGroundedAreas; ++i)
    {
        for (int j = i; j > 0 && distanceToAreas[j - 1] > distanceToAreas[j]; --j)
        {
            std::swap(groundedAreas[j], groundedAreas[j - 1]);
            std::swap(distanceToAreas[j], distanceToAreas[j - 1]);
        }
    }

    for (int i = 0; i < numGroundedAreas; ++i)
    {
        if (TryLandOnArea(areas[i], moveVec, ucmd))
            return;
    }
    // Do not press keys each frame
    float r;
    if ((r = random()) > 0.8)
        ucmd->forwardmove = r > 0.9 ? -1 : 1;
    if ((r = random()) > 0.8)
        ucmd->sidemove = r > 0.9 ? -1 : 1;
    if (random() > 0.8)
        ucmd->buttons |= BUTTON_SPECIAL;
}

bool Bot::TryLandOnArea(int areaNum, Vec3 *moveVec, usercmd_t *ucmd)
{
    Vec3 areaPoint(aasworld.areas[areaNum].center);

    // Lower area point to a bottom of area. Area mins/maxs are absolute.
    areaPoint.z() = aasworld.areas[areaNum].mins[2];
    // Do not try to "land" on upper areas
    if (areaPoint.z() > self->s.origin[2])
        return false;

    // We have to offset traced end point since we do not test a zero-width ray
    Vec3 areaPointToBotVec(self->s.origin);
    areaPointToBotVec -= areaPoint;
    areaPointToBotVec.NormalizeFast();
    Vec3 traceEnd(areaPoint);

    trace_t trace;
    G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, traceEnd.data(), self, MASK_AISOLID);
    if (trace.fraction == 1.0f)
    {
        ucmd->forwardmove = 1;
        *moveVec = -areaPointToBotVec;
        return true;
    }

    return false;
}

void Bot::MoveRidingPlatform(Vec3 *moveVec, usercmd_t *ucmd)
{
    /*
    vec3_t v1, v2;
    VectorCopy(self->s.origin, v1);
    VectorCopy(nodes[self->ai->next_node].origin, v2);
    v1[2] = v2[2] = 0;
    if (DistanceFast(v1, v2) > 32 && DotProduct(lookdir, pathdir) > BOT_FORWARD_EPSILON)
        ucmd->forwardmove = 1; // walk to center

     */

    // TODO: Change to "MoveHoldingASpot(Vec3 spotOrigin, int spotAreaNum);
    // TODO: Spot origin and area may be updated

    ucmd->buttons |= BUTTON_WALK;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    moveVec->z() = 0;
}

void Bot::MoveEnteringPlatform(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;

    // TODO: Change platform
}

void Bot::MoveFallingOrJumping(Vec3 *moveVec, usercmd_t *ucmd)
{
    // TODO: Use aircontrol to reach destination

    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 1;
}

void Bot::MoveStartingARocketjump(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    // TODO: Switch to a weapon
    if (!hasTriggeredRj && self->groundentity && (self->s.weapon == WEAP_ROCKETLAUNCHER))
    {
        self->s.angles[PITCH] = 170;
        ucmd->upmove = 1;
        ucmd->buttons |= BUTTON_ATTACK;
        hasTriggeredRj = true;
        rjTimeout = level.time + 500;
    }
}

void Bot::MoveSwimming(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = true;

    // TODO: Check reachibility, if we are close, exit water
    //if (!(G_PointContents(nodes[self->ai->next_node].origin) & MASK_WATER))  // Exit water
    //    ucmd->upmove = 1;
}

void Bot::CheckAndTryAvoidObstacles(Vec3 *moveVec, usercmd_t *ucmd, float speed)
{
    float moveVecSqLen = moveVec->SquaredLength();
    if (moveVecSqLen < 0.01f)
        return;

    *moveVec *= Q_RSqrt(moveVecSqLen);

    Vec3 baseOffsetVec(*moveVec);
    baseOffsetVec *= 24.0f + 96.0f * BoundedFraction(speed, 900);

    Vec3 forwardVec(baseOffsetVec);
    forwardVec += self->s.origin;

    float *const mins = vec3_origin;
    float *const maxs = playerbox_stand_maxs;

    trace_t trace;
    G_Trace(&trace, self->s.origin, mins, maxs, forwardVec.data(), self, MASK_AISOLID);

    if (trace.fraction == 1.0f)
        return;

    // If we are in air, check whether we may crouch to prevent bumping a ceiling by head
    // We do not switch to crouch movement style, since we are still in air and have bunny speed
    if (!self->groundentity)
    {
        trace_t crouchTrace;
        G_Trace(&crouchTrace, self->s.origin, nullptr, playerbox_crouch_maxs, forwardVec.data(), self, MASK_AISOLID);
        if (crouchTrace.fraction == 1.0f)
        {
            ucmd->upmove = -1;
            return;
        }
    }

    // Trace both a bit left and a bit right directions. Don't reject early (otherwise one side will be always preferred)
    float angleStep = 35.0f - 15.0f * BoundedFraction(speed, 900);

    float bestFraction = trace.fraction;
    float *bestVec = baseOffsetVec.data();

    float sign = -1.0f;
    int stepNum = 1;
    while (stepNum < 4)
    {
        mat3_t matrix;
        vec3_t offsetVec;
        vec3_t angles;

        VectorSet(angles, 0, sign * angleStep * stepNum, 0);
        AnglesToAxis(angles, matrix);
        Matrix3_TransformVector(matrix, baseOffsetVec.data(), offsetVec);
        G_Trace(&trace, self->s.origin, mins, maxs, (Vec3(offsetVec) + self->s.origin).data(), self, MASK_AISOLID);

        if (trace.fraction > bestFraction)
        {
            bestFraction = trace.fraction;
            VectorCopy(bestVec, moveVec->data());
        }

        if (sign > 0)
            stepNum++;
        sign = -sign;
    }
    // If bestFraction is still a fraction of the forward trace, moveVec is kept as is
}

void Bot::StraightenOrInterpolateMoveVec(Vec3 *moveVec, float speed)
{
    if (nextReaches.empty())
    {
        // Looks like we are in air above a ground, keep as is waiting for landing.
        VectorCopy(self->velocity, moveVec->data());
        return;
    }
    if (currAasAreaNum == goalAasAreaNum)
    {
        *moveVec = goalTargetPoint - self->s.origin;
        return;
    }

    if (!TryStraightenMoveVec(moveVec, speed))
        InterpolateMoveVec(moveVec, speed);
}

bool Bot::TryStraightenMoveVec(Vec3 *moveVec, float speed)
{
    // We may miss target if we straighten path

    Vec3 botToGoalDir = goalTargetPoint - self->s.origin;
    Vec3 velocityDir(self->velocity);
    // Check normalization conditions
    float squareDistanceToGoal = botToGoalDir.SquaredLength();
    if (squareDistanceToGoal < 0.1f || speed < 0.1f)
        return false;
    // Normalize direction vectors
    botToGoalDir *= Q_RSqrt(squareDistanceToGoal);
    velocityDir *= 1.0f / speed;

    float proximityLimit = 64.0f;
    // 1 for opposite directions, 0 for matching directions
    float directionFactor = 0.5f - 0.5f * botToGoalDir.Dot(velocityDir);
    float speedFactor = BoundedFraction(speed - 160, 640);
    proximityLimit += 400.0f * directionFactor * speedFactor;

    if (squareDistanceToGoal < proximityLimit * proximityLimit)
        return false;

    // First, count how many reach. are bunny-friendly
    int bunnyLikeReachesCount = 0;
    for (unsigned i = 0; i < nextReaches.size(); ++i)
    {
        // TODO: Check area travel types too?
        int travelType = nextReaches[i].traveltype;
        if (travelType == TRAVEL_WALK || travelType == TRAVEL_WALKOFFLEDGE)
            bunnyLikeReachesCount++;
        else if (travelType == TRAVEL_JUMP || travelType == TRAVEL_JUMPPAD)
            bunnyLikeReachesCount++;
        else
            break;
    }

    if (bunnyLikeReachesCount == 0)
        return false;

    int areas[MAX_REACH_CACHED + 1];
    vec3_t points[MAX_REACH_CACHED + 1];
    while (bunnyLikeReachesCount > 1)
    {
        auto &endReach = nextReaches[bunnyLikeReachesCount - 1];
        int numAreas = AAS_TraceAreas(self->s.origin, endReach.start, areas, points, bunnyLikeReachesCount + 1);

        if (numAreas != bunnyLikeReachesCount + 1)
            goto lesserStep;

        if (areas[0] != currAasAreaNum)
            goto lesserStep;

        for (int i = 0; i < bunnyLikeReachesCount; ++i)
            if (nextReaches[i].areanum != areas[i + 1])
                goto lesserStep;

        // There are not cheap ways to check whether we may fail following straightened path.
        // So at least do not try to straighten path when some reachabilities are higher than current origin
        for (int i = 0; i < bunnyLikeReachesCount; ++i)
            if (nextReaches[i].start[2] > self->s.origin[2] || nextReaches[i].end[2] > self->s.origin[2])
                goto lesserStep;

        // Try trace obstacles now. Use null (= zero) as a mins (do not stop on small ground obstacles)
        trace_t trace;
        G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, endReach.start, self, MASK_AISOLID);
        if (trace.fraction != 1.0f)
            goto lesserStep;

        *moveVec = Vec3(endReach.start) - self->s.origin;
        return true;

lesserStep:
        bunnyLikeReachesCount /= 2;
    }

    return false;
}

void Bot::InterpolateMoveVec(Vec3 *moveVec, float speed)
{
    if (nextReaches.empty())
        FailWith("InterpolateMoveVec(): nextReaches is empty");

    const float radius = 72.0f + 72.0f * BoundedFraction(speed - 320, 640);

    if (distanceToNextReachStart > radius)
    {
        SetMoveVecToPendingReach(moveVec);
        return;
    }

    vec3_t weightedDirsToReachStart[MAX_REACH_CACHED];

    // If true, next reach. is outside of a transition radius
    bool hasOnlySingleFarReach = false;
    vec3_t singleFarNextReachDir;

    int nearestReachesCount = 0;
    for (unsigned i = 1; i < nextReaches.size(); ++i)
    {
        auto &reach = nextReaches[i];
        int travelType = reach.traveltype;
        // Make conditions fit line limit
        if (travelType != TRAVEL_WALK && travelType != TRAVEL_WALKOFFLEDGE)
            if (travelType != TRAVEL_JUMP && travelType != TRAVEL_STRAFEJUMP)
                break;

        float squareDist = DistanceSquared(reach.start, self->s.origin);
        if (squareDist > radius * radius)
        {
            // If we haven't found next reach. yet
            if (nearestReachesCount == 0)
            {
                VectorCopy(reach.start, singleFarNextReachDir);
                VectorSubtract(singleFarNextReachDir, self->s.origin, singleFarNextReachDir);
                VectorNormalizeFast(singleFarNextReachDir);
                hasOnlySingleFarReach = true;
                nearestReachesCount = 1;
            }
            break;
        }

        float *dir = weightedDirsToReachStart[nearestReachesCount];
        // Copy vector from self origin to reach. start
        VectorCopy(reach.start, dir);
        VectorSubtract(dir, self->s.origin, dir);
        // Compute the vector length
        float invDistance = Q_RSqrt(squareDist);
        // Normalize the vector
        VectorScale(dir, invDistance, dir);
        // Scale by distance factor
        //VectorScale(dir, 1.0f - (1.0f / invDistance) / radius, dir);

        nearestReachesCount++;
        if (nearestReachesCount == MAX_REACH_CACHED)
            break;
    }

    if (!nearestReachesCount || moveVec->SquaredLength() < 0.01f)
    {
        SetMoveVecToPendingReach(moveVec);
        return;
    }

    *moveVec = Vec3(nextReaches.front().start) - self->s.origin;
    moveVec->NormalizeFast();
    if (hasOnlySingleFarReach)
    {
        float distanceFactor = distanceToNextReachStart / radius; // 0..1
        *moveVec *= distanceFactor;
        VectorScale(singleFarNextReachDir, 1.0f - distanceFactor, singleFarNextReachDir);
        *moveVec += singleFarNextReachDir;
    }
    else
    {
        *moveVec *= distanceToNextReachStart / radius;
        for (int i = 0; i < nearestReachesCount; ++i)
            *moveVec += weightedDirsToReachStart[i];
    }
    // moveVec is not required to be normalized, leave it as is
}

void Bot::SetMoveVecToPendingReach(Vec3 *moveVec)
{
    Vec3 linkVec = Vec3(nextReaches.front().end) - nextReaches.front().start;
    linkVec.NormalizeFast();
    *moveVec = Vec3(16 * linkVec + nextReaches.front().start) - self->s.origin;
}

void Bot::SetPendingLandingDash(usercmd_t *ucmd)
{
    if (isOnGroundThisFrame)
        return;

    ucmd->forwardmove = 0;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;
    hasPendingLandingDash = true;
    pendingLandingDashTimeout = level.time + 700;
    requestedViewTurnSpeedMultiplier = 1.35f;
}

void Bot::TryApplyPendingLandingDash(usercmd_t *ucmd)
{
    if (!hasPendingLandingDash)
        return;
    if (!isOnGroundThisFrame || wasOnGroundPrevFrame)
        return;

    ucmd->forwardmove = 1;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;
    ucmd->buttons |= BUTTON_SPECIAL;
    hasPendingLandingDash = false;
    requestedViewTurnSpeedMultiplier = 1.0f;
}

void Bot::CheckPendingLandingDashTimedOut()
{
    if (hasPendingLandingDash && pendingLandingDashTimeout <= level.time)
    {
        hasPendingLandingDash = false;
        requestedViewTurnSpeedMultiplier = 1.0f;
    }
}

void Bot::MoveGenericRunning(Vec3 *moveVec, usercmd_t *ucmd)
{
    if (hasPendingLandingDash)
    {
        TryApplyPendingLandingDash(ucmd);
        return;
    }

    // moveDir is initially set to a vector from self to currMoveTargetPoint.
    // However, if we have any tangential velocity in a trajectory and it is not directed to the target.
    // we are likely going to miss the target point. We have to check actual look angles
    // and assist by turning to the target harder and/or assisting by strafe keys (when picking an item)

    Vec3 velocityVec(self->velocity);
    float speed = velocityVec.SquaredLength() > 0.01f ? velocityVec.LengthFast() : 0;

    StraightenOrInterpolateMoveVec(moveVec, speed);

    // This call may set ucmd->upmove to -1 to prevent bumping a ceiling in air
    CheckAndTryAvoidObstacles(moveVec, ucmd, speed);

    Vec3 toTargetDir2D(*moveVec);
    toTargetDir2D.z() = 0;

    // Do not use look dir as an actual dir, bot may move backwards
    Vec3 actualDir2D(velocityVec);
    actualDir2D.z() = 0;

    float actualDir2DSqLen = actualDir2D.SquaredLength();
    float toTargetDir2DSqLen = toTargetDir2D.SquaredLength();

    if (actualDir2DSqLen > 0.1f)
    {
        actualDir2D *= Q_RSqrt(actualDir2DSqLen);

        ucmd->forwardmove = 1;
        isBunnyHopping = false;

        if (speed < DEFAULT_DASHSPEED - 16)
        {
            ucmd->upmove = 0;
            if (self->groundentity)
                ucmd->buttons |= BUTTON_SPECIAL;
        }
        else
        {
            // If we are not crouching in air to prevent bumping a ceiling, keep jump key pressed
            if (ucmd->upmove != -1)
                ucmd->upmove = 1;
            isBunnyHopping = true;
        }

        if (toTargetDir2DSqLen > 0.1f)
        {
            toTargetDir2D *= Q_RSqrt(toTargetDir2DSqLen);

            float actualToTarget2DDot = actualDir2D.Dot(toTargetDir2D);
            if (actualToTarget2DDot > 0.99)
            {
                // TODO: Implement "true" strafejumping
                if (Skill() > 0.33f)
                {
                    float skillFactor = 0.15f + 0.20f * Skill();
                    VectorScale(self->velocity, 1.0f + skillFactor * 0.0001f * game.frametime, self->velocity);
                }
            }
            else
            {
                // Given an actual move dir line (a line that goes through selfOrigin to selfOrigin + actualDir2D),
                // determine on which side the move target (defined by moveVec) is
                float lineNormalX = +actualDir2D.y();
                float lineNormalY = -actualDir2D.x();
                int side = Q_sign(lineNormalX * moveVec->x() + lineNormalY * moveVec->y());

                // Check whether we may increase requested turn direction
                if (actualToTarget2DDot > -0.5)
                {
                    // currAngle is an angle between actual lookDir and moveDir
                    // moveDir is a requested look direction
                    // due to view angles change latency requested angles may be not set immediately in this frame
                    // We have to request an angle that is greater than it is needed
                    float extraTurn = 0.1f;
                    // Turning using forward (GS_NEWBUNNY) aircontrol is not suitable. Use old Quakeworld style.
                    if (actualToTarget2DDot < 0.5)
                    {
                        // Release +forward and press strafe key in accordance to view turn direction
                        ucmd->forwardmove = 0;
                        ucmd->sidemove = side;
                        extraTurn = 0.03f;
                    }
                    float currAngle = RAD2DEG(acosf(actualToTarget2DDot)) * side;
                    mat3_t matrix;
                    AnglesToAxis(Vec3(0, currAngle * (1.0f + extraTurn), 0).data(), matrix);
                    Matrix3_TransformVector(matrix, moveVec->data(), moveVec->data());
                }
                else if (!isOnGroundThisFrame && Skill() > 0.33f)
                {
                    // Center view before spinning, do not spin looking at the sky or a floor
                    moveVec->z() = 0;
                    SetPendingLandingDash(ucmd);
                }
                else
                {
                    // Try move backwards to a goal
                    if (IsCloseToAnyGoal())
                    {
                        ucmd->upmove = 0;
                        ucmd->forwardmove = -1;
                        ucmd->sidemove = side;
                    }
                    else
                    {
                        // Just reduce a push
                        ucmd->upmove = 1;
                        ucmd->sidemove = 0;
                        ucmd->forwardmove = 0;
                    }
                }
            }

            // Don't bend too hard
            moveVec->z() *= 0.33f;
        }
    }
    else
    {
        // Looks like we are falling on target or being pushed up through it
        ucmd->forwardmove = 0;
        ucmd->upmove = 0;
    }
}

void Bot::CheckTargetReached()
{
    if (currAasAreaNum != goalAasAreaNum)
        return;

    if (botBrain.MayReachLongTermGoalNow())
    {
        botBrain.OnLongTermGoalReached();
        return;
    }

    if (botBrain.MayReachShortTermGoalNow())
    {
        botBrain.OnShortTermGoalReached();
        return;
    }
}

Vec3 Bot::MakeEvadeDirection(const Danger &danger)
{
    if (danger.splash)
    {
        Vec3 result(0, 0, 0);
        Vec3 selfToHitDir = danger.hitPoint - self->s.origin;
        RotatePointAroundVector(result.data(), &axis_identity[AXIS_UP], selfToHitDir.data(), -self->s.angles[YAW]);
        result.NormalizeFast();

        if (fabs(result.x()) < 0.3) result.x() = 0;
        if (fabs(result.y()) < 0.3) result.y() = 0;
        result.z() = 0;
        result.x() *= -1.0f;
        result.y() *= -1.0f;
        return result;
    }

    Vec3 selfToHitPoint = danger.hitPoint - self->s.origin;
    selfToHitPoint.z() = 0;
    // If bot is not hit in its center, try pick a direction that is opposite to a vector from bot center to hit point
    if (selfToHitPoint.SquaredLength() > 4 * 4)
    {
        selfToHitPoint.NormalizeFast();
        // Check whether this direction really helps to evade the danger
        // (the less is the abs. value of the dot product, the closer is the chosen direction to a perpendicular one)
        if (fabs(selfToHitPoint.Dot(danger.direction)) < 0.5f)
        {
            if (fabs(selfToHitPoint.x()) < 0.3) selfToHitPoint.x() = 0;
            if (fabs(selfToHitPoint.y()) < 0.3) selfToHitPoint.y() = 0;
            return -selfToHitPoint;
        }
    }

    // Otherwise just pick a direction that is perpendicular to the danger direction
    float maxCrossSqLen = 0.0f;
    Vec3 result(0, 1, 0);
    for (int i = 0; i < 3; ++i)
    {
        Vec3 cross = danger.direction.Cross(&axis_identity[i * 3]);
        cross.z() = 0;
        float crossSqLen = cross.SquaredLength();
        if (crossSqLen > maxCrossSqLen)
        {
            maxCrossSqLen = crossSqLen;
            float invLen = Q_RSqrt(crossSqLen);
            result.x() = cross.x() * invLen;
            result.y() = cross.y() * invLen;
        }
    }
    return result;
}

//==========================================
// BOT_DMclass_CombatMovement
//
// NOTE: Very simple for now, just a basic move about avoidance.
//       Change this routine for more advanced attack movement.
//==========================================
void Bot::CombatMovement(usercmd_t *ucmd, bool hasToEvade)
{
    // TODO: Check whether we are holding/camping a point

    const CombatTask &combatTask = botBrain.combatTask;

    if ((!combatTask.aimEnemy && !combatTask.spamEnemy))
    {
        Move(ucmd);
        return;
    }

    const float dist = (combatTask.TargetOrigin() - self->s.origin).LengthFast();
    const float c = random();

    if (combatMovePushTimeout <= level.time)
    {
        combatMovePushTimeout = level.time + AI_COMBATMOVE_TIMEOUT;

        VectorClear(combatMovePushes);

        if (hasToEvade)
        {
            ApplyEvadeMovePushes(ucmd);
        }
        else
        {
            if (dist < 150.0f && self->s.weapon == WEAP_GUNBLADE) // go into him!
            {
                ucmd->buttons &= ~BUTTON_ATTACK; // remove pressing fire
                if (closeAreaProps.frontTest.CanWalk())  // move to your enemy
                    combatMovePushes[0] = 1;
                else if (c <= 0.5 && closeAreaProps.leftTest.CanWalk())
                    combatMovePushes[1] = -1;
                else if (c <= 0.5 && closeAreaProps.rightTest.CanWalk())
                    combatMovePushes[1] = 1;
            }
            else
            {
                // First, establish mapping from CombatTask tactical directions (if any) to bot movement key directions
                int tacticalXMove, tacticalYMove;
                bool advance = TacticsToAprioriMovePushes(&tacticalXMove, &tacticalYMove);

                const auto &placeProps = closeAreaProps;  // Shorthand
                auto moveXAndUp = ApplyTacticalMove(tacticalXMove, advance, placeProps.frontTest, placeProps.backTest);
                auto moveYAndUp = ApplyTacticalMove(tacticalYMove, advance, placeProps.rightTest, placeProps.leftTest);

                combatMovePushes[0] = moveXAndUp.first;
                combatMovePushes[1] = moveYAndUp.first;
                combatMovePushes[2] = moveXAndUp.second || moveYAndUp.second;
            }
        }
    }

    if (!hasToEvade && combatTask.inhibit)
    {
        Move( ucmd );
    }
    else
    {
        if (MayApplyCombatDash())
            ucmd->buttons |= BUTTON_SPECIAL;
    }

    ucmd->forwardmove = combatMovePushes[0];
    ucmd->sidemove = combatMovePushes[1];
    ucmd->upmove = combatMovePushes[2];
}

void Bot::ApplyEvadeMovePushes(usercmd_t *ucmd)
{
    Vec3 evadeDir = MakeEvadeDirection(*dangersDetector.primaryDanger);
#ifdef _DEBUG
    Vec3 drawnDirStart(self->s.origin);
    drawnDirStart.z() += 32;
    Vec3 drawnDirEnd = drawnDirStart + 64.0f * evadeDir;
    AITools_DrawLine(drawnDirStart.data(), drawnDirEnd.data());
#endif

    int walkingEvades = 0;
    int walkingMovePushes[3] = {0, 0, 0};
    int jumpingEvades = 0;
    int jumpingMovePushes[3] = {0, 0, 0};

    if (evadeDir.x())
    {
        if ((evadeDir.x() < 0))
        {
            if (closeAreaProps.backTest.CanWalkOrFallQuiteSafely())
            {
                walkingMovePushes[0] = -1;
                ++walkingEvades;
            }
            else if (closeAreaProps.backTest.CanJump())
            {
                jumpingMovePushes[0] = -1;
                ++jumpingEvades;
            }
        }
        else if ((evadeDir.x() > 0))
        {
            if (closeAreaProps.frontTest.CanWalkOrFallQuiteSafely())
            {
                walkingMovePushes[0] = 1;
                ++walkingEvades;
            }
            else if (closeAreaProps.frontTest.CanJump())
            {
                jumpingMovePushes[0] = 1;
                ++jumpingEvades;
            }
        }
    }
    if (evadeDir.y())
    {
        if ((evadeDir.y() < 0))
        {
            if (closeAreaProps.leftTest.CanWalkOrFallQuiteSafely())
            {
                walkingMovePushes[1] = -1;
                ++walkingEvades;
            }
            else if (closeAreaProps.leftTest.CanJump())
            {
                jumpingMovePushes[1] = -1;
                ++jumpingEvades;
            }
        }
        else if ((evadeDir.y() > 0))
        {
            if (closeAreaProps.rightTest.CanWalkOrFallQuiteSafely())
            {
                walkingMovePushes[1] = 1;
                ++walkingEvades;
            }
            else if (closeAreaProps.rightTest.CanJump())
            {
                jumpingMovePushes[1] = 1;
                ++jumpingEvades;
            }
        }
    }

    // Walked evades involve dashes, so they are more important
    if (walkingEvades > jumpingEvades)
    {
        VectorCopy(walkingMovePushes, combatMovePushes);
        if (Skill() > 0.85f || (random() < (Skill() - 0.25f)))
        {
            ucmd->buttons |= BUTTON_SPECIAL;
        }
    }
    else if (jumpingEvades > 0)
    {
        jumpingMovePushes[2] = 1;
        VectorCopy(jumpingMovePushes, combatMovePushes);
    }
}

bool Bot::MayApplyCombatDash()
{
    if (Skill() <= 0.25)
        return false;

    const auto &pmove = self->r.client->ps.pmove;
    // Try to dash in fight depending of skill, if not already doing that
    if (pmove.pm_flags & (PMF_DASHING | PMF_WALLJUMPING))
        return false;

    float prob = Skill() - 0.25f;
    const auto &oldPmove = self->r.client->old_pmove;
    // If bot has been stunned in previous frame, try to do the possible blocked by stun dash with high priority
    if (oldPmove.stats[PM_STAT_STUN] || oldPmove.stats[PM_STAT_KNOCKBACK])
    {
        if (Skill() > 0.85f)
        {
            prob = 1.0f;
        }
        else if (Skill() > 0.66f)
        {
            prob *= 2;
        }
    }
    return random() < prob;
}

bool Bot::TacticsToAprioriMovePushes(int *tacticalXMove, int *tacticalYMove)
{
    *tacticalXMove = 0;
    *tacticalYMove = 0;

    const CombatTask &combatTask = botBrain.combatTask;

    if (!combatTask.advance && !combatTask.retreat)
        return false;

    Vec3 botToEnemyDir(self->s.origin);
    if (combatTask.aimEnemy)
        botToEnemyDir -= combatTask.aimEnemy->LastSeenPosition();
    else
        botToEnemyDir -= combatTask.spamSpot;
    // Normalize (and invert since we initialized a points difference by vector start, not the end)
    botToEnemyDir *= -1.0f * Q_RSqrt(botToEnemyDir.SquaredLength());

    Vec3 forward(0, 0, 0), right(0, 0, 0);
    AngleVectors(self->s.angles, forward.data(), right.data(), nullptr);

    float forwardDotToEnemyDir = forward.Dot(botToEnemyDir);
    float rightDotToEnemyDir = right.Dot(botToEnemyDir);

    // Currently we always prefer being cautious...
    bool advance = combatTask.advance && !combatTask.retreat;
    bool retreat = combatTask.retreat;
    if (fabsf(forwardDotToEnemyDir) > 0.25f)
    {
        if (advance)
            *tacticalXMove = +Q_sign(forwardDotToEnemyDir);
        if (retreat)
            *tacticalXMove = -Q_sign(forwardDotToEnemyDir);
    }
    if (fabsf(rightDotToEnemyDir) > 0.25f)
    {
        if (advance)
            *tacticalYMove = +Q_sign(rightDotToEnemyDir);
        if (retreat)
            *tacticalYMove = -Q_sign(rightDotToEnemyDir);
    }
    return advance;
}

std::pair<int, int> Bot::ApplyTacticalMove(int tacticalMove, bool advance, const MoveTestResult &positiveDirTest, const MoveTestResult &negativeDirTest)
{
    auto result = std::make_pair(0, 0);
    if (tacticalMove && random() < 0.9f)
    {
        const MoveTestResult &moveTestResult = tacticalMove > 0 ? positiveDirTest : negativeDirTest;
        if (moveTestResult.CanWalkOrFallQuiteSafely())
        {
            // Only fall down to enemies while advancing, do not escape accidentally while trying to attack
            if (moveTestResult.CanFall() && advance)
            {
                // Allow to fall while attacking when enemy is still on bots height
                if (self->s.origin[2] - moveTestResult.PotentialFallDepth() + 16 > botBrain.combatTask.TargetOrigin().z())
                    result.first = tacticalMove;
            }
            else
                result.first = tacticalMove;
        }
        else if (moveTestResult.CanJump())
        {
            result.first = tacticalMove;
            result.second = 1;
        }
    }
    else
    {
        int movePushValue;
        const MoveTestResult *moveTestResult;
        if (random() < 0.5f)
        {
            movePushValue = 1;
            moveTestResult = &positiveDirTest;
        }
        else
        {
            movePushValue = -1;
            moveTestResult = &negativeDirTest;
        }
        if (moveTestResult->CanWalk())
            result.first = movePushValue;
        else if (moveTestResult->CanJump())
        {
            result.first = movePushValue;
            result.second = 1;
        }
    }
    return result;
}

