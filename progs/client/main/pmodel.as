namespace CGame {

const float MIN_LEANING_SPEED = 10.0f;

class PModelInfo {
	ModelHandle @model;
	GS::Anim::PModelAnimSet animSet;
	array<int> rootAnims(GS::Anim::PMODEL_PARTS);
	array<array<int> @> rotators(GS::Anim::PMODEL_PARTS);

	PModelInfo( PlayerModel @pmodel ) {
		@model = null;

		if( @pmodel is null ) {
			return;
		}

		@model = pmodel.model;

		for( uint i = 0; i < pmodel.numAnims; i++ ) {
			int fps;
			pmodel.getAnim( i, animSet.firstframe[i], animSet.lastframe[i], animSet.loopingframes[i], fps );
			animSet.frametime[i] = 1000.0f / float( fps > 10 ? fps : 10 );
		}

		// animation ANIM_NONE (0) is always at frame 0, and it's never
		// received from the game, but just used on the client when none
		// animation was ever set for a model (head).
		animSet.firstframe[GS::Anim::ANIM_NONE] = 0;
		animSet.lastframe[GS::Anim::ANIM_NONE] = 0;
		animSet.loopingframes[GS::Anim::ANIM_NONE] = 1;
		animSet.frametime[GS::Anim::ANIM_NONE] = 1000.0f / 15.0f;

		rootAnims[GS::Anim::UPPER] = pmodel.getRootAnim( "upper" );

		@rotators[GS::Anim::UPPER] = @pmodel.getRotators( "upper" );
		@rotators[GS::Anim::HEAD] = @pmodel.getRotators( "head" );
	}
}

class PModel {
	//static data
	PModelInfo @pmodelinfo;
	SkinHandle @skin;

	//dynamic
	GS::Anim::PModelAnimState animState;

	array<Vec3> angles(GS::Anim::PMODEL_PARTS);     // for rotations
	array<Vec3> oldangles(GS::Anim::PMODEL_PARTS);  // for rotations

	//effects
	CGame::Scene::Orientation projectionSource;     // for projectile weapon. Not sure about keeping it here
	int64 flashTime;
	int64 barrelTime;

	void ClearEventAnimations() {
		animState.ClearEventAnimations();

		// reset the weapon animation timers for new players
		flashTime = 0;
		barrelTime = 0;
	}

	void AddAnimation( array<int> &anim, int channel ) {
		animState.AddAnimation( anim, channel );
	}

	void LeanAngles( const Vec3 &in angles_, const float yawVelocity, const Vec3 &in animVelocity ) {
		const float scale = 0.04f;

		Vec3 hvelocity( animVelocity.x, animVelocity.y, 0.0f );
		float speed = hvelocity.length();

		if( speed * scale <= 1.0f ) {
			return;
		}

		Mat3 axis;
		Vec3( 0, angles_[YAW], 0 ).anglesToAxis( axis );
		array<Vec3> leanAngles(GS::Anim::PMODEL_PARTS);

		float front = scale * ( hvelocity * axis.x );
		if( front < -0.1 || front > 0.1 ) {
			leanAngles[GS::Anim::LOWER][PITCH] += front;
			leanAngles[GS::Anim::UPPER][PITCH] -= front * 0.25;
			leanAngles[GS::Anim::HEAD][PITCH] -= front * 0.5;
		}

		float aside = ( front * 0.001f ) * yawVelocity;
		if( aside != 0.0f ) {
			float asidescale = 75;
			leanAngles[GS::Anim::LOWER][ROLL] -= aside * 0.5 * asidescale;
			leanAngles[GS::Anim::UPPER][ROLL] += aside * 1.75 * asidescale;
			leanAngles[GS::Anim::HEAD][ROLL] -= aside * 0.35 * asidescale;
		}

		float side = scale * ( hvelocity * axis.y );
		if( side < -1.0f || side > 1.0f ) {
			leanAngles[GS::Anim::LOWER][ROLL] -= side * 0.5;
			leanAngles[GS::Anim::UPPER][ROLL] += side * 0.5;
			leanAngles[GS::Anim::HEAD][ROLL] += side * 0.25;
		}

		leanAngles[GS::Anim::LOWER][PITCH] = bound( -45.0f, leanAngles[GS::Anim::LOWER][PITCH], 45.0f );
		leanAngles[GS::Anim::LOWER][ROLL] = bound( -15.0f, leanAngles[GS::Anim::LOWER][ROLL], 15.0f );

		leanAngles[GS::Anim::UPPER][PITCH] = bound( -45.0f, leanAngles[GS::Anim::UPPER][PITCH], 45.0f );
		leanAngles[GS::Anim::UPPER][ROLL] = bound( -20.0f, leanAngles[GS::Anim::UPPER][ROLL], 20.0f );

		leanAngles[GS::Anim::HEAD][PITCH] = bound( -45.0f, leanAngles[GS::Anim::HEAD][PITCH], 45.0f );
		leanAngles[GS::Anim::HEAD][ROLL] = bound( -20.0f, leanAngles[GS::Anim::HEAD][ROLL], 20.0f );

		for( int j = GS::Anim::LOWER; j < GS::Anim::PMODEL_PARTS; j++ ) {
			for( int i = 0; i < 3; i++ ) {
				angles[i][j] = AngleNormalize180( angles[i][j] + leanAngles[i][j] );
			}
		}
	}
}

void RegisterBasePModel( void ) {
	String filename;

	// pmodelinfo
	filename = StringUtils::Format( "%s/%s", "models/players", DEFAULT_PLAYERMODEL );
	@cgs.basePModelInfo = PModelInfo( CGame::RegisterPlayerModel( filename ) );
	if( @cgs.basePModelInfo.model is null ) {
		CGame::Error( StringUtils::Format( "Default Player Model '%s' failed to load", DEFAULT_PLAYERMODEL ) );
	}

	filename = StringUtils::Format( "%s/%s/%s", "models/players", DEFAULT_PLAYERMODEL, DEFAULT_PLAYERSKIN );
	@cgs.baseSkin = CGame::RegisterSkin( filename );
	if( @cgs.baseSkin is null ) {
		CGame::Error( StringUtils::Format( "Default Player Skin '%s' failed to load", filename ) );
	}
}

}
