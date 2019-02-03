#pragma once

#include <stddef.h>
#include <stdint.h>

// fnv1a
uint32_t Hash32( const void * data, size_t n, uint32_t basis = UINT32_C( 2166136261 ) );
uint64_t Hash64( const void * data, size_t n, uint64_t basis = UINT64_C( 14695981039346656037 ) );

// compile time hashing
constexpr uint32_t Hash32_CT( const char * str, size_t n, uint32_t basis = UINT32_C( 2166136261 ) ) {
	return n == 0 ? basis : Hash32_CT( str + 1, n - 1, ( basis ^ str[ 0 ] ) * UINT32_C( 16777619 ) );
}

constexpr uint64_t Hash64_CT( const char * str, size_t n, uint64_t basis = UINT64_C( 14695981039346656037 ) ) {
	return n == 0 ? basis : Hash64_CT( str + 1, n - 1, ( basis ^ str[ 0 ] ) * UINT64_C( 1099511628211 ) );
}
