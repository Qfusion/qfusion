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

    if( AI_GetNodeFlags( self->ai->next_node ) & (NODEFLAGS_REACHATTOUCH|NODEFLAGS_ENTITYREACH) )
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
    else if( AI_GetNodeFlags( self->ai->next_node ) & NODEFLAGS_PLATFORM )
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
        AI_NodeReached( self );

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
            else if( CanMove( BOT_MOVE_FORWARD ) )
            {
                ucmd->forwardmove = 1;
                ucmd->buttons |= BUTTON_WALK;
            }

            return;
        }

        // Otherwise move slowly, walking wondering what's going on
        ucmd->buttons |= BUTTON_WALK;
    }

    if( CanMove( BOT_MOVE_FORWARD ) )
        ucmd->forwardmove = 1;
    else
        ucmd->forwardmove = -1;
}

bool Bot::FindRocket(vec3_t away_from_rocket)
{
#define AI_ROCKET_DETECT_RADIUS 1000
#define AI_ROCKET_DANGER_RADIUS 200
    int i, numtargets;
    int targets[MAX_EDICTS];
    edict_t *target;
    float min_roxx_time = 1.0f;
    bool any_rocket = false;

    numtargets = GClip_FindRadius( self->s.origin, AI_ROCKET_DETECT_RADIUS, targets, MAX_EDICTS );
    for( i = 0; i < numtargets; i++ )
    {
        target = game.edicts + targets[i];

        // Missile detection code
        if( target->r.svflags & SVF_PROJECTILE && target->s.type != ET_PLASMA ) // (plasmas come in bunchs so are too complex for the bot to dodge)
        {
            if( target->r.owner && target->r.owner != self )
            {
                vec3_t end;
                trace_t trace;

                VectorMA( target->s.origin, 2, target->velocity, end );
                G_Trace( &trace, target->s.origin, target->r.mins, target->r.maxs, end, target, MASK_SOLID );
                if( trace.fraction < min_roxx_time )
                {
                    vec_t l;

                    any_rocket = true;
                    min_roxx_time = trace.fraction;
                    VectorSubtract( trace.endpos, self->s.origin, end );
                    // ok... end is where the impact will be.
                    // trace.fraction is the time.

                    if( ( l = VectorLengthFast( end ) ) < AI_ROCKET_DANGER_RADIUS )
                    {
                        RotatePointAroundVector( away_from_rocket, &axis_identity[AXIS_UP], end, -self->s.angles[YAW] );
                        VectorNormalize( away_from_rocket );

                        if( fabs( away_from_rocket[0] ) < 0.3 ) away_from_rocket[0] = 0;
                        if( fabs( away_from_rocket[1] ) < 0.3 ) away_from_rocket[1] = 0;
                        away_from_rocket[2] = 0;
                        away_from_rocket[0] *= -1.0f;
                        away_from_rocket[1] *= -1.0f;

                        if( nav.debugMode && bot_showcombat->integer > 2 )
                            G_PrintChasersf( self, "%s: ^1projectile dodge: ^2%f, %f d=%f^7\n", self->ai->pers.netname, away_from_rocket[0], away_from_rocket[1], l );
                    }
                }
            }
        }
    }

    return any_rocket;

#undef AI_ROCKET_DETECT_RADIUS
#undef AI_ROCKET_DANGER_RADIUS
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
    bool rocket = false;
    vec3_t away_from_rocket = { 0, 0, 0 };

    if( !self->enemy || self->ai->rush_item )
    {
        Move(ucmd);
        return;
    }

    if( self->ai->pers.skillLevel >= 0.25f )
        rocket = FindRocket(away_from_rocket);

    dist = DistanceFast( self->s.origin, self->enemy->s.origin );
    c = random();

    if( level.time > self->ai->combatmovepush_timeout )
    {
        bool canMOVELEFT, canMOVERIGHT, canMOVEFRONT, canMOVEBACK;

        canMOVELEFT = CanMove( BOT_MOVE_LEFT );
        canMOVERIGHT = CanMove( BOT_MOVE_RIGHT );
        canMOVEFRONT = CanMove( BOT_MOVE_FORWARD );
        canMOVEBACK = CanMove( BOT_MOVE_BACK );

        self->ai->combatmovepush_timeout = level.time + AI_COMBATMOVE_TIMEOUT;
        VectorClear( self->ai->combatmovepushes );

        if( rocket )
        {
            //VectorScale(away_from_rocket,1,self->ai->combatmovepushes);
            if( away_from_rocket[0] )
            {
                if( ( away_from_rocket[0] < 0 ) && canMOVEBACK )
                    self->ai->combatmovepushes[0] = -1;
                else if( ( away_from_rocket[0] > 0 ) && canMOVEFRONT )
                    self->ai->combatmovepushes[0] = 1;
            }
            if( away_from_rocket[1] )
            {
                if( ( away_from_rocket[1] < 0 ) && canMOVELEFT )
                    self->ai->combatmovepushes[1] = -1;
                else if( ( away_from_rocket[1] > 0 ) && canMOVERIGHT )
                    self->ai->combatmovepushes[1] = 1;
            }

            ucmd->buttons |= BUTTON_SPECIAL;
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

    if( !rocket && ( self->health < 25 || ( dist >= 500 && c < 0.2 ) || ( dist >= 1000 && c < 0.5 ) ) )
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



//==========================================
// BOT_DMclass_FindEnemy
// Scan for enemy (simplifed for now to just pick any visible enemy)
//==========================================
void Bot::FindEnemy()
{
#define WEIGHT_MAXDISTANCE_FACTOR 15000
    nav_ents_t *goalEnt;
    edict_t *bestTarget = NULL;
    float dist, weight, bestWeight = 9999999;
    vec3_t forward, vec;
    int i;

    if( G_ISGHOSTING( self )
        || GS_MatchState() == MATCH_STATE_COUNTDOWN
        || GS_ShootingDisabled() )
    {
        self->ai->enemyReactionDelay = 0;
        self->enemy = self->ai->latched_enemy = NULL;
        return;
    }

    // we also latch NULL enemies, so the bot can loose them
    if( self->ai->enemyReactionDelay > 0 )
    {
        self->ai->enemyReactionDelay -= game.frametime;
        return;
    }

    self->enemy = self->ai->latched_enemy;

    FOREACH_GOALENT( goalEnt )
    {
        i = goalEnt->id;

        if( !goalEnt->ent || !goalEnt->ent->r.inuse )
            continue;

        if( !goalEnt->ent->r.client ) // this may be changed, there could be enemies which aren't clients
            continue;

        if( G_ISGHOSTING( goalEnt->ent ) )
            continue;

        if( self->ai->status.entityWeights[i] <= 0 || goalEnt->ent->flags & (FL_NOTARGET|FL_BUSY) )
            continue;

        if( GS_TeamBasedGametype() && goalEnt->ent->s.team == self->s.team )
            continue;

        dist = DistanceFast( self->s.origin, goalEnt->ent->s.origin );

        // ignore very soft weighted enemies unless they are in your face
        if( dist > 500 && self->ai->status.entityWeights[i] <= 0.1f )
            continue;

        //if( dist > 700 && dist > WEIGHT_MAXDISTANCE_FACTOR * self->ai->status.entityWeights[i] )
        //	continue;

        weight = dist / self->ai->status.entityWeights[i];

        if( weight < bestWeight )
        {
            if( trap_inPVS( self->s.origin, goalEnt->ent->s.origin ) && G_Visible( self, goalEnt->ent ) )
            {
                bool close = dist < 2000 || goalEnt->ent == self->ai->last_attacker;

                if( !close )
                {
                    VectorSubtract( goalEnt->ent->s.origin, self->s.origin, vec );
                    VectorNormalize( vec );
                    close = DotProduct( vec, forward ) > 0.3;
                }

                if( close )
                {
                    bestWeight = weight;
                    bestTarget = goalEnt->ent;
                }
            }
        }
    }

    NewEnemyInView( bestTarget );
#undef WEIGHT_MAXDISTANCE_FACTOR
}

//==========================================
// BOT_DMClass_ChangeWeapon
//==========================================
bool Bot::ChangeWeapon(int weapon)
{
    if( weapon == self->r.client->ps.stats[STAT_PENDING_WEAPON] )
        return false;

    if( !GS_CheckAmmoInWeapon( &self->r.client->ps , weapon ) )
        return false;

    // Change to this weapon
    self->r.client->ps.stats[STAT_PENDING_WEAPON] = weapon;
    self->ai->changeweapon_timeout = level.time + 2000 + ( 4000 * ( 1.0 - self->ai->pers.skillLevel ) );

    return true;
}

//==========================================
// BOT_DMclass_ChooseWeapon
// Choose weapon based on range & weights
//==========================================
float Bot::ChooseWeapon()
{
    float dist;
    int i;
    float best_weight = 0.0;
    gsitem_t *weaponItem;
    int curweapon, weapon_range = 0, best_weapon = WEAP_NONE;

    curweapon = self->r.client->ps.stats[STAT_PENDING_WEAPON];

    // if no enemy, then what are we doing here?
    if( !self->enemy )
    {
        weapon_range = AIWEAP_MEDIUM_RANGE;
        if( curweapon == WEAP_GUNBLADE || curweapon == WEAP_NONE )
            self->ai->changeweapon_timeout = level.time;
    }
    else // Base weapon selection on distance:
    {
        dist = DistanceFast( self->s.origin, self->enemy->s.origin );

        if( dist < 150 )
            weapon_range = AIWEAP_MELEE_RANGE;
        else if( dist < 500 )  // Medium range limit is Grenade launcher range
            weapon_range = AIWEAP_SHORT_RANGE;
        else if( dist < 900 )
            weapon_range = AIWEAP_MEDIUM_RANGE;
        else
            weapon_range = AIWEAP_LONG_RANGE;
    }

    if( self->ai->changeweapon_timeout > level.time )
        return AIWeapons[curweapon].RangeWeight[weapon_range];

    for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ )
    {
        float rangeWeight;

        if( ( weaponItem = GS_FindItemByTag( i ) ) == NULL )
            continue;

        if( !GS_CheckAmmoInWeapon( &self->r.client->ps, i ) )
            continue;

        rangeWeight = AIWeapons[i].RangeWeight[weapon_range] * self->ai->pers.cha.weapon_affinity[i - ( WEAP_GUNBLADE - 1 )];

        // weigh up if having strong ammo
        if( self->r.client->ps.inventory[weaponItem->ammo_tag] )
            rangeWeight *= 1.25;

        // add a small random factor (less random the more skill)
        rangeWeight += brandom( -( 1.0 - self->ai->pers.skillLevel ), 1.0 - self->ai->pers.skillLevel );

        // compare range weights
        if( rangeWeight > best_weight )
        {
            best_weight = rangeWeight;
            best_weapon = i;
        }
    }

    // do the change (same weapon, or null best_weapon is covered at ChangeWeapon)
    if( best_weapon != WEAP_NONE )
        ChangeWeapon(best_weapon);

    return AIWeapons[curweapon].RangeWeight[weapon_range]; // return current
}

//==========================================
// BOT_DMclass_CheckShot
// Checks if shot is blocked (doesn't verify it would hit)
//==========================================
bool Bot::CheckShot(vec3_t point)
{
    trace_t tr;
    vec3_t start, forward, right, offset;

    if( random() > self->ai->pers.cha.firerate )
        return false;

    AngleVectors( self->r.client->ps.viewangles, forward, right, NULL );

    VectorSet( offset, 0, 0, self->viewheight );
    G_ProjectSource( self->s.origin, offset, forward, right, start );

    // blocked, don't shoot
    G_Trace( &tr, start, vec3_origin, vec3_origin, point, self, MASK_AISOLID );
    if( tr.fraction < 0.8f )
    {
        if( tr.ent < 1 || !game.edicts[tr.ent].takedamage || game.edicts[tr.ent].movetype == MOVETYPE_PUSH )
            return false;

        // check if the player we found is at our team
        if( game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype() )
            return false;
    }

    return true;
}

//==========================================
// BOT_DMclass_PredictProjectileShot
// predict target movement
//==========================================
void Bot::PredictProjectileShot(vec3_t fire_origin, float projectile_speed, vec3_t target, vec3_t target_velocity)
{
    vec3_t predictedTarget;
    vec3_t targetMovedir;
    float targetSpeed;
    float predictionTime;
    float distance;
    trace_t	trace;
    int contents;

    if( projectile_speed <= 0.0f )
        return;

    targetSpeed = VectorNormalize2( target_velocity, targetMovedir );

    // ok, this is not going to be 100% precise, since we will find the
    // time our projectile will take to travel to enemy's CURRENT position,
    // and them find enemy's position given his CURRENT velocity and his CURRENT dir
    // after prediction time. The result will be much better if the player
    // is moving to the sides (relative to us) than in depth (relative to us).
    // And, of course, when the player moves in a curve upwards it will totally miss (ie, jumping).

    // but in general it does a great job, much better than any human player :)

    distance = DistanceFast( fire_origin, target );
    predictionTime = distance/projectile_speed;
    VectorMA( target, predictionTime*targetSpeed, targetMovedir, predictedTarget );

    // if this position is inside solid, try finding a position at half of the prediction time
    contents = G_PointContents( predictedTarget );
    if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
    {
        VectorMA( target, ( predictionTime * 0.5f )*targetSpeed, targetMovedir, predictedTarget );
        contents = G_PointContents( predictedTarget );
        if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
            return; // INVALID
    }

    // if we can see this point, we use it, otherwise we keep the current position
    G_Trace( &trace, fire_origin, vec3_origin, vec3_origin, predictedTarget, self, MASK_SHOT );
    if( trace.fraction == 1.0f || ( trace.ent && game.edicts[trace.ent].takedamage ) )
        VectorCopy( predictedTarget, target );
}

//==========================================
// BOT_DMclass_FireWeapon
// Fire if needed
//==========================================
bool Bot::FireWeapon(usercmd_t *ucmd)
{
#define WFAC_GENERIC_PROJECTILE 300.0
#define WFAC_GENERIC_INSTANT 150.0
    float firedelay;
    vec3_t target;
    int weapon, i;
    float wfac;
    vec3_t fire_origin;
    trace_t	trace;
    bool continuous_fire = false;
    firedef_t *firedef = GS_FiredefForPlayerState( &self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON] );

    if( !self->enemy )
        return false;

    weapon = self->s.weapon;
    if( weapon < 0 || weapon >= WEAP_TOTAL )
        weapon = 0;

    if( !firedef )
        return false;

    // Aim to center of the box
    for( i = 0; i < 3; i++ )
        target[i] = self->enemy->s.origin[i] + ( 0.5f * ( self->enemy->r.maxs[i] + self->enemy->r.mins[i] ) );
    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;

    if( self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN )
        continuous_fire = true;

    if( !continuous_fire && !CheckShot(target) )
        return false;

    // find out our weapon AIM style
    if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION_EXPLOSIVE )
    {
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);

        wfac = WFAC_GENERIC_PROJECTILE * 1.3;

        // aim to the feet when enemy isn't higher
        if( fire_origin[2] > ( target[2] + ( self->enemy->r.mins[2] * 0.8 ) ) )
        {
            vec3_t checktarget;
            VectorSet( checktarget,
                       self->enemy->s.origin[0],
                       self->enemy->s.origin[1],
                       self->enemy->s.origin[2] + self->enemy->r.mins[2] + 4 );

            G_Trace( &trace, fire_origin, vec3_origin, vec3_origin, checktarget, self, MASK_SHOT );
            if( trace.fraction == 1.0f || ( trace.ent > 0 && game.edicts[trace.ent].takedamage ) )
                VectorCopy( checktarget, target );
        }
        else if( !IsStep( self->enemy ) )
            wfac *= 2.5; // more imprecise for air rockets
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION )
    {
        if( self->s.weapon == WEAP_PLASMAGUN )
            wfac = WFAC_GENERIC_PROJECTILE * 0.5;
        else
            wfac = WFAC_GENERIC_PROJECTILE;

        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_DROP )
    {
        //jalToDo
        wfac = WFAC_GENERIC_PROJECTILE;
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity );
    }
    else // AI_AIMSTYLE_INSTANTHIT
    {
        if( self->s.weapon == WEAP_ELECTROBOLT )
            wfac = WFAC_GENERIC_INSTANT;
        else if( self->s.weapon == WEAP_LASERGUN )
            wfac = WFAC_GENERIC_INSTANT * 1.5;
        else
            wfac = WFAC_GENERIC_INSTANT;
    }

    wfac = 25 + wfac * ( 1.0f - self->ai->pers.skillLevel );

    // look to target
    VectorSubtract( target, fire_origin, self->ai->move_vector );

    if( self->r.client->ps.weaponState == WEAPON_STATE_READY ||
        self->r.client->ps.weaponState == WEAPON_STATE_REFIRE ||
        self->r.client->ps.weaponState == WEAPON_STATE_REFIRESTRONG )
    {
        // in continuous fire weapons don't add delays
        if( self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN )
            firedelay = 1.0f;
        else
            firedelay = ( 1.0f - self->ai->pers.skillLevel ) - ( random()-0.25f );

        if( firedelay > 0.0f )
        {
            if( G_InFront( self, self->enemy ) ) {
                ucmd->buttons |= BUTTON_ATTACK; // could fire, but wants to?
            }
            // mess up angles only in the attacking frames
            if( self->r.client->ps.weaponState == WEAPON_STATE_READY ||
                self->r.client->ps.weaponState == WEAPON_STATE_REFIRE ||
                self->r.client->ps.weaponState == WEAPON_STATE_REFIRESTRONG )
            {
                if( (self->s.weapon == WEAP_LASERGUN) || (self->s.weapon == WEAP_PLASMAGUN) ) {
                    target[0] += sinf( (float)level.time/100.0) * wfac;
                    target[1] += cosf( (float)level.time/100.0) * wfac;
                }
                else
                {
                    target[0] += ( random()-0.5f ) * wfac;
                    target[1] += ( random()-0.5f ) * wfac;
                }
            }
        }
    }

    //update angles
    VectorSubtract( target, fire_origin, self->ai->move_vector );
    ChangeAngle();

    if( nav.debugMode && bot_showcombat->integer )
        G_PrintChasersf( self, "%s: attacking %s\n", self->ai->pers.netname, self->enemy->r.client ? self->enemy->r.client->netname : self->classname );

    return true;
}

