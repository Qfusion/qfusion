/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2013 Victor Luchits

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

// r_main.c

#include "r_local.h"

r_globals_t rf;

mapconfig_t mapConfig;

refinst_t rn;

r_scene_t rsc;

const elem_t r_boxedges[24] = {
	0, 1, 0, 2, 1, 3, 2, 3,
	4, 5, 4, 6, 5, 7, 6, 7,
	0, 4, 1, 5, 2, 6, 3, 7,
};

/*
* R_TransformBounds
*/
void R_TransformBounds( const vec3_t origin, const mat3_t axis, vec3_t mins, vec3_t maxs, vec3_t bbox[8] ) {
	int i;
	vec3_t tmp;
	mat3_t axis_;

	Matrix3_Transpose( axis, axis_ );   // switch row-column order

	// rotate local bounding box and compute the full bounding box
	for( i = 0; i < 8; i++ ) {
		vec_t *corner = bbox[i];

		corner[0] = ( ( i & 1 ) ? mins[0] : maxs[0] );
		corner[1] = ( ( i & 2 ) ? mins[1] : maxs[1] );
		corner[2] = ( ( i & 4 ) ? mins[2] : maxs[2] );

		Matrix3_TransformVector( axis_, corner, tmp );
		VectorAdd( tmp, origin, corner );
	}
}

/*
* R_TransformForWorld
*/
void R_TransformForWorld( void ) {
	Matrix4_Identity( rn.objectMatrix );
	Matrix4_Copy( rn.cameraMatrix, rn.modelviewMatrix );

	RB_LoadObjectMatrix( mat4x4_identity );
}

/*
* R_TranslateForEntity
*/
void R_TranslateForEntity( const entity_t *e ) {
	Matrix4_ObjectMatrix( e->origin, axis_identity, e->scale, rn.objectMatrix );
	Matrix4_MultiplyFast( rn.cameraMatrix, rn.objectMatrix, rn.modelviewMatrix );

	RB_LoadObjectMatrix( rn.objectMatrix );
}

/*
* R_TransformForEntity
*/
void R_TransformForEntity( const entity_t *e ) {
	if( e->rtype != RT_MODEL ) {
		R_TransformForWorld();
		return;
	}
	if( e == rsc.worldent ) {
		R_TransformForWorld();
		return;
	}

	Matrix4_ObjectMatrix( e->origin, e->axis, e->scale, rn.objectMatrix );
	Matrix4_MultiplyFast( rn.cameraMatrix, rn.objectMatrix, rn.modelviewMatrix );

	RB_LoadObjectMatrix( rn.objectMatrix );
}

/*
* R_FogForBounds
*/
mfog_t *R_FogForBounds( const vec3_t mins, const vec3_t maxs ) {
	unsigned int i, j;
	mfog_t *fog;

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) || !rsh.worldBrushModel->numfogs ) {
		return NULL;
	}
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return NULL;
	}
	if( rsh.worldBrushModel->globalfog ) {
		return rsh.worldBrushModel->globalfog;
	}

	fog = rsh.worldBrushModel->fogs;
	for( i = 0; i < rsh.worldBrushModel->numfogs; i++, fog++ ) {
		if( !fog->shader ) {
			continue;
		}

		for( j = 0; j < 3; j++ ) {
			if( mins[j] >= fog->maxs[j] ) {
				break;
			}
			if( maxs[j] <= fog->mins[j] ) {
				break;
			}
		}

		if( j == 3 ) {
			return fog;
		}
	}

	return NULL;
}

/*
* R_FogForSphere
*/
mfog_t *R_FogForSphere( const vec3_t centre, const float radius ) {
	int i;
	vec3_t mins, maxs;

	for( i = 0; i < 3; i++ ) {
		mins[i] = centre[i] - radius;
		maxs[i] = centre[i] + radius;
	}

	return R_FogForBounds( mins, maxs );
}

/*
* R_ComputeLOD
*/
int R_ComputeLOD( float dist, float lodDistance, float lodScale, int lodBias ) {
	int lodi;
	float lod;

	if( lodDistance < 1.0f )
		lodDistance = 1.0f;

	//dist *= tan( DEG2RAD( rn.refdef.fov_x ) * 0.5f );

	if( dist <= lodDistance ) {
		lod = 0;
	} else {
		lod = sqrt( dist / lodDistance ) * lodScale;
	}

	lodi = (int)( lod + 0.5f ) + lodBias;
	return clamp_low( lodi, 0 );
}

/*
=============================================================

CUSTOM COLORS

=============================================================
*/

/*
* R_InitCustomColors
*/
void R_InitCustomColors( void ) {
	memset( rsh.customColors, 255, sizeof( rsh.customColors ) );
}

