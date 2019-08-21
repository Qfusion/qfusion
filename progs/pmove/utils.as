/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2007 Chasseur de Bots
Copyright (C) 2019 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

namespace PM {

float Boundf( float a, float b, float c ) {
	if( b < a ) {
		return a;
	}
	if( b > c ) {
		return c;
	}
	return b;
}

int CrouchLerpPlayerSize( int timer, Vec3 &out mins, Vec3 &out maxs, float &out viewHeight ) {
	if( timer < 0 ) {
		timer = 0;
	} else if( timer > CROUCHTIME ) {
		timer = CROUCHTIME;
	}

	float crouchFrac = Boundf( 0.0f, float( timer ) / float( CROUCHTIME ), 1.0f );
	mins = playerboxStandMins - ( crouchFrac * ( playerboxStandMins - playerboxCrouchMins ) );
	maxs = playerboxStandMaxs - ( crouchFrac * ( playerboxStandMaxs - playerboxCrouchMaxs ) );
	viewHeight = playerboxStandViewheight - ( crouchFrac * ( playerboxStandViewheight - playerboxCrouchViewheight ) );

	return timer;
}

bool IsWalkablePlane( const Vec3 &in normal ) {
	return normal.z >= 0.7;
}

float HorizontalLength( const Vec3 &in v ) {
	float x = v.x, y = v.y;
	return sqrt( x * x + y * y );
}

}
