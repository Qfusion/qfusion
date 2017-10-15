#include "bot.h"
#include "bot_movement.h"
#include "ai_aas_world.h"
#include "tactical_spots_registry.h"

#ifndef PUBLIC_BUILD
#define CHECK_ACTION_SUGGESTION_LOOPS
#define ENABLE_MOVEMENT_ASSERTIONS
#endif

// Useful for debugging but freezes even Release version
#if 0
#define ENABLE_MOVEMENT_DEBUG_OUTPUT
#endif

typedef BotBaseMovementAction::AreaAndScore AreaAndScore;

inline float GetPMoveStatValue( const player_state_t *playerState, int statIndex, float defaultValue ) {
	float value = playerState->pmove.stats[statIndex];
	// Put likely case (the value is not specified) first
	return value < 0 ? defaultValue : value;
}

inline float BotMovementPredictionContext::GetJumpSpeed() const {
	return GetPMoveStatValue( this->currPlayerState, PM_STAT_JUMPSPEED, DEFAULT_JUMPSPEED * GRAVITY_COMPENSATE );
}

inline float BotMovementPredictionContext::GetDashSpeed() const {
	return GetPMoveStatValue( this->currPlayerState, PM_STAT_DASHSPEED, DEFAULT_DASHSPEED );
}

inline float BotMovementPredictionContext::GetRunSpeed() const {
	return GetPMoveStatValue( this->currPlayerState, PM_STAT_MAXSPEED, DEFAULT_PLAYERSPEED );
}

inline Vec3 BotMovementPredictionContext::NavTargetOrigin() const {
	return self->ai->botRef->NavTargetOrigin();
}

inline float BotMovementPredictionContext::NavTargetRadius() const {
	return self->ai->botRef->NavTargetRadius();
}

inline bool BotMovementPredictionContext::IsCloseToNavTarget() const {
	float distance = NavTargetRadius() + 32.0f;
	return NavTargetOrigin().SquareDistanceTo( movementState->entityPhysicsState.Origin() ) < distance * distance;
}

inline int BotMovementPredictionContext::CurrAasAreaNum() const {
	if( int currAasAreaNum = movementState->entityPhysicsState.CurrAasAreaNum() ) {
		return currAasAreaNum;
	}

	return movementState->entityPhysicsState.DroppedToFloorAasAreaNum();
}

inline int BotMovementPredictionContext::CurrGroundedAasAreaNum() const {
	const auto *aasWorld = AiAasWorld::Instance();
	const auto &entityPhysicsState = movementState->entityPhysicsState;
	int areaNums[2] = { entityPhysicsState.CurrAasAreaNum(), entityPhysicsState.DroppedToFloorAasAreaNum() };
	for( int i = 0, end = ( areaNums[0] != areaNums[1] ? 2 : 1 ); i < end; ++i ) {
		if( areaNums[i] && aasWorld->AreaGrounded( areaNums[i] ) ) {
			return areaNums[i];
		}
	}
	return 0;
}

inline int BotMovementPredictionContext::NavTargetAasAreaNum() const {
	return self->ai->botRef->NavTargetAasAreaNum();
}

inline bool BotMovementPredictionContext::IsInNavTargetArea() const {
	const int navTargetAreaNum = NavTargetAasAreaNum();
	if( !navTargetAreaNum ) {
		return false;
	}

	const auto &entityPhysicsState = this->movementState->entityPhysicsState;
	if( navTargetAreaNum == entityPhysicsState.CurrAasAreaNum() ) {
		return true;
	}

	if( navTargetAreaNum == entityPhysicsState.DroppedToFloorAasAreaNum() ) {
		return true;
	}

	return false;
}

inline bool IsInsideHugeArea( const float *origin, const aas_area_t &area, float offset ) {
	if( area.mins[0] > origin[0] - offset || area.maxs[0] < origin[0] + offset ) {
		return false;
	}

	if( area.mins[1] > origin[1] - offset || area.maxs[1] < origin[1] + offset ) {
		return false;
	}

	return true;
}

inline bool BotMovementPredictionContext::CanSafelyKeepHighSpeed() {
	if( const bool *cachedValue = canSafelyKeepHighSpeedCachesStack.GetCached() ) {
		return *cachedValue;
	}

	bool result = TestWhetherCanSafelyKeepHighSpeed();
	canSafelyKeepHighSpeedCachesStack.SetCachedValue( result );
	return result;
}

bool BotMovementPredictionContext::TestWhetherCanSafelyKeepHighSpeed() {
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasAreas = aasWorld->Areas();
	const auto *routeCache = self->ai->botRef->routeCache;
	const auto &entityPhysicsState = movementState->entityPhysicsState;

	const float offset = 20.0f + 48.0f * BoundedFraction( entityPhysicsState.HeightOverGround(), 32 );
	int prevAreaNum = CurrGroundedAasAreaNum();
	if( prevAreaNum && IsInsideHugeArea( entityPhysicsState.Origin(), aasAreas[prevAreaNum], offset ) ) {
		return true;
	}

	const int navTargetAreaNum = this->NavTargetAasAreaNum();
	const int travelFlags[] = { self->ai->botRef->preferredAasTravelFlags, self->ai->botRef->allowedAasTravelFlags };
	int startTravelTime = this->TravelTimeToNavTarget();
	int travelTime = 0;

	Vec3 origin( entityPhysicsState.Origin() );
	Vec3 velocity( entityPhysicsState.Velocity() );
	Vec3 prevOrigin( origin );
	float secondsAhead = 0.0f;
	trace_t trace;
	for(;; ) {
		secondsAhead += 0.1f;
		velocity.Set( entityPhysicsState.Velocity() );
		velocity.Z() = entityPhysicsState.Velocity()[2] - secondsAhead * level.gravity;
		origin += secondsAhead * velocity;

		// Try use cheap AAS area bounds tests
		int areaNum = aasWorld->FindAreaNum( origin );
		if( areaNum != prevAreaNum && areaNum ) {
			if( aasWorld->AreaGrounded( areaNum ) ) {
				if( IsInsideHugeArea( entityPhysicsState.Origin(), aasAreas[areaNum], offset ) ) {
					return true;
				}
			}
		}
		prevAreaNum = areaNum;

		SolidWorldTrace( &trace, prevOrigin.Data(), origin.Data(), playerbox_stand_mins, playerbox_stand_maxs );
		if( trace.fraction != 1.0f ) {
			// No area is found on landing
			if( !areaNum ) {
				return false;
			}

			// Disallow bumping into walls (it can lead to cycling in tight environments)
			if( !ISWALKABLEPLANE( &trace.plane.normal ) ) {
				return false;
			}

			// Check travel time to the nav target if it is present
			if( navTargetAreaNum ) {
				for( int flags: travelFlags ) {
					if( ( travelTime = routeCache->TravelTimeToGoalArea( areaNum, navTargetAreaNum, flags ) ) ) {
						break;
					}
				}
				if( !travelTime || travelTime > startTravelTime + 100 ) {
					return false;
				}
			}
			return trace.endpos[2] + 8 - playerbox_stand_mins[2] >= entityPhysicsState.Origin()[2];
		}

		// The ground is still not found
		if( secondsAhead > 0.5f ) {
			return false;
		}

		prevOrigin = origin;
	}
}

void BotMovementPredictionContext::NextReachNumAndTravelTimeToNavTarget( int *reachNum, int *travelTimeToNavTarget ) {
	*reachNum = 0;
	*travelTimeToNavTarget = 0;

	// Do NOT use cached reachability chain for the frame (if any).
	// It might be invalid after movement step, and the route cache does caching itself pretty well.

	const int navTargetAreaNum = NavTargetAasAreaNum();
	if( !navTargetAreaNum ) {
		return;
	}

	const auto &entityPhysicsState = movementState->entityPhysicsState;
	const auto &routeCache = self->ai->botRef->routeCache;

	int fromAreaNums[2];
	int numFromAreas = 0;

	if( int currAasAreaNum = entityPhysicsState.CurrAasAreaNum() ) {
		fromAreaNums[numFromAreas++] = currAasAreaNum;
	}
	if( int droppedToFloorAasAreaNum = entityPhysicsState.DroppedToFloorAasAreaNum() ) {
		fromAreaNums[numFromAreas++] = droppedToFloorAasAreaNum;
	}

	// Perform search twice only if two these areas are distinct
	if( numFromAreas == 2 && ( fromAreaNums[0] == fromAreaNums[1] ) ) {
		numFromAreas = 1;
	}

	for( int i = 0; i < numFromAreas; ++i ) {
		for( int travelFlags: { Bot::ALLOWED_TRAVEL_FLAGS, Bot::PREFERRED_TRAVEL_FLAGS } ) {
			int routeReachNum, routeTravelTime;
			if( routeCache->ReachAndTravelTimeToGoalArea( fromAreaNums[i], navTargetAreaNum, travelFlags,
														  &routeReachNum, &routeTravelTime ) ) {
				*reachNum = routeReachNum;
				*travelTimeToNavTarget = routeTravelTime;
				return;
			}
			;
		}
	}
}

