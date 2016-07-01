#include "bot.h"
#include "aas.h"

void Bot::MoveFrame(usercmd_t *ucmd, bool inhibitCombat)
{
    isOnGroundThisFrame = self->groundentity != nullptr;

    CheckPendingLandingDashTimedOut();

    bool hasToEvade = false;
    if (Skill() > 0.25f && !ShouldSkipThinkFrame())
        hasToEvade = dangersDetector.FindDangers();

    if (inhibitCombat && !hasToEvade)
        Move(ucmd);
    else
        CombatMovement(ucmd, hasToEvade);

    TryMoveAwayIfBlocked(ucmd);

    CheckTargetProximity();

    wasOnGroundPrevFrame = isOnGroundThisFrame;
}

void Bot::Move(usercmd_t *ucmd)
{
    if (currAasAreaNum == 0 || goalAasAreaNum == 0)
        return;

    Vec3 intendedLookVec(self->velocity);  // Use as a default one
    if (currAasAreaNum != goalAasAreaNum)
    {
        if (!nextReaches.empty())
        {
            const auto &nextReach = nextReaches.front();
            if (!IsCloseToReachStart())
                intendedLookVec = Vec3(nextReach.start) - self->s.origin;
            else
            {
                Vec3 linkVec(nextReach.end);
                linkVec -= nextReach.start;
                linkVec.NormalizeFast();
                intendedLookVec = (16.0f * linkVec + nextReach.end) - self->s.origin;
            }
        }
    }
    else
    {
        intendedLookVec = Vec3(self->s.origin) - goalTargetPoint;
    }

    if (self->is_ladder)
    {
        MoveOnLadder(&intendedLookVec, ucmd);
    }
    else if (hasTouchedJumppad)
    {
        MoveEnteringJumppad(&intendedLookVec, ucmd);
    }
    else if (hasEnteredJumppad)
    {
        MoveRidingJummpad(&intendedLookVec, ucmd);
    }
    else if (self->groundentity && Use_Plat == self->groundentity->use)
    {
        MoveOnPlatform(&intendedLookVec, ucmd);
    }
    else // standard movement
    {
        if (isWaitingForItemSpawn)
        {
            MoveCampingASpot(&intendedLookVec, ucmd);
        }
        else if (self->is_swim)
        {
            MoveSwimming(&intendedLookVec, ucmd);
        }
        else
        {
            MoveGenericRunning(&intendedLookVec, ucmd);
        }
    }

    if (!hasPendingLookAtPoint)
    {
        float turnSpeedMultiplier = requestedViewTurnSpeedMultiplier;
        if (HasEnemy())
        {
            intendedLookVec.NormalizeFast();
            Vec3 toEnemy(EnemyOrigin());
            toEnemy -= self->s.origin;
            float squareDistanceToEnemy = toEnemy.SquaredLength();
            if (squareDistanceToEnemy > 1)
            {
                float invDistanceToEnemy = Q_RSqrt(squareDistanceToEnemy);
                toEnemy *= invDistanceToEnemy;
                if (intendedLookVec.Dot(toEnemy) < -0.3f)
                {
                    // Check whether we should center view to prevent looking at the sky or a floor while spinning
                    float factor = fabsf(self->s.origin[2] - EnemyOrigin().Z()) * invDistanceToEnemy;
                    // If distance to enemy is 4x more than height difference, center view
                    if (factor < 0.25f)
                    {
                        intendedLookVec.Z() *= 0.0001f;
                    }
                    ucmd->forwardmove *= -1;
                    intendedLookVec *= -1;
                    turnSpeedMultiplier = 1.35f;
                }
            }
        }
        ChangeAngle(intendedLookVec, turnSpeedMultiplier);
    }
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

void Bot::MoveOnLadder(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;
}

template <typename T> struct AttributedArea
{
    int areaNum;
    T attr;
    AttributedArea() {}
    AttributedArea(int areaNum, T attr): areaNum(areaNum), attr(attr) {}
    bool operator<(const AttributedArea &that) const { return attr < that.attr; }
};

void Bot::MoveEnteringJumppad(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (hasEnteredJumppad)
        return;

    jumppadLandingAreasCount = 0;
    // Estimate distance (we may use bot origin as approximate jumppad origin since bot has just triggered it)
    float approxDistance = (jumppadTarget - self->s.origin).LengthFast();
    // Use larger box for huge jumppads
    float baseSide = 56.0f + 72.0f * BoundedFraction(approxDistance, 1500.0f);
    // Jumppad target has been set in Bot::TouchedJumppad()
    Vec3 bboxMins = jumppadTarget + Vec3(-1.25f * baseSide, -1.25f * baseSide, -0.45f * baseSide);
    Vec3 bboxMaxs = jumppadTarget + Vec3(+1.25f * baseSide, +1.25f * baseSide, +0.15f * baseSide);
    // First, fetch all areas in the target bounding box (more than required)
    int rawAreas[MAX_LANDING_AREAS * 2];
    int rawAreasCount = AAS_BBoxAreas(bboxMins.Data(), bboxMaxs.Data(), rawAreas, MAX_LANDING_AREAS * 2);
    // Then filter raw areas and sort by distance to jumppad target
    AttributedArea<float> filteredAreas[MAX_LANDING_AREAS * 2];
    int filteredAreasCount = 0;
    for (int i = 0; i < rawAreasCount; ++i)
    {
        int areaNum = rawAreas[i];
        // Skip areas above target that a-priori may not be a landing site. Areas bounds are absolute.
        if (aasworld.areas[areaNum].mins[2] + 8 > jumppadTarget.Z())
            continue;
        // Skip non-grounded areas
        if (!AAS_AreaGrounded(areaNum))
            continue;
        // Skip "do not enter" areas
        if (AAS_AreaDoNotEnter(areaNum))
            continue;
        float squareDistance = (jumppadTarget - aasworld.areas[areaNum].center).SquaredLength();
        filteredAreas[filteredAreasCount++] = AttributedArea<float>(areaNum, squareDistance);
    }
    std::sort(filteredAreas, filteredAreas + filteredAreasCount);

    // Select no more than MAX_LANDING_AREAS feasible areas
    // Since areas are sorted by proximity to jumppad target point,
    // its unlikely that best areas may be rejected by this limit.
    AttributedArea<int> areasAndTravelTimes[MAX_LANDING_AREAS];
    int selectedAreasCount = 0;
    for (int i = 0, end = filteredAreasCount; i < end; ++i)
    {
        int areaNum = filteredAreas[i].areaNum;
        // Project the area center to the ground manually.
        // (otherwise the following pathfinder call may perform a trace for it)
        // Note that AAS area mins are absolute.
        Vec3 origin(aasworld.areas[areaNum].center);
        origin.Z() = aasworld.areas[areaNum].mins[2] + 8;
        // Returns 1 as a lowest feasible travel time value (in seconds ^-2), 0 when a path can't be found
        int aasTravelTime = FindAASTravelTimeToGoalArea(areaNum, origin.Data(), goalAasAreaNum);
        if (aasTravelTime)
        {
            areasAndTravelTimes[selectedAreasCount++] = AttributedArea<int>(areaNum, aasTravelTime);
            if (selectedAreasCount == MAX_LANDING_AREAS)
                break;
        }
    }
    // Sort landing areas by travel time to a goal, closest to goal areas first
    std::sort(areasAndTravelTimes, areasAndTravelTimes + selectedAreasCount);
    // Store areas for landing
    for (int i = 0; i < selectedAreasCount; ++i)
        jumppadLandingAreas[jumppadLandingAreasCount++] = areasAndTravelTimes[i].areaNum;

    ucmd->forwardmove = 1;
    hasEnteredJumppad = true;
}

void Bot::MoveRidingJummpad(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    // First check whether bot finally landed to some area
    if (self->groundentity)
    {
        hasEnteredJumppad = false;
        ucmd->forwardmove = 1;
        return;
    }

    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (jumppadMoveTimeout <= level.time)
    {
        if (jumppadLandingAreasCount)
        {
            // `jumppadLandingAreas` is assumed to be sorted, best areas first
            for (int i = 0; i < jumppadLandingAreasCount; ++i)
            {
                if (TryLandOnArea(jumppadLandingAreas[i], intendedLookVec, ucmd))
                    return;
            }
        }
        // TryLandOnNearbyAreas() is expensive. TODO: Use timeout between calls based on current speed
        TryLandOnNearbyAreas(intendedLookVec, ucmd);
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

void Bot::TryLandOnNearbyAreas(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    Vec3 bboxMins(self->s.origin), bboxMaxs(self->s.origin);
    bboxMins += Vec3(-128, -128, -128);
    bboxMaxs += Vec3(+128, +128, +128);

    constexpr int MAX_LANDING_AREAS = 16;
    int areas[MAX_LANDING_AREAS];
    int groundedAreas[MAX_LANDING_AREAS];
    float distanceToAreas[MAX_LANDING_AREAS];

    int numAllAreas = AAS_BBoxAreas(bboxMins.Data(), bboxMaxs.Data(), areas, MAX_LANDING_AREAS);
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
        if (TryLandOnArea(areas[i], intendedLookVec, ucmd))
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

bool Bot::TryLandOnArea(int areaNum, Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    Vec3 areaPoint(aasworld.areas[areaNum].center);

    // Lower area point to a bottom of area. Area mins/maxs are absolute.
    areaPoint.Z() = aasworld.areas[areaNum].mins[2];
    // Do not try to "land" on upper areas
    if (areaPoint.Z() > self->s.origin[2])
        return false;

    // We have to offset traced end point since we do not test a zero-width ray
    Vec3 areaPointToBotVec(self->s.origin);
    areaPointToBotVec -= areaPoint;
    areaPointToBotVec.NormalizeFast();
    Vec3 traceEnd(areaPoint);

    trace_t trace;
    G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, traceEnd.Data(), self, MASK_AISOLID);
    if (trace.fraction == 1.0f)
    {
        ucmd->forwardmove = 1;
        *intendedLookVec = -areaPointToBotVec;
        return true;
    }

    return false;
}

void Bot::MoveCampingASpot(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    // If hasCampingLookAtPoint is false and this function has not been called yet since last camping spot setting,
    // campingSpotLookAtPoint contains a junk, so it need to be overwritten
    Vec3 lookAtPoint(campingSpotLookAtPoint);
    // If the camping action does not have a defined look direction, choose some random one
    if (!hasCampingLookAtPoint)
    {
        // If the previously chosen look at point has timed out, choose a new one
        if (campingSpotLookAtPointTimeout <= level.time)
        {
            // Choose some random point to look at
            campingSpotLookAtPoint.X() = self->s.origin[0] - 50.0f + 100.0f * random();
            campingSpotLookAtPoint.Y() = self->s.origin[1] - 50.0f + 100.0f * random();
            campingSpotLookAtPoint.Z() = self->s.origin[2] - 15.0f + 30.0f * random();
            campingSpotLookAtPointTimeout = level.time + 1500 - (unsigned)(1250.0f * campingAlertness);
        }
        lookAtPoint = campingSpotLookAtPoint;
    }
    MoveCampingASpotWithGivenLookAtPoint(lookAtPoint, intendedLookVec, ucmd);
}

void KeyMoveVecToUcmd(const Vec3 &keyMoveVec, const vec3_t actualLookDir, const vec3_t actualRightDir, usercmd_t *ucmd)
{
    ucmd->forwardmove = 0;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;

    float dotForward = keyMoveVec.Dot(actualLookDir);
    if (dotForward > 0.3)
        ucmd->forwardmove = 1;
    else if (dotForward < -0.3)
        ucmd->forwardmove = -1;

    float dotRight = keyMoveVec.Dot(actualRightDir);
    if (dotRight > 0.3)
        ucmd->sidemove = 1;
    else if (dotRight < -0.3)
        ucmd->sidemove = -1;
}

void Bot::MoveCampingASpotWithGivenLookAtPoint(const Vec3 &lookAtPoint, Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    vec3_t actualLookDir, actualRightDir;
    AngleVectors(self->s.angles, actualLookDir, actualRightDir, nullptr);

    Vec3 botToSpot = campingSpotOrigin - self->s.origin;
    float distance = botToSpot.Length();

    if (distance / campingSpotRadius > 2.0f)
    {
        // Bot should return to a point
        ucmd->forwardmove = 1;
        ucmd->sidemove = 0;
        ucmd->upmove = 0;

        *intendedLookVec = botToSpot;
        // Very cheap workaround for "moving in planet gravity field" glitch by lowering move speed
        Vec3 botToSpotDir = botToSpot * (1.0f / distance);
        if (botToSpotDir.Dot(actualLookDir) < 0.7f)
            ucmd->buttons |= BUTTON_WALK;

        return;
    }

    Vec3 expectedLookDir = lookAtPoint - campingSpotOrigin;
    expectedLookDir.NormalizeFast();

    if (expectedLookDir.Dot(actualLookDir) < 0.85)
    {
        if (!hasPendingLookAtPoint)
        {
            SetPendingLookAtPoint(campingSpotLookAtPoint, 1.1f);
            ucmd->forwardmove = 0;
            ucmd->sidemove = 0;
            ucmd->upmove = 0;
            ucmd->buttons |= BUTTON_WALK;
            return;
        }
    }

    // Keep actual look dir as-is, adjust position by keys only
    *intendedLookVec = Vec3(actualLookDir);

    if (campingSpotStrafeTimeout < level.time)
    {
        // This means we may strafe randomly
        if (distance / campingSpotRadius < 0.7f)
        {
            campingSpotStrafeDir.X() = -0.5f + random();
            campingSpotStrafeDir.Y() = -0.5f + random();
            campingSpotStrafeDir.Z() = 0.0f;
            campingSpotStrafeTimeout = level.time + 500 + (unsigned)(100.0f * random() - 250.0f * campingAlertness);
        }
        else
        {
            campingSpotStrafeDir = botToSpot;
        }
        campingSpotStrafeDir.NormalizeFast();
    }

    Vec3 strafeMoveVec = campingSpotStrafeDir;

    KeyMoveVecToUcmd(strafeMoveVec, actualLookDir, actualRightDir, ucmd);

    if (random() > campingAlertness * 0.75f)
        ucmd->buttons |= BUTTON_WALK;
}

void Bot::MoveOnPlatform(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    switch (self->groundentity->moveinfo.state)
    {
        case STATE_TOP:
            // Start bunnying off the platform
            MoveGenericRunning(intendedLookVec, ucmd);
            break;
        default:
            // Its poor but platforms are not widely used.
            ucmd->forwardmove = 0;
            ucmd->sidemove = 0;
            ucmd->upmove = 0;
            // Prevent treating standing on the same point as being blocked
            blockedTimeout += game.frametime;
            break;
    }
}

void Bot::MoveSwimming(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = true;

    // TODO: Check reachibility, if we are close, exit water
    //if (!(G_PointContents(nodes[self->ai->next_node].origin) & MASK_WATER))  // Exit water
    //    ucmd->upmove = 1;
}

constexpr float Z_NO_BEND_SCALE = 0.25f;

bool Bot::CheckAndTryAvoidObstacles(Vec3 *intendedLookVec, usercmd_t *ucmd, float speed)
{
    Vec3 baseForwardVec(*intendedLookVec);
    baseForwardVec.Z() *= Z_NO_BEND_SCALE;
    // Treat inability to check obstacles as obstacles presence
    if (baseForwardVec.SquaredLength() < 0.01f)
        return true;

    baseForwardVec.NormalizeFast();
    baseForwardVec *= 24.0f + 96.0f * BoundedFraction(speed, 900);

    Vec3 forwardPoint = Vec3(baseForwardVec) + self->s.origin;

    float *const mins = vec3_origin;
    float *const maxs = playerbox_stand_maxs;

    trace_t trace;
    G_Trace(&trace, self->s.origin, mins, maxs, forwardPoint.Data(), self, MASK_AISOLID);

    if (trace.fraction == 1.0f)
        return false;

    if (trace.fraction > 0.5f && ISWALKABLEPLANE(&trace.plane))
        return false;

    // If we are in air, check whether we may crouch to prevent bumping a ceiling by head
    // We do not switch to crouch movement style, since we are still in air and have bunny speed
    if (!self->groundentity)
    {
        trace_t crouchTrace;
        G_Trace(&crouchTrace, self->s.origin, nullptr, playerbox_crouch_maxs, forwardPoint.Data(), self, MASK_AISOLID);
        if (crouchTrace.fraction == 1.0f)
        {
            ucmd->upmove = -1;
            return true;
        }
    }

    // Trace both a bit left and a bit right directions. Don't reject early (otherwise one side will be always preferred)
    float angleStep = 35.0f - 15.0f * BoundedFraction(speed, 900);

    float bestFraction = trace.fraction;

    float sign = -1.0f;
    int stepNum = 1;

    Vec3 angles(0, 0, 0);
    while (stepNum < 4)
    {
        mat3_t matrix;
        vec3_t rotatedForwardVec;

        // First, rotate baseForwardVec by angle = sign * angleStep * stepNum
        angles.Y() = sign * angleStep * stepNum;
        AnglesToAxis(angles.Data(), matrix);
        Matrix3_TransformVector(matrix, baseForwardVec.Data(), rotatedForwardVec);

        Vec3 rotatedForwardPoint = Vec3(rotatedForwardVec) + self->s.origin;
        G_Trace(&trace, self->s.origin, mins, maxs, rotatedForwardPoint.Data(), self, MASK_AISOLID);

        if (trace.fraction > bestFraction)
        {
            bestFraction = trace.fraction;
            // Copy found less blocked rotated forward vector to the intendedLookVec
            VectorCopy(rotatedForwardVec, intendedLookVec->Data());
            // Compensate applied Z scale applied to baseForwardVec (intendedLookVec Z is likely to be scaled again)
            intendedLookVec->Z() *= 1.0f / Z_NO_BEND_SCALE;
        }

        if (sign > 0)
            stepNum++;
        sign = -sign;
    }

    // If bestFraction is still a fraction of the forward trace, moveVec is kept as is
    return true;
}

bool Bot::StraightenOrInterpolateLookVec(Vec3 *intendedLookVec, float speed)
{
    if (nextReaches.empty())
    {
        if (currAasAreaNum != goalAasAreaNum)
        {
            // Looks like we are in air above a ground, keep as is waiting for landing.
            VectorCopy(self->velocity, intendedLookVec->Data());
            return false;
        }
        // Move to a goal origin
        *intendedLookVec = goalTargetPoint - self->s.origin;
        return true;
    }

    if (TryStraightenLookVec(intendedLookVec))
        return true;

    InterpolateLookVec(intendedLookVec, speed);
    return false;
}

bool Bot::TryStraightenLookVec(Vec3 *intendedLookVec)
{
    if (nextReaches.empty())
        FailWith("TryStraightenLookVec(): nextReaches is empty");

    // First, loop over known next reachabilities checking its travel type and Z level.
    // If Z differs too much, path can't be straightened, thus, reject straightening early.
    // Break on first non-TRAVEL_WALK reachability.
    unsigned i = 0;
    float minZ = self->s.origin[2];
    float maxZ = self->s.origin[2];
    for (; i < nextReaches.size(); ++i) {
        const auto &reach = nextReaches[i];
        float currZ = 0.5f * (reach.start[2] + reach.end[2]);
        if (currZ < maxZ - 24)
            return false;
        if (currZ > maxZ + 24)
            return false;
        if (currZ < minZ)
            minZ = currZ;
        else if (currZ > maxZ)
            maxZ = currZ;

        if (reach.traveltype != TRAVEL_WALK)
            break;
    }

    bool traceStraightenedPath = false;
    Vec3 lookAtPoint(0, 0, 0);

    // All next known reachabilities. have TRAVEL_WALK travel type
    if (i == nextReaches.size()) {
        // If a reachablities chain contains goal area, goal area is last in the chain
        if (nextReaches[i - 1].areanum == goalAasAreaNum) {
            traceStraightenedPath = true;
            lookAtPoint = goalTargetPoint;
        }
    }
    else
    {
        switch (nextReaches[i].traveltype)
        {
            case TRAVEL_TELEPORT:
            case TRAVEL_JUMPPAD:
            case TRAVEL_ELEVATOR:
                // Look at the trigger
                traceStraightenedPath = true;
                lookAtPoint = Vec3(nextReaches[i].start);
        }
    }

    if (!traceStraightenedPath)
        return false;

    // Check for obstacles on the straightened path
    trace_t trace;
    G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, lookAtPoint.Data(), self, MASK_AISOLID);
    if (trace.fraction != 1.0f)
        return false;

    *intendedLookVec = lookAtPoint - self->s.origin;
    return true;
}

void Bot::InterpolateLookVec(Vec3 *intendedLookVec, float speed)
{
    if (nextReaches.empty())
        FailWith("InterpolateLookVec(): nextReaches is empty");

    const float radius = 72.0f + 72.0f * BoundedFraction(speed - 320, 640);

    vec3_t weightedDirsToReachStart[MAX_REACH_CACHED];
    vec3_t weightedDirsToAreaCenter[MAX_REACH_CACHED];

    // If true, next reach. is outside of a transition radius
    bool hasOnlySingleFarReach = false;
    Vec3 singleFarNextReachDir(0, 0, 0);
    Vec3 singleFarNextAreaCenterDir(0, 0, 0);

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
                singleFarNextReachDir = Vec3(reach.start) - self->s.origin;
                singleFarNextReachDir.NormalizeFast();
                singleFarNextAreaCenterDir = Vec3(aasworld.areas[reach.areanum].center) - self->s.origin;
                singleFarNextAreaCenterDir.NormalizeFast();
                hasOnlySingleFarReach = true;
                nearestReachesCount = 1;
            }
            break;
        }

        float *reachDir = weightedDirsToReachStart[nearestReachesCount];
        // Copy vector from self origin to reach. start
        VectorCopy(reach.start, reachDir);
        VectorSubtract(reachDir, self->s.origin, reachDir);
        // Compute the vector length
        float invDistanceToReach = Q_RSqrt(squareDist);
        // Normalize the vector to reach. start
        VectorScale(reachDir, invDistanceToReach, reachDir);
        // Compute and apply reach. distance factor (closest reach'es should have greater weight)
        float reachDistanceFactor = 1.0f - (1.0f / invDistanceToReach) / radius;
        VectorScale(reachDir, reachDistanceFactor, reachDir);

        float *centerDir = weightedDirsToAreaCenter[nearestReachesCount];
        // Copy vector from self origin to area center
        VectorCopy(aasworld.areas[reach.areanum].center, centerDir);
        VectorSubtract(centerDir, self->s.origin, centerDir);
        // Normalize the vector to area center
        float invDistanceToCenter = Q_RSqrt(VectorLengthSquared(centerDir));
        VectorScale(centerDir, invDistanceToCenter, centerDir);
        // Compute and apply center distance factor (closest center points should have greater weight)
        float centerDistanceFactor = 1.0f - (1.0f / invDistanceToCenter) / radius;
        VectorScale(centerDir, centerDistanceFactor, centerDir);

        nearestReachesCount++;
        if (nearestReachesCount == MAX_REACH_CACHED)
            break;
    }

    if (!nearestReachesCount || intendedLookVec->SquaredLength() < 0.01f)
    {
        SetLookVecToPendingReach(intendedLookVec);
        return;
    }

    *intendedLookVec = Vec3(nextReaches.front().start) - self->s.origin;
    intendedLookVec->NormalizeFast();
    if (hasOnlySingleFarReach)
    {
        float pendingReachDistanceFactor = distanceToNextReachStart / radius;
        float nextReachDistanceFactor = 1.0f - pendingReachDistanceFactor;
        *intendedLookVec *= pendingReachDistanceFactor;
        singleFarNextReachDir *= 1.0f - nextReachDistanceFactor;
        singleFarNextAreaCenterDir *= 1.0f - nextReachDistanceFactor;
        *intendedLookVec += 0.5f * (singleFarNextReachDir + singleFarNextAreaCenterDir);
    }
    else
    {
        // Closest reach. start should have greater weight
        *intendedLookVec *= 1.0f - distanceToNextReachStart / radius;
        for (int i = 0; i < nearestReachesCount; ++i)
            *intendedLookVec += 0.5f * (Vec3(weightedDirsToReachStart[i]) + weightedDirsToAreaCenter[i]);
    }
    // intendedLookVec is not required to be normalized, leave it as is
}

