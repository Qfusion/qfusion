/*
Copyright (C) 2002-2014 Victor Luchits
Copyright (C) 2003 Dr. Tim Ferguson

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

RoQ FORMAT PLAYBACK

Based on work by Dr. Tim Ferguson: http://www.csse.monash.edu.au/~timf/

=======================================================================
*/

#include "cin_roq.h"
#include "roq.h"

typedef struct {
	roq_chunk_t chunk;
	roq_cell_t cells[256];
	roq_qcell_t qcells[256];

	int width_2;
	int height_2;

	cin_yuv_t cyuv[2];
	uint8_t         *yuv_pixels;
} roq_info_t;

static short snd_sqr_arr[256];

/*
* RoQ_Init
*/
static void RoQ_Init( void ) {
	int i;
	static bool init = false;

	if( init ) {
		return;
	}

	init = true;

	for( i = 0; i < 128; i++ ) {
		snd_sqr_arr[i] = i * i;
		snd_sqr_arr[i + 128] = -( i * i );
	}
}

/*
* RoQ_ReadChunk
*/
static void RoQ_ReadChunk( cinematics_t *cin ) {
	roq_info_t *roq = cin->fdata;
	roq_chunk_t *chunk = &roq->chunk;

	trap_FS_Read( &chunk->id, sizeof( short ), cin->file );
	trap_FS_Read( &chunk->size, sizeof( int ), cin->file );
	trap_FS_Read( &chunk->argument, sizeof( short ), cin->file );

	chunk->id = LittleShort( chunk->id );
	chunk->size = LittleLong( chunk->size );
	chunk->argument = LittleShort( chunk->argument );
}

/*
* RoQ_SkipBlock
*/
static inline void RoQ_SkipBlock( cinematics_t *cin, int size ) {
	trap_FS_Seek( cin->file, size, FS_SEEK_CUR );
}

/*
* RoQ_SkipChunk
*/
static void RoQ_SkipChunk( cinematics_t *cin ) {
	roq_info_t *roq = cin->fdata;
	RoQ_SkipBlock( cin, roq->chunk.size );
}

/*
* RoQ_ReadInfo
*/
static void RoQ_ReadInfo( cinematics_t *cin ) {
	short t[4];
	roq_info_t *roq = cin->fdata;
	cin_yuv_t *cyuv = roq->cyuv;
	uint8_t *pixels;
	int width, height;

	trap_FS_Read( t, sizeof( short ) * 4, cin->file );

	width = LittleShort( t[0] );
	height = LittleShort( t[1] );
	if( cin->width != width || cin->height != height ) {
		int i, j;
		int width_2 = width / 2, height_2 = height / 2;

		cin->width = width;
		cin->height = height;

		if( roq->yuv_pixels ) {
			CIN_Free( roq->yuv_pixels );
		}

		roq->width_2 = width_2;
		roq->height_2 = height_2;
		roq->yuv_pixels = CIN_Alloc( cin->mempool,
									 ( width * height + width_2 * height_2 * 2 ) * 2 );

		pixels = roq->yuv_pixels;
		for( i = 0; i < 2; i++ ) {
			cyuv[i].width = width;
			cyuv[i].height = height;
			cyuv[i].image_width = width;
			cyuv[i].image_height = height;
			cyuv[i].x_offset = 0;
			cyuv[i].y_offset = 0;

			// Y
			cyuv[i].yuv[0].width = width;
			cyuv[i].yuv[0].height = height;
			cyuv[i].yuv[0].stride = width;
			cyuv[i].yuv[0].data = pixels;
			pixels += width * height;

			// UV
			for( j = 1; j < 3; j++ ) {
				cyuv[i].yuv[j].width = width_2;
				cyuv[i].yuv[j].height = height_2;
				cyuv[i].yuv[j].stride = width_2;
				cyuv[i].yuv[j].data = pixels;
				pixels += width_2 * height_2;
			}
		}
	}
}

/*
* RoQ_ReadCodebook
*/
static void RoQ_ReadCodebook( cinematics_t *cin ) {
	unsigned int nv1, nv2;
	roq_info_t *roq = cin->fdata;
	roq_chunk_t *chunk = &roq->chunk;

	nv1 = ( chunk->argument >> 8 ) & 0xFF;
	if( !nv1 ) {
		nv1 = 256;
	}

	nv2 = chunk->argument & 0xFF;
	if( !nv2 && ( nv1 * 6 < chunk->size ) ) {
		nv2 = 256;
	}

	trap_FS_Read( roq->cells, sizeof( roq_cell_t ) * nv1, cin->file );
	trap_FS_Read( roq->qcells, sizeof( roq_qcell_t ) * nv2, cin->file );
}

