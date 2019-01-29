/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2013 Victor Luchits

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
#include "qcommon/patch.h"

typedef struct mempool_s mempool_t;
typedef struct qthread_s qthread_t;
typedef struct qmutex_s qmutex_t;
typedef struct qbufPipe_s qbufPipe_t;

typedef unsigned short elem_t;

typedef vec_t instancePoint_t[8]; // quaternion for rotation + xyz pos + uniform scale

#include "r_math.h"

#define NUM_CUSTOMCOLORS        16

#define SUBDIVISIONS_MIN        3
#define SUBDIVISIONS_MAX        16
#define SUBDIVISIONS_DEFAULT    5

#define MIN_FRAMECACHE_SIZE		32 * 1024

#define BACKFACE_EPSILON        4

#define ON_EPSILON              0.03125 // 1/32 to keep floating point happy

#define Z_NEAR                  4.0f
#define Z_BIAS                  64.0f

#define POLYOFFSET_FACTOR       -2.0f
#define POLYOFFSET_UNITS        -1.0f

#define SIDE_FRONT              0
#define SIDE_BACK               1
#define SIDE_ON                 2

#define RF_BIT( x )             ( 1ULL << ( x ) )

#define RF_NONE                 0
#define RF_DRAWFLAT             ( 1 << 0 )
#define RF_SOFT_PARTICLES       ( 1 << 1 )

#define MAX_REF_ENTITIES        ( MAX_ENTITIES + 48 ) // must not exceed 2048 because of sort key packing

#define BLUENOISE_TEXTURE_SIZE 128

#include "r_public.h"
#include "r_vattribs.h"
#include "r_glimp.h"
#include "r_surface.h"
#include "r_image.h"
#include "r_mesh.h"
#include "r_shader.h"
#include "r_backend.h"
#include "r_model.h"
#include "r_trace.h"
#include "r_program.h"
#include "r_jobs.h"

extern const elem_t r_boxedges[24];

//===================================================================

// cached for this frame for zero LOD
typedef struct {
	int mod_type;
	bool rotated;
	float radius;

	vec3_t mins, maxs;
	vec3_t absmins, absmaxs;
} entSceneCache_t;

typedef struct refScreenTexSet_s {
	image_t         *screenTex;
	image_t         *screenTexCopy;
	image_t         *screenPPCopies[2];
	image_t         *screenDepthTex;
	image_t         *screenDepthTexCopy;
	int multisampleTarget;                // multisample fbo
} refScreenTexSet_t;

typedef struct refinst_s {
	unsigned int clipFlags;
	unsigned int renderFlags;

	int renderTarget;                       // target framebuffer object
	bool multisampleDepthResolved;

	int scissor[4];
	int viewport[4];

	//
	// view origin
	//
	vec3_t viewOrigin;
	mat3_t viewAxis;
	cplane_t frustum[6];
	vec3_t frustumCorners[4];

	float nearClip, farClip;
	float polygonFactor, polygonUnits;

	vec3_t visMins, visMaxs;
	vec3_t pvsMins, pvsMaxs;
	float visFarClip;
	float hdrExposure;

	int viewcluster, viewarea;

	vec3_t pvsOrigin;
	cplane_t clipPlane;

	mat4_t objectMatrix;
	mat4_t cameraMatrix;

	mat4_t modelviewMatrix;
	mat4_t projectionMatrix;

	mat4_t cameraProjectionMatrix;                  // cameraMatrix * projectionMatrix
	mat4_t modelviewProjectionMatrix;               // modelviewMatrix * projectionMatrix

	refdef_t refdef;

	unsigned int numEntities;
	int *entities;
	uint8_t *entpvs;

	struct refinst_s *parent;

	refScreenTexSet_t *st;                  // points to either either a 8bit or a 16bit float set

	drawList_t      *meshlist;              // meshes to be rendered

	uint8_t			*pvs, *areabits;
} refinst_t;

//====================================================

