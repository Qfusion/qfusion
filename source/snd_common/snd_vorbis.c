/*
Copyright (C) 2020 Victor Luchits

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

#include "snd_local.h"
#include "snd_vorbis.h"

#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"

int qvorbis_load_file( const char *filename, int *channels, int *rate, short **data ) {
	int flen;
	uint8_t *fcontents;
	int filenum, samples;

	assert( filename && filename[0] );
	if( !filename ) {
		return -1;
	}

	flen = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( !filenum ) {
		return -1;
	}

	fcontents = S_Malloc( flen );
	trap_FS_Read( fcontents, flen, filenum );
	trap_FS_FCloseFile( filenum );

	samples = stb_vorbis_decode_memory( fcontents, flen, channels, rate, data );
	S_Free( fcontents );

	return samples;
}

int qvorbis_stream_readbytes( qvorbis_stream_t *ogg_stream, int c ) {
	int w;

	if( ogg_stream->readbuf.wpos + c > ogg_stream->readbuf.size ) {
		size_t newbuf_size = ogg_stream->readbuf.wpos + c + ogg_stream->readbuf.size / 2;
		uint8_t *newbuf = S_Malloc( newbuf_size );

		memcpy( newbuf, ogg_stream->readbuf.data, ogg_stream->readbuf.size );
		ogg_stream->readbuf.size = newbuf_size;
		S_Free( ogg_stream->readbuf.data );
		ogg_stream->readbuf.data = newbuf;
	}

	w = trap_FS_Read( ogg_stream->readbuf.data + ogg_stream->readbuf.wpos, c, ogg_stream->filenum );
	ogg_stream->readbuf.wpos += w;
	return w;
}

int qvorbis_stream_advance( qvorbis_stream_t *ogg_stream, int c ) {
	if( ogg_stream->readbuf.rpos + c > ogg_stream->readbuf.wpos ) {
		c = ogg_stream->readbuf.wpos - ogg_stream->readbuf.rpos;
	}
	ogg_stream->readbuf.rpos += c;
	return c;
}

const uint8_t *qvorbis_stream_datap( qvorbis_stream_t *ogg_stream ) {
	return ogg_stream->readbuf.data + ogg_stream->readbuf.rpos;
}

size_t qvorbis_stream_datasize( qvorbis_stream_t *ogg_stream ) {
	return ogg_stream->readbuf.wpos - ogg_stream->readbuf.rpos;
}

void qvorbis_stream_rewind( qvorbis_stream_t *ogg_stream ) {
	if( ogg_stream->readbuf.rpos == 0 ) {
		return;
	}
	memmove( ogg_stream->readbuf.data, ogg_stream->readbuf.data + ogg_stream->readbuf.rpos, ogg_stream->readbuf.wpos - ogg_stream->readbuf.rpos );
	ogg_stream->readbuf.wpos -= ogg_stream->readbuf.rpos;
	ogg_stream->readbuf.rpos = 0;
}

bool qvorbis_stream_init( qvorbis_stream_t *ogg_stream, int *rate, int *channels ) {
	int used = 0;
	stb_vorbis_info info;

	while( 1 ) {
		int error;

		int p = qvorbis_stream_readbytes( ogg_stream, 1024 );
		if( p == 0 ) {
			Com_Printf( "Error initializing .ogg stream (no header)\n" );
			return false;
		}

		ogg_stream->v = stb_vorbis_open_pushdata( qvorbis_stream_datap( ogg_stream ), qvorbis_stream_datasize( ogg_stream ), &used, &error, NULL );
		if( ogg_stream->v != NULL ) {
			break;
		}
		if( error == VORBIS_need_more_data ) {
			continue;
		}

		Com_Printf( "Error initializing .ogg stream (%d)\n", error );
		return false;
	}

	qvorbis_stream_advance( ogg_stream, used );

	info = stb_vorbis_get_info( ogg_stream->v );
	if( rate ) {
		*rate = info.sample_rate;
	}
	if( channels ) {
		*channels = info.channels;
	}

	return true;
}

void qvorbis_stream_deinit( qvorbis_stream_t* ogg_stream ) {
	stb_vorbis_close( ogg_stream->v );
	ogg_stream->v = NULL;
	S_Free( ogg_stream->readbuf.data );
	memset( &ogg_stream->stb, 0, sizeof( ogg_stream->stb ) );
	memset( &ogg_stream->readbuf, 0, sizeof( ogg_stream->readbuf ) );
}

int qvorbis_stream_reset( qvorbis_stream_t* ogg_stream ) {
	qvorbis_stream_deinit( ogg_stream );

	if( trap_FS_Seek( ogg_stream->filenum, 0, FS_SEEK_SET ) != 0 ) {
		return false;
	}

	return qvorbis_stream_init( ogg_stream, NULL, NULL );
}

static int qvorbis_stream_output_samples16( qvorbis_stream_t *ogg_stream, int samples, int c, void *buffer ) {
	int s, fc = ogg_stream->stb.frame_channels;
	float** outputs = ogg_stream->stb.frame_outputs;
	const float *left = outputs[0], *right = fc > 1 ? outputs[1] : outputs[0];
	const float scale = 32768.0f;
	short* shorts = buffer;

	left += ogg_stream->stb.frame_samples_offs;
	right += ogg_stream->stb.frame_samples_offs;

	Q_clamp( samples, 0, ogg_stream->stb.frame_samples - ogg_stream->stb.frame_samples_offs );

	if( c == 1 ) {
		for( s = 0; s < samples; s++ ) {
			int l = (int)(scale * left[s]);
			shorts[0] = (short)Q_bound( l, -32768, 32767 );
			shorts++;
		}
	} else {
		for( s = 0; s < samples; s++ ) {
			int l = (int)(scale * left[s]);
			int r = (int)(scale * right[s]);
			shorts[0] = (short)Q_bound( l, -32768, 32767 );
			shorts[1] = (short)Q_bound( r, -32768, 32767 );
			shorts += 2;
		}
	}

	ogg_stream->stb.frame_samples_offs += s;
	return s;
}

int qvorbis_stream_read_samples( qvorbis_stream_t *ogg_stream, int samples, int c, int w, void* buffer ) {
	int r;
	const int bps = c * w;

	assert( w == 2 );

	for( r = 0; r < samples; ) {
		int fc, fs, used;
		float **outputs;

		if( ogg_stream->stb.frame_samples_offs < ogg_stream->stb.frame_samples ) {
			r += qvorbis_stream_output_samples16( ogg_stream, samples - r, c, ((uint8_t *)buffer) + (size_t)bps * r );
			if( r == samples ) {
				break;
			}
		}

		used = stb_vorbis_decode_frame_pushdata( ogg_stream->v, 
			qvorbis_stream_datap( ogg_stream ), qvorbis_stream_datasize( ogg_stream ), &fc, &outputs, &fs );
		if( used == 0 ) {
			const int chunk = Q_bound( SND_OGG_MIN_PRELOAD_SAMPLES, samples - r, SND_OGG_MAX_PRELOAD_SAMPLES ) * bps;

			qvorbis_stream_rewind( ogg_stream );
			if( qvorbis_stream_readbytes( ogg_stream, chunk ) == 0 ) {
				break;
			}
			continue;
		}

		qvorbis_stream_advance( ogg_stream, used );

		ogg_stream->stb.frame_outputs = outputs;
		ogg_stream->stb.frame_samples = fs;
		ogg_stream->stb.frame_channels = fc;
		ogg_stream->stb.frame_samples_offs = 0;
	}

	return r;
}
