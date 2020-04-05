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
// cm_q1bsp.c -- Q1 BSP model loading

#include "qcommon.h"
#include "cm_local.h"

typedef struct chull_s {
	cnode_t         *clipnodes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
} chull_t;

//=========================================================

/*
* CMod_SurfaceFlags
*/
static int CMod_SurfaceFlags( int oldcontents ) {
	switch( oldcontents ) {
		case Q1_CONTENTS_WATER:
		case Q1_CONTENTS_SLIME:
		case Q1_CONTENTS_LAVA:
			return SURF_NOMARKS;
		case Q1_CONTENTS_SKY:
			return SURF_SKY | SURF_NOIMPACT | SURF_NOMARKS | SURF_NODLIGHT;
	}

	return 0;
}

/*
* CMod_SurfaceContents
*/
static int CMod_SurfaceContents( int oldcontents ) {
	switch( oldcontents ) {
		case Q1_CONTENTS_EMPTY:
			return 0;
		case Q1_CONTENTS_SOLID:
			return CONTENTS_SOLID;
		case Q1_CONTENTS_WATER:
			return CONTENTS_WATER;
		case Q1_CONTENTS_SLIME:
			return CONTENTS_SLIME;
		case Q1_CONTENTS_LAVA:
			return CONTENTS_LAVA;
		case Q1_CONTENTS_CLIP:
			return CONTENTS_SOLID;
	}

	return 0;
}

/*
===============================================================================

HULL BOXES

===============================================================================
*/

/*
* CM_HullForBSP
*
* Returns a hull that can be used for testing or clipping an object of mins/maxs size.
*/
static chull_t *CM_HullForBSP( cmodel_state_t *cms, cmodel_t *cmodel, const vec3_t mins, const vec3_t maxs ) {
	vec3_t size;
	int hullindex;

	VectorSubtract( maxs, mins, size );

	hullindex = cmodel - cms->map_cmodels;
	if( hullindex < 0 || hullindex >= cms->numcmodels ) {
		Com_Error( ERR_DROP, "CM_HullForBSP: bad cmodel" );
	}

	hullindex *= cms->nummaphulls;
	if( size[0] < 3 ) {
		return &cms->map_hulls[hullindex + 0];
	}
	if( size[0] <= 32 ) {
		return &cms->map_hulls[hullindex + 1];
	}
	return &cms->map_hulls[hullindex + 2];
}

/*
* CM_HullSizeForBBox
*/
void CM_HullSizeForBBox( cmodel_state_t *cms, vec3_t mins, vec3_t maxs, cmodel_t *cmodel ) {
	chull_t *hull;

	assert( mins && maxs );

	hull = CM_HullForBSP( cms, cmodel, mins, maxs );
	VectorCopy( hull->clip_mins, mins );
	VectorCopy( hull->clip_maxs, maxs );
}

/*
* CM_HullPointContents
*/
static int CM_HullPointContents( cmodel_state_t *cms, chull_t *hull, int num, vec3_t p ) {
	float d;
	cnode_t     *node;
	cplane_t    *plane;

	c_pointcontents++;

	while( num >= 0 ) {
		if( num < hull->firstclipnode || num > hull->lastclipnode ) {
			Com_Error( ERR_DROP, "CM_HullPointContents: bad node number" );
		}

		node = hull->clipnodes + num;
		plane = node->plane;
		d = PlaneDiff( p, plane );
		num = node->children[( d < 0 )];
	}

	return num;
}

