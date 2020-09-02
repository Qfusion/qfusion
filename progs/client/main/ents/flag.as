namespace CGame {

void AddFlagModelOnTag( CEntity @cent, int teamcolor, const String @tagname ) {
	CGame::Scene::Entity flag;
	CGame::Scene::Orientation tag;

	if( ( cent.effects & EF_FLAG_TRAIL ) == 0 ) {
		return;
	}

	@flag.model = @cgs.media.modFlag;
	if( @flag.model == null ) {
		return;
	}

	flag.rtype = RT_MODEL;
	flag.renderfx = cent.refEnt.renderfx;
	flag.shaderRGBA = teamcolor;
	flag.origin = cent.refEnt.origin;
	flag.origin2 = cent.refEnt.origin2;
	flag.lightingOrigin = cent.refEnt.lightingOrigin;

	// place the flag on the tag if available
	if( @tagname != null && CGame::Scene::GrabTag( tag, @cent.refEnt, tagname ) ) {
		flag.axis = cent.refEnt.axis;
		CGame::Scene::PlaceModelOnTag( @flag, @cent.refEnt, tag );
	} else {   // Flag dropped
		Vec3 angles;

		// quick & dirty client-side rotation animation, rotate once every 2 seconds
		if( cent.flyStopTime == 0 ) {
			cent.flyStopTime = cg.time;
		}

		angles[0] = LerpAngle( cent.prev.angles[0], cent.current.angles[0], cg.lerpfrac ) - 75; // Let it stand up 75 degrees
		angles[1] = ( 360.0 * ( ( cent.flyStopTime - cg.time ) % 2000 ) ) / 2000.0;
		angles[2] = LerpAngle( cent.prev.angles[2], cent.current.angles[2], cg.lerpfrac );

		angles.anglesToAxis( flag.axis );
		flag.origin += 16 * flag.axis.x; // Move the flag up a bit
	}

	CGame::Scene::AddEntityToScene( @flag );

	// add the light & energy effects
	if( CGame::Scene::GrabTag( tag, @flag, "tag_color" ) ) {
		CGame::Scene::PlaceModelOnTag( @flag, @flag, tag );
	}

	// FIXME: convert this to an autosprite mesh in the flag model
	if( ( cent.refEnt.renderfx & RF_VIEWERMODEL ) == 0 ) {
		flag.rtype = RT_SPRITE;
		@flag.model = null;
		flag.renderfx = RF_NOSHADOW | RF_FULLBRIGHT;
		flag.frame = flag.oldFrame = 0;
		flag.radius = 32.0f;
		@flag.customShader = @cgs.media.shaderFlagFlare;
		CGame::Scene::AddEntityToScene( @flag );
	}

	// if on a player, flag drops colored particles and lights up
/*
	if( cent.current.type == ET_PLAYER ) {
		CG_AddLightToScene( flag.origin, 350, teamcolor[0] / 255, teamcolor[1] / 255, teamcolor[2] / 255 );

		if( cent.localEffects[LOCALEFFECT_FLAGTRAIL_LAST_DROP] + FLAG_TRAIL_DROP_DELAY < cg.time ) {
			cent.localEffects[LOCALEFFECT_FLAGTRAIL_LAST_DROP] = cg.time;
			CG_FlagTrail( flag.origin, cent.trailOrigin, cent.refEnt.origin, teamcolor[0] / 255, teamcolor[1] / 255, teamcolor[2] / 255 );
		}
	}
*/
}

void UpdateFlagBaseEnt( CEntity @cent ) {
	// set entity color based on team
	cent.refEnt.shaderRGBA = ColorForEntity( @cent, false );

	cent.refEnt.scale = 1.0f;

	@cent.item = GS::FindItemByTag( cent.current.itemNum );
	if( @cent.item != null ) {
		cent.effects |= cent.item.effects;
	}

	cent.refEnt.rtype = RT_MODEL;
	cent.refEnt.frame = cent.current.frame;
	cent.refEnt.oldFrame = cent.prev.frame;

	// set up the model
	int modelindex = cent.current.modelindex;
	if( modelindex > 0 && modelindex < MAX_MODELS ) {
		@cent.refEnt.model = @cgs.modelDraw[modelindex];
	}
	@cent.skel = CGame::SkeletonForModel( cent.refEnt.model );
}

void AddFlagBaseEnt( CEntity @cent ) {
	if( cent.refEnt.scale == 0.0f )
		return;

	// if set to invisible, skip
	if( cent.current.modelindex == 0 )
		return;

	// bobbing & auto-rotation
	if( cent.current.type != ET_PLAYER && ( cent.effects & EF_ROTATE_AND_BOB ) != 0 ) {
		EntAddBobEffect( cent );
		cent.refEnt.axis = cg.autorotateAxis;
	}

	// render effects
	cent.refEnt.renderfx = cent.renderfx | RF_NOSHADOW;

	if( @cent.skel != null ) {
		// get space in cache, interpolate, transform, link
		@cent.refEnt.boneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( @cent.skel );
		@cent.refEnt.oldBoneposes = @cent.refEnt.boneposes;
		CGame::Scene::LerpSkeletonPoses( @cent.skel, cent.refEnt.frame, cent.refEnt.oldFrame, @cent.refEnt.boneposes, 1.0 - cent.refEnt.backLerp );
		CGame::Scene::TransformBoneposes( @cent.skel, @cent.refEnt.boneposes, @cent.refEnt.boneposes );
	}

	// add to refresh list
	CGame::Scene::AddEntityToScene( @cent.refEnt );

	@cent.refEnt.customSkin = null;
	@cent.refEnt.customShader = null;  // never use a custom skin on others

	// see if we have to add a flag
	if( ( cent.effects & EF_FLAG_TRAIL ) != 0 ) {
		int teamcolor = ColorForEntity( @cent, false );
		AddFlagModelOnTag( cent, teamcolor, "tag_flag1" );
	}
}

}