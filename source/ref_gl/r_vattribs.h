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
#ifndef __R_VATTRIBS_H__
#define __R_VATTRIBS_H__

typedef enum vattrib_e
{
	VATTRIB_POSITION		= 0,

	VATTRIB_NORMAL			= 1,

	VATTRIB_SVECTOR			= 2,

	VATTRIB_COLOR0			= 3,

	VATTRIB_TEXCOORDS		= 4,

	VATTRIB_SPRITEPOINT		= 5,

	VATTRIB_BONESINDICES	= 6,
	VATTRIB_BONESWEIGHTS	= 7,

	VATTRIB_COLOR1			= 6,
	VATTRIB_COLOR2			= 7,
	VATTRIB_COLOR3			= 8,

	VATTRIB_LMCOORDS0		= 6,
	VATTRIB_LMCOORDS1		= 7,
	VATTRIB_LMCOORDS2		= 8,
	VATTRIB_LMCOORDS3		= 9,

	VATTRIB_INSTANCE_QUAT	= 10,
	VATTRIB_INSTANCE_XYZS	= 11,

	NUM_VERTEX_ATTRIBS		= 12
} vattrib_t;

#define VATTRIB_BIT(va)		(1<<(va))

typedef enum vattribbit_e
{
	VATTRIB_POSITION_BIT	= VATTRIB_BIT(0),

	VATTRIB_NORMAL_BIT		= VATTRIB_BIT(1),

	VATTRIB_SVECTOR_BIT		= VATTRIB_BIT(2),

	VATTRIB_COLOR0_BIT		= VATTRIB_BIT(3),

	VATTRIB_TEXCOORDS_BIT	= VATTRIB_BIT(4),

	VATTRIB_AUTOSPRITE_BIT	= VATTRIB_BIT(5),
	VATTRIB_AUTOSPRITE2_BIT	= VATTRIB_BIT(6) | VATTRIB_SVECTOR_BIT,

	VATTRIB_BONESINDICES_BIT= VATTRIB_BIT(7),
	VATTRIB_BONESWEIGHTS_BIT= VATTRIB_BIT(8),

	VATTRIB_BONES_BITS		= VATTRIB_BONESINDICES_BIT|VATTRIB_BONESWEIGHTS_BIT,

	VATTRIB_COLOR1_BIT		= VATTRIB_BIT(9),
	VATTRIB_COLOR2_BIT		= VATTRIB_BIT(10),
	VATTRIB_COLOR3_BIT		= VATTRIB_BIT(11),

	VATTRIB_COLORS_BITS		= VATTRIB_COLOR0_BIT|VATTRIB_COLOR1_BIT|VATTRIB_COLOR2_BIT|VATTRIB_COLOR3_BIT,

	VATTRIB_LMCOORDS0_BIT	= VATTRIB_BIT(12),
	VATTRIB_LMCOORDS1_BIT	= VATTRIB_BIT(13),
	VATTRIB_LMCOORDS2_BIT	= VATTRIB_BIT(14),
	VATTRIB_LMCOORDS3_BIT	= VATTRIB_BIT(15),

	VATTRIB_LMCOORDS_BITS	= VATTRIB_LMCOORDS0_BIT|VATTRIB_LMCOORDS1_BIT|VATTRIB_LMCOORDS2_BIT|VATTRIB_LMCOORDS3_BIT,

	VATTRIB_INSTANCE_QUAT_BIT= VATTRIB_BIT(16),
	VATTRIB_INSTANCE_XYZS_BIT= VATTRIB_BIT(17),

	VATTRIB_INSTANCES_BITS	= VATTRIB_INSTANCE_QUAT_BIT|VATTRIB_INSTANCE_XYZS_BIT,

	VATTRIBS_MASK			= VATTRIB_BIT(18)-1
} vattribbit_t;

typedef unsigned int vattribmask_t;

#define FLOAT_VATTRIB_TYPE(vattrib,halfFloatVattribs) \
	((int)(halfFloatVattribs & vattrib) == vattrib ? GL_HALF_FLOAT : GL_FLOAT)

#define FLOAT_VATTRIB_SIZE(vattrib,halfFloatVattribs) \
	(FLOAT_VATTRIB_TYPE(vattrib,halfFloatVattribs) == GL_HALF_FLOAT ? sizeof( GLhalfARB ) : sizeof( float ))

#endif
