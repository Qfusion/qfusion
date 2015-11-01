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

r_frontend_t rf;

mapconfig_t mapConfig;

refinst_t rn;

r_scene_t rsc;

/*
* R_TransformBounds
*/
void R_TransformBounds( const vec3_t origin, const mat3_t axis, vec3_t mins, vec3_t maxs, vec3_t bbox[8] )
{
	int i;
	vec3_t tmp;
	mat3_t axis_;

	Matrix3_Transpose( axis, axis_ );	// switch row-column order

	// rotate local bounding box and compute the full bounding box
	for( i = 0; i < 8; i++ )
	{
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
void R_TransformForWorld( void )
{
	Matrix4_Identity( rn.objectMatrix );
	Matrix4_Copy( rn.cameraMatrix, rn.modelviewMatrix );

	RB_LoadObjectMatrix( mat4x4_identity );
}

/*
* R_TranslateForEntity
*/
void R_TranslateForEntity( const entity_t *e )
{
	Matrix4_Identity( rn.objectMatrix );

	rn.objectMatrix[0] = e->scale;
	rn.objectMatrix[5] = e->scale;
	rn.objectMatrix[10] = e->scale;
	rn.objectMatrix[12] = e->origin[0];
	rn.objectMatrix[13] = e->origin[1];
	rn.objectMatrix[14] = e->origin[2];

	RB_LoadObjectMatrix( rn.objectMatrix );
}

/*
* R_TransformForEntity
*/
void R_TransformForEntity( const entity_t *e )
{
	if( e->rtype != RT_MODEL ) {
		R_TransformForWorld();
		return;
	}
	if( e == rsc.worldent ) {
		R_TransformForWorld();
		return;
	}

	if( e->scale != 1.0f ) {
		rn.objectMatrix[0] = e->axis[0] * e->scale;
		rn.objectMatrix[1] = e->axis[1] * e->scale;
		rn.objectMatrix[2] = e->axis[2] * e->scale;
		rn.objectMatrix[4] = e->axis[3] * e->scale;
		rn.objectMatrix[5] = e->axis[4] * e->scale;
		rn.objectMatrix[6] = e->axis[5] * e->scale;
		rn.objectMatrix[8] = e->axis[6] * e->scale;
		rn.objectMatrix[9] = e->axis[7] * e->scale;
		rn.objectMatrix[10] = e->axis[8] * e->scale;
	} else {
		rn.objectMatrix[0] = e->axis[0];
		rn.objectMatrix[1] = e->axis[1];
		rn.objectMatrix[2] = e->axis[2];
		rn.objectMatrix[4] = e->axis[3];
		rn.objectMatrix[5] = e->axis[4];
		rn.objectMatrix[6] = e->axis[5];
		rn.objectMatrix[8] = e->axis[6];
		rn.objectMatrix[9] = e->axis[7];
		rn.objectMatrix[10] = e->axis[8];
	}

	rn.objectMatrix[3] = 0;
	rn.objectMatrix[7] = 0;
	rn.objectMatrix[11] = 0;
	rn.objectMatrix[12] = e->origin[0];
	rn.objectMatrix[13] = e->origin[1];
	rn.objectMatrix[14] = e->origin[2];
	rn.objectMatrix[15] = 1.0;

	Matrix4_MultiplyFast( rn.cameraMatrix, rn.objectMatrix, rn.modelviewMatrix );

	RB_LoadObjectMatrix( rn.objectMatrix );
}

/*
* R_LerpTag
*/
bool R_LerpTag( orientation_t *orient, const model_t *mod, int oldframe, int frame, float lerpfrac, const char *name )
{
	if( !orient )
		return false;

	VectorClear( orient->origin );
	Matrix3_Identity( orient->axis );

	if( !name )
		return false;

	if( mod->type == mod_alias )
		return R_AliasModelLerpTag( orient, (const maliasmodel_t *)mod->extradata, oldframe, frame, lerpfrac, name );

	return false;
}

/*
* R_FogForBounds
*/
mfog_t *R_FogForBounds( const vec3_t mins, const vec3_t maxs )
{
	unsigned int i, j;
	mfog_t *fog;

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) || !rsh.worldBrushModel->numfogs )
		return NULL;
	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return NULL;
	if( rsh.worldBrushModel->globalfog )
		return rsh.worldBrushModel->globalfog;

	fog = rsh.worldBrushModel->fogs;
	for( i = 0; i < rsh.worldBrushModel->numfogs; i++, fog++ )
	{
		if( !fog->shader )
			continue;

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
mfog_t *R_FogForSphere( const vec3_t centre, const float radius )
{
	int i;
	vec3_t mins, maxs;

	for( i = 0; i < 3; i++ ) {
		mins[i] = centre[i] - radius;
		maxs[i] = centre[i] + radius;
	}

	return R_FogForBounds( mins, maxs );
}

/*
* R_CompletelyFogged
*/
bool R_CompletelyFogged( const mfog_t *fog, vec3_t origin, float radius )
{
	// note that fog->distanceToEye < 0 is always true if
	// globalfog is not NULL and we're inside the world boundaries
	if( fog && fog->shader && fog == rn.fog_eye )
	{
		float vpnDist = ( ( rn.viewOrigin[0] - origin[0] ) * rn.viewAxis[AXIS_FORWARD+0] + 
			( rn.viewOrigin[1] - origin[1] ) * rn.viewAxis[AXIS_FORWARD+1] + 
			( rn.viewOrigin[2] - origin[2] ) * rn.viewAxis[AXIS_FORWARD+2] );
		return ( ( vpnDist + radius ) / fog->shader->fog_dist ) < -1;
	}

	return false;
}

/*
* R_LODForSphere
*/
int R_LODForSphere( const vec3_t origin, float radius )
{
	float dist;
	int lod;

	dist = DistanceFast( origin, rn.lodOrigin );
	dist *= rn.lod_dist_scale_for_fov;

	lod = (int)( dist / radius );
	if( r_lodscale->integer )
		lod /= r_lodscale->integer;
	lod += r_lodbias->integer;

	if( lod < 1 )
		return 0;
	return lod;
}

/*
=============================================================

CUSTOM COLORS

=============================================================
*/

static byte_vec4_t r_customColors[NUM_CUSTOMCOLORS];

/*
* R_InitCustomColors
*/
void R_InitCustomColors( void )
{
	memset( r_customColors, 255, sizeof( r_customColors ) );
}

/*
* R_SetCustomColor
*/
void R_SetCustomColor( int num, int r, int g, int b )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return;
	Vector4Set( r_customColors[num], (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 );
}
/*
* R_GetCustomColor
*/
int R_GetCustomColor( int num )
{
	if( num < 0 || num >= NUM_CUSTOMCOLORS )
		return COLOR_RGBA( 255, 255, 255, 255 );
	return *(int *)r_customColors[num];
}

/*
* R_ShutdownCustomColors
*/
void R_ShutdownCustomColors( void )
{
	memset( r_customColors, 255, sizeof( r_customColors ) );
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
void R_BatchSpriteSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, const portalSurface_t *portalSurface, unsigned int shadowBits, drawSurfaceType_t *drawSurf )
{
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

	if( rotation )
	{
		RotatePointAroundVector( v_left, &rn.viewAxis[AXIS_FORWARD], &rn.viewAxis[AXIS_RIGHT], rotation );
		CrossProduct( &rn.viewAxis[AXIS_FORWARD], v_left, v_up );
	}
	else
	{
		VectorCopy( &rn.viewAxis[AXIS_RIGHT], v_left );
		VectorCopy( &rn.viewAxis[AXIS_UP], v_up );
	}

	if( rn.renderFlags & (RF_MIRRORVIEW|RF_FLIPFRONTFACE) )
		VectorInverse( v_left );

	VectorMA( e->origin, -radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[0] );
	VectorMA( point, -radius, v_left, xyz[3] );

	VectorMA( e->origin, radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[1] );
	VectorMA( point, -radius, v_left, xyz[2] );

	for( i = 0; i < 4; i++ )
	{
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

	RB_AddDynamicMesh( e, shader, fog, portalSurface, 0, &mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_AddSpriteToDrawList
*/
static bool R_AddSpriteToDrawList( const entity_t *e )
{
	float dist;

	if( e->flags & RF_NOSHADOW )
	{
		if( rn.renderFlags & RF_SHADOWMAPVIEW )
			return false;
	}

	if( e->radius <= 0 || e->customShader == NULL || e->scale <= 0 ) {
		return false;
	}

	dist =
		( e->origin[0] - rn.refdef.vieworg[0] ) * rn.viewAxis[AXIS_FORWARD+0] +
		( e->origin[1] - rn.refdef.vieworg[1] ) * rn.viewAxis[AXIS_FORWARD+1] +
		( e->origin[2] - rn.refdef.vieworg[2] ) * rn.viewAxis[AXIS_FORWARD+2];
	if( dist <= 0 )
		return false; // cull it because we don't want to sort unneeded things

	if( !R_AddSurfToDrawList( rn.meshlist, e, R_FogForSphere( e->origin, e->radius ), 
		e->customShader, dist, 0, NULL, &spriteDrawSurf ) ) {
		return false;
	}

	return true;
}

//==================================================================================

static drawSurfaceType_t nullDrawSurf = ST_NULLMODEL;

/*
* R_InitNullModelVBO
*/
mesh_vbo_t *R_InitNullModelVBO( void )
{
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

	R_UploadVBOVertexData( vbo, 0, vattribs, &mesh );
	R_UploadVBOElemData( vbo, 0, 0, &mesh );

	return vbo;
}

/*
* R_DrawNullSurf
*/
void R_DrawNullSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, const portalSurface_t *portalSurface, unsigned int shadowBits, drawSurfaceType_t *drawSurf )
{
	assert( rsh.nullVBO != NULL );
	if( !rsh.nullVBO ) {
		return;
	}

	RB_BindVBO( rsh.nullVBO->index, GL_LINES );

	RB_DrawElements( 0, 6, 0, 6, 0, 0, 0, 0 );
}

/*
* R_AddNullSurfToDrawList
*/
static bool R_AddNullSurfToDrawList( const entity_t *e )
{
	if( !R_AddSurfToDrawList( rn.meshlist, e, R_FogForSphere( e->origin, 0.1f ), 
		rsh.whiteShader, 0, 0, NULL, &nullDrawSurf ) ) {
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
static mesh_t pic_mesh = { 4, pic_xyz, pic_normals, NULL, pic_st, { 0, 0, 0, 0 }, { 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, NULL, NULL, 6, pic_elems };

/*
* R_Set2DMode
*
* Note that this sets the viewport to size of the active framebuffer.
*/
void R_Set2DMode( bool enable )
{
	int width, height;

	width = rf.frameBufferWidth;
	height = rf.frameBufferHeight;

	if( rf.in2D == true && enable == true && width == rf.width2D && height == rf.height2D ) {
		return;
	} else if( rf.in2D == false && enable == false ) {
		return;
	}

	rf.in2D = enable;

	if( enable )
	{
		rf.width2D = width;
		rf.height2D = height;

		Matrix4_OrthogonalProjection( 0, width, height, 0, -99999, 99999, rn.projectionMatrix );
		Matrix4_Copy( mat4x4_identity, rn.modelviewMatrix );
		Matrix4_Copy( rn.projectionMatrix, rn.cameraProjectionMatrix );

		// set 2D virtual screen size
		RB_Scissor( 0, 0, width, height );
		RB_Viewport( 0, 0, width, height );

		RB_LoadProjectionMatrix( rn.projectionMatrix );
		RB_LoadCameraMatrix( mat4x4_identity );
		RB_LoadObjectMatrix( mat4x4_identity );

		RB_SetShaderStateMask( ~0, GLSTATE_NO_DEPTH_TEST );

		RB_SetRenderFlags( 0 );
	}
	else
	{
		// render previously batched 2D geometry, if any
		RB_FlushDynamicMeshes();

		RB_SetShaderStateMask( ~0, 0 );
	}
}

/*
* R_DrawRotatedStretchPic
*/
void R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle, 
	const vec4_t color, const shader_t *shader )
{
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
		bound( 0, ( int )( color[0] * 255.0f ), 255 ), 
		bound( 0, ( int )( color[1] * 255.0f ), 255 ),
		bound( 0, ( int )( color[2] * 255.0f ), 255 ), 
		bound( 0, ( int )( color[3] * 255.0f ), 255 ) );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	Vector2Set( pic_xyz[1], x+w, y );
	Vector2Set( pic_st[1], s2, t1 );
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	Vector2Set( pic_xyz[2], x+w, y+h );
	Vector2Set( pic_st[2], s2, t2 );
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	Vector2Set( pic_xyz[3], x, y+h );
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

		for( j = 0; j < 4; j++ )
		{
			t1 = pic_st[j][0];
			t2 = pic_st[j][1];
			pic_st[j][0] = cost * (t1 - 0.5f) - sint * (t2 - 0.5f) + 0.5f;
			pic_st[j][1] = cost * (t2 - 0.5f) + sint * (t1 - 0.5f) + 0.5f;
		}
	}

	RB_AddDynamicMesh( NULL, shader, NULL, NULL, 0, &pic_mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_DrawStretchPic
*/
void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
	const vec4_t color, const shader_t *shader )
{
	R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, 0, color, shader );
}

/*
* R_DrawStretchRaw
*
* Passing NULL for data redraws last uploaded frame
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, 
	float s1, float t1, float s2, float t2, uint8_t *data )
{
	float h_scale, v_scale;

	if( !rows || !cols ) {
		return;
	}

	if( data ) {
		if( rsh.rawTexture->width != cols || rsh.rawTexture->height != rows ) {
			uint8_t *nodata[1] = { NULL };
			R_ReplaceImage( rsh.rawTexture, nodata, cols, rows, rsh.rawTexture->flags, 1, 3 );
		}
		R_ReplaceSubImage( rsh.rawTexture, 0, 0, 0, &data, cols, rows );
	}

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
	float s1, float t1, float s2, float t2, 
	ref_img_plane_t *yuv, image_t **yuvTextures, int flip )
{
	static char *s_name = "$builtinyuv";
	static shaderpass_t p;
	static shader_t s;
	float h_scale, v_scale;
	float s2_, t2_;
	float h_ofs, v_ofs;

	s.vattribs = VATTRIB_POSITION_BIT|VATTRIB_TEXCOORDS_BIT;
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

	if( yuv ) {
		int i;

		for( i = 0; i < 3; i++ ) {
			uint8_t *data = yuv[i].data;
			int flags = yuvTextures[i]->flags;
			int stride = yuv[i].stride;
			int height = yuv[i].height;

			if( stride < 0 ) {
				// negative stride flips the image vertically
				data = data + stride * height;
				flags = (flags & ~(IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL)) | IT_FLIPY;
				stride = -stride;
			}

			if( yuvTextures[i]->width != stride || yuvTextures[i]->height != height ) {
				uint8_t *nodata[1] = { NULL };
				R_ReplaceImage( yuvTextures[i], nodata, stride, height, flags, 1, 1 );
			}
			R_ReplaceSubImage( yuvTextures[i], 0, 0, 0, &data, stride, height );
		}
	}

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

	RB_FlushDynamicMeshes();
}

/*
* R_DrawStretchRawYUV
*
* Passing NULL for data redraws last uploaded frame
*/
void R_DrawStretchRawYUV( int x, int y, int w, int h, 
	float s1, float t1, float s2, float t2, ref_img_plane_t *yuv )
{
	R_DrawStretchRawYUVBuiltin( x, y, w, h, s1, t1, s2, t2, yuv, rsh.rawYUVTextures, 0 );
}

/*
* R_DrawStretchImage
*/
void R_DrawStretchQuick( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
	const vec4_t color, int program_type, image_t *image, int blendMask )
{
	static char *s_name = "$builtinimage";
	static shaderpass_t p;
	static shader_t s;
	static float rgba[4];

	s.vattribs = VATTRIB_POSITION_BIT|VATTRIB_TEXCOORDS_BIT;
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
void R_BindFrameBufferObject( int object )
{
	int width, height;

	RFB_GetObjectSize( object, &width, &height );

	rf.frameBufferWidth = width;
	rf.frameBufferHeight = height;

	RB_BindFrameBufferObject( object );

	RB_Viewport( rn.viewport[0], rn.viewport[1], rn.viewport[2], rn.viewport[3] );
	RB_Scissor( rn.scissor[0], rn.scissor[1], rn.scissor[2], rn.scissor[3] );
}

/*
* R_Scissor
*
* Set scissor region for 2D drawing.
* x and y represent the top left corner of the region/rectangle.
*/
void R_Scissor( int x, int y, int w, int h )
{
	RB_Scissor( x, y, w, h );
}

/*
* R_GetScissor
*/
void R_GetScissor( int *x, int *y, int *w, int *h )
{
	RB_GetScissor( x, y, w, h );
}

/*
* R_ResetScissor
*/
void R_ResetScissor( void )
{
	RB_Scissor( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
}

/*
* R_PolyBlend
*/
static void R_PolyBlend( void )
{
	if( !r_polyblend->integer )
		return;
	if( rsc.refdef.blend[3] < 0.01f )
		return;

	R_Set2DMode( true );
	R_DrawStretchPic( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight, 0, 0, 1, 1, rsc.refdef.blend, rsh.whiteShader );
	RB_FlushDynamicMeshes();
}

/*
* R_ApplyBrightness
*/
static void R_ApplyBrightness( void )
{
	float c;
	vec4_t color;

	c = r_brightness->value;
	if( c < 0.005 )
		return;
	else if( c > 1.0 )
		c = 1.0;

	color[0] = color[1] = color[2] = c, color[3] = 1;

	R_Set2DMode( true );
	R_DrawStretchQuick( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight, 0, 0, 1, 1,
		color, GLSL_PROGRAM_TYPE_NONE, rsh.whiteTexture, GLSTATE_SRCBLEND_ONE|GLSTATE_DSTBLEND_ONE );
}

/*
* R_InitPostProcessingVBO
*/
mesh_vbo_t *R_InitPostProcessingVBO( void )
{
	vec4_t xyz[4] = { {0,0,0,1}, {1,0,0,1}, {1,1,0,1}, {0,1,0,1} };
	vec2_t texcoords[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
	elem_t elems[6] = { 0, 1, 2, 0, 2, 3 };
	mesh_t mesh;
	vattribmask_t vattribs = VATTRIB_POSITION_BIT|VATTRIB_TEXCOORDS_BIT;
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

	R_UploadVBOVertexData( vbo, 0, vattribs, &mesh );
	R_UploadVBOElemData( vbo, 0, 0, &mesh );

	return vbo;
}

//=======================================================================

/*
* R_DefaultFarClip
*/
float R_DefaultFarClip( void )
{
	float farclip_dist;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return rn.shadowGroup->projDist;
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
* R_SetVisFarClip
*/
static float R_SetVisFarClip( void )
{
	int i;
	float dist;
	vec3_t tmp;
	float farclip_dist;

	if( !rsh.worldModel || ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return rn.visFarClip;
	}

	rn.visFarClip = 0;

	farclip_dist = 0;
	for( i = 0; i < 8; i++ )
	{
		tmp[0] = ( ( i & 1 ) ? rn.visMins[0] : rn.visMaxs[0] );
		tmp[1] = ( ( i & 2 ) ? rn.visMins[1] : rn.visMaxs[1] );
		tmp[2] = ( ( i & 4 ) ? rn.visMins[2] : rn.visMaxs[2] );

		dist = DistanceSquared( tmp, rn.viewOrigin );
		farclip_dist = max( farclip_dist, dist );
	}

	rn.visFarClip = sqrt( farclip_dist );
	return rn.visFarClip;
}

/*
* R_SetFarClip
*/
static void R_SetFarClip( void )
{
	float farclip;

	if( rn.refdef.rdflags & RDF_NOWORLDMODEL ) {
		rn.farClip = R_DefaultFarClip();
		return;
	}

	farclip = R_SetVisFarClip();

	if( rsh.worldBrushModel->globalfog )
	{
		float fogdist = rsh.worldBrushModel->globalfog->shader->fog_dist;
		if( farclip > fogdist )
			farclip = fogdist;
		else
			rn.clipFlags &= ~16;
	}

	rn.farClip = max( Z_NEAR, farclip ) + Z_BIAS;
}

/*
* R_SetupFrame
*/
static void R_SetupFrame( void )
{
	int viewcluster;
	int viewarea;

	// build the transformation matrix for the given view angles
	VectorCopy( rn.refdef.vieworg, rn.viewOrigin );
	Matrix3_Copy( rn.refdef.viewaxis, rn.viewAxis );

	rn.lod_dist_scale_for_fov = tan( rn.refdef.fov_x * ( M_PI/180 ) * 0.5f );

	// current viewcluster
	if( !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		mleaf_t *leaf;

		VectorCopy( rsh.worldModel->mins, rn.visMins );
		VectorCopy( rsh.worldModel->maxs, rn.visMaxs );

		leaf = Mod_PointInLeaf( rn.pvsOrigin, rsh.worldModel );
		viewcluster = leaf->cluster;
		viewarea = leaf->area;

		if( rf.worldModelSequence != rsh.worldModelSequence ) {
			rf.frameCount = 0;
			rf.viewcluster = -1; // force R_MarkLeaves
			rf.worldModelSequence = rsh.worldModelSequence;

			// load all world images if not yet
			R_FinishLoadingImages();
		}
	}
	else
	{
		viewcluster = -1;
		viewarea = -1;
	}

	rf.oldviewcluster = rf.viewcluster;
	rf.viewcluster = viewcluster;
	rf.viewarea = viewarea;

	rf.frameCount++;
}

/*
* R_SetupViewMatrices
*/
static void R_SetupViewMatrices( void )
{
	refdef_t *rd = &rn.refdef;

	Matrix4_Modelview( rd->vieworg, rd->viewaxis, rn.cameraMatrix );

	if( rd->rdflags & RDF_USEORTHO ) {
		Matrix4_OrthogonalProjection( -rd->ortho_x, rd->ortho_x, -rd->ortho_y, rd->ortho_y, 
			-rn.farClip, rn.farClip, rn.projectionMatrix );
	}
	else {
		Matrix4_PerspectiveProjection( rd->fov_x, rd->fov_y, 
			Z_NEAR, rn.farClip, rf.cameraSeparation, rn.projectionMatrix );
	}

	if( rd->rdflags & RDF_FLIPPED ) {
		rn.projectionMatrix[0] = -rn.projectionMatrix[0];
		rn.renderFlags |= RF_FLIPFRONTFACE;
	}

	Matrix4_Multiply( rn.projectionMatrix, rn.cameraMatrix, rn.cameraProjectionMatrix );
}

/*
* R_Clear
*/
static void R_Clear( int bitMask )
{
	int bits;
	vec4_t envColor;
	bool clearColor = false;
	bool rgbShadow = ( rn.renderFlags & RF_SHADOWMAPVIEW ) && rn.fbColorAttachment != NULL ? true : false;
	bool depthPortal = ( rn.renderFlags & (RF_MIRRORVIEW|RF_PORTALVIEW) ) != 0 && ( rn.renderFlags & RF_PORTAL_CAPTURE ) == 0;

	if( ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) || rgbShadow ) {
		clearColor = rgbShadow;
		Vector4Set( envColor, 1, 1, 1, 1 );
	} else {
		clearColor = !rn.numDepthPortalSurfaces || R_FASTSKY();
		if( rsh.worldBrushModel && rsh.worldBrushModel->globalfog && rsh.worldBrushModel->globalfog->shader ) {
			Vector4Scale( rsh.worldBrushModel->globalfog->shader->fog_color, 1.0/255.0, envColor );
		} else {
			Vector4Scale( mapConfig.environmentColor, 1.0/255.0, envColor );
		}
	}

	bits = 0;
	if( !depthPortal )
		bits |= GL_DEPTH_BUFFER_BIT;
	if( clearColor )
		bits |= GL_COLOR_BUFFER_BIT;
	if( glConfig.stencilBits )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	RB_Clear( bits, envColor[0], envColor[1], envColor[2], 1 );
}

/*
* R_SetupGL
*/
static void R_SetupGL( void )
{
	RB_Scissor( rn.scissor[0], rn.scissor[1], rn.scissor[2], rn.scissor[3] );
	RB_Viewport( rn.viewport[0], rn.viewport[1], rn.viewport[2], rn.viewport[3] );

	if( rn.renderFlags & RF_CLIPPLANE )
	{
		cplane_t *p = &rn.clipPlane;
		Matrix4_ObliqueNearClipping( p->normal, -p->dist, rn.cameraMatrix, rn.projectionMatrix );
	}

	RB_SetZClip( Z_NEAR, rn.farClip );

	RB_SetCamera( rn.viewOrigin, rn.viewAxis );

	RB_SetLightParams( rn.refdef.minLight, rn.refdef.rdflags & RDF_NOWORLDMODEL ? true : false );

	RB_SetRenderFlags( rn.renderFlags );

	RB_LoadProjectionMatrix( rn.projectionMatrix );

	RB_LoadCameraMatrix( rn.cameraMatrix );

	RB_LoadObjectMatrix( mat4x4_identity );

	if( rn.renderFlags & RF_FLIPFRONTFACE )
		RB_FlipFrontFace();

	if( ( rn.renderFlags & RF_SHADOWMAPVIEW ) && glConfig.ext.shadow )
		RB_SetShaderStateMask( ~0, GLSTATE_NO_COLORWRITE );
}

/*
* R_EndGL
*/
static void R_EndGL( void )
{
	if( ( rn.renderFlags & RF_SHADOWMAPVIEW ) && glConfig.ext.shadow )
		RB_SetShaderStateMask( ~0, 0 );

	if( rn.renderFlags & RF_FLIPFRONTFACE )
		RB_FlipFrontFace();
}

/*
* R_DrawEntities
*/
static void R_DrawEntities( void )
{
	unsigned int i;
	entity_t *e;
	bool shadowmap = ( ( rn.renderFlags & RF_SHADOWMAPVIEW ) != 0 );
	bool culled = true;

	if( rn.renderFlags & RF_ENVVIEW )
	{
		for( i = 0; i < rsc.numBmodelEntities; i++ )
		{
			e = rsc.bmodelEntities[i];
			if( !r_lerpmodels->integer )
				e->backlerp = 0;
			e->outlineHeight = rsc.worldent->outlineHeight;
			Vector4Copy( rsc.worldent->outlineRGBA, e->outlineColor );
			R_AddBrushModelToDrawList( e );
		}
		return;
	}

	for( i = rsc.numLocalEntities; i < rsc.numEntities; i++ )
	{
		e = R_NUM2ENT(i);
		culled = true;

		if( !r_lerpmodels->integer )
			e->backlerp = 0;

		switch( e->rtype )
		{
		case RT_MODEL:
			if( !e->model ) {
				R_AddNullSurfToDrawList( e );
				continue;
			}

			switch( e->model->type )
			{
			case mod_alias:
				culled = ! R_AddAliasModelToDrawList( e );
				break;
			case mod_skeletal:
				culled = ! R_AddSkeletalModelToDrawList( e );
				break;
			case mod_brush:
				e->outlineHeight = rsc.worldent->outlineHeight;
				Vector4Copy( rsc.worldent->outlineRGBA, e->outlineColor );
				culled = ! R_AddBrushModelToDrawList( e );
			default:
				break;
			}
			break;
		case RT_SPRITE:
			culled = ! R_AddSpriteToDrawList( e );
			break;
		default:
			break;
		}

		if( shadowmap && !culled ) {
			if( rsc.entShadowGroups[i] != rn.shadowGroup->id ||
				r_shadows_self_shadow->integer ) {
				// not from the casting group, mark as shadowed
				rsc.entShadowBits[i] |= rn.shadowGroup->bit;
			}
		}
	}
}

//=======================================================================

/*
* R_BindRefInstFBO
*/
static void R_BindRefInstFBO( void )
{
	int fbo;

	if( rn.fbColorAttachment ) {
		fbo = rn.fbColorAttachment->fbo;
	}
	else if( rn.fbDepthAttachment ) {
		fbo = rn.fbDepthAttachment->fbo;
	}
	else {
		fbo = 0;
	}

	R_BindFrameBufferObject( fbo );
}

/*
* R_RenderView
*/
void R_RenderView( const refdef_t *fd )
{
	int msec = 0;
	bool shadowMap = rn.renderFlags & RF_SHADOWMAPVIEW ? true : false;

	rn.refdef = *fd;
	rn.numVisSurfaces = 0;

	// load view matrices with default far clip value
	R_SetupViewMatrices();

	rn.fog_eye = NULL;

	rn.shadowBits = 0;
	rn.dlightBits = 0;
	
	rn.numPortalSurfaces = 0;
	rn.numDepthPortalSurfaces = 0;
	rn.skyportalSurface = NULL;

	ClearBounds( rn.visMins, rn.visMaxs );

	R_ClearSky();

	if( r_novis->integer ) {
		rn.renderFlags |= RF_NOVIS;
	}

	if( r_lightmap->integer ) {
		rn.renderFlags |= RF_LIGHTMAP;
	}

	if( r_drawflat->integer ) {
		rn.renderFlags |= RF_DRAWFLAT;
	}

	R_ClearDrawList( rn.meshlist );

	R_ClearDrawList( rn.portalmasklist );

	if( !rsh.worldModel && !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Com_Error( ERR_DROP, "R_RenderView: NULL worldmodel" );

	R_SetupFrame();

	R_SetupFrustum( &rn.refdef, rn.farClip, rn.frustum );

	// we know the initial farclip at this point after determining visible world leafs
	// R_DrawEntities can make adjustments as well

	if( !shadowMap ) {
		if( r_speeds->integer )
			msec = ri.Sys_Milliseconds();
		R_MarkLeaves();
		if( r_speeds->integer )
			rf.stats.t_mark_leaves += ( ri.Sys_Milliseconds() - msec );

		if( ! ( rn.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			R_DrawWorld();

			if( !rn.numVisSurfaces ) {
				// no world surfaces visible
				return;
			}

			rn.fog_eye = R_FogForSphere( rn.viewOrigin, 0.5 );
		}

		R_DrawCoronas();

		if( r_speeds->integer )
			msec = ri.Sys_Milliseconds();
		R_DrawPolys();
		if( r_speeds->integer )
			rf.stats.t_add_polys += ( ri.Sys_Milliseconds() - msec );
	}

	if( r_speeds->integer )
		msec = ri.Sys_Milliseconds();
	R_DrawEntities();
	if( r_speeds->integer )
		rf.stats.t_add_entities += ( ri.Sys_Milliseconds() - msec );

	if( !shadowMap ) {
		// now set  the real far clip value and reload view matrices
		R_SetFarClip();

		R_SetupViewMatrices();

		// render to depth textures, mark shadowed entities and surfaces
		R_DrawShadowmaps();
	}

	R_SortDrawList( rn.meshlist );

	R_BindRefInstFBO();

	R_SetupGL();

	R_DrawPortals();

	if( r_portalonly->integer && !( rn.renderFlags & ( RF_MIRRORVIEW|RF_PORTALVIEW ) ) )
		return;

	R_Clear( ~0 );

	if( r_speeds->integer )
		msec = ri.Sys_Milliseconds();
	R_DrawSurfaces( rn.meshlist );
	if( r_speeds->integer )
		rf.stats.t_draw_meshes += ( ri.Sys_Milliseconds() - msec );

	rf.stats.c_slices_verts += rn.meshlist->numSliceVerts;
	rf.stats.c_slices_verts_real += rn.meshlist->numSliceVertsReal;

	rf.stats.c_slices_elems += rn.meshlist->numSliceElems;
	rf.stats.c_slices_elems_real += rn.meshlist->numSliceElemsReal;

	if( r_showtris->integer )
		R_DrawOutlinedSurfaces( rn.meshlist );

	R_TransformForWorld();

	R_EndGL();
}

#define REFINST_STACK_SIZE	64
static refinst_t riStack[REFINST_STACK_SIZE];
static unsigned int riStackSize;

/*
* R_ClearRefInstStack
*/
void R_ClearRefInstStack( void )
{
	riStackSize = 0;
}

/*
* R_PushRefInst
*/
bool R_PushRefInst( void )
{
	if( riStackSize == REFINST_STACK_SIZE ) {
		return false;
	}
	riStack[riStackSize++] = rn;
	R_EndGL();
	return true;
}

/*
* R_PopRefInst
*/
void R_PopRefInst( void )
{
	if( !riStackSize ) {
		return;
	}

	rn = riStack[--riStackSize];
	R_BindRefInstFBO();

	R_SetupGL();
}

//=======================================================================

/*
* R_SwapInterval
*/
static void R_SwapInterval( int swapInterval )
{
	static int currentSwapInterval = 0;

	if( ( swapInterval != currentSwapInterval ) && !r_swapinterval_min->integer && !glConfig.stereoEnabled )
	{
		GLimp_SetSwapInterval( swapInterval );
		currentSwapInterval = swapInterval;
	}
}

/*
* R_UpdateHWGamma
*/
static void R_UpdateHWGamma( void )
{
	int i, v;
	double invGamma, div;
	unsigned short gammaRamp[3*GAMMARAMP_STRIDE];

	if( !glConfig.hwGamma )
		return;

	invGamma = 1.0 / bound( 0.5, r_gamma->value, 3.0 );
	div = (double)( 1 << 0 ) / (glConfig.gammaRampSize - 0.5);

	for( i = 0; i < glConfig.gammaRampSize; i++ )
	{
		v = ( int )( 65535.0 * pow( ( (double)i + 0.5 ) * div, invGamma ) + 0.5 );
		gammaRamp[i] = gammaRamp[i + GAMMARAMP_STRIDE] = gammaRamp[i + 2*GAMMARAMP_STRIDE] = ( ( unsigned short )bound( 0, v, 65535 ) );
	}

	GLimp_SetGammaRamp( GAMMARAMP_STRIDE, glConfig.gammaRampSize, gammaRamp );
}

/*
* R_ScreenDisabled
*/
bool R_ScreenEnabled( void )
{
	return GLimp_ScreenEnabled();
}

/*
* R_BeginFrame
*/
void R_BeginFrame( float cameraSeparation, bool forceClear, bool forceVsync )
{
	GLimp_BeginFrame();

	RB_BeginFrame();

	if( !glConfig.stereoEnabled || !R_ScreenEnabled() )
		cameraSeparation = 0;

	if( rf.cameraSeparation != cameraSeparation )
	{
		rf.cameraSeparation = cameraSeparation;
#ifdef GL_ES_VERSION_2_0
		if( glConfig.ext.multiview_draw_buffers )
		{
			int location = GL_MULTIVIEW_EXT;
			int index = ( cameraSeparation > 0 ) ? 1 : 0;
			qglDrawBuffersIndexedEXT( 1, &location, &index );
		}
#else
		if( cameraSeparation < 0 )
			qglDrawBuffer( GL_BACK_LEFT );
		else if( cameraSeparation > 0 )
			qglDrawBuffer( GL_BACK_RIGHT );
		else
			qglDrawBuffer( GL_BACK );
#endif
	}

	// update gamma
	if( r_gamma->modified )
	{
		r_gamma->modified = false;
		R_UpdateHWGamma();
	}

	if( r_wallcolor->modified || r_floorcolor->modified ) {
		int i;

		// parse and clamp colors for walls and floors we will copy into our texture
		sscanf( r_wallcolor->string,  "%3f %3f %3f", &rsh.wallColor[0], &rsh.wallColor[1], &rsh.wallColor[2] );
		sscanf( r_floorcolor->string, "%3f %3f %3f", &rsh.floorColor[0], &rsh.floorColor[1], &rsh.floorColor[2] );
		for( i = 0; i < 3; i++ ) {
			rsh.wallColor[i] = bound( 0, floor(rsh.wallColor[i]) / 255.0, 1.0 );
			rsh.floorColor[i] = bound( 0, floor(rsh.floorColor[i]) / 255.0, 1.0 );
		}

		r_wallcolor->modified = r_floorcolor->modified = false;
	}

	// run cinematic passes on shaders
	R_RunAllCinematics();

	// draw buffer stuff
	if( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;

#ifndef GL_ES_VERSION_2_0
		if( cameraSeparation == 0 || !glConfig.stereoEnabled )
		{
			if( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
#endif
	}

	if( forceClear )
	{
		RB_Clear( GL_COLOR_BUFFER_BIT, 0, 0, 0, 1 );
	}

	// texturemode stuff
	if( r_texturemode->modified )
	{
		R_TextureMode( r_texturemode->string );
		r_texturemode->modified = false;
	}

	if( r_texturefilter->modified )
	{
		R_AnisotropicFilter( r_texturefilter->integer );
		r_texturefilter->modified = false;
	}

	// keep r_outlines_cutoff value in sane bounds to prevent wallhacking
	if( r_outlines_scale->modified ) {
		if( r_outlines_scale->value < 0 ) {
			ri.Cvar_ForceSet( r_outlines_scale->name, "0" );
		}
		else if( r_outlines_scale->value > 3 ) {
			ri.Cvar_ForceSet( r_outlines_scale->name, "3" );
		}
		r_outlines_scale->modified = false;
	}

	// set swap interval (vertical synchronization)
	R_SwapInterval( ( r_swapinterval->integer || forceVsync ) ? 1 : 0 );

	memset( &rf.stats, 0, sizeof( rf.stats ) );

	R_Set2DMode( true );
}

/*
* R_EndFrame
*/
void R_EndFrame( void )
{
	// render previously batched 2D geometry, if any
	RB_FlushDynamicMeshes();

	R_PolyBlend();
	
	R_ApplyBrightness();

	// reset the 2D state so that the mode will be 
	// properly set back again in R_BeginFrame
	R_Set2DMode( false );

	RB_EndFrame();

	GLimp_EndFrame();
}

/*
* R_AppActivate
*/
void R_AppActivate( bool active, bool destroy )
{
	qglFlush();
	GLimp_AppActivate( active, destroy );
}

/*
* R_SpeedsMessage
*/
const char *R_SpeedsMessage( char *out, size_t size )
{
	char backend_msg[1024];

	if( !out || !size ) {
		return out;
	}

	out[0] = '\0';
	if( r_speeds->integer && !( rn.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		switch( r_speeds->integer )
		{
		case 1:
		default:
			RB_StatsMessage( backend_msg, sizeof( backend_msg ) );

			Q_snprintfz( out, size,
				"%4u wpoly %4u leafs\n"
				"sverts: %5u\\%5u  stris: %5u\\%5u\n"
				"%s",
				rf.stats.c_brush_polys, rf.stats.c_world_leafs,
				rf.stats.c_slices_verts, rf.stats.c_slices_verts_real, rf.stats.c_slices_elems/3, rf.stats.c_slices_elems_real/3,
				backend_msg
			);
			break;
		case 2:
		case 3:
			Q_snprintfz( out, size,
				"lvs: %5u  node: %5u\n"
				"polys\\ents: %5u\\%5i  draw: %5u\n",
				rf.stats.t_mark_leaves, rf.stats.t_world_node,
				rf.stats.t_add_polys, rf.stats.t_add_entities, rf.stats.t_draw_meshes
			);
			break;
		case 4:
		case 5:
			if( rsc.debugSurface )
			{
				int numVerts = 0, numTris = 0;

				Q_snprintfz( out, size,
					"%s type:%i sort:%i", 
					rsc.debugSurface->shader->name, rsc.debugSurface->facetype, rsc.debugSurface->shader->sort );

				Q_strncatz( out, "\n", size );

				if( r_speeds->integer == 5 && rsc.debugSurface->drawSurf->vbo ) {
					numVerts = rsc.debugSurface->drawSurf->vbo->numVerts;
					numTris = rsc.debugSurface->drawSurf->vbo->numElems / 3;
				}
				else if( rsc.debugSurface->mesh ) {
					numVerts = rsc.debugSurface->mesh->numVerts;
					numTris = rsc.debugSurface->mesh->numElems;
				}

				if( numVerts ) {
					Q_snprintfz( out + strlen( out ), size - strlen( out ),
						"verts: %5i tris: %5i", numVerts, numTris );
				}

				Q_strncatz( out, "\n", size );

				if( rsc.debugSurface->fog && rsc.debugSurface->fog->shader
					&& rsc.debugSurface->fog->shader != rsc.debugSurface->shader )
					Q_strncatz( out, rsc.debugSurface->fog->shader->name, size );
			}
			break;
		case 6:
			Q_snprintfz( out, size,
				"%.1f %.1f %.1f",
				rn.refdef.vieworg[0], rn.refdef.vieworg[1], rn.refdef.vieworg[2]
				);
			break;
		}
	}

	out[size-1] = '\0';
	return out;
}

//==================================================================================

/*
* R_BeginAviDemo
*/
void R_BeginAviDemo( void )
{
}

/*
* R_WriteAviFrame
*/
void R_WriteAviFrame( int frame, bool scissor )
{
	int x, y, w, h;
	int quality;
	const char *writedir, *gamedir;
	size_t checkname_size;
	char *checkname;
	const char *extension;

	if( !R_ScreenEnabled() )
		return;

	if( scissor )
	{
		x = rsc.refdef.x;
		y = glConfig.height - rsc.refdef.height - rsc.refdef.y;
		w = rsc.refdef.width;
		h = rsc.refdef.height;
	}
	else
	{
		x = 0;
		y = 0;
		w = glConfig.width;
		h = glConfig.height;
	}

	if( r_screenshot_jpeg->integer ) {
		extension = ".jpg";
		quality = r_screenshot_jpeg_quality->integer;
	}
	else {
		extension = ".tga";
		quality = 100;
	}

	writedir = ri.FS_WriteDirectory();
	gamedir = ri.FS_GameDirectory();
	checkname_size = strlen( writedir ) + 1 + strlen( gamedir ) + strlen( "/avi/avi" ) + 6 + strlen( extension ) + 1;
	checkname = alloca( checkname_size );
	Q_snprintfz( checkname, checkname_size, "%s/%s/avi/avi%06i", writedir, gamedir, frame );
	COM_DefaultExtension( checkname, extension, checkname_size );

	R_ScreenShot( checkname, x, y, w, h, quality, false, false, false, true );
}

/*
* R_StopAviDemo
*/
void R_StopAviDemo( void )
{
}

/*
* R_TransformVectorToScreen
*/
void R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out )
{
	mat4_t p, m;
	vec4_t temp, temp2;

	if( !rd || !in || !out )
		return;

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;
	
	if( rd->rdflags & RDF_USEORTHO ) {
		Matrix4_OrthogonalProjection( rd->ortho_x, rd->ortho_x, rd->ortho_y, rd->ortho_y, 
			-4096.0f, 4096.0f, p );
	}
	else {
		Matrix4_InfinitePerspectiveProjection( rd->fov_x, rd->fov_y, Z_NEAR, rf.cameraSeparation, 
			p, glConfig.depthEpsilon );
	}

	if( rd->rdflags & RDF_FLIPPED ) {
		p[0] = -p[0];
	}

	Matrix4_Modelview( rd->vieworg, rd->viewaxis, m );

	Matrix4_Multiply_Vector( m, temp, temp2 );
	Matrix4_Multiply_Vector( p, temp2, temp );

	if( !temp[3] )
		return;

	out[0] = rd->x + ( temp[0] / temp[3] + 1.0f ) * rd->width * 0.5f;
	out[1] = glConfig.height - (rd->y + ( temp[1] / temp[3] + 1.0f ) * rd->height * 0.5f);
}

/*
* R_GetShaderForOrigin
*
* Trace 64 units in all axial directions to find the closest surface
*/
shader_t *R_GetShaderForOrigin( const vec3_t origin )
{
	int i, j;
	vec3_t dir, end;
	rtrace_t tr;
	shader_t *best = NULL;
	float best_frac = 1000.0f;

	for( i = 0; i < 3; i++ ) {
		VectorClear( dir );

		for( j = -1; j <= 1; j += 2 ) {
			dir[i] = j;
			VectorMA( origin, 64, dir, end );

			R_TraceLine( &tr, origin, end, 0 );
			if( !tr.shader ) {
				continue;
			}

			if( tr.fraction < best_frac ) {
				best = tr.shader;
				best_frac = tr.fraction;
			}
		}
	}

	return best;
}

/*
* R_GetShaderCinematic
*/
struct cinematics_s *R_GetShaderCinematic( shader_t *shader )
{
	if( !shader ) {
		return NULL;
	}
	return R_GetCinematicById( shader->cin );
}

//===================================================================

/*
* R_NormToLatLong
*/
void R_NormToLatLong( const vec_t *normal, uint8_t latlong[2] )
{
	float flatlong[2];

	NormToLatLong( normal, flatlong );
	latlong[0] = (int)( flatlong[0] * 255.0 / M_TWOPI ) & 255;
	latlong[1] = (int)( flatlong[1] * 255.0 / M_TWOPI ) & 255;
}

/*
* R_LatLongToNorm4
*/
void R_LatLongToNorm4( const uint8_t latlong[2], vec4_t out )
{
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
void R_LatLongToNorm( const uint8_t latlong[2], vec3_t out )
{
	vec4_t t;
	R_LatLongToNorm4( latlong, t );
	VectorCopy( t, out );
}

/*
* R_CopyString
*/
char *R_CopyString_( const char *in, const char *filename, int fileline )
{
	char *out;

	out = ri.Mem_AllocExt( r_mempool, ( strlen( in ) + 1 ), 0, 1, filename, fileline );
	strcpy( out, in );

	return out;
}

/*
* R_LoadFile
*/
int R_LoadFile_( const char *path, int flags, void **buffer, const char *filename, int fileline )
{
	uint8_t *buf;
	unsigned int len;
	int fhandle;

	buf = NULL; // quiet compiler warning

	// look for it in the filesystem or pack files
	len = ri.FS_FOpenFile( path, &fhandle, FS_READ|flags );

	if( !fhandle )
	{
		if( buffer )
			*buffer = NULL;
		return -1;
	}

	if( !buffer )
	{
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
void R_FreeFile_( void *buffer, const char *filename, int fileline )
{
	ri.Mem_Free( buffer, filename, fileline );
}
