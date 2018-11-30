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
 *     http://www.pcg-random.org
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

struct PCG {
	// RNG state. All values are possible.
	uint64_t state; 
	// Controls which RNG sequence (stream) is selected. Must *always* be odd.
	uint64_t inc;   
};

PCG new_pcg();
PCG new_pcg( uint64_t state, uint64_t seq );

// return a random number in [0, 2^32)
uint32_t random_u32( PCG * pcg );

// return a random number in [0, 2^64)
uint64_t random_u64( PCG * pcg );

// return a random number in [lo, hi)
int random_uniform( PCG * pcg, int lo, int hi );

// return a random float in [0, 1)
float random_float( PCG * pcg );

// return a random double in [0, 1)
double random_double( PCG * pcg );

// returns true with probability p
bool random_p( PCG * pcg, float p );

template< typename T, size_t N >
T * random_select( PCG * pcg, T * ( &arr )[ N ] ) {
	return arr[ random_uniform( pcg, 0, N ) ];
}
