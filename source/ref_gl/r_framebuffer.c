/*
Copyright (C) 2008 Victor Luchits

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

// r_framebuffer.c - Framebuffer Objects support

#include "r_local.h"

#define MAX_FRAMEBUFFER_OBJECTS     1024
#define MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 2

typedef struct {
	int registrationSequence; // -1 if builtin
	unsigned int objectID;
	unsigned int depthRenderBuffer;
	unsigned int stencilRenderBuffer;
	unsigned int colorRenderBuffer;
	int width, height;
	int samples;
	bool sRGB;
	image_t *depthTexture;
	image_t *colorTexture[MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
} r_fbo_t;

static bool r_frambuffer_objects_initialized;
static int r_bound_framebuffer_objectID;
static r_fbo_t *r_bound_framebuffer_object;
static int r_num_framebuffer_objects;
static r_fbo_t r_framebuffer_objects[MAX_FRAMEBUFFER_OBJECTS];

/*
* RFB_Init
*/
void RFB_Init( void ) {
	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	r_bound_framebuffer_objectID = 0;
	r_bound_framebuffer_object = NULL;

	r_frambuffer_objects_initialized = true;
}

/*
* RFB_DeleteObject
*
* Delete framebuffer object along with attached render buffer
*/
static void RFB_DeleteObject( r_fbo_t *fbo ) {
	if( !fbo ) {
		return;
	}

	if( fbo->depthRenderBuffer ) {
		qglDeleteRenderbuffersEXT( 1, &fbo->depthRenderBuffer );
	}

	if( fbo->stencilRenderBuffer && ( fbo->stencilRenderBuffer != fbo->depthRenderBuffer ) ) {
		qglDeleteRenderbuffersEXT( 1, &fbo->stencilRenderBuffer );
	}

	if( fbo->colorRenderBuffer ) {
		qglDeleteRenderbuffersEXT( 1, &fbo->colorRenderBuffer );
	}

	if( fbo->objectID ) {
		qglDeleteFramebuffersEXT( 1, &fbo->objectID );
	}

	fbo->depthRenderBuffer = 0;
	fbo->stencilRenderBuffer = 0;
	fbo->colorRenderBuffer = 0;
	fbo->objectID = 0;
}

/*
* RFB_RegisterObject
*/
int RFB_RegisterObject( int width, int height, bool builtin, bool depthRB, bool stencilRB,
						bool colorRB, int samples, bool useFloat, bool sRGB ) {
	int i;
	int format;
	GLuint fbID;
	GLuint rbID = 0;
	r_fbo_t *fbo = NULL;

	if( !r_frambuffer_objects_initialized ) {
		return 0;
	}

#ifdef GL_ES_VERSION_2_0
	if( samples ) {
		return 0;
	}
#else
	if( samples && !glConfig.ext.framebuffer_multisample ) {
		return 0;
	}
#endif

	for( i = 0, fbo = r_framebuffer_objects; i < r_num_framebuffer_objects; i++, fbo++ ) {
		if( !fbo->objectID ) {
			// free slot
			goto found;
		}
	}

	if( i == MAX_FRAMEBUFFER_OBJECTS ) {
		Com_Printf( S_COLOR_YELLOW "RFB_RegisterObject: framebuffer objects limit exceeded\n" );
		return 0;
	}

	clamp_high( samples, glConfig.maxFramebufferSamples );

	i = r_num_framebuffer_objects++;
	fbo = r_framebuffer_objects + i;

found:
	qglGenFramebuffersEXT( 1, &fbID );
	memset( fbo, 0, sizeof( *fbo ) );
	fbo->objectID = fbID;
	if( builtin ) {
		fbo->registrationSequence = -1;
	} else {
		fbo->registrationSequence = rsh.registrationSequence;
	}
	fbo->width = width;
	fbo->height = height;
	fbo->samples = samples;
	fbo->sRGB = sRGB;

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

	if( colorRB ) {
		format = glConfig.forceRGBAFramebuffers ? GL_RGBA : GL_RGB;
		if( useFloat ) {
			format = glConfig.forceRGBAFramebuffers ? GL_RGBA16F_ARB : GL_RGB16F_ARB;
		}

		qglGenRenderbuffersEXT( 1, &rbID );
		fbo->colorRenderBuffer = rbID;
		qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rbID );

#ifndef GL_ES_VERSION_2_0
		if( samples ) {
			qglRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, format, width, height );
		} else
#endif
		qglRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, format, width, height );
	}
