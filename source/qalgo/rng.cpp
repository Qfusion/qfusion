/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

#include <assert.h>

#include "rng.h"

PCG new_pcg() {
	PCG pcg;
	pcg.state = UINT64_C( 0x853c49e6748fea9b );
	pcg.inc = UINT64_C( 0xda3e39cb94b95bdb );
	return pcg;
}

PCG new_pcg( uint64_t state, uint64_t seq ) {
	PCG pcg;
	pcg.state = 0;
	pcg.inc = ( seq << 1 ) | 1;
	random_u32( &pcg );
	pcg.state += state;
	random_u32( &pcg );
	return pcg;
}

uint32_t random_u32( PCG * pcg ) {
	uint64_t oldstate = pcg->state;
	pcg->state = oldstate * UINT64_C( 6364136223846793005 ) + pcg->inc;
	uint32_t xorshifted = uint32_t( ( ( oldstate >> 18 ) ^ oldstate ) >> 27 );
	uint32_t rot = uint32_t( oldstate >> 59 );
	return ( xorshifted >> rot ) | ( xorshifted << ( ( -rot ) & 31 ) );
}

uint64_t random_u64( PCG * pcg ) {
	uint64_t hi = uint64_t( random_u32( pcg ) ) << uint64_t( 31 );
	return hi | random_u32( pcg );
}

// http://www.pcg-random.org/posts/bounded-rands.html
int random_uniform( PCG * pcg, int lo, int hi ) {
	assert( lo <= hi );
	uint32_t range = uint32_t( hi ) - uint32_t( lo );
	uint32_t x = random_u32( pcg );

	uint64_t m = uint64_t( x ) * uint64_t( range );
	uint32_t l = uint32_t( m );
	if( l < range ) {
		uint32_t t = -range;
		if( t >= range ) {
			t -= range;
			if( t >= range )
				t %= range;
		}
		while( l < t ) {
			x = random_u32( pcg );
			m = uint64_t( x ) * uint64_t( range );
			l = uint32_t( m );
		}
	}
	return lo + ( m >> 32 );
}

float random_float( PCG * pcg ) {
        return float( random_u32( pcg ) ) / ( float( UINT32_MAX ) + 1 );
}

double random_double( PCG * pcg ) {
        uint64_t r64 = ( uint64_t( random_u32( pcg ) ) << 32 ) | random_u32( pcg );
        uint64_t r53 = r64 & ( ( uint64_t( 1 ) << 53 ) - 1 );

        return double( r53 ) / double( ( uint64_t( 1 ) << 53 ) + 1 );
}

bool random_p( PCG * pcg, float p ) {
	return random_u32( pcg ) < uint32_t( p * UINT32_MAX );
}
