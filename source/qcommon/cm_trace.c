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
// cmodel_trace.c

#include "qcommon.h"
#include "cm_local.h"

typedef struct {
	int leaf_topnode;
	int leaf_count, leaf_maxcount;
	int *leaf_list;
	float *leaf_mins, *leaf_maxs;
} boxLeafsWork_t;

typedef struct {
	bool ispoint;
	int contents;
	int checkcount;
	float realfraction;

	vec3_t extents;

	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t startmins, startmaxs;
	vec3_t endmins, endmaxs;
	vec3_t absmins, absmaxs;

	cmodel_state_t *cms;
	cmodel_t *cmodel;

	int negativedir[3];
	vec3_t totalmove;

	trace_t *trace;

	int nummarkbrushes;
	cbrush_t *brushes;
	int *markbrushes;

	int nummarkfaces;
	cface_t *faces;
	int *markfaces;

	int *brush_checkcounts;
	int *face_checkcounts;
} traceWork_t;

struct bihbox_s {
	vec3_t min;
	vec3_t max;
};

static void CM_RecursiveBIHTest( traceWork_t *tw );
static void CM_RecursiveBIHTrace( traceWork_t *tw );
static int CM_PointContentsBIH( const bihnode_t *node, const vec3_t p );

/*
 * CM_InitBoxHull
 *
 * Set up the planes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
void CM_InitBoxHull( cmodel_state_t *cms )
{
	int i;
	cplane_t *p;
	cbrushside_t *s;

	cms->box_brush->numsides = 6;
	cms->box_brush->brushsides = cms->box_brushsides;
	cms->box_brush->contents = CONTENTS_BODY;

	// Make sure CM_CollideBox() will not reject the brush by its bounds
	ClearBounds( cms->box_brush->maxs, cms->box_brush->mins );

	cms->box_markbrushes[0] = 0;

	cms->box_cmodel->brushes = cms->box_brush;
	cms->box_cmodel->builtin = true;
	cms->box_cmodel->nummarkfaces = 0;
	cms->box_cmodel->markfaces = NULL;
	cms->box_cmodel->markbrushes = cms->box_markbrushes;
	cms->box_cmodel->nummarkbrushes = 1;

	for( i = 0; i < 6; i++ ) {
		// brush sides
		s = cms->box_brushsides + i;
		s->surfFlags = 0;

		// planes
		p = &s->plane;
		VectorClear( p->normal );

		if( ( i & 1 ) ) {
			p->type = PLANE_NONAXIAL;
			p->normal[i >> 1] = -1;
			p->signbits = ( 1 << ( i >> 1 ) );
		} else {
			p->type = i >> 1;
			p->normal[i >> 1] = 1;
			p->signbits = 0;
		}
	}
}

/*
 * CM_InitOctagonHull
 *
 * Set up the planes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
void CM_InitOctagonHull( cmodel_state_t *cms )
{
	int i;
	cplane_t *p;
	cbrushside_t *s;
	const vec3_t oct_dirs[4] = { { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 } };

	cms->oct_brush->numsides = 10;
	cms->oct_brush->brushsides = cms->oct_brushsides;
	cms->oct_brush->contents = CONTENTS_BODY;

	// Make sure CM_CollideBox() will not reject the brush by its bounds
	ClearBounds( cms->oct_brush->maxs, cms->oct_brush->mins );

	cms->oct_markbrushes[0] = 0;

	cms->oct_cmodel->brushes = cms->oct_brush;
	cms->oct_cmodel->builtin = true;
	cms->oct_cmodel->nummarkfaces = 0;
	cms->oct_cmodel->markfaces = NULL;
	cms->oct_cmodel->markbrushes = cms->oct_markbrushes;
	cms->oct_cmodel->nummarkbrushes = 1;

	// axial planes
	for( i = 0; i < 6; i++ ) {
		// brush sides
		s = cms->oct_brushsides + i;
		s->surfFlags = 0;

		// planes
		p = &s->plane;
		VectorClear( p->normal );

		if( ( i & 1 ) ) {
			p->type = PLANE_NONAXIAL;
			p->normal[i >> 1] = -1;
			p->signbits = ( 1 << ( i >> 1 ) );
		} else {
			p->type = i >> 1;
			p->normal[i >> 1] = 1;
			p->signbits = 0;
		}
	}

	// non-axial planes
	for( i = 6; i < 10; i++ ) {
		// brush sides
		s = cms->oct_brushsides + i;
		s->surfFlags = 0;

		// planes
		p = &s->plane;
		VectorCopy( oct_dirs[i - 6], p->normal );

		p->type = PLANE_NONAXIAL;
		p->signbits = SignbitsForPlane( p );
	}
}

/*
 * CM_ModelForBBox
 *
 * To keep everything totally uniform, bounding boxes are turned into inline models
 */
cmodel_t *CM_ModelForBBox( cmodel_state_t *cms, vec3_t mins, vec3_t maxs )
{
	cms->box_brushsides[0].plane.dist = maxs[0];
	cms->box_brushsides[1].plane.dist = -mins[0];
	cms->box_brushsides[2].plane.dist = maxs[1];
	cms->box_brushsides[3].plane.dist = -mins[1];
	cms->box_brushsides[4].plane.dist = maxs[2];
	cms->box_brushsides[5].plane.dist = -mins[2];

	VectorCopy( mins, cms->box_cmodel->mins );
	VectorCopy( maxs, cms->box_cmodel->maxs );

	return cms->box_cmodel;
}

/*
 * CM_OctagonModelForBBox
 *
 * Same as CM_ModelForBBox with 4 additional planes at corners.
 * Internally offset to be symmetric on all sides.
 */
