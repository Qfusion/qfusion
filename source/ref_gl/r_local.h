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
#ifndef __R_LOCAL_H__
#define __R_LOCAL_H__

#include "../qcommon/qcommon.h"

typedef unsigned int elem_t;

typedef vec_t instancePoint_t[8]; // quaternion for rotation + xyz pos + uniform scale

#include "r_math.h"
#include "r_glimp.h"
#include "r_public.h"
#include "r_vattribs.h"
#include "r_surface.h"
#include "r_image.h"

#ifdef CGAMEGETLIGHTORIGIN
#define SHADOW_MAPPING			2
#else
#define SHADOW_MAPPING			1
#endif

#define SUBDIVISIONS_MIN		3
#define SUBDIVISIONS_MAX		16
#define SUBDIVISIONS_DEFAULT	5

#define MAX_PORTAL_SURFACES		32
#define MAX_PORTAL_TEXTURES		64

#define BACKFACE_EPSILON		4

#define	ON_EPSILON				0.1         // point on plane side epsilon

#define Z_NEAR					4

#define	SIDE_FRONT				0
#define	SIDE_BACK				1
#define	SIDE_ON					2

#define RP_NONE					0x0
#define RP_MIRRORVIEW			0x1     // lock pvs at vieworg
#define RP_PORTALVIEW			0x2
#define RP_ENVVIEW				0x4
#define RP_SKYPORTALVIEW		0x8
#define RP_OLDVIEWCLUSTER		0x10
#define RP_SHADOWMAPVIEW		0x20
#define RP_FLIPFRONTFACE		0x40
#define RP_DRAWFLAT				0x80
#define RP_CLIPPLANE			0x100
#define RP_PVSCULL				0x200
#define RP_NOVIS				0x400
#define RP_NOENTS				0x800
#define RP_LIGHTMAP				0x1000

#define RP_CUBEMAPVIEW			( RP_ENVVIEW )
#define RP_NONVIEWERREF			( RP_PORTALVIEW|RP_MIRRORVIEW|RP_ENVVIEW|RP_SKYPORTALVIEW|RP_SHADOWMAPVIEW )

//===================================================================

typedef struct
{
	vec3_t			origin;
	vec3_t			color;
	float			intensity;
} dlight_t;

typedef struct superLightStyle_s
{
	vattribmask_t	vattribs;
	int				lightmapNum[MAX_LIGHTMAPS];
	int				lightmapStyles[MAX_LIGHTMAPS];
	int				vertexStyles[MAX_LIGHTMAPS];
	float			stOffset[MAX_LIGHTMAPS][2];
} superLightStyle_t;

#include "r_mesh.h"
#include "r_shader.h"
#include "r_backend.h"
#include "r_shadow.h"
#include "r_model.h"
#include "r_trace.h"
#include "r_program.h"

typedef struct portalSurface_s
{
	const entity_t	*entity;
	cplane_t		plane, untransformed_plane;
	const shader_t	*shader;
	vec3_t			mins, maxs, centre;
	image_t			*texures[2];			// front and back portalmaps
} portalSurface_t;

typedef struct
{
	unsigned int	params;					// rendering parameters

	image_t			*fbColorAttachment;
	image_t			*fbDepthAttachment;

	refdef_t		refdef;
	int				scissor[4];
	int				viewport[4];
	drawList_t		*meshlist;				// meshes to be rendered

	unsigned int	dlightBits;

	unsigned int	shadowBits;
	shadowGroup_t	*shadowGroup;

	entity_t		*currententity;

	//
	// view origin
	//
	vec3_t			viewOrigin;
	mat3_t			viewAxis;
	cplane_t		frustum[6];
	float			farClip;
	unsigned int	clipFlags;
	vec3_t			visMins, visMaxs;
	unsigned int	numVisSurfaces;

	mat4_t			objectMatrix;
	mat4_t			cameraMatrix;

	mat4_t			modelviewMatrix;
	mat4_t			projectionMatrix;

	mat4_t			cameraProjectionMatrix;			// cameraMatrix * projectionMatrix
	mat4_t			modelviewProjectionMatrix;		// modelviewMatrix * projectionMatrix

	shader_t		*skyShader;
	mfog_t			*skyFog;
	float			skyMins[2][6];
	float			skyMaxs[2][6];

	float			lod_dist_scale_for_fov;

	mfog_t			*fog_eye;
	float			fog_dist_to_eye[MAX_MAP_FOGS];

	unsigned int	numPortalSurfaces;
	portalSurface_t	portalSurfaces[MAX_PORTAL_SURFACES];

	vec3_t			lodOrigin;
	vec3_t			pvsOrigin;
	cplane_t		clipPlane;
} refinst_t;

