namespace CGame {

class ClientStatic {
	String serverName;
	uint playerNum;
	bool demoplaying;
	String demoName;
	bool pure;
	uint snapFrameTime;
	int protocol;
	String demoExtension;
	bool gameStart;

	array<String> configStrings(GS::MAX_CONFIGSTRINGS);
	array<ModelHandle @> modelDraw(GS::MAX_MODELS);
	array<SoundHandle @> soundPrecache(GS::MAX_SOUNDS);
	array<ShaderHandle @> imagePrecache(GS::MAX_IMAGES);
	array<SkinHandle @> skinPrecache(GS::MAX_SKINFILES);

	array<PlayerModel @> pModels(GS::MAX_MODELS);
	PlayerModel @basePModelInfo; //fall back replacements
	SkinHandle @baseSkin;

	array<PlayerModel @> teamModelInfo(GS::MAX_TEAMS);
	array<SkinHandle @> teamCustomSkin(GS::MAX_TEAMS); // user defined
	array<int> teamColor(GS::MAX_TEAMS);

	CMedia media;

	bool demoPlaying;
	bool precacheDone;

	CGame::Scene::Orientation weaponItemTag;
}

class ClientState {
	int64 time;
	int64 realTime;
	int64 frameCount;
	int frameTime;
	int realFrameTime;
	uint extrapolationTime;

	float lerpfrac;					 // between oldframe and frame
	float xerpTime;
	float oldXerpTime;
	float xerpSmoothFrac;

