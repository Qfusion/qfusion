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

const float DEFAULT_WALKSPEED = 160.0f;
const float DEFAULT_CROUCHEDSPEED = 100.0f;
const float DEFAULT_LADDERSPEED = 250.0f;

const float SPEEDKEY = 500.0f;

const int CROUCHTIME = 100;

const int PM_DASHJUMP_TIMEDELAY = 1000; // delay in milliseconds
const int PM_WALLJUMP_TIMEDELAY = 1300;
const int PM_WALLJUMP_FAILED_TIMEDELAY = 700;
const int PM_SPECIAL_CROUCH_INHIBIT = 400;
const int PM_AIRCONTROL_BOUNCE_DELAY = 200;
const int PM_CROUCHSLIDE_TIMEDELAY = 700;
const int PM_CROUCHSLIDE_CONTROL = 3;
const int PM_FORWARD_ACCEL_TIMEDELAY = 0; // delay before the forward acceleration kicks in
const int PM_SKIM_TIME = 230;
const float PM_OVERBOUNCE = 1.01f;

const float FALL_DAMAGE_MIN_DELTA = 675.0f;
const float FALL_STEP_MIN_DELTA = 400.0f;
const float MAX_FALLING_DAMAGE = 15;
const float FALL_DAMAGE_SCALE = 1.0;

const float pm_friction = 8; // ( initially 6 )
const float pm_waterfriction = 1;
const float pm_wateraccelerate = 10; // user intended acceleration when swimming ( initially 6 )

const float pm_accelerate = 12; // user intended acceleration when on ground or fly movement ( initially 10 )
const float pm_decelerate = 12; // user intended deceleration when on ground

const float pm_airaccelerate = 1; // user intended aceleration when on air
const float pm_airdecelerate = 2.0f; // air deceleration (not +strafe one, just at normal moving).

// special movement parameters

const float pm_aircontrol = 150.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
const float pm_strafebunnyaccel = 70; // forward acceleration when strafe bunny hopping
const float pm_wishspeed = 30;

const float pm_dashupspeed = ( 174.0f * GRAVITY_COMPENSATE );

Vec3 playerboxStandMins, playerboxStandMaxs;
float playerboxStandViewheight;

Vec3 playerboxCrouchMins, playerboxCrouchMaxs;
float playerboxCrouchViewheight;

Vec3 playerboxGibMins, playerboxGibMaxs;
float playerboxGibViewheight;

class PMoveLocal {
	Vec3 origin;          // full float precision
	Vec3 velocity;        // full float precision

	Vec3 mins, maxs;

	Vec3 forward, right, up;
	Vec3 flatforward;     // normalized forward without z component, saved here because it needs
	// special handling for looking straight up or down

	float frametime;

	int groundSurfFlags;
	Vec3 groundPlaneNormal;
	float groundPlaneDist;
	int groundContents;

	Vec3 previousOrigin;
	bool ladder;

	float forwardPush, sidePush, upPush;

	float maxPlayerSpeed;
	float maxWalkSpeed;
	float maxCrouchedSpeed;
	float jumpPlayerSpeed;
	float dashPlayerSpeed;


	void BeginMove( PMove @pm ) {
		auto @pml = @this;
		auto @playerState = @pm.playerState;
		auto @pmoveState = @playerState.pmove;

		// clear results
		pm.numTouchEnts = 0;
		pm.groundEntity = -1;
		pm.waterType = 0;
		pm.waterLevel = 0;
		pm.step = 0.0f;

		pml.origin = pmoveState.origin;
		pml.velocity = pmoveState.velocity;

		// save old org in case we get stuck
		pml.previousOrigin = pml.origin;

		playerState.viewAngles.angleVectors( pml.forward, pml.right, pml.up );

		pml.flatforward = pml.forward;
		pml.flatforward.z = 0.0f;
		pml.flatforward.normalize();

		pml.frametime = pm.cmd.msec * 0.001;

		pml.maxPlayerSpeed = pmoveState.stats[PM_STAT_MAXSPEED];
		pml.jumpPlayerSpeed = float( pmoveState.stats[PM_STAT_JUMPSPEED] ) * GRAVITY_COMPENSATE;
		pml.dashPlayerSpeed = pmoveState.stats[PM_STAT_DASHSPEED];

		pml.maxWalkSpeed = DEFAULT_WALKSPEED;
		if( pml.maxWalkSpeed > pml.maxPlayerSpeed * 0.66f ) {
			pml.maxWalkSpeed = pml.maxPlayerSpeed * 0.66f;
		}

		pml.maxCrouchedSpeed = DEFAULT_CROUCHEDSPEED;
		if( pml.maxCrouchedSpeed > pml.maxPlayerSpeed * 0.5f ) {
			pml.maxCrouchedSpeed = pml.maxPlayerSpeed * 0.5f;
		}

		pm.getSize( pml.mins, pml.maxs );

		pml.forwardPush = float( pm.cmd.forwardmove ) * SPEEDKEY / 127.0f;
		pml.sidePush = float( pm.cmd.sidemove ) * SPEEDKEY / 127.0f;
		pml.upPush = float( pm.cmd.upmove ) * SPEEDKEY / 127.0f;
	}

