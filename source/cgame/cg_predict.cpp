/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "cg_local.h"

int cg_numSolids;
static entity_state_t *cg_solidList[MAX_SNAPSHOT_ENTITIES];

int cg_numTriggers;
static entity_state_t *cg_triggersList[MAX_SNAPSHOT_ENTITIES];
static bool			   cg_triggersListTriggered[MAX_SNAPSHOT_ENTITIES];

static bool ucmdReady = false;

/*
* CG_PredictedEvent - shared code can fire events during prediction
*/
void CG_PredictedEvent( int entNum, int ev, int parm ) {
	if( ev >= PREDICTABLE_EVENTS_MAX ) {
		return;
	}

	// ignore this action if it has already been predicted (the unclosed ucmd has timestamp zero)
	if( ucmdReady && ( cg.predictingTimeStamp > cg.predictedEventTimes[ev] ) ) {
		// inhibit the fire event when there is a weapon change predicted
		if( ev == EV_FIREWEAPON ) {
			if( cg.predictedWeaponSwitch && ( cg.predictedWeaponSwitch != cg.predictedPlayerState.stats[STAT_PENDING_WEAPON] ) ) {
				return;
			}
		}

		cg.predictedEventTimes[ev] = cg.predictingTimeStamp;
		CG_EntityEvent( &cg_entities[entNum].current, ev, parm, true );
	}
}

/*
* CG_Predict_ChangeWeapon
*/
void CG_Predict_ChangeWeapon( int new_weapon ) {
	if( cg.view.playerPrediction ) {
		cg.predictedWeaponSwitch = new_weapon;
	}
}

/*
* CG_CheckPredictionError
*/
void CG_CheckPredictionError( void ) {
	int delta[3];
	int frame;
	vec3_t origin;

	if( !cg.view.playerPrediction ) {
		return;
	}

	// calculate the last usercmd_t we sent that the server has processed
	frame = cg.frame.ucmdExecuted & CMD_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorCopy( cg.predictedOrigins[frame], origin );

	if( cg.predictedGroundEntity != -1 ) {
		entity_state_t *ent = &cg_entities[cg.predictedGroundEntity].current;
		if( ent->solid == SOLID_BMODEL ) {
			if( ent->linearMovement ) {
				vec3_t move;
				GS_LinearMovementDelta( ent, cg.oldFrame.serverTime, cg.frame.serverTime, move );
				VectorAdd( cg.predictedOrigins[frame], move, origin );
			}
		}
	}

	VectorSubtract( cg.frame.playerState.pmove.origin, origin, delta );

	// save the prediction error for interpolation
	if( abs( delta[0] ) > 128 || abs( delta[1] ) > 128 || abs( delta[2] ) > 128 ) {
		if( cg_showMiss->integer ) {
			CG_Printf( "prediction miss on %" PRIi64 ": %i\n", cg.frame.serverFrame, abs( delta[0] ) + abs( delta[1] ) + abs( delta[2] ) );
		}
		VectorClear( cg.predictionError );          // a teleport or something
	} else {
		if( cg_showMiss->integer && ( delta[0] || delta[1] || delta[2] ) ) {
			CG_Printf( "prediction miss on %" PRIi64" : %i\n", cg.frame.serverFrame, abs( delta[0] ) + abs( delta[1] ) + abs( delta[2] ) );
		}
		VectorCopy( cg.frame.playerState.pmove.origin, cg.predictedOrigins[frame] );
		VectorCopy( delta, cg.predictionError ); // save for error interpolation
	}
}

/*
* CG_BuildSolidList
*/
void CG_BuildSolidList( void ) {
	int i;
	entity_state_t *ent;

	cg_numSolids = 0;
	cg_numTriggers = 0;
	for( i = 0; i < cg.frame.numEntities; i++ ) {
		ent = &cg.frame.entities[i];
		if( ISEVENTENTITY( ent ) ) {
			continue;
		}

		if( ent->solid ) {
			switch( ent->type ) {
				// the following entities can never be solid
				case ET_BEAM:
				case ET_PORTALSURFACE:
				case ET_BLASTER:
				case ET_ELECTRO_WEAK:
				case ET_ROCKET:
				case ET_GRENADE:
				case ET_PLASMA:
				case ET_LASERBEAM:
				case ET_CURVELASERBEAM:
				case ET_MINIMAP_ICON:
				case ET_DECAL:
				case ET_ITEM_TIMER:
				case ET_PARTICLES:
					break;

				case ET_PUSH_TRIGGER:
					cg_triggersList[cg_numTriggers++] = &cg_entities[ ent->number ].current;
					break;

				default:
					cg_solidList[cg_numSolids++] = &cg_entities[ ent->number ].current;
					break;
			}
		}
	}
}

