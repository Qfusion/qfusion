#include "bot.h"
#include "ai_local.h"
#include "../../gameshared/q_collision.h"

void Bot::SpecialMove(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd)
{
    bool wallJump = false;
#if 0
    bool dash = true;
#endif
    bool bunnyhop = true;
    trace_t trace;
    vec3_t end;
    int n1, n2, nextMoveType;

    self->ai->is_bunnyhop = false;

    if( self->ai->path.numNodes < MIN_BUNNY_NODES )
        return;

    // verify that the 2nd node is in front of us for dashing
    n1 = self->ai->path.nodes[self->ai->path.numNodes];
    n2 = self->ai->path.nodes[self->ai->path.numNodes-1];

    // do not dash if the next link will be a fall, jump or
    // any other kind of special link
    nextMoveType = AI_PlinkMoveType( n1, n2 );
#if 0
    if( nextMoveType & (LINK_LADDER|LINK_PLATFORM|LINK_ROCKETJUMP|LINK_FALL|LINK_JUMP|LINK_CROUCH) )
		dash = false;
#endif

    if( nextMoveType &(LINK_LADDER|LINK_PLATFORM|LINK_FALL|LINK_CROUCH) )
        bunnyhop = false;
#if 0
    if( VectorLengthFast( self->velocity ) < AI_JUMP_SPEED )
	{
		if( dash && self->groundentity ) // attempt dash
		{
			if( DotProduct( lookdir, pathdir ) > 0.9 )
			{
				// do not dash unless both next nodes are visible
				if( AI_ReachabilityVisible( self, nodes[n1].origin ) &&
					AI_ReachabilityVisible( self, nodes[n2].origin ) )
				{
					ucmd->buttons |= BUTTON_SPECIAL;
					ucmd->sidemove = 0;
					ucmd->forwardmove = 1;
					self->ai->is_bunnyhop = true;
				}
			}
		}
	}
	else
#endif
    if( bunnyhop && ( (nextMoveType & (LINK_JUMP|LINK_MOVE)) || level.gametype.spawnableItemsMask == 0 ) )
    {
        // Can't accelerate anymore (we have to add some delta to the default dash speed)
        if( VectorLengthFast(self->velocity) >= DEFAULT_DASHSPEED - 16 )
        {
            vec3_t n1origin, n2origin;
            GetNodeOrigin(n1, n1origin);
            GetNodeOrigin(n2, n2origin);
            Vec3 linkVec(n2origin);
            linkVec -= n1origin;
            linkVec.z() *= 0.25f;

            VectorCopy(linkVec.data(), self->ai->move_vector);

            ucmd->forwardmove = 1;
            ucmd->upmove = 1;
        }
        // Get an initial speed by dash
        else
        {
            ucmd->buttons |= BUTTON_SPECIAL;
        }

#if 0
        // fake strafe-jumping acceleration
		if( VectorLengthFast( self->velocity ) < 700 && DotProduct( lookdir, pathdir ) > 0.6 )
			VectorMA( self->velocity, 0.1f, lookdir, self->velocity );
#endif
        self->ai->is_bunnyhop = true;
    }

    if( wallJump )
    {
        if( self->ai->move_vector[2] > 25 && DotProduct( self->velocity, pathdir ) < -0.2 )
        {
            VectorMA( self->s.origin, 0.02, self->velocity, end );
            G_Trace( &trace, self->s.origin, self->r.mins, self->r.maxs, end, self, MASK_AISOLID );

            if( trace.fraction != 1.0f )
                ucmd->buttons |= BUTTON_SPECIAL;
        }
    }

    // if pushing in the opposite direction of the path, reduce the push
    if( !self->ai->is_bunnyhop && DotProduct( lookdir, pathdir ) < -0.33f )
        ucmd->forwardmove = 0;
}

