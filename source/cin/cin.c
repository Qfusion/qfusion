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
#include "cin_ogg.h"
#include "cin_roq.h"

enum
{
	CIN_TYPE_NONE = -1,

	CIN_TYPE_OGG,
	CIN_TYPE_ROQ,

	CIN_NUM_TYPES
};

typedef struct
{
	const char * const extensions;
	qboolean ( *init )( cinematics_t *cin );
	void ( *shutdown )( cinematics_t *cin );
	void ( *reset )( cinematics_t *cin );
	qboolean ( *need_next_frame )( cinematics_t *cin );
	qbyte *( *read_next_frame )( cinematics_t *cin, qboolean *redraw );
} cin_type_t;

static const cin_type_t cin_types[] = 
{
	// Ogg Theora
	{
		OGG_FILE_EXTENSIONS,
		Ogg_Init_CIN,
		Ogg_Shutdown_CIN,
		Ogg_Reset_CIN,
		Ogg_NeedNextFrame_CIN,
		Ogg_ReadNextFrame_CIN
	},

	// RoQ - http://wiki.multimedia.cx/index.php?title=ROQ
	{
		ROQ_FILE_EXTENSIONS,
		RoQ_Init_CIN,
		RoQ_Shutdown_CIN,
		RoQ_Reset_CIN,
		RoQ_NeedNextFrame_CIN,
		RoQ_ReadNextFrame_CIN
	},

	// NULL safe guard
	{
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
cinematics_t *CIN_Open( const char *name, unsigned int start_time, int flags )
{
	int i;
	size_t name_size;
	const cin_type_t *type;
	qboolean res;
	struct mempool_s *mempool;
	cinematics_t *cin = NULL;

	name_size = strlen( "video/" ) + strlen( name ) + /*strlen( ".roq" )*/10 + 1;

	mempool = CIN_AllocPool( name );
	cin = CIN_Alloc( mempool, sizeof( *cin ) );
	memset( cin, 0, sizeof( *cin ) );

	cin->mempool = mempool;
	cin->file = 0;
	cin->flags = flags;
	cin->name = CIN_Alloc( cin->mempool, name_size );
	cin->frame = 0;
	cin->width = cin->height = 0;
	cin->aspect_numerator = cin->aspect_denominator = 0; // do not keep aspect ratio
	cin->start_time = cin->cur_time = start_time;

	if( trap_FS_IsUrl( name ) )
	{
		cin->type = CIN_TYPE_OGG;
		Q_strncpyz( cin->name, name, name_size );
		trap_FS_FOpenFile( cin->name, &cin->file, FS_READ );
	}
	else
	{
		cin->type = CIN_TYPE_NONE;
		Q_snprintfz( cin->name, name_size, "video/%s", name );
	}

	// loop through the list of supported formats
	for( i = 0, type = cin_types; i < CIN_NUM_TYPES && cin->type == CIN_TYPE_NONE; i++, type++ )
	{
		char *s, *t;
		const char *ext;

		// no extensions, break, probably a safe guard
		if( !type->extensions )
			break;

		// scan filesystem, trying all known extensions for this format
		s = CIN_CopyString( type->extensions );

		t = strtok( s, " " );
		while( t != NULL )
		{
			ext = t;
			COM_ReplaceExtension( cin->name, ext, name_size );

			trap_FS_FOpenFile( cin->name, &cin->file, FS_READ );
			if( cin->file )
			{
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
	}
	else {
		res = qfalse;
	}

	if( !res && cin )
	{
		CIN_Free( cin );
		return NULL;
	}

	return cin;
}

/*
* CIN_NeedNextFrame
*/
qboolean CIN_NeedNextFrame( cinematics_t *cin, unsigned int curtime )
{
	const cin_type_t *type;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];

	cin->cur_time = curtime;
	if( cin->cur_time <= cin->start_time )
		return qfalse;

	return type->need_next_frame( cin );
}

/*
* CIN_ReadNextFrame
*/
qbyte *CIN_ReadNextFrame( cinematics_t *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, qboolean *redraw )
{
	int i;
	qbyte *frame = NULL;
	const cin_type_t *type;
	qboolean redraw_ = qfalse;

	assert( cin );
	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	type = &cin_types[cin->type];

	for( i = 0; i < 2; i++ )
	{
		redraw_ = qfalse;
		frame = type->read_next_frame( cin, &redraw_ );
		if( frame || !( cin->flags & CIN_LOOP ) )
			break;

		// try again from the beginning if looping
		type->reset( cin );
		cin->frame = 0;
		cin->start_time = cin->cur_time;
	}

	if( width )
		*width = cin->width;
	if( height )
		*height = cin->height;
	if( aspect_numerator )
		*aspect_numerator = cin->aspect_numerator;
	if( aspect_denominator )
		*aspect_denominator = cin->aspect_denominator;
	if( redraw )
		*redraw = redraw_;

	return frame;
}

/*
* CIN_Close
*/
void CIN_Close( cinematics_t *cin )
{
	struct mempool_s *mempool;
	const cin_type_t *type;

	if( !cin )
		return;

	assert( cin->type > CIN_TYPE_NONE && cin->type < CIN_NUM_TYPES );

	mempool = cin->mempool;
	assert( mempool != NULL );

	type = &cin_types[cin->type];
	type->shutdown( cin );

	cin->cur_time = 0;
	cin->start_time = 0; // done

	if( cin->file )
	{
		trap_FS_FCloseFile( cin->file );
		cin->file = 0;
	}

	if( cin->fdata )
	{
		CIN_Free( cin->fdata );
		cin->fdata = NULL;
	}

	if( cin->name )
	{
		CIN_Free( cin->name );
		cin->name = NULL;
	}

	if( cin->vid_buffer )
	{
		CIN_Free( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}

	CIN_Free( cin );
	CIN_FreePool( &mempool );
}
