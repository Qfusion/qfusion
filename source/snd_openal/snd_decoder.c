/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "snd_decoder.h"
#include "snd_vorbis.h"

static snd_decoder_t *decoders;

/*
* Local helper functions
*/

// This should always be called with the extension removed
static snd_decoder_t *findCodec( const char *filename ) {
	snd_decoder_t *decoder = decoders;
	const char *extension = COM_FileExtension( filename );

	if( extension ) {
		while( decoder ) {
			if( !Q_stricmp( extension, decoder->ext ) ) {
				return decoder;
			}

			decoder = decoder->next;
		}
	}

	return NULL;
}

static void decoder_register( snd_decoder_t *decoder ) {
	decoder->next = decoders;
	decoders = decoder;
}

/**
* Sound system wide functions (snd_local.h)
*/

bool S_InitDecoders( bool verbose ) {
	// First codec has the priority.
	decoders = NULL;

	decoder_register( &wav_decoder );
	if( SNDOGG_Init( verbose ) ) {
		decoder_register( &ogg_decoder );
	}

	return true;
}

void S_ShutdownDecoders( bool verbose ) {
	decoders = NULL;
	SNDOGG_Shutdown( verbose );
}

void *S_LoadSound( const char *filename, snd_info_t *info ) {
	snd_decoder_t *decoder;
	char fn[MAX_QPATH];

	decoder = findCodec( filename );
	if( !decoder ) {
		//Com_Printf( "No decoder found for file: %s\n", filename );
		return NULL;
	}

	Q_strncpyz( fn, filename, sizeof( fn ) );
	COM_DefaultExtension( fn, decoder->ext, sizeof( fn ) );

	return decoder->load( fn, info );
}

snd_stream_t *S_OpenStream( const char *filename ) {
	snd_decoder_t *decoder;
	char fn[MAX_QPATH];

	decoder = findCodec( filename );
	if( !decoder ) {
		//Com_Printf( "No decoder found for file: %s\n", filename );
		return NULL;
	}

	Q_strncpyz( fn, filename, sizeof( fn ) );
	COM_DefaultExtension( fn, decoder->ext, sizeof( fn ) );

	return decoder->open( fn );
}

int S_ReadStream( snd_stream_t *stream, int samples, void *buffer ) {
	return stream->decoder->read( stream, samples, buffer );
}

void S_CloseStream( snd_stream_t *stream ) {
	stream->decoder->close( stream );
}

bool S_ResetStream( snd_stream_t *stream ) {
	return stream->decoder->reset( stream );
}

/**
* Util functions used by decoders (snd_decoder.h)
*/
snd_stream_t *decoder_stream_init( snd_decoder_t *decoder ) {
	snd_stream_t *stream;

	// Allocate a stream
	stream = S_Malloc( sizeof( snd_stream_t ) );
	stream->decoder = decoder;
	return stream;
}

void decoder_stream_shutdown( snd_stream_t *stream ) {
	S_Free( stream );
}

//=============================================================================

/*
* SNDOGG_Shutdown
*/
void SNDOGG_Shutdown( bool verbose ) {
}

/*
* SNDOGG_Init
*/
bool SNDOGG_Init( bool verbose ) {
	return true;
}

/**
* OGG decoder
*/
snd_decoder_t ogg_decoder =
{
	".ogg",
	decoder_ogg_load,
	decoder_ogg_open,
	decoder_ogg_read,
	decoder_ogg_close,
	decoder_ogg_reset,
	NULL
};

void *decoder_ogg_load( const char *filename, snd_info_t *info ) {
	int channels = 0, rate = 0;
	void *buffer;
	short *data;
	int samples;

	samples = qvorbis_load_file( filename, &channels, &rate, &data );

	if( samples < 0 ) {
		Com_Printf( "Error unsupported .ogg file: %s\n", filename );
		return NULL;
	}

	if( channels != 1 && channels != 2 ) {
		Com_Printf( "Error unsupported .ogg file (unsupported number of channels: %i): %s\n", channels, filename );
		free( data );
		return NULL;
	}

	info->samples = samples;
	info->rate = rate;
	info->width = 2;
	info->channels = channels;
	info->size = info->samples * info->width;

	buffer = S_Malloc( info->size );
	memcpy( buffer, data, info->size );
	free( data );

	return buffer;
}

