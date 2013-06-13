/*
Copyright (C) 2002-2011 Victor Luchits

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

#include "r_local.h"
#include "r_backend_local.h"

ALIGN( 16 ) vec3_t batchVertsArray[MAX_BATCH_VERTS];
ALIGN( 16 ) vec3_t batchNormalsArray[MAX_BATCH_VERTS];
ALIGN( 16 ) vec4_t batchSVectorsArray[MAX_BATCH_VERTS];
ALIGN( 16 ) vec2_t batchSTCoordsArray[MAX_BATCH_VERTS];
ALIGN( 16 ) vec2_t batchLMCoordsArray[MAX_LIGHTMAPS][MAX_BATCH_VERTS];
ALIGN( 16 ) byte_vec4_t batchColorsArray[MAX_LIGHTMAPS][MAX_BATCH_VERTS];
ALIGN( 16 ) elem_t batchElements[MAX_BATCH_ELEMENTS];

rbackend_t rb;

static void RB_InitBatchMesh( void );
static void RB_SetGLDefaults( void );
static void RB_RegisterStreamVBOs( void );
static void RB_UploadStaticQuadIndices( void );

/*
* RB_Init
*/
void RB_Init( void )
{
	memset( &rb, 0, sizeof( rb ) );

	rb.mempool = Mem_AllocPool( NULL, "Rendering Backend" );

	// set default OpenGL state
	RB_SetGLDefaults();

	// initialize shading
	RB_InitShading();

	// intialize batching
	RB_InitBatchMesh();

	// create VBO's we're going to use for streamed data
	RB_RegisterStreamVBOs();

	// upload persistent quad indices
	RB_UploadStaticQuadIndices();
}

/*
* RB_Shutdown
*/
void RB_Shutdown( void )
{
	Mem_FreePool( &rb.mempool );
}

/*
* RB_BeginRegistration
*/
void RB_BeginRegistration( void )
{
	RB_RegisterStreamVBOs();
}

/*
* RB_EndRegistration
*/
void RB_EndRegistration( void )
{
}

/*
* RB_SetTime
*/
void RB_SetTime( unsigned int time )
{
	rb.time = time;
	rb.nullEnt.shaderTime = Sys_Milliseconds();
}

/*
* RB_BeginFrame
*/
void RB_BeginFrame( void )
{
	Vector4Set( rb.nullEnt.shaderRGBA, 1, 1, 1, 1 );
	rb.nullEnt.scale = 1;
	VectorClear( rb.nullEnt.origin );
	Matrix3_Identity( rb.nullEnt.axis );

	memset( &rb.stats, 0, sizeof( rb.stats ) );

	RB_SetShaderStateMask( ~0, 0 );
}

/*
* RB_EndFrame
*/
void RB_EndFrame( void )
{
}

/*
* RB_StatsMessage
*/
void RB_StatsMessage( char *msg, size_t size )
{
	Q_snprintfz( msg, size, 
		"%4i verts %4i tris\n"
		"%4i draws",		
		rb.stats.c_totalVerts, rb.stats.c_totalTris,
		rb.stats.c_totalDraws
	);
}

