#include "bot.h"
#include "ai_aas_world.h"
#include "ai_ground_trace_cache.h"

void Bot::MoveFrame(usercmd_t *ucmd)
{
    pendingLandingDashState.isOnGroundThisFrame = self->groundentity != nullptr;

    pendingLandingDashState.TryInvalidate();
    rocketJumpMovementState.TryInvalidate();

    // These triggered actions should be processed
    if (jumppadMovementState.IsActive() || rocketJumpMovementState.IsActive() || pendingLandingDashState.IsActive())
    {
        Move(ucmd);
    }
    else
    {
        if (!selectedEnemies.AreValid() || !ShouldKeepXhairOnEnemy() || MayHitWhileRunning())
            Move(ucmd);
        else
            CombatMovement(ucmd);
    }

    CheckTargetProximity();

    pendingLandingDashState.wasOnGroundPrevFrame = pendingLandingDashState.isOnGroundThisFrame;
    rocketJumpMovementState.wasTriggeredPrevFrame = rocketJumpMovementState.hasTriggeredRocketJump;
}

void Bot::Move(usercmd_t *ucmd)
{
    if (currAasAreaNum == 0)
        return;

    if (!botBrain.HasNavTarget())
        return;

    const int goalAasAreaNum = NavTargetAasAreaNum();
    if (nextReaches.empty() && currAasAreaNum != goalAasAreaNum)
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
        intendedLookVec = Vec3(self->s.origin) - NavTargetOrigin();
    }

    if (self->is_ladder)
    {
        MoveOnLadder(&intendedLookVec, ucmd);
    }
    else if (jumppadMovementState.hasTouchedJumppad)
    {
        MoveEnteringJumppad(&intendedLookVec, ucmd);
    }
    else if (jumppadMovementState.hasEnteredJumppad)
    {
        MoveRidingJummpad(&intendedLookVec, ucmd);
    }
    else if (self->groundentity && Use_Plat == self->groundentity->use)
    {
        MoveOnPlatform(&intendedLookVec, ucmd);
    }
    else if (rocketJumpMovementState.IsActive())
    {
        MoveTriggeredARocketJump(&intendedLookVec, ucmd);
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

    if (!HasPendingLookAtPoint() && !rocketJumpMovementState.HasBeenJustTriggered())
    {
        float turnSpeedMultiplier = pendingLandingDashState.EffectiveTurnSpeedMultiplier(1.0f);
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

void Bot::MoveOnLadder(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;
}

void Bot::MoveTriggeredARocketJump(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    *intendedLookVec = rocketJumpMovementState.jumpTarget - self->s.origin;

    ucmd->forwardmove = 0;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (!rocketJumpMovementState.hasCorrectedRocketJump)
    {
        Vec3 newVelocity(rocketJumpMovementState.fireTarget);
        newVelocity -= self->s.origin;
        float squareCorrectedDirLen = newVelocity.SquaredLength();
        if (squareCorrectedDirLen > 1)
        {
            float speed = (float)VectorLength(self->velocity);
            newVelocity *= Q_RSqrt(squareCorrectedDirLen);
            newVelocity *= speed;
            VectorCopy(newVelocity.Data(), self->velocity);
        }
        rocketJumpMovementState.hasCorrectedRocketJump = true;
    }
    else if (rocketJumpMovementState.timeoutAt - level.time < 300)
    {
        ucmd->forwardmove = 1;
        // Bounce off walls
        if (!closeAreaProps.leftTest.CanWalk() || !closeAreaProps.rightTest.CanWalk())
            ucmd->buttons |= BUTTON_SPECIAL;
    }
}

template <typename T> struct AttributedArea
{
    int areaNum;
    T attr;
    AttributedArea() {}
    AttributedArea(int areaNum_, T attr_): areaNum(areaNum_), attr(attr_) {}
    bool operator<(const AttributedArea &that) const { return attr < that.attr; }
};

void Bot::MoveEnteringJumppad(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (jumppadMovementState.hasEnteredJumppad)
        return;

    if (!botBrain.HasNavTarget())
        return;

    const int goalAasAreaNum = NavTargetAasAreaNum();

    // Cache reference to avoid indirections
    const aas_area_t *aasWorldAreas = aasWorld->Areas();

    constexpr auto MAX_LANDING_AREAS = JumppadMovementState::MAX_LANDING_AREAS;
    int &jumppadLandingAreasCount = jumppadMovementState.landingAreasCount;
    int *jumppadLandingAreas = jumppadMovementState.landingAreas;
    const Vec3 &jumppadTarget = jumppadMovementState.jumppadTarget;

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
    int rawAreasCount = aasWorld->BBoxAreas(bboxMins, bboxMaxs, rawAreas, MAX_LANDING_AREAS * 2);
    // Then filter raw areas and sort by distance to jumppad target
    AttributedArea<float> filteredAreas[MAX_LANDING_AREAS * 2];
    int filteredAreasCount = 0;
    for (int i = 0; i < rawAreasCount; ++i)
    {
        int areaNum = rawAreas[i];
        // Skip areas above target that a-priori may not be a landing site. Areas bounds are absolute.
        if (aasWorldAreas[areaNum].mins[2] + 8 > jumppadTarget.Z())
            continue;
        // Skip non-grounded areas
        if (!aasWorld->AreaGrounded(areaNum))
            continue;
        // Skip "do not enter" areas
        if (aasWorld->AreaDoNotEnter(areaNum))
            continue;
        float squareDistance = (jumppadTarget - aasWorld->Areas()[areaNum].center).SquaredLength();
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
        Vec3 origin(aasWorldAreas[areaNum].center);
        origin.Z() = aasWorldAreas[areaNum].mins[2] + 8;
        // Returns 1 as a lowest feasible travel time value (in seconds ^-2), 0 when a path can't be found
        int aasTravelTime = routeCache->TravelTimeToGoalArea(areaNum, origin.Data(), goalAasAreaNum, PreferredTravelFlags());
        if (!aasTravelTime)
            aasTravelTime = routeCache->TravelTimeToGoalArea(areaNum, origin.Data(), goalAasAreaNum, AllowedTravelFlags());
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
    jumppadMovementState.hasEnteredJumppad = true;
}

void Bot::MoveRidingJummpad(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    // First check whether bot finally landed to some area
    if (self->groundentity)
    {
        jumppadMovementState.hasEnteredJumppad = false;
        ucmd->forwardmove = 1;
        return;
    }

    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    if (jumppadMovementState.jumppadMoveTimeout <= level.time)
    {
        int jumppadLandingAreasCount = jumppadMovementState.landingAreasCount;
        const int *jumppadLandingAreas = jumppadMovementState.landingAreas;
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

    int numAllAreas = aasWorld->BBoxAreas(bboxMins.Data(), bboxMaxs.Data(), areas, MAX_LANDING_AREAS);
    if (!numAllAreas)
        return;

    int numGroundedAreas = 0;
    for (int i = 0; i < numAllAreas; ++i)
    {
        if (aasWorld->AreaGrounded(areas[i]))
            groundedAreas[numGroundedAreas++] = areas[i];
    }

    // Sort areas by distance from bot to area bottom

    for (int i = 0; i < numGroundedAreas; ++i)
    {
        const aas_area_t &area = aasWorld->Areas()[groundedAreas[i]];
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
    Vec3 areaPoint(aasWorld->Areas()[areaNum].center);

    // Lower area point to a bottom of area. Area mins/maxs are absolute.
    areaPoint.Z() = aasWorld->Areas()[areaNum].mins[2];
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
    Vec3 lookAtPoint(campingSpotState.lookAtPoint);
    // If the camping action does not have a defined look direction, choose some random one
    if (!campingSpotState.hasLookAtPoint)
    {
        // If the previously chosen look at point has timed out, choose a new one
        if (campingSpotState.lookAtPointTimeoutAt <= level.time)
        {
            // Choose some random point to look at
            campingSpotState.lookAtPoint.X() = self->s.origin[0] - 50.0f + 100.0f * random();
            campingSpotState.lookAtPoint.Y() = self->s.origin[1] - 50.0f + 100.0f * random();
            campingSpotState.lookAtPoint.Z() = self->s.origin[2] - 15.0f + 30.0f * random();
            campingSpotState.lookAtPointTimeoutAt = level.time + 1500 - (unsigned)(1250.0f * campingSpotState.alertness);
        }
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

    Vec3 botToSpot = campingSpotState.spotOrigin - self->s.origin;
    float distance = botToSpot.Length();

    if (distance / campingSpotState.spotRadius > 2.0f)
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

    Vec3 expectedLookDir = lookAtPoint - campingSpotState.spotOrigin;
    expectedLookDir.NormalizeFast();

    if (expectedLookDir.Dot(actualLookDir) < 0.85)
    {
        if (!HasPendingLookAtPoint())
        {
            SetPendingLookAtPoint(campingSpotState.lookAtPoint, 1.1f);
            ucmd->forwardmove = 0;
            ucmd->sidemove = 0;
            ucmd->upmove = 0;
            ucmd->buttons |= BUTTON_WALK;
            return;
        }
    }

    // Keep actual look dir as-is, adjust position by keys only
    *intendedLookVec = Vec3(actualLookDir);

    if (campingSpotState.strafeTimeoutAt < level.time)
    {
        // This means we may strafe randomly
        if (distance / campingSpotState.spotRadius < 0.7f)
        {
            campingSpotState.strafeDir.X() = -0.5f + random();
            campingSpotState.strafeDir.Y() = -0.5f + random();
            campingSpotState.strafeDir.Z() = 0.0f;
            campingSpotState.strafeTimeoutAt = level.time + 500;
            campingSpotState.strafeTimeoutAt += (unsigned)(100.0f * random() - 250.0f * campingSpotState.alertness);
        }
        else
        {
            campingSpotState.strafeDir = botToSpot;
        }
        campingSpotState.strafeDir.NormalizeFast();
    }

    Vec3 strafeMoveVec = campingSpotState.strafeDir;

    KeyMoveVecToUcmd(strafeMoveVec, actualLookDir, actualRightDir, ucmd);

    if (random() > campingSpotState.alertness * 0.75f)
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
        if (currAasAreaNum != NavTargetAasAreaNum())
        {
            // Looks like we are in air above a ground, keep as is waiting for landing.
            VectorCopy(self->velocity, intendedLookVec->Data());
            return false;
        }
        // Move to a goal origin
        *intendedLookVec = NavTargetOrigin() - self->s.origin;
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
    if (i == nextReaches.size())
    {
        // If a reachablities chain contains goal area, goal area is last in the chain
        if (nextReaches[i - 1].areanum == NavTargetAasAreaNum())
        {
            traceStraightenedPath = true;
            lookAtPoint = NavTargetOrigin();
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

    // Cache a referece to avoid indirection
    const aas_area_t *areas = aasWorld->Areas();

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
                singleFarNextAreaCenterDir = Vec3(areas[reach.areanum].center) - self->s.origin;
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
        VectorCopy(areas[reach.areanum].center, centerDir);
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

bool Bot::MaySetPendingLandingDash()
{
    // A bot is on ground
    if (self->groundentity)
        return false;

    // Can't dash
    if (!(self->r.client->ps.stats[PM_STAT_FEATURES] & PMFEAT_DASH))
        return false;

    // 2D speed is too low
    if (self->velocity[0] * self->velocity[0] + self->velocity[1] * self->velocity[1] < 600.0f * 600.0f)
        return false;

    // Set a pending landing dash only being in middle of large area.
    // Many small areas may provide enough space to do a pending dash,
    // but in this case its likely that the intended movement vector will change its direction too quickly.
    if (!aasWorld->AreaGrounded(droppedToFloorAasAreaNum))
        return false;
    const aas_area_t &area = aasWorld->Areas()[droppedToFloorAasAreaNum];
    if (area.maxs[0] - self->s.origin[0] < 40.0f || self->s.origin[0] - area.mins[0] < 40.0f)
        return false;
    if (area.maxs[1] - self->s.origin[1] < 40.0f || self->s.origin[1] - area.mins[1] < 40.0f)
        return false;

    float traceDepth = -playerbox_stand_mins[2] + AI_JUMPABLE_HEIGHT;
    trace_t trace;
    AiGroundTraceCache::Instance()->GetGroundTrace(self, traceDepth, &trace);
    // A bot is too high in air, so area assumptions are unlikely to be valid
    if (trace.fraction == 1.0f)
        return false;

    float distanceToGround = traceDepth * trace.fraction + playerbox_stand_maxs[2];
    // A bot will not be able to turn view to the desired direction to the moment of landing
    if (distanceToGround < 6.0f && self->velocity[2] < 0.0f)
        return false;

    return true;
}

void Bot::SetPendingLandingDash(usercmd_t *ucmd)
{
    ucmd->forwardmove = 0;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;

    pendingLandingDashState.SetTriggered(700);
}

void Bot::ApplyPendingLandingDash(usercmd_t *ucmd)
{
    if (!pendingLandingDashState.MayApplyDash())
        return;

    ucmd->forwardmove = 1;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;
    ucmd->buttons |= BUTTON_SPECIAL;

    pendingLandingDashState.Invalidate();
}

void Bot::MoveGenericRunning(Vec3 *intendedLookVec, usercmd_t *ucmd)
{
    if (pendingLandingDashState.IsActive())
    {
        ApplyPendingLandingDash(ucmd);
        return;
    }

    // TryRocketJumpShortcut() is expensive, call it only in Think() frames
    if (!ShouldBeSilent() && !ShouldSkipThinkFrame() && TryRocketJumpShortcut(ucmd))
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

    const short *movementSettings = self->r.client->ps.pmove.stats;
    const short movementFeatures = movementSettings[PM_STAT_FEATURES];

    if (speed2DSquared > 0.1f)
    {
        velocityDir2D *= Q_RSqrt(speed2DSquared);

        ucmd->forwardmove = 1;

        if (movementFeatures & PMFEAT_DASH)
        {
            if (speed < movementSettings[PM_STAT_DASHSPEED])
            {
                ucmd->upmove = 0;
                if (self->groundentity)
                    ucmd->buttons |= BUTTON_SPECIAL;
            }
            // If we are not crouching in air to prevent bumping a ceiling, keep jump key pressed
            else if (ucmd->upmove != -1 && movementFeatures & PMFEAT_JUMP)
                ucmd->upmove = 1;
        }
        else
        {
            if (speed < movementSettings[PM_STAT_MAXSPEED])
                ucmd->upmove = 0;
            else if (ucmd->upmove != -1 && movementFeatures & PMFEAT_JUMP)
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
                    if (movementFeatures & PMFEAT_AIRCONTROL)
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
                }
                else if (ShouldMoveCarefully())
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
                else if (velocityToTarget2DDot < 0.1f)
                {
                    if (!ShouldBeSilent() && MaySetPendingLandingDash())
                    {
                        SetPendingLandingDash(ucmd);
                        return;
                    }
                }
            }

            // Apply cheating acceleration if bot is moving quite straight to a target
            constexpr float accelDotThreshold = 0.9f;
            if (velocityToTarget2DDot > accelDotThreshold)
            {
                if (!self->groundentity && !hasObstacles && !ShouldMoveCarefully() && Skill() > 0.33f)
                {
                    float runSpeed = movementSettings[PM_STAT_MAXSPEED];
                    if (speed > runSpeed) // Avoid division by zero and logic errors
                    {
                        // Max accel is measured in units per second and decreases with speed
                        // For speed of runSpeed maxAccel is 180
                        // For speed of 900 and greater maxAccel is 0
                        // This means cheating acceleration is not applied for speeds greater than 900 ups
                        // However the bot may reach greater speed since builtin GS_NEWBUNNY forward accel is enabled
                        float maxAccel = 180.0f * (1.0f - BoundedFraction(speed - runSpeed, 900.0f - runSpeed));

                        // Modify maxAccel to respect player class movement limitations
                        maxAccel *= movementSettings[PM_STAT_MAXSPEED] / 320.0f;

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

            // Prevent bending except in air (where it is useful to push a bot to a goal)
            if (self->groundentity || !IsCloseToNavTarget())
                intendedLookVec->Z() *= Z_NO_BEND_SCALE;
        }
    }
    else
    {
        // Looks like we are falling on target or being pushed up through it
        ucmd->forwardmove = 0;
        ucmd->upmove = 0;
    }

    // Prevent falling by switching to a cautious move style
    if (closeAreaProps.frontTest.CanFall() || closeAreaProps.leftTest.CanFall() || closeAreaProps.rightTest.CanFall())
    {
        ucmd->buttons &= ~BUTTON_SPECIAL;
        ucmd->upmove = 0;
    }
    // If there is a side wall
    else if (!closeAreaProps.leftTest.CanWalk() || !closeAreaProps.rightTest.CanWalk())
    {
        // If bot is in air and there are no obstacles on chosen forward direction, do a WJ.
        if (!self->groundentity && !hasObstacles)
            if (movementFeatures & PMFEAT_WALLJUMP)
                ucmd->buttons |= BUTTON_SPECIAL;
    }

    // Skip dash and WJ near triggers to prevent missing a trigger
    if (!nextReaches.empty())
    {
        switch (nextReaches.front().traveltype)
        {
            case TRAVEL_TELEPORT:
            case TRAVEL_JUMPPAD:
            case TRAVEL_ELEVATOR:
            case TRAVEL_LADDER:
            case TRAVEL_BARRIERJUMP:
                ucmd->buttons &= ~BUTTON_SPECIAL;
            default:
                if (IsCloseToNavTarget())
                    ucmd->buttons &= ~BUTTON_SPECIAL;
        }
    }

    if (ShouldBeSilent())
    {
        ucmd->upmove = 0;
        ucmd->buttons &= ~BUTTON_SPECIAL;
    }
}

bool Bot::TryRocketJumpShortcut(usercmd_t *ucmd)
{
    // Try to do coarse cheap checks first to prevent wasting CPU cycles in G_Trace()

    if (!botBrain.HasNavTarget())
        return false;
    // No need for that
    if (currAasAreaNum == NavTargetAasAreaNum())
        return false;

    float squareDistanceToGoal = DistanceSquared(self->s.origin, NavTargetOrigin().Data());

    // Too close to the goal
    if (squareDistanceToGoal < 128.0f * 128.0f)
        return false;

    bool canRefillHealthAndArmor = true;
    canRefillHealthAndArmor &= (level.gametype.spawnableItemsMask & IT_HEALTH) != 0;
    canRefillHealthAndArmor &= (level.gametype.spawnableItemsMask & IT_ARMOR) != 0;

    // This means a bot would inflict himself a damage (he can't switch to a safe gun)
    if (GS_SelfDamage() && !Inventory()[WEAP_INSTAGUN])
    {
        float damageToBeKilled = DamageToKill(self, g_armor_protection->value, g_armor_degradation->value);
        if (HasQuad(self))
            damageToBeKilled /= 4.0f;
        if (HasShell(self))
            damageToBeKilled *= 4.0f;

        // Can't refill health and armor picking it on the map
        if (damageToBeKilled < 60.0f || (damageToBeKilled < 200.0f && !canRefillHealthAndArmor))
            return false;

        // We tested minimal estimated damage values to cut off
        // further expensive tests that involve lots of traces.
        // Selfdamage should be tested again when weapon selection is performed.
    }

    if (!self->groundentity)
    {
        // Check whether a bot has a ground surface to push off it
        // TODO: Check for walls?
        trace_t trace;
        AiGroundTraceCache::Instance()->GetGroundTrace(self, 36, &trace);
        if (trace.fraction == 1.0f)
            return false;
        if (trace.surfFlags & SURF_NOIMPACT)
            return false;
    }

    Vec3 targetOrigin(0, 0, 0);
    Vec3 fireTarget(0, 0, 0);
    if (squareDistanceToGoal < 750.0f * 750.0f)
    {
        if (!AdjustDirectRocketJumpToAGoalTarget(&targetOrigin, &fireTarget))
            return false;
    }
    else
    {
        // The goal does not seam to be reachable for RJ.
        // Try just shortcut a path.
        if (!AdjustRocketJumpTargetForPathShortcut(&targetOrigin, &fireTarget))
            return false;
    }

    return TryTriggerWeaponJump(ucmd, targetOrigin, fireTarget);
}

bool Bot::AdjustRocketJumpTargetForPathShortcut(Vec3 *targetOrigin, Vec3 *fireTarget) const
{
    // Cache references to avoid indirections
    const aas_area_t *aasWorldAreas = aasWorld->Areas();
    const aas_areasettings_t *aasWorldAreaSettings = aasWorld->AreaSettings();

    const aas_area_t &currArea = aasWorldAreas[currAasAreaNum];
    trace_t trace;
    // Avoid occasional unsigned overflow (the loop starts from nextReaches.size() - 1 or 0)
    for (unsigned i = std::min(nextReaches.size(), nextReaches.size() - 1); i >= 1; --i)
    {
        const auto &reach = nextReaches[i];
        // Reject non-grounded areas
        if (!aasWorldAreaSettings[reach.areanum].areaflags & AREA_GROUNDED)
            continue;
        const auto &area = aasWorldAreas[reach.areanum];
        // Can't RJ to lower areas
        if (area.mins[2] - currArea.mins[2] < 48.0f)
            continue;
        // Can't be reached using RJ aprori
        if (area.mins[2] - currArea.mins[2] > 280.0f)
            continue;
        // Avoid rocketjumping to degenerate areas
        if (area.maxs[0] - area.mins[0] < 32.0f)
            continue;
        if (area.maxs[1] - area.mins[1] < 32.0f)
            continue;

        // Then we try to select a grounded area point nearest to a bot
        // Offset this Z a bit above the ground to ensure that a point is strictly inside the area.
        const float groundZ = area.mins[2] + 4.0f;
        // Find nearest point of area AABB base
        const float *bounds[2] = { area.mins, area.maxs };
        vec3_t nearestBasePoint = { NAN, NAN, NAN };
        float minDistance = std::numeric_limits<float>::max();
        for (int j = 0; j < 4; ++j)
        {
            vec3_t basePoint;
            basePoint[0] = bounds[(j >> 0) & 1][0];
            basePoint[1] = bounds[(j >> 1) & 1][1];
            basePoint[2] = groundZ;
            float distance = DistanceSquared(self->s.origin, basePoint);
            if (distance < minDistance)
            {
                minDistance = distance;
                VectorCopy(basePoint, nearestBasePoint);
            }
        }

        // We can't simply use nearest point of area AABB
        // since AABB may be greater than area itself (areas may be an arbitrary prism).
        // Use an average of area grounded center and nearest grounded point for approximation of target point.
        Vec3 areaPoint(area.center);
        areaPoint.Z() = groundZ;
        areaPoint += nearestBasePoint;
        areaPoint *= 0.5f;

        float squareDistanceToArea = DistanceSquared(self->s.origin, areaPoint.Data());
        if (squareDistanceToArea > 550.0f * 550.0f)
            continue;
        if (squareDistanceToArea < 64.0f * 64.0f)
            continue;

        float distanceToArea = 1.0f / Q_RSqrt(squareDistanceToArea);

        Vec3 traceEnd(areaPoint);
        traceEnd.Z() += 48.0f + 64.0f * BoundedFraction(distanceToArea, 500);

        // Check whether there are no obstacles on a segment from bot origin to fireTarget
        G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, traceEnd.Data(), self, MASK_AISOLID);
        if (trace.fraction == 1.0f)
        {
            VectorCopy(areaPoint.Data(), targetOrigin->Data());
            VectorCopy(traceEnd.Data(), fireTarget->Data());
            return true;
        }
    }

    return false;
}

bool Bot::AdjustDirectRocketJumpToAGoalTarget(Vec3 *targetOrigin, Vec3 *fireTarget) const
{
    // Make aliases for convenience to avoid pointer operations
    Vec3 &targetOriginRef = *targetOrigin;
    Vec3 &fireTargetRef = *fireTarget;

    targetOriginRef = NavTargetOrigin();

    // Check target height for feasibility
    float height = targetOriginRef.Z() - self->s.origin[2];
    if (height < 48.0f || height > 280.0f)
        return false;

    // We are sure this distance not only non-zero but greater than 1.5-2x player height
    float distanceToGoal = DistanceFast(self->s.origin, targetOriginRef.Data());

    fireTargetRef = targetOriginRef;
    fireTargetRef.Z() += 48.0f + 64.0f * BoundedFraction(distanceToGoal, 500);

    Vec3 botToGoal2DDir(targetOriginRef);
    botToGoal2DDir -= self->s.origin;
    botToGoal2DDir.Z() = 0;
    float botToGoalSquareDist2D = botToGoal2DDir.SquaredLength();
    float botToGoalDist2D = -1;

    if (botToGoalSquareDist2D > 1)
    {
        botToGoalDist2D = 1.0f / Q_RSqrt(botToGoalSquareDist2D);
        botToGoal2DDir *= 1.0f / botToGoalDist2D;
        // Aim not directly to goal but a bit closer to bot in XY plane
        // (he will fly down to a goal from the trajectory zenith)
        *fireTarget -= 0.33f * botToGoalDist2D * botToGoal2DDir;
    }

    trace_t trace;
    G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, fireTargetRef.Data(), self, MASK_AISOLID);
    // If there are obstacles
    if (trace.fraction != 1.0f)
    {
        if (botToGoalDist2D < 48.0f)
            return false;

        int targetAreaNum = TryFindRocketJumpAreaCloseToGoal(botToGoal2DDir, botToGoalDist2D);
        if (!targetAreaNum)
            return false;

        const aas_area_t &targetArea = aasWorld->Areas()[targetAreaNum];
        VectorCopy(targetArea.center, targetOriginRef.Data());
        targetOriginRef.Z() = targetArea.mins[2] + 16.0f;

        // Recalculate
        distanceToGoal = DistanceFast(self->s.origin, targetOriginRef.Data());

        VectorCopy(targetOriginRef.Data(), fireTargetRef.Data());
        fireTargetRef.Z() += 48.0f + 64.0f * BoundedFraction(distanceToGoal, 500);
    }

    return true;
}

int Bot::TryFindRocketJumpAreaCloseToGoal(const Vec3 &botToGoalDir2D, float botToGoalDist2D) const
{
    const int goalAreaNum = NavTargetAasAreaNum();
    const aas_area_t &targetArea = aasWorld->Areas()[goalAreaNum];
    Vec3 targetOrigin(targetArea.center);
    targetOrigin.Z() = targetArea.mins[2] + 16.0f;

    Vec3 areaTraceStart(targetArea.center);
    areaTraceStart.Z() = targetArea.mins[2] + 16.0f;

    Vec3 areaTraceEnd(areaTraceStart);
    areaTraceEnd -= (botToGoalDist2D - 48.0f) * botToGoalDir2D;

    int areas[16];
    int numTracedAreas = aasWorld->TraceAreas(targetOrigin.Data(), areaTraceEnd.Data(), areas, nullptr, 16);

    int travelFlags = TFL_WALK | TFL_WALKOFFLEDGE | TFL_AIR;
    trace_t trace;
    // Start from the area closest to the bot, end on first area in trace after the goal area (i = 0)
    for (int i = numTracedAreas - 1; i >= 1; --i)
    {
        // Do not try to reach a non-grounded area
        if (!aasWorld->AreaGrounded(areas[i]))
            continue;

        // Check whether goal area is reachable from area pointed by areas[i]
        // (note, we check reachablity from area to goal area, and not the opposite,
        // because the given travel flags are not reversible).
        const aas_area_t &area = aasWorld->Areas()[areas[i]];
        Vec3 areaPoint(area.center);
        areaPoint.Z() = area.mins[2] + 4.0f;
        int reachNum = routeCache->ReachabilityToGoalArea(areas[i], areaPoint.Data(), goalAreaNum, travelFlags);
        // This means goal area is not reachable with these travel flags
        if (!reachNum)
            continue;

        // Set fireTarget to a point a bit above a grounded center of the area
        // Check whether there are obstacles on a line from bot origin to fireTarget
        Vec3 fireTarget = areaPoint;
        fireTarget.Z() += 48.0f + 64.0f * BoundedFraction((areaPoint - self->s.origin).LengthFast(), 500);

        G_Trace(&trace, self->s.origin, nullptr, playerbox_stand_maxs, fireTarget.Data(), self, MASK_AISOLID);
        // fireTarget is reachable
        if (trace.fraction == 1.0f)
            return areas[i];
    }

    return 0;
}

bool Bot::TryTriggerWeaponJump(usercmd_t *ucmd, const Vec3 &targetOrigin, const Vec3 &fireTarget)
{
    // Update values for adjusted target
    float originDistance = DistanceFast(self->s.origin, targetOrigin.Data());
    float originHeight = targetOrigin.Z() - self->s.origin[2];

    // TODO: Compute actual trajectory
    if (originDistance > 550.0f || originHeight > 280.0f)
        return false;

    // Select appropriate weapon
    // TODO: Compute actual trajectory
    int weapon = WEAP_GUNBLADE;
    if (originDistance > 450.0f || originHeight > 240.0f)
    {
        // Should use a powerful weapon. Try to select an IG to prevent damaging itself.
        if (Inventory()[WEAP_INSTAGUN])
            weapon = WEAP_INSTAGUN;
        else if (Inventory()[WEAP_ROCKETLAUNCHER] && (Inventory()[AMMO_ROCKETS] || Inventory()[AMMO_WEAK_ROCKETS]))
            weapon = WEAP_ROCKETLAUNCHER;
        // Can't switch to IG or RL
        if (weapon == WEAP_GUNBLADE)
            return false;
        // Check selfdamage aposteriori
        if (GS_SelfDamage() && weapon == WEAP_ROCKETLAUNCHER)
        {
            float damageToBeKilled = DamageToKill(self, g_armor_protection->value, g_armor_degradation->value);
            if (HasQuad(self))
                damageToBeKilled /= 4.0f;
            if (HasShell(self))
                damageToBeKilled *= 4.0f;
            if (damageToBeKilled < 125)
                return false;
            if (damageToBeKilled < 200.0f && !(level.gametype.spawnableItemsMask & (IT_HEALTH|IT_ARMOR)))
                return false;
        }
    }
    else
    {
        // Should use GB but can't resist it
        if (GS_SelfDamage())
        {
            float damageToBeKilled = DamageToKill(self, g_armor_protection->value, g_armor_degradation->value);
            if (HasQuad(self))
                damageToBeKilled /= 4.0f;
            if (HasShell(self))
                damageToBeKilled *= 4.0f;
            if (damageToBeKilled < 60.0f)
                weapon = WEAP_INSTAGUN;
            else if (damageToBeKilled < 150.0f && !(level.gametype.spawnableItemsMask & (IT_HEALTH|IT_ARMOR)))
                weapon = WEAP_INSTAGUN;
        }
    }

    ChangeWeapon(weapon);
    if (self->r.client->ps.stats[STAT_WEAPON] != weapon)
        return false;
    if (self->r.client->ps.stats[STAT_WEAPON_TIME])
        return false;

    TriggerWeaponJump(ucmd, targetOrigin, fireTarget);
    return true;
}

void Bot::TriggerWeaponJump(usercmd_t *ucmd, const Vec3 &targetOrigin, const Vec3 &fireTarget)
{
    Vec3 botToFireTarget = fireTarget - self->s.origin;
    vec3_t lookAngles = {0, 0, 0};
    VecToAngles((-botToFireTarget).Data(), lookAngles);
    // Is corrected when hasCorrectedRocketJump is checked
    lookAngles[PITCH] = 170.0f;
    VectorCopy(lookAngles, self->s.angles);

    ucmd->forwardmove = 0;
    ucmd->sidemove = 0;
    ucmd->upmove = 1;
    ucmd->buttons |= (BUTTON_ATTACK|BUTTON_SPECIAL);

    rocketJumpMovementState.SetTriggered(targetOrigin, fireTarget, 750);
}

void Bot::CheckTargetProximity()
{
    if (!botBrain.HasNavTarget())
        return;

    if (botBrain.IsCloseToNavTarget(128.0f))
    {
        if (botBrain.TryReachNavTargetByProximity())
        {
            OnNavTargetReset();
            return;
        }
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

void Bot::CombatMovement(usercmd_t *ucmd)
{
    if (combatMovePushTimeout <= level.time)
    {
        combatMovePushTimeout = level.time + AI_COMBATMOVE_TIMEOUT;
        UpdateCombatMovePushes();
    }

    ucmd->forwardmove = combatMovePushes[0];
    ucmd->sidemove = combatMovePushes[1];
    ucmd->upmove = combatMovePushes[2];

    // Dash is a single-frame event not affected by friction, so it should be checked each frame
    if (MayApplyCombatDash())
        ucmd->buttons |= BUTTON_SPECIAL;

    if (self->groundentity)
    {
        if (!(ucmd->buttons & BUTTON_SPECIAL))
        {
            ApplyCheatingGroundAcceleration(ucmd);

            const float maxGroundSpeed = self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED];
            const float squareGroundSpeed = VectorLengthSquared(self->velocity);

            // If bot would not dash, do at least a jump
            if (squareGroundSpeed > 0.64f * maxGroundSpeed * maxGroundSpeed)
                ucmd->upmove = 1;
        }
    }
    else
    {
        // Release forward button in air to use aircontrol
        if (ucmd->sidemove != 0)
            ucmd->forwardmove = 0;
    }
}

void Bot::ApplyCheatingGroundAcceleration(const usercmd_t *ucmd)
{
    vec3_t forward, right;
    AngleVectors(self->s.angles, forward, right, nullptr);

    float speedGainPerSecond = 500.0f * Skill();
    float frameTimeSeconds = 0.0001f * game.frametime;
    float factor = speedGainPerSecond * frameTimeSeconds;

    VectorMA(self->velocity, factor * ucmd->forwardmove, forward, self->velocity);
    VectorMA(self->velocity, factor * ucmd->sidemove, right, self->velocity);

    float squareSpeed = VectorLengthSquared(self->velocity);
    if (squareSpeed > 1)
    {
        float speed = 1.0f / Q_RSqrt(squareSpeed);
        float maxGroundSpeed = self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED];
        if (speed > maxGroundSpeed)
        {
            // Normalize current direction
            VectorScale(self->velocity, 1.0f / speed, self->velocity);
            // Set current velocity magnitude to maxGroundSpeed
            VectorScale(self->velocity, maxGroundSpeed, self->velocity);
        }
    }
}

void Bot::UpdateCombatMovePushes()
{
    VectorClear(combatMovePushes);

    // Use the same algorithm for combat move vec as for roaming look vec. The difference is:
    // In roaming movement we keep forward pressed and align view to look vec
    // In combat movement we keep view as-is (it set by aiming ai part) and move to goal using appropriate keys
    Vec3 intendedMoveVec(0, 0, 0);
    if (botBrain.HasNavTarget())
    {
        StraightenOrInterpolateLookVec(&intendedMoveVec, (float) VectorLength(self->velocity));
    }
    else
    {
        intendedMoveVec.X() = 0.5f - random();
        intendedMoveVec.Y() = 0.5f - random();
        intendedMoveVec.Z() = 0.5f - random();
    }
    intendedMoveVec.NormalizeFast();

    vec3_t forward, right;
    AngleVectors(self->s.angles, forward, right, nullptr);

    const float moveDotForward = intendedMoveVec.Dot(forward);
    const float moveDotRight = intendedMoveVec.Dot(right);

    Vec3 toEnemyDir = EnemyOrigin() - self->s.origin;
    toEnemyDir.NormalizeFast();

    if (moveDotForward > 0.3f)
        combatMovePushes[0] = +1;
    else if (moveDotForward < -0.3f)
        combatMovePushes[0] = -1;
    else if (random() > 0.85f)
        combatMovePushes[0] = Q_sign(random() - 0.5f);

    if (moveDotRight > 0.3f)
        combatMovePushes[1] = +1;
    else if (moveDotRight < -0.3f)
        combatMovePushes[1] = -1;
    else if (random() > 0.85f)
        combatMovePushes[0] = Q_sign(random() - 0.5f);

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