#define CHECK_STATE_FLAG( state, bit )                                                    \
	if( ( expectedStatesMask & ( 1 << bit ) ) != ( ( (unsigned)state.IsActive() ) << ( 1 << bit ) ) )  \
	{                                                                                       \
		result = false;                                                                     \
		if( logFunc ) {                                                                        \
			logFunc( format, Nick( owner ), #state ".IsActive()", (unsigned)state.IsActive() ); } \
	}

bool BotMovementState::TestActualStatesForExpectedMask( unsigned expectedStatesMask, const edict_t *owner ) const {
	// Might be set to null if verbose logging is not needed
#ifdef ENABLE_MOVEMENT_DEBUG_OUTPUT
	void ( *logFunc )( const char *format, ... ) = G_Printf;
#else
	void ( *logFunc )( const char *format, ... ) = nullptr;
#endif
	constexpr const char *format = "BotMovementState(%s): %s %d has mismatch with the mask value\n";

	bool result = true;
	CHECK_STATE_FLAG( jumppadMovementState, 0 );
	CHECK_STATE_FLAG( weaponJumpMovementState, 1 );
	CHECK_STATE_FLAG( pendingLookAtPointState, 2 );
	CHECK_STATE_FLAG( campingSpotState, 3 );
	// Skip combatMoveDirsState.
	// It either should not affect movement at all if regular movement is chosen,
	// or should be handled solely by the combat movement code.
	CHECK_STATE_FLAG( flyUntilLandingMovementState, 4 );
	return result;
}

BotBaseMovementAction *BotMovementPredictionContext::GetCachedActionAndRecordForCurrTime( BotMovementActionRecord *record_ ) {
	const int64_t levelTime = level.time;
	PredictedMovementAction *prevPredictedAction = nullptr;
	PredictedMovementAction *nextPredictedAction = nullptr;
	for( PredictedMovementAction &predictedAction: predictedMovementActions ) {
		if( predictedAction.timestamp >= levelTime ) {
			nextPredictedAction = &predictedAction;
			break;
		}
		prevPredictedAction = &predictedAction;
	}

	if( !nextPredictedAction ) {
		Debug( "Cannot use predicted movement action: next one (its timestamp is not in the past) cannot be found\n" );
		return nullptr;
	}

	if( !prevPredictedAction ) {
		// If there were no activated actions, the next state must be recently computed for current level time.
		Assert( nextPredictedAction->timestamp == levelTime );
		// These assertions have already spotted a bug
		Assert( VectorCompare( nextPredictedAction->entityPhysicsState.Origin(), self->s.origin ) );
		Assert( VectorCompare( nextPredictedAction->entityPhysicsState.Velocity(), self->velocity ) );
		// If there is a modified velocity, it will be copied with this record and then applied
		*record_ = nextPredictedAction->record;
		Debug( "Using just computed predicted movement action %s\n", nextPredictedAction->action->Name() );
		return nextPredictedAction->action;
	}

	Assert( prevPredictedAction->timestamp + prevPredictedAction->stepMillis == nextPredictedAction->timestamp );

	// Fail prediction if both previous and next predicted movement states mask mismatch the current movement state
	const auto &actualMovementState = self->ai->botRef->movementState;
	if( !actualMovementState.TestActualStatesForExpectedMask( prevPredictedAction->movementStatesMask, self ) ) {
		if( !actualMovementState.TestActualStatesForExpectedMask( nextPredictedAction->movementStatesMask, self ) ) {
			return nullptr;
		}
	}

	// Check whether predicted action is valid for an actual bot entity physics state
	float stateLerpFrac = (float)( levelTime - prevPredictedAction->timestamp );
	stateLerpFrac *= 1.0f / ( nextPredictedAction->timestamp - prevPredictedAction->timestamp );
	Assert( stateLerpFrac > 0 && stateLerpFrac <= 1.0f );
	const char *format =
		"Prev predicted action timestamp is " PRId64 ", next predicted action is " PRId64 ", level.time is %" PRId64 "\n";
	Debug( format, prevPredictedAction->timestamp, nextPredictedAction->timestamp, level.time );
	Debug( "Should interpolate entity physics state using fraction %f\n", stateLerpFrac );

	const auto &prevPhysicsState = prevPredictedAction->entityPhysicsState;
	const auto &nextPhysicsState = nextPredictedAction->entityPhysicsState;

	// Prevent cache invalidation on each frame if bot is being hit by a continuous fire weapon and knocked back.
	// Perform misprediction test only on the 3rd frame after a last knockback timestamp.
	if( level.time - self->ai->botRef->lastKnockbackAt > 32 ) {
		vec3_t expectedOrigin;
		VectorLerp( prevPhysicsState.Origin(), stateLerpFrac, nextPhysicsState.Origin(), expectedOrigin );
		float squaredDistanceMismatch = DistanceSquared( self->s.origin, expectedOrigin );
		if( squaredDistanceMismatch > 3.0f * 3.0f ) {
			float distanceMismatch = SQRTFAST( squaredDistanceMismatch );
			const char *format_ = "Cannot use predicted movement action: distance mismatch %f is too high for lerp frac %f\n";
			Debug( format_, distanceMismatch, stateLerpFrac );
			return nullptr;
		}

		float expectedSpeed = ( 1.0f - stateLerpFrac ) * prevPhysicsState.Speed() + stateLerpFrac * nextPhysicsState.Speed();
		float actualSpeed = self->ai->botRef->entityPhysicsState->Speed();
		float speedMismatch = fabsf( actualSpeed - expectedSpeed );
		if( speedMismatch > 0.005f * expectedSpeed ) {
			Debug( "Expected speed: %.1f, actual speed: %.1f, speed mismatch: %.1f\n", expectedSpeed, actualSpeed,
				   speedMismatch );
			Debug( "Cannot use predicted movement action: speed mismatch is too high\n" );
			return nullptr;
		}

		if( actualSpeed > 30.0f ) {
			vec3_t expectedVelocity;
			VectorLerp( prevPhysicsState.Velocity(), stateLerpFrac, nextPhysicsState.Velocity(), expectedVelocity );
			Vec3 expectedVelocityDir( expectedVelocity );
			expectedVelocityDir *= 1.0f / expectedSpeed;
			Vec3 actualVelocityDir( self->velocity );
			actualVelocityDir *= 1.0f / actualSpeed;
			float cosine = expectedVelocityDir.Dot( actualVelocityDir );
			static const float MIN_COSINE = cosf( (float) DEG2RAD( 5.0f ) );
			if( cosine < MIN_COSINE ) {
				Debug( "An angle between expected and actual velocities is %f degrees\n",(float) RAD2DEG( acosf( cosine ) ) );
				Debug( "Cannot use predicted movement action:  expected and actual velocity directions differ significantly\n" );
				return nullptr;
			}
		}

		if( !nextPredictedAction->record.botInput.canOverrideLookVec ) {
			Vec3 prevStateAngles( prevPhysicsState.Angles() );
			Vec3 nextStateAngles( nextPhysicsState.Angles() );

			vec3_t expectedAngles;
			for( int i : { YAW, ROLL } )
				expectedAngles[i] = LerpAngle( prevStateAngles.Data()[i], nextStateAngles.Data()[i], stateLerpFrac );

			if( !nextPredictedAction->record.botInput.canOverridePitch ) {
				expectedAngles[PITCH] = LerpAngle( prevStateAngles.Data()[PITCH], nextStateAngles.Data()[PITCH],
												   stateLerpFrac );
			} else {
				expectedAngles[PITCH] = self->s.angles[PITCH];
			}

			vec3_t expectedLookDir;
			AngleVectors( expectedAngles, expectedLookDir, nullptr, nullptr );
			float cosine = self->ai->botRef->entityPhysicsState->ForwardDir().Dot( expectedLookDir );
			static const float MIN_COSINE = cosf( (float) DEG2RAD( 5.0f ) );
			if( cosine < MIN_COSINE ) {
				Debug( "An angle between and actual look directions is %f degrees\n", (float) RAD2DEG( acosf( cosine ) ) );
				Debug( "Cannot use predicted movement action: expected and actual look directions differ significantly\n" );
				return nullptr;
			}
		}
	}

	// If next predicted state is likely to be completed next frame, use its input as-is (except the velocity)
	if( nextPredictedAction->timestamp - levelTime <= game.frametime ) {
		*record_ = nextPredictedAction->record;
		// Apply modified velocity only once for an exact timestamp
		if( nextPredictedAction->timestamp != levelTime ) {
			record_->hasModifiedVelocity = false;
		}
		return nextPredictedAction->action;
	}

	float inputLerpFrac = game.frametime / ( (float)( nextPredictedAction->timestamp - levelTime ) );
	Assert( inputLerpFrac > 0 && inputLerpFrac <= 1.0f );
	// If next predicted time is likely to be pending next frame again, interpolate input for a single frame ahead
	*record_ = nextPredictedAction->record;
	// Prevent applying a modified velocity from the new state
	record_->hasModifiedVelocity = false;
	if( !record_->botInput.canOverrideLookVec ) {
		Vec3 actualLookDir( self->ai->botRef->entityPhysicsState->ForwardDir() );
		Vec3 intendedLookVec( record_->botInput.IntendedLookDir() );
		VectorLerp( actualLookDir.Data(), inputLerpFrac, intendedLookVec.Data(), intendedLookVec.Data() );
		record_->botInput.SetIntendedLookDir( intendedLookVec );
	}

	auto prevRotationMask = (unsigned)prevPredictedAction->record.botInput.allowedRotationMask;
	auto nextRotationMask = (unsigned)nextPredictedAction->record.botInput.allowedRotationMask;
	record_->botInput.allowedRotationMask = (BotInputRotation)( prevRotationMask & nextRotationMask );

	return nextPredictedAction->action;
}

BotBaseMovementAction *BotMovementPredictionContext::GetActionAndRecordForCurrTime( BotMovementActionRecord *record_ ) {
	auto *action = GetCachedActionAndRecordForCurrTime( record_ );
	if( !action ) {
		BuildPlan();
		action = GetCachedActionAndRecordForCurrTime( record_ );
	}

	//AITools_DrawColorLine(self->s.origin, (Vec3(0, 0, 48) + self->s.origin).Data(), action->DebugColor(), 0);
	return action;
}

void BotMovementPredictionContext::ShowBuiltPlanPath() const {
	for( unsigned i = 0, j = 1; j < predictedMovementActions.size(); ++i, ++j ) {
		int color;
		switch( i % 3 ) {
			case 0: color = COLOR_RGB( 192, 0, 0 ); break;
			case 1: color = COLOR_RGB( 0, 192, 0 ); break;
			case 2: color = COLOR_RGB( 0, 0, 192 ); break;
		}
		const float *v1 = predictedMovementActions[i].entityPhysicsState.Origin();
		const float *v2 = predictedMovementActions[j].entityPhysicsState.Origin();
		AITools_DrawColorLine( v1, v2, color, 0 );
	}
}

const Ai::ReachChainVector &BotMovementPredictionContext::NextReachChain() {
	if( const auto *cachedReachChain = reachChainsCachesStack.GetCached() ) {
		return *cachedReachChain;
	}

	Ai::ReachChainVector dummy;
	const Ai::ReachChainVector *oldReachChain = &dummy;
	if( const auto *cachedOldReachChain = reachChainsCachesStack.GetCachedValueBelowTopOfStack() ) {
		oldReachChain = cachedOldReachChain;
	}

	auto *newReachChain = new( reachChainsCachesStack.GetUnsafeBufferForCachedValue() ) Ai::ReachChainVector;
	self->ai->botRef->UpdateReachChain( *oldReachChain, newReachChain, movementState->entityPhysicsState );
	return *newReachChain;
};

inline BotEnvironmentTraceCache &BotMovementPredictionContext::EnvironmentTraceCache() {
	return environmentTestResultsStack.back();
}

typedef BotEnvironmentTraceCache::ObstacleAvoidanceResult ObstacleAvoidanceResult;

inline ObstacleAvoidanceResult BotMovementPredictionContext::TryAvoidFullHeightObstacles( float correctionFraction ) {
	// Make a modifiable copy of the intended look dir
	Vec3 intendedLookVec( this->record->botInput.IntendedLookDir() );
	auto result = EnvironmentTraceCache().TryAvoidFullHeightObstacles( this, &intendedLookVec, correctionFraction );
	if( result == ObstacleAvoidanceResult::CORRECTED ) {
		// Write the modified intended look dir back in this case
		this->record->botInput.SetIntendedLookDir( intendedLookVec );
	}
	return result;
}

inline ObstacleAvoidanceResult BotMovementPredictionContext::TryAvoidJumpableObstacles( float correctionFraction ) {
	// Make a modifiable copy of the intended look dir
	Vec3 intendedLookVec( this->record->botInput.IntendedLookDir() );
	auto result = EnvironmentTraceCache().TryAvoidJumpableObstacles( this, &intendedLookVec, correctionFraction );
	if( result == ObstacleAvoidanceResult::CORRECTED ) {
		// Write the modified intended look dir back in this case
		this->record->botInput.SetIntendedLookDir( intendedLookVec );
	}
	return result;
}

static void Intercepted_PredictedEvent( int entNum, int ev, int parm ) {
	game.edicts[entNum].ai->botRef->OnInterceptedPredictedEvent( ev, parm );
}

static void Intercepted_PMoveTouchTriggers( pmove_t *pm, vec3_t previous_origin ) {
	game.edicts[pm->playerState->playerNum + 1].ai->botRef->OnInterceptedPMoveTouchTriggers( pm, previous_origin );
}

static void Intercepted_Trace( trace_t *t, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end,
							   int ignore, int contentmask, int timeDelta ) {
	trap_CM_TransformedBoxTrace( t, start, end, mins, maxs, nullptr, contentmask, nullptr, nullptr );
}

static int Intercepted_PointContents( vec3_t p, int timeDelta ) {
	return trap_CM_TransformedPointContents( p, nullptr, nullptr, nullptr );
}

void BotMovementPredictionContext::OnInterceptedPredictedEvent( int ev, int parm ) {
	switch( ev ) {
		case EV_JUMP:
			this->frameEvents.hasJumped = true;
			break;
		case EV_DASH:
			this->frameEvents.hasDashed = true;
			break;
		case EV_WALLJUMP:
			this->frameEvents.hasWalljumped = true;
			break;
		case EV_FALL:
			this->frameEvents.hasTakenFallDamage = true;
			break;
		default: // Shut up an analyzer
			break;
	}
}

void BotMovementPredictionContext::OnInterceptedPMoveTouchTriggers( pmove_t *pm, vec3_t const previousOrigin ) {
	edict_t *ent = game.edicts + pm->playerState->POVnum;
	// update the entity with the new position
	VectorCopy( pm->playerState->pmove.origin, ent->s.origin );
	VectorCopy( pm->playerState->pmove.velocity, ent->velocity );
	VectorCopy( pm->playerState->viewangles, ent->s.angles );
	ent->viewheight = (int)pm->playerState->viewheight;
	VectorCopy( pm->mins, ent->r.mins );
	VectorCopy( pm->maxs, ent->r.maxs );

	ent->waterlevel = pm->waterlevel;
	ent->watertype = pm->watertype;
	if( pm->groundentity == -1 ) {
		ent->groundentity = NULL;
	} else {
		ent->groundentity = &game.edicts[pm->groundentity];
		ent->groundentity_linkcount = ent->groundentity->linkcount;
	}

	GClip_LinkEntity( ent );

	// expand the search bounds to include the space between the previous and current origin
	vec3_t mins, maxs;
	for( int i = 0; i < 3; i++ ) {
		if( previousOrigin[i] < pm->playerState->pmove.origin[i] ) {
			mins[i] = previousOrigin[i] + pm->maxs[i];
			if( mins[i] > pm->playerState->pmove.origin[i] + pm->mins[i] ) {
				mins[i] = pm->playerState->pmove.origin[i] + pm->mins[i];
			}
			maxs[i] = pm->playerState->pmove.origin[i] + pm->maxs[i];
		} else {
			mins[i] = pm->playerState->pmove.origin[i] + pm->mins[i];
			maxs[i] = previousOrigin[i] + pm->mins[i];
			if( maxs[i] < pm->playerState->pmove.origin[i] + pm->maxs[i] ) {
				maxs[i] = pm->playerState->pmove.origin[i] + pm->maxs[i];
			}
		}
	}

	int *triggerEnts = this->frameEvents.touchedTriggerEnts;
	this->frameEvents.numTouchedTriggers =
		GClip_AreaEdicts( mins, maxs, triggerEnts, FrameEvents::MAX_TOUCHED_TRIGGERS, AREA_TRIGGERS, 0 );

	// Save this reference on stack for faster access
	edict_t *gameEdicts = game.edicts;
	for( int i = 0, end = this->frameEvents.numTouchedTriggers; i < end; i++ ) {
		if( !ent->r.inuse ) {
			break;
		}

		edict_t *hit = gameEdicts + triggerEnts[i];
		if( !hit->r.inuse ) {
			continue;
		}

		if( !hit->classname ) {
			continue;
		}

		// Speed up a bit by inline testing of first character before Q_stricmp() call
		// TODO: Entity classname must have a hash attribute
		switch( hit->classname[0] ) {
			case 'f':
			case 'F':
				if( !Q_stricmp( "unc_plat", hit->classname + 1 ) && GClip_EntityContact( mins, maxs, hit ) ) {
					this->frameEvents.hasTouchedPlatform = true;
				}
				break;
			case 't':
			case 'T':
				if( !Q_stricmp( "rigger_push", hit->classname + 1 ) ) {
					if( GClip_EntityContact( mins, maxs, ent ) ) {
						this->frameEvents.hasTouchedJumppad = true;
					}
				} else if( !Q_stricmp( "rigger_teleport", hit->classname + 1 ) ) {
					if( GClip_EntityContact( mins, maxs, ent ) ) {
						this->frameEvents.hasTouchedTeleporter = true;
					}
				}
				break;
		}
	}
}

void BotMovementPredictionContext::SetupStackForStep() {
	PredictedMovementAction *topOfStack;
	if( topOfStackIndex > 0 ) {
		Assert( predictedMovementActions.size() );
		Assert( botMovementStatesStack.size() == predictedMovementActions.size() );
		Assert( playerStatesStack.size() == predictedMovementActions.size() );
		Assert( pendingWeaponsStack.size() == predictedMovementActions.size() );

		Assert( defaultBotInputsCachesStack.Size() == predictedMovementActions.size() );
		Assert( reachChainsCachesStack.Size() == predictedMovementActions.size() );
		Assert( mayHitWhileRunningCachesStack.Size() == predictedMovementActions.size() );
		Assert( canSafelyKeepHighSpeedCachesStack.Size() == predictedMovementActions.size() );
		Assert( environmentTestResultsStack.size() == predictedMovementActions.size() );

		// topOfStackIndex already points to a needed array element in case of rolling back
		const auto &belowTopOfStack = predictedMovementActions[topOfStackIndex - 1];
		// For case of rolling back to savepoint we have to truncate grew stacks to it
		// The only exception is rolling back to the same top of stack.
		if( this->shouldRollback ) {
			Assert( this->topOfStackIndex <= predictedMovementActions.size() );
			predictedMovementActions.truncate( topOfStackIndex );
			botMovementStatesStack.truncate( topOfStackIndex );
			playerStatesStack.truncate( topOfStackIndex );
			pendingWeaponsStack.truncate( topOfStackIndex );

			defaultBotInputsCachesStack.PopToSize( topOfStackIndex );
			reachChainsCachesStack.PopToSize( topOfStackIndex );
			mayHitWhileRunningCachesStack.PopToSize( topOfStackIndex );
			canSafelyKeepHighSpeedCachesStack.PopToSize( topOfStackIndex );
			environmentTestResultsStack.truncate( topOfStackIndex );
		} else {
			// For case of growing stack topOfStackIndex must point at the first
			// 'illegal' yet free element at top of the stack
			Assert( predictedMovementActions.size() == topOfStackIndex );
		}

		topOfStack = new( predictedMovementActions.unsafe_grow_back() )PredictedMovementAction( belowTopOfStack );

		// Push a copy of previous player state onto top of the stack
		oldPlayerState = &playerStatesStack.back();
		playerStatesStack.push_back( *oldPlayerState );
		currPlayerState = &playerStatesStack.back();
		// Push a copy of previous movement state onto top of the stack
		botMovementStatesStack.push_back( botMovementStatesStack.back() );
		pendingWeaponsStack.push_back( belowTopOfStack.record.pendingWeapon );

		oldStepMillis = belowTopOfStack.stepMillis;
		Assert( belowTopOfStack.timestamp >= level.time );
		Assert( belowTopOfStack.stepMillis > 0 );
		totalMillisAhead = (unsigned)( belowTopOfStack.timestamp - level.time ) + belowTopOfStack.stepMillis;
	} else {
		predictedMovementActions.clear();
		botMovementStatesStack.clear();
		playerStatesStack.clear();
		pendingWeaponsStack.clear();

		defaultBotInputsCachesStack.PopToSize( 0 );
		reachChainsCachesStack.PopToSize( 0 );
		mayHitWhileRunningCachesStack.PopToSize( 0 );
		canSafelyKeepHighSpeedCachesStack.PopToSize( 0 );
		environmentTestResultsStack.clear();

		topOfStack = new( predictedMovementActions.unsafe_grow_back() )PredictedMovementAction;
		// Push the actual bot player state onto top of the stack
		oldPlayerState = &self->r.client->ps;
		playerStatesStack.push_back( *oldPlayerState );
		currPlayerState = &playerStatesStack.back();
		// Push the actual bot movement state onto top of the stack
		botMovementStatesStack.push_back( self->ai->botRef->movementState );
		pendingWeaponsStack.push_back( (signed char)oldPlayerState->stats[STAT_PENDING_WEAPON] );

		oldStepMillis = game.frametime;
		totalMillisAhead = 0;
	}
	// Check whether topOfStackIndex really points at the last element of the array
	Assert( predictedMovementActions.size() == topOfStackIndex + 1 );

	movementState = &botMovementStatesStack.back();
	// Provide a predicted movement state for Ai base class
	self->ai->botRef->entityPhysicsState = &movementState->entityPhysicsState;

	// Set the current action record
	this->record = &topOfStack->record;
	this->record->pendingWeapon = pendingWeaponsStack.back();

	Assert( reachChainsCachesStack.Size() + 1 == predictedMovementActions.size() );
	Assert( mayHitWhileRunningCachesStack.Size() + 1 == predictedMovementActions.size() );
	// Check caches size, a cache size must match the stack size after addition of a single placeholder element.
	Assert( defaultBotInputsCachesStack.Size() + 1 == predictedMovementActions.size() );
	// Then put placeholders for non-cached yet values onto top of caches stack
	defaultBotInputsCachesStack.PushDummyNonCachedValue();
	// The different method is used (there is no copy/move constructors for the template type)
	reachChainsCachesStack.UnsafeGrowForNonCachedValue();
	mayHitWhileRunningCachesStack.PushDummyNonCachedValue();
	canSafelyKeepHighSpeedCachesStack.PushDummyNonCachedValue();
	new ( environmentTestResultsStack.unsafe_grow_back() )BotEnvironmentTraceCache;

	this->shouldRollback = false;

	// Save a movement state BEFORE movement step
	topOfStack->entityPhysicsState = this->movementState->entityPhysicsState;
	topOfStack->movementStatesMask = this->movementState->GetContainedStatesMask();
}

inline BotBaseMovementAction *BotMovementPredictionContext::SuggestAnyAction() {
	if( BotBaseMovementAction *action = this->SuggestSuitableAction() ) {
		return action;
	}

	// If no action has been suggested, use a default/dummy one.
	// We have to check the combat action since it might be disabled due to planning stack overflow.
	if( self->ai->botRef->GetSelectedEnemies().AreValid() && self->ai->botRef->ShouldAttack() ) {
		if( !self->ai->botRef->combatDodgeSemiRandomlyToTargetMovementAction.IsDisabledForPlanning() ) {
			return &self->ai->botRef->combatDodgeSemiRandomlyToTargetMovementAction;
		}
	}

	return &self->ai->botRef->dummyMovementAction;
}

BotBaseMovementAction *BotMovementPredictionContext::SuggestSuitableAction() {
	Assert( !this->actionSuggestedByAction );

	const auto &entityPhysicsState = this->movementState->entityPhysicsState;

	if( entityPhysicsState.waterLevel > 1 ) {
		return &self->ai->botRef->swimMovementAction;
	}

	if( movementState->jumppadMovementState.hasTouchedJumppad ) {
		if( movementState->jumppadMovementState.hasEnteredJumppad ) {
			if( movementState->flyUntilLandingMovementState.IsActive() ) {
				if( movementState->flyUntilLandingMovementState.CheckForLanding( this ) ) {
					return &self->ai->botRef->landOnSavedAreasSetMovementAction;
				}

				return &self->ai->botRef->flyUntilLandingMovementAction;
			}
			// Fly until landing movement state has been deactivate,
			// switch to bunnying (and, implicitly, to a dummy action if it fails)
			return &self->ai->botRef->walkCarefullyMovementAction;
		}
		return &self->ai->botRef->handleTriggeredJumppadMovementAction;
	}

	if( const edict_t *groundEntity = entityPhysicsState.GroundEntity() ) {
		if( groundEntity->use == Use_Plat ) {
			// (prevent blocking if touching platform but not actually triggering it like @ wdm1 GA)
			const auto &mins = groundEntity->r.absmin;
			const auto &maxs = groundEntity->r.absmax;
			if( mins[0] <= entityPhysicsState.Origin()[0] && maxs[0] >= entityPhysicsState.Origin()[0] ) {
				if( mins[1] <= entityPhysicsState.Origin()[1] && maxs[1] >= entityPhysicsState.Origin()[1] ) {
					return &self->ai->botRef->ridePlatformMovementAction;
				}
			}
		}
	}

	if( movementState->campingSpotState.IsActive() ) {
		return &self->ai->botRef->campASpotMovementAction;
	}

	return &self->ai->botRef->walkCarefullyMovementAction;
}

inline void BotMovementPredictionContext::SaveActionOnStack( BotBaseMovementAction *action ) {
	auto *topOfStack = &this->predictedMovementActions[this->topOfStackIndex];
	// This was a source of an annoying bug! movement state has been modified during a prediction step!
	// We expect that record state is a saved state BEFORE the step!
	//topOfStack->entityPhysicsState = this->movementState->entityPhysicsState;
	topOfStack->action = action;
	// Make sure the angles can always be modified for input interpolation or aiming
	topOfStack->record.botInput.hasAlreadyComputedAngles = false;
	topOfStack->timestamp = level.time + this->totalMillisAhead;
	// Check the value for sanity, huge values are a product of negative values wrapping in unsigned context
	Assert( this->predictionStepMillis < 100 );
	Assert( this->predictionStepMillis % 16 == 0 );
	topOfStack->stepMillis = this->predictionStepMillis;
	this->topOfStackIndex++;
}

inline void BotMovementPredictionContext::MarkSavepoint( BotBaseMovementAction *markedBy, unsigned frameIndex ) {
	Assert( !this->cannotApplyAction );
	Assert( !this->shouldRollback );

	Assert( frameIndex == this->topOfStackIndex || frameIndex == this->topOfStackIndex + 1 );
	this->savepointTopOfStackIndex = frameIndex;
	Debug( "%s has marked frame %d as a savepoint\n", markedBy->Name(), frameIndex );
}

inline void BotMovementPredictionContext::RollbackToSavepoint() {
	Assert( !this->isCompleted );
	Assert( this->shouldRollback );
	Assert( this->cannotApplyAction );

	constexpr const char *format = "Rolling back to savepoint frame %d from ToS frame %d\n";
	Debug( format, this->savepointTopOfStackIndex, this->topOfStackIndex );
	Assert( this->topOfStackIndex >= this->savepointTopOfStackIndex );
	this->topOfStackIndex = this->savepointTopOfStackIndex;
}

inline void BotMovementPredictionContext::SetPendingWeapon( int weapon ) {
	Assert( weapon >= WEAP_NONE && weapon < WEAP_TOTAL );
	record->pendingWeapon = ( decltype( record->pendingWeapon ) )weapon;
	pendingWeaponsStack.back() = record->pendingWeapon;
}

inline void BotMovementPredictionContext::SaveSuggestedActionForNextFrame( BotBaseMovementAction *action ) {
	//Assert(!this->actionSuggestedByAction);
	this->actionSuggestedByAction = action;
}

inline unsigned BotMovementPredictionContext::MillisAheadForFrameStart( unsigned frameIndex ) const {
	Assert( frameIndex <= topOfStackIndex );
	if( frameIndex < topOfStackIndex ) {
		return (unsigned)( predictedMovementActions[frameIndex].timestamp - level.time );
	}
	return totalMillisAhead;
}

bool BotMovementPredictionContext::NextPredictionStep() {
	SetupStackForStep();

	// Reset prediction step millis time.
	// Actions might set their custom step value (otherwise it will be set to a default one).
	this->predictionStepMillis = 0;
#ifdef CHECK_ACTION_SUGGESTION_LOOPS
	Assert( self->ai->botRef->movementActions.size() < 32 );
	uint32_t testedActionsMask = 0;
	StaticVector<BotBaseMovementAction *, 32> testedActionsList;
#endif

	// Get an initial suggested a-priori action
	BotBaseMovementAction *action;
	if( this->actionSuggestedByAction ) {
		action = this->actionSuggestedByAction;
		this->actionSuggestedByAction = nullptr;
	} else {
		action = this->SuggestSuitableAction();
	}

#ifdef CHECK_ACTION_SUGGESTION_LOOPS
	testedActionsMask |= ( 1 << action->ActionNum() );
	testedActionsList.push_back( action );
#endif

	this->sequenceStopReason = UNSPECIFIED;
	for(;; ) {
		this->cannotApplyAction = false;
		// Prevent reusing record from the switched on the current frame action
		this->record->Clear();
		if( this->activeAction != action ) {
			// If there was an active previous action, stop its application sequence.
			if( this->activeAction ) {
				unsigned stoppedAtFrameIndex = topOfStackIndex;

				// Never pass the UNSPECIFIED reason to the OnApplicationSequenceStopped() call
				if( sequenceStopReason == UNSPECIFIED ) {
					sequenceStopReason = SWITCHED;
				}

				this->activeAction->OnApplicationSequenceStopped( this, sequenceStopReason, stoppedAtFrameIndex );
			}

			sequenceStopReason = UNSPECIFIED;

			this->activeAction = action;
			// Start the action application sequence
			this->activeAction->OnApplicationSequenceStarted( this );
		}

		Debug( "About to call action->PlanPredictionStep() for %s at ToS frame %d\n", action->Name(), topOfStackIndex );
		action->PlanPredictionStep( this );
		// Check for rolling back necessity (an action application chain has lead to an illegal state)
		if( this->shouldRollback ) {
			// Stop an action application sequence manually with a failure.
			this->activeAction->OnApplicationSequenceStopped( this, BotBaseMovementAction::FAILED, (unsigned)-1 );
			// An action can be suggested again after rolling back on the next prediction step.
			// Force calling action->OnApplicationSequenceStarted() on the next prediction step.
			this->activeAction = nullptr;
			Debug( "Prediction step failed after action->PlanPredictionStep() call for %s\n", action->Name() );
			this->RollbackToSavepoint();
			// Continue planning by returning true (the stack will be restored to a savepoint index)
			return true;
		}

		if( this->cannotApplyAction ) {
			// If current action suggested an alternative, use it
			// Otherwise use the generic suggestion algorithm
			if( this->actionSuggestedByAction ) {
				Debug( "Cannot apply %s, but it has suggested %s\n", action->Name(), this->actionSuggestedByAction->Name() );
				action = this->actionSuggestedByAction;
				this->actionSuggestedByAction = nullptr;
			} else {
				auto *rejectedAction = action;
				action = this->SuggestAnyAction();
				Debug( "Cannot apply %s, using %s suggested by SuggestSuitableAction()\n", rejectedAction->Name(), action->Name() );
			}

#ifdef CHECK_ACTION_SUGGESTION_LOOPS
			if( testedActionsMask & ( 1 << action->ActionNum() ) ) {
				Debug( "List of actions suggested (and tested) this frame #%d:\n", this->topOfStackIndex );
				for( unsigned i = 0; i < testedActionsList.size(); ++i ) {
					if( Q_stricmp( testedActionsList[i]->Name(), action->Name() ) ) {
						Debug( "  %02d: %s\n", i, testedActionsList[i]->Name() );
					} else {
						Debug( ">>%02d: %s\n", i, testedActionsList[i]->Name() );
					}
				}

				AI_FailWith( __FUNCTION__, "An infinite action application loop has been detected\n" );
			}
			testedActionsMask |= ( 1 << action->ActionNum() );
			testedActionsList.push_back( action );
#endif
			// Test next action. Action switch will be handled by the logic above before calling action->PlanPredictionStep().
			continue;
		}

		// Movement prediction is completed
		if( this->isCompleted ) {
			constexpr const char *format = "Movement prediction is completed on %s, ToS frame %d, %d millis ahead\n";
			Debug( format, action->Name(), this->topOfStackIndex, this->totalMillisAhead );
			// Stop an action application sequence manually with a success.
			action->OnApplicationSequenceStopped( this, BotBaseMovementAction::SUCCEEDED, this->topOfStackIndex );
			// Save the predicted movement action
			this->SaveActionOnStack( action );
			// Stop planning by returning false
			return false;
		}

		// An action can be applied, stop testing suitable actions
		break;
	}

	Assert( action == this->activeAction );

	// If prediction step millis time has not been set, set it to a default value
	if( !this->predictionStepMillis ) {
		this->predictionStepMillis = 48;
	}

	NextMovementStep();

	action->CheckPredictionStepResults( this );
	// If results check has been passed
	if( !this->shouldRollback ) {
		// If movement planning is completed, there is no need to do a next step
		if( this->isCompleted ) {
			constexpr const char *format = "Movement prediction is completed on %s, ToS frame %d, %d millis ahead\n";
			Debug( format, action->Name(), this->topOfStackIndex, this->totalMillisAhead );
			SaveActionOnStack( action );
			// Stop action application sequence manually with a success.
			// Prevent duplicated OnApplicationSequenceStopped() call
			// (it might have been done in action->CheckPredictionStepResults() for this->activeAction)
			if( this->activeAction ) {
				this->activeAction->OnApplicationSequenceStopped( this, BotBaseMovementAction::SUCCEEDED, topOfStackIndex );
			}
			// Stop planning by returning false
			return false;
		}

		// Check whether next prediction step is possible
		if( this->CanGrowStackForNextStep() ) {
			SaveActionOnStack( action );
			// Continue planning by returning true
			return true;
		}

		// Disable this action for further planning (it has lead to stack overflow)
		action->isDisabledForPlanning = true;
		Debug( "Stack overflow on action %s, this action will be disabled for further planning\n", action->Name() );
		this->SetPendingRollback();
	}

	constexpr const char *format = "Prediction step failed for %s after calling action->CheckPredictionStepResults()\n";
	Debug( format, action->Name() );

	// An active action might have been already reset in action->CheckPredictionStepResults()
	if( this->activeAction ) {
		// Stop action application sequence with a failure manually.
		this->activeAction->OnApplicationSequenceStopped( this, BotBaseMovementAction::FAILED, (unsigned)-1 );
	}
	// An action can be suggested again after rolling back on the next prediction step.
	// Force calling action->OnApplicationSequenceStarted() on the next prediction step.
	this->activeAction = nullptr;

	this->RollbackToSavepoint();
	// Continue planning by returning true
	return true;
}

void BotMovementPredictionContext::BuildPlan() {
	for( auto *movementAction: self->ai->botRef->movementActions )
		movementAction->BeforePlanning();

	// Intercept these calls implicitly performed by PMove()
	const auto general_PMoveTouchTriggers = module_PMoveTouchTriggers;
	const auto general_PredictedEvent = module_PredictedEvent;

	module_PMoveTouchTriggers = &Intercepted_PMoveTouchTriggers;
	module_PredictedEvent = &Intercepted_PredictedEvent;

	// The entity state might be modified by Intercepted_PMoveTouchTriggers(), so we have to save it
	const Vec3 origin( self->s.origin );
	const Vec3 velocity( self->velocity );
	const Vec3 angles( self->s.angles );
	const int viewHeight = self->viewheight;
	const Vec3 mins( self->r.mins );
	const Vec3 maxs( self->r.maxs );
	const int waterLevel = self->waterlevel;
	const int waterType = self->watertype;
	edict_t *const groundEntity = self->groundentity;
	const int groundEntityLinkCount = self->groundentity_linkcount;

	auto savedPlayerState = self->r.client->ps;
	auto savedPMove = self->r.client->old_pmove;

	Assert( self->ai->botRef->entityPhysicsState == &self->ai->botRef->movementState.entityPhysicsState );
	// Save current entity physics state (it will be modified even for a single prediction step)
	const AiEntityPhysicsState currEntityPhysicsState = self->ai->botRef->movementState.entityPhysicsState;

	// Remember to reset these values before each planning session
	this->totalMillisAhead = 0;
	this->savepointTopOfStackIndex = 0;
	this->topOfStackIndex = 0;
	this->activeAction = nullptr;
	this->actionSuggestedByAction = nullptr;
	this->sequenceStopReason = UNSPECIFIED;
	this->isCompleted = false;
	this->shouldRollback = false;
	for(;; ) {
		if( !NextPredictionStep() ) {
			break;
		}
	}

	// The entity might be linked for some predicted state by Intercepted_PMoveTouchTriggers()
	GClip_UnlinkEntity( self );

	// Restore entity state
	origin.CopyTo( self->s.origin );
	velocity.CopyTo( self->velocity );
	angles.CopyTo( self->s.angles );
	self->viewheight = viewHeight;
	mins.CopyTo( self->r.mins );
	maxs.CopyTo( self->r.maxs );
	self->waterlevel = waterLevel;
	self->watertype = waterType;
	self->groundentity = groundEntity;
	self->groundentity_linkcount = groundEntityLinkCount;

	self->r.client->ps = savedPlayerState;
	self->r.client->old_pmove = savedPMove;

	// Set first predicted movement state as the current bot movement state
	self->ai->botRef->movementState = botMovementStatesStack[0];
	// Even the first predicted movement state usually has modified physics state, restore it to a saved value
	self->ai->botRef->movementState.entityPhysicsState = currEntityPhysicsState;
	// Restore the current entity physics state reference in Ai subclass
	self->ai->botRef->entityPhysicsState = &self->ai->botRef->movementState.entityPhysicsState;
	// These assertions helped to find an annoying bug during development
	Assert( VectorCompare( self->s.origin, self->ai->botRef->entityPhysicsState->Origin() ) );
	Assert( VectorCompare( self->velocity, self->ai->botRef->entityPhysicsState->Velocity() ) );

	module_PMoveTouchTriggers = general_PMoveTouchTriggers;
	module_PredictedEvent = general_PredictedEvent;

	for( auto *movementAction: self->ai->botRef->movementActions )
		movementAction->AfterPlanning();
}

void BotMovementPredictionContext::NextMovementStep() {
	auto *botInput = &this->record->botInput;
	auto *entityPhysicsState = &movementState->entityPhysicsState;

	// Make sure we're modify botInput/entityPhysicsState before copying to ucmd

	// Simulate Bot::Think();
	self->ai->botRef->ApplyPendingTurnToLookAtPoint( botInput, this );
	// Simulate Bot::MovementFrame();
	this->activeAction->ExecActionRecord( this->record, botInput, this );
	// Simulate Bot::Think();
	self->ai->botRef->ApplyInput( botInput, this );

	// ExecActionRecord() might fail or complete the planning execution early.
	// Do not call PMove() in these cases
	if( this->cannotApplyAction || this->isCompleted ) {
		return;
	}

	// Prepare for PMove()
	currPlayerState->POVnum = (unsigned)ENTNUM( self );
	currPlayerState->playerNum = (unsigned)PLAYERNUM( self );

	VectorCopy( entityPhysicsState->Origin(), currPlayerState->pmove.origin );
	VectorCopy( entityPhysicsState->Velocity(), currPlayerState->pmove.velocity );
	Vec3 angles( entityPhysicsState->Angles() );
	angles.CopyTo( currPlayerState->viewangles );

	currPlayerState->pmove.gravity = (int)level.gravity;
	currPlayerState->pmove.pm_type = PM_NORMAL;

	pmove_t pm;
	// TODO: Eliminate this call?
	memset( &pm, 0, sizeof( pmove_t ) );

	pm.playerState = currPlayerState;
	botInput->CopyToUcmd( &pm.cmd );

	for( int i = 0; i < 3; i++ )
		pm.cmd.angles[i] = (short)ANGLE2SHORT( angles.Data()[i] ) - currPlayerState->pmove.delta_angles[i];

	VectorSet( currPlayerState->pmove.delta_angles, 0, 0, 0 );

	// Check for unsigned value wrapping
	Assert( this->predictionStepMillis && this->predictionStepMillis < 100 );
	Assert( this->predictionStepMillis % 16 == 0 );
	pm.cmd.msec = (uint8_t)this->predictionStepMillis;
	pm.cmd.serverTimeStamp = game.serverTime + this->totalMillisAhead;

	this->frameEvents.Clear();

	// The naive solution of supplying a dummy trace function
	// (that yields a zeroed output with fraction = 1) does not work.
	// An actual logic tied to this flag has to be added in Pmove() for each module_Trace() call.
	pm.skipCollision = EnvironmentTraceCache().CanSkipPMoveCollision( this );

	// We currently test collisions only against a solid world on each movement step and the corresponding PMove() call.
	// Touching trigger entities is handled by Intercepted_PMoveTouchTriggers(), also we use AAS sampling for it.
	// Actions that involve touching trigger entities currently are never predicted ahead.
	// If an action really needs to test against entities, a corresponding prediction step flag
	// should be added and this interception of the module_Trace() should be skipped if the flag is set.

	// Save the G_GS_Trace() pointer
	auto oldModuleTrace = module_Trace;
	module_Trace = Intercepted_Trace;

	// Do not test entities contents for same reasons
	// Save the G_PointContents4D() pointer
	auto oldModulePointContents = module_PointContents;
	module_PointContents = Intercepted_PointContents;

	Pmove( &pm );

	// Restore the G_GS_Trace() pointer
	module_Trace = oldModuleTrace;
	// Restore the G_PointContents4D() pointer
	module_PointContents = oldModulePointContents;

	// Update the entity physics state that is going to be used in the next prediction frame
	entityPhysicsState->UpdateFromPMove( &pm );
	// Update the entire movement state that is going to be used in the next prediction frame
	this->movementState->Frame( this->predictionStepMillis );
	this->movementState->TryDeactivateContainedStates( self, this );
}

void BotMovementPredictionContext::Debug( const char *format, ... ) const {
#ifdef ENABLE_MOVEMENT_DEBUG_OUTPUT
	char tag[64];
	Q_snprintfz( tag, 64, "^6MovementPredictionContext(%s)", Nick( self ) );

	va_list va;
	va_start( va, format );
	AI_Debugv( tag, format, va );
	va_end( va );
#endif
}

inline void BotMovementPredictionContext::Assert( bool condition, const char *message ) const {
#ifdef ENABLE_MOVEMENT_ASSERTIONS
	if( !condition ) {
		if( message ) {
			AI_FailWith( "BotMovementPredictionContext::Assert()", "%s\n", message );
		} else {
			AI_FailWith( "BotMovementPredictionContext::Assert()", "An assertion has failed\n" );
		}
	}
#endif
}

constexpr float Z_NO_BEND_SCALE = 0.5f;

void BotMovementPredictionContext::SetDefaultBotInput() {
	// Check for cached value first
	if( const BotInput *cachedDefaultBotInput = defaultBotInputsCachesStack.GetCached() ) {
		this->record->botInput = *cachedDefaultBotInput;
		return;
	}

	const auto &entityPhysicsState = movementState->entityPhysicsState;
	auto *botInput = &this->record->botInput;

	botInput->ClearMovementDirections();
	botInput->SetSpecialButton( false );
	botInput->SetWalkButton( false );
	botInput->isUcmdSet = true;

	// If angles are already set (e.g. by pending look at point), do not try to set angles for following AAS reach. chain
	if( botInput->hasAlreadyComputedAngles ) {
		// Save cached value and return
		defaultBotInputsCachesStack.SetCachedValue( *botInput );
		return;
	}

	botInput->isLookDirSet = true;
	botInput->canOverrideLookVec = false;
	botInput->canOverridePitch = true;
	botInput->canOverrideUcmd = false;

	const int navTargetAasAreaNum = NavTargetAasAreaNum();
	const int currAasAreaNum = CurrAasAreaNum();
	// If a current area and a nav target area are defined
	if( currAasAreaNum && navTargetAasAreaNum ) {
		// If bot is not in nav target area
		if( currAasAreaNum != navTargetAasAreaNum ) {
			const int nextReachNum = this->NextReachNum();
			if( nextReachNum ) {
				const auto &nextReach = AiAasWorld::Instance()->Reachabilities()[nextReachNum];
				if( DistanceSquared( entityPhysicsState.Origin(), nextReach.start ) < 12 * 12 ) {
					Vec3 intendedLookVec( nextReach.end );
					intendedLookVec -= nextReach.start;
					intendedLookVec.NormalizeFast();
					intendedLookVec *= 16.0f;
					intendedLookVec += nextReach.start;
					intendedLookVec -= entityPhysicsState.Origin();
					if( entityPhysicsState.GroundEntity() ) {
						intendedLookVec.Z() = 0;
					}
					botInput->SetIntendedLookDir( intendedLookVec );
				} else {
					Vec3 intendedLookVec3( nextReach.start );
					intendedLookVec3 -= entityPhysicsState.Origin();
					botInput->SetIntendedLookDir( intendedLookVec3 );
				}
				// Save a cached value and return
				defaultBotInputsCachesStack.SetCachedValue( *botInput );
				return;
			}
		} else {
			// Look at the nav target
			Vec3 intendedLookVec( NavTargetOrigin() );
			intendedLookVec -= entityPhysicsState.Origin();
			botInput->SetIntendedLookDir( intendedLookVec );
			// Save a cached value and return
			defaultBotInputsCachesStack.SetCachedValue( *botInput );
			return;
		}
	}

	// A valid reachability chain does not exist, use dummy relaxed movement
	if( entityPhysicsState.Speed() > 1 ) {
		// Follow the existing velocity direction
		botInput->SetIntendedLookDir( entityPhysicsState.Velocity() );
	} else {
		// The existing velocity is too low to extract a direction from it, keep looking in the same direction
		botInput->SetAlreadyComputedAngles( entityPhysicsState.Angles() );
	}

	// Allow overriding look angles for aiming (since they are set to dummy ones)
	botInput->canOverrideLookVec = true;
	// Save a cached value and return
	defaultBotInputsCachesStack.SetCachedValue( *botInput );
}

void BotBaseMovementAction::RegisterSelf() {
	this->self = bot->self;
	this->actionNum = bot->movementActions.size();
	bot->movementActions.push_back( this );
}

inline BotBaseMovementAction &BotBaseMovementAction::DummyAction() {
	// We have to check the combat action since it might be disabled due to planning stack overflow.
	if( self->ai->botRef->ShouldKeepXhairOnEnemy() && self->ai->botRef->GetSelectedEnemies().AreValid() ) {
		if( !self->ai->botRef->combatDodgeSemiRandomlyToTargetMovementAction.IsDisabledForPlanning() ) {
			return self->ai->botRef->combatDodgeSemiRandomlyToTargetMovementAction;
		}
	}

	return self->ai->botRef->dummyMovementAction;
}

inline BotBaseMovementAction &BotBaseMovementAction::DefaultWalkAction() {
	return self->ai->botRef->walkCarefullyMovementAction;
}
inline BotBaseMovementAction &BotBaseMovementAction::DefaultBunnyAction() {
	return self->ai->botRef->bunnyInterpolatingReachChainMovementAction;
}
inline BotBaseMovementAction &BotBaseMovementAction::FallbackBunnyAction() {
	return self->ai->botRef->walkOrSlideInterpolatingReachChainMovementAction;
}
inline BotFlyUntilLandingMovementAction &BotBaseMovementAction::FlyUntilLandingAction() {
	return self->ai->botRef->flyUntilLandingMovementAction;
}
inline BotLandOnSavedAreasMovementAction &BotBaseMovementAction::LandOnSavedAreasAction() {
	return self->ai->botRef->landOnSavedAreasSetMovementAction;
}

void BotBaseMovementAction::Debug( const char *format, ... ) const {
#ifdef ENABLE_MOVEMENT_DEBUG_OUTPUT
	char tag[128];
	Q_snprintfz( tag, 128, "^5%s(%s)", this->Name(), Nick( self ) );

	va_list va;
	va_start( va, format );
	AI_Debugv( tag, format, va );
	va_end( va );
#endif
}

inline void BotBaseMovementAction::Assert( bool condition, const char *message ) const {
#ifdef ENABLE_MOVEMENT_ASSERTIONS
	if( !condition ) {
		if( message ) {
			AI_FailWith("BotBaseMovementAction::Assert()", "An assertion has failed: %s\n", message );
		} else {
			AI_FailWith("BotBaseMovementAction::Assert()", "An assertion has failed\n");
		}
	}
#endif
}

void BotBaseMovementAction::ExecActionRecord( const BotMovementActionRecord *record,
											  BotInput *inputWillBeUsed,
											  BotMovementPredictionContext *context ) {
	Assert( inputWillBeUsed );
	*inputWillBeUsed = record->botInput;

	if( context ) {
		if( record->hasModifiedVelocity ) {
			context->movementState->entityPhysicsState.SetVelocity( record->ModifiedVelocity() );
		}

		// Pending weapon must have been set in PlanPredictionStep()
		// (in planning context it is defined by record->pendingWeapon, pendingWeaponsStack.back()).
		if( record->pendingWeapon >= WEAP_NONE ) {
			//Assert(record->pendingWeapon == context->PendingWeapon());
		}
		return;
	}

	if( record->hasModifiedVelocity ) {
		record->ModifiedVelocity().CopyTo( self->velocity );
	}

	if( record->pendingWeapon != -1 ) {
		self->r.client->ps.stats[STAT_PENDING_WEAPON] = record->pendingWeapon;
	}
}

void BotBaseMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	// These flags might be set by ExecActionRecord(). Skip checks in this case.
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;
	const auto &oldEntityPhysicsState = context->PhysicsStateBeforeStep();

	// This is a default basic test that suits many relatively simple actions
	// Forbid movement from regular contents to "bad" contents
	// (if old contents are "bad" too, a movement step is considered legal)
	// Note: we do not check any points between these two ones,
	// and this can lead to missing "bad contents" for large prediction time step

	constexpr auto badContents = CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_DONOTENTER;
	if( newEntityPhysicsState.waterType & badContents ) {
		if( !( oldEntityPhysicsState.waterType & badContents ) ) {
			if( badContents & CONTENTS_LAVA ) {
				Debug( "A prediction step has lead to entering CONTENTS_LAVA point\n" );
			} else if( badContents & CONTENTS_SLIME ) {
				Debug( "A prediction step has lead to entering CONTENTS_SLIME point\n" );
			} else if( badContents & CONTENTS_DONOTENTER ) {
				Debug( "A prediction step has lead to entering CONTENTS_DONOTENTER point\n" );
			}

			context->SetPendingRollback();
			return;
		}
	}

	if( stopPredictionOnEnteringWater && newEntityPhysicsState.waterLevel > 1 ) {
		Assert( this != &self->ai->botRef->swimMovementAction );
		Debug( "A prediction step has lead to entering water, should stop planning\n" );
		context->isCompleted = true;
		return;
	}

	// Check AAS areas in the same way
	int oldAasAreaNum = oldEntityPhysicsState.CurrAasAreaNum();
	int newAasAreaNum = newEntityPhysicsState.CurrAasAreaNum();
	if( newAasAreaNum != oldAasAreaNum ) {
		const auto *aasAreaSettings = AiAasWorld::Instance()->AreaSettings();
		const auto &currAreaSettings = aasAreaSettings[newAasAreaNum];
		const auto &prevAreaSettings = aasAreaSettings[oldAasAreaNum];

		if( currAreaSettings.areaflags & AREA_DISABLED ) {
			if( !( prevAreaSettings.areaflags & AREA_DISABLED ) ) {
				Debug( "A prediction step has lead to entering an AREA_DISABLED AAS area\n" );
				context->SetPendingRollback();
				return;
			}
		}

		if( currAreaSettings.contents & AREACONTENTS_DONOTENTER ) {
			if( !( prevAreaSettings.contents & AREACONTENTS_DONOTENTER ) ) {
				Debug( "A prediction step has lead to entering an AREACONTENTS_DONOTENTER AAS area\n" );
				context->SetPendingRollback();
				return;
			}
		}
	}

	if( this->stopPredictionOnTouchingJumppad && context->frameEvents.hasTouchedJumppad ) {
		Debug( "A prediction step has lead to touching a jumppad, should stop planning\n" );
		context->isCompleted = true;
		return;
	}
	if( this->stopPredictionOnTouchingTeleporter && context->frameEvents.hasTouchedTeleporter ) {
		Debug( "A prediction step has lead to touching a teleporter, should stop planning\n" );
		context->isCompleted = true;
		return;
	}
	if( this->stopPredictionOnTouchingPlatform && context->frameEvents.hasTouchedPlatform ) {
		Debug( "A prediction step has lead to touching a platform, should stop planning\n" );
		context->isCompleted = true;
		return;
	}

	if( this->stopPredictionOnTouchingNavEntity ) {
		const edict_t *gameEdicts = game.edicts;
		const int *ents = context->frameEvents.touchedTriggerEnts;
		for( int i = 0, end = context->frameEvents.numTouchedTriggers; i < end; ++i ) {
			const edict_t *ent = gameEdicts + ents[i];
			if( self->ai->botRef->IsNavTargetBasedOnEntity( ent ) ) {
				const char *entName = ent->classname ? ent->classname : "???";
				Debug( "A prediction step has lead to touching a nav entity %s, should stop planning\n", entName );
				context->isCompleted = true;
				return;
			}
		}
	}
}

void BotBaseMovementAction::BeforePlanning() {
	isDisabledForPlanning = false;
	sequenceStartFrameIndex = std::numeric_limits<unsigned>::max();
	sequenceEndFrameIndex = std::numeric_limits<unsigned>::max();
}

void BotBaseMovementAction::OnApplicationSequenceStarted( BotMovementPredictionContext *context ) {
	Debug( "OnApplicationSequenceStarted(context): context->topOfStackIndex=%d\n", context->topOfStackIndex );

	constexpr auto invalidValue = std::numeric_limits<unsigned>::max();
	Assert( sequenceStartFrameIndex == invalidValue );
	sequenceEndFrameIndex = invalidValue;
	sequenceStartFrameIndex = context->topOfStackIndex;
	originAtSequenceStart.Set( context->movementState->entityPhysicsState.Origin() );
}

void BotBaseMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
														  SequenceStopReason reason,
														  unsigned stoppedAtFrameIndex ) {
	constexpr auto invalidValue = std::numeric_limits<unsigned>::max();
	Assert( sequenceStartFrameIndex != invalidValue );
	Assert( sequenceEndFrameIndex == invalidValue );
	Assert( sequenceStartFrameIndex <= stoppedAtFrameIndex );
	sequenceStartFrameIndex = invalidValue;
	sequenceEndFrameIndex = stoppedAtFrameIndex;

	const char *format = "OnApplicationSequenceStopped(context, %s, %d): context->topOfStackIndex=%d\n";
	switch( reason ) {
		case UNSPECIFIED:
			// Should not be reached
			Assert( false );
			break;
		case SUCCEEDED:
			Debug( format, "succeeded", stoppedAtFrameIndex, context->topOfStackIndex );
			context->MarkSavepoint( this, stoppedAtFrameIndex + 1 );
			break;
		case SWITCHED:
			Debug( format, "switched", stoppedAtFrameIndex, context->topOfStackIndex );
			context->MarkSavepoint( this, stoppedAtFrameIndex );
			break;
		case DISABLED:
			Debug( format, "disabled", stoppedAtFrameIndex, context->topOfStackIndex );
			context->MarkSavepoint( this, stoppedAtFrameIndex );
			break;
		case FAILED:
			Debug( format, "failed", stoppedAtFrameIndex, context->topOfStackIndex );
			break;
	}
}

