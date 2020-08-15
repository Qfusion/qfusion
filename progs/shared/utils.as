/*
Copyright (C) 2009-2011 Chasseur de bots

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

int rint( float x )
{
	return int( x < 0 ? ( x - 0.5f ) : ( x + 0.5f ) );
}

int rint( double x )
{
	return int( x < 0 ? ( x - 0.5f ) : ( x + 0.5f ) );
}

float bound( float x, float a, float b )
{
	return x < a ? a : ( (x > b ? b : x) );
}

double bound( double x, double a, double b )
{
	return x < a ? a : ( (x > b ? b : x) );
}

int bound( int x, int a, int b )
{
	return x < a ? a : ( (x > b ? b : x) );
}

uint bound( uint x, uint a, uint b )
{
	return x < a ? a : ( (x > b ? b : x) );
}

int COLOR_RGBA( uint8 r, uint8 g, uint8 b, uint8 a )
{
	return (( r & 255 ) << 0 ) | (( g & 255 ) << 8 ) | (( b & 255 ) << 16 ) | (( a & 255 ) << 24 );
}

int COLOR_ZEROA( int c )
{
	return c & ( ( 1 << 24 ) - 1);
}

uint8 COLOR_A( int c )
{
	return (c>>24) & 255;
}

int COLOR_REPLACEA( int c, uint8 a )
{
	return COLOR_ZEROA( c ) | (( a & 255 ) << 24 );
}

int ColorIndex( uint8 c ) {
	uint i = uint( c - '0' );
	if( i >= colorTable.length() ) {
		i = 7;
	}
	return int( i );
}

Vec4 ColorToVec4( int c ) {
	return Vec4( ( c & 255 ) / 255.0f,
		( (c>>8) & 255 ) / 255.0f,
		( (c>>16) & 255 ) / 255.0f,
		( (c>>24) & 255 ) / 255.0f );
}

int Vec4ToColor( const Vec4 &in v ) {
	return COLOR_RGBA( bound( 0, int(v[0] * 255.0), 255 ),
		bound( 0, int(v[1] * 255.0), 255 ),
		bound( 0, int(v[2] * 255.0), 255 ),
		bound( 0, int(v[3] * 255.0), 255 ) );
}

int ColorByIndex( int i, uint8 alpha ) {
	if( i < 0 || i >= int( colorTable.length() ) ) {
		return 0;
	}
	auto c = colorTable[i];
	int r = bound( 0, rint( c[0] * 255.0f ), 255 );
	int g = bound( 0, rint( c[1] * 255.0f ), 255 );
	int b = bound( 0, rint( c[2] * 255.0f ), 255 );
	return COLOR_RGBA( r, g, b, alpha );
}

int ColorByIndex( int i ) {
	return ColorByIndex( i, 255 );
}

Vec3 stringToVec3( const String &str )
{
	float x, y, z;
	
	x = str.getToken(0).toFloat();
	y = str.getToken(1).toFloat();
	z = str.getToken(2).toFloat();
	return Vec3( x, y, z );
}

String @vec3ToString( Vec3 vec )
{
	 return "" + vec.x + " " + vec.y + " " + vec.z;
}
