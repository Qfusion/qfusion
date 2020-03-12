/*
Copyright 2004-2008 Paul Hsieh 

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "hash.h"

#undef get16bits
#if ( defined( __GNUC__ ) && defined( __i386__ ) ) || defined( __WATCOMC__ ) \
	|| defined( _MSC_VER ) || defined ( __BORLANDC__ ) || defined ( __TURBOC__ )
#define get16bits( d ) ( *( (const unsigned short *) ( d ) ) )
#endif

#if !defined ( get16bits )
#define get16bits( d ) ( ( ( (unsigned int)( ( (const unsigned char *)( d ) )[1] ) ) << 8 ) \
						 + (unsigned int)( ( (const unsigned char *)( d ) )[0] ) )
#endif

#define Com_SuperFastHash_Avalanche( hash ) \
	hash ^= hash << 3; \
	hash += hash >> 5; \
	hash ^= hash << 4; \
	hash += hash >> 17; \
	hash ^= hash << 25; \
	hash += hash >> 6;

/*
* COM_SuperFastHash
*
* Adaptation of Paul Hsieh's incremental SuperFastHash function.
* Initialize hash to some non-zero value for the first call, like len.
*/
unsigned int COM_SuperFastHash( const uint8_t * data, size_t len ) {
	unsigned int tmp;
	unsigned int rem;
	unsigned int hash = (unsigned int)len;

	if( len <= 0 || data == NULL ) {
		return 0;
	}

	rem = len & 3;
	len >>= 2;

	// main loop
	for( ; len > 0; len-- ) {
		hash  += get16bits( data );
		tmp    = ( get16bits( data + 2 ) << 11 ) ^ hash;
		hash   = ( hash << 16 ) ^ tmp;
		data  += 2 * sizeof( unsigned short );
		hash  += hash >> 11;
	}

	// Handle end cases
	switch( rem ) {
		case 3: hash += get16bits( data );
			hash ^= hash << 16;
			hash ^= data[sizeof( unsigned short )] << 18;
			hash += hash >> 11;
			break;
		case 2: hash += get16bits( data );
			hash ^= hash << 11;
			hash += hash >> 17;
			break;
		case 1: hash += *data;
			hash ^= hash << 10;
			hash += hash >> 1;
	}

	// Force "avalanching" of final 127 bits
	Com_SuperFastHash_Avalanche( hash );

	return hash;
}

/*
* COM_SuperFastHash64BitInt
*
* Com_SuperFastHash that takes a 64bit integer as an argument
*/
unsigned int COM_SuperFastHash64BitInt( uint64_t data ) {
	unsigned int hash;
	unsigned int il, ih;
	unsigned int tmp;

	hash = sizeof( data );

	// split into two
	il = data & ULONG_MAX;
	ih = ( data >> 32 ) & ULONG_MAX;

	hash += ( il & 0xFFFF );
	tmp = ( ( ( il >> 16 ) & 0xFFFF ) << 11 ) ^ hash;
	hash = ( hash << 16 ) ^ tmp;
	hash += hash >> 11;

	hash += ( ih & 0xFFFF );
	tmp = ( ( ( ih >> 16 ) & 0xFFFF ) << 11 ) ^ hash;
	hash = ( hash << 16 ) ^ tmp;
	hash += hash >> 11;

	// Force "avalanching" of final 127 bits
	Com_SuperFastHash_Avalanche( hash );

	return hash;
}
