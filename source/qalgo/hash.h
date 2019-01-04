#pragma once

#include <stddef.h>
#include <stdint.h>

// fnv1a
uint32_t Hash32( const void * data, size_t n, uint32_t basis = UINT32_C( 2166136261 ) );
uint64_t Hash64( const void * data, size_t n, uint64_t basis = UINT64_C( 14695981039346656037 ) );