inline unsigned BotBaseMovementAction::SequenceDuration( const BotMovementPredictionContext *context ) const {
	unsigned millisAheadAtSequenceStart = context->MillisAheadForFrameStart( sequenceStartFrameIndex );
	// TODO: Ensure that the method gets called only after prediction step in some way
	// (We need a valid and actual prediction step millis)
	Assert( context->predictionStepMillis );
	Assert( context->predictionStepMillis % 16 == 0 );
	Assert( context->totalMillisAhead + context->predictionStepMillis > millisAheadAtSequenceStart );
	return context->totalMillisAhead + context->predictionStepMillis - millisAheadAtSequenceStart;
}

// Height threshold should be set according to used time step
// (we might miss crouch sliding activation if its low and the time step is large)
inline bool ShouldPrepareForCrouchSliding( BotMovementPredictionContext *context, float heightThreshold = 12.0f ) {
	if( !(context->currPlayerState->pmove.stats[PM_STAT_FEATURES ] & PMFEAT_CROUCHSLIDING ) ) {
		return false;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.GroundEntity() ) {
		return false;
	}

	if( entityPhysicsState.Velocity()[2] > 0 ) {
		return false;
	}

	if( entityPhysicsState.HeightOverGround() > heightThreshold ) {
		return false;
	}

	if( entityPhysicsState.Speed() < context->GetRunSpeed() ) {
		return false;
	}

	return true;
}

inline float Distance2DSquared( const vec3_t a, const vec3_t b ) {
	float dx = a[0] - b[0];
	float dy = a[1] - b[1];
	return dx * dx + dy * dy;
}

#ifndef SQUARE
#define SQUARE( x ) ( ( x ) * ( x ) )
#endif

bool BotDummyMovementAction::HandleWalkOrFallReachability( BotMovementPredictionContext *context,
														   const aas_reachability_t &reach,
														   float zNoBendScale ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	float squareDistance2D = Distance2DSquared( entityPhysicsState.Origin(), reach.start );
	if( squareDistance2D > SQUARE( 20.0f ) ) {
		return false;
	}

	Vec3 reachDir( reach.start );
	reachDir -= entityPhysicsState.Origin();
	reachDir.NormalizeFast();

	if( reachDir.Z() < 0.5f ) {
		reachDir.Z() *= zNoBendScale;
		reachDir.NormalizeFast();
	}

	context->record->botInput.SetIntendedLookDir( reachDir, true );
	context->record->botInput.SetForwardMovement( 1 );
	if( reachDir.Dot( entityPhysicsState.ForwardDir() ) < 0.7f ) {
		context->record->botInput.SetWalkButton( true );
	} else {
		if( !entityPhysicsState.GroundEntity() ) {
			context->record->botInput.SetForwardMovement( 0 );
		} else if( squareDistance2D < SQUARE( 12.0f ) ) {
			context->record->botInput.SetWalkButton( true );
		}
	}

	return true;
}

bool BotDummyMovementAction::HandleLongJumpReachability( BotMovementPredictionContext *context,
														 const aas_reachability_t &reach ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	float squareDistance2D = Distance2DSquared( entityPhysicsState.Origin(), reach.start );
	if( squareDistance2D > SQUARE( 24.0f ) ) {
		return false;
	}

	Vec3 startDir( reach.start );
	startDir -= entityPhysicsState.Origin();
	startDir.NormalizeFast();

	context->record->botInput.SetIntendedLookDir( startDir, true );
	context->record->botInput.SetForwardMovement( 1 );
	if( startDir.Dot( entityPhysicsState.ForwardDir() ) > 0.9f ) {
		if( squareDistance2D < SQUARE( 14.0f ) ) {
			context->record->botInput.SetUpMovement( 1 );
		}
	} else if( !entityPhysicsState.GroundEntity() ) {
		context->record->botInput.SetForwardMovement( 0 );
		// The bot has just jumped. Try using aircontrol to hit the reach end.
		if( entityPhysicsState.Velocity()[2] > 0 ) {
			Vec3 endDir( reach.end );
			endDir -= entityPhysicsState.Origin();
			endDir.NormalizeFast();
			context->record->botInput.SetIntendedLookDir( endDir, true );
			float dotRight = endDir.Dot( entityPhysicsState.Origin() );
			if( dotRight > 0.1f ) {
				context->record->botInput.SetRightMovement( +1 );
			} else if( dotRight < -0.1f ) {
				context->record->botInput.SetRightMovement( -1 );
			}
		}
	} else {
		if( squareDistance2D < SQUARE( 14.0f ) ) {
			context->record->botInput.SetWalkButton( true );
		}
	}

	return true;
}

bool BotDummyMovementAction::HandleClimbJumpReachability( BotMovementPredictionContext *context,
														  const aas_reachability_t &reach ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	float squareDistance2D = Distance2DSquared( entityPhysicsState.Origin(), reach.start );
	if( squareDistance2D > SQUARE( 24.0f ) ) {
		return false;
	}

	if( squareDistance2D < SQUARE( 14.0f ) ) {
		// Jump and press special when start falling
		if( entityPhysicsState.GroundEntity() ) {
			context->record->botInput.SetUpMovement( 1 );
		} else if( entityPhysicsState.Velocity()[2] < 35.0f ) {
			context->record->botInput.SetSpecialButton( true );
		} else if( entityPhysicsState.Velocity()[2] > 0.66f * DEFAULT_JUMPSPEED ) {
			float heightOverGround = entityPhysicsState.HeightOverGround();
			// Hack! boost jump Z speed for bots at jump start
			if( heightOverGround < 12.0f ) {
				Vec3 newVelocity( entityPhysicsState.Velocity() );
				newVelocity.Z() = DEFAULT_JUMPSPEED + 30.0f;
				context->record->SetModifiedVelocity( newVelocity );
			}
		}

		Vec3 reachVec( reach.end );
		reachVec -= reach.start;
		context->record->botInput.SetIntendedLookDir( reachVec, false );
		context->record->botInput.SetTurnSpeedMultiplier( 2.0f );
	} else {
		Vec3 reachDir( reach.start );
		reachDir -= entityPhysicsState.Origin();
		reachDir.NormalizeFast();

		context->record->botInput.SetForwardMovement( 1 );
		if( reachDir.Dot( entityPhysicsState.ForwardDir() ) < 0.7f ) {
			context->record->botInput.SetWalkButton( true );
		}

		context->record->botInput.SetIntendedLookDir( reachDir, true );
	}

	return true;
}

bool BotDummyMovementAction::ShouldCrouchSlideNow( BotMovementPredictionContext *context ) const {
	if( !( context->currPlayerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_CROUCHSLIDING ) ) {
		return false;
	}

	if( context->currPlayerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) {
		if( context->currPlayerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] > PM_CROUCHSLIDE_FADE ) {
			return true;
		}
	}

	if( context->movementState->entityPhysicsState.Speed2D() > context->GetRunSpeed() * 1.2f ) {
		return true;
	}

	return false;
}

void BotDummyMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	auto *botInput = &context->record->botInput;
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;

	int nextReachNum = 0;
	bool handledSpecialMovement = false;

	if( context->IsInNavTargetArea() ) {
		Vec3 intendedLookDir( context->NavTargetOrigin() );
		intendedLookDir -= entityPhysicsState.Origin();
		intendedLookDir.NormalizeFast();
		botInput->SetIntendedLookDir( intendedLookDir, true );

		if( entityPhysicsState.GroundEntity() ) {
			botInput->SetForwardMovement( true );
			if( self->ai->botRef->ShouldMoveCarefully() ) {
				botInput->SetWalkButton( true );
			} else if( context->IsCloseToNavTarget() ) {
				botInput->SetWalkButton( true );
			}
		} else {
			// Try apply QW-like aircontrol
			float dotForward = intendedLookDir.Dot( entityPhysicsState.ForwardDir() );
			if( dotForward > 0 ) {
				float dotRight = intendedLookDir.Dot( entityPhysicsState.RightDir() );
				if( dotRight > 0.3f ) {
					botInput->SetRightMovement( +1 );
				} else if( dotRight < -0.3f ) {
					botInput->SetRightMovement( -1 );
				}
			}
		}

		handledSpecialMovement = true;
	} else if( ( nextReachNum = context->NextReachNum() ) ) {
		const auto &nextReach = AiAasWorld::Instance()->Reachabilities()[nextReachNum];
		switch( nextReach.traveltype & TRAVELTYPE_MASK ) {
			case TRAVEL_WALK:
				if( !ShouldCrouchSlideNow( context ) ) {
					handledSpecialMovement = HandleWalkOrFallReachability( context, nextReach, 0.33f );
					// Prepare for crouch sliding even if a special movement (reach. transition) was handled
					// (Check whether it was handled to avoid duplicated ShouldPrepareForCrouchSliding() test).
					if( handledSpecialMovement ) {
						if( !self->ai->botRef->ShouldMoveCarefully() && ShouldPrepareForCrouchSliding( context ) ) {
							botInput->SetUpMovement( -1 );
						}
					}
				}
				break;
			case TRAVEL_WALKOFFLEDGE:
				handledSpecialMovement = HandleWalkOrFallReachability( context, nextReach, 1.00f );
				break;
			case TRAVEL_JUMP:
			case TRAVEL_STRAFEJUMP:
				handledSpecialMovement = HandleLongJumpReachability( context, nextReach );
				break;
			case TRAVEL_BARRIERJUMP:
			case TRAVEL_DOUBLEJUMP:
				handledSpecialMovement = HandleClimbJumpReachability( context, nextReach );
				break;
		}
	}

	if( handledSpecialMovement ) {
		botInput->SetAllowedRotationMask( BotInputRotation::NONE );
	} else {
		if( !nextReachNum || !context->NavTargetAasAreaNum() ) {
			// Looks like the nav target is lost due to being high above the ground
			if( entityPhysicsState.IsHighAboveGround() ) {
				if( entityPhysicsState.Speed2D() > context->GetRunSpeed() ) {
					// If there is a substantial 2D speed, looks like the bot is jumping over a gap
					Vec3 intendedLookDir( entityPhysicsState.Velocity() );
					intendedLookDir *= 1.0f / entityPhysicsState.Speed();
					botInput->SetIntendedLookDir( intendedLookDir, true );
				} else {
					// Force falling down
					Vec3 intendedLookDir( 0, 0, -1 );
					botInput->SetIntendedLookDir( intendedLookDir, true );
					// Try apply the weak forward air control
					// This condition contains the degenerate dot product with the intended look dir
					if( entityPhysicsState.ForwardDir().Z() < -0.7 ) {
						botInput->SetForwardMovement( 1 );
					}
				}
			}
		} else {
			Vec3 intendedLookVec( AiAasWorld::Instance()->Reachabilities()[nextReachNum].start );
			intendedLookVec -= entityPhysicsState.Origin();
			botInput->SetIntendedLookDir( intendedLookVec, false );
			botInput->SetForwardMovement( 1 );
		}

		const auto &miscTactics = &self->ai->botRef->GetMiscTactics();
		if( !miscTactics->shouldMoveCarefully && !( context->NavTargetAasAreaNum() && context->IsCloseToNavTarget() ) ) {
			if( ShouldCrouchSlideNow( context ) || ShouldPrepareForCrouchSliding( context ) ) {
				botInput->SetUpMovement( -1 );
			} else if( !miscTactics->shouldBeSilent ) {
				if( entityPhysicsState.GroundEntity() && entityPhysicsState.Speed2D() > context->GetRunSpeed() ) {
					if( context->CanSafelyKeepHighSpeed() ) {
						botInput->ClearMovementDirections();
						botInput->SetUpMovement( +1 );
					}
				}
			}
		}
	}

	botInput->isUcmdSet = true;
	botInput->canOverrideUcmd = true;
	botInput->canOverrideLookVec = !handledSpecialMovement;
	Debug( "Planning is complete: the action should never be predicted ahead\n" );
	context->isCompleted = true;
}

void BotRidePlatformMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, &DefaultWalkAction() ) ) {
		return;
	}

	const edict_t *platform = GetPlatform( context );
	if( !platform ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DefaultWalkAction();
		Debug( "Cannot apply the action (cannot find a platform below)\n" );
		return;
	}

	if( platform->moveinfo.state == STATE_TOP ) {
		SetupExitPlatformMovement( context, platform );
	} else {
		SetupIdleRidingPlatformMovement( context, platform );
	}
}

void BotRidePlatformMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const int targetAreaNum = self->ai->botRef->savedPlatformAreas[currTestedAreaIndex];
	const int currAreaNum = entityPhysicsState.CurrAasAreaNum();
	const int droppedToFloorAreaNum = entityPhysicsState.DroppedToFloorAasAreaNum();
	if( currAreaNum == targetAreaNum || droppedToFloorAreaNum == targetAreaNum ) {
		Debug( "The bot has entered the target exit area, should stop planning\n" );
		context->isCompleted = true;
		return;
	}

	const unsigned sequenceDuration = SequenceDuration( context );
	if( sequenceDuration < 250 ) {
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	if( originAtSequenceStart.SquareDistance2DTo( entityPhysicsState.Origin() ) < 32 * 32 ) {
		Debug( "The bot is likely stuck trying to use area #%d to exit the platform\n", currTestedAreaIndex );
		context->SetPendingRollback();
		return;
	}

	if( sequenceDuration > 550 ) {
		Debug( "The bot still has not reached the target exit area\n" );
		context->SetPendingRollback();
		return;
	}

	context->SaveSuggestedActionForNextFrame( this );
}

void BotRidePlatformMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
																  SequenceStopReason stopReason,
																  unsigned stoppedAtFrameIndex ) {
	BotBaseMovementAction::OnApplicationSequenceStopped( context, stopReason, stoppedAtFrameIndex );
	if( stopReason != FAILED && stopReason != DISABLED ) {
		return;
	}

	currTestedAreaIndex++;
}

void BotRidePlatformMovementAction::SetupIdleRidingPlatformMovement( BotMovementPredictionContext *context,
																	 const edict_t *platform ) {
	if( self->ai->botRef->savedPlatformAreas.empty() && context->NavTargetAasAreaNum() ) {
		TrySaveExitAreas( context, platform );
	}

	auto *botInput = &context->record->botInput;
	if( self->ai->botRef->HasEnemy() ) {
		Vec3 toEnemy( self->ai->botRef->EnemyOrigin() );
		toEnemy -= context->movementState->entityPhysicsState.Origin();
		botInput->SetIntendedLookDir( toEnemy, false );
	} else {
		float height = platform->moveinfo.start_origin[2] - platform->moveinfo.end_origin[2];
		float frac = ( platform->s.origin[2] - platform->moveinfo.end_origin[2] ) / height;
		if( frac > 0.5f && !self->ai->botRef->savedPlatformAreas.empty() ) {
			const auto &area = AiAasWorld::Instance()->Areas()[self->ai->botRef->savedPlatformAreas.front()];
			Vec3 lookVec( area.center );
			lookVec -= context->movementState->entityPhysicsState.Origin();
			botInput->SetIntendedLookDir( lookVec, false );
		} else {
			Vec3 lookVec( context->movementState->entityPhysicsState.ForwardDir() );
			lookVec.Z() = 0.5f - 1.0f * frac;
			botInput->SetIntendedLookDir( lookVec, false );
		}
	}

	botInput->isUcmdSet = true;
	botInput->canOverrideUcmd = true;

	Debug( "Stand idle on the platform, do not plan ahead\n" );
	context->isCompleted = true;
}

