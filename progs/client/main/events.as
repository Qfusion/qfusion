namespace CGame {

//=========================================================

const int MAX_ANNOUNCER_EVENTS = 32;
const int MAX_ANNOUNCER_EVENTS_MASK = MAX_ANNOUNCER_EVENTS - 1;
const int ANNOUNCER_EVENTS_FRAMETIME = 1500; // the announcer will speak each 1.5 seconds

class AnnouncerEvent {
    SoundHandle@ sound;
}

array<AnnouncerEvent> cg_announcerEvents(MAX_ANNOUNCER_EVENTS);
int cg_announcerEventsCurrent = 0;
int cg_announcerEventsHead = 0;
int cg_announcerEventsDelay = 0;

/*
 * ClearAnnouncerEvents
 */
void ClearAnnouncerEvents() {
    cg_announcerEventsCurrent = 0;
    cg_announcerEventsHead = 0;
}

/*
 * AddAnnouncerEvent
 */
void AddAnnouncerEvent(SoundHandle@ sound, bool queued) {
    if (sound is null) {
        return;
    }

    if (!queued) {
        Sound::StartLocalSound(sound, CHAN_ANNOUNCER, cg_volume_announcer.value);
        cg_announcerEventsDelay = ANNOUNCER_EVENTS_FRAMETIME; // wait
        return;
    }

    // full buffer (we do nothing, just let it overwrite the oldest)
    if (cg_announcerEventsCurrent + MAX_ANNOUNCER_EVENTS >= cg_announcerEventsHead) {
        // do nothing
    }

    // add it
    @cg_announcerEvents[cg_announcerEventsHead & MAX_ANNOUNCER_EVENTS_MASK].sound = @sound;
    cg_announcerEventsHead++;
}

/*
 * ReleaseAnnouncerEvents
 */
void ReleaseAnnouncerEvents() {
    // see if enough time has passed
    cg_announcerEventsDelay -= cg.realFrameTime;
    if (cg_announcerEventsDelay > 0) {
        return;
    }

    if (cg_announcerEventsCurrent < cg_announcerEventsHead) {
        auto @sound = cg_announcerEvents[cg_announcerEventsCurrent & MAX_ANNOUNCER_EVENTS_MASK].sound;
        if (sound !is null) {
            Sound::StartLocalSound(sound, CHAN_ANNOUNCER, cg_volume_announcer.value);
            cg_announcerEventsDelay = ANNOUNCER_EVENTS_FRAMETIME; // wait
        }
        cg_announcerEventsCurrent++;
    } else {
        cg_announcerEventsDelay = 0; // no wait
    }
}

//=========================================================

void StartVoiceTokenEffect( int entNum, int type, int vsay ) {
	if( !cg_voiceChats.boolean || cg_volume_voicechats.value <= 0.0f ) {
		return;
	}
	if( vsay < 0 || vsay >= VSAY_TOTAL ) {
		return;
	}

	CEntity @cent = @cgEnts[entNum];

	// ignore repeated/flooded events
	if( cent.localEffects[LEF_VSAY_HEADICON_TIMEOUT] > cg.time ) {
		return;
	}

	// set the icon effect
	cent.localEffects[LEF_VSAY_HEADICON] = vsay;
	cent.localEffects[LEF_VSAY_HEADICON_TIMEOUT] = cg.time + HEADICON_TIMEOUT;

	// play the sound
	SoundHandle @sound = @cgs.media.sfxVSaySounds[vsay];
	if( @sound is null ) {
		return;
	}

	// played as it was made by the 1st person player
	Sound::StartLocalSound( sound, CHAN_AUTO, cg_volume_voicechats.value );
}

void FireWeaponEvent( int entNum, int weapon, int fireMode )
{
	if( weapon <= WEAP_NONE || weapon >= WEAP_TOTAL ) {
		return;
	}

	// hack idle attenuation on the plasmagun to reduce sound flood on the scene
	float attenuation = ATTN_NORM;
	if( weapon == WEAP_PLASMAGUN ) {
		attenuation = ATTN_IDLE;
	}

	bool viewer = IsViewerEntity( entNum );
	CEntity @cent = @cgEnts[entNum];
	PModel @pmodel = @cent.pmodel;
	SoundHandle @sound = null;
	WModelInfo @weaponInfo = @cgs.weaponModelInfo[weapon];
	if( @weaponInfo is null )  {
		return;
	}

	// sound
	if( fireMode == FIRE_MODE_STRONG ) {
		if( weaponInfo.strongFireSounds.length() != 0 ) {
			@sound = @weaponInfo.strongFireSounds[int( brandom( 0, weaponInfo.strongFireSounds.length() ) )];
		}
	} else {
		if( weaponInfo.fireSounds.length() != 0 ) {
			@sound = @weaponInfo.fireSounds[int( brandom( 0, weaponInfo.fireSounds.length() ) )];
		}
	}

	if( @sound !is null ) {
		if( viewer ) {
			Sound::StartGlobalSound( sound, CHAN_MUZZLEFLASH, cg_volume_effects.value );
		} else {
			// fixed position is better for location, but the channels are used from worldspawn
			// and openal runs out of channels quick on cheap cards. Relative sound uses per-entity channels.
			Sound::StartRelativeSound( sound, entNum, CHAN_MUZZLEFLASH, cg_volume_effects.value, attenuation );
		}

		if( ( cgEnts[entNum].current.effects & EF_QUAD ) != 0 && ( weapon != WEAP_LASERGUN ) ) {
			SoundHandle @quadSfx = @cgs.media.sfxQuadFireSound;
			if( viewer ) {
				Sound::StartGlobalSound( quadSfx, CHAN_AUTO, cg_volume_effects.value );
			} else {
				Sound::StartRelativeSound( quadSfx, entNum, CHAN_AUTO, cg_volume_effects.value, attenuation );
			}
		}
	}

	// flash and barrel effects

	if( weapon == WEAP_GUNBLADE ) { // gunblade is special
		if( fireMode == FIRE_MODE_STRONG ) {
			// light flash
			if( cg_weaponFlashes.boolean && weaponInfo.flashTime != 0 ) {
				pmodel.flashTime = cg.time + weaponInfo.flashTime;
			}
		} else {
			// start barrel rotation or offsetting
			if( weaponInfo.barrelTime != 0 ) {
				pmodel.barrelTime = cg.time + weaponInfo.barrelTime;
			}
		}
	} else {
		// light flash
		if( cg_weaponFlashes.boolean && weaponInfo.flashTime != 0 ) {
			pmodel.flashTime = cg.time + weaponInfo.flashTime;
		}

		// start barrel rotation or offsetting
		if( weaponInfo.barrelTime != 0 ) {
			pmodel.barrelTime = cg.time + weaponInfo.barrelTime;
		}
	}

	// add animation to the player model
	int anim = 0;
	switch( weapon ) {
		case WEAP_GUNBLADE:
			if( fireMode == FIRE_MODE_WEAK ) {
				anim = GS::Anim::Anim::TORSO_SHOOT_BLADE;
			} else {
				anim = GS::Anim::Anim::TORSO_SHOOT_PISTOL;
			}
			break;

		case WEAP_LASERGUN:
			anim = GS::Anim::Anim::TORSO_SHOOT_PISTOL;
			break;

		case WEAP_ROCKETLAUNCHER:
		case WEAP_GRENADELAUNCHER:
			anim = GS::Anim::Anim::TORSO_SHOOT_HEAVYWEAPON;
			break;

		case WEAP_ELECTROBOLT:
			anim = GS::Anim::Anim::TORSO_SHOOT_AIMWEAPON;
			break;

		case WEAP_RIOTGUN:
		case WEAP_PLASMAGUN:
		default:
			anim = GS::Anim::Anim::TORSO_SHOOT_LIGHTWEAPON;
			break;
	}

	if( anim != 0 ) {
		pmodel.AddAnimation( {0, anim, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	}

	// add animation to the view weapon model
	if( viewer && !Camera::GetMainCamera().thirdPerson ) {
		cg.vweapon.StartAnimationEvent( fireMode == FIRE_MODE_STRONG ? WEAPANIM_ATTACK_STRONG : WEAPANIM_ATTACK_WEAK );
	}
}

void Event_Fall( const EntityState @state, int parm ) {
	if( IsViewerEntity( state.number ) ) {
		if( PredictedPlayerState.pmove.pm_type != PM_NORMAL ) {
			SexedSound( state.number, CHAN_AUTO, "*fall_0", cg_volume_players.value, state.attenuation );
			return;
		}

		StartFallKickEffect( ( parm + 5 ) * 10 );

		if( parm >= 15 ) {
			DamageIndicatorAdd( parm, Vec3( 0, 0, 1 ) );
		}
	}

	if( parm > 10 ) {
		int anim;
		switch( rand() % 3 ) {
			case 0:
				anim = GS::Anim::Anim::TORSO_PAIN1;
				break;
			case 1:
				anim = GS::Anim::Anim::TORSO_PAIN2;
				break;
			case 2:
			default:
				anim = GS::Anim::Anim::TORSO_PAIN3;
				break;
		}

		PModel @pmodel = @cgEnts[state.number].pmodel;	
		pmodel.AddAnimation( {0, anim, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	}

	if( parm > 10 ) {
		SexedSound( state.number, CHAN_PAIN, "*fall_2", cg_volume_players.value, state.attenuation );
	} else if( parm > 0 ) {
		SexedSound( state.number, CHAN_PAIN, "*fall_1", cg_volume_players.value, state.attenuation );
	} else {
		SexedSound( state.number, CHAN_PAIN, "*fall_0", cg_volume_players.value, state.attenuation );
	}

	// smoke effect
	if( parm > 0 && ( cg_cartoonEffects.integer & 2 ) != 0 ) {
		Vec3 start, end;

		if( IsViewerEntity( state.number ) ) {
			start = PredictedPlayerState.pmove.origin;
		} else {
			start = state.origin;
		}
		end = start + Vec3( 0, 0, playerboxStandMins[2] - 48.0f );

		Trace trace;
		trace.doTrace( start, vec3Origin, vec3Origin, end, state.number, MASK_PLAYERSOLID );

		if( trace.entNum == -1 ) {
			start.z += playerboxStandMins.z + 8;
			LE::DustCircle( start, Vec3( 0, 0, 1 ), 50, 12 );
		} else if( ( trace.surfFlags & SURF_NODAMAGE ) == 0 ) {
			end = trace.endPos + 8 * trace.planeNormal;
			LE::DustCircle( end, trace.planeNormal, 50, 12 );
		}
	}
}

void Event_Pain( const EntityState @state, int parm )
{
	if( parm == PAIN_WARSHELL ) {
		if( IsViewerEntity( state.number ) ) {
			Sound::StartGlobalSound( cgs.media.sfxShellHit, CHAN_PAIN,
									 cg_volume_players.value );
		} else {
			Sound::StartRelativeSound( cgs.media.sfxShellHit, state.number, CHAN_PAIN,
									   cg_volume_players.value, state.attenuation );
		}
	} else {
		SexedSound( state.number, CHAN_PAIN, StringUtils::Format( S_PLAYER_PAINS, 25 * ( parm + 1 ) ),
					   cg_volume_players.value, state.attenuation );
	}

	int anim = 0;
	switch( int( brandom( 0, 3 ) ) ) {
		case 0:
			anim = GS::Anim::Anim::TORSO_PAIN1;
			break;
		case 1:
			anim = GS::Anim::Anim::TORSO_PAIN2;
			break;
		case 2:
		default:
			anim = GS::Anim::Anim::TORSO_PAIN3;
			break;
	}

	if( anim == 0 ) {
		return;
	}

	PModel @pmodel = @cgEnts[state.number].pmodel;
	pmodel.AddAnimation( {0, anim, 0}, GS::Anim::Channel::EVENT_CHANNEL );
}

void Event_Die( const EntityState @state, int parm )
{
	SexedSound( state.number, CHAN_PAIN, S_PLAYER_DEATH, cg_volume_players.value, state.attenuation );

	int anim = 0;
	switch( parm ) {
		case 1:
			anim = GS::Anim::Anim::BOTH_DEATH2;
			break;
		case 2:
			anim = GS::Anim::Anim::BOTH_DEATH3;
			break;
		case 0:
		default:
			anim = GS::Anim::Anim::BOTH_DEATH3;
			break;
	}

	if( anim == 0 ) {
		return;
	}

	PModel @pmodel = @cgEnts[state.number].pmodel;
	pmodel.AddAnimation( {anim, anim, 0}, GS::Anim::Channel::EVENT_CHANNEL );
}

void Event_WallJump( const EntityState @state, int parm, int ev )
{
	Vec3 normal = GS::ByteToDir( parm );

	Vec3 f, r, u;
	Vec3( state.angles.x, state.angles.y, 0.0f ).angleVectors( f, r, u );

	PModel @pmodel = @cgEnts[state.number].pmodel;
	if( normal * r > 0.3 ) {
		pmodel.AddAnimation( {GS::Anim::Anim::LEGS_WALLJUMP_RIGHT, 0, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	} else if( -( normal * r ) > 0.3 ) {
		pmodel.AddAnimation( {GS::Anim::Anim::LEGS_WALLJUMP_LEFT, 0, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	} else if( -( normal * f ) > 0.3 ) {
		pmodel.AddAnimation( {GS::Anim::Anim::LEGS_WALLJUMP_BACK, 0, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	} else {
		pmodel.AddAnimation( {GS::Anim::Anim::LEGS_WALLJUMP, 0, 0}, GS::Anim::Channel::EVENT_CHANNEL );
	}

	if( ev == EV_WALLJUMP_FAILED ) {
		if( IsViewerEntity( state.number ) ) {
			Sound::StartGlobalSound( @cgs.media.sfxWalljumpFailed,
				CHAN_BODY, cg_volume_effects.value );
		} else {
			Sound::StartRelativeSound( @cgs.media.sfxWalljumpFailed, state.number,
				CHAN_BODY, cg_volume_effects.value, ATTN_NORM );
		}
	} else {
		SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_WALLJUMP_1_to_2, ( rand() & 1 ) + 1 ),
					   cg_volume_players.value, state.attenuation );

		// smoke effect
		if( ( cg_cartoonEffects.integer & 1 ) != 0 ) {
			Vec3 pos = state.origin;
			pos.z += 15;
			LE::DustCircle( pos, normal, 65.0f, 12 );
		}
	}
}

void Event_DoubleJump( const EntityState @state, int parm ) {
	SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
		cg_volume_players.value, state.attenuation );
}

void Event_Jump( const EntityState @state, int parm ) {
	const float MOVEDIREPSILON = 0.25f;
	auto @cent = @cgEnts[state.number];
	PModel @pmodel = @cent.pmodel;

	float xyspeedcheck = Vec3( cent.animVelocity[0], cent.animVelocity[1], 0.0f ).length();

	if( xyspeedcheck < 100 ) { // the player is jumping on the same place, not running
		pmodel.AddAnimation( { GS::Anim::Anim::LEGS_JUMP_NEUTRAL, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
		SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
					   cg_volume_players.value, state.attenuation );
	} else {
		Vec3 movedir;
		Mat3 viewaxis;

		movedir[0] = cent.animVelocity[0];
		movedir[1] = cent.animVelocity[1];
		movedir[2] = 0;
		movedir.normalize();

		Vec3( 0.0f, state.angles[YAW], 0.0f ).anglesToAxis( viewaxis );

		// see what's his relative movement direction
		if( movedir * viewaxis.x > MOVEDIREPSILON ) {
			cent.jumpedLeft = !cent.jumpedLeft;

			if( !cent.jumpedLeft ) {
				pmodel.AddAnimation( { GS::Anim::Anim::LEGS_JUMP_LEG2, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
				SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
							   cg_volume_players.value, state.attenuation );
			} else {
				pmodel.AddAnimation( { GS::Anim::Anim::LEGS_JUMP_LEG1, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
				SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
							   cg_volume_players.value, state.attenuation );
			}
		} else {
			pmodel.AddAnimation( { GS::Anim::Anim::LEGS_JUMP_NEUTRAL, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, state.attenuation );
		}
	}
}

void Event_Dash( const EntityState @state, int parm ) {
	auto @cent = @cgEnts[state.number];
	PModel @pmodel = @cent.pmodel;

	switch( parm ) {
		case 0: // dash front
			pmodel.AddAnimation( { GS::Anim::Anim::LEGS_DASH, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_DASH_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, state.attenuation );
			break;
		case 1: // dash left
			pmodel.AddAnimation( { GS::Anim::Anim::LEGS_DASH_LEFT, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_DASH_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, state.attenuation );
			break;
		case 2: // dash right
			pmodel.AddAnimation( { GS::Anim::Anim::LEGS_DASH_RIGHT, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_DASH_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, state.attenuation );
			break;
		case 3: // dash back
			pmodel.AddAnimation( { GS::Anim::Anim::LEGS_DASH_BACK, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_DASH_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, state.attenuation );
			break;
		default:
			break;			
	}

	LE::DashEffect( @cent ); // Dash smoke effect

	// since most dash animations jump with right leg, reset the jump to start with left leg after a dash
	cent.jumpedLeft = true;
}

void Event_Mover( const EntityState @state, int parm )
{
	Vec3 org, vel;
	GetEntitySpatilization( state.number, org, vel );
	Sound::StartFixedSound( cgs.soundPrecache[parm], org, CHAN_AUTO, cg_volume_effects.value, ATTN_STATIC );
}

bool EntityEvent( const EntityState @ent, int ev, int parm, bool predicted )
{
	CEntity @cent = @cgEnts[ent.number];
	bool viewer = IsViewerEntity( ent.number );
	auto @cam = Camera::GetMainCamera();
	int weapon, fireMode;
	Vec3 dir;
	Vec3 color;
	int count;
	Vec3 fv, rv, uv;
	auto @pps = @PredictedPlayerState;

	if( !cg_test.boolean ) {
		return false;
	}
	if( viewer && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
		return true;
	}

	switch( ev ) {
		case EV_NONE:
			break;
	
		case EV_WEAPONACTIVATE:
			cent.pmodel.AddAnimation( {0, GS::Anim::Anim::TORSO_WEAPON_SWITCHIN, 0}, GS::Anim::Channel::EVENT_CHANNEL );

			weapon = ( parm >> 1 ) & 0x3f;
			fireMode = ( parm & 0x1 ) != 0 ? FIRE_MODE_STRONG : FIRE_MODE_WEAK;
			if( predicted ) {
				cent.current.weapon = weapon;
				if( fireMode == FIRE_MODE_STRONG ) {
					cent.current.effects |= EF_STRONG_WEAPON;
				}

				cg.vweapon.RefreshAnimation();
			}

			if( viewer ) {
				cg.predictedWeaponSwitch = 0;
			}

			// reset weapon animation timers
			cent.pmodel.flashTime = 0;
			cent.pmodel.barrelTime = 0;

			if( viewer ) {
				Sound::StartGlobalSound( @cgs.media.sfxWeaponUp, CHAN_AUTO, cg_volume_effects.value );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxWeaponUp, ent.origin, CHAN_AUTO, cg_volume_effects.value, ATTN_NORM );
			}
			return true;

		case EV_SMOOTHREFIREWEAPON: // the server never sends this event
			if( predicted ) {
				weapon = ( parm >> 1 ) & 0x3f;
				fireMode = ( parm & 0x1 ) != 0 ? FIRE_MODE_STRONG : FIRE_MODE_WEAK;

				cgEnts[ent.number].current.weapon = weapon;
				if( fireMode == FIRE_MODE_STRONG ) {
					cgEnts[ent.number].current.effects |= EF_STRONG_WEAPON;
				}

				cg.vweapon.RefreshAnimation();

				if( weapon == WEAP_LASERGUN ) {
					Event_LaserBeam( ent.number, weapon, fireMode );
				}
			}
			return true;

		case EV_FIREWEAPON:
			weapon = ( parm >> 1 ) & 0x3f;
			fireMode = ( parm & 0x1 ) != 0 ? FIRE_MODE_STRONG : FIRE_MODE_WEAK;

			if( predicted ) {
				cgEnts[ent.number].current.weapon = weapon;
				if( fireMode == FIRE_MODE_STRONG ) {
					cgEnts[ent.number].current.effects |= EF_STRONG_WEAPON;
				}
			}

			FireWeaponEvent( ent.number, weapon, fireMode );

			// riotgun bullets, electrobolt and instagun beams are predicted when the weapon is fired
			if( predicted ) {
				Vec3 origin;

				if( ( weapon == WEAP_ELECTROBOLT
					  && fireMode == FIRE_MODE_STRONG
					  )
					|| weapon == WEAP_INSTAGUN ) {
					origin = pps.pmove.origin;
					origin.z += pps.viewHeight;
					fv = pps.viewAngles.anglesToForward();
					Event_WeaponBeam( origin, fv, pps.POVnum, weapon, fireMode );
				} else if( weapon == WEAP_RIOTGUN || weapon == WEAP_MACHINEGUN ) {
					int seed = cg.predictedEventTimes[EV_FIREWEAPON] & 255;

					origin = pps.pmove.origin;
					origin.z += pps.viewHeight;
					pps.viewAngles.angleVectors( fv, rv, uv );

					if( weapon == WEAP_RIOTGUN ) {
						Event_FireRiotgun( origin, fv, rv, uv, weapon, fireMode, seed, pps.POVnum );
					} else {
						Event_FireMachinegun( origin, fv, rv, uv, weapon, fireMode, seed, pps.POVnum );
					}
				} else if( weapon == WEAP_LASERGUN ) {
					Event_LaserBeam( ent.number, weapon, fireMode );
				}
			}
			return true;

		case EV_ELECTROTRAIL:
			// check the owner for predicted case
			if( IsViewerEntity( parm ) && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
				return true;
			}
			Event_WeaponBeam( ent.origin, ent.origin2, parm, WEAP_ELECTROBOLT, ent.fireMode );
			return true;

		case EV_INSTATRAIL:
			// check the owner for predicted case
			if( IsViewerEntity( parm ) && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
				return true;
			}
			Event_WeaponBeam( ent.origin, ent.origin2, parm, WEAP_INSTAGUN, FIRE_MODE_STRONG );
			return true;

		case EV_FIRE_RIOTGUN:

			// check the owner for predicted case
			if( IsViewerEntity( ent.ownerNum ) && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
				return true;
			}
			
			fv = ent.origin2;
			rv = ent.origin3;
			uv = rv ^ fv;
			Event_FireRiotgun( ent.origin, fv, rv, uv, ent.weapon, ent.fireMode, parm, ent.ownerNum );
			return true;

		case EV_FIRE_BULLET:

			// check the owner for predicted case
			if( IsViewerEntity( ent.ownerNum ) && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
				return true;
			}

			fv = ent.origin2;
			rv = ent.origin3;
			uv = rv ^ fv;
			Event_FireMachinegun( ent.origin, fv, rv, uv, ent.weapon, ent.fireMode, parm, ent.ownerNum );
			return true;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				Sound::StartGlobalSound( @cgs.media.sfxWeaponUpNoAmmo, CHAN_ITEM, cg_volume_effects.value );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxWeaponUpNoAmmo, ent.origin, CHAN_ITEM, cg_volume_effects.value, ATTN_IDLE );
			}
			return true;

		case EV_DASH:
			Event_Dash( @ent, parm );
			return true;

		case EV_WALLJUMP:
		case EV_WALLJUMP_FAILED:
			Event_WallJump( @ent, parm, ev );
			return true;

		case EV_DOUBLEJUMP:
			Event_DoubleJump( @ent, parm );
			return true;

		case EV_JUMP:
			Event_Jump( @ent, parm );
			return true;

		case EV_JUMP_PAD:
			SexedSound( ent.number, CHAN_BODY, StringUtils::Format( S_PLAYER_JUMP_1_to_2, ( rand() & 1 ) + 1 ),
						   cg_volume_players.value, ent.attenuation );
			cent.pmodel.AddAnimation( { GS::Anim::LEGS_JUMP_NEUTRAL, 0, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			return true;

		case EV_FALL:
			Event_Fall( ent, parm );
			break;

		//  NON PREDICTABLE EVENTS

		case EV_ITEM_RESPAWN:
			cgEnts[ent.number].respawnTime = cg.time;
			Sound::StartRelativeSound( @cgs.media.sfxItemRespawn, ent.number, CHAN_AUTO, 
				cg_volume_effects.value, ATTN_IDLE );
			return true;

		case EV_PLAYER_RESPAWN:
			if( uint( ent.ownerNum ) == cgs.playerNum + 1 ) {
				cg.vweapon.ResetKickAngles();
				ResetColorBlend();
				ResetDamageIndicator();
			}

			if( IsViewerEntity( ent.ownerNum ) ) {
				Sound::StartGlobalSound( @cgs.media.sfxPlayerRespawn, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxPlayerRespawn, ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			}

			if( ent.ownerNum != 0 && ent.ownerNum < GS::maxClients + 1 ) {
				auto @ce = @cgEnts[ent.ownerNum];
				ce.localEffects[LEF_EV_PLAYER_TELEPORT_IN] = cg.time;
				ce.teleportedTo = ent.origin;
			}
			return true;

		case EV_PLAYER_TELEPORT_IN:
			if( IsViewerEntity( ent.ownerNum ) ) {
				Sound::StartGlobalSound( @cgs.media.sfxTeleportIn, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxTeleportIn, ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			}

			if( ent.ownerNum != 0 && ent.ownerNum < GS::maxClients + 1 ) {
				auto @ce = @cgEnts[ent.ownerNum];
				ce.localEffects[LEF_EV_PLAYER_TELEPORT_IN] = cg.time;
				ce.teleportedTo = ent.origin;
			}
			return true;

		case EV_PLAYER_TELEPORT_OUT:
			if( IsViewerEntity( ent.ownerNum ) ) {
				Sound::StartGlobalSound( @cgs.media.sfxTeleportOut, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxTeleportOut, ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			}

			if( ent.ownerNum != 0 && ent.ownerNum < GS::maxClients + 1 ) {
				auto @ce = @cgEnts[ent.ownerNum];
				ce.localEffects[LEF_EV_PLAYER_TELEPORT_OUT] = cg.time;
				ce.teleportedFrom = ent.origin;
			}
			return true;

		case EV_VSAY:
			StartVoiceTokenEffect( ent.ownerNum, EV_VSAY, parm );
			return true;

		case EV_WEAPONDROP: // deactivate is not predictable
			cent.pmodel.AddAnimation( { 0, GS::Anim::TORSO_WEAPON_SWITCHOUT, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			return true;

		case EV_SEXEDSOUND:
			if( parm == 2 ) {
				SexedSound( ent.number, CHAN_AUTO, S_PLAYER_GASP, cg_volume_players.value, ent.attenuation );
			} else if( parm == 1 ) {
				SexedSound( ent.number, CHAN_AUTO, S_PLAYER_DROWN, cg_volume_players.value, ent.attenuation );
			}
			return true;

		case EV_PAIN:
			Event_Pain( @ent, parm );
			return true;

		case EV_DIE:
			Event_Die( @ent, parm );
			return true;

		case EV_GIB:
			return true;

		case EV_GESTURE:
			SexedSound( ent.number, CHAN_BODY, "*taunt", cg_volume_players.value, ent.attenuation );
			return true;

		case EV_DROP:
			cent.pmodel.AddAnimation( { 0, GS::Anim::TORSO_DROP, 0 }, GS::Anim::Channel::EVENT_CHANNEL );
			return true;

		case EV_GRENADE_BOUNCE:
			if( parm == FIRE_MODE_STRONG ) {
				Sound::StartRelativeSound( @cgs.media.sfxGrenadeStrongBounce[rand() & 1], ent.number, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
			} else {
				Sound::StartRelativeSound( @cgs.media.sfxGrenadeWeakBounce[rand() & 1], ent.number, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
			}
			return true;

		case EV_GREEN_LASER:
			LE::GreenLaser( ent.origin, ent.origin2 );
			return true;

		case EV_SPARKS:
			dir = GS::ByteToDir( parm );
			if( ent.damage > 0 ) {
				count = bound( 1, int( ent.damage * 0.25f ), 10 );
			} else {
				count = 6;
			}

			SplashParticles( ent.origin, dir, 1.0f, 0.67f, 0.0f, count );
			return true;

		case EV_BULLET_SPARKS:
			dir = GS::ByteToDir( parm );
			LE::BulletExplosion( ent.origin, dir );
			SplashParticles( ent.origin, dir, 1.0f, 0.67f, 0.0f, 6 );
			Sound::StartFixedSound( @cgs.media.sfxRic[rand() % 2], ent.origin, CHAN_AUTO,
									cg_volume_effects.value, ATTN_STATIC );
			return true;

		case EV_LASER_SPARKS:
			dir = GS::ByteToDir( parm );
			color = ColorToVec3( ent.colorRGBA );
			SplashParticles2( ent.origin, dir, color.x, color.y, color.z, ent.counterNum );
			return true;

		case EV_BLADE_IMPACT:
			LE::BladeImpact( ent.origin, ent.origin2 );
			return true;

		case EV_BLOOD:
			dir = GS::ByteToDir( parm );
			LE::BloodDamageEffect( ent.origin, dir, ent.damage );
			return true;

		case EV_EXPLOSION1:
			LE::GenericExplosion( ent.origin, vec3Origin, FIRE_MODE_WEAK, parm * 8, false );
			return true;

		case EV_EXPLOSION2:
			LE::GenericExplosion( ent.origin, vec3Origin, FIRE_MODE_STRONG, parm * 16, false );
			return true;

		case EV_SPOG:
			LE::SmallPileOfGibs( ent.origin, parm, ent.origin2, ent.team );
			return true;

		case EV_PLASMA_EXPLOSION:
			dir = GS::ByteToDir( parm );
			LE::PlasmaExplosion( ent.origin, dir, ent.fireMode, float( ent.weapon ) * 8.0f );
			if( ent.fireMode == FIRE_MODE_STRONG ) {
				Sound::StartFixedSound( @cgs.media.sfxPlasmaStrongHit, ent.origin, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
				cg.vweapon.StartKickAnglesEffect( ent.origin, 50, ent.weapon * 8, 100 );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxPlasmaWeakHit, ent.origin, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
				cg.vweapon.StartKickAnglesEffect( ent.origin, 30, ent.weapon * 8, 75 );
			}
			return true;

		case EV_BOLT_EXPLOSION:
			dir = GS::ByteToDir( parm );
			LE::BoltExplosionMode( ent.origin, dir, ent.fireMode, 0 );
			return true;

		case EV_INSTA_EXPLOSION:
			dir = GS::ByteToDir( parm );
			LE::InstaExplosionMode( ent.origin, dir, ent.fireMode, 0, ent.ownerNum );
			return true;

		case EV_GRENADE_EXPLOSION:
			if( parm != 0 ) {
				// we have a direction
				dir = GS::ByteToDir( parm );
				LE::GrenadeExplosionMode( ent.origin, dir, ent.fireMode, float( ent.weapon ) * 8.0f );
			} else {
				// no direction
				LE::GrenadeExplosionMode( ent.origin, vec3Origin, ent.fireMode, float( ent.weapon ) * 8.0f );
			}

			if( ent.fireMode == FIRE_MODE_STRONG ) {
				cg.vweapon.StartKickAnglesEffect( ent.origin, 135, ent.weapon * 8, 325 );
			} else {
				cg.vweapon.StartKickAnglesEffect( ent.origin, 125, ent.weapon * 8, 300 );
			}
			return true;

		case EV_ROCKET_EXPLOSION:
			dir = GS::ByteToDir( parm );
			LE::RocketExplosionMode( ent.origin, dir, ent.fireMode, float( ent.weapon ) * 8.0f );

			if( ent.fireMode == FIRE_MODE_STRONG ) {
				cg.vweapon.StartKickAnglesEffect( ent.origin, 135, ent.weapon * 8, 300 );
			} else {
				cg.vweapon.StartKickAnglesEffect( ent.origin, 125, ent.weapon * 8, 275 );
			}
			return true;
			
		case EV_GUNBLADEBLAST_IMPACT:
			dir = GS::ByteToDir( parm );
			LE::GunBladeBlastImpact( ent.origin, dir, float( ent.weapon ) * 8 );
			if( ent.skinNum > 64 ) {
				Sound::StartFixedSound( @cgs.media.sfxGunbladeStrongHit[2], ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_DISTANT );
			} else if( ent.skinNum > 34 ) {
				Sound::StartFixedSound( @cgs.media.sfxGunbladeStrongHit[1], ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			} else {
				Sound::StartFixedSound( @cgs.media.sfxGunbladeStrongHit[0], ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_IDLE );
			}

			//ent.skinnum is knockback value
			cg.vweapon.StartKickAnglesEffect( ent.origin, ent.skinNum * 8, ent.weapon * 8, 200 );
			break;

		case EV_PNODE:
			Scene::SpawnPolyBeam( ent.origin, ent.origin2, ColorToVec4( ent.colorRGBA ), 4, 2000.0f, 0.0f, @cgs.media.shaderLaser, 64, 0 );
			break;

		case EV_PLAT_HIT_TOP:
		case EV_PLAT_HIT_BOTTOM:
		case EV_PLAT_START_MOVING:
		case EV_DOOR_HIT_TOP:
		case EV_DOOR_HIT_BOTTOM:
		case EV_DOOR_START_MOVING:
		case EV_BUTTON_FIRE:
		case EV_TRAIN_STOP:
		case EV_TRAIN_START:
			Event_Mover( @ent, parm );
			break;

		default:
			break;
	}

	return false;
}

/*
* FirePlayerStateEvents
* These events are only received by this client, and only affect it.
*/
void FirePlayerStateEvents() {
    uint event, parm, i, count;
    Vec3 dir;
	auto @cam = Camera::GetMainCamera();
	auto @ps = @Snap.playerState;
	auto @pps = @PredictedPlayerState;

    if (cam.POVent != int(ps.POVnum)) {
        return;
    }

    for (count = 0; count < 2; count++) {
        // first byte is event number, second is parm
        event = ps.events[count] & 127;
        parm = ps.eventParms[count] & 0xFF;

        switch (event) {
            case PSEV_HIT:
                if (parm > 6) {
                    break;
                }
                if (parm < 4) { // hit of some caliber
                    Sound::StartLocalSound(@cgs.media.sfxWeaponHit[parm], CHAN_AUTO, cg_volume_hitsound.value);
					// TODO
                    //ScreenCrosshairDamageUpdate();
                } else if (parm == 4) {  // killed an enemy
                    Sound::StartLocalSound(@cgs.media.sfxWeaponKill, CHAN_AUTO, cg_volume_hitsound.value);
					// TODO
                    //ScreenCrosshairDamageUpdate();
                } else {  // hit a teammate
                    Sound::StartLocalSound(@cgs.media.sfxWeaponHitTeam, CHAN_AUTO, cg_volume_hitsound.value);
                    if (cg_showhelp.boolean) {
                        if (random() <= 0.5f) {
                            CenterPrint("Don't shoot at members of your team!");
                        } else {
                            CenterPrint("You are shooting at your team-mates!");
                        }
                    }
                }
                break;

            case PSEV_PICKUP:
                if (cg_pickup_flash.boolean && !cam.thirdPerson) {
                    StartColorBlendEffect(1.0f, 1.0f, 1.0f, 0.25f, 150);
                }

                // auto-switch
                if (cg_weaponAutoSwitch.boolean && (parm > WEAP_NONE && parm < WEAP_TOTAL)) {
                    if (!cgs.demoPlaying && pps.pmove.pm_type == PM_NORMAL
                        && pps.POVnum == cgs.playerNum + 1) {
                        // auto-switch only works when the user didn't have the just-picked weapon
                        if (OldSnap.playerState.inventory[parm] == 0) {
                            // switch when player's only weapon is gunblade
                            if (cg_weaponAutoSwitch.integer == 2) {
                                for (i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++) {
                                    if (i == parm) {
                                        continue;
                                    }
                                    if (pps.inventory[i] != 0) {
                                        break;
                                    }
                                }

                                if (i == WEAP_TOTAL) { // didn't have any weapon
									// TODO
                                    //UseItem(String(parm));
                                }

                            }
                            // switch when the new weapon improves player's selected weapon
                            else if (cg_weaponAutoSwitch.integer == 1) {
                                uint best = WEAP_GUNBLADE;
                                for (i = WEAP_GUNBLADE + 1; i < WEAP_TOTAL; i++) {
                                    if (i == parm) {
                                        continue;
                                    }
                                    if (pps.inventory[i] != 0) {
                                        best = i;
                                    }
                                }

                                if (best < parm) {
									// TODO
                                    //UseItem(String(parm));
                                }
                            }
                        }
                    }
                }
                break;

            case PSEV_DAMAGE_20:
                dir = GS::ByteToDir(parm);
                DamageIndicatorAdd(20, dir);
                break;

            case PSEV_DAMAGE_40:
                dir = GS::ByteToDir(parm);
                DamageIndicatorAdd(40, dir);
                break;

            case PSEV_DAMAGE_60:
                dir = GS::ByteToDir(parm);
                DamageIndicatorAdd(60, dir);
                break;

            case PSEV_DAMAGE_80:
                dir = GS::ByteToDir(parm);
                DamageIndicatorAdd(80, dir);
                break;

            case PSEV_INDEXEDSOUND:
                if (@cgs.soundPrecache[parm] !is null) {
                    Sound::StartGlobalSound(@cgs.soundPrecache[parm], CHAN_AUTO, cg_volume_effects.value);
                }
                break;

            case PSEV_ANNOUNCER:
                AddAnnouncerEvent(@cgs.soundPrecache[parm], false);
                break;

            case PSEV_ANNOUNCER_QUEUED:
                AddAnnouncerEvent(@cgs.soundPrecache[parm], true);
                break;

            default:
                break;
        }
    }
}

}