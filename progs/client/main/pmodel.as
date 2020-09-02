namespace CGame {
    
class PModel {
	//static data
	PlayerModel @pmodelinfo;
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

	void AddAnimation( int loweranim, int upperanim, int headanim, int channel ) {
		animState.AddAnimation( loweranim, upperanim, headanim, channel );
	}
}

void RegisterBasePModel( void ) {
	String filename;

	// pmodelinfo
	filename = StringUtils::Format( "%s/%s", "models/players", DEFAULT_PLAYERMODEL );
	@cgs.basePModelInfo = CGame::RegisterPlayerModel( filename );
	if( cgs.basePModelInfo is null ) {
		CGame::Error( StringUtils::Format( "Default Player Model '%s' failed to load", DEFAULT_PLAYERMODEL ) );
	}

	filename = StringUtils::Format( "%s/%s/%s", "models/players", DEFAULT_PLAYERMODEL, DEFAULT_PLAYERSKIN );
	@cgs.baseSkin = CGame::RegisterSkin( filename );
	if( cgs.baseSkin is null ) {
		CGame::Error( StringUtils::Format( "Default Player Skin '%s' failed to load", filename ) );
	}

}

}
