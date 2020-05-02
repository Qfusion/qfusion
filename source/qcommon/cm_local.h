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

#include "qthreads.h"

#define MAX_CM_LEAFS        ( MAX_MAP_LEAFS )

#define CM_SUBDIV_LEVEL     ( 16 )

//#define TRACEVICFIX
#define TRACE_NOAXIAL_SAFETY_OFFSET 0.1

// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define SURFACE_CLIP_EPSILON    ( 0.125 )

typedef struct {
	int contents;
	int flags;
	char *name;
} cshaderref_t;

typedef struct {
	int children[2];            // negative numbers are leafs
	cplane_t *plane;
} cnode_t;

typedef struct {
	int surfFlags;
	cplane_t plane;
} cbrushside_t;

typedef struct {
	int contents;
	int numsides;

	vec3_t mins, maxs;

	cbrushside_t *brushsides;
} cbrush_t;

typedef struct {
	int contents;
	int numfacets;

	vec3_t mins, maxs;

	cbrush_t *facets;
} cface_t;

typedef struct {
	int contents;
	int cluster;

	int area;

	int nummarkbrushes;
	int nummarkfaces;

	int *markbrushes;
	int *markfaces;
} cleaf_t;

typedef struct cmodel_s {
	bool builtin;

	int nummarkfaces;
	int nummarkbrushes;

	float cyl_halfheight;
	float cyl_radius;
	vec3_t cyl_offset;

	vec3_t mins, maxs;

	cbrush_t *brushes;
	cface_t *faces;

	// dummy iterators for the tracing code
	// which treats brush models as leafs
	int *markfaces;
	int *markbrushes;
} cmodel_t;

typedef struct {
	int floodnum;               // if two areas have equal floodnums, they are connected
	int floodvalid;
} carea_t;

struct cmodel_state_s {
	volatile int refcount;
	qmutex_t *refcount_mutex;

	int checkcount;
	int floodvalid;

	struct cmodel_state_s *parent;
	struct mempool_s *mempool;

	const bspFormatDesc_t *cmap_bspFormat;

	char map_name[MAX_CONFIGSTRING_CHARS];
	unsigned int checksum;

	int numbrushsides;
	cbrushside_t *map_brushsides;

	int numshaderrefs;
	cshaderref_t *map_shaderrefs;

	int numplanes;
	cplane_t *map_planes;

	int numnodes;
	cnode_t *map_nodes;

	int numleafs;                   // = 1
	cleaf_t map_leaf_empty;         // allow leaf funcs to be called without a map
	cleaf_t *map_leafs;             // = &map_leaf_empty;

	int nummarkbrushes;
	int *map_markbrushes;

	int numcmodels;
	cmodel_t map_cmodel_empty;
	cmodel_t *map_cmodels;          // = &map_cmodel_empty;
	vec3_t world_mins, world_maxs;

	int numbrushes;
	cbrush_t *map_brushes;

	int numfaces;
	cface_t *map_faces;

	int nummarkfaces;
	int *map_markfaces;

	vec3_t *map_verts;              // this will be freed
	int numvertexes;

	// each area has a list of portals that lead into other areas
	// when portals are closed, other areas may not be visible or
	// hearable even if the vis info says that it should be
	int numareas;                   // = 1
	carea_t map_area_empty;
	carea_t *map_areas;             // = &map_area_empty;
	int *map_areaportals;

	dvis_t *map_pvs;
	int map_visdatasize;

	uint8_t nullrow[MAX_CM_LEAFS / 8];

	int numentitychars;
	char map_entitystring_empty;
	char *map_entitystring;         // = &map_entitystring_empty;

	uint8_t *cmod_base;

	// cm_trace.c
	cbrushside_t box_brushsides[6];
	cbrush_t box_brush[1];
	int box_markbrushes[1];
	cmodel_t box_cmodel[1];
	int box_checkcount;

	cbrushside_t oct_brushsides[10];
	cbrush_t oct_brush[1];
	int oct_markbrushes[1];
	cmodel_t oct_cmodel[1];
	int oct_checkcount;

	int *map_brush_checkcheckouts;
	int *map_face_checkcheckouts;

	// ==== Q1 specific stuff ===
	int numclipnodes;
	cnode_t *map_clipnodes;

	int nummaphulls;
	struct chull_s *map_hulls;      // nummaphulls * numcmodels
	// ==== Q1 specific stuff ===

	int leaf_count, leaf_maxcount;
	int *leaf_list;
	float *leaf_mins, *leaf_maxs;
	int leaf_topnode;

	// optional special handling of line tracing and point contents
	void ( *CM_TransformedBoxTrace )( struct cmodel_state_s *cms, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles );
	int ( *CM_TransformedPointContents )( struct cmodel_state_s *cms, vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles );
	void ( *CM_RoundUpToHullSize )( struct cmodel_state_s *cms, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel );
};

//=======================================================================

void    CM_InitBoxHull( cmodel_state_t *cms );
void    CM_InitOctagonHull( cmodel_state_t *cms );

void    CM_FloodAreaConnections( cmodel_state_t *cms );

void	CM_BoundBrush( cbrush_t *brush );

uint8_t *CM_DecompressVis( const uint8_t *in, int rowsize, uint8_t *decompressed );