/*
* CM_TransformedHullContents
*
* Handles offseting and rotation of the end points for moving and
* rotating entities
*/
static int CM_TransformedHullContents( cmodel_state_t *cms, vec3_t p, cmodel_t *cmodel, vec3_t origin, vec3_t angles ) {
	vec3_t p_l;
	vec3_t offset;
	chull_t *hull;

	// subtract origin offset
	hull = CM_HullForBSP( cms, cmodel, vec3_origin, vec3_origin );
	VectorAdd( hull->clip_mins, origin, offset );
	VectorSubtract( p, offset, p_l );

	// rotate start and end into the models frame of reference
	if( ( angles[0] || angles[1] || angles[2] )
		&& ( cmodel != cms->box_cmodel )
		) {
		vec3_t temp;
		mat3_t axis;

		AnglesToAxis( angles, axis );
		VectorCopy( p_l, temp );
		Matrix3_TransformVector( axis, temp, p_l );
	}

	return CMod_SurfaceContents( CM_HullPointContents( cms, hull, hull->firstclipnode, p_l ) );
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON    ( 0.03125 )

#define HULLCHECKSTATE_EMPTY 0
#define HULLCHECKSTATE_SOLID 1
#define HULLCHECKSTATE_DONE 2

static trace_t *trace_trace;

static vec3_t trace_start, trace_end;
static int trace_contents;

/*
* CM_RecursiveHullCheck
*/
static int CM_RecursiveHullCheck( cmodel_state_t *cms, chull_t *hull, int nodenum, float p1f, float p2f, vec3_t p1, vec3_t p2 ) {
	cnode_t     *node;
	cplane_t    *plane;
	float t1, t2;
	float frac;
	vec3_t mid;
	int side;
	int ret;
	float midf;

start:
	// check for empty
	if( nodenum < 0 ) {
		int contents;

		contents = CMod_SurfaceContents( nodenum );
		if( trace_contents & contents ) {
			c_brush_traces++;

			trace_trace->contents = contents;
			trace_trace->surfFlags = CMod_SurfaceFlags( nodenum );
			if( trace_trace->allsolid ) {
				trace_trace->startsolid = true;
			}
			return HULLCHECKSTATE_SOLID;
		} else {
			trace_trace->allsolid = false;
			return HULLCHECKSTATE_EMPTY;
		}
	}

	if( nodenum < hull->firstclipnode || nodenum > hull->lastclipnode ) {
		Com_Error( ERR_DROP, "SV_RecursiveHullCheck: bad node number" );
	}

	// find the point distances
	node = hull->clipnodes + nodenum;
	plane = node->plane;

	t1 = PlaneDiff( p1, plane );
	t2 = PlaneDiff( p2, plane );

	if( t1 >= 0 && t2 >= 0 ) {
		nodenum = node->children[0];    // go down the front side
		goto start;
	}
	if( t1 < 0 && t2 < 0 ) {
		nodenum = node->children[1];    // go down the back side
		goto start;
	}

	// find the intersection point
	frac = t1 / ( t1 - t2 );
	frac = Q_bound( 0, frac, 1 );
	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( p1, frac, p2, mid );
	side = t1 < 0;

	// recurse both sides, front side first

	ret = CM_RecursiveHullCheck( cms, hull, node->children[side], p1f, midf, p1, mid );
	// if this side is not empty, return what it is (solid or done)
	if( ret != HULLCHECKSTATE_EMPTY ) {
		return ret;
	}

	ret = CM_RecursiveHullCheck( cms, hull, node->children[side ^ 1], midf, p2f, mid, p2 );
	// if other side is not solid, return what it is (empty or done)
	if( ret != HULLCHECKSTATE_SOLID ) {
		return ret;
	}

	// the other side of the node is solid, this is the impact point
	if( !side ) {
		trace_trace->plane = *plane;
	} else {
		VectorNegate( plane->normal, trace_trace->plane.normal );
		trace_trace->plane.dist = -plane->dist;
		CategorizePlane( &trace_trace->plane );
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if( side ) {
		frac = ( t1 + DIST_EPSILON ) / ( t1 - t2 );
	} else {
		frac = ( t1 - DIST_EPSILON ) / ( t1 - t2 );
	}
	midf = p1f + ( p2f - p1f ) * Q_bound( 0, frac, 1 );

	trace_trace->fraction = Q_bound( 0, midf, 1 );
	VectorLerp( p1, frac, p2, trace_trace->endpos );

	return HULLCHECKSTATE_DONE;
}

/*
* CM_TransformedHullTrace
*
* Handles offseting and rotation of the end points for moving and
* rotating entities
*/
static void CM_TransformedHullTrace( cmodel_state_t *cms, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
									 cmodel_t *cmodel, int brushmask, vec3_t origin, vec3_t angles ) {
	chull_t *hull;
	vec3_t offset;
	vec3_t start_l, end_l;
	vec3_t a, temp;
	mat3_t axis;
	bool rotated;

	if( !tr ) {
		return;
	}

	cms->checkcount++;  // for multi-check avoidance
	c_traces++;     // for statistics, may be zeroed

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );
	tr->fraction = 1;

	if( !cms->numnodes ) { // map not loaded
		return;
	}

	// subtract origin offset
	hull = CM_HullForBSP( cms, cmodel, mins, maxs );
	VectorSubtract( hull->clip_mins, mins, offset );
	VectorAdd( offset, origin, offset );
	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	tr->allsolid = true;
	trace_trace = tr;
	trace_contents = brushmask;
	VectorCopy( start_l, trace_start );
	VectorCopy( end_l, trace_end );

	// rotate start and end into the models frame of reference
	if( ( angles[0] || angles[1] || angles[2] )
#ifndef CM_ALLOW_ROTATED_BBOXES
		&& ( cmodel != cms->box_cmodel )
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
	CM_RecursiveHullCheck( cms, hull, hull->firstclipnode, 0, 1, start_l, end_l );

	// check for position test special case
	if( VectorCompare( start, end ) ) {
		VectorCopy( start, trace_trace->endpos );
		return;
	}

	if( tr->fraction == 1 ) {
		VectorCopy( end, tr->endpos );
		return;
	}

	if( rotated ) {
		VectorNegate( angles, a );
		AnglesToAxis( a, axis );

		VectorCopy( tr->plane.normal, temp );
		Matrix3_TransformVector( axis, temp, tr->plane.normal );
	}

	// fix trace up by the offset
	VectorAdd( tr->endpos, offset, tr->endpos );
}

/*
===============================================================================

MAP LOADING

===============================================================================
*/

/*
* CMod_LoadClipnodes
*/
void CMod_LoadClipnodes( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	q1dclipnode_t   *in;
	cnode_t         *out;
	cnode_t         *node, *clipnode;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CM_LoadClipnodes: funny lump size" );
	}

	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no clipnodes" );
	}

	out = cms->map_clipnodes = Mem_Alloc( cms->mempool, (count + cms->numnodes) * sizeof( *out ) );
	cms->numclipnodes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = cms->map_planes + LittleLong( in->planenum );
		out->children[0] = LittleShort( in->children[0] );
		out->children[1] = LittleShort( in->children[1] );
	}

	// duplicate the drawing hull structure as clipping hull for hull0
	clipnode = cms->map_clipnodes + count;
	node = cms->map_nodes;
	for( i = 0; i < cms->numnodes; i++, node++, clipnode++ ) {
		clipnode->plane = node->plane;

		for( j = 0; j < 2; j++ ) {
			int child = node->children[j];
			if( child < 0 ) {
				clipnode->children[j] = cms->map_leafs[-child - 1].contents;
			} else {
				clipnode->children[j] = child;
			}
		}
	}
}