/*
* CG_ClipEntityContact
*/
static bool CG_ClipEntityContact( const vec3_t origin, const vec3_t mins, const vec3_t maxs, int entNum ) {
	centity_t *cent;
	struct cmodel_s *cmodel;
	trace_t tr;
	vec3_t absmins, absmaxs;
	vec3_t entorigin, entangles;
	int64_t serverTime = cg.frame.serverTime;

	if( !mins ) {
		mins = vec3_origin;
	}
	if( !maxs ) {
		maxs = vec3_origin;
	}

	// find the cmodel
	cmodel = CG_CModelForEntity( entNum );
	if( !cmodel ) {
		return false;
	}

	cent = &cg_entities[entNum];

	// find the origin
	if( cent->current.solid == SOLID_BMODEL ) { // special value for bmodel
		if( cent->current.linearMovement ) {
			GS_LinearMovement( &cent->current, serverTime, entorigin );
		} else {
			VectorCopy( cent->current.origin, entorigin );
		}
		VectorCopy( cent->current.angles, entangles );
	} else {   // encoded bbox
		VectorCopy( cent->current.origin, entorigin );
		VectorClear( entangles ); // boxes don't rotate
	}

	// convert the box to compare to absolute coordinates
	VectorAdd( origin, mins, absmins );
	VectorAdd( origin, maxs, absmaxs );
	trap_CM_TransformedBoxTrace( &tr, vec3_origin, vec3_origin, absmins, absmaxs, cmodel, MASK_ALL, entorigin, entangles );
	return tr.startsolid == true || tr.allsolid == true;
}

