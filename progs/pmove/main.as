/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2007 Chasseur de Bots
Copyright (C) 2019 Victor Luchits

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

namespace PM {

Vec3 playerboxStandMins, playerboxStandMaxs;
float playerboxStandViewheight;

Vec3 playerboxCrouchMins, playerboxCrouchMaxs;
float playerboxCrouchViewheight;

Vec3 playerboxGibMins, playerboxGibMaxs;
float playerboxGibViewheight;

void Load() {
	GS::GetPlayerStandSize( playerboxStandMins, playerboxStandMaxs );
	playerboxStandViewheight = GS::GetPlayerStandViewHeight();

	GS::GetPlayerCrouchSize( playerboxCrouchMins, playerboxCrouchMaxs );
	playerboxCrouchViewheight = GS::GetPlayerCrouchHeight();

	GS::GetPlayerGibSize( playerboxGibMins, playerboxGibMaxs );
	playerboxGibViewheight = GS::GetPlayerGibHeight();
}

void PMove( PMove @pm, PlayerState @ps, UserCmd cmd ) {
	PMoveLocal pml;
	auto @pmoveState = @ps.pmove;
	int pm_type = pmoveState.pm_type;

	if( @pm == null ) {
		//GS::Print( "PM::PMove: @pm == null\n" );
		return;
	}
	if( @ps == null ) {
		//GS::Print( "PM::PMove: @ps == null\n" );
		return;
	}

	pml.BeginMove( pm, ps, cmd );

	float fallvelocity = ( ( pml.velocity.z < 0.0f ) ? abs( pml.velocity.z ) : 0.0f );

	pml.PostBeginMove();

	if( pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED ) == 0 ) {
			pml.ClearDash();

			pml.ClearWallJump();

			pml.ClearStun();

			pmoveState.stats[PM_STAT_KNOCKBACK] = 0;
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			pmoveState.stats[PM_STAT_ZOOMTIME] = 0;
			pmoveState.pm_flags = pmoveState.pm_flags & int( ~( PMF_JUMPPAD_TIME | PMF_DOUBLEJUMPED | PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_SPECIAL_HELD ) );

			pml.AdjustBBox();
		}

		pml.AdjustViewheight();

		if( pm_type == PM_SPECTATOR ) {
			pml.FlyMove( false );
		} else {
			pml.forwardPush = pml.sidePush = pml.upPush = 0.0f;
		}

		pml.EndMove();
		return;
	}

	// set mins, maxs, viewheight and fov

	pml.AdjustBBox();

	pml.CheckZoom();

	pml.AdjustViewheight();

	pml.CategorizePosition();

	int oldGroundEntity = pm.groundEntity;

	pml.CheckSpecialMovement();

	if( ( pmoveState.pm_flags & PMF_TIME_TELEPORT ) != 0 ) {
		// teleport pause stays exactly in place
	} else if( ( pmoveState.pm_flags & PMF_TIME_WATERJUMP ) != 0 ) {
		// waterjump has no control, but falls

		pml.velocity.z -= pmoveState.gravity * pml.frametime;
		if( pml.velocity.z < 0 ) {
			// cancel as soon as we are falling down again
			pmoveState.pm_flags = pmoveState.pm_flags & int( ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT ) );
			pmoveState.pm_time = 0;
		}

		pml.StepSlideMove();
	} else {
		// Kurim
		// Keep this order !
		pml.CheckJump();

		pml.CheckDash();

		pml.CheckWallJump();

		pml.CheckCrouchSlide();

		pml.Friction();

		if( pm.waterLevel >= 2 ) {
			pml.WaterMove();
		} else {
			Vec3 angles = ps.viewAngles;

			if( angles[PITCH] > 180.0f ) {
				angles[PITCH] = angles[PITCH] - 360.0f;
			}
			angles[PITCH] /= 3.0f;

			angles.angleVectors( pml.forward, pml.right, pml.up );

			// hack to work when looking straight up and straight down
			if( pml.forward.z == -1.0f ) {
				pml.flatforward = pml.up;
			} else if( pml.forward.z == 1.0f ) {
				pml.flatforward = pml.up * -1.0f;
			} else {
				pml.flatforward = pml.forward;
			}
			pml.flatforward.z = 0.0f;
			pml.flatforward.normalize();

			pml.Move();
		}
	}
	
	// set groundentity, watertype, and waterlevel for final spot
	pml.CategorizePosition();

	pml.EndMove();

	// falling event

	// Execute the triggers that are touched.
	// We check the entire path between the origin before the pmove and the
	// current origin to ensure no triggers are missed at high velocity.
	// Note that this method assumes the movement has been linear.
	pm.touchTriggers( ps, pml.previousOrigin );

	// touching triggers may force groundentity off
	if( ( ( pmoveState.pm_flags & PMF_ON_GROUND ) == 0 ) && ( pm.groundEntity != -1 ) ) {
		pm.groundEntity = -1;
		pml.velocity.z = 0;
	}

	if( pm.groundEntity != -1 ) { // remove wall-jump and dash bits when touching ground
		// always keep the dash flag 50 msecs at least (to prevent being removed at the start of the dash)
		if( pmoveState.stats[PM_STAT_DASHTIME] < ( PM_DASHJUMP_TIMEDELAY - 50 ) ) {
			pmoveState.pm_flags = pmoveState.pm_flags & ~PMF_DASHING;
		}

		if( pmoveState.stats[PM_STAT_WJTIME] < ( PM_WALLJUMP_TIMEDELAY - 50 ) ) {
			pml.ClearWallJump();
		}
	}

	if( oldGroundEntity == -1 ) {
		float damage;
		float falldelta = fallvelocity - ( ( pml.velocity.z < 0.0f ) ? abs( pml.velocity.z ) : 0.0f );

		// scale delta if in water
		if( pm.waterLevel == 3 ) {
			falldelta = 0;
		}
		if( pm.waterLevel == 2 ) {
			falldelta *= 0.25;
		}
		if( pm.waterLevel == 1 ) {
			falldelta *= 0.5;
		}

		if( falldelta > FALL_STEP_MIN_DELTA ) {
			if( ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_FALLDAMAGE ) == 0 || 
				( pml.groundSurfFlags & SURF_NODAMAGE ) != 0 || 
				( pmoveState.pm_flags & PMF_JUMPPAD_TIME ) != 0 ) {
				damage = 0.0f;
			} else {
				damage = ( ( falldelta - FALL_DAMAGE_MIN_DELTA ) / 10 ) * FALL_DAMAGE_SCALE;
			}

			int damageParam = int( damage );
			if( damage < 0 ) {
				damage = 0;
			} else if( damage > MAX_FALLING_DAMAGE ) {
				damage = MAX_FALLING_DAMAGE;
			}

			GS::PredictedEvent( ps.POVnum, EV_FALL, damageParam );
		}

		pmoveState.pm_flags = pmoveState.pm_flags & int( ~PMF_JUMPPAD_TIME );
	}
}

}