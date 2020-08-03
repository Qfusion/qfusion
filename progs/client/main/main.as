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
    array<ModelHandle> modelDraw(GS::MAX_MODELS);
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

	float lerpfrac;                     // between oldframe and frame
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

    if( index >= CS_MODELS && index < CS_MODELS + MAX_MODELS ) {
        if( s.substr(0, 1) == "$" ) {  // indexed pmodel
            //cgs.pModelsIndex[index - CS_MODELS] = RegisterPlayerModel( s );
        } else {
            cgs.modelDraw[index - CS_MODELS] = RegisterModel( s );
        }
    }
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
}

}