// globals shared by the frontend and the backend
// the backend should never attempt modifying any of these
typedef struct {
	// any asset (model, shader, texture, etc) with has not been registered
	// or "touched" during the last registration sequence will be freed
	volatile int registrationSequence;
	volatile bool registrationOpen;

	// bumped each time R_RegisterWorldModel is called
	volatile int worldModelSequence;

	float sinTableByte[256];

	model_t *worldModel;
	mbrushmodel_t *worldBrushModel;

	struct mesh_vbo_s *nullVBO;
	struct mesh_vbo_s *postProcessingVBO;
	struct mesh_vbo_s *skyVBO;

	vec3_t wallColor, floorColor;

	image_t *rawTexture;                // cinematic texture (RGB)
	image_t *noTexture;                 // use for bad textures
	image_t *whiteTexture;
	image_t *whiteCubemapTexture;
	image_t *blackTexture;
	image_t *greyTexture;
	image_t *blankBumpTexture;
	image_t *particleTexture;           // little dot for particles
	image_t *blueNoiseTexture;

	refScreenTexSet_t st, stf, st2D;

	shader_t *envShader;
	shader_t *whiteShader;

	byte_vec4_t customColors[NUM_CUSTOMCOLORS];
} r_shared_t;

typedef struct {
	// bumped each R_ClearScene
	unsigned int frameCount;

	int worldModelSequence;

	unsigned int numEntities;
	unsigned int numLocalEntities;
	entity_t entities[MAX_REF_ENTITIES];
	entSceneCache_t entSceneCache[MAX_REF_ENTITIES];
	entity_t *worldent;
	entity_t *polyent;
	entity_t *polyweapent;

	unsigned int numPolys;
	drawSurfacePoly_t polys[MAX_POLYS];

	unsigned int numBmodelEntities;
	int bmodelEntities[MAX_REF_ENTITIES];

	refdef_t refdef;
} r_scene_t;

// global frontend variables are stored here
// the backend should never attempt reading or modifying them
typedef struct {
	struct {
		bool enabled;
		int width, height;
		bool multiSamples;
	} twoD;

	// the default or multi-sampled framebuffer 
	int renderTarget;

	int frameBufferWidth, frameBufferHeight;

	// used for dlight push checking
	unsigned int frameCount;

	struct {
		unsigned int c_brush_polys, c_world_leafs;
		unsigned int c_world_draw_surfs;
		unsigned int c_world_lights, c_dynamic_lights;
		unsigned int c_ents_total, c_ents_bmodels;
		unsigned int t_cull_world_nodes, t_cull_world_surfs;
		unsigned int t_world_node, t_light_node;
		unsigned int t_add_world_surfs;
		unsigned int t_add_polys, t_add_entities;
		unsigned int t_draw_meshes;
	} stats;

	struct {
		unsigned average;        // updates 4 times per second
		int64_t time, oldTime;
		unsigned count, oldCount;
	} frameTime;

	char speedsMsg[2048];
	qmutex_t        *speedsMsgLock;

	rtrace_t		debugTrace;
	msurface_t      *debugSurface;
	qmutex_t        *debugSurfaceLock;
} r_globals_t;

extern ref_import_t ri;

extern r_shared_t rsh;
extern r_scene_t rsc;
extern r_globals_t rf;

#define R_ENT2NUM( ent ) ( ( ent ) - rsc.entities )
#define R_NUM2ENT( num ) ( rsc.entities + ( num ) )

#define R_ENTNUMCACHE( num ) ( rsc.entSceneCache + num )
#define R_ENTCACHE( ent ) R_ENTNUMCACHE( R_ENT2NUM( ent ) )

extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_sRGB;

extern cvar_t *r_subdivisions;
extern cvar_t *r_showtris;
extern cvar_t *r_showtris2D;
extern cvar_t *r_leafvis;

extern cvar_t *r_lighting_specular;
extern cvar_t *r_lighting_glossintensity;
extern cvar_t *r_lighting_glossexponent;
extern cvar_t *r_lighting_ambientscale;
extern cvar_t *r_lighting_directedscale;

extern cvar_t *r_outlines_world;
extern cvar_t *r_outlines_scale;
extern cvar_t *r_outlines_cutoff;

extern cvar_t *r_soft_particles;
extern cvar_t *r_soft_particles_scale;

extern cvar_t *r_hdr;
extern cvar_t *r_hdr_gamma;
extern cvar_t *r_hdr_exposure;

extern cvar_t *r_samples;

extern cvar_t *r_gamma;
extern cvar_t *r_texturefilter;
extern cvar_t *r_mode;
extern cvar_t *r_polyblend;
extern cvar_t *r_screenshot_fmtstr;