void Bot::Move(usercmd_t *ucmd)
{
    bool nodeReached = false;
    bool specialMovement = false;
    vec3_t lookdir, pathdir;

    if (self->ai->next_node == NODE_INVALID || self->ai->goal_node == NODE_INVALID)
    {
        MoveWander(ucmd);
        return;
    }

    const unsigned linkType = CurrentLinkType();

    specialMovement = (self->ai->path.numNodes >= MIN_BUNNY_NODES) ? true : false;

    if (GetNodeFlags(self->ai->next_node) & (NODEFLAGS_REACHATTOUCH | NODEFLAGS_ENTITYREACH))
        specialMovement = false;

    if (linkType & (LINK_JUMP | LINK_JUMPPAD | LINK_CROUCH | LINK_FALL | LINK_WATER | LINK_LADDER | LINK_ROCKETJUMP))
        specialMovement = false;

    if (self->ai->pers.skillLevel < 0.33f)
        specialMovement = false;

    if (specialMovement == false || self->groundentity)
        self->ai->is_bunnyhop = false;

    VectorSubtract(nodes[self->ai->next_node].origin, self->s.origin, self->ai->move_vector);

    // 2D, normalized versions of look and path directions
    pathdir[0] = self->ai->move_vector[0];
    pathdir[1] = self->ai->move_vector[1];
    pathdir[2] = 0.0f;
    VectorNormalize(pathdir);

    AngleVectors(self->s.angles, lookdir, NULL, NULL);
    lookdir[2] = 0.0f;
    VectorNormalize(lookdir);

    // Ladder movement
    if (self->is_ladder)
    {
        nodeReached = MoveOnLadder(lookdir, pathdir, ucmd);
    }
    else if (linkType & LINK_JUMPPAD)
    {
        nodeReached = MoveOnJumppad(lookdir, pathdir, ucmd);
    }
    // Platform riding - No move, riding elevator
    else if (linkType & LINK_PLATFORM)
    {
        nodeReached = MoveRidingPlatform(lookdir, pathdir, ucmd);
    }
    // Entering platform
    else if (GetNodeFlags(self->ai->next_node) & NODEFLAGS_PLATFORM)
    {
        nodeReached = MoveEnteringPlatform(lookdir, pathdir, ucmd);
    }
    // Falling off ledge or jumping
    else if (!self->groundentity && !self->is_step && !self->is_swim && !self->ai->is_bunnyhop)
    {
        nodeReached = MoveFallingOrJumping(lookdir, pathdir, ucmd);
    }
    else // standard movement
    {
        // starting a jump
        if ((linkType & LINK_JUMP))
        {
            nodeReached = MoveStartingAJump(lookdir, pathdir, ucmd);
        }
            // starting a rocket jump
        else if ((linkType & LINK_ROCKETJUMP))
        {
            nodeReached = MoveStartingARocketjump(lookdir, pathdir, ucmd);
        }
        else
        {
            nodeReached = MoveLikeHavingShortGoal(lookdir, pathdir, ucmd, specialMovement);
        }
        TryMoveAwayIfBlocked(ucmd);
    }

    // swimming
    if (self->is_swim)
    {
        if (!(G_PointContents(nodes[self->ai->next_node].origin) & MASK_WATER))  // Exit water
            ucmd->upmove = 1;
    }

    ChangeAngle();

    if (nodeReached)
        NodeReached();
}

void Bot::TryMoveAwayIfBlocked(usercmd_t *ucmd)
{
    // if static assume blocked and try to get free
    if (VectorLengthFast(self->velocity) < 37 && (ucmd->forwardmove || ucmd->sidemove || ucmd->upmove))
    {
        if (random() > 0.1 && self->ai->aiRef->SpecialMove(ucmd))  // jumps, crouches, turns...
            return;

        self->s.angles[YAW] += brandom(-90, 90);
    }
}

static constexpr float BOT_FORWARD_EPSILON = 0.5f;

bool Bot::MoveOnLadder(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    ucmd->forwardmove = 0;
    ucmd->upmove = 1;
    ucmd->sidemove = 0;

    if (nav.debugMode && printLink)
        G_PrintChasersf(self, "LINK_LADDER\n");

    return NodeReachedGeneric();
}

bool Bot::MoveOnJumppad(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    vec3_t v1, v2;
    VectorCopy(self->s.origin, v1);
    VectorCopy(nodes[self->ai->next_node].origin, v2);
    v1[2] = v2[2] = 0;
    if (DistanceFast(v1, v2) > 32 && DotProduct(lookdir, pathdir) > BOT_FORWARD_EPSILON)
    {
        ucmd->forwardmove = 1; // push towards destination
        ucmd->buttons |= BUTTON_WALK;
    }
    return self->groundentity != NULL && NodeReachedGeneric();
}

bool Bot::MoveRidingPlatform(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    vec3_t v1, v2;
    VectorCopy(self->s.origin, v1);
    VectorCopy(nodes[self->ai->next_node].origin, v2);
    v1[2] = v2[2] = 0;
    if (DistanceFast(v1, v2) > 32 && DotProduct(lookdir, pathdir) > BOT_FORWARD_EPSILON)
        ucmd->forwardmove = 1; // walk to center

    ucmd->buttons |= BUTTON_WALK;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (nav.debugMode && printLink)
        G_PrintChasersf(self, "LINK_PLATFORM (riding)\n");

    self->ai->move_vector[2] = 0; // put view horizontal

    return NodeReachedPlatformEnd();
}

