namespace CGame {

void UpdateGenericEnt( CEntity @cent ) {
	cent.refEnt.reset();
	cent.refEnt.shaderRGBA = ColorForEntity( @cent, false );

	cent.refEnt.frame = cent.current.frame;
	cent.refEnt.oldFrame = cent.prev.frame;
	cent.refEnt.rtype = RT_MODEL;

	int modelindex = cent.current.modelindex;
	if( modelindex > 0 && modelindex < MAX_MODELS ) {
		@cent.refEnt.model = @cgs.modelDraw[modelindex];
	}

	@cent.skel = SkeletonForModel( cent.refEnt.model );
}

void LerpGenericEnt( CEntity @cent ) {
	int POVent = CGame::Camera::GetMainCamera().POVent;
	Vec3 ent_angles;

	cent.refEnt.backLerp = 1.0f - cg.lerpfrac;

	if( CGame::IsViewerEntity( cent.current.number ) || POVent == cent.current.number ) {
		ent_angles = CGame::PredictedPlayerState.viewAngles;
	} else {
		// interpolate angles
		ent_angles = LerpAngles( cent.prev.angles, cent.current.angles, cg.lerpfrac );
	}

	if( ent_angles.x != 0.0f || ent_angles.y != 0.0f || ent_angles.z != 0.0f ) {
		ent_angles.anglesToAxis( cent.refEnt.axis );
	} else {
		cent.refEnt.axis.identity();
	}

	if( ( cent.renderfx & RF_FRAMELERP ) != 0 ) {
		// step origin discretely, because the frames
		// do the animation properly
		Vec3 delta, move;

		// FIXME: does this still work?
		move = cent.current.origin2 - cent.current.origin;
		delta = cent.refEnt.axis * move;
		cent.refEnt.origin = cent.current.origin + cent.refEnt.backLerp * delta;
	} else if( CGame::IsViewerEntity( cent.current.number ) || POVent == cent.current.number ) {
		cent.refEnt.origin = CGame::PredictedPlayerState.pmove.origin;
		cent.refEnt.origin2 = cent.refEnt.origin;
	} else {
		if( cg.extrapolationTime != 0 && cent.canExtrapolate ) { // extrapolation
			Vec3 origin, xorigin1, xorigin2;

			float lerpfrac = cg.lerpfrac;
			if( lerpfrac < 0.0f ) lerpfrac = 0.0f;
			if( lerpfrac > 1.0f ) lerpfrac = 1.0f;

			// extrapolation with half-snapshot smoothing
			if( cg.xerpTime >= 0 || !cent.canExtrapolatePrev ) {
				xorigin1 = cent.current.origin + cg.xerpTime * cent.velocity;
			} else {
				xorigin1 = cent.current.origin + cg.xerpTime * cent.velocity;

				if( cent.canExtrapolatePrev ) {
					Vec3 oldPosition = cent.prev.origin + cg.oldXerpTime * cent.prevVelocity;
					xorigin1 = oldPosition + cg.xerpSmoothFrac * (xorigin1 - oldPosition);
				}
			}

			// extrapolation with full-snapshot smoothing
			xorigin2 = cent.current.origin + cg.xerpTime * cent.velocity;
			if( cent.canExtrapolatePrev ) {
				Vec3 oldPosition = cent.prev.origin + cg.oldXerpTime * cent.prevVelocity;
				xorigin2 = oldPosition + lerpfrac * (xorigin2 - oldPosition);
			}

			origin = xorigin1 + 0.5f * (xorigin2 - xorigin1);

			/*
			// Interpolation between 2 extrapolated positions
			if( !cent.canExtrapolatePrev )
			    VectorMA( cent.current.origin, cg.xerpTime, cent.velocity, xorigin2 );
			else
			{
			    float frac = cg.lerpfrac;
			    clamp( frac, 0.0f, 1.0f );
			    VectorLerp( cent.prevExtrapolatedOrigin, frac, cent.extrapolatedOrigin, xorigin2 );
			}
			*/

			if( cent.microSmooth == 2 ) {
				Vec3 oldsmoothorigin = cent.microSmoothOrigin2 + 0.65f * (cent.microSmoothOrigin - cent.microSmoothOrigin2);
				cent.refEnt.origin = origin + 0.5f * (oldsmoothorigin - origin);
			} else if( cent.microSmooth == 1 ) {
				cent.refEnt.origin = origin + 0.5f * (cent.microSmoothOrigin - origin);
			} else {
				cent.refEnt.origin = origin;
			}

			if( cent.microSmooth != 0 ) {
				cent.microSmoothOrigin2 = cent.microSmoothOrigin;
			}

			cent.microSmoothOrigin = origin;
			cent.microSmooth++;
			if( cent.microSmooth > 2 ) cent.microSmooth = 2;

			cent.refEnt.origin2 = cent.refEnt.origin;
		} else {   // plain interpolation
			cent.refEnt.origin = cent.prev.origin + cg.lerpfrac * (cent.current.origin - cent.prev.origin);
			cent.refEnt.origin2 = cent.refEnt.origin;
		}
	}

	cent.refEnt.lightingOrigin = cent.refEnt.origin;
}

void AddGenericEnt( CEntity @cent ) {
	if( cent.refEnt.scale <= 0.001f ) {
		return;
	}

	// if set to invisible, skip
	if( cent.current.modelindex == 0 && ( cent.effects & EF_FLAG_TRAIL ) == 0 ) {
		return;
	}

	// bobbing & auto-rotation
	if( ( cent.effects & EF_ROTATE_AND_BOB ) != 0 ) {
		EntAddBobEffect( cent );
		cent.refEnt.axis = cg.autorotateAxis;
	}

	if( ( cent.effects & EF_TEAMCOLOR_TRANSITION ) != 0 ) {
		EntAddTeamColorTransitionEffect( cent );
	}

	// render effects
	cent.refEnt.renderfx = cent.renderfx;

	Item @item = cent.item;
	if( @item != null ) {
		if( ( item.type & ( IT_HEALTH | IT_POWERUP ) ) != 0 ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}

		if( ( cent.effects & EF_AMMOBOX ) != 0 ) {
			// Ugly hack for the release. Armor models are way too big
			cent.refEnt.scale *= 0.9f;

			// find out the ammo box color
			auto @colorToken = @item.colorToken;
			if( colorToken.length() > 1 ) {
				cent.refEnt.shaderRGBA = ColorByIndex( ColorIndex( colorToken[1] ) );
			} else {   // set white
				cent.refEnt.shaderRGBA = COLOR_RGBA( 255, 255, 255, 255 );
			}
		}

		if( ( cent.effects & EF_GHOST ) != 0 ) {
			cent.refEnt.renderfx |= RF_ALPHAHACK | RF_GREYSCALE;
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 100 );
		} else {
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 255 );
		}

