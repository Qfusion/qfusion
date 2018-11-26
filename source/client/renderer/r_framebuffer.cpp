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

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
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
		glDeleteRenderbuffers( 1, &fbo->depthRenderBuffer );
	}

	if( fbo->stencilRenderBuffer && ( fbo->stencilRenderBuffer != fbo->depthRenderBuffer ) ) {
		glDeleteRenderbuffers( 1, &fbo->stencilRenderBuffer );
	}

	if( fbo->colorRenderBuffer ) {
		glDeleteRenderbuffers( 1, &fbo->colorRenderBuffer );
	}

	if( fbo->objectID ) {
		glDeleteFramebuffers( 1, &fbo->objectID );
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
	glGenFramebuffers( 1, &fbID );
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

	glBindFramebuffer( GL_FRAMEBUFFER, fbo->objectID );

	if( colorRB ) {
		format = glConfig.forceRGBAFramebuffers ? GL_RGBA : GL_RGB;
		if( useFloat ) {
			format = glConfig.forceRGBAFramebuffers ? GL_RGBA16F : GL_RGB16F;
		}

		glGenRenderbuffers( 1, &rbID );
		fbo->colorRenderBuffer = rbID;
		glBindRenderbuffer( GL_RENDERBUFFER, rbID );

		if( samples ) {
			glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, format, width, height );
		} else {
			glRenderbufferStorage( GL_RENDERBUFFER, format, width, height );
		}
	} else {
		// until a color texture is attached, don't enable drawing to the buffer
		glDrawBuffer( GL_NONE );
		glReadBuffer( GL_NONE );
	}

	if( depthRB ) {
		glGenRenderbuffers( 1, &rbID );
		fbo->depthRenderBuffer = rbID;
		glBindRenderbuffer( GL_RENDERBUFFER, rbID );

		if( stencilRB ) {
			format = GL_DEPTH24_STENCIL8;
		} else {
			format = GL_DEPTH_COMPONENT24;
		}

		if( samples ) {
			glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, format, width, height );
		} else {
			glRenderbufferStorage( GL_RENDERBUFFER, format, width, height );
		}

		if( stencilRB ) {
			fbo->stencilRenderBuffer = rbID;
		}
	}

	if( rbID ) {
		glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	}

	if( fbo->colorRenderBuffer ) {
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fbo->colorRenderBuffer );
	}
	if( fbo->depthRenderBuffer ) {
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo->depthRenderBuffer );
	}
	if( fbo->stencilRenderBuffer ) {
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbo->stencilRenderBuffer );
	}

	if( colorRB && depthRB ) {
		if( !RFB_CheckObjectStatus() ) {
			goto fail;
		}
	}

	if( r_bound_framebuffer_objectID ) {
		glBindFramebuffer( GL_FRAMEBUFFER, r_bound_framebuffer_object->objectID );
	} else {
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	return i + 1;

fail:
	RFB_DeleteObject( fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
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
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
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
	glBindFramebuffer( GL_FRAMEBUFFER, r_bound_framebuffer_object->objectID );
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

	fbo = r_framebuffer_objects + object - 1;
	glBindFramebuffer( GL_FRAMEBUFFER, fbo->objectID );

bind:
	if( depth ) {
		attachment = GL_DEPTH_ATTACHMENT;

		if( texture ) {
			assert( texture->flags & IT_DEPTH );
			texnum = texture->texnum;
			texture->fbo = object;
		}
	} else {
		const GLenum fboBuffers[8] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3,
			GL_COLOR_ATTACHMENT4,
			GL_COLOR_ATTACHMENT5,
			GL_COLOR_ATTACHMENT6,
			GL_COLOR_ATTACHMENT7,
		};

		attachment = GL_COLOR_ATTACHMENT0 + target;

		if( target > 0 && texture ) {
			glDrawBuffers( target + 1, fboBuffers );
		} else {
			glDrawBuffers( 0, fboBuffers );
			glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			glReadBuffer( GL_COLOR_ATTACHMENT0 );
		}

		if( texture ) {
			assert( !( texture->flags & IT_DEPTH ) );
			texnum = texture->texnum;
			texture->fbo = object;
		}
	}

	// attach texture
	glFramebufferTexture2D( GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texnum, 0 );
	if( texture ) {
		if( ( texture->flags & ( IT_DEPTH | IT_STENCIL ) ) == ( IT_DEPTH | IT_STENCIL ) ) {
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texnum, 0 );
		}
	}
	else {
		if( depth ) {
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texnum, 0 );
		}
	}
	glBindFramebuffer( GL_FRAMEBUFFER, r_bound_framebuffer_objectID ? r_bound_framebuffer_object->objectID : 0 );

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
		return NULL;
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

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo->objectID );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, destObj );

	if( src == 0 ) {
		glReadBuffer( GL_BACK );
		glDrawBuffer( GL_COLOR_ATTACHMENT0 + drawAtt );
	} else {
		glReadBuffer( GL_COLOR_ATTACHMENT0 + readAtt );
		glDrawBuffer( GL_COLOR_ATTACHMENT0 + drawAtt );
	}

	glBlitFramebuffer( 0, 0, fbo->width, fbo->height, dx, dy, dx + dw, dy + dh, bits, filter );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo->objectID );

	if( src == 0 ) {
		glReadBuffer( GL_BACK );
		glDrawBuffer( GL_BACK );
	} else {
		glReadBuffer( GL_COLOR_ATTACHMENT0 );
		glDrawBuffer( GL_COLOR_ATTACHMENT0 );
	}
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

	status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	assert( status == GL_FRAMEBUFFER_COMPLETE );
	return status == GL_FRAMEBUFFER_COMPLETE;
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

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	r_bound_framebuffer_objectID = 0;

	r_frambuffer_objects_initialized = false;
	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );
}
