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
	PPFX_BLUR,
};

enum {
	PPFX_BIT_SOFT_PARTICLES = RF_BIT( PPFX_SOFT_PARTICLES ),
	PPFX_BIT_TONE_MAPPING = RF_BIT( PPFX_TONE_MAPPING ),
	PPFX_BIT_COLOR_CORRECTION = RF_BIT( PPFX_COLOR_CORRECTION ),
	PPFX_BIT_BLUR = RF_BIT( PPFX_BLUR ),
};

static void R_ClearDebugBounds( void );
static void R_RenderDebugBounds( void );

/*
* R_ClearScene
*/
void R_ClearScene( void ) {
	R_ClearRefInstStack();

	R_FrameCache_Clear();

	R_ClearSkeletalCache();

	R_ClearDebugBounds();

	rsc.numLocalEntities = 0;
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

	rsc.frameCount++;
}

/*
* R_CacheSceneEntity
*/
static void R_CacheSceneEntity( entity_t *e ) {
	entSceneCache_t *cache = R_ENTCACHE( e );

	switch( e->rtype ) {
	case RT_MODEL:
		if( !e->model ) {
			cache->mod_type = mod_bad;
			return;
		}

		cache->mod_type = e->model->type;
		cache->radius = 0;
		cache->rotated = false;
		ClearBounds( cache->mins, cache->maxs );
		ClearBounds( cache->absmins, cache->absmaxs );

		switch( e->model->type ) {
		case mod_alias:
			R_CacheAliasModelEntity( e );
			break;
		case mod_skeletal:
			R_CacheSkeletalModelEntity( e );
			break;
		case mod_brush:
			R_CacheBrushModelEntity( e );
			break;
		default:
			e->model->type = mod_bad;
			break;
		}
		break;
	case RT_SPRITE:
		R_CacheSpriteEntity( e );
		break;
	default:
		break;
	}
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

		if( de->rtype == RT_MODEL ) {
			if( de->model && de->model->type == mod_brush ) {
				de->flags |= RF_FORCENOLOD;
				rsc.bmodelEntities[rsc.numBmodelEntities++] = eNum;
			}
		} else if( de->rtype == RT_SPRITE ) {
			// simplifies further checks
			de->model = NULL;
			de->flags |= RF_FORCENOLOD;
			if( !de->customShader || de->radius <= 0 || de->scale <= 0 ) {
				return;
			}
		}

		if( de->renderfx & RF_ALPHAHACK ) {
			if( de->shaderRGBA[3] == 255 ) {
				de->renderfx &= ~RF_ALPHAHACK;
			}
		}

		R_CacheSceneEntity( de );

		rsc.numEntities++;
	}
}