static bool decoder_ogg_init( snd_stream_t* stream ) {
	int rate, channels;

	if( !qvorbis_stream_init( stream->ptr, &rate, &channels ) ) {
		return false;
	}

	stream->info.channels = channels;
	stream->info.rate = rate;
	stream->info.width = 2;
	stream->info.size = 0;
	return true;
}

snd_stream_t *decoder_ogg_open( const char *filename ) {
	snd_stream_t *stream;
	qvorbis_stream_t *ogg_stream;

	// Open
	stream = decoder_stream_init( &ogg_decoder );
	if( !stream ) {
		Com_Printf( "Error initializing .ogg stream: %s\n", filename );
		return NULL;
	}

	stream->ptr = S_Malloc( sizeof( qvorbis_stream_t ) );

	ogg_stream = (qvorbis_stream_t *)stream->ptr;

	trap_FS_FOpenFile( filename, &ogg_stream->filenum, FS_READ | FS_NOSIZE );
	if( !ogg_stream->filenum ) {
		decoder_ogg_close( stream );
		return NULL;
	}

	if( !decoder_ogg_init( stream ) ) {
		decoder_ogg_close( stream );
		return NULL;
	}

	return stream;
}

int decoder_ogg_read( snd_stream_t *stream, int samples, void *buffer ) {
	return qvorbis_stream_read_samples( stream->ptr, samples, stream->info.channels, stream->info.width, buffer );
}

void decoder_ogg_close( snd_stream_t *stream ) {
	if( stream->ptr != NULL ) {
		qvorbis_stream_t* ogg_stream = (qvorbis_stream_t*)stream->ptr;

		qvorbis_stream_deinit( ogg_stream );

		if( ogg_stream->filenum ) {
			trap_FS_FCloseFile( ogg_stream->filenum );
		}
	}

	S_Free( stream->ptr );
	decoder_stream_shutdown( stream );

}

bool decoder_ogg_reset( snd_stream_t *stream ) {
	return qvorbis_stream_reset( stream->ptr );
}

typedef struct snd_wav_stream_s snd_wav_stream_t;

struct snd_wav_stream_s {
	int filenum;
	int position;
	int content_start;
};


/**
* Wave file reading
*/
static int FGetLittleLong( int f ) {
	int v;

	trap_FS_Read( &v, sizeof( v ), f );

	return LittleLong( v );
}

static int FGetLittleShort( int f ) {
	short v;

	trap_FS_Read( &v, sizeof( v ), f );

	return LittleShort( v );
}

static int readChunkInfo( int f, char *name ) {
	int len, read;

	name[4] = 0;

	read = trap_FS_Read( name, 4, f );
	if( read != 4 ) {
		return 0;
	}

	len = FGetLittleLong( f );
	if( len < 0 || len > 0xffffffff ) {
		return 0;
	}

	len = ( len + 1 ) & ~1; // pad to word boundary
	return len;
}

static void skipChunk( int f, int length ) {
	size_t toread;
	uint8_t buffer[32 * 1024];

	while( length > 0 ) {
		toread = length;
		if( toread > sizeof( buffer ) ) {
			toread = sizeof( buffer );
		}
		trap_FS_Read( buffer, toread, f );
		length -= toread;
	}
}

// returns the length of the data in the chunk, or 0 if not found
static int findWavChunk( int filenum, const char *chunk ) {
	char name[5];
	int len;

	// This is a bit dangerous...
	while( true ) {
		len = readChunkInfo( filenum, name );

		// Read failure?
		if( !len ) {
			return 0;
		}

		// If this is the right chunk, return
		if( !strcmp( name, chunk ) ) {
			return len;
		}

		// Not the right chunk - skip it
		skipChunk( filenum, len );
	}
}

static void byteSwapRawSamples( int samples, int width, int channels, const uint8_t *data ) {
	int i;

	if( LittleShort( 256 ) == 256 ) {
		return;
	}

	if( width != 2 ) {
		return;
	}

	if( channels == 2 ) {
		samples <<= 1;
	}

	for( i = 0; i < samples; i++ )
		( (short *)data )[i] = LittleShort( ( (short *)data )[i] );
}

