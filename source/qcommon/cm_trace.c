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

	cms->box_markbrushes[0] = cms->box_brush;

	cms->box_cmodel->builtin = true;
	cms->box_cmodel->nummarkfaces = 0;
	cms->box_cmodel->markfaces = NULL;
	cms->box_cmodel->markbrushes = cms->box_markbrushes;
	cms->box_cmodel->nummarkbrushes = 1;

	for( i = 0; i < 6; i++ )
	{
		// brush sides
		s = cms->box_brushsides + i;
		s->plane = cms->box_planes + i;
		s->surfFlags = 0;

		// planes
		p = &cms->box_planes[i];
		VectorClear( p->normal );

		if( ( i & 1 ) )
		{
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1;
			p->signbits = ( 1 << ( i >> 1 ) );
		}
		else
		{
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
	const vec3_t oct_dirs[4] = {
		{  1,  1, 0 },
		{ -1,  1, 0 },
		{ -1, -1, 0 },
		{  1, -1, 0 }
	};

	cms->oct_brush->numsides = 10;
	cms->oct_brush->brushsides = cms->oct_brushsides;
	cms->oct_brush->contents = CONTENTS_BODY;

	cms->oct_markbrushes[0] = cms->oct_brush;

	cms->oct_cmodel->builtin = true;
	cms->oct_cmodel->nummarkfaces = 0;
	cms->oct_cmodel->markfaces = NULL;
	cms->oct_cmodel->markbrushes = cms->oct_markbrushes;
	cms->oct_cmodel->nummarkbrushes = 1;

	// axial planes
	for( i = 0; i < 6; i++ )
	{
		// brush sides
		s = cms->oct_brushsides + i;
		s->plane = cms->oct_planes + i;
		s->surfFlags = 0;

		// planes
		p = &cms->oct_planes[i];
		VectorClear( p->normal );

		if( ( i & 1 ) )
		{
			p->type = PLANE_NONAXIAL;
			p->normal[i>>1] = -1;
			p->signbits = ( 1 << ( i >> 1 ) );
		}
		else
		{
			p->type = i >> 1;
			p->normal[i >> 1] = 1;
			p->signbits = 0;
		}
	}

	// non-axial planes
	for( i = 6; i < 10; i++ ) {
		// brush sides
		s = cms->oct_brushsides + i;
		s->plane = cms->oct_planes + i;
		s->surfFlags = 0;

		// planes
		p = &cms->oct_planes[i];
		VectorCopy( oct_dirs[i-6], p->normal );

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
	cms->box_planes[0].dist = maxs[0];
	cms->box_planes[1].dist = -mins[0];
	cms->box_planes[2].dist = maxs[1];
	cms->box_planes[3].dist = -mins[1];
	cms->box_planes[4].dist = maxs[2];
	cms->box_planes[5].dist = -mins[2];

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

	cms->oct_planes[0].dist = size[1][0];
	cms->oct_planes[1].dist = -size[0][0];
	cms->oct_planes[2].dist = size[1][1];
	cms->oct_planes[3].dist = -size[0][1];
	cms->oct_planes[4].dist = size[1][2];
	cms->oct_planes[5].dist = -size[0][2];

	a = size[1][0]; // halfx
	b = size[1][1]; // halfy
	d = sqrt( a * a + b * b ); // hypothenuse

	cosa = a / d;
	sina = b / d;

	// swap sin and cos, which is the same thing as adding pi/2 radians to the original angle
	t = sina;
	sina = cosa;
	cosa = t;

	// elleptical radius
	d = a * b / sqrt( a * a * cosa * cosa + b * b * sina * sina );
	//d = a * b / sqrt( a * a  + b * b ); // produces a rectangle, inscribed at middle points

	// the following should match normals and signbits set in CM_InitOctagonHull

	VectorSet( cms->oct_planes[6].normal, cosa, sina, 0 );
	cms->oct_planes[6].dist = d;

	VectorSet( cms->oct_planes[7].normal, -cosa, sina, 0 );
	cms->oct_planes[7].dist = d;

	VectorSet( cms->oct_planes[8].normal, -cosa, -sina, 0 );
	cms->oct_planes[8].dist = d;

	VectorSet( cms->oct_planes[9].normal, cosa, -sina, 0 );
	cms->oct_planes[9].dist = d;

	return cms->oct_cmodel;
}

/*
* CM_PointLeafnum
*/
int CM_PointLeafnum( cmodel_state_t *cms, const vec3_t p )
{
	int num = 0;
	cnode_t	*node;

	if( !cms->numplanes )
		return 0; // sound may call this without map loaded

	do
	{
		node = cms->map_nodes + num;
		num = node->children[PlaneDiff( p, node->plane ) < 0];
	}
	while( num >= 0 );

	return -1 - num;
}

/*
* CM_BoxLeafnums
*
* Fills in a list of all the leafs touched
*/
static void CM_BoxLeafnums_r( cmodel_state_t *cms, int nodenum )
{
	int s;
	cnode_t	*node;

	while( nodenum >= 0 )
	{
		node = &cms->map_nodes[nodenum];
		s = BOX_ON_PLANE_SIDE( cms->leaf_mins, cms->leaf_maxs, node->plane ) - 1;

		if( s < 2 )
		{
			nodenum = node->children[s];
			continue;
		}

		// go down both sides
		if( cms->leaf_topnode == -1 )
			cms->leaf_topnode = nodenum;
		CM_BoxLeafnums_r( cms, node->children[0] );
		nodenum = node->children[1];
	}

	if( cms->leaf_count < cms->leaf_maxcount )
		cms->leaf_list[cms->leaf_count++] = -1 - nodenum;
}

/*
* CM_BoxLeafnums
*/
int CM_BoxLeafnums( cmodel_state_t *cms, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode )
{
	cms->leaf_list = list;
	cms->leaf_count = 0;
	cms->leaf_maxcount = listsize;
	cms->leaf_mins = mins;
	cms->leaf_maxs = maxs;

	cms->leaf_topnode = -1;

	CM_BoxLeafnums_r( cms, 0 );

	if( topnode )
		*topnode = cms->leaf_topnode;

	return cms->leaf_count;
}

/*
* CM_RoundUpToHullSize
*/
void CM_RoundUpToHullSize( cmodel_state_t *cms, vec3_t mins, vec3_t maxs, cmodel_t *cmodel )
{
	if( !cmodel )
		cmodel = cms->map_cmodels;

	// special rounding code
	if( !cmodel->builtin && cms->CM_RoundUpToHullSize )
	{
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
		if( PlaneDiff( p, brushside->plane ) > 0 )
			return 0;

	return brush->contents;
}

/*
* CM_PatchContents
*/
static inline int CM_PatchContents( cface_t *patch, vec3_t p )
{
	int i, c;
	cbrush_t *facet;

	for( i = 0, facet = patch->facets; i < patch->numfacets; i++, patch++ )
		if( ( c = CM_BrushContents( facet, p ) ) )
			return c;

	return 0;
}

/*
* CM_PointContents
*/
static int CM_PointContents( cmodel_state_t *cms, vec3_t p, cmodel_t *cmodel )
{
	int i, superContents, contents;
	int nummarkfaces, nummarkbrushes;
	cface_t	*patch, **markface;
	cbrush_t *brush, **markbrush;

	if( !cms->numnodes )    // map not loaded
		return 0;

	c_pointcontents++; // optimize counter

	if( cmodel == cms->map_cmodels )
	{
		cleaf_t	*leaf;

		leaf = &cms->map_leafs[CM_PointLeafnum( cms, p )];
		superContents = leaf->contents;

		markbrush = leaf->markbrushes;
		nummarkbrushes = leaf->nummarkbrushes;

		markface = leaf->markfaces;
		nummarkfaces = leaf->nummarkfaces;
	}
	else
	{
		superContents = ~0;

		markbrush = cmodel->markbrushes;
		nummarkbrushes = cmodel->nummarkbrushes;

		markface = cmodel->markfaces;
		nummarkfaces = cmodel->nummarkfaces;
	}

	contents = superContents;

	for( i = 0; i < nummarkbrushes; i++ )
	{
		brush = markbrush[i];

		// check if brush adds something to contents
		if( contents & brush->contents )
		{
			if( !( contents &= ~CM_BrushContents( brush, p ) ) )
				return superContents;
		}
	}

	if( !cm_noCurves->integer )
	{
		for( i = 0; i < nummarkfaces; i++ )
		{
			patch = markface[i];

			// check if patch adds something to contents
			if( contents & patch->contents )
			{
				if( BoundsIntersect( p, p, patch->mins, patch->maxs ) )
				{
					if( !( contents &= ~CM_PatchContents( patch, p ) ) )
						return superContents;
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

	if( !cms->numnodes )  // map not loaded
		return 0;

	if( !cmodel || cmodel == cms->map_cmodels )
	{
		cmodel = cms->map_cmodels;
		origin = vec3_origin;
		angles = vec3_origin;
	}
	else
	{
		if( !origin )
			origin = vec3_origin;
		if( !angles )
			angles = vec3_origin;
	}

	// special point contents code
	if( !cmodel->builtin && cms->CM_TransformedPointContents )
		return cms->CM_TransformedPointContents( cms, p, cmodel, origin, angles );

	// subtract origin offset
	VectorSubtract( p, origin, p_l );

	// rotate start and end into the models frame of reference
	if( ( angles[0] || angles[1] || angles[2] ) 
		&& !cmodel->builtin	
		)
	{
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

//#define TRACEVICFIX

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	( 1.0f / 32.0f )
#ifdef TRACEVICFIX
#define FRAC_EPSILON    ( 1.0f / 1024.0f )
#endif
#define RADIUS_EPSILON		1.0f

static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_startmins, trace_endmins;
static vec3_t trace_startmaxs, trace_endmaxs;
static vec3_t trace_absmins, trace_absmaxs;
static vec3_t trace_extents;

static trace_t	*trace_trace;
#ifdef TRACEVICFIX
static float trace_realfraction;
#endif
static int trace_contents;
static bool trace_ispoint;      // optimized case

/*
* CM_ClipBoxToBrush
*/
static void CM_ClipBoxToBrush( cmodel_state_t *cms, cbrush_t *brush )
{
	int i;
	cplane_t *p, *clipplane;
	float enterfrac, leavefrac;
#ifdef TRACEVICFIX
	float enterdist = 0, move = 1;
#endif
	float d1, d2, f;
	bool getout, startout;
	cbrushside_t *side, *leadside;

	if( !brush->numsides )
		return;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	c_brush_traces++;

	getout = false;
	startout = false;
	leadside = NULL;
	side = brush->brushsides;

	for( i = 0; i < brush->numsides; i++, side++ )
	{
		p = side->plane;

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 )
		{
			d1 = trace_startmins[p->type] - p->dist;
			d2 = trace_endmins[p->type] - p->dist;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 1:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 2:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 3:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmins[2] - p->dist;
				break;
			case 4:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 5:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmins[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 6:
				d1 = p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmins[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			case 7:
				d1 = p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] - p->dist;
				d2 = p->normal[0]*trace_endmaxs[0] + p->normal[1]*trace_endmaxs[1] + p->normal[2]*trace_endmaxs[2] - p->dist;
				break;
			default:
				d1 = d2 = 0; // shut up compiler
				assert( 0 );
				break;
			}
		}

		if( d2 > 0 )
			getout = true; // endpoint is not in solid
		if( d1 > 0 )
			startout = true;

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 )
			return;
		if( d1 <= 0 && d2 <= 0 )
			continue;
#ifdef TRACEVICFIX
		// crosses face
		f = d1 - d2;
		if( f > 0 )
		{                   // enter
			f = d1 / f;
			if( f > enterfrac )
			{
				enterdist = d1;
				move = d1 - d2;
				enterfrac = f;
				clipplane = p;
				leadside = side;
			}
		}
		else if( f < 0 )
		{                   // leave
			f = d1 / f;
			if( f < leavefrac )
				leavefrac = f;
		}
#else
		// crosses face
		f = d1 - d2;
		if( f > 0 )
		{               // enter
			f = ( d1 - DIST_EPSILON ) / f;
			if( f > enterfrac )
			{
				enterfrac = f;
				clipplane = p;
				leadside = side;
			}
		}
		else if( f < 0 )
		{               // leave
			f = ( d1 + DIST_EPSILON ) / f;
			if( f < leavefrac )
				leavefrac = f;
		}
#endif
	}

	if( !startout )
	{
		// original point was inside brush
		trace_trace->startsolid = true;
		trace_trace->contents = brush->contents;
		if( !getout )
		{
			trace_trace->allsolid = true;
			trace_trace->fraction = 0;
		}
		return;
	}
#ifdef TRACEVICFIX
	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1 && enterfrac < trace_realfraction )
		{
			if( enterfrac < 0 )
				enterfrac = 0;
			trace_realfraction = enterfrac;
			trace_trace->plane = *clipplane;
			trace_trace->surfFlags = leadside->surfFlags;
			trace_trace->contents = brush->contents;
			trace_trace->fraction = ( enterdist - DIST_EPSILON ) / move;
			if( trace_trace->fraction < 0 )
				trace_trace->fraction = 0;
		}
	}
#else
	if( enterfrac - ( 1.0f / 1024.0f ) <= leavefrac )
	{
		if( enterfrac > -1 && enterfrac < trace_trace->fraction )
		{
			if( enterfrac < 0 )
				enterfrac = 0;
			trace_trace->fraction = enterfrac;
			trace_trace->plane = *clipplane;
			trace_trace->surfFlags = leadside->surfFlags;
			trace_trace->contents = brush->contents;
		}
	}
#endif
}

/*
* CM_TestBoxInBrush
*/
static void CM_TestBoxInBrush( cmodel_state_t *cms, cbrush_t *brush )
{
	int i;
	cplane_t *p;
	cbrushside_t *side;

	if( !brush->numsides )
		return;

	side = brush->brushsides;
	for( i = 0; i < brush->numsides; i++, side++ )
	{
		p = side->plane;

		// push the plane out appropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 )
		{
			if( trace_startmins[p->type] > p->dist )
				return;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 1:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 2:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 3:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmins[2] > p->dist )
					return;
				break;
			case 4:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 5:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmins[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 6:
				if( p->normal[0]*trace_startmins[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			case 7:
				if( p->normal[0]*trace_startmaxs[0] + p->normal[1]*trace_startmaxs[1] + p->normal[2]*trace_startmaxs[2] > p->dist )
					return;
				break;
			default:
				assert( 0 );
				return;
			}
		}
	}

	// inside this brush
	trace_trace->startsolid = trace_trace->allsolid = true;
	trace_trace->fraction = 0;
	trace_trace->contents = brush->contents;
}

/*
* CM_CollideBox
*/
static void CM_CollideBox( cmodel_state_t *cms, cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces,
						  int nummarkfaces, void ( *func )( cmodel_state_t *cms, cbrush_t *b ) )
{
	int i, j;
	cbrush_t *b;
	cface_t	*patch;
	cbrush_t *facet;

	// trace line against all brushes
	for( i = 0; i < nummarkbrushes; i++ )
	{
		b = markbrushes[i];
		if( b->checkcount == cms->checkcount )
			continue; // already checked this brush
		b->checkcount = cms->checkcount;
		if( !( b->contents & trace_contents ) )
			continue;
		func( cms, b );
		if( !trace_trace->fraction )
			return;
	}

	if( cm_noCurves->integer || !nummarkfaces )
		return;

	// trace line against all patches
	for( i = 0; i < nummarkfaces; i++ )
	{
		patch = markfaces[i];
		if( patch->checkcount == cms->checkcount )
			continue; // already checked this patch
		patch->checkcount = cms->checkcount;
		if( !( patch->contents & trace_contents ) )
			continue;
		if( !BoundsIntersect( patch->mins, patch->maxs, trace_absmins, trace_absmaxs ) )
			continue;
		facet = patch->facets;
		for( j = 0; j < patch->numfacets; j++, facet++ )
		{
			func( cms, facet );
			if( !trace_trace->fraction )
				return;
		}
	}
}

/*
* CM_ClipBox
*/
static inline void CM_ClipBox( cmodel_state_t *cms, cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces,
							  int nummarkfaces )
{
	CM_CollideBox( cms, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_ClipBoxToBrush );
}

/*
* CM_TestBox
*/
static inline void CM_TestBox( cmodel_state_t *cms, cbrush_t **markbrushes, int nummarkbrushes, cface_t **markfaces,
							  int nummarkfaces )
{
	CM_CollideBox( cms, markbrushes, nummarkbrushes, markfaces, nummarkfaces, CM_TestBoxInBrush );
}

/*
* CM_RecursiveHullCheck
*/
static void CM_RecursiveHullCheck( cmodel_state_t *cms, int num, float p1f, float p2f, vec3_t p1, vec3_t p2 )
{
	cnode_t	*node;
	cplane_t *plane;
	int side;
	float t1, t2, offset;
	float frac, frac2;
	float idist, midf;
	vec3_t mid;

loc0:
#ifdef TRACEVICFIX
	if( trace_realfraction <= p1f )
		return; // already hit something nearer
#else
	if( trace_trace->fraction <= p1f )
		return; // already hit something nearer
#endif
	// if < 0, we are in a leaf node
	if( num < 0 )
	{
		cleaf_t	*leaf;

		leaf = &cms->map_leafs[-1 - num];
		if( leaf->contents & trace_contents )
			CM_ClipBox( cms, leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = cms->map_nodes + num;
	plane = node->plane;

	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
		if( trace_ispoint )
			offset = 0;
		else
			offset = fabs( trace_extents[0] * plane->normal[0] ) +
			fabs( trace_extents[1] * plane->normal[1] ) +
			fabs( trace_extents[2] * plane->normal[2] );
	}

	// see which sides we need to consider
	if( t1 >= offset && t2 >= offset )
	{
		num = node->children[0];
		goto loc0;
	}
	if( t1 < -offset && t2 < -offset )
	{
		num = node->children[1];
		goto loc0;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( t1 < t2 )
	{
		idist = 1.0 / ( t1 - t2 );
		side = 1;
#ifdef TRACEVICFIX
		frac2 = ( t1 + offset ) * idist;
		frac = ( t1 - offset ) * idist;
#else
		frac2 = ( t1 + offset + DIST_EPSILON ) * idist;
		frac = ( t1 - offset + DIST_EPSILON ) * idist;
#endif
	}
	else if( t1 > t2 )
	{
		idist = 1.0 / ( t1 - t2 );
		side = 0;
#ifdef TRACEVICFIX
		frac2 = ( t1 - offset ) * idist;
		frac = ( t1 + offset ) * idist;
#else
		frac2 = ( t1 - offset - DIST_EPSILON ) * idist;
		frac = ( t1 + offset + DIST_EPSILON ) * idist;
#endif
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	clamp( frac, 0, 1 );
	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( p1, frac, p2, mid );

	CM_RecursiveHullCheck( cms, node->children[side], p1f, midf, p1, mid );

	// go past the node
	clamp( frac2, 0, 1 );
	midf = p1f + ( p2f - p1f ) * frac2;
	VectorLerp( p1, frac2, p2, mid );

	CM_RecursiveHullCheck( cms, node->children[side^1], midf, p2f, mid, p2 );
}

//======================================================================

/*
* CM_BoxTrace
*/
static void CM_BoxTrace( cmodel_state_t *cms, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
						cmodel_t *cmodel, vec3_t origin, int brushmask )
{
	bool notworld;

	notworld = ( cmodel != cms->map_cmodels ? true : false );

	cms->checkcount++;  // for multi-check avoidance
	c_traces++;     // for statistics, may be zeroed

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );
#ifdef TRACEVICFIX
	tr->fraction = trace_realfraction = 1;
#else
	tr->fraction = 1;
#endif
	if( !cms->numnodes )  // map not loaded
		return;

	trace_trace = tr;
	trace_contents = brushmask;
	VectorCopy( start, trace_start );
	VectorCopy( end, trace_end );
	VectorCopy( mins, trace_mins );
	VectorCopy( maxs, trace_maxs );

	// build a bounding box of the entire move
	ClearBounds( trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_mins, trace_startmins );
	AddPointToBounds( trace_startmins, trace_absmins, trace_absmaxs );

	VectorAdd( start, trace_maxs, trace_startmaxs );
	AddPointToBounds( trace_startmaxs, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_mins, trace_endmins );
	AddPointToBounds( trace_endmins, trace_absmins, trace_absmaxs );

	VectorAdd( end, trace_maxs, trace_endmaxs );
	AddPointToBounds( trace_endmaxs, trace_absmins, trace_absmaxs );

	//
	// check for position test special case
	//
	if( VectorCompare( start, end ) )
	{
		int leafs[1024];
		int i, numleafs;
		vec3_t c1, c2;
		int topnode;
		cleaf_t	*leaf;

		if( notworld )
		{
			if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ) )
			{
				CM_TestBox( cms, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );
			}
		}
		else
		{
			for( i = 0; i < 3; i++ )
			{
				c1[i] = start[i] + mins[i] - 1;
				c2[i] = start[i] + maxs[i] + 1;
			}

			numleafs = CM_BoxLeafnums( cms, c1, c2, leafs, 1024, &topnode );
			for( i = 0; i < numleafs; i++ )
			{
				leaf = &cms->map_leafs[leafs[i]];

				if( leaf->contents & trace_contents )
				{
					CM_TestBox( cms, leaf->markbrushes, leaf->nummarkbrushes, leaf->markfaces, leaf->nummarkfaces );
					if( tr->allsolid )
						break;
				}
			}
		}

		VectorCopy( start, tr->endpos );
		return;
	}

	//
	// check for point special case
	//
	if( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) )
	{
		trace_ispoint = true;
		VectorClear( trace_extents );
	}
	else
	{
		trace_ispoint = false;
		VectorSet( trace_extents,
			-mins[0] > maxs[0] ? -mins[0] : maxs[0],
			-mins[1] > maxs[1] ? -mins[1] : maxs[1],
			-mins[2] > maxs[2] ? -mins[2] : maxs[2] );
	}

	//
	// general sweeping through world
	//
	if( !notworld )
		CM_RecursiveHullCheck( cms, 0, 0, 1, start, end );
	else if( BoundsIntersect( cmodel->mins, cmodel->maxs, trace_absmins, trace_absmaxs ) )
		CM_ClipBox( cms, cmodel->markbrushes, cmodel->nummarkbrushes, cmodel->markfaces, cmodel->nummarkfaces );

#ifdef TRACEVICFIX
	clamp( tr->fraction, 0, 1 );
#endif
	if( tr->fraction == 1 )
		VectorCopy( end, tr->endpos );
	else
	{
		VectorLerp( start, tr->fraction, end, tr->endpos );
#ifdef TRACE_NOAXIAL
		if( PlaneTypeForNormal( tr->plane.normal ) == PLANE_NONAXIAL )
		{
			VectorMA( tr->endpos, TRACE_NOAXIAL_SAFETY_OFFSET, tr->plane.normal, tr->endpos );
		}
#endif
	}
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

	if( !tr )
		return;

	if( !cmodel || cmodel == cms->map_cmodels )
	{
		cmodel = cms->map_cmodels;
		origin = vec3_origin;
		angles = vec3_origin;
	}
	else
	{
		if( !origin )
			origin = vec3_origin;
		if( !angles )
			angles = vec3_origin;
	}

	// special tracing code
	if( !cmodel->builtin && cms->CM_TransformedPointContents )
	{
		cms->CM_TransformedBoxTrace( cms, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
		return;
	}

	// cylinder offset
	if( cmodel == cms->oct_cmodel )
	{
		VectorSubtract( start, cmodel->cyl_offset, start_l );
		VectorSubtract( end, cmodel->cyl_offset, end_l );
	}
	else
	{
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
		)
		rotated = true;
	else
		rotated = false;

	if( rotated )
	{
		AnglesToAxis( angles, axis );

		VectorCopy( start_l, temp );
		Matrix3_TransformVector( axis, temp, start_l );

		VectorCopy( end_l, temp );
		Matrix3_TransformVector( axis, temp, end_l );
	}

	// sweep the box through the model
	CM_BoxTrace( cms, tr, start_l, end_l, mins, maxs, cmodel, origin, brushmask );

	if( rotated && tr->fraction != 1.0 )
	{
		VectorNegate( angles, a );
		AnglesToAxis( a, axis );

		VectorCopy( tr->plane.normal, temp );
		Matrix3_TransformVector( axis, temp, tr->plane.normal );
	}

	if( tr->fraction == 1 )
		VectorCopy( end, tr->endpos );
	else
	{
		VectorLerp( start, tr->fraction, end, tr->endpos );
#ifdef TRACE_NOAXIAL
		if( PlaneTypeForNormal( tr->plane.normal ) == PLANE_NONAXIAL )
		{
			VectorMA( tr->endpos, TRACE_NOAXIAL_SAFETY_OFFSET, tr->plane.normal, tr->endpos );
		}
#endif
	}
}
