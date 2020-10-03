namespace CGame {

Vec3 playerboxStandMins, playerboxStandMaxs;
float playerboxStandViewheight;

Vec3 playerboxCrouchMins, playerboxCrouchMaxs;
float playerboxCrouchViewheight;

Vec3 playerboxGibMins, playerboxGibMaxs;
float playerboxGibViewheight;

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

	array<String> configStrings(MAX_CONFIGSTRINGS);
	array<ModelHandle @> modelDraw(MAX_MODELS);
	array<SoundHandle @> soundPrecache(MAX_SOUNDS);
	array<ShaderHandle @> imagePrecache(MAX_IMAGES);
	array<SkinHandle @> skinPrecache(MAX_SKINFILES);

	array<PModelInfo @> pModels(MAX_MODELS);
	PModelInfo @basePModelInfo; //fall back replacements
	SkinHandle @baseSkin;

	array<ClientInfo> clientInfo(MAX_CLIENTS);

	array<WModelInfo @> weaponModelInfo(WEAP_TOTAL);

	array<PModelInfo @> teamModelInfo(MAX_TEAMS);
	array<SkinHandle @> teamCustomSkin(MAX_TEAMS); // user defined
	array<int> teamColor(MAX_TEAMS);

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

	CGame::Scene::Boneposes @tempBoneposes;

	int effects;
	Mat3 autorotateAxis;
}

ClientStatic cgs;
ClientState cg;
const int SKM_MAX_BONES = 256;

void ConfigString( int index, const String @s )
{
	cgs.configStrings[index] = s;

	if( index >= CS_MODELS && index < CS_MODELS + MAX_MODELS ) {
		index -= CS_MODELS;

		@cgs.modelDraw[index] = null;
		
		if( index == 0 ) {
			return;
		}
		if( s.empty() ) {
			return;
		}

		if( s.substr( 0, 1 ) == "$" ) {  // indexed pmodel
			@cgs.pModels[index] = PModelInfo( s.substr( 1 ) );
		} else {
			@cgs.modelDraw[index] = RegisterModel( s );
		}
	} else if( index >= CS_SOUNDS && index < CS_SOUNDS + MAX_SOUNDS ) {
		index -= CS_SOUNDS;

		@cgs.soundPrecache[index] = null;

		if( s.empty() ) {
			return;	
		}
		if( s.substr( 0, 1 ) != "*" ) {
			@cgs.soundPrecache[index] = RegisterSound( s );
		}
	} else if( index >= CS_IMAGES && index < CS_IMAGES + MAX_IMAGES ) {
		index -= CS_IMAGES;

		@cgs.imagePrecache[index] = null;

		if( s.empty() ) {
			return;
		}

		@cgs.imagePrecache[index] = RegisterShader( s );
	} else if( index >= CS_SKINFILES && index < CS_SKINFILES + MAX_SKINFILES ) {
		index -= CS_SKINFILES;

		@cgs.skinPrecache[index] = null;

		if( !s.empty() ) {
			@cgs.skinPrecache[index] = RegisterSkin( s );
		}
	} else if( index >= CS_PLAYERINFOS && index < CS_PLAYERINFOS + MAX_CLIENTS ) {
		LoadClientInfo( index - CS_PLAYERINFOS );
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

	GS::GetPlayerStandSize( playerboxStandMins, playerboxStandMaxs );
	playerboxStandViewheight = GS::GetPlayerStandViewHeight();

	GS::GetPlayerCrouchSize( playerboxCrouchMins, playerboxCrouchMaxs );
	playerboxCrouchViewheight = GS::GetPlayerCrouchHeight();

	GS::GetPlayerGibSize( playerboxGibMins, playerboxGibMaxs );
	playerboxGibViewheight = GS::GetPlayerGibHeight();
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
	for( int i = 0; i < MAX_SOUNDS; i++ ) {
		ConfigString( CS_SOUNDS + i, CGame::GetConfigString( CS_SOUNDS + i ) );
	}
	for( int i = 0; i < MAX_CLIENTS; i++ ) {
		ConfigString( CS_PLAYERINFOS + i, CGame::GetConfigString( CS_PLAYERINFOS + i ) );
	}
	for( int i = 0; i < MAX_SKINFILES; i++ ) {
		ConfigString( CS_SKINFILES + i, CGame::GetConfigString( CS_SKINFILES + i ) );
	}

	RegisterForceModels();

	RegisterWeaponModels();

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

	@cg.tempBoneposes = CGame::Scene::RegisterTemporaryExternalBoneposes( SKM_MAX_BONES );

	LerpEntities();
}

void Reset()
{
	ResetClientInfos();
}

}