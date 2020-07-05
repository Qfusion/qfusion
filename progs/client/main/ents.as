namespace CGame {

class Entity {
	EntityState current;
	EntityState prev;
	int64 serverFrame;

	Vec3 velocity;
	Vec3 prevVelocity;

	bool canExtrapolate;
	bool canExtrapolatePrev;

	int microSmooth;

	Vec3 trailOrigin;
}

array< Entity > cgEnts( MAX_EDICTS );

void NewPacketEntityState( const EntityState @state ) {
	Entity @cent = cgEnts[state.number];

	if( GS::IsEventEntity( state ) ) {
		cent.prev = cent.current;
		cent.current = state;
		cent.serverFrame = CGame::Snap.serverFrame;
		cent.canExtrapolatePrev = false;
		cent.prevVelocity.clear();
	} else if( state.linearMovement ) {
		// if teleported or the trajectory type has changed, force nolerp
		if( state.teleported ||	state.linearMovement != cent.current.linearMovement 
			|| state.linearMovementTimeStamp != cent.current.linearMovementTimeStamp ) {
			cent.serverFrame = -99;
		}

		if( cent.serverFrame != CGame::OldSnap.serverFrame ) {
			// wasn't in the last update
			cent.prev = state;
		} else {
			cent.prev = cent.current;
		}
		cent.current = state;
		cent.serverFrame = CGame::Snap.serverFrame;

		cent.canExtrapolate = false;
		cent.canExtrapolatePrev = false;
		//cent->linearProjectileCanDraw = CG_UpdateLinearProjectilePosition( cent );

		cent.velocity = cent.prevVelocity = state.linearMovementVelocity;
		cent.trailOrigin = state.origin;
	} else {
		// if it moved too much force no lerping
		if(  abs( int( cent.current.origin[0] - state.origin[0] ) ) > 512
			 || abs( int( cent.current.origin[1] - state.origin[1] ) ) > 512
			 || abs( int( cent.current.origin[2] - state.origin[2] ) ) > 512 ) {
			cent.serverFrame = -99;
		}

		// some data changes will force no lerping as well
		if( state.modelindex != cent.current.modelindex
			|| state.teleported 
			|| state.linearMovement != cent.current.linearMovement ) {
			cent.serverFrame = -99;
		}

		if( cent.serverFrame != CGame::OldSnap.serverFrame ) {
			// wasn't in last update, so initialize some things
			// duplicate the current state so lerping doesn't hurt anything
			cent.prev = state;
			cent.microSmooth = 0;
		} else {
			// shuffle the last state to previous
			cent.prev = cent.current;
		}

		cent.current = state;
		cent.trailOrigin = state.origin;
		cent.prevVelocity = cent.velocity;

		cent.canExtrapolatePrev = cent.canExtrapolate;
		cent.canExtrapolate = false;
		cent.velocity.clear();
		cent.serverFrame = CGame::Snap.serverFrame;

		// set up velocities for this entity
		if( CGame::ExtrapolationTime > 0 &&
			( state.type == ET_PLAYER || state.type == ET_CORPSE ) ) {
			cent.velocity = cent.current.origin2;
			cent.prevVelocity = cent.prev.origin;
			cent.canExtrapolate = cent.canExtrapolatePrev = true;
		} else if( cent.prev.origin != state.origin ) {
			int snapTime = int( CGame::Snap.serverTime - CGame::OldSnap.serverTime );

			if( snapTime < 1 ) {
				snapTime = CGame::SnapFrameTime;
			}
			float scale = 1000.0f / float( snapTime );

			cent.velocity = ( cent.current.origin - cent.prev.origin ) * scale;
		}

		if( ( state.type == ET_GENERIC || state.type == ET_PLAYER
			  || state.type == ET_GIB || state.type == ET_GRENADE
			  || state.type == ET_ITEM || state.type == ET_CORPSE ) ) {
			cent.canExtrapolate = true;
		}

		if( GS::IsBrushModel( state.modelindex ) ) {
			// disable extrapolation on movers
			cent.canExtrapolate = false;
		}
	}
}

}