extern cvar_t *r_drawflat;
extern cvar_t *r_wallcolor;
extern cvar_t *r_floorcolor;

//====================================================================

void R_NormToLatLong( const vec_t *normal, uint8_t latlong[2] );
void R_LatLongToNorm( const uint8_t latlong[2], vec3_t out );
void R_LatLongToNorm4( const uint8_t latlong[2], vec4_t out );

#define R_LinearFloatFromsRGBFloat( c ) ( ( ( c ) <= 0.04045f ) ? ( c ) * ( 1.0f / 12.92f ) : (float)pow( ( ( c ) + 0.055f ) * ( 1.0f / 1.055f ), 2.4f ) )
#define R_sRGBFloatFromLinearFloat( c ) ( ( ( c ) < 0.0031308f ) ? ( c ) * 12.92f : 1.055f * (float)pow( ( c ), 1.0f / 2.4f ) - 0.055f )
#define R_LinearFloatFromsRGB( c ) Image_LinearFloatFromsRGBFloat( ( c ) * ( 1.0f / 255.0f ) )
#define R_sRGBFloatFromLinear( c ) Image_sRGBFloatFromLinearFloat( ( c ) * ( 1.0f / 255.0f ) )

//====================================================================

//
// r_alias.c
//
void R_CacheAliasModelEntity( const entity_t *e );
bool R_AddAliasModelToDrawList( const entity_t *e, int lod );
void R_DrawAliasSurf( const entity_t *e, const shader_t *shader, drawSurfaceAlias_t *drawSurf );
bool R_AliasModelLerpTag( orientation_t *orient, const maliasmodel_t *aliasmodel, int framenum, int oldframenum,
	float lerpfrac, const char *name );
void R_AliasModelFrameBounds( const model_t *mod, int frame, vec3_t mins, vec3_t maxs );

void Mod_LoadAliasMD3Model( model_t *mod, const model_t *parent, void *buffer, bspFormatDesc_t *unused );
void Mod_LoadSkeletalModel( model_t *mod, const model_t *parent, void *buffer, bspFormatDesc_t *unused );
void Mod_LoadQ3BrushModel( model_t *mod, const model_t *parent, void *buffer, bspFormatDesc_t *format );

//
// r_cmds.c
//
void R_TakeScreenShot( const char *path, const char *name, const char *fmtString, int x, int y, int w, int h, bool silent );
void R_ScreenShot_f( void );
void R_ImageList_f( void );
void R_ShaderList_f( void );
void R_ShaderDump_f( void );

//
// r_cull.c
//
void R_SetupFrustum( const refdef_t *rd, float nearClip, float farClip, cplane_t *frustum, vec3_t corner[4] );
int R_DeformFrustum( const cplane_t *frustum, const vec3_t corners[4], const vec3_t origin, const vec3_t point, cplane_t *deformed );
bool R_CullBoxCustomPlanes( const cplane_t *p, unsigned nump, const vec3_t mins, const vec3_t maxs, unsigned int clipflags );
bool R_CullSphereCustomPlanes( const cplane_t *p, unsigned nump, const vec3_t centre, const float radius, unsigned int clipflags );
bool R_CullBox( const vec3_t mins, const vec3_t maxs, const unsigned int clipflags );
bool R_CullSphere( const vec3_t centre, const float radius, const unsigned int clipflags );
bool R_VisCullBox( const vec3_t mins, const vec3_t maxs );
bool R_VisCullSphere( const vec3_t origin, float radius );
bool R_CullModelEntity( const entity_t *e, bool pvsCull );
void R_OrthoFrustumPlanesFromCorners( vec3_t corners[8], cplane_t *frustum );
float R_ProjectFarFrustumCornersOnBounds( vec3_t corners[8], const vec3_t mins, const vec3_t maxs );

//
// r_framebuffer.c
//
enum {
	FBO_COPY_NORMAL = 0,
	FBO_COPY_CENTREPOS = 1,
	FBO_COPY_INVERT_Y = 2,
	FBO_COPY_NORMAL_DST_SIZE = 3,
};