/*
* RoQ_ApplyVector2x2
*/
static void RoQ_ApplyVector2x2( cinematics_t *cin, int xpos, int ypos, const roq_cell_t *cell ) {
	uint8_t *dst_y0, *dst_y1;
	uint8_t *dst_u, *dst_v;
	roq_info_t *roq = cin->fdata;
	cin_img_plane_t *plane;
	int xpos_2 = xpos / 2, ypos_2 = ypos / 2;

	// Y
	plane = &roq->cyuv[0].yuv[0];
	dst_y0 = plane->data + ypos * plane->stride + xpos;
	dst_y1 = dst_y0 + plane->stride;

	// U
	plane = &roq->cyuv[0].yuv[1];
	dst_u = plane->data + ypos_2 * plane->stride + xpos_2;

	// V
	plane = &roq->cyuv[0].yuv[2];
	dst_v = plane->data + ypos_2 * plane->stride + xpos_2;

	dst_y0[0] = cell->y[0];
	dst_y0[1] = cell->y[1];
	dst_y1[0] = cell->y[2];
	dst_y1[1] = cell->y[3];
	*dst_u = cell->u;
	*dst_v = cell->v;
}

/*
* RoQ_ApplyVector4x4
*/
static void RoQ_ApplyVector4x4( cinematics_t *cin, int xpos, int ypos, const roq_cell_t *cell ) {
	uint8_t *dst_y0, *dst_y1;
	uint8_t *dst_u0, *dst_v0;
	uint8_t p[4], u[2], v[2];
	roq_info_t *roq = cin->fdata;
	cin_img_plane_t *y_plane, *u_plane, *v_plane;
	int xpos_2 = xpos / 2, ypos_2 = ypos / 2;

	// Y
	y_plane = &roq->cyuv[0].yuv[0];
	dst_y0 = y_plane->data + ypos * y_plane->stride + xpos;
	dst_y1 = dst_y0 + y_plane->stride;

	p[0] = p[1] = cell->y[0];
	p[2] = p[3] = cell->y[1];
	*(int *)dst_y0 = *(int *)p;
	*(int *)dst_y1 = *(int *)p;

	dst_y0 += y_plane->stride * 2;
	dst_y1 += y_plane->stride * 2;

	p[0] = p[1] = cell->y[2];
	p[2] = p[3] = cell->y[3];
	*(int *)dst_y0 = *(int *)p;
	*(int *)dst_y1 = *(int *)p;

	// U
	u_plane = &roq->cyuv[0].yuv[1];
	dst_u0 = u_plane->data + ypos_2 * u_plane->stride + xpos_2;

	// V
	v_plane = &roq->cyuv[0].yuv[2];
	dst_v0 = v_plane->data + ypos_2 * v_plane->stride + xpos_2;

	u[0] = u[1] = cell->u;
	v[0] = v[1] = cell->v;

	*(short *)dst_u0 = *(short *)u;
	*(short *)dst_v0 = *(short *)v;

	dst_u0 += u_plane->stride;
	dst_v0 += v_plane->stride;

	*(short *)dst_u0 = *(short *)u;
	*(short *)dst_v0 = *(short *)v;
}

/*
* RoQ_ApplyMotion4x4
*/
static void RoQ_ApplyMotion4x4( cinematics_t *cin, int xpos, int ypos, uint8_t mv, char mean_x, char mean_y ) {
	int i, j;
	int xpos_2, ypos_2;
	int xpos1, ypos1, xpos1_2, ypos1_2;
	uint8_t *src, *dst;
	roq_info_t *roq = cin->fdata;
	cin_img_plane_t *plane, *plane1;

	// calc source coords
	xpos1 = xpos + 8 - ( mv >> 4 ) - mean_x;
	ypos1 = ypos + 8 - ( mv & 0xF ) - mean_y;

	xpos_2 = xpos / 2;
	ypos_2 = ypos / 2;
	xpos1_2 = xpos1 / 2;
	ypos1_2 = ypos1 / 2;

	// Y
	plane  = &roq->cyuv[0].yuv[0];
	plane1 = &roq->cyuv[1].yuv[0];
	dst = plane->data  + ( ypos *  plane->stride  + xpos );
	src = plane1->data + ( ypos1 * plane1->stride + xpos1 );
	for( j = 0; j < 4; j++ ) {
		*(int *)dst = *(int *)src;
		src += plane1->stride;
		dst += plane->stride;
	}

	// UV
	for( i = 1; i < 3; i++ ) {
		plane  = &roq->cyuv[0].yuv[i];
		plane1 = &roq->cyuv[1].yuv[i];
		dst = plane->data  + ( ypos_2 *  plane->stride  + xpos_2 );
		src = plane1->data + ( ypos1_2 * plane1->stride + xpos1_2 );
		for( j = 0; j < 2; j++ ) {
			*(short *)dst = *(short *)src;
			src += plane1->stride;
			dst += plane->stride;
		}
	}
}

