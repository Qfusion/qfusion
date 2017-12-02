/*
Copyright (C) 2007 German Garcia

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

// gs_players.c	-	player model animations

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"


#define MOVEDIREPSILON  0.3f
#define WALKEPSILON 5.0f
#define RUNEPSILON  220.0f

// movement flags for animation control
#define ANIMMOVE_FRONT      0x00000001  //	Player is pressing fordward
#define ANIMMOVE_BACK       0x00000002  //	Player is pressing backpedal
#define ANIMMOVE_LEFT       0x00000004  //	Player is pressing sideleft
#define ANIMMOVE_RIGHT      0x00000008  //	Player is pressing sideright
#define ANIMMOVE_WALK       0x00000010  //	Player is pressing the walk key
#define ANIMMOVE_RUN        0x00000020  //	Player is running
#define ANIMMOVE_DUCK       0x00000040  //	Player is crouching
#define ANIMMOVE_SWIM       0x00000080  //	Player is swimming
#define ANIMMOVE_AIR        0x00000100  //	Player is at air, but not jumping

typedef struct {
	int moveflags;              //moving direction
	int animState[PMODEL_PARTS];
} pm_anim_t;

/*
* GS_SetBaseAnimUpper
*/
static void GS_SetBaseAnimUpper( pm_anim_t *pmanim, int carried_weapon ) {
	//SWIMMING
	if( pmanim->moveflags & ANIMMOVE_SWIM ) {
		pmanim->animState[UPPER] = TORSO_SWIM;
	} else {
		switch( carried_weapon ) {
			case WEAP_NONE:
				pmanim->animState[UPPER] = TORSO_HOLD_BLADE; // fixme: a special animation should exist
				break;
			case WEAP_GUNBLADE:
				pmanim->animState[UPPER] =  TORSO_HOLD_BLADE;
				break;
			case WEAP_LASERGUN:
				pmanim->animState[UPPER] =  TORSO_HOLD_PISTOL;
				break;
			default:
			case WEAP_RIOTGUN:
			case WEAP_PLASMAGUN:
				pmanim->animState[UPPER] =  TORSO_HOLD_LIGHTWEAPON;
				break;
			case WEAP_ROCKETLAUNCHER:
			case WEAP_GRENADELAUNCHER:
				pmanim->animState[UPPER] =  TORSO_HOLD_HEAVYWEAPON;
				break;
			case WEAP_ELECTROBOLT:
				pmanim->animState[UPPER] =  TORSO_HOLD_AIMWEAPON;
				break;
		}
	}
}

/*
* GS_SetBaseAnimLower
*/
static void GS_SetBaseAnimLower( pm_anim_t *pmanim ) {
	//SWIMMING
	if( pmanim->moveflags & ANIMMOVE_SWIM ) {
		if( pmanim->moveflags & ANIMMOVE_FRONT ) {
			pmanim->animState[LOWER] = LEGS_SWIM_FORWARD;
		} else {
			pmanim->animState[LOWER] = LEGS_SWIM_NEUTRAL;
		}
	}
	//CROUCH
	else if( pmanim->moveflags & ANIMMOVE_DUCK ) {
		if( pmanim->moveflags & ( ANIMMOVE_WALK | ANIMMOVE_RUN ) ) {
			pmanim->animState[LOWER] = LEGS_CROUCH_WALK;
		} else {
			pmanim->animState[LOWER] = LEGS_CROUCH_IDLE;
		}
	}
	//FALLING
	else if( pmanim->moveflags & ANIMMOVE_AIR ) {
		pmanim->animState[LOWER] = LEGS_JUMP_NEUTRAL;
	}
	// RUN
	else if( pmanim->moveflags & ANIMMOVE_RUN ) {
		//front/backward has priority over side movements
		if( pmanim->moveflags & ANIMMOVE_FRONT ) {
			pmanim->animState[LOWER] = LEGS_RUN_FORWARD;

		} else if( pmanim->moveflags & ANIMMOVE_BACK ) {
			pmanim->animState[LOWER] = LEGS_RUN_BACK;

		} else if( pmanim->moveflags & ANIMMOVE_RIGHT ) {
			pmanim->animState[LOWER] = LEGS_RUN_RIGHT;

		} else if( pmanim->moveflags & ANIMMOVE_LEFT ) {
			pmanim->animState[LOWER] = LEGS_RUN_LEFT;

		} else {   //is moving by inertia
			pmanim->animState[LOWER] = LEGS_WALK_FORWARD;
		}
	}
	//WALK
	else if( pmanim->moveflags & ANIMMOVE_WALK ) {
		//front/backward has priority over side movements
		if( pmanim->moveflags & ANIMMOVE_FRONT ) {
			pmanim->animState[LOWER] = LEGS_WALK_FORWARD;

		} else if( pmanim->moveflags & ANIMMOVE_BACK ) {
			pmanim->animState[LOWER] = LEGS_WALK_BACK;

		} else if( pmanim->moveflags & ANIMMOVE_RIGHT ) {
			pmanim->animState[LOWER] = LEGS_WALK_RIGHT;

		} else if( pmanim->moveflags & ANIMMOVE_LEFT ) {
			pmanim->animState[LOWER] = LEGS_WALK_LEFT;

		} else {   //is moving by inertia
			pmanim->animState[LOWER] = LEGS_WALK_FORWARD;
		}
	} else {   // STAND
		pmanim->animState[LOWER] = LEGS_STAND_IDLE;
	}
}