cmodel_t *CM_OctagonModelForBBox( cmodel_state_t *cms, vec3_t mins, vec3_t maxs )
{
	int i;
	float a, b, d, t;
	float sina, cosa;
	vec3_t offset, size[2];

	for( i = 0; i < 3; i++ ) {
		offset[i] = ( mins[i] + maxs[i] ) * 0.5;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
	}

	VectorCopy( offset, cms->oct_cmodel->cyl_offset );
	VectorCopy( size[0], cms->oct_cmodel->mins );
	VectorCopy( size[1], cms->oct_cmodel->maxs );

	cms->oct_brushsides[0].plane.dist = size[1][0];
	cms->oct_brushsides[1].plane.dist = -size[0][0];
	cms->oct_brushsides[2].plane.dist = size[1][1];
	cms->oct_brushsides[3].plane.dist = -size[0][1];
	cms->oct_brushsides[4].plane.dist = size[1][2];
	cms->oct_brushsides[5].plane.dist = -size[0][2];

	a = size[1][0];			   // halfx
	b = size[1][1];			   // halfy
	d = sqrt( a * a + b * b ); // hypothenuse

	cosa = a / d;
	sina = b / d;

	// swap sin and cos, which is the same thing as adding pi/2 radians to the original angle
	t = sina;
	sina = cosa;
	cosa = t;

	// elleptical radius
	d = a * b / sqrt( a * a * cosa * cosa + b * b * sina * sina );
	// d = a * b / sqrt( a * a  + b * b ); // produces a rectangle, inscribed at middle points

	// the following should match normals and signbits set in CM_InitOctagonHull

	VectorSet( cms->oct_brushsides[6].plane.normal, cosa, sina, 0 );
	cms->oct_brushsides[6].plane.dist = d;

	VectorSet( cms->oct_brushsides[7].plane.normal, -cosa, sina, 0 );
	cms->oct_brushsides[7].plane.dist = d;

	VectorSet( cms->oct_brushsides[8].plane.normal, -cosa, -sina, 0 );
	cms->oct_brushsides[8].plane.dist = d;

	VectorSet( cms->oct_brushsides[9].plane.normal, cosa, -sina, 0 );
	cms->oct_brushsides[9].plane.dist = d;

	return cms->oct_cmodel;
}

/*
 * CM_PointLeafnum
 */
int CM_PointLeafnum( cmodel_state_t *cms, const vec3_t p )
{
	int num = 0;
	cnode_t *node;

	if( !cms->numplanes ) {
		return 0; // sound may call this without map loaded
	}
	do {
		node = cms->map_nodes + num;
		num = node->children[PlaneDiff( p, node->plane ) < 0];
	} while( num >= 0 );

	return -1 - num;
}

/*
 * CM_BoxLeafnums
 *
 * Fills in a list of all the leafs touched
 */
static void CM_BoxLeafnums_r( boxLeafsWork_t *bw, cmodel_state_t *cms, int nodenum )
{
	int s;
	cnode_t *node;

	while( nodenum >= 0 ) {
		node = &cms->map_nodes[nodenum];
		s = BOX_ON_PLANE_SIDE( bw->leaf_mins, bw->leaf_maxs, node->plane ) - 1;

		if( s < 2 ) {
			nodenum = node->children[s];
			continue;
		}

		// go down both sides
		if( bw->leaf_topnode == -1 ) {
			bw->leaf_topnode = nodenum;
		}
		CM_BoxLeafnums_r( bw, cms, node->children[0] );
		nodenum = node->children[1];
	}

	if( bw->leaf_count < bw->leaf_maxcount ) {
		bw->leaf_list[bw->leaf_count++] = -1 - nodenum;
	}
}

/*
 * CM_BoxLeafnums
 */
int CM_BoxLeafnums( cmodel_state_t *cms, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode )
{
	boxLeafsWork_t bw;

	bw.leaf_list = list;
	bw.leaf_count = 0;
	bw.leaf_maxcount = listsize;
	bw.leaf_mins = mins;
	bw.leaf_maxs = maxs;
	bw.leaf_topnode = -1;

	CM_BoxLeafnums_r( &bw, cms, 0 );

	if( topnode ) {
		*topnode = bw.leaf_topnode;
	}

	return bw.leaf_count;
}

/*
 * CM_RoundUpToHullSize
 */
void CM_RoundUpToHullSize( cmodel_state_t *cms, vec3_t mins, vec3_t maxs, cmodel_t *cmodel )
{
	if( !cmodel ) {
		cmodel = cms->map_cmodels;
	}

	// special rounding code
	if( !cmodel->builtin && cms->CM_RoundUpToHullSize ) {
		cms->CM_RoundUpToHullSize( cms, mins, maxs, cmodel );
		return;
	}
}

/*
 * CM_BrushContents
 */
static inline int CM_BrushContents( cbrush_t *brush, vec3_t p )
{
	int i;
	cbrushside_t *brushside;

	for( i = 0, brushside = brush->brushsides; i < brush->numsides; i++, brushside++ )
		if( PlaneDiff( p, &brushside->plane ) > 0 ) {
			return 0;
		}

	return brush->contents;
}

/*
 * CM_PatchContents
 */
static inline int CM_PatchContents( cface_t *patch, vec3_t p )
{
	int i, c;
	cbrush_t *facet;

	for( i = 0, facet = patch->facets; i < patch->numfacets; i++, facet++ )
		if( ( c = CM_BrushContents( facet, p ) ) ) {
			return c;
		}

	return 0;
}

/*
 * CM_PointContents
 */
static int CM_PointContents( cmodel_state_t *cms, vec3_t p, cmodel_t *cmodel )
{
	int i, superContents, contents;
	int nummarkfaces, nummarkbrushes;
	cface_t *faces;
	int *markface;
	cbrush_t *brushes;
	int *markbrush;

	if( !cms->numnodes ) { // map not loaded
		return 0;
	}

	c_pointcontents++; // optimize counter

	if( cmodel->bihnodes ) {
		return CM_PointContentsBIH( cmodel->bihnodes, p );
	}

	if( cmodel == cms->map_cmodels ) {
		cleaf_t *leaf;

		leaf = &cms->map_leafs[CM_PointLeafnum( cms, p )];
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	} else {
		superContents = ~0;

		markbrush = cmodel->markbrushes;
		nummarkbrushes = cmodel->nummarkbrushes;

		markface = cmodel->markfaces;
		nummarkfaces = cmodel->nummarkfaces;
	}

	contents = superContents;
	brushes = cmodel->brushes;
	faces = cmodel->faces;

	for( i = 0; i < nummarkbrushes; i++ ) {
		cbrush_t *brush = brushes + markbrush[i];

		// check if brush adds something to contents
		if( contents & brush->contents ) {
			if( !( contents &= ~CM_BrushContents( brush, p ) ) ) {
				return superContents;
			}
		}
	}

	if( !cm_noCurves->integer ) {
		for( i = 0; i < nummarkfaces; i++ ) {
			cface_t *patch = faces + markface[i];

			// check if patch adds something to contents
			if( contents & patch->contents ) {
				if( BoundsOverlap( p, p, patch->mins, patch->maxs ) ) {
					if( !( contents &= ~CM_PatchContents( patch, p ) ) ) {
						return superContents;
					}
				}
			}
		}
	}

	return ~contents & superContents;
}