//====================================================

typedef struct
{
	unsigned int numEntities;
	entity_t entities[MAX_ENTITIES];
	entity_t *worldent;

	unsigned int numDlights;
	dlight_t dlights[MAX_DLIGHTS];

	unsigned int numPolys;
	drawSurfacePoly_t polys[MAX_POLYS];

	lightstyle_t lightStyles[MAX_LIGHTSTYLES];

	unsigned int numBmodelEntities;
	entity_t *bmodelEntities[MAX_ENTITIES];

	unsigned int numShadowGroups;
	shadowGroup_t shadowGroups[MAX_SHADOWGROUPS];
	unsigned int entShadowGroups[MAX_ENTITIES];
	unsigned int entShadowBits[MAX_ENTITIES];

	float	farClipMin, farClipBias;

	refdef_t refdef;
} r_scene_t;

typedef struct
{
	// any asset (model, shader, texture, etc) with has not been registered
	// or "touched" during the last registration sequence will be freed
	int				registrationSequence;
	int				lastRegistrationSequence;

	 // bumped each R_ClearScene
	unsigned int	sceneFrameCount;
	unsigned int	sceneShadowBits;

	// FIXME: move most of the global variables below here
	vec3_t			wallColor, floorColor;

	shader_t		*envShader;
	shader_t		*skyShader;
	shader_t		*whiteShader;
	shader_t		*builtinRawShader;
	shader_t		*skyclipShader;

	struct mesh_vbo_s *nullVBO;

	qboolean		in2D;

	float			cameraSeparation;

	float			sinTableByte[256];
} r_frontend_t;

extern r_scene_t rsc;
extern r_frontend_t rf;

extern image_t *r_rawtexture;
extern image_t *r_notexture;
extern image_t *r_whitetexture;
extern image_t *r_blacktexture;
extern image_t *r_greytexture;
extern image_t *r_blankbumptexture;
extern image_t *r_particletexture;
extern image_t *r_coronatexture;
extern image_t *r_portaltextures[];
extern image_t *r_portaldepthtextures[];
extern image_t *r_shadowmapTextures[];
extern image_t *r_screentexture;
extern image_t *r_screendepthtexture;
extern image_t *r_screentexturecopy;
extern image_t *r_screendepthtexturecopy;
extern image_t *r_screenfxaacopy;
extern image_t *r_screenweapontexture;

extern unsigned int r_pvsframecount;
extern unsigned int r_framecount;
extern unsigned int c_brush_polys, c_world_leafs;

extern unsigned int r_mark_leaves, r_world_node;
extern unsigned int r_add_polys, r_add_entities;
extern unsigned int r_draw_meshes;

extern msurface_t *r_debug_surface;

extern int gl_filter_min, gl_filter_max;

extern float gldepthmin, gldepthmax;

#define R_ENT2NUM(ent) ((ent)-rsc.entities)
#define R_NUM2ENT(num) (rsc.entities+(num))

extern int r_viewcluster, r_oldviewcluster, r_viewarea;

extern model_t *r_worldmodel;
extern mbrushmodel_t *r_worldbrushmodel;

extern cvar_t *r_norefresh;
extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_drawelements;
extern cvar_t *r_fullbright;
extern cvar_t *r_lightmap;
extern cvar_t *r_novis;
extern cvar_t *r_nocull;
extern cvar_t *r_lerpmodels;
extern cvar_t *r_mapoverbrightbits;

extern cvar_t *r_dynamiclight;
extern cvar_t *r_coronascale;
extern cvar_t *r_detailtextures;
extern cvar_t *r_subdivisions;
	extern cvar_t *r_showtris;
extern cvar_t *r_shownormals;
extern cvar_t *r_draworder;

extern cvar_t *r_fastsky;
extern cvar_t *r_portalonly;
extern cvar_t *r_portalmaps;
extern cvar_t *r_portalmaps_maxtexsize;

extern cvar_t *r_lighting_bumpscale;
extern cvar_t *r_lighting_deluxemapping;
extern cvar_t *r_lighting_diffuse2heightmap;
extern cvar_t *r_lighting_specular;
extern cvar_t *r_lighting_glossintensity;
extern cvar_t *r_lighting_glossexponent;
extern cvar_t *r_lighting_ambientscale;
extern cvar_t *r_lighting_directedscale;
extern cvar_t *r_lighting_packlightmaps;
extern cvar_t *r_lighting_maxlmblocksize;
extern cvar_t *r_lighting_vertexlight;
extern cvar_t *r_lighting_maxglsldlights;
extern cvar_t *r_lighting_grayscale;

