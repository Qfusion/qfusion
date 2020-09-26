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
	if( cent.localEffects[LEF_VSAY_HEADICON_TIMEOUT] > cg.time ) {
		return;
	}

	// set the icon effect
	cent.localEffects[LEF_VSAY_HEADICON] = vsay;
	cent.localEffects[LEF_VSAY_HEADICON_TIMEOUT] = cg.time + HEADICON_TIMEOUT;

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

		case EV_PLAYER_TELEPORT_IN:
			if( IsViewerEntity( ent.ownerNum ) ) {
				CGame::Sound::StartGlobalSound( @cgs.media.sfxTeleportIn, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxTeleportIn, ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			}

			if( ent.ownerNum != 0 && ent.ownerNum < GS::maxClients + 1 ) {
				auto @ce = @cgEnts[ent.ownerNum];
				ce.localEffects[LEF_EV_PLAYER_TELEPORT_IN] = cg.time;
				ce.teleportedTo = ent.origin;
			}
			return true;

		case EV_PLAYER_TELEPORT_OUT:
			if( IsViewerEntity( ent.ownerNum ) ) {
				CGame::Sound::StartGlobalSound( @cgs.media.sfxTeleportOut, CHAN_AUTO,
										 cg_volume_effects.value );
			} else {
				CGame::Sound::StartFixedSound( @cgs.media.sfxTeleportOut, ent.origin, CHAN_AUTO,
										cg_volume_effects.value, ATTN_NORM );
			}

			if( ent.ownerNum != 0 && ent.ownerNum < GS::maxClients + 1 ) {
				auto @ce = @cgEnts[ent.ownerNum];
				ce.localEffects[LEF_EV_PLAYER_TELEPORT_OUT] = cg.time;
				ce.teleportedFrom = ent.origin;
			}
			return true;

		case EV_VSAY:
			StartVoiceTokenEffect( ent.ownerNum, EV_VSAY, parm );
			return true;

		default:
			break;
	}

	return false;
}

}