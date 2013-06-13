/*
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
#ifndef __R_PUBLIC_H__
#define __R_PUBLIC_H__

#include "../cgame/ref.h"

rserr_t		R_Init( void *hinstance, void *wndproc, void *parenthWnd, int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen, qboolean verbose );
rserr_t		R_SetMode( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen );
void		R_BeginRegistration( void );
void		R_EndRegistration( void );
void		R_Shutdown( qboolean verbose );

void		R_RegisterWorldModel( const char *model, const dvis_t *pvsData );
void		R_ModelBounds( const struct model_s *model, vec3_t mins, vec3_t maxs );
void		R_ModelFrameBounds( const struct model_s *model, int frame, vec3_t mins, vec3_t maxs );

struct model_s *R_RegisterModel( const char *name );
struct shader_s *R_RegisterPic( const char *name );
struct shader_s *R_RegisterRawPic( const char *name, int width, int height, qbyte *data );
struct shader_s *R_RegisterLevelshot( const char *name, struct shader_s *defaultShader, qboolean *matchesDefault );
struct shader_s *R_RegisterSkin( const char *name );
struct skinfile_s *R_RegisterSkinFile( const char *name );
struct shader_s *R_RegisterVideo( const char *name );

void		R_RemapShader( const char *from, const char *to, int timeOffset );
void		R_GetShaderDimensions( const struct shader_s *shader, int *width, int *height );

void		R_ClearScene( void );
void		R_AddEntityToScene( const entity_t *ent );
void		R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void		R_AddPolyToScene( const poly_t *poly );
void		R_AddLightStyleToScene( int style, float r, float g, float b );
void		R_RenderScene( const refdef_t *fd );

void		R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
							 const float *color, const struct shader_s *shader );
void		R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
							 float angle, const vec4_t color, const struct shader_s *shader );
void		R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, qbyte *data );
void		R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset );
void		R_SetScissorRegion( int x, int y, int w, int h );
void		R_GetScissorRegion( int *x, int *y, int *w, int *h );

void		R_SetCustomColor( int num, int r, int g, int b );
void		R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );

qboolean	R_LerpTag( orientation_t *orient, const struct model_s *mod, int oldframe, int frame, float lerpfrac,
					  const char *name );

int			R_SkeletalGetNumBones( const struct model_s *mod, int *numFrames );
int			R_SkeletalGetBoneInfo( const struct model_s *mod, int bone, char *name, size_t name_size, int *flags );
void		R_SkeletalGetBonePose( const struct model_s *mod, int bone, int frame, bonepose_t *bonepose );

int			R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, 
								  int maxfragments, fragment_t *fragments );

void		R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out );

void		R_BeginFrame( float cameraSeparation, qboolean forceClear );
void		R_EndFrame( void );
const char *R_SpeedsMessage( char *out, size_t size );

void		R_BeginAviDemo( void );
void		R_WriteAviFrame( int frame, qboolean scissor );
void		R_StopAviDemo( void );

void		GLimp_AppActivate( qboolean active, qboolean destroy );

#endif /*__R_PUBLIC_H__*/