	Mat3 autorotateAxis;
}

ClientStatic cgs;
ClientState cg;

void ConfigString( int index, const String @s )
{
	cgs.configStrings[index] = s;

	if( index >= CS_MODELS && index < CS_MODELS + GS::MAX_MODELS ) {
		index -= CS_MODELS;

		if( index == 0 ) {
			return;
		}

		if( s.empty() ) {
			@cgs.modelDraw[index] = null;
		} else if( s.substr( 0, 1 ) == "$" ) {  // indexed pmodel
			@cgs.pModels[index] = RegisterPlayerModel( s.substr( 1 ) );
		} else {
			@cgs.modelDraw[index] = RegisterModel( s );
		}
	} else if( index >= CS_SOUNDS && index < CS_SOUNDS + GS::MAX_SOUNDS ) {
		index -= CS_SOUNDS;

		if( s.empty() ) {
			@cgs.soundPrecache[index] = null;
		} else if( s.substr( 0, 1 ) != "*" ) {
			@cgs.soundPrecache[index] = RegisterSound( s );
		}
	} else if( index >= CS_IMAGES && index < CS_IMAGES + GS::MAX_IMAGES ) {
		index -= CS_IMAGES;

		if( s.empty() ) {
			@cgs.imagePrecache[index] = null;
		} else {
			@cgs.imagePrecache[index] = RegisterShader( s );
		}
	} else if( index >= CS_SKINFILES && index < CS_SKINFILES + GS::MAX_SKINFILES ) {
		index -= CS_SKINFILES;

		if( s.empty() ) {
			@cgs.skinPrecache[index] = null;
		} else {
			@cgs.skinPrecache[index] = RegisterSkin( s );
		}
	}
}

void Load()
{
	cg_teamPLAYERSmodel.modified = true;
	cg_teamPLAYERSmodelForce.modified = true;
	cg_teamPLAYERSskin.modified = true;
	cg_teamPLAYERScolor.modified = true;
	cg_teamPLAYERScolorForce.modified = true;

	cg_teamALPHAmodel.modified = true;
	cg_teamALPHAmodelForce.modified = true;
	cg_teamALPHAskin.modified = true;
	cg_teamALPHAcolor.modified = true;

	cg_teamBETAmodel.modified = true;
	cg_teamBETAmodelForce.modified = true;
	cg_teamBETAskin.modified = true;
	cg_teamBETAcolor.modified = true;
}

void Init( const String @serverName, uint playerNum, bool demoPlaying, const String @demoName, 
	bool pure, uint snapFrameTime, int protocol, const String @demoExtension, bool gameStart )
{
	cgs.serverName = serverName;
	cgs.playerNum = playerNum;
	cgs.demoPlaying = demoPlaying;
	cgs.demoName = demoName;
	cgs.pure = pure;
	cgs.snapFrameTime = snapFrameTime;
	cgs.protocol = protocol;
	cgs.demoExtension = demoExtension;
	cgs.gameStart = gameStart;
	cgs.precacheDone = false;

	cgs.weaponItemTag.axis.identity();
	cgs.weaponItemTag.origin = -14.0 * cgs.weaponItemTag.axis.x;
}

void Precache()
{
	for( int i = 0; i < MAX_MODELS; i++ ) {
		ConfigString( CS_MODELS + i, CGame::GetConfigString( CS_MODELS + i ) );
	}

	cgs.media.PrecacheShaders();

	cgs.media.PrecacheModels();

	cgs.media.PrecacheSounds();

	cgs.precacheDone = true;
}

void Frame( int frameTime, int realFrameTime, int64 realTime, int64 serverTime, 
	float stereoSeparation, uint extrapolationTime )
{
	cg.realTime = realTime;
	cg.frameTime = frameTime;
	cg.realFrameTime = realFrameTime;
	cg.frameCount++;
	cg.time = serverTime;
	cg.extrapolationTime = extrapolationTime;

	int snapTime = ( CGame::Snap.serverTime - CGame::OldSnap.serverTime );
	if( snapTime == 0 ) {
		snapTime = cgs.snapFrameTime;
	}

	if( CGame::OldSnap.serverTime == CGame::Snap.serverTime ) {
		cg.lerpfrac = 1.0f;
	} else {
		cg.lerpfrac = ( double( cg.time - cg.extrapolationTime ) - double( CGame::OldSnap.serverTime ) ) / double( snapTime );
	}

	if( cg.extrapolationTime != 0 ) {
		cg.xerpTime = 0.001f * double( cg.time - CGame::Snap.serverTime );
		cg.oldXerpTime = 0.001f * double( cg.time - CGame::OldSnap.serverTime );

		if( cg.time >= CGame::Snap.serverTime ) {
			cg.xerpSmoothFrac = double( cg.time - CGame::Snap.serverTime ) / double( cg.extrapolationTime );
		} else {
			cg.xerpSmoothFrac = double( CGame::Snap.serverTime - cg.time ) / double( cg.extrapolationTime );
			if( cg.xerpSmoothFrac < -1.0f ) cg.xerpSmoothFrac = -1.0f;
			if( cg.xerpSmoothFrac >  0.0f ) cg.xerpSmoothFrac =  0.0f;
			cg.xerpSmoothFrac = 1.0f - cg.xerpSmoothFrac;
		}

		if( cg.xerpSmoothFrac < 0.0f ) cg.xerpSmoothFrac = 0.0f;
		if( cg.xerpSmoothFrac > 1.0f ) cg.xerpSmoothFrac = 1.0f;

		if( cg.xerpTime < -( cg.extrapolationTime * 0.001f ) )
			cg.xerpTime = -( cg.extrapolationTime * 0.001f );

		//clamp( cg.xerpTime, -( cgs.extrapolationTime * 0.001f ), ( cgs.extrapolationTime * 0.001f ) );
		//clamp( cg.oldXerpTime, 0, ( ( snapTime + cgs.extrapolationTime ) * 0.001f ) );
	} else {
		cg.xerpTime = 0.0f;
		cg.xerpSmoothFrac = 0.0f;
	}

	if( cg_showClamp.boolean ) {
		if( cg.lerpfrac > 1.0f ) {
			CGame::Print( "high clamp " + cg.lerpfrac + "\n" );
		} else if( cg.lerpfrac < 0.0f ) {
			CGame::Print( "low clamp " + cg.lerpfrac + "\n" );
		}
	}

	if( cg.lerpfrac < 0.0f ) cg.xerpSmoothFrac = 0.0f;
	if( cg.lerpfrac > 1.0f ) cg.xerpSmoothFrac = 1.0f;

	Vec3 autorotate;
	bool flipped = CGame::Camera::GetMainCamera().flipped;

	autorotate[YAW] = ( cg.time % 3600 ) * 0.1 * ( flipped ? -1.0f : 1.0f );
	autorotate.anglesToAxis( cg.autorotateAxis );

	LerpEntities();
}

}