#ifndef GL_ES_VERSION_2_0
	else {
		// until a color texture is attached, don't enable drawing to the buffer
		qglDrawBuffer( GL_NONE );
		qglReadBuffer( GL_NONE );
	}
#endif

	if( depthRB ) {
		qglGenRenderbuffersEXT( 1, &rbID );
		fbo->depthRenderBuffer = rbID;
		qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rbID );

		if( stencilRB ) {
			format = GL_DEPTH24_STENCIL8_EXT;
		} else if( glConfig.ext.depth24 ) {
			format = GL_DEPTH_COMPONENT24;
		} else if( glConfig.ext.depth_nonlinear ) {
			format = GL_DEPTH_COMPONENT16_NONLINEAR_NV;
		} else {
			format = GL_DEPTH_COMPONENT16;
		}

#ifndef GL_ES_VERSION_2_0
		if( samples ) {
			qglRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, format, width, height );
		} else
#endif
		qglRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, format, width, height );

		if( stencilRB ) {
			fbo->stencilRenderBuffer = rbID;
		}
	}

	if( rbID ) {
		qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
	}

	if( colorRB ) {
		qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, fbo->colorRenderBuffer );
	}
	if( depthRB ) {
		if( stencilRB ) {
			qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->stencilRenderBuffer );
		} else {
			qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->depthRenderBuffer );
		}
	}

	if( colorRB && depthRB ) {
		if( !RFB_CheckObjectStatus() ) {
			goto fail;
		}
	}

	if( r_bound_framebuffer_objectID ) {
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_bound_framebuffer_object->objectID );
	} else {
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	}

	return i + 1;

fail:
	RFB_DeleteObject( fbo );
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	return 0;
}

/*
* RFB_UnregisterObject
*/
void RFB_UnregisterObject( int object ) {
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( !object ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;
	RFB_DeleteObject( fbo );
}

/*
* RFB_TouchObject
*/
void RFB_TouchObject( int object ) {
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( !object ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;
	fbo->registrationSequence = rsh.registrationSequence;
}

/*
* RFB_BoundObject
*/
int RFB_BoundObject( void ) {
	return r_bound_framebuffer_objectID;
}

/*
* RFB_BindObject
*
* DO NOT call this function directly, use R_BindFrameBufferObject instead.
*/
void RFB_BindObject( int object ) {
	if( !object ) {
		if( r_frambuffer_objects_initialized ) {
			qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
		}
		r_bound_framebuffer_objectID = 0;
		r_bound_framebuffer_object = NULL;
		return;
	}

	if( !r_frambuffer_objects_initialized ) {
		return;
	}

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return;
	}

	if( r_bound_framebuffer_objectID == object ) {
		return;
	}

	r_bound_framebuffer_objectID = object;
	r_bound_framebuffer_object = r_framebuffer_objects + object - 1;
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_bound_framebuffer_object->objectID );
}

/*
* RFB_AttachTextureToObject
*/
bool RFB_AttachTextureToObject( int object, bool depth, int target, image_t *texture ) {
	r_fbo_t *fbo;
	int attachment;
	GLuint texnum = 0;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return false;
	}

	if( target < 0 || target >= MAX_FRAMEBUFFER_COLOR_ATTACHMENTS ) {
		return false;
	}
	if( target > 0 && !glConfig.ext.draw_buffers ) {
		return false;
	}

	fbo = r_framebuffer_objects + object - 1;
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