/*
* R_SetCustomColor
*/
void R_SetCustomColor( int num, int r, int g, int b ) {
	if( num < 0 || num >= NUM_CUSTOMCOLORS ) {
		return;
	}
	Vector4Set( rsh.customColors[num], (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 );
}
/*
* R_GetCustomColor
*/
int R_GetCustomColor( int num ) {
	if( num < 0 || num >= NUM_CUSTOMCOLORS ) {
		return COLOR_RGBA( 255, 255, 255, 255 );
	}
	return *(int *)rsh.customColors[num];
}

/*
* R_ShutdownCustomColors
*/
void R_ShutdownCustomColors( void ) {
	memset( rsh.customColors, 255, sizeof( rsh.customColors ) );
}

/*
=============================================================

SPRITE MODELS

=============================================================
*/

static drawSurfaceType_t spriteDrawSurf = ST_SPRITE;

/*
* R_BatchSpriteSurf
*/
void R_BatchSpriteSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, int lightStyleNum, 
	const portalSurface_t *portalSurface, drawSurfaceType_t *drawSurf, bool mergable ) {
	int i;
	vec3_t point;
	vec3_t v_left, v_up;
	elem_t elems[6] = { 0, 1, 2, 0, 2, 3 };
	vec4_t xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
	vec4_t normals[4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
	byte_vec4_t colors[4];
	vec2_t texcoords[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
	mesh_t mesh;
	float radius = e->radius * e->scale;
	float rotation = e->rotation;

	if( rotation ) {
		RotatePointAroundVector( v_left, &rn.viewAxis[AXIS_FORWARD], &rn.viewAxis[AXIS_RIGHT], rotation );
		CrossProduct( &rn.viewAxis[AXIS_FORWARD], v_left, v_up );
	} else {
		VectorCopy( &rn.viewAxis[AXIS_RIGHT], v_left );
		VectorCopy( &rn.viewAxis[AXIS_UP], v_up );
	}

	if( rn.renderFlags & ( RF_MIRRORVIEW | RF_FLIPFRONTFACE ) ) {
		VectorInverse( v_left );
	}

	VectorMA( e->origin, -radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[0] );
	VectorMA( point, -radius, v_left, xyz[3] );

	VectorMA( e->origin, radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[1] );
	VectorMA( point, -radius, v_left, xyz[2] );

	for( i = 0; i < 4; i++ ) {
		VectorNegate( &rn.viewAxis[AXIS_FORWARD], normals[i] );
		Vector4Copy( e->color, colors[i] );
	}

	mesh.elems = elems;
	mesh.numElems = 6;
	mesh.numVerts = 4;
	mesh.xyzArray = xyz;
	mesh.normalsArray = normals;
	mesh.lmstArray[0] = NULL;
	mesh.lmlayersArray[0] = NULL;
	mesh.stArray = texcoords;
	mesh.colorsArray[0] = colors;
	mesh.colorsArray[1] = NULL;
	mesh.sVectorsArray = NULL;

	RB_AddDynamicMesh( e, shader, fog, portalSurface, &mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_CacheSpriteEntity
*/
void R_CacheSpriteEntity( const entity_t *e ) {
	int i;
	float corner;
	entSceneCache_t *cache = R_ENTCACHE( e );

	cache->rotated = true;
	cache->radius = e->radius;
	cache->fog = R_FogForSphere( e->origin, e->radius );

	corner = e->radius * 1.74;
	for( i = 0; i < 3; i++ ) {
		cache->mins[i] = e->origin[i] - corner;
		cache->maxs[i] = e->origin[i] + corner;
	}
}

/*
* R_AddSpriteToDrawList
*/
static bool R_AddSpriteToDrawList( const entity_t *e ) {
	float dist;
	const shader_t *shader = e->customShader;

	dist =
		( e->origin[0] - rn.refdef.vieworg[0] ) * rn.viewAxis[AXIS_FORWARD + 0] +
		( e->origin[1] - rn.refdef.vieworg[1] ) * rn.viewAxis[AXIS_FORWARD + 1] +
		( e->origin[2] - rn.refdef.vieworg[2] ) * rn.viewAxis[AXIS_FORWARD + 2];

	if( dist <= 0 ) {
		return false; // cull it because we don't want to sort unneeded things
	}

	if( ( rn.renderFlags & RF_SHADOWMAPVIEW ) && R_ShaderNoShadow( shader ) ) {
		return false;
	}

	if( !R_AddSurfToDrawList( rn.meshlist, e, shader, 
		R_FogForSphere( e->origin, e->radius ), -1, dist, 0, NULL, &spriteDrawSurf ) ) {
		return false;
	}

	return true;
}

//==================================================================================

static drawSurfaceType_t nullDrawSurf = ST_NULLMODEL;

/*
* R_InitNullModelVBO
*/
mesh_vbo_t *R_InitNullModelVBO( void ) {
	float scale = 15;
	vec4_t xyz[6] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
	vec4_t normals[6] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
	byte_vec4_t colors[6];
	vec2_t texcoords[6] = { {0,0}, {0,1}, {0,0}, {0,1}, {0,0}, {0,1} };
	elem_t elems[6] = { 0, 1, 2, 3, 4, 5 };
	mesh_t mesh;
	vattribmask_t vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT | VATTRIB_COLOR0_BIT;
	mesh_vbo_t *vbo;

	vbo = R_CreateMeshVBO( &rf, 6, 6, 0, vattribs, VBO_TAG_NONE, vattribs );
	if( !vbo ) {
		return NULL;
	}

	VectorSet( xyz[0], 0, 0, 0 );
	VectorSet( xyz[1], scale, 0, 0 );
	Vector4Set( colors[0], 255, 0, 0, 127 );
	Vector4Set( colors[1], 255, 0, 0, 127 );

	VectorSet( xyz[2], 0, 0, 0 );
	VectorSet( xyz[3], 0, scale, 0 );
	Vector4Set( colors[2], 0, 255, 0, 127 );
	Vector4Set( colors[3], 0, 255, 0, 127 );

	VectorSet( xyz[4], 0, 0, 0 );
	VectorSet( xyz[5], 0, 0, scale );
	Vector4Set( colors[4], 0, 0, 255, 127 );
	Vector4Set( colors[5], 0, 0, 255, 127 );

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = 6;
	mesh.xyzArray = xyz;
	mesh.normalsArray = normals;
	mesh.stArray = texcoords;
	mesh.colorsArray[0] = colors;
	mesh.numElems = 6;
	mesh.elems = elems;

	R_UploadVBOVertexData( vbo, 0, vattribs, &mesh, 0 );
	R_UploadVBOElemData( vbo, 0, 0, &mesh );

	return vbo;
}

/*
* R_DrawNullSurf
*/
void R_DrawNullSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, int lightStyleNum, 
	const portalSurface_t *portalSurface, drawSurfaceType_t *drawSurf ) {
	assert( rsh.nullVBO != NULL );
	if( !rsh.nullVBO ) {
		return;
	}

	RB_BindVBO( rsh.nullVBO->index, GL_LINES );

	RB_DrawElements( 0, 6, 0, 6 );
}

/*
* R_AddNullSurfToDrawList
*/
static bool R_AddNullSurfToDrawList( const entity_t *e ) {
	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return false;
	}

	if( !R_AddSurfToDrawList( rn.meshlist, e, rsh.whiteShader, 
		R_FogForSphere( e->origin, 0.1f ), -1, 0, 0, NULL, &nullDrawSurf ) ) {
		return false;
	}

	return true;
}

//==================================================================================

static vec4_t pic_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec4_t pic_normals[4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
static vec2_t pic_st[4];
static byte_vec4_t pic_colors[4];
static elem_t pic_elems[6] = { 0, 1, 2, 0, 2, 3 };
static mesh_t pic_mesh = { 4, 6, pic_elems, pic_xyz, pic_normals, NULL, pic_st, { 0, 0, 0, 0 }, { 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, NULL, NULL };

/*
* R_Begin2D
*
* Note that this sets the viewport to size of the active framebuffer.
*/
void R_Begin2D( bool multiSamples ) {
	int width, height;

	width = rf.frameBufferWidth;
	height = rf.frameBufferHeight;

	if( rf.twoD.enabled == true ) {
		if( width == rf.twoD.width && height == rf.twoD.height && multiSamples == rf.twoD.multiSamples ) {
			return;
		}
	}

	RB_FlushDynamicMeshes();

	if( multiSamples )
		R_BindFrameBufferObject( rf.renderTarget );
	else
		R_BindFrameBufferObject( 0 );

	rf.twoD.enabled = true;
	rf.twoD.width = rf.frameBufferWidth;
	rf.twoD.height = rf.frameBufferHeight;
	rf.twoD.multiSamples = multiSamples;

	RB_EnableTriangleOutlines( multiSamples && ( r_showtris2D->integer != 0 ) );

	R_SetupGL2D();
}

/*
* R_SetupGL2D
*
* Note that this sets the viewport to size of the active framebuffer.
*/
void R_SetupGL2D( void ) {
	int width, height;
	/*ATTRIBUTE_ALIGNED( 16 ) */mat4_t projectionMatrix;

	width = rf.twoD.width;
	height = rf.twoD.height;

	Matrix4_OrthoProjection( 0, width, height, 0, -1, 1, projectionMatrix );

	// set 2D virtual screen size
	RB_Scissor( 0, 0, width, height );
	RB_Viewport( 0, 0, width, height );

	RB_SetZClip( Z_NEAR, 1024 );
	RB_SetCamera( vec3_origin, axis_identity );

	RB_LoadCameraMatrix( mat4x4_identity );

	if( rf.transformMatrixStackSize[0] > 0 )
		RB_LoadObjectMatrix( rf.transformMatricesStack[0][rf.transformMatrixStackSize[0] - 1] );
	else
		RB_LoadObjectMatrix( mat4x4_identity );

	if( rf.transformMatrixStackSize[1] > 0 )
		RB_LoadProjectionMatrix( rf.transformMatricesStack[1][rf.transformMatrixStackSize[1] - 1] );
	else
		RB_LoadProjectionMatrix( projectionMatrix );

	RB_SetShaderStateMask( ~0, GLSTATE_NO_DEPTH_TEST );

	RB_SetRtLightParams( 0, NULL, 0, NULL );

	RB_SetRenderFlags( 0 );
}

/*
* R_End2D
*/
void R_End2D( void ) {
	if( rf.twoD.enabled == false ) {
		return;
	}

	rf.twoD.enabled = false;

	// render previously batched 2D geometry, if any
	RB_FlushDynamicMeshes();

	RB_EnableTriangleOutlines( false );

	RB_SetShaderStateMask( ~0, 0 );

	RB_SetRenderFlags( 0 );
}

/*
* R_DrawRotatedStretchPic
*/
void R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle,
							  const vec4_t color, const shader_t *shader ) {
	int bcolor;

	if( !shader ) {
		return;
	}

	if( shader->cin ) {
		R_UploadCinematicShader( shader );
	}

	// lower-left
	Vector2Set( pic_xyz[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Set( pic_colors[0],
				Q_bound( 0, ( int )( color[0] * 255.0f ), 255 ),
				Q_bound( 0, ( int )( color[1] * 255.0f ), 255 ),
				Q_bound( 0, ( int )( color[2] * 255.0f ), 255 ),
				Q_bound( 0, ( int )( color[3] * 255.0f ), 255 ) );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	Vector2Set( pic_xyz[1], x + w, y );
	Vector2Set( pic_st[1], s2, t1 );
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	Vector2Set( pic_xyz[2], x + w, y + h );
	Vector2Set( pic_st[2], s2, t2 );
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	Vector2Set( pic_xyz[3], x, y + h );
	Vector2Set( pic_st[3], s1, t2 );
	*(int *)pic_colors[3] = bcolor;

	// rotated image
	angle = anglemod( angle );
	if( angle ) {
		int j;
		float sint, cost;

		angle = DEG2RAD( angle );
		sint = sin( angle );
		cost = cos( angle );

		for( j = 0; j < 4; j++ ) {
			t1 = pic_st[j][0];
			t2 = pic_st[j][1];
			pic_st[j][0] = cost * ( t1 - 0.5f ) - sint * ( t2 - 0.5f ) + 0.5f;
			pic_st[j][1] = cost * ( t2 - 0.5f ) + sint * ( t1 - 0.5f ) + 0.5f;
		}
	}

	RB_AddDynamicMesh( NULL, shader, NULL, NULL, &pic_mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_DrawStretchPic
*/
void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
					   const vec4_t color, const shader_t *shader ) {
	R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, 0, color, shader );
}

/*
* R_UploadRawPic
*/
void R_UploadRawPic( image_t *texture, int cols, int rows, uint8_t *data ) {
	if( texture->width != cols || texture->height != rows ) {
		uint8_t *nodata[1] = { NULL };
		R_ReplaceImage( texture, nodata, cols, rows, texture->flags, 1, 3 );
	}
	R_ReplaceSubImage( texture, 0, 0, 0, &data, cols, rows );
}

/*
* R_UploadRawYUVPic
*/
void R_UploadRawYUVPic( image_t **yuvTextures, ref_img_plane_t *yuv ) {
	int i;

	for( i = 0; i < 3; i++ ) {
		uint8_t *data = yuv[i].data;
		int flags = yuvTextures[i]->flags;
		int stride = yuv[i].stride;
		int height = yuv[i].height;

		if( stride < 0 ) {
			// negative stride flips the image vertically
			data = data + stride * height;
			flags = ( flags & ~( IT_FLIPX | IT_FLIPY | IT_FLIPDIAGONAL ) ) | IT_FLIPY;
			stride = -stride;
		}

		if( yuvTextures[i]->width != stride || yuvTextures[i]->height != height ) {
			uint8_t *nodata[1] = { NULL };
			R_ReplaceImage( yuvTextures[i], nodata, stride, height, flags, 1, 1 );
		}
		R_ReplaceSubImage( yuvTextures[i], 0, 0, 0, &data, stride, height );
	}
}

/*
* R_DrawStretchRaw
*/
void R_DrawStretchRaw( int x, int y, int w, int h, float s1, float t1, float s2, float t2 ) {
	float h_scale, v_scale;

	h_scale = (float)rsh.rawTexture->width / rsh.rawTexture->upload_width;
	v_scale = (float)rsh.rawTexture->height / rsh.rawTexture->upload_height;
	s1 *= h_scale;
	s2 *= h_scale;
	t1 *= v_scale;
	t2 *= v_scale;

	R_DrawStretchQuick( x, y, w, h, s1, t1, s2, t2, colorWhite, GLSL_PROGRAM_TYPE_NONE, rsh.rawTexture, 0 );
}

/*
* R_DrawStretchRawYUVBuiltin
*
* Set bit 0 in 'flip' to flip the image horizontally
* Set bit 1 in 'flip' to flip the image vertically
*/
void R_DrawStretchRawYUVBuiltin( int x, int y, int w, int h,
								 float s1, float t1, float s2, float t2, image_t **yuvTextures, int flip ) {
	static char *s_name = "$builtinyuv";
	static shaderpass_t p;
	static shader_t s;
	float h_scale, v_scale;
	float s2_, t2_;
	float h_ofs, v_ofs;

	s.vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT;
	s.sort = SHADER_SORT_NEAREST;
	s.numpasses = 1;
	s.name = s_name;
	s.passes = &p;

	p.rgbgen.type = RGB_GEN_IDENTITY;
	p.alphagen.type = ALPHA_GEN_IDENTITY;
	p.tcgen = TC_GEN_BASE;
	p.images[0] = yuvTextures[0];
	p.images[1] = yuvTextures[1];
	p.images[2] = yuvTextures[2];
	p.flags = 0;
	p.program_type = GLSL_PROGRAM_TYPE_YUV;

	h_scale = (float)yuvTextures[0]->width / yuvTextures[0]->upload_width;
	v_scale = (float)yuvTextures[0]->height / yuvTextures[0]->upload_height;
	h_ofs = 1.0f / yuvTextures[0]->upload_width;
	v_ofs = 1.0f / yuvTextures[0]->upload_height;

	s1 *= h_scale;
	s2 *= h_scale;
	t1 *= v_scale;
	t2 *= v_scale;

	s2_ = s2;
	t2_ = t2;
	if( flip & 1 ) {
		s1 = s2_ - s1, s2 = s2_ - s2;
	}
	if( flip & 2 ) {
		t1 = t2_ - t1, t2 = t2_ - t2;
	}

	// avoid lerp seams
	if( s1 > s2 ) {
		s1 -= h_ofs, s2 += h_ofs;
	} else {
		s1 += h_ofs, s2 -= h_ofs;
	}
	if( t1 > t2 ) {
		t1 -= v_ofs, t2 += v_ofs;
	} else {
		t1 += v_ofs, t2 -= v_ofs;
	}

	R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, 0, colorWhite, &s );
}

/*
* R_DrawStretchRawYUV
*/
void R_DrawStretchRawYUV( int x, int y, int w, int h, float s1, float t1, float s2, float t2 ) {
	R_DrawStretchRawYUVBuiltin( x, y, w, h, s1, t1, s2, t2, rsh.rawYUVTextures, 0 );
}

/*
* R_DrawStretchQuick
*/
void R_DrawStretchQuick( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
						 const vec4_t color, int program_type, image_t *image, int blendMask ) {
	static char *s_name = "$builtinimage";
	static shaderpass_t p;
	static shader_t s;
	static float rgba[4];

	s.vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT;
	s.sort = SHADER_SORT_NEAREST;
	s.numpasses = 1;
	s.name = s_name;
	s.passes = &p;

	Vector4Copy( color, rgba );
	p.rgbgen.type = RGB_GEN_CONST;
	p.rgbgen.args = rgba;
	p.alphagen.type = ALPHA_GEN_CONST;
	p.alphagen.args = &rgba[3];
	p.tcgen = TC_GEN_BASE;
	p.images[0] = image;
	p.flags = blendMask;
	p.program_type = program_type;

	R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, 0, color, &s );

	RB_FlushDynamicMeshes();
}

/*
* R_BindFrameBufferObject
*/
void R_BindFrameBufferObject( int object ) {
	int width, height;

	RFB_GetObjectSize( object, &width, &height );

	rf.frameBufferWidth = width;
	rf.frameBufferHeight = height;

	RB_BindFrameBufferObject( object );
}

/*
* R_Scissor
*
* Set scissor region for 2D drawing.
* x and y represent the top left corner of the region/rectangle.
*/
void R_Scissor( int x, int y, int w, int h ) {
	RB_Scissor( x, y, w, h );
}

/*
* R_GetScissor
*/
void R_GetScissor( int *x, int *y, int *w, int *h ) {
	RB_GetScissor( x, y, w, h );
}

/*
* R_ResetScissor
*/
void R_ResetScissor( void ) {
	RB_Scissor( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
}

/*
* R_PolyBlend
*/
static void R_PolyBlend( void ) {
	if( !r_polyblend->integer ) {
		return;
	}
	if( rsc.refdef.blend[3] < 0.01f ) {
		return;
	}

	R_DrawStretchPic( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight, 0, 0, 1, 1, rsc.refdef.blend, rsh.whiteShader );
	RB_FlushDynamicMeshes();
}

/*
* R_InitPostProcessingVBO
*/
mesh_vbo_t *R_InitPostProcessingVBO( void ) {
	vec4_t xyz[4] = { {0,0,0,1}, {1,0,0,1}, {1,1,0,1}, {0,1,0,1} };
	vec2_t texcoords[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
	elem_t elems[6] = { 0, 1, 2, 0, 2, 3 };
	mesh_t mesh;
	vattribmask_t vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT;
	mesh_vbo_t *vbo;

	vbo = R_CreateMeshVBO( &rf, 4, 6, 0, vattribs, VBO_TAG_NONE, vattribs );
	if( !vbo ) {
		return NULL;
	}

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = 4;
	mesh.xyzArray = xyz;
	mesh.stArray = texcoords;
	mesh.numElems = 6;
	mesh.elems = elems;

	R_UploadVBOVertexData( vbo, 0, vattribs, &mesh, 0 );
	R_UploadVBOElemData( vbo, 0, 0, &mesh );

	return vbo;
}

//=======================================================================

/*
* R_DefaultFarClip
*/
float R_DefaultFarClip( void ) {
	float farclip_dist;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		farclip_dist = 0;
	} else if( rn.refdef.rdflags & RDF_NOWORLDMODEL ) {
		farclip_dist = 1024;
	} else if( rsh.worldModel && rsh.worldBrushModel->globalfog ) {
		farclip_dist = rsh.worldBrushModel->globalfog->shader->fog_dist;
	} else {
		farclip_dist = Z_NEAR;
	}

	return max( Z_NEAR, farclip_dist ) + Z_BIAS;
}

/*
* R_SetupPVSFromCluster
*/
void R_SetupPVSFromCluster( int cluster, int area ) {
	uint8_t *pvs = NULL;
	uint8_t *areabits = NULL;
	int arearowbytes, areabytes;

	if( !rsh.worldBrushModel || !rsh.worldBrushModel->pvs ) {
		rn.viewcluster = CLUSTER_INVALID;
		rn.viewarea = -1;
		rn.pvs = NULL;
		rn.areabits = NULL;
		return;
	}

	// current viewcluster
	pvs = Mod_ClusterPVS( cluster, rsh.worldBrushModel );

	arearowbytes = ( ( rsh.worldBrushModel->numareas + 7 ) / 8 );
	areabytes = arearowbytes;
#ifdef AREAPORTALS_MATRIX
	areabytes *= rsh.worldBrushModel->numareas;
#endif

	if( area > -1 && rn.refdef.areabits && area < rsh.worldBrushModel->numareas )
#ifdef AREAPORTALS_MATRIX
	{ areabits = rn.refdef.areabits + area * arearowbytes;}
#else
	{ areabits = rn.refdef.areabits;}
#endif
	else {
		areabits = NULL;
	}

	rn.viewcluster = cluster;
	rn.viewarea = area;
	rn.pvs = pvs;
	rn.areabits = areabits;
}

/*
* R_SetupPVS
*/
void R_SetupPVS( const refdef_t *fd ) {
	const mleaf_t *leaf;

	if( ( fd->rdflags & RDF_NOWORLDMODEL ) || !rsh.worldBrushModel || r_novis->integer ) {
		R_SetupPVSFromCluster( -1, -1 );
		return;
	}

	leaf = Mod_PointInLeaf( fd->vieworg, rsh.worldBrushModel );
	R_SetupPVSFromCluster( leaf->cluster, leaf->area );
}

/*
* R_SetCameraAndProjectionMatrices
*/
void R_SetCameraAndProjectionMatrices( const mat4_t cam, const mat4_t proj ) {
	Matrix4_Copy( cam, rn.cameraMatrix );
	Matrix4_Copy( proj, rn.projectionMatrix );
	Matrix4_Multiply( proj, cam, rn.cameraProjectionMatrix );
}

/*
* R_SetupViewMatrices_
*/
static void R_SetupViewMatrices_( const refdef_t *rd, const mat4_t camTransform ) {
	mat4_t cam, cam_, proj;

	Matrix4_Modelview( rd->vieworg, rd->viewaxis, cam );

	if( rd->rdflags & RDF_USEORTHO ) {
		Matrix4_OrthoProjection( -rd->ortho_x, rd->ortho_x, -rd->ortho_y, rd->ortho_y,
			-rn.farClip, rn.farClip, proj );
	} else {
		Matrix4_PerspectiveProjection( rd->fov_x, rd->fov_y,
			rn.nearClip, rn.farClip, rf.cameraSeparation, proj );
	}

	if( rd->rdflags & RDF_FLIPPED ) {
		proj[0] = -proj[0];
		rn.renderFlags |= RF_FLIPFRONTFACE;
	}

	Matrix4_Multiply( camTransform, cam, cam_ );
	R_SetCameraAndProjectionMatrices( cam_, proj );
}

/*
* R_SetupViewMatrices
*/
void R_SetupViewMatrices( const refdef_t *rd ) {
	const mat4_t flip = { 
		0, 0, -1, 0,
		-1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 0, 1
	};

	R_SetupViewMatrices_( rd, flip );
}

/*
* R_SetupSideViewMatrices
*/
void R_SetupSideViewMatrices( const refdef_t *rd, int side ) {
	const mat4_t rectviewmatrix[6] =
	{
		// sign-preserving cubemap projections
		{ // +X
			0, 0,-1, 0,
			0, 1, 0, 0,
			1, 0, 0, 0,
			0, 0, 0, 1,
		},
		{ // -X
			0, 0, 1, 0,
			0, 1, 0, 0,
			1, 0, 0, 0,
			0, 0, 0, 1,
		},
		{ // +Y
			1, 0, 0, 0,
			0, 0,-1, 0,
			0, 1, 0, 0,
			0, 0, 0, 1,
		},
		{ // -Y
			1, 0, 0, 0,
			0, 0, 1, 0,
			0, 1, 0, 0,
			0, 0, 0, 1,
		},
		{ // +Z
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0,-1, 0,
			0, 0, 0, 1,
		},
		{ // -Z
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1,
		},
	};

	assert( side >= 0 && side < 6 );
	if( side < 0 || side >= 6 ) {
		return;
	}

	R_SetupViewMatrices_( rd, rectviewmatrix[side] );
}

/*
* R_Clear
*/
static void R_Clear( int bitMask ) {
	int fbo;
	int bits;
	vec4_t envColor;
	bool clearColor = false;
	bool rgbShadow = ( rn.renderFlags & ( RF_SHADOWMAPVIEW | RF_SHADOWMAPVIEW_RGB ) ) == ( RF_SHADOWMAPVIEW | RF_SHADOWMAPVIEW_RGB );
	bool depthPortal = ( rn.renderFlags & ( RF_MIRRORVIEW | RF_PORTALVIEW ) ) != 0 && ( rn.renderFlags & RF_PORTAL_CAPTURE ) == 0;

	if( depthPortal ) {
		return;
	}
	if( rn.renderFlags & RF_LIGHTVIEW ) {
		return;
	}
	if( rn.renderFlags & RF_SKYSHADOWVIEW ) {
		// R_DrawPortals has already set up the depth buffer
		return;
	}

	fbo = RB_BoundFrameBufferObject();

	if( rgbShadow ) {
		clearColor = true;
		Vector4Set( envColor, 1, 1, 1, 1 );
	} else if( rn.refdef.rdflags & RDF_NOWORLDMODEL ) {
		clearColor = rn.renderTarget != rf.renderTarget;
		Vector4Set( envColor, 1, 1, 1, 0 );
	} else {
		if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
			clearColor = false;
		} else {
			clearColor = (rn.portalmasklist && !rn.portalmasklist->numDrawSurfs) || R_FASTSKY();
		}

		if( rsh.worldBrushModel && rsh.worldBrushModel->globalfog && rsh.worldBrushModel->globalfog->shader ) {
			Vector4Scale( rsh.worldBrushModel->globalfog->shader->fog_color, 1.0 / 255.0, envColor );
		} else {
			Vector4Scale( mapConfig.environmentColor, 1.0 / 255.0, envColor );
		}
	}

	bits = 0;
	//if( !depthPortal ) {
		bits |= GL_DEPTH_BUFFER_BIT;
	//}
	if( clearColor ) {
		bits |= GL_COLOR_BUFFER_BIT;
	}
	if( RFB_HasStencilRenderBuffer( fbo ) ) {
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	bits &= bitMask;

	RB_Clear( bits, envColor[0], envColor[1], envColor[2], envColor[3] );
}

/*
* R_SetupGL
*/
static void R_SetupGL( void ) {
	RB_Scissor( rn.scissor[0], rn.scissor[1], rn.scissor[2], rn.scissor[3] );
	RB_Viewport( rn.viewport[0], rn.viewport[1], rn.viewport[2], rn.viewport[3] );

	if( rn.renderFlags & RF_CLIPPLANE ) {
		cplane_t *p = &rn.clipPlane;
		Matrix4_ObliqueNearClipping( p->normal, -p->dist, rn.cameraMatrix, rn.projectionMatrix );
	}

	RB_SetZClip( rn.nearClip, rn.farClip );

	RB_SetCamera( rn.viewOrigin, rn.viewAxis );

	RB_SetLightParams( rn.refdef.minLight, ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) != 0, rn.hdrExposure );

	RB_SetRenderFlags( rn.renderFlags );

	RB_LoadProjectionMatrix( rn.projectionMatrix );

	RB_LoadCameraMatrix( rn.cameraMatrix );

	RB_LoadObjectMatrix( mat4x4_identity );

	if( rn.renderFlags & RF_FLIPFRONTFACE ) {
		RB_FlipFrontFace();
	}

	RB_PolygonOffset( rn.polygonFactor, rn.polygonUnits );

	if( rn.renderFlags & RF_LIGHTVIEW ) {
		RB_SetRtLightParams( 1, &rn.rtLight, 0, NULL );
	} else {
		RB_SetRtLightParams( 0, NULL, 0, NULL );
	}

	if( ( rn.renderFlags & RF_SHADOWMAPVIEW ) && glConfig.ext.shadow ) {
		RB_SetShaderStateMask( ~0, GLSTATE_NO_COLORWRITE|GLSTATE_OFFSET_FILL|GLSTATE_DEPTHWRITE );
	} else {
		RB_SetShaderStateMask( ~0, 0 );
	}
}

/*
* R_EndGL
*/
static void R_EndGL( void ) {
	RB_FlushDynamicMeshes();

	if( ( rn.renderFlags & RF_SHADOWMAPVIEW ) && glConfig.ext.shadow ) {
		RB_SetShaderStateMask( ~0, 0 );
	}

	if( rn.renderFlags & RF_FLIPFRONTFACE ) {
		RB_FlipFrontFace();
	}
}

/*
* R_CullEntities
*/
static void R_CullEntities( void ) {
	unsigned int i;
	int entNum;
	entity_t *e;
	entSceneCache_t *cache;

	rn.entities = NULL;
	rn.entpvs = NULL;
	rn.numEntities = 0;

	if( rn.renderFlags & RF_NOENTS ) {
		return;
	}

	rn.entities = R_FrameCache_Alloc( sizeof( *rn.entities ) * rsc.numEntities );
	rn.entpvs = R_FrameCache_Alloc( sizeof( *rn.entpvs ) * (rsc.numEntities+7)/8 );
	memset( rn.entpvs, 0, (rsc.numEntities+7)/8 );

	if( rn.renderFlags & RF_ENVVIEW ) {
		for( i = 0; i < rsc.numBmodelEntities; i++ ) {
			entNum = rsc.bmodelEntities[i];
			e = R_NUM2ENT( entNum );

			if( R_CullModelEntity( e, false ) != 0 ) {
				continue;
			}

			rn.entpvs[entNum>>3] |= (1<<(entNum&7));
			rn.entities[rn.numEntities++] = entNum;
		}
		return;
	}

	if( rn.renderFlags & RF_LIGHTVIEW ) {
		for( i = 0; i < rn.numRtLightEntities; i++ ) {
			entNum = rn.rtLightEntities[i];

			if( !(rn.parent->entpvs[entNum>>3] & (1<<(entNum&7))) ) {
				// not visible in parent view
				continue;
			}

			rn.entpvs[entNum>>3] |= (1<<(entNum&7));
			rn.entities[rn.numEntities++] = entNum;
		}
		return;
	}

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		for( i = 0; i < rn.numRtLightEntities; i++ ) {
			entNum = rn.rtLightEntities[i];
			e = R_NUM2ENT( entNum );
			cache = R_ENTCACHE( e );

			if( R_CullModelEntity( e, false ) ) {
				continue;
			}

			rn.entpvs[entNum>>3] |= (1<<(entNum&7));
			rn.entities[rn.numEntities++] = entNum;
		}
		return;
	}

	for( i = rsc.numLocalEntities; i < rsc.numEntities; i++ ) {
		bool culled;

		entNum = i;
		e = R_NUM2ENT( entNum );
		culled = true;

		if( e->flags & RF_WEAPONMODEL ) {
			if( rn.renderFlags & RF_NONVIEWERREF ) {
				continue;
			}
			goto add;
		}

		if( e->flags & RF_VIEWERMODEL ) {
			//if( !(rn.renderFlags & RF_NONVIEWERREF) )
			if( !( rn.renderFlags & ( RF_MIRRORVIEW ) ) ) {
				continue;
			}
		}

		if( e->flags & RF_NODEPTHTEST ) {
			goto add;
		}

		switch( e->rtype ) {
		case RT_MODEL:
			culled = R_CullModelEntity( e, false ) != 0;
			break;
		case RT_SPRITE:
			culled = R_CullSpriteEntity( e ) != 0;
			break;
		default:
			break;
		}

		if( culled ) {
			continue;
		}

add:
		rn.entpvs[entNum>>3] |= (1<<(entNum&7));
		rn.entities[rn.numEntities++] = entNum;
	}
}

/*
* R_DrawEntities
*/
static void R_DrawEntities( void ) {
	unsigned int i;
	int lod;
	entity_t *e;
	entSceneCache_t *cache;

	for( i = 0; i < rn.numEntities; i++ ) {
		e = R_NUM2ENT( rn.entities[i] );
		cache = R_ENTCACHE( e );

		lod = 0;
		if( !(e->flags & RF_FORCENOLOD ) ) {
			float dist = BoundsNearestDistance( rn.lodOrigin, cache->absmins, cache->absmaxs );
			lod = R_ComputeLOD( dist, cache->radius, rn.lodScale, rn.lodBias );
		}

		switch( e->rtype ) {
		case RT_MODEL:
			switch( cache->mod_type ) {
			case mod_alias:
				R_AddAliasModelToDrawList( e, lod );
				break;
			case mod_skeletal:
				R_AddSkeletalModelToDrawList( e, lod );
				break;
			case mod_brush:
				R_AddBrushModelToDrawList( e );
				rf.stats.c_ents_bmodels++;
				break;
			case mod_bad:
				R_AddNullSurfToDrawList( e );
				break;
			default:
				break;
			}
			break;
		case RT_SPRITE:
			R_AddSpriteToDrawList( e );
			break;
		default:
			break;
		}

		rf.stats.c_ents_total++;
	}
}

//=======================================================================

/*
* R_BindRefInstFBO
*/
static void R_BindRefInstFBO( void ) {
	int fbo = rn.renderTarget;
	R_BindFrameBufferObject( fbo );
}

/*
* R_RenderView
*/
void R_RenderView( const refdef_t *fd ) {
	int msec = 0;
	bool lightOrShadow = rn.renderFlags & (RF_LIGHTVIEW|RF_SHADOWMAPVIEW) ? true : false;

	rf.frameCount++;

	rn.refdef = *fd;

	rn.fog_eye = NULL;
	rn.hdrExposure = 1;

	rn.numRealtimeLights = 0;
	rn.numPortalSurfaces = 0;
	rn.numDepthPortalSurfaces = 0;
	rn.skyportalSurface = NULL;

	ClearBounds( rn.visMins, rn.visMaxs );
	ClearBounds( rn.pvsMins, rn.pvsMaxs );

	VectorCopy( rn.refdef.vieworg, rn.viewOrigin );
	Matrix3_Copy( rn.refdef.viewaxis, rn.viewAxis );

	R_ClearSky( &rn.skyDrawSurface );

	if( r_lightmap->integer ) {
		rn.renderFlags |= RF_LIGHTMAP;
	}

	if( r_drawflat->integer ) {
		rn.renderFlags |= RF_DRAWFLAT;
	}

	R_ClearDrawList( rn.meshlist );

	R_ClearDrawList( rn.portalmasklist );

	if( !rsh.worldModel && !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return;
	}

	// we know the initial farclip at this point after determining visible world leafs
	// R_DrawEntities can make adjustments as well

	if( rn.renderFlags & RF_LIGHTVIEW ) {
		R_DrawWorldShadowNode();
	} else if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		R_DrawRtLightWorld();
	} else {
		R_DrawWorldNode();
	}

	if( !lightOrShadow ) {
		if( !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			rn.fog_eye = R_FogForSphere( rn.viewOrigin, 0.5 );
			rn.hdrExposure = R_LightExposureForOrigin( rn.viewOrigin );
		}

		R_DrawCoronas();

		if( r_speeds->integer ) {
			msec = ri.Sys_Milliseconds();
		}
		R_DrawPolys();
		if( r_speeds->integer ) {
			rf.stats.t_add_polys += ( ri.Sys_Milliseconds() - msec );
		}
	}

	if( r_speeds->integer ) {
		msec = ri.Sys_Milliseconds();
	}

	R_CullEntities();

	R_DrawEntities();

	if( r_speeds->integer ) {
		rf.stats.t_add_entities += ( ri.Sys_Milliseconds() - msec );
	}

	RJ_FinishJobs();

	R_SortDrawList( rn.meshlist );

	R_BindRefInstFBO();

	R_SetupGL();

	R_DrawPortals();

	if( r_portalonly->integer && !( rn.renderFlags & ( RF_MIRRORVIEW | RF_PORTALVIEW ) ) ) {
		goto end;
	}

	R_Clear( ~0 );

	R_DrawShadows();

	if( r_speeds->integer ) {
		msec = ri.Sys_Milliseconds();
	}

	R_DrawSurfaces( rn.meshlist );

	if( r_speeds->integer ) {
		rf.stats.t_draw_meshes += ( ri.Sys_Milliseconds() - msec );
	}

	if( r_showtris->integer ) {
		R_DrawOutlinedSurfaces( rn.meshlist );
	}

end:
	R_TransformForWorld();

	R_EndGL();
}

#define REFINST_STACK_SIZE  64
static refinst_t riStack[REFINST_STACK_SIZE];
static unsigned int riStackSize;

/*
* R_ClearRefInstStack
*/
void R_ClearRefInstStack( void ) {
	riStackSize = 0;
	memset( riStack, 0, sizeof( riStack ) );
	memset( &rn, 0, sizeof( refinst_t ) );
}

/*
* R_PushRefInst
*/
refinst_t *R_PushRefInst( void ) {
	if( riStackSize == REFINST_STACK_SIZE ) {
		return NULL;
	}
	riStack[riStackSize++] = rn;
	R_EndGL();
	return &riStack[riStackSize-1];
}

/*
* R_PopRefInst
*/
void R_PopRefInst( void ) {
	if( !riStackSize ) {
		return;
	}

	RB_FlushDynamicMeshes();

	rn = riStack[--riStackSize];
	R_BindRefInstFBO();

	R_SetupGL();
}

//=======================================================================

/*
* R_PushTransformMatrix
*/
void R_PushTransformMatrix( bool projection, const float *pm ) {
	int i;
	int p;
	int l = projection ? 1 : 0;

	p = rf.transformMatrixStackSize[l];
	if( p == MAX_PROJMATRIX_STACK_SIZE ) {
		return;
	}
	for( i = 0; i < 16; i++ ) {
		rf.transformMatricesStack[l][p][i] = pm[i];
	}

	RB_FlushDynamicMeshes();

	RB_LoadObjectMatrix( rf.transformMatricesStack[l][p] );
	rf.transformMatrixStackSize[l]++;
}

/*
* R_PopTransformMatrix
*/
void R_PopTransformMatrix( bool projection ) {
	int p;
	int l = projection ? 1 : 0;

	p = rf.transformMatrixStackSize[l];
	if( p == 0 ) {
		return;
	}

	RB_FlushDynamicMeshes();

	if( p == 1 ) {
		rf.transformMatrixStackSize[l] = 0;
		RB_LoadObjectMatrix( mat4x4_identity );
		return;
	}

	RB_LoadObjectMatrix( rf.transformMatricesStack[l][p - 1] );
	rf.transformMatrixStackSize[l]--;
}

//=======================================================================

void R_Finish( void ) {
	qglFinish();
}

void R_Flush( void ) {
	qglFlush();
}

void R_DeferDataSync( void ) {
	if( rsh.registrationOpen ) {
		return;
	}

	rf.dataSync = true;
	qglFlush();
	RB_FlushTextureCache();
}

void R_DataSync( void ) {
	if( rf.dataSync ) {
		if( glConfig.multithreading ) {
			// synchronize data we might have uploaded this frame between the threads
			// FIXME: only call this when absolutely necessary
			qglFinish();
		}
		rf.dataSync = false;
	}
}

/*
* R_SetSwapInterval
*/
int R_SetSwapInterval( int swapInterval, int oldSwapInterval ) {
	if( glConfig.stereoEnabled ) {
		return oldSwapInterval;
	}

	if( swapInterval != oldSwapInterval ) {
		GLimp_SetSwapInterval( swapInterval );
	}
	return swapInterval;
}

/*
* R_SetGamma
*/
void R_SetGamma( float gamma ) {
	int i, v;
	double invGamma, div;
	unsigned short gammaRamp[3 * GAMMARAMP_STRIDE];

	if( !glConfig.hwGamma ) {
		return;
	}

	invGamma = 1.0 / Q_bound( 0.5, gamma, 3.0 );
	div = (double)( 1 << 0 ) / ( glConfig.gammaRampSize - 0.5 );

	for( i = 0; i < glConfig.gammaRampSize; i++ ) {
		v = ( int )( 65535.0 * pow( ( (double)i + 0.5 ) * div, invGamma ) + 0.5 );
		gammaRamp[i] = gammaRamp[i + GAMMARAMP_STRIDE] = gammaRamp[i + 2 * GAMMARAMP_STRIDE] = ( ( unsigned short )Q_bound( 0, v, 65535 ) );
	}

	GLimp_SetGammaRamp( GAMMARAMP_STRIDE, glConfig.gammaRampSize, gammaRamp );
}

/*
* R_SetWallFloorColors
*/
void R_SetWallFloorColors( const vec3_t wallColor, const vec3_t floorColor ) {
	int i;
	for( i = 0; i < 3; i++ ) {
		rsh.wallColor[i] = Q_bound( 0, floor( wallColor[i] ) / 255.0, 1.0 );
		rsh.floorColor[i] = Q_bound( 0, floor( floorColor[i] ) / 255.0, 1.0 );
	}
}

/*
* R_SetDrawBuffer
*/
void R_SetDrawBuffer( const char *drawbuffer ) {
	Q_strncpyz( rf.drawBuffer, drawbuffer, sizeof( rf.drawBuffer ) );
	rf.newDrawBuffer = true;
}

/*
* R_IsRenderingToScreen
*/
bool R_IsRenderingToScreen( void ) {
	bool surfaceRenderable = true;
	GLimp_GetWindowSurface( &surfaceRenderable );
	return surfaceRenderable;
}

/*
* R_MultisampleSamples
*/
int R_MultisampleSamples( int samples ) {
	if( !glConfig.ext.framebuffer_multisample || samples <= 1 ) {
		return 0;
	}
	return Q_bound( 2, samples, glConfig.maxFramebufferSamples );
}

/*
* R_BlitTextureToScrFbo
*/
void R_BlitTextureToScrFbo( const refdef_t *fd, image_t *image, int dstFbo,
	int program_type, const vec4_t color, int blendMask, int numShaderImages, image_t **shaderImages,
	int iParam0 ) {
	int x, y;
	int w, h, fw, fh;
	int scissor[4], viewport[4];
	static char s_name[] = "$builtinpostprocessing";
	static shaderpass_t p;
	static shader_t s;
	int i;
	static tcmod_t tcmod;
	mat4_t m;

	assert( rsh.postProcessingVBO );
	if( numShaderImages >= MAX_SHADER_IMAGES ) {
		numShaderImages = MAX_SHADER_IMAGES;
	}

	// blit + flip using a static mesh to avoid redundant buffer uploads
	// (also using custom PP effects like FXAA with the stream VBO causes
	// Adreno to mark the VBO as "slow" (due to some weird bug)
	// for the rest of the frame and drop FPS to 10-20).
	RB_FlushDynamicMeshes();

	RB_BindFrameBufferObject( dstFbo );

	RB_GetScissor( &scissor[0], &scissor[1], &scissor[2], &scissor[3] );
	RB_GetViewport( &viewport[0], &viewport[1], &viewport[2], &viewport[3] );

	if( !dstFbo ) {
		// default framebuffer
		// set the viewport to full resolution
		// but keep the scissoring region
		if( fd ) {
			x = fd->x;
			y = fd->y;
			w = fw = fd->width;
			h = fh = fd->height;
		} else {
			x = 0;
			y = 0;
			w = fw = glConfig.width;
			h = fh = glConfig.height;
		}
		RB_Viewport( 0, 0, glConfig.width, glConfig.height );
		RB_Scissor( scissor[0], scissor[1], scissor[2], scissor[3] );
	} else {
		// aux framebuffer
		// set the viewport to full resolution of the framebuffer (without the NPOT padding if there's one)
		// draw quad on the whole framebuffer texture
		// set scissor to default framebuffer resolution
		image_t *cb = RFB_GetObjectTextureAttachment( dstFbo, false, 0 );
		x = 0;
		y = 0;
		w = fw = rf.frameBufferWidth;
		h = fh = rf.frameBufferHeight;
		if( cb ) {
			fw = cb->upload_width;
			fh = cb->upload_height;
		}
		RB_Viewport( 0, 0, w, h );
		RB_Scissor( 0, 0, glConfig.width, glConfig.height );
	}

	s.vattribs = VATTRIB_POSITION_BIT | VATTRIB_TEXCOORDS_BIT;
	s.sort = SHADER_SORT_NEAREST;
	s.numpasses = 1;
	s.name = s_name;
	s.passes = &p;

	p.rgbgen.type = RGB_GEN_IDENTITY;
	p.alphagen.type = ALPHA_GEN_IDENTITY;
	p.tcgen = TC_GEN_NONE;
	p.images[0] = image;
	for( i = 1; i < numShaderImages + 1; i++ ) {
		if( i >= MAX_SHADER_IMAGES ) {
			break;
		}
		p.images[i] = shaderImages[i - 1];
	}
	for( ; i < MAX_SHADER_IMAGES; i++ )
		p.images[i] = NULL;
	p.flags = blendMask | SHADERPASS_NOSRGB;
	p.program_type = program_type;
	p.anim_numframes = iParam0;

	if( !dstFbo ) {
		tcmod.type = TC_MOD_TRANSFORM;
		tcmod.args[0] = ( float )( w ) / ( float )( image->upload_width );
		tcmod.args[1] = ( float )( h ) / ( float )( image->upload_height );
		tcmod.args[4] = ( float )( x ) / ( float )( image->upload_width );
		tcmod.args[5] = ( float )( image->upload_height - h - y ) / ( float )( image->upload_height );
		p.numtcmods = 1;
		p.tcmods = &tcmod;
	} else {
		p.numtcmods = 0;
	}

	Matrix4_Identity( m );
	Matrix4_Scale2D( m, fw, fh );
	Matrix4_Translate2D( m, x, y );
	RB_LoadObjectMatrix( m );

	RB_BindShader( NULL, &s, NULL );
	RB_BindVBO( rsh.postProcessingVBO->index, GL_TRIANGLES );
	RB_DrawElements( 0, 4, 0, 6 );

	RB_LoadObjectMatrix( mat4x4_identity );

	// restore 2D viewport and scissor
	RB_Viewport( viewport[0], viewport[1], viewport[2], viewport[3] );
	RB_Scissor( scissor[0], scissor[1], scissor[2], scissor[3] );
}

/*
* R_WriteSpeedsMessage
*/
const char *R_WriteSpeedsMessage( char *out, size_t size ) {
	char backend_msg[1024];

	if( !out || !size ) {
		return out;
	}

	out[0] = '\0';
	if( r_speeds->integer && !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		switch( r_speeds->integer ) {
			case 1:
			case 2:
			case 3:
				RB_StatsMessage( backend_msg, sizeof( backend_msg ) );

				Q_snprintfz( out, size,
							 "%u fps\n"
							 "%4u wpoly %4u leafs %4u surfs\n"
							 "cull nodes\\surfs\\lights: %5u\\%5u\\%5u\n"
							 "node world\\light: %5u\\%5u\n"
							 "polys\\ents: %5u\\%5u  draw: %5u\n"
							 "world\\dynamic: lights %3u\\%3u  shadows %3u\\%3u\n"
							 "ents total: %5u bmodels: %5u\n"
							 "frame cache: %.3fMB\n"
							 "%s",
							 (int)(1000.0 / rf.frameTime.average),
							 rf.stats.c_brush_polys, rf.stats.c_world_leafs, rf.stats.c_world_draw_surfs,
							 rf.stats.t_cull_world_nodes, rf.stats.t_cull_world_surfs, rf.stats.t_cull_rtlights, 
							 rf.stats.t_world_node, rf.stats.t_light_node,
							 rf.stats.t_add_polys, rf.stats.t_add_entities, rf.stats.t_draw_meshes,
							 rf.stats.c_world_lights, rf.stats.c_dynamic_lights, rf.stats.c_world_light_shadows, rf.stats.c_dynamic_light_shadows,
							 rf.stats.c_ents_total, rf.stats.c_ents_bmodels,
							 R_FrameCache_TotalSize() / 1048576.0,
							 backend_msg
							);
				break;
			case 4:
			case 5:
				if( rf.debugSurface ) {
					int numVerts = 0, numTris = 0;
					msurface_t *debugSurface = rf.debugSurface;
					drawSurfaceBSP_t *drawSurf = rf.debugSurface ? &rsh.worldBrushModel->drawSurfaces[debugSurface->drawSurf - 1] : NULL;

					Q_snprintfz( out, size,
								 "%s type:%i sort:%i",
								 debugSurface->shader->name, debugSurface->facetype, debugSurface->shader->sort );

					if( r_speeds->integer == 5 && drawSurf && drawSurf->vbo ) {
						mesh_vbo_t *vbo = R_GetVBOByIndex( drawSurf->vbo );
						numVerts = vbo->numVerts;
						numTris = vbo->numElems / 3;
					} else {
						numVerts = debugSurface->mesh.numVerts;
						numTris = debugSurface->mesh.numElems;
					}

					if( numVerts ) {
						Q_strncatz( out, "\n", size );
						Q_snprintfz( out + strlen( out ), size - strlen( out ),
									 "verts: %5i tris: %5i", numVerts, numTris );
					}

					if( debugSurface->fog && debugSurface->fog->shader
						&& debugSurface->fog->shader != debugSurface->shader ) {
						Q_strncatz( out, "\n", size );
						Q_strncatz( out, debugSurface->fog->shader->name, size );
					}
				}
				break;
			case 6:
				Q_snprintfz( out, size,
							 "%.1f %.1f %.1f",
							 rn.refdef.vieworg[0], rn.refdef.vieworg[1], rn.refdef.vieworg[2]
							 );
				break;
			default:
				Q_snprintfz( out, size,
							 "%u fps",
							 (int)(1000.0 / rf.frameTime.average)
							 );
				break;
		}
	}

	out[size - 1] = '\0';
	return out;
}

/*
* R_GetDebugSurface
*/
const msurface_t *R_GetDebugSurface( void ) {
	msurface_t *debugSurface;

	ri.Mutex_Lock( rf.debugSurfaceLock );
	debugSurface = rf.debugSurface;
	ri.Mutex_Unlock( rf.debugSurfaceLock );

	return debugSurface;
}

/*
* R_RenderDebugSurface
*/
void R_RenderDebugSurface( const refdef_t *fd ) {
	rtrace_t tr = { 0 };
	vec3_t forward;
	vec3_t start, end;
	msurface_t *debugSurf = NULL;

	if( r_speeds->integer == 4 || r_speeds->integer == 5 ) {
		msurface_t *surf = NULL;

		VectorCopy( &fd->viewaxis[AXIS_FORWARD], forward );
		VectorCopy( fd->vieworg, start );
		VectorMA( start, 4096, forward, end );

		surf = R_TraceLine( &tr, start, end, 0 );
		if( surf && surf->drawSurf && !r_showtris->integer ) {
			drawSurfaceBSP_t *drawSurf = rsh.worldBrushModel->drawSurfaces + surf->drawSurf - 1;

			R_ClearDrawList( rn.meshlist );

			R_ClearDrawList( rn.portalmasklist );

			if( R_AddSurfToDrawList( rn.meshlist, R_NUM2ENT( tr.ent ), surf->shader, NULL, -1, 0, 0, NULL, drawSurf ) ) {
				if( rn.refdef.rdflags & RDF_FLIPPED ) {
					RB_FlipFrontFace();
				}

				if( r_speeds->integer == 5 ) {
					unsigned i;

					// vbo debug mode
					for( i = 0; i < drawSurf->numWorldSurfaces; i++ )
						rn.meshlist->worldSurfVis[drawSurf->worldSurfaces[i]] = 1;
				} else {
					// individual surface
					rn.meshlist->worldSurfVis[surf - rsh.worldBrushModel->surfaces] = 1;
				}

				R_DrawOutlinedSurfaces( rn.meshlist );

				if( rn.refdef.rdflags & RDF_FLIPPED ) {
					RB_FlipFrontFace();
				}

				debugSurf = surf;
			}
		}
	}

	ri.Mutex_Lock( rf.debugSurfaceLock );
	rf.debugTrace = tr;
	rf.debugSurface = debugSurf;
	ri.Mutex_Unlock( rf.debugSurfaceLock );
}

/*
* R_BeginFrame
*/
void R_BeginFrame( float cameraSeparation, bool forceClear, int swapInterval ) {
	int samples;
	int64_t time = ri.Sys_Milliseconds();

	GLimp_BeginFrame();

	RB_BeginFrame();

#ifndef GL_ES_VERSION_2_0
	if( cameraSeparation && ( !glConfig.stereoEnabled || !R_IsRenderingToScreen() ) ) {
		cameraSeparation = 0;
	}

	if( rf.cameraSeparation != cameraSeparation ) {
		rf.cameraSeparation = cameraSeparation;
		if( cameraSeparation < 0 ) {
			qglDrawBuffer( GL_BACK_LEFT );
		} else if( cameraSeparation > 0 ) {
			qglDrawBuffer( GL_BACK_RIGHT );
		} else {
			qglDrawBuffer( GL_BACK );
		}
	}
#endif

	// draw buffer stuff
	if( rf.newDrawBuffer ) {
		rf.newDrawBuffer = false;

#ifndef GL_ES_VERSION_2_0
		if( cameraSeparation == 0 || !glConfig.stereoEnabled ) {
			if( Q_stricmp( rf.drawBuffer, "GL_FRONT" ) == 0 ) {
				qglDrawBuffer( GL_FRONT );
			} else {
				qglDrawBuffer( GL_BACK );
			}
		}
#endif
	}

	// set swap interval (vertical synchronization)
	rf.swapInterval = R_SetSwapInterval( swapInterval, rf.swapInterval );

	memset( &rf.stats, 0, sizeof( rf.stats ) );

	// update fps meter
	rf.frameTime.count++;
	rf.frameTime.time = time;
	if( rf.frameTime.time - rf.frameTime.oldTime >= 50 ) {
		rf.frameTime.average = time - rf.frameTime.oldTime;
		rf.frameTime.average = ((float)rf.frameTime.average / ( rf.frameTime.count - rf.frameTime.oldCount )) + 0.5f;
		rf.frameTime.oldTime = time;
		rf.frameTime.oldCount = rf.frameTime.count;
	}

	samples = R_MultisampleSamples( r_samples2D->integer );
	rf.renderTarget = R_RegisterMultisampleTarget( &rsh.st2D, samples, false, true );

	R_Begin2D( true );

	if( forceClear ) {
		RB_Clear( GL_COLOR_BUFFER_BIT, 0, 0, 0, 1 );
	}
}

/*
* R_EndFrame
*/
void R_EndFrame( void ) {
	// render previously batched 2D geometry, if any
	RB_FlushDynamicMeshes();

	R_BindFrameBufferObject( 0 );

	R_Begin2D( false );

	RB_EnableTriangleOutlines( false );

	// resolve multisampling and blit to default framebuffer
	if( rf.renderTarget > 0 ) {
		RB_BlitFrameBufferObject( rf.renderTarget, rsh.st2D.screenTex->fbo, 
			GL_COLOR_BUFFER_BIT, FBO_COPY_NORMAL, GL_NEAREST, 0, 0 );

		R_BlitTextureToScrFbo( NULL, rsh.st2D.screenTex, 0, GLSL_PROGRAM_TYPE_NONE, 
			colorWhite, 0, 0, NULL, 0 );

		RB_FlushDynamicMeshes();
	}

	R_PolyBlend();

	if( 0 )
	{
		int side = /*r_temp2->integer - 1*/0;
		image_t *atlas = R_GetShadowmapAtlasTexture();

		if( side >= 0 && atlas && rn.numRealtimeLights > 0 ) {
			int size = rn.rtlights[0]->shadowSize;
			float x = rn.rtlights[0]->shadowOffset[0];
			float y = rn.rtlights[0]->shadowOffset[1];
			float st = (float)size / rsh.shadowmapAtlasTexture->upload_width;

			x = (1.0 * x + (side & 1) * size) / (float)rsh.shadowmapAtlasTexture->upload_width;
			y = (1.0 * y + (side >> 1) * size) / (float)rsh.shadowmapAtlasTexture->upload_width;

			R_DrawStretchQuick( 0, 0, 128, 128, 
				x, y, 
				x + st, y + st, colorRed, GLSL_PROGRAM_TYPE_NONE,
				rsh.whiteTexture, 0 );

			R_DrawStretchQuick( 0, 0, 128, 128, 
				x, y, 
				x + st, y + st, colorWhite, GLSL_PROGRAM_TYPE_NONE,
				atlas, 0 );
		}
	}

	R_End2D();

	RB_EndFrame();

	GLimp_EndFrame();

	rf.transformMatrixStackSize[0] = 0;
	rf.transformMatrixStackSize[1] = 0;

	assert( qglGetError() == GL_NO_ERROR );
}

//===================================================================

/*
* R_NormToLatLong
*/
void R_NormToLatLong( const vec_t *normal, uint8_t latlong[2] ) {
	float flatlong[2];

	NormToLatLong( normal, flatlong );
	latlong[0] = (int)( flatlong[0] * 255.0 / M_TWOPI ) & 255;
	latlong[1] = (int)( flatlong[1] * 255.0 / M_TWOPI ) & 255;
}

/*
* R_LatLongToNorm4
*/
void R_LatLongToNorm4( const uint8_t latlong[2], vec4_t out ) {
	static float * const sinTable = rsh.sinTableByte;
	float sin_a, sin_b, cos_a, cos_b;

	cos_a = sinTable[( latlong[0] + 64 ) & 255];
	sin_a = sinTable[latlong[0]];
	cos_b = sinTable[( latlong[1] + 64 ) & 255];
	sin_b = sinTable[latlong[1]];

	Vector4Set( out, cos_b * sin_a, sin_b * sin_a, cos_a, 0 );
}

/*
* R_LatLongToNorm
*/
void R_LatLongToNorm( const uint8_t latlong[2], vec3_t out ) {
	vec4_t t;
	R_LatLongToNorm4( latlong, t );
	VectorCopy( t, out );
}

/*
* R_CopyString
*/
ATTRIBUTE_MALLOC void *R_Malloc_( size_t size, const char *filename, int fileline ) {
	return ri.Mem_AllocExt( r_mempool, size, 16, 1, filename, fileline );
}

/*
* R_CopyString
*/
char *R_CopyString_( const char *in, const char *filename, int fileline ) {
	char *out;

	out = ri.Mem_AllocExt( r_mempool, ( strlen( in ) + 1 ), 0, 1, filename, fileline );
	strcpy( out, in );

	return out;
}

/*
* R_LoadFile
*/
int R_LoadFile_( const char *path, int flags, void **buffer, const char *filename, int fileline ) {
	uint8_t *buf;
	unsigned int len;
	int fhandle;

	buf = NULL; // quiet compiler warning

	// look for it in the filesystem or pack files
	len = ri.FS_FOpenFile( path, &fhandle, FS_READ | flags );

	if( !fhandle ) {
		if( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}

	if( !buffer ) {
		ri.FS_FCloseFile( fhandle );
		return len;
	}

	buf = ( uint8_t *)ri.Mem_AllocExt( r_mempool, len + 1, 16, 0, filename, fileline );
	buf[len] = 0;
	*buffer = buf;

	ri.FS_Read( buf, len, fhandle );
	ri.FS_FCloseFile( fhandle );

	return len;
}

/*
* R_FreeFile
*/
void R_FreeFile_( void *buffer, const char *filename, int fileline ) {
	ri.Mem_Free( buffer, filename, fileline );
}

//===================================================================

//#define FRAMECACHE_DEBUG

typedef struct r_framecachemark_s {
	uint8_t *ptr;
	struct r_framecache_s *cache;
} r_framecachemark_t;

typedef struct r_framecache_s {
	size_t dataSize;
	uint8_t *dataRover;
	r_framecachemark_t mark;
	struct r_framecache_s *next;
} r_framecache_t;

static r_framecache_t *r_frameCacheHead;
static size_t r_frameCacheTotalSize;

/*
* R_FrameCache_Free
*/
void R_FrameCache_Free( void ) {
	r_framecache_t *next, *cache;

	cache = r_frameCacheHead;
	while( cache ) {
		next = cache->next;
		R_Free( cache );
		cache = next;
	}

	r_frameCacheHead = NULL;
	r_frameCacheTotalSize = 0;
}

/*
* R_FrameCache_ResetBlock
*/
static void R_FrameCache_ResetBlock( r_framecache_t *cache ) {
	cache->dataRover = (uint8_t *)(((uintptr_t)(cache + 1) + 15) & ~15);
	cache->mark.cache = cache;
	cache->mark.ptr = cache->dataRover;
}

/*
* R_FrameCache_NewBlock
*/
r_framecache_t *R_FrameCache_NewBlock( size_t size ) {
	r_framecache_t *cache;

	cache = R_Malloc( size + sizeof( r_framecache_t ) + 16 );
	cache->dataSize = size;

	R_FrameCache_ResetBlock( cache );

	r_frameCacheTotalSize += size;
	return cache;
}

/*
* R_FrameCache_Clear
*
* Allocate a whole new block of heap memory to accomodate all data from the previous frame.
*/
void R_FrameCache_Clear( void ) {
	size_t newSize;
	
	newSize = r_frameCacheTotalSize;
	if( newSize < MIN_FRAMECACHE_SIZE )
		newSize = MIN_FRAMECACHE_SIZE;

	if( !r_frameCacheHead || r_frameCacheHead->dataSize < newSize ) {
		r_framecache_t *cache;

		R_FrameCache_Free();

		cache = R_FrameCache_NewBlock( newSize );

		r_frameCacheHead = cache;
	}

	R_FrameCache_ResetBlock( r_frameCacheHead );

#ifdef FRAMECACHE_DEBUG
	Com_Printf( "R_FrameCache_Clear\n" );
#endif
}

/*
* R_FrameCache_Alloc
*/
void *R_FrameCache_Alloc_( size_t size, const char *filename, int fileline ) {
	uint8_t *data;
	r_framecache_t *cache = r_frameCacheHead;
	size_t used;

	if( !size ) {
		return NULL;
	}

#ifdef FRAMECACHE_DEBUG
	Com_Printf( "R_FrameCache_Alloc_: %s:%d\n", filename, fileline );
#endif

	size = ((size + 15) & ~15);

	assert( cache != NULL );
	if( cache == NULL ) {
		return NULL;
	}

	used = cache->dataRover - (uint8_t *)cache;
	if( used + size > cache->dataSize ) {
		size_t newSize = r_frameCacheTotalSize / 2;

		if( newSize < MIN_FRAMECACHE_SIZE ) {
			newSize = MIN_FRAMECACHE_SIZE;
		}
		if( newSize < size ) {
			newSize = size;
		}

		cache = R_FrameCache_NewBlock( newSize );
		cache->next = r_frameCacheHead;
		r_frameCacheHead = cache;
	}

	data = cache->dataRover;
	cache->dataRover += size;
	return data;
}

/*
* R_FrameCache_TotalSize
*/
size_t R_FrameCache_TotalSize( void ) {
	return r_frameCacheTotalSize;
}

/*
* R_FrameCache_SetMark
*/
void *R_FrameCache_SetMark_( const char *filename, int fileline ) {
	r_framecache_t *cache = r_frameCacheHead;
	r_framecachemark_t *cmark = &cache->mark;
	cmark->ptr = cache->dataRover;
#ifdef FRAMECACHE_DEBUG
	Com_Printf( "R_FrameCache_SetMark_: %s:%d\n", filename, fileline );
#endif
	return (void *)cmark;
}

/*
* R_FrameCache_FreeToMark_
*/
void R_FrameCache_FreeToMark_( void *mark, const char *filename, int fileline ) {
	r_framecachemark_t *cmark = mark;
	r_framecache_t *cache = cmark->cache;
#ifdef FRAMECACHE_DEBUG
	Com_Printf( "R_FrameCache_FreeToMark_: %s:%d\n", filename, fileline );
#endif
	cache->dataRover = cmark->ptr;
}
