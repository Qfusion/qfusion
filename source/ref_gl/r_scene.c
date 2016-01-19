/*
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

#include "r_local.h"

static void R_ClearDebugBounds( void );
static void R_RenderDebugBounds( void );

/*
* R_ClearScene
*/
void R_ClearScene( void )
{
	rsc.numLocalEntities = 0;
	rsc.numDlights = 0;
	rsc.numPolys = 0;

	rsc.worldent = R_NUM2ENT( rsc.numLocalEntities );
	rsc.worldent->scale = 1.0f;
	rsc.worldent->model = rsh.worldModel;
	rsc.worldent->rtype = RT_MODEL;
	Matrix3_Identity( rsc.worldent->axis );
	rsc.numLocalEntities++;

	rsc.polyent = R_NUM2ENT( rsc.numLocalEntities );
	rsc.polyent->scale = 1.0f;
	rsc.polyent->model = NULL;
	rsc.polyent->rtype = RT_MODEL;
	Matrix3_Identity( rsc.polyent->axis );
	rsc.numLocalEntities++;

	rsc.skyent = R_NUM2ENT( rsc.numLocalEntities );
	*rsc.skyent = *rsc.worldent;
	rsc.numLocalEntities++;

	rsc.numEntities = rsc.numLocalEntities;

	rsc.numBmodelEntities = 0;

	rsc.renderedShadowBits = 0;
	rsc.frameCount++;

	R_ClearDebugBounds();

	R_ClearShadowGroups();

	R_ClearSkeletalCache();
}

/*
* R_AddEntityToScene
*/
void R_AddEntityToScene( const entity_t *ent )
{
	if( !r_drawentities->integer )
		return;

	if( ( ( rsc.numEntities - rsc.numLocalEntities ) < MAX_ENTITIES ) && ent )
	{
		int eNum = rsc.numEntities;
		entity_t *de = R_NUM2ENT(eNum);

		*de = *ent;
		if( r_outlines_scale->value <= 0 )
			de->outlineHeight = 0;
		rsc.entShadowBits[eNum] = 0;
		rsc.entShadowGroups[eNum] = 0;

		if( de->rtype == RT_MODEL ) {
			if( de->model && de->model->type == mod_brush ) {
				rsc.bmodelEntities[rsc.numBmodelEntities++] = de;
			}
			if( !(de->renderfx & RF_NOSHADOW) ) {
				R_AddLightOccluder( de ); // build groups and mark shadow casters
			}
		}
		else if( de->rtype == RT_SPRITE ) {
			// simplifies further checks
			de->model = NULL;
		}

		if( de->renderfx & RF_ALPHAHACK ) {
			if( de->shaderRGBA[3] == 255 ) {
				de->renderfx &= ~RF_ALPHAHACK;
			}
		}

		rsc.numEntities++;

		// add invisible fake entity for depth write
		if( (de->renderfx & (RF_WEAPONMODEL|RF_ALPHAHACK)) == (RF_WEAPONMODEL|RF_ALPHAHACK) ) {
			entity_t tent = *ent;
			tent.renderfx &= ~RF_ALPHAHACK;
			tent.renderfx |= RF_NOCOLORWRITE|RF_NOSHADOW;
			R_AddEntityToScene( &tent );
		}
	}
}

/*
* R_AddLightToScene
*/
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )
{
	if( ( rsc.numDlights < MAX_DLIGHTS ) && intensity && ( r != 0 || g != 0 || b != 0 ) )
	{
		dlight_t *dl = &rsc.dlights[rsc.numDlights];

		VectorCopy( org, dl->origin );
		dl->intensity = intensity * DLIGHT_SCALE;
		dl->color[0] = r;
		dl->color[1] = g;
		dl->color[2] = b;

		if( r_lighting_grayscale->integer ) {
			vec_t grey = ColorGrayscale( dl->color );
			dl->color[0] = dl->color[1] = dl->color[2] = bound( 0, grey, 1 );
		}

		rsc.numDlights++;
	}
}

