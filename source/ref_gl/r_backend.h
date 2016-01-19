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
#ifndef R_BACKEND_H
#define R_BACKEND_H

enum
{
	RB_VBO_STREAM_COMPACT		= -2, // bind RB_VBO_STREAM instead
	RB_VBO_STREAM				= -1,
	RB_VBO_NONE					= 0,
	RB_VBO_NUM_STREAMS			= -RB_VBO_STREAM_COMPACT
};

//===================================================================

struct shader_s;
struct mfog_s;
struct superLightStyle_s;
struct portalSurface_s;

// core
void RB_Init( void );
void RB_Shutdown( void );
void RB_SetTime( unsigned int time );
void RB_BeginFrame( void );
void RB_EndFrame( void );
void RB_BeginRegistration( void );
void RB_EndRegistration( void );

void RB_LoadCameraMatrix( const mat4_t m );
void RB_LoadObjectMatrix( const mat4_t m );
void RB_LoadProjectionMatrix( const mat4_t m );

void RB_DepthRange( float depthmin, float depthmax );
void RB_GetDepthRange( float* depthmin, float *depthmax );
void RB_DepthOffset( bool enable );
void RB_ClearDepth( float depth );
void RB_Cull( int cull );
void RB_SetState( int state );
void RB_FrontFace( bool front );
void RB_FlipFrontFace( void );
void RB_Scissor( int x, int y, int w, int h );
void RB_GetScissor( int *x, int *y, int *w, int *h );
void RB_ApplyScissor( void );
void RB_Viewport( int x, int y, int w, int h );
void RB_Clear( int bits, float r, float g, float b, float a );
void RB_SetZClip( float zNear, float zFar );

void RB_BindFrameBufferObject( int object );
int RB_BoundFrameBufferObject( void );
void RB_BlitFrameBufferObject( int dest, int bitMask, int mode );

void RB_BindVBO( int id, int primitive );

void RB_AddDynamicMesh( const entity_t *entity, const shader_t *shader,
	const struct mfog_s *fog, const struct portalSurface_s *portalSurface, unsigned int shadowBits,
	const struct mesh_s *mesh, int primitive, float x_offset, float y_offset );
void RB_FlushDynamicMeshes( void );

void RB_DrawElements( int firstVert, int numVerts, int firstElem, int numElems,
	int firstShadowVert, int numShadowVerts, int firstShadowElem, int numShadowElems );
void RB_DrawElementsInstanced( int firstVert, int numVerts, int firstElem, int numElems,
	int firstShadowVert, int numShadowVerts, int firstShadowElem, int numShadowElems,
	int numInstances, instancePoint_t *instances );

void RB_FlushTextureCache( void );

// shader
void RB_BindShader( const entity_t *e, const struct shader_s *shader, const struct mfog_s *fog );
void RB_SetLightstyle( const struct superLightStyle_s *lightStyle );
void RB_SetDlightBits( unsigned int dlightBits );
void RB_SetShadowBits( unsigned int shadowBits );
void RB_SetBonesData( int numBones, dualquat_t *dualQuats, int maxWeights );
void RB_SetPortalSurface( const struct portalSurface_s *portalSurface );
void RB_SetSkyboxShader( const shader_t *shader );
void RB_SetSkyboxSide( int side );
void RB_SetRenderFlags( int flags );
void RB_SetLightParams( float minLight, bool noWorldLight );
void RB_SetShaderStateMask( int ANDmask, int ORmask );
void RB_SetCamera( const vec3_t cameraOrigin, const mat3_t cameraAxis );
bool RB_EnableTriangleOutlines( bool enable );

vattribmask_t RB_GetVertexAttribs( void );

void RB_StatsMessage( char *msg, size_t size );

#endif // R_BACKEND_H
