namespace CGame {

float ViewSmoothFallKick( void ) {
	// fallkick offset
	if( cg.fallEffectTime > cg.time ) {
		float fallfrac = float( cg.time - cg.fallEffectRebounceTime ) / float( cg.fallEffectTime - cg.fallEffectRebounceTime );
		float fallkick = -1.0f * sin( deg2rad( fallfrac * 180 ) ) * ( ( cg.fallEffectTime - cg.fallEffectRebounceTime ) * 0.01f );
		return fallkick;
	} else {
		cg.fallEffectTime = cg.fallEffectRebounceTime = 0;
	}
	return 0.0f;
}

void StartFallKickEffect( int bounceTime ) {
	if( !cg_viewBob.boolean ) {
		cg.fallEffectTime = 0;
		cg.fallEffectRebounceTime = 0;
		return;
	}

	if( cg.fallEffectTime > cg.time ) {
		cg.fallEffectRebounceTime = 0;
	}

	bounceTime += 200;
    if( bounceTime > 400 ) {
        bounceTime = 400;
    }

	cg.fallEffectTime = cg.time + bounceTime;
	if( cg.fallEffectRebounceTime != 0 ) {
		cg.fallEffectRebounceTime = cg.time - ( ( cg.time - cg.fallEffectRebounceTime ) / 2 );
	} else {
		cg.fallEffectRebounceTime = cg.time;
	}
}

}