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
#ifndef R_GLIMP_H
#define R_GLIMP_H

#ifdef __cplusplus
#define QGL_EXTERN extern "C"
#else
#define QGL_EXTERN extern
#endif

#ifdef _WIN32
#include <windows.h>

#define QGL_WGL( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_WGL_EXT( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )
#define QGL_EGL( type, name, params )
#define QGL_EGL_EXT( type, name, params )

#elif defined ( __ANDROID__ )
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )
#define QGL_EGL( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_EGL_EXT( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;

#elif defined ( __MACOSX__ )
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params )
#define QGL_GLX_EXT( type, name, params )
#define QGL_EGL( type, name, params )
#define QGL_EGL_EXT( type, name, params )

#else
#define QGL_WGL( type, name, params )
#define QGL_WGL_EXT( type, name, params )
#define QGL_GLX( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_GLX_EXT( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_EGL( type, name, params )
#define QGL_EGL_EXT( type, name, params )
#endif

#define QGL_FUNC( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_FUNC_OPT( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;
#define QGL_EXT( type, name, params ) QGL_EXTERN type( APIENTRY * q ## name ) params;

#include "qgl.h"

#undef QGL_EGL_EXT
#undef QGL_EGL
#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_WGL_EXT
#undef QGL_WGL
#undef QGL_EXT
#undef QGL_FUNC_OPT
#undef QGL_FUNC

//====================================================================

#define MAX_TEXTURE_UNITS               8

#define MAX_GLSL_UNIFORM_BONES          100
#define MAX_GLSL_UNIFORM_INSTANCES      40

#define GAMMARAMP_STRIDE                4096

extern cvar_t *r_stencilbits;
extern cvar_t *gl_drawbuffer;
extern cvar_t *gl_driver;

//====================================================================

enum {
	GLSTATE_NONE = 0,

	//
	// glBlendFunc args
	//
	GLSTATE_SRCBLEND_ZERO                   = 1,
	GLSTATE_SRCBLEND_ONE                    = 2,
	GLSTATE_SRCBLEND_DST_COLOR              = 1 | 2,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR    = 4,
	GLSTATE_SRCBLEND_SRC_ALPHA              = 1 | 4,
	GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA    = 2 | 4,
	GLSTATE_SRCBLEND_DST_ALPHA              = 1 | 2 | 4,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA    = 8,

	GLSTATE_DSTBLEND_ZERO                   = 16,
	GLSTATE_DSTBLEND_ONE                    = 32,
	GLSTATE_DSTBLEND_SRC_COLOR              = 16 | 32,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR    = 64,
	GLSTATE_DSTBLEND_SRC_ALPHA              = 16 | 64,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA    = 32 | 64,
	GLSTATE_DSTBLEND_DST_ALPHA              = 16 | 32 | 64,
	GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA    = 128,

	GLSTATE_NO_COLORWRITE                   = 0x100,
	GLSTATE_ALPHAWRITE                      = 0x200,

	GLSTATE_DEPTHWRITE                      = 0x400,
	GLSTATE_DEPTHFUNC_EQ                    = 0x800,
	GLSTATE_DEPTHFUNC_GT                    = 0x1000,

	GLSTATE_OFFSET_FILL                     = 0x2000,
	GLSTATE_NO_DEPTH_TEST                   = 0x4000,

	GLSTATE_STENCIL_TEST                    = 0x8000,

	GLSTATE_ALPHATEST                       = 0x10000,

	GLSTATE_MARK_END                        = 0x20000 // SHADERPASS_MARK_BEGIN
};

#define GLSTATE_MASK        ( GLSTATE_MARK_END - 1 )

// #define SHADERPASS_SRCBLEND_MASK (((GLSTATE_SRCBLEND_DST_ALPHA)<<1)-GLSTATE_SRCBLEND_ZERO)
#define GLSTATE_SRCBLEND_MASK   0xF
// #define SHADERPASS_DSTBLEND_MASK (((GLSTATE_DSTBLEND_DST_ALPHA)<<1)-GLSTATE_DSTBLEND_ZERO)
#define GLSTATE_DSTBLEND_MASK   0xF0
#define GLSTATE_BLEND_MASK      ( GLSTATE_SRCBLEND_MASK | GLSTATE_DSTBLEND_MASK )

#define GLSTATE_BLEND_ADD       ( GLSTATE_SRCBLEND_ONE | GLSTATE_DSTBLEND_ONE )

//====================================================================