/*
* CMod_LoadSubmodels
*
* Returns number of clusters (visleafs of world model)
*/
static int CMod_LoadSubmodels( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int numvisleafs;
	int count;
	int headnode;
	q1dmodel_t  *in;
	cmodel_t    *out;
	chull_t     *hull;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadSubmodels: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no models" );
	}

	// allocate and initialize clipping hulls
	cms->nummaphulls = Q1_MAX_MAP_HULLS;
	cms->map_hulls = Mem_Alloc( cms->mempool, cms->nummaphulls * count * sizeof( chull_t ) );

	// initialize the clipping hull for world submodel

	hull = &cms->map_hulls[0 + 0];
	hull->clipnodes = cms->map_clipnodes + cms->numclipnodes;
	hull->firstclipnode = 0;
	hull->lastclipnode = cms->numnodes - 1;
	VectorSet( hull->clip_mins, 0, 0, 0 );
	VectorSet( hull->clip_maxs, 0, 0, 0 );

	hull = &cms->map_hulls[0 + 1];
	hull->clipnodes = cms->map_clipnodes;
	hull->firstclipnode = 0;
	hull->lastclipnode = cms->numclipnodes - 1;
	VectorSet( hull->clip_mins, -16, -16, -24 );
	VectorSet( hull->clip_maxs, 16, 16, 32 );

	hull = &cms->map_hulls[0 + 2];
	hull->clipnodes = cms->map_clipnodes;
	hull->firstclipnode = 0;
	hull->lastclipnode = cms->numclipnodes - 1;
	VectorSet( hull->clip_mins, -32, -32, -24 );
	VectorSet( hull->clip_maxs, 32, 32, 64 );

	// hull3 is unused in vanilla Q1

	// now load submodels
	out = cms->map_cmodels = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numcmodels = count;

	numvisleafs = LittleLong( in->visleafs );

	hull = cms->map_hulls;
	for( i = 0; i < count; i++, in++, out++ ) {
		for( j = 0; j < cms->nummaphulls; j++, hull++ ) {
			// if not world, copy hull from world submodel
			if( i ) {
				*hull = cms->map_hulls[0 + j];
			}

			headnode = LittleLong( in->headnode[j] );
			hull->firstclipnode = headnode;
		}

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}
	}

	return numvisleafs;
}

