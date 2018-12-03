/*
Copyright (C) 2011 Victor Luchits

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

#define MAX_STREAM_VBO_VERTS        8192
#define MAX_STREAM_VBO_ELEMENTS     MAX_STREAM_VBO_VERTS * 6
#define MAX_STREAM_VBO_TRIANGLES    MAX_STREAM_VBO_ELEMENTS / 3

#define MAX_DYNAMIC_DRAWS           2048

typedef struct r_backend_stats_s {
	unsigned int numVerts, numElems;
	unsigned int c_totalVerts, c_totalTris, c_totalStaticVerts, c_totalStaticTris, c_totalDraws, c_totalBinds;
	unsigned int c_totalPrograms;
} rbStats_t;

typedef struct {
	unsigned int numBones;
	dualquat_t dualQuats[MAX_GLSL_UNIFORM_BONES];
	unsigned int maxWeights;
} rbBonesData_t;

typedef struct {
	unsigned int firstVert;
	unsigned int numVerts;
	unsigned int firstElem;
	unsigned int numElems;
	unsigned int numInstances;
} rbDrawElements_t;

typedef struct {
	mesh_vbo_t *vbo;
	uint8_t *vertexData;
	rbDrawElements_t drawElements;
} rbDynamicStream_t;

typedef struct {
	const entity_t *entity;
	const shader_t *shader;
	const mfog_t *fog;
	const portalSurface_t *portalSurface;
	vattribmask_t vattribs; // based on the fields above - cached to avoid rebinding
	int streamId;
	int primitive;
	vec2_t offset;
	int scissor[4];
	rbDrawElements_t drawElements;
} rbDynamicDraw_t;

typedef struct r_backend_s {
	mempool_t           *mempool;

	struct {
		int state;

		int currentArrayVBO;
		int currentElemArrayVBO;

		int faceCull;
		bool frontFace;

		int viewport[4];
		int scissor[4];
		bool scissorChanged;

		unsigned int vertexAttribEnabled;
		vattribmask_t lastVAttribs, lastHalfFloatVAttribs;

		int fbWidth, fbHeight;

		float polygonfactor, polygonunits;

		float depthmin, depthmax;

		bool depthoffset;

		bool flushTextures;
		int currentTMU;
		unsigned currentTextures[MAX_TEXTURE_UNITS];
	} gl;

	int64_t time;

	rbStats_t stats;

	mat4_t cameraMatrix;
	mat4_t objectMatrix;
	mat4_t objectToLightMatrix;
	vec3_t lightDir;
	mat4_t modelviewMatrix;
	mat4_t projectionMatrix;
	mat4_t modelviewProjectionMatrix;
	float zNear, zFar;

	int mode;
	int renderFlags;
	int surfFlags;

	vec3_t cameraOrigin;
	mat3_t cameraAxis;

	const entity_t *currentEntity;
	modtype_t currentModelType;
	const mesh_vbo_t *currentMeshVBO;
	rbBonesData_t bonesData;
	const portalSurface_t *currentPortalSurface;

	// glUseProgram cache
	int currentProgram;
	int currentProgramObject;

	// RP_RegisterProgram cache
	int currentRegProgram;
	int currentRegProgramType;
	r_glslfeat_t currentRegProgramFeatures;

	rbDynamicStream_t dynamicStreams[RB_VBO_NUM_STREAMS];
	rbDynamicDraw_t dynamicDraws[MAX_DYNAMIC_DRAWS];
	int numDynamicDraws;

	instancePoint_t *drawInstances;
	int maxDrawInstances;

	rbDrawElements_t drawElements;

	vattribmask_t currentVAttribs;

	int primitive;
	int currentVBOId;
	mesh_vbo_t *currentVBO;

	const shader_t *skyboxShader;
	int skyboxSide;

	// shader state
	const shader_t *currentShader;
	double currentShaderTime;
	int currentShaderState;
	int shaderStateORmask, shaderStateANDmask;
	bool dirtyUniformState;
	bool doneDepthPass;
	int donePassesTotal;

	bool triangleOutlines;

	const superLightStyle_t *superLightStyle, *realSuperLightStyle;

	uint8_t entityColor[4];
	uint8_t entityOutlineColor[4];
	entity_t nullEnt;

	const mfog_t *fog, *texFog, *colorFog;

	bool greyscale;
	bool alphaHack;
	bool noDepthTest;
	bool noColorWrite;
	bool depthEqual;
	float hackedAlpha;

	float minLight;
	float hdrExposure;
	bool noWorldLight;
	refScreenTexSet_t st;

	unsigned numRealtimeLights;
	rtlight_t *rtlights[MAX_DRAWSURF_RTLIGHTS];
} rbackend_t;

extern rbackend_t rb;

// r_backend.c
#define RB_Alloc( size ) R_MallocExt( rb.mempool, size, 16, 1 )
#define RB_Free( data ) R_Free( data )

void RB_DrawElementsReal( rbDrawElements_t *de );
#define RB_IsAlphaBlending( blendsrc,blenddst ) \
	( ( blendsrc ) == GLSTATE_SRCBLEND_SRC_ALPHA || ( blenddst ) == GLSTATE_DSTBLEND_SRC_ALPHA ) || \
	( ( blendsrc ) == GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA || ( blenddst ) == GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA )

// r_backend_program.c
void RB_InitShading( void );
void RB_DrawOutlinedElements( void );
void RB_DrawShadedElements( void );
int RB_RegisterProgram( int type, const char *name, const char *deformsKey,
						const deformv_t *deforms, int numDeforms, r_glslfeat_t features );
int RB_BindProgram( int program );
void RB_BindImage( int tmu, const image_t *tex );
void RB_BindArrayBuffer( int buffer );
void RB_BindElementArrayBuffer( int buffer );
void RB_SetInstanceData( int numInstances, instancePoint_t *instances );
bool RB_ScissorForBounds( vec3_t bbox[8], int *x, int *y, int *w, int *h );
