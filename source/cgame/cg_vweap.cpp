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


// - Adding the View Weapon to the scene


#include "cg_local.h"


/*
* CG_ViewWeapon_UpdateProjectionSource
*/
static void CG_ViewWeapon_UpdateProjectionSource( const vec3_t hand_origin, const mat3_t hand_axis, const vec3_t weap_origin, const mat3_t weap_axis ) {
	orientation_t *tag_result = &cg.weapon.projectionSource;
	orientation_t tag_weapon;
	weaponinfo_t *weaponInfo;

	VectorCopy( vec3_origin, tag_weapon.origin );
	Matrix3_Copy( axis_identity, tag_weapon.axis );

	// move to tag_weapon
	CG_MoveToTag( tag_weapon.origin, tag_weapon.axis,
				  hand_origin, hand_axis,
				  weap_origin, weap_axis );

	weaponInfo = CG_GetWeaponInfo( cg.weapon.weapon );

	// move to projectionSource tag
	if( weaponInfo ) {
		VectorCopy( vec3_origin, tag_result->origin );
		Matrix3_Copy( axis_identity, tag_result->axis );
		CG_MoveToTag( tag_result->origin, tag_result->axis,
					  tag_weapon.origin, tag_weapon.axis,
					  weaponInfo->tag_projectionsource.origin, weaponInfo->tag_projectionsource.axis );
		return;
	}

	// fall back: copy gun origin and move it front by 16 units and 8 up
	VectorCopy( tag_weapon.origin, tag_result->origin );
	Matrix3_Copy( tag_weapon.axis, tag_result->axis );
	VectorMA( tag_result->origin, 16, &tag_result->axis[AXIS_FORWARD], tag_result->origin );
	VectorMA( tag_result->origin, 8, &tag_result->axis[AXIS_UP], tag_result->origin );
}

/*
* CG_ViewWeapon_AddAngleEffects
*/
static void CG_ViewWeapon_AddAngleEffects( vec3_t angles ) {
	int i;
	float delta;

	if( !cg.view.drawWeapon ) {
		return;
	}

	if( cg_gun->integer && cg_gunbob->integer ) {
		// gun angles from bobbing
		if( cg.bobCycle & 1 ) {
			angles[ROLL] -= cg.xyspeed * cg.bobFracSin * 0.012;
			angles[YAW] -= cg.xyspeed * cg.bobFracSin * 0.006;
		} else {
			angles[ROLL] += cg.xyspeed * cg.bobFracSin * 0.012;
			angles[YAW] += cg.xyspeed * cg.bobFracSin * 0.006;
		}
		angles[PITCH] += cg.xyspeed * cg.bobFracSin * 0.012;

		// gun angles from delta movement
		for( i = 0; i < 3; i++ ) {
			delta = ( cg.oldFrame.playerState.viewangles[i] - cg.frame.playerState.viewangles[i] ) * cg.lerpfrac;
			if( delta > 180 ) {
				delta -= 360;
			}
			if( delta < -180 ) {
				delta += 360;
			}
			clamp( delta, -45, 45 );


			if( i == YAW ) {
				angles[ROLL] += 0.001 * delta;
			}
			angles[i] += 0.002 * delta;
		}

		// gun angles from kicks
		CG_AddKickAngles( angles );
	}
}

/*
* CG_ViewWeapon_baseanimFromWeaponState
*/
static int CG_ViewWeapon_baseanimFromWeaponState( int weaponState ) {
	int anim;

	switch( weaponState ) {
		case WEAPON_STATE_ACTIVATING:
			anim = WEAPANIM_WEAPONUP;
			break;

		case WEAPON_STATE_DROPPING:
			anim = WEAPANIM_WEAPDOWN;
			break;

		case WEAPON_STATE_FIRING:
		case WEAPON_STATE_REFIRE:
		case WEAPON_STATE_REFIRESTRONG:

		/* fall through. Activated by event */
		case WEAPON_STATE_POWERING:
		case WEAPON_STATE_COOLDOWN:
		case WEAPON_STATE_RELOADING:
		case WEAPON_STATE_NOAMMOCLICK:

		/* fall through. Not used */
		default:
		case WEAPON_STATE_READY:
			if( cg_gunbob->integer ) {
				anim = WEAPANIM_STANDBY;
			} else {
				anim = WEAPANIM_NOANIM;
			}
			break;
	}

	return anim;
}

