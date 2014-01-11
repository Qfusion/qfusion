/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// r_cin.c
#include "r_local.h"

#define MAX_CINEMATICS	256

typedef struct r_cinhandle_s
{
	unsigned int	id;
	int				registrationSequence;
	qboolean		reset;
	char			*name;
	char			*uploadName;
	struct cinematics_s	*cin;
	image_t			*image;
	int				width, height;
	qbyte			*pic;
	qboolean		new_frame;
	qboolean		yuv;
	ref_yuv_t		*cyuv;
	image_t			*yuv_images[3];
	struct r_cinhandle_s *prev, *next;
} r_cinhandle_t;

static r_cinhandle_t *r_cinematics;
static r_cinhandle_t r_cinematics_headnode, *r_free_cinematics;

/*
* R_RunCin
*/
static qboolean R_RunCin( r_cinhandle_t *h )
{
	qboolean redraw;
	unsigned int now = ri.Sys_Milliseconds();

	// don't advance cinematics during registration
	if( rsh.registrationOpen ) {
		return qfalse;
	}

	if( h->reset ) {
		h->reset = qfalse;
		ri.CIN_Reset( h->cin, now );
	}

	if( !ri.CIN_NeedNextFrame( h->cin, now ) ) {
		return qfalse;
	}

	if( h->yuv ) {
		h->cyuv = ri.CIN_ReadNextFrameYUV( h->cin, &h->width, &h->height, NULL, NULL, &redraw );
		h->pic = ( qbyte * )h->cyuv;
	}
	else {
		h->pic = ri.CIN_ReadNextFrame( h->cin, &h->width, &h->height, NULL, NULL, &redraw );
	}
	return redraw;
}

/*
* R_ResampleCinematicFrame
*/
static image_t *R_ResampleCinematicFrame( r_cinhandle_t *handle )
{
	const int samples = 4;

	if( !handle->pic ) {
		// we haven't yet read a new frame, return whatever image we got
		// this will return NULL until at least one frame has been read
		return handle->image;
	}

	if( handle->yuv ) {
		int i;

		if( !handle->yuv_images[0] ) {
			qbyte *fake_data[1] = { NULL };
			const char *letters[3] = { "y", "u", "v" };

			for( i = 0; i < 3; i++ ) {
				handle->yuv_images[i] = R_LoadImage( va( "%s_%s", handle->name, letters[i] ), 
					fake_data, 1, 1, IT_CINEMATIC|IT_LUMINANCE, 1 );
			}
			handle->new_frame = qtrue;
		}
		
		if( handle->new_frame ) {
			int fbo;
			qboolean in2D;
			
			// render/convert three 8-bit YUV images into RGB framebuffer

			in2D = rf.in2D;
			fbo = RFB_BoundObject();

			if( !in2D ) {
				R_PushRefInst();
			}

			R_InitViewportTexture( &handle->image, handle->name, 0, 
				handle->cyuv->image_width, handle->cyuv->image_height, 
				0, IT_CINEMATIC|IT_FRAMEBUFFER, samples );

			R_BindFrameBufferObject( handle->image->fbo );

			R_Set2DMode( qtrue );

			RB_Scissor( 0, 0, handle->image->upload_width, handle->image->upload_height );

			RB_Viewport( 0, 0, handle->image->upload_width, handle->image->upload_height );

			// flip the image vertically because we're rendering to a FBO
			R_DrawStretchRawYUVBuiltin( 
				0, 0, 
				handle->image->upload_width, handle->image->upload_height,
				(float)handle->cyuv->x_offset / handle->cyuv->image_width, 
				(float)handle->cyuv->y_offset / handle->cyuv->image_height, 
				(float)(handle->cyuv->x_offset + handle->cyuv->width) / handle->cyuv->image_width, 
				(float)(handle->cyuv->y_offset + handle->cyuv->height) / handle->cyuv->image_height, 
				handle->cyuv->yuv, handle->yuv_images, 2 );

			if( !in2D ) {
				R_PopRefInst( 0 );
			}
			R_BindFrameBufferObject( fbo );

			R_Set2DMode( in2D );

			handle->new_frame = qfalse;
		}
	}
	else {
		if( !handle->image ) {
			handle->image = R_LoadImage( handle->name, &handle->pic, handle->width, handle->height, 
				IT_CINEMATIC, samples );
			handle->new_frame = qfalse;
		} else if( handle->new_frame ) {
			R_ReplaceImage( handle->image, &handle->pic, handle->width, handle->height, 
				handle->image->flags, samples );
			handle->new_frame = qfalse;
		}
	}

	return handle->image;
}

