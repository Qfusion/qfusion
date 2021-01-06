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

void DamageIndicatorAdd( int damage, const Vec3 &in dir ) {
	const int TOP_BLEND = 0;
	const int RIGHT_BLEND = 1;
	const int BOTTOM_BLEND = 2;
	const int LEFT_BLEND = 3;
	// epsilons are 30 degrees
	const float INDICATOR_EPSILON = 0.5f;
	const float INDICATOR_EPSILON_UP = 0.85f;
	int64 damageTime;
	array<int64> blends(4);
	float forward, side;

	if( !cg_damage_indicator.boolean ) {
		return;
	}

	Mat3 playerAxis;
	Vec3 playerAngles( 0, CGame::PredictedPlayerState.viewAngles.y, 0 );
	playerAngles.anglesToMarix( playerAxis );

	if( cg_damage_indicator_time.value < 0 ) {
		cg_damage_indicator_time.set( 0 );
	}

	for( int i = 0; i < 4; i++ ) {
		blends[i] = 0;
	}
	damageTime = int64( damage * cg_damage_indicator_time.value );

	// up and down go distributed equally to all blends and assumed when no dir is given
	if( dir == vec3Origin || cg_damage_indicator.integer == 2 || GS::Instagib() ||
		( abs( dir * playerAxis.z ) > INDICATOR_EPSILON_UP ) ) {
		blends[RIGHT_BLEND] += damageTime;
		blends[LEFT_BLEND] += damageTime;
		blends[TOP_BLEND] += damageTime;
		blends[BOTTOM_BLEND] += damageTime;
	} else {
		side = dir * playerAxis.y;
		if( side > INDICATOR_EPSILON ) {
			blends[LEFT_BLEND] += damageTime;
		} else if( side < -INDICATOR_EPSILON ) {
			blends[RIGHT_BLEND] += damageTime;
		}

		forward = dir * playerAxis.x;
		if( forward > INDICATOR_EPSILON ) {
			blends[BOTTOM_BLEND] += damageTime;
		} else if( forward < -INDICATOR_EPSILON ) {
			blends[TOP_BLEND] += damageTime;
		}
	}

	for( int i = 0; i < 4; i++ ) {
		if( cg.damageBlends[i] < cg.time + blends[i] ) {
			cg.damageBlends[i] = cg.time + blends[i];
		}
	}
}

void ResetDamageIndicator( void ) {
	for( uint i = 0; i < cg.damageBlends.size(); i++ ) {
		cg.damageBlends[i] = 0;
	}
}

void ResetColorBlend( void ) {
	for( uint i = 0; i < cg.colorblends.size(); i++ ) {
		cg.colorblends[i].blendtime = 0;
		cg.colorblends[i].timestamp = 0;
	}
}

void StartColorBlendEffect( float r, float g, float b, float a, int time ) {
	int bnum = -1;

	if( a <= 0.0f || time <= 0 ) {
		return;
	}

	//find first free colorblend spot, or the one closer to be finished
	for( int i = 0; i < MAX_COLORBLENDS; i++ ) {
		if( cg.time > cg.colorblends[i].timestamp + cg.colorblends[i].blendtime ) {
			bnum = i;
			break;
		}
	}

	// all in use. Choose the closer to be finished
	if( bnum == -1 ) {
		int64 remaintime;
		int64 best = ( cg.colorblends[0].timestamp + cg.colorblends[0].blendtime ) - cg.time;
		bnum = 0;
		for( int i = 1; i < MAX_COLORBLENDS; i++ ) {
			remaintime = ( cg.colorblends[i].timestamp + cg.colorblends[i].blendtime ) - cg.time;
			if( remaintime < best ) {
				best = remaintime;
				bnum = i;
			}
		}
	}

	// assign the color blend
	cg.colorblends[bnum].blend[0] = r;
	cg.colorblends[bnum].blend[1] = g;
	cg.colorblends[bnum].blend[2] = b;
	cg.colorblends[bnum].blend[3] = a;

	cg.colorblends[bnum].timestamp = cg.time;
	cg.colorblends[bnum].blendtime = time;
}

}