void Bot::SetLookVecToPendingReach(Vec3 *intendedLookVec)
{
    Vec3 linkVec = Vec3(nextReaches.front().end) - nextReaches.front().start;
    linkVec.NormalizeFast();
    *intendedLookVec = Vec3(16 * linkVec + nextReaches.front().start) - self->s.origin;
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

bool Bot::TryApplyPendingLandingDash(usercmd_t *ucmd)
{
    if (!hasPendingLandingDash)
        return false;
    if (!isOnGroundThisFrame || wasOnGroundPrevFrame)
        return false;

    ucmd->forwardmove = 1;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;
    ucmd->buttons |= BUTTON_SPECIAL;
    hasPendingLandingDash = false;
    requestedViewTurnSpeedMultiplier = 1.0f;
    return true;
}

bool Bot::CheckPendingLandingDashTimedOut()
{
    if (hasPendingLandingDash && pendingLandingDashTimeout <= level.time)
    {
        hasPendingLandingDash = false;
        requestedViewTurnSpeedMultiplier = 1.0f;
        return true;
    }
    return false;
}

void Bot::MoveGenericRunning(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    if (TryApplyPendingLandingDash(ucmd))
        return;

    Vec3 velocityVec(self->velocity);
    float speed = velocityVec.SquaredLength() > 0.01f ? velocityVec.LengthFast() : 0;

    bool lookingOnImportantItem = StraightenOrInterpolateLookVec(intendedLookVec, speed);
    bool hasObstacles = CheckAndTryAvoidObstacles(intendedLookVec, ucmd, speed);

    Vec3 toTargetDir2D(*intendedLookVec);
    toTargetDir2D.Z() = 0;

    Vec3 velocityDir2D(velocityVec);
    velocityDir2D.Z() = 0;

    float speed2DSquared = velocityDir2D.SquaredLength();
    float toTargetDir2DSqLen = toTargetDir2D.SquaredLength();

    if (speed2DSquared > 0.1f)
    {
        velocityDir2D *= Q_RSqrt(speed2DSquared);

        ucmd->forwardmove = 1;

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
        }

        if (toTargetDir2DSqLen > 0.1f)
        {
            toTargetDir2D *= Q_RSqrt(toTargetDir2DSqLen);

            float velocityToTarget2DDot = velocityDir2D.Dot(toTargetDir2D);
            if (velocityToTarget2DDot < 0.99)
            {
                // Correct trajectory using cheating aircontrol
                if (velocityToTarget2DDot > 0)
                {
                    // Make correction less effective for large angles multiplying it
                    // by the dot product to avoid a weird-looking cheating movement
                    float controlMultiplier = 0.005f + velocityToTarget2DDot * 0.05f;
                    if (lookingOnImportantItem)
                        controlMultiplier += 0.33f;
                    Vec3 newVelocity(velocityVec);
                    newVelocity *= 1.0f / speed;
                    newVelocity += controlMultiplier * toTargetDir2D;
                    // Preserve velocity magnitude
                    newVelocity.NormalizeFast();
                    newVelocity *= speed;
                    VectorCopy(newVelocity.Data(), self->velocity);
                }
                else if (IsCloseToAnyGoal())
                {
                    // velocity and forwardLookDir may mismatch, retrieve these actual look dirs
                    vec3_t forwardLookDir, rightLookDir;
                    AngleVectors(self->s.angles, forwardLookDir, rightLookDir, nullptr);

                    // Stop bunnying and move to goal using only keys
                    ucmd->upmove = 0;
                    ucmd->buttons &= ~BUTTON_SPECIAL;

                    ucmd->forwardmove = 0;
                    ucmd->sidemove = 0;

                    float targetDirDotForward = toTargetDir2D.Dot(forwardLookDir);
                    float targetDirDotRight = toTargetDir2D.Dot(rightLookDir);

                    if (targetDirDotForward > 0.3f)
                        ucmd->forwardmove = 1;
                    else if (targetDirDotForward < -0.3f)
                        ucmd->forwardmove = -1;

                    if (targetDirDotRight > 0.3f)
                        ucmd->sidemove = 1;
                    else if (targetDirDotRight > 0.3f)
                        ucmd->sidemove = -1;

                    // Prevent blocking if neither forwardmove, not sidemove has been chosen
                    if (!ucmd->forwardmove && !ucmd->sidemove)
                        ucmd->forwardmove = -1;
                }
            }

            // Apply cheating acceleration if bot is moving quite straight to a target
            constexpr float accelDotThreshold = 0.9f;
            if (velocityToTarget2DDot > accelDotThreshold)
            {
                if (!self->groundentity && !hasObstacles && !IsCloseToAnyGoal() && Skill() > 0.33f)
                {
                    if (speed > 320.0f) // Avoid division by zero and logic errors
                    {
                        // Max accel is measured in units per second and decreases with speed
                        // For speed of 320 maxAccel is 120
                        // For speed of 700 and greater maxAccel is 0
                        // This means cheating acceleration is not applied for speeds greater than 700 ups
                        // However the bot may reach greater speed since builtin GS_NEWBUNNY forward accel is enabled
                        float maxAccel = 120.0f * (1.0f - BoundedFraction(speed - 320.0f, 700.0f - 320.0f));
                        // Accel contains of constant and directional parts
                        // If velocity dir exactly matches target dir, accel = maxAccel
                        float accel = 0.5f;
                        accel += 0.5f * (velocityToTarget2DDot - accelDotThreshold) / (1.0f - accelDotThreshold);
                        accel *= maxAccel;

                        Vec3 velocityBoost(self->velocity);
                        // Normalize velocity direction
                        velocityBoost *= 1.0f / speed;
                        // Multiply by accel and frame time in millis
                        velocityBoost *= accel * 0.001f * game.frametime;
                        // Modify bot entity velocity
                        VectorAdd(self->velocity, velocityBoost.Data(), self->velocity);
                    }
                }
            }

            // Don't bend too hard
            intendedLookVec->Z() *= Z_NO_BEND_SCALE;
        }
    }
    else
    {
        // Looks like we are falling on target or being pushed up through it
        ucmd->forwardmove = 0;
        ucmd->upmove = 0;
    }
}