/*
* RoQ_ApplyMotion8x8
*/
static void RoQ_ApplyMotion8x8( cinematics_t *cin, int xpos, int ypos, uint8_t mv, char mean_x, char mean_y ) {
	int i, j;
	int xpos_2, ypos_2;
	int xpos1, ypos1, xpos1_2, ypos1_2;
	uint8_t *src, *dst;
	roq_info_t *roq = cin->fdata;
	cin_img_plane_t *plane, *plane1;

	// calc source coords
	xpos1 = xpos + 8 - ( mv >> 4 ) - mean_x;
	ypos1 = ypos + 8 - ( mv & 0xF ) - mean_y;

	xpos_2 = xpos / 2;
	ypos_2 = ypos / 2;
	xpos1_2 = xpos1 / 2;
	ypos1_2 = ypos1 / 2;

	// Y
	plane  = &roq->cyuv[0].yuv[0];
	plane1 = &roq->cyuv[1].yuv[0];
	dst = plane->data  + ( ypos *  plane->stride  + xpos );
	src = plane1->data + ( ypos1 * plane1->stride + xpos1 );
	for( j = 0; j < 8; j++ ) {
		memcpy( dst, src, 8 );
		src += plane1->stride;
		dst += plane->stride;
	}

	// UV
	for( i = 1; i < 3; i++ ) {
		plane  = &roq->cyuv[0].yuv[i];
		plane1 = &roq->cyuv[1].yuv[i];
		dst = plane->data  + ( ypos_2 *  plane->stride  + xpos_2 );
		src = plane1->data + ( ypos1_2 * plane1->stride + xpos1_2 );
		for( j = 0; j < 4; j++ ) {
			*(int *)dst = *(int *)src;
			src += plane1->stride;
			dst += plane->stride;
		}
	}
}

