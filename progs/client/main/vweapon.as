namespace CGame {

class KickAngles {
	int64 timestamp;
	int64 kicktime;
	float v_roll, v_pitch;
}

class ViewBlend {
	int64 timestamp;
	int64 blendtime;
	array<float> blend(4);
}

class ViewWeapon {
	CGame::Scene::Entity ent;

	uint POVnum;
	int weapon;

	// animation
	int baseAnim;
	int64 baseAnimStartTime;
	int eventAnim;
	int64 eventAnimStartTime;

	//
	// all cyclic walking effects
	//
	float xyspeed;

	float oldBobTime;
	int bobCycle;                   // odd cycles are right foot going forward
	float bobFracSin;               // sin(bobfrac*M_PI)

	//
	// kick angles
	//
	array<KickAngles> kickangles(MAX_ANGLES_KICKS);

	// other effects
	CGame::Scene::Orientation projectionSource;

    void UpdateProjectionSource( const Vec3 &in hand_origin, const Mat3 &in hand_axis, const Vec3 &in weap_origin, const Mat3 &in weap_axis ) {
        // move to tag_weapon
        CGame::Scene::Orientation tag_weapon = CGame::Scene::MoveToTag( 
            CGame::Scene::Orientation( hand_origin, hand_axis ),
            CGame::Scene::Orientation( weap_origin, weap_axis ) );

        // move to projectionSource tag
        if( weapon > WEAP_NONE && weapon < WEAP_TOTAL ) {
            auto @weaponInfo = @cgs.weaponModelInfo[weapon];
            projectionSource = CGame::Scene::MoveToTag( tag_weapon,
                        weaponInfo.tag_projectionsource );
            return;
        }

        // fall back: copy gun origin and move it front by 16 units and 8 up
        projectionSource = tag_weapon;
        projectionSource.origin += 16.0f * projectionSource.axis.x;
        projectionSource.origin += 8.0f * projectionSource.axis.z;
    }

    int BaseanimFromWeaponState( int weaponState ) {
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
            case WEAPON_STATE_READY:
            default:
                if( cg_gunbob.boolean ) {
                    anim = WEAPANIM_STANDBY;
                } else {
                    anim = WEAPANIM_NOANIM;
                }
                break;
        }

        return anim;
    }

    Vec3 AddAngleEffects( const Vec3 angles_ ) {
        if( !cg_gun.boolean || !cg_gunbob.boolean ) {
            return angles_;
        }

        Vec3 angles = angles_;

        // gun angles from bobbing
        if( ( bobCycle & 1 ) != 0 ) {
            angles[ROLL] -= xyspeed * bobFracSin * 0.012;
            angles[YAW] -= xyspeed * bobFracSin * 0.006;
        } else {
            angles[ROLL] += xyspeed * bobFracSin * 0.012;
            angles[YAW] += xyspeed * bobFracSin * 0.006;
        }
        angles[PITCH] += xyspeed * bobFracSin * 0.012;

        // gun angles from delta movement
        PlayerState @state = @CGame::Snap.playerState;
        PlayerState @oldState = @CGame::OldSnap.playerState;
        Vec3 deltaAngles = oldState.viewAngles - state.viewAngles;

        for( int i = 0; i < 3; i++ ) {
            float delta = deltaAngles[i] * cg.lerpfrac;
            delta = AngleNormalize180( delta );
            delta = bound( delta, -45.0f, 45.0f );

            if( i == YAW ) {
                angles[ROLL] += 0.001 * delta;
            }
            angles[i] += 0.002 * delta;
        }

        // gun angles from kicks
        angles = AddKickAngles( angles );

        return angles;
    }

    void CalcViewBob( CGame::Camera::Camera @cam ) {
        float bobMove, bobTime, bobScale;
        PlayerState @state = @CGame::PredictedPlayerState;
        Vec3 velocity = state.pmove.velocity;
        Vec3 xyvelocity( velocity.x, velocity.y, 0.0f );

        // calculate speed and cycle to be used for all cyclic walking effects
        xyspeed = xyvelocity.length();

        bobScale = 0;
        if( xyspeed < 5 ) {
            oldBobTime = 0;  // start at beginning of cycle again
        } else if( cg_gunbob.boolean ) {
            if( !IsViewerEntity( cam.POVent ) ) {
                bobScale = 0.0f;
            } else if( ( GS::PointContents( cam.origin ) & MASK_WATER ) != 0 ) {
                bobScale =  0.75f;
            } else {
                CEntity @cent = @cgEnts[cam.POVent];

                Vec3 mins, maxs;
                GS::BBoxForEntityState( @cent.current, mins, maxs );
                maxs.z = mins.z;
                mins.z -= ( 1.6f * STEPSIZE );

                Trace tr;
                tr.doTrace( state.pmove.origin, mins, maxs, state.pmove.origin, cam.POVent, MASK_PLAYERSOLID );

                if( tr.startSolid || tr.allSolid ) {
                    if( state.pmove.stats[PM_STAT_CROUCHTIME] != 0 ) {
                        bobScale = 1.5f;
                    } else {
                        bobScale = 2.5f;
                    }
                }
            }
        }

        bobMove = cg.frameTime * bobScale * 0.001f;
        bobTime = ( oldBobTime += bobMove );

        bobCycle = int( bobTime );
        bobFracSin = abs( sin( deg2rad( bobTime * 180.0f ) ) );
    }

    void ResetKickAngles() {
        for( int i = 0; i < MAX_ANGLES_KICKS; i++ ) {
            kickangles[i].timestamp = 0;
            kickangles[i].kicktime = 0;
        }
    }

    void StartKickAnglesEffect( const Vec3 &in source, float knockback, float radius, int time ) {
        float side;
        float ftime;
        int kicknum = -1;

        if( knockback <= 0 || time <= 0 || radius <= 0.0f ) {
            return;
        }

        // if spectator but not in chasecam, don't get any kick
        if( CGame::Snap.playerState.pmove.pm_type == PM_SPECTATOR ) {
            return;
        }
 
        // predictedPlayerState is predicted only when prediction is enabled, otherwise it is interpolated
        Vec3 playerorigin = CGame::PredictedPlayerState.pmove.origin;
        Vec3 v = source - playerorigin;
        float dist = v.normalize();
        if( dist > radius ) {
            return;
        }

        float delta = 1.0f - ( dist / radius );
        if( delta > 1.0f ) {
            delta = 1.0f;
        }
        if( delta <= 0.0f ) {
            return;
        }

        float kick = abs( knockback ) * delta;
        if( kick <= 0.0f ) { // kick of 0 means no view adjust at all
            return;
        }

        //find first free kick spot, or the one closer to be finished
        for( int i = 0; i < MAX_ANGLES_KICKS; i++ ) {
            if( cg.time > kickangles[i].timestamp + kickangles[i].kicktime ) {
                kicknum = i;
                break;
            }
        }

        // all in use. Choose the closer to be finished
        if( kicknum == -1 ) {
            int remaintime;
            int best = ( kickangles[0].timestamp + kickangles[0].kicktime ) - cg.time;
            kicknum = 0;
            for( int i = 1; i < MAX_ANGLES_KICKS; i++ ) {
                remaintime = ( kickangles[i].timestamp + kickangles[i].kicktime ) - cg.time;
                if( remaintime < best ) {
                    best = remaintime;
                    kicknum = i;
                }
            }
        }

        Vec3 forward, right, up;
        CGame::PredictedPlayerState.viewAngles.angleVectors( forward, right, up );

        if( kick < 1.0f ) {
            kick = 1.0f;
        }

        side = v * right;
        kickangles[kicknum].v_roll = bound( -20.0f, kick * side * 0.3, 20.0f );

        side = -( v * forward );
        kickangles[kicknum].v_pitch = bound( -20.0f, kick * side * 0.3, 20.0f );

        kickangles[kicknum].timestamp = cg.time;
        ftime = float( time ) * delta;
        if( ftime < 100 ) {
            ftime = 100;
        }
        kickangles[kicknum].kicktime = int( ftime );
    }

    Vec3 AddKickAngles( Vec3 viewangles_ ) {
        Vec3 viewangles = viewangles_;

        for( int i = 0; i < MAX_ANGLES_KICKS; i++ ) {
            if( cg.time > kickangles[i].timestamp + kickangles[i].kicktime ) {
                continue;
            }

            float time = float( ( kickangles[i].timestamp + kickangles[i].kicktime ) - cg.time );
            float uptime = float ( kickangles[i].kicktime ) * 0.5f;
            float delta = 1.0f - abs( time - uptime ) / uptime;

            if( delta > 1.0f ) {
                delta = 1.0f;
            }
            if( delta <= 0.0f ) {
                continue;
            }

            viewangles[PITCH] += kickangles[i].v_pitch * delta;
            viewangles[ROLL] += kickangles[i].v_roll * delta;
        }

        return viewangles;
    }

    bool RefreshAnimation() {
        int baseAnim;
        int curframe = -1;
        float framefrac;
        bool nolerp = false;
        PlayerState @state = @CGame::PredictedPlayerState;

        // if the pov changed, or weapon changed, force restart
        if( POVnum != state.POVnum || weapon != state.stats[STAT_WEAPON] ) {
            nolerp = true;
            eventAnim = 0;
            eventAnimStartTime = 0;
            baseAnim = 0;
            baseAnimStartTime = 0;
        }

        POVnum = state.POVnum;
        weapon = state.stats[STAT_WEAPON];

        WModelInfo @weaponInfo = null;
        if( weapon >= WEAP_NONE || weapon < WEAP_TOTAL ) {
            @weaponInfo = @cgs.weaponModelInfo[weapon];
        }

        // hack cause of missing animation config
        if( @weaponInfo is null ) {
            ent.frame = ent.oldFrame = 0;
            ent.backLerp = 0.0f;
            eventAnim = 0;
            eventAnimStartTime = 0;
            return false;
        }

        baseAnim = BaseanimFromWeaponState( state.weaponState );

        // Full restart
        if( baseAnimStartTime == 0 ) {
            baseAnim = baseAnim;
            baseAnimStartTime = cg.time;
            nolerp = true;
        }

        // base animation changed?
        if( baseAnim != baseAnim ) {
            baseAnim = baseAnim;
            baseAnimStartTime = cg.time;
        }

        // if a eventual animation is running override the baseAnim
        if( eventAnim != 0 ) {
            if( eventAnimStartTime == 0 ) {
                eventAnimStartTime = cg.time;
            }

            framefrac = GS::FrameForTime( curframe, cg.time, eventAnimStartTime, weaponInfo.animSet.frametime[eventAnim],
                                        weaponInfo.animSet.firstframe[eventAnim], weaponInfo.animSet.lastframe[eventAnim],
                                        weaponInfo.animSet.loopingframes[eventAnim], false );

            if( curframe < 0 ) {
                // disable event anim and fall through
                eventAnim = 0;
                eventAnimStartTime = 0;
            }
        }

        if( curframe < 0 ) {
            // find new frame for the current animation
            framefrac = GS::FrameForTime( curframe, cg.time, baseAnimStartTime, weaponInfo.animSet.frametime[baseAnim],
                                        weaponInfo.animSet.firstframe[baseAnim], weaponInfo.animSet.lastframe[baseAnim],
                                        weaponInfo.animSet.loopingframes[baseAnim], true );
        }

        if( curframe < 0 ) {
            CGame::Error( "UpdateAnimation(2): Base Animation without a defined loop.\n" );
        }

        if( nolerp ) {
            framefrac = 0;
            ent.oldFrame = curframe;
        } else {
            framefrac = bound( framefrac, 0.0f, 1.0f );
            if( curframe != ent.frame ) {
                ent.oldFrame = ent.frame;
            }
        }

        ent.frame = curframe;
        ent.backLerp = 1.0f - framefrac;
        return true;
    }

    void StartAnimationEvent( int newAnim ) {
        eventAnim = newAnim;
        eventAnimStartTime = cg.time;
        RefreshAnimation();
    }

    void CalcViewWeapon( CGame::Camera::Camera @cam ) {
        CGame::Scene::Orientation tag;
        PlayerState @state = @CGame::PredictedPlayerState;
        CGame::Camera::Viewport @view = @CGame::Camera::GetViewport();

        CalcViewBob( @cam );

        if( !RefreshAnimation() ) {
            return;
        }

        auto @weaponInfo = @cgs.weaponModelInfo[weapon];
        @ent.model = @weaponInfo.model[WEAPMODEL_HAND];
        ent.renderfx = RF_MINLIGHT | RF_WEAPONMODEL | RF_FORCENOLOD | ( cg_shadows.integer < 2 ? int( RF_NOSHADOW ) : 0 );
        ent.scale = 1.0f;
        @ent.customShader = null;
        @ent.customSkin = null;
        ent.rtype = RT_MODEL;
        @ent.boneposes = null;
        @ent.oldBoneposes = null;
        ent.shaderRGBA = COLOR_RGBA( 255, 255, 255, 255 );

        if( cg_gun_alpha.value < 1.0f ) {
            ent.renderfx |= RF_ALPHAHACK;
            ent.shaderRGBA = COLOR_REPLACEA( ent.shaderRGBA, uint8( bound( 0, int( cg_gun_alpha.value * 255.0f ), 255 ) ) );
        }

        // calculate the entity position
        ent.origin = cam.origin;

        auto @skel = @weaponInfo.skel[WEAPMODEL_HAND];
        if( @skel !is null ) {
            // get space in cache, interpolate, transform, link
            @ent.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @skel );
            @ent.oldBoneposes = @ent.boneposes;
            CGame::Scene::LerpSkeletonPoses( @skel, ent.frame, ent.oldFrame, @ent.boneposes, 1.0 - ent.backLerp );
            CGame::Scene::TransformBoneposes( @skel, ent.boneposes, ent.boneposes );
        }

        // weapon config offsets
        Vec3 gunAngles = weaponInfo.handAngles + state.viewAngles;
        Vec3 gunOffset = weaponInfo.handOrigin + Vec3( cg_gunz.value, cg_gunx.value, cg_guny.value );

        // scale forward gun offset depending on fov and aspect ratio
        gunOffset.x = gunOffset.x * view.width / ( view.height * cam.fracDistFOV ) ;

        // hand cvar offset
        float handOffset = 0.0f;
        if( cgs.demoPlaying ) {
            if( cg_hand.integer == 0 ) {
                handOffset = cg_handOffset.value;
            } else if( cg_hand.integer == 1 ) {
                handOffset = -cg_handOffset.value;
            }
        } else {
            if( cgs.clientInfo[cam.POVent - 1].hand == 0 ) {
                handOffset = cg_handOffset.value;
            } else if( cgs.clientInfo[cam.POVent - 1].hand == 1 ) {
                handOffset = -cg_handOffset.value;
            }
        }

        gunOffset.y += handOffset;
        if( cg_gun.boolean && cg_gunbob.boolean ) {
            gunOffset.z += ViewSmoothFallKick();
        }

        // apply the offsets
        ent.origin += gunOffset.x * cam.axis.x;
        ent.origin += gunOffset.y * cam.axis.y;
        ent.origin += gunOffset.z * cam.axis.z;

        // add angles effects
        gunAngles = AddAngleEffects( gunAngles );

        // finish
        gunAngles.anglesToAxis( ent.axis );

        if( cg_gun_fov.boolean && state.pmove.stats[PM_STAT_ZOOMTIME] == 0 ) {
            float gun_fov_y = CGame::Camera::WidescreenFov( bound( 20.0f, cg_gun_fov.value, 160.0f ) );
            float gun_fov_x = CGame::Camera::CalcHorizontalFov( gun_fov_y, view.width, view.height );
            float fracWeapFOV = tan( deg2rad( gun_fov_x ) * 0.5f ) / cam.fracDistFOV;
            ent.axis.x = fracWeapFOV * ent.axis.x;
        }

        // if the player doesn't want to view the weapon we still have to build the projection source
        if( CGame::Scene::GrabTag( tag, @ent, "tag_weapon" ) ) {
            UpdateProjectionSource( ent.origin, ent.axis, tag.origin, tag.axis );
        } else {
            UpdateProjectionSource( ent.origin, ent.axis, vec3Origin, Mat3() );
        }
    }

    void AddViewWeapon( CGame::Camera::Camera @cam ) {
        int64 flashTime = 0;
        PlayerState @state = @CGame::PredictedPlayerState;

        if( !cam.drawWeapon || weapon == WEAP_NONE ) {
            return;
        }

        // update the other origins
        ent.origin2 = ent.origin;
        ent.lightingOrigin = cgEnts[POVnum].refEnt.lightingOrigin;

        CGame::Scene::AddEntityToScene( @ent );

        AddShellEffects( @ent, cg.effects );

        if( cg_weaponFlashes.integer == 2 ) {
            flashTime = cgEnts[POVnum].pmodel.flashTime;
        }

        // add attached weapon
        CGame::Scene::Orientation tag;
        if( CGame::Scene::GrabTag( tag, @ent, "tag_weapon" ) ) {
            auto @firedef = GS::FiredefForPlayerState( @state, state.stats[STAT_WEAPON] );

            AddWeaponOnTag( @ent, tag, weapon, cg.effects | EF_OUTLINE, flashTime, 
                cgEnts[POVnum].pmodel.barrelTime, state.inventory[firedef.ammoID] );
        }
    }
}

}