	void ClearDash( PMove @pm ) {
		auto @pmoveState = @pm.playerState.pmove;
		pmoveState.pm_flags = pmoveState.pm_flags & int( ~PMF_DASHING );
		pmoveState.stats[PM_STAT_DASHTIME] = 0;
	}

	void ClearWallJump( PMove @pm ) {
		auto @pmoveState = @pm.playerState.pmove;
		pmoveState.pm_flags = pmoveState.pm_flags & int( ~(PMF_WALLJUMPING|PMF_WALLJUMPCOUNT) );
		pmoveState.stats[PM_STAT_WJTIME] = 0;
	}

	void ClearStun( PMove @pm ) {
		auto @pmoveState = @pm.playerState.pmove;
		pmoveState.stats[PM_STAT_STUN] = 0;
	}

	void DecPMoveStat( PMove @pm, int stat ) {
		auto @pmoveState = @pm.playerState.pmove;

		int value = pmoveState.stats[stat];
		if( value > 0 ) {
			value -= pm.cmd.msec;
		} else if( value < 0 ) {
			value = 0;
		}

		pmoveState.stats[stat] = value;
	}

	void PostBeginMove( PMove @pm ) {
		auto @pml = @this;
		auto @pmoveState = @pm.playerState.pmove;

		// assign a contentmask for the movement type
		switch( pmoveState.pm_type ) {
			case PM_FREEZE:
			case PM_CHASECAM:
				pmoveState.pm_flags = pmoveState.pm_flags | PMF_NO_PREDICTION;
				pm.contentMask = 0;
				break;

			case PM_GIB:
				pmoveState.pm_flags = pmoveState.pm_flags | PMF_NO_PREDICTION;
				pm.contentMask = MASK_DEADSOLID;
				break;

			case PM_SPECTATOR:
				pmoveState.pm_flags = pmoveState.pm_flags & ~PMF_NO_PREDICTION;
				pm.contentMask = MASK_DEADSOLID;
				break;

			case PM_NORMAL:
			default:
				pmoveState.pm_flags = pmoveState.pm_flags & ~PMF_NO_PREDICTION;

				if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_GHOSTMOVE ) != 0 ) {
					pm.contentMask = MASK_DEADSOLID;
				} else {
					pm.contentMask = MASK_PLAYERSOLID;
				}
				break;
		}

		if( ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED ) == 0 ) {
			int stat;

			// drop timing counters
			if( pmoveState.pm_time != 0 ) {
				int msec;

				msec = pm.cmd.msec / 8;
				if( msec < 0 ) {
					msec = 1;
				}

				if( msec >= pmoveState.pm_time ) {
					pmoveState.pm_flags = pmoveState.pm_flags & int( ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT ) );
					pmoveState.pm_time = 0;
				} else {
					pmoveState.pm_time -= msec;
				}
			}

			DecPMoveStat( pm, PM_STAT_NOUSERCONTROL ); 
			DecPMoveStat( pm, PM_STAT_KNOCKBACK ); 

			// PM_STAT_CROUCHTIME is handled at PM_AdjustBBox
			// PM_STAT_ZOOMTIME is handled at PM_CheckZoom

			DecPMoveStat( pm, PM_STAT_DASHTIME ); 
			DecPMoveStat( pm, PM_STAT_WJTIME ); 
			DecPMoveStat( pm, PM_STAT_NOAUTOATTACK ); 
			DecPMoveStat( pm, PM_STAT_STUN ); 
			DecPMoveStat( pm, PM_STAT_FWDTIME ); 
		}

		if( pmoveState.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
			pml.forwardPush = 0.0f;
			pml.sidePush = 0.0f;
			pml.upPush = 0.0f;
			pm.cmd.buttons = 0;
		}

		// in order the forward accelt to kick in, one has to keep +fwd pressed
		// for some time without strafing
		if( pml.forwardPush <= 0.0f || pml.sidePush != 0.0f ) {
			pmoveState.stats[PM_STAT_FWDTIME] = PM_FORWARD_ACCEL_TIMEDELAY;
		}
	}

	int CrouchLerpPlayerSize( int timer, Vec3 &out mins, Vec3 &out maxs, float &out viewHeight ) {
		if( timer < 0 ) {
			timer = 0;
		} else if( timer > CROUCHTIME ) {
			timer = CROUCHTIME;
		}

		float crouchFrac = float( timer ) / float( CROUCHTIME );
		mins = playerboxStandMins - ( crouchFrac * ( playerboxStandMins - playerboxCrouchMins ) );
		maxs = playerboxStandMaxs - ( crouchFrac * ( playerboxStandMaxs - playerboxCrouchMaxs ) );
		viewHeight = playerboxStandViewheight - ( crouchFrac * ( playerboxStandViewheight - playerboxCrouchViewheight ) );

		return timer;
	}

	void CheckZoom( PMove @pm ) {
		auto @pml = @this;
		auto @playerState = @pm.playerState;
		auto @pmoveState = @pm.playerState.pmove;

		if( pmoveState.pm_type != PM_NORMAL ) {
			pmoveState.stats[PM_STAT_ZOOMTIME] = 0;
			return;
		}

		int zoom = pmoveState.stats[PM_STAT_ZOOMTIME];
		if( ( pm.cmd.buttons & BUTTON_ZOOM ) != 0 && ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_ZOOM ) != 0 ) {
			zoom += pm->cmd.msec;
		} else if( zoom > 0 ) {
			zoom -= pm->cmd.msec;
		}

		if( zoom < 0 ) {
			zoom = 0;
		} else if( zoom > ZOOMTIME ) {
			zoom = ZOOMTIME;
		}

		pmoveState.stats[PM_STAT_ZOOMTIME] = zoom;
	}

	/*
	* AdjustBBox
	*
	* Sets mins, maxs, and pm->viewheight
	*/
	void AdjustBBox( PMove @pm ) {
		auto @pml = @this;
		auto @playerState = @pm.playerState;
		auto @pmoveState = @pm.playerState.pmove;
		float crouchFrac;
		Trace trace;

		if( pmoveState.pm_type == PM_GIB ) {
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			pm.setSize( playerboxGibMins, playerboxGibMaxs );
			playerState.viewHeight = playerboxGibViewheight;
			return;
		}

		if( pmoveState.pm_type >= PM_FREEZE ) {
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			playerState.viewHeight = 0;
			return;
		}

		if( pmoveState.pm_type == PM_SPECTATOR ) {
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			playerState.viewHeight = playerboxStandViewheight;
		}

		if( pml.upPush < 0.0f && ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_CROUCH ) != 0 &&
			pmoveState.stats[PM_STAT_WJTIME] < ( PM_WALLJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) &&
			pmoveState.stats[PM_STAT_DASHTIME] < ( PM_DASHJUMP_TIMEDELAY - PM_SPECIAL_CROUCH_INHIBIT ) ) {
			Vec3 curmins, curmaxs;
			float curviewheight;

			int newcrouchtime = pmoveState.stats[PM_STAT_CROUCHTIME] + pm.cmd.msec;
			newcrouchtime = CrouchLerpPlayerSize( newcrouchtime, curmins, curmaxs, curviewheight );

			// it's going down, so, no need of checking for head-chomping
			pmoveState.stats[PM_STAT_CROUCHTIME] = newcrouchtime;
			pm.setSize( curmins, curmaxs );
			playerState.viewHeight = curviewheight;
			return;
		}

		// it's crouched, but not pressing the crouch button anymore, try to stand up
		if( pmoveState.stats[PM_STAT_CROUCHTIME] != 0 ) {
			Vec3 curmins, curmaxs, wishmins, wishmaxs;
			float curviewheight, wishviewheight;

			// find the current size
			CrouchLerpPlayerSize( pmoveState.stats[PM_STAT_CROUCHTIME], curmins, curmaxs, curviewheight );

			if( pm.cmd.msec == 0 ) { // no need to continue
				pm.setSize( curmins, curmaxs );
				playerState.viewHeight = curviewheight;
				return;
			}

			// find the desired size
			int newcrouchtime = pmoveState.stats[PM_STAT_CROUCHTIME] - pm.cmd.msec;
			newcrouchtime = CrouchLerpPlayerSize( newcrouchtime, wishmins, wishmaxs, wishviewheight );

			// check that the head is not blocked
			trace.doTrace( pml.origin, wishmins, wishmaxs, pml.origin, playerState.POVnum, pm.contentMask );
			if( trace.allSolid || trace.startSolid ) {
				// can't do the uncrouching, let the time alone and use old position
				pm.setSize( curmins, curmaxs );
				playerState.viewHeight = curviewheight;
				return;
			}

			// can do the uncrouching, use new position and update the time
			pmoveState.stats[PM_STAT_CROUCHTIME] = newcrouchtime;
			pm.setSize( wishmins, wishmaxs );
			playerState.viewHeight = wishviewheight;
			return;
		}

		// the player is not crouching at all
		pm.setSize( playerboxStandMins, playerboxStandMaxs );
		playerState.viewHeight = playerboxStandViewheight;
	}

	void AdjustViewheight( PMove @pm ) {
		float height;
		Vec3 pm_mins, pm_maxs, mins, maxs;
		auto @playerState = @pm.playerState;
		auto @pmoveState = @pm.playerState.pmove;

		pm.getSize( pm_mins, pm_maxs );
		if( pmoveState.pm_type == PM_SPECTATOR ) {
			pm_mins = playerboxStandMins;
			pm_maxs = playerboxStandMaxs;
		}

		GS::RoundUpToHullSize( pm_mins, pm_maxs, mins, maxs );

		height = pm_maxs.z - maxs.z;
		if( height > 0.0f ) {
			playerState.viewHeight -= height;
		}
	}

	void FlyMove( PMove @pm, bool doClip ) {
		auto @pml = @this;
		int i;
		float speed, drop, friction, control, newspeed;
		float currentspeed, addspeed, accelspeed, maxspeed;
		Vec3 wishvel;
		float fmove, smove;
		Vec3 wishdir;
		float wishspeed;
		Vec3 end;
		Trace trace;

		maxspeed = pml.maxPlayerSpeed * 1.5;

		if( ( pm.cmd.buttons & BUTTON_SPECIAL ) != 0 ) {
			maxspeed *= 2.0;
		}

		// friction
		speed = pml.velocity.length();
		if( speed < 1 ) {
			pml.velocity.clear();
		} else {
			drop = 0;

			friction = pm_friction * 1.5; // extra friction
			control = speed < pm_decelerate ? pm_decelerate : speed;
			drop += control * friction * pml.frametime;

			// scale the velocity
			newspeed = speed - drop;
			if( newspeed < 0 ) {
				newspeed = 0;
			}
			newspeed /= speed;
			pml.velocity *= newspeed;
		}

		// accelerate
		fmove = pml.forwardPush;
		smove = pml.sidePush;

		if( ( pm.cmd.buttons & BUTTON_SPECIAL ) != 0 ) {
			fmove *= 2.0f;
			smove *= 2.0f;
		}

		pml.forward.normalize();
		pml.right.normalize();

		wishvel = pml.forward * fmove + pml.right * smove;
		wishvel.z += pml.upPush;

		wishdir = wishvel;
		wishspeed = wishdir.normalize();

		// clamp to server defined max speed
		//
		if( wishspeed > maxspeed ) {
			wishspeed = maxspeed / wishspeed;
			wishvel *= wishspeed;
			wishspeed = maxspeed;
		}

		currentspeed = pml.velocity * wishdir;
		addspeed = wishspeed - currentspeed;
		if( addspeed > 0 ) {
			accelspeed = pm_accelerate * pml.frametime * wishspeed;
			if( accelspeed > addspeed ) {
				accelspeed = addspeed;
			}

			pml.velocity += wishdir * accelspeed;
		}

		// move
		end = pml.origin + pml.velocity * pml.frametime;

		if( doClip ) {
			trace.doTrace( pml.origin, pml.mins, pml.maxs, end, pm.playerState.POVnum, pm.contentMask );
			end = trace.endPos;
		}

		pml.origin = end;
	}

	void EndMove( PMove @pm ) {
		auto @pml = this;
		auto @playerState = pm.playerState;

		playerState.pmove.origin = pml.origin;
		playerState.pmove.velocity = pml.velocity;
	}
};