/*
* RoQ_ReadVideo
*/
#define RoQ_READ_BLOCK  0x4000
static cin_yuv_t *RoQ_ReadVideo( cinematics_t *cin ) {
	roq_info_t *roq = cin->fdata;
	roq_chunk_t *chunk = &roq->chunk;
	int i, vqflg, vqflg_pos, vqid;
	int xpos, ypos, x, y, xp, yp;
	uint8_t c;
	roq_qcell_t *qcell;
	uint8_t raw[RoQ_READ_BLOCK];
	unsigned remaining, bpos, read;

	vqflg = 0;
	vqflg_pos = -1;
	xpos = ypos = 0;

#define RoQ_ReadRaw() read = min( sizeof( raw ), remaining ); remaining -= read; trap_FS_Read( raw, read, cin->file );
#define RoQ_ReadByte( x ) if( bpos >= read ) { RoQ_ReadRaw(); bpos = 0; } ( x ) = raw[bpos++];
#define RoQ_ReadShort( x ) if( bpos + 1 == read ) { c = raw[bpos]; RoQ_ReadRaw(); ( x ) = ( raw[0] << 8 ) | c; bpos = 1; } \
	else { if( bpos + 1 > read ) { RoQ_ReadRaw(); bpos = 0; } ( x ) = ( raw[bpos + 1] << 8 ) | raw[bpos]; bpos += 2; }
#define RoQ_ReadFlag() if( vqflg_pos < 0 ) { RoQ_ReadShort( vqflg ); vqflg_pos = 7; } \
	vqid = ( vqflg >> ( vqflg_pos * 2 ) ) & 0x3; vqflg_pos--;

	for( bpos = read = 0, remaining = chunk->size; bpos < read || remaining; ) {
		for( yp = ypos; yp < ypos + 16; yp += 8 )
			for( xp = xpos; xp < xpos + 16; xp += 8 ) {
				RoQ_ReadFlag();

				switch( vqid ) {
					case RoQ_ID_MOT:
						break;

					case RoQ_ID_FCC:
						RoQ_ReadByte( c );
						RoQ_ApplyMotion8x8( cin, xp, yp, c,
											( char )( ( chunk->argument >> 8 ) & 0xff ),
											( char )( chunk->argument & 0xff ) );
						break;

					case RoQ_ID_SLD:
						RoQ_ReadByte( c );
						qcell = roq->qcells + c;
						RoQ_ApplyVector4x4( cin, xp, yp, roq->cells + qcell->idx[0] );
						RoQ_ApplyVector4x4( cin, xp + 4, yp, roq->cells + qcell->idx[1] );
						RoQ_ApplyVector4x4( cin, xp, yp + 4, roq->cells + qcell->idx[2] );
						RoQ_ApplyVector4x4( cin, xp + 4, yp + 4, roq->cells + qcell->idx[3] );
						break;

					case RoQ_ID_CCC:
						for( i = 0; i < 4; i++ ) {
							x = xp; if( i & 0x01 ) {
								x += 4;
							}
							y = yp; if( i & 0x02 ) {
								y += 4;
							}

							RoQ_ReadFlag();

							switch( vqid ) {
								case RoQ_ID_MOT:
									break;

								case RoQ_ID_FCC:
									RoQ_ReadByte( c );
									RoQ_ApplyMotion4x4( cin, x, y, c,
														( char )( ( chunk->argument >> 8 ) & 0xff ),
														( char )( chunk->argument & 0xff ) );
									break;

								case RoQ_ID_SLD:
									RoQ_ReadByte( c );
									qcell = roq->qcells + c;
									RoQ_ApplyVector2x2( cin, x, y, roq->cells + qcell->idx[0] );
									RoQ_ApplyVector2x2( cin, x + 2, y, roq->cells + qcell->idx[1] );
									RoQ_ApplyVector2x2( cin, x, y + 2, roq->cells + qcell->idx[2] );
									RoQ_ApplyVector2x2( cin, x + 2, y + 2, roq->cells + qcell->idx[3] );
									break;

								case RoQ_ID_CCC:
									RoQ_ReadByte( c ); RoQ_ApplyVector2x2( cin, x, y, roq->cells + c );
									RoQ_ReadByte( c ); RoQ_ApplyVector2x2( cin, x + 2, y, roq->cells + c );
									RoQ_ReadByte( c ); RoQ_ApplyVector2x2( cin, x, y + 2, roq->cells + c );
									RoQ_ReadByte( c ); RoQ_ApplyVector2x2( cin, x + 2, y + 2, roq->cells + c );
									break;

								default:
									Com_DPrintf( "Unknown vq code: %d\n", vqid );
									break;
							}
						}
						break;

					default:
						Com_DPrintf( "Unknown vq code: %d\n", vqid );
						break;
				}
			}

		xpos += 16;
		if( xpos >= cin->width ) {
			xpos -= cin->width;

			ypos += 16;
			if( ypos >= cin->height ) {
				RoQ_SkipBlock( cin, remaining );     // ignore remaining trash
				break;
			}
		}
	}

	return roq->cyuv;
}

/*
* RoQ_ReadAudio
*/
static void RoQ_ReadAudio( cinematics_t *cin ) {
	unsigned int i;
	int snd_left, snd_right;
	uint8_t raw[RoQ_READ_BLOCK];
	short samples[RoQ_READ_BLOCK];
	roq_info_t *roq = cin->fdata;
	roq_chunk_t *chunk = &roq->chunk;
	unsigned int remaining, read;

	if( chunk->id == RoQ_SOUND_MONO ) {
		snd_left = chunk->argument;
		snd_right = 0;
	} else {
		snd_left = chunk->argument & 0xff00;
		snd_right = ( chunk->argument & 0xff ) << 8;
	}

	for( remaining = chunk->size; remaining > 0; remaining -= read ) {
		read = min( sizeof( raw ), remaining );
		trap_FS_Read( raw, read, cin->file );

		if( chunk->id == RoQ_SOUND_MONO ) {
			for( i = 0; i < read; i++ ) {
				snd_left += snd_sqr_arr[raw[i]];
				samples[i] = (short)snd_left;
				snd_left = (short)snd_left;
			}

			CIN_RawSamplesToListeners( cin, read, cin->s_rate, 2, 1, (uint8_t *)samples );
		} else if( chunk->id == RoQ_SOUND_STEREO ) {
			for( i = 0; i < read; i += 2 ) {
				snd_left += snd_sqr_arr[raw[i]];
				samples[i + 0] = (short)snd_left;
				snd_left = (short)snd_left;

				snd_right += snd_sqr_arr[raw[i + 1]];
				samples[i + 1] = (short)snd_right;
				snd_right = (short)snd_right;
			}

			CIN_RawSamplesToListeners( cin, read / 2, cin->s_rate, 2, 2, (uint8_t *)samples );
		}
	}
}