typedef struct {
	int _extMarker;

	//
	// only uint8_ts must follow the extensionsBoolMarker
	//

	char draw_range_elements
	,multitexture
	,texture_cube_map
	,texture_edge_clamp
	,texture_filter_anisotropic
	,texture_compression
	,compressed_ETC1_RGB8_texture
	,vertex_buffer_object
	,GLSL
	,GLSL_core
	,GLSL130
	,depth_texture
	,framebuffer_object
	,vertex_shader
	,fragment_shader
	,shader_objects
	,shading_language_100
	,shading_language_130
	,bgra
	,gamma_control
	,swap_control
	,draw_instanced
	,instanced_arrays
	,gpu_memory_info
	,meminfo
	,framebuffer_blit
	,depth24
	,depth_nonlinear
	,get_program_binary
	,rgb8_rgba8
	,ES3_compatibility
	,blend_func_separate
	,texture_array
	,fragment_precision_high
	,packed_depth_stencil
	,texture_lod
	,gpu_shader5
	,texture_float
	,texture_sRGB
	,draw_buffers
	,multisample
	,pixel_format
	,framebuffer_multisample
	;
	union { char shadow, shadow_samplers; };
	union { char texture3D, texture_3D; };
	union { char texture_non_power_of_two, texture_npot; };
	union { char half_float_vertex, vertex_half_float; };
} glextinfo_t;

typedef struct {
	const char      *rendererString;
	const char      *vendorString;
	const char      *versionString;
	const char      *extensionsString;
	const char      *glwExtensionsString;
	const char      *shadingLanguageVersionString;
	unsigned versionHash;

	const char      *applicationName;
	const char      *screenshotPrefix;
	int startupColor;

	int version;
	int shadingLanguageVersion;

	int width, height;
	bool fullScreen;
	bool borderless;

	bool stereoEnabled;
	int stencilBits;

	bool hwGamma;
	unsigned short gammaRampSize;
	unsigned short originalGammaRamp[3 * GAMMARAMP_STRIDE];

	float depthEpsilon;

	int maxTextureSize
	,maxTextureUnits
	,maxTextureCubemapSize
	,maxTexture3DSize
	,maxTextureLayers
	,maxTextureFilterAnisotropic
	,maxRenderbufferSize
	,maxVaryingFloats
	,maxVertexUniformComponents
	,maxVertexAttribs
	,maxFragmentUniformComponents
	,maxFramebufferSamples
	;
	unsigned int maxGLSLBones;      // the maximum amount of bones we can handle in a vertex shader

	bool forceRGBAFramebuffers;             // PowerVR hack - its blending interprets alpha in RGB FBs as 0, not 1
	bool multithreading;
	bool sSRGB;

	glextinfo_t ext;
} glconfig_t;

extern glconfig_t glConfig;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

bool    GLimp_RenderingEnabled( void );
void    GLimp_BeginFrame( void );
void    GLimp_EndFrame( void );
bool    GLimp_Init( const char *applicationName, void *hinstance, void *wndproc, void *parenthWnd,
					int iconResource, const int *iconXPM );
void    GLimp_Shutdown( void );

rserr_t GLimp_SetMode( int x, int y, int width, int height, bool fullscreen, bool stereo, bool borderless );

rserr_t GLimp_SetWindow( void *hinstance, void *wndproc, void *parenthWnd, bool *surfaceChangePending );

/*
* GLimp_SetFullscreen
* Attempts to set the fullscreen mode and updates the window position in windowed mode.
*/
rserr_t GLimp_SetFullscreen( bool fullscreen, int xpos, int ypos );

void    GLimp_AppActivate( bool active, bool minimize, bool destroy );
bool    GLimp_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp );
void    GLimp_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp );
void    GLimp_SetSwapInterval( int swapInterval );

bool    GLimp_MakeCurrent( void *context, void *surface );

void    GLimp_EnableMultithreadedRendering( bool enable );
// When multithreaded rendering is enabled, GetWindowSurface must be called from the rendering thread, not the main one.
// The window surface may be managed by the rendering thread in this case, and the main thread may have a fake surface instead
// if the context implementation doesn't support multiple contexts bound to the same surface.
void    *GLimp_GetWindowSurface( bool *renderable );
void    GLimp_UpdatePendingWindowSurface( void ); // Call from the rendering thread.

bool    GLimp_SharedContext_Create( void **context, void **surface );
void    GLimp_SharedContext_Destroy( void *context, void *surface );

#endif // R_GLIMP_H