/*
* R_AddPolyToScene
*/
void R_AddPolyToScene( const poly_t *poly )
{
	assert( sizeof( *poly->elems ) == sizeof( elem_t ) );

	if( ( rsc.numPolys < MAX_POLYS ) && poly && poly->numverts )
	{
		drawSurfacePoly_t *dp = &rsc.polys[rsc.numPolys];

		assert( poly->shader != NULL );
		if( !poly->shader ) {
			return;
		}

		dp->type = ST_POLY;
		dp->shader = poly->shader;
		dp->numVerts = min( poly->numverts, MAX_POLY_VERTS );
		dp->xyzArray = poly->verts;
		dp->normalsArray = poly->normals;
		dp->stArray = poly->stcoords;
		dp->colorsArray = poly->colors;
		dp->numElems = poly->numelems;
		dp->elems = ( elem_t * )poly->elems;
		dp->fogNum = poly->fognum;

		// if fogNum is unset, we need to find the volume for polygon bounds
		if( !dp->fogNum ) {
			int i;
			mfog_t *fog;
			vec3_t dpmins, dpmaxs;

			ClearBounds( dpmins, dpmaxs );

			for( i = 0; i < dp->numVerts; i++ ) {
				AddPointToBounds( dp->xyzArray[i], dpmins, dpmaxs );
			}

			fog = R_FogForBounds( dpmins, dpmaxs );
			dp->fogNum = (fog ? fog - rsh.worldBrushModel->fogs + 1 : -1);
		}

		rsc.numPolys++;
	}
}

/*
* R_AddLightStyleToScene
*/
void R_AddLightStyleToScene( int style, float r, float g, float b )
{
	lightstyle_t *ls;

	if( style < 0 || style >= MAX_LIGHTSTYLES )
		ri.Com_Error( ERR_DROP, "R_AddLightStyleToScene: bad light style %i", style );

	ls = &rsc.lightStyles[style];
	ls->rgb[0] = max( 0, r );
	ls->rgb[1] = max( 0, g );
	ls->rgb[2] = max( 0, b );
}

