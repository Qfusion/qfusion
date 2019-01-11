/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "q_arch.h"
#include "q_math.h"
#include "qcommon/qfiles.h"

//==============================================================
//
//COLLISION DETECTION
//
//==============================================================

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID          1           // an eye is never valid in a solid
#define CONTENTS_LAVA           8
#define CONTENTS_SLIME          16
#define CONTENTS_WATER          32

#define CONTENTS_AREAPORTAL     0x8000

#define CONTENTS_PLAYERCLIP     0x10000
#define CONTENTS_MONSTERCLIP    0x20000

#define CONTENTS_TELEPORTER     0x40000
#define CONTENTS_JUMPPAD        0x80000
#define CONTENTS_CLUSTERPORTAL  0x100000
#define CONTENTS_DONOTENTER     0x200000

#define CONTENTS_TEAMALPHA      0x400000
#define CONTENTS_TEAMBETA       0x800000

#define CONTENTS_ORIGIN         0x1000000   // removed before bsping an entity

#define CONTENTS_BODY           0x2000000   // should never be on a brush, only in game
#define CONTENTS_CORPSE         0x4000000
#define CONTENTS_DETAIL         0x8000000   // brushes not used for the bsp
#define CONTENTS_STRUCTURAL     0x10000000  // brushes used for the bsp
#define CONTENTS_TRANSLUCENT    0x20000000  // don't consume surface fragments inside
#define CONTENTS_TRIGGER        0x40000000
#define CONTENTS_NODROP         0x80000000  // don't leave bodies or items (death fog, lava)

// content masks
#define MASK_ALL            ( -1 )
#define MASK_SOLID          ( CONTENTS_SOLID )
#define MASK_PLAYERSOLID    ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY )
#define MASK_DEADSOLID      ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP )
#define MASK_MONSTERSOLID   ( CONTENTS_SOLID | CONTENTS_MONSTERCLIP | CONTENTS_BODY )
#define MASK_WATER          ( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME )
#define MASK_OPAQUE         ( CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA )
#define MASK_SHOT           ( CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE )

#define MASK_ALPHAPLAYERSOLID  ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_TEAMBETA )
#define MASK_BETAPLAYERSOLID   ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_TEAMALPHA )

// a trace is returned when a box is swept through the world
typedef struct {
	bool allsolid;          // if true, plane is not valid
	bool startsolid;        // if true, the initial point was in a solid area
	float fraction;             // time completed, 1.0 = didn't hit anything
	vec3_t endpos;              // final position
	cplane_t plane;             // surface normal at impact
	int surfFlags;              // surface hit
	int contents;               // contents on other side of surface hit
	int ent;                    // not set by CM_*() functions
} trace_t;