/*
* RB_SetGLDefaults
*/
static void RB_SetGLDefaults( void )
{
	int i;

	qglClearColor( 1, 0, 0.5, 0.5 );

	if( glConfig.stencilEnabled )
	{
		qglStencilMask( ( GLuint ) ~0 );
		qglStencilFunc( GL_EQUAL, 128, 0xFF );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	// properly disable multitexturing at startup
	for( i = glConfig.maxTextureUnits-1; i >= 0; i-- )
	{
		RB_SelectTextureUnit( i );
		qglDisable( GL_TEXTURE_2D );
	}
	qglEnable( GL_TEXTURE_2D );

	qglDisable( GL_CULL_FACE );
	qglFrontFace( GL_CCW );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglDepthFunc( GL_LEQUAL );
	qglDepthMask( GL_FALSE );
	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	qglEnable( GL_DEPTH_TEST );
	qglShadeModel( GL_SMOOTH );
	if( qglPolygonMode ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}
	qglFrontFace( GL_CCW );
}

/*
* RB_SelectTextureUnit
*/
void RB_SelectTextureUnit( int tmu )
{
	if( tmu == rb.gl.currentTMU )
		return;

	rb.gl.currentTMU = tmu;

	qglActiveTextureARB( tmu + GL_TEXTURE0_ARB );
	qglClientActiveTextureARB( tmu + GL_TEXTURE0_ARB );
}

/*
* RB_BindTexture
*/
void RB_BindTexture( int tmu, const image_t *tex )
{
	GLuint texnum;

	assert( tex != NULL );

	if( r_nobind->integer && r_notexture && tex->texnum != 0 )  // performance evaluation option
		tex = r_notexture;

	RB_SelectTextureUnit( tmu );

	texnum = tex->texnum;
	if( rb.gl.currentTextures[tmu] == texnum )
		return;

	rb.gl.anyTexturesBound = 1;
	rb.gl.currentTextures[tmu] = texnum;
	if( tex->flags & IT_CUBEMAP )
		qglBindTexture( GL_TEXTURE_CUBE_MAP_ARB, texnum );
	else
		qglBindTexture( GL_TEXTURE_2D, texnum );
}

/*
* RB_AllocTextureNum
*/
void RB_AllocTextureNum( image_t *tex )
{
	qglGenTextures( 1, &tex->texnum );
}

/*
* RB_FreeTextureNum
*/
void RB_FreeTextureNum( image_t *tex )
{
	qglDeleteTextures( 1, &tex->texnum );
	tex->texnum = 0;

	// Ensures that the RB_BindTexture call that may follow will work
	if( rb.gl.anyTexturesBound ) {
		rb.gl.anyTexturesBound = 0;
		memset( rb.gl.currentTextures, 0, sizeof( rb.gl.currentTextures ) );
	}
}

/*
* RB_DepthRange
*/
void RB_DepthRange( float depthmin, float depthmax )
{
	gldepthmin = bound( 0, depthmin, 1 );
	gldepthmax = bound( 0, depthmax, 1 );
	qglDepthRange( gldepthmin, gldepthmax );
}

/*
* RB_LoadObjectMatrix
*/
void RB_LoadObjectMatrix( const mat4_t m )
{
	Matrix4_Copy( m, rb.objectMatrix );
}

/*
* RB_LoadModelviewMatrix
*/
void RB_LoadModelviewMatrix( const mat4_t m )
{
	Matrix4_Copy( m, rb.modelviewMatrix );
}

/*
* RB_LoadProjectionMatrix
*/
void RB_LoadProjectionMatrix( const mat4_t m )
{
	Matrix4_Copy( m, rb.projectionMatrix );
}

/*
* RB_Cull
*/
void RB_Cull( int cull )
{
	if( rb.gl.faceCull == cull )
		return;

	if( !cull )
	{
		qglDisable( GL_CULL_FACE );
		rb.gl.faceCull = 0;
		return;
	}

	if( !rb.gl.faceCull )
		qglEnable( GL_CULL_FACE );
	qglCullFace( cull );
	rb.gl.faceCull = cull;
}

/*
* RB_PolygonOffset
*/
void RB_PolygonOffset( float factor, float offset )
{
	if( rb.gl.polygonOffset[0] == factor && rb.gl.polygonOffset[1] == offset )
		return;

	qglPolygonOffset( factor, offset );
	rb.gl.polygonOffset[0] = factor;
	rb.gl.polygonOffset[1] = offset;
}

/*
* RB_SetState
*/
void RB_SetState( int state )
{
	int diff;

	diff = rb.gl.state ^ state;
	if( !diff )
		return;

	if( diff & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
	{
		if( state & ( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK ) )
		{
			int blendsrc, blenddst;

			switch( state & GLSTATE_SRCBLEND_MASK )
			{
			case GLSTATE_SRCBLEND_ZERO:
				blendsrc = GL_ZERO;
				break;
			case GLSTATE_SRCBLEND_DST_COLOR:
				blendsrc = GL_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR:
				blendsrc = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLSTATE_SRCBLEND_SRC_ALPHA:
				blendsrc = GL_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				blendsrc = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_SRCBLEND_DST_ALPHA:
				blendsrc = GL_DST_ALPHA;
				break;
			case GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA:
				blendsrc = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
			case GLSTATE_SRCBLEND_ONE:
				blendsrc = GL_ONE;
				break;
			}

			switch( state & GLSTATE_DSTBLEND_MASK )
			{
			case GLSTATE_DSTBLEND_ONE:
				blenddst = GL_ONE;
				break;
			case GLSTATE_DSTBLEND_SRC_COLOR:
				blenddst = GL_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR:
				blenddst = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLSTATE_DSTBLEND_SRC_ALPHA:
				blenddst = GL_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				blenddst = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLSTATE_DSTBLEND_DST_ALPHA:
				blenddst = GL_DST_ALPHA;
				break;
			case GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA:
				blenddst = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
			case GLSTATE_DSTBLEND_ZERO:
				blenddst = GL_ZERO;
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( blendsrc, blenddst );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	if( diff & GLSTATE_ALPHAFUNC )
	{
		int alphafunc = state & GLSTATE_ALPHAFUNC;

		if( alphafunc )
		{
			qglEnable( GL_ALPHA_TEST );
			if( alphafunc == GLSTATE_AFUNC_GT0 )
				qglAlphaFunc( GL_GREATER, 0 );
			else if( alphafunc == GLSTATE_AFUNC_LT128 )
				qglAlphaFunc( GL_LESS, 0.5f );
			else
				qglAlphaFunc( GL_GEQUAL, 0.5f );
		}
		else
		{
			qglDisable( GL_ALPHA_TEST );
		}
	}

	if( diff & GLSTATE_NO_COLORWRITE )
	{
		if( state & GLSTATE_NO_COLORWRITE )
		{
			qglShadeModel( GL_FLAT );
			qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		}
		else
		{
			qglShadeModel( GL_SMOOTH );
			qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		}
	}

	if( diff & GLSTATE_DEPTHFUNC_EQ )
	{
		if( state & GLSTATE_DEPTHFUNC_EQ )
			qglDepthFunc( GL_EQUAL );
		else
			qglDepthFunc( GL_LEQUAL );
	}

	if( diff & GLSTATE_DEPTHWRITE )
	{
		if( state & GLSTATE_DEPTHWRITE )
			qglDepthMask( GL_TRUE );
		else
			qglDepthMask( GL_FALSE );
	}

	if( diff & GLSTATE_NO_DEPTH_TEST )
	{
		if( state & GLSTATE_NO_DEPTH_TEST )
			qglDisable( GL_DEPTH_TEST );
		else
			qglEnable( GL_DEPTH_TEST );
	}

	if( diff & GLSTATE_OFFSET_FILL )
	{
		if( state & GLSTATE_OFFSET_FILL )
			qglEnable( GL_POLYGON_OFFSET_FILL );
		else
			qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	if( diff & GLSTATE_STENCIL_TEST )
	{
		if( glConfig.stencilEnabled )
		{
			if( state & GLSTATE_STENCIL_TEST )
				qglEnable( GL_STENCIL_TEST );
			else
				qglDisable( GL_STENCIL_TEST );
		}
	}

	rb.gl.state = state;
}

/*
* RB_FrontFace
*/
void RB_FrontFace( qboolean front )
{
	qglFrontFace( front ? GL_CW : GL_CCW );
	rb.gl.frontFace = front;
}

/*
* RB_FlipFrontFace
*/
void RB_FlipFrontFace( void )
{
	RB_FrontFace( !rb.gl.frontFace );
}

/*
* RB_BindArrayBuffer
*/
void RB_BindArrayBuffer( int buffer )
{
	if( buffer != rb.gl.currentArrayVBO )
	{
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer );
		rb.gl.currentArrayVBO = buffer;
	}
}

/*
* RB_BindElementArrayBuffer
*/
void RB_BindElementArrayBuffer( int buffer )
{
	if( buffer != rb.gl.currentElemArrayVBO )
	{
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, buffer );
		rb.gl.currentElemArrayVBO = buffer;
	}
}

/*
* GL_EnableVertexAttrib
*/
static void GL_EnableVertexAttrib( int index, qboolean enable )
{
	unsigned int bit;
	unsigned int diff;

	bit = 1 << index;
	diff = (rb.gl.vertexAttribEnabled & bit) ^ (enable ? bit : 0);
	if( !diff ) {
		return;
	}

	if( enable ) {
		rb.gl.vertexAttribEnabled |= bit;
		qglEnableVertexAttribArrayARB( index );
	}
	else {
		rb.gl.vertexAttribEnabled &= ~bit;
		qglDisableVertexAttribArrayARB( index );
	}
}

/*
* RB_Scissor
*/
void RB_Scissor( int x, int y, int w, int h )
{
	qglScissor( x, glConfig.height - h - y, w, h );

	rb.gl.scissorX = x;
	rb.gl.scissorY = y;
	rb.gl.scissorW = w;
	rb.gl.scissorH = h;
}

/*
* RB_GetScissorRegion
*/
void RB_GetScissorRegion( int *x, int *y, int *w, int *h )
{
	if( x ) {
		*x = rb.gl.scissorX;
	}
	if( y ) {
		*y = rb.gl.scissorY;
	}
	if( w ) {
		*w = rb.gl.scissorW;
	}
	if( h ) {
		*h = rb.gl.scissorH;
	}
}

/*
* RB_Viewport
*/
void RB_Viewport( int x, int y, int w, int h )
{
	qglViewport( x, glConfig.height - h - y, w, h );
}

/*
* RB_Clear
*/
void RB_Clear( int bits, byte_vec4_t clearColor )
{
	// this is required for glClear(GL_DEPTH_BUFFER_BIT) to work
	if( bits & GL_DEPTH_BUFFER_BIT )
		RB_SetState( GLSTATE_DEPTHWRITE );

	if( bits & GL_STENCIL_BUFFER_BIT )
		qglClearStencil( 128 );

	if( bits & GL_COLOR_BUFFER_BIT )
		qglClearColor( clearColor[0] * 1.0/255.0, clearColor[1] * 1.0/255.0, clearColor[2] * 1.0/255.0, 1 );

	qglClear( bits );

	RB_DepthRange( 0, 1 );
}

/*
* RB_UploadQuadIndicesToStream
*/
static void RB_UploadStaticQuadIndices( void )
{
	int leftVerts, numVerts, numElems;
	int vertsOffset, elemsOffset;
	mesh_t mesh;
	mesh_vbo_t *vbo = rb.streamVBOs[-RB_VBO_STREAM_QUAD - 1];

	assert( MAX_BATCH_VERTS < MAX_STREAM_VBO_VERTS );

	vertsOffset = 0;
	elemsOffset = 0;
	
	memset( &mesh, 0, sizeof( mesh ) );

	for( leftVerts = MAX_STREAM_VBO_VERTS; leftVerts > 0; leftVerts -= numVerts ) {
		numVerts = min( MAX_BATCH_VERTS, leftVerts );
		numElems = numVerts/4*6;

		mesh.numElems = numElems;
		mesh.numVerts = numVerts;

		R_UploadVBOElemData( vbo, vertsOffset, elemsOffset, &mesh, VBO_HINT_ELEMS_QUAD );
		vertsOffset += numVerts;
		elemsOffset += numElems;
	}
}

/*
* RB_RegisterStreamVBOs
*
* Allocate/keep alive dynamic vertex buffers object 
* we'll steam the dynamic geometry into
*/
void RB_RegisterStreamVBOs( void )
{
	int i;
	mesh_vbo_t *vbo;
	vbo_tag_t tags[RB_VBO_NUM_STREAMS] = {
		VBO_TAG_STREAM,
		VBO_TAG_STREAM_STATIC_ELEMS
	};

	// allocate stream VBO's
	for( i = 0; i < RB_VBO_NUM_STREAMS; i++ ) {
		vbo = rb.streamVBOs[i];
		if( vbo ) {
			R_TouchMeshVBO( vbo );
			continue;
		}
		rb.streamVBOs[i] = R_CreateMeshVBO( &rb, 
			MAX_STREAM_VBO_VERTS, MAX_STREAM_VBO_ELEMENTS, MAX_STREAM_VBO_INSTANCES,
			VATTRIBS_MASK, tags[i] );
	}
}

/*
* RB_InitBatchMesh
*/
static void RB_InitBatchMesh( void )
{
	int i;
	mesh_t *mesh = &rb.batchMesh;

	mesh->numVerts = 0;
	mesh->numElems = 0;
	mesh->xyzArray = batchVertsArray;
	mesh->normalsArray = batchNormalsArray;
	mesh->sVectorsArray = batchSVectorsArray;
	mesh->stArray = batchSTCoordsArray;
	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		mesh->lmstArray[i] = batchLMCoordsArray[i];
		mesh->colorsArray[i] = batchColorsArray[i];
	}
	mesh->elems = batchElements;
}

/*
* RB_BindVBO
*/
void RB_BindVBO( int id, int primitive )
{
	mesh_vbo_t *vbo;
	vboSlice_t *batch;

	if( rb.currentVBOId == id ) {
		return;
	}

	if( id < RB_VBO_NONE ) {
		vbo = rb.streamVBOs[-id - 1];
		batch = &rb.batches[-id - 1];
	} else if( id == RB_VBO_NONE ) {
		vbo = NULL;
		batch = NULL;
	}
	else {
		vbo = R_GetVBOByIndex( id );
		batch = NULL;
	}

	rb.primitive = primitive;
	rb.currentVBOId = id;
	rb.currentVBO = vbo;
	rb.currentBatch = batch;
	if( !vbo ) {
		RB_BindArrayBuffer( 0 );
		RB_BindElementArrayBuffer( 0 );
		return;
	}

	RB_BindArrayBuffer( vbo->vertexId );
	RB_BindElementArrayBuffer( vbo->elemId );
}

/*
* RB_UploadMesh
*/
void RB_UploadMesh( const mesh_t *mesh )
{
	int stream;
	mesh_vbo_t *vbo;
	vboSlice_t *offset;
	vbo_hint_t vbo_hint = VBO_HINT_NONE;
	int numVerts = mesh->numVerts, numElems = mesh->numElems;

	assert( rb.currentVBOId < RB_VBO_NONE );
	if( rb.currentVBOId >= RB_VBO_NONE ) {
		return;
	}
	
	if( rb.currentVBOId == RB_VBO_STREAM_QUAD ) {
		numElems = numVerts/4*6;
	} else if( !numElems && rb.currentVBOId == RB_VBO_STREAM ) {
		numElems = (max(numVerts, 2) - 2) * 3;
	}

	if( !numVerts || !numElems ) {
		return;
	}

	vbo = rb.currentVBO;
	stream = -rb.currentVBOId - 1;
	offset = &rb.streamOffset[stream];

	if( offset->firstVert+offset->numVerts+numVerts > MAX_STREAM_VBO_VERTS || 
		offset->firstElem+offset->numVerts+numElems > MAX_STREAM_VBO_ELEMENTS ) {

		RB_DrawElements( offset->firstVert, offset->numVerts, 
			offset->firstElem, offset->numElems );

		R_DiscardVBOVertexData( vbo );
		if( rb.currentVBOId != RB_VBO_STREAM_QUAD ) {
			R_DiscardVBOElemData( vbo );
		}

		offset->firstVert = 0;
		offset->firstElem = 0;
		offset->numVerts = 0;
		offset->numElems = 0;
	}

	if( numVerts > MAX_STREAM_VBO_VERTS ||
		numElems > MAX_STREAM_VBO_ELEMENTS ) {
		// FIXME: do something about this?
		return;
	}

	if( rb.currentVBOId == RB_VBO_STREAM_QUAD ) {
		vbo_hint = VBO_HINT_ELEMS_QUAD;

		// quad indices are stored in a static vbo, don't call R_UploadVBOElemData
	} else {
		if( mesh->elems ) {
			vbo_hint = VBO_HINT_NONE;
		} else if( rb.currentVBOId == RB_VBO_STREAM ) {
			vbo_hint = VBO_HINT_ELEMS_TRIFAN;
		} else {
			assert( 0 );
		}
		R_UploadVBOElemData( vbo, offset->firstVert + offset->numVerts, 
			offset->firstElem + offset->numElems, mesh, vbo_hint );
	}

	R_UploadVBOVertexData( vbo, offset->firstVert + offset->numVerts, 
		rb.currentVAttribs, mesh, vbo_hint );

	offset->numElems += numElems;
	offset->numVerts += numVerts;
}

/*
* RB_UploadBatchMesh
*/
static void RB_UploadBatchMesh( vboSlice_t *batch )
{
	rb.batchMesh.numVerts = batch->numVerts;
	rb.batchMesh.numElems = batch->numElems;

	RB_UploadMesh( &rb.batchMesh );

	batch->numElems = batch->numVerts = 0;
	batch->firstElem = batch->firstVert = 0;
}

/*
* RB_MapBatchMesh
*/
mesh_t *RB_MapBatchMesh( int numVerts, int numElems )
{
	int stream;
	vboSlice_t *batch, *offset;

	assert( rb.currentVBOId < RB_VBO_NONE );
	if( rb.currentVBOId >= RB_VBO_NONE ) {
		return NULL;
	}

	if( numVerts > MAX_BATCH_VERTS || 
		numElems > MAX_BATCH_ELEMENTS ) {
		return NULL;
	}

	stream = -rb.currentVBOId - 1;
	batch = &rb.batches[stream];
	offset = &rb.streamOffset[stream];

	batch->numElems = batch->numVerts = 0;
	batch->firstElem = batch->firstVert = 0;

	RB_InitBatchMesh();

	return &rb.batchMesh;
}

/*
* RB_BeginBatch
*/
void RB_BeginBatch( void )
{
	mesh_t *mesh;

	mesh = RB_MapBatchMesh( 0, 0 );
	assert( mesh != NULL );
}

/*
* RB_BatchMesh
*/
void RB_BatchMesh( const mesh_t *mesh )
{
	int stream;
	vboSlice_t *batch;
	int numVerts = mesh->numVerts, numElems = mesh->numElems;

	if( rb.currentVBOId == RB_VBO_STREAM_QUAD ) {
		numElems = numVerts/4*6;
	} else if( !numElems && rb.currentVBOId == RB_VBO_STREAM ) {
		numElems = (max(numVerts, 2) - 2) * 3;
	}

	if( !numVerts || !numElems ) {
		return;
	}

	assert( rb.currentVBOId < RB_VBO_NONE );
	if( rb.currentVBOId >= RB_VBO_NONE ) {
		return;
	}

	stream = -rb.currentVBOId - 1;
	batch = &rb.batches[stream];

	if( numVerts+batch->numVerts > MAX_BATCH_VERTS || 
		numElems+batch->numElems > MAX_BATCH_ELEMENTS ) {
		RB_UploadBatchMesh( batch );
	}

	if( numVerts > MAX_BATCH_VERTS || 
		numElems > MAX_BATCH_ELEMENTS ) {
		RB_UploadMesh( mesh );
	}
	else {
		int i;
		vattrib_t vattribs = rb.currentVAttribs;

		memcpy( rb.batchMesh.xyzArray + batch->numVerts, mesh->xyzArray, numVerts * sizeof( vec3_t ) );
		if( rb.currentVBOId == RB_VBO_STREAM_QUAD ) {
			// quad indices are stored in a static vbo
		} else if( mesh->elems ) {
			if( rb.primitive == GL_TRIANGLES ) {
				R_CopyOffsetTriangles( mesh->elems, numElems, batch->numVerts, rb.batchMesh.elems + batch->numElems );
			}
			else {
				R_CopyOffsetElements( mesh->elems, numElems, batch->numVerts, rb.batchMesh.elems + batch->numElems );
			}
		} else if( rb.currentVBOId == RB_VBO_STREAM ) {
			R_BuildTrifanElements( batch->numVerts, numElems, rb.batchMesh.elems + batch->numElems );
		} else {
			assert( 0 );
		}
		if( mesh->normalsArray && (vattribs & VATTRIB_NORMAL_BIT) ) {
			memcpy( rb.batchMesh.normalsArray + batch->numVerts, mesh->normalsArray, numVerts * sizeof( vec3_t ) );
		}
		if( mesh->sVectorsArray && (vattribs & VATTRIB_SVECTOR_BIT) ) {
			memcpy( rb.batchMesh.sVectorsArray + batch->numVerts, mesh->sVectorsArray, numVerts * sizeof( vec4_t ) );
		}
		if( mesh->stArray && (vattribs & VATTRIB_TEXCOORDS_BIT) ) {
			memcpy( rb.batchMesh.stArray + batch->numVerts, mesh->stArray, numVerts * sizeof( vec2_t ) );
		}
		
		if( mesh->lmstArray[0] && (vattribs & VATTRIB_LMCOORDS_BIT) ) {
			memcpy( rb.batchMesh.lmstArray[0] + batch->numVerts, mesh->lmstArray[0], numVerts * sizeof( vec2_t ) );

			for( i = 1; i < MAX_LIGHTMAPS; i++ ) {
				if( !mesh->lmstArray[i] || !(vattribs & (VATTRIB_LMCOORDS1_BIT<<(i-1))) ) {
					break;
				}
				memcpy( rb.batchMesh.lmstArray[i] + batch->numVerts, mesh->lmstArray[i], numVerts * sizeof( vec2_t ) );
			}
		}

		if( mesh->colorsArray[0] && (vattribs & VATTRIB_COLOR_BIT) ) {
			memcpy( rb.batchMesh.colorsArray[0] + batch->numVerts, mesh->colorsArray[0], numVerts * sizeof( byte_vec4_t ) );

			for( i = 1; i < MAX_LIGHTMAPS; i++ ) {
				if( !mesh->colorsArray[i] || !(vattribs & (VATTRIB_COLOR1_BIT<<(i-1))) ) {
					break;
				}
				memcpy( rb.batchMesh.colorsArray[i] + batch->numVerts, mesh->colorsArray[i], numVerts * sizeof( byte_vec4_t ) );
			}
		}

		batch->numVerts += numVerts;
		batch->numElems += numElems;
	}
}

/*
* RB_EndBatch
*/
void RB_EndBatch( void )
{
	int stream;
	vboSlice_t *batch;
	vboSlice_t *offset;

	if( rb.currentVBOId >= RB_VBO_NONE ) {
		return;
	}

	stream = -rb.currentVBOId - 1;
	offset = &rb.streamOffset[stream];
	batch = &rb.batches[stream];

	if( batch->numVerts ) {
		RB_UploadBatchMesh( batch );
	}

	if( !offset->numVerts || !offset->numElems ) {
		return;
	}

	RB_DrawElements( offset->firstVert, offset->numVerts, offset->firstElem, offset->numElems );

	offset->firstVert += offset->numVerts;
	offset->firstElem += offset->numElems;
	offset->numVerts = offset->numElems = 0;
}

/*
* RB_EnableVertexAttribs
*/
static void RB_EnableVertexAttribs( void )
{
	int i;
	vattrib_t vattribs = rb.currentVAttribs;
	mesh_vbo_t *vbo = rb.currentVBO;

	// xyz position
	GL_EnableVertexAttrib( VATTRIB_POSITION, qtrue );
	qglVertexAttribPointerARB( VATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 
		( const GLvoid * )0 );

	// normal
	if( vattribs & VATTRIB_NORMAL_BIT ) {
		GL_EnableVertexAttrib( VATTRIB_NORMAL, qtrue );
		qglVertexAttribPointerARB( VATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 0,
			( const GLvoid * )vbo->normalsOffset );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_NORMAL, qfalse );
	}

	// s-vector
	if( vattribs & VATTRIB_SVECTOR_BIT ) {
		GL_EnableVertexAttrib( VATTRIB_SVECTOR, qtrue );
		qglVertexAttribPointerARB( VATTRIB_SVECTOR, 4, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->sVectorsOffset );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_SVECTOR, qfalse );
	}
	
	// color
	if( vattribs & VATTRIB_COLOR_BIT ) {
		GL_EnableVertexAttrib( VATTRIB_COLOR, qtrue );
		qglVertexAttribPointerARB( VATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, (
			const GLvoid * )vbo->colorsOffset[0] );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_COLOR, qfalse );
	}

	// texture coordinates
	if( vattribs & VATTRIB_TEXCOORDS_BIT ) {
		GL_EnableVertexAttrib( VATTRIB_TEXCOORDS, qtrue );
		qglVertexAttribPointerARB( VATTRIB_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->stOffset );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_TEXCOORDS, qfalse );
	}

	if( (vattribs & VATTRIB_AUTOSPRITE2_BIT) == VATTRIB_AUTOSPRITE2_BIT ) {
		// submit sprite centre and the longest edge
		GL_EnableVertexAttrib( VATTRIB_SPRITEPOINT, qtrue );
		qglVertexAttribPointerARB( VATTRIB_SPRITEPOINT, 4, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->spritePointsOffset );

		GL_EnableVertexAttrib( VATTRIB_SPRITERAXIS, qtrue );
		qglVertexAttribPointerARB( VATTRIB_SPRITERAXIS, 3, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->spriteRightAxesOffset );

		GL_EnableVertexAttrib( VATTRIB_SPRITEUAXIS, qtrue );
		qglVertexAttribPointerARB( VATTRIB_SPRITEUAXIS, 3, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->spriteUpAxesOffset );
	}
	else if( (vattribs & VATTRIB_AUTOSPRITE_BIT) == VATTRIB_AUTOSPRITE_BIT ) {
		// submit sprite point
		GL_EnableVertexAttrib( VATTRIB_SPRITERAXIS, qfalse );
		GL_EnableVertexAttrib( VATTRIB_SPRITEUAXIS, qfalse );
		GL_EnableVertexAttrib( VATTRIB_SPRITEPOINT, qtrue );
		qglVertexAttribPointerARB( VATTRIB_SPRITEPOINT, 4, GL_FLOAT, GL_FALSE, 0, 
			( const GLvoid * )vbo->spritePointsOffset );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_SPRITEPOINT, qfalse );
		GL_EnableVertexAttrib( VATTRIB_SPRITERAXIS, qfalse );
		GL_EnableVertexAttrib( VATTRIB_SPRITEUAXIS, qfalse );
	}

	// bones (skeletal models)
	if( (vattribs & VATTRIB_BONES_BIT) == VATTRIB_BONES_BIT ) {
		// submit indices
		GL_EnableVertexAttrib( VATTRIB_BONESINDICES, qtrue );
		qglVertexAttribPointerARB( VATTRIB_BONESINDICES, 4, GL_UNSIGNED_BYTE, GL_FALSE, SKM_MAX_WEIGHTS, 
			( const GLvoid * )vbo->bonesIndicesOffset );

		// submit weights
		GL_EnableVertexAttrib( VATTRIB_BONESWEIGHTS, qtrue );
		qglVertexAttribPointerARB( VATTRIB_BONESWEIGHTS, 4, GL_UNSIGNED_BYTE, GL_TRUE, SKM_MAX_WEIGHTS, 
			( const GLvoid * )vbo->bonesWeightsOffset );
	}
	else {
		GL_EnableVertexAttrib( VATTRIB_BONESINDICES, qfalse );
		GL_EnableVertexAttrib( VATTRIB_BONESWEIGHTS, qfalse );

		// lightmap texture coordinates
		if( vattribs & VATTRIB_LMCOORDS_BIT ) {
			GL_EnableVertexAttrib( VATTRIB_LMCOORDS, qtrue );
			qglVertexAttribPointerARB( VATTRIB_LMCOORDS, 2, GL_FLOAT, GL_FALSE, 0, 
				( const GLvoid * )vbo->lmstOffset[0] );
		}
		else {
			GL_EnableVertexAttrib( VATTRIB_LMCOORDS, qfalse );
		}

		for( i = 0; i < MAX_LIGHTMAPS-1; i++ ) {
			if( vattribs & (VATTRIB_LMCOORDS1_BIT<<i) ) {
				GL_EnableVertexAttrib( VATTRIB_LMCOORDS1+i, qtrue );
				qglVertexAttribPointerARB( VATTRIB_LMCOORDS1+i, 2, GL_FLOAT, GL_FALSE, 0, 
					( const GLvoid * )vbo->lmstOffset[i+1] );
			}
			else {
				GL_EnableVertexAttrib( VATTRIB_LMCOORDS1+i, qfalse );
			}
		}
	}

	if( (vattribs & VATTRIB_INSTANCES_BIT) == VATTRIB_INSTANCES_BIT ) {
		GL_EnableVertexAttrib( VATTRIB_INSTANCE_QUAT, qtrue );
		qglVertexAttribPointerARB( VATTRIB_INSTANCE_QUAT, 4, GL_FLOAT, GL_FALSE, 8 * sizeof( vec_t ), 
			( const GLvoid * )vbo->instancesOffset );
		qglVertexAttribDivisorARB( VATTRIB_INSTANCE_QUAT, 1 );

		GL_EnableVertexAttrib( VATTRIB_INSTANCE_XYZS, qtrue );
		qglVertexAttribPointerARB( VATTRIB_INSTANCE_XYZS, 4, GL_FLOAT, GL_FALSE, 8 * sizeof( vec_t ), 
			( const GLvoid * )( vbo->instancesOffset + sizeof( vec_t ) * 4 ) );
		qglVertexAttribDivisorARB( VATTRIB_INSTANCE_XYZS, 1 );
	} else {
		GL_EnableVertexAttrib( VATTRIB_INSTANCE_QUAT, qfalse );
		GL_EnableVertexAttrib( VATTRIB_INSTANCE_XYZS, qfalse );
	}
}

