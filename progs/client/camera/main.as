namespace CGame {

namespace Camera {

Cvar cg_thirdPersonAngle( "cg_thirdPersonAngle", "0", CVAR_ARCHIVE );
Cvar cg_thirdPersonRange( "cg_thirdPersonRange", "70", CVAR_ARCHIVE );

void ThirdPersonOffsetView( Camera @cam ) {
	Trace tr;
	const Vec3 mins( -4, -4, -4 ), maxs( 4, 4, 4 );

	// calc exact destination
	float a = deg2rad( cg_thirdPersonAngle.value );
	float f = -cos( a );
	float r = -sin( a );

	Vec3 chase_dest = cam.origin;
	chase_dest += cg_thirdPersonRange.value * f * cam.axis.x;
	chase_dest += cg_thirdPersonRange.value * r * cam.axis.y;
	chase_dest.z += 8;

	// find the spot the player is looking at
	Vec3 dest = cam.origin + 512.0f * cam.axis.x;
	tr.doTrace( cam.origin, mins, maxs, dest, cam.POVent, MASK_SOLID );

	// calculate pitch to look at the same spot from camera
	Vec3 stop = tr.endPos - cam.origin;
	float dist = sqrt( stop.x * stop.x + stop.y * stop.y );
	if( dist < 1 ) {
		dist = 1;
	}
	cam.angles[PITCH] = rad2deg( -atan2( stop.z, dist ) );
	cam.angles[YAW] -= cg_thirdPersonAngle.value;
	cam.angles.anglesToMarix( cam.axis );

	// move towards destination
	if( tr.doTrace( cam.origin, mins, maxs, chase_dest, cam.POVent, MASK_SOLID ) ) {
		stop = tr.endPos;
		stop.z += ( 1.0 - tr.fraction ) * 32;
		tr.doTrace( cam.origin, mins, maxs, stop, cam.POVent, MASK_SOLID );
		chase_dest = tr.endPos;
	}

	cam.origin = chase_dest;
}

void SetupCamera( Camera @cam ) {
	if( cam.thirdPerson ) {
		ThirdPersonOffsetView( cam );
	}

	cg.vweapon.CalcViewWeapon( @cam );
}

void SetupRefdef( Camera @cam ) {
	if( !cg_test.boolean ) {
		return;
	}
	cg.vweapon.AddViewWeapon( @cam );
}

}

}

