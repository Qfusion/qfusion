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

class PMoveLocal {
	PMove @pm;
	PlayerState @playerState;
	UserCmd cmd;

	Vec3 origin;          // full float precision
	Vec3 velocity;        // full float precision

	Vec3 pm_mins, pm_maxs;
	int pm_flags;

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

	void BeginMove( PMove @pm, PlayerState @ps, UserCmd cmd ) {
		auto @pml = @this;
		auto @pmoveState = @ps.pmove;

		@pml.pm = pm;
		@pml.playerState = ps;
		pml.cmd = cmd;
		pml.origin = pmoveState.origin;
		pml.velocity = pmoveState.velocity;

		// save old org in case we get stuck
		pml.previousOrigin = pml.origin;

		ps.viewAngles.angleVectors( pml.forward, pml.right, pml.up );

		pml.flatforward = pml.forward;
		pml.flatforward.z = 0.0f;
		pml.flatforward.normalize();

		pml.frametime = cmd.msec * 0.001;

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

		pml.forwardPush = float( cmd.forwardmove ) * SPEEDKEY / 127.0f;
		pml.sidePush = float( cmd.sidemove ) * SPEEDKEY / 127.0f;
		pml.upPush = float( cmd.upmove ) * SPEEDKEY / 127.0f;

		pml.pm_flags = pmoveState.pm_flags;
	}

	float Speed() {
		return velocity.length();
	}

	float HorizontalSpeed() {
		return HorizontalLength( velocity );
	}

	void SetHorizontalSpeed( float speed ) {
		float hnorm = HorizontalSpeed();
		if( hnorm != 0.0f ) {
			hnorm = 1.0f / hnorm;
		}
		hnorm *= speed;

		velocity.x *= hnorm;
		velocity.y *= hnorm;
	}

	void SetSize( const Vec3 &in mins, const Vec3 &in maxs ) {
		this.pm_mins = mins;
		this.pm_maxs = maxs;
		pm.setSize( mins, maxs );
	}

	int SlideMove() {
		auto @pml = @this;

		if( pm.groundEntity != -1 ) { // clip velocity to ground, no need to wait
			// if the ground is not horizontal (a ramp) clipping will slow the player down
			if( pml.groundPlaneNormal.z == 1.0f && pml.velocity.z < 0.0f ) {
				pml.velocity.z = 0.0f;
			}
		}

		pm.velocity = pml.velocity;
		pm.origin = pml.origin;
		pm.remainingTime = pml.frametime;
		pm.slideBounce = PM_OVERBOUNCE;

		int blockedmask = pm.slideMove();

		pml.velocity = pm.velocity;
		pml.origin = pm.origin;

		return blockedmask;
	}

	/*
	* StepSlideMove
	*
	* Each intersection will try to step over the obstruction instead of
	* sliding along it.
	*/
	void StepSlideMove() {
		auto @pml = @this;
		Vec3 start_o, start_v;
		Vec3 down_o, down_v;
		Trace trace;
		float down_dist, up_dist;
		float hspeed;
		Vec3 up, down;
		int blocked;

		start_o = pml.origin;
		start_v = pml.velocity;

		blocked = SlideMove();

		// We have modified the origin in PM_SlideMove() in this case.
		// No further computations are required.
		if( pm.skipCollision ) {
			return;
		}

		down_o = pml.origin;
		down_v = pml.velocity;

		up = start_o;
		up.z += STEPSIZE;

		trace.doTrace( up, pm_mins, pm_maxs, up, playerState.POVnum, pm.contentMask );
		if( trace.allSolid ) {
			return; // can't step up

		}
		// try sliding above
		pml.origin = up;
		pml.velocity = start_v;

		SlideMove();

		// push down the final amount
		down = pml.origin;
		down.z -= STEPSIZE;
		trace.doTrace( pml.origin, pm_mins, pm_maxs, down, playerState.POVnum, pm.contentMask );
		if( !trace.allSolid ) {
			pml.origin = trace.endPos;
		}

		up = pml.origin;

		// decide which one went farther
		down_dist = ( down_o.x - start_o.x ) * ( down_o.x - start_o.x )
					+ ( down_o.y - start_o.y ) * ( down_o.y - start_o.y );
		up_dist = ( up.x - start_o.x ) * ( up.x - start_o.x )
				  + ( up.y - start_o.y ) * ( up.y - start_o.y );

		if( down_dist >= up_dist || trace.allSolid || ( trace.fraction != 1.0 && !IsWalkablePlane( trace.planeNormal ) ) ) {
			pml.origin = down_o;
			pml.velocity = down_v;
			return;
		}

		// only add the stepping output when it was a vertical step
		if( ( blocked & SLIDEMOVEFLAG_WALL_BLOCKED ) != 0 ){
			pm.step = ( pml.origin.z - pml.previousOrigin.z );
		}

		// Preserve speed when sliding up ramps
		hspeed = HorizontalLength( start_v );
		if( hspeed > 0.0f && IsWalkablePlane( trace.planeNormal ) ) {
			if( trace.planeNormal.z >= 1.0f - SLIDEMOVE_PLANEINTERACT_EPSILON ) {
				pml.velocity = start_v;
			} else {
				SetHorizontalSpeed( hspeed );
			}
		}

		// wsw : jal : The following line is what produces the ramp sliding.

		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pml.velocity.z = down_v.z;
	}