float Bot::PlayerWeight(edict_t *enemy)
{
    bool rage_mode = false;

    if( !enemy || enemy == self )
        return 0;

    if( G_ISGHOSTING( enemy ) || enemy->flags & (FL_NOTARGET|FL_BUSY) )
        return 0;

    if( self->r.client->ps.inventory[POWERUP_QUAD] || self->r.client->ps.inventory[POWERUP_SHELL] )
        rage_mode = true;

    // don't fight against powerups.
    if( enemy->r.client && ( enemy->r.client->ps.inventory[POWERUP_QUAD] || enemy->r.client->ps.inventory[POWERUP_SHELL] ) )
        return 0.2;

    //if not team based give some weight to every one
    if( GS_TeamBasedGametype() && ( enemy->s.team == self->s.team ) )
        return 0;

    // if having EF_CARRIER we can assume it's someone important
    if( enemy->s.effects & EF_CARRIER )
        return 2.0f;

    if( enemy == self->ai->last_attacker )
        return rage_mode ? 4.0f : 1.0f;

    return rage_mode ? 4.0f : 0.3f;
}

//==========================================
// BOT_DMclass_UpdateStatus
// update ai.status values based on bot state,
// so ai can decide based on these settings
//==========================================
void Bot::UpdateStatus()
{
    float LowNeedFactor = 0.5;
    gclient_t *client;
    int i;
    bool onlyGotGB = true;
    edict_t *ent;
    ai_handle_t *ai;
    nav_ents_t *goalEnt;

    client = self->r.client;

    ai = self->ai;

    FOREACH_GOALENT( goalEnt )
    {
        i = goalEnt->id;
        ent = goalEnt->ent;

        // item timing disabled by now
        if( ent->r.solid == SOLID_NOT )
        {
            ai->status.entityWeights[i] = 0;
            continue;
        }

        if( ent->r.client )
        {
            ai->status.entityWeights[i] = PlayerWeight( ent ) * self->ai->pers.cha.offensiveness;
            continue;
        }

        if( ent->item )
        {
            if( ent->r.solid == SOLID_NOT )
            {
                ai->status.entityWeights[i] = 0;
                continue;
            }

            if( ent->item->type & IT_WEAPON )
            {
                if( client->ps.inventory[ent->item->tag] )
                {
                    if( client->ps.inventory[ent->item->ammo_tag] )
                    {
                        // find ammo item for this weapon
                        gsitem_t *ammoItem = GS_FindItemByTag( ent->item->ammo_tag );
                        if( ammoItem->inventory_max )
                        {
                            ai->status.entityWeights[i] *= (0.5 + 0.5 * (1.0 - (float)client->ps.inventory[ent->item->ammo_tag] / ammoItem->inventory_max));
                        }
                        ai->status.entityWeights[i] *= LowNeedFactor;
                    }
                    else
                    {
                        // we need some ammo
                        ai->status.entityWeights[i] *= LowNeedFactor;
                    }
                    onlyGotGB = false;
                }
            }
            else if( ent->item->type & IT_AMMO )
            {
                if( client->ps.inventory[ent->item->tag] >= ent->item->inventory_max )
                {
                    ai->status.entityWeights[i] = 0.0;
                }
                else
                {
#if 0
                    // find weapon item for this ammo
					gsitem_t *weaponItem;
					int weapon;

					for( weapon = WEAP_GUNBLADE; weapon < WEAP_TOTAL; weapon++ )
					{
						weaponItem = GS_FindItemByTag( weapon );
						if( weaponItem->ammo_tag == ent->item->tag )
						{
							if( !client->ps.inventory[weaponItem->tag] )
								self->ai->status.entityWeights[i] *= LowNeedFactor;
						}
					}
#endif
                }
            }
            else if( ent->item->type & IT_ARMOR )
            {
                if ( self->r.client->resp.armor < ent->item->inventory_max || !ent->item->inventory_max )
                {
                    if( ent->item->inventory_max )
                    {
                        if( ( (float)self->r.client->resp.armor / (float)ent->item->inventory_max ) > 0.75 )
                            ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag] * LowNeedFactor;
                    }
                    else
                        ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
                }
                else
                {
                    ai->status.entityWeights[i] = 0;
                }
            }
            else if( ent->item->type & IT_HEALTH )
            {
                if( ent->item->tag == HEALTH_MEGA || ent->item->tag == HEALTH_ULTRA || ent->item->tag == HEALTH_SMALL )
                    ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
                else
                {
                    if( self->health >= self->max_health )
                        ai->status.entityWeights[i] = 0;
                    else
                    {
                        float health_func;

                        health_func = self->health / self->max_health;
                        health_func *= health_func;

                        ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag] + ( 1.1f - health_func );
                    }
                }
            }
            else if( ent->item->type & IT_POWERUP )
            {
                ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
            }
        }
    }

    if( onlyGotGB )
    {
        FOREACH_GOALENT( goalEnt )
        {
            i = goalEnt->id;
            ent = goalEnt->ent;

            if( ent->item && ent->item->type & IT_WEAPON )
                self->ai->status.entityWeights[i] *= 2.0f;
        }
    }
}

