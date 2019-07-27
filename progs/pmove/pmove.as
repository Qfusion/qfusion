namespace PM {

const float DEFAULT_WALKSPEED = 160.0f;
const float DEFAULT_CROUCHEDSPEED = 100.0f;
const float DEFAULT_LADDERSPEED = 250.0f;

const float SPEEDKEY = 500.0f;

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

	PMove @pm;

	void BeginMove( PMove @pm ) {
		auto @pml = @this;
		auto @playerState = @pm.playerState;

		// clear results
		pm.numTouchEnts = 0;
		pm.groundEntity = -1;
		pm.waterType = 0;
		pm.waterLevel = 0;
		pm.step = 0.0f;

		@pml.pm = @pm;
		pml.origin = playerState.pmove.origin;
		pml.velocity = playerState.pmove.velocity;

		// save old org in case we get stuck
		pml.previousOrigin = pml.origin;

		playerState.viewAngles.angleVectors( pml.forward, pml.right, pml.up );

		pml.flatforward = pml.forward;
		pml.flatforward.z = 0.0f;
		pml.flatforward.normalize();

		pml.frametime = pm.cmd.msec * 0.001;

		pml.maxPlayerSpeed = pm.playerState.pmove.stats[PM_STAT_MAXSPEED];
		pml.jumpPlayerSpeed = float( pm.playerState.pmove.stats[PM_STAT_JUMPSPEED] ) * GRAVITY_COMPENSATE;
		pml.dashPlayerSpeed = pm.playerState.pmove.stats[PM_STAT_DASHSPEED];

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

	void DecPMoveStat( int stat ) {
		auto @pml = @this;
		PMove @pm = @this.pm;
		auto @playerState = @pm.playerState;
		int msec = pm.cmd.msec;

		int value = playerState.pmove.stats[stat];
		if( value > 0 ) {
			value -= msec;
		} else if( value < 0 ) {
			value = 0;
		}
		playerState.pmove.stats[stat] = value;
	}

	void PostBeginMove() {
		auto @pml = @this;
		PMove @pm = @this.pm;
		auto @playerState = @pm.playerState;

		if( ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED ) == 0 ) {
			int stat;

			// drop timing counters
			if( playerState.pmove.pm_time != 0 ) {
				int msec;

				msec = pm.cmd.msec / 8;
				if( msec < 0 ) {
					msec = 1;
				}

				if( msec >= playerState.pmove.pm_time ) {
					playerState.pmove.pm_flags = playerState.pmove.pm_flags & int( ~( PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT ) );
					playerState.pmove.pm_time = 0;
				} else {
					playerState.pmove.pm_time -= msec;
				}
			}

			DecPMoveStat( PM_STAT_NOUSERCONTROL ); 
			DecPMoveStat( PM_STAT_KNOCKBACK ); 

			// PM_STAT_CROUCHTIME is handled at PM_AdjustBBox
			// PM_STAT_ZOOMTIME is handled at PM_CheckZoom

			DecPMoveStat( PM_STAT_DASHTIME ); 
			DecPMoveStat( PM_STAT_WJTIME ); 
			DecPMoveStat( PM_STAT_NOAUTOATTACK ); 
			DecPMoveStat( PM_STAT_STUN ); 
			DecPMoveStat( PM_STAT_FWDTIME ); 
		}

		if( playerState.pmove.stats[PM_STAT_NOUSERCONTROL] > 0 ) {
			pml.forwardPush = 0.0f;
			pml.sidePush = 0.0f;
			pml.upPush = 0.0f;
			pm.cmd.buttons = 0;
		}
	}

	void FlyMove( bool doClip ) {
		int i;
		float speed, drop, friction, control, newspeed;
		float currentspeed, addspeed, accelspeed, maxspeed;
		Vec3 wishvel;
		float fmove, smove;
		Vec3 wishdir;
		float wishspeed;
		Vec3 end;
		Trace trace;
		auto @pml = @this;
		PMove @pm = @this.pm;

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

	void EndMove() {
		auto @pml = this;
		PMove @pm = @this.pm;
		auto @playerState = pm.playerState;

		playerState.pmove.origin = pml.origin;
		playerState.pmove.velocity = pml.velocity;
	}
};

void Init() {
}

void PMove( PMove @pm ) {
	PMoveLocal pml;

	pml.BeginMove( pm );

	pml.PostBeginMove();

	pml.FlyMove( false );

	pml.EndMove();
}

}