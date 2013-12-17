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

#define MAX_FRAMEBUFFER_OBJECTS	    1024

typedef struct
{
	int registrationSequence;
	int objectID;
	int renderBufferAttachment;
	int width, height;
	image_t *depthTexture;
	image_t *colorTexture;
} r_fbo_t;

static qboolean r_frambuffer_objects_initialized;
static int r_bound_framebuffer_objectID;
static r_fbo_t *r_bound_framebuffer_object;
static int r_num_framebuffer_objects;
static r_fbo_t r_framebuffer_objects[MAX_FRAMEBUFFER_OBJECTS];

/*
* R_InitFBObjects
*/
void R_InitFBObjects( void )
{
	if( !glConfig.ext.framebuffer_object )
		return;

	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	r_bound_framebuffer_objectID = 0;
	r_bound_framebuffer_object = NULL;

	r_frambuffer_objects_initialized = qtrue;
}

/*
* R_DeleteFBObject
* 
* Delete framebuffer object along with attached render buffer
*/
static void R_DeleteFBObject( r_fbo_t *fbo )
{
	GLuint t;

	if( fbo->renderBufferAttachment )
	{
		t = fbo->renderBufferAttachment;
		qglDeleteRenderbuffersEXT( 1, &t );
		fbo->renderBufferAttachment = 0;
	}

	if( fbo->objectID )
	{
		t = fbo->objectID;
		qglDeleteFramebuffersEXT( 1, &t );
		fbo->objectID = 0;
	}
}

/*
* R_RegisterFBObject
*/
int R_RegisterFBObject( int width, int height )
{
	int i;
	GLuint fbID;
	GLuint rbID;
	r_fbo_t *fbo;

	if( !r_frambuffer_objects_initialized )
		return 0;

	for( i = 0, fbo = r_framebuffer_objects; i < r_num_framebuffer_objects; i++, fbo++ ) {
		if( !fbo->objectID ) {
			// free slot
			goto found;
		}
	}

	if( i == MAX_FRAMEBUFFER_OBJECTS )
	{
		Com_Printf( S_COLOR_YELLOW "R_RegisterFBObject: framebuffer objects limit exceeded\n" );
		return 0;
	}

	i = r_num_framebuffer_objects++;
	fbo = r_framebuffer_objects + i;

found:
	qglGenFramebuffersEXT( 1, &fbID );
	memset( fbo, 0, sizeof( *fbo ) );
	fbo->objectID = fbID;
	fbo->registrationSequence = rf.registrationSequence;
	fbo->width = width;
	fbo->height = height;

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

	qglGenRenderbuffersEXT( 1, &rbID );
	fbo->renderBufferAttachment = rbID;

	// setup 24bit depth buffer for render-to-texture
	qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, fbo->renderBufferAttachment );
	qglRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height );
	qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );

	// attach depth renderbuffer
	qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, 
		GL_RENDERBUFFER_EXT, fbo->renderBufferAttachment );

	if( r_bound_framebuffer_objectID )
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_bound_framebuffer_object->objectID );
	else
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

	return i+1;
}

/*
* R_UnregisterFBObject
*/
void R_UnregisterFBObject( int object )
{
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( !object ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;
	R_DeleteFBObject( fbo );
}

/*
* R_TouchFBObject
*/
void R_TouchFBObject( int object )
{
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( !object ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;
	fbo->registrationSequence = rf.registrationSequence;
}

/*
* R_ActiveFBObject
*/
int R_ActiveFBObject( void )
{
	return r_bound_framebuffer_objectID;
}

/*
* R_UseFBObject
*
* DO NOT call this function directly, use R_BindFrameBufferObject instead.
*/
void R_UseFBObject( int object )
{
	if( !object )
	{
		if( r_frambuffer_objects_initialized )
			qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
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

#define FBO_ATTACHMENT(depth) (depth ? GL_DEPTH_ATTACHMENT_EXT : GL_COLOR_ATTACHMENT0_EXT)

/*
* R_AttachTextureToFBObject
*/
void R_AttachTextureToFBObject( int object, image_t *texture )
{
	qboolean depth;
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return;
	}

	assert( texture != NULL );
	if( !texture ) {
		return;
	}

	fbo = r_framebuffer_objects + object - 1;

	if( texture->flags & IT_DEPTH ) {
		depth = qtrue;
		fbo->depthTexture = texture;
	} else {
		depth = qfalse;
		fbo->colorTexture = texture;
	}
	texture->fbo = object;

	// attach texture
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );
	qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, FBO_ATTACHMENT(depth), GL_TEXTURE_2D, texture->texnum, 0 );
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_bound_framebuffer_objectID ? r_bound_framebuffer_object->objectID : 0 );
}