bind:
	if( depth ) {
		attachment = GL_DEPTH_ATTACHMENT_EXT;

		if( texture ) {
			assert( texture->flags & IT_DEPTH );
			texnum = texture->texnum;
			texture->fbo = object;
		}
	} else {
#ifndef GL_ES_VERSION_2_0
		const GLenum fboBuffers[8] = {
			GL_COLOR_ATTACHMENT0_EXT,
			GL_COLOR_ATTACHMENT1_EXT,
			GL_COLOR_ATTACHMENT2_EXT,
			GL_COLOR_ATTACHMENT3_EXT,
			GL_COLOR_ATTACHMENT4_EXT,
			GL_COLOR_ATTACHMENT5_EXT,
			GL_COLOR_ATTACHMENT6_EXT,
			GL_COLOR_ATTACHMENT7_EXT,
		};
#endif

		attachment = GL_COLOR_ATTACHMENT0_EXT + target;

#ifndef GL_ES_VERSION_2_0
		if( target > 0 && texture ) {
			qglDrawBuffersARB( target + 1, fboBuffers );
		} else {
			if( glConfig.ext.draw_buffers ) {
				qglDrawBuffersARB( 0, fboBuffers );
			}
			qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
			qglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
		}
#endif

		if( texture ) {
			assert( !( texture->flags & IT_DEPTH ) );
			texnum = texture->texnum;
			texture->fbo = object;
		}
	}

	// attach texture
	qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, attachment, GL_TEXTURE_2D, texnum, 0 );
	if( texture ) {
		if( ( texture->flags & ( IT_DEPTH | IT_STENCIL ) ) == ( IT_DEPTH | IT_STENCIL ) ) {
			qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, texnum, 0 );
		}
	}
	else {
		if( depth ) {
			qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texnum, 0 );
		}
	}
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_bound_framebuffer_objectID ? r_bound_framebuffer_object->objectID : 0 );

	// check framebuffer status and unbind if failed
	if( !RFB_CheckObjectStatus() ) {
		if( texture ) {
			texture = NULL;
			goto bind;
		}
		return false;
	}

	if( depth ) {
		fbo->depthTexture = texture;
	} else {
		fbo->colorTexture[target] = texture;
	}
	return true;
}

/*
* RFB_GetObjectTextureAttachment
*/
image_t *RFB_GetObjectTextureAttachment( int object, bool depth, int target ) {
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return NULL;
	}
	if( target < 0 || target >= MAX_FRAMEBUFFER_COLOR_ATTACHMENTS ) {
		return false;
	}

	fbo = r_framebuffer_objects + object - 1;
	return depth ? fbo->depthTexture : fbo->colorTexture[target];
}

/*
* RFB_HasColorRenderBuffer
*/
bool RFB_HasColorRenderBuffer( int object ) {
	int i;
	r_fbo_t *fbo;

	assert( object >= 0 && object <= r_num_framebuffer_objects );
	if( object == 0 ) {
		return true;
	}
	if( object < 0 || object > r_num_framebuffer_objects ) {
		return false;
	}

	fbo = r_framebuffer_objects + object - 1;
	if( fbo->colorRenderBuffer != 0 ) {
		return true;
	}

	for( i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++ ) {
		if( fbo->colorTexture[i] != NULL ) {
			return true;
		}
	}
	return false;
}

/*
* RFB_HasDepthRenderBuffer
*/
bool RFB_HasDepthRenderBuffer( int object ) {
	r_fbo_t *fbo;

	assert( object >= 0 && object <= r_num_framebuffer_objects );
	if( object == 0 ) {
		return true;
	}
	if( object < 0 || object > r_num_framebuffer_objects ) {
		return false;
	}

	fbo = r_framebuffer_objects + object - 1;
	return fbo->depthRenderBuffer != 0 || fbo->depthTexture != NULL;
}

/*
* RFB_HasStencilRenderBuffer
*/
bool RFB_HasStencilRenderBuffer( int object ) {
	r_fbo_t *fbo;

	assert( object >= 0 && object <= r_num_framebuffer_objects );
	if( object == 0 ) {
		return glConfig.stencilBits != 0;
	}
	if( object < 0 || object > r_num_framebuffer_objects ) {
		return false;
	}

	fbo = r_framebuffer_objects + object - 1;
	return fbo->stencilRenderBuffer != 0;
}

/*
* RFB_GetSamples
*/
int RFB_GetSamples( int object ) {
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return 0;
	}
	fbo = r_framebuffer_objects + object - 1;
	return fbo->samples;
}

/*
* RFB_sRGBColorSpace
*/
bool RFB_sRGBColorSpace( int object ) {
	r_fbo_t *fbo;

	assert( object >= 0 && object <= r_num_framebuffer_objects );
	if( object == 0 ) {
		return true;
	}
	if( object < 0 || object > r_num_framebuffer_objects ) {
		return false;
	}
	fbo = r_framebuffer_objects + object - 1;
	return fbo->sRGB;
}

