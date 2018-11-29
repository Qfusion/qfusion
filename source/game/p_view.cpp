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

#include "g_local.h"

float xyspeed;

//====================================================================
// DEAD VIEW
//====================================================================

/*
* G_ProjectThirdPersonView
*/
static void G_ProjectThirdPersonView( vec3_t vieworg, vec3_t viewangles, edict_t *passent ) {
	float thirdPersonRange = 60;
	float thirdPersonAngle = 0;
	float dist, f, r;
	vec3_t dest, stop;
	vec3_t chase_dest;
	trace_t trace;
	vec3_t mins = { -4, -4, -4 };
	vec3_t maxs = { 4, 4, 4 };
	vec3_t v_forward, v_right, v_up;

	AngleVectors( viewangles, v_forward, v_right, v_up );

	// calc exact destination
	VectorCopy( vieworg, chase_dest );
	r = DEG2RAD( thirdPersonAngle );
	f = -cos( r );
	r = -sin( r );
	VectorMA( chase_dest, thirdPersonRange * f, v_forward, chase_dest );
	VectorMA( chase_dest, thirdPersonRange * r, v_right, chase_dest );
	chase_dest[2] += 8;

	// find the spot the player is looking at
	VectorMA( vieworg, 512, v_forward, dest );
	G_Trace( &trace, vieworg, mins, maxs, dest, passent, MASK_SOLID );

	// calculate pitch to look at the same spot from camera
	VectorSubtract( trace.endpos, vieworg, stop );
	dist = sqrt( stop[0] * stop[0] + stop[1] * stop[1] );
	if( dist < 1 ) {
		dist = 1;
	}
	viewangles[PITCH] = RAD2DEG( -atan2( stop[2], dist ) );
	viewangles[YAW] -= thirdPersonAngle;
	AngleVectors( viewangles, v_forward, v_right, v_up );

	// move towards destination
	G_Trace( &trace, vieworg, mins, maxs, chase_dest, passent, MASK_SOLID );

	if( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, stop );
		stop[2] += ( 1.0 - trace.fraction ) * 32;
		G_Trace( &trace, vieworg, mins, maxs, stop, passent, MASK_SOLID );
		VectorCopy( trace.endpos, chase_dest );
	}

	VectorCopy( chase_dest, vieworg );
}

/*
* G_Client_DeadView
*/
static void G_Client_DeadView( edict_t *ent ) {
	edict_t *body;
	gclient_t *client;
	trace_t trace;

	client = ent->r.client;

	// find the body
	for( body = game.edicts + gs.maxclients; ENTNUM( body ) < gs.maxclients + BODY_QUEUE_SIZE + 1; body++ ) {
		if( !body->r.inuse || body->r.svflags & SVF_NOCLIENT ) {
			continue;
		}
		if( body->activator == ent ) { // this is our body
			break;
		}
	}

	if( body->activator != ent ) { // ran all the list and didn't find our body
		return;
	}

	// move us to body position
	VectorCopy( body->s.origin, ent->s.origin );
	ent->s.teleported = true;
	client->ps.viewangles[ROLL] = 0;
	client->ps.viewangles[PITCH] = 0;

	// see if our killer is still in view
	if( body->enemy && ( body->enemy != ent ) ) {
		G_Trace( &trace, ent->s.origin, vec3_origin, vec3_origin, body->enemy->s.origin, body, MASK_OPAQUE );
		if( trace.fraction != 1.0f ) {
			body->enemy = NULL;
		} else {
			client->ps.viewangles[YAW] = LookAtKillerYAW( ent, NULL, body->enemy );
		}
	} else {   // nobody killed us, so just circle around the body ?

	}

	G_ProjectThirdPersonView( ent->s.origin, client->ps.viewangles, body );
	VectorCopy( client->ps.viewangles, ent->s.angles );
	VectorCopy( ent->s.origin, client->ps.pmove.origin );
	VectorClear( client->ps.pmove.velocity );
}

//====================================================================
// EFFECTS
//====================================================================

/*
* G_ClientAddDamageIndicatorImpact
*/
void G_ClientAddDamageIndicatorImpact( gclient_t *client, int damage, const vec3_t basedir ) {
	vec3_t dir;
	float frac;

	if( damage < 1 ) {
		return;
	}

	if( !client || client - game.clients < 0 || client - game.clients >= gs.maxclients ) {
		return;
	}

	if( !basedir ) {
		VectorCopy( vec3_origin, dir );
	} else {
		VectorNormalize2( basedir, dir );

		//#define ACCENT_SCALE 2.0f
#ifdef ACCENT_SCALE

		// accent the vertical or horizontal aspect of the direction
		if( VectorLengthFast( tv( dir[0], dir[1], 0 ) ) > dir[2] ) {
			dir[0] *= ACCENT_SCALE;
			dir[1] *= ACCENT_SCALE;
		} else {
			dir[2] *= ACCENT_SCALE;
		}

		VectorNormalizeFast( dir );
#endif
#undef ACCENT_SCALE
	}

	frac = (float)damage / ( damage + client->resp.snap.damageTaken );
	VectorLerp( client->resp.snap.damageTakenDir, frac, dir, client->resp.snap.damageTakenDir );
	client->resp.snap.damageTaken += damage;
}

