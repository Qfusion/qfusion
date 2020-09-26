namespace CGame {

void StartVoiceTokenEffect( int entNum, int type, int vsay ) {
	if( !cg_voiceChats.boolean || cg_volume_voicechats.value <= 0.0f ) {
		return;
	}
	if( vsay < 0 || vsay >= VSAY_TOTAL ) {
		return;
	}

	CEntity @cent = @cgEnts[entNum];

	// ignore repeated/flooded events
	if( cent.localEffects[VSAY_HEADICON_TIMEOUT] > cg.time ) {
		return;
	}

	// set the icon effect
	cent.localEffects[VSAY_HEADICON] = vsay;
	cent.localEffects[VSAY_HEADICON_TIMEOUT] = cg.time + HEADICON_TIMEOUT;

	// play the sound
	SoundHandle @sound = cgs.media.sfxVSaySounds[vsay];
	if( @sound is null ) {
		return;
	}

	// played as it was made by the 1st person player
	CGame::Sound::StartLocalSound( sound, CHAN_AUTO, cg_volume_voicechats.value );
}

bool EntityEvent( const EntityState @ent, int ev, int parm, bool predicted )
{
	bool viewer = IsViewerEntity( ent.number );
	auto @cam = CGame::Camera::GetMainCamera();

	if( viewer && ( ev < PREDICTABLE_EVENTS_MAX ) && ( predicted != cam.playerPrediction ) ) {
		return true;
	}

	switch( ev ) {
		case EV_NONE:
			break;

		case EV_ITEM_RESPAWN:
			cgEnts[ent.number].respawnTime = cg.time;
			CGame::Sound::StartRelativeSound( @cgs.media.sfxItemRespawn, ent.number, CHAN_AUTO, 
				cg_volume_effects.value, ATTN_IDLE );
			return true;

		case EV_VSAY:
			StartVoiceTokenEffect( ent.ownerNum, EV_VSAY, parm );
			break;

		default:
			break;
	}

	return false;
}

}