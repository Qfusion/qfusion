/*
Copyright (C) 2016 Victor Luchits

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

#ifndef R_FRONTEND_H
#define R_FRONTEND_H

#include "r_local.h"
#include "r_cmdque.h"

// sync-to-async frontend adapter
typedef struct {
	void            *owner;             // pointer to parent ref_frontend_t
	void            *GLcontext;
	unsigned        frameNum;
	qthread_t       *thread;
	qmutex_t        *frameLock;
	ref_cmdpipe_t   *cmdPipe;
	volatile bool   shutdown;
	volatile bool   noWait;
} ref_frontendAdapter_t;

typedef struct {
	unsigned frameNum;                  // wrapped
	unsigned lastFrameNum;

	ref_cmdbuf_t    *frames[3];         // triple-buffered
	ref_cmdbuf_t    *frame;             // current frontend frame

	void            *auxGLContext;

	ref_frontendAdapter_t adapter;

	// these fields serve as the frontend cache which can also queried by the public API
	int scissor[4];
	float cameraSeparation;
	byte_vec4_t customColors[NUM_CUSTOMCOLORS];
} ref_frontend_t;

// public API
rserr_t RF_Init( const char *applicationName, const char *screenshotPrefix, int startupColor,
				 int iconResource, const int *iconXPM, void *hinstance, void *wndproc, void *parenthWnd,  bool verbose );
rserr_t RF_SetMode( int x, int y, int width, int height, bool fullScreen, bool stereo, bool borderless );
void RF_AppActivate( bool active, bool minimize, bool destroy );
rserr_t RF_SetWindow( void *hinstance, void *wndproc, void *parenthWnd );
void RF_Shutdown( bool verbose );
void RF_BeginFrame( float cameraSeparation, bool forceClear, bool forceVsync, bool uncappedFPS );
void RF_EndFrame( void );
void RF_BeginRegistration( void );
void RF_EndRegistration( void );
void RF_RegisterWorldModel( const char *model );
void RF_ClearScene( void );
void RF_AddEntityToScene( const entity_t *ent );
void RF_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RF_AddPolyToScene( const poly_t *poly );
void RF_AddLightStyleToScene( int style, float r, float g, float b );
void RF_RenderScene( const refdef_t *fd );
void RF_BlurScreen( void );
void RF_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
						const vec4_t color, const shader_t *shader );
void RF_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle,
							   const vec4_t color, const shader_t *shader );
void RF_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows,
						float s1, float t1, float s2, float t2, uint8_t *data );
void RF_DrawStretchRawYUV( int x, int y, int w, int h,
						   float s1, float t1, float s2, float t2, ref_img_plane_t *yuv );
void RF_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset );
void RF_SetScissor( int x, int y, int w, int h );
void RF_GetScissor( int *x, int *y, int *w, int *h );
void RF_ResetScissor( void );
void RF_SetCustomColor( int num, int r, int g, int b );
void RF_ScreenShot( const char *path, const char *name, const char *fmtstring, bool silent );
void RF_EnvShot( const char *path, const char *name, unsigned pixels );
bool RF_RenderingEnabled( void );
const char *RF_GetSpeedsMessage( char *out, size_t size );
int RF_GetAverageFrametime( void );
void RF_ReplaceRawSubPic( shader_t *shader, int x, int y, int width, int height, uint8_t *data );
void RF_BeginAviDemo( void );
void RF_WriteAviFrame( int frame, bool scissor );
void RF_StopAviDemo( void );
void RF_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out );
bool RF_LerpTag( orientation_t *orient, const model_t *mod, int oldframe, int frame, float lerpfrac, const char *name );
void RF_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
shader_t *RF_GetShaderForOrigin( const vec3_t origin );
struct cinematics_s *RF_GetShaderCinematic( shader_t *shader );
void RF_Finish( void );

/**
* Called by UI when it wants to set the current transform matrix to a new matrix.
* Passing a NULL pointer will set the transform matrix to identity.
*/
void RF_SetTransformMatrix( const float *m );

#endif // R_FRONTEND_H