/*
* CG_ViewWeapon_RefreshAnimation
*/
void CG_ViewWeapon_RefreshAnimation( cg_viewweapon_t *viewweapon ) {
	int baseAnim;
	weaponinfo_t *weaponInfo;
	int curframe = 0;
	float framefrac;
	bool nolerp = false;

	// if the pov changed, or weapon changed, force restart
	if( viewweapon->POVnum != cg.predictedPlayerState.POVnum ||
		viewweapon->weapon != cg.predictedPlayerState.stats[STAT_WEAPON] ) {
		nolerp = true;
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
		viewweapon->baseAnim = 0;
		viewweapon->baseAnimStartTime = 0;
	}

	viewweapon->POVnum = cg.predictedPlayerState.POVnum;
	viewweapon->weapon = cg.predictedPlayerState.stats[STAT_WEAPON];

	// hack cause of missing animation config
	if( viewweapon->weapon == WEAP_NONE ) {
		viewweapon->ent.frame = viewweapon->ent.oldframe = 0;
		viewweapon->ent.backlerp = 0.0f;
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
		return;
	}

	baseAnim = CG_ViewWeapon_baseanimFromWeaponState( cg.predictedPlayerState.weaponState );
	weaponInfo = CG_GetWeaponInfo( viewweapon->weapon );

	// Full restart
	if( !viewweapon->baseAnimStartTime ) {
		viewweapon->baseAnim = baseAnim;
		viewweapon->baseAnimStartTime = cg.time;
		nolerp = true;
	}

	// base animation changed?
	if( baseAnim != viewweapon->baseAnim ) {
		viewweapon->baseAnim = baseAnim;
		viewweapon->baseAnimStartTime = cg.time;
	}

	// if a eventual animation is running override the baseAnim
	if( viewweapon->eventAnim ) {
		if( !viewweapon->eventAnimStartTime ) {
			viewweapon->eventAnimStartTime = cg.time;
		}

		framefrac = GS_FrameForTime( &curframe, cg.time, viewweapon->eventAnimStartTime, weaponInfo->frametime[viewweapon->eventAnim],
									 weaponInfo->firstframe[viewweapon->eventAnim], weaponInfo->lastframe[viewweapon->eventAnim],
									 weaponInfo->loopingframes[viewweapon->eventAnim], false );

		if( curframe >= 0 ) {
			goto setupframe;
		}

		// disable event anim and fall through
		viewweapon->eventAnim = 0;
		viewweapon->eventAnimStartTime = 0;
	}

	// find new frame for the current animation
	framefrac = GS_FrameForTime( &curframe, cg.time, viewweapon->baseAnimStartTime, weaponInfo->frametime[viewweapon->baseAnim],
								 weaponInfo->firstframe[viewweapon->baseAnim], weaponInfo->lastframe[viewweapon->baseAnim],
								 weaponInfo->loopingframes[viewweapon->baseAnim], true );

	if( curframe < 0 ) {
		CG_Error( "CG_ViewWeapon_UpdateAnimation(2): Base Animation without a defined loop.\n" );
	}

setupframe:
	if( nolerp ) {
		framefrac = 0;
		viewweapon->ent.oldframe = curframe;
	} else {
		clamp( framefrac, 0, 1 );
		if( curframe != viewweapon->ent.frame ) {
			viewweapon->ent.oldframe = viewweapon->ent.frame;
		}
	}

	viewweapon->ent.frame = curframe;
	viewweapon->ent.backlerp = 1.0f - framefrac;
}

/*
* CG_ViewWeapon_StartAnimationEvent
*/
void CG_ViewWeapon_StartAnimationEvent( int newAnim ) {
	if( !cg.view.drawWeapon ) {
		return;
	}

	cg.weapon.eventAnim = newAnim;
	cg.weapon.eventAnimStartTime = cg.time;
	CG_ViewWeapon_RefreshAnimation( &cg.weapon );
}