void BotRidePlatformMovementAction::SetupExitPlatformMovement( BotMovementPredictionContext *context,
															   const edict_t *platform ) {
	const ExitAreasVector &suggestedAreas = SuggestExitAreas( context, platform );
	if( suggestedAreas.empty() ) {
		Debug( "Warning: there is no platform exit areas, do not plan ahead\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}

	if( currTestedAreaIndex >= suggestedAreas.size() ) {
		Debug( "All suggested exit area tests have failed\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	auto *botInput = &context->record->botInput;

	const auto &area = AiAasWorld::Instance()->Areas()[suggestedAreas[currTestedAreaIndex]];
	Vec3 intendedLookDir( area.center );
	intendedLookDir.Z() = area.mins[2] + 32;
	intendedLookDir -= entityPhysicsState.Origin();
	float distance = intendedLookDir.NormalizeFast();
	botInput->SetIntendedLookDir( intendedLookDir, true );

	botInput->isUcmdSet = true;
	float dot = intendedLookDir.Dot( entityPhysicsState.ForwardDir() );
	if( dot < 0.0f ) {
		botInput->SetTurnSpeedMultiplier( 3.0f );
		return;
	}

	if( dot < 0.7f ) {
		botInput->SetWalkButton( true );
		return;
	}

	if( distance > 64.0f ) {
		botInput->SetSpecialButton( true );
	}
}

const edict_t *BotRidePlatformMovementAction::GetPlatform( BotMovementPredictionContext *context ) const {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const edict_t *groundEntity = entityPhysicsState.GroundEntity();
	if( groundEntity ) {
		return groundEntity->use == Use_Plat ? groundEntity : nullptr;
	}

	trace_t trace;
	Vec3 startPoint( entityPhysicsState.Origin() );
	startPoint.Z() += playerbox_stand_mins[2];
	Vec3 endPoint( entityPhysicsState.Origin() );
	endPoint.Z() += playerbox_stand_mins[2];
	endPoint.Z() -= 32.0f;
	G_Trace( &trace, startPoint.Data(), playerbox_stand_mins, playerbox_stand_maxs, endPoint.Data(), self, MASK_ALL );
	if( trace.ent != -1 ) {
		groundEntity = game.edicts + trace.ent;
		if( groundEntity->use == Use_Plat ) {
			return groundEntity;
		}
	}

	return nullptr;
}

void BotRidePlatformMovementAction::TrySaveExitAreas( BotMovementPredictionContext *context, const edict_t *platform ) {
	auto &savedAreas = self->ai->botRef->savedPlatformAreas;
	savedAreas.clear();

	int navTargetAreaNum = self->ai->botRef->NavTargetAasAreaNum();
	Assert( navTargetAreaNum );

	FindExitAreas( context, platform, tmpExitAreas );

	const auto *routeCache = self->ai->botRef->routeCache;
	const int travelFlags = self->ai->botRef->allowedAasTravelFlags;

	int areaTravelTimes[MAX_SAVED_AREAS];
	for( unsigned i = 0, end = tmpExitAreas.size(); i < end; ++i ) {
		int travelTime = routeCache->TravelTimeToGoalArea( tmpExitAreas[i], navTargetAreaNum, travelFlags );
		if( !travelTime ) {
			continue;
		}

		savedAreas.push_back( tmpExitAreas[i] );
		areaTravelTimes[i] = travelTime;
	}

	// Sort areas
	for( unsigned i = 1, end = savedAreas.size(); i < end; ++i ) {
		int area = savedAreas[i];
		int travelTime = areaTravelTimes[i];
		int j = i - 1;
		for(; j >= 0 && areaTravelTimes[j] < travelTime; j-- ) {
			savedAreas[j + 1] = savedAreas[j];
			areaTravelTimes[j + 1] = areaTravelTimes[j];
		}

		savedAreas[j + 1] = area;
		areaTravelTimes[j + 1] = travelTime;
	}
}

typedef BotRidePlatformMovementAction::ExitAreasVector ExitAreasVector;

const ExitAreasVector &BotRidePlatformMovementAction::SuggestExitAreas( BotMovementPredictionContext *context,
																		const edict_t *platform ) {
	if( !self->ai->botRef->savedPlatformAreas.empty() ) {
		return self->ai->botRef->savedPlatformAreas;
	}

	FindExitAreas( context, platform, tmpExitAreas );

	// Save found areas to avoid repeated FindExitAreas() calls while testing next area after rollback
	for( int areaNum: tmpExitAreas )
		self->ai->botRef->savedPlatformAreas.push_back( areaNum );

	return tmpExitAreas;
};

void BotRidePlatformMovementAction::FindExitAreas( BotMovementPredictionContext *context,
												   const edict_t *platform,
												   ExitAreasVector &exitAreas ) {
	const auto &aasWorld = AiAasWorld::Instance();
	const auto *aasAreas = aasWorld->Areas();
	const auto *aasAreaSettings = aasWorld->AreaSettings();

	const BotMovementState &movementState = context ? *context->movementState : self->ai->botRef->movementState;

	exitAreas.clear();

	Vec3 mins( platform->r.absmin );
	Vec3 maxs( platform->r.absmax );
	// SP_func_plat(): start is the top position, end is the bottom
	mins.Z() = platform->moveinfo.start_origin[2];
	maxs.Z() = platform->moveinfo.start_origin[2] + 96.0f;
	// We have to extend bounds... check whether wdm9 movement is OK if one changes these values.
	for( int i = 0; i < 2; ++i ) {
		mins.Data()[i] -= 96.0f;
		maxs.Data()[i] += 96.0f;
	}

	Vec3 finalBotOrigin( movementState.entityPhysicsState.Origin() );
	// Add an extra 1 unit offset for tracing purposes
	finalBotOrigin.Z() += platform->moveinfo.start_origin[2] - platform->s.origin[2] + 1.0f;

	trace_t trace;
	int bboxAreaNums[48];
	const int numBBoxAreas = aasWorld->BBoxAreas( mins, maxs, bboxAreaNums, 48 );
	for( int i = 0; i < numBBoxAreas; ++i ) {
		const int areaNum = bboxAreaNums[i];
		const auto &area = aasAreas[areaNum];
		const auto &areaSettings = aasAreaSettings[areaNum];
		if( !( areaSettings.areaflags & AREA_GROUNDED ) ) {
			continue;
		}
		if( areaSettings.areaflags & ( AREA_JUNK | AREA_DISABLED ) ) {
			continue;
		}
		if( areaSettings.contents & ( AREACONTENTS_DONOTENTER | AREACONTENTS_MOVER ) ) {
			continue;
		}

		Vec3 areaPoint( area.center );
		areaPoint.Z() = area.mins[2] + 8.0f;

		// Do not use player box as trace mins/maxs (it leads to blocking when a bot rides a platform near a wall)
		G_Trace( &trace, finalBotOrigin.Data(), nullptr, nullptr, areaPoint.Data(), self, MASK_ALL );
		if( trace.fraction != 1.0f ) {
			continue;
		}

		exitAreas.push_back( areaNum );
		if( exitAreas.size() == exitAreas.capacity() ) {
			break;
		}
	}
}

void BotSwimMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context ) ) {
		return;
	}

	int nextReachNum = context->NextReachNum();
	if( !nextReachNum ) {
		context->cannotApplyAction = true;
		Debug( "Cannot apply action: next reachability is undefined in the given context state\n" );
		return;
	}

	context->SetDefaultBotInput();
	context->record->botInput.canOverrideLookVec = true;
	context->record->botInput.SetForwardMovement( 1 );
	context->TryAvoidFullHeightObstacles( 0.3f );

	const auto &nextReach = AiAasWorld::Instance()->Reachabilities()[nextReachNum];
	if( nextReach.traveltype == TRAVEL_SWIM ) {
		return;
	}

	if( DistanceSquared( nextReach.start, context->movementState->entityPhysicsState.Origin() ) > 24 * 24 ) {
		return;
	}

	// Exit water (might it be above a regular next area? this case is handled by the condition)
	if( nextReach.start[2] < nextReach.end[2] ) {
		context->record->botInput.SetUpMovement( 1 );
	} else {
		context->record->botInput.SetUpMovement( -1 );
	}
}

void BotSwimMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	const auto &oldPhysicsState = context->PhysicsStateBeforeStep();
	const auto &newPhysicsState = context->movementState->entityPhysicsState;

	Assert( oldPhysicsState.waterLevel > 1 );
	if( newPhysicsState.waterLevel < 2 ) {
		context->isCompleted = true;
		Debug( "A movement step has lead to exiting water, should stop planning\n" );
		return;
	}
}

void BotFlyUntilLandingMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context ) ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.GroundEntity() ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DummyAction();
		Debug( "A bot has landed on a ground in the given context state\n" );
		return;
	}

	auto *flyUntilLandingMovementState = &context->movementState->flyUntilLandingMovementState;
	if( flyUntilLandingMovementState->CheckForLanding( context ) ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &LandOnSavedAreasAction();
		Debug( "Bot should perform landing in the given context state\n" );
		return;
	}

	auto *botInput = &context->record->botInput;
	// Relax all keys
	botInput->ClearMovementDirections();
	botInput->isUcmdSet = true;
	// Look at the target (do not keep the angles from the flight beginning,
	// in worst case a bot is unable to turn quickly if the landing site is in opposite direction)
	Vec3 intendedLookVec( flyUntilLandingMovementState->Target() );
	intendedLookVec -= entityPhysicsState.Origin();
	botInput->SetIntendedLookDir( intendedLookVec, false );
	botInput->SetTurnSpeedMultiplier( 1.5f );
	botInput->canOverrideLookVec = true;
	Debug( "Planning is completed (the action should never be predicted ahead\n" );
	context->isCompleted = true;
}

void Bot::MovementFrame( BotInput *input ) {
	this->movementState.Frame( game.frametime );
	this->movementState.TryDeactivateContainedStates( self, nullptr );

	BotMovementActionRecord movementActionRecord;
	BotBaseMovementAction *movementAction = movementPredictionContext.GetActionAndRecordForCurrTime( &movementActionRecord );

	movementAction->ExecActionRecord( &movementActionRecord, input, nullptr );

	CheckTargetProximity();
	CheckGroundPlatform();
}

void Bot::CheckGroundPlatform() {
	if( !self->groundentity ) {
		return;
	}

	// Reset saved platform areas after touching a solid world ground
	if( self->groundentity == world ) {
		self->ai->botRef->savedPlatformAreas.clear();
		return;
	}

	if( self->groundentity->use != Use_Plat ) {
		return;
	}

	if( self->groundentity->moveinfo.state != STATE_BOTTOM ) {
		return;
	}

	self->ai->botRef->ridePlatformMovementAction.TrySaveExitAreas( nullptr, self->groundentity );
}

constexpr float STRAIGHT_MOVEMENT_DOT_THRESHOLD = 0.8f;

BotMovementPredictionContext::HitWhileRunningTestResult BotMovementPredictionContext::MayHitWhileRunning() {
	if( const auto *cachedResult = mayHitWhileRunningCachesStack.GetCached() ) {
		return *cachedResult;
	}

	if( !self->ai->botRef->HasEnemy() ) {
		mayHitWhileRunningCachesStack.SetCachedValue( HitWhileRunningTestResult::Failure() );
		return HitWhileRunningTestResult::Failure();
	}

	const auto &entityPhysicsState = movementState->entityPhysicsState;
	Vec3 botLookDir( entityPhysicsState.ForwardDir() );

	Vec3 botToEnemyDir( self->ai->botRef->EnemyOrigin() );
	botToEnemyDir -= entityPhysicsState.Origin();
	// We are sure it has non-zero length (enemies collide with the bot)
	botToEnemyDir.NormalizeFast();

	// Check whether the bot may hit while running
	if( botToEnemyDir.Dot( botLookDir ) > STRAIGHT_MOVEMENT_DOT_THRESHOLD ) {
		HitWhileRunningTestResult result;
		result.canHitAsIs = true;
		result.mayHitOverridingPitch = true;
		mayHitWhileRunningCachesStack.SetCachedValue( result );
		return result;
	}

	// Check whether we can change pitch
	botLookDir.Z() = botToEnemyDir.Z();
	// Normalize again
	float lookDirSquareLength = botLookDir.SquaredLength();
	if( lookDirSquareLength < 0.000001f ) {
		mayHitWhileRunningCachesStack.SetCachedValue( HitWhileRunningTestResult::Failure() );
		return HitWhileRunningTestResult::Failure();
	}

	botLookDir *= Q_RSqrt( lookDirSquareLength );
	if( botToEnemyDir.Dot( botLookDir ) > STRAIGHT_MOVEMENT_DOT_THRESHOLD ) {
		HitWhileRunningTestResult result;
		result.canHitAsIs = false;
		result.mayHitOverridingPitch = true;
		mayHitWhileRunningCachesStack.SetCachedValue( result );
		return result;
	}

	mayHitWhileRunningCachesStack.SetCachedValue( HitWhileRunningTestResult::Failure() );
	return HitWhileRunningTestResult::Failure();
}

int BotLandOnSavedAreasMovementAction::FindJumppadAreaNum( const edict_t *jumppadEntity ) {
	// TODO: This can be precomputed at level start
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasAreaSettings = aasWorld->AreaSettings();

	// Jumppad entity origin is not what one might think...
	Vec3 jumppadOrigin( jumppadEntity->r.absmin );
	jumppadOrigin += jumppadEntity->r.absmax;
	jumppadOrigin *= 0.5f;

	int entAreaNum = aasWorld->FindAreaNum( jumppadOrigin );
	if( entAreaNum ) {
		const auto &areaSettings = aasAreaSettings[entAreaNum];
		const int contents = areaSettings.contents;
		const int areaflags = areaSettings.areaflags;
		if( ( contents & AREACONTENTS_JUMPPAD ) && !( contents & AREACONTENTS_DONOTENTER ) && !( areaflags & AREA_DISABLED ) ) {
			return entAreaNum;
		}
	}

	int areaNums[32];
	Vec3 mins( -64, -64, -64 );
	Vec3 maxs( +64, +64, +64 );
	mins += jumppadOrigin;
	maxs += jumppadOrigin;
	int numAreas = aasWorld->BBoxAreas( mins, maxs, areaNums, 32 );
	for( int i = 0; i < numAreas; ++i ) {
		const int areaNum = areaNums[i];
		const auto &areaSettings = aasAreaSettings[areaNum];
		const int contents = areaSettings.contents;
		if( !( contents & AREACONTENTS_JUMPPAD ) ) {
			continue;
		}
		if( contents & AREACONTENTS_DONOTENTER ) {
			continue;
		}
		if( areaSettings.areaflags & AREA_DISABLED ) {
			continue;
		}

		return areaNum;
	}

	// Ensure the area is always found. Do not hide the bug, a bot would keep jumping on the trigger forever.
	constexpr const char *tag = "BotLandOnSavedAreasMovementAction::FindJumppadAreaNum()";
	constexpr const char *format = "Can't find an AAS area num for the jumppad @ %.1f %.1f %.1f\n";
	AI_FailWith( tag, format, jumppadOrigin.X(), jumppadOrigin.Y(), jumppadOrigin.Z() );
}

static float PointToSegmentSquareDistance( const vec3_t point, const vec3_t start, const vec3_t end ) {
	Vec3 segmentVec( end );
	segmentVec -= start;
	Vec3 pointToStart( start );
	pointToStart -= point;

	float pointToStartDotVec = pointToStart.Dot( segmentVec );
	if( pointToStartDotVec >= 0.0f ) {
		return DistanceSquared( point, start );
	}

	Vec3 pointToEnd( end );
	pointToEnd -= point;

	if( pointToEnd.Dot( segmentVec ) <= 0.0f ) {
		return DistanceSquared( point, end );
	}

	Vec3 projection( segmentVec );
	projection *= -pointToStartDotVec / segmentVec.SquaredLength();
	projection += pointToStart;
	return projection.SquaredLength();
}

static float PointToAreaSquareDistance( const vec3_t point, const aas_area_t &area ) {
	if( area.mins[2] > point[2] ) {
		return std::numeric_limits<float>::max();
	}

	if( area.mins[0] >= point[0] && area.maxs[0] <= point[0] && area.mins[1] >= point[1] && area.maxs[1] <= point[1] ) {
		return 0.0f;
	}

	float minDistance = std::numeric_limits<float>::max();
	vec3_t sideStart, sideEnd;
	sideStart[2] = sideEnd[2] = area.mins[2];
	const float *bounds[] = { area.mins, area.maxs };
	// For each side
	for( int i = 0; i < 4; ++i ) {
		// Make side segment
		for( int j = 0; j < 2; ++j ) {
			sideStart[j] = bounds[( ( i + 0 ) >> j ) & 1][j];
			sideEnd[j] = bounds[( ( i + 1 ) >> j ) & 1][j];
		}

		float distance = PointToSegmentSquareDistance( point, sideStart, sideEnd );
		if( distance < minDistance ) {
			minDistance = distance;
		}
	}

	return minDistance;
}

float BotLandOnSavedAreasMovementAction::SaveJumppadLandingAreas( const edict_t *jumppadEntity ) {
	savedLandingAreas.clear();

	int jumppadAreaNum = FindJumppadAreaNum( jumppadEntity );
	if( !jumppadAreaNum ) {
		return -999999.9f;
	}

	const auto *aasWorld = AiAasWorld::Instance();
	const auto *routeCache = self->ai->botRef->routeCache;
	if( int navTargetAreaNum = self->ai->botRef->botBrain.NavTargetAasAreaNum() ) {
		int reachNum = 0;
		for( int travelFlags: { self->ai->botRef->preferredAasTravelFlags, self->ai->botRef->allowedAasTravelFlags } ) {
			if( ( reachNum = routeCache->ReachabilityToGoalArea( jumppadAreaNum, navTargetAreaNum, travelFlags ) ) ) {
				break;
			}
		}

		if( reachNum ) {
			int jumppadTargetAreaNum = aasWorld->Reachabilities()[reachNum].areanum;
			return SaveLandingAreasForJumppadTargetArea( jumppadEntity, navTargetAreaNum, jumppadTargetAreaNum );
		}
	}

	// The nav target is not reachable. Try to find any areas reachable from the jumppad area by using the jumppad
	const auto &jumppadAreaSettings = aasWorld->AreaSettings()[jumppadAreaNum];
	const auto *aasAreas = aasWorld->Areas();
	const auto *aasAreaSettings = aasWorld->AreaSettings();
	const auto *aasReach = aasWorld->Reachabilities();
	const float *targetOrigin = jumppadEntity->target_ent->s.origin;
	FilteredAreas filteredAreas;
	// Find an area closest to the jumppad target
	for( int i = 0; i < jumppadAreaSettings.numreachableareas; ++i ) {
		const auto &reach = aasReach[i + jumppadAreaSettings.firstreachablearea];
		if( reach.traveltype != TRAVEL_JUMPPAD ) {
			continue;
		}

		const int areaNum = reach.areanum;
		const auto &areaSettings = aasAreaSettings[areaNum];
		if( areaSettings.areaflags & AREA_DISABLED ) {
			continue;
		}
		if( areaSettings.contents & AREACONTENTS_DONOTENTER ) {
			continue;
		}
		const auto &area = aasAreas[areaNum];
		// Skip areas that are higher than the jumppad target entity
		if( area.mins[2] + 16 > jumppadEntity->target_ent->s.origin[2] ) {
			continue;
		}
		// Closer to the jumppad entity target areas get greater score
		float score = 1.0f / ( 1.0f + PointToAreaSquareDistance( targetOrigin, area ) );
		filteredAreas.emplace_back( AreaAndScore( areaNum, score ) );
		if( filteredAreas.size() == filteredAreas.capacity() ) {
			break;
		}
	}

	// Sort areas so best areas are first
	std::sort( filteredAreas.begin(), filteredAreas.end() );

	return SaveFilteredCandidateAreas( jumppadEntity, 0, filteredAreas );
}

float BotLandOnSavedAreasMovementAction::SaveLandingAreasForJumppadTargetArea( const edict_t *jumppadEntity,
																			   int navTargetAreaNum,
																			   int jumppadTargetAreaNum ) {
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *routeCache = self->ai->botRef->routeCache;
	const int testedTravelFlags[] = { self->ai->botRef->preferredAasTravelFlags, self->ai->botRef->allowedAasTravelFlags };

	const auto *aasAreas = aasWorld->Areas();
	const auto *aasAreaSettings = aasWorld->AreaSettings();

	// Get areas around the jumppad area
	const auto &jumppadTargetArea = aasAreas[jumppadTargetAreaNum];
	Vec3 mins( -256, -256, -16 );
	Vec3 maxs( +256, +256, +16 );
	mins += jumppadTargetArea.mins;
	maxs += jumppadTargetArea.maxs;
	int bboxAreas[48];
	const int numBBoxAreas = aasWorld->BBoxAreas( mins, maxs, bboxAreas, 48 );

	int baseTravelTime = 0;
	for( int travelFlags: testedTravelFlags ) {
		if( ( baseTravelTime = routeCache->TravelTimeToGoalArea( jumppadTargetAreaNum, navTargetAreaNum, travelFlags ) ) ) {
			break;
		}
	}
	// If the target is for some reasons unreachable or the jumppad target area is the nav target area too
	if( baseTravelTime <= 1 ) {
		// Return some default values in hope they are useful
		savedLandingAreas.push_back( jumppadTargetAreaNum );
		return jumppadTargetArea.mins[2];
	}

	// Filter raw nearby areas
	FilteredAreas filteredAreas;
	for( int i = 0; i < numBBoxAreas; ++i ) {
		const int areaNum = bboxAreas[i];
		// Skip tests for the target area
		if( areaNum == jumppadTargetAreaNum ) {
			continue;
		}

		const auto &rawArea = aasAreas[areaNum];
		// Skip areas that are lower than the target area more than 16 units
		if( rawArea.mins[2] + 16 < jumppadTargetArea.mins[2] ) {
			continue;
		}
		// Skip areas that are higher than the jumppad target entity
		if( rawArea.mins[2] + 16 > jumppadEntity->target_ent->s.origin[2] ) {
			continue;
		}

		const auto &areaSettings = aasAreaSettings[areaNum];
		if( !( areaSettings.areaflags & AREA_GROUNDED ) ) {
			continue;
		}
		if( areaSettings.contents & AREACONTENTS_DONOTENTER ) {
			continue;
		}
		if( areaSettings.areaflags & ( AREA_JUNK | AREA_DISABLED ) ) {
			continue;
		}

		int travelTime = 0;
		for( int travelFlags: testedTravelFlags ) {
			if( ( travelTime = routeCache->TravelTimeToGoalArea( areaNum, navTargetAreaNum, travelFlags ) ) ) {
				break;
			}
		}
		// If the nav target is not reachable from the box area or
		// it leads to a greater travel time than the jumppad target area
		if( !travelTime || travelTime >= baseTravelTime ) {
			continue;
		}

		// The score is greater if it shortens travel time greater
		float score = (float)baseTravelTime / (float)travelTime;
		// Apply penalty for ledge areas (prevent falling just after landing)
		if( areaSettings.areaflags & AREA_LEDGE ) {
			score *= 0.5f;
		}

		filteredAreas.emplace_back( AreaAndScore( areaNum, score ) );
		if( filteredAreas.size() == filteredAreas.capacity() ) {
			break;
		}
	}

	// Sort filtered areas so best areas are first
	std::sort( filteredAreas.begin(), filteredAreas.end() );

	return SaveFilteredCandidateAreas( jumppadEntity, jumppadTargetAreaNum, filteredAreas );
}

float BotLandOnSavedAreasMovementAction::SaveFilteredCandidateAreas( const edict_t *jumppadEntity,
																	 int jumppadTargetAreaNum,
																	 const FilteredAreas &filteredAreas ) {
	Assert( savedLandingAreas.empty() );
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasAreas = aasWorld->Areas();

	for( unsigned i = 0, end = std::min( filteredAreas.size(), savedLandingAreas.capacity() ); i < end; ++i )
		savedLandingAreas.push_back( filteredAreas[i].areaNum );

	// Always add the target area (with the lowest priority)
	if( jumppadTargetAreaNum ) {
		if( savedLandingAreas.size() == savedLandingAreas.capacity() ) {
			savedLandingAreas.pop_back();
		}

		savedLandingAreas.push_back( jumppadTargetAreaNum );
	}

	float maxAreaZ = std::numeric_limits<float>::min();
	for( int areaNum: savedLandingAreas ) {
		const auto &area = aasAreas[areaNum];
		Assert( area.mins[2] < jumppadEntity->target_ent->s.origin[2] );
		Assert( area.mins[2] > jumppadEntity->r.absmin[2] );
		if( maxAreaZ < area.mins[2] ) {
			maxAreaZ = area.mins[2];
		}
	}

	return maxAreaZ;
}

void BotLandOnSavedAreasMovementAction::BeforePlanning() {
	BotBaseMovementAction::BeforePlanning();
	currAreaIndex = 0;
	totalTestedAreas = 0;

	this->savedLandingAreas.clear();
	auto *botSavedAreas = &self->ai->botRef->savedLandingAreas;
	for( int areaNum: *botSavedAreas )
		this->savedLandingAreas.push_back( areaNum );

	botSavedAreas->clear();
}

void BotLandOnSavedAreasMovementAction::AfterPlanning() {
	BotBaseMovementAction::AfterPlanning();
	if( this->isDisabledForPlanning ) {
		return;
	}

	auto *botSavedAreas = &self->ai->botRef->savedLandingAreas;
	for( int areaNum: this->savedLandingAreas )
		botSavedAreas->push_back( areaNum );
}

void BotHandleTriggeredJumppadMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, &DummyAction() ) ) {
		return;
	}

	auto *jumppadMovementState = &context->movementState->jumppadMovementState;
	Assert( jumppadMovementState->IsActive() );

	if( jumppadMovementState->hasEnteredJumppad ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &FlyUntilLandingAction();
		Debug( "The bot has already processed jumppad trigger touch in the given context state, fly until landing\n" );
		return;
	}

	jumppadMovementState->hasEnteredJumppad = true;

	auto *botInput = &context->record->botInput;
	botInput->Clear();

	const edict_t *jumppadEntity = jumppadMovementState->JumppadEntity();
	float startLandingAtZ = self->ai->botRef->landOnSavedAreasSetMovementAction.SaveJumppadLandingAreas( jumppadEntity );
	context->movementState->flyUntilLandingMovementState.Activate( startLandingAtZ );
	// Stop prediction (jumppad triggers are not simulated by Exec() code)
	context->isCompleted = true;
}

bool BotLandOnSavedAreasMovementAction::TryLandingStepOnArea( int areaNum, BotMovementPredictionContext *context ) {
	auto *botInput = &context->record->botInput;
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const float *origin = entityPhysicsState.Origin();

	const auto &area = AiAasWorld::Instance()->Areas()[areaNum];
	Vec3 areaPoint( area.center );
	// Lower area point to a bottom of area. Area mins/maxs are absolute.
	areaPoint.Z() = area.mins[2];
	// Do not try to "land" on upper areas
	if( areaPoint.Z() > origin[2] ) {
		Debug( "Cannot land on an area that is above the bot origin in the given movement state\n" );
		return false;
	}

	botInput->Clear();
	botInput->isUcmdSet = true;

	Vec3 intendedLookDir( 0, 0, -1 );
	// Prevent flying over the area.
	if( area.mins[0] > origin[0] || area.maxs[0] < origin[0] || area.mins[1] > origin[1] || area.maxs[1] < origin[1] ) {
		// Most likely case (the bot is outside of the area bounds)
		intendedLookDir.Set( areaPoint );
		intendedLookDir -= origin;
		intendedLookDir.NormalizeFast();
	}
	botInput->SetIntendedLookDir( intendedLookDir, true );

	// Apply QW-like air control
	float dotRight = entityPhysicsState.RightDir().Dot( intendedLookDir );
	if( dotRight > 0.7f ) {
		botInput->SetRightMovement( +1 );
	} else if( dotRight < -0.7f ) {
		botInput->SetRightMovement( -1 );
	} else {
		// While we do not use forwardbunny, there is still a little air control from forward key
		float dotForward = entityPhysicsState.ForwardDir().Dot( intendedLookDir );
		if( dotForward > 0.3f ) {
			botInput->SetForwardMovement( +1 );
		} else if( dotForward < -0.3f ) {
			botInput->SetForwardMovement( -1 );
		}
	}

	return true;
}

void BotLandOnSavedAreasMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, &DummyAction() ) ) {
		return;
	}

	// This list might be empty if all nearby areas have been disabled (e.g. as blocked by enemy).
	if( savedLandingAreas.empty() ) {
		Debug( "Cannot apply action: the saved landing areas list is empty\n" );
		this->isDisabledForPlanning = true;
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DummyAction();
		return;
	}

	// If there the current tested area is set
	if( currAreaIndex >= 0 ) {
		Assert( (int)savedLandingAreas.size() > currAreaIndex );
		// Continue testing this area
		if( TryLandingStepOnArea( savedLandingAreas[currAreaIndex], context ) ) {
			context->SaveSuggestedActionForNextFrame( this );
			return;
		}

		// Schedule next saved area for testing
		const char *format = "Landing on area %d/%d has failed, roll back to initial landing state for next area\n";
		Debug( format, currAreaIndex, savedLandingAreas.size() );
		currAreaIndex = -1;
		totalTestedAreas++;
		// Force rolling back to savepoint
		context->SetPendingRollback();
		// (the method execution implicitly will be continued on the code below outside this condition on next call)
		return;
	}

	// There is not current tested area set, try choose one that fit
	for(; totalTestedAreas < savedLandingAreas.size(); totalTestedAreas++ ) {
		// Test each area left using a-priori feasibility of an area
		if( TryLandingStepOnArea( savedLandingAreas[totalTestedAreas], context ) ) {
			// Set the area as current
			currAreaIndex = totalTestedAreas;
			// Create a savepoint
			context->savepointTopOfStackIndex = context->topOfStackIndex;
			// (the method execution will be implicitly continue on the code inside the condition above on next call)
			Debug( "Area %d/%d has been chosen for landing tests\n", currAreaIndex, savedLandingAreas.size() );
			context->SaveSuggestedActionForNextFrame( this );
			return;
		}
	}

	// All areas have been tested, and there is no suitable area for landing
	Debug( "Warning: An area suitable for landing has not been found\n" );

	// Just look at the jumppad target
	const auto &movementState = context->movementState;
	auto *botInput = &context->record->botInput;
	Vec3 toTargetDir( movementState->jumppadMovementState.JumppadEntity()->target_ent->s.origin );
	toTargetDir -= movementState->entityPhysicsState.Origin();
	toTargetDir.NormalizeFast();

	botInput->SetIntendedLookDir( toTargetDir, true );
	// Try apply air control preferring QW-style one
	float dotForward = toTargetDir.Dot( movementState->entityPhysicsState.ForwardDir() );
	if( dotForward < -0.3f ) {
		botInput->SetTurnSpeedMultiplier( 3.0f );
	}

	float dotRight = toTargetDir.Dot( movementState->entityPhysicsState.RightDir() );
	if( dotRight > 0.5f ) {
		botInput->SetRightMovement( +1 );
	} else if( dotRight < -0.5f ) {
		botInput->SetRightMovement( -1 );
	} else if( dotForward > 0.5f ) {
		botInput->SetForwardMovement( +1 );
	}

	// Disallow any input rotation while landing, it relies on a side aircontrol.
	botInput->SetAllowedRotationMask( BotInputRotation::NONE );

	botInput->isUcmdSet = true;

	// Do not predict ahead.
	context->isCompleted = true;
}

void BotLandOnSavedAreasMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	// If movement step failed, make sure that the next area (if any) will be tested after rollback
	if( context->cannotApplyAction ) {
		totalTestedAreas++;
		currAreaIndex = -1;
		return;
	}

	if( context->isCompleted ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( !entityPhysicsState.GroundEntity() ) {
		return;
	}

	// Check which area bot has landed in
	Assert( currAreaIndex >= 0 && currAreaIndex == (int)totalTestedAreas && currAreaIndex < (int)savedLandingAreas.size() );
	const int landingArea = savedLandingAreas[currAreaIndex];
	if( landingArea == entityPhysicsState.CurrAasAreaNum() || landingArea == entityPhysicsState.DroppedToFloorAasAreaNum() ) {
		Debug( "A prediction step has lead to touching a ground in the target landing area, should stop planning\n" );
		context->isCompleted = true;
		return;
	}

	Debug( "A prediction step has lead to touching a ground in an unexpected area\n" );
	context->SetPendingRollback();
	// Make sure that the next area (if any) will be tested after rolling back
	totalTestedAreas++;
	currAreaIndex = -1;
}

void DirToKeyInput( const Vec3 &desiredDir, const vec3_t actualForwardDir, const vec3_t actualRightDir, BotInput *input ) {
	input->ClearMovementDirections();

	float dotForward = desiredDir.Dot( actualForwardDir );
	if( dotForward > 0.3 ) {
		input->SetForwardMovement( 1 );
	} else if( dotForward < -0.3 ) {
		input->SetForwardMovement( -1 );
	}

	float dotRight = desiredDir.Dot( actualRightDir );
	if( dotRight > 0.3 ) {
		input->SetRightMovement( 1 );
	} else if( dotRight < -0.3 ) {
		input->SetRightMovement( -1 );
	}

	// Prevent being blocked
	if( !input->ForwardMovement() && !input->RightMovement() ) {
		input->SetForwardMovement( 1 );
	}
}

void BotCampASpotMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, &DefaultWalkAction() ) ) {
		return;
	}

	if( this->disabledForApplicationFrameIndex == context->topOfStackIndex ) {
		context->sequenceStopReason = DISABLED;
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DefaultWalkAction();
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	auto *campingSpotState = &context->movementState->campingSpotState;
	auto *botInput = &context->record->botInput;

	context->SetDefaultBotInput();
	context->record->botInput.canOverrideLookVec = true;

	const Vec3 spotOrigin( campingSpotState->Origin() );
	float distance = spotOrigin.Distance2DTo( entityPhysicsState.Origin() );

	AiPendingLookAtPoint lookAtPoint( campingSpotState->GetOrUpdateRandomLookAtPoint() );

	const Vec3 actualLookDir( entityPhysicsState.ForwardDir() );
	Vec3 expectedLookDir( lookAtPoint.Origin() );
	expectedLookDir -= spotOrigin;
	expectedLookDir.NormalizeFast();

	if( expectedLookDir.Dot( actualLookDir ) < 0.85 ) {
		if( !context->movementState->pendingLookAtPointState.IsActive() ) {
			AiPendingLookAtPoint pendingLookAtPoint( campingSpotState->GetOrUpdateRandomLookAtPoint() );
			context->movementState->pendingLookAtPointState.Activate( pendingLookAtPoint, 300 );
			botInput->ClearMovementDirections();
			botInput->SetWalkButton( true );
			return;
		}
	}

	context->predictionStepMillis = 16;
	// Keep actual look dir as-is, adjust position by keys only
	botInput->SetIntendedLookDir( actualLookDir, true );
	// This means we may strafe randomly
	if( distance / campingSpotState->Radius() < 1.0f ) {
		if( !campingSpotState->AreKeyMoveDirsValid() ) {
			auto &traceCache = context->EnvironmentTraceCache();
			int keyMoves[2];
			Vec3 botToSpotDir( spotOrigin );
			botToSpotDir -= entityPhysicsState.Origin();
			botToSpotDir.NormalizeFast();
			traceCache.MakeRandomizedKeyMovesToTarget( context, botToSpotDir, keyMoves );
			campingSpotState->SetKeyMoveDirs( keyMoves[0], keyMoves[1] );
		} else {
			// Move dirs are kept and the bot is in the spot radius, use lesser prediction precision
			context->predictionStepMillis = 32;
		}
		botInput->SetForwardMovement( campingSpotState->ForwardMove() );
		botInput->SetRightMovement( campingSpotState->RightMove() );
		if( !botInput->ForwardMovement() && !botInput->RightMovement() ) {
			botInput->SetUpMovement( -1 );
		}
	} else {
		Vec3 botToSpotDir( spotOrigin );
		botToSpotDir -= entityPhysicsState.Origin();
		botToSpotDir.NormalizeFast();
		DirToKeyInput( botToSpotDir, actualLookDir.Data(), entityPhysicsState.RightDir().Data(), botInput );
	}

	botInput->SetWalkButton( random() > campingSpotState->Alertness() * 0.75f );
}

void BotCampASpotMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	Vec3 origin( context->movementState->entityPhysicsState.Origin() );
	const auto &campingSpotState = context->movementState->campingSpotState;
	if( !campingSpotState.IsActive() ) {
		Debug( "A prediction step has lead to camping spot state deactivation (the bot is too far from its origin)\n" );
		context->SetPendingRollback();
		return;
	}

	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;
	const auto &oldEntityPhysicsState = context->PhysicsStateBeforeStep();
	const float radius = campingSpotState.Radius();
	Vec3 spotOrigin( campingSpotState.Origin() );

	const float oldSquareDistanceToOrigin = spotOrigin.SquareDistance2DTo( oldEntityPhysicsState.Origin() );
	const float newSquareDistanceToOrigin = spotOrigin.SquareDistance2DTo( newEntityPhysicsState.Origin() );
	if( oldSquareDistanceToOrigin > SQUARE( 1.3f * radius ) ) {
		if( newSquareDistanceToOrigin > oldSquareDistanceToOrigin ) {
			Debug( "A prediction step has lead to even greater distance to the spot origin while bot should return to it\n" );
			context->SetPendingRollback();
			return;
		}
	}

	// Wait for landing
	if( !newEntityPhysicsState.GroundEntity() ) {
		return;
	}

	if( newSquareDistanceToOrigin < SQUARE( radius ) ) {
		const unsigned sequenceDuration = this->SequenceDuration( context );
		const unsigned completionMillisThreshold = (unsigned) ( 512 * ( 1.0f - 0.5f * campingSpotState.Alertness() ) );
		if( sequenceDuration > completionMillisThreshold ) {
			Debug( "Bot is close to the spot origin and there is enough predicted data ahead\n" );
			context->isCompleted = true;
			return;
		}
	}
}

void BotCampASpotMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
															   SequenceStopReason stopReason,
															   unsigned stoppedAtFrameIndex ) {
	BotBaseMovementAction::OnApplicationSequenceStopped( context, stopReason, stoppedAtFrameIndex );

	if( stopReason == DISABLED ) {
		return;
	}

	if( stopReason == FAILED ) {
		disabledForApplicationFrameIndex = context->savepointTopOfStackIndex;
		return;
	}

	disabledForApplicationFrameIndex = std::numeric_limits<unsigned>::max();
}

struct ReachChainInterpolator {
	Vec3 intendedLookDir;
	// Continue interpolating while a next reach has these travel types
	const int *compatibleReachTypes;
	int numCompatibleReachTypes;
	// Stop interpolating on these reach types but include a reach start in interpolation
	const int *allowedEndReachTypes;
	int numAllowedEndReachTypes;
	// Note: Ignored when there is only a single far reach.
	float stopAtDistance;

	inline ReachChainInterpolator()
		: intendedLookDir( 0, 0, 0 ),
		compatibleReachTypes( nullptr ),
		numCompatibleReachTypes( 0 ),
		allowedEndReachTypes( nullptr ),
		numAllowedEndReachTypes( 0 ),
		stopAtDistance( 256 )
	{}

	inline void SetCompatibleReachTypes( const int *reachTravelTypes, int numTravelTypes ) {
		this->compatibleReachTypes = reachTravelTypes;
		this->numCompatibleReachTypes = numTravelTypes;
	}

	inline void SetAllowedEndReachTypes( const int *reachTravelTypes, int numTravelTypes ) {
		this->allowedEndReachTypes = reachTravelTypes;
		this->numAllowedEndReachTypes = numTravelTypes;
	}

	inline bool IsCompatibleReachType( int reachTravelType ) const {
		assert( ( reachTravelType & TRAVELTYPE_MASK ) == reachTravelType );
		const int *end = compatibleReachTypes + numCompatibleReachTypes;
		return std::find( compatibleReachTypes, end, reachTravelType ) != end;
	}

	inline bool IsAllowedEndReachType( int reachTravelType ) const {
		assert( ( reachTravelType & TRAVELTYPE_MASK ) == reachTravelType );
		const int *end = allowedEndReachTypes + numAllowedEndReachTypes;
		return std::find( allowedEndReachTypes, end, reachTravelType ) != end;
	}

	bool Exec( BotMovementPredictionContext *context );

	inline const Vec3 &Result() const { return intendedLookDir; }
};

bool ReachChainInterpolator::Exec( BotMovementPredictionContext *context ) {
	trace_t trace;
	vec3_t firstReachDir;
	const auto &reachChain = context->NextReachChain();
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasReach = aasWorld->Reachabilities();
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const float *origin = entityPhysicsState.Origin();
	const aas_reachability_t *singleFarReach = nullptr;
	const float squareDistanceThreshold = SQUARE( stopAtDistance );
	const int navTargetAreaNum = context->NavTargetAasAreaNum();
	unsigned numReachFound = 0;
	bool endsInNavTargetArea = false;

	intendedLookDir.Set( 0, 0, 0 );
	for( unsigned i = 0; i < reachChain.size(); ++i ) {
		const auto &reach = aasReach[reachChain[i].ReachNum()];

		int travelType = reach.traveltype & TRAVELTYPE_MASK;
		// If the reach type is not in compatible types, we will have to make an immediate or pending break of the loop
		if( !IsCompatibleReachType( travelType ) ) {
			// If the reach type is not even mentioned in the allowed stop reach types, break immediately
			if( !IsAllowedEndReachType( travelType ) ) {
				break;
			}

			// If the reach type is mentioned in the allowed stop reach types, process the reach but stop at it.
			// This line acts as a pending break after the iteration
			i = reachChain.size() + 1;
		}

		if( DistanceSquared( origin, reach.start ) > squareDistanceThreshold ) {
			assert( !singleFarReach );
			// The trace segment might be very long, test PVS first
			if( trap_inPVS( origin, reach.start ) ) {
				SolidWorldTrace( &trace, origin, reach.start );
				if( trace.fraction == 1.0f ) {
					singleFarReach = &reach;
				}
			}
			break;
		}

		SolidWorldTrace( &trace, origin, reach.start );
		if( trace.fraction != 1.0f ) {
			break;
		}

		if( reach.areanum == navTargetAreaNum ) {
			endsInNavTargetArea = true;
		}

		Vec3 reachDir( reach.start );
		reachDir -= origin;
		reachDir.NormalizeFast();
		// Add a reach dir to the dirs list (be optimistic to avoid extra temporaries)
		if( numReachFound ) {
			// Limit dirs angular spread checking against a first found dir as a base dir
			if( reachDir.Dot( firstReachDir ) < 0.5f ) {
				break;
			}
		} else {
			reachDir.CopyTo( firstReachDir );
		}

		intendedLookDir += reachDir;
		numReachFound++;
	}

	if( !numReachFound ) {
		if( !singleFarReach ) {
			if( context->IsInNavTargetArea() ) {
				intendedLookDir.Set( context->NavTargetOrigin() );
				intendedLookDir -= origin;
				intendedLookDir.NormalizeFast();
				return true;
			}
			return false;
		}

		intendedLookDir.Set( singleFarReach->start );
		intendedLookDir -= origin;
		intendedLookDir.NormalizeFast();
		return true;
	}

	if( endsInNavTargetArea ) {
		Vec3 navTargetOrigin( context->NavTargetOrigin() );
		SolidWorldTrace( &trace, origin, navTargetOrigin.Data() );
		if( trace.fraction == 1.0f ) {
			// Add the direction to the nav target to the interpolated dir
			Vec3 toTargetDir( navTargetOrigin );
			toTargetDir -= origin;
			toTargetDir.NormalizeFast();
			if( toTargetDir.Dot( firstReachDir ) > 0.5f ) {
				intendedLookDir += toTargetDir;
				// Count it as an additional interpolated reachability
				numReachFound++;
			}
		}
	}

	// If there were more than a single reach dir added to the look dir, it requires normalization
	if( numReachFound > 1 ) {
		intendedLookDir.NormalizeFast();
	}

	return true;
}

void BotBunnyInterpolatingReachChainMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, &self->ai->botRef->bunnyStraighteningReachChainMovementAction ) ) {
		return;
	}

	if( !CheckCommonBunnyingActionPreconditions( context ) ) {
		return;
	}

	context->record->botInput.isUcmdSet = true;
	// Continue interpolating while a next reach has these travel types
	const int compatibleReachTypes[4] = { TRAVEL_WALK, TRAVEL_WALKOFFLEDGE, TRAVEL_JUMP, TRAVEL_STRAFEJUMP };
	// Stop interpolating on these reach types but include a reach start in interpolation
	const int allowedEndReachTypes[4] = { TRAVEL_TELEPORT, TRAVEL_JUMPPAD, TRAVEL_ELEVATOR, TRAVEL_LADDER };
	ReachChainInterpolator interpolator;
	interpolator.stopAtDistance = 256;
	interpolator.SetCompatibleReachTypes( compatibleReachTypes, sizeof( compatibleReachTypes ) / sizeof( int ) );
	interpolator.SetAllowedEndReachTypes( allowedEndReachTypes, sizeof( allowedEndReachTypes ) / sizeof( int ) );
	if( !interpolator.Exec( context ) ) {
		context->SetPendingRollback();
		Debug( "Cannot apply action: cannot interpolate reach chain\n" );
		return;
	}

	context->record->botInput.SetIntendedLookDir( interpolator.Result(), true );

	if( !SetupBunnying( context->record->botInput.IntendedLookDir(), context ) ) {
		context->SetPendingRollback();
		return;
	}
}

void BotBunnyTestingMultipleLookDirsMovementAction::BeforePlanning() {
	BotGenericRunBunnyingMovementAction::BeforePlanning();
	currSuggestedLookDirNum = 0;
	suggestedLookDirs.clear();
	dirsBaseAreas.clear();

	// Ensure the suggested action has been set in subtype constructor
	Assert( suggestedAction );
}

void BotBunnyTestingMultipleLookDirsMovementAction::OnApplicationSequenceStarted( BotMovementPredictionContext *ctx ) {
	BotGenericRunBunnyingMovementAction::OnApplicationSequenceStarted( ctx );
	// If there is no dirs tested yet
	if( currSuggestedLookDirNum == 0 ) {
		suggestedLookDirs.clear();
		dirsBaseAreas.clear();
		if( ctx->NavTargetAasAreaNum() ) {
			SaveSuggestedLookDirs( ctx );
		}
	}
}

void BotBunnyTestingMultipleLookDirsMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
																				  SequenceStopReason stopReason,
																				  unsigned stoppedAtFrameIndex ) {
	BotGenericRunBunnyingMovementAction::OnApplicationSequenceStopped( context, stopReason, stoppedAtFrameIndex );
	// If application sequence succeeded
	if( stopReason != FAILED ) {
		if( stopReason != DISABLED ) {
			currSuggestedLookDirNum = 0;
		}
		return;
	}

	// If the action has been disabled due to prediction stack overflow
	if( this->isDisabledForPlanning ) {
		return;
	}

	// If rolling back is available for the current suggested dir
	if( disabledForApplicationFrameIndex != context->savepointTopOfStackIndex ) {
		return;
	}

	// If another suggested look dir exists
	if( currSuggestedLookDirNum + 1 < suggestedLookDirs.size() ) {
		currSuggestedLookDirNum++;
		// Allow the action application after the context rollback to savepoint
		this->disabledForApplicationFrameIndex = std::numeric_limits<unsigned>::max();
		// Ensure this action will be used after rollback
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}
	// Otherwise use the first dir in a new sequence started on some other frame
	currSuggestedLookDirNum = 0;
}

inline float SuggestObstacleAvoidanceCorrectionFraction( const BotMovementPredictionContext *context ) {
	// Might be negative!
	float speedOverRunSpeed = context->movementState->entityPhysicsState.Speed() - context->GetRunSpeed();
	if( speedOverRunSpeed > 500.0f ) {
		return 0.15f;
	}
	return 0.35f - 0.20f * speedOverRunSpeed / 500.0f;
}

void BotBunnyTestingMultipleLookDirsMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	if( !GenericCheckIsActionEnabled( context, suggestedAction ) ) {
		return;
	}

	// Do this test after GenericCheckIsActionEnabled(), otherwise disabledForApplicationFrameIndex does not get tested
	if( currSuggestedLookDirNum >= suggestedLookDirs.size() ) {
		Debug( "There is no suggested look dirs yet/left\n" );
		context->SetPendingRollback();
		return;
	}

	if( !CheckCommonBunnyingActionPreconditions( context ) ) {
		return;
	}

	context->record->botInput.SetIntendedLookDir( suggestedLookDirs[currSuggestedLookDirNum], true );

	if( isTryingObstacleAvoidance ) {
		context->TryAvoidJumpableObstacles( SuggestObstacleAvoidanceCorrectionFraction( context ) );
	}

	if( !SetupBunnying( context->record->botInput.IntendedLookDir(), context ) ) {
		context->SetPendingRollback();
		return;
	}
}

inline AreaAndScore *BotBunnyTestingMultipleLookDirsMovementAction::TakeBestCandidateAreas( AreaAndScore *inputBegin,
																							AreaAndScore *inputEnd,
																							unsigned maxAreas ) {
	Assert( inputEnd >= inputBegin );
	const uintptr_t numAreas = inputEnd - inputBegin;
	const uintptr_t numResultAreas = numAreas < maxAreas ? numAreas : maxAreas;

	// Move best area to the array head, repeat it for the array tail
	for( uintptr_t i = 0, end = numResultAreas; i < end; ++i ) {
		// Set the start area as a current best one
		auto &startArea = *( inputBegin + i );
		for( uintptr_t j = i + 1; j < numAreas; ++j ) {
			auto &currArea = *( inputBegin + j );
			// If current area is better (<) than the start one, swap these areas
			if( currArea.score < startArea.score ) {
				std::swap( currArea, startArea );
			}
		}
	}

	return inputBegin + numResultAreas;
}

void BotBunnyTestingMultipleLookDirsMovementAction::SaveCandidateAreaDirs( BotMovementPredictionContext *context,
																		   AreaAndScore *candidateAreasBegin,
																		   AreaAndScore *candidateAreasEnd ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const int navTargetAreaNum = context->NavTargetAasAreaNum();
	const auto *aasAreas = AiAasWorld::Instance()->Areas();

	AreaAndScore *takenAreasBegin = candidateAreasBegin;
	unsigned maxAreas = suggestedLookDirs.capacity() - suggestedLookDirs.size();
	AreaAndScore *takenAreasEnd = TakeBestCandidateAreas( candidateAreasBegin, candidateAreasEnd, maxAreas );

	for( auto iter = takenAreasBegin; iter < takenAreasEnd; ++iter ) {
		int areaNum = ( *iter ).areaNum;
		void *mem = suggestedLookDirs.unsafe_grow_back();
		dirsBaseAreas.push_back( areaNum );
		if( areaNum != navTargetAreaNum ) {
			Vec3 *toAreaDir = new(mem)Vec3( aasAreas[areaNum].center );
			toAreaDir->Z() = aasAreas[areaNum].mins[2] + 32.0f;
			*toAreaDir -= entityPhysicsState.Origin();
			toAreaDir->Z() *= Z_NO_BEND_SCALE;
			toAreaDir->NormalizeFast();
		} else {
			Vec3 *toTargetDir = new(mem)Vec3( context->NavTargetOrigin() );
			*toTargetDir -= entityPhysicsState.Origin();
			toTargetDir->NormalizeFast();
		}
	}
}

BotBunnyStraighteningReachChainMovementAction::BotBunnyStraighteningReachChainMovementAction( Bot *bot_ )
	: BotBunnyTestingMultipleLookDirsMovementAction( bot_, NAME, COLOR_RGB( 0, 192, 0 ) ) {
	supportsObstacleAvoidance = true;
	maxSuggestedLookDirs = 3;
	// The constructor cannot be defined in the header due to this bot member access
	suggestedAction = &bot_->bunnyToBestShortcutAreaMovementAction;
}

void BotBunnyStraighteningReachChainMovementAction::SaveSuggestedLookDirs( BotMovementPredictionContext *context ) {
	Assert( suggestedLookDirs.empty() );
	Assert( dirsBaseAreas.empty() );
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const int navTargetAasAreaNum = context->NavTargetAasAreaNum();
	Assert( navTargetAasAreaNum );

	// Do not modify look vec in this case (we assume its set to nav target)
	if( context->IsInNavTargetArea() ) {
		void *mem = suggestedLookDirs.unsafe_grow_back();
		dirsBaseAreas.push_back( navTargetAasAreaNum );
		Vec3 *toTargetDir = new(mem)Vec3( context->NavTargetOrigin() );
		*toTargetDir -= entityPhysicsState.Origin();
		toTargetDir->NormalizeFast();
		return;
	}

	const auto &nextReachChain = context->NextReachChain();
	if( nextReachChain.empty() ) {
		Debug( "Cannot straighten look vec: next reach. chain is empty\n" );
		return;
	}

	const AiAasWorld *aasWorld = AiAasWorld::Instance();
	const aas_reachability_t *aasReachabilities = aasWorld->Reachabilities();

	int lastValidReachIndex = -1;
	constexpr unsigned MAX_TESTED_REACHABILITIES = 16U;
	const unsigned maxTestedReachabilities = std::min( MAX_TESTED_REACHABILITIES, nextReachChain.size() );
	const aas_reachability_t *reachStoppedAt = nullptr;
	for( unsigned i = 0; i < maxTestedReachabilities; ++i ) {
		const auto &reach = aasReachabilities[nextReachChain[i].ReachNum()];
		if( reach.traveltype != TRAVEL_WALK && reach.traveltype != TRAVEL_WALKOFFLEDGE ) {
			if( reach.traveltype != TRAVEL_JUMP && reach.traveltype != TRAVEL_STRAFEJUMP ) {
				reachStoppedAt = &reach;
				break;
			}
		}

		lastValidReachIndex++;
	}

	if( lastValidReachIndex < 0 || lastValidReachIndex >= (int)maxTestedReachabilities ) {
		Debug( "There were no supported for bunnying reachabilities\n" );
		return;
	}
	Assert( lastValidReachIndex < (int)maxTestedReachabilities );

	AreaAndScore candidates[MAX_TESTED_REACHABILITIES];
	AreaAndScore *candidatesEnd = SelectCandidateAreas( context, candidates, (unsigned)lastValidReachIndex );

	SaveCandidateAreaDirs( context, candidates, candidatesEnd );

	if( suggestedLookDirs.size() == maxSuggestedLookDirs ) {
		return;
	}

	// If there is a trigger entity in the reach chain, try keep looking at it
	if( reachStoppedAt ) {
		int travelType = reachStoppedAt->traveltype;
		if( travelType == TRAVEL_TELEPORT || travelType == TRAVEL_JUMPPAD || travelType == TRAVEL_ELEVATOR ) {
			void *mem = suggestedLookDirs.unsafe_grow_back();
			// reachStoppedAt->areanum is an area num of reach destination, not the trigger itself.
			// Saving or restoring the trigger area num does not seem worth this minor case.
			Vec3 *toTriggerDir = new(mem)Vec3( reachStoppedAt->start );
			*toTriggerDir -= entityPhysicsState.Origin();
			toTriggerDir->NormalizeFast();
			return;
		}
	}

	if( suggestedLookDirs.size() == 0 ) {
		Debug( "Cannot straighten look vec: cannot find a suitable area in reach. chain to aim for\n" );
	}
}

