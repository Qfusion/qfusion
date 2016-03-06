#include "bot.h"

void Bot::SpecialMove(vec3_t lookdir, vec3_t pathdir, usercmd_t *ucmd)
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

    if( !Ai::IsInFront2D( lookdir, self->s.origin, nodes[n2].origin, 0.5 ) )
        bunnyhop = false;

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
    if( bunnyhop && ( (nextMoveType &LINK_JUMP) || level.gametype.spawnableItemsMask == 0 ) )
    {
        if( self->groundentity )
            ucmd->upmove = 1;

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
    if( DotProduct( lookdir, pathdir ) < -0.33f )
        ucmd->forwardmove = 0;
}

void Bot::Move(usercmd_t *ucmd)
{
#define BOT_FORWARD_EPSILON 0.5f
    int i;
    unsigned int linkType;
    bool printLink = false;
    bool nodeReached = false;
    bool specialMovement = false;
    vec3_t v1, v2;
    vec3_t lookdir, pathdir;
    float lookDot;

    if( self->ai->next_node == NODE_INVALID || self->ai->goal_node == NODE_INVALID )
    {
        MoveWander( ucmd );
        return;
    }

    linkType = CurrentLinkType();

    specialMovement = ( self->ai->path.numNodes >= MIN_BUNNY_NODES ) ? true : false;

    if( GetNodeFlags( self->ai->next_node ) & (NODEFLAGS_REACHATTOUCH|NODEFLAGS_ENTITYREACH) )
        specialMovement = false;

    if( linkType & (LINK_JUMP|LINK_JUMPPAD|LINK_CROUCH|LINK_FALL|LINK_WATER|LINK_LADDER|LINK_ROCKETJUMP) )
        specialMovement = false;

    if( self->ai->pers.skillLevel < 0.33f )
        specialMovement = false;

    if( specialMovement == false || self->groundentity )
        self->ai->is_bunnyhop = false;

    VectorSubtract( nodes[self->ai->next_node].origin, self->s.origin, self->ai->move_vector );

    // 2D, normalized versions of look and path directions
    pathdir[0] = self->ai->move_vector[0];
    pathdir[1] = self->ai->move_vector[1];
    pathdir[2] = 0.0f;
    VectorNormalize( pathdir );

    AngleVectors( self->s.angles, lookdir, NULL, NULL );
    lookdir[2] = 0.0f;
    VectorNormalize( lookdir );

    lookDot = DotProduct( lookdir, pathdir );

    // Ladder movement
    if( self->is_ladder )
    {
        ucmd->forwardmove = 0;
        ucmd->upmove = 1;
        ucmd->sidemove = 0;

        if( nav.debugMode && printLink )
            G_PrintChasersf( self, "LINK_LADDER\n" );

        nodeReached = NodeReachedGeneric();
    }
    else if( linkType & LINK_JUMPPAD )
    {
        VectorCopy( self->s.origin, v1 );
        VectorCopy( nodes[self->ai->next_node].origin, v2 );
        v1[2] = v2[2] = 0;
        if( DistanceFast( v1, v2 ) > 32 && lookDot > BOT_FORWARD_EPSILON ) {
            ucmd->forwardmove = 1; // push towards destination
            ucmd->buttons |= BUTTON_WALK;
        }
        nodeReached = self->groundentity != NULL && NodeReachedGeneric();
    }
        // Platform riding - No move, riding elevator
    else if( linkType & LINK_PLATFORM )
    {
        VectorCopy( self->s.origin, v1 );
        VectorCopy( nodes[self->ai->next_node].origin, v2 );
        v1[2] = v2[2] = 0;
        if( DistanceFast( v1, v2 ) > 32 && lookDot > BOT_FORWARD_EPSILON )
            ucmd->forwardmove = 1; // walk to center

        ucmd->buttons |= BUTTON_WALK;
        ucmd->upmove = 0;
        ucmd->sidemove = 0;

        if( nav.debugMode && printLink )
            G_PrintChasersf( self, "LINK_PLATFORM (riding)\n" );

        self->ai->move_vector[2] = 0; // put view horizontal

        nodeReached = NodeReachedPlatformEnd();
    }
        // entering platform
    else if( GetNodeFlags( self->ai->next_node ) & NODEFLAGS_PLATFORM )
    {
        ucmd->forwardmove = 1;
        ucmd->upmove = 0;
        ucmd->sidemove = 0;

        if( lookDot <= BOT_FORWARD_EPSILON )
            ucmd->buttons |= BUTTON_WALK;

        if( nav.debugMode && printLink )
            G_PrintChasersf( self, "NODEFLAGS_PLATFORM (moving to plat)\n" );

        // is lift down?
        for( i = 0; i < nav.num_navigableEnts; i++ )
        {
            if( nav.navigableEnts[i].node == self->ai->next_node )
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
                if( ( nav.navigableEnts[i].ent->s.origin[2] + nav.navigableEnts[i].ent->r.maxs[2] ) > ( self->s.origin[2] + self->r.mins[2] + AI_JUMPABLE_HEIGHT ) &&
                    nav.navigableEnts[i].ent->moveinfo.state != STATE_BOTTOM )
                {
                    self->ai->blocked_timeout = level.time + 10000;
                    ucmd->forwardmove = 0;
                }
            }
        }

        nodeReached = NodeReachedPlatformStart();
    }
        // Falling off ledge or jumping
    else if( !self->groundentity && !self->is_step && !self->is_swim && !self->ai->is_bunnyhop )
    {
        ucmd->upmove = 0;
        ucmd->sidemove = 0;
        ucmd->forwardmove = 0;

        if( lookDot > BOT_FORWARD_EPSILON )
        {
            ucmd->forwardmove = 1;

            // add fake strafe accel
            if( !(linkType & LINK_FALL) || linkType & (LINK_JUMP|LINK_ROCKETJUMP) )
            {
                if( linkType & LINK_JUMP )
                {
                    if( AttemptWalljump() ) {
                        ucmd->buttons |= BUTTON_SPECIAL;
                    }
                    if( VectorLengthFast( tv( self->velocity[0], self->velocity[1], 0 ) ) < 600 )
                        VectorMA( self->velocity, 6.0f, lookdir, self->velocity );
                }
                else
                {
                    if( VectorLengthFast( tv( self->velocity[0], self->velocity[1], 0 ) ) < 450 )
                        VectorMA( self->velocity, 1.0f, lookdir, self->velocity );
                }
            }
        }
        else if( lookDot < -BOT_FORWARD_EPSILON )
            ucmd->forwardmove = -1;

        if( nav.debugMode && printLink )
            G_PrintChasersf( self, "FLY MOVE\n" );

        nodeReached = NodeReachedGeneric();
    }
    else // standard movement
    {
        ucmd->forwardmove = 1;
        ucmd->upmove = 0;
        ucmd->sidemove = 0;

        // starting a jump
        if( ( linkType & LINK_JUMP ) )
        {
            if( self->groundentity )
            {
                trace_t trace;
                vec3_t v1, v2;

                if( nav.debugMode && printLink )
                    G_PrintChasersf( self, "LINK_JUMP\n" );

                //check floor in front, if there's none... Jump!
                VectorCopy( self->s.origin, v1 );
                VectorNormalize2( self->ai->move_vector, v2 );
                VectorMA( v1, 18, v2, v1 );
                v1[2] += self->r.mins[2];
                VectorCopy( v1, v2 );
                v2[2] -= AI_JUMPABLE_HEIGHT;
                G_Trace( &trace, v1, vec3_origin, vec3_origin, v2, self, MASK_AISOLID );
                if( !trace.startsolid && trace.fraction == 1.0 )
                {
                    //jump!

                    // prevent double jumping on crates
                    VectorCopy( self->s.origin, v1 );
                    v1[2] += self->r.mins[2];
                    G_Trace( &trace, v1, tv( -12, -12, -8 ), tv( 12, 12, 0 ), v1, self, MASK_AISOLID );
                    if( trace.startsolid )
                        ucmd->upmove = 1;
                }
            }

            nodeReached = NodeReachedGeneric();
        }
            // starting a rocket jump
        else if( ( linkType & LINK_ROCKETJUMP ) )
        {
            if( nav.debugMode && printLink )
                G_PrintChasersf( self, "LINK_ROCKETJUMP\n" );

            if( !self->ai->rj_triggered && self->groundentity && ( self->s.weapon == WEAP_ROCKETLAUNCHER ) )
            {
                self->s.angles[PITCH] = 170;
                ucmd->upmove = 1;
                ucmd->buttons |= BUTTON_ATTACK;
                self->ai->rj_triggered = true;
            }

            nodeReached = NodeReachedGeneric();
        }
        else
        {
            // Move To Short Range goal (not following paths)
            // plats, grapple, etc have higher priority than SR Goals, cause the bot will
            // drop from them and have to repeat the process from the beginning
            if( MoveToShortRangeGoalEntity( ucmd ) )
            {
                nodeReached = NodeReachedGeneric();
            }
            else if( specialMovement && !self->is_swim ) // bunny-hopping movement here
            {
                SpecialMove( lookdir, pathdir, ucmd );
                nodeReached = NodeReachedSpecial();
            }
            else
            {
                nodeReached = NodeReachedGeneric();
            }
        }

        // if static assume blocked and try to get free
        if( VectorLengthFast( self->velocity ) < 37 && ( ucmd->forwardmove || ucmd->sidemove || ucmd->upmove ) )
        {
            if( random() > 0.1 && self->ai->aiRef->SpecialMove( ucmd ) )  // jumps, crouches, turns...
                return;

            self->s.angles[YAW] += brandom( -90, 90 );
        }
    }

    // swimming
    if( self->is_swim )
    {
        if( !( G_PointContents( nodes[self->ai->next_node].origin ) & MASK_WATER ) )  // Exit water
            ucmd->upmove = 1;
    }

    ChangeAngle();

    if( nodeReached )
        NodeReached();

