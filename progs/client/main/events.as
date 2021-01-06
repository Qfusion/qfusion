namespace CGame {

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
	CGame::Sound::StartLocalSound( sound, CHAN_AUTO, cg_volume_voicechats.value );
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
			CGame::Sound::StartGlobalSound( sound, CHAN_MUZZLEFLASH, cg_volume_effects.value );
		} else {
			// fixed position is better for location, but the channels are used from worldspawn
			// and openal runs out of channels quick on cheap cards. Relative sound uses per-entity channels.
			CGame::Sound::StartRelativeSound( sound, entNum, CHAN_MUZZLEFLASH, cg_volume_effects.value, attenuation );
		}

		if( ( cgEnts[entNum].current.effects & EF_QUAD ) != 0 && ( weapon != WEAP_LASERGUN ) ) {
			SoundHandle @quadSfx = @cgs.media.sfxQuadFireSound;
			if( viewer ) {
				CGame::Sound::StartGlobalSound( quadSfx, CHAN_AUTO, cg_volume_effects.value );
			} else {
				CGame::Sound::StartRelativeSound( quadSfx, entNum, CHAN_AUTO, cg_volume_effects.value, attenuation );
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
	if( viewer && !CGame::Camera::GetMainCamera().thirdPerson ) {
		cg.vweapon.StartAnimationEvent( fireMode == FIRE_MODE_STRONG ? WEAPANIM_ATTACK_STRONG : WEAPANIM_ATTACK_WEAK );
	}
}

void Event_Pain( const EntityState @state, int parm )
{
	if( parm == PAIN_WARSHELL ) {
		if( IsViewerEntity( state.number ) ) {
			CGame::Sound::StartGlobalSound( cgs.media.sfxShellHit, CHAN_PAIN,
									 cg_volume_players.value );
		} else {
			CGame::Sound::StartRelativeSound( cgs.media.sfxShellHit, state.number, CHAN_PAIN,
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
			CGame::Sound::StartGlobalSound( @cgs.media.sfxWalljumpFailed,
				CHAN_BODY, cg_volume_effects.value );
		} else {
			CGame::Sound::StartRelativeSound( @cgs.media.sfxWalljumpFailed, state.number,
				CHAN_BODY, cg_volume_effects.value, ATTN_NORM );
		}
	} else {
		SexedSound( state.number, CHAN_BODY, StringUtils::Format( S_PLAYER_WALLJUMP_1_to_2, ( rand() & 1 ) + 1 ),
					   cg_volume_players.value, state.attenuation );

		// smoke effect
		if( ( cg_cartoonEffects.integer & 1 ) != 0 ) {
			Vec3 pos = state.origin;
			pos.z += 15;
			CGame::LE::DustCircle( pos, normal, 65.0f, 12 );
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

bool EntityEvent( const EntityState @ent, int ev, int parm, bool predicted )
{
	CEntity @cent = @cgEnts[ent.number];
	bool viewer = IsViewerEntity( ent.number );
	auto @cam = CGame::Camera::GetMainCamera();
	int weapon, fireMode;
	Vec3 dir;
	int count;

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
				CGame::Sound::StartGlobalSound( @cgs.media.sfxWeaponUp, CHAN_AUTO, cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxWeaponUp, ent.origin, CHAN_AUTO, cg_volume_effects.value, ATTN_NORM );
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
					//TODO
					//CG_Event_LaserBeam( ent.number, weapon, fireMode );
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
				//TODO
/*				Vec3 origin;

				if( ( weapon == WEAP_ELECTROBOLT
					  && fireMode == FIRE_MODE_STRONG
					  )
					|| weapon == WEAP_INSTAGUN ) {
					VectorCopy( cg.predictedPlayerState.pmove.origin, origin );
					origin[2] += cg.predictedPlayerState.viewheight;
					AngleVectors( cg.predictedPlayerState.viewangles, fv, NULL, NULL );
					CG_Event_WeaponBeam( origin, fv, cg.predictedPlayerState.POVnum, weapon, fireMode );
				} else if( weapon == WEAP_RIOTGUN || weapon == WEAP_MACHINEGUN ) {
					int seed = cg.predictedEventTimes[EV_FIREWEAPON] & 255;

					VectorCopy( cg.predictedPlayerState.pmove.origin, origin );
					origin[2] += cg.predictedPlayerState.viewheight;
					AngleVectors( cg.predictedPlayerState.viewangles, fv, rv, uv );

					if( weapon == WEAP_RIOTGUN ) {
						CG_Event_FireRiotgun( origin, fv, rv, uv, weapon, fireMode, seed, cg.predictedPlayerState.POVnum );
					} else {
						CG_Event_FireMachinegun( origin, fv, rv, uv, weapon, fireMode, seed, cg.predictedPlayerState.POVnum );
					}
				} else if( weapon == WEAP_LASERGUN ) {
					CG_Event_LaserBeam( ent.number, weapon, fireMode );
				}*/
			}
			return true;

		case EV_NOAMMOCLICK:
			if( viewer ) {
				CGame::Sound::StartGlobalSound( @cgs.media.sfxWeaponUpNoAmmo, CHAN_ITEM, cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxWeaponUpNoAmmo, ent.origin, CHAN_ITEM, cg_volume_effects.value, ATTN_IDLE );
			}
			return true;

		case EV_ITEM_RESPAWN:
			cgEnts[ent.number].respawnTime = cg.time;
			CGame::Sound::StartRelativeSound( @cgs.media.sfxItemRespawn, ent.number, CHAN_AUTO, 
				cg_volume_effects.value, ATTN_IDLE );
			return true;

		case EV_PLAYER_TELEPORT_IN:
			if( IsViewerEntity( ent.ownerNum ) ) {
				CGame::Sound::StartGlobalSound( @cgs.media.sfxTeleportIn, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxTeleportIn, ent.origin, CHAN_AUTO,
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
				CGame::Sound::StartGlobalSound( @cgs.media.sfxTeleportOut, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxTeleportOut, ent.origin, CHAN_AUTO,
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
				CGame::Sound::StartRelativeSound( @cgs.media.sfxGrenadeStrongBounce[rand() & 1], ent.number, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
			} else {
				CGame::Sound::StartRelativeSound( @cgs.media.sfxGrenadeWeakBounce[rand() & 1], ent.number, CHAN_AUTO, cg_volume_effects.value, ATTN_IDLE );
			}
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
			CGame::Sound::StartFixedSound( @cgs.media.sfxRic[rand() % 2], ent.origin, CHAN_AUTO,
									cg_volume_effects.value, ATTN_STATIC );
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
			
		default:
			break;
	}

	return false;
}

}