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

#define MAX_CINEMATICS  256

typedef struct r_cinhandle_s {
	unsigned int id;
	int registrationSequence;
	volatile bool reset;
	char            *name;
	char            *uploadName;
	struct cinematics_s *cin;
	image_t         *image;
	int width, height;
	volatile uint8_t *pic;
	volatile bool new_frame;
	bool yuv;
	qmutex_t        *lock;
	ref_yuv_t       *cyuv;
	image_t         *yuv_images[3];
	struct r_cinhandle_s *prev, *next;
} r_cinhandle_t;

static r_cinhandle_t *r_cinematics;
static r_cinhandle_t r_cinematics_headnode, *r_free_cinematics;

/*
* R_RunCin
*/
static void R_RunCin( r_cinhandle_t *h ) {
	bool redraw = false;
	int64_t now = ri.Sys_Milliseconds();

	// don't advance cinematics during registration
	if( rsh.registrationOpen ) {
		return;
	}

	ri.Mutex_Lock( h->lock );

	if( h->reset ) {
		h->new_frame = false;
		h->reset = false;
		ri.CIN_Reset( h->cin, now );
	}

	if( ri.CIN_NeedNextFrame( h->cin, now ) ) {
		if( h->yuv ) {
			h->cyuv = ri.CIN_ReadNextFrameYUV( h->cin, &h->width, &h->height, NULL, NULL, &redraw );
			h->pic = ( uint8_t * )h->cyuv;
		} else {
			h->pic = ri.CIN_ReadNextFrame( h->cin, &h->width, &h->height, NULL, NULL, &redraw );
		}
	}

	if( h->pic == NULL ) {
		h->new_frame = false;
	} else {
		h->new_frame |= redraw;
	}

	ri.Mutex_Unlock( h->lock );
}

/*
* R_UploadCinematicFrame
*/
static void R_UploadCinematicFrame( r_cinhandle_t *handle ) {
	const int samples = 4;

	ri.Mutex_Lock( handle->lock );

	if( !handle->cin || !handle->pic ) {
		ri.Mutex_Unlock( handle->lock );
		return;
	}

	if( handle->yuv ) {
		int i;

		if( !handle->yuv_images[0] ) {
			char tn[256];
			uint8_t *fake_data[1] = { NULL };
			const char *letters[3] = { "y", "u", "v" };

			for( i = 0; i < 3; i++ ) {
				handle->yuv_images[i] = R_LoadImage(
					va_r( tn, sizeof( tn ), "%s_%s", handle->name, letters[i] ),
					fake_data, 1, 1, IT_SPECIAL | IT_NO_DATA_SYNC, 1, IMAGE_TAG_GENERIC, 1 );
			}
			handle->new_frame = true;
		}

		if( handle->new_frame ) {
			bool multiSamples2D;
			bool in2D;
			int w, h;
			/*ATTRIBUTE_ALIGNED( 16 ) */mat4_t projectionMatrix;
	
			// render/convert three 8-bit YUV images into RGB framebuffer
			in2D = rf.twoD.enabled;
			multiSamples2D = rf.twoD.multiSamples;

			if( in2D ) {
				R_End2D();
			} else {
				R_PushRefInst();
			}

			R_InitViewportTexture( &handle->image, handle->name, 0,
								   handle->cyuv->image_width, handle->cyuv->image_height,
								   0, IT_SPECIAL | IT_FRAMEBUFFER, IMAGE_TAG_GENERIC, samples );

			w = handle->image->upload_width;
			h = handle->image->upload_height;

			R_BindFrameBufferObject( handle->image->fbo );

			R_SetupGL2D();

			Matrix4_OrthoProjection( 0, w, h, 0, -1, 1, projectionMatrix );

			RB_LoadProjectionMatrix( projectionMatrix );

			RB_Scissor( 0, 0, w,h );

			RB_Viewport( 0, 0, w, h );

			R_UploadRawYUVPic( handle->yuv_images, handle->cyuv->yuv );

			// flip the image vertically because we're rendering to a FBO
			R_DrawStretchRawYUVBuiltin(
				0, 0,
				w, h,
				(float)handle->cyuv->x_offset / handle->cyuv->image_width,
				(float)handle->cyuv->y_offset / handle->cyuv->image_height,
				(float)( handle->cyuv->x_offset + handle->cyuv->width ) / handle->cyuv->image_width,
				(float)( handle->cyuv->y_offset + handle->cyuv->height ) / handle->cyuv->image_height,
				handle->yuv_images, 2 );

			if( in2D ) {
				R_Begin2D( multiSamples2D );
			} else {
				R_PopRefInst();
			}

			handle->new_frame = false;
		}
	} else {
		if( !handle->image ) {
			handle->image = R_LoadImage( handle->name, (uint8_t **)&handle->pic, handle->width, handle->height,
										 IT_SPECIAL | IT_NO_DATA_SYNC, 1, IMAGE_TAG_GENERIC, samples );
		}

		if( handle->new_frame ) {
			R_ReplaceImage( handle->image, (uint8_t **)&handle->pic, handle->width, handle->height,
							handle->image->flags, 1, samples );
			handle->new_frame = false;
		}
	}

	ri.Mutex_Unlock( handle->lock );
}