void RFB_Init( void );
int RFB_RegisterObject( int width, int height, bool builtin, bool depthRB, bool colorRB, int samples, bool useFloat, bool sRGB );
void RFB_UnregisterObject( int object );
void RFB_TouchObject( int object );
void RFB_BindObject( int object );
int RFB_BoundObject( void );
bool RFB_AttachTextureToObject( int object, bool depth, int target, image_t *texture );
image_t *RFB_GetObjectTextureAttachment( int object, bool depth, int target );
bool RFB_HasColorRenderBuffer( int object );
bool RFB_HasDepthRenderBuffer( int object );
int RFB_GetSamples( int object );
bool RFB_sRGBColorSpace( int object );
void RFB_BlitObject( int src, int dest, int bitMask, int mode, int filter, int readAtt, int drawAtt );
bool RFB_CheckObjectStatus( void );
void RFB_GetObjectSize( int object, int *width, int *height );
void RFB_FreeUnusedObjects( void );
void RFB_Shutdown( void );

//
// r_main.c
//
extern mempool_t *r_mempool;

#define R_Malloc( size ) R_Malloc_( size, __FILE__, __LINE__ )
#define R_Realloc( data, size ) Mem_Realloc( data, size )
#define R_Free( data ) Mem_Free( data )
#define R_AllocPool( parent, name ) Mem_AllocPool( parent, name )
#define R_FreePool( pool ) Mem_FreePool( pool )
#define R_MallocExt( pool, size, align, z ) _Mem_AllocExt( pool, size, align, z, 0, 0, __FILE__, __LINE__ )

ATTRIBUTE_MALLOC void * R_Malloc_( size_t size, const char *filename, int fileline );
char *R_CopyString_( const char *in, const char *filename, int fileline );
#define     R_CopyString( in ) R_CopyString_( in,__FILE__,__LINE__ )

int R_LoadFile_( const char *path, int flags, void **buffer, const char *filename, int fileline );
void R_FreeFile_( void *buffer, const char *filename, int fileline );

#define     R_LoadFile( path,buffer ) R_LoadFile_( path,0,buffer,__FILE__,__LINE__ )
#define     R_LoadCacheFile( path,buffer ) R_LoadFile_( path,FS_CACHE,buffer,__FILE__,__LINE__ )
#define     R_FreeFile( buffer ) R_FreeFile_( buffer,__FILE__,__LINE__ )

bool R_IsRenderingToScreen( void );
void R_BeginFrame();
void R_EndFrame( void );
void R_SetGamma( float gamma );
void R_SetWallFloorColors( const vec3_t wallColor, const vec3_t floorColor );
void R_SetupPVSFromCluster( int cluster, int area );
void R_SetupPVS( const refdef_t *fd );
void R_SetCameraAndProjectionMatrices( const mat4_t cam, const mat4_t proj );
void R_SetupViewMatrices( const refdef_t *rd );
void R_RenderView( const refdef_t *fd );
const msurface_t *R_GetDebugSurface( void );
const char *R_WriteSpeedsMessage( char *out, size_t size );
void R_RenderDebugSurface( const refdef_t *fd );
void R_Finish( void );
void R_Flush( void );

void R_Begin2D( bool multiSamples );
void R_SetupGL2D( void );
void R_End2D( void );

int R_MultisampleSamples( int samples );
int R_RegisterMultisampleTarget( refScreenTexSet_t *st, int samples, bool useFloat, bool sRGB );

void R_BlitTextureToScrFbo( const refdef_t *fd, image_t *image, int dstFbo,
	int program_type, const vec4_t color, int blendMask, int numShaderImages, image_t **shaderImages,
	int iParam0 );

/**
 * Defer R_DataSync call at the start/end of the next frame.
 */
void R_DeferDataSync( void );

float R_DefaultFarClip( void );

void R_BatchSpriteSurf( const entity_t *e, const shader_t *shader, drawSurfaceType_t *drawSurf, bool mergable );

struct mesh_vbo_s *R_InitNullModelVBO( void );
void R_DrawNullSurf( const entity_t *e, const shader_t *shader, drawSurfaceType_t *drawSurf );

void R_CacheSpriteEntity( const entity_t *e );

struct mesh_vbo_s *R_InitPostProcessingVBO( void );

void R_TransformForWorld( void );
void R_TransformForEntity( const entity_t *e );
void R_TranslateForEntity( const entity_t *e );
void R_TransformBounds( const vec3_t origin, const mat3_t axis, vec3_t mins, vec3_t maxs, vec3_t bbox[8] );

