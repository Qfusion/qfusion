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

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

struct cmodels_s;

// TEMP MOVE ME
gs_state_t gs;

void GS_asInitializeExport( void );

/*
* GS_TouchPushTrigger
*/
void GS_TouchPushTrigger( player_state_t *playerState, entity_state_t *pusher ) {
	// spectators don't use jump pads
	if( playerState->pmove.pm_type != PM_NORMAL ) {
		return;
	}

	VectorCopy( pusher->origin2, playerState->pmove.velocity );

	// reset walljump counter
	playerState->pmove.pm_flags &= ~PMF_WALLJUMPCOUNT;
	playerState->pmove.pm_flags |= PMF_JUMPPAD_TIME;
	playerState->pmove.pm_flags &= ~PMF_ON_GROUND;
	gs.api.PredictedEvent( playerState->POVnum, EV_JUMP_PAD, 0 );
}

/*
* GS_WaterLevel
*/
int GS_WaterLevel( entity_state_t *state, vec3_t mins, vec3_t maxs ) {
	vec3_t point;
	int cont;
	int waterlevel;

	waterlevel = 0;

	point[0] = state->origin[0];
	point[1] = state->origin[1];
	point[2] = state->origin[2] + mins[2] + 1;
	cont = gs.api.PointContents( point, 0 );
	if( cont & MASK_WATER ) {
		waterlevel = 1;
		point[2] += 26;
		cont = gs.api.PointContents( point, 0 );
		if( cont & MASK_WATER ) {
			waterlevel = 2;
			point[2] += 22;
			cont = gs.api.PointContents( point, 0 );
			if( cont & MASK_WATER ) {
				waterlevel = 3;
			}
		}
	}

	return waterlevel;
}

/*
* GS_BBoxForEntityState
*/
void GS_BBoxForEntityState( entity_state_t *state, vec3_t mins, vec3_t maxs ) {
	int x, zd, zu;

	if( state->solid == SOLID_BMODEL ) {
		gs.api.InlineModelBounds( gs.api.InlineModel( state->modelindex ), mins, maxs );
		return;
	}

    // encoded bbox
	x = 8 * ( state->solid & 31 );
	zd = 8 * ( ( state->solid >> 5 ) & 31 );
	zu = 8 * ( ( state->solid >> 10 ) & 63 ) - 32;

	mins[0] = mins[1] = -x;
	maxs[0] = maxs[1] = x;
	mins[2] = -zd;
	maxs[2] = zu;
}

/*
* GS_FrameForTime
* Returns the frame and interpolation fraction for current time in an animation started at a given time.
* When the animation is finished it will return frame -1. Takes looping into account. Looping animations
* are never finished.
*/
float GS_FrameForTime( int *frame, int64_t curTime, int64_t startTimeStamp, float frametime, int firstframe, int lastframe, int loopingframes, bool forceLoop ) {
	int64_t runningtime, framecount;
	int curframe;
	float framefrac;

	if( curTime <= startTimeStamp ) {
		*frame = firstframe;
		return 0.0f;
	}

	if( firstframe == lastframe ) {
		*frame = firstframe;
		return 1.0f;
	}

	runningtime = curTime - startTimeStamp;
	framefrac = ( (double)runningtime / (double)frametime );
	framecount = (unsigned int)framefrac;
	framefrac -= framecount;

	curframe = firstframe + framecount;
	if( curframe > lastframe ) {
		if( forceLoop && !loopingframes ) {
			loopingframes = lastframe - firstframe;
		}

		if( loopingframes ) {
			unsigned int numloops;
			unsigned int startcount;

			startcount = ( lastframe - firstframe ) - loopingframes;

			numloops = ( framecount - startcount ) / loopingframes;
			curframe -= loopingframes * numloops;
			if( loopingframes == 1 ) {
				framefrac = 1.0f;
			}
		} else {
			curframe = -1;
		}
	}

	*frame = curframe;

	return framefrac;
}

//============================================================================

/*
* GS_SetGametypeName
*/
void GS_SetGametypeName( const char *name ) {
	Q_strncpyz( gs.gametypeName, name, sizeof( gs.gametypeName ) );
}

