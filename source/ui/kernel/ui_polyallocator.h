/*
 * PolyAllocator.h
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#ifndef POLY_ALLOCATOR_H_
#define POLY_ALLOCATOR_H_

#include "../cgame/ref.h"

// PolyAllocator holds a poly that is always at least the required size.
class PolyAllocator
{
public:
	PolyAllocator();
	virtual ~PolyAllocator();

	poly_t *alloc( int numverts, int numelems );
	void free( poly_t *poly );

	poly_t *get_temp( int numverts, int numelems );

private:
	void assignPointers( poly_t *p, unsigned char *b );
	size_t sizeForPolyData( int numverts, int numelems );

	poly_t poly_temp;
	unsigned char *base_temp;
	size_t size_temp;
};

#endif /* POLY_ALLOCATOR_H_ */