bool R_ScissorForCorners( const refinst_t *rnp, vec3_t corner[8], int *scissor );
bool R_ScissorForBBox( const refinst_t *rnp, vec3_t mins, vec3_t maxs, int *scissor );

void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
							  const vec4_t color, const shader_t *shader );
void R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
									 float angle, const vec4_t color, const shader_t *shader );
void R_DrawStretchQuick( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
								const vec4_t color, int program_type, image_t *image, int blendMask );

void R_InitCustomColors( void );
void R_SetCustomColor( int num, int r, int g, int b );
int R_GetCustomColor( int num );
void R_ShutdownCustomColors( void );

void R_ClearRefInstStack( void );
refinst_t  *R_PushRefInst( void );
void R_PopRefInst( void );

void R_BindFrameBufferObject( int object );

void R_Scissor( int x, int y, int w, int h );
void R_ResetScissor( void );

void R_FrameCache_Free( void );
void R_FrameCache_Clear( void );
void *R_FrameCache_Alloc_( size_t size, const char *filename, int fileline );
size_t R_FrameCache_TotalSize( void );
void *R_FrameCache_SetMark_( const char *filename, int fileline );
void R_FrameCache_FreeToMark_( void *mark, const char *filename, int fileline );

#define R_FrameCache_Alloc(s) R_FrameCache_Alloc_(s,__FILE__,__LINE__)
#define R_FrameCache_SetMark() R_FrameCache_SetMark_(__FILE__,__LINE__)
#define R_FrameCache_FreeToMark(m) R_FrameCache_FreeToMark_(m,__FILE__,__LINE__)

//
// r_mesh.c
//
void R_InitDrawList( drawList_t *list );
void R_ClearDrawList( drawList_t *list );
void *R_AddSurfToDrawList( drawList_t *list, const entity_t *e, const shader_t *shader, float dist, unsigned int order, void *drawSurf );
void R_ReserveDrawListWorldSurfaces( drawList_t *list );

void R_InitDrawLists( void );

void R_SortDrawList( drawList_t *list );
void R_DrawSurfaces( drawList_t *list );
void R_DrawOutlinedSurfaces( drawList_t *list );
void R_WalkDrawList( drawList_t *list, walkDrawSurf_cb_cb cb, void *ptr );

void R_CopyOffsetElements( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_CopyOffsetTriangles( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_BuildTrifanElements( int vertsOffset, int numVerts, elem_t *elems );
void R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray, vec2_t *stArray,
							int numTris, elem_t *elems, vec4_t *sVectorsArray );

//
// r_poly.c
//
void R_BatchPolySurf( const entity_t *e, const shader_t *shader, drawSurfacePoly_t *poly, bool mergable );
void R_DrawPolys( void );
bool R_SurfPotentiallyFragmented( const msurface_t *surf );
int R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts,
								   vec4_t *fverts, int maxfragments, fragment_t *fragments );

//
// r_register.c
//
rserr_t R_Init( bool verbose );
void R_BindGlobalVAO();
void R_BeginRegistration( void );
void R_EndRegistration( void );
void R_Shutdown( bool verbose );

//
// r_scene.c
//
extern drawList_t r_worldlist;

void R_AddDebugCorners( const vec3_t corners[8], const vec4_t color );
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs, const vec4_t color );
void R_ClearScene( void );
void R_AddEntityToScene( const entity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void R_AddPolyToScene( const poly_t *poly );
void R_RenderScene( const refdef_t *fd );
void R_BlurScreen( void );

//
// r_surf.c
//
#define MAX_SURF_QUERIES        0x1E0

void R_DrawWorldNode( void );
bool R_SurfNoDraw( const msurface_t *surf );
bool R_SurfNoDlight( const msurface_t *surf );
void R_CacheBrushModelEntity( const entity_t *e );
bool R_AddBrushModelToDrawList( const entity_t *e );
float R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, bool *rotated );
void R_BatchBSPSurf( const entity_t *e, const shader_t *shader, drawSurfaceBSP_t *drawSurf, bool mergable );
void R_FlushBSPSurfBatch( void );
void R_WalkBSPSurf( const entity_t *e, const shader_t *shader, drawSurfaceBSP_t *drawSurf, walkDrawSurf_cb_cb cb, void *ptr );