/*
* GS_Obituary
*
* Can be called by either the server or the client
*/
void GS_Obituary( void *victim, int gender, void *attacker, int mod, char *message, char *message2 ) {
	message[0] = 0;
	message2[0] = 0;

	if( !attacker || attacker == victim ) {
		switch( mod ) {
			case MOD_SUICIDE:
				strcpy( message, "suicides" );
				break;
			case MOD_FALLING:
				strcpy( message, "cratered" );
				break;
			case MOD_CRUSH:
				strcpy( message, "was squished" );
				break;
			case MOD_WATER:
				strcpy( message, "sank like a rock" );
				break;
			case MOD_SLIME:
				strcpy( message, "melted" );
				break;
			case MOD_LAVA:
				strcpy( message, "sacrificed to the lava god" ); // wsw : pb : some killed messages
				break;
			case MOD_EXPLOSIVE:
			case MOD_BARREL:
				strcpy( message, "blew up" );
				break;
			case MOD_EXIT:
				strcpy( message, "found a way out" );
				break;
			case MOD_BOMB:
			case MOD_SPLASH:
			case MOD_TRIGGER_HURT:
				strcpy( message, "was in the wrong place" );
				break;
			default:
				strcpy( message, "died" );
				break;
		}
		return;
	}

	switch( mod ) {
		case MOD_TELEFRAG:
			strcpy( message, "tried to invade" );
			strcpy( message2, "'s personal space" );
			break;
		case MOD_GUNBLADE_W:
			strcpy( message, "was impaled by" );
			strcpy( message2, "'s gunblade" );
			break;
		case MOD_GUNBLADE_S:
			strcpy( message, "could not hide from" );
			strcpy( message2, "'s almighty gunblade" );
			break;
		case MOD_MACHINEGUN_W:
		case MOD_MACHINEGUN_S:
			strcpy( message, "was penetrated by" );
			strcpy( message2, "'s machinegun" );
			break;
		case MOD_RIOTGUN_W:
		case MOD_RIOTGUN_S:
			strcpy( message, "was shred by" );
			strcpy( message2, "'s riotgun" );
			break;
		case MOD_GRENADE_W:
		case MOD_GRENADE_S:
			strcpy( message, "was popped by" );
			strcpy( message2, "'s grenade" );
			break;
		case MOD_ROCKET_W:
		case MOD_ROCKET_S:
			strcpy( message, "ate" );
			strcpy( message2, "'s rocket" );
			break;
		case MOD_PLASMA_W:
		case MOD_PLASMA_S:
			strcpy( message, "was melted by" );
			strcpy( message2, "'s plasmagun" );
			break;
		case MOD_ELECTROBOLT_W:
		case MOD_ELECTROBOLT_S:
			strcpy( message, "was bolted by" );
			strcpy( message2, "'s electrobolt" );
			break;
		case MOD_INSTAGUN_W:
		case MOD_INSTAGUN_S:
			strcpy( message, "was instagibbed by" );
			strcpy( message2, "'s instabeam" );
			break;
		case MOD_LASERGUN_W:
		case MOD_LASERGUN_S:
			strcpy( message, "was cut by" );
			strcpy( message2, "'s lasergun" );
			break;
		case MOD_GRENADE_SPLASH_W:
		case MOD_GRENADE_SPLASH_S:
			strcpy( message, "didn't see" );
			strcpy( message2, "'s grenade" );
			break;
		case MOD_ROCKET_SPLASH_W:
		case MOD_ROCKET_SPLASH_S:
			strcpy( message, "almost dodged" );
			strcpy( message2, "'s rocket" );
			break;

		case MOD_PLASMA_SPLASH_W:
		case MOD_PLASMA_SPLASH_S:
			strcpy( message, "was melted by" );
			strcpy( message2, "'s plasmagun" );
			break;

		default:
			strcpy( message, "was fragged by" );
			break;
	}
}

/*
* GS_InitModule
*/
void GS_InitModule( int module, int maxClients, gs_module_api_t *api ) {
	memset( &gs, 0, sizeof( gs_state_t ) );

	gs.module = module;
	gs.maxclients = maxClients;
	gs.api = *api;

	GS_asInitializeExport();
}