/*
* R_AddLightToScene
*/
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	// TODO
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
		dp->renderfx = poly->renderfx;

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
	int fbFlags = 0;
	int ppFrontBuffer = 0;
	int samples = 0;
	image_t *ppSource;
	shader_t *cc;

	R_End2D();

	RB_SetTime( fd->time );

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		rsc.refdef = *fd;

		if( rsc.worldModelSequence != rsh.worldModelSequence ) {
			rsc.frameCount = !rsc.frameCount;
			rsc.worldModelSequence = rsh.worldModelSequence;
		}
	}

	rn.refdef = *fd;
	if( !rn.refdef.minLight ) {
		rn.refdef.minLight = 0.1f;
	}

	fd = &rn.refdef;

	rn.renderFlags = RF_NONE;

	rn.nearClip = Z_NEAR;
	rn.farClip = R_DefaultFarClip();
	rn.polygonFactor = POLYOFFSET_FACTOR;
	rn.polygonUnits = POLYOFFSET_UNITS;
	rn.clipFlags = 15;
	rn.meshlist = &r_worldlist;
	rn.portalmasklist = &r_portalmasklist;
	rn.numEntities = 0;

	rn.st = &rsh.st;
	rn.renderTarget = 0;
	rn.multisampleDepthResolved = false;

	fbFlags = 0;
	cc = rn.refdef.colorCorrection;
	if( !( cc && cc->numpasses > 0 && cc->passes[0].images[0] && cc->passes[0].images[0] != rsh.noTexture ) ) {
		cc = NULL;
	}

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		bool useFloat;

		useFloat = glConfig.sSRGB && rsh.stf.screenTex;
		if( useFloat ) {
			rn.st = &rsh.stf;
		}

		// reload the multisample framebuffer if needed
		samples = R_MultisampleSamples( r_samples->integer );
		if( samples > 0 ) {
			rn.renderTarget = R_RegisterMultisampleTarget( rn.st, samples, useFloat, false );
			if( rn.renderTarget == 0 ) {
				samples = 0;
			}
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
			if( fd->rdflags & RDF_BLURRED ) {
				fbFlags |= PPFX_BIT_BLUR;
			}

			if( fbFlags != oldFlags ) {
				// use a FBO without a depth renderbuffer attached, unless we need one for soft particles
				if( !rn.renderTarget ) {
					rn.renderTarget = rn.st->screenPPCopies[0]->fbo;
					ppFrontBuffer = 1;
				}
			}
		}
	} else {
		fbFlags = 0;
		rn.renderTarget = rf.renderTarget;
	}

	// clip new scissor region to the one currently set
	Vector4Set( rn.scissor, fd->scissor_x, fd->scissor_y, fd->scissor_width, fd->scissor_height );
	Vector4Set( rn.viewport, fd->x, fd->y, fd->width, fd->height );
	VectorCopy( fd->vieworg, rn.pvsOrigin );
	VectorCopy( fd->vieworg, rn.viewOrigin );
	Matrix3_Copy( fd->viewaxis, rn.viewAxis );

	R_BindFrameBufferObject( 0 );

	R_SetupViewMatrices( fd );

	R_SetupFrustum( fd, rn.nearClip, rn.farClip, rn.frustum, rn.frustumCorners );

	R_SetupPVS( fd );

	R_RenderView( fd );

	if( !(fd->rdflags & RDF_NOWORLDMODEL) ) {
		R_RenderDebugSurface( fd );

		R_RenderDebugBounds();
	}

	R_Begin2D( false );

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
		if( rn.renderTarget != rf.renderTarget ) {
			ppSource = RFB_GetObjectTextureAttachment( rn.renderTarget, false, 0 );
		} else {
			ppSource = NULL;
		}
	}

	if( ( ppSource != NULL ) && ( fbFlags == PPFX_BIT_SOFT_PARTICLES || fbFlags == 0 ) ) {
		// blit soft particles or resolved MSAA to default FB as don't have any other post 
		// processing effects, otherwise use the source FBO as the base texture on the next 
		// layer to avoid wasting time on resolves in the fragment shader
		R_BlitTextureToScrFbo( fd,
							   ppSource, rf.renderTarget,
							   GLSL_PROGRAM_TYPE_NONE,
							   colorWhite, 0,
							   0, NULL, 0 );
		goto done;
	}

	fbFlags &= ~PPFX_BIT_SOFT_PARTICLES;

	// apply tone mapping/color correction
	if( fbFlags & PPFX_BIT_TONE_MAPPING ) {
		unsigned numImages = 0;
		image_t *images[MAX_SHADER_IMAGES] = { NULL };
		image_t *dest;

		fbFlags &= ~PPFX_BIT_TONE_MAPPING;
		dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL; // LDR

		if( cc ) {
			images[0] = cc->passes[0].images[0];
			numImages = 2;
			fbFlags &= ~PPFX_BIT_COLOR_CORRECTION;
			dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL; // re-evaluate
			cc = NULL;
		}

		R_BlitTextureToScrFbo( fd,
							   ppSource, dest ? dest->fbo : rf.renderTarget,
							   GLSL_PROGRAM_TYPE_COLOR_CORRECTION,
							   colorWhite, 0,
							   numImages, images, 0 );

		ppFrontBuffer ^= 1;
		ppSource = dest;
	}

	// apply color correction
	if( fbFlags & PPFX_BIT_COLOR_CORRECTION ) {
		image_t *dest;
		unsigned numImages = 0;
		image_t *images[MAX_SHADER_IMAGES] = { NULL };

		fbFlags &= ~PPFX_BIT_COLOR_CORRECTION;
		images[0] = cc ? cc->passes[0].images[0] : NULL;
		numImages = MAX_SHADER_IMAGES;

		dest = fbFlags ? rsh.st.screenPPCopies[ppFrontBuffer] : NULL;
		R_BlitTextureToScrFbo( fd,
							   ppSource, dest ? dest->fbo : rf.renderTarget,
							   GLSL_PROGRAM_TYPE_COLOR_CORRECTION,
							   colorWhite, 0,
							   numImages, images, 0 );

		ppFrontBuffer ^= 1;
		ppSource = dest;
	}

	if( fbFlags & PPFX_BIT_BLUR ) {
		ppSource = R_BlurTextureToScrFbo( fd, ppSource, rsh.st.screenPPCopies[ppFrontBuffer] );
		R_BlitTextureToScrFbo( fd,
							   ppSource, rf.renderTarget,
							   GLSL_PROGRAM_TYPE_NONE,
							   colorWhite, 0,
							   0, NULL, 0 );
	}