/*
 * CM_TransformedPointContents
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
int CM_TransformedPointContents( cmodel_state_t *cms, vec3_t p, cmodel_t *cmodel, vec3_t origin, vec3_t angles )
{
	vec3_t p_l;

	if( !cms->numnodes ) { // map not loaded
		return 0;
	}

	if( !cmodel || cmodel == cms->map_cmodels ) {
		cmodel = cms->map_cmodels;
		origin = vec3_origin;
		angles = vec3_origin;
	} else {
		if( !origin ) {
			origin = vec3_origin;
		}
		if( !angles ) {
			angles = vec3_origin;
		}
	}

	// special point contents code
	if( !cmodel->builtin && cms->CM_TransformedPointContents ) {
		return cms->CM_TransformedPointContents( cms, p, cmodel, origin, angles );
	}

	// subtract origin offset
	VectorSubtract( p, origin, p_l );

	// rotate start and end into the models frame of reference
	if( ( angles[0] || angles[1] || angles[2] ) && !cmodel->builtin ) {
		vec3_t temp;
		mat3_t axis;

		AnglesToAxis( angles, axis );
		VectorCopy( p_l, temp );
		Matrix3_TransformVector( axis, temp, p_l );
	}

	return CM_PointContents( cms, p_l, cmodel );
}

/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON ( 1.0f / 32.0f )

/*
 * CM_ClipBoxToBrush
 */
static void CM_ClipBoxToBrush( traceWork_t *tw, const cbrush_t *brush )
{
	int i;
	const cplane_t *p, *clipplane;
	float enterfrac = -1, leavefrac = 1;
	float enterfrac2 = -1;
	float d1, d2, f;
	bool getout, startout;
	const cbrushside_t *side, *leadside;

	if( !brush->numsides ) {
		return;
	}

	clipplane = NULL;

	c_brush_traces++;

	getout = false;
	startout = false;
	leadside = NULL;
	side = brush->brushsides;

	for( i = 0; i < brush->numsides; i++, side++ ) {
		p = &side->plane;

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 ) {
			d1 = tw->startmins[p->type] - p->dist;
			d2 = tw->endmins[p->type] - p->dist;
		} else {
			switch( p->signbits ) {
				case 0:
					d1 = p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmins[1] +
						 p->normal[2] * tw->startmins[2] - p->dist;
					d2 = p->normal[0] * tw->endmins[0] + p->normal[1] * tw->endmins[1] + p->normal[2] * tw->endmins[2] -
						 p->dist;
					break;
				case 1:
					d1 = p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmins[1] +
						 p->normal[2] * tw->startmins[2] - p->dist;
					d2 = p->normal[0] * tw->endmaxs[0] + p->normal[1] * tw->endmins[1] + p->normal[2] * tw->endmins[2] -
						 p->dist;
					break;
				case 2:
					d1 = p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmaxs[1] +
						 p->normal[2] * tw->startmins[2] - p->dist;
					d2 = p->normal[0] * tw->endmins[0] + p->normal[1] * tw->endmaxs[1] + p->normal[2] * tw->endmins[2] -
						 p->dist;
					break;
				case 3:
					d1 = p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmaxs[1] +
						 p->normal[2] * tw->startmins[2] - p->dist;
					d2 = p->normal[0] * tw->endmaxs[0] + p->normal[1] * tw->endmaxs[1] + p->normal[2] * tw->endmins[2] -
						 p->dist;
					break;
				case 4:
					d1 = p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmins[1] +
						 p->normal[2] * tw->startmaxs[2] - p->dist;
					d2 = p->normal[0] * tw->endmins[0] + p->normal[1] * tw->endmins[1] + p->normal[2] * tw->endmaxs[2] -
						 p->dist;
					break;
				case 5:
					d1 = p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmins[1] +
						 p->normal[2] * tw->startmaxs[2] - p->dist;
					d2 = p->normal[0] * tw->endmaxs[0] + p->normal[1] * tw->endmins[1] + p->normal[2] * tw->endmaxs[2] -
						 p->dist;
					break;
				case 6:
					d1 = p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmaxs[1] +
						 p->normal[2] * tw->startmaxs[2] - p->dist;
					d2 = p->normal[0] * tw->endmins[0] + p->normal[1] * tw->endmaxs[1] + p->normal[2] * tw->endmaxs[2] -
						 p->dist;
					break;
				case 7:
					d1 = p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmaxs[1] +
						 p->normal[2] * tw->startmaxs[2] - p->dist;
					d2 = p->normal[0] * tw->endmaxs[0] + p->normal[1] * tw->endmaxs[1] + p->normal[2] * tw->endmaxs[2] -
						 p->dist;
					break;
				default:
					d1 = d2 = 0; // shut up compiler
					assert( 0 );
					break;
			}
		}

		if( d2 > 0 ) {
			getout = true; // endpoint is not in solid
		}
		if( d1 > 0 ) {
			startout = true;
		}

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 ) {
			return;
		}

		if( d1 <= 0 && d2 <= 0 ) {
			continue;
		}

		// crosses face
		f = d1 - d2;
		if( f > 0 ) { // enter
			f = d1 / f;
			if( f > enterfrac ) {
				enterfrac = f;
				clipplane = p;
				leadside = side;
				enterfrac2 = ( d1 - DIST_EPSILON ) / ( d1 - d2 ); // nudged fraction
			}
		} else if( f < 0 ) { // leave
			f = d1 / f;
			if( f < leavefrac ) {
				leavefrac = f;
			}
		}
	}

	if( !startout ) {
		// original point was inside brush
		tw->trace->startsolid = true;
		tw->contents = brush->contents;
		if( !getout ) {
			tw->realfraction = 0;
			tw->trace->allsolid = true;
			tw->trace->fraction = 0;
		}
		return;
	}

	if( enterfrac <= -1 ) {
		return;
	}
	if( enterfrac > leavefrac ) {
		return;
	}

	// check if this will reduce the collision time range
	if( enterfrac < tw->realfraction ) {
		if( enterfrac2 < tw->trace->fraction ) {
			tw->realfraction = enterfrac;
			tw->trace->plane = *clipplane;
			tw->trace->surfFlags = leadside->surfFlags;
			tw->trace->contents = brush->contents;
			tw->trace->fraction = enterfrac2;
		}
	}
}