static bool read_wav_header( int filenum, snd_info_t *info ) {
	char dump[16];
	int fmtlen = 0;

	// skip the riff wav header
	trap_FS_Read( dump, 12, filenum );

	// Scan for the format chunk
	if( !( fmtlen = findWavChunk( filenum, "fmt " ) ) ) {
		Com_Printf( "Error reading wav header: No fmt chunk\n" );
		return false;
	}

	// Save the parameters
	FGetLittleShort( filenum );
	info->channels = FGetLittleShort( filenum );
	info->rate = FGetLittleLong( filenum );
	FGetLittleLong( filenum );
	FGetLittleShort( filenum );
	info->width = FGetLittleShort( filenum ) / 8;

	// Skip the rest of the format chunk if required
	if( fmtlen > 16 ) {
		fmtlen -= 16;
		skipChunk( filenum, fmtlen );
	}

	// Scan for the data chunk
	if( !( info->size = findWavChunk( filenum, "data" ) ) ) {
		Com_Printf( "Error reading wav header: No data chunk\n" );
		return false;
	}
	info->samples = ( info->size / info->width ) / info->channels;

	return true;
}

static void decoder_wav_stream_shutdown( snd_stream_t *stream ) {
	S_Free( stream->ptr );
	decoder_stream_shutdown( stream );
}

//=============================================================================

/**
* WAV decoder
*/
snd_decoder_t wav_decoder =
{
	".wav",
	decoder_wav_load,
	decoder_wav_open,
	decoder_wav_read,
	decoder_wav_close,
	decoder_wav_reset,
	NULL
};

void *decoder_wav_load( const char *filename, snd_info_t *info ) {
	int filenum;
	int read;
	void *buffer;

	if( trap_FS_IsUrl( filename ) ) {
		return NULL;
	}

	trap_FS_FOpenFile( filename, &filenum, FS_READ | FS_NOSIZE );
	if( !filenum ) {
		return NULL;
	}

	if( !read_wav_header( filenum, info ) ) {
		trap_FS_FCloseFile( filenum );
		Com_Printf( "Can't understand .wav file: %s\n", filename );
		return NULL;
	}

	buffer = S_Malloc( info->size );
	read = trap_FS_Read( buffer, info->size, filenum );
	if( read != info->size ) {
		S_Free( buffer );
		trap_FS_FCloseFile( filenum );
		Com_Printf( "Error reading .wav file: %s\n", filename );
		return NULL;
	}

	byteSwapRawSamples( info->samples, info->width, info->channels, (uint8_t *)buffer );

	trap_FS_FCloseFile( filenum );

	return buffer;
}

snd_stream_t *decoder_wav_open( const char *filename ) {
	snd_stream_t *stream;
	snd_wav_stream_t *wav_stream;

	stream = decoder_stream_init( &wav_decoder );
	if( !stream ) {
		return NULL;
	}

	stream->ptr = S_Malloc( sizeof( snd_wav_stream_t ) );
	wav_stream = (snd_wav_stream_t *)stream->ptr;

	trap_FS_FOpenFile( filename, &wav_stream->filenum, FS_READ | FS_NOSIZE );
	if( !wav_stream->filenum ) {
		decoder_wav_stream_shutdown( stream );
		return NULL;
	}

	if( !read_wav_header( wav_stream->filenum, &stream->info ) ) {
		decoder_wav_close( stream );
		return NULL;
	}

	wav_stream->content_start = wav_stream->position;
	return stream;
}

int decoder_wav_read( snd_stream_t *stream, int samples, void *buffer ) {
	snd_wav_stream_t *wav_stream = (snd_wav_stream_t *)stream->ptr;
	int remaining = stream->info.size - wav_stream->position;
	int bytes, bytes_read;

	if( remaining <= 0 ) {
		return 0;
	}

	bytes = samples * stream->info.width * stream->info.channels;
	if( bytes > remaining ) {
		bytes = remaining;
	}
	bytes_read = trap_FS_Read( buffer, bytes, wav_stream->filenum );

	wav_stream->position += bytes_read;
	samples = ( bytes_read / stream->info.width ) / stream->info.channels;

	byteSwapRawSamples( samples, stream->info.width, stream->info.channels, buffer );

	return samples;
}

void decoder_wav_close( snd_stream_t *stream ) {
	snd_wav_stream_t *wav_stream = (snd_wav_stream_t *)stream->ptr;

	trap_FS_FCloseFile( wav_stream->filenum );
	decoder_wav_stream_shutdown( stream );
}

bool decoder_wav_reset( snd_stream_t *stream ) {
	snd_wav_stream_t *wav_stream = (snd_wav_stream_t *)stream->ptr;

	if( trap_FS_Seek( wav_stream->filenum, wav_stream->content_start, FS_SEEK_SET ) ) {
		return false;
	}

	wav_stream->position = wav_stream->content_start;
	return true;
}