/*
* G_ClientDamageFeedback
*
* Adds color blends, hitsounds, etc
*/
void G_ClientDamageFeedback( edict_t *ent ) {
	if( ent->r.client->resp.snap.damageTaken ) {
		int damage = ent->r.client->resp.snap.damageTaken;
		int byteDir = DirToByte( ent->r.client->resp.snap.damageTakenDir );

		if( damage <= 20 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_20, byteDir );
		} else if( damage <= 40 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_40, byteDir );
		} else if( damage <= 60 ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_60, byteDir );
		} else {
			G_AddPlayerStateEvent( ent->r.client, PSEV_DAMAGE_80, byteDir );
		}
	}

	// add hitsounds from given damage
	if( ent->snap.damage_given || ent->snap.damageteam_given || ent->snap.kill || ent->snap.teamkill ) {
		// we can't make team damage hit sound at the same time as we do damage hit sound
		// let's determine what's more relevant
		if( ent->snap.teamkill || ent->snap.damageteam_given > 50 ||
			( ent->snap.damageteam_given > 2 * ent->snap.damage_given && !ent->snap.kill ) ) {
			G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 5 );
		} else {
			if( ent->snap.kill ) {
				G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 4 );
			} else if( ent->snap.damage_given >= 75 ) {
				G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 0 );
			} else if( ent->snap.damage_given >= 45 ) {
				G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 1 );
			} else if( ent->snap.damage_given >= 20 ) {
				G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 2 );
			} else {
				G_AddPlayerStateEvent( ent->r.client, PSEV_HIT, 3 );
			}
		}
	}
}

/*
* G_PlayerWorldEffects
*/
static void G_PlayerWorldEffects( edict_t *ent ) {
	int waterlevel, old_waterlevel;
	int watertype, old_watertype;

	if( ent->movetype == MOVETYPE_NOCLIP ) {
		ent->air_finished = level.time + ( 12 * 1000 ); // don't need air
		return;
	}

	waterlevel = ent->waterlevel;
	watertype = ent->watertype;
	old_waterlevel = ent->r.client->resp.old_waterlevel;
	old_watertype = ent->r.client->resp.old_watertype;
	ent->r.client->resp.old_waterlevel = waterlevel;
	ent->r.client->resp.old_watertype = watertype;

	//
	// if just entered a water volume, play a sound
	//
	if( !old_waterlevel && waterlevel ) {
		if( ent->watertype & CONTENTS_LAVA ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_LAVA_IN ), ATTN_NORM );
		} else if( ent->watertype & CONTENTS_SLIME ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_SLIME_IN ), ATTN_NORM );
		} else if( ent->watertype & CONTENTS_WATER ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_WATER_IN ), ATTN_NORM );
		}

		ent->flags |= FL_INWATER;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if( old_waterlevel && !waterlevel ) {
		if( old_watertype & CONTENTS_LAVA ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_LAVA_OUT ), ATTN_NORM );
		} else if( old_watertype & CONTENTS_SLIME ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_SLIME_OUT ), ATTN_NORM );
		} else if( old_watertype & CONTENTS_WATER ) {
			G_Sound( ent, CHAN_AUTO, trap_SoundIndex( S_WORLD_WATER_OUT ), ATTN_NORM );
		}

		ent->flags &= ~FL_INWATER;
	}

	//
	// check for head just coming out of water
	//
	if( old_waterlevel == 3 && waterlevel != 3 ) {
		if( ent->air_finished < level.time ) { // gasp for air
			                                   // wsw : jal : todo : better variations of gasp sounds
			G_AddEvent( ent, EV_SEXEDSOUND, 1, true );
		} else if( ent->air_finished < level.time + 11000 ) {   // just break surface
			                                                    // wsw : jal : todo : better variations of gasp sounds
			G_AddEvent( ent, EV_SEXEDSOUND, 2, true );
		}
	}

	//
	// check for drowning
	//
	if( waterlevel == 3 ) {
		// if out of air, start drowning
		if( ent->air_finished < level.time ) { // drown!
			if( ent->r.client->resp.next_drown_time < level.time && !G_IsDead( ent ) ) {
				ent->r.client->resp.next_drown_time = level.time + 1000;

				// take more damage the longer underwater
				ent->r.client->resp.drowningDamage += 2;
				if( ent->r.client->resp.drowningDamage > 15 ) {
					ent->r.client->resp.drowningDamage = 15;
				}

				// wsw : jal : todo : better variations of gasp sounds
				// play a gurp sound instead of a normal pain sound
				if( HEALTH_TO_INT( ent->health ) - ent->r.client->resp.drowningDamage <= 0 ) {
					G_AddEvent( ent, EV_SEXEDSOUND, 2, true );
				} else {
					G_AddEvent( ent, EV_SEXEDSOUND, 1, true );
				}
				ent->pain_debounce_time = level.time;

				G_Damage( ent, world, world, vec3_origin, vec3_origin, ent->s.origin, ent->r.client->resp.drowningDamage, 0, 0, MOD_WATER );
			}
		}
	} else {
		ent->air_finished = level.time + 12000;
		ent->r.client->resp.drowningDamage = 2;
	}

	//
	// check for sizzle damage
	//
	if( waterlevel && ( ent->watertype & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) ) {
		if( ent->watertype & CONTENTS_LAVA ) {
			// wsw: Medar: We don't have the sounds yet and this seems to overwrite the normal pain sounds
			//if( !G_IsDead(ent) && ent->pain_debounce_time <= level.time )
			//{
			//	G_Sound( ent, CHAN_BODY, trap_SoundIndex(va(S_PLAYER_BURN_1_to_2, (rand()&1)+1)), 1, ATTN_NORM );
			//	ent->pain_debounce_time = level.time + 1000;
			//}
			G_Damage( ent, world, world, vec3_origin, vec3_origin, ent->s.origin,
					  ( 30 * waterlevel ) * game.snapFrameTime / 1000.0f, 0, 0, MOD_LAVA );
		}

		if( ent->watertype & CONTENTS_SLIME ) {
			G_Damage( ent, world, world, vec3_origin, vec3_origin, ent->s.origin,
					  ( 10 * waterlevel ) * game.snapFrameTime / 1000.0f, 0, 0, MOD_SLIME );
		}
	}
}

