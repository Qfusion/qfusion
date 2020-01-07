/*
Copyright (C) 2002-2011 Victor Luchits

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

/*
=======================================================================

CINEMATICS PLAYBACK ABSTRACTION LAYER

=======================================================================
*/

#include "cin_local.h"
#include "cin_roq.h"

enum {
	CIN_TYPE_NONE = -1,

	CIN_TYPE_ROQ,

	CIN_NUM_TYPES
};

typedef struct {
	const char * const extensions;
	bool ( *init )( cinematics_t *cin );
	bool ( *has_ogg_audio )( cinematics_t *cin );
	void ( *shutdown )( cinematics_t *cin );
	void ( *reset )( cinematics_t *cin );
	bool ( *need_next_frame )( cinematics_t *cin );
	uint8_t *( *read_next_frame )( cinematics_t * cin, bool * redraw );
	cin_yuv_t *( *read_next_frame_yuv )( cinematics_t * cin, bool * redraw );
} cin_type_t;

static const cin_type_t cin_types[] =
{
	// RoQ - http://wiki.multimedia.cx/index.php?title=ROQ
	{
		ROQ_FILE_EXTENSIONS,
		RoQ_Init_CIN,
		RoQ_HasOggAudio_CIN,
		RoQ_Shutdown_CIN,
		RoQ_Reset_CIN,
		RoQ_NeedNextFrame_CIN,
		NULL,
		RoQ_ReadNextFrameYUV_CIN
	},

	// NULL safe guard
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	}
};

// =====================================================================

/*
* CIN_Open
*/
cinematics_t *CIN_Open( const char *name, int64_t start_time,
						int flags, bool *yuv, float *framerate ) {
	int i;
	size_t name_size;
	const cin_type_t *type;
	bool res;
	struct mempool_s *mempool;
	cinematics_t *cin = NULL;
	unsigned load_msec;

	load_msec = trap_Milliseconds();
	name_size = strlen( name ) + /*strlen( ".roq" )*/ 10 + 1;

	mempool = CIN_AllocPool( name );
	cin = CIN_Alloc( mempool, sizeof( *cin ) );

	memset( cin, 0, sizeof( *cin ) );

	cin->mempool = mempool;
	cin->file = 0;
	cin->name = CIN_Alloc( cin->mempool, name_size );
	cin->frame = 0;
	cin->width = cin->height = 0;
	cin->aspect_numerator = cin->aspect_denominator = 0; // do not keep aspect ratio
	cin->start_time = cin->cur_time = start_time;
	cin->flags = 0;
	cin->flags = flags;

	cin->type = CIN_TYPE_NONE;
	Q_snprintfz( cin->name, name_size, "%s", name );

	// loop through the list of supported formats
	for( i = 0, type = cin_types; i < CIN_NUM_TYPES && cin->type == CIN_TYPE_NONE; i++, type++ ) {
		char *s, *t;
		const char *ext;

		// no extensions, break, probably a safe guard
		if( !type->extensions ) {
			break;
		}

		// scan filesystem, trying all known extensions for this format
		s = CIN_CopyString( type->extensions );

		t = strtok( s, " " );
		while( t != NULL ) {
			ext = t;
			COM_ReplaceExtension( cin->name, ext, name_size );

			trap_FS_FOpenFile( cin->name, &cin->file, FS_READ );
			if( cin->file ) {
				// found the file
				cin->type = i;
				break;
			}

			t = strtok( NULL, " " );
		}

		CIN_Free( s );
	}

	// call format-dependent initializer, return NULL if it fails
	type = cin_types + cin->type;
	if( cin->type != CIN_TYPE_NONE ) {
		res = type->init( cin );
		if( !res ) {
			type->shutdown( cin );
		}
	} else {
		res = false;
	}

	if( !res ) {
		CIN_Free( cin );
		return NULL;
	}

	if( yuv ) {
		*yuv = cin->yuv;
	}
	if( framerate ) {
		*framerate = cin->framerate;
	}

	// update the timers to account for loading
	load_msec = trap_Milliseconds() - load_msec;
	cin->start_time = cin->cur_time = start_time + load_msec;

	return cin;
}

/*
* CIN_HasOggAudio
*/
bool CIN_HasOggAudio( cinematics_t *cin ) {
	const cin_type_t *type;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];
	return type->has_ogg_audio( cin );
}

/*
* CIN_Filename
*/
const char *CIN_FileName( cinematics_t *cin ) {
	return cin->name;
}

/*
* CIN_NeedNextFrame
*/
bool CIN_NeedNextFrame( cinematics_t *cin, int64_t curtime ) {
	const cin_type_t *type;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];

	cin->cur_time = curtime;
	cin->s_samples_length = CIN_GetRawSamplesLengthFromListeners( cin );

	if( cin->cur_time < cin->start_time ) {
		return false;
	}

	return type->need_next_frame( cin );
}

