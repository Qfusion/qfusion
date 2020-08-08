namespace GS {

int LinearMovement( const EntityState @ent, int64 time, Vec3 &out dest ) {
	int moveTime;
	float moveFrac;

	moveTime = int( time - ent.linearMovementTimeStamp );
	if( moveTime < 0 ) {
		moveTime = 0;
	}

	if( ent.linearMovementDuration != 0 ) {
		if( moveTime > int(ent.linearMovementDuration) ) {
			moveTime = ent.linearMovementDuration;
		}

		Vec3 dist = ent.linearMovementEnd - ent.linearMovementBegin;
		moveFrac = float( moveTime ) / float( ent.linearMovementDuration );
		if( moveFrac < 0.0f ) {
			moveFrac = 0.0f;
		} else if( moveFrac > 1.0f ) {
			moveFrac = 1.0f;
		}
		dest = ent.linearMovementBegin + moveFrac * dist;
	} else {
		moveFrac = float( moveTime ) * 0.001f;
		dest = ent.linearMovementBegin + ent.linearMovementVelocity * moveFrac;
	}

	return moveTime;
}

Vec3 LinearMovementDelta( const EntityState @ent, int64 oldTime, int64 curTime ) {
	Vec3 p1, p2;
	LinearMovement( ent, oldTime, p1 );
	LinearMovement( ent, curTime, p2 );
	return p2 - p1;
}

}