/*
* R_BlitTextureToScrFbo
*/
static void R_BlitTextureToScrFbo( const refdef_t *fd, image_t *image, int dstFbo, 
	int program_type, const vec4_t color, int blendMask, int numShaderImages, image_t **shaderImages )
{
	int x, y;
	int w, h, fw, fh;
	static char s_name[] = "$builtinpostprocessing";
	static shaderpass_t p;
	static shader_t s;
	int i;
	static tcmod_t tcmod;
	mat4_t m;

	assert( rsh.postProcessingVBO );

	// blit + flip using a static mesh to avoid redundant buffer uploads
	// (also using custom PP effects like FXAA with the stream VBO causes
	// Adreno to mark the VBO as "slow" (due to some weird bug)
	// for the rest of the frame and drop FPS to 10-20).
	RB_FlushDynamicMeshes();

	RB_BindFrameBufferObject( dstFbo );

	if( !dstFbo ) {
		// default framebuffer
		// set the viewport to full resolution
		// but keep the scissoring region
		x = fd->x;
		y = fd->y;
		w = fw = fd->width;
		h = fh = fd->height;
		RB_Viewport( 0, 0, glConfig.width, glConfig.height );
		RB_Scissor( rn.scissor[0], rn.scissor[1], rn.scissor[2], rn.scissor[3] );
	}
	else {
		// aux framebuffer
		// set the viewport to full resolution of the framebuffer (without the NPOT padding if there's one)
		// draw quad on the whole framebuffer texture
		// set scissor to default framebuffer resolution
		image_t *cb = RFB_GetObjectTextureAttachment( dstFbo, false );
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

	s.vattribs = VATTRIB_POSITION_BIT|VATTRIB_TEXCOORDS_BIT;
	s.sort = SHADER_SORT_NEAREST;
	s.numpasses = 1;
	s.name = s_name;
	s.passes = &p;

	p.rgbgen.type = RGB_GEN_IDENTITY;
	p.alphagen.type = ALPHA_GEN_IDENTITY;
	p.tcgen = TC_GEN_NONE;
	p.images[0] = image;
	for( i = 0; i < numShaderImages; i++ )
		p.images[i + 1] = shaderImages[i];
	p.flags = blendMask;
	p.program_type = program_type;

	if( !dstFbo ) {
		tcmod.type = TC_MOD_TRANSFORM;
		tcmod.args[0] = ( float )( w ) / ( float )( image->upload_width );
		tcmod.args[1] = ( float )( h ) / ( float )( image->upload_height );
		tcmod.args[4] = ( float )( x ) / ( float )( image->upload_width );
		tcmod.args[5] = ( float )( image->upload_height - h - y ) / ( float )( image->upload_height );
		p.numtcmods = 1;
		p.tcmods = &tcmod;
	}
	else {
		p.numtcmods = 0;
	}

	Matrix4_Identity( m );
	Matrix4_Scale2D( m, fw, fh );
	Matrix4_Translate2D( m, x, y );
	RB_LoadObjectMatrix( m );

	RB_BindShader( NULL, &s, NULL );
	RB_BindVBO( rsh.postProcessingVBO->index, GL_TRIANGLES );
	RB_DrawElements( 0, 4, 0, 6, 0, 0, 0, 0 );

	RB_LoadObjectMatrix( mat4x4_identity );

	// restore 2D viewport and scissor
	RB_Viewport( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
	RB_Scissor( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
}

/*
* R_RenderScene
*/
void R_RenderScene( const refdef_t *fd )
{
	int fbFlags = 0;
	int ppFrontBuffer = 0;
	image_t *ppSource;

	if( r_norefresh->integer )
		return;

	R_Set2DMode( false );

	RB_SetTime( fd->time );

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) )
		rsc.refdef = *fd;

	rn.refdef = *fd;
	if( !rn.refdef.minLight ) {
		rn.refdef.minLight = 0.1f;
	}

	fd = &rn.refdef;

	rn.renderFlags = RF_NONE;

	rn.farClip = R_DefaultFarClip();
	rn.clipFlags = 15;
	if( rsh.worldModel && !( fd->rdflags & RDF_NOWORLDMODEL ) && rsh.worldBrushModel->globalfog )
		rn.clipFlags |= 16;
	rn.meshlist = &r_worldlist;
	rn.portalmasklist = &r_portalmasklist;
	rn.shadowBits = 0;
	rn.dlightBits = 0;
	rn.shadowGroup = NULL;

	fbFlags = 0;
	rn.fbColorAttachment = rn.fbDepthAttachment = NULL;
	
	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		if( r_soft_particles->integer && ( rsh.screenTexture != NULL ) ) {
			rn.fbColorAttachment = rsh.screenTexture;
			rn.fbDepthAttachment = rsh.screenDepthTexture;
			rn.renderFlags |= RF_SOFT_PARTICLES;
			fbFlags |= 1;
		}

		if( rsh.screenPPCopies[0] && rsh.screenPPCopies[1] ) {
			int oldFlags = fbFlags;
			shader_t *cc = rn.refdef.colorCorrection;

			if( r_fxaa->integer ) {
				fbFlags |= 2;
			}

			if( cc && cc->numpasses > 0 && cc->passes[0].images[0] && cc->passes[0].images[0] != rsh.noTexture ) {
				fbFlags |= 4;
			}

			if( fbFlags != oldFlags ) {
				if( !rn.fbColorAttachment ) {
					rn.fbColorAttachment = rsh.screenPPCopies[0];
					ppFrontBuffer = 1;
				}
			}
		}
	}

	ppSource = rn.fbColorAttachment;

	// clip new scissor region to the one currently set
	Vector4Set( rn.scissor, fd->scissor_x, fd->scissor_y, fd->scissor_width, fd->scissor_height );
	Vector4Set( rn.viewport, fd->x, fd->y, fd->width, fd->height );
	VectorCopy( fd->vieworg, rn.pvsOrigin );
	VectorCopy( fd->vieworg, rn.lodOrigin );

	R_BindFrameBufferObject( 0 );

	R_BuildShadowGroups();

	R_RenderView( fd );

	R_RenderDebugSurface( fd );

	R_RenderDebugBounds();

	R_BindFrameBufferObject( 0 );

	R_Set2DMode( true );

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Mutex_Lock( rf.speedsMsgLock );
		R_WriteSpeedsMessage( rf.speedsMsg, sizeof( rf.speedsMsg ) );
		ri.Mutex_Unlock( rf.speedsMsgLock );
	}

	// blit and blend framebuffers in proper order

	if( fbFlags == 1 ) {
		// only blit soft particles directly when we don't have any other post processing
		// otherwise use the soft particles FBO as the base texture on the next layer
		// to avoid wasting time on resolves and the fragment shader to blit to a temp texture
		R_BlitTextureToScrFbo( fd,
			ppSource, 0,
			GLSL_PROGRAM_TYPE_NONE,
			colorWhite, 0,
			0, NULL );
	}
	fbFlags &= ~1;

	// apply FXAA
	if( fbFlags & 2 ) {
		image_t *dest;

		fbFlags &= ~2;
		dest = fbFlags ? rsh.screenPPCopies[ppFrontBuffer] : NULL;

		R_BlitTextureToScrFbo( fd,
			ppSource, dest ? dest->fbo : 0,
			GLSL_PROGRAM_TYPE_FXAA,
			colorWhite, 0,
			0, NULL );

		ppFrontBuffer ^= 1;
		ppSource = dest;
	}

	// apply color correction
	if( fbFlags & 4 ) {
		image_t *dest;

		fbFlags &= ~4;
		dest = fbFlags ? rsh.screenPPCopies[ppFrontBuffer] : NULL;

		R_BlitTextureToScrFbo( fd,
			ppSource, dest ? dest->fbo : 0,
			GLSL_PROGRAM_TYPE_COLORCORRECTION,
			colorWhite, 0,
			1, &( rn.refdef.colorCorrection->passes[0].images[0] ) );
	}
}

