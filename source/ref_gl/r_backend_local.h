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
#ifndef __R_BACKEND_LOCAL_H__
#define __R_BACKEND_LOCAL_H__

#define MAX_STREAM_VBO_VERTS		32768
#define MAX_STREAM_VBO_ELEMENTS		MAX_STREAM_VBO_VERTS*6
#define MAX_STREAM_VBO_TRIANGLES	MAX_STREAM_VBO_ELEMENTS/3
#define MAX_STREAM_VBO_INSTANCES	8192

#define MAX_BATCH_VERTS				4096
#define MAX_BATCH_ELEMENTS			MAX_BATCH_VERTS*6
#define MAX_BATCH_TRIANGLES			MAX_BATCH_ELEMENTS/3

typedef struct r_backend_stats_s
{
	unsigned int numVerts, numElems;
	unsigned int c_totalVerts, c_totalTris, c_totalStaticVerts, c_totalStaticTris, c_totalDraws;
} rbStats_t;

typedef struct
{
	unsigned int numBones;
	dualquat_t dualQuats[MAX_GLSL_UNIFORM_BONES];
	unsigned int maxWeights;
} rbBonesData_t;

typedef struct r_backend_s
{
	mempool_t			*mempool;

	struct
	{
		int				state;

		int				currentTMU;
		GLuint			currentTextures[MAX_TEXTURE_UNITS];
		GLuint			anyTexturesBound;
		int 			currentArrayVBO;
		int 			currentElemArrayVBO;

		int				faceCull;
		qboolean		frontFace;

		int				scissorX, scissorY;
		int				scissorW, scissorH;

		unsigned int	vertexAttribEnabled;

		int				fbWidth, fbHeight;

		float			polygonOffset[2];
	} gl;

	unsigned int time;

	rbStats_t stats;

	mat4_t objectMatrix;
	mat4_t modelviewMatrix;
	mat4_t projectionMatrix;
	mat4_t modelviewProjectionMatrix;
	int viewport[4];
	float zNear, zFar;

	const entity_t *currentEntity;
	modtype_t currentModelType;
	const mesh_vbo_t *currentMeshVBO;
	rbBonesData_t bonesData;
	const portalSurface_t *currentPortalSurface;
	int	currentProgram;
	int currentProgramObject;

	mesh_t batchMesh;
	vboSlice_t batches[RB_VBO_NUM_STREAMS];
	vboSlice_t streamOffset[RB_VBO_NUM_STREAMS];
	mesh_vbo_t *streamVBOs[RB_VBO_NUM_STREAMS];

	instancePoint_t *drawInstances;
	int maxDrawInstances;

	struct {
		unsigned int firstVert;
		unsigned int numVerts;
		unsigned int firstElem;
		unsigned int numElems;
		unsigned int numInstances;
	} drawElements;

	vattribmask_t currentVAttribs;

	int primitive;
	int currentVBOId;
	mesh_vbo_t *currentVBO;
	vboSlice_t *currentBatch;

	unsigned int currentDlightBits;
	unsigned int currentShadowBits;

	const shader_t *skyboxShader;
	int skyboxSide;

	// shader state
	const shader_t *currentShader;
	float currentShaderTime;
	int currentShaderState;
	int shaderStateORmask, shaderStateANDmask;
	qboolean dirtyUniformState;
	qboolean doneDepthPass;
	int donePassesTotal;

	qboolean triangleOutlines;

	const superLightStyle_t *superLightStyle;

	qbyte entityColor[4];
	qbyte entityOutlineColor[4];
	entity_t nullEnt;

	const mfog_t *fog, *texFog, *colorFog;

	qboolean greyscale;
	qboolean alphaHack;
	float hackedAlpha;

	float minLight;
} rbackend_t;

extern rbackend_t rb;

// r_backend.c
#define RB_Alloc(size) R_MallocExt( rb.mempool, size, 16, 1 )
#define RB_Free(data) R_Free(data)

void RB_DrawElementsReal( void );
#define RB_IsAlphaBlending(blendsrc,blenddst) \
	( (blendsrc) == GLSTATE_SRCBLEND_SRC_ALPHA || (blenddst) == GLSTATE_DSTBLEND_SRC_ALPHA ) || \
	( (blendsrc) == GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA || (blenddst) == GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA )

void RB_SelectTextureUnit( int tmu );

// r_backend_program.c
void RB_InitShading( void );
void RB_DrawOutlinedElements( void );
void RB_DrawShadedElements( void );
int RB_BindProgram( int program );
void RB_SetInstanceData( int numInstances, instancePoint_t *instances );

#endif /*__R_BACKEND_LOCAL_H__*/
