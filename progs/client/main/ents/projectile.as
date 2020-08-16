namespace CGame {

const float MIN_DRAWDISTANCE_FIRSTPERSON = 86;
const float MIN_DRAWDISTANCE_THIRDPERSON = 52;

bool UpdateLinearProjectilePosition( CEntity @cent ) {
	Vec3 origin;
	EntityState @state;
	int moveTime;
	int64 serverTime;

	@state = @cent.current;

	if( !state.linearMovement ) {
		return true;
	}

	if( GS::MatchPaused() ) {
		serverTime = CGame::Snap.serverTime;
	} else {
		serverTime = cg.time + cg.extrapolationTime;
	}

	if( state.solid != SOLID_BMODEL ) {
		// add a time offset to counter antilag visualization
		if( !cgs.demoPlaying && cg_projectileAntilagOffset.value > 0.0f &&
			!IsViewerEntity( state.ownerNum ) && ( cgs.playerNum + 1 != CGame::PredictedPlayerState.POVnum ) ) {
			serverTime += int64( float( state.modelindex2 ) * cg_projectileAntilagOffset.value );
		}
	}

	moveTime = GS::LinearMovement( state, serverTime, origin );
	state.origin = origin;

	if( ( moveTime < 0 ) && ( state.solid != SOLID_BMODEL ) ) {
		// when flyTime is negative don't offset it backwards more than PROJECTILE_PRESTEP value
		// FIXME: is this still valid?
		float maxBackOffset;

		if( IsViewerEntity( state.ownerNum ) ) {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_FIRSTPERSON );
		} else {
			maxBackOffset = ( PROJECTILE_PRESTEP - MIN_DRAWDISTANCE_THIRDPERSON );
		}

		if( state.origin2.distance( state.origin ) > maxBackOffset ) {
			return false;
		}
	}

	return true;
}

void ExtrapolateLinearProjectile( CEntity @cent ) {
	int i;

	cent.linearProjectileCanDraw = UpdateLinearProjectilePosition( cent );

	cent.refEnt.backLerp = 1.0f;

	cent.refEnt.origin = cent.current.origin;
	cent.refEnt.origin2 = cent.refEnt.origin;
	cent.refEnt.lightingOrigin = cent.refEnt.origin;
	cent.current.angles.anglesToAxis( cent.refEnt.axis );
}

}