void Load() {
	GS::GetPlayerStandSize( playerboxStandMins, playerboxStandMaxs );
	playerboxStandViewheight = GS::GetPlayerStandViewHeight();

	GS::GetPlayerCrouchSize( playerboxCrouchMins, playerboxCrouchMaxs );
	playerboxCrouchViewheight = GS::GetPlayerCrouchHeight();

	GS::GetPlayerGibSize( playerboxGibMins, playerboxGibMaxs );
	playerboxGibViewheight = GS::GetPlayerGibHeight();
}

void PMove( PMove @pm ) {
	PMoveLocal pml;
	auto @playerState = @pm.playerState;
	auto @pmoveState = @playerState.pmove;
	int pm_type = pmoveState.pm_type;

	pml.BeginMove( pm );

	float fallvelocity = ( ( pml.velocity.z < 0.0f ) ? abs( pml.velocity.z ) : 0.0f );

	pml.PostBeginMove( pm );

	if( pm_type != PM_NORMAL ) { // includes dead, freeze, chasecam...
		if( ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED ) == 0 ) {
			pml.ClearDash( pm );

			pml.ClearWallJump( pm );

			pml.ClearStun( pm );

			pmoveState.stats[PM_STAT_KNOCKBACK] = 0;
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			pmoveState.stats[PM_STAT_ZOOMTIME] = 0;
			pmoveState.pm_flags = pmoveState.pm_flags & int( ~( PMF_JUMPPAD_TIME | PMF_DOUBLEJUMPED | PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT | PMF_SPECIAL_HELD ) );

			pml.AdjustBBox( pm );
		}

		pml.AdjustViewheight( pm );

		if( pm_type == PM_SPECTATOR ) {
			pml.FlyMove( pm, false );
		} else {
			pml.forwardPush = pml.sidePush = pml.upPush = 0.0f;
		}

		pml.EndMove( pm );
		return;
	}

	// set mins, maxs, viewheight and fov

	pml.AdjustBBox( pm );

	pml.CheckZoom( pm );

	pml.AdjustViewheight( pm );

	//pml.CategorizePosition();

	int oldGroundEntity = pm.groundEntity;

	//pml.CheckSpecialMovement();

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

		//pml.StepSlideMove();
	} else {
		// Kurim
		// Keep this order !
		//pml.CheckJump();

		//pml.CheckDash();

		//pml.CheckWallJump();

		//pml.CheckCrouchSlide();

		//pml.Friction();

		if( pm.waterLevel >= 2 ) {
			//pml.WaterMove();
		} else {
			Vec3 angles = playerState.viewAngles;

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

			//pml.Move();
		}
	}
	
	// set groundentity, watertype, and waterlevel for final spot
	//pml.CategorizePosition();

	pml.EndMove( pm );

	// falling event

	// Execute the triggers that are touched.
	// We check the entire path between the origin before the pmove and the
	// current origin to ensure no triggers are missed at high velocity.
	// Note that this method assumes the movement has been linear.
	pm.touchTriggers( pml.previousOrigin );

	// touching triggers may force groundentity off
	if( ( ( pmoveState.pm_flags & PMF_ON_GROUND ) == 0 ) && ( pm.groundEntity != -1 ) ) {
		pm.groundEntity = -1;
		pml.velocity.z = 0;
	}

	if( pm.groundEntity != -1 ) { // remove wall-jump and dash bits when touching ground
		// always keep the dash flag 50 msecs at least (to prevent being removed at the start of the dash)
		if( pmoveState.stats[PM_STAT_DASHTIME] < ( PM_DASHJUMP_TIMEDELAY - 50 ) ) {
			pmoveState.pm_flags = pmoveState.pm_flags & int( ~PMF_DASHING );
		}

		if( pmoveState.stats[PM_STAT_WJTIME] < ( PM_WALLJUMP_TIMEDELAY - 50 ) ) {
			//pml.ClearWallJump();
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

			GS::PredictedEvent( playerState.POVnum, EV_FALL, damageParam );
		}

		pmoveState.pm_flags = pmoveState.pm_flags & int( ~PMF_JUMPPAD_TIME );
	}
}

}