AreaAndScore *BotBunnyStraighteningReachChainMovementAction::SelectCandidateAreas( BotMovementPredictionContext *context,
																				   AreaAndScore *candidatesBegin,
																				   unsigned lastValidReachIndex ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const auto &nextReachChain = context->NextReachChain();
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasReachabilities = aasWorld->Reachabilities();
	const auto *aasAreas = aasWorld->Areas();
	const auto *aasAreaSettings = aasWorld->AreaSettings();
	const int navTargetAasAreaNum = context->NavTargetAasAreaNum();

	// Do not make it speed-depended, it leads to looping/jitter!
	const float distanceThreshold = 256.0f + 512.0f * self->ai->botRef->Skill();

	AreaAndScore *candidatesPtr = candidatesBegin;
	float minScore = 0.0f;

	Vec3 traceStartPoint( entityPhysicsState.Origin() );
	traceStartPoint.Z() += playerbox_stand_viewheight;
	trace_t trace;
	for( int i = lastValidReachIndex; i >= 0; --i ) {
		const int reachNum = nextReachChain[i].ReachNum();
		const aas_reachability_t &reachability = aasReachabilities[reachNum];
		const int areaNum = reachability.areanum;
		const aas_area_t &area = aasAreas[areaNum];
		const aas_areasettings_t &areaSettings = aasAreaSettings[areaNum];
		if( areaSettings.contents & AREACONTENTS_DONOTENTER ) {
			continue;
		}

		int areaFlags = areaSettings.areaflags;
		if( !( areaFlags & AREA_GROUNDED ) ) {
			continue;
		}
		if( areaFlags & AREA_DISABLED ) {
			continue;
		}

		Vec3 areaPoint( area.center[0], area.center[1], area.mins[2] + 4.0f );
		// Skip areas higher than the bot (to allow moving on a stairs chain, we test distance/height ratio)
		if( area.mins[2] > entityPhysicsState.Origin()[2] ) {
			float distance = areaPoint.FastDistance2DTo( entityPhysicsState.Origin() );
			if( area.mins[2] - entityPhysicsState.Origin()[2] > M_SQRT1_2 * distance ) {
				continue;
			}
		}

		const float squareDistanceToArea = DistanceSquared( area.center, entityPhysicsState.Origin() );
		// Skip way too close areas (otherwise the bot might fall into endless looping)
		if( squareDistanceToArea < SQUARE( 0.4f * distanceThreshold ) ) {
			continue;
		}

		// Skip way too far areas (this is mainly an optimization for the following SolidWorldTrace() call)
		if( squareDistanceToArea > SQUARE( distanceThreshold ) ) {
			continue;
		}

		// Compute score first to cut off expensive tracing
		const float prevMinScore = minScore;
		// Give far areas greater initial score
		float score = 999999.0f;
		if( areaNum != navTargetAasAreaNum ) {
			// Avoid a division by zero by shifting both nominator and denominator by 1.
			// Note that it slightly shifts score too but there are no upper bounds for the score.
			score = 0.5f + 0.5f * ( (float)( i + 1 ) / (float)( lastValidReachIndex + 1 ) );
			// Try skip "junk" areas (sometimes these areas cannot be avoided in the shortest path)
			if( areaFlags & AREA_JUNK ) {
				score *= 0.1f;
			}
			// Give ledge areas a bit smaller score (sometimes these areas cannot be avoided in the shortest path)
			if( areaFlags & AREA_LEDGE ) {
				score *= 0.7f;
			}
			// Prefer not bounded by walls areas to avoid bumping into walls
			if( !( areaFlags & AREA_WALL ) ) {
				score *= 1.6f;
			}

			// Do not test lower score areas if there is already enough tested candidates
			if( score > minScore ) {
				minScore = score;
			} else if( candidatesPtr - candidatesBegin >= (ptrdiff_t)maxSuggestedLookDirs ) {
				continue;
			}
		}

		// Make sure the bot can see the ground
		SolidWorldTrace( &trace, traceStartPoint.Data(), areaPoint.Data() );
		if( trace.fraction != 1.0f ) {
			// Restore minScore (it might have been set to the value of the rejected area score on this loop step)
			minScore = prevMinScore;
			continue;
		}

		new ( candidatesPtr++ )AreaAndScore( areaNum, score );
	}

	return candidatesPtr;
}

BotBunnyToBestShortcutAreaMovementAction::BotBunnyToBestShortcutAreaMovementAction( Bot *bot_ )
	: BotBunnyTestingMultipleLookDirsMovementAction( bot_, NAME, COLOR_RGB( 255, 64, 0 ) ) {
	supportsObstacleAvoidance = false;
	maxSuggestedLookDirs = 2;
	// The constructor cannot be defined in the header due to this bot member access
	suggestedAction = &bot_->walkOrSlideInterpolatingReachChainMovementAction;
}

void BotBunnyToBestShortcutAreaMovementAction::SaveSuggestedLookDirs( BotMovementPredictionContext *context ) {
	Assert( suggestedLookDirs.empty() );
	Assert( context->NavTargetAasAreaNum() );

	int startTravelTime = FindActualStartTravelTime( context );
	if( !startTravelTime ) {
		return;
	}

	AreaAndScore candidates[MAX_BBOX_AREAS];
	AreaAndScore *candidatesEnd = SelectCandidateAreas( context, candidates, startTravelTime );
	SaveCandidateAreaDirs( context, candidates, candidatesEnd );
}

inline int BotBunnyToBestShortcutAreaMovementAction::FindActualStartTravelTime( BotMovementPredictionContext *context ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const auto *aasRouteCache = self->ai->botRef->routeCache;
	const int travelFlags = self->ai->botRef->PreferredTravelFlags();
	const int navTargetAreaNum = context->NavTargetAasAreaNum();

	int startAreaNums[2] = { entityPhysicsState.DroppedToFloorAasAreaNum(), entityPhysicsState.CurrAasAreaNum() };
	int startTravelTimes[2];

	int j = 0;
	for( int i = 0, end = ( startAreaNums[0] != startAreaNums[1] ) ? 2 : 1; i < end; ++i ) {
		if( int travelTime = aasRouteCache->TravelTimeToGoalArea( startAreaNums[i], navTargetAreaNum, travelFlags ) ) {
			startTravelTimes[j++] = travelTime;
		}
	}

	switch( j ) {
		case 2:
			return std::min( startTravelTimes[0], startTravelTimes[1] );
		case 1:
			return startTravelTimes[0];
		default:
			return 0;
	}
}

inline int BotBunnyToBestShortcutAreaMovementAction::FindBBoxAreas( BotMovementPredictionContext *context,
																	int *areaNums, int maxAreas ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	// Do not make it speed-depended, it leads to looping/jitter!
	const float side = 256.0f + 256.0f * self->ai->botRef->Skill();

	Vec3 boxMins( -side, -side, -0.33f * side );
	Vec3 boxMaxs( +side, +side, 0 );
	boxMins += entityPhysicsState.Origin();
	boxMaxs += entityPhysicsState.Origin();

	return AiAasWorld::Instance()->BBoxAreas( boxMins, boxMaxs, areaNums, maxAreas );
}

AreaAndScore *BotBunnyToBestShortcutAreaMovementAction::SelectCandidateAreas( BotMovementPredictionContext *context,
																			  AreaAndScore *candidatesBegin,
																			  int startTravelTime ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const auto *aasWorld = AiAasWorld::Instance();
	const auto *aasRouteCache = self->ai->botRef->routeCache;
	const auto *aasAreas = aasWorld->Areas();
	const auto *aasAreaSettings = aasWorld->AreaSettings();

	const int navTargetAreaNum = context->NavTargetAasAreaNum();
	const int travelFlags = self->ai->botRef->PreferredTravelFlags();
	const int currAreaNum = entityPhysicsState.CurrAasAreaNum();
	const int droppedToFloorAreaNum = entityPhysicsState.DroppedToFloorAasAreaNum();

	const auto &prevTestedAction = self->ai->botRef->bunnyStraighteningReachChainMovementAction;
	Assert( prevTestedAction.suggestedAction == this );
	const auto &prevTestedAreas = prevTestedAction.dirsBaseAreas;

	const float speed = entityPhysicsState.Speed();
	Vec3 velocityDir( entityPhysicsState.Velocity() );
	velocityDir *= 1.0f / speed;

	int minTravelTimeSave = 0;
	AreaAndScore *candidatesPtr = candidatesBegin;

	Vec3 traceStartPoint( entityPhysicsState.Origin() );
	traceStartPoint.Z() += playerbox_stand_viewheight;
	trace_t trace;

	int bboxAreaNums[MAX_BBOX_AREAS];
	int numBBoxAreas = FindBBoxAreas( context, bboxAreaNums, MAX_BBOX_AREAS );
	for( int i = 0; i < numBBoxAreas; ++i ) {
		const int areaNum = bboxAreaNums[i];
		if( areaNum == droppedToFloorAreaNum || areaNum == currAreaNum ) {
			continue;
		}

		const auto &areaSettings = aasAreaSettings[areaNum];
		int areaFlags = areaSettings.areaflags;
		if( !( areaFlags & AREA_GROUNDED ) ) {
			continue;
		}
		if( areaFlags & ( AREA_JUNK | AREA_DISABLED ) ) {
			continue;
		}
		if( areaSettings.contents & ( AREACONTENTS_WATER | AREACONTENTS_LAVA | AREACONTENTS_SLIME | AREACONTENTS_DONOTENTER ) ) {
			continue;
		}

		// Skip areas that have lead to the previous action failure
		if( prevTestedAction.disabledForApplicationFrameIndex == context->topOfStackIndex ) {
			if( std::find( prevTestedAreas.begin(), prevTestedAreas.end(), areaNum ) != prevTestedAreas.end() ) {
				continue;
			}
		}

		const auto &area = aasAreas[areaNum];
		Vec3 areaPoint( area.center[0], area.center[1], area.mins[2] + 4.0f );
		// Skip areas higher than the bot (to allow moving on a stairs chain, we test distance/height ratio)
		if( area.mins[2] > entityPhysicsState.Origin()[2] ) {
			float distance = areaPoint.FastDistance2DTo( entityPhysicsState.Origin() );
			if( area.mins[2] - entityPhysicsState.Origin()[2] > M_SQRT1_2 * distance ) {
				continue;
			}
		}

		Vec3 toAreaDir( areaPoint );
		toAreaDir -= entityPhysicsState.Origin();
		float distanceToArea = toAreaDir.LengthFast();

		// Reject areas that are very close to the bot.
		// This for example helps to skip some junk areas in stair-like environment.
		if( distanceToArea < 96 ) {
			continue;
		}

		toAreaDir *= 1.0f / distanceToArea;
		// Reject areas behind/not in front depending on speed
		float speedDotFactor = -1.0f + 2 * 0.99f * BoundedFraction( speed, 900 );
		if( velocityDir.Dot( toAreaDir ) < speedDotFactor ) {
			continue;
		}

		int areaToTargetAreaTravelTime = aasRouteCache->TravelTimeToGoalArea( areaNum, navTargetAreaNum, travelFlags );
		if( !areaToTargetAreaTravelTime ) {
			continue;
		}

		// Time saved on traveling to goal
		const int travelTimeSave = startTravelTime - areaToTargetAreaTravelTime;
		// Try to reject non-feasible areas to cut off expensive trace computation
		if( travelTimeSave <= 0 ) {
			continue;
		}

		const int prevMinTravelTimeSave = minTravelTimeSave;
		// Do not test lower score areas if there is already enough tested candidates
		if( travelTimeSave > minTravelTimeSave ) {
			minTravelTimeSave = travelTimeSave;
		} else if( candidatesPtr - candidatesBegin >= (ptrdiff_t)maxSuggestedLookDirs ) {
			continue;
		}

		SolidWorldTrace( &trace, traceStartPoint.Data(), areaPoint.Data() );
		if( trace.fraction != 1.0f ) {
			// Restore minTravelTimeSave (it might has been set to the value of the rejected area on this loop step)
			minTravelTimeSave = prevMinTravelTimeSave;
			continue;
		}

		// We DO not check whether traveling to the best nearby area takes less time
		// than time traveling from best area to nav target saves.
		// Otherwise only areas in the reachability chain conform to the condition if the routing algorithm works properly.
		// We hope for shortcuts the routing algorithm is not aware of.

		new( candidatesPtr++ )AreaAndScore( areaNum, travelTimeSave );
	}

	return candidatesPtr;
}

void BotWalkCarefullyMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	BotBaseMovementAction *suggestedAction = &DefaultBunnyAction();
	if( !GenericCheckIsActionEnabled( context, suggestedAction ) ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( !entityPhysicsState.GroundEntity() && entityPhysicsState.HeightOverGround() > 4.0f ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = suggestedAction;
		Debug( "Cannot apply action: the bot is quite high above the ground\n" );
		return;
	}

	const int currAasAreaNum = context->CurrAasAreaNum();
	if( !currAasAreaNum ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DummyAction();
		Debug( "Cannot apply action: current AAS area num is undefined\n" );
		return;
	}

	const int navTargetAasAreaNum = context->NavTargetAasAreaNum();
	if( !navTargetAasAreaNum ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DummyAction();
		Debug( "Cannot apply action: nav target AAS area num is undefined\n" );
		return;
	}

	if( self->ai->botRef->ShouldMoveCarefully() || self->ai->botRef->ShouldBeSilent() ) {
		context->SetDefaultBotInput();
		context->record->botInput.ClearMovementDirections();
		context->record->botInput.SetForwardMovement( 1 );
		if( self->ai->botRef->ShouldMoveCarefully() || context->IsInNavTargetArea() ) {
			context->record->botInput.SetWalkButton( true );
		}
		return;
	}

	// First test whether there is a gap in front of the bot
	// (if this test is omitted, bots would use this no-jumping action instead of jumping over gaps and fall down)

	const float zOffset = playerbox_stand_mins[2] - 32.0f - entityPhysicsState.HeightOverGround();
	Vec3 frontTestPoint( entityPhysicsState.ForwardDir() );
	frontTestPoint *= 8.0f;
	frontTestPoint += entityPhysicsState.Origin();
	frontTestPoint.Z() += zOffset;

	trace_t trace;
	SolidWorldTrace( &trace, entityPhysicsState.Origin(), frontTestPoint.Data() );
	if( trace.fraction == 1.0f ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = suggestedAction;
		return;
	}

	int hazardContentsMask = CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_DONOTENTER;
	// Prevent touching a water if not bot is not already walking in it
	if( !entityPhysicsState.waterLevel ) {
		hazardContentsMask |= CONTENTS_WATER;
	}

	// An action might be applied if there are gap or hazard from both sides
	// or from a single side and there is non-walkable plane from an other side
	int gapSidesNum = 2;
	int hazardSidesNum = 0;

	const float sideOffset = playerbox_stand_maxs[1] - playerbox_stand_mins[1];
	Vec3 sidePoints[2] = { Vec3( entityPhysicsState.RightDir() ), -Vec3( entityPhysicsState.RightDir() ) };
	for( int i = 0; i < 2; ++i ) {
		sidePoints[i] *= sideOffset;
		sidePoints[i] += entityPhysicsState.Origin();
		sidePoints[i].Z() += zOffset;
		SolidWorldTrace( &trace, entityPhysicsState.Origin(), sidePoints[i].Data() );
		// Put likely case first
		if( trace.fraction != 1.0f ) {
			// Put likely case first
			if( !( trace.contents & hazardContentsMask ) ) {
				if( ISWALKABLEPLANE( &trace.plane ) ) {
					context->cannotApplyAction = true;
					context->actionSuggestedByAction = suggestedAction;
					Debug( "Cannot apply action: there is no gap, wall or hazard to the right below\n" );
					return;
				}
			} else {
				hazardSidesNum++;
			}

			gapSidesNum--;
		}
	}

	if( !( hazardSidesNum + gapSidesNum ) ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = suggestedAction;
		Debug( "Cannot apply action: there are just two walls from both sides, no gap or hazard\n" );
		return;
	}

	context->SetDefaultBotInput();
	context->record->botInput.ClearMovementDirections();
	context->record->botInput.SetForwardMovement( 1 );
	// Be especially careful when there is a nearby hazard area
	if( hazardSidesNum ) {
		context->record->botInput.SetWalkButton( true );
	}
}

bool BotGenericRunBunnyingMovementAction::GenericCheckIsActionEnabled( BotMovementPredictionContext *context,
																	   BotBaseMovementAction *suggestedAction ) {
	if( !BotBaseMovementAction::GenericCheckIsActionEnabled( context, suggestedAction ) ) {
		return false;
	}

	if( this->disabledForApplicationFrameIndex != context->topOfStackIndex ) {
		return true;
	}

	Debug( "Cannot apply action: the action has been disabled for application on frame %d\n", context->topOfStackIndex );
	context->sequenceStopReason = DISABLED;
	context->cannotApplyAction = true;
	context->actionSuggestedByAction = suggestedAction;
	return false;
}

bool BotGenericRunBunnyingMovementAction::CheckCommonBunnyingActionPreconditions( BotMovementPredictionContext *context ) {
	int currAasAreaNum = context->CurrAasAreaNum();
	if( !currAasAreaNum ) {
		Debug( "Cannot apply action: curr AAS area num is undefined\n" );
		context->SetPendingRollback();
		return false;
	}

	int navTargetAasAreaNum = context->NavTargetAasAreaNum();
	if( !navTargetAasAreaNum ) {
		Debug( "Cannot apply action: nav target AAS area num is undefined\n" );
		context->SetPendingRollback();
		return false;
	}

	if( self->ai->botRef->GetSelectedEnemies().AreValid() && self->ai->botRef->GetMiscTactics().shouldKeepXhairOnEnemy ) {
		if( !context->MayHitWhileRunning().CanHit() ) {
			Debug( "Cannot apply action: cannot hit an enemy while keeping the crosshair on it is required\n" );
			context->SetPendingRollback();
			this->isDisabledForPlanning = true;
			return false;
		}
	}

	// Cannot find a next reachability in chain while it should exist
	// (looks like the bot is too high above the ground)
	if( !context->IsInNavTargetArea() && !context->NextReachNum() ) {
		Debug( "Cannot apply action: next reachability is undefined and bot is not in the nav target area\n" );
		context->SetPendingRollback();
		return false;
	}

	if( !( context->currPlayerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_JUMP ) ) {
		Debug( "Cannot apply action: bot does not have the jump movement feature\n" );
		context->SetPendingRollback();
		this->isDisabledForPlanning = true;
		return false;
	}

	if( self->ai->botRef->ShouldBeSilent() ) {
		Debug( "Cannot apply action: bot should be silent\n" );
		context->SetPendingRollback();
		this->isDisabledForPlanning = true;
		return false;
	}

	return true;
}

void BotGenericRunBunnyingMovementAction::SetupCommonBunnyingInput( BotMovementPredictionContext *context ) {
	const auto *pmoveStats = context->currPlayerState->pmove.stats;

	auto *botInput = &context->record->botInput;
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;

	botInput->SetForwardMovement( 1 );
	const auto &hitWhileRunningTestResult = context->MayHitWhileRunning();
	if( self->ai->botRef->GetSelectedEnemies().AreValid() && self->ai->botRef->ShouldKeepXhairOnEnemy() ) {
		Assert( hitWhileRunningTestResult.CanHit() );
	}

	botInput->canOverrideLookVec = hitWhileRunningTestResult.canHitAsIs;
	botInput->canOverridePitch = true;

	if( ( pmoveStats[PM_STAT_FEATURES] & PMFEAT_DASH ) && !pmoveStats[PM_STAT_DASHTIME] && !pmoveStats[PM_STAT_STUN] ) {
		bool shouldDash = false;
		if( entityPhysicsState.Speed() < context->GetDashSpeed() && entityPhysicsState.GroundEntity() ) {
			// Prevent dashing into obstacles
			auto &traceCache = context->EnvironmentTraceCache();
			traceCache.TestForResultsMask( context, traceCache.FullHeightMask( traceCache.FRONT ) );
			if( traceCache.FullHeightFrontTrace().trace.fraction == 1.0f ) {
				shouldDash = true;
			}
		}

		if( shouldDash ) {
			botInput->SetSpecialButton( true );
			botInput->SetUpMovement( 0 );
			// Predict dash precisely
			context->predictionStepMillis = 16;
		} else {
			botInput->SetUpMovement( 1 );
		}
	} else {
		if( entityPhysicsState.Speed() < context->GetRunSpeed() ) {
			botInput->SetUpMovement( 0 );
		} else {
			botInput->SetUpMovement( 1 );
		}
	}
}

bool BotGenericRunBunnyingMovementAction::SetupBunnying( const Vec3 &intendedLookVec,
														 BotMovementPredictionContext *context ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	auto *botInput = &context->record->botInput;

	Vec3 toTargetDir2D( intendedLookVec );
	toTargetDir2D.Z() = 0;

	Vec3 velocityDir2D( entityPhysicsState.Velocity() );
	velocityDir2D.Z() = 0;

	float squareSpeed2D = entityPhysicsState.SquareSpeed2D();
	float toTargetDir2DSqLen = toTargetDir2D.SquaredLength();

	if( squareSpeed2D > 1.0f ) {
		SetupCommonBunnyingInput( context );

		velocityDir2D *= 1.0f / entityPhysicsState.Speed2D();

		if( toTargetDir2DSqLen > 0.1f ) {
			const auto &oldPMove = context->oldPlayerState->pmove;
			const auto &newPMove = context->currPlayerState->pmove;
			// If not skimming
			if( !( newPMove.skim_time && newPMove.skim_time != oldPMove.skim_time ) ) {
				toTargetDir2D *= Q_RSqrt( toTargetDir2DSqLen );
				float velocityDir2DDotToTargetDir2D = velocityDir2D.Dot( toTargetDir2D );
				if( velocityDir2DDotToTargetDir2D > 0.0f ) {
					// Apply cheating acceleration
					CheatingAccelerate( context, velocityDir2DDotToTargetDir2D );
				}
				if( velocityDir2DDotToTargetDir2D < STRAIGHT_MOVEMENT_DOT_THRESHOLD ) {
					// Correct trajectory using cheating aircontrol
					CheatingCorrectVelocity( context, velocityDir2DDotToTargetDir2D, toTargetDir2D );
				}
			}
		}
	}
	// Looks like the bot is in air falling vertically
	else if( !entityPhysicsState.GroundEntity() ) {
		// Release keys to allow full control over view in air without affecting movement
		if( self->ai->botRef->ShouldAttack() && CanFlyAboveGroundRelaxed( context ) ) {
			botInput->ClearMovementDirections();
			botInput->canOverrideLookVec = true;
		}
		return true;
	} else {
		SetupCommonBunnyingInput( context );
		return true;
	}

	if( self->ai->botRef->ShouldAttack() && CanFlyAboveGroundRelaxed( context ) ) {
		botInput->ClearMovementDirections();
		botInput->canOverrideLookVec = true;
	}

	// Skip dash and WJ near triggers and nav targets to prevent missing a trigger/nav target
	const int nextReachNum = context->NextReachNum();
	if( !nextReachNum ) {
		// Preconditions check must not allow bunnying outside of nav target area having an empty reach. chain
		Assert( context->IsInNavTargetArea() );
		botInput->SetSpecialButton( false );
		botInput->canOverrideLookVec = false;
		botInput->canOverridePitch = false;
		return true;
	}

	switch( AiAasWorld::Instance()->Reachabilities()[nextReachNum].traveltype ) {
		case TRAVEL_TELEPORT:
		case TRAVEL_JUMPPAD:
		case TRAVEL_ELEVATOR:
		case TRAVEL_LADDER:
		case TRAVEL_BARRIERJUMP:
			botInput->SetSpecialButton( false );
			botInput->canOverrideLookVec = false;
			botInput->canOverridePitch = true;
			return true;
		default:
			if( context->IsCloseToNavTarget() ) {
				botInput->SetSpecialButton( false );
				botInput->canOverrideLookVec = false;
				botInput->canOverridePitch = false;
				return true;
			}
	}

	if( ShouldPrepareForCrouchSliding( context, 8.0f ) ) {
		botInput->SetUpMovement( -1 );
		context->predictionStepMillis = 16;
	}

	TrySetWalljump( context );
	return true;
}

bool BotGenericRunBunnyingMovementAction::CanFlyAboveGroundRelaxed( const BotMovementPredictionContext *context ) const {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.GroundEntity() ) {
		return false;
	}

	float desiredHeightOverGround = 0.3f * AI_JUMPABLE_HEIGHT;
	return entityPhysicsState.HeightOverGround() >= desiredHeightOverGround;
}

inline void BotGenericRunBunnyingMovementAction::TrySetWalljump( BotMovementPredictionContext *context ) {
	if( !CanSetWalljump( context ) ) {
		return;
	}

	auto *botInput = &context->record->botInput;
	botInput->ClearMovementDirections();
	botInput->SetSpecialButton( true );
	// Predict a frame precisely for walljumps
	context->predictionStepMillis = 16;
}

#define TEST_TRACE_RESULT_NORMAL( traceResult )                              \
	do                                                                         \
	{                                                                          \
		if( traceResult.trace.fraction != 1.0f )                                \
		{                                                                      \
			if( velocity2DDir.Dot( traceResult.trace.plane.normal ) < -0.5f ) {     \
				return false; }                                                  \
			hasGoodWalljumpNormal = true;                                      \
		}                                                                      \
	} while( 0 )

bool BotGenericRunBunnyingMovementAction::CanSetWalljump( BotMovementPredictionContext *context ) const {
	const short *pmoveStats = context->currPlayerState->pmove.stats;
	if( !( pmoveStats[PM_STAT_FEATURES] & PMFEAT_WALLJUMP ) ) {
		return false;
	}

	if( pmoveStats[PM_STAT_WJTIME] ) {
		return false;
	}

	if( pmoveStats[PM_STAT_STUN] ) {
		return false;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.GroundEntity() ) {
		return false;
	}

	if( entityPhysicsState.HeightOverGround() < 8.0f && entityPhysicsState.Velocity()[2] <= 0 ) {
		return false;
	}

	float speed2D = entityPhysicsState.Speed2D();
	// The 2D speed is too low for walljumping
	if( speed2D < 400 ) {
		return false;
	}

	Vec3 velocity2DDir( entityPhysicsState.Velocity()[0], entityPhysicsState.Velocity()[1], 0 );
	velocity2DDir *= 1.0f / speed2D;

	auto &traceCache = context->EnvironmentTraceCache();
	traceCache.TestForResultsMask( context, traceCache.FullHeightMask( traceCache.FRONT ) );
	const auto &frontResult = traceCache.FullHeightFrontTrace();
	if( velocity2DDir.Dot( frontResult.traceDir ) < 0.7f ) {
		return false;
	}

	bool hasGoodWalljumpNormal = false;
	TEST_TRACE_RESULT_NORMAL( frontResult );

	// Do not force full-height traces for sides to be computed.
	// Walljump height rules are complicated, and full simulation of these rules seems to be excessive.
	// In worst case a potential walljump might be skipped.
	auto sidesMask = traceCache.FULL_SIDES_MASK & ~( traceCache.BACK | traceCache.BACK_LEFT | traceCache.BACK_RIGHT );
	traceCache.TestForResultsMask( context, traceCache.JumpableHeightMask( sidesMask ) );

	TEST_TRACE_RESULT_NORMAL( traceCache.JumpableHeightLeftTrace() );
	TEST_TRACE_RESULT_NORMAL( traceCache.JumpableHeightRightTrace() );
	TEST_TRACE_RESULT_NORMAL( traceCache.JumpableHeightFrontLeftTrace() );
	TEST_TRACE_RESULT_NORMAL( traceCache.JumpableHeightFrontLeftTrace() );

	return hasGoodWalljumpNormal;
}

#undef TEST_TRACE_RESULT_NORMAL

void BotBaseMovementAction::CheatingAccelerate( BotMovementPredictionContext *context, float frac ) const {
	if( self->ai->botRef->ShouldMoveCarefully() ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.GroundEntity() ) {
		return;
	}

	const float speed = entityPhysicsState.Speed();
	const float speedThreshold = context->GetRunSpeed() - 15.0f;
	// Respect player class speed properties
	const float groundSpeedScale = speedThreshold / ( DEFAULT_PLAYERSPEED_STANDARD - 15.0f );

	// Avoid division by zero and logic errors
	if( speed < speedThreshold ) {
		return;
	}

	if( frac <= 0.0f ) {
		return;
	}

	const float maxSpeedGainPerSecond = 250.0f;
	const float minSpeedGainPerSecond = 75.0f;
	float speedGainPerSecond;
	// If speed = pivotSpeed speedGainPerSecond remains the same
	const float pivotSpeed = 550.0f * groundSpeedScale;
	const float constantAccelSpeed = 900.0f;
	if( speed > pivotSpeed ) {
		speedGainPerSecond = minSpeedGainPerSecond;
		// In this case speedGainPerSecond slowly decreases to minSpeedGainPerSecond
		// on speed = constantAccelSpeed or greater
		if( speed <= constantAccelSpeed ) {
			float speedFrac = BoundedFraction( speed - pivotSpeed, constantAccelSpeed - pivotSpeed );
			Assert( speedFrac >= 0.0f && speedFrac <= 1.0f );
			speedGainPerSecond += ( maxSpeedGainPerSecond - minSpeedGainPerSecond ) * ( 1.0f - speedFrac );
		}
	} else {
		// In this case speedGainPerSecond might be 2x as greater than maxSpeedGainPerSecond
		Assert( speedThreshold < pivotSpeed );
		float speedFrac = BoundedFraction( speed - speedThreshold, pivotSpeed - speedThreshold );
		Assert( speedFrac >= 0.0f && speedFrac <= 1.0f );
		speedGainPerSecond = maxSpeedGainPerSecond * ( 1.0f + ( 1.0f - speedFrac ) );
		// Also modify the frac to ensure the bot accelerates fast to the pivotSpeed in all cases
		// (the real applied frac is frac^(1/4))
		frac = SQRTFAST( frac );
	}

	speedGainPerSecond *= groundSpeedScale;
	if( self->ai->botRef->Skill() <= 0.33f ) {
		speedGainPerSecond *= 0.33f;
	}

	clamp_high( frac, 1.0f );
	frac = SQRTFAST( frac );

	Vec3 newVelocity( entityPhysicsState.Velocity() );
	// Normalize velocity boost direction
	newVelocity *= 1.0f / speed;
	// Make velocity boost vector
	newVelocity *= ( frac * speedGainPerSecond ) * ( 0.001f * context->oldStepMillis );
	// Add velocity boost to the entity velocity in the given physics state
	newVelocity += entityPhysicsState.Velocity();

	context->record->SetModifiedVelocity( newVelocity );
}