/*
* RB_DrawElementsReal
*/
void RB_DrawElementsReal( void )
{
	int firstVert, numVerts, firstElem, numElems;
	int numInstances;

	if( ! ( r_drawelements->integer || rb.currentEntity == &rb.nullEnt || ri.refdef.rdflags & RDF_NOWORLDMODEL ) )
		return;

	numVerts = rb.drawElements.numVerts;
	numElems = rb.drawElements.numElems;
	firstVert = rb.drawElements.firstVert;
	firstElem = rb.drawElements.firstElem;
	numInstances = rb.drawElements.numInstances;

	if( numInstances ) {
		if( glConfig.ext.instanced_arrays ) {
			// the instance data is contained in vertex attributes
			qglDrawElementsInstancedARB( rb.primitive, numElems, GL_UNSIGNED_INT, 
				(GLvoid *)(firstElem * sizeof( int )), numInstances );

			rb.stats.c_totalDraws++;
		} else if( glConfig.ext.draw_instanced ) {
			int i, numUInstances = 0;

			// manually update uniform values for instances for currently bound program,
			// respecting the MAX_GLSL_UNIFORM_INSTANCES limit
			for( i = 0; i < numInstances; i += numUInstances ) {
				numUInstances = min( numInstances - i, MAX_GLSL_UNIFORM_INSTANCES );

				RB_SetInstanceData( numUInstances, rb.drawInstances + i );

				qglDrawElementsInstancedARB( rb.primitive, numElems, GL_UNSIGNED_INT, 
					(GLvoid *)(firstElem * sizeof( int )), numUInstances );

				rb.stats.c_totalDraws++;
			}
		} else {
			int i;

			// manually update uniform values for instances for currently bound program,
			// one by one
			for( i = 0; i < numInstances; i++ ) {
				RB_SetInstanceData( 1, rb.drawInstances + i );

				qglDrawRangeElementsEXT( rb.primitive, 
					firstVert, firstVert + numVerts - 1, numElems, 
					GL_UNSIGNED_INT, (GLvoid *)(firstElem * sizeof( int )) );

				rb.stats.c_totalDraws++;
			}
		}
	}
	else {
		numInstances = 1;

		qglDrawRangeElementsEXT( rb.primitive, 
			firstVert, firstVert + numVerts - 1, numElems, 
			GL_UNSIGNED_INT, (GLvoid *)(firstElem * sizeof( int )) );

		rb.stats.c_totalDraws++;
	}

	rb.stats.c_totalVerts += numVerts * numInstances;
	if( rb.primitive == GL_TRIANGLES ) {
		rb.stats.c_totalTris += numElems * numInstances / 3;
	}
}