void Bot::CheckTargetProximity()
{
    // This is the only action related to reach-at-touch items that may be performed here.
    // Other actions for that kind of goals are performed in Ai::TouchedEntity, Ai/Bot::TouchedNotSolidTriggerEntity
    if (currAasAreaNum != goalAasAreaNum)
    {
        // If the bot was waiting for item spawn and for example has been pushed from the goal, stop camping a goal
        if (isWaitingForItemSpawn)
        {
            ClearCampingSpot();
            isWaitingForItemSpawn = false;
        }
        return;
    }

    if (botBrain.IsCloseToAnyGoal())
    {
        if (botBrain.TryReachGoalByProximity())
        {
            goalAasAreaNum = 0;
            nextReaches.clear();
            return;
        }
        if (botBrain.ShouldWaitForGoal())
        {
            if (!isWaitingForItemSpawn)
            {
                SetCampingSpot(botBrain.ClosestGoalOrigin(), 36.0f, 0.75f);
                isWaitingForItemSpawn = true;
            }
        }
    }
}

void Bot::OnGoalCleanedUp(const NavEntity *goalEnt)
{
    if (isWaitingForItemSpawn)
    {
        ClearCampingSpot();
        isWaitingForItemSpawn = false;
    }
}

Vec3 Bot::MakeEvadeDirection(const Danger &danger)
{
    if (danger.splash)
    {
        Vec3 result(0, 0, 0);
        Vec3 selfToHitDir = danger.hitPoint - self->s.origin;
        RotatePointAroundVector(result.Data(), &axis_identity[AXIS_UP], selfToHitDir.Data(), -self->s.angles[YAW]);
        result.NormalizeFast();

        if (fabs(result.X()) < 0.3) result.X() = 0;
        if (fabs(result.Y()) < 0.3) result.Y() = 0;
        result.Z() = 0;
        result.X() *= -1.0f;
        result.Y() *= -1.0f;
        return result;
    }

    Vec3 selfToHitPoint = danger.hitPoint - self->s.origin;
    selfToHitPoint.Z() = 0;
    // If bot is not hit in its center, try pick a direction that is opposite to a vector from bot center to hit point
    if (selfToHitPoint.SquaredLength() > 4 * 4)
    {
        selfToHitPoint.NormalizeFast();
        // Check whether this direction really helps to evade the danger
        // (the less is the abs. value of the dot product, the closer is the chosen direction to a perpendicular one)
        if (fabs(selfToHitPoint.Dot(danger.direction)) < 0.5f)
        {
            if (fabs(selfToHitPoint.X()) < 0.3) selfToHitPoint.X() = 0;
            if (fabs(selfToHitPoint.Y()) < 0.3) selfToHitPoint.Y() = 0;
            return -selfToHitPoint;
        }
    }

    // Otherwise just pick a direction that is perpendicular to the danger direction
    float maxCrossSqLen = 0.0f;
    Vec3 result(0, 1, 0);
    for (int i = 0; i < 3; ++i)
    {
        Vec3 cross = danger.direction.Cross(&axis_identity[i * 3]);
        cross.Z() = 0;
        float crossSqLen = cross.SquaredLength();
        if (crossSqLen > maxCrossSqLen)
        {
            maxCrossSqLen = crossSqLen;
            float invLen = Q_RSqrt(crossSqLen);
            result.X() = cross.X() * invLen;
            result.Y() = cross.Y() * invLen;
        }
    }
    return result;
}

