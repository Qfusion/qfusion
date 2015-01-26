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

#define MAX_CM_LEAFS		( MAX_MAP_LEAFS )

#define CM_SUBDIV_LEVEL		( 16 )

//#define TRACEVICFIX
#define TRACE_NOAXIAL_SAFETY_OFFSET 0.1

// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define	SURFACE_CLIP_EPSILON	(0.125)

typedef struct
{
	char *name;
	int contents;
	int flags;
} cshaderref_t;

typedef struct
{
	cplane_t *plane;
	int children[2];            // negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t *plane;
	int surfFlags;
} cbrushside_t;

typedef struct
{
	int contents;
	int checkcount;             // to avoid repeated testings

	int numsides;
	cbrushside_t *brushsides;
} cbrush_t;

typedef struct
{
	int contents;
	int checkcount;             // to avoid repeated testings

	vec3_t mins, maxs;

	int numfacets;
	cbrush_t *facets;
} cface_t;

typedef struct
{
	int contents;
	int cluster;

	int area;

	int nummarkbrushes;
	cbrush_t **markbrushes;

	int nummarkfaces;
	cface_t	**markfaces;
} cleaf_t;

typedef struct cmodel_s
{
	vec3_t mins, maxs;

	int nummarkfaces;
	cface_t	**markfaces;

	int nummarkbrushes;
	cbrush_t **markbrushes;

	vec3_t cyl_offset;
	float cyl_halfheight;
	float cyl_radius;

	bool builtin;
} cmodel_t;

typedef struct
{
	int floodnum;               // if two areas have equal floodnums, they are connected
	int floodvalid;
} carea_t;

struct cmodel_state_s
{
	int checkcount;
	int refcount;
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
	cnode_t	*map_nodes;

	int numleafs;                   // = 1
	cleaf_t	map_leaf_empty;         // allow leaf funcs to be called without a map
	cleaf_t	*map_leafs;             // = &map_leaf_empty;

	int nummarkbrushes;
	cbrush_t **map_markbrushes;

	int numcmodels;
	cmodel_t map_cmodel_empty;
	cmodel_t *map_cmodels;          // = &map_cmodel_empty;
	vec3_t world_mins, world_maxs;

	int numbrushes;
	cbrush_t *map_brushes;

	int numfaces;
	cface_t	*map_faces;

	int nummarkfaces;
	cface_t	**map_markfaces;

	vec3_t *map_verts;              // this will be freed
	int numvertexes;

	// each area has a list of portals that lead into other areas
	// when portals are closed, other areas may not be visible or
	// hearable even if the vis info says that it should be
	int numareas;                   // = 1
	carea_t	map_area_empty;
	carea_t	*map_areas;             // = &map_area_empty;
	int *map_areaportals;

	dvis_t *map_pvs, *map_phs;
	int map_visdatasize;

	uint8_t nullrow[MAX_CM_LEAFS/8];

	int numentitychars;
	char map_entitystring_empty;
	char *map_entitystring;         // = &map_entitystring_empty;

	int floodvalid;

	uint8_t *cmod_base;

	// cm_trace.c
	cplane_t box_planes[6];
	cbrushside_t box_brushsides[6];
	cbrush_t box_brush[1];
	cbrush_t *box_markbrushes[1];
	cmodel_t box_cmodel[1];

	cplane_t oct_planes[10];
	cbrushside_t oct_brushsides[10];
	cbrush_t oct_brush[1];
	cbrush_t *oct_markbrushes[1];
	cmodel_t oct_cmodel[1];

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

void	CM_InitBoxHull( cmodel_state_t *cms );
void	CM_InitOctagonHull( cmodel_state_t *cms );

void	CM_FloodAreaConnections( cmodel_state_t *cms );