extern cvar_t *r_offsetmapping;
extern cvar_t *r_offsetmapping_scale;
extern cvar_t *r_offsetmapping_reliefmapping;

extern cvar_t *r_shadows;
extern cvar_t *r_shadows_alpha;
extern cvar_t *r_shadows_nudge;
extern cvar_t *r_shadows_projection_distance;
extern cvar_t *r_shadows_maxtexsize;
extern cvar_t *r_shadows_pcf;
extern cvar_t *r_shadows_self_shadow;
extern cvar_t *r_shadows_dither;

extern cvar_t *r_outlines_world;
extern cvar_t *r_outlines_scale;
extern cvar_t *r_outlines_cutoff;

extern cvar_t *r_soft_particles;
extern cvar_t *r_soft_particles_scale;

extern cvar_t *r_fxaa;

extern cvar_t *r_lodbias;
extern cvar_t *r_lodscale;

extern cvar_t *r_gamma;
extern cvar_t *r_texturebits;
extern cvar_t *r_texturemode;
extern cvar_t *r_texturefilter;
extern cvar_t *r_mode;
extern cvar_t *r_nobind;
extern cvar_t *r_picmip;
extern cvar_t *r_skymip;
extern cvar_t *r_clear;
extern cvar_t *r_polyblend;
extern cvar_t *r_lockpvs;
extern cvar_t *r_screenshot_fmtstr;
extern cvar_t *r_screenshot_jpeg;
extern cvar_t *r_screenshot_jpeg_quality;
extern cvar_t *r_swapinterval;

extern cvar_t *r_temp1;

extern cvar_t *r_drawflat;
extern cvar_t *r_wallcolor;
extern cvar_t *r_floorcolor;

extern cvar_t *r_maxglslbones;

extern cvar_t *gl_finish;
extern cvar_t *gl_cull;

extern cvar_t *vid_displayfrequency;
extern cvar_t *vid_multiscreen_head;

//====================================================================

void R_LatLongToNorm( const qbyte latlong[2], vec3_t out );

//====================================================================

//
// r_alias.c
//
qboolean	R_AddAliasModelToDrawList( const entity_t *e );
qboolean	R_DrawAliasSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceAlias_t *drawSurf );
qboolean	R_AliasModelLerpTag( orientation_t *orient, const maliasmodel_t *aliasmodel, int framenum, int oldframenum,
				float lerpfrac, const char *name );
float		R_AliasModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs );
void		R_AliasModelFrameBounds( const model_t *mod, int frame, vec3_t mins, vec3_t maxs );

//
// r_cin.c
//
void		R_InitCinematics( void );
void		R_ShutdownCinematics( void );
unsigned int R_StartCinematic( const char *arg );
void		R_FreeCinematic( unsigned int id );
void		R_RunAllCinematics( void );
void		R_TouchCinematic( unsigned int id );
void		R_FreeUnusedCinematics( void );
image_t		*R_UploadCinematic( unsigned int id );

//
// r_cull.c
//
void		R_SetupFrustum( const refdef_t *rd, float farClip, cplane_t *frustum );
qboolean	R_CullBox( const vec3_t mins, const vec3_t maxs, const unsigned int clipflags );
qboolean	R_CullSphere( const vec3_t centre, const float radius, const unsigned int clipflags );
qboolean	R_VisCullBox( const vec3_t mins, const vec3_t maxs );
qboolean	R_VisCullSphere( const vec3_t origin, float radius );
int			R_CullModelEntity( const entity_t *e, vec3_t mins, vec3_t maxs, float radius, qboolean sphereCull );
qboolean	R_CullSpriteEntity( const entity_t *e );

//
// r_framebuffer.c
//
enum
{
	FBO_COPY_NORMAL = 0,
	FBO_COPY_CENTREPOS = 1,
	FBO_COPY_INVERT_Y = 2
};