/*
* CG_Predict_TouchTriggers
*/
void CG_Predict_TouchTriggers( pmove_t *pm, player_state_t *ps, vec3_t previous_origin ) {
	int i;
	entity_state_t *state;

	// fixme: more accurate check for being able to touch or not
	if( ps->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	for( i = 0; i < cg_numTriggers; i++ ) {
		state = cg_triggersList[i];

		if( state->type == ET_PUSH_TRIGGER ) {
			if( !cg_triggersListTriggered[i] ) {
				if( CG_ClipEntityContact( ps->pmove.origin, pm->mins, pm->maxs, state->number ) ) {
					GS_TouchPushTrigger( ps, state );
					cg_triggersListTriggered[i] = true;
				}
			}
		}
	}

	VectorCopy( ps->pmove.origin, pm->origin );
	VectorCopy( ps->pmove.velocity, pm->velocity );
}

/*
* CG_ClipMoveToEntities
*/
static void CG_ClipMoveToEntities( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int ignore, int contentmask, trace_t *tr ) {
	int i, x, zd, zu;
	trace_t trace;
	vec3_t origin, angles;
	entity_state_t *ent;
	struct cmodel_s *cmodel;
	vec3_t bmins, bmaxs;
	int64_t serverTime = cg.frame.serverTime;

	for( i = 0; i < cg_numSolids; i++ ) {
		ent = cg_solidList[i];

		if( ent->number == ignore ) {
			continue;
		}
		if( !( contentmask & CONTENTS_CORPSE ) && ( ( ent->type == ET_CORPSE ) || ( ent->type == ET_GIB ) ) ) {
			continue;
		}

		if( ent->solid == SOLID_BMODEL ) { // special value for bmodel
			cmodel = trap_CM_InlineModel( ent->modelindex );
			if( !cmodel ) {
				continue;
			}

			if( ent->linearMovement ) {
				GS_LinearMovement( ent, serverTime, origin );
			} else {
				VectorCopy( ent->origin, origin );
			}

			VectorCopy( ent->angles, angles );
		} else {   // encoded bbox
			x = 8 * ( ent->solid & 31 );
			zd = 8 * ( ( ent->solid >> 5 ) & 31 );
			zu = 8 * ( ( ent->solid >> 10 ) & 63 ) - 32;

			bmins[0] = bmins[1] = -x;
			bmaxs[0] = bmaxs[1] = x;
			bmins[2] = -zd;
			bmaxs[2] = zu;

			VectorCopy( ent->origin, origin );
			VectorClear( angles ); // boxes don't rotate

			if( ent->type == ET_PLAYER || ent->type == ET_CORPSE ) {
				cmodel = trap_CM_OctagonModelForBBox( bmins, bmaxs );
			} else {
				cmodel = trap_CM_ModelForBBox( bmins, bmaxs );
			}
		}

		trap_CM_TransformedBoxTrace( &trace, (vec_t *)start, (vec_t *)end, (vec_t *)mins, (vec_t *)maxs, cmodel, contentmask, origin, angles );
		if( trace.allsolid || trace.fraction < tr->fraction ) {
			trace.ent = ent->number;
			*tr = trace;
		} else if( trace.startsolid ) {
			tr->startsolid = true;
		}

		if( tr->allsolid ) {
			return;
		}
	}
}

/*
* CG_Trace
*/
void CG_Trace( trace_t *t, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int contentmask ) {
	// check against world
	trap_CM_TransformedBoxTrace( t, start, end, mins, maxs, NULL, contentmask, NULL, NULL );
	t->ent = t->fraction < 1.0 ? 0 : -1; // world entity is 0
	if( t->fraction == 0 ) {
		return; // blocked by the world

	}

	// check all other solid models
	CG_ClipMoveToEntities( start, mins, maxs, end, ignore, contentmask, t );
}

/*
* CG_PointContents
*/
int CG_PointContents( const vec3_t point ) {
	int i;
	entity_state_t *ent;
	struct cmodel_s *cmodel;
	int contents;

	contents = trap_CM_TransformedPointContents( (vec_t *)point, NULL, NULL, NULL );

	for( i = 0; i < cg_numSolids; i++ ) {
		ent = cg_solidList[i];
		if( ent->solid != SOLID_BMODEL ) { // special value for bmodel
			continue;
		}

		cmodel = trap_CM_InlineModel( ent->modelindex );
		if( cmodel ) {
			contents |= trap_CM_TransformedPointContents( (vec_t *)point, cmodel, ent->origin, ent->angles );
		}
	}

	return contents;
}


static float predictedSteps[CMD_BACKUP]; // for step smoothing
/*
* CG_PredictAddStep
*/
static void CG_PredictAddStep( int virtualtime, int predictiontime, float stepSize ) {

	float oldStep;
	int delta;

	// check for stepping up before a previous step is completed
	delta = cg.realTime - cg.predictedStepTime;
	if( delta < PREDICTED_STEP_TIME ) {
		oldStep = cg.predictedStep * ( (float)( PREDICTED_STEP_TIME - delta ) / (float)PREDICTED_STEP_TIME );
	} else {
		oldStep = 0;
	}

	cg.predictedStep = oldStep + stepSize;
	cg.predictedStepTime = cg.realTime - ( predictiontime - virtualtime );
}

/*
* CG_PredictSmoothSteps
*/
static void CG_PredictSmoothSteps( void ) {
	int64_t outgoing;
	int64_t frame;
	usercmd_t cmd;
	int i;
	int virtualtime = 0, predictiontime = 0;

	cg.predictedStepTime = 0;
	cg.predictedStep = 0;

	trap_NET_GetCurrentState( NULL, &outgoing, NULL );

	i = outgoing;
	while( predictiontime < PREDICTED_STEP_TIME ) {
		if( outgoing - i >= CMD_BACKUP ) {
			break;
		}

		frame = i & CMD_MASK;
		trap_NET_GetUserCmd( frame, &cmd );
		predictiontime += cmd.msec;
		i--;
	}

	// run frames
	while( ++i <= outgoing ) {
		frame = i & CMD_MASK;
		trap_NET_GetUserCmd( frame, &cmd );
		virtualtime += cmd.msec;

		if( predictedSteps[frame] ) {
			CG_PredictAddStep( virtualtime, predictiontime, predictedSteps[frame] );
		}
	}
}

/*
* CG_PredictMovement
*
* Sets cg.predictedVelocty, cg.predictedOrigin and cg.predictedAngles
*/
void CG_PredictMovement( void ) {
	int64_t ucmdExecuted, ucmdHead;
	int64_t frame;
	pmove_t pm;
	usercmd_t ucmd;

	trap_NET_GetCurrentState( NULL, &ucmdHead, NULL );
	ucmdExecuted = cg.frame.ucmdExecuted;

	if( ucmdHead - cg.predictFrom >= CMD_BACKUP ) {
		cg.predictFrom = 0;
	}

	if( cg.predictFrom > 0 ) {
		ucmdExecuted = cg.predictFrom;
		cg.predictedPlayerState = cg.predictFromPlayerState;
		cg_entities[cg.frame.playerState.POVnum].current = cg.predictFromEntityState;
	} else {
		cg.predictedPlayerState = cg.frame.playerState; // start from the final position
	}

	cg.predictedPlayerState.POVnum = cgs.playerNum + 1;

	// if we are too far out of date, just freeze
	if( ucmdHead - ucmdExecuted >= CMD_BACKUP ) {
		if( cg_showMiss->integer ) {
			CG_Printf( "exceeded CMD_BACKUP\n" );
		}

		cg.predictingTimeStamp = cg.time;
		return;
	}

	// copy current state to pmove
	memset( &pm, 0, sizeof( pm ) );

	// clear the triggered toggles for this prediction round
	memset( &cg_triggersListTriggered, false, sizeof( cg_triggersListTriggered ) );

	// run frames
	while( ++ucmdExecuted <= ucmdHead ) {
		frame = ucmdExecuted & CMD_MASK;
		trap_NET_GetUserCmd( frame, &ucmd );

		ucmdReady = ( ucmd.serverTimeStamp != 0 );
		if( ucmdReady ) {
			cg.predictingTimeStamp = ucmd.serverTimeStamp;
		}

		PM_Pmove( &pm, &cg.predictedPlayerState, &ucmd, &CG_asGetViewAnglesClamp, &CG_asPMove );

		// copy for stair smoothing
		predictedSteps[frame] = pm.step;

		if( ucmdReady ) { // hmm fixme: the wip command may not be run enough time to get proper key presses
			if( ucmdExecuted >= ucmdHead - 1 ) {
				GS_AddLaserbeamPoint( &cg.weaklaserTrail, &cg.predictedPlayerState, ucmd.serverTimeStamp );
			}

			cg_entities[cg.predictedPlayerState.POVnum].current.weapon = GS_ThinkPlayerWeapon( &cg.predictedPlayerState, ucmd.buttons, ucmd.msec, 0 );
		}

		// save for debug checking
		VectorCopy( cg.predictedPlayerState.pmove.origin, cg.predictedOrigins[frame] ); // store for prediction error checks

		// backup the last predicted ucmd which has a timestamp (it's closed)
		if( ucmdExecuted == ucmdHead - 1 ) {
			if( ucmdExecuted != cg.predictFrom ) {
				cg.predictFrom = ucmdExecuted;
				cg.predictFromPlayerState = cg.predictedPlayerState;
				cg.predictFromEntityState = cg_entities[cg.frame.playerState.POVnum].current;
			}
		}
	}

	cg.predictedGroundEntity = pm.groundentity;

	// compensate for ground entity movement
	if( pm.groundentity != -1 ) {
		entity_state_t *ent = &cg_entities[pm.groundentity].current;

		if( ent->solid == SOLID_BMODEL ) {
			if( ent->linearMovement ) {
				vec3_t move;
				int64_t serverTime;

				serverTime = GS_MatchPaused() ? cg.frame.serverTime : cg.time + cgs.extrapolationTime;
				GS_LinearMovementDelta( ent, cg.frame.serverTime, serverTime, move );
				VectorAdd( cg.predictedPlayerState.pmove.origin, move, cg.predictedPlayerState.pmove.origin );
			}
		}
	}

	CG_PredictSmoothSteps();
}