//==================================================================================

/*
* R_CinList_f
*/
void R_CinList_f( void ) {
	image_t *image;
	r_cinhandle_t *handle, *hnode;

	Com_Printf( "Active cintematics:" );
	hnode = &r_cinematics_headnode;
	handle = hnode->prev;
	if( handle == hnode ) {
		Com_Printf( " none\n" );
		return;
	}

	Com_Printf( "\n" );
	do {
		assert( handle->cin );

		image = handle->image;
		if( image && ( handle->width != image->upload_width || handle->height != image->upload_height ) ) {
			Com_Printf( "%s %i(%i)x%i(%i)\n", handle->name, handle->width,
						image->upload_width, handle->height, image->upload_height );
		} else {
			Com_Printf( "%s %ix%i\n", handle->name, handle->width, handle->height );
		}

		handle = handle->next;
	} while( handle != hnode );
}

/*
* R_InitCinematics
*/
void R_InitCinematics( void ) {
	int i;

	r_cinematics = R_Malloc( sizeof( r_cinhandle_t ) * MAX_CINEMATICS );
	memset( r_cinematics, 0, sizeof( r_cinhandle_t ) * MAX_CINEMATICS );

	// link cinemtics
	r_free_cinematics = r_cinematics;
	r_cinematics_headnode.id = 0;
	r_cinematics_headnode.prev = &r_cinematics_headnode;
	r_cinematics_headnode.next = &r_cinematics_headnode;
	for( i = 0; i < MAX_CINEMATICS - 1; i++ ) {
		if( i < MAX_CINEMATICS - 1 ) {
			r_cinematics[i].next = &r_cinematics[i + 1];
		}
		r_cinematics[i].id = i + 1;
	}
}

/*
* R_RunAllCinematic
*/
void R_RunAllCinematics( void ) {
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next ) {
		next = handle->prev;
		R_RunCin( handle );
	}
}

/*
* R_GetCinematicHandleById
*/
static r_cinhandle_t *R_GetCinematicHandleById( unsigned int id ) {
	if( id == 0 || id > MAX_CINEMATICS ) {
		return NULL;
	}
	return &r_cinematics[id - 1];
}