/*
* CG_CalcViewWeapon
*/
void CG_CalcViewWeapon( cg_viewweapon_t *viewweapon ) {
	orientation_t tag;
	weaponinfo_t *weaponInfo;
	vec3_t gunAngles;
	vec3_t gunOffset;
	float handOffset;
	cgs_skeleton_t *skel;

	CG_ViewWeapon_RefreshAnimation( viewweapon );

	weaponInfo = CG_GetWeaponInfo( viewweapon->weapon );
	viewweapon->ent.model = weaponInfo->model[WEAPMODEL_HAND];
	viewweapon->ent.renderfx = RF_MINLIGHT | RF_WEAPONMODEL | RF_FORCENOLOD | RF_NOSHADOW;
	viewweapon->ent.scale = 1.0f;
	viewweapon->ent.customShader = NULL;
	viewweapon->ent.customSkin = NULL;
	viewweapon->ent.rtype = RT_MODEL;
	viewweapon->ent.boneposes = viewweapon->ent.oldboneposes = NULL;
	Vector4Set( viewweapon->ent.shaderRGBA, 255, 255, 255, 255 );

	// calculate the entity position
	VectorCopy( cg.view.origin, viewweapon->ent.origin );

	skel = weaponInfo->skel[WEAPMODEL_HAND];
	if( skel ) {
		// get space in cache, interpolate, transform, link
		viewweapon->ent.boneposes = viewweapon->ent.oldboneposes = CG_RegisterTemporaryExternalBoneposes( skel );
		CG_LerpSkeletonPoses( skel, viewweapon->ent.frame, viewweapon->ent.oldframe, viewweapon->ent.boneposes, 1.0 - viewweapon->ent.backlerp );
		CG_TransformBoneposes( skel, viewweapon->ent.boneposes, viewweapon->ent.boneposes );
	}

	// weapon config offsets
	VectorAdd( weaponInfo->handpositionAngles, cg.predictedPlayerState.viewangles, gunAngles );
	gunOffset[FORWARD] = cg_gunz->value + weaponInfo->handpositionOrigin[FORWARD];
	gunOffset[RIGHT] = cg_gunx->value + weaponInfo->handpositionOrigin[RIGHT];
	gunOffset[UP] = cg_guny->value + weaponInfo->handpositionOrigin[UP];

	// scale forward gun offset depending on fov and aspect ratio
	gunOffset[FORWARD] = gunOffset[FORWARD] * cgs.vidWidth / ( cgs.vidHeight * cg.view.fracDistFOV ) ;

	// hand cvar offset
	handOffset = 0.0f;
	if( cgs.demoPlaying ) {
		if( cg_hand->integer == 0 ) {
			handOffset = cg_handOffset->value;
		} else if( cg_hand->integer == 1 ) {
			handOffset = -cg_handOffset->value;
		}
	} else {
		if( cgs.clientInfo[cg.view.POVent - 1].hand == 0 ) {
			handOffset = cg_handOffset->value;
		} else if( cgs.clientInfo[cg.view.POVent - 1].hand == 1 ) {
			handOffset = -cg_handOffset->value;
		}
	}

	gunOffset[RIGHT] += handOffset;
	if( cg_gun->integer && cg_gunbob->integer ) {
		gunOffset[UP] += CG_ViewSmoothFallKick();
	}

	// apply the offsets
	VectorMA( viewweapon->ent.origin, gunOffset[FORWARD], &cg.view.axis[AXIS_FORWARD], viewweapon->ent.origin );
	VectorMA( viewweapon->ent.origin, gunOffset[RIGHT], &cg.view.axis[AXIS_RIGHT], viewweapon->ent.origin );
	VectorMA( viewweapon->ent.origin, gunOffset[UP], &cg.view.axis[AXIS_UP], viewweapon->ent.origin );

	// add angles effects
	CG_ViewWeapon_AddAngleEffects( gunAngles );

	// finish
	AnglesToAxis( gunAngles, viewweapon->ent.axis );

	if( cg_gun_fov->integer && !cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] ) {
		float fracWeapFOV;
		float gun_fov_y = WidescreenFov( bound( 20, cg_gun_fov->value, 160 ) );
		float gun_fov_x = CalcHorizontalFov( gun_fov_y, scr_vrect.width, scr_vrect.height );

		fracWeapFOV = tan( DEG2RAD( gun_fov_x ) * 0.5f ) / cg.view.fracDistFOV;

		VectorScale( &viewweapon->ent.axis[AXIS_FORWARD], fracWeapFOV, &viewweapon->ent.axis[AXIS_FORWARD] );
	}

	// if the player doesn't want to view the weapon we still have to build the projection source
	if( CG_GrabTag( &tag, &viewweapon->ent, "tag_weapon" ) ) {
		CG_ViewWeapon_UpdateProjectionSource( viewweapon->ent.origin, viewweapon->ent.axis, tag.origin, tag.axis );
	} else {
		CG_ViewWeapon_UpdateProjectionSource( viewweapon->ent.origin, viewweapon->ent.axis, vec3_origin, axis_identity );
	}
}

/*
* CG_AddViewWeapon
*/
void CG_AddViewWeapon( cg_viewweapon_t *viewweapon ) {
	orientation_t tag;
	int64_t flash_time = 0;

	if( !cg.view.drawWeapon || viewweapon->weapon == WEAP_NONE ) {
		return;
	}

	// update the other origins
	VectorCopy( viewweapon->ent.origin, viewweapon->ent.origin2 );
	VectorCopy( cg_entities[viewweapon->POVnum].ent.lightingOrigin, viewweapon->ent.lightingOrigin );

	CG_AddColoredOutLineEffect( &viewweapon->ent, cg.effects, 0, 0, 0, viewweapon->ent.shaderRGBA[3] );
	CG_AddEntityToScene( &viewweapon->ent );
	CG_AddShellEffects( &viewweapon->ent, cg.effects );

	if( cg_weaponFlashes->integer == 2 ) {
		flash_time = cg_entPModels[viewweapon->POVnum].flash_time;
	}

	// add attached weapon
	if( CG_GrabTag( &tag, &viewweapon->ent, "tag_weapon" ) ) {
		CG_AddWeaponOnTag( &viewweapon->ent, &tag, viewweapon->weapon, cg.effects | EF_OUTLINE, NULL, flash_time, 
			cg_entPModels[viewweapon->POVnum].barrel_time );
	}
}
