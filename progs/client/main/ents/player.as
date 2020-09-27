namespace CGame {

void PlayerModelAddFlag( CEntity @cent ) {
	int team = cent.current.team;
	int flagTeam  = ( team == TEAM_ALPHA ) ? TEAM_BETA : TEAM_ALPHA;
	int color;

	// color for team
	if( team != TEAM_BETA && team != TEAM_ALPHA ) {
		color = COLOR_RGBA( 255, 255, 255, 255 );
	} else {
		color = cgs.teamColor[flagTeam];
	}

	AddFlagModelOnTag( @cent, color, "tag_flag1" );
}

void AddHeadIcon( CEntity @cent ) {
	ShaderHandle @iconShader;
	float radius = 6, upoffset = 8;
	CGame::Scene::Orientation tag_head;

	if( ( cent.refEnt.renderfx & RF_VIEWERMODEL ) != 0 ) {
		return;
	}

	if( ( cent.effects & EF_BUSYICON ) != 0 ) {
		@iconShader = @cgs.media.shaderChatBalloon;
		radius = 12.0f;
		upoffset = 2.0f;
	} else if( cent.localEffects[LEF_VSAY_HEADICON_TIMEOUT] > cg.time ) {
		if( cent.localEffects[LEF_VSAY_HEADICON] < VSAY_TOTAL ) {
			@iconShader = @cgs.media.shaderVSayIcon[cent.localEffects[LEF_VSAY_HEADICON]];
		} else {
			@iconShader = @cgs.media.shaderVSayIcon[VSAY_GENERIC];
		}

		radius = 12.0f;
		upoffset = 0.0f;
	}

	bool stunned = ( cent.effects & EF_PLAYER_STUNNED ) != 0 || ( cent.prev.effects & EF_PLAYER_STUNNED ) != 0;
	bool showIcon = ( @iconShader !is null || stunned );

	// add the current active icon
	if( !showIcon ) {
		return;
	}

	CGame::Scene::Entity balloon;
	balloon.renderfx = RF_NOSHADOW;

	if( CGame::Scene::GrabTag( tag_head, @cent.refEnt, "tag_head" ) ) {
		balloon.origin = tag_head.origin;
		balloon.origin.z += upoffset;
		CGame::Scene::PlaceModelOnTag( @balloon, @cent.refEnt, tag_head );
	} else {
		balloon.origin = cent.refEnt.origin;
		balloon.origin.z += playerboxStandMins.z + upoffset;
	}
	balloon.origin2 = balloon.origin;

	if( @iconShader !is null ) {
		balloon.rtype = RT_SPRITE;
		@balloon.customShader = @iconShader;
		balloon.radius = radius;
		@balloon.model = null;
		CGame::Scene::AddEntityToScene( @balloon );
	}

	// add stun effect: not really a head icon, but there's no point in finding the head location twice
	if( stunned ) {
		balloon.rtype = RT_MODEL;
		@balloon.customShader = null;
		balloon.radius = 0;
		@balloon.model = @cgs.media.modHeadStun;

		if( ( cent.current.effects & EF_PLAYER_STUNNED ) == 0 ) {
			balloon.shaderRGBA = COLOR_REPLACEA( balloon.shaderRGBA, uint8( 255.0f * ( 1.0f - cg.lerpfrac ) ) );
		}

		CGame::Scene::AddEntityToScene( @balloon );
	}
}

/*
* UpdatePlayerModelEnt
* Called each new serverframe
*/
void UpdatePlayerModelEnt( CEntity @cent ) {
	int i;
	PModel @pmodel = @cent.pmodel;

	// start from clean
    cent.refEnt.reset();

    PModelForCentity( @cent, @pmodel.pmodelinfo, @pmodel.skin );
	if( @pmodel.pmodelinfo is null )
		return;

    cent.refEnt.shaderRGBA = ColorForEntity( @cent, true );

	if( cg_raceGhosts.boolean && !CGame::IsViewerEntity( cent.current.number ) && GS::RaceGametype() ) {
		cent.effects &= ~EF_OUTLINE;
		cent.effects |= EF_RACEGHOST;
	} else {
		cent.effects &= ~EF_OUTLINE;
	}

	// fallback
	if( @pmodel.pmodelinfo.model is null || @pmodel.skin is null ) {
		@pmodel.pmodelinfo = @cgs.basePModelInfo;
		@pmodel.skin = @cgs.baseSkin;
	}

	// make sure al poses have their memory space
	@cent.skel = CGame::SkeletonForModel( @pmodel.pmodelinfo.model );
	if( @cent.skel is null ) {
		CGame::Error( "UpdatePlayerModelEnt: ET_PLAYER without a skeleton\n" );
	}

	// update parts rotation angles
	for( i = GS::Anim::LOWER; i < GS::Anim::PMODEL_PARTS; i++ )
        pmodel.oldangles[i] = pmodel.angles[i];

	if( cent.current.type == ET_CORPSE || cent.current.type == ET_MONSTER_CORPSE ) {
        cent.animVelocity.clear();
		cent.yawVelocity = 0;
	} else {
		// update smoothed velocities used for animations and leaning angles
        int serverFrame = CGame::Snap.serverFrame;

		// rotational yaw velocity
		float adelta = GS::AngleDelta( cent.current.angles[YAW], cent.prev.angles[YAW] );
        adelta = bound( -35.0f, adelta, 35.0f );;
    
		// smooth a velocity vector between the last snaps 
		cent.lastVelocities[serverFrame & 3][0] = cent.velocity[0];
		cent.lastVelocities[serverFrame & 3][1] = cent.velocity[1];
		cent.lastVelocities[serverFrame & 3][2] = 0;
		cent.lastYawVelocities[serverFrame & 3] = adelta;
		cent.lastVelocitiesFrames[serverFrame & 3] = serverFrame;

        cent.animVelocity.clear();
		cent.yawVelocity = 0;

		int count = 0;
		for( i = serverFrame; ( i >= 0 ) && ( count < 3 ) && ( i == cent.lastVelocitiesFrames[i & 3] ); i-- ) {
            cent.animVelocity += cent.lastVelocities[i&3];
			cent.yawVelocity += cent.lastYawVelocities[i&3];
			count++;
		}

		// safety/static code analysis check
		if( count > 0 ) {
            cent.animVelocity *= 1.0f / float(count);
	    	cent.yawVelocity *= 1.0f / float(count);
        }

		//
		// Calculate angles for each model part
		//

		// lower has horizontal direction, and zeroes vertical
		pmodel.angles[GS::Anim::LOWER][PITCH] = 0;
		pmodel.angles[GS::Anim::LOWER][YAW] = cent.current.angles[YAW];
		pmodel.angles[GS::Anim::LOWER][ROLL] = 0;

		// upper marks vertical direction (total angle, so it fits aim)
		if( cent.current.angles[PITCH] > 180 ) {
			pmodel.angles[GS::Anim::UPPER][PITCH] = ( -360 + cent.current.angles[PITCH] );
		} else {
			pmodel.angles[GS::Anim::UPPER][PITCH] = cent.current.angles[PITCH];
		}

		pmodel.angles[GS::Anim::UPPER][YAW] = 0;
		pmodel.angles[GS::Anim::UPPER][ROLL] = 0;

		// head adds a fraction of vertical angle again
		if( cent.current.angles[PITCH] > 180 ) {
			pmodel.angles[GS::Anim::HEAD][PITCH] = ( -360 + cent.current.angles[PITCH] ) / 3;
		} else {
			pmodel.angles[GS::Anim::HEAD][PITCH] = cent.current.angles[PITCH] / 3;
		}

		pmodel.angles[GS::Anim::HEAD][YAW] = 0;
		pmodel.angles[GS::Anim::HEAD][ROLL] = 0;

        pmodel.LeanAngles( cent.current.angles, cent.yawVelocity, cent.animVelocity );
	}

	// Spawning (teleported bit) forces nobacklerp and the interruption of EVENT_CHANNEL animations
	if( cent.current.teleported ) {
		for( i = GS::Anim::LOWER; i < GS::Anim::PMODEL_PARTS; i++ )
            pmodel.oldangles[i] = pmodel.angles[i];
	}

	cent.pendingAnimationsUpdate = true;
}

void AddPlayerEnt( CEntity @cent ) {
	//CGame::Scene::Orientation tag_weapon;
    auto @cam = @CGame::Camera::GetMainCamera();
    bool isViewer = CGame::IsViewerEntity( cent.current.number );

	cent.UpdatePModelAnimations();

	PModel @pmodel = @cent.pmodel;
	if( @pmodel.pmodelinfo is null )
		return;

	// render effects
	cent.refEnt.renderfx = cent.renderfx;
	cent.refEnt.renderfx |= RF_MINLIGHT;

	// if viewer model, and casting shadows, offset the entity to predicted player position
	// for view and shadow accuracy

	if( isViewer ) {
		Vec3 org = cent.refEnt.origin;

		if( cam.playerPrediction ) {
			float backlerp = 1.0f - cg.lerpfrac;

            org = CGame::PredictedPlayerState.pmove.origin - backlerp * CGame::PredictionError();
			org = CGame::Camera::SmoothPredictedSteps( org );
		}

		cg.effects = cent.effects;
		if( !cam.thirdPerson ) {
			cent.refEnt.renderfx |= RF_VIEWERMODEL; // only draw from mirrors
		}

		// (cheap trick) if not thirdperson offset it some units back so the shadow looks more at our feet
		if( ( cent.refEnt.renderfx & (RF_VIEWERMODEL|RF_NOSHADOW) ) == RF_VIEWERMODEL ) {
			if( cg_shadows.integer == 1 ) {
                org = org - 24.0f * cent.refEnt.axis.x;
			}
		}

        cent.refEnt.origin = org;
        cent.refEnt.origin2 = org;
        cent.refEnt.lightingOrigin = org;      
	}

	if( cent.current.modelindex == 0 || cent.current.team == TEAM_SPECTATOR ) {
		return;
	}

	// since origin is displaced in player models set lighting origin to the center of the bbox
    cent.refEnt.lightingOrigin = cent.refEnt.origin + ( 0.5f * ( cent.mins + cent.maxs ) );

	GS::Anim::PModelAnimState @animState = @pmodel.animState;

	// transform animation values into frames, and set up old-current poses pair
    animState.AnimToFrame( cg.time, @pmodel.pmodelinfo.animSet );

	// register temp boneposes for this skeleton
	if( @cent.skel is null ) {
		CGame::Error( "CG_PlayerModelEntityAddToScene: ET_PLAYER without a skeleton" );
	}

	@cent.refEnt.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @cent.skel );
	@cent.refEnt.oldBoneposes = @cent.refEnt.boneposes;

	// fill base pose with lower animation already interpolated
	CGame::Scene::LerpSkeletonPoses( @cent.skel, animState.frame[GS::Anim::LOWER], animState.oldframe[GS::Anim::LOWER], cent.refEnt.boneposes, animState.lerpFrac[GS::Anim::LOWER] );

	// create an interpolated pose of the animation to be blent
	CGame::Scene::LerpSkeletonPoses( @cent.skel, animState.frame[GS::Anim::UPPER], animState.oldframe[GS::Anim::UPPER], cg.tempBoneposes, animState.lerpFrac[GS::Anim::UPPER] );

	// blend it into base pose
	int rootanim = pmodel.pmodelinfo.rootAnims[GS::Anim::UPPER];
	CGame::Scene::RecurseBlendSkeletalBone( @cent.skel, @cg.tempBoneposes, @cent.refEnt.boneposes, rootanim, 1.0f );

	// add skeleton effects (pose is unmounted yet)
	if( cent.current.type != ET_CORPSE ) {
        Vec3 tmpangles;

		// if it's our client use the predicted angles
		if( isViewer && cam.playerPrediction && ( uint( cam.POVent ) == cgs.playerNum + 1 ) ) {
           tmpangles = Vec3( 0.0f, CGame::PredictedPlayerState.viewAngles[YAW], 0.0f );
		} else {
			// apply interpolated LOWER angles to entity
            for( int j = 0; j < 3; j++ )
                tmpangles[j] = LerpAngle( pmodel.oldangles[GS::Anim::LOWER][j], pmodel.angles[GS::Anim::LOWER][j], cg.lerpfrac );
    	}

        tmpangles.anglesToAxis( cent.refEnt.axis );
    }

	// apply angles to rotator bones
	// also add rotations from velocity leaning
	if( cent.current.type != ET_CORPSE ) {
		array<int> parts = { GS::Anim::UPPER, GS::Anim::HEAD };

		for( uint i = 0; i < parts.size(); i++ ) {
			int part = parts[i];
			Vec3 tmpangles;

			for( int j = 0; j < 3; j++ ) {
				tmpangles[j] = LerpAngle( pmodel.oldangles[part][j], pmodel.angles[part][j], cg.lerpfrac );
			}

			CGame::Scene::RotateBonePoses( tmpangles, @cent.refEnt.boneposes, @pmodel.pmodelinfo.rotators[part] );
		}
	}

	// finish (mount) pose. Now it's the final skeleton just as it's drawn.
	CGame::Scene::TransformBoneposes( @cent.skel, cent.refEnt.boneposes, cent.refEnt.boneposes );

	// Vic: Hack in frame numbers to aid frustum culling
	cent.refEnt.backLerp = 1.0 - cg.lerpfrac;
	cent.refEnt.frame = animState.frame[GS::Anim::LOWER];
	cent.refEnt.oldFrame = animState.oldframe[GS::Anim::LOWER];

	// Add playermodel ent
	cent.refEnt.scale = 1.0f;
	cent.refEnt.rtype = RT_MODEL;
	@cent.refEnt.model = @pmodel.pmodelinfo.model;
	@cent.refEnt.customShader = null;
	@cent.refEnt.customSkin = @pmodel.skin;
	cent.refEnt.renderfx |= RF_NOSHADOW;

	if( ( cent.renderfx & RF_NOSHADOW ) == 0 && ( cg_showSelfShadow.boolean || ( cent.refEnt.renderfx & RF_VIEWERMODEL ) == 0 ) ) {
		if( cg_shadows.integer == 1 ) {
			//TODO
			//CG_AllocShadeBox( cent.current.number, cent.refEnt.origin, playerbox_stand_mins, playerbox_stand_maxs, NULL );
		} else if( cg_shadows.boolean ) {
			cent.refEnt.renderfx &= ~RF_NOSHADOW;
		}
	}

	if( ( cent.effects & EF_RACEGHOST ) == 0 ) {
		CGame::Scene::AddEntityToScene( @cent.refEnt );
	}

	if( @cent.refEnt.model is null ) {
		return;
	}

	PlayerModelAddFlag( cent );

	AddShellEffects( @cent.refEnt, cent.effects );

	AddHeadIcon( @cent );

/*
	// add teleporter sfx if needed
	CG_PModel_SpawnTeleportEffect( cent );
*/

	// add weapon model
	CGame::Scene::Orientation tag_weapon;
	if( cent.current.weapon != 0 && CGame::Scene::GrabTag( tag_weapon, @cent.refEnt, "tag_weapon" ) ) {
		pmodel.projectionSource = AddWeaponOnTag( @cent.refEnt, tag_weapon, cent.current.weapon, 
			cent.effects, pmodel.flashTime, pmodel.barrelTime, -1 );
	}

	// corpses can never have a model in modelindex2
	if( cent.current.type == ET_CORPSE ) {
		return;
	}

	AddLinkedModel( @cent );
}

}