//==========================================
// BOT_DMclass_VSAYmessages
//==========================================
void Bot::SayVoiceMessages()
{
    if( GS_MatchState() != MATCH_STATE_PLAYTIME )
        return;
    if( level.gametype.dummyBots || bot_dummy->integer )
        return;

    if( self->snap.damageteam_given > 25 )
    {
        if( rand() & 1 )
        {
            if( rand() & 1 )
            {
                G_BOTvsay_f( self, "oops", true );
            }
            else
            {
                G_BOTvsay_f( self, "sorry", true );
            }
        }
        return;
    }

    if( self->ai->vsay_timeout > level.time )
        return;

    if( GS_MatchDuration() && game.serverTime + 4000 > GS_MatchEndTime() )
    {
        self->ai->vsay_timeout = game.serverTime + ( 1000 + (GS_MatchEndTime() - game.serverTime) );
        if( rand() & 1 )
            G_BOTvsay_f( self, "goodgame", false );
        return;
    }

    self->ai->vsay_timeout = level.time + ( ( 8+random()*12 ) * 1000 );

    // the more bots, the less vsays to play
    if( random() > 0.1 + 1.0f / game.numBots )
        return;

    if( GS_TeamBasedGametype() && !GS_InvidualGameType() )
    {
        if( self->health < 20 && random() > 0.3 )
        {
            G_BOTvsay_f( self, "needhealth", true );
            return;
        }

        if( ( self->s.weapon == 0 || self->s.weapon == 1 ) && random() > 0.7 )
        {
            G_BOTvsay_f( self, "needweapon", true );
            return;
        }

        if( self->r.client->resp.armor < 10 && random() > 0.8 )
        {
            G_BOTvsay_f( self, "needarmor", true );
            return;
        }
    }

    // NOT team based here

    if( random() > 0.2 )
        return;

    switch( (int)brandom( 1, 8 ) )
    {
        default:
            break;
        case 1:
            G_BOTvsay_f( self, "roger", false );
            break;
        case 2:
            G_BOTvsay_f( self, "noproblem", false );
            break;
        case 3:
            G_BOTvsay_f( self, "yeehaa", false );
            break;
        case 4:
            G_BOTvsay_f( self, "yes", false );
            break;
        case 5:
            G_BOTvsay_f( self, "no", false );
            break;
        case 6:
            G_BOTvsay_f( self, "booo", false );
            break;
        case 7:
            G_BOTvsay_f( self, "attack", false );
            break;
        case 8:
            G_BOTvsay_f( self, "ok", false );
            break;
    }
}