/*
* CIN_ReadNextFrame_
*/
static uint8_t *CIN_ReadNextFrame_( cinematics_t *cin, int *width, int *height,
									int *aspect_numerator, int *aspect_denominator, bool *redraw, bool yuv ) {
	int i;
	uint8_t *frame = NULL;
	const cin_type_t *type;
	bool redraw_ = false;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];

	cin->haveAudio = false;

	for( i = 0; i < 2; i++ ) {
		redraw_ = false;
		if( yuv ) {
			frame = ( uint8_t * )type->read_next_frame_yuv( cin, &redraw_ );
		} else {
			frame = type->read_next_frame( cin, &redraw_ );
		}
		if( frame || !( cin->flags & CIN_LOOP ) ) {
			break;
		}

		// try again from the beginning if looping
		type->reset( cin );
		cin->frame = 0;
		cin->start_time = cin->cur_time;
	}

	if( width ) {
		*width = cin->width;
	}
	if( height ) {
		*height = cin->height;
	}
	if( aspect_numerator ) {
		*aspect_numerator = cin->aspect_numerator;
	}
	if( aspect_denominator ) {
		*aspect_denominator = cin->aspect_denominator;
	}
	if( redraw ) {
		*redraw = redraw_;
	}

	if( cin->haveAudio ) {
		CIN_ClearRawSamplesListeners( cin );
		cin->haveAudio = false;
	}

	return frame;
}

/*
* CIN_ReadNextFrame
*/
uint8_t *CIN_ReadNextFrame( cinematics_t *cin, int *width, int *height,
							int *aspect_numerator, int *aspect_denominator, bool *redraw ) {
	return CIN_ReadNextFrame_( cin, width, height,
							   aspect_numerator, aspect_denominator, redraw, false );
}

/*
* CIN_ReadNextFrameYUV
*/
cin_yuv_t *CIN_ReadNextFrameYUV( cinematics_t *cin, int *width, int *height,
								 int *aspect_numerator, int *aspect_denominator, bool *redraw ) {
	return ( cin_yuv_t * )CIN_ReadNextFrame_( cin, width, height, aspect_numerator, aspect_denominator, redraw, true );
}

/*
* CIN_ClearRawSamplesListeners
*/
void CIN_ClearRawSamplesListeners( cinematics_t *cin ) {
	cin->num_listeners = 0;
}

/*
* CIN_AddRawSamplesListener
*/
bool CIN_AddRawSamplesListener( cinematics_t *cin, void *listener,
								cin_raw_samples_cb_t raw_samples, cin_get_raw_samples_cb_t get_raw_samples ) {
	int i;

	if( !cin ) {
		return false;
	}
	if( !raw_samples ) {
		return false;
	}

	if( cin->num_listeners >= CIN_MAX_RAW_SAMPLES_LISTENERS ) {
		return false;
	}
	if( cin->flags & CIN_NOAUDIO ) {
		return false;
	}

	for( i = 0; i < cin->num_listeners; i++ ) {
		if( cin->listeners[i].listener == listener
			&& cin->listeners[i].raw_samples == raw_samples ) {
			return true;
		}
	}

	cin->listeners[cin->num_listeners].listener = listener;
	cin->listeners[cin->num_listeners].raw_samples = raw_samples;
	cin->listeners[cin->num_listeners].get_raw_samples = get_raw_samples;
	cin->num_listeners++;

	return true;
}

/*
* CIN_RawSamplesToListeners
*/
void CIN_RawSamplesToListeners( cinematics_t *cin, unsigned int samples, unsigned int rate,
								unsigned short width, unsigned short channels, const uint8_t *data ) {
	int i;

	if( cin->flags & CIN_NOAUDIO ) {
		return;
	}

	for( i = 0; i < cin->num_listeners; i++ ) {
		cin->listeners[i].raw_samples( cin->listeners[i].listener, samples, rate, width, channels, data );
	}

	cin->haveAudio = true;
	cin->s_samples_length = CIN_GetRawSamplesLengthFromListeners( cin );
}

/*
* CIN_GetRawSamplesLengthFromListeners
*
* Returns maximum samples length of all listeners. This can result
* in underruns for some listeners but never in overruns.
*/
unsigned int CIN_GetRawSamplesLengthFromListeners( cinematics_t *cin ) {
	int i;
	unsigned int length = 0;

	for( i = 0; i < cin->num_listeners; i++ ) {
		unsigned int l = cin->listeners[i].get_raw_samples ?
						 cin->listeners[i].get_raw_samples( cin->listeners[i].listener ) : 0;
		length = max( length, l );
	}

	return length;
}

/*
* CIN_Reset
*/
void CIN_Reset( cinematics_t *cin, int64_t cur_time ) {
	const cin_type_t *type;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];

	type->reset( cin );
	cin->frame = 0;
	cin->cur_time = cur_time;
	cin->start_time = cur_time;
}

/*
* CIN_Close
*/
void CIN_Close( cinematics_t *cin ) {
	struct mempool_s *mempool;
	const cin_type_t *type;

	if( !cin ) {
		return;
	}

	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	mempool = cin->mempool;
	assert( mempool != NULL );

	type = &cin_types[cin->type];
	type->shutdown( cin );

	cin->cur_time = 0;
	cin->start_time = 0; // done

	if( cin->file ) {
		trap_FS_FCloseFile( cin->file );
		cin->file = 0;
	}

	if( cin->fdata ) {
		CIN_Free( cin->fdata );
		cin->fdata = NULL;
	}

	if( cin->name ) {
		CIN_Free( cin->name );
		cin->name = NULL;
	}

	if( cin->vid_buffer ) {
		CIN_Free( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}

	CIN_Free( cin );
	CIN_FreePool( &mempool );
}