bool Bot::MoveEnteringPlatform(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (DotProduct(lookdir, pathdir) <= BOT_FORWARD_EPSILON)
        ucmd->buttons |= BUTTON_WALK;

    if (nav.debugMode && printLink)
        G_PrintChasersf(self, "NODEFLAGS_PLATFORM (moving to plat)\n");

    // is lift down?
    for (int i = 0; i < nav.num_navigableEnts; i++)
    {
        if (nav.navigableEnts[i].node == self->ai->next_node)
        {
            //testing line
            //vec3_t	tPoint;
            //int		j;
            //for(j=0; j<3; j++)//center of the ent
            //	tPoint[j] = nav.ents[i].ent->s.origin[j] + 0.5*(nav.ents[i].ent->r.mins[j] + nav.ents[i].ent->r.maxs[j]);
            //tPoint[2] = nav.ents[i].ent->s.origin[2] + nav.ents[i].ent->r.maxs[2];
            //tPoint[2] += 8;
            //AITools_DrawLine( self->s.origin, tPoint );

            //if not reachable, wait for it (only height matters)
            if ((nav.navigableEnts[i].ent->s.origin[2] + nav.navigableEnts[i].ent->r.maxs[2]) >
                (self->s.origin[2] + self->r.mins[2] + AI_JUMPABLE_HEIGHT) &&
                nav.navigableEnts[i].ent->moveinfo.state != STATE_BOTTOM)
            {
                self->ai->blocked_timeout = level.time + 10000;
                ucmd->forwardmove = 0;
            }
        }
    }

    return NodeReachedPlatformStart();
}

bool Bot::MoveFallingOrJumping(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    ucmd->upmove = 0;
    ucmd->sidemove = 0;
    ucmd->forwardmove = 0;

    const float lookDot = DotProduct(lookdir, pathdir);

    if (lookDot > BOT_FORWARD_EPSILON)
    {
        ucmd->forwardmove = 1;

        const unsigned linkType = CurrentLinkType();

        // add fake strafe accel
        if (!(linkType & LINK_FALL) || linkType & (LINK_JUMP | LINK_ROCKETJUMP))
        {
            if (linkType & LINK_JUMP)
            {
                if (AttemptWalljump())
                {
                    ucmd->buttons |= BUTTON_SPECIAL;
                }
                if (VectorLengthFast(tv(self->velocity[0], self->velocity[1], 0)) < 600)
                    VectorMA(self->velocity, 6.0f, lookdir, self->velocity);
            }
            else
            {
                if (VectorLengthFast(tv(self->velocity[0], self->velocity[1], 0)) < 450)
                    VectorMA(self->velocity, 1.0f, lookdir, self->velocity);
            }
        }
    }
    else if (lookDot < -BOT_FORWARD_EPSILON)
        ucmd->forwardmove = -1;

    if (nav.debugMode && printLink)
        G_PrintChasersf(self, "FLY MOVE\n");

    return NodeReachedGeneric();
}