//==========================================
// BOT_DMClass_BlockedTimeout
// the bot has been blocked for too long
//==========================================
void Bot::BlockedTimeout()
{
    if( level.gametype.dummyBots || bot_dummy->integer ) {
        self->ai->blocked_timeout = level.time + 15000;
        return;
    }
    self->health = 0;
    self->ai->blocked_timeout = level.time + 15000;
    self->die( self, self, self, 100000, vec3_origin );
    G_Killed( self, self, self, 999, vec3_origin, MOD_SUICIDE );
    self->nextThink = level.time + 1;
}

//==========================================
// BOT_DMclass_DeadFrame
// ent is dead = run this think func
//==========================================
void Bot::GhostingFrame()
{
    usercmd_t ucmd;

    AI_ClearGoal( self );

    self->ai->blocked_timeout = level.time + 15000;
    self->nextThink = level.time + 100;

    // wait 4 seconds after entering the level
    if( self->r.client->level.timeStamp + 4000 > level.time || !level.canSpawnEntities )
        return;

    if( self->r.client->team == TEAM_SPECTATOR )
    {
        // try to join a team
        // note that G_Teams_JoinAnyTeam is quite slow so only call it per frame
        if( !self->r.client->queueTimeStamp && self == level.think_client_entity )
            G_Teams_JoinAnyTeam( self, false );

        if( self->r.client->team == TEAM_SPECTATOR ) // couldn't join, delay the next think
            self->nextThink = level.time + 2000 + (int)( 4000 * random() );
        else
            self->nextThink = level.time + 1;
        return;
    }

    memset( &ucmd, 0, sizeof( ucmd ) );

    // set approximate ping and show values
    ucmd.serverTimeStamp = game.serverTime;
    ucmd.msec = game.frametime;
    self->r.client->r.ping = 0;

    // ask for respawn if the minimum bot respawning time passed
    if( level.time > self->deathTimeStamp + 3000 )
        ucmd.buttons = BUTTON_ATTACK;

    ClientThink( self, &ucmd, 0 );
}


