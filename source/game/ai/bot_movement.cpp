#include "bot.h"
#include "ai_local.h"
#include "aas.h"

void Bot::Move(usercmd_t *ucmd)
{
    if (currAasAreaNum == 0 || goalAasAreaNum == 0)
    {
        // Request a goal to be assigned asap
        longRangeGoalTimeout = 0;
        shortRangeGoalTimeout = 0;
        statusUpdateTimeout = 0;
        return;
    }

    if (hasTriggeredRj && rjTimeout <= level.time)
        hasTriggeredRj = false;

    aas_areainfo_t currAreaInfo;
    AAS_AreaInfo(currAasAreaNum, &currAreaInfo);
    const int currAreaContents = currAreaInfo.contents;

    Vec3 moveVec(currMoveTargetPoint);
    moveVec -= self->s.origin;

    // Ladder movement
    if (self->is_ladder)
    {
        MoveOnLadder(&moveVec, ucmd);
    }
    else if (currAreaContents & AREACONTENTS_JUMPPAD)
    {
        MoveOnJumppad(&moveVec, ucmd);
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
        // starting a jump
        if (IsCloseToReachStart() && nextAreaReach->traveltype == TRAVEL_JUMP)
        {
            MoveStartingAJump(&moveVec, ucmd);
        }
        // starting a rocket jump
        else if (IsCloseToReachStart() && nextAreaReach->traveltype == TRAVEL_ROCKETJUMP)
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

    if (!hasPendingLookAtPoint)
    {
        float turnSpeedMultiplier = 1.0f;
        if (AimEnemy())
        {
            moveVec.NormalizeFast();
            Vec3 toEnemy(AimEnemy()->ent->s.origin);
            toEnemy -= self->s.origin;
            toEnemy.NormalizeFast();
            if (moveVec.Dot(toEnemy) < -0.3f)
            {
                ucmd->forwardmove *= -1;
                moveVec *= -1;
                turnSpeedMultiplier += Skill();
            }
        }
        ChangeAngle(moveVec, turnSpeedMultiplier);
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

void Bot::MoveOnLadder(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;
}

void Bot::MoveOnJumppad(Vec3 *moveVec, usercmd_t *ucmd)
{
    *moveVec = Vec3(nextAreaReach->end) - self->s.origin;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 1;
    if (IsCloseToReachEnd())
    {
        // Try to jump off wall to leave jumppad area
        ucmd->buttons |= BUTTON_SPECIAL;
    }
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

void Bot::MoveStartingAJump(Vec3 *moveVec, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (self->groundentity)
    {
        trace_t trace;
        vec3_t v1, v2;

        //check floor in front, if there's none... Jump!
        VectorCopy(self->s.origin, v1);
        VectorNormalize2(moveVec->data(), v2);
        VectorMA(v1, 18, v2, v1);
        v1[2] += self->r.mins[2];
        VectorCopy(v1, v2);
        v2[2] -= AI_JUMPABLE_HEIGHT;
        G_Trace(&trace, v1, vec3_origin, vec3_origin, v2, self, MASK_AISOLID);
        if (!trace.startsolid && trace.fraction == 1.0)
        {
            //jump!

            // prevent double jumping on crates
            VectorCopy(self->s.origin, v1);
            v1[2] += self->r.mins[2];
            G_Trace(&trace, v1, tv(-12, -12, -8), tv(12, 12, 0), v1, self, MASK_AISOLID);
            if (trace.startsolid)
                ucmd->upmove = 1;
        }
    }
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

void Bot::CheckAndTryAvoidObstacles(Vec3 *moveVec, float speed)
{
    float moveVecSqLen = moveVec->SquaredLength();
    if (moveVecSqLen < 0.01f)
        return;

    *moveVec *= Q_RSqrt(moveVecSqLen);

    Vec3 baseOffsetVec(*moveVec);
    baseOffsetVec *= 24.0f + 96.0f * BoundedFraction(speed, 900);

    float *const mins = playerbox_stand_mins;
    float *const maxs = playerbox_stand_maxs;

    trace_t trace;
    G_Trace(&trace, self->s.origin, mins, maxs, (baseOffsetVec + self->s.origin).data(), self, MASK_AISOLID);

    if (trace.fraction == 1.0f)
        return;

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

bool Bot::CheckAndTryStartNextReachTransition(Vec3 *moveVec, float speed)
{
    const float transitionRadius = 36.0f + 128.0f * BoundedFraction(speed - 320, 640);

    if (distanceToNextReachStart > transitionRadius)
        return false;

    int nextInChainReachNum = currAasAreaNum;
    aas_reachability_t nextInChainReach;
    AAS_ReachabilityFromNum(nextInChainReachNum, &nextInChainReach);

    constexpr int MAX_LOOKAHEAD = 8;
    vec3_t weightedDirsToReachStart[MAX_LOOKAHEAD];

    // If true, next reach. is outside of a transition radius
    bool hasOnlySingleFarReach = false;
    vec3_t singleFarNextReachDir;

    int nearestReachesCount = 0;
    for (;;)
    {
        if (nextInChainReach.areanum == goalAasAreaNum)
            break;

        nextInChainReachNum = AAS_AreaReachabilityToGoalArea(nextInChainReach.areanum, nextInChainReach.end, goalAasAreaNum, preferredAasTravelFlags);
        if (!nextInChainReachNum)
            break;

        AAS_ReachabilityFromNum(nextInChainReachNum, &nextInChainReach);
        if (nextInChainReach.traveltype != TRAVEL_WALK && nextInChainReach.traveltype != TRAVEL_WALKOFFLEDGE &&
            nextInChainReach.traveltype != TRAVEL_JUMP && nextInChainReach.traveltype != TRAVEL_STRAFEJUMP)
            break;

        float squareDist = DistanceSquared(nextInChainReach.start, self->s.origin);
        if (squareDist > transitionRadius * transitionRadius)
        {
            // If we haven't found next reach. yet
            if (nearestReachesCount == 0)
            {
                VectorCopy(nextInChainReach.start, singleFarNextReachDir);
                VectorSubtract(singleFarNextReachDir, self->s.origin, singleFarNextReachDir);
                VectorNormalizeFast(singleFarNextReachDir);
                hasOnlySingleFarReach = true;
            }
            break;
        }

        float *dir = weightedDirsToReachStart[nearestReachesCount];
        // Copy vector from self origin to reach. start
        VectorCopy(nextInChainReach.start, dir);
        VectorSubtract(dir, self->s.origin, dir);
        // Compute the vector length
        float invDistance = Q_RSqrt(squareDist);
        // Normalize the vector
        VectorScale(dir, invDistance, dir);
        // Scale by distance factor
        VectorScale(dir, 1.0f - (1.0f / invDistance) / transitionRadius, dir);

        nearestReachesCount++;
        if (nearestReachesCount == MAX_LOOKAHEAD)
            break;
    }

    if (nearestReachesCount && moveVec->SquaredLength() > 0.01f)
    {
        moveVec->NormalizeFast();
        if (hasOnlySingleFarReach)
        {
            float factor = distanceToNextReachStart / transitionRadius; // 0..1
            *moveVec *= factor;
            VectorScale(singleFarNextReachDir, 1.0f - factor, singleFarNextReachDir);
            *moveVec += singleFarNextReachDir;
        }
        else
        {
            *moveVec *= distanceToNextReachStart / transitionRadius;
            for (int i = 0; i < nearestReachesCount; ++i)
                *moveVec += weightedDirsToReachStart[i];
        }
        // moveVec is not required to be normalized, leave it as is
        return true;
    }
    return false;
}

void Bot::MoveGenericRunning(Vec3 *moveVec, usercmd_t *ucmd)
{
    // moveDir is initially set to a vector from self to currMoveTargetPoint.
    // However, if we have any tangential velocity in a trajectory and it is not directed to the target.
    // we are likely going to miss the target point. We have to check actual look angles
    // and assist by turning to the target harder and/or assisting by strafe keys (when picking an item)

    Vec3 velocityVec(self->velocity);
    float speed = velocityVec.SquaredLength() > 0.01f ? velocityVec.LengthFast() : 0;

    bool inTransition = CheckAndTryStartNextReachTransition(moveVec, speed);

    CheckAndTryAvoidObstacles(moveVec, speed);

    Vec3 toTargetDir2D(*moveVec);
    toTargetDir2D.z() = 0;

    Vec3 lookDir2D(0, 0, 0);
    AngleVectors(self->s.angles, lookDir2D.data(), nullptr, nullptr);
    lookDir2D.z() = 0;

    float lookDir2DSqLen = lookDir2D.SquaredLength();
    float toTarget2DSqLen = toTargetDir2D.SquaredLength();

    if (lookDir2DSqLen > 0.1f)
    {
        lookDir2D *= Q_RSqrt(lookDir2DSqLen);

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
            ucmd->upmove = 1;
            isBunnyHopping = true;
        }

        if (toTarget2DSqLen > 0.1f && !inTransition)
        {
            toTargetDir2D *= Q_RSqrt(toTarget2DSqLen);

            float lookToTarget2DDot = lookDir2D.Dot(toTargetDir2D);
            if (lookToTarget2DDot > 0.99)
            {
                // TODO: Enable strafejumping
                AITools_DrawColorLine(self->s.origin, (64 * toTargetDir2D + self->s.origin).data(), COLOR_RGB(0, 255, 0), 0);
            }
            else
            {
                // Given an actual move dir line (a line that goes through selfOrigin to selfOrigin + lookDir2D),
                // determine on which side the move target (defined by moveVec) is
                float lineNormalX = +lookDir2D.y();
                float lineNormalY = -lookDir2D.x();
                int side = Q_sign(lineNormalX * moveVec->x() + lineNormalY * moveVec->y());

                if (lookToTarget2DDot > 0.7)
                {
                    // currAngle is an angle between actual lookDir and moveDir
                    // moveDir is a requested look direction
                    // due to view angles change latency requested angles may be not set immediately in this frame
                    // We have to request an angle that is greater than it is needed
                    float currAngle = RAD2DEG(acosf(lookToTarget2DDot)) * side;
                    mat3_t matrix;
                    AnglesToAxis(Vec3(0, currAngle * 1.1f, 0).data(), matrix);
                    Matrix3_TransformVector(matrix, moveVec->data(), moveVec->data());
                }
                else
                {
                    // Release +forward and use aircontrol to turn to the movetarget
                    ucmd->forwardmove = 0;
                    ucmd->sidemove = side;
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

    // TODO: This is a condition for long term goal located in `goalAasAreaNum`, short term goals are not covered
    if (!self->movetarget || currAasAreaNum != goalAasAreaNum)
        return;

    if (!self->goalentity)
        FailWith("Movetarget is present, but goalentity is absent\n");

    // TODO: Implement goal timeout (including short-range one) in common AI code, not there

    // Check whether we have reached the target
    if (goalAasAreaNodeFlags & (NODEFLAGS_ENTITYREACH | NODEFLAGS_REACHATTOUCH))
    {
        if (BoundsIntersect(self->goalentity->r.absmin, self->goalentity->r.absmax, self->r.absmin, self->r.absmax))
        {
            Ai::ReachedEntity();
            return;
        }
    }
    else
    {
        if ((goalTargetPoint - self->s.origin).SquaredLength() < 64 * 64)
        {
            Ai::ReachedEntity();
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
void Bot::CombatMovement(usercmd_t *ucmd)
{
    // TODO: Check whether we are holding/camping a point

    const CombatTask &combatTask = enemyPool.combatTask;

    if ((!combatTask.aimEnemy && !combatTask.spamEnemy))
    {
        Move(ucmd);
        return;
    }

    bool hasToEvade = false;
    if (Skill() >= 0.25f)
        hasToEvade = dangersDetector.FindDangers();

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

    if(!hasToEvade && aimTarget.inhibit)
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

    const CombatTask &combatTask = enemyPool.combatTask;

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
            if (moveTestResult.forwardGroundTrace.fraction == 1.0 && advance)
            {
                // It is finite and not very large, since CanWalkOrFallQuiteSafely() returned true
                float fallHeight = self->s.origin[2] - moveTestResult.forwardGroundTrace.endpos[2];
                // Allow to fall while attacking when enemy is still on bots height
                if (self->s.origin[2] - fallHeight + 16 > enemyPool.combatTask.TargetOrigin().z())
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

