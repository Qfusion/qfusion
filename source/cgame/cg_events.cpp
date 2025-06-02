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

//=========================================================
#define CG_MAX_ANNOUNCER_EVENTS 32
#define CG_MAX_ANNOUNCER_EVENTS_MASK ( CG_MAX_ANNOUNCER_EVENTS - 1 )
#define CG_ANNOUNCER_EVENTS_FRAMETIME 1500 // the announcer will speak each 1.5 seconds
typedef struct cg_announcerevent_s {
	struct sfx_s *sound;
} cg_announcerevent_t;
cg_announcerevent_t cg_announcerEvents[CG_MAX_ANNOUNCER_EVENTS];
static int			cg_announcerEventsCurrent = 0;
static int			cg_announcerEventsHead = 0;
static int			cg_announcerEventsDelay = 0;

/*
 * CG_ClearAnnouncerEvents
 */
void CG_ClearAnnouncerEvents( void )
{
	cg_announcerEventsCurrent = cg_announcerEventsHead = 0;
}

/*
 * CG_AddAnnouncerEvent
 */
void CG_AddAnnouncerEvent( struct sfx_s *sound, bool queued )
{
	if( !sound ) {
		return;
	}

	if( !queued ) {
		trap_S_StartLocalSound( sound, CHAN_ANNOUNCER, cg_volume_announcer->value );
		cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		return;
	}

	if( cg_announcerEventsCurrent + CG_MAX_ANNOUNCER_EVENTS >= cg_announcerEventsHead ) {
		// full buffer (we do nothing, just let it overwrite the oldest
	}

	// add it
	cg_announcerEvents[cg_announcerEventsHead & CG_MAX_ANNOUNCER_EVENTS_MASK].sound = sound;
	cg_announcerEventsHead++;
}

/*
 * CG_ReleaseAnnouncerEvents
 */
void CG_ReleaseAnnouncerEvents( void )
{
	// see if enough time has passed
	cg_announcerEventsDelay -= cg.realFrameTime;
	if( cg_announcerEventsDelay > 0 ) {
		return;
	}

	if( cg_announcerEventsCurrent < cg_announcerEventsHead ) {
		struct sfx_s *sound;

		// play the event
		sound = cg_announcerEvents[cg_announcerEventsCurrent & CG_MAX_ANNOUNCER_EVENTS_MASK].sound;
		if( sound ) {
			trap_S_StartLocalSound( sound, CHAN_ANNOUNCER, cg_volume_announcer->value );
			cg_announcerEventsDelay = CG_ANNOUNCER_EVENTS_FRAMETIME; // wait
		}
		cg_announcerEventsCurrent++;
	} else {
		cg_announcerEventsDelay = 0; // no wait
	}
}

/*
* CG_EntityEvent
*/
void CG_EntityEvent( entity_state_t *ent, int ev, int parm, bool predicted ) {
	CG_asEntityEvent( ent, ev, parm, predicted );
}

/*
* CG_FireEvents
*/
static void CG_FireEntityEvents( void ) {
	int pnum;

	for( pnum = 0; pnum < cg.frame.numEntities; pnum++ ) {
		entity_state_t *state = &cg.frame.entities[pnum];

		for( int j = 0; j < 2; j++ ) {
			CG_EntityEvent( state, state->events[j], state->eventParms[j], false );
		}
	}
}

