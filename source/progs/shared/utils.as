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

int COLOR_RGBA( int r, int g, int b, int a )
{
	return (( r & 255 ) << 0 ) | (( g & 255 ) << 8 ) | (( b & 255 ) << 16 ) | (( a & 255 ) << 24 );
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
