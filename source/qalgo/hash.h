#pragma once

#include <stddef.h>
#include <stdint.h>

uint32_t fnv1a32( const void * data, size_t n );
uint64_t fnv1a64( const void * data, size_t n );