constexpr auto AI_COMBATMOVE_TIMEOUT = 400;

void Bot::CombatMovement(usercmd_t *ucmd, bool hasToEvade)
{
    if (hasToEvade)
    {
        MakeEvadeMovePushes(ucmd);
    }
    else if (combatMovePushTimeout <= level.time)
    {
        combatMovePushTimeout = level.time + AI_COMBATMOVE_TIMEOUT;
        UpdateCombatMovePushes();
    }

    // Dash is a single-frame event not affected by friction, so it should be checked each frame
    if (MayApplyCombatDash())
        ucmd->buttons |= BUTTON_SPECIAL;

    // Copy cached or updated key directions to ucmd
    ucmd->forwardmove = combatMovePushes[0];
    ucmd->sidemove = combatMovePushes[1];
    ucmd->upmove = combatMovePushes[2];
}

void Bot::UpdateCombatMovePushes()
{
    VectorClear(combatMovePushes);

    // Use the same algorithm for combat move vec as for roaming look vec. The difference is:
    // In roaming movement we keep forward pressed and align view to look vec
    // In combat movement we keep view as-is (it set by aiming ai part) and move to goal using appropriate keys
    Vec3 intendedMoveVec(0, 0, 0);
    StraightenOrInterpolateLookVec(&intendedMoveVec, (float)VectorLength(self->velocity));
    intendedMoveVec.NormalizeFast();

    vec3_t forward, right;
    AngleVectors(self->s.angles, forward, right, nullptr);

    const float moveDotForward = intendedMoveVec.Dot(forward);
    const float moveDotRight = intendedMoveVec.Dot(right);

    Vec3 toEnemyDir = EnemyOrigin() - self->s.origin;
    toEnemyDir.NormalizeFast();

    float forwardBackRandomness = 0.2f;
    float rightLeftRandomness = 0.2f;
    // Increase randomness for directions perpendicular to enemy
    if (fabsf(toEnemyDir.Dot(forward)) < 0.3f)
        forwardBackRandomness += 0.15f;
    if (fabsf(toEnemyDir.Dot(right)) < 0.3f)
        rightLeftRandomness += 0.15f;

    // Choose direction randomly but leaning to intendedMoveVec direction
    // Do not cache random() call to make choices independent

    if (moveDotForward > 0.3f)
        combatMovePushes[0] = random() > forwardBackRandomness ? 1 : -1;
    else if (moveDotForward < -0.3f)
        combatMovePushes[0] = random() > forwardBackRandomness ? -1 : 1;
    else if (random() > 0.85f)
        combatMovePushes[0] = Q_sign(random() - 0.5f);

    if (moveDotRight > 0.3f)
        combatMovePushes[1] = random() > rightLeftRandomness ? 1 : -1;
    else if (moveDotRight < -0.3f)
        combatMovePushes[1] = random() > rightLeftRandomness ? -1 : 1;
    else if (random() > 0.85f)
        combatMovePushes[1] = Q_sign(random() - 0.5f);

    // If neither forward-back, nor left-right direction has been chosen, chose directions randomly
    if (!combatMovePushes[0] && !combatMovePushes[1])
    {
        combatMovePushes[0] = Q_sign(random() - 0.5f);
        combatMovePushes[1] = Q_sign(random() - 0.5f);
    }

    // Tend to jump or crouch excessively if we have height advantage
    if (toEnemyDir.Z() < -0.3)
        combatMovePushes[2] = random() > 0.75f ? Q_sign(random() - 0.5f) : 0;
    // Otherwise do these moves sparingly only to surprise enemy sometimes
    else
        combatMovePushes[2] = random() > 0.9f ? Q_sign(random() - 0.5f) : 0;
}

void Bot::MakeEvadeMovePushes(usercmd_t *ucmd)
{
    Vec3 evadeDir = MakeEvadeDirection(*dangersDetector.primaryDanger);
#ifdef _DEBUG
    Vec3 drawnDirStart(self->s.origin);
    drawnDirStart.Z() += 32;
    Vec3 drawnDirEnd = drawnDirStart + 64.0f * evadeDir;
    AITools_DrawLine(drawnDirStart.Data(), drawnDirEnd.Data());
#endif

    int walkingEvades = 0;
    int walkingMovePushes[3] = {0, 0, 0};
    int jumpingEvades = 0;
    int jumpingMovePushes[3] = {0, 0, 0};

    if (evadeDir.X())
    {
        if ((evadeDir.X() < 0))
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
        else if ((evadeDir.X() > 0))
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
    if (evadeDir.Y())
    {
        if ((evadeDir.Y() < 0))
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
        else if ((evadeDir.Y() > 0))
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

