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
	image_t *textureAttachment;
} r_fbo_t;

static qboolean r_frambuffer_objects_initialized;
static int r_bound_framebuffer_object;
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
	r_bound_framebuffer_object = 0;

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
int R_RegisterFBObject( void )
{
	int i;
	GLuint fbID;
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

	return i+1;
}

/*
* R_TouchFBObject
*/
void R_TouchFBObject( int object )
{
	r_fbo_t *fbo;

	assert( object > 0 && object <= r_num_framebuffer_objects );

	fbo = r_framebuffer_objects + object - 1;
	fbo->registrationSequence = rf.registrationSequence;
}

/*
* R_ActiveFBObject
*/
int R_ActiveFBObject( void )
{
	return r_bound_framebuffer_object;
}

/*
* R_UseFBObject
*/
qboolean R_UseFBObject( int object )
{
	qboolean status;

	if( !object )
	{
		if( r_frambuffer_objects_initialized )
			qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
		r_bound_framebuffer_object = 0;

		return r_frambuffer_objects_initialized;
	}

	assert( object > 0 && object <= r_num_framebuffer_objects );

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_framebuffer_objects[object-1].objectID );
	r_bound_framebuffer_object = object;

	status = R_CheckFBObjectStatus ();

	return status;
}

/*
* R_AttachTextureToFBOject
*/
qboolean R_AttachTextureToFBOject( int object, image_t *texture, qboolean depthOnly )
{
	r_fbo_t *fbo;
	qboolean status;

	if( !object )
		return qfalse;
	if( !r_frambuffer_objects_initialized )
		return qfalse;

	assert( object > 0 && object <= r_num_framebuffer_objects );
	assert( texture );

	fbo = r_framebuffer_objects + object - 1;

	qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbo->objectID );

	if( depthOnly )
	{
        // Set up depth_tex for render-to-texture
		qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texture->texnum, 0 );

		// inform the driver we do not wish to render to the color buffer
		qglDrawBuffer( GL_NONE );
		qglReadBuffer( GL_NONE );
	}
	else
	{
		// initialize depth renderbuffer
		if( !fbo->renderBufferAttachment )
		{
			GLuint rbID;
			qglGenRenderbuffersEXT( 1, &rbID );
			fbo->renderBufferAttachment = rbID;
		}

		// setup 24bit depth buffer for render-to-texture
		qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, fbo->renderBufferAttachment );
		qglRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
			texture->upload_width, texture->upload_height );
		qglBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );

		// attach depth renderbuffer
		qglFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, 
			GL_RENDERBUFFER_EXT, fbo->renderBufferAttachment );

		// attach texture
		qglFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D, texture->texnum, 0 );
		fbo->textureAttachment = texture;
	}

	status = R_CheckFBObjectStatus ();

	if( r_bound_framebuffer_object )
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, r_framebuffer_objects[r_bound_framebuffer_object-1].objectID );
	else
		qglBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

	return status;
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
	r_bound_framebuffer_object = 0;

	r_frambuffer_objects_initialized = qfalse;
	r_num_framebuffer_objects = 0;
	memset( r_framebuffer_objects, 0, sizeof( r_framebuffer_objects ) );
}