/*
* CMod_LoadMiptex
*/
static void CMod_LoadMiptex( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count, ofs;
	q1miptex_t  *in;
	q1dmiptexlump_t *miptex_lump;
	char texture[MAX_QPATH];
	cshaderref_t *out;
	size_t len, bufLen, bufSize;
	char        *buffer;

	miptex_lump = ( void * )( cms->cmod_base + l->fileofs );
	count = LittleLong( miptex_lump->nummiptex );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map has no miptex" );
	}

	out = cms->map_shaderrefs = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numshaderrefs = count;

	buffer = NULL;
	bufLen = bufSize = 0;

	for( i = 0; i < count; i++, out++, bufLen += len + 1 ) {
		ofs = LittleLong( miptex_lump->dataofs[i] );
		if( ofs < 0 ) {
			in = NULL;
		} else {
			in = ( q1miptex_t * )( ( uint8_t * )miptex_lump + ofs );
		}

		if( !in || !in->name[0] ) {
			Q_snprintfz( texture, sizeof( texture ), "textures/unnamed%d", i );
		} else {
			Q_snprintfz( texture, sizeof( texture ), "textures/%s", in->name );
			COM_StripExtension( texture );
		}

		len = strlen( texture );
		if( bufLen + len >= bufSize ) {
			bufSize = bufLen + len + 128;
			if( buffer ) {
				buffer = Mem_Realloc( buffer, bufSize );
			} else {
				buffer = Mem_Alloc( cms->mempool, bufSize );
			}
		}

		out->flags = 0;
		out->name = ( char * )( ( void * )bufLen );
		strcpy( buffer + bufLen, texture );

		if( !in || !in->name[0] ) {
			continue;
		}

		if( in->name[0] == '*' ) {
			out->flags |= Q2_SURF_WARP;
		} else if( !Q_strnicmp( in->name, "sky", 3 ) ) {
			out->flags |= Q2_SURF_SKY;
		}
	}

	for( i = 0; i < count; i++ )
		cms->map_shaderrefs[i].name = buffer + ( size_t )( ( void * )cms->map_shaderrefs[i].name );
}

/*
* CMod_LoadNodes
*/
static void CMod_LoadNodes( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	q1dnode_t   *in;
	cnode_t     *out;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadNodes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map has no nodes" );
	}

	out = cms->map_nodes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numnodes = count;

	for( i = 0; i < 3; i++ ) {
		cms->world_mins[i] = (float)LittleShort( in->mins[i] );
		cms->world_maxs[i] = (float)LittleShort( in->maxs[i] );
	}

	for( i = 0; i < count; i++, out++, in++ ) {
		out->plane = cms->map_planes + LittleLong( in->planenum );
		out->children[0] = LittleShort( in->children[0] );
		out->children[1] = LittleShort( in->children[1] );
	}
}

/*
* CMod_LoadLeafs
*/
static void CMod_LoadLeafs( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	cleaf_t     *out;
	q1dleaf_t   *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadLeafs: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leafs" );
	}

	out = cms->map_leafs = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = LittleLong( in->contents );
		out->cluster = LittleLong( in->visofs );
		if( out->cluster < 0 ) {
			out->cluster = -1;
		}
		out->area = 0;
	}

	cms->numareas = 1;
}