//
// r_skin.c
//
void R_InitSkinFiles( void );
void R_ShutdownSkinFiles( void );
void R_FreeUnusedSkinFiles( void );
struct skinfile_s *R_SkinFile_Load( const char *name );
struct skinfile_s *R_RegisterSkinFile( const char *name );
shader_t    *R_FindShaderForSkinFile( const struct skinfile_s *skinfile, const char *meshname );

//
// r_skm.c
//
void R_CacheSkeletalModelEntity( const entity_t *e );
bool R_AddSkeletalModelToDrawList( const entity_t *e, int lod );
void R_DrawSkeletalSurf( const entity_t *e, const shader_t *shader, drawSurfaceSkeletal_t *drawSurf );
void R_SkeletalModelFrameBounds( const model_t *mod, int frame, vec3_t mins, vec3_t maxs );
int R_SkeletalGetBoneInfo( const model_t *mod, int bonenum, char *name, size_t name_size, int *flags );
void R_SkeletalGetBonePose( const model_t *mod, int bonenum, int frame, bonepose_t *bonepose );
int R_SkeletalGetNumBones( const model_t *mod, int *numFrames );
bool R_SkeletalModelLerpTag( orientation_t *orient, const mskmodel_t *skmodel, int oldframenum, int framenum, float lerpfrac, const char *name );
void R_ClearSkeletalCache( void );

//
// r_vbo.c
//

typedef enum {
	VBO_TAG_NONE,
	VBO_TAG_WORLD,
	VBO_TAG_MODEL,
	VBO_TAG_STREAM
} vbo_tag_t;

typedef struct mesh_vbo_s {
	unsigned int index;
	int registrationSequence;
	vbo_tag_t tag;

	unsigned int vertexId;
	unsigned int elemId;
	unsigned int visframe;

	unsigned int numVerts;
	unsigned int numElems;

	size_t vertexSize;
	size_t arrayBufferSize;
	size_t elemBufferSize;

	vattribmask_t vertexAttribs;
	vattribmask_t halfFloatAttribs;

	size_t normalsOffset;
	size_t sVectorsOffset;
	size_t stOffset;
	size_t siOffset;
	size_t colorsOffset;
	size_t bonesIndicesOffset;
	size_t bonesWeightsOffset;
	size_t spritePointsOffset;              // autosprite or autosprite2 centre + radius
	size_t instancesOffset;

	void *owner;							// opaque pointer
	struct mesh_vbo_s *vertsVbo;			// pointer to linked vertex data VBO, only relevant for elems-only VBOs
} mesh_vbo_t;

void R_InitVBO( void );
mesh_vbo_t *R_CreateMeshVBO( void *owner, int numVerts, int numElems, int numInstances,
							 vattribmask_t vattribs, vbo_tag_t tag, vattribmask_t halfFloatVattribs );
mesh_vbo_t *R_CreateElemsVBO( void *owner, mesh_vbo_t *vertsVbo, int numElems, vbo_tag_t tag );
void R_ReleaseMeshVBO( mesh_vbo_t *vbo );
void R_TouchMeshVBO( mesh_vbo_t *vbo );
mesh_vbo_t *R_GetVBOByIndex( int index );
int R_GetNumberOfActiveVBOs( void );
vattribmask_t R_FillVBOVertexDataBuffer( mesh_vbo_t *vbo, vattribmask_t vattribs, const mesh_t *mesh, void *outData, int surfIndex );
void R_UploadVBOVertexRawData( mesh_vbo_t *vbo, int vertsOffset, int numVerts, const void *data );
vattribmask_t R_UploadVBOVertexData( mesh_vbo_t *vbo, int vertsOffset, vattribmask_t vattribs, const mesh_t *mesh, int surfIndex );
void R_UploadVBOElemData( mesh_vbo_t *vbo, int vertsOffset, int elemsOffset, const mesh_t *mesh );
vattribmask_t R_UploadVBOInstancesData( mesh_vbo_t *vbo, int instOffset, int numInstances, instancePoint_t *instances );
void R_FreeVBOsByTag( vbo_tag_t tag );
void R_FreeUnusedVBOs( void );
void R_ShutdownVBO( void );

//
// r_sky.c
//

void R_InitSky();
void R_DrawSky( const refdef_t * rd );

//====================================================================

typedef struct {
	float ambient[3];
	byte_vec4_t outlineColor;
	byte_vec4_t environmentColor;
} mapconfig_t;

extern mapconfig_t mapConfig;
extern refinst_t rn;