void BotBaseMovementAction::CheatingCorrectVelocity( BotMovementPredictionContext *context,
													 float velocity2DDirDotToTarget2DDir,
													 const Vec3 &toTargetDir2D ) const {
	// Respect player class movement limitations
	if( !( context->currPlayerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_AIRCONTROL ) ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;

	// Make correction less effective for large angles multiplying it
	// by the dot product to avoid a weird-looking cheating movement
	float controlMultiplier = 0.05f + fabsf( velocity2DDirDotToTarget2DDir ) * 0.05f;
	// Use lots of correction near items
	if( self->ai->botRef->ShouldMoveCarefully() ) {
		controlMultiplier += 0.10f;
	}

	const float speed = entityPhysicsState.Speed();
	if( speed < 100 ) {
		return;
	}

	// Check whether the direction to the target is normalized
	Assert( toTargetDir2D.LengthFast() > 0.99f && toTargetDir2D.LengthFast() < 1.01f );

	Vec3 newVelocity( entityPhysicsState.Velocity() );
	// Normalize current velocity direction
	newVelocity *= 1.0f / speed;
	// Modify velocity direction
	newVelocity += controlMultiplier * toTargetDir2D;
	// Normalize velocity direction again after modification
	newVelocity.Normalize();
	// Restore velocity magnitude
	newVelocity *= speed;

	context->record->SetModifiedVelocity( newVelocity );
}

bool BotGenericRunBunnyingMovementAction::CheckStepSpeedGainOrLoss( BotMovementPredictionContext *context ) {
	const auto *oldPMove = &context->oldPlayerState->pmove;
	const auto *newPMove = &context->currPlayerState->pmove;
	// Make sure this test is skipped along with other ones while skimming
	Assert( !( newPMove->skim_time && newPMove->skim_time != oldPMove->skim_time ) );

	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;
	const auto &oldEntityPhysicsState = context->PhysicsStateBeforeStep();

	// Test for a huge speed loss in case of hitting of an obstacle
	const float *oldVelocity = oldEntityPhysicsState.Velocity();
	const float *newVelocity = newEntityPhysicsState.Velocity();
	const float oldSquare2DSpeed = oldEntityPhysicsState.SquareSpeed2D();
	const float newSquare2DSpeed = newEntityPhysicsState.SquareSpeed2D();

	if( newSquare2DSpeed < 10 * 10 && oldSquare2DSpeed > 100 * 100 ) {
		// Bumping into walls on high speed in nav target areas is OK
		if( !context->IsInNavTargetArea() && !context->IsCloseToNavTarget() ) {
			Debug( "A prediction step has lead to close to zero 2D speed while it was significant\n" );
			this->shouldTryObstacleAvoidance = true;
			return false;
		}
	}

	// Check for unintended bouncing back (starting from some speed threshold)
	if( oldSquare2DSpeed > 100 * 100 && newSquare2DSpeed > 1 * 1 ) {
		if( this->SequenceDuration( context ) > 128 ) {
			Vec3 oldVelocity2DDir( oldVelocity[0], oldVelocity[1], 0 );
			oldVelocity2DDir *= 1.0f / oldEntityPhysicsState.Speed2D();
			Vec3 newVelocity2DDir( newVelocity[0], newVelocity[1], 0 );
			newVelocity2DDir *= 1.0f / newEntityPhysicsState.Speed2D();
			if( oldVelocity2DDir.Dot( newVelocity2DDir ) < 0.1f ) {
				Debug( "A prediction step has lead to an unintended bouncing back\n" );
				return false;
			}
		}
	}

	// Check for regular speed loss
	const float oldSpeed = oldEntityPhysicsState.Speed();
	const float newSpeed = newEntityPhysicsState.Speed();

	Assert( context->predictionStepMillis );
	float actualSpeedGainPerSecond = ( newSpeed - oldSpeed ) / ( 0.001f * context->predictionStepMillis );
	if( actualSpeedGainPerSecond >= minDesiredSpeedGainPerSecond || context->IsInNavTargetArea() ) {
		// Reset speed loss timer
		currentSpeedLossSequentialMillis = 0;
		return true;
	}

	const char *format = "Actual speed gain per second %.3f is lower than the desired one %.3f\n";
	Debug( "oldSpeed: %.1f, newSpeed: %1.f, speed gain per second: %.1f\n", oldSpeed, newSpeed, actualSpeedGainPerSecond );
	Debug( format, actualSpeedGainPerSecond, minDesiredSpeedGainPerSecond );

	currentSpeedLossSequentialMillis += context->predictionStepMillis;
	if( tolerableSpeedLossSequentialMillis < currentSpeedLossSequentialMillis ) {
		const char *format_ = "A sequential speed loss interval of %d millis exceeds the tolerable one of %d millis\n";
		Debug( format_, currentSpeedLossSequentialMillis, tolerableSpeedLossSequentialMillis );
		this->shouldTryObstacleAvoidance = true;
		return false;
	}

	return true;
}

bool BotGenericRunBunnyingMovementAction::IsMovingIntoNavEntity( BotMovementPredictionContext *context ) const {
	// Sometimes the camping spot movement action might be not applicable to reach a target.
	// Prevent missing the target when THIS action is likely to be really applied to reach it.
	// We do a coarse prediction of bot path for a second testing its intersection with the target (and no solid world).
	// The following ray-sphere test is very coarse but yields satisfactory results.
	// We might call G_Trace() but it is expensive and would fail if the target is not solid yet.
	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;
	Vec3 navTargetOrigin( context->NavTargetOrigin() );
	float navTargetRadius = context->NavTargetRadius();
	// If bot is walking on ground, use an ordinary ray-sphere test for a linear movement assumed to the bot
	if( newEntityPhysicsState.GroundEntity() && fabsf( newEntityPhysicsState.Velocity()[2] ) < 1.0f ) {
		Vec3 velocityDir( newEntityPhysicsState.Velocity() );
		velocityDir *= 1.0f / newEntityPhysicsState.Speed();

		Vec3 botToTarget( navTargetOrigin );
		botToTarget -= newEntityPhysicsState.Origin();

		float botToTargetDotVelocityDir = botToTarget.Dot( velocityDir );
		if( botToTarget.SquaredLength() > navTargetRadius * navTargetRadius && botToTargetDotVelocityDir < 0 ) {
			return false;
		}

		// |botToTarget| is the length of the triangle hypotenuse
		// |botToTargetDotVelocityDir| is the length of the adjacent triangle side
		// |distanceVec| is the length of the opposite triangle side
		// (The distanceVec is directed to the target center)

		// distanceVec = botToTarget - botToTargetDotVelocityDir * dir
		Vec3 distanceVec( velocityDir );
		distanceVec *= -botToTargetDotVelocityDir;
		distanceVec += botToTarget;

		return distanceVec.SquaredLength() <= navTargetRadius * navTargetRadius;
	}

	Vec3 botOrigin( newEntityPhysicsState.Origin() );
	Vec3 velocity( newEntityPhysicsState.Velocity() );
	const float gravity = level.gravity;
	constexpr float timeStep = 0.125f;
	for( unsigned stepNum = 0; stepNum < 8; ++stepNum ) {
		velocity.Z() -= gravity * timeStep;
		botOrigin += timeStep * velocity;

		Vec3 botToTarget( navTargetOrigin );
		botToTarget -= botOrigin;

		if( botToTarget.SquaredLength() > navTargetRadius * navTargetRadius ) {
			// The bot has already missed the nav entity (same check as in ray-sphere test)
			if( botToTarget.Dot( velocity ) < 0 ) {
				return false;
			}

			// The bot is still moving in the nav target direction
			continue;
		}

		// The bot is inside the target radius
		return true;
	}

	return false;
}

void BotGenericRunBunnyingMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	const auto &oldPMove = context->oldPlayerState->pmove;
	const auto &newPMove = context->currPlayerState->pmove;

	// Skip tests while skimming
	if( newPMove.skim_time && newPMove.skim_time != oldPMove.skim_time ) {
		// The only exception is testing covered distance to prevent
		// jumping in front of wall contacting it forever updating skim timer
		if( this->SequenceDuration( context ) > 400 ) {
			if( originAtSequenceStart.SquareDistance2DTo( newPMove.origin ) < SQUARE( 128 ) ) {
				context->SetPendingRollback();
				Debug( "Looks like the bot is stuck and is resetting the skim timer forever by jumping\n" );
				return;
			}
		}

		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	if( !CheckStepSpeedGainOrLoss( context ) ) {
		context->SetPendingRollback();
		return;
	}

	// This entity physics state has been modified after prediction step
	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;

	bool isInNavTargetArea = context->IsInNavTargetArea();
	if( isInNavTargetArea || context->IsCloseToNavTarget() ) {
		if( !IsMovingIntoNavEntity( context ) ) {
			Debug( "The bot is likely to miss the nav target\n" );
			context->SetPendingRollback();
			return;
		}

		// Skip other checks in this case
		if( isInNavTargetArea ) {
			// Avoid prediction stack overflow in huge areas
			if( newEntityPhysicsState.GroundEntity() && this->SequenceDuration( context ) > 250 ) {
				Debug( "The bot is on ground in the nav target area, moving into the target, there is enough predicted data\n" );
				context->isCompleted = true;
				return;
			}
			// Keep this action as an active bunny action
			context->SaveSuggestedActionForNextFrame( this );
			return;
		}
	}

	int currTravelTimeToNavTarget = context->TravelTimeToNavTarget();
	if( !currTravelTimeToNavTarget ) {
		currentUnreachableTargetSequentialMillis += context->predictionStepMillis;
		// Be very strict in case when bot does another jump after landing.
		// (Prevent falling in a gap immediately after successful landing on a ledge).
		if( currentUnreachableTargetSequentialMillis > tolerableUnreachableTargetSequentialMillis ) {
			context->SetPendingRollback();
			Debug( "A prediction step has lead to undefined travel time to the nav target\n" );
			return;
		}

		context->SaveSuggestedActionForNextFrame( this );
		return;
	}
	// Reset unreachable target timer
	currentUnreachableTargetSequentialMillis = 0;

	if( currTravelTimeToNavTarget <= minTravelTimeToNavTargetSoFar ) {
		minTravelTimeToNavTargetSoFar = currTravelTimeToNavTarget;
		minTravelTimeAreaNumSoFar = context->CurrAasAreaNum();
		if( int groundedAreaNum = context->CurrGroundedAasAreaNum() ) {
			minTravelTimeAreaGroundZ = AiAasWorld::Instance()->Areas()[groundedAreaNum].mins[2];
		}
	} else {
		constexpr const char *format = "A prediction step has lead to increased travel time to nav target\n";
		if( currTravelTimeToNavTarget > (int)( minTravelTimeToNavTargetSoFar + tolerableWalkableIncreasedTravelTimeMillis ) ) {
			context->SetPendingRollback();
			Debug( format );
			return;
		}

		if( minTravelTimeAreaGroundZ != std::numeric_limits<float>::max() ) {
			int groundedAreaNum = context->CurrGroundedAasAreaNum();
			if( !groundedAreaNum ) {
				context->SetPendingRollback();
				Debug( format );
				return;
			}
			float currGroundAreaZ = AiAasWorld::Instance()->Areas()[groundedAreaNum].mins[2];
			// Disallow significant difference in height with the min travel time area.
			// If the current area is higher than the min travel time area,
			// use an increased height delta (falling is easier than climbing)
			if( minTravelTimeAreaGroundZ > currGroundAreaZ + 8 || minTravelTimeAreaGroundZ + 48 < currGroundAreaZ ) {
				context->SetPendingRollback();
				Debug( format );
				return;
			}
		}

		if( minTravelTimeAreaNumSoFar ) {
			// Disallow moving into an area if the min travel time area cannot be reached by walking from the area
			int areaNums[2] = { newEntityPhysicsState.CurrAasAreaNum(), newEntityPhysicsState.DroppedToFloorAasAreaNum() };
			bool walkable = false;
			for( int i = 0, end = ( areaNums[0] != areaNums[1] ? 2 : 1 ); i < end; ++i ) {
				int travelFlags = TRAVEL_WALK | TRAVEL_WALKOFFLEDGE;
				int toAreaNum = minTravelTimeAreaNumSoFar;
				if( int aasTime = self->ai->botRef->routeCache->TravelTimeToGoalArea( areaNums[i], toAreaNum, travelFlags ) ) {
					// aasTime is in seconds^-2
					if( aasTime * 10 < (int)tolerableWalkableIncreasedTravelTimeMillis ) {
						walkable = true;
						break;
					}
				}
			}
			if( !walkable ) {
				context->SetPendingRollback();
				Debug( format );
				return;
			}
		}
	}

	if( originAtSequenceStart.SquareDistanceTo( newEntityPhysicsState.Origin() ) < 72 * 72 ) {
		if( SequenceDuration( context ) < 512 ) {
			context->SaveSuggestedActionForNextFrame( this );
			return;
		}

		// Prevent wasting CPU cycles on further prediction
		Debug( "The bot still has not covered 72 units yet in 512 millis\n" );
		context->SetPendingRollback();
		return;
	}

	if( newEntityPhysicsState.GroundEntity() ) {
		Debug( "The bot has covered 96 units and is on ground, should stop prediction\n" );
		context->isCompleted = true;
		return;
	}

	// Continue prediction in the bot does not move down.
	// Test velocity dir Z, use some negative threshold to avoid wasting CPU cycles
	// on traces that are parallel to the ground.
	if( newEntityPhysicsState.Velocity()[2] / newEntityPhysicsState.Speed() > -0.1f ) {
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	if( newEntityPhysicsState.IsHighAboveGround() ) {
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	// Try stop prediction if the bot is inside a huge "good" area
	const auto *aasWorld = AiAasWorld::Instance();
	const int currAreaNum = context->CurrAasAreaNum();
	const auto &area = aasWorld->Areas()[currAreaNum];
	const auto &areaSettings = aasWorld->AreaSettings()[currAreaNum];

	if( areaSettings.areaflags & AREA_GROUNDED ) {
		const float *origin = newEntityPhysicsState.Origin();
		if( area.mins[0] < origin[0] - 48 && area.maxs[0] > origin[0] + 48 ) {
			if( area.mins[1] < origin[1] - 48 && area.maxs[1] > origin[1] + 48 ) {
				Debug( "The bot is not very high above the ground and is inside a \"good\" area\n" );
				context->isCompleted = true;
				return;
			}
		}
	}

	Assert( context->NavTargetAasAreaNum() );

	// We might wait for landing, but it produces bad results
	// (rejects many legal moves probably due to prediction stack overflow)
	// The tracing is expensive but we did all possible cutoffs above

	Vec3 predictedOrigin( newEntityPhysicsState.Origin() );
	float predictionSeconds = 0.2f;
	for( int i = 0; i < 3; ++i ) {
		predictedOrigin.Data()[i] += newEntityPhysicsState.Velocity()[i] * predictionSeconds;
	}
	predictedOrigin.Data()[2] -= 0.5f * level.gravity * predictionSeconds * predictionSeconds;

	trace_t trace;
	SolidWorldTrace( &trace, newEntityPhysicsState.Origin(), predictedOrigin.Data(), playerbox_stand_mins );
	constexpr auto badContents = CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_NODROP | CONTENTS_DONOTENTER;
	if( trace.fraction == 1.0f || !ISWALKABLEPLANE( &trace.plane ) || ( trace.contents & badContents ) ) {
		// Can't say much. The test is very coarse, continue prediction.
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	Vec3 groundPoint( trace.endpos );
	groundPoint.Z() += 4.0f;
	int groundAreaNum = aasWorld->FindAreaNum( groundPoint );
	if( !groundAreaNum ) {
		// Can't say much. The test is very coarse, continue prediction.
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	const auto &groundAreaSettings = aasWorld->AreaSettings()[groundAreaNum];
	if( !( groundAreaSettings.areaflags & AREA_GROUNDED ) || ( groundAreaSettings.areaflags & ( AREA_DISABLED | AREA_JUNK ) ) ) {
		// Can't say much. The test is very coarse, continue prediction.
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}
	if( ( groundAreaSettings.contents & ( AREACONTENTS_LAVA | AREACONTENTS_SLIME | AREACONTENTS_DONOTENTER ) ) ) {
		// Can't say much. The test is very coarse, continue prediction.
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	int travelFlags = self->ai->botRef->allowedAasTravelFlags;
	const auto *routeCache = self->ai->botRef->routeCache;
	int travelTime = routeCache->TravelTimeToGoalArea( groundAreaNum, context->NavTargetAasAreaNum(), travelFlags );
	if( travelTime && travelTime < currTravelTimeToNavTarget ) {
		Debug( "The bot is not very high above the ground and looks like it lands in a \"good\" area\n" );
		context->isCompleted = true;
		return;
	}

	// Can't say much. The test is very coarse, continue prediction
	context->SaveSuggestedActionForNextFrame( this );
	return;
}

void BotGenericRunBunnyingMovementAction::OnApplicationSequenceStarted( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::OnApplicationSequenceStarted( context );
	context->MarkSavepoint( this, context->topOfStackIndex );

	minTravelTimeToNavTargetSoFar = std::numeric_limits<int>::max();
	minTravelTimeAreaNumSoFar = 0;
	minTravelTimeAreaGroundZ = std::numeric_limits<float>::max();
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( context->NavTargetAasAreaNum() ) {
		if( int travelTime = context->TravelTimeToNavTarget() ) {
			minTravelTimeToNavTargetSoFar = travelTime;
			if( int groundedAreaNum = context->CurrGroundedAasAreaNum() ) {
				minTravelTimeAreaGroundZ = AiAasWorld::Instance()->Areas()[groundedAreaNum].mins[2];
			}
		}
	}

	originAtSequenceStart.Set( entityPhysicsState.Origin() );

	currentSpeedLossSequentialMillis = 0;
	currentUnreachableTargetSequentialMillis = 0;
}

void BotGenericRunBunnyingMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
																		SequenceStopReason reason,
																		unsigned stoppedAtFrameIndex ) {
	BotBaseMovementAction::OnApplicationSequenceStopped( context, reason, stoppedAtFrameIndex );

	if( reason != FAILED ) {
		ResetObstacleAvoidanceState();
		if( reason != DISABLED ) {
			this->disabledForApplicationFrameIndex = std::numeric_limits<unsigned>::max();
		}
		return;
	}

	// If the action has been disabled due to prediction stack overflow
	if( this->isDisabledForPlanning ) {
		return;
	}

	if( !supportsObstacleAvoidance ) {
		// However having shouldTryObstacleAvoidance flag is legal (it should be ignored in this case).
		// Make sure THIS method logic (that sets isTryingObstacleAvoidance) works as intended.
		Assert( !isTryingObstacleAvoidance );
		// Disable applying this action after rolling back to the savepoint
		this->disabledForApplicationFrameIndex = context->savepointTopOfStackIndex;
		return;
	}

	if( !isTryingObstacleAvoidance && shouldTryObstacleAvoidance ) {
		// Try using obstacle avoidance after rolling back to the savepoint
		// (We rely on skimming for the first try).
		isTryingObstacleAvoidance = true;
		// Make sure this action will be chosen again after rolling back
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	// Disable applying this action after rolling back to the savepoint
	this->disabledForApplicationFrameIndex = context->savepointTopOfStackIndex;
	this->ResetObstacleAvoidanceState();
}

void BotGenericRunBunnyingMovementAction::BeforePlanning() {
	BotBaseMovementAction::BeforePlanning();
	this->disabledForApplicationFrameIndex = std::numeric_limits<unsigned>::max();
	ResetObstacleAvoidanceState();
}

void BotWalkOrSlideInterpolatingReachChainMovementAction::SetupMovementInTargetArea( BotMovementPredictionContext *context ) {
	Vec3 intendedMoveVec( context->NavTargetOrigin() );
	intendedMoveVec -= context->movementState->entityPhysicsState.Origin();
	intendedMoveVec.NormalizeFast();

	if( TrySetupCrouchSliding( context, intendedMoveVec ) ) {
		return;
	}

	int keyMoves[2];
	if( self->ai->botRef->HasEnemy() && !context->IsCloseToNavTarget() ) {
		context->EnvironmentTraceCache().MakeRandomizedKeyMovesToTarget( context, intendedMoveVec, keyMoves );
	} else {
		context->EnvironmentTraceCache().MakeKeyMovesToTarget( context, intendedMoveVec, keyMoves );
	}

	auto *botInput = &context->record->botInput;
	botInput->SetForwardMovement( keyMoves[0] );
	botInput->SetRightMovement( keyMoves[1] );
	botInput->SetIntendedLookDir( intendedMoveVec, true );
	botInput->isUcmdSet = true;
	botInput->canOverrideLookVec = true;
}

bool BotWalkOrSlideInterpolatingReachChainMovementAction::TrySetupCrouchSliding( BotMovementPredictionContext *context,
																				 const Vec3 &intendedLookDir ) {
	if( !( context->currPlayerState->pmove.pm_flags & PMF_CROUCH_SLIDING ) ) {
		return false;
	}

	if( context->currPlayerState->pmove.stats[PM_STAT_CROUCHSLIDETIME] <= ( 3 * PM_CROUCHSLIDE_FADE ) / 4 ) {
		return false;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( entityPhysicsState.Speed() < context->GetRunSpeed() ) {
		return false;
	}

	if( self->ai->botRef->HasEnemy() ) {
		return false;
	}

	Vec3 velocityDir( entityPhysicsState.Velocity() );
	velocityDir *= 1.0f / entityPhysicsState.Speed();

	// These directions mismatch is way too high for using crouch slide control
	if( velocityDir.Dot( entityPhysicsState.ForwardDir() ) < 0 ) {
		return false;
	}

	if( velocityDir.Dot( intendedLookDir ) < 0 ) {
		return false;
	}

	auto *botInput = &context->record->botInput;
	botInput->SetIntendedLookDir( intendedLookDir, true );
	botInput->SetUpMovement( -1 );
	botInput->isUcmdSet = true;

	float dotRight = intendedLookDir.Dot( entityPhysicsState.RightDir() );
	if( dotRight > 0.2f ) {
		botInput->SetRightMovement( 1 );
	} else if( dotRight < -0.2f ) {
		botInput->SetRightMovement( -1 );
	}

	botInput->SetAllowedRotationMask( BotInputRotation::NONE );

	return true;
}

void BotWalkOrSlideInterpolatingReachChainMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	totalNumFrames++;
	if( !GenericCheckIsActionEnabled( context, &DummyAction() ) ) {
		return;
	}

	int navTargetAreaNum = context->NavTargetAasAreaNum();
	if( !navTargetAreaNum ) {
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	if( !entityPhysicsState.GroundEntity() && entityPhysicsState.HeightOverGround() > 12 ) {
		context->cannotApplyAction = true;
		context->actionSuggestedByAction = &DummyAction();
		Debug( "Cannot apply action: the bot is way too high above the ground\n" );
		return;
	}

	// Walk to the nav target in this case
	if( context->IsInNavTargetArea() ) {
		SetupMovementInTargetArea( context );
		return;
	}

	// Continue interpolating while a next reach has these travel types
	const int compatibleReachTypes[1] = { TRAVEL_WALK };
	// Stop interpolating on these reach types but include a reach start in interpolation
	const int allowedEndReachTypes[6] =
	{
		TRAVEL_WALKOFFLEDGE, TRAVEL_JUMP, TRAVEL_TELEPORT, TRAVEL_JUMPPAD, TRAVEL_ELEVATOR, TRAVEL_LADDER
	};
	ReachChainInterpolator interpolator;
	interpolator.stopAtDistance = 128.0f;
	interpolator.SetCompatibleReachTypes( compatibleReachTypes, sizeof( compatibleReachTypes ) / sizeof( int ) );
	interpolator.SetAllowedEndReachTypes( allowedEndReachTypes, sizeof( allowedEndReachTypes ) / sizeof( int ) );
	if( !interpolator.Exec( context ) ) {
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		Debug( "Cannot apply action: cannot interpolate reach chain\n" );
		return;
	}

	const auto &miscTactics = self->ai->botRef->GetMiscTactics();
	if( !miscTactics.shouldBeSilent && !miscTactics.shouldMoveCarefully ) {
		if( entityPhysicsState.Speed2D() > context->GetRunSpeed() ) {
			// Check whether the bot is moving in a "proper" direction to prevent cycling in a loop around a reachability
			Vec3 velocityDir( entityPhysicsState.Velocity() );
			velocityDir *= 1.0f / ( entityPhysicsState.Speed() + 0.000001f );
			if( velocityDir.Dot( interpolator.Result() ) > 0.7f ) {
				context->cannotApplyAction = true;
				context->actionSuggestedByAction = &DummyAction();
				Debug( "Cannot apply action: do not lose a high speed, this speed is safe in the current environment\n" );
				return;
			}
		}
	}

	if( TrySetupCrouchSliding( context, interpolator.Result() ) ) {
		// Predict crouch sliding precisely
		context->predictionStepMillis = 16;
		numSlideFrames++;
		return;
	}

	int keyMoves[2] = { 0, 0 };
	auto *botInput = &context->record->botInput;
	if( entityPhysicsState.GroundEntity() ) {
		auto &environmentTraceCache = context->EnvironmentTraceCache();
		if( self->ai->botRef->HasEnemy() ) {
			environmentTraceCache.MakeRandomizedKeyMovesToTarget( context, interpolator.Result(), keyMoves );
		} else {
			environmentTraceCache.MakeKeyMovesToTarget( context, interpolator.Result(), keyMoves );
		}
	} else if( ShouldPrepareForCrouchSliding( context ) ) {
		botInput->SetUpMovement( -1 );
		// Predict crouch sliding precisely
		context->predictionStepMillis = 16;
	}

	botInput->SetForwardMovement( keyMoves[0] );
	botInput->SetRightMovement( keyMoves[1] );
	botInput->SetIntendedLookDir( interpolator.Result(), true );
	botInput->isUcmdSet = true;
	botInput->canOverrideLookVec = true;
}

void BotWalkOrSlideInterpolatingReachChainMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	const auto &newPMove = context->currPlayerState->pmove;
	const auto &oldPMove = context->oldPlayerState->pmove;
	// Disallow skimming to kick in.
	// A bot is not intended to move fast enough in this action to allow hiding bumping into obstacles by skimming.
	if( newPMove.skim_time && newPMove.skim_time != oldPMove.skim_time ) {
		Debug( "A prediction step has lead to skimming. A skimming is not allowed in this action\n" );
		this->isDisabledForPlanning = true;
		return;
	}

	const auto &newEntityPhysicsState = context->movementState->entityPhysicsState;
	const auto &oldEntityPhysicsState = context->PhysicsStateBeforeStep();
	if( !newEntityPhysicsState.GroundEntity() ) {
		// Allow being in air on steps/stairs
		if( newEntityPhysicsState.HeightOverGround() > 12 && oldEntityPhysicsState.HeightOverGround() < 12 ) {
			Debug( "A prediction step has lead to being way too high above the ground\n" );
			this->isDisabledForPlanning = true;
			context->SetPendingRollback();
			return;
		}
	}

	int currTravelTimeToNavTarget = context->TravelTimeToNavTarget();
	if( !currTravelTimeToNavTarget ) {
		Debug( "A prediction step has lead to an undefined travel time to the nav target\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}
	if( currTravelTimeToNavTarget > minTravelTimeToTarget ) {
		Debug( "A prediction step has lead to an increased travel time to the nav target\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}
	minTravelTimeToTarget = currTravelTimeToNavTarget;

	if( this->SequenceDuration( context ) < 200 ) {
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	float distanceThreshold = 20.0f + 28.0f * ( numSlideFrames / (float)totalNumFrames );
	if( originAtSequenceStart.SquareDistanceTo( newEntityPhysicsState.Origin() ) < SQUARE( distanceThreshold ) ) {
		Debug( "The bot is likely to be stuck after 200 millis\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}

	if( newEntityPhysicsState.Speed() < context->GetRunSpeed() - 10 ) {
		Debug( "The bot speed is still below run speed after 200 millis\n" );
		this->isDisabledForPlanning = true;
		context->SetPendingRollback();
		return;
	}

	// Wait for hitting the ground
	if( !newEntityPhysicsState.GroundEntity() ) {
		context->SaveSuggestedActionForNextFrame( this );
		return;
	}

	Debug( "There is enough predicted data ahead\n" );
	context->isCompleted = true;
}

void BotWalkOrSlideInterpolatingReachChainMovementAction::OnApplicationSequenceStarted( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::OnApplicationSequenceStarted( context );

	minTravelTimeToTarget = context->TravelTimeToNavTarget();
	totalNumFrames = 0;
	numSlideFrames = 0;
}

inline unsigned BotEnvironmentTraceCache::SelectNonBlockedDirs( BotMovementPredictionContext *context, unsigned *nonBlockedDirIndices ) {
	this->TestForResultsMask( context, this->FullHeightMask( FULL_SIDES_MASK ) );

	unsigned numNonBlockedDirs = 0;
	for( unsigned i = 0; i < 8; ++i ) {
		if( this->FullHeightTraceForSideIndex( i ).IsEmpty() ) {
			nonBlockedDirIndices[numNonBlockedDirs++] = i;
		}
	}
	return numNonBlockedDirs;
}

void BotEnvironmentTraceCache::MakeRandomizedKeyMovesToTarget( BotMovementPredictionContext *context,
															   const Vec3 &intendedMoveDir, int *keyMoves ) {
	unsigned nonBlockedDirIndices[8];
	unsigned numNonBlockedDirs = SelectNonBlockedDirs( context, nonBlockedDirIndices );

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const Vec3 forwardDir( entityPhysicsState.ForwardDir() );
	const Vec3 rightDir( entityPhysicsState.RightDir() );
	assert( ( intendedMoveDir.Length() - 1.0f ) < 0.0001f );

	// Choose randomly from all non-blocked dirs based on scores
	// For each non - blocked area make an interval having a corresponding to the area score length.
	// An interval is defined by lower and upper bounds.
	// Upper bounds are stored in the array.
	// Lower bounds are upper bounds of the previous array memner (if any) or 0 for the first array memeber.
	float dirDistributionUpperBound[8];
	float scoresSum = 0.0f;
	for( unsigned i = 0; i < numNonBlockedDirs; ++i ) {
		vec3_t keyMoveVec;
		const float *dirFractions = sideDirXYFractions[nonBlockedDirIndices[i]];
		VectorScale( forwardDir.Data(), dirFractions[0], keyMoveVec );
		VectorMA( keyMoveVec, dirFractions[1], rightDir.Data(), keyMoveVec );
		scoresSum += 0.55f + 0.45f * intendedMoveDir.Dot( keyMoveVec );
		dirDistributionUpperBound[i] = scoresSum;
	}

	// A uniformly distributed random number in (0, scoresSum)
	const float rn = random() * scoresSum;
	for( unsigned i = 0; i < numNonBlockedDirs; ++i ) {
		if( rn > dirDistributionUpperBound[i] ) {
			continue;
		}

		int dirIndex = nonBlockedDirIndices[i];
		const int *dirMoves = sideDirXYMoves[dirIndex];
		Vector2Copy( dirMoves, keyMoves );
		return;
	}

	Vector2Set( keyMoves, 0, 0 );
}

void BotEnvironmentTraceCache::MakeKeyMovesToTarget( BotMovementPredictionContext *context,
													 const Vec3 &intendedMoveDir, int *keyMoves ) {
	unsigned nonBlockedDirIndices[8];
	unsigned numNonBlockedDirs = SelectNonBlockedDirs( context, nonBlockedDirIndices );

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const Vec3 forwardDir( entityPhysicsState.ForwardDir() );
	const Vec3 rightDir( entityPhysicsState.RightDir() );
	assert( ( intendedMoveDir.Length() - 1.0f ) < 0.0001f );

	float bestScore = 0.0f;
	unsigned bestDirIndex = (unsigned)-1;
	for( unsigned i = 0; i < numNonBlockedDirs; ++i ) {
		vec3_t keyMoveVec;
		unsigned dirIndex = nonBlockedDirIndices[i];
		const float *dirFractions = sideDirXYFractions[dirIndex];
		VectorScale( forwardDir.Data(), dirFractions[0], keyMoveVec );
		VectorMA( keyMoveVec, dirFractions[1], rightDir.Data(), keyMoveVec );
		float score = 0.55f + 0.45f * intendedMoveDir.Dot( keyMoveVec );
		if( score > bestScore ) {
			bestScore = score;
			bestDirIndex = dirIndex;
		}
	}
	if( bestScore > 0 ) {
		const int *dirMoves = sideDirXYMoves[bestDirIndex];
		Vector2Copy( dirMoves, keyMoves );
		return;
	}

	Vector2Set( keyMoves, 0, 0 );
}

void BotEnvironmentTraceCache::MakeRandomKeyMoves( BotMovementPredictionContext *context, int *keyMoves ) {
	unsigned nonBlockedDirIndices[8];
	unsigned numNonBlockedDirs = SelectNonBlockedDirs( context, nonBlockedDirIndices );
	if( numNonBlockedDirs ) {
		int dirIndex = nonBlockedDirIndices[(unsigned)( 0.9999f * numNonBlockedDirs * random() )];
		const int *dirMoves = sideDirXYMoves[dirIndex];
		Vector2Copy( dirMoves, keyMoves );
		return;
	}
	Vector2Set( keyMoves, 0, 0 );
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::UpdateKeyMoveDirs( BotMovementPredictionContext *context ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	auto *combatMovementState = &context->movementState->combatMoveDirsState;
	Assert( combatMovementState->IsActive() );

	int keyMoves[2];
	auto &traceCache = context->EnvironmentTraceCache();
	if( int nextReachNum = context->NextReachNum() ) {
		const aas_reachability_t &nextReach = AiAasWorld::Instance()->Reachabilities()[nextReachNum];
		Vec3 intendedMoveDir( nextReach.start );
		intendedMoveDir += nextReach.end;
		intendedMoveDir *= 0.5f;
		intendedMoveDir -= entityPhysicsState.Origin();
		intendedMoveDir.NormalizeFast();

		if( ShouldTryRandomness() ) {
			traceCache.MakeRandomizedKeyMovesToTarget( context, intendedMoveDir, keyMoves );
		} else {
			traceCache.MakeKeyMovesToTarget( context, intendedMoveDir, keyMoves );
		}
	} else {
		traceCache.MakeRandomKeyMoves( context, keyMoves );
	}

	combatMovementState->Activate( keyMoves[0], keyMoves[1] );
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::PlanPredictionStep( BotMovementPredictionContext *context ) {
	Assert( self->ai->botRef->ShouldKeepXhairOnEnemy() );
	Assert( self->ai->botRef->GetSelectedEnemies().AreValid() );

	auto *botInput = &context->record->botInput;
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;

	botInput->canOverrideLookVec = true;
	botInput->shouldOverrideLookVec = true;
	botInput->isUcmdSet = true;

	if( attemptNum == maxAttempts ) {
		Debug( "Attempts count has reached its limit. Should stop planning\n" );
		// There is no fallback action since this action is a default one for combat state.
		botInput->SetForwardMovement( 0 );
		botInput->SetRightMovement( 0 );
		botInput->SetUpMovement( random() > 0.5f ? -1 : +1 );
		context->isCompleted = true;
	}

	Vec3 botToEnemies( self->ai->botRef->GetSelectedEnemies().LastSeenOrigin() );
	botToEnemies -= entityPhysicsState.Origin();

	const short *pmStats = context->currPlayerState->pmove.stats;
	if( entityPhysicsState.GroundEntity() ) {
		if( ShouldTrySpecialMovement() ) {
			if( pmStats[PM_STAT_FEATURES] & PMFEAT_DASH ) {
				const float speedThreshold = context->GetDashSpeed() - 10;
				if( entityPhysicsState.Speed() < speedThreshold ) {
					if( !pmStats[PM_STAT_DASHTIME] && !pmStats[PM_STAT_STUN] ) {
						botInput->SetSpecialButton( true );
						context->predictionStepMillis = 16;
					}
				}
			}
		}
		auto *combatMovementState = &context->movementState->combatMoveDirsState;
		if( combatMovementState->IsActive() ) {
			UpdateKeyMoveDirs( context );
		}

		botInput->SetForwardMovement( combatMovementState->ForwardMove() );
		botInput->SetRightMovement( combatMovementState->RightMove() );
		// Set at least a single key or button while on ground (forward/right move keys might both be zero)
		if( !botInput->ForwardMovement() && !botInput->RightMovement() && !botInput->UpMovement() ) {
			if( !botInput->IsSpecialButtonSet() ) {
				botInput->SetUpMovement( -1 );
			}
		}
	} else {
		if( ShouldTrySpecialMovement() ) {
			if( ( pmStats[PM_STAT_FEATURES] & PMFEAT_WALLJUMP ) && !pmStats[PM_STAT_WJTIME] && !pmStats[PM_STAT_STUN] ) {
				botInput->SetSpecialButton( true );
				context->predictionStepMillis = 16;
			}
		}

		const float skill = self->ai->botRef->Skill();
		if( skill > 0.33f ) {
			if( !botInput->IsSpecialButtonSet() && entityPhysicsState.Speed2D() < 650 ) {
				const auto &oldPMove = context->oldPlayerState->pmove;
				const auto &newPMove = context->currPlayerState->pmove;
				// If not skimming
				if( !( newPMove.skim_time && newPMove.skim_time != oldPMove.skim_time ) ) {
					CheatingAccelerate( context, skill );
				}
			}
		}
	}
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::CheckPredictionStepResults( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::CheckPredictionStepResults( context );
	if( context->cannotApplyAction || context->isCompleted ) {
		return;
	}

	// If there is no nav target, skip nav target reachability tests
	if( this->minTravelTimeToTarget ) {
		Assert( context->NavTargetAasAreaNum() );
		int newTravelTimeToTarget = context->TravelTimeToNavTarget();
		if( !newTravelTimeToTarget ) {
			Debug( "A prediction step has lead to an undefined travel time to the nav target\n" );
			context->SetPendingRollback();
			return;
		}

		if( newTravelTimeToTarget < this->minTravelTimeToTarget ) {
			this->minTravelTimeToTarget = newTravelTimeToTarget;
		} else if( newTravelTimeToTarget > this->minTravelTimeToTarget + 50 ) {
			Debug( "A prediction step has lead to a greater travel time to the nav target\n" );
			context->SetPendingRollback();
			return;
		}
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const auto &moveDirsState = context->movementState->combatMoveDirsState;
	// Do not check total distance in case when bot has/had zero move dirs set
	if( !moveDirsState.ForwardMove() && !moveDirsState.RightMove() ) {
		// If the bot is on ground and move dirs set at a sequence start were invalidated
		if( entityPhysicsState.GroundEntity() && !moveDirsState.IsActive() ) {
			context->isCompleted = true;
		} else {
			context->SaveSuggestedActionForNextFrame( this );
		}
		return;
	}

	const float *oldOrigin = context->PhysicsStateBeforeStep().Origin();
	const float *newOrigin = entityPhysicsState.Origin();
	totalCovered2DDistance += SQRTFAST( SQUARE( newOrigin[0] - oldOrigin[0] ) + SQUARE( newOrigin[1] - oldOrigin[1] ) );

	// If the bot is on ground and move dirs set at a sequence start were invalidated
	if( entityPhysicsState.GroundEntity() && !moveDirsState.IsActive() ) {
		// Check for blocking
		if( totalCovered2DDistance < 24 ) {
			Debug( "Total covered distance since sequence start is too low\n" );
			context->SetPendingRollback();
			return;
		}
		context->isCompleted = true;
		return;
	}

	context->SaveSuggestedActionForNextFrame( this );
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::OnApplicationSequenceStarted( BotMovementPredictionContext *context ) {
	BotBaseMovementAction::OnApplicationSequenceStarted( context );

	this->minTravelTimeToTarget = context->TravelTimeToNavTarget();
	this->totalCovered2DDistance = 0.0f;
	// Always reset combat move dirs state to ensure that the action will be predicted for the entire move dirs lifetime
	context->movementState->combatMoveDirsState.Deactivate();
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::OnApplicationSequenceStopped( BotMovementPredictionContext *context,
																					 SequenceStopReason stopReason,
																					 unsigned stoppedAtFrameIndex ) {
	BotBaseMovementAction::OnApplicationSequenceStopped( context, stopReason, stoppedAtFrameIndex );
	if( stopReason != FAILED ) {
		attemptNum = 0;
		return;
	}

	attemptNum++;
	Assert( attemptNum <= maxAttempts );
}

void BotCombatDodgeSemiRandomlyToTargetMovementAction::BeforePlanning() {
	BotBaseMovementAction::BeforePlanning();
	attemptNum = 0;
	maxAttempts = self->ai->botRef->Skill() > 0.33f ? 4 : 2;
}

void Bot::CheckTargetProximity() {
	if( !botBrain.HasNavTarget() ) {
		return;
	}

	if( !botBrain.IsCloseToNavTarget( 128.0f ) ) {
		return;
	}

	// Save the origin for the roaming manager to avoid its occasional modification in the code below
	Vec3 navTargetOrigin( NavTargetOrigin() );
	if( !botBrain.TryReachNavTargetByProximity() ) {
		return;
	}

	roamingManager.OnNavTargetReached( navTargetOrigin );
	OnNavTargetTouchHandled();
}

void BotEnvironmentTraceCache::SetFullHeightCachedTracesEmpty( const vec3_t front2DDir, const vec3_t right2DDir ) {
	for( unsigned i = 0; i < 6; ++i ) {
		auto &fullResult = results[i + 0];
		auto &jumpableResult = results[i + 6];
		fullResult.trace.fraction = 1.0f;
		fullResult.trace.fraction = 1.0f;
		// We have to save a legal trace dir
		MakeTraceDir( i, front2DDir, right2DDir, fullResult.traceDir );
		VectorCopy( fullResult.traceDir, jumpableResult.traceDir );
	}
	resultsMask |= ALL_SIDES_MASK;
	hasNoFullHeightObstaclesAround = true;
}

void BotEnvironmentTraceCache::SetJumpableHeightCachedTracesEmpty( const vec3_t front2DDir, const vec3_t right2DDir ) {
	for( unsigned i = 0; i < 6; ++i ) {
		auto &result = results[i + 6];
		result.trace.fraction = 1.0f;
		// We have to save a legal trace dir
		MakeTraceDir( i, front2DDir, right2DDir, result.traceDir );
	}
	resultsMask |= JUMPABLE_SIDES_MASK;
}

inline bool BotEnvironmentTraceCache::CanSkipTracingForAreaHeight( const vec3_t origin,
																   const aas_area_t &area,
																   float minZOffset ) {
	if( area.mins[2] >= origin[2] + minZOffset ) {
		return false;
	}
	if( area.maxs[2] <= origin[2] + playerbox_stand_maxs[2] ) {
		return false;
	}

	return true;
}

const int BotEnvironmentTraceCache::sideDirXYMoves[8][2] =
{
	{ +1, +0 }, // forward
	{ -1, +0 }, // back
	{ +0, -1 }, // left
	{ +0, +1 }, // right
	{ +1, -1 }, // front left
	{ +1, +1 }, // front right
	{ -1, -1 }, // back left
	{ -1, +1 }, // back right
};

const float BotEnvironmentTraceCache::sideDirXYFractions[8][2] =
{
	{ +1.000f, +0.000f }, // front
	{ -1.000f, +0.000f }, // back
	{ +0.000f, -1.000f }, // left
	{ +0.000f, +1.000f }, // right
	{ +0.707f, -0.707f }, // front left
	{ +0.707f, +0.707f }, // front right
	{ -0.707f, -0.707f }, // back left
	{ -0.707f, +0.707f }, // back right
};

inline void BotEnvironmentTraceCache::MakeTraceDir( unsigned dirNum, const vec3_t front2DDir,
													const vec3_t right2DDir, vec3_t traceDir ) {
	const float *dirFractions = sideDirXYFractions[dirNum];
	VectorScale( front2DDir, dirFractions[0], traceDir );
	VectorMA( traceDir, dirFractions[1], right2DDir, traceDir );
	VectorNormalizeFast( traceDir );
}

bool BotEnvironmentTraceCache::TrySkipTracingForCurrOrigin( BotMovementPredictionContext *context,
															const vec3_t front2DDir, const vec3_t right2DDir ) {
	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	const int areaNum = entityPhysicsState.CurrAasAreaNum();
	if( !areaNum ) {
		return false;
	}

	const auto *aasWorld = AiAasWorld::Instance();
	const auto &area = aasWorld->Areas()[areaNum];
	const auto &areaSettings = aasWorld->AreaSettings()[areaNum];
	const float *origin = entityPhysicsState.Origin();

	// Extend playerbox XY bounds by TRACE_DEPTH
	Vec3 mins( origin[0] - TRACE_DEPTH, origin[1] - TRACE_DEPTH, origin[2] );
	Vec3 maxs( origin[0] + TRACE_DEPTH, origin[1] + TRACE_DEPTH, origin[2] );
	mins += playerbox_stand_mins;
	maxs += playerbox_stand_maxs;

	// We have to add some offset to the area bounds (an area is not always a box)
	const float areaBoundsOffset = ( areaSettings.areaflags & AREA_WALL ) ? 40.0f : 16.0f;

	int sideNum = 0;
	for(; sideNum < 2; ++sideNum ) {
		if( area.mins[sideNum] + areaBoundsOffset >= mins.Data()[sideNum] ) {
			break;
		}
		if( area.maxs[sideNum] + areaBoundsOffset <= maxs.Data()[sideNum] ) {
			break;
		}
	}

	// If the area bounds test has lead to conclusion that there is enough free space in side directions
	if( sideNum == 2 ) {
		if( CanSkipTracingForAreaHeight( origin, area, playerbox_stand_mins[2] + 0.25f ) ) {
			SetFullHeightCachedTracesEmpty( front2DDir, right2DDir );
			return true;
		}

		if( CanSkipTracingForAreaHeight( origin, area, playerbox_stand_maxs[2] + AI_JUMPABLE_HEIGHT - 0.5f ) ) {
			SetJumpableHeightCachedTracesEmpty( front2DDir, right2DDir );
			// We might still need to perform full height traces in TestForResultsMask()
			return false;
		}
	}

	// Test bounds around the origin.
	// Doing these tests can save expensive trace calls for separate directions

	// Convert these bounds to relative for being used as trace args
	mins -= origin;
	maxs -= origin;

	trace_t trace;
	mins.Z() += 0.25f;
	SolidWorldTrace( &trace, origin, origin, mins.Data(), maxs.Data() );
	if( trace.fraction == 1.0f ) {
		SetFullHeightCachedTracesEmpty( front2DDir, right2DDir );
		return true;
	}

	mins.Z() += AI_JUMPABLE_HEIGHT - 1.0f;
	SolidWorldTrace( &trace, origin, origin, mins.Data(), maxs.Data() );
	if( trace.fraction == 1.0f ) {
		SetJumpableHeightCachedTracesEmpty( front2DDir, right2DDir );
		// We might still need to perform full height traces in TestForResultsMask()
		return false;
	}

	return false;
}

void BotEnvironmentTraceCache::TestForResultsMask( BotMovementPredictionContext *context, unsigned requiredResultsMask ) {
	// There must not be any extra bits
	Assert( ( requiredResultsMask & ~ALL_SIDES_MASK ) == 0 );
	// All required traces have been already cached
	if( ( this->resultsMask & requiredResultsMask ) == requiredResultsMask ) {
		return;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	vec3_t front2DDir, right2DDir, traceEnd;
	Vec3 angles( entityPhysicsState.Angles() );
	angles.Data()[PITCH] = 0.0f;
	AngleVectors( angles.Data(), front2DDir, right2DDir, nullptr );

	if( !this->didAreaTest ) {
		this->didAreaTest = true;
		if( TrySkipTracingForCurrOrigin( context, front2DDir, right2DDir ) ) {
			return;
		}
	}

	const float *origin = entityPhysicsState.Origin();

	// First, test all full side traces.
	// If a full side trace is empty, a corresponding "jumpable" side trace can be set as empty too.

	// Test these bits for a quick computations shortcut
	unsigned actualFullSides = this->resultsMask & FULL_SIDES_MASK;
	unsigned resultFullSides = requiredResultsMask & FULL_SIDES_MASK;
	if( ( actualFullSides & resultFullSides ) != resultFullSides ) {
		const unsigned endMask = FullHeightMask( LAST_SIDE );
		for( unsigned i = 0, mask = FullHeightMask( FIRST_SIDE ); mask <= endMask; ++i, mask *= 2 ) {
			if( !( mask & requiredResultsMask ) || ( mask & this->resultsMask ) ) {
				continue;
			}

			MakeTraceDir( i, front2DDir, right2DDir, traceEnd );
			// Save the trace dir
			auto &fullResult = results[i];
			VectorCopy( traceEnd, fullResult.traceDir );
			// Convert from a direction to the end point
			VectorScale( traceEnd, TRACE_DEPTH, traceEnd );
			VectorAdd( traceEnd, origin, traceEnd );
			SolidWorldTrace( &fullResult.trace, origin, traceEnd, playerbox_stand_mins, playerbox_stand_maxs );
			this->resultsMask |= mask;
			// If full trace is empty, we can set partial trace as empty too
			if( fullResult.trace.fraction == 1.0f ) {
				auto &jumpableResult = results[i + 6];
				jumpableResult.trace.fraction = 1.0f;
				VectorCopy( fullResult.traceDir, jumpableResult.traceDir );
				this->resultsMask |= ( mask << 6 );
			}
		}
	}

	unsigned actualJumpableSides = this->resultsMask & JUMPABLE_SIDES_MASK;
	unsigned resultJumpableSides = requiredResultsMask & JUMPABLE_SIDES_MASK;
	if( ( actualJumpableSides & resultJumpableSides ) != resultJumpableSides ) {
		Vec3 mins( playerbox_stand_mins );
		mins.Z() += AI_JUMPABLE_HEIGHT;
		const unsigned endMask = JumpableHeightMask( LAST_SIDE );
		for( unsigned i = 0, mask = JumpableHeightMask( FIRST_SIDE ); mask <= endMask; ++i, mask *= 2 ) {
			if( !( mask & requiredResultsMask ) || ( mask & this->resultsMask ) ) {
				continue;
			}

			MakeTraceDir( i, front2DDir, right2DDir, traceEnd );
			// Save the trace dir
			auto &result = results[i + 6];
			VectorCopy( traceEnd, result.traceDir );
			// Convert from a direction to the end point
			VectorScale( traceEnd, TRACE_DEPTH, traceEnd );
			VectorAdd( traceEnd, origin, traceEnd );
			SolidWorldTrace( &result.trace, origin, traceEnd, mins.Data(), playerbox_stand_maxs );
			this->resultsMask |= mask;
		}
	}

	// Check whether all requires side traces have been computed
	Assert( ( this->resultsMask & requiredResultsMask ) == requiredResultsMask );
}

bool BotEnvironmentTraceCache::CanSkipPMoveCollision( BotMovementPredictionContext *context ) {
	// Do not force computations in this case.
	// Otherwise there is no speedup shown according to testing results.
	if( !this->didAreaTest ) {
		return false;
	}

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	// We might still need to check steps even if there is no full height obstacles around.
	if( entityPhysicsState.GroundEntity() ) {
		return false;
	}
	// If the bot is likely to land on this step
	if( entityPhysicsState.HeightOverGround() <= 8.0f && entityPhysicsState.Velocity()[2] < 0 ) {
		return false;
	}

	return this->hasNoFullHeightObstaclesAround;
}

// Make a type alias to fit into a line length limit
typedef BotEnvironmentTraceCache::ObstacleAvoidanceResult ObstacleAvoidanceResult;

ObstacleAvoidanceResult BotEnvironmentTraceCache::TryAvoidObstacles( BotMovementPredictionContext *context,
																	 Vec3 *intendedLookVec,
																	 float correctionFraction,
																	 unsigned sidesShift ) {
	TestForResultsMask( context, FRONT << sidesShift );
	const TraceResult &frontResult = results[0 + sidesShift];
	if( frontResult.trace.fraction == 1.0f ) {
		return ObstacleAvoidanceResult::NO_OBSTACLES;
	}

	TestForResultsMask( context, ( LEFT | RIGHT | FRONT_LEFT | FRONT_RIGHT ) << sidesShift );

	const auto &entityPhysicsState = context->movementState->entityPhysicsState;
	Vec3 velocityDir2D( entityPhysicsState.Velocity()[0], entityPhysicsState.Velocity()[1], 0 );
	velocityDir2D *= 1.0f / entityPhysicsState.Speed2D();

	// Make velocity direction dot product affect score stronger for lower correction fraction (for high speed)

	// This weight corresponds to a kept part of a trace fraction
	const float alpha = 0.51f + 0.24f * correctionFraction;
	// This weight corresponds to an added or subtracted part of a trace fraction multiplied by the dot product
	const float beta = 0.49f - 0.24f * correctionFraction;
	// Make sure a score is always positive
	Assert( alpha > beta );
	// Make sure that score is kept as is for the maximal dot product
	Assert( fabsf( alpha + beta - 1.0f ) < 0.0001f );

	float maxScore = frontResult.trace.fraction * ( alpha + beta * velocityDir2D.Dot( results[0 + sidesShift].traceDir ) );
	const TraceResult *bestTraceResult = nullptr;

	for( unsigned i = 2; i <= 5; ++i ) {
		const TraceResult &result = results[i + sidesShift];
		float score = result.trace.fraction;
		// Make sure that non-blocked directions are in another category
		if( score == 1.0f ) {
			score *= 3.0f;
		}

		score *= alpha + beta * velocityDir2D.Dot( result.traceDir );
		if( score <= maxScore ) {
			continue;
		}

		maxScore = score;
		bestTraceResult = &result;
	}

	if( bestTraceResult ) {
		intendedLookVec->NormalizeFast();
		*intendedLookVec *= ( 1.0f - correctionFraction );
		VectorMA( intendedLookVec->Data(), correctionFraction, bestTraceResult->traceDir, intendedLookVec->Data() );
		// There is no need to normalize intendedLookVec (we had to do it for correction fraction application)
		return ObstacleAvoidanceResult::CORRECTED;
	}

	return ObstacleAvoidanceResult::KEPT_AS_IS;
}