/*
* RB_GetVertexAttribs
*/
vattribmask_t RB_GetVertexAttribs( void )
{
	return rb.currentVAttribs;
}

/*
* RB_DrawElements_
*/
static void RB_DrawElements_( int firstVert, int numVerts, int firstElem, int numElems )
{
	if( !numVerts || !numElems ) {
		return;
	}

	assert( rb.currentShader != NULL );

	Matrix4_Multiply( rb.projectionMatrix, rb.modelviewMatrix, rb.modelviewProjectionMatrix );

	rb.drawElements.numVerts = numVerts;
	rb.drawElements.numElems = numElems;
	rb.drawElements.firstVert = firstVert;
	rb.drawElements.firstElem = firstElem;

	RB_EnableVertexAttribs();

	if( rb.triangleOutlines ) {
		if( !qglPolygonMode ) {
			// OpenGL ES systems don't support glPolygonMode
			return;
		}
		RB_DrawOutlinedElements();
	} else {
		RB_DrawShadedElements();
	}
}

/*
* RB_DrawElements_
*/
void RB_DrawElements( int firstVert, int numVerts, int firstElem, int numElems )
{
	rb.currentVAttribs &= ~VATTRIB_INSTANCES_BIT;
	rb.drawElements.numInstances = 0;
	RB_DrawElements_( firstVert, numVerts, firstElem, numElems );
}