//==========================================
// BOT_DMclass_RunFrame
// States Machine & call client movement
//==========================================
void Bot::RunFrame()
{
    usercmd_t ucmd;
    float weapon_quality;
    bool inhibitCombat = false;
    int i;

    if( G_ISGHOSTING( self ) )
    {
        GhostingFrame();
        return;
    }

    memset( &ucmd, 0, sizeof( ucmd ) );

    //get ready if in the game
    if( GS_MatchState() <= MATCH_STATE_WARMUP && !level.ready[PLAYERNUM(self)]
        && self->r.client->teamstate.timeStamp + 4000 < level.time )
        G_Match_Ready( self );

    if( level.gametype.dummyBots || bot_dummy->integer )
    {
        self->r.client->level.last_activity = level.time;
    }
    else
    {
        FindEnemy();

        weapon_quality = ChooseWeapon();

        inhibitCombat = ( CurrentLinkType() & (LINK_JUMPPAD|LINK_JUMP|LINK_ROCKETJUMP) ) != 0 ? true : false;

        if( self->enemy && weapon_quality >= 0.3 && !inhibitCombat ) // don't fight with bad weapons
        {
            if( FireWeapon( &ucmd ) )
                self->ai->state_combat_timeout = level.time + AI_COMBATMOVE_TIMEOUT;
        }

        if( inhibitCombat )
            self->ai->state_combat_timeout = 0;

        if( self->ai->state_combat_timeout > level.time )
        {
            CombatMovement( &ucmd );
        }
        else
        {
            Move( &ucmd );
        }

        //set up for pmove
        for( i = 0; i < 3; i++ )
            ucmd.angles[i] = ANGLE2SHORT( self->s.angles[i] ) - self->r.client->ps.pmove.delta_angles[i];

        VectorSet( self->r.client->ps.pmove.delta_angles, 0, 0, 0 );
    }

    // set approximate ping and show values
    ucmd.msec = game.frametime;
    ucmd.serverTimeStamp = game.serverTime;

    ClientThink( self, &ucmd, 0 );
    self->nextThink = level.time + 1;

    SayVoiceMessages();
}