	/*
	* Friction -- Modified for wsw
	*
	* Handles both ground friction and water friction
	*/
	void Friction() {
		float speed, newspeed, control;
		float friction;
		float drop;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		drop = 0.0f;
		speed = Speed();
		if( speed < 1 ) {
			pml.velocity.x = pml.velocity.y = 0;
			return;
		}

		// apply ground friction
		if( ( ( ( ( pm.groundEntity != -1 ) && ( pml.groundSurfFlags & SURF_SLICK ) == 0 ) )
			  && ( pm.waterLevel < 2 ) ) || pml.ladder ) {
			if( pmoveState.stats[PM_STAT_KNOCKBACK] <= 0 ) {
				friction = pm_friction;
				control = speed < pm_decelerate ? pm_decelerate : speed;
				if( ( pml.pm_flags & PMF_CROUCH_SLIDING ) != 0 ) {
					if( pmoveState.stats[PM_STAT_CROUCHSLIDETIME] < PM_CROUCHSLIDE_FADE ) {
						friction *= 1 - sqrt( float( pmoveState.stats[PM_STAT_CROUCHSLIDETIME] ) / PM_CROUCHSLIDE_FADE );
					} else {
						friction = 0;
					}
				}
				drop += control * friction * pml.frametime;
			}
		}

		// apply water friction
		if( ( pm.waterLevel >= 2 ) && !pml.ladder ) {
			drop += speed * pm_waterfriction * pm.waterLevel * pml.frametime;
		}

		// scale the velocity
		newspeed = speed - drop;
		if( newspeed <= 0 ) {
			pml.velocity.clear();
		} else {
			newspeed /= speed;
			pml.velocity *= newspeed;
		}
	}

	/*
	* Accelerate
	*
	* Handles user intended acceleration
	*/
	void Accelerate( const Vec3 &in wishdir, float wishspeed, float accel ) {
		float addspeed, accelspeed, currentspeed, realspeed, newspeed;
		bool crouchslide;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		realspeed = Speed();

		currentspeed = pml.velocity * wishdir;
		addspeed = wishspeed - currentspeed;
		if( addspeed <= 0 ) {
			return;
		}

		accelspeed = accel * pml.frametime * wishspeed;
		if( accelspeed > addspeed ) {
			accelspeed = addspeed;
		}

		crouchslide = ( pml.pm_flags & PMF_CROUCH_SLIDING ) != 0 && pm.groundEntity != -1 && ( pml.groundSurfFlags & SURF_SLICK ) == 0;

		if( crouchslide ) {
			accelspeed *= PM_CROUCHSLIDE_CONTROL;
		}

		pml.velocity += accelspeed * wishdir;

		if( crouchslide ) { // disable overacceleration while crouch sliding
			newspeed = Speed();
			if( newspeed > wishspeed && newspeed != 0 ) {
				pml.velocity *= max( wishspeed, realspeed ) / newspeed;
			}
		}
	}

	void AirAccelerate( const Vec3 &in wishdir, float wishspeed ) {
		Vec3 heading( velocity.x, velocity.y, 0 );
		float speed = heading.normalize();
		auto @pml = @this;

		// Speed is below player walk speed
		if( speed <= pml.maxPlayerSpeed ) {
			// Apply acceleration
			pml.velocity += ( pml.maxPlayerSpeed * pml.frametime ) * wishdir;
			return;
		}

		// Calculate a dot product between heading and wishdir
		// Looking straight results in better acceleration
		float dot = Boundf( 0.0f, 50 * ( heading * wishdir - 0.98 ), 1.0f );

		// Calculate resulting acceleration
		float accel = dot * pml.maxPlayerSpeed * pml.maxPlayerSpeed * pml.maxPlayerSpeed / ( speed * speed );

		// Apply acceleration
		pml.velocity += accel * pml.frametime * heading;
	}

	// when using +strafe convert the inertia to forward speed.
	void Aircontrol( const Vec3 &in wishdir, float wishspeed ) {
		int i;
		float zspeed, speed, dot, k;
		float smove;
		auto @pml = @this;

		if( pm_aircontrol == 0 ) {
			return;
		}

		// accelerate
		smove = pml.sidePush;

		if( ( smove > 0 || smove < 0 ) || ( wishspeed == 0.0 ) ) {
			return; // can't control movement if not moving forward or backward
		}

		zspeed = pml.velocity.z;
		pml.velocity.z = 0;
		speed = pml.velocity.normalize();

		dot = pml.velocity * wishdir;
		k = 32.0f * pm_aircontrol * dot * dot * pml.frametime;

		if( dot > 0 ) {
			// we can't change direction while slowing down
			pml.velocity = pml.velocity * speed + wishdir * k;
			pml.velocity.normalize();
		}

		pml.velocity.x *= speed;
		pml.velocity.y *= speed;
		pml.velocity.z = zspeed;
	}

	Vec3 AddCurrents( Vec3 wishvel ) {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		//
		// account for ladders
		//

		if( pml.ladder && abs( pml.velocity[2] ) <= DEFAULT_LADDERSPEED ) {
			if( ( playerState.viewAngles[PITCH] <= -15 ) && ( pml.forwardPush > 0 ) ) {
				wishvel.z = DEFAULT_LADDERSPEED;
			} else if( ( playerState.viewAngles[PITCH] >= 15 ) && ( pml.forwardPush > 0 ) ) {
				wishvel.z = -DEFAULT_LADDERSPEED;
			} else if( pml.upPush > 0 ) {
				wishvel.z = DEFAULT_LADDERSPEED;
			} else if( pml.upPush < 0 ) {
				wishvel.z = -DEFAULT_LADDERSPEED;
			} else {
				wishvel.z = 0;
			}

			// limit horizontal speed when on a ladder
			wishvel.x = Boundf( -25.0f, wishvel.x, 25.0f );
			wishvel.y = Boundf( -25.0f, wishvel.y, 25.0f );
		}

		return wishvel;
	}

	/*
	* WaterMove
	*/
	void WaterMove() {
		int i;
		Vec3 wishvel;
		float wishspeed;
		Vec3 wishdir;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		// user intentions
		wishvel = pml.forward * pml.forwardPush + pml.right * pml.sidePush;

		if( pml.forwardPush == 0 && pml.sidePush == 0 && pml.upPush == 0 ) {
			wishvel.z -= 60; // drift towards bottom
		} else {
			wishvel.z += pml.upPush;
		}

		wishvel = AddCurrents( wishvel );
		wishdir = wishvel;
		wishspeed = wishdir.normalize();

		if( wishspeed > pml.maxPlayerSpeed ) {
			wishspeed = pml.maxPlayerSpeed / wishspeed;
			wishvel *= wishspeed;
			wishspeed = pml.maxPlayerSpeed;
		}
		wishspeed *= 0.5;

		Accelerate( wishdir, wishspeed, pm_wateraccelerate );
		StepSlideMove();
	}

