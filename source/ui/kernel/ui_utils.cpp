/*
Copyright (C) 2011 Victor Luchits

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

#include "ui_precompiled.h"
#include "ui_utils.h"

namespace WSWUI
{

// ch : note, if these are moved here they should be renamed to
// something redonkilous like RgbString2HexString and
// HexString2RgbString.. no thx.
std::string rgb2hex( const char *rgbstr )
{
	int rgb;
	int r, g, b;
	std::stringstream instream( rgbstr );
	std::stringstream outstream;
	instream >> r >> g >> b;	// to int
	rgb = ( b | (g << 8) | (r << 16) );
	outstream << "#" << std::hex << std::setw(6) << std::setfill('0') << rgb;	// to hex string
	return outstream.str();
}

std::string hex2rgb( const char *hexstr )
{
	std::stringstream instream( hexstr + 1 );	// bypass '#'
	std::stringstream outstream;
	unsigned int rgb;
	instream >> std::hex >> rgb;	// to int
	outstream << ((rgb>>16)&0xff) << " " << ((rgb>>8)&0xff) << " " << (rgb&0xff);	// to rgb string
	return outstream.str();
}

//==============================================================

const char *int_to_addr( uint64_t r )
{
	return va( "%d.%d.%d.%d:%d", r&0xff, (r>>8)&0xff, (r>>16)&0xff, (r>>24)&0xff, (r>>32)&0xffff );
}

uint64_t addr_to_int( const std::string &adr )
{
	uint64_t r = 0, acc = 0, dots = 0;

	for( size_t i = 0; i < adr.size(); i++ )
	{
		uint64_t c = adr[i];
		if( c == '.' || c == ':' )
		{
			// add to result and reset part
			r |= ( acc << ( dots << 3 ) );
			acc = 0;
			++dots;
		}
		else
		{
			acc = acc * 10 + ( c - '0' );
		}
	}

	// and the remaining part
	r |= ( acc << ( dots << 3 ) );
	// Com_Printf("addr_to_int %s %s\n", adr.c_str(), int_to_addr( r ) );
	return r;
}

//==============================================================

void tokenize( const std::string &str, char sep, std::vector<std::string> &tokens )
{
	size_t len, left, right = 0;

	tokens.clear();
	while( right != std::string::npos ) {
		left = str.find_first_not_of( sep, right );
		if( left == std::string::npos )
			break;
		right = str.find_first_of( sep, left );
		// fix length
		len = ( right == std::string::npos ? std::string::npos : (right - left) );
		tokens.push_back( str.substr( left, len ) );
	}
}

}
