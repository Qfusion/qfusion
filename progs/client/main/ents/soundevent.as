namespace CGame {

void UpdateSoundEventEnt( CEntity @cent ) {
	int channel, soundindex, owner;
	float attenuation;
	bool fixed;
    EntityState @state = @cent.current;

    if( !cg_test.boolean )
    	return;

	soundindex = state.sound;
	owner = state.ownerNum;
	channel = state.channel & ~CHAN_FIXED;
	fixed = ( state.channel & CHAN_FIXED ) != 0 ? true : false;
	attenuation = state.attenuation;

	if( attenuation == ATTN_NONE ) {
		if( @cgs.soundPrecache[soundindex] !is null ) {
			CGame::Sound::StartGlobalSound( @cgs.soundPrecache[soundindex], channel & ~CHAN_FIXED, 1.0f );
		}
		return;
	}

	if( owner != 0 ) {
		if( owner < 0 || owner >= MAX_EDICTS ) {
			CGame::Print( "UpdateSoundEventEnt: bad owner number\n" );
			return;
		}
		if( cgEnts[owner].serverFrame != CGame::Snap.serverFrame ) {
			owner = 0;
		}
	}

	if( owner == 0 ) {
		fixed = true;
	}

	// sexed sounds are not in the sound index and ignore attenuation
	if( @cgs.soundPrecache[soundindex] is null ) {
		if( owner != 0 ) {
			const String @cstring = @cgs.configStrings[CS_SOUNDS + soundindex];
			if( cstring[0] == '*' ) {
				SexedSound( owner, channel | ( fixed ? int( CHAN_FIXED ) : 0 ), cstring, 1.0f, attenuation );
			}
		}
		return;
	}

	if( fixed ) {
		CGame::Sound::StartFixedSound( @cgs.soundPrecache[soundindex], state.origin, channel, 1.0f, attenuation );
	} else if( IsViewerEntity( owner ) ) {
		CGame::Sound::StartGlobalSound( @cgs.soundPrecache[soundindex], channel, 1.0f );
	} else {
		CGame::Sound::StartRelativeSound( @cgs.soundPrecache[soundindex], owner, channel, 1.0f, attenuation );
	}
}

}