/*
* GS_SetBaseAnims
*/
static void GS_SetBaseAnims( pm_anim_t *pmanim, int carried_weapon ) {
	int part;

	for( part = 0; part < PMODEL_PARTS; part++ ) {
		switch( part ) {
			case LOWER:
				GS_SetBaseAnimLower( pmanim );
				break;

			case UPPER:
				GS_SetBaseAnimUpper( pmanim, carried_weapon );
				break;

			case HEAD:
			default:
				pmanim->animState[part] = 0;
				break;
		}
	}
}

/*
* GS_UpdateBaseAnims
*/
int GS_UpdateBaseAnims( entity_state_t *state, vec3_t velocity ) {
	pm_anim_t pmanim;
	vec3_t movedir;
	vec3_t hvel;
	mat3_t viewaxis;
	float xyspeedcheck;
	int waterlevel;
	vec3_t mins, maxs;
	vec3_t point;
	trace_t trace;

	if( !state ) {
		gs.api.Error( "GS_UpdateBaseAnims: NULL state\n" );
		return 0;
	}

	GS_BBoxForEntityState( state, mins, maxs );

	memset( &pmanim, 0, sizeof( pm_anim_t ) );

	// determine if player is at ground, for walking or falling
	// this is not like having groundEntity, we are more generous with
	// the tracing size here to include small steps
	point[0] = state->origin[0];
	point[1] = state->origin[1];
	point[2] = state->origin[2] - ( 1.6 * STEPSIZE );
	gs.api.Trace( &trace, state->origin, mins, maxs, point, state->number, MASK_PLAYERSOLID, 0 );
	if( trace.ent == -1 || ( trace.fraction < 1.0f && !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid ) ) {
		pmanim.moveflags |= ANIMMOVE_AIR;
	}

	// crouching : fixme? : it assumes the entity is using the player box sizes
	if( VectorCompare( maxs, playerbox_crouch_maxs ) ) {
		pmanim.moveflags |= ANIMMOVE_DUCK;
	}

	// find out the water level
	waterlevel = GS_WaterLevel( state, mins, maxs );
	if( waterlevel >= 2 || ( waterlevel && ( pmanim.moveflags & ANIMMOVE_AIR ) ) ) {
		pmanim.moveflags |= ANIMMOVE_SWIM;
	}

	//find out what are the base movements the model is doing

	hvel[0] = velocity[0];
	hvel[1] = velocity[1];
	hvel[2] = 0;
	xyspeedcheck = VectorNormalize2( hvel, movedir );
	if( xyspeedcheck > WALKEPSILON ) {
		VectorNormalizeFast( movedir );
		Matrix3_FromAngles( tv( 0, state->angles[YAW], 0 ), viewaxis );

		// if it's moving to where is looking, it's moving forward
		if( DotProduct( movedir, &viewaxis[AXIS_RIGHT] ) > MOVEDIREPSILON ) {
			pmanim.moveflags |= ANIMMOVE_RIGHT;
		} else if( -DotProduct( movedir, &viewaxis[AXIS_RIGHT] ) > MOVEDIREPSILON ) {
			pmanim.moveflags |= ANIMMOVE_LEFT;
		}
		if( DotProduct( movedir, &viewaxis[AXIS_FORWARD] ) > MOVEDIREPSILON ) {
			pmanim.moveflags |= ANIMMOVE_FRONT;
		} else if( -DotProduct( movedir, &viewaxis[AXIS_FORWARD] ) > MOVEDIREPSILON ) {
			pmanim.moveflags |= ANIMMOVE_BACK;
		}

		if( xyspeedcheck > RUNEPSILON ) {
			pmanim.moveflags |= ANIMMOVE_RUN;
		} else if( xyspeedcheck > WALKEPSILON ) {
			pmanim.moveflags |= ANIMMOVE_WALK;
		}
	}

	GS_SetBaseAnims( &pmanim, state->weapon );
	return ( ( pmanim.animState[LOWER] & 0x3F ) | ( pmanim.animState[UPPER] & 0x3F ) << 6 | ( pmanim.animState[HEAD] & 0xF ) << 12 );
}

#undef MOVEDIREPSILON
#undef WALKEPSILON
#undef RUNEPSILON

/*
*GS_PModel_AnimToFrame
*
* BASE_CHANEL plays continuous animations forced to loop.
* if the same animation is received twice it will *not* restart
* but continue looping.
*
* EVENT_CHANNEL overrides base channel and plays until
* the animation is finished. Then it returns to base channel.
* If an animation is received twice, it will be restarted.
* If an event channel animation has a loop setting, it will
* continue playing it until a new event chanel animation
* is fired.
*/
void GS_PModel_AnimToFrame( int64_t curTime, gs_pmodel_animationset_t *animSet, gs_pmodel_animationstate_t *anim ) {
	int i, channel = BASE_CHANNEL;

	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		for( channel = BASE_CHANNEL; channel < PLAYERANIM_CHANNELS; channel++ ) {
			gs_animstate_t *thisAnim = &anim->curAnims[i][channel];

			// see if there are new animations to be played
			if( anim->buffer[channel].newanim[i] != ANIM_NONE ) {
				if( channel == EVENT_CHANNEL ||
					( channel == BASE_CHANNEL && anim->buffer[channel].newanim[i] != thisAnim->anim ) ) {
					thisAnim->anim = anim->buffer[channel].newanim[i];
					thisAnim->startTimestamp = curTime;
				}

				anim->buffer[channel].newanim[i] = ANIM_NONE;
			}

			if( thisAnim->anim ) {
				bool forceLoop = ( channel == BASE_CHANNEL ) ? true : false;

				thisAnim->lerpFrac = GS_FrameForTime( &thisAnim->frame, curTime, thisAnim->startTimestamp,
													  animSet->frametime[thisAnim->anim], animSet->firstframe[thisAnim->anim], animSet->lastframe[thisAnim->anim],
													  animSet->loopingframes[thisAnim->anim], forceLoop );

				// the animation was completed
				if( thisAnim->frame < 0 ) {
					assert( channel != BASE_CHANNEL );
					thisAnim->anim = ANIM_NONE;
				}
			}
		}
	}

	// we set all animations up, but now select which ones are going to be shown
	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		int lastframe = anim->frame[i];
		channel = ( anim->curAnims[i][EVENT_CHANNEL].anim != ANIM_NONE ) ? EVENT_CHANNEL : BASE_CHANNEL;
		anim->frame[i] = anim->curAnims[i][channel].frame;
		anim->lerpFrac[i] = anim->curAnims[i][channel].lerpFrac;
		if( !lastframe || !anim->oldframe[i] ) {
			anim->oldframe[i] = anim->frame[i];
		} else if( anim->frame[i] != lastframe ) {
			anim->oldframe[i] = lastframe;
		}
	}
}

/*
* GS_PModel_ClearEventAnimations
*/
void GS_PlayerModel_ClearEventAnimations( gs_pmodel_animationset_t *animSet, gs_pmodel_animationstate_t *animState ) {
	int i;

	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		animState->buffer[EVENT_CHANNEL].newanim[i] = ANIM_NONE;
		animState->curAnims[i][EVENT_CHANNEL].anim = ANIM_NONE;
	}
}

/*
* GS_PModel_AddAnimation
*/
void GS_PlayerModel_AddAnimation( gs_pmodel_animationstate_t *animState, int loweranim, int upperanim, int headanim, int channel ) {
	int i;
	int newanim[PMODEL_PARTS];
	gs_animationbuffer_t *buffer;

	assert( animState != NULL );

	newanim[LOWER] = loweranim;
	newanim[UPPER] = upperanim;
	newanim[HEAD] = headanim;

	buffer = &animState->buffer[channel];

	for( i = LOWER; i < PMODEL_PARTS; i++ ) {
		//ignore new events if in death
		if( channel && buffer->newanim[i] && ( buffer->newanim[i] <= BOTH_DEAD1 ) ) {
			continue;
		}

		if( newanim[i] && ( newanim[i] < PMODEL_TOTAL_ANIMATIONS ) ) {
			buffer->newanim[i] = newanim[i];
		}
	}
}
