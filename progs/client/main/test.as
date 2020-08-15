namespace CGame {

void DrawTestBox( const Vec3 &in origin, const Vec3 &in mins, const Vec3 &in maxs ) {
	Vec3 start, end;
	const float linewidth = 6;


    start = origin + Vec3( mins[0], mins[1], mins[2] );
    end = origin + Vec3( mins[0], mins[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( mins[0], maxs[1], mins[2] );
    end = origin + Vec3( mins[0], maxs[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( maxs[0], mins[1], mins[2] );
    end = origin + Vec3( maxs[0], mins[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( maxs[0], maxs[1], mins[2] );
    end = origin + Vec3( maxs[0], maxs[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );


    start = origin + Vec3( mins[0], mins[1], mins[2] );
    end = origin + Vec3( maxs[0], mins[1], mins[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( mins[0], maxs[1], maxs[2] );
    end = origin + Vec3( maxs[0], maxs[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( mins[0], maxs[1], mins[2] );
    end = origin + Vec3( maxs[0], maxs[1], mins[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( mins[0], mins[1], maxs[2] );
    end = origin + Vec3( maxs[0], mins[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );


    start = origin + Vec3( mins[0], mins[1], mins[2] );
    end = origin + Vec3( mins[0], maxs[1], mins[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( maxs[0], mins[1], maxs[2] );
    end = origin + Vec3( maxs[0], maxs[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( maxs[0], mins[1], mins[2] );
    end = origin + Vec3( maxs[0], maxs[1], mins[2] );
	QuickPolyBeam( start, end, linewidth, null );

    start = origin + Vec3( mins[0], mins[1], maxs[2] );
    end = origin + Vec3( mins[0], maxs[1], maxs[2] );
	QuickPolyBeam( start, end, linewidth, null );
}

}
