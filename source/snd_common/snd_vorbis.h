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

#include <stdint.h>

#define SND_OGG_MIN_PRELOAD_SAMPLES 8 * 1024
#define SND_OGG_MAX_PRELOAD_SAMPLES 64 * 1024

typedef struct {
	int filenum;
	void *v;
	struct {
		float **frame_outputs;
		int frame_samples;
		int frame_channels;
		int frame_samples_offs;
	} stb;
	struct {
		uint8_t* data;
		size_t size;
		size_t rpos, wpos;
	} readbuf;
} qvorbis_stream_t;

int qvorbis_load_file( const char* filename, int* channels, int* rate, short** data );

int qvorbis_stream_readbytes( qvorbis_stream_t *ogg_stream, int c );
int qvorbis_stream_advance( qvorbis_stream_t *ogg_stream, int c );
const uint8_t *qvorbis_stream_datap( qvorbis_stream_t *ogg_stream );
size_t qvorbis_stream_datasize( qvorbis_stream_t *ogg_stream );
void qvorbis_stream_rewind( qvorbis_stream_t *ogg_stream );
bool qvorbis_stream_init( qvorbis_stream_t *ogg_stream, int *rate, int *channels );
void qvorbis_stream_deinit( qvorbis_stream_t* ogg_stream );
int qvorbis_stream_reset( qvorbis_stream_t* ogg_stream );
int qvorbis_stream_read_samples( qvorbis_stream_t *ogg_stream, int samples, int c, int w, void* buffer );