done:
	Vector4Set( rn.scissor, 0, 0, glConfig.width, glConfig.height );
	Vector4Set( rn.viewport, 0, 0, glConfig.width, glConfig.height );
	R_BindFrameBufferObject( rf.renderTarget );
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

	// render previously batched 2D geometry, if any
	RB_FlushDynamicMeshes();

	R_Begin2D( false );

	RB_BlitFrameBufferObject( rf.renderTarget, rsh.st.screenPPCopies[0]->fbo, GL_COLOR_BUFFER_BIT, FBO_COPY_NORMAL, GL_NEAREST, 0, 0 );

	ppSource = R_BlurTextureToScrFbo( fd, rsh.st.screenPPCopies[0], rsh.st.screenPPCopies[1] );

	R_BlitTextureToScrFbo( fd, ppSource, rf.renderTarget, GLSL_PROGRAM_TYPE_NONE, colorWhite, 0, 0, NULL, 0 );
}

/*
=============================================================================

BOUNDING BOXES

=============================================================================
*/

typedef struct {
	vec4_t corners[8];
	byte_vec4_t colors[8];
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
* R_AddDebugCorners
*/
void R_AddDebugCorners( const vec3_t corners[8], const vec4_t color ) {
	unsigned i, j, k;
	r_debug_bound_t *b;

	i = r_num_debug_bounds;
	r_num_debug_bounds++;

	if( r_num_debug_bounds > r_debug_bounds_current_size ) {
		r_debug_bounds_current_size = ALIGN( r_num_debug_bounds, 256 );
		if( r_debug_bounds ) {
			r_debug_bounds = ( r_debug_bound_t * ) R_Realloc( r_debug_bounds, r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
		} else {
			r_debug_bounds = ( r_debug_bound_t * ) R_Malloc( r_debug_bounds_current_size * sizeof( r_debug_bound_t ) );
		}
	}

	b = &r_debug_bounds[i];

	for( j = 0; j < 8; j++ ) {
		VectorCopy( corners[j], b->corners[j] );
		b->corners[j][3] = 1;

		for( k = 0; k < 3; k++ ) {
			b->colors[j][k] = color[k] * 255;
		}

		b->colors[j][3] = 255;
	}
}

/*
* R_AddDebugBounds
*/
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs, const vec4_t color ) {
	vec3_t corners[8];

	BoundsCorners( mins, maxs, corners );

	R_AddDebugCorners( corners, color );
}

/*
* R_RenderDebugBounds
*/
static void R_RenderDebugBounds( void ) {
	unsigned i;
	mesh_t mesh;
	elem_t elems[24];

	if( !r_num_debug_bounds ) {
		return;
	}

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = 8;
	mesh.numElems = 24;
	mesh.elems = elems;

	RB_SetShaderStateMask( ~0, GLSTATE_NO_DEPTH_TEST );

	RB_SetLightstyle( NULL, NULL );

	for( i = 0; i < 24; i++ ) {
		elems[i] = r_boxedges[i];
	}

	for( i = 0; i < r_num_debug_bounds; i++ ) {
		mesh.xyzArray = r_debug_bounds[i].corners;
		mesh.colorsArray[0] = r_debug_bounds[i].colors;

		RB_AddDynamicMesh( rsc.worldent, rsh.whiteShader, NULL, &mesh, GL_LINES, 0.0f, 0.0f );
	}

	RB_FlushDynamicMeshes();

	RB_SetShaderStateMask( ~0, 0 );
}

//=======================================================================