/*
* G_SetClientEffects
*/
static void G_SetClientEffects( edict_t *ent ) {
	gclient_t *client = ent->r.client;

	if( G_IsDead( ent ) || GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		return;
	}

	if( client->ps.inventory[POWERUP_QUAD] > 0 ) {
		ent->s.effects |= EF_QUAD;
		if( client->ps.inventory[POWERUP_QUAD] < 6 ) {
			ent->s.effects |= EF_EXPIRING_QUAD;
		}
	}

	if( client->ps.inventory[POWERUP_SHELL] > 0 ) {
		ent->s.effects |= EF_SHELL;
		if( client->ps.inventory[POWERUP_SHELL] < 6 ) {
			ent->s.effects |= EF_EXPIRING_SHELL;
		}
	}

	if( client->ps.inventory[POWERUP_REGEN] > 0 ) {
		ent->s.effects |= EF_REGEN;
		if( client->ps.inventory[POWERUP_REGEN] < 6 ) {
			ent->s.effects |= EF_EXPIRING_REGEN;
		}
	}

	if( ent->s.weapon ) {
		firedef_t *firedef = GS_FiredefForPlayerState( &client->ps, ent->s.weapon );
		if( firedef && firedef->fire_mode == FIRE_MODE_STRONG ) {
			ent->s.effects |= EF_STRONG_WEAPON;
		}
	}

	// show cheaters!!!
	if( ent->flags & FL_GODMODE ) {
		ent->s.effects |= EF_GODMODE;
	}

	// add chatting icon effect
	if( ent->r.client->resp.snap.buttons & BUTTON_BUSYICON ) {
		ent->s.effects |= EF_BUSYICON;
	}
}

/*
* G_SetClientSound
*/
static void G_SetClientSound( edict_t *ent ) {
	if( ent->waterlevel == 3 ) {
		if( ent->watertype & CONTENTS_LAVA ) {
			ent->s.sound = trap_SoundIndex( S_WORLD_UNDERLAVA );
		} else if( ent->watertype & CONTENTS_SLIME ) {
			ent->s.sound = trap_SoundIndex( S_WORLD_UNDERSLIME );
		} else if( ent->watertype & CONTENTS_WATER ) {
			ent->s.sound = trap_SoundIndex( S_WORLD_UNDERWATER );
		}
	} else {
		ent->s.sound = 0;
	}
}

/*
* G_SetClientFrame
*/
void G_SetClientFrame( edict_t *ent ) {
	if( ent->s.type != ET_PLAYER ) {
		return;
	}

	ent->s.frame = 0;
}

/*
* G_ClientEndSnapFrame
*
* Called for each player at the end of the server frame
* and right after spawning
*/
void G_ClientEndSnapFrame( edict_t *ent ) {
	gclient_t *client;
	int i;

	if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED ) {
		return;
	}

	client = ent->r.client;

	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	if( GS_MatchState() >= MATCH_STATE_POSTMATCH ) {
		G_SetClientStats( ent );
	} else {
		if( G_IsDead( ent ) && !level.gametype.customDeadBodyCam ) {
			G_Client_DeadView( ent );
		}

		G_PlayerWorldEffects( ent ); // burn from lava, etc
		G_ClientDamageFeedback( ent ); // show damage taken along the snap
		G_SetClientStats( ent );
		G_SetClientEffects( ent );
		G_SetClientSound( ent );
		G_SetClientFrame( ent );

		client->ps.plrkeys = client->resp.snap.plrkeys;
	}

	G_ReleaseClientPSEvent( client );

	// set the delta angle
	for( i = 0; i < 3; i++ )
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT( client->ps.viewangles[i] ) - client->ucmd.angles[i];

	// this is pretty hackish
	if( !G_ISGHOSTING( ent ) ) {
		VectorCopy( ent->velocity, ent->s.origin2 );
	}
}