/*
=============================================================================

BOUNDING BOXES

=============================================================================
*/

typedef struct
{
	vec3_t mins;
	vec3_t maxs;
	byte_vec4_t color;
} r_debug_bound_t;

static unsigned r_num_debug_bounds;
static size_t r_debug_bounds_current_size;
static r_debug_bound_t *r_debug_bounds;

/*
* R_ClearDebugBounds
*/
static void R_ClearDebugBounds( void )
{
	r_num_debug_bounds = 0;
}

/*
* R_AddDebugBounds
*/
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs, const byte_vec4_t color )
{
	unsigned i;

	i = r_num_debug_bounds;
	r_num_debug_bounds++;

	if( r_num_debug_bounds > r_debug_bounds_current_size )
	{
		r_debug_bounds_current_size = ALIGN( r_num_debug_bounds, 256 );
		if( r_debug_bounds )
			r_debug_bounds = R_Realloc( r_debug_bounds, r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
		else
			r_debug_bounds = R_Malloc( r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
	}

	VectorCopy( mins, r_debug_bounds[i].mins );
	VectorCopy( maxs, r_debug_bounds[i].maxs );
	Vector4Copy( color, r_debug_bounds[i].color );
}

/*
* R_RenderDebugBounds
*/
static void R_RenderDebugBounds( void )
{
	unsigned i, j;
	const vec_t *mins, *maxs;
	const uint8_t *color;
	mesh_t mesh;
	vec4_t verts[8];
	byte_vec4_t colors[8];
	elem_t elems[24] =
	{
		0, 1, 1, 3, 3, 2, 2, 0,
		0, 4, 1, 5, 2, 6, 3, 7,
		4, 5, 5, 7, 7, 6, 6, 4
	};

	if( !r_num_debug_bounds )
		return;

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = 8;
	mesh.xyzArray = verts;
	mesh.numElems = 24;
	mesh.elems = elems;
	mesh.colorsArray[0] = colors;

	RB_SetShaderStateMask( ~0, GLSTATE_NO_DEPTH_TEST );

	for( i = 0; i < r_num_debug_bounds; i++ )
	{
		mins = r_debug_bounds[i].mins;
		maxs = r_debug_bounds[i].maxs;
		color = r_debug_bounds[i].color;

		for( j = 0; j < 8; j++ )
		{
			verts[j][0] = ( ( j & 1 ) ? mins[0] : maxs[0] );
			verts[j][1] = ( ( j & 2 ) ? mins[1] : maxs[1] );
			verts[j][2] = ( ( j & 4 ) ? mins[2] : maxs[2] );
			verts[j][3] = 1.0f;
			Vector4Copy( color, colors[j] );
		}

		RB_AddDynamicMesh( rsc.worldent, rsh.whiteShader, NULL, NULL, 0, &mesh, GL_LINES, 0.0f, 0.0f );
	}

	RB_FlushDynamicMeshes();

	RB_SetShaderStateMask( ~0, 0 );
}

//=======================================================================