/*
* R_GetCinematicById
*/
struct cinematics_s *R_GetCinematicById( unsigned int id ) {
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
image_t *R_GetCinematicImage( unsigned int id ) {
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
void R_UploadCinematic( unsigned int id ) {
	r_cinhandle_t *handle;

	handle = R_GetCinematicHandleById( id );
	if( handle ) {
		R_UploadCinematicFrame( handle );
	}
}

/*
* R_StartCinematic
*/
unsigned int R_StartCinematic( const char *arg ) {
	char uploadName[128];
	size_t name_size;
	char *name;
	r_cinhandle_t *handle, *hnode, *next;
	struct cinematics_s *cin;
	bool yuv;

	name_size = strlen( "video/" ) + strlen( arg ) + 1;
	name = alloca( name_size );

	if( strstr( arg, "/" ) == NULL && strstr( arg, "\\" ) == NULL ) {
		Q_snprintfz( name, name_size, "video/%s", arg );
	} else {
		Q_snprintfz( name, name_size, "%s", arg );
	}

	// find cinematics with the same name
	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next ) {
		next = handle->prev;
		assert( handle->cin );

		// reuse
		if( !Q_stricmp( handle->name, name ) ) {
			return handle->id;
		}
	}

	// open the file, read header, etc
	cin = ri.CIN_Open( name, ri.Sys_Milliseconds(), &yuv, NULL );

	// take a free cinematic handle if possible
	if( !r_free_cinematics || !cin ) {
		return 0;
	}

	handle = r_free_cinematics;
	r_free_cinematics = handle->next;

	// copy name
	handle->name = R_CopyString( name );

	// copy upload name
	Q_snprintfz( uploadName, sizeof( uploadName ), "***r_cinematic%i***", handle->id - 1 );
	name_size = strlen( uploadName ) + 1;
	handle->uploadName = R_Malloc( name_size );
	memcpy( handle->uploadName, uploadName, name_size );

	handle->cin = cin;
	handle->new_frame = false;
	handle->yuv = yuv;
	handle->image = NULL;
	handle->yuv_images[0] = handle->yuv_images[1] = handle->yuv_images[2] = NULL;
	handle->registrationSequence = rsh.registrationSequence;
	handle->pic = NULL;
	handle->cyuv = NULL;
	handle->lock = ri.Mutex_Create();

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
void R_TouchCinematic( unsigned int id ) {
	int i;
	r_cinhandle_t *handle;

	handle = R_GetCinematicHandleById( id );
	if( !handle ) {
		return;
	}

	ri.Mutex_Lock( handle->lock );

	handle->registrationSequence = rsh.registrationSequence;

	if( handle->image ) {
		R_TouchImage( handle->image, IMAGE_TAG_GENERIC );
	}
	for( i = 0; i < 3; i++ ) {
		if( handle->yuv_images[i] ) {
			R_TouchImage( handle->yuv_images[i], IMAGE_TAG_GENERIC );
		}
	}

	// do not attempt to reupload the new frame until successful R_RunCin
	handle->new_frame = false;
	handle->pic = NULL;
	handle->cyuv = NULL;

	ri.Mutex_Unlock( handle->lock );
}

/*
* R_FreeUnusedCinematics
*/
void R_FreeUnusedCinematics( void ) {
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
void R_FreeCinematic( unsigned int id ) {
	qmutex_t *lock;
	r_cinhandle_t *handle;

	handle = R_GetCinematicHandleById( id );
	if( !handle ) {
		return;
	}

	lock = handle->lock;
	ri.Mutex_Lock( lock );

	ri.CIN_Close( handle->cin );
	handle->cin = NULL;
	handle->lock = NULL;

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

	ri.Mutex_Unlock( lock );

	ri.Mutex_Destroy( &lock );
}

/*
* R_ResetCinematic
*/
static void R_ResetCinematic( r_cinhandle_t *handle ) {
	ri.Mutex_Lock( handle->lock );
	handle->reset = true;
	ri.Mutex_Unlock( handle->lock );
}

/*
* R_RestartCinematics
*/
void R_RestartCinematics( void ) {
	r_cinhandle_t *handle, *hnode;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = handle->prev ) {
		R_ResetCinematic( handle );
	}
}

/*
* R_ShutdownCinematics
*/
void R_ShutdownCinematics( void ) {
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next ) {
		next = handle->prev;
		R_FreeCinematic( handle->id );
	}

	R_Free( r_cinematics );
}