#undef BOT_FORWARD_EPSILON
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
                MoveTestResult forwardTest;
                TestMove(&forwardTest, BOT_MOVE_FORWARD);
                if (forwardTest.CanWalk())
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

    MoveTestResult forwardTest;
    TestMove(&forwardTest, BOT_MOVE_FORWARD);
    if( forwardTest.CanWalk() )
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
    float c;
    float dist;
    bool hasToEvade = false;

    if( !self->enemy || self->ai->rush_item )
    {
        Move(ucmd);
        return;
    }

    if( self->ai->pers.skillLevel >= 0.25f )
        hasToEvade = dangersDetector.FindDangers();

    dist = DistanceFast( self->s.origin, self->enemy->s.origin );
    c = random();

    if( level.time > self->ai->combatmovepush_timeout )
    {
        MoveTestResult leftTest;
        MoveTestResult rightTest;
        MoveTestResult frontTest;
        MoveTestResult backTest;
        TestMove(&leftTest, BOT_MOVE_LEFT);
        TestMove(&rightTest, BOT_MOVE_RIGHT);
        TestMove(&frontTest, BOT_MOVE_FORWARD);
        TestMove(&backTest, BOT_MOVE_RIGHT);

        bool canMOVELEFT = leftTest.CanWalk();
        bool canMOVERIGHT = rightTest.CanWalk();
        bool canMOVEFRONT = frontTest.CanWalk();
        bool canMOVEBACK = backTest.CanWalk();

        self->ai->combatmovepush_timeout = level.time + AI_COMBATMOVE_TIMEOUT;
        VectorClear( self->ai->combatmovepushes );

        if (hasToEvade)
        {
            Vec3 evadeDir = MakeEvadeDirection(*dangersDetector.primaryDanger);
#ifdef _DEBUG
            Vec3 drawnDirStart(self->s.origin);
            drawnDirStart.z() += 32;
            Vec3 drawnDirEnd = drawnDirStart + 64.0f * evadeDir;
            AITools_DrawLine(drawnDirStart.vec, drawnDirEnd.vec);
#endif

            int walkingEvades = 0;
            int walkingMovePushes[2] = {0, 0};
            int jumpingEvades = 0;
            int jumpingMovePushes[2] = {0, 0};

            if (evadeDir.x())
            {
                if ((evadeDir.x() < 0))
                {
                    if (backTest.CanWalkOrFallQuiteSafely())
                    {
                        walkingMovePushes[0] = -1;
                        ++walkingEvades;
                    }
                    else if (backTest.CanJump())
                    {
                        jumpingMovePushes[0] = -1;
                        ++jumpingEvades;
                    }
                }
                else if ((evadeDir.x() > 0))
                {
                    if (frontTest.CanWalkOrFallQuiteSafely())
                    {
                        walkingMovePushes[0] = 1;
                        ++walkingEvades;
                    }
                    else if (frontTest.CanJump())
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
                    if (leftTest.CanWalkOrFallQuiteSafely())
                    {
                        walkingMovePushes[1] = -1;
                        ++walkingEvades;
                    }
                    else if (leftTest.CanJump())
                    {
                        jumpingMovePushes[1] = -1;
                        ++jumpingEvades;
                    }
                }
                else if ((evadeDir.y() > 0))
                {
                    if (rightTest.CanWalkOrFallQuiteSafely())
                    {
                        walkingMovePushes[1] = 1;
                        ++walkingEvades;
                    }
                    else if (rightTest.CanJump())
                    {
                        jumpingMovePushes[1] = 1;
                        ++jumpingEvades;
                    }
                }
            }

            // Evades with dash have priority unless the bot is stunned
            if (walkingEvades > 0 && !self->r.client->ps.stats[PM_STAT_STUN])
            {
                VectorCopy(walkingMovePushes, self->ai->combatmovepushes);
                ucmd->buttons |= BUTTON_SPECIAL;
            }
            else if (jumpingEvades > 0)
            {
                jumpingMovePushes[2] = 1;
                VectorCopy(jumpingMovePushes, self->ai->combatmovepushes);
            }
        }
        else
        if( dist < 150 ) // range = AIWEAP_MELEE_RANGE;
        {
            if( self->s.weapon == WEAP_GUNBLADE ) // go into him!
            {
                ucmd->buttons &= ~BUTTON_ATTACK; // remove pressing fire
                if( canMOVEFRONT )  // move to your enemy
                    self->ai->combatmovepushes[0] = 1;
                else if( c <= 0.5 && canMOVELEFT )
                    self->ai->combatmovepushes[1] = -1;
                else if( canMOVERIGHT )
                    self->ai->combatmovepushes[1] = 1;
            }
            else
            {
                //priorize sides
                if( canMOVELEFT || canMOVERIGHT )
                {
                    if( canMOVELEFT && canMOVERIGHT )
                    {
                        self->ai->combatmovepushes[1] = c < 0.5 ? -1 : 1;
                    }
                    else if( canMOVELEFT )
                    {
                        self->ai->combatmovepushes[1] = -1;
                    }
                    else
                    {
                        self->ai->combatmovepushes[1] = 1;
                    }
                }

                if( c < 0.3 && canMOVEBACK )
                    self->ai->combatmovepushes[0] = -1;
            }
        }
        else if( dist < 500 ) //AIWEAP_SHORT_RANGE limit is Grenade Laucher range
        {
            if( canMOVELEFT || canMOVERIGHT )
            {
                if( canMOVELEFT && canMOVERIGHT )
                {
                    self->ai->combatmovepushes[1] = c < 0.5 ? -1 : 1;
                }
                else if( canMOVELEFT )
                {
                    self->ai->combatmovepushes[1] = -1;
                }
                else
                {
                    self->ai->combatmovepushes[1] = 1;
                }
            }

            if( c < 0.3 && canMOVEFRONT )
            {
                self->ai->combatmovepushes[0] = 1;
            }
        }
        else if( dist < 900 )
        {
            if( canMOVELEFT || canMOVERIGHT )
            {
                if( canMOVELEFT && canMOVERIGHT )
                {
                    self->ai->combatmovepushes[1] = c < 0.5 ? -1 : 1;
                }
                else if( canMOVELEFT )
                {
                    self->ai->combatmovepushes[1] = -1;
                }
                else
                {
                    self->ai->combatmovepushes[1] = 1;
                }
            }
        }
        else //range = AIWEAP_LONG_RANGE;
        {
            if( c < 0.75 && ( canMOVELEFT || canMOVERIGHT ) )
            {
                if( canMOVELEFT && canMOVERIGHT )
                {
                    self->ai->combatmovepushes[1] = c < 0.5 ? -1 : 1;
                }
                else if( canMOVELEFT )
                {
                    self->ai->combatmovepushes[1] = -1;
                }
                else
                {
                    self->ai->combatmovepushes[1] = 1;
                }
            }
        }
    }

    if( !hasToEvade && ( self->health < 25 || ( dist >= 500 && c < 0.2 ) || ( dist >= 1000 && c < 0.5 ) ) )
    {
        Move( ucmd );
    }

    if( !self->ai->camp_item )
    {
        ucmd->forwardmove = self->ai->combatmovepushes[0];
    }
    ucmd->sidemove = self->ai->combatmovepushes[1];
    ucmd->upmove = self->ai->combatmovepushes[2];
}