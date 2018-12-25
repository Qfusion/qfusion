#pragma once

#include <stddef.h>
#include <stdint.h>

// fnv1a
uint32_t Hash32( const void * data, size_t n );
uint64_t Hash64( const void * data, size_t n );