		// add shadows for items (do it before offseting for weapons)
		if( !cg_shadows.boolean || ( cent.renderfx & RF_NOSHADOW ) != 0 ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		} else if( cg_shadows.integer == 1 ) {
			//TODO
			//CG_AllocShadeBox( cent.current.number, cent.refEnt.origin, item_box_mins, item_box_maxs, NULL );
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}

		cent.refEnt.renderfx |= RF_MINLIGHT;

		// offset weapon items by their special tag
		if( ( cent.item.type & IT_WEAPON ) != 0 ) {
			CGame::Scene::PlaceModelOnTag( @cent.refEnt, @cent.refEnt, cgs.weaponItemTag );
		}
	} else {
		if( cent.current.solid != SOLID_BMODEL ) {
			cent.refEnt.renderfx |= RF_NOSHADOW;
		}
	}

	if( @cent.skel != null ) {
		// get space in cache, interpolate, transform, link
		@cent.refEnt.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @cent.skel );
		@cent.refEnt.oldBoneposes = @cent.refEnt.boneposes;
		CGame::Scene::LerpSkeletonPoses( @cent.skel, cent.refEnt.frame, cent.refEnt.oldFrame, @cent.refEnt.boneposes, 1.0 - cent.refEnt.backLerp );
		CGame::Scene::TransformBoneposes( @cent.skel, @cent.refEnt.boneposes, @cent.refEnt.boneposes );
	}

	// flags are special
	if( ( cent.effects & EF_FLAG_TRAIL ) != 0 ) {
		AddFlagModelOnTag( @cent, cent.refEnt.shaderRGBA, "tag_linked" );
	}

	if( cent.current.modelindex == 0 ) {
		return;
	}

	CGame::Scene::AddEntityToScene( @cent.refEnt );

	AddLinkedModel( @cent );
}

}