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
// snd_mem.c: sound caching

#include "snd_local.h"
#include "snd_vorbis.h"

/*
* ResampleSfx
*/
unsigned int ResampleSfx( unsigned int numsamples, unsigned int speed, unsigned short channels, unsigned short width, const uint8_t *data, uint8_t *outdata, char *name ) {
	size_t srclength, outcount;

	outcount = (size_t) ( (double)numsamples * (double)dma.speed / (double)speed );
	srclength = numsamples * channels;

	// Trivial case (direct transfer)
	if( speed == dma.speed ) {
		if( width == 1 ) {
			size_t i;

			for( i = 0; i < srclength; i++ )
				( (signed char*)outdata )[i] = data[i] - 128;
		} else {   // if (width == 2)
			memcpy( outdata, data, srclength * width );
		}

		return numsamples;
	}

	// General case (linear interpolation with a fixed-point fractional
	// step, 18-bit integer part and 14-bit fractional part)
	// Can handle up to 2^18 (262144) samples per second (> 96KHz stereo)
#   define FRACTIONAL_BITS 14
#   define FRACTIONAL_MASK ( ( 1 << FRACTIONAL_BITS ) - 1 )
#   define INTEGER_BITS ( sizeof( samplefrac ) * 8 - FRACTIONAL_BITS )
	else {
		const unsigned int fracstep = (unsigned int)( (double)speed / dma.speed * ( 1 << FRACTIONAL_BITS ) );
		size_t remain_in = srclength, total_out = 0;
		unsigned int samplefrac;
		const unsigned char *in_ptr = data;
		unsigned char *out_ptr = outdata;

		// Check that we can handle one second of that sound
		if( speed * channels > ( 1 << INTEGER_BITS ) ) {
			Com_Printf( "ResampleSfx: sound quality too high for resampling (%uHz, %u channel(s))\n",
						speed, channels );
			return 0;
		}

		// We work 1 sec at a time to make sure we don't accumulate any
		// significant error when adding "fracstep" over several seconds, and
		// also to be able to handle very long sounds.
		while( total_out < outcount ) {
			size_t tmpcount, interpolation_limit, i, j;
			unsigned int srcsample;

			samplefrac = 0;

			// If more than 1 sec of sound remains to be converted
			if( outcount > dma.speed + total_out ) {
				tmpcount = dma.speed;
				interpolation_limit = tmpcount;  // all samples can be interpolated
			} else {
				tmpcount = outcount - total_out;
				interpolation_limit = (int)ceil( (double)( ( ( remain_in / channels ) - 1 ) << FRACTIONAL_BITS ) / fracstep );
				if( interpolation_limit > tmpcount ) {
					interpolation_limit = tmpcount;
				}
			}

			// 16 bit samples
			if( width == 2 ) {
				const short* in_ptr_short;

				// Interpolated part
				for( i = 0; i < interpolation_limit; i++ ) {
					srcsample = ( samplefrac >> FRACTIONAL_BITS ) * channels;
					in_ptr_short = &( (const short*)in_ptr )[srcsample];

					for( j = 0; j < channels; j++ ) {
						int a, b;

						a = *in_ptr_short;
						b = *( in_ptr_short + channels );
						*( (short*)out_ptr ) = ( ( ( b - a ) * ( samplefrac & FRACTIONAL_MASK ) ) >> FRACTIONAL_BITS ) + a;

						in_ptr_short++;
						out_ptr += sizeof( short );
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for( /* nothing */; i < tmpcount; i++ ) {
					srcsample = ( samplefrac >> FRACTIONAL_BITS ) * channels;
					in_ptr_short = &( (const short*)in_ptr )[srcsample];

					for( j = 0; j < channels; j++ ) {
						*( (short*)out_ptr ) = *in_ptr_short;

						in_ptr_short++;
						out_ptr += sizeof( short );
					}

					samplefrac += fracstep;
				}
			}
			// 8 bit samples
			else { // if (width == 1)
				const unsigned char* in_ptr_byte;

				// Convert up to 1 sec of sound
				for( i = 0; i < interpolation_limit; i++ ) {
					srcsample = ( samplefrac >> FRACTIONAL_BITS ) * channels;
					in_ptr_byte = &( (const unsigned char*)in_ptr )[srcsample];

					for( j = 0; j < channels; j++ ) {
						int a, b;

						a = *in_ptr_byte - 128;
						b = *( in_ptr_byte + channels ) - 128;
						*( (signed char*)out_ptr ) = ( ( ( b - a ) * ( samplefrac & FRACTIONAL_MASK ) ) >> FRACTIONAL_BITS ) + a;

						in_ptr_byte++;
						out_ptr += sizeof( signed char );
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for( /* nothing */; i < tmpcount; i++ ) {
					srcsample = ( samplefrac >> FRACTIONAL_BITS ) * channels;
					in_ptr_byte = &( (const unsigned char*)in_ptr )[srcsample];

					for( j = 0; j < channels; j++ ) {
						*( (signed char*)out_ptr ) = *in_ptr_byte - 128;

						in_ptr_byte++;
						out_ptr += sizeof( signed char );
					}

					samplefrac += fracstep;
				}
			}

			// Update the counters and the buffer position
			remain_in -= speed * channels;
			in_ptr += speed * channels * width;
			total_out += tmpcount;
		}
	}

	return outcount;
}


//=============================================================================

/*
* S_LoadSound_Wav
*/
sfxcache_t *S_LoadSound_Wav( sfx_t *s ) {
	char namebuffer[MAX_QPATH];
	uint8_t *data;
	wavinfo_t info;
	int len, file;
	sfxcache_t *sc;
	int size;

	assert( s && s->name[0] );
	assert( !s->cache );

	// load it in
	Q_strncpyz( namebuffer, s->name, sizeof( namebuffer ) );
	size = trap_FS_FOpenFile( namebuffer, &file, FS_READ );

	if( !file ) {
		//Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	data = S_Malloc( size );
	trap_FS_Read( data, size, file );
	trap_FS_FCloseFile( file );

	info = GetWavinfo( s->name, data, size );
	if( info.channels < 1 || info.channels > 2 ) {
		Com_Printf( "%s has an invalid number of channels\n", s->name );
		S_Free( data );
		return NULL;
	}

	// calculate resampled length
	len = (int) ( (double) info.samples * (double) dma.speed / (double) info.rate );
	len = len * info.width * info.channels;

	sc = S_Malloc( len + sizeof( sfxcache_t ) );
	if( !sc ) {
		S_Free( data );
		return NULL;
	}

	if( sc->width == 2 ) {
		int i;
		short *wdata;

		wdata = ( short * )( data + info.dataofs );
		len = sc->length * sc->channels;
		for( i = 0; i < len; i++ ) {
			wdata[i] = LittleLong( wdata[i] );
		}
	}

	sc->length = ResampleSfx( info.samples, info.rate, info.channels, info.width, data + info.dataofs, sc->data, s->name );
	sc->channels = info.channels;
	sc->width = info.width;
	sc->speed = dma.speed;
	s->cache = sc;

	S_Free( data );

	return sc;
}

/*
* S_LoadSound
*/
sfxcache_t *S_LoadSound( sfx_t *s ) {
	const char *extension;

	if( !s->name[0] ) {
		return NULL;
	}
	if( s->isUrl ) {
		return NULL;
	}

	// see if still in memory
	if( s->cache ) {
		return s->cache;
	}

	extension = COM_FileExtension( s->name );
	if( extension ) {
		if( !Q_stricmp( extension, ".wav" ) ) {
			return S_LoadSound_Wav( s );
		}
		if( !Q_stricmp( extension, ".ogg" ) ) {
			return SNDOGG_Load( s );
		}
	}

	return NULL;
}



/*
===============================================================================

WAV loading

===============================================================================
*/


uint8_t *data_p;
uint8_t *iff_end;
uint8_t *last_chunk;
uint8_t *iff_data;
int iff_chunk_len;


static short GetLittleShort( void ) {
	short val = 0;
	val = *data_p;
	val = val + ( *( data_p + 1 ) << 8 );
	data_p += 2;
	return val;
}

static int GetLittleLong( void ) {
	int val = 0;
	val = *data_p;
	val = val + ( *( data_p + 1 ) << 8 );
	val = val + ( *( data_p + 2 ) << 16 );
	val = val + ( *( data_p + 3 ) << 24 );
	data_p += 4;
	return val;
}

static void FindNextChunk( char *name ) {
	while( 1 ) {
		data_p = last_chunk;

		if( data_p >= iff_end ) { // didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if( iff_chunk_len < 0 ) {
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( ( iff_chunk_len + 1 ) & ~1 );
		if( !strncmp( (char *) data_p, name, 4 ) ) {
			return;
		}
	}
}

static void FindChunk( char *name ) {
	last_chunk = iff_data;
	FindNextChunk( name );
}

/*
* GetWavinfo
*/
wavinfo_t GetWavinfo( const char *name, uint8_t *wav, int wavlength ) {
	wavinfo_t info;
	int format;

	memset( &info, 0, sizeof( info ) );

	if( !wav ) {
		return info;
	}

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk( "RIFF" );
	if( !( data_p && !strncmp( (char *) data_p + 8, "WAVE", 4 ) ) ) {
		Com_Printf( "Missing RIFF/WAVE chunks\n" );
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk( "fmt " );
	if( !data_p ) {
		Com_Printf( "Missing fmt chunk\n" );
		return info;
	}

	data_p += 8;
	format = GetLittleShort();
	if( format != 1 ) {
		Com_Printf( "Microsoft PCM format only\n" );
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;
	info.width = GetLittleShort() / 8;

	// find data chunk
	FindChunk( "data" );
	if( !data_p ) {
		Com_Printf( "Missing data chunk\n" );
		return info;
	}

	data_p += 4;
	info.samples = GetLittleLong() / info.width / info.channels;

	info.dataofs = data_p - wav;

	return info;
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
void SNDOGG_Init( bool verbose ) {
}

static int SNDOGG_Read( bgTrack_t *track, void *ptr, int samples );
static bool SNDOGG_Reset( bgTrack_t *track );
static void SNDOGG_FClose( bgTrack_t *track );

/*
* SNDOGG_Load
*/
sfxcache_t *SNDOGG_Load( sfx_t *s ) {
	sfxcache_t *sc;
	int channels = 0, rate = 0;
	short *data;
	int len, samples;

	assert( s && s->name[0] );
	assert( !s->cache );

	samples = qvorbis_load_file( s->name, &channels, &rate, &data );

	if( samples < 0 ) {
		Com_Printf( "Error unsupported .ogg file: %s\n", s->name );
		return NULL;
	}

	if( channels != 1 && channels != 2 ) {
		Com_Printf( "Error unsupported .ogg file (unsupported number of channels: %i): %s\n", channels, s->name );
		free( data );
		return NULL;
	}

	len = (int) ( (double) samples * (double) dma.speed / (double) rate );
	len = len * 2 * channels;

	sc = s->cache = S_Malloc( len + sizeof( sfxcache_t ) );
	sc->length = samples;
	sc->speed = rate;
	sc->channels = channels;
	sc->width = 2;
	if( sc->speed != dma.speed ) {
		sc->length = ResampleSfx( samples, sc->speed, sc->channels, 2, (uint8_t *)data, sc->data, s->name );
	} else {
		memcpy( sc->data, data, len );
	}
	sc->speed = dma.speed;

	free( data );

	return sc;
}

/*
* SNDOGG_OpenTrack
*/
bool SNDOGG_OpenTrack( bgTrack_t *track ) {
	int file;
	int rate, channels;
	const char *real_path;
	qvorbis_stream_t *v;

	if( !track ) {
		return false;
	}

	real_path = track->filename;
	trap_FS_FOpenFile( real_path, &file, FS_READ | FS_NOSIZE );

	if( !file ) {
		return false;
	}

	v = S_Malloc( sizeof( qvorbis_stream_t ) );
	v->filenum = file;

	if( !qvorbis_stream_init( v, &rate, &channels ) ) {
		goto error;
	}

	track->file = file;
	track->read = SNDOGG_Read;
	track->reset = SNDOGG_Reset;
	track->close = SNDOGG_FClose;
	track->vorbisFile = v;

	track->info.rate = rate;
	track->info.channels = channels;
	track->info.width = 2;
	track->info.dataofs = 0;
	track->info.samples = 0;

	return true;

error:
	qvorbis_stream_deinit( v );
	S_Free( v );

	trap_FS_FCloseFile( file );

	track->file = 0;
	track->vorbisFile = NULL;
	track->read = NULL;
	track->reset = NULL;
	track->close = NULL;

	return false;
}

/*
* SNDOGG_FRead
*/
static int SNDOGG_Read( bgTrack_t *track, void *ptr, int samples ) {
	return qvorbis_stream_read_samples( track->vorbisFile, samples, track->info.channels, track->info.width, ptr );
}

/*
* SNDOGG_FSeek
*/
static bool SNDOGG_Reset( bgTrack_t *track ) {
	return qvorbis_stream_reset( track->vorbisFile );
}

/*
* SNDOGG_FClose
*/
static void SNDOGG_FClose( bgTrack_t *track ) {
	if( track->vorbisFile != NULL ) {
		qvorbis_stream_deinit( track->vorbisFile );
		S_Free( track->vorbisFile );
	}

	if( track->file ) {
		trap_FS_FCloseFile( track->file );
	}

	track->file = 0;
	track->vorbisFile = 0;
}