	/*
	* Move -- Kurim
	*/
	void Move() {
		int i;
		Vec3 wishvel;
		float fmove, smove;
		Vec3 wishdir;
		float wishspeed;
		float maxspeed;
		float accel;
		float wishspeed2;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		fmove = pml.forwardPush;
		smove = pml.sidePush;

		wishvel = pml.forward * fmove + pml.right * smove;
		wishvel.z = 0;
		wishvel = AddCurrents( wishvel );

		wishdir = wishvel;
		wishspeed = wishdir.normalize();

		// clamp to server defined max speed

		if( pmoveState.stats[PM_STAT_CROUCHTIME] != 0 ) {
			maxspeed = pml.maxCrouchedSpeed;
		} else if( ( pml.cmd.buttons & BUTTON_WALK ) != 0 && ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_WALK ) != 0 ) {
			maxspeed = pml.maxWalkSpeed;
		} else {
			maxspeed = pml.maxPlayerSpeed;
		}

		if( wishspeed > maxspeed ) {
			wishspeed = maxspeed / wishspeed;
			wishvel *= wishspeed;
			wishspeed = maxspeed;
		}

		if( pml.ladder ) {
			Accelerate( wishdir, wishspeed, pm_accelerate );

			if( wishvel.z == 0.0f ) {
				if( pml.velocity.z > 0 ) {
					pml.velocity.z -= pmoveState.gravity * pml.frametime;
					if( pml.velocity.z < 0 ) {
						pml.velocity.z = 0;
					}
				} else {
					pml.velocity.z += pmoveState.gravity * pml.frametime;
					if( pml.velocity.z > 0 ) {
						pml.velocity.z  = 0;
					}
				}
			}

			StepSlideMove();
		} else if( pm.groundEntity != -1 ) {
			// walking on ground
			if( pml.velocity.z > 0 ) {
				pml.velocity.z = 0; //!!! this is before the accel
			}

			Accelerate( wishdir, wishspeed, pm_accelerate );

			// fix for negative trigger_gravity fields
			if( pmoveState.gravity > 0 ) {
				if( pml.velocity.z > 0 ) {
					pml.velocity.z = 0;
				}
			} else {
				pml.velocity.z -= pmoveState.gravity * pml.frametime;
			}

			if( pml.velocity.x == 0.0f && pml.velocity.y == 0.0f ) {
				return;
			}

			StepSlideMove();
		} else if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_AIRCONTROL ) != 0
				   && ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_FWDBUNNY ) == 0 ) {
			// Air Control
			wishspeed2 = wishspeed;
			if( pml.velocity * wishdir < 0
				&& ( pml.pm_flags & PMF_WALLJUMPING ) == 0
				&& ( pmoveState.stats[PM_STAT_KNOCKBACK] <= 0 ) ) {
				accel = pm_airdecelerate;
			} else {
				accel = pm_airaccelerate;
			}

			// ch : remove knockback test here
			if( ( pml.pm_flags & PMF_WALLJUMPING ) != 0
				/* || ( pmoveState.stats[PM_STAT_KNOCKBACK] > 0 ) */ ) {
				accel = 0; // no stopmove while walljumping
			}

			if( ( smove > 0 || smove < 0 ) && ( fmove == 0 ) && ( pmoveState.stats[PM_STAT_KNOCKBACK] <= 0 ) ) {
				if( wishspeed > pm_wishspeed ) {
					wishspeed = pm_wishspeed;
				}
				accel = pm_strafebunnyaccel;
			}

			// Air control
			Accelerate( wishdir, wishspeed, accel );
			if( pm_aircontrol > 0 && ( pml.pm_flags & PMF_WALLJUMPING ) == 0 && ( pmoveState.stats[PM_STAT_KNOCKBACK] <= 0 ) ) { // no air ctrl while wjing
				Aircontrol( wishdir, wishspeed2 );
			}

			// add gravity
			pml.velocity.z -= pmoveState.gravity * pml.frametime;
			StepSlideMove();
		} else {   // air movement (old school)
			bool inhibit = false;
			bool accelerating, decelerating;

			accelerating = ( pml.velocity * wishdir ) > 0.0f ? true : false;
			decelerating = ( pml.velocity * wishdir ) < -0.0f ? true : false;

			if( ( pml.pm_flags & PMF_WALLJUMPING ) != 0 &&
				( pmoveState.stats[PM_STAT_WJTIME] >= ( PM_WALLJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
				inhibit = true;
			}

			if( ( pml.pm_flags & PMF_DASHING ) != 0 &&
				( pmoveState.stats[PM_STAT_DASHTIME] >= ( PM_DASHJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
				inhibit = true;
			}

			if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_FWDBUNNY ) == 0 ||
				pmoveState.stats[PM_STAT_FWDTIME] > 0 ) {
				inhibit = true;
			}

			// ch : remove this because of the knockback 'bug'?
			/*
			if( pmoveState.stats[PM_STAT_KNOCKBACK] > 0 )
				inhibit = true;
			*/

			// (aka +fwdbunny) pressing forward or backward but not pressing strafe and not dashing
			if( accelerating && !inhibit && smove == 0 && fmove != 0 ) {
				AirAccelerate( wishdir, wishspeed );
				Aircontrol( wishdir, wishspeed );
			} else {   // strafe running
				bool aircontrol = true;

				wishspeed2 = wishspeed;
				if( decelerating &&
					( pml.pm_flags & PMF_WALLJUMPING ) == 0 ) {
					accel = pm_airdecelerate;
				} else {
					accel = pm_airaccelerate;
				}

				// ch : knockback out
				if( ( pml.pm_flags & PMF_WALLJUMPING ) != 0
					/*	|| ( pmoveState.stats[PM_STAT_KNOCKBACK] > 0 ) */) {
					accel = 0; // no stop-move while wall-jumping
					aircontrol = false;
				}

				if( ( pml.pm_flags & PMF_DASHING ) != 0 &&
					( pmoveState.stats[PM_STAT_DASHTIME] >= ( PM_DASHJUMP_TIMEDELAY - PM_AIRCONTROL_BOUNCE_DELAY ) ) ) {
					aircontrol = false;
				}

				if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_AIRCONTROL ) == 0 ) {
					aircontrol = false;
				}

				// +strafe bunnyhopping
				if( aircontrol && smove != 0 && fmove == 0 ) {
					if( wishspeed > pm_wishspeed ) {
						wishspeed = pm_wishspeed;
					}

					Accelerate( wishdir, wishspeed, pm_strafebunnyaccel );
					Aircontrol( wishdir, wishspeed2 );
				} else {   // standard movement (includes strafejumping)
					Accelerate( wishdir, wishspeed, accel );
				}
			}

			// add gravity
			pml.velocity.z -= pmoveState.gravity * pml.frametime;
			StepSlideMove();
		}
	}

	/*
	* If the player hull point one-quarter unit down is solid, the player is on ground
	*/
	void GroundTrace( Trace &out trace ) {
		Vec3 mins, maxs;
		auto @pml = @this;

		if( pm.skipCollision ) {
			return;
		}

		// see if standing on something solid
		Vec3 point( pml.origin.x, pml.origin.y, pml.origin.z - 0.25 );

		trace.doTrace( pml.origin, pm_mins, pm_maxs, point, playerState.POVnum, pm.contentMask );
	}

	void UnstickPosition( Trace &out trace ) {
		int j;
		Vec3 origin;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		if( pm.skipCollision ) {
			return;
		}
		if( pmoveState.pm_type == PM_SPECTATOR ) {
			return;
		}

		origin = pml.origin;

		// try all combinations
		for( j = 0; j < 8; j++ ) {
			origin = pml.origin;

			origin.x += ( j & 1 ) != 0 ? -1.0f : 1.0f;
			origin.y += ( j & 2 ) != 0 ? -1.0f : 1.0f;
			origin.z += ( j & 4 ) != 0 ? -1.0f : 1.0f;

			trace.doTrace( origin, pm_mins, pm_maxs, origin, playerState.POVnum, pm.contentMask );

			if( !trace.allSolid ) {
				pml.origin = origin;
				GroundTrace( trace );
				return;
			}
		}

		// go back to the last position
		pml.origin = pml.previousOrigin;
	}

	void CategorizePosition() {
		auto @pml = @this;
		auto @playerState = @pml.playerState;
		auto @pmoveState = @playerState.pmove;

		if( pml.velocity.z > 180 ) { // !!ZOID changed from 100 to 180 (ramp accel)
			pml.pm_flags &= ~PMF_ON_GROUND;
			pm.groundEntity = -1;
		} else {
			Trace trace;

			// see if standing on something solid
			GroundTrace( trace );

			if( trace.allSolid ) {
				// try to unstick position
				UnstickPosition( trace );
			}

			pml.groundPlaneNormal = trace.planeNormal;
			pml.groundPlaneDist = trace.planeDist;
			pml.groundSurfFlags = trace.surfFlags;
			pml.groundContents = trace.contents;

			if( ( trace.fraction == 1.0f ) || ( !IsWalkablePlane( trace.planeNormal ) && !trace.startSolid ) ) {
				pm.groundEntity = -1;
				pml.pm_flags &= ~PMF_ON_GROUND;
			} else {
				pm.groundEntity = trace.entNum;
				pm.groundPlaneNormal = trace.planeNormal;
				pm.groundPlaneDist = trace.planeDist;
				pm.groundSurfFlags = trace.surfFlags;
				pm.groundContents = trace.contents;

				// hitting solid ground will end a waterjump
				if( ( pml.pm_flags & PMF_TIME_WATERJUMP ) != 0 ) {
					pml.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
					pmoveState.pm_time = 0;
				}

				if( ( pml.pm_flags & PMF_ON_GROUND ) == 0 ) { // just hit the ground
					pml.pm_flags |= PMF_ON_GROUND;
				}
			}

			if( trace.fraction < 1.0 ) {
				pm.addTouchEnt( trace.entNum );
			}
		}

		//
		// get waterlevel, accounting for ducking
		//
		pm.waterLevel = 0;
		pm.waterType = 0;

		float sample2 = playerState.viewHeight - pm_mins.z;
		float sample1 = sample2 / 2.0f;

		Vec3 point = pml.origin;
		point.z = pml.origin.z + pm_mins.z + 1.0f;
		int cont = GS::PointContents( point );

		if( ( cont & MASK_WATER ) != 0 ) {
			pm.waterType = cont;
			pm.waterLevel = 1;
			point.z = pml.origin.z + pm_mins.z + sample1;
			cont = GS::PointContents( point );
			if( ( cont & MASK_WATER ) != 0 ) {
				pm.waterLevel = 2;
				point.z = pml.origin.z + pm_mins.z + sample2;
				cont = GS::PointContents( point );
				if( ( cont & MASK_WATER ) != 0 ) {
					pm.waterLevel = 3;
				}
			}
		}
	}

	void ClearDash() {
		auto @pmoveState = @playerState.pmove;
		pml.pm_flags &= ~PMF_DASHING;
		pmoveState.stats[PM_STAT_DASHTIME] = 0;
	}

	void ClearWallJump() {
		auto @pmoveState = @playerState.pmove;
		pml.pm_flags &= ~(PMF_WALLJUMPING|PMF_WALLJUMPCOUNT);
		pmoveState.stats[PM_STAT_WJTIME] = 0;
	}

	void ClearStun() {
		auto @pmoveState = @playerState.pmove;
		pmoveState.stats[PM_STAT_STUN] = 0;
	}

	void ClipVelocityAgainstGround( void ) {
		auto @pml = @this;
		
		// clip against the ground when jumping if moving that direction
		if( pml.groundPlaneNormal.z > 0 && pml.velocity.z < 0 ) {
			Vec3 n2( pml.groundPlaneNormal.x, pml.groundPlaneNormal.y, 0.0f );
			Vec3 v2( pml.velocity.x, pml.velocity.y, 0.0f );
			if( n2 * v2 > 0.0f ) {
				pml.velocity = GS::ClipVelocity( pml.velocity, pml.groundPlaneNormal, PM_OVERBOUNCE );
			}
		}
	}

	// Walljump wall availability check
	// nbTestDir is the number of directions to test around the player
	// maxZnormal is the max Z value of the normal of a poly to consider it a wall
	// normal becomes a pointer to the normal of the most appropriate wall
	Vec3 PlayerTouchWall( int nbTestDir, float maxZnormal ) {
		int i;
		Trace trace;
		Vec3 zero, dir, mins, maxs;
		bool alternate;
		float r, d, dx, dy, m;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		// if there is nothing at all within the checked area, we can skip the individual checks
		// this optimization must always overapproximate the combination of those checks
		mins.x = pm_mins.x - pm_maxs.x;
		mins.y = pm_mins.y - pm_maxs.y;
		maxs.x = pm_maxs.x + pm_maxs.x;
		maxs.y = pm_maxs.y + pm_maxs.y;
		if( pml.velocity[0] > 0 ) {
			maxs.x += pml.velocity.x * 0.015f;
		} else {
			mins.x += pml.velocity.x * 0.015f;
		}
		if( pml.velocity[1] > 0 ) {
			maxs.y += pml.velocity.y * 0.015f;
		} else {
			mins.y += pml.velocity.y * 0.015f;
		}
		mins.z = maxs.z = 0;
		trace.doTrace( pml.origin, mins, maxs, pml.origin, playerState.POVnum, pm.contentMask );
		if( !trace.allSolid && trace.fraction == 1.0f ) {
			return Vec3( 0.0f );
		}

		// determine the primary direction
		if( pml.sidePush > 0 ) {
			r = -180.0f / 2.0f;
		} else if( pml.sidePush < 0 ) {
			r = 180.0f / 2.0f;
		} else if( pml.forwardPush > 0 ) {
			r = 0.0f;
		} else {
			r = 180.0f;
		}
		alternate = pml.sidePush == 0 || pml.forwardPush == 0;

		d = 0.0f; // current distance from the primary direction

		for( i = 0; i < nbTestDir; i++ ) {
			if( i != 0 ) {
				if( alternate ) {
					r += 180.0f; // switch front and back
				}
				if( ( !alternate && i % 2 == 0 ) || ( alternate && i % 4 == 0 ) ) { // switch from left to right
					r -= 2 * d;
				} else if( !alternate || ( alternate && i % 4 == 2 ) ) {   // switch from right to left and move further away
					r += d;
					d += 360.0f / nbTestDir;
					r += d;
				}
			}

			// determine the relative offsets from the origin
			dx = cos( deg2rad( playerState.viewAngles[YAW] + r ) );
			dy = sin( deg2rad( playerState.viewAngles[YAW] + r ) );

			// project onto the player box
			if( dx == 0.0f ) {
				m = pm_maxs[1];
			} else if( dy == 0.0f ) {
				m = pm_maxs[0];
			} else if( abs( dx / pm_maxs.x ) > abs( dy / pm_maxs.y ) ) {
				m = abs( pm_maxs.x / dx );
			} else {
				m = abs( pm_maxs.y / dy );
			}

			// allow a gap between the player and the wall
			m += pm_maxs.x;

			dir.x = pml.origin.x + dx * m + pml.velocity.x * 0.015f;
			dir.y = pml.origin.y + dy * m + pml.velocity.y * 0.015f;
			dir.z = pml.origin.z;

			trace.doTrace( pml.origin, zero, zero, dir, playerState.POVnum, pm.contentMask );

			if( trace.allSolid ) {
				return Vec3( 0.0f );
			}

			if( trace.fraction == 1.0f ) {
				continue; // no wall in this direction

			}
			if( ( trace.surfFlags & ( SURF_SKY | SURF_NOWALLJUMP ) ) != 0 ) {
				continue;
			}

			if( trace.entNum > 0 && GS::GetEntityState( trace.entNum ).type == ET_PLAYER ) {
				continue;
			}

			if( trace.fraction > 0.0f && abs( trace.planeNormal.z ) < maxZnormal ) {
				return trace.planeNormal;
			}
		}

		return Vec3( 0.0f );
	}

	/*
	* CheckJump
	*/
	void CheckJump() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		if( pml.upPush < 10 ) {
			// not holding jump
			if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) == 0 ) {
				pml.pm_flags &= ~PMF_JUMP_HELD;
			}
			return;
		}

		if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) == 0 ) {
			// must wait for jump to be released
			if( ( pml.pm_flags & PMF_JUMP_HELD ) != 0 ) {
				return;
			}
		}

		if( pmoveState.pm_type != PM_NORMAL ) {
			return;
		}

		if( pm.waterLevel >= 2 ) { // swimming, not jumping
			pm.groundEntity = -1;
			return;
		}

		if( pm.groundEntity == -1 ) {
			return;
		}

		if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_JUMP ) == 0 ) {
			return;
		}

		if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_CONTINOUSJUMP ) == 0 ) {
			pml.pm_flags |= PMF_JUMP_HELD;
		}

		pm.groundEntity = -1;

		ClipVelocityAgainstGround();

		if( pml.velocity.z > 100 ) {
			GS::PredictedEvent( playerState.POVnum, EV_DOUBLEJUMP, 0 );
			pml.velocity.z += pml.jumpPlayerSpeed;
		} else if( pml.velocity[2] > 0 ) {
			GS::PredictedEvent( playerState.POVnum, EV_JUMP, 0 );
			pml.velocity.z += pml.jumpPlayerSpeed;
		} else {
			GS::PredictedEvent( playerState.POVnum, EV_JUMP, 0 );
			pml.velocity.z = pml.jumpPlayerSpeed;
		}

		// remove wj count
		pml.pm_flags &= ~PMF_JUMPPAD_TIME;

		ClearDash();
		ClearWallJump();
	}

	void CheckDash() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;
		float upspeed;
		Vec3 dashdir;

		if( ( pml.cmd.buttons & BUTTON_SPECIAL ) == 0 ) {
			pml.pm_flags &= ~PMF_SPECIAL_HELD;
		}

		if( pmoveState.pm_type != PM_NORMAL ) {
			return;
		}

		if( pmoveState.stats[PM_STAT_DASHTIME] > 0 ) {
			return;
		}

		if( pmoveState.stats[PM_STAT_KNOCKBACK] > 0 ) { // can not start a new dash during knockback time
			return;
		}

		if( ( pml.cmd.buttons & BUTTON_SPECIAL ) != 0 && pm.groundEntity != -1
			&& ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_DASH ) != 0 ) {
			if( ( pml.pm_flags & PMF_SPECIAL_HELD ) != 0 ) {
				return;
			}

			pml.pm_flags &= ~PMF_JUMPPAD_TIME;
			ClearWallJump();

			pml.pm_flags |= PMF_DASHING | PMF_SPECIAL_HELD;
			pm.groundEntity = -1;

			ClipVelocityAgainstGround();

			if( pml.velocity.z <= 0.0f ) {
				upspeed = pm_dashupspeed;
			} else {
				upspeed = pm_dashupspeed + pml.velocity.z;
			}

			// ch : we should do explicit forwardPush here, and ignore sidePush ?
			dashdir = pml.flatforward * pml.forwardPush + pml.sidePush * pml.right;
			dashdir.z = 0.0;

			if( dashdir.length() < 0.01f ) { // if not moving, dash like a "forward dash"
				dashdir = pml.flatforward;
			} else {
				dashdir.normalize();
			}

			float actual_velocity = HorizontalSpeed();
			if( actual_velocity <= pml.dashPlayerSpeed ) {
				dashdir *= pml.dashPlayerSpeed;
			} else {
				dashdir *= actual_velocity;
			}

			pml.velocity = dashdir;
			pml.velocity.z = upspeed;

			pmoveState.stats[PM_STAT_DASHTIME] = PM_DASHJUMP_TIMEDELAY;

			// return sound events
			if( abs( pml.sidePush ) > 10 && abs( pml.sidePush ) >= abs( pml.forwardPush ) ) {
				if( pml.sidePush > 0 ) {
					GS::PredictedEvent( playerState.POVnum, EV_DASH, 2 );
				} else {
					GS::PredictedEvent( playerState.POVnum, EV_DASH, 1 );
				}
			} else if( pml.forwardPush < -10 ) {
				GS::PredictedEvent( playerState.POVnum, EV_DASH, 3 );
			} else {
				GS::PredictedEvent( playerState.POVnum, EV_DASH, 0 );
			}
		} else if( pm.groundEntity == -1 ) {
			pml.pm_flags &= ~PMF_DASHING;
		}
	}

	/*
	* CheckWallJump -- By Kurim
	*/
	void CheckWallJump() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;
		Vec3 normal;
		float hspeed;
		Vec3 pm_mins, pm_maxs;
		const float pm_wjminspeed = ( ( pml.maxWalkSpeed + pml.maxPlayerSpeed ) * 0.5f );

		if( ( pml.cmd.buttons & BUTTON_SPECIAL ) == 0 ) {
			pml.pm_flags &= ~PMF_SPECIAL_HELD;
		}

		if( pm.groundEntity != -1 ) {
			pml.pm_flags &= ~(PMF_WALLJUMPING | PMF_WALLJUMPCOUNT);
		}

		if( ( pmoveState.pm_flags & PMF_WALLJUMPING ) != 0 && pml.velocity.z < 0.0 ) {
			pml.pm_flags &= ~PMF_WALLJUMPING;
		}

		if( pmoveState.stats[PM_STAT_WJTIME] <= 0 ) { // reset the wj count after wj delay
			pml.pm_flags &= ~PMF_WALLJUMPCOUNT;
		}

		if( pmoveState.pm_type != PM_NORMAL ) {
			return;
		}

		// don't walljump in the first 100 milliseconds of a dash jump
		if( ( pml.pm_flags & PMF_DASHING ) != 0
			&& ( pmoveState.stats[PM_STAT_DASHTIME] > ( PM_DASHJUMP_TIMEDELAY - 100 ) ) ) {
			return;
		}

		// markthis

		if( pm.groundEntity == -1 && ( pml.cmd.buttons & BUTTON_SPECIAL ) != 0
			&& ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_WALLJUMP ) != 0 &&
			( ( pml.pm_flags & PMF_WALLJUMPCOUNT ) == 0 )
			&& pmoveState.stats[PM_STAT_WJTIME] <= 0
			&& !pm.skipCollision
			) {
			Trace trace;
			Vec3 point = pml.origin;
			point.z -= STEPSIZE;

			// don't walljump if our height is smaller than a step
			// unless jump is pressed or the player is moving faster than dash speed and upwards
			hspeed = HorizontalSpeed();
			trace.doTrace( pml.origin, pm_mins, pm_maxs, point, playerState.POVnum, pm.contentMask );

			if( pml.upPush >= 10
				|| ( hspeed > pmoveState.stats[PM_STAT_DASHSPEED] && pml.velocity[2] > 8 )
				|| ( trace.fraction == 1.0f ) || ( !IsWalkablePlane( trace.planeNormal ) && !trace.startSolid ) ) {
				normal = PlayerTouchWall( 20, 0.3f );
				if( normal.length() == 0.0f ) {
					return;
				}

				if( ( pml.pm_flags & PMF_SPECIAL_HELD ) == 0
					&& ( pml.pm_flags & PMF_WALLJUMPING ) == 0 ) {
					float oldupvelocity = pml.velocity.z;
					pml.velocity.z = 0.0;

					hspeed = pml.velocity.normalize();

					// if stunned almost do nothing
					if( pmoveState.stats[PM_STAT_STUN] > 0 ) {
						pml.velocity = GS::ClipVelocity( pml.velocity, normal, 1.0f );
						pml.velocity += pm_failedwjbouncefactor * normal;

						pml.velocity.normalize();

						pml.velocity *= hspeed;
						pml.velocity.z = ( oldupvelocity + pm_failedwjupspeed > pm_failedwjupspeed ) ? oldupvelocity : oldupvelocity + pm_failedwjupspeed;
					} else {
						pml.velocity = GS::ClipVelocity( pml.velocity, normal, 1.0005f );
						pml.velocity += pm_wjbouncefactor * normal;

						if( hspeed < pm_wjminspeed ) {
							hspeed = pm_wjminspeed;
						}

						pml.velocity.normalize();

						pml.velocity *= hspeed;
						pml.velocity.z = ( oldupvelocity > pm_wjupspeed ) ? oldupvelocity : pm_wjupspeed; // jal: if we had a faster upwards speed, keep it
					}

					// set the walljumping state
					pml.ClearDash();

					pml.pm_flags &= ~PMF_JUMPPAD_TIME;
					pml.pm_flags |= PMF_WALLJUMPING | PMF_SPECIAL_HELD | PMF_WALLJUMPCOUNT;

					if( pmoveState.stats[PM_STAT_STUN] > 0 ) {
						pmoveState.stats[PM_STAT_WJTIME] = PM_WALLJUMP_FAILED_TIMEDELAY;

						// Create the event
						GS::PredictedEvent( playerState.POVnum, EV_WALLJUMP_FAILED, GS::DirToByte( normal ) );
					} else {
						pmoveState.stats[PM_STAT_WJTIME] = PM_WALLJUMP_TIMEDELAY;

						// Create the event
						GS::PredictedEvent( playerState.POVnum, EV_WALLJUMP, GS::DirToByte( normal ) );
					}
				}
			}
		} else {
			pml.pm_flags &= ~PMF_WALLJUMPING;
		}
	}

	void DecreasePMoveStat( int stat ) {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		int value = pmoveState.stats[stat];
		if( value > 0 ) {
			value -= pml.cmd.msec;
		} else if( value < 0 ) {
			value = 0;
		}

		pmoveState.stats[stat] = value;
	}

	void PostBeginMove() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		// clear results
		pm.numTouchEnts = 0;
		pm.groundEntity = -1;
		pm.waterType = 0;
		pm.waterLevel = 0;
		pm.step = 0.0f;

		// assign a contentmask for the movement type
		switch( pmoveState.pm_type ) {
			case PM_FREEZE:
			case PM_CHASECAM:
				pml.pm_flags |= PMF_NO_PREDICTION;
				pm.contentMask = 0;
				break;

			case PM_GIB:
				pml.pm_flags |= PMF_NO_PREDICTION;
				pm.contentMask = MASK_DEADSOLID;
				break;

			case PM_SPECTATOR:
				pml.pm_flags &= ~PMF_NO_PREDICTION;
				pm.contentMask = MASK_DEADSOLID;
				break;

			case PM_NORMAL:
			default:
				pml.pm_flags &= ~PMF_NO_PREDICTION;

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

				msec = pml.cmd.msec / 8;
				if( msec < 0 ) {
					msec = 1;
				}

				if( msec >= pmoveState.pm_time ) {
					pml.pm_flags &= ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT );
					pmoveState.pm_time = 0;
				} else {
					pmoveState.pm_time -= msec;
				}
			}

			DecreasePMoveStat( PM_STAT_NOUSERCONTROL ); 
			DecreasePMoveStat( PM_STAT_KNOCKBACK ); 

			// PM_STAT_CROUCHTIME is handled at PM_AdjustBBox
			// PM_STAT_ZOOMTIME is handled at PM_CheckZoom

			DecreasePMoveStat( PM_STAT_DASHTIME ); 
			DecreasePMoveStat( PM_STAT_WJTIME ); 
			DecreasePMoveStat( PM_STAT_NOAUTOATTACK ); 
			DecreasePMoveStat( PM_STAT_STUN ); 
			DecreasePMoveStat( PM_STAT_FWDTIME ); 
		}

		if( pmoveState.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
			pml.forwardPush = 0.0f;
			pml.sidePush = 0.0f;
			pml.upPush = 0.0f;
			pml.cmd.buttons = 0;
		}

		// in order the forward accelt to kick in, one has to keep +fwd pressed
		// for some time without strafing
		if( pml.forwardPush <= 0.0f || pml.sidePush != 0.0f ) {
			pmoveState.stats[PM_STAT_FWDTIME] = PM_FORWARD_ACCEL_TIMEDELAY;
		}

		pm.passEnt = int( playerState.POVnum );
	}

	void CheckZoom() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		if( pmoveState.pm_type != PM_NORMAL ) {
			pmoveState.stats[PM_STAT_ZOOMTIME] = 0;
			return;
		}

		int zoom = pmoveState.stats[PM_STAT_ZOOMTIME];
		if( ( pml.cmd.buttons & BUTTON_ZOOM ) != 0 && ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_ZOOM ) != 0 ) {
			zoom += pml.cmd.msec;
		} else if( zoom > 0 ) {
			zoom -= pml.cmd.msec;
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
	void AdjustBBox() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;
		float crouchFrac;
		Trace trace;

		if( pmoveState.pm_type == PM_GIB ) {
			pmoveState.stats[PM_STAT_CROUCHTIME] = 0;
			SetSize( playerboxGibMins, playerboxGibMaxs );
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

			int newcrouchtime = pmoveState.stats[PM_STAT_CROUCHTIME] + pml.cmd.msec;
			newcrouchtime = CrouchLerpPlayerSize( newcrouchtime, curmins, curmaxs, curviewheight );

			// it's going down, so, no need of checking for head-chomping
			pmoveState.stats[PM_STAT_CROUCHTIME] = newcrouchtime;
			SetSize( curmins, curmaxs );
			playerState.viewHeight = curviewheight;
			return;
		}

		// it's crouched, but not pressing the crouch button anymore, try to stand up
		if( pmoveState.stats[PM_STAT_CROUCHTIME] != 0 ) {
			Vec3 curmins, curmaxs, wishmins, wishmaxs;
			float curviewheight, wishviewheight;

			// find the current size
			CrouchLerpPlayerSize( pmoveState.stats[PM_STAT_CROUCHTIME], curmins, curmaxs, curviewheight );

			if( pml.cmd.msec == 0 ) { // no need to continue
				SetSize( curmins, curmaxs );
				playerState.viewHeight = curviewheight;
				return;
			}

			// find the desired size
			int newcrouchtime = pmoveState.stats[PM_STAT_CROUCHTIME] - pml.cmd.msec;
			newcrouchtime = CrouchLerpPlayerSize( newcrouchtime, wishmins, wishmaxs, wishviewheight );

			// check that the head is not blocked
			trace.doTrace( pml.origin, wishmins, wishmaxs, pml.origin, playerState.POVnum, pm.contentMask );
			if( trace.allSolid || trace.startSolid ) {
				// can't do the uncrouching, let the time alone and use old position
				SetSize( curmins, curmaxs );
				playerState.viewHeight = curviewheight;
				return;
			}

			// can do the uncrouching, use new position and update the time
			pmoveState.stats[PM_STAT_CROUCHTIME] = newcrouchtime;
			SetSize( wishmins, wishmaxs );
			playerState.viewHeight = wishviewheight;
			return;
		}

		// the player is not crouching at all
		SetSize( playerboxStandMins, playerboxStandMaxs );
		playerState.viewHeight = playerboxStandViewheight;
	}

	void AdjustViewheight() {
		float height;
		Vec3 pm_mins_, pm_maxs_, mins, maxs;
		auto @pmoveState = @playerState.pmove;

		pm_mins_ = pm_mins;
		pm_maxs_ = pm_maxs;
		if( pmoveState.pm_type == PM_SPECTATOR ) {
			pm_mins_ = playerboxStandMins;
			pm_maxs_ = playerboxStandMaxs;
		}

		GS::RoundUpToHullSize( pm_mins_, pm_maxs_, mins, maxs );

		height = pm_maxs_.z - maxs.z;
		if( height > 0.0f ) {
			playerState.viewHeight -= height;
		}
	}

	
	/*
	* CheckCrouchSlide
	*/
	void CheckCrouchSlide() {
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;

		if( ( pmoveState.stats[PM_STAT_FEATURES] & PMFEAT_CROUCHSLIDING ) == 0 ) {
			return;
		}

		if( pml.upPush < 0 && HorizontalSpeed() > pml.maxWalkSpeed ) {
			if( pmoveState.stats[PM_STAT_CROUCHSLIDETIME] > 0 ) {
				return; // cooldown or already sliding
			}

			if( pm.groundEntity != -1 ) {
				return; // already on the ground
			}

			// start sliding when we land
			pml.pm_flags |= PMF_CROUCH_SLIDING;
			pmoveState.stats[PM_STAT_CROUCHSLIDETIME] = PM_CROUCHSLIDE + PM_CROUCHSLIDE_FADE;
		} else if( ( pml.pm_flags & PMF_CROUCH_SLIDING ) != 0 ) {
			if( pmoveState.stats[PM_STAT_CROUCHSLIDETIME] > PM_CROUCHSLIDE_FADE ) {
				pmoveState.stats[PM_STAT_CROUCHSLIDETIME] = PM_CROUCHSLIDE_FADE;
			}
		}
	}

	void CheckSpecialMovement() {
		Vec3 spot;
		int cont;
		Trace trace;
		auto @pml = @this;
		auto @pmoveState = @playerState.pmove;
		Vec3 mins, maxs;

		pm.ladder = false;

		if( pmoveState.pm_time != 0 ) {
			return;
		}

		pml.ladder = false;

		// check for ladder
		if( !pm.skipCollision ) {
			spot = pml.origin + pml.flatforward;

			trace.doTrace( pml.origin, pm_mins, pm_maxs, spot, playerState.POVnum, pm.contentMask );
			if( ( trace.fraction < 1.0f ) && ( trace.surfFlags & SURF_LADDER ) != 0 ) {
				pml.ladder = true;
				pm.ladder = true;
			}
		}

		// check for water jump
		if( pm.waterLevel != 2 ) {
			return;
		}

		spot = pml.origin + 30.0f * pml.flatforward;
		spot.z += 4;
		cont = GS::PointContents( spot );
		if( ( cont & CONTENTS_SOLID ) == 0 ) {
			return;
		}

		spot.z += 16;
		cont = GS::PointContents( spot );
		if( cont != 0 ) {
			return;
		}

		// jump out of water
		pml.velocity = pml.flatforward * 50.0f;
		pml.velocity.z = 350.0f;

		pml.pm_flags |= PMF_TIME_WATERJUMP;
		pmoveState.pm_time = 255;
	}

	void FlyMove( bool doClip ) {
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

		if( ( pml.cmd.buttons & BUTTON_SPECIAL ) != 0 ) {
			maxspeed *= 2.0;
		}

		// friction
		speed = Speed();
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

		if( ( pml.cmd.buttons & BUTTON_SPECIAL ) != 0 ) {
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
			trace.doTrace( pml.origin, pm_mins, pm_maxs, end, playerState.POVnum, pm.contentMask );
			end = trace.endPos;
		}

		pml.origin = end;
	}

	void EndMove() {
		auto @pml = this;

		pm.origin = pml.origin;
		pm.velocity = pml.velocity;

		pml.playerState.pmove.origin = pml.origin;
		pml.playerState.pmove.velocity = pml.velocity;
		pml.playerState.pm_flags = pml.pm_flags;
	}
};

}