/*
* CMod_LoadPlanes
*/
static void CMod_LoadPlanes( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	cplane_t    *out;
	q1dplane_t  *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadPlanes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no planes" );
	}

	out = cms->map_planes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->signbits = 0;
		out->type = PLANE_NONAXIAL;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 ) {
				out->signbits |= ( 1 << j );
			}
			if( out->normal[j] == 1.0f ) {
				out->type = j;
			}
		}

		out->dist = LittleFloat( in->dist );
	}
}

/*
* CMod_LoadVisibility
*
* Decompresses PVS data, sets proper cluster values for leafs
*/
static void CMod_LoadVisibility( cmodel_state_t *cms, lump_t *l, int numvisleafs ) {
	int i;
	int numclusters;
	int rowsize, rowbytes;
	int visofs;
	uint8_t *in;
	cleaf_t *leaf;

	cms->map_visdatasize = l->filelen;
	if( !cms->map_visdatasize ) {
		// reset clusters for leafs
		for( i = 0, leaf = cms->map_leafs; i < cms->numleafs; i++, leaf++ ) {
			if( leaf->cluster >= 0 ) {
				leaf->cluster = 0;
			}
		}
		cms->map_pvs = NULL;
		return;
	}

	in = ( void * )( cms->cmod_base + l->fileofs );

	numclusters = numvisleafs;
	rowbytes = ( numclusters + 7 ) >> 3;
	rowsize = ( rowbytes + 15 ) & ~15;
	cms->map_visdatasize = sizeof( *( cms->map_pvs ) ) + numclusters * rowsize;

	cms->map_pvs = Mem_Alloc( cms->mempool, cms->map_visdatasize );
	cms->map_pvs->numclusters = numclusters;
	cms->map_pvs->rowsize = rowsize;

	for( i = 0, leaf = cms->map_leafs; i < cms->numleafs; i++, leaf++ ) {
		visofs = leaf->cluster;
		if( visofs >= 0 && ( i > 0 && i < numclusters + 1 ) ) {
			// cluster == visofs at this point
			leaf->cluster = i - 1;
			CM_DecompressVis( in + visofs, rowbytes, cms->map_pvs->data + leaf->cluster * rowsize );
		} else {
			leaf->cluster = -1;
		}
	}
}

/*
* CMod_LoadEntityString
*/
static void CMod_LoadEntityString( cmodel_state_t *cms, lump_t *l ) {
	cms->numentitychars = l->filelen;
	if( !l->filelen ) {
		return;
	}

	cms->map_entitystring = Mem_Alloc( cms->mempool, cms->numentitychars );
	memcpy( cms->map_entitystring, cms->cmod_base + l->fileofs, l->filelen );
}

/*
* CM_LoadQ1BrushModel
*/
void CM_LoadQ1BrushModel( cmodel_state_t *cms, void *parent, void *buf, bspFormatDesc_t *format ) {
	int i;
	int numvisleafs;
	q1dheader_t header;

	cms->cmap_bspFormat = format;

	header = *( q1dheader_t * )buf;
	for( i = 0; i < sizeof( header ) / 4; i++ )
		( (int *)&header )[i] = LittleLong( ( (int *)&header )[i] );
	cms->cmod_base = ( uint8_t * )buf;

	// load into heap
	CMod_LoadPlanes( cms, &header.lumps[Q1_LUMP_PLANES] );
	CMod_LoadMiptex( cms, &header.lumps[Q1_LUMP_TEXTURES] );
	CMod_LoadLeafs( cms, &header.lumps[Q1_LUMP_LEAFS] );
	CMod_LoadNodes( cms, &header.lumps[Q1_LUMP_NODES] );
	CMod_LoadClipnodes( cms, &header.lumps[Q1_LUMP_CLIPNODES] );
	numvisleafs = CMod_LoadSubmodels( cms, &header.lumps[Q1_LUMP_MODELS] );
	CMod_LoadVisibility( cms, &header.lumps[Q1_LUMP_VISIBILITY], numvisleafs );
	CMod_LoadEntityString( cms, &header.lumps[Q1_LUMP_ENTITIES] );

	// we don't have any brush info so override generic tracing code
	cms->CM_TransformedBoxTrace = CM_TransformedHullTrace;
	cms->CM_TransformedPointContents = CM_TransformedHullContents;
	cms->CM_RoundUpToHullSize = CM_HullSizeForBBox;

	FS_FreeFile( buf );
}
