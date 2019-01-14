/*
Copyright (C) 2013 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#pragma once

enum {
	VATTRIB_POSITION,
	VATTRIB_NORMAL,
	VATTRIB_SVECTOR,
	VATTRIB_COLOR0,
	VATTRIB_TEXCOORDS,
	VATTRIB_SPRITEPOINT,
	VATTRIB_BONESINDICES,
	VATTRIB_BONESWEIGHTS,
	VATTRIB_SURFINDEX,
	VATTRIB_INSTANCE_QUAT,
	VATTRIB_INSTANCE_XYZS,
	NUM_VERTEX_ATTRIBS,
};

#define VATTRIB_BIT( va )     ( 1 << ( va ) )

enum {
	VATTRIB_POSITION_BIT    = VATTRIB_BIT( 0 ),

	VATTRIB_NORMAL_BIT      = VATTRIB_BIT( 1 ),

	VATTRIB_SVECTOR_BIT     = VATTRIB_BIT( 2 ),

	VATTRIB_COLOR0_BIT      = VATTRIB_BIT( 3 ),

	VATTRIB_TEXCOORDS_BIT   = VATTRIB_BIT( 4 ),

	VATTRIB_AUTOSPRITE_BIT  = VATTRIB_BIT( 5 ),
	VATTRIB_AUTOSPRITE2_BIT = VATTRIB_BIT( 6 ) | VATTRIB_SVECTOR_BIT,

	VATTRIB_BONESINDICES_BIT = VATTRIB_BIT( 7 ),
	VATTRIB_BONESWEIGHTS_BIT = VATTRIB_BIT( 8 ),

	VATTRIB_BONES_BITS      = VATTRIB_BONESINDICES_BIT | VATTRIB_BONESWEIGHTS_BIT,

	VATTRIB_SURFINDEX_BIT   = VATTRIB_BIT( 9 ),

	VATTRIB_COLORS_BITS     = VATTRIB_COLOR0_BIT,

	VATTRIB_INSTANCE_QUAT_BIT = VATTRIB_BIT( 10 ),
	VATTRIB_INSTANCE_XYZS_BIT = VATTRIB_BIT( 11 ),

	VATTRIB_INSTANCES_BITS  = VATTRIB_INSTANCE_QUAT_BIT | VATTRIB_INSTANCE_XYZS_BIT,

	VATTRIBS_MASK           = VATTRIB_BIT( 16 ) - 1
};

typedef unsigned int vattribmask_t;

#define FLOAT_VATTRIB_TYPE( vattrib,halfFloatVattribs ) \
	( (int)( halfFloatVattribs & vattrib ) == vattrib ? GLhalfARB : float )

#define FLOAT_VATTRIB_GL_TYPE( vattrib,halfFloatVattribs ) \
	( (int)( halfFloatVattribs & vattrib ) == vattrib ? GL_HALF_FLOAT : GL_FLOAT )

#define FLOAT_VATTRIB_SIZE( vattrib,halfFloatVattribs ) \
	( (int)( halfFloatVattribs & vattrib ) == vattrib ? sizeof( GLhalfARB ) : sizeof( float ) )