/*
* RFB_BlitObject
*
* The target FBO must be equal or greater in both dimentions than
* the currently bound FBO!
*/
void RFB_BlitObject( int src, int dest, int bitMask, int mode, int filter, int readAtt, int drawAtt ) {
	int bits;
	int destObj;
	int dx, dy, dw, dh;
	r_fbo_t scrfbo;
	r_fbo_t *fbo;
	r_fbo_t *destfbo;

	if( !glConfig.ext.framebuffer_blit ) {
		return;
	}

	assert( src >= 0 && src <= r_num_framebuffer_objects );
	if( src < 0 || src > r_num_framebuffer_objects ) {
		return;
	}

	if( src == 0 ) {
		fbo = &scrfbo;
	} else {
		fbo = r_framebuffer_objects + src - 1;
	}

	assert( dest >= 0 && dest <= r_num_framebuffer_objects );
	if( dest < 0 || dest > r_num_framebuffer_objects ) {
		return;
	}

	if( dest ) {
		destfbo = r_framebuffer_objects + dest - 1;
	} else {
		destfbo = NULL;
	}

	bits = bitMask;
	if( !bits ) {
		return;
	}

	RB_ApplyScissor();

	if( src == 0 ) {
		memset( fbo, 0, sizeof( *fbo ) );
		fbo->width = glConfig.width;
		fbo->height = glConfig.height;
	}

	if( destfbo ) {
		dw = destfbo->width;
		dh = destfbo->height;
		destObj = destfbo->objectID;
	} else {
		dw = glConfig.width;
		dh = glConfig.height;
		destObj = 0;
	}

	switch( mode ) {
		case FBO_COPY_CENTREPOS:
			dx = ( dw - fbo->width ) / 2;
			dy = ( dh - fbo->height ) / 2;
			dw = fbo->width;
			dh = fbo->height;
			break;
		case FBO_COPY_INVERT_Y:
			dx = 0;
			dy = dh - fbo->height;
			dw = fbo->width;
			dh = fbo->height;
			break;
		case FBO_COPY_NORMAL_DST_SIZE:
			dx = 0;
			dy = 0;
			//dw = dw;
			//dh = dh;
			break;
		default:
			dx = 0;
			dy = 0;
			dw = fbo->width;
			dh = fbo->height;
			break;
	}

	qglGetError();

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, fbo->objectID );
	qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, destObj );

#ifndef GL_ES_VERSION_2_0
	if( src == 0 ) {
		qglReadBuffer( GL_BACK );
		qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT + drawAtt );
	} else {
		qglReadBuffer( GL_COLOR_ATTACHMENT0_EXT + readAtt );
		qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT + drawAtt );
	}
#endif

	qglBlitFramebufferEXT( 0, 0, fbo->width, fbo->height, dx, dy, dx + dw, dy + dh, bits, filter );
	qglBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

#ifndef GL_ES_VERSION_2_0
	if( src == 0 ) {
		qglReadBuffer( GL_BACK );
		qglDrawBuffer( GL_BACK );
	} else {
		qglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
		qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	}
#endif

	assert( qglGetError() == GL_NO_ERROR );
}

/*
* RFB_CheckObjectStatus
*
* Boolean, returns false in case of error
*/
bool RFB_CheckObjectStatus( void ) {
	GLenum status;

	if( !r_frambuffer_objects_initialized ) {
		return false;
	}

	status = qglCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
	switch( status ) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return true;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT );
			return false;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
			assert( status != GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT );
			return false;
		default:
			// programming error; will fail on all hardware
			assert( 0 );
	}

	return false;
}

/*
* RFB_GetObjectSize
*/
void RFB_GetObjectSize( int object, int *width, int *height ) {
	r_fbo_t *fbo;

	if( !object ) {
		*width = glConfig.width;
		*height = glConfig.height;
		return;
	}

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;
	*width = fbo->width;
	*height = fbo->height;
}

/*
* RFB_FreeUnusedObjects
*/
void RFB_FreeUnusedObjects( void ) {
	int i;
	r_fbo_t *fbo = r_framebuffer_objects;
	int registrationSequence;

	if( !r_frambuffer_objects_initialized ) {
		return;
	}

	for( i = 0; i < r_num_framebuffer_objects; i++, fbo++ ) {
		registrationSequence = fbo->registrationSequence;
		if( ( registrationSequence < 0 ) || ( registrationSequence == rsh.registrationSequence ) ) {
			continue;
		}
		RFB_DeleteObject( fbo );
	}
}

/*
* RFB_Shutdown
*
* Delete all registered framebuffer and render buffer objects, clear memory
*/
void RFB_Shutdown( void ) {
	int i;

	if( !r_frambuffer_objects_initialized ) {
		return;
	}

	for( i = 0; i < r_num_framebuffer_objects; i++ ) {
		RFB_DeleteObject( r_framebuffer_objects + i );
	}

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	r_bound_framebuffer_objectID = 0;

	r_frambuffer_objects_initialized = false;
	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );
}