/*
 * CM_TestBoxInBrush
 */
static void CM_TestBoxInBrush( traceWork_t *tw, const cbrush_t *brush )
{
	int i;
	const cplane_t *p;
	const cbrushside_t *side;

	if( !brush->numsides ) {
		return;
	}

	side = brush->brushsides;
	for( i = 0; i < brush->numsides; i++, side++ ) {
		p = &side->plane;

		// push the plane out appropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 ) {
			if( tw->startmins[p->type] > p->dist ) {
				return;
			}
		} else {
			switch( p->signbits ) {
				case 0:
					if( p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmins[1] +
							p->normal[2] * tw->startmins[2] >
						p->dist ) {
						return;
					}
					break;
				case 1:
					if( p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmins[1] +
							p->normal[2] * tw->startmins[2] >
						p->dist ) {
						return;
					}
					break;
				case 2:
					if( p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmaxs[1] +
							p->normal[2] * tw->startmins[2] >
						p->dist ) {
						return;
					}
					break;
				case 3:
					if( p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmaxs[1] +
							p->normal[2] * tw->startmins[2] >
						p->dist ) {
						return;
					}
					break;
				case 4:
					if( p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmins[1] +
							p->normal[2] * tw->startmaxs[2] >
						p->dist ) {
						return;
					}
					break;
				case 5:
					if( p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmins[1] +
							p->normal[2] * tw->startmaxs[2] >
						p->dist ) {
						return;
					}
					break;
				case 6:
					if( p->normal[0] * tw->startmins[0] + p->normal[1] * tw->startmaxs[1] +
							p->normal[2] * tw->startmaxs[2] >
						p->dist ) {
						return;
					}
					break;
				case 7:
					if( p->normal[0] * tw->startmaxs[0] + p->normal[1] * tw->startmaxs[1] +
							p->normal[2] * tw->startmaxs[2] >
						p->dist ) {
						return;
					}
					break;
				default:
					assert( 0 );
					return;
			}
		}
	}

	// inside this brush
	tw->trace->startsolid = tw->trace->allsolid = true;
	tw->trace->fraction = 0;
	tw->trace->contents = brush->contents;
}

/*
 * CM_CollideBox
 */
static void CM_CollideBox( traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces,
	int nummarkfaces, void ( *func )( traceWork_t *, const cbrush_t *b ) )
{
	int i, j;
	const cbrush_t *brushes = tw->brushes;
	const cface_t *faces = tw->faces;
	int checkcount = tw->checkcount;

	// trace line against all brushes
	for( i = 0; i < nummarkbrushes; i++ ) {
		int mb = markbrushes[i];
		const cbrush_t *b = brushes + mb;

		if( tw->brush_checkcounts[mb] == checkcount ) {
			continue; // already checked this brush
		}
		tw->brush_checkcounts[mb] = checkcount;

		if( !( b->contents & tw->contents ) ) {
			continue;
		}
		if( !BoundsOverlap( b->mins, b->maxs, tw->absmins, tw->absmaxs ) ) {
			continue;
		}
		func( tw, b );
		if( !tw->trace->fraction ) {
			return;
		}
	}

	if( cm_noCurves->integer || !nummarkfaces ) {
		return;
	}

	// trace line against all patches
	for( i = 0; i < nummarkfaces; i++ ) {
		int mf = markfaces[i];
		const cface_t *patch = faces + mf;
		const cbrush_t *facet;

		if( tw->face_checkcounts[mf] == checkcount ) {
			continue; // already checked this brush
		}
		tw->face_checkcounts[mf] = checkcount;

		if( !( patch->contents & tw->contents ) ) {
			continue;
		}
		if( !BoundsOverlap( patch->mins, patch->maxs, tw->absmins, tw->absmaxs ) ) {
			continue;
		}
		facet = patch->facets;
		for( j = 0; j < patch->numfacets; j++, facet++ ) {
			if( !BoundsOverlap( facet->mins, facet->maxs, tw->absmins, tw->absmaxs ) ) {
				continue;
			}
			func( tw, facet );
			if( !tw->trace->fraction ) {
				return;
			}
		}
	}
}

/*
 * CM_ClipBox
 */
static inline void CM_ClipBox(
	traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces, int nummarkfaces )
{
	CM_CollideBox( tw, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_ClipBoxToBrush );
}

/*
 * CM_TestBox
 */
static inline void CM_TestBox(
	traceWork_t *tw, const int *markbrushes, int nummarkbrushes, const int *markfaces, int nummarkfaces )
{
	CM_CollideBox( tw, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_TestBoxInBrush );
}

//======================================================================

/*
 * CM_BoxTrace
 */
static void CM_BoxTrace( traceWork_t *tw, cmodel_state_t *cms, trace_t *tr, const vec3_t start, const vec3_t end,
	const vec3_t mins, const vec3_t maxs, cmodel_t *cmodel, const vec3_t origin, int brushmask )
{
	int i;
	bool world = ( cmodel == cms->map_cmodels ? true : false );

	c_traces++; // for statistics, may be zeroed

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );
	tr->fraction = 1;

	if( !cms->numnodes ) { // map not loaded
		return;
	}

	cms->checkcount++; // for multi-check avoidance

	memset( tw, 0, sizeof( *tw ) );
	// the epsilon considers blockers with realfraction == 1 and nudged fraction < 1
	tw->realfraction = 1 + DIST_EPSILON;
	tw->checkcount = cms->checkcount;
	tw->trace = tr;
	tw->contents = brushmask;
	tw->cms = cms;
	tw->cmodel = cmodel;
	VectorCopy( start, tw->start );
	VectorCopy( end, tw->end );
	VectorCopy( mins, tw->mins );
	VectorCopy( maxs, tw->maxs );
	VectorSubtract( end, start, tw->totalmove );
	for( i = 0; i < 3; i++ )
		tw->negativedir[i] = ( end[i] - start[i] ) < 0;

	// build a bounding box of the entire move
	ClearBounds( tw->absmins, tw->absmaxs );

	VectorAdd( start, tw->mins, tw->startmins );
	AddPointToBounds( tw->startmins, tw->absmins, tw->absmaxs );

	VectorAdd( start, tw->maxs, tw->startmaxs );
	AddPointToBounds( tw->startmaxs, tw->absmins, tw->absmaxs );

	VectorAdd( end, tw->mins, tw->endmins );
	AddPointToBounds( tw->endmins, tw->absmins, tw->absmaxs );

	VectorAdd( end, tw->maxs, tw->endmaxs );
	AddPointToBounds( tw->endmaxs, tw->absmins, tw->absmaxs );

	tw->brushes = cmodel->brushes;
	tw->faces = cmodel->faces;

	if( cmodel == cms->oct_cmodel ) {
		tw->brush_checkcounts = &cms->oct_checkcount;
		tw->face_checkcounts = NULL;
	} else if( cmodel == cms->box_cmodel ) {
		tw->brush_checkcounts = &cms->box_checkcount;
		tw->face_checkcounts = NULL;
	} else {
		tw->brush_checkcounts = cms->map_brush_checkcheckouts;
		tw->face_checkcounts = cms->map_face_checkcheckouts;
	}

	//
	// check for point special case
	//
	if( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) ) {
		tw->ispoint = true;
		VectorClear( tw->extents );
	} else {
		tw->ispoint = false;
		VectorSet( tw->extents, -mins[0] > maxs[0] ? -mins[0] : maxs[0], -mins[1] > maxs[1] ? -mins[1] : maxs[1],
			-mins[2] > maxs[2] ? -mins[2] : maxs[2] );
	}

	VectorAdd( tw->absmins, tv( -1, -1, -1 ), tw->absmins );
	VectorAdd( tw->absmaxs, tv( 1, 1, 1 ), tw->absmaxs );
	VectorAdd( tw->extents, tv( 1, 1, 1 ), tw->extents );

	//
	// check for position test special case
	//
	if( VectorCompare( start, end ) ) {
		int leafs[1024];
		int numleafs;
		vec3_t c1, c2;
		int topnode;
		cleaf_t *leaf;

		if( world ) {
			CM_RecursiveBIHTest( tw );
		} else if( BoundsOverlap( cmodel->mins, cmodel->maxs, tw->absmins, tw->absmaxs ) ) {
			CM_TestBox( tw, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
		}

		VectorCopy( start, tr->endpos );
		return;
	}

	if( cmodel->bihnodes ) {
		CM_RecursiveBIHTrace( tw );
	} else if( BoundsOverlap( cmodel->mins, cmodel->maxs, tw->absmins, tw->absmaxs ) ) {
		CM_ClipBox( tw, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
	}

	Q_clamp( tr->fraction, 0, 1 );
	VectorLerp( start, tr->fraction, end, tr->endpos );
}

/*
 * CM_TransformedBoxTrace
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
void CM_TransformedBoxTrace( cmodel_state_t *cms, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
	cmodel_t *cmodel, int brushmask, vec3_t origin, vec3_t angles )
{
	vec3_t start_l, end_l;
	vec3_t a, temp;
	mat3_t axis;
	bool rotated;
	traceWork_t tw;

	if( !tr ) {
		return;
	}

	if( !cmodel || cmodel == cms->map_cmodels ) {
		cmodel = cms->map_cmodels;
		origin = vec3_origin;
		angles = vec3_origin;
	} else {
		if( !origin ) {
			origin = vec3_origin;
		}
		if( !angles ) {
			angles = vec3_origin;
		}
	}

	// special tracing code
	if( !cmodel->builtin && cms->CM_TransformedBoxTrace ) {
		cms->CM_TransformedBoxTrace( cms, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
		return;
	}

	// cylinder offset
	if( cmodel == cms->oct_cmodel ) {
		VectorSubtract( start, cmodel->cyl_offset, start_l );
		VectorSubtract( end, cmodel->cyl_offset, end_l );
	} else {
		VectorCopy( start, start_l );
		VectorCopy( end, end_l );
	}

	// subtract origin offset
	VectorSubtract( start_l, origin, start_l );
	VectorSubtract( end_l, origin, end_l );

	// ch : here we could try back-rotate the vector for aabb to get
	// 'cylinder-like' shape, ie width of the aabb is constant for all directions
	// in this case, the orientation of vector would be ( normalize(origin-start), cross(x,z), up )

	// rotate start and end into the models frame of reference
	if( ( angles[0] || angles[1] || angles[2] )
#ifndef CM_ALLOW_ROTATED_BBOXES
		&& !cmodel->builtin
#endif
	) {
		rotated = true;
	} else {
		rotated = false;
	}

	if( rotated ) {
		AnglesToAxis( angles, axis );

		VectorCopy( start_l, temp );
		Matrix3_TransformVector( axis, temp, start_l );

		VectorCopy( end_l, temp );
		Matrix3_TransformVector( axis, temp, end_l );
	}

	// sweep the box through the model
	CM_BoxTrace( &tw, cms, tr, start_l, end_l, mins, maxs, cmodel, origin, brushmask );

	if( rotated && tr->fraction != 1.0 ) {
		VectorNegate( angles, a );
		AnglesToAxis( a, axis );

		VectorCopy( tr->plane.normal, temp );
		Matrix3_TransformVector( axis, temp, tr->plane.normal );
	}

	VectorLerp( start, tr->fraction, end, tr->endpos );
}

static void CM_RecursiveBIHTrace_r(
	traceWork_t *tw, const bihnode_t *node, const struct bihbox_s *movesubbounds, const struct bihbox_s *nodebox )
{
	// if the tree were 1d, we wouldn't need to be so careful with the bounds, but if the trace is long then we want to
	// avoid hitting all surfaces within that entire-map-encompassing move aabb
	switch( node->type ) { // leaf
		case BIH_PATCHBRUSH:
			if( cm_noCurves->integer ) {
				return;
			}
			case BIH_BRUSH: {
			cbrush_t *b = node->data.brush;
			if( node->data.contents & tw->contents ) {
				if( BoundsOverlap( b->mins, b->maxs, movesubbounds->min, movesubbounds->max ) )
					CM_ClipBoxToBrush( tw, b );
			}
		}
			return;
		case BIH_TRISOUP:
			return;
		case BIH_GROUP: {
			int i;
			for( i = 0; i < node->group.numchildren; i++ )
				CM_RecursiveBIHTrace_r( tw, node + node->group.firstchild + i, movesubbounds, nodebox );
		}
			return;
		case BIH_X:
		case BIH_Y:
		case BIH_Z: {
			struct bihbox_s bounds;
			struct bihbox_s newbounds;
			float distnear, distfar, nearfrac, farfrac, min, max;
			unsigned int axis = node->type, child;
			vec3_t points[2];

			if( !tw->totalmove[axis] ) { // doesn't move with respect to this axis. don't allow infinities.
				for( child = 0; child < 2; child++ ) { // only recurse if we are actually within the child
					min = node->node.cmin[child] - tw->extents[axis];
					max = node->node.cmax[child] + tw->extents[axis];
					if( min <= tw->start[axis] && tw->start[axis] <= max ) {
						bounds = *nodebox;
						bounds.min[axis] = min;
						bounds.max[axis] = max;
						CM_RecursiveBIHTrace_r( tw, node + node->node.firstchild + child, movesubbounds, &bounds );
					}
				}
			} else if( tw->negativedir[axis] ) { // trace goes from right to left so favour the right.
				for( child = 2; child-- > 0; ) {
					bounds = *nodebox;
					bounds.min[axis] = node->node.cmin[child] - tw->extents[axis];
					bounds.max[axis] =
						node->node.cmax[child] + tw->extents[axis]; // expand the bounds according to the player's size

					if( !BoundsOverlap( movesubbounds->min, movesubbounds->max, bounds.min, bounds.max ) )
						continue;
					//					if (movesubbounds->max[axis] < bounds.min[axis])
					//						continue;	//(clipped) move bounds is outside this child
					//					if (bounds.max[axis] < movesubbounds->min[axis])
					//						continue;	//(clipped) move bounds is outside this child

					distnear = bounds.max[axis] - tw->start[axis];
					nearfrac = distnear / tw->totalmove[axis];
					if( nearfrac <= tw->realfraction ) {
						VectorMA( tw->start, nearfrac, tw->totalmove,
							points[0] ); // clip the new movebounds (this is more to clip the other axis too)
						distfar = bounds.min[axis] - tw->start[axis];
						farfrac = distfar / tw->totalmove[axis];
						VectorMA( tw->start, farfrac, tw->totalmove,
							points[1] ); // clip the new movebounds (this is more to clip the other axis too)

						newbounds.min[0] = max( points[tw->negativedir[0]][0] - tw->extents[axis], bounds.min[0] );
						newbounds.max[0] = min( points[!tw->negativedir[0]][0] + tw->extents[axis], bounds.max[0] );
						newbounds.min[1] = max( points[tw->negativedir[1]][1] - tw->extents[axis], bounds.min[1] );
						newbounds.max[1] = min( points[!tw->negativedir[1]][1] + tw->extents[axis], bounds.max[1] );
						newbounds.min[2] = max( points[tw->negativedir[2]][2] - tw->extents[axis], bounds.min[2] );
						newbounds.max[2] = min( points[!tw->negativedir[2]][2] + tw->extents[axis], bounds.max[2] );
						CM_RecursiveBIHTrace_r( tw, node + node->node.firstchild + child, &newbounds, &bounds );
					}
				}
			} else { // trace goes from left to right
				for( child = 0; child < 2; child++ ) {
					bounds = *nodebox;
					bounds.min[axis] = node->node.cmin[child] - tw->extents[axis];
					bounds.max[axis] =
						node->node.cmax[child] + tw->extents[axis]; // expand the bounds according to the player's size

					if( !BoundsOverlap( movesubbounds->min, movesubbounds->max, bounds.min, bounds.max ) )
						continue;
					//					if (movesubbounds->max[axis] < bounds.min[axis])
					//						continue;	//(clipped) move bounds is outside this child
					//					if (bounds.max[axis] < movesubbounds->min[axis])
					//						continue;	//(clipped) move bounds is outside this child

					distnear = bounds.min[axis] - tw->start[axis];
					nearfrac = distnear / tw->totalmove[axis];
					if( nearfrac <= tw->realfraction ) {
						VectorMA( tw->start, nearfrac, tw->totalmove,
							points[0] ); // clip the new movebounds (this is more to clip the other axis too)
						distfar = bounds.max[axis] - tw->start[axis];
						farfrac = distfar / tw->totalmove[axis];
						VectorMA( tw->start, farfrac, tw->totalmove,
							points[1] ); // clip the new movebounds (this is more to clip the other axis too)

						newbounds.min[0] = max( points[tw->negativedir[0]][0] - tw->extents[axis], bounds.min[0] );
						newbounds.max[0] = min( points[!tw->negativedir[0]][0] + tw->extents[axis], bounds.max[0] );
						newbounds.min[1] = max( points[tw->negativedir[1]][1] - tw->extents[axis], bounds.min[1] );
						newbounds.max[1] = min( points[!tw->negativedir[1]][1] + tw->extents[axis], bounds.max[1] );
						newbounds.min[2] = max( points[tw->negativedir[2]][2] - tw->extents[axis], bounds.min[2] );
						newbounds.max[2] = min( points[!tw->negativedir[2]][2] + tw->extents[axis], bounds.max[2] );
						CM_RecursiveBIHTrace_r( tw, node + node->node.firstchild + child, &newbounds, &bounds );
					}
				}
			}
		}
			return;
	}
}

static void CM_RecursiveBIHTrace( traceWork_t *tw )
{
	const cmodel_t *mod = tw->cmodel;
	struct bihbox_s trbounds, modsize;

	VectorCopy( mod->mins, modsize.min );
	VectorCopy( mod->maxs, modsize.max );
	VectorCopy( tw->absmins, trbounds.min );
	VectorCopy( tw->absmaxs, trbounds.max );

	CM_RecursiveBIHTrace_r( tw, mod->bihnodes, &trbounds, &modsize );
}

static void CM_RecursiveBIHTest_r( traceWork_t *tw, const bihnode_t *node )
{ // with BIH, its possible for a large child node to have a box larger than its sibling.
	int i;
	switch( node->type ) {
		case BIH_PATCHBRUSH:
			if( cm_noCurves->integer ) {
				return;
			}
		case BIH_BRUSH: {
			cbrush_t *b = node->data.brush;
			if( node->data.contents & tw->contents )
				//				if (BoundsIntersect(tw->bounds.min, tw->bounds.max, b->absmins, b->absmaxs))
				CM_TestBoxInBrush( tw, b );
		}
			return;
		case BIH_TRISOUP:
			return;
		case BIH_GROUP:
			for( i = 0; i < node->group.numchildren; i++ ) {
				CM_RecursiveBIHTest_r( tw, node + node->group.firstchild + i );
				if( tw->trace->allsolid )
					break;
			}
			return;
		case BIH_X:
		case BIH_Y:
		case BIH_Z: { // node (x y or z)
			float min;
			float max;
			int axis = node->type;
			min = node->node.cmin[0] - tw->extents[axis];
			max = node->node.cmax[0] + tw->extents[axis]; // expand the bounds according to the player's size

			// the point can potentially be within both children, or neither.
			// it doesn't really matter which order we walk the tree, just be sure to do it efficiently.
			if( min <= tw->start[axis] && tw->start[axis] <= max ) {
				CM_RecursiveBIHTest_r( tw, node + node->node.firstchild + 0 );
				if( tw->trace->allsolid )
					return;
			}

			min = node->node.cmin[1] - tw->extents[axis];
			max = node->node.cmax[1] + tw->extents[axis];
			if( min <= tw->start[axis] && tw->start[axis] <= max )
				CM_RecursiveBIHTest_r( tw, node + node->node.firstchild + 1 );
		}
			return;
	}
}

static void CM_RecursiveBIHTest( traceWork_t *tw )
{
	CM_RecursiveBIHTest_r( tw, tw->cmodel->bihnodes );
}

static int CM_PointContentsBIH( const bihnode_t *node, const vec3_t p )
{
	switch( node->type ) { // leaf
		case BIH_PATCHBRUSH:
			if( !cm_noCurves->integer ) {
				return 0;
			}
		case BIH_BRUSH: {
			cbrush_t *b = node->data.brush;
			cbrushside_t *brushside = b->brushsides;
			size_t j;
			if( !BoundsOverlap( p, p, b->mins, b->maxs ) )
				return 0;

			for( j = 0; j < b->numsides; j++, brushside++ ) {
				if( PlaneDiff( p, &brushside->plane ) > 0 )
					return 0;
			}
			return b->contents; // inside all planes
		}
		case BIH_GROUP: {
			int i;
			int contents = 0;
			for( i = 0; i < node->group.numchildren; i++ )
				contents |= CM_PointContentsBIH( node + node->group.firstchild + i, p );
			return contents;
		}
		case BIH_X:
		case BIH_Y:
		case BIH_Z: { // node (x y or z)
			int contents;

			// the point can potentially be within both children, or neither.
			// it doesn't really matter which order we walk the tree, just be sure to do it efficiently.
			if( node->node.cmin[0] <= p[node->type] && p[node->type] <= node->node.cmax[0] )
				contents = CM_PointContentsBIH( node + node->node.firstchild + 0, p );
			else
				contents = 0;

			if( node->node.cmin[1] <= p[node->type] && p[node->type] <= node->node.cmax[1] )
				contents |= CM_PointContentsBIH( node + node->node.firstchild + 1, p );
			return contents;
		}
	}
	return 0;
}

struct bihleaf_s {
	int type;
	vec3_t mins;
	vec3_t maxs;
	struct bihdata_s data;
};

static int CM_SortBIH_X( const void *va, const void *vb )
{
	const struct bihleaf_s *a = va, *b = vb;
	float am = a->maxs[0] + a->mins[0];
	float bm = b->maxs[0] + b->mins[0];
	if( am == bm )
		return 0;
	return am > bm;
}
static int CM_SortBIH_Y( const void *va, const void *vb )
{
	const struct bihleaf_s *a = va, *b = vb;
	float am = a->maxs[1] + a->mins[1];
	float bm = b->maxs[1] + b->mins[1];
	if( am == bm )
		return 0;
	return am > bm;
}
static int CM_SortBIH_Z( const void *va, const void *vb )
{
	const struct bihleaf_s *a = va, *b = vb;
	float am = a->maxs[2] + a->mins[2];
	float bm = b->maxs[2] + b->mins[2];
	if( am == bm )
		return 0;
	return am > bm;
}
static struct bihbox_s CM_BuildBIHNode(
	bihnode_t *node, bihnode_t **freenodes, struct bihleaf_s *leafs, size_t numleafs )
{
	struct bihbox_s bounds;
	if( numleafs == 1 ) // the leaf just gives the brush pointer.
	{
		size_t i;
		VectorCopy( leafs[0].mins, bounds.min );
		VectorCopy( leafs[0].maxs, bounds.max );
		node->type = leafs[0].type;
		node->data = leafs[0].data;

		// expand by 1qu, to avoid precision issues.
		for( i = 0; i < 3; i++ ) {
			bounds.min[i] -= 1;
			bounds.max[i] += 1;
		}
	} else if( numleafs < 8 ) // the leaf just gives the brush pointer.
	{
		bihnode_t *cnodes;
		struct bihbox_s cb;
		size_t i;
		node->type = BIH_GROUP;

		cnodes = *freenodes;
		*freenodes += numleafs;
		node->group.firstchild = cnodes - node;
		node->group.numchildren = numleafs;

		bounds = CM_BuildBIHNode( cnodes + 0, freenodes, leafs + 0, 1 );
		for( i = 1; i < numleafs; i++ ) {
			cb = CM_BuildBIHNode( cnodes + i, freenodes, leafs + i, 1 );
			AddPointToBounds( cb.min, bounds.min, bounds.max );
			AddPointToBounds( cb.max, bounds.min, bounds.max );
		}
	} else {
		size_t i, j;
		size_t numleft = numleafs / 2; // this ends up splitting at the median point.
		size_t numright = numleafs - numleft;
		struct bihbox_s left, right;
		bihnode_t *cnodes;
		static int ( *sorts[3] )( const void *va, const void *vb ) = { CM_SortBIH_X, CM_SortBIH_Y, CM_SortBIH_Z };
		VectorCopy( leafs[0].mins, bounds.min );
		VectorCopy( leafs[0].maxs, bounds.max );
		for( i = 1; i < numleafs; i++ ) {
			for( j = 0; j < 3; j++ ) {
				if( bounds.min[j] > leafs[i].mins[j] )
					bounds.min[j] = leafs[i].mins[j];
				if( bounds.max[j] < leafs[i].maxs[j] )
					bounds.max[j] = leafs[i].maxs[j];
			}
		}
#if 1
		{ // balanced by counts
			vec3_t mid;
			int onleft[3], onright[3], weight[3];
			VectorAvg( bounds.max, bounds.min, mid );
			VectorClear( onleft );
			VectorClear( onright );
			for( i = 0; i < numleafs; i++ ) {
				for( j = 0; j < 3; j++ ) { // ignore leafs that split the node.
					if( leafs[i].maxs[j] < mid[j] )
						onleft[j]++;
					if( mid[j] > leafs[i].mins[j] )
						onright[j]++;
				}
			}
			for( j = 0; j < 3; j++ )
				weight[j] = onleft[j] + onright[j] - abs( onleft[j] - onright[j] );
			// pick the most balanced.
			if( weight[0] > weight[1] && weight[0] > weight[2] )
				node->type = BIH_X;
			else if( weight[1] > weight[2] )
				node->type = BIH_Y;
			else
				node->type = BIH_Z;
		}
#else
		{ // balanced by volume
			vec3_t size;
			VectorSubtract( bounds.max, bounds.min, size );
			if( size[0] > size[1] && size[0] > size[2] )
				node->type = BIH_X;
			else if( size[1] > size[2] )
				node->type = BIH_Y;
			else
				node->type = BIH_Z;
			* /
		}
#endif
		qsort( leafs, numleafs, sizeof( *leafs ), sorts[node->type] );

		cnodes = *freenodes;
		*freenodes += 2;
		node->node.firstchild = cnodes - node;
		left = CM_BuildBIHNode( cnodes + 0, freenodes, leafs, numleft );
		right = CM_BuildBIHNode( cnodes + 1, freenodes, &leafs[numleft], numright );

		node->node.cmin[0] = left.min[node->type];
		node->node.cmax[0] = left.max[node->type];
		node->node.cmin[1] = right.min[node->type];
		node->node.cmax[1] = right.max[node->type];

		bounds = left;
		AddPointToBounds( right.min, bounds.min, bounds.max );
		AddPointToBounds( right.max, bounds.min, bounds.max );
	}
	return bounds;
}

bihnode_t *CM_BuildBIH( cmodel_state_t *cms, cmodel_t *mod )
{
	size_t numleafs, numnodes, i, j;
	bihnode_t *nodes, *tmpnodes;
	struct bihleaf_s *leafs, *leaf;
	numleafs = mod->nummarkbrushes;
	for( i = 0; i < mod->nummarkfaces; i++ )
		numleafs += mod->faces[mod->markfaces[i]].numfacets;
	numnodes = numleafs * 2 - 1;
	leafs = Mem_Alloc( cms->mempool, sizeof( *leafs ) * numleafs );
	nodes = Mem_Alloc( cms->mempool, sizeof( *nodes ) * numnodes );
	for( leaf = leafs, i = 0; i < mod->nummarkbrushes; i++, leaf++ ) {
		cbrush_t *b = mod->brushes + mod->markbrushes[i];
		leaf->type = BIH_BRUSH;
		leaf->data.contents = b->contents;
		leaf->data.brush = b;
		VectorCopy( b->mins, leaf->mins );
		VectorCopy( b->maxs, leaf->maxs );
	}
	for( i = 0; i < mod->nummarkfaces; i++ ) {
		cface_t *p = mod->faces + mod->markfaces[i];
		for( j = 0; j < p->numfacets; j++, leaf++ ) {
			leaf->type = BIH_PATCHBRUSH;
			leaf->data.contents = p->facets[j].contents;
			leaf->data.brush = &p->facets[j];
			VectorCopy( p->facets[j].mins, leaf->mins );
			VectorCopy( p->facets[j].maxs, leaf->maxs );
		}
	}
	tmpnodes = nodes + 1;
	CM_BuildBIHNode( nodes, &tmpnodes, leafs, numleafs );
	if( tmpnodes > nodes + numnodes )
		Com_Error( ERR_DROP, "CM_BuildBIH: generated wrong number of nodes" );
	Mem_Free( leafs ); // just for temporary storage so that CM_BuildBIHNode doesn't need to care
	return nodes;
}