/*
* CG_FirePlayerStateEvents
* This events are only received by this client, and only affect it.
*/
static void CG_FirePlayerStateEvents( void ) {
	unsigned int event, parm, i, count;
	vec3_t dir;

	if( cg.view.POVent != (int)cg.frame.playerState.POVnum ) {
		return;
	}

	for( count = 0; count < 2; count++ ) {
		// first byte is event number, second is parm
		event = cg.frame.playerState.event[count] & 127;
		parm = cg.frame.playerState.eventParm[count] & 0xFF;

		switch( event ) {
			case PSEV_HIT:
				if( parm > 6 ) {
					break;
				}
				if( parm < 4 ) { // hit of some caliber
					trap_S_StartLocalSound( cgs.media.sfxWeaponHit[parm], CHAN_AUTO, cg_volume_hitsound->value );
					CG_ScreenCrosshairDamageUpdate();
				} else if( parm == 4 ) {  // killed an enemy
					trap_S_StartLocalSound( cgs.media.sfxWeaponKill, CHAN_AUTO, cg_volume_hitsound->value );
					CG_ScreenCrosshairDamageUpdate();
				} else {  // hit a teammate
					trap_S_StartLocalSound( cgs.media.sfxWeaponHitTeam, CHAN_AUTO, cg_volume_hitsound->value );
					if( cg_showhelp->integer ) {
						if( random() <= 0.5f ) {
							CG_CenterPrint( "Don't shoot at members of your team!" );
						} else {
							CG_CenterPrint( "You are shooting at your team-mates!" );
						}
					}
				}
				break;

			case PSEV_PICKUP:
				if( cg_pickup_flash->integer && !cg.view.thirdperson ) {
					CG_StartColorBlendEffect( 1.0f, 1.0f, 1.0f, 0.25f, 150 );
				}

				// auto-switch
				if( cg_weaponAutoSwitch->integer && ( parm > WEAP_NONE && parm < WEAP_TOTAL ) ) {
					if( !cgs.demoPlaying && cg.predictedPlayerState.pmove.pm_type == PM_NORMAL
						&& cg.predictedPlayerState.POVnum == cgs.playerNum + 1 ) {
						// auto-switch only works when the user didn't have the just-picked weapon
						if( !cg.oldFrame.playerState.inventory[parm] ) {
							// switch when player's only weapon is gunblade
							if( cg_weaponAutoSwitch->integer == 2 ) {
								for( i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ ) {
									if( i == parm ) {
										continue;
									}
									if( cg.predictedPlayerState.inventory[i] ) {
										break;
									}
								}

								if( i == WEAP_TOTAL ) { // didn't have any weapon
									CG_UseItem( va( "%i", parm ) );
								}

							}
							// switch when the new weapon improves player's selected weapon
							else if( cg_weaponAutoSwitch->integer == 1 ) {
								unsigned int best = WEAP_GUNBLADE;
								for( i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++ ) {
									if( i == parm ) {
										continue;
									}
									if( cg.predictedPlayerState.inventory[i] ) {
										best = i;
									}
								}

								if( best < parm ) {
									CG_UseItem( va( "%i", parm ) );
								}
							}
						}
					}
				}
				break;

			case PSEV_DAMAGE_20:
				ByteToDir( parm, dir );
				CG_DamageIndicatorAdd( 20, dir );
				break;

			case PSEV_DAMAGE_40:
				ByteToDir( parm, dir );
				CG_DamageIndicatorAdd( 40, dir );
				break;

			case PSEV_DAMAGE_60:
				ByteToDir( parm, dir );
				CG_DamageIndicatorAdd( 60, dir );
				break;

			case PSEV_DAMAGE_80:
				ByteToDir( parm, dir );
				CG_DamageIndicatorAdd( 80, dir );
				break;

			case PSEV_INDEXEDSOUND:
				if( cgs.soundPrecache[parm] ) {
					trap_S_StartGlobalSound( cgs.soundPrecache[parm], CHAN_AUTO, cg_volume_effects->value );
				}
				break;

			case PSEV_ANNOUNCER:
				CG_AddAnnouncerEvent( cgs.soundPrecache[parm], false );
				break;

			case PSEV_ANNOUNCER_QUEUED:
				CG_AddAnnouncerEvent( cgs.soundPrecache[parm], true );
				break;

			default:
				break;
		}
	}
}

/*
* CG_FireEvents
*/
void CG_FireEvents( void ) {
	CG_FireEntityEvents();

	CG_FirePlayerStateEvents();
}
