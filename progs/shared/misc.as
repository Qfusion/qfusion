namespace GS {

bool IsWalkablePlane( const Vec3 &in normal )
{
	return normal.z >= 0.7;
}

void BBoxForEntityState( EntityState @state, Vec3 &out mins, Vec3 &out maxs )
{
    if( state.solid == 0 ) {
		mins = Vec3(0.0f, 0.0f, 0.0f);
		maxs = Vec3(0.0f, 0.0f, 0.0f);
        return;
    }

	if( state.solid == SOLID_BMODEL ) {	
		InlineModelBounds( InlineModel( state.modelindex ), mins, maxs );
        return;
    }

	int x = 8 * ( state.solid & 31 );
	int zd = 8 * ( ( state.solid >> 5 ) & 31 );
	int zu = 8 * ( ( state.solid >> 10 ) & 63 ) - 32;
	mins[0] = mins[1] = -x;
	maxs[0] = maxs[1] = x;
	mins[2] = -zd;
	maxs[2] = zu;
}

int WaterLevel( EntityState @state, Vec3 &in mins, Vec3 &in maxs ) {
	int waterlevel = 0;

	Vec3 point = state.origin;
    point.z += mins.z + 1;

	int cont = PointContents( point );
	if( ( cont & MASK_WATER ) != 0 ) {
		waterlevel = 1;
		point.z += 26;
		cont = PointContents( point );
		if( ( cont & MASK_WATER ) != 0 ) {
			waterlevel = 2;
			point.z += 22;
			cont = PointContents( point );
			if( ( cont & MASK_WATER ) != 0 ) {
				waterlevel = 3;
			}
		}
	}

	return waterlevel;
}

}
