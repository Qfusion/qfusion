/*
 * PolyAllocator.cpp
 *
 *  Created on: 26.6.2011
 *      Author: hc
 */

#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_polyallocator.h"

PolyAllocator::PolyAllocator()
{
	memset( &poly_temp, 0, sizeof( poly_temp ) );
	base_temp = 0;
	size_temp = 0;
}

PolyAllocator::~PolyAllocator()
{
	// TODO Auto-generated destructor stub
	if( base_temp != 0 )
		__delete__( base_temp );
}

void PolyAllocator::assignPointers( poly_t *p, unsigned char *b )
{
	// rely that base is set
	p->verts = ( vec4_t* )b;
	p->normals = ( vec4_t* )( p->verts + p->numverts );
	p->stcoords = ( vec2_t* )( p->normals + p->numverts );
	p->colors = ( byte_vec4_t* )( p->stcoords + p->numverts );
	p->elems = ( unsigned short* )( p->colors + p->numverts );
}

size_t PolyAllocator::sizeForPolyData( int numverts, int numelems )
{
	return numverts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + sizeof( byte_vec4_t ) ) +
		numelems * sizeof( unsigned short );
}

poly_t *PolyAllocator::get_temp( int numverts, int numelems )
{
	size_t newsize;

	newsize = sizeForPolyData( numverts, numelems );
	if( size_temp < newsize || !base_temp )
	{
		if( base_temp != 0 ) {
			__delete__( base_temp );
		}

		base_temp = __newa__( unsigned char, newsize );
		size_temp = newsize;
	}

	poly_temp.numverts = numverts;
	poly_temp.numelems = numelems;
	assignPointers( &poly_temp, base_temp );
	return &poly_temp;
}

poly_t *PolyAllocator::alloc( int numverts, int numelems )
{
	size_t size;

	size = sizeForPolyData( numverts, numelems ) + sizeof( poly_t );
	unsigned char *base = __newa__( unsigned char, size );

	poly_t *poly = ( poly_t * )base;
	poly->numverts = numverts;
	poly->numelems = numelems;
	assignPointers( poly, base + sizeof( poly_t ) );
	return poly;
}

void PolyAllocator::free( poly_t *poly )
{
	__delete__( poly );
}