bool Bot::MoveStartingAJump(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (self->groundentity)
    {
        trace_t trace;
        vec3_t v1, v2;

        if (nav.debugMode && printLink)
            G_PrintChasersf(self, "LINK_JUMP\n");

        //check floor in front, if there's none... Jump!
        VectorCopy(self->s.origin, v1);
        VectorNormalize2(self->ai->move_vector, v2);
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

    return NodeReachedGeneric();
}

bool Bot::MoveStartingARocketjump(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    if (nav.debugMode && printLink)
        G_PrintChasersf(self, "LINK_ROCKETJUMP\n");

    if (!self->ai->rj_triggered && self->groundentity && (self->s.weapon == WEAP_ROCKETLAUNCHER))
    {
        self->s.angles[PITCH] = 170;
        ucmd->upmove = 1;
        ucmd->buttons |= BUTTON_ATTACK;
        self->ai->rj_triggered = true;
    }

    return NodeReachedGeneric();
}

bool Bot::MoveLikeHavingShortGoal(const vec_t *lookdir, const vec_t *pathdir, usercmd_t *ucmd, bool specialMovement)
{
    ucmd->forwardmove = 1;
    ucmd->upmove = 0;
    ucmd->sidemove = 0;

    // Move To Short Range goal (not following paths)
    // plats, grapple, etc have higher priority than SR Goals, cause the bot will
    // drop from them and have to repeat the process from the beginning
    if (MoveToShortRangeGoalEntity(ucmd))
    {
        return NodeReachedGeneric();
    }
    else if (specialMovement && !self->is_swim) // bunny-hopping movement here
    {
        SpecialMove(lookdir, pathdir, ucmd);
        return NodeReachedSpecial();
    }
    else
    {
        return NodeReachedGeneric();
    }
}

void Bot::MoveWander(usercmd_t *ucmd)
{
    vec3_t temp;

    if( self->deadflag )
        return;
    if( !self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED] ) {
        return;
    }

    // Special check for elevators, stand still until the ride comes to a complete stop.
    if( self->groundentity && self->groundentity->use == Use_Plat )
    {
        if( self->groundentity->moveinfo.state != STATE_UP &&
            self->groundentity->moveinfo.state != STATE_DOWN )
        {
            self->velocity[0] = 0;
            self->velocity[1] = 0;
            self->velocity[2] = 0;
            return;
        }
    }

    // Move To Goal (Short Range Goal, not following paths)
    if( !MoveToShortRangeGoalEntity( ucmd ) )
    {
        // Swimming?
        VectorCopy( self->s.origin, temp );
        temp[2] += 24;

        if( G_PointContents( temp ) & MASK_WATER )
        {
            // If drowning and no node, move up
            if( self->r.client && self->r.client->resp.next_drown_time > 0 )
            {
                ucmd->upmove = 1;
                self->s.angles[PITCH] = -45;
            }
            else
                ucmd->upmove = 1;

            ucmd->forwardmove = 1;
        }
        // else self->r.client->next_drown_time = 0; // probably shound not be messing with this, but


        // Lava?
        temp[2] -= 48;
        if( G_PointContents( temp ) & ( CONTENTS_LAVA|CONTENTS_SLIME ) )
        {
            self->s.angles[YAW] += random() * 360 - 180;
            ucmd->forwardmove = 1;
            if( self->groundentity )
                ucmd->upmove = 1;
            else
                ucmd->upmove = 0;
            return;
        }


        // Check for special movement
        if( VectorLengthFast( self->velocity ) < 37 )
        {
            if( random() > 0.1 && SpecialMove( ucmd ) )  //jumps, crouches, turns...
                return;

            self->s.angles[YAW] += random() * 180 - 90;

            if( !self->is_step )  // if there is ground continue otherwise wait for next move
                ucmd->forwardmove = 0; //0
            else
            {
                if (closeAreaProps.frontTest.CanWalk())
                {
                    ucmd->forwardmove = 1;
                    ucmd->buttons |= BUTTON_WALK;
                }
            }

            return;
        }

        // Otherwise move slowly, walking wondering what's going on
        ucmd->buttons |= BUTTON_WALK;
    }

    if( closeAreaProps.frontTest.CanWalk() )
        ucmd->forwardmove = 1;
    else
        ucmd->forwardmove = -1;
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

    const CombatTask &combatTask = enemyPool.combatTask;

    if ((!combatTask.aimEnemy && !combatTask.spamEnemy) || self->ai->rush_item)
    {
        Move(ucmd);
        return;
    }

    bool hasToEvade = false;
    if (Skill() >= 0.25f)
        hasToEvade = dangersDetector.FindDangers();

    const float dist = (combatTask.TargetOrigin() - self->s.origin).LengthFast();
    const float c = random();

    if( level.time > self->ai->combatmovepush_timeout )
    {
        self->ai->combatmovepush_timeout = level.time + AI_COMBATMOVE_TIMEOUT;

        VectorClear( self->ai->combatmovepushes );

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
                    self->ai->combatmovepushes[0] = 1;
                else if (c <= 0.5 && closeAreaProps.leftTest.CanWalk())
                    self->ai->combatmovepushes[1] = -1;
                else if (c <= 0.5 && closeAreaProps.rightTest.CanWalk())
                    self->ai->combatmovepushes[1] = 1;
            }
            else
            {
                // First, establish mapping from CombatTask tactical directions (if any) to bot movement key directions
                int tacticalXMove, tacticalYMove;
                bool advance = TacticsToAprioriMovePushes(&tacticalXMove, &tacticalYMove);

                const auto &placeProps = closeAreaProps;  // Shorthand
                auto moveXAndUp = ApplyTacticalMove(tacticalXMove, advance, placeProps.frontTest, placeProps.backTest);
                auto moveYAndUp = ApplyTacticalMove(tacticalYMove, advance, placeProps.rightTest, placeProps.leftTest);

                self->ai->combatmovepushes[0] = moveXAndUp.first;
                self->ai->combatmovepushes[1] = moveYAndUp.first;
                self->ai->combatmovepushes[2] = moveXAndUp.second || moveYAndUp.second;
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

    if( !self->ai->camp_item )
    {
        ucmd->forwardmove = self->ai->combatmovepushes[0];
    }
    ucmd->sidemove = self->ai->combatmovepushes[1];
    ucmd->upmove = self->ai->combatmovepushes[2];
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
        VectorCopy(walkingMovePushes, self->ai->combatmovepushes);
        if (Skill() > 0.85f || (random() < (Skill() - 0.25f)))
        {
            ucmd->buttons |= BUTTON_SPECIAL;
        }
    }
    else if (jumpingEvades > 0)
    {
        jumpingMovePushes[2] = 1;
        VectorCopy(jumpingMovePushes, self->ai->combatmovepushes);
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
    float c = random();
    if (tacticalMove && c < 0.9)
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
        if (c < 0.45)
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

