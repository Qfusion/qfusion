/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2007 Victor Luchits

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

#include "qcommon/qcommon.h"
#include "r_mesh.h"
#include "r_shader.h"
#include "r_surface.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

#define CLUSTER_UNKNOWN -2
#define CLUSTER_INVALID -1

//
// in memory representation
//
typedef struct {
	float radius;

	unsigned int firstModelSurface;
	unsigned int numModelSurfaces;

	unsigned int firstModelDrawSurface;
	unsigned int numModelDrawSurfaces;

	vec3_t mins, maxs;
} mmodel_t;

typedef struct mshaderref_s {
	char name[MAX_QPATH];
	int flags;
	int contents;
	shader_t *shaders[NUM_SHADER_TYPES_BSP];
} mshaderref_t;

typedef struct msurface_s {
	unsigned int facetype, flags;

	unsigned int firstDrawSurfVert, firstDrawSurfElem;

	unsigned int numInstances;

	unsigned int drawSurf;

	int fragmentframe;                  // for multi-check avoidance

	vec4_t plane;

	union {
		float origin[3];
		float mins[3];
	};
	union {
		float maxs[3];
		float color[3];
	};

	mesh_t mesh;

	instancePoint_t *instances;

	shader_t *shader;

	int superLightStyle;
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	cplane_t        *plane;

	// node specific
	struct mnode_s  *children[2];
} mnode_t;

typedef struct mleaf_s {
	// common with node
	cplane_t        *plane;

	// leaf specific
	int cluster, area;

	float mins[3];
	float maxs[3];                      // for bounding box culling

	unsigned numVisSurfaces;
	unsigned *visSurfaces;

	unsigned numFragmentSurfaces;
	unsigned *fragmentSurfaces;
} mleaf_t;

typedef struct mbrushmodel_s {
	const bspFormatDesc_t *format;

	dvis_t          *pvs;

	unsigned int numsubmodels;
	mmodel_t        *submodels;
	struct model_s  *inlines;

	unsigned int numModelSurfaces;
	unsigned int firstModelSurface;

	unsigned int numModelDrawSurfaces;
	unsigned int firstModelDrawSurface;

	msurface_t      *modelSurfaces;

	unsigned int numplanes;
	cplane_t        *planes;

	unsigned int numleafs;              // number of visible leafs, not counting 0
	mleaf_t         *leafs;

	unsigned int numnodes;
	mnode_t         *nodes;

	unsigned int numsurfaces;
	msurface_t      *surfaces;

	/*unsigned*/ int numareas;

	vec3_t gridSize;
	vec3_t gridMins;
	int gridBounds[4];

	unsigned int numDrawSurfaces;
	drawSurfaceBSP_t *drawSurfaces;

	unsigned entityStringLen;
	char *entityString;
} mbrushmodel_t;

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

//
// in memory representation
//
typedef struct {
	short point[3];
	uint8_t latlong[2];                     // use bytes to keep 8-byte alignment
} maliasvertex_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t scale;
	vec3_t translate;
	float radius;
} maliasframe_t;

typedef struct {
	char name[MD3_MAX_PATH];
	quat_t quat;
	vec3_t origin;
} maliastag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	shader_t        *shader;
} maliasskin_t;

typedef struct maliasmesh_s {
	char name[MD3_MAX_PATH];

	int numverts;
	maliasvertex_t *vertexes;
	vec2_t          *stArray;

	vec4_t          *xyzArray;
	vec4_t          *normalsArray;
	vec4_t          *sVectorsArray;

	int numtris;
	elem_t          *elems;

	int numskins;
	maliasskin_t    *skins;

	struct mesh_vbo_s *vbo;
} maliasmesh_t;

typedef struct maliasmodel_s {
	int numframes;
	maliasframe_t   *frames;

	int numtags;
	maliastag_t     *tags;

	int nummeshes;
	maliasmesh_t    *meshes;
	drawSurfaceAlias_t *drawSurfs;

	int numskins;
	maliasskin_t    *skins;

	int numverts;             // sum of numverts for all meshes
	int numtris;             // sum of numtris for all meshes
} maliasmodel_t;

/*
==============================================================================

SKELETAL MODELS

==============================================================================
*/

//
// in memory representation
//
#define SKM_MAX_WEIGHTS     4

//
// in memory representation
//
typedef struct {
	char            *name;
	shader_t        *shader;
} mskskin_t;