void		R_InitFBObjects( void );
int			R_RegisterFBObject( int width, int height );
void		R_UnregisterFBObject( int object );
void		R_TouchFBObject( int object );
void		R_UseFBObject( int object );
int			R_ActiveFBObject( void );
void		R_AttachTextureToFBObject( int object, image_t *texture );
void		R_DetachTextureFromFBObject( qboolean depth );
image_t		*R_GetFBObjectTextureAttachment( int object, qboolean depth );
void		R_DisableFBObjectDrawBuffer( void );
void		R_CopyFBObject( int dest, int bitMask, int mode );
qboolean	R_CheckFBObjectStatus( void );
void		R_FreeUnusedFBObjects( void );
void		R_ShutdownFBObjects( void );

//
// r_light.c
//
#define DLIGHT_SCALE	    0.5f
#define MAX_SUPER_STYLES    128

unsigned int R_AddSurfaceDlighbits( const msurface_t *surf, unsigned int checkDlightBits );
void		R_AddDynamicLights( unsigned int dlightbits, int state );
void		R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
void		R_BuildLightmaps( model_t *mod, int numLightmaps, int w, int h, const qbyte *data, mlightmapRect_t *rects );
void		R_InitLightStyles( model_t *mod );
superLightStyle_t	*R_AddSuperLightStyle( model_t *mod, const int *lightmaps, 
	const qbyte *lightmapStyles, const qbyte *vertexStyles, mlightmapRect_t **lmRects );
void		R_SortSuperLightStyles( model_t *mod );
void		R_TouchLightmapImages( model_t *mod );

void		R_InitCoronas( void );
qboolean	R_BeginCoronaSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceType_t *drawSurf );
void		R_BatchCoronaSurf(  const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceType_t *drawSurf );
void		R_DrawCoronas( void );
void		R_ShutdownCoronas( void );

//
// r_main.c
//
#define R_FASTSKY() (r_fastsky->integer || r_viewcluster == -1)

extern mempool_t *r_mempool;

#define	R_Malloc( size ) Mem_Alloc( r_mempool, size )
#define	R_Realloc( data, size ) Mem_Realloc( data, size )
#define	R_Free( data ) Mem_Free( data )

void		R_BeginFrame( float cameraSeparation, qboolean forceClear );
void		R_EndFrame( void );
void		R_Set2DMode( qboolean enable );
void		R_RenderView( const refdef_t *fd );
void		R_ClearStats( void );
const char *R_SpeedsMessage( char *out, size_t size );

mfog_t		*R_FogForBounds( const vec3_t mins, const vec3_t maxs );
mfog_t		*R_FogForSphere( const vec3_t centre, const float radius );
qboolean	R_CompletelyFogged( const mfog_t *fog, vec3_t origin, float radius );
int			R_LODForSphere( const vec3_t origin, float radius );
float		R_DefaultFarClip( void );

qboolean	R_BeginSpriteSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceType_t *drawSurf );
void		R_BatchSpriteSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceType_t *drawSurf );

struct mesh_vbo_s *R_InitNullModelVBO( void );
qboolean	R_DrawNullSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceType_t *drawSurf );

void		R_TransformForWorld( void );
void		R_TransformForEntity( const entity_t *e );
void		R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out );
void		R_TransformBounds( const vec3_t origin, const mat3_t axis, vec3_t mins, vec3_t maxs, vec3_t bbox[8] );
qboolean	R_ScissorForBounds( vec3_t bbox[8], int *x, int *y, int *w, int *h );
qboolean	R_ScissorForEntity( const entity_t *ent, vec3_t mins, vec3_t maxs, int *x, int *y, int *w, int *h );

void		R_AddDebugBounds( const vec3_t mins, const vec3_t maxs );

void		R_BeginStretchBatch( const shader_t *shader, float x_offset, float y_offset );
void		R_EndStretchBatch( void );
void		R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
	const vec4_t color, const shader_t *shader );
void		R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, qbyte *data );
void		R_DrawStretchQuick( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
	const vec4_t color, int program_type, image_t *image, qboolean blend );

#define NUM_CUSTOMCOLORS	16
void		R_InitCustomColors( void );
void		R_SetCustomColor( int num, int r, int g, int b );
int			R_GetCustomColor( int num );
void		R_ShutdownCustomColors( void );

#define ENTITY_OUTLINE(ent) (( !(ri.params & RP_MIRRORVIEW) && ((ent)->renderfx & RF_VIEWERMODEL) ) ? 0 : (ent)->outlineHeight)

void		R_ClearRefInstStack( void );
qboolean	R_PushRefInst( void );
void		R_PopRefInst( int clearBitMask );