/*
* R_DetachTextureFromFBObject
*/
void R_DetachTextureFromFBObject( qboolean depth )
{
	r_fbo_t *fbo = r_bound_framebuffer_object;

	if( !r_bound_framebuffer_object )
		return;

	if( depth ) {
		fbo->depthTexture = NULL;
	} else {
		fbo->colorTexture = NULL;
	}

	// attach texture
	qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, FBO_ATTACHMENT(depth), GL_TEXTURE_2D, 0, 0 );
}

/*
* R_GetFBObjectTextureAttachment
*/
image_t	*R_GetFBObjectTextureAttachment( int object, qboolean depth )
{
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	if( object <= 0 || object > r_num_framebuffer_objects ) {
		return NULL;
	}

	fbo = r_framebuffer_objects + object - 1;
	return depth ? fbo->depthTexture : fbo->colorTexture;
}

/*
* R_DisableFBObjectDrawBuffer
*/
void R_DisableFBObjectDrawBuffer( void )
{
	if( !r_bound_framebuffer_object )
		return;

	qglDrawBuffer( GL_NONE );
	qglReadBuffer( GL_NONE );
}

/*
* R_CopyFBObject
*
* The target FBO must be equal or greater in both dimentions than
* the currently bound FBO!
*/
void R_CopyFBObject( int dest, int bitMask, int mode )
{
	int bits;
	int dx, dy, dw, dh;
	r_fbo_t *fbo = r_bound_framebuffer_object, 
		*destfbo = r_framebuffer_objects + dest - 1;

	if( !r_bound_framebuffer_object ) {
		return;
	}
	if( !glConfig.ext.framebuffer_blit ) {
		return;
	}

	assert( dest > 0 && dest <= r_num_framebuffer_objects );
	if( dest <= 0 || dest > r_num_framebuffer_objects ) {
		return;
	}

	bits = 0;
	if( fbo->colorTexture && destfbo->colorTexture ) {
		bits |= GL_COLOR_BUFFER_BIT;
	}
	if( fbo->depthTexture && destfbo->depthTexture ) {
		bits |= GL_DEPTH_BUFFER_BIT;
	}
	bits &= bitMask;

	if( !bits ) {
		return;
	}

	switch( mode ) {
		case FBO_COPY_CENTREPOS:
			dx = (destfbo->width - fbo->width) / 2;
			dy = (destfbo->height - fbo->height) / 2;
			dw = fbo->width;
			dh = fbo->height;
			break;
		case FBO_COPY_INVERT_Y:
			dx = 0;
			dy = destfbo->height - fbo->height;
			dw = fbo->width;
			dh = fbo->height;
			break;
		default:
			dx = 0;
			dy = 0;
			dw = fbo->width;
			dh = fbo->height;
			break;
	}

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, fbo->objectID );
	qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, destfbo->objectID );
	qglBlitFramebufferEXT( 0, 0, fbo->width, fbo->height, dx, dy, dx + dw, dy + dh, bits, GL_NEAREST );
	qglBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 );
	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

	assert( qglGetError() == GL_NO_ERROR );
}

/*
* R_CheckFBObjectStatus
* 
* Boolean, returns qfalse in case of error
*/
qboolean R_CheckFBObjectStatus( void )
{
	GLenum status;

	if( !r_frambuffer_objects_initialized )
		return qfalse;

	status = qglCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
	switch( status )
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return qtrue;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			return qfalse;
		default:
			// programming error; will fail on all hardware
			assert( 0 );
	}
	
	return qfalse;
}

/*
* R_GetFBObjectSize
*/
void R_GetFBObjectSize( int object, int *width, int *height )
{
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
* R_FreeUnusedFBObjects
*/
void R_FreeUnusedFBObjects( void )
{
	int i;

	if( !r_frambuffer_objects_initialized )
		return;

	for( i = 0; i < r_num_framebuffer_objects; i++ ) {
		if( r_framebuffer_objects[i].registrationSequence == rf.registrationSequence ) {
			continue;
		}
		R_DeleteFBObject( r_framebuffer_objects + i );
	}
}

/*
* R_ShutdownFBObjects
* 
* Delete all registered framebuffer and render buffer objects, clear memory
*/
void R_ShutdownFBObjects( void )
{
	int i;

	if( !r_frambuffer_objects_initialized )
		return;

	for( i = 0; i < r_num_framebuffer_objects; i++ ) {
		R_DeleteFBObject( r_framebuffer_objects + i );
	}

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	r_bound_framebuffer_objectID = 0;

	r_frambuffer_objects_initialized = qfalse;
	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );
}