//==================================================================================

/*
* R_CinList_f
*/
void R_CinList_f( void )
{
	image_t *image;
	r_cinhandle_t *handle, *hnode;

	Com_Printf( "Active cintematics:" );
	hnode = &r_cinematics_headnode;
	handle = hnode->prev;
	if( handle == hnode )
	{
		Com_Printf( " none\n" );
		return;
	}

	Com_Printf( "\n" );
	do {
		assert( handle->cin );

		image = handle->image;
		if( image && (handle->width != image->upload_width || handle->height != image->upload_height) )
			Com_Printf( "%s %i(%i)x%i(%i)\n", handle->name, handle->width, 
				image->upload_width, handle->height, image->upload_height );
		else
			Com_Printf( "%s %ix%i\n", handle->name, handle->width, handle->height );

		handle = handle->next;
	} while( handle != hnode );
}

/*
* R_InitCinematics
*/
void R_InitCinematics( void )
{
	int i;

	r_cinematics = R_Malloc( sizeof( r_cinhandle_t ) * MAX_CINEMATICS );
	memset( r_cinematics, 0, sizeof( r_cinhandle_t ) * MAX_CINEMATICS );

	// link cinemtics
	r_free_cinematics = r_cinematics;
	r_cinematics_headnode.id = 0;
	r_cinematics_headnode.prev = &r_cinematics_headnode;
	r_cinematics_headnode.next = &r_cinematics_headnode;
	for( i = 0; i < MAX_CINEMATICS - 1; i++ )
	{
		if( i < MAX_CINEMATICS - 1 )
			r_cinematics[i].next = &r_cinematics[i+1];
		r_cinematics[i].id = i + 1;
	}

	ri.Cmd_AddCommand( "cinlist", R_CinList_f );
}

/*
* R_RunAllCinematic
*/
void R_RunAllCinematics( void )
{
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		handle->new_frame = R_RunCin( handle );
	}
}

/*
* R_GetCinematicHandleById
*/
static r_cinhandle_t *R_GetCinematicHandleById( unsigned int id )
{
	assert( id > 0 && id <= MAX_CINEMATICS );
	if( id == 0 || id > MAX_CINEMATICS ) {
		return NULL;
	}
	return &r_cinematics[id - 1];
}

/*
* R_GetCinematicById
*/
struct cinematics_s *R_GetCinematicById( unsigned int id )
{
	r_cinhandle_t *handle;
	
	handle = R_GetCinematicHandleById( id );
	if( handle ) {
		return handle->cin;
	}
	return NULL;
}

/*
* R_GetCinematicImage
*/
image_t *R_GetCinematicImage( unsigned int id )
{
	r_cinhandle_t *handle;
	
	handle = R_GetCinematicHandleById( id );
	if( handle ) {
		return handle->image;
	}
	return NULL;

}

/*
* R_UploadCinematic
*/
void R_UploadCinematic( unsigned int id )
{
	r_cinhandle_t *handle;
	
	handle = R_GetCinematicHandleById( id );
	if( handle ) {
		R_ResampleCinematicFrame( handle );
	}
}