//
// r_mesh.c
//
void R_InitDrawList( drawList_t *list );
void R_InitDrawLists( void );
void R_ClearDrawList( void );
qboolean R_AddDSurfToDrawList( const entity_t *e, const mfog_t *fog, const shader_t *shader, 
	float dist, unsigned int order, const portalSurface_t *portalSurf, void *drawSurf );
void R_AddVBOSlice( unsigned int index, unsigned int numVerts, unsigned int numElems, 
	unsigned int firstVert, unsigned int firstElem );
vboSlice_t *R_GetVBOSlice( unsigned int index );

void R_SortDrawList( void );
void R_DrawSurfaces( void );
void R_DrawOutlinedSurfaces( void );

void R_CopyOffsetElements( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_CopyOffsetTriangles( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems );
void R_BuildQuadElements( int vertsOffset, int numVerts, elem_t *elems );
void R_BuildTrifanElements( int vertsOffset, int numVerts, elem_t *elems );
void R_BuildTangentVectors( int numVertexes, vec3_t *xyzArray, vec3_t *normalsArray, vec2_t *stArray,
	int numTris, elem_t *elems, vec4_t *sVectorsArray );

//
// r_portals.c
//
extern drawList_t r_portallist, r_skyportallist;

portalSurface_t *R_AddPortalSurface( const entity_t *ent, const mesh_t *mesh, 
	const vec3_t mins, const vec3_t maxs, const shader_t *shader );

void R_DrawPortals( void );
void R_DrawSkyPortal( const entity_t *e, skyportal_t *skyportal, vec3_t mins, vec3_t maxs );

//
// r_poly.c
//
qboolean	R_BeginPolySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfacePoly_t *drawSurf );
void		R_BatchPolySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfacePoly_t *poly );
void		R_DrawPolys( void );
void		R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset );
qboolean	R_SurfPotentiallyFragmented( msurface_t *surf );
int			R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts,
								  vec3_t *fverts, int maxfragments, fragment_t *fragments );

//
// r_register.c
//
rserr_t		R_Init( void *hinstance, void *wndproc, void *parenthWnd, int x, int y, int width, int height, qboolean fullscreen, qboolean wideScreen, qboolean verbose );
void		R_BeginRegistration( void );
void		R_EndRegistration( void );
void		R_Shutdown( qboolean verbose );


//
// r_scene.c
//
extern drawList_t r_worldlist;

void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs );
void R_ClearScene( void );
void R_AddEntityToScene( const entity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void R_AddPolyToScene( const poly_t *poly );
void R_AddLightStyleToScene( int style, float r, float g, float b );
void R_RenderScene( const refdef_t *fd );

//
// r_surf.c
//
#define MAX_SURF_QUERIES		0x1E0

void		R_MarkLeaves( void );
void		R_DrawWorld( void );
qboolean	R_SurfPotentiallyVisible( const msurface_t *surf );
qboolean	R_SurfPotentiallyShadowed( const msurface_t *surf );
qboolean	R_SurfPotentiallyLit( const msurface_t *surf );
qboolean	R_AddBrushModelToDrawList( const entity_t *e );
float		R_BrushModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs, qboolean *rotated );
qboolean	R_DrawBSPSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceBSP_t *drawSurf );

//
// r_skin.c
//
void		R_InitSkinFiles( void );
void		R_ShutdownSkinFiles( void );
void		R_FreeUnusedSkinFiles( void );
struct skinfile_s *R_SkinFile_Load( const char *name );
struct skinfile_s *R_RegisterSkinFile( const char *name );
shader_t	*R_FindShaderForSkinFile( const struct skinfile_s *skinfile, const char *meshname );

//
// r_skm.c
//
qboolean	R_AddSkeletalModelToDrawList( const entity_t *e );
qboolean	R_DrawSkeletalSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceSkeletal_t *drawSurf );
float		R_SkeletalModelBBox( const entity_t *e, vec3_t mins, vec3_t maxs );
void		R_SkeletalModelFrameBounds( const model_t *mod, int frame, vec3_t mins, vec3_t maxs );
int			R_SkeletalGetBoneInfo( const model_t *mod, int bonenum, char *name, size_t name_size, int *flags );
void		R_SkeletalGetBonePose( const model_t *mod, int bonenum, int frame, bonepose_t *bonepose );

void		R_InitSkeletalCache( void );
void		R_ClearSkeletalCache( void );
void		R_ShutdownSkeletalCache( void );

//
// r_vbo.c
//

