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

enum {
	PPFX_SOFT_PARTICLES,
	PPFX_TONE_MAPPING,
	PPFX_COLOR_CORRECTION,
	PPFX_OVERBRIGHT_TARGET,
	PPFX_BLOOM,
	PPFX_FXAA,
	PPFX_BLUR,
};

enum {
	PPFX_BIT_SOFT_PARTICLES = RF_BIT( PPFX_SOFT_PARTICLES ),
	PPFX_BIT_TONE_MAPPING = RF_BIT( PPFX_TONE_MAPPING ),
	PPFX_BIT_COLOR_CORRECTION = RF_BIT( PPFX_COLOR_CORRECTION ),
	PPFX_BIT_OVERBRIGHT_TARGET = RF_BIT( PPFX_OVERBRIGHT_TARGET ),
	PPFX_BIT_BLOOM = RF_BIT( PPFX_BLOOM ),
	PPFX_BIT_FXAA = RF_BIT( PPFX_FXAA ),
	PPFX_BIT_BLUR = RF_BIT( PPFX_BLUR ),
};

static void R_ClearDebugBounds( void );
static void R_RenderDebugBounds( void );

/*
* R_ClearScene
*/
void R_ClearScene( void ) {
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

	rsc.polyweapent = R_NUM2ENT( rsc.numLocalEntities );
	rsc.polyweapent->scale = 1.0f;
	rsc.polyweapent->model = NULL;
	rsc.polyweapent->rtype = RT_MODEL;
	rsc.polyweapent->renderfx = RF_WEAPONMODEL;
	Matrix3_Identity( rsc.polyweapent->axis );
	rsc.numLocalEntities++;

	rsc.polyviewerent = R_NUM2ENT( rsc.numLocalEntities );
	rsc.polyviewerent->scale = 1.0f;
	rsc.polyviewerent->model = NULL;
	rsc.polyviewerent->rtype = RT_MODEL;
	rsc.polyviewerent->renderfx = RF_VIEWERMODEL;
	Matrix3_Identity( rsc.polyviewerent->axis );
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
void R_AddEntityToScene( const entity_t *ent ) {
	if( !r_drawentities->integer ) {
		return;
	}

	if( ( ( rsc.numEntities - rsc.numLocalEntities ) < MAX_ENTITIES ) && ent ) {
		int eNum = rsc.numEntities;
		entity_t *de = R_NUM2ENT( eNum );

		*de = *ent;
		if( r_outlines_scale->value <= 0 ) {
			de->outlineHeight = 0;
		}
		rsc.entShadowBits[eNum] = 0;
		rsc.entShadowGroups[eNum] = 0;

		if( de->rtype == RT_MODEL ) {
			if( de->model && de->model->type == mod_brush ) {
				rsc.bmodelEntities[rsc.numBmodelEntities++] = de;
			}
			if( !( de->renderfx & RF_NOSHADOW ) ) {
				R_AddLightOccluder( de ); // build groups and mark shadow casters
			}
		} else if( de->rtype == RT_SPRITE ) {
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
		if( ( de->renderfx & ( RF_WEAPONMODEL | RF_ALPHAHACK ) ) == ( RF_WEAPONMODEL | RF_ALPHAHACK ) ) {
			entity_t tent = *ent;
			tent.renderfx &= ~RF_ALPHAHACK;
			tent.renderfx |= RF_NOCOLORWRITE | RF_NOSHADOW;
			R_AddEntityToScene( &tent );
		}
	}
}

/*
* R_AddLightToScene
*/
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	if( ( rsc.numDlights < MAX_DLIGHTS ) && intensity && ( r != 0 || g != 0 || b != 0 ) ) {
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
void R_AddPolyToScene( const poly_t *poly ) {
	assert( sizeof( *poly->elems ) == sizeof( elem_t ) );

	if( ( rsc.numPolys < MAX_POLYS ) && poly && poly->numverts ) {
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
		dp->renderfx = poly->renderfx;

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
			dp->fogNum = ( fog ? fog - rsh.worldBrushModel->fogs + 1 : -1 );
		}

		rsc.numPolys++;
	}
}

/*
* R_AddLightStyleToScene
*/
void R_AddLightStyleToScene( int style, float r, float g, float b ) {
	lightstyle_t *ls;

	if( style < 0 || style >= MAX_LIGHTSTYLES ) {
		ri.Com_Error( ERR_DROP, "R_AddLightStyleToScene: bad light style %i", style );
		return;
	}

	ls = &rsc.lightStyles[style];
	ls->rgb[0] = max( 0, r );
	ls->rgb[1] = max( 0, g );
	ls->rgb[2] = max( 0, b );
}

/*
* R_BlitTextureToScrFbo
*/
static void R_BlitTextureToScrFbo( const refdef_t *fd, image_t *image, int dstFbo,
								   int program_type, const vec4_t color, int blendMask, int numShaderImages, image_t **shaderImages,
								   int iParam0 ) {
	int x, y;
	int w, h, fw, fh;
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
	RB_DrawElements( 0, 4, 0, 6, 0, 0, 0, 0 );

	RB_LoadObjectMatrix( mat4x4_identity );

	// restore 2D viewport and scissor
	RB_Viewport( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
	RB_Scissor( 0, 0, rf.frameBufferWidth, rf.frameBufferHeight );
}

/*
* R_BlurTextureToScrFbo
*
* Performs Kawase blur which approximates standard Gaussian blur in multiple passes.
* Supposedly performs better on high resolution inputs.
*/
static image_t *R_BlurTextureToScrFbo( const refdef_t *fd, image_t *image, image_t *otherImage ) {
	unsigned i;
	image_t *images[2];
	const int kernel35x35[] =
	{ 0, 1, 2, 2, 3 };     //  equivalent to 35x35 kernel
	;
	const int kernel63x63[] =
	{ 0, 1, 2, 3, 4, 4, 5 }     // equivalent to 63x63 kernel
	;
	const int *kernel;
	unsigned numPasses;

	if( true ) {
		kernel = kernel63x63;
		numPasses = sizeof( kernel63x63 ) / sizeof( kernel63x63[0] );
	} else {
		kernel = kernel35x35;
		numPasses = sizeof( kernel35x35 ) / sizeof( kernel35x35[0] );
	}

	images[0] = image;
	images[1] = otherImage;
	for( i = 0; i < numPasses; i++ ) {
		R_BlitTextureToScrFbo( fd, images[i & 1], images[( i + 1 ) & 1]->fbo, GLSL_PROGRAM_TYPE_KAWASE_BLUR, 
			colorWhite, 0, 0, NULL, kernel[i] );
	}

	return images[( i + 1 ) & 1];
}

/*
* R_RenderScene
*/
void R_RenderScene( const refdef_t *fd ) {
	int l;
	int fbFlags = 0;
	int ppFrontBuffer = 0;
	int samples = 0;
	image_t *ppSource;
	shader_t *cc;
	image_t *bloomTex[NUM_BLOOM_LODS];

	if( r_norefresh->integer ) {
		return;
	}

	R_Set2DMode( false );

	RB_SetTime( fd->time );

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		rsc.refdef = *fd;
	}

	rn.refdef = *fd;
	if( !rn.refdef.minLight ) {
		rn.refdef.minLight = 0.1f;
	}

	fd = &rn.refdef;

	rn.renderFlags = RF_NONE;

	rn.farClip = R_DefaultFarClip();
	rn.clipFlags = 15;
	if( rsh.worldModel && !( fd->rdflags & RDF_NOWORLDMODEL ) && rsh.worldBrushModel->globalfog ) {
		rn.clipFlags |= 16;
	}
	rn.meshlist = &r_worldlist;
	rn.portalmasklist = &r_portalmasklist;
	rn.shadowBits = 0;
	rn.dlightBits = 0;
	rn.shadowGroup = NULL;

	rn.st = &rsh.st;
	rn.renderTarget = 0;
	rn.multisampleDepthResolved = false;

	fbFlags = 0;
	cc = rn.refdef.colorCorrection;
	if( !( cc && cc->numpasses > 0 && cc->passes[0].images[0] && cc->passes[0].images[0] != rsh.noTexture ) ) {
		cc = NULL;
	}

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		if( !glConfig.ext.framebuffer_multisample ) {
			samples = 0;
		} else {
			samples = bound( 0, r_samples->integer, glConfig.maxFramebufferSamples );
		}

		if( glConfig.sSRGB && rsh.stf.screenTex ) {
			rn.st = &rsh.stf;
		}

		// reload the multisample framebuffer if needed
		if( samples > 0 && (  !rn.st->multisampleTarget || RFB_GetSamples( rn.st->multisampleTarget ) != samples ) ) {
			int width, height;
			R_GetRenderBufferSize( glConfig.width, glConfig.height, 0, IT_SPECIAL, &width, &height );

			if( rn.st->multisampleTarget ) {
				RFB_UnregisterObject( rn.st->multisampleTarget );
			}
			rn.st->multisampleTarget = RFB_RegisterObject( width, height, true, true, glConfig.stencilBits != 0, true,
														   samples, rn.st == &rsh.stf );
		}

		// ignore r_samples values below 2 or if we failed to allocate the multisample fb
		if( samples > 1 && rn.st->multisampleTarget != 0 ) {
			rn.renderTarget = rn.st->multisampleTarget;
		} else {
			samples = 0;
		}

		if( r_soft_particles->integer && ( rn.st->screenTex != NULL ) ) {
			// use FBO with depth renderbuffer attached
			if( !rn.renderTarget ) {
				rn.renderTarget = rn.st->screenTex->fbo;
			}
			rn.renderFlags |= RF_SOFT_PARTICLES;
			fbFlags |= PPFX_BIT_SOFT_PARTICLES;
		}

		if( rn.st->screenPPCopies[0] && rn.st->screenPPCopies[1] ) {
			int oldFlags = fbFlags;

			if( rn.st == &rsh.stf ) {
				fbFlags |= PPFX_BIT_TONE_MAPPING | PPFX_BIT_COLOR_CORRECTION;
			}
			if( cc ) {
				fbFlags |= PPFX_BIT_COLOR_CORRECTION;
			}
			if( r_fxaa->integer ) {
				fbFlags |= PPFX_BIT_FXAA;
			}
			if( r_bloom->integer && rn.st == &rsh.stf && rsh.st.screenBloomLodTex[NUM_BLOOM_LODS - 1][1] ) {
				fbFlags |= PPFX_BIT_OVERBRIGHT_TARGET | PPFX_BIT_COLOR_CORRECTION;
			}
			if( fd->rdflags & RDF_BLURRED ) {
				fbFlags |= PPFX_BIT_BLUR;
				fbFlags &= ~PPFX_BIT_FXAA;
			}

			if( fbFlags != oldFlags ) {
				// use a FBO without a depth renderbuffer attached, unless we need one for soft particles
				if( !rn.renderTarget ) {
					rn.renderTarget = rn.st->screenPPCopies[0]->fbo;
					ppFrontBuffer = 1;
				}
			}
		}
	}

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

	// resolve the multisample framebuffer
	if( samples > 0 ) {
		int bits = GL_COLOR_BUFFER_BIT;

		if( !rn.multisampleDepthResolved ) {
			bits |= GL_DEPTH_BUFFER_BIT;
			rn.multisampleDepthResolved = true;
		}

		RB_BlitFrameBufferObject( rn.renderTarget, rn.st->screenTexCopy->fbo, bits, FBO_COPY_NORMAL, GL_NEAREST, 0, 0 );
		ppSource = rn.st->screenTexCopy;
	} else {
		if( rn.renderTarget ) {
			ppSource = RFB_GetObjectTextureAttachment( rn.renderTarget, false, 0 );
		} else {
			ppSource = NULL;
		}
	}

	if( fbFlags == PPFX_BIT_SOFT_PARTICLES ) {
		// only blit soft particles directly when we don't have any other post processing
		// otherwise use the soft particles FBO as the base texture on the next layer
		// to avoid wasting time on resolves and the fragment shader to blit to a temp texture
		R_BlitTextureToScrFbo( fd,
							   ppSource, 0,
							   GLSL_PROGRAM_TYPE_NONE,
							   colorWhite, 0,
							   0, NULL, 0 );
		return;
	}

	fbFlags &= ~PPFX_BIT_SOFT_PARTICLES;

	// apply tone mapping (and possibly color correction too, if not doing the bloom as well)
	if( fbFlags & PPFX_BIT_TONE_MAPPING ) {
		unsigned numImages = 0;
		image_t *images[MAX_SHADER_IMAGES] = { NULL };
		image_t *dest;

		fbFlags &= ~PPFX_BIT_TONE_MAPPING;
		dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL; // LDR

		if( fbFlags & PPFX_BIT_OVERBRIGHT_TARGET ) {
			fbFlags &= ~PPFX_BIT_OVERBRIGHT_TARGET;

			if( !RFB_AttachTextureToObject( dest->fbo, false, 1, rsh.st.screenOverbrightTex ) ) {
				dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL; // re-evaluate
			} else {
				fbFlags |= PPFX_BIT_BLOOM;
			}
		}

		if( fbFlags & PPFX_BIT_BLOOM ) {
			images[1] = rsh.st.screenOverbrightTex;
			numImages = 2;
		} else if( cc ) {
			images[0] = cc->passes[0].images[0];
			numImages = 2;
			fbFlags &= ~PPFX_BIT_COLOR_CORRECTION;
			dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL; // re-evaluate
			cc = NULL;
		}

		R_BlitTextureToScrFbo( fd,
							   ppSource, dest ? dest->fbo : 0,
							   GLSL_PROGRAM_TYPE_COLOR_CORRECTION,
							   colorWhite, 0,
							   numImages, images, 0 );

		ppFrontBuffer ^= 1;
		ppSource = dest;

		if( fbFlags & PPFX_BIT_BLOOM ) {
			// detach
			RFB_AttachTextureToObject( dest->fbo, false, 1, NULL );
		}
	}

	// apply bloom
	if( fbFlags & PPFX_BIT_BLOOM ) {
		image_t *src;

		// continously downscale and blur
		src = rsh.st.screenOverbrightTex;
		for( l = 0; l < NUM_BLOOM_LODS; l++ ) {
			// halve the resolution
			R_BlitTextureToScrFbo( fd,
								   src, rsh.st.screenBloomLodTex[l][0]->fbo,
								   GLSL_PROGRAM_TYPE_NONE,
								   colorWhite, 0,
								   0, NULL, 0 );

			src = R_BlurTextureToScrFbo( fd, rsh.st.screenBloomLodTex[l][0], rsh.st.screenBloomLodTex[l][1] );
			bloomTex[l] = src;
		}
	}

	// apply color correction
	if( fbFlags & PPFX_BIT_COLOR_CORRECTION ) {
		image_t *dest;
		unsigned numImages = 0;
		image_t *images[MAX_SHADER_IMAGES] = { NULL };

		fbFlags &= ~PPFX_BIT_COLOR_CORRECTION;
		if( fbFlags & PPFX_BIT_BLOOM ) {
			memcpy( images + 2, bloomTex, sizeof( image_t * ) * NUM_BLOOM_LODS );
			fbFlags &= ~PPFX_BIT_BLOOM;
		}
		images[0] = cc ? cc->passes[0].images[0] : NULL;
		numImages = MAX_SHADER_IMAGES;

		dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL;
		R_BlitTextureToScrFbo( fd,
							   ppSource, dest ? dest->fbo : 0,
							   GLSL_PROGRAM_TYPE_COLOR_CORRECTION,
							   colorWhite, 0,
							   numImages, images, 0 );

		ppFrontBuffer ^= 1;
		ppSource = dest;
	}

	// apply FXAA
	if( fbFlags & PPFX_BIT_FXAA ) {
		assert( fbFlags == PPFX_BIT_FXAA );

		// not that FXAA only works on LDR input
		R_BlitTextureToScrFbo( fd,
							   ppSource, 0,
							   GLSL_PROGRAM_TYPE_FXAA,
							   colorWhite, 0,
							   0, NULL, 0 );
		return;
	}

	if( fbFlags & PPFX_BIT_BLUR ) {
		ppSource = R_BlurTextureToScrFbo( fd, ppSource, rsh.st.screenPPCopies[ppFrontBuffer] );
		R_BlitTextureToScrFbo( fd,
							   ppSource, 0,
							   GLSL_PROGRAM_TYPE_NONE,
							   colorWhite, 0,
							   0, NULL, 0 );
	}
}

/*
* R_BlurScreen
*/
void R_BlurScreen( void ) {
	refdef_t dummy, *fd;
	image_t *ppSource;

	fd = &dummy;
	memset( fd, 0, sizeof( *fd ) );
	fd->width = rf.frameBufferWidth;
	fd->height = rf.frameBufferHeight;

	RB_FlushDynamicMeshes();

	RB_BlitFrameBufferObject( 0, rsh.st.screenPPCopies[0]->fbo, GL_COLOR_BUFFER_BIT, FBO_COPY_NORMAL, GL_NEAREST, 0, 0 );

	ppSource = R_BlurTextureToScrFbo( fd, rsh.st.screenPPCopies[0], rsh.st.screenPPCopies[1] );

	R_BlitTextureToScrFbo( fd, ppSource, 0, GLSL_PROGRAM_TYPE_NONE, colorWhite, 0, 0, NULL, 0 );
}

/*
=============================================================================

BOUNDING BOXES

=============================================================================
*/

typedef struct {
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
static void R_ClearDebugBounds( void ) {
	r_num_debug_bounds = 0;
}

/*
* R_AddDebugBounds
*/
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs, const byte_vec4_t color ) {
	unsigned i;

	i = r_num_debug_bounds;
	r_num_debug_bounds++;

	if( r_num_debug_bounds > r_debug_bounds_current_size ) {
		r_debug_bounds_current_size = ALIGN( r_num_debug_bounds, 256 );
		if( r_debug_bounds ) {
			r_debug_bounds = R_Realloc( r_debug_bounds, r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
		} else {
			r_debug_bounds = R_Malloc( r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
		}
	}

	VectorCopy( mins, r_debug_bounds[i].mins );
	VectorCopy( maxs, r_debug_bounds[i].maxs );
	Vector4Copy( color, r_debug_bounds[i].color );
}

/*
* R_RenderDebugBounds
*/
static void R_RenderDebugBounds( void ) {
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

	if( !r_num_debug_bounds ) {
		return;
	}

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = 8;
	mesh.xyzArray = verts;
	mesh.numElems = 24;
	mesh.elems = elems;
	mesh.colorsArray[0] = colors;

	RB_SetShaderStateMask( ~0, GLSTATE_NO_DEPTH_TEST );

	for( i = 0; i < r_num_debug_bounds; i++ ) {
		mins = r_debug_bounds[i].mins;
		maxs = r_debug_bounds[i].maxs;
		color = r_debug_bounds[i].color;

		for( j = 0; j < 8; j++ ) {
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