/*
* R_StartCinematic
*/
unsigned int R_StartCinematic( const char *arg )
{
	char uploadName[128];
	size_t name_size;
	r_cinhandle_t *handle, *hnode, *next;
	struct cinematics_s *cin;
	qboolean yuv;

	// find cinematics with the same name
	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		assert( handle->cin );

		// reuse
		if( !Q_stricmp( handle->name, arg ) )
			return handle->id;
	}

	// open the file, read header, etc
	cin = ri.CIN_Open( arg, ri.Sys_Milliseconds(), qtrue, &yuv, NULL );

	// take a free cinematic handle if possible
	if( !r_free_cinematics || !cin )
		return 0;

	handle = r_free_cinematics;
	r_free_cinematics = handle->next;

	// copy name
	name_size = strlen( arg ) + 1;
	handle->name = R_Malloc( name_size );
	memcpy( handle->name, arg, name_size );

	// copy upload name
	Q_snprintfz( uploadName, sizeof( uploadName ), "***r_cinematic%i***", handle->id-1 );
	name_size = strlen( uploadName ) + 1;
	handle->uploadName = R_Malloc( name_size );
	memcpy( handle->uploadName, uploadName, name_size );

	handle->cin = cin;
	handle->new_frame = qfalse;
	handle->yuv = yuv;
	handle->image = NULL;
	handle->yuv_images[0] = handle->yuv_images[1] = handle->yuv_images[2] = NULL;
	handle->registrationSequence = rsh.registrationSequence;
	handle->pic = NULL;
	handle->cyuv = NULL;

	// put handle at the start of the list
	handle->prev = &r_cinematics_headnode;
	handle->next = r_cinematics_headnode.next;
	handle->next->prev = handle;
	handle->prev->next = handle;

	return handle->id;
}

/*
* R_TouchCinematic
*/
void R_TouchCinematic( unsigned int id )
{
	int i;
	r_cinhandle_t *handle;
	
	handle = R_GetCinematicHandleById( id );
	if( !handle ) {
		return;
	}

	handle->registrationSequence = rsh.registrationSequence;

	if( handle->image ) {
		R_TouchImage( handle->image );
	}
	for( i = 0; i < 3; i++ ) {
		if( handle->yuv_images[i] ) {
			R_TouchImage( handle->yuv_images[i] );
		}
	}

	// do not attempt to reupload the new frame until successful R_RunCin 
	handle->new_frame = qfalse;
	handle->pic = NULL;
	handle->cyuv = NULL;
}

/*
* R_FreeUnusedCinematics
*/
void R_FreeUnusedCinematics( void )
{
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next ) {
		next = handle->prev;
		if( handle->registrationSequence != rsh.registrationSequence ) {
			R_FreeCinematic( handle->id );
		}
	}
}

/*
* R_FreeCinematic
*/
void R_FreeCinematic( unsigned int id )
{
	r_cinhandle_t *handle;
	
	handle = R_GetCinematicHandleById( id );
	if( !handle ) {
		return;
	}

	ri.CIN_Close( handle->cin );
	handle->cin = NULL;

	assert( handle->name );
	R_Free( handle->name );
	handle->name = NULL;

	assert( handle->uploadName );
	R_Free( handle->uploadName );
	handle->uploadName = NULL;

	// remove from linked active list
	handle->prev->next = handle->next;
	handle->next->prev = handle->prev;

	// insert into linked free list
	handle->next = r_free_cinematics;
	r_free_cinematics = handle;
}

/*
* R_ResetCinematic
*/
static void R_ResetCinematic( r_cinhandle_t *handle )
{
	handle->reset = qtrue;
}

/*
* R_RestartCinematics
*/
void R_RestartCinematics( void )
{
	r_cinhandle_t *handle, *hnode;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = handle->prev ) {
		R_ResetCinematic( handle );
	}
}

/*
* R_ShutdownCinematics
*/
void R_ShutdownCinematics( void )
{
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		R_FreeCinematic( handle->id );
	}

	R_Free( r_cinematics );

	ri.Cmd_RemoveCommand( "cinlist" );
}