/*
* RB_DrawElementsInstanced
*
* Draws <numInstances> instances of elements
*/
void RB_DrawElementsInstanced( int firstVert, int numVerts, int firstElem, int numElems, 
	int numInstances, const instancePoint_t *instances )
{
	if( !numInstances ) {
		return;
	}

	// check for vertex-attrib-divisor style instancing
	if( glConfig.ext.instanced_arrays ) {
		// upload instances
		if( rb.currentVBOId < RB_VBO_NONE ) {
			rb.currentVAttribs |= VATTRIB_INSTANCES_BIT;

			// FIXME: this is nasty!
			while( numInstances > MAX_STREAM_VBO_INSTANCES ) {
				R_UploadVBOInstancesData( rb.currentVBO, 0, MAX_STREAM_VBO_INSTANCES, instances );

				rb.drawElements.numInstances = MAX_STREAM_VBO_INSTANCES;
				RB_DrawElements_( firstVert, numVerts, firstElem, numElems );

				instances += MAX_STREAM_VBO_INSTANCES;
				numInstances -= MAX_STREAM_VBO_INSTANCES;
			}

			if( !numInstances ) {
				return;
			}

			R_UploadVBOInstancesData( rb.currentVBO, 0, numInstances, instances );
		} else if( rb.currentVBO->instancesOffset ) {
			// static VBO's must come with their own set of instance data
			rb.currentVAttribs |= VATTRIB_INSTANCES_BIT;
		}
	}

	if( !( rb.currentVAttribs & VATTRIB_INSTANCES_BIT ) ) {
		// can't use instanced arrays so we'll have to manually update
		// the uniform state in between draw calls
		if( rb.maxDrawInstances < numInstances ) {
			if( rb.drawInstances ) {
				RB_Free( rb.drawInstances );
			}
			rb.drawInstances = RB_Alloc( numInstances * sizeof( *rb.drawInstances ) );
			rb.maxDrawInstances = numInstances;
		}
		memcpy( rb.drawInstances, instances, numInstances * sizeof( *instances ) );
	}

	rb.drawElements.numInstances = numInstances;
	RB_DrawElements_( firstVert, numVerts, firstElem, numElems );
}

/*
* RB_EnableTriangleOutlines
*
* Returns triangle outlines state before the call
*/
qboolean RB_EnableTriangleOutlines( qboolean enable )
{
	qboolean oldVal = rb.triangleOutlines;

	if( rb.triangleOutlines != enable ) {
		rb.triangleOutlines = enable;

		// OpenGL ES systems don't support glPolygonMode
		// so check whether the function is actually present
		if( qglPolygonMode ) {
			if( enable ) {
				RB_SetShaderStateMask( 0, GLSTATE_NO_DEPTH_TEST );
				qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			}
			else {
				RB_SetShaderStateMask( ~0, 0 );
				qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			}
		}
	}

	return oldVal;
}