/*
* RoQ_ReadNextFrameYUV_CIN
*/
cin_yuv_t *RoQ_ReadNextFrameYUV_CIN( cinematics_t *cin, bool *redraw ) {
	roq_info_t *roq = cin->fdata;
	roq_chunk_t *chunk = &roq->chunk;
	cin_yuv_t *cyuv = NULL;

	while( !trap_FS_Eof( cin->file ) ) {
		RoQ_ReadChunk( cin );

		if( trap_FS_Eof( cin->file ) ) {
			return NULL;
		}
		if( chunk->size <= 0 ) {
			continue;
		}

		if( chunk->id == RoQ_INFO ) {
			RoQ_ReadInfo( cin );
		} else if( ( chunk->id == RoQ_SOUND_MONO || chunk->id == RoQ_SOUND_STEREO ) ) {
			RoQ_ReadAudio( cin );
		} else if( chunk->id == RoQ_QUAD_VQ ) {
			*redraw = true;
			cyuv = RoQ_ReadVideo( cin );
			break;
		} else if( chunk->id == RoQ_QUAD_CODEBOOK ) {
			RoQ_ReadCodebook( cin );
		} else {
			RoQ_SkipChunk( cin );
		}
	}

	if( cyuv ) {
		if( cin->frame > 0 ) {
			// swap buffers
			cin_yuv_t tp;
			tp = roq->cyuv[0]; roq->cyuv[0] = roq->cyuv[1]; roq->cyuv[1] = tp;
		} else {
			int i;
			// init back buffer for inter-frame motion compensation
			for( i = 0; i < 3; i++ ) {
				memcpy( roq->cyuv[1].yuv[i].data, roq->cyuv[0].yuv[i].data,
						roq->cyuv[0].yuv[i].width * roq->cyuv[0].yuv[i].height );
			}
		}
		cin->frame++;
	}

	return cyuv;
}

/*
* RoQ_Init_CIN
*/
bool RoQ_Init_CIN( cinematics_t *cin ) {
	roq_info_t *roq;
	roq_chunk_t *chunk;

	roq = CIN_Alloc( cin->mempool, sizeof( *roq ) );
	cin->fdata = roq;
	chunk = &roq->chunk;

	// nasty hack
	cin->framerate = RoQ_FRAMERATE;
	cin->s_rate = 22050;
	cin->s_width = 2;
	cin->yuv = true;

	RoQ_Init();

	// read header
	RoQ_ReadChunk( cin );
	if( chunk->id != RoQ_HEADER1 || chunk->size != RoQ_HEADER2 || chunk->argument != RoQ_HEADER3 ) {
		Com_Printf( S_COLOR_YELLOW "Invalid video file %s\n", cin->name );
		return false;
	}

	cin->headerlen = trap_FS_Tell( cin->file );

	return true;
}

/*
* RoQ_HasOggAudio_CIN
*/
bool RoQ_HasOggAudio_CIN( cinematics_t *cin ) {
	return false;
}

/*
* RoQ_Shutdown_CIN
*/
void RoQ_Shutdown_CIN( cinematics_t *cin ) {

}

/*
* RoQ_Reset_CIN
*/
void RoQ_Reset_CIN( cinematics_t *cin ) {
	// try again from the beginning if looping
	trap_FS_Seek( cin->file, cin->headerlen, FS_SEEK_SET );
}

/*
* RoQ_NeedNextFrame
*/
bool RoQ_NeedNextFrame_CIN( cinematics_t *cin ) {
	unsigned int frame;

	if( cin->cur_time <= cin->start_time ) {
		return false;
	}

	frame = ( cin->cur_time - cin->start_time ) * cin->framerate / 1000.0;
	if( frame <= cin->frame ) {
		return false;
	}

	if( frame > cin->frame + 1 ) {
		Com_DPrintf( "Dropped frame: %i > %i\n", frame, cin->frame + 1 );
		cin->start_time = cin->cur_time - cin->frame * 1000 / cin->framerate;
	}

	return true;
}