typedef enum
{
	VBO_TAG_NONE,
	VBO_TAG_WORLD,
	VBO_TAG_MODEL,
	VBO_TAG_STREAM,
	VBO_TAG_STREAM_STATIC_ELEMS
} vbo_tag_t;

typedef enum
{
	VBO_HINT_NONE			= 0,
	VBO_HINT_ELEMS_QUAD		= 1<<0,
	VBO_HINT_ELEMS_TRIFAN	= 1<<1
} vbo_hint_t;

typedef struct mesh_vbo_s
{
	unsigned int		index;
	int					registrationSequence;
	vbo_tag_t			tag;

	unsigned int 		vertexId;
	unsigned int		elemId;
	void 				*owner;
	unsigned int 		visframe;

	unsigned int 		numVerts;
	unsigned int 		numElems;

	size_t				arrayBufferSize;
	size_t				elemBufferSize;

	size_t 				normalsOffset;
	size_t 				sVectorsOffset;
	size_t 				stOffset;
	size_t 				lmstOffset[MAX_LIGHTMAPS];
	size_t 				colorsOffset[MAX_LIGHTMAPS];
	size_t				bonesIndicesOffset;
	size_t				bonesWeightsOffset;
	size_t				spritePointsOffset; // autosprite or autosprite2 centre + radius
	size_t				spriteRightAxesOffset;
	size_t				spriteUpAxesOffset;
	size_t				instancesOffset;
} mesh_vbo_t;

void 		R_InitVBO( void );
mesh_vbo_t *R_CreateMeshVBO( void *owner, int numVerts, int numElems, int numInstances,
	vattribmask_t vattribs, vbo_tag_t tag );
void		R_ReleaseMeshVBO( mesh_vbo_t *vbo );
void		R_TouchMeshVBO( mesh_vbo_t *vbo );
mesh_vbo_t *R_GetVBOByIndex( int index );
int			R_GetNumberOfActiveVBOs( void );
void		R_DiscardVBOVertexData( mesh_vbo_t *vbo );
void		R_DiscardVBOElemData( mesh_vbo_t *vbo );
vattribmask_t R_UploadVBOVertexData( mesh_vbo_t *vbo, int vertsOffset, vattribmask_t vattribs, 
	const mesh_t *mesh, vbo_hint_t hint );
void 		R_UploadVBOElemData( mesh_vbo_t *vbo, int vertsOffset, int elemsOffset, 
	const mesh_t *mesh, vbo_hint_t hint );
vattribmask_t R_UploadVBOBonesData( mesh_vbo_t *vbo, int vertsOffset, int numVerts, 
	qbyte *bonesIndices, qbyte *bonesWeights );
vattribmask_t R_UploadVBOInstancesData( mesh_vbo_t *vbo, int instOffset,
	int numInstances, instancePoint_t *instances );
void		R_FreeVBOsByTag( vbo_tag_t tag );
void		R_FreeUnusedVBOs( void );
void 		R_ShutdownVBO( void );

//
// r_sky.c
//

enum
{
	SKYBOX_RIGHT,
	SKYBOX_LEFT,
	SKYBOX_FRONT,
	SKYBOX_BACK,
	SKYBOX_TOP,
	SKYBOX_BOTTOM		// not used for skydome, but is used for skybox
};

struct skydome_s *R_CreateSkydome( model_t *model );
void		R_TouchSkydome( struct skydome_s *skydome );
qboolean	R_DrawSkySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfaceBSP_t *drawSurf );
void		R_ClearSky( void );
void		R_AddSkyToDrawList( const msurface_t *fa );

//====================================================================

typedef struct
{
	int				overbrightBits;			// map specific overbright bits
	int				pow2MapOvrbr;
	float			mapLightColorScale;		// 1<<overbrightbits * intensity

	float			ambient[3];
	byte_vec4_t		outlineColor;
	byte_vec4_t		environmentColor;

	float			lightingIntensity;

	qboolean		lightmapsPacking;
	qboolean		deluxeMaps;				// true if there are valid deluxemaps in the .bsp
	qboolean		deluxeMappingEnabled;	// true if deluxeMaps is true and r_lighting_deluxemaps->integer != 0

	qboolean		depthWritingSky;		// draw invisible sky surfaces with writing to depth buffer enabled
	qboolean		checkWaterCrossing;		// check above and below so crossing solid water doesn't draw wrong

	qboolean		forceClear;
} mapconfig_t;

extern mapconfig_t	mapConfig;
extern refinst_t	ri;

#endif /*__R_LOCAL_H__*/
