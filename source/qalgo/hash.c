#include "hash.h"

uint32_t fnv1a32( const void * data, size_t n ) {
	const uint32_t basis = UINT32_C( 2166136261 );
	const uint32_t prime = UINT32_C( 16777619 );

	const char * cdata = ( const char * ) data;
	uint32_t hash = basis;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}

uint64_t fnv1a64( const void * data, size_t n ) {
	const uint64_t basis = UINT64_C( 14695981039346656037 );
	const uint64_t prime = UINT64_C( 1099511628211 );

	const char * cdata = ( const char * ) data;
	uint64_t hash = basis;
	for( size_t i = 0; i < n; i++ ) {
		hash = ( hash ^ cdata[ i ] ) * prime;
	}
	return hash;
}