typedef struct {
	uint8_t indices[SKM_MAX_WEIGHTS];
	uint8_t weights[SKM_MAX_WEIGHTS];
} mskblend_t;

typedef struct mskmesh_s {
	char            *name;

	uint8_t         *blendIndices;
	uint8_t         *blendWeights;

	unsigned int numverts;
	vec4_t          *xyzArray;
	vec4_t          *normalsArray;
	vec2_t          *stArray;
	vec4_t          *sVectorsArray;

	unsigned int    *vertexBlends;  // [0..numbones-1] reference directly to bones
	                                // [numbones..numbones+numblendweights-1] reference to model blendweights

	unsigned int maxWeights;        // the maximum number of bones, affecting a single vertex in the mesh

	unsigned int numtris;
	elem_t          *elems;

	mskskin_t skin;

	struct mesh_vbo_s *vbo;
} mskmesh_t;

typedef struct {
	char            *name;
	signed int parent;
	unsigned int flags;
} mskbone_t;

typedef struct {
	vec3_t mins, maxs;
	float radius;
	bonepose_t      *boneposes;
} mskframe_t;

typedef struct mskmodel_s {
	unsigned int numbones;
	mskbone_t       *bones;

	unsigned int nummeshes;
	mskmesh_t       *meshes;
	drawSurfaceSkeletal_t *drawSurfs;

	unsigned int numtris;
	elem_t          *elems;

	unsigned int numverts;
	vec4_t          *xyzArray;
	vec4_t          *normalsArray;
	vec2_t          *stArray;
	vec4_t          *sVectorsArray;
	uint8_t         *blendIndices;
	uint8_t         *blendWeights;

	unsigned int numblends;
	mskblend_t      *blends;
	unsigned int    *vertexBlends;  // [0..numbones-1] reference directly to bones
	                                // [numbones..numbones+numblendweights-1] reference to blendweights

	unsigned int numframes;
	mskframe_t      *frames;
	bonepose_t      *invbaseposes;
} mskmodel_t;

//===================================================================

//
// Whole model
//

typedef enum { mod_bad = -1, mod_free, mod_brush, mod_alias, mod_skeletal, mod_sprite } modtype_t;
typedef void ( *mod_touch_t )( struct model_s *model );

#define MOD_MAX_LODS    4

typedef struct model_s {
	char            *name;
	int registrationSequence;
	mod_touch_t touch;          // touching a model updates registration sequence, images and VBO's

	modtype_t type;

	//
	// volume occupied by the model graphics
	//
	vec3_t mins, maxs;
	float radius;

	//
	// memory representation pointer
	//
	void            *extradata;

	int lodnum;                 // LOD index, 0 for parent model, 1..MOD_MAX_LODS for LOD models
	int numlods;
	struct model_s  *lods[MOD_MAX_LODS];

	mempool_t       *mempool;
} model_t;

//============================================================================

extern model_t *r_prevworldmodel;

void        R_InitModels( void );
void        R_ShutdownModels( void );
void        R_FreeUnusedModels( void );

void        R_ModelBounds( const model_t *model, vec3_t mins, vec3_t maxs );
void        R_ModelFrameBounds( const struct model_s *model, int frame, vec3_t mins, vec3_t maxs );
void        R_RegisterWorldModel( const char *model );
struct model_s *R_RegisterModel( const char *name );

void		R_GetTransformBufferForMesh( mesh_t *mesh, bool positions, bool normals, bool sVectors );

model_t     *Mod_ForName( const char *name, bool crash );
mleaf_t     *Mod_PointInLeaf( const vec3_t p, mbrushmodel_t *bmodel );
uint8_t     *Mod_ClusterPVS( int cluster, mbrushmodel_t *bmodel );
uint8_t		*Mod_SpherePVS( const vec3_t origin, float radius, mbrushmodel_t *bmodel, uint8_t *fatpvs );

unsigned int Mod_Handle( const model_t *mod );
model_t     *Mod_ForHandle( unsigned int elem );

// force 16-bytes alignment for all memory chunks allocated for model data
#define     Mod_Malloc( mod, size ) Mem_AllocExt( ( mod )->mempool, size, 1 )
#define     Mod_Realloc( data, size ) Mem_Realloc( data, size )
#define     Mod_MemFree( data ) Mem_Free( data )

void        Mod_StripLODSuffix( char *name );

void        Mod_Modellist_f( void );
