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

OGG THEORA FORMAT PLAYBACK

=======================================================================
*/

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/theoradec.h>
#include "cin_theora.h"

#define OGG_BUFFER_SIZE     4 * 1024

typedef struct {
	bool a_stream;
	bool a_eos;
	bool v_stream;
	bool v_eos;

	double s_rate_msec;
	ogg_int64_t s_samples_read;
	ogg_int64_t s_samples_need;
	int64_t s_sound_time;

	ogg_sync_state oy;              /* sync and verify incoming physical bitstream */
	ogg_stream_state os_audio;
	ogg_stream_state os_video;

	vorbis_dsp_state vd;            /* central working state for the packet->PCM decoder */
	vorbis_info vi;                 /* struct that stores all the static vorbis bitstream settings */
	vorbis_comment vc;              /* struct that stores all the bitstream user comments */

	th_setup_info   *tsi;
	th_dec_ctx      *tctx;
	th_comment tc;
	th_info ti;
	ogg_int64_t th_granulepos;
	unsigned int th_granulemsec;
	th_ycbcr_buffer th_yuv;
	cin_yuv_t pub_yuv;
	unsigned int th_seek_msec_to;
	bool th_seek_to_keyframe;
	unsigned int th_max_keyframe_interval;      /* maximum time between keyframes in msecs */
} qtheora_info_t;

static void *oggLibrary;

#ifdef OGGLIB_RUNTIME

static int ( *qogg_sync_init )( ogg_sync_state * );
static int (*qogg_sync_clear)( ogg_sync_state * );
static char *(*qogg_sync_buffer)( ogg_sync_state *, long );
static int (*qogg_sync_wrote)( ogg_sync_state *, long );
static int (*qogg_sync_pageout)( ogg_sync_state *, ogg_page * );
static int (*qogg_stream_init)( ogg_stream_state *, int );
static int (*qogg_stream_clear)( ogg_stream_state * );
static int (*qogg_stream_pagein)( ogg_stream_state *, ogg_page * );
static int (*qogg_stream_packetout)( ogg_stream_state *, ogg_packet * );
static int (*qogg_page_bos)( const ogg_page * );
static int (*qogg_page_serialno)( const ogg_page * );

static dllfunc_t oggfuncs[] =
{
	{ "ogg_sync_init", ( void **)&qogg_sync_init },
	{ "ogg_sync_clear", ( void **)&qogg_sync_clear },
	{ "ogg_sync_buffer", ( void **)&qogg_sync_buffer },
	{ "ogg_sync_wrote", ( void **)&qogg_sync_wrote },
	{ "ogg_sync_pageout", ( void **)&qogg_sync_pageout },
	{ "ogg_stream_init", ( void **)&qogg_stream_init },
	{ "ogg_stream_clear", ( void **)&qogg_stream_clear },
	{ "ogg_stream_pagein", ( void **)&qogg_stream_pagein },
	{ "ogg_stream_packetout", ( void **)&qogg_stream_packetout },
	{ "ogg_page_bos", ( void **)&qogg_page_bos },
	{ "ogg_page_serialno", ( void **)&qogg_page_serialno },
	{ NULL, NULL },
};

#else

#define qogg_sync_init ogg_sync_init
#define qogg_sync_clear ogg_sync_clear
#define qogg_sync_buffer ogg_sync_buffer
#define qogg_sync_wrote ogg_sync_wrote
#define qogg_sync_pageout ogg_sync_pageout
#define qogg_stream_init ogg_stream_init
#define qogg_stream_clear ogg_stream_clear
#define qogg_stream_pagein ogg_stream_pagein
#define qogg_stream_packetout ogg_stream_packetout
#define qogg_page_bos ogg_page_bos
#define qogg_page_serialno ogg_page_serialno

#endif

static void *vorbisLibrary;

#ifdef VORBISLIB_RUNTIME

static int ( *qvorbis_block_init )( vorbis_dsp_state *v, vorbis_block *vb );
static int (*qvorbis_block_clear)( vorbis_block *vb );
static void (*qvorbis_info_init)( vorbis_info *vi );
static void (*qvorbis_info_clear)( vorbis_info *vi );
static void (*qvorbis_comment_init)( vorbis_comment *vc );
static void (*qvorbis_comment_clear)( vorbis_comment *vc );
static void (*qvorbis_dsp_clear)( vorbis_dsp_state *v );
static int (*qvorbis_synthesis_init)( vorbis_dsp_state *v, vorbis_info *vi );
static int (*qvorbis_synthesis)( vorbis_block *vb, ogg_packet *op );
static int (*qvorbis_synthesis_blockin)( vorbis_dsp_state *v, vorbis_block *vb );
static int (*qvorbis_synthesis_pcmout)( vorbis_dsp_state *v, float ***pcm );
static int (*qvorbis_synthesis_headerin)( vorbis_info *vi, vorbis_comment *vc, ogg_packet *op );
static int (*qvorbis_synthesis_read)( vorbis_dsp_state *v, int samples );

static dllfunc_t vorbisfuncs[] =
{
	{ "vorbis_block_init", ( void **)&qvorbis_block_init },
	{ "vorbis_block_clear", ( void **)&qvorbis_block_clear },
	{ "vorbis_info_init", ( void **)&qvorbis_info_init },
	{ "vorbis_info_clear", ( void **)&qvorbis_info_clear },
	{ "vorbis_comment_init", ( void **)&qvorbis_comment_init },
	{ "vorbis_comment_clear", ( void **)&qvorbis_comment_clear },
	{ "vorbis_dsp_clear", ( void **)&qvorbis_dsp_clear },
	{ "vorbis_synthesis_init", ( void **)&qvorbis_synthesis_init },
	{ "vorbis_synthesis", ( void **)&qvorbis_synthesis },
	{ "vorbis_synthesis_blockin", ( void **)&qvorbis_synthesis_blockin },
	{ "vorbis_synthesis_pcmout", ( void **)&qvorbis_synthesis_pcmout },
	{ "vorbis_synthesis_headerin", ( void **)&qvorbis_synthesis_headerin },
	{ "vorbis_synthesis_read", ( void **)&qvorbis_synthesis_read },
	{ NULL, NULL },
};

#else

#define qvorbis_block_init vorbis_block_init
#define qvorbis_block_clear vorbis_block_clear
#define qvorbis_info_init vorbis_info_init
#define qvorbis_info_clear vorbis_info_clear
#define qvorbis_comment_init vorbis_comment_init
#define qvorbis_comment_clear vorbis_comment_clear
#define qvorbis_dsp_clear vorbis_dsp_clear
#define qvorbis_synthesis_init vorbis_synthesis_init
#define qvorbis_synthesis vorbis_synthesis
#define qvorbis_synthesis_blockin vorbis_synthesis_blockin
#define qvorbis_synthesis_pcmout vorbis_synthesis_pcmout
#define qvorbis_synthesis_headerin vorbis_synthesis_headerin
#define qvorbis_synthesis_read vorbis_synthesis_read

#endif

static void *theoraLibrary;

#ifdef THEORALIB_RUNTIME

static double ( *qth_granule_time )( void *_encdec, ogg_int64_t _granpos );
static int (*qth_decode_ctl)( th_dec_ctx *_dec, int _req,void *_buf,size_t _buf_sz );
static int (*qth_packet_iskeyframe)( ogg_packet *_op );
static int (*qth_decode_headerin)( th_info *_info,th_comment *_tc, th_setup_info **_setup,ogg_packet *_op );
static int (*qth_decode_packetin)( th_dec_ctx *_dec, const ogg_packet *_op,ogg_int64_t *_granpos );
static int (*qth_packet_isheader)( ogg_packet *_op );
static int (*qth_decode_ycbcr_out)( th_dec_ctx *_dec, th_ycbcr_buffer _ycbcr );
static ogg_int64_t (*qth_granule_frame)( void *_encdec,ogg_int64_t _granpos );
static void (*qth_comment_init)( th_comment *_tc );
static void (*qth_comment_clear)( th_comment *_tc );
static void (*qth_info_init)( th_info *_info );
static void (*qth_info_clear)( th_info *_info );
static void (*qth_setup_free)( th_setup_info *_setup );
static th_dec_ctx *(*qth_decode_alloc)( const th_info *_info,const th_setup_info *_setup );
static void (*qth_decode_free)( th_dec_ctx *_dec );

static dllfunc_t theorafuncs[] =
{
	{ "th_granule_time", ( void **)&qth_granule_time },
	{ "th_decode_ctl", ( void **)&qth_decode_ctl },
	{ "th_packet_iskeyframe", ( void **)&qth_packet_iskeyframe },
	{ "th_decode_headerin", ( void **)&qth_decode_headerin },
	{ "th_decode_packetin", ( void **)&qth_decode_packetin },
	{ "th_packet_isheader", ( void **)&qth_packet_isheader },
	{ "th_decode_ycbcr_out", ( void **)&qth_decode_ycbcr_out },
	{ "th_granule_frame", ( void **)&qth_granule_frame },
	{ "th_comment_init", ( void **)&qth_comment_init },
	{ "th_comment_clear", ( void **)&qth_comment_clear },
	{ "th_info_init", ( void **)&qth_info_init },
	{ "th_info_clear", ( void **)&qth_info_clear },
	{ "th_setup_free", ( void **)&qth_setup_free },
	{ "th_decode_alloc", ( void **)&qth_decode_alloc },
	{ "th_decode_free", ( void **)&qth_decode_free },
	{ NULL, NULL },
};

#else

#define qth_granule_time th_granule_time
#define qth_decode_ctl th_decode_ctl
#define qth_packet_iskeyframe th_packet_iskeyframe
#define qth_decode_headerin th_decode_headerin
#define qth_decode_packetin th_decode_packetin
#define qth_packet_isheader th_packet_isheader
#define qth_decode_ycbcr_out th_decode_ycbcr_out
#define qth_granule_frame th_granule_frame
#define qth_comment_init th_comment_init
#define qth_comment_clear th_comment_clear
#define qth_info_init th_info_init
#define qth_info_clear th_info_clear
#define qth_setup_free th_setup_free
#define qth_decode_alloc th_decode_alloc
#define qth_decode_free th_decode_free

#endif

// =============================================================================

/*
* Theora_UnloadOggLibrary
*/
static void Theora_UnloadOggLibrary( void ) {
#ifdef OGGLIB_RUNTIME
	if( oggLibrary ) {
		trap_UnloadLibrary( &oggLibrary );
	}
#endif
	oggLibrary = NULL;
}

/*
* Theora_LoadOggLibrary
*/
void Theora_LoadOggLibrary( void ) {
	Theora_UnloadOggLibrary();

#ifdef OGGLIB_RUNTIME
	oggLibrary = trap_LoadLibrary( LIBOGG_LIBNAME, oggfuncs );
#else
	oggLibrary = (void *)1;
#endif
}

/*
* Theora_UnloadVorbisLibrary
*/
static void Theora_UnloadVorbisLibrary( void ) {
#ifdef VORBISLIB_RUNTIME
	if( vorbisLibrary ) {
		trap_UnloadLibrary( &vorbisLibrary );
	}
#endif
	vorbisLibrary = NULL;
}

/*
* Theora_LoadVorbisLibrary
*/
void Theora_LoadVorbisLibrary( void ) {
	Theora_UnloadVorbisLibrary();

#ifdef VORBISLIB_RUNTIME
	vorbisLibrary = trap_LoadLibrary( LIBVORBIS_LIBNAME, vorbisfuncs );
#else
	vorbisLibrary = (void *)1;
#endif
}

/*
* Theora_UnloadTheoraLibrary
*/
void Theora_UnloadTheoraLibrary( void ) {
#ifdef VORBISLIB_RUNTIME
	if( theoraLibrary ) {
		trap_UnloadLibrary( &theoraLibrary );
	}
#endif
	theoraLibrary = NULL;
}

/*
* Theora_LoadTheoraLibrary
*/
static void Theora_LoadTheoraLibrary( void ) {
#ifdef THEORALIB_RUNTIME
	theoraLibrary = trap_LoadLibrary( LIBTHEORA_LIBNAME, theorafuncs );
#else
	theoraLibrary = (void *)1;
#endif
}

/*
* Theora_UnloadTheoraLibraries
*/
void Theora_UnloadTheoraLibraries( void ) {
	Theora_UnloadOggLibrary();
	Theora_UnloadVorbisLibrary();
	Theora_UnloadTheoraLibrary();
}

/*
* Theora_LoadTheoraLibraries
*/
void Theora_LoadTheoraLibraries( void ) {
	Theora_LoadOggLibrary();
	Theora_LoadVorbisLibrary();
	Theora_LoadTheoraLibrary();

	if( !oggLibrary || !vorbisLibrary || !theoraLibrary ) {
		Theora_UnloadTheoraLibraries();
	}
}

// =============================================================================


/*
* Ogg_LoadBlockToSync
*
* Returns number of bytes transferred
*/
static int Ogg_LoadBlockToSync( cinematics_t *cin ) {
	int bytes;
	char *buffer;
	qtheora_info_t *qth = cin->fdata;

	if( trap_FS_Eof( cin->file ) ) {
		return 0;
	}

	buffer = qogg_sync_buffer( &qth->oy, OGG_BUFFER_SIZE );
	bytes = trap_FS_Read( buffer, OGG_BUFFER_SIZE, cin->file );
	qogg_sync_wrote( &qth->oy, bytes );

	return bytes;
}

/*
* Ogg_LoadPagesToStreams
*/
static void Ogg_LoadPagesToStreams( qtheora_info_t *qth, ogg_page *page ) {
	// this can be done blindly; a stream won't accept a page that doesn't belong to it
	if( qth->a_stream ) {
		qogg_stream_pagein( &qth->os_audio, page );
	}
	if( qth->v_stream ) {
		qogg_stream_pagein( &qth->os_video, page );
	}
}


#define RAW_BUFFER_SIZE     8 * 1024
#define AUDIO_PRELOAD_MSEC  200

/*
* OggVorbis_NeedAudioData
*/
static bool OggVorbis_NeedAudioData( cinematics_t *cin ) {
	ogg_int64_t audio_time;
	qtheora_info_t *qth = cin->fdata;

	if( !qth->a_stream || qth->a_eos ) {
		return false;
	}

	audio_time = (ogg_int64_t)cin->cur_time - cin->start_time - cin->s_samples_length + AUDIO_PRELOAD_MSEC;
	if( audio_time <= 0 ) {
		return false;
	}

	qth->s_samples_need = (ogg_int64_t)( (double)audio_time * qth->s_rate_msec );

	// read only as much samples as we need according to the timer
	if( qth->s_samples_read >= qth->s_samples_need ) {
		return false;
	}

	return true;
}

/*
* OggVorbis_LoadAudioFrame
*
* Returns true if no additional audio packets are needed
*/
static bool OggVorbis_LoadAudioFrame( cinematics_t *cin ) {
	int i, val;
	short* ptr;
	float **pcm;
	float *right,*left;
	bool haveAudio = false;
	int samples, samplesNeeded;
	uint8_t rawBuffer[RAW_BUFFER_SIZE];
	ogg_packet op;
	vorbis_block vb;
	qtheora_info_t *qth = cin->fdata;

	memset( &op, 0, sizeof( op ) );
	memset( &vb, 0, sizeof( vb ) );

	qvorbis_block_init( &qth->vd, &vb );

read_samples:
	while( ( samples = qvorbis_synthesis_pcmout( &qth->vd, &pcm ) ) > 0 ) {
		// vorbis -> raw
		ptr = (short *)rawBuffer;

		samplesNeeded = sizeof( rawBuffer ) / ( cin->s_width * cin->s_channels );
		if( samplesNeeded > samples ) {
			samplesNeeded = samples;
		}
		if( samplesNeeded > qth->s_samples_need - qth->s_samples_read ) {
			samplesNeeded = qth->s_samples_need - qth->s_samples_read;
		}

		if( cin->num_listeners > 0 ) {
			if( cin->s_channels == 1 ) {
				left = right = pcm[0];
				for( i = 0; i < samplesNeeded; i++ ) {
					val = ( left[i] * 32767.f + 0.5f );
					ptr[0] = Q_bound( -32768, val, 32767 );

					ptr += 1;
				}
			} else {
				left = pcm[0];
				right = pcm[1];
				for( i = 0; i < samplesNeeded; i++ ) {
					val = ( left[i] * 32767.f + 0.5f );
					ptr[0] = Q_bound( -32768, val, 32767 );

					val = ( right[i] * 32767.f + 0.5f );
					ptr[1] = Q_bound( -32768, val, 32767 );

					ptr += cin->s_channels;
				}
			}

			CIN_RawSamplesToListeners( cin, i, cin->s_rate, cin->s_width, cin->s_channels, rawBuffer );
		}

		// tell libvorbis how many samples we actually consumed
		qvorbis_synthesis_read( &qth->vd, samplesNeeded );

		haveAudio = true;
		qth->s_samples_read += samplesNeeded;

		if( !OggVorbis_NeedAudioData( cin ) ) {
			qvorbis_block_clear( &vb );
			return true;
		}
	}

	if( qogg_stream_packetout( &qth->os_audio, &op ) ) {
		if( op.e_o_s ) {
			// end of stream packet
			qth->a_eos = true;
		} else if( qvorbis_synthesis( &vb, &op ) == 0 ) {
			qvorbis_synthesis_blockin( &qth->vd, &vb );
			goto read_samples;
		}
	}

	qvorbis_block_clear( &vb );
	return haveAudio;
}

/*
* OggTheora_NeedVideoData
*/
static bool OggTheora_NeedVideoData( cinematics_t *cin ) {
	unsigned int realframe;
	qtheora_info_t *qth = cin->fdata;
	int64_t sync_time = qth->s_sound_time;

	if( !cin->width ) {
		// need at least one valid frame
		return true;
	}

	// sync to audio timer
	realframe = sync_time * cin->framerate / 1000.0;
	if( realframe > cin->frame ) {
		return true;
	}

	return false;
}

/*
* OggTheora_LoadVideoFrame
*
* Return true if a new video frame has been successfully loaded
*/
#define VIDEO_LAG_TOLERANCE_MSEC    500

static bool OggTheora_LoadVideoFrame( cinematics_t *cin ) {
	int i;
	ogg_packet op;
	qtheora_info_t *qth = cin->fdata;
	th_ycbcr_buffer yuv;
	int64_t sync_time = qth->s_sound_time;

	memset( &op, 0, sizeof( op ) );

	while( qogg_stream_packetout( &qth->os_video, &op ) ) {
		int error;
		int width, height;

		if( op.e_o_s ) {
			// we've encountered end of stream packet
			qth->v_eos = true;
			break;
		}

		if( op.granulepos >= 0 ) {
			qth->th_granulemsec = qth_granule_time( qth->tctx, op.granulepos ) * 1000.0;
			qth_decode_ctl( qth->tctx, TH_DECCTL_SET_GRANPOS, &op.granulepos, sizeof( op.granulepos ) );
		}

		// if lagging behind audio, seek forward to max_keyframe_interval before the target,
		// then skip to nearest keyframe
		if( ( op.granulepos >= 0 )
			&& ( qth->a_stream != false )
			&& ( qth->a_eos == false )
			&& ( sync_time > qth->th_granulemsec + VIDEO_LAG_TOLERANCE_MSEC ) ) {
			qth->th_seek_msec_to =
				sync_time <= qth->th_max_keyframe_interval
				? qth->th_max_keyframe_interval : sync_time - qth->th_max_keyframe_interval;
			qth->th_seek_to_keyframe = true;
		}

		// seek to msec
		if( qth->th_seek_msec_to > 0 ) {
			if( ( op.granulepos >= 0 ) && ( qth->th_seek_msec_to <= cin->start_time + qth->th_granulemsec ) ) {
				qth->th_seek_msec_to = 0;
			} else {
				Com_DPrintf( "Dropped frame %i\n", cin->frame );
				continue;
			}
		}

		// seek to keyframe
		if( qth->th_seek_to_keyframe ) {
			if( !qth_packet_iskeyframe( &op ) ) {
				Com_DPrintf( "Dropped frame %i\n", cin->frame );
				continue;
			}
			qth->th_seek_to_keyframe = false;
		}

		error = qth_decode_packetin( qth->tctx, &op, &qth->th_granulepos );
		if( error < 0 ) {
			// bad packet
			continue;
		}

		if( qth_packet_isheader( &op ) ) {
			// header packet, skip
			continue;
		}

		if( error == TH_DUPFRAME ) {
			return true;
		}

		if( qth_decode_ycbcr_out( qth->tctx, yuv ) != 0 ) {
			// error
			continue;
		}

		memcpy( &qth->th_yuv, &yuv, sizeof( yuv ) );

		for( i = 0; i < 3; i++ ) {
			qth->pub_yuv.yuv[i].stride = yuv[i].stride;
			qth->pub_yuv.yuv[i].width = yuv[i].width;
			qth->pub_yuv.yuv[i].height = yuv[i].height;
			qth->pub_yuv.yuv[i].data = yuv[i].data;
		}

		width  = qth->ti.pic_width & ~1;
		height = qth->ti.pic_height & ~1;

		qth->pub_yuv.width  = width;
		qth->pub_yuv.height = height;
		qth->pub_yuv.x_offset = qth->ti.pic_x & ~1;
		qth->pub_yuv.y_offset = qth->ti.pic_y & ~1;
		qth->pub_yuv.image_width = max( abs( yuv[0].stride ), (int)qth->ti.frame_width );
		qth->pub_yuv.image_height = qth->ti.frame_height;

		if( cin->width != width || cin->height != height ) {
			size_t size;

			if( cin->vid_buffer ) {
				CIN_Free( cin->vid_buffer );
			}

			cin->width = width;
			cin->height = height;

			size = cin->width * cin->height * 3;
			cin->vid_buffer = CIN_Alloc( cin->mempool, size );
			memset( cin->vid_buffer, 0xFF, size );
		}

		cin->frame = qth_granule_frame( qth->tctx, qth->th_granulepos );

		return true;
	}

	return false;
}

/*
* Theora_ReadNextFrame_CIN_
*/
static bool Theora_ReadNextFrame_CIN_( cinematics_t *cin, bool *redraw, bool *eos ) {
	unsigned int bytes, pages = 0;
	bool redraw_ = false;
	qtheora_info_t *qth = cin->fdata;
	bool haveAudio = false, haveVideo = false;

	*eos = false;

	while( 1 ) {
		ogg_page og;
		bool needAudio, needVideo;

		needAudio = !haveAudio && OggVorbis_NeedAudioData( cin );
		needVideo = !haveVideo && OggTheora_NeedVideoData( cin );
		redraw_ = redraw_ || needVideo;

		if( !needAudio && !needVideo ) {
			break;
		}

		if( needAudio ) {
			haveAudio = OggVorbis_LoadAudioFrame( cin );
			needAudio = !haveAudio;
		}
		if( needVideo ) {
			haveVideo = OggTheora_LoadVideoFrame( cin );
			needVideo = !haveVideo;
		}

		if( qth->v_eos ) {
			// end of video stream
			*eos = true;
			return false;
		}

		if( !needAudio && !needVideo ) {
			break;
		}

		bytes = Ogg_LoadBlockToSync( cin ); // returns 0 if EOF

		// process all read pages
		pages = 0;
		while( qogg_sync_pageout( &qth->oy, &og ) > 0 ) {
			pages++;
			Ogg_LoadPagesToStreams( qth, &og );
		}

		if( !bytes && !pages ) {
			// end of FILE, no pages remaining
			*eos = true;
			return false;
		}
	}

	*redraw = redraw_;
	return haveVideo;
}

#ifdef THEORA_SOFTWARE_YUV2RGB

// taken from http://www.gamedev.ru/code/articles/?id=4252&page=3
#define Theora_InitYCbCrTable() \
	int cc = 0; \
	int b0_[256], b1_[256], b2_[256], b3_[256]; \
 \
	for( cc = 0; cc < 256; cc++ ) \
		b0_[cc] = ( 113443 * ( cc - 128 ) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b1_[cc] = (  45744 * ( cc - 128 ) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b2_[cc] = (  22020 * ( cc - 128 ) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b3_[cc] = ( 113508 * ( cc - 128 ) + 32768 ) >> 16; \

/*
* Theora_DecodeYCbCr2RGB_420
*/
static void Theora_DecodeYCbCr2RGB_420( cin_yuv_t *cyuv, int bytes, uint8_t *out ) {
	int
		x_offset = cyuv->x_offset,
		y_offset = cyuv->y_offset;
	unsigned int
		width = cyuv->width,
		height = cyuv->height;
	int
		yStride = cyuv->yuv[0].stride,
		uStride = cyuv->yuv[1].stride,
		vStride = cyuv->yuv[2].stride,
		outStride = width * bytes;
	uint8_t
	*yData = cyuv->yuv[0].data + ( x_offset     ) + yStride * ( y_offset     ),
	*uData = cyuv->yuv[1].data + ( x_offset >> 1 ) + uStride * ( y_offset >> 1 ),
	*vData = cyuv->yuv[2].data + ( x_offset >> 1 ) + vStride * ( y_offset >> 1 );
	uint8_t
	*yRow  = yData,
	*yRow2 = yData + yStride,
	*uRow  = uData,
	*vRow  = vData;
	uint8_t
	*oRow  = out,
	*oRow2 = out + outStride;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Theora_InitYCbCrTable();

	yStride += yStride - width;
	uStride += 0 - ( width >> 1 );
	vStride += 0 - ( width >> 1 );
	outStride += outStride - width * bytes;

	for( yPos = 1; yPos <= height; yPos += 2 ) {
		for( xPos = 1; xPos <= width; xPos++ ) {
			u = *uRow;
			v = *vRow;

			y = *yRow++;
			VectorSet( c, y + b0_[v], y - b1_[v] - b2_[u], y + b3_[u] );
			oRow[0] = bound( 0, c[0], 255 );
			oRow[1] = bound( 0, c[1], 255 );
			oRow[2] = bound( 0, c[2], 255 );
			oRow   += bytes;

			y = *yRow2++;
			VectorSet( c, y + b0_[v], y - b1_[v] - b2_[u], y + b3_[u] );
			oRow2[0] = bound( 0, c[0], 255 );
			oRow2[1] = bound( 0, c[1], 255 );
			oRow2[2] = bound( 0, c[2], 255 );
			oRow2   += bytes;

			if( ( xPos & 1 ) != 0 ) {
				uRow++;
				vRow++;
			}
		}

		yRow  += yStride;
		yRow2 += yStride;
		uRow  += uStride;
		vRow  += vStride;

		oRow  += outStride;
		oRow2 += outStride;
	}
}

/*
* Theora_DecodeYCbCr2RGB_422
*/
static void Theora_DecodeYCbCr2RGB_422( cin_yuv_t *cyuv, int bytes, uint8_t *out ) {
	int
		x_offset = cyuv->x_offset,
		y_offset = cyuv->y_offset;
	unsigned int
		width = cyuv->width,
		height = cyuv->height;
	int
		yStride = cyuv->yuv[0].stride,
		uStride = cyuv->yuv[1].stride,
		vStride = cyuv->yuv[2].stride,
		outStride = width * bytes;
	uint8_t
	*yData = cyuv->yuv[0].data + ( x_offset     ) + yStride * ( y_offset ),
	*uData = cyuv->yuv[1].data + ( x_offset >> 1 ) + uStride * ( y_offset ),
	*vData = cyuv->yuv[2].data + ( x_offset >> 1 ) + vStride * ( y_offset );
	uint8_t
	*yRow  = yData,
	*uRow  = uData,
	*vRow  = vData;
	uint8_t
	*oRow  = out;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Theora_InitYCbCrTable();

	yStride -= width;
	uStride -= width / 2;
	vStride -= width / 2;
	outStride -= width * bytes;

	for( yPos = 1; yPos <= height; yPos++ ) {
		for( xPos = 1; xPos <= width; xPos += 2 ) {
			u = *uRow++;
			v = *vRow++;

			y = *yRow++;
			VectorSet( c, y + b0_[v], y - b1_[v] - b2_[u], y + b3_[u] );
			oRow[0] = bound( 0, c[0], 255 );
			oRow[1] = bound( 0, c[1], 255 );
			oRow[2] = bound( 0, c[2], 255 );
			oRow   += bytes;

			y = *yRow++;
			VectorSet( c, y + b0_[v], y - b1_[v] - b2_[u], y + b3_[u] );
			oRow[0] = bound( 0, c[0], 255 );
			oRow[1] = bound( 0, c[1], 255 );
			oRow[2] = bound( 0, c[2], 255 );
			oRow   += bytes;
		}

		yRow  += yStride;
		uRow  += uStride;
		vRow  += vStride;

		oRow  += outStride;
	}
}

/*
* Theora_DecodeYCbCr2RGB_444
*/
static void Theora_DecodeYCbCr2RGB_444( cin_yuv_t *cyuv, int bytes, uint8_t *out ) {
	int
		x_offset = cyuv->x_offset,
		y_offset = cyuv->y_offset;
	unsigned int
		width = cyuv->width,
		height = cyuv->height;
	int
		yStride = cyuv->yuv[0].stride,
		uStride = cyuv->yuv[1].stride,
		vStride = cyuv->yuv[2].stride,
		outStride = width * bytes;
	uint8_t
	*yData = cyuv->yuv[0].data + x_offset + yStride * y_offset,
	*uData = cyuv->yuv[1].data + x_offset + uStride * y_offset,
	*vData = cyuv->yuv[2].data + x_offset + vStride * y_offset;
	uint8_t
	*yRow  = yData,
	*uRow  = uData,
	*vRow  = vData;
	uint8_t
	*oRow  = out;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Theora_InitYCbCrTable();

	yStride -= width;
	uStride -= width;
	vStride -= width;
	outStride -= width * bytes;

	for( yPos = 1; yPos <= height; yPos++ ) {
		for( xPos = 1; xPos <= width; xPos++ ) {
			u = *uRow++;
			v = *vRow++;
			y = *yRow++;

			VectorSet( c, y + b0_[v], y - b1_[v] - b2_[u], y + b3_[u] );
			oRow[0] = bound( 0, c[0], 255 );
			oRow[1] = bound( 0, c[1], 255 );
			oRow[2] = bound( 0, c[2], 255 );
			oRow   += bytes;
		}

		yRow  += yStride;
		uRow  += uStride;
		vRow  += vStride;

		oRow  += outStride;
	}
}

/*
* Theora_DecodeYCbCr2RGB
*/
static void Theora_DecodeYCbCr2RGB( th_pixel_fmt pfmt, cin_yuv_t *cyuv, int bytes, uint8_t *out ) {
	switch( pfmt ) {
		case TH_PF_444:
			Theora_DecodeYCbCr2RGB_444( cyuv, bytes, out );
			break;
		case TH_PF_422:
			Theora_DecodeYCbCr2RGB_422( cyuv, bytes, out );
			break;
		case TH_PF_420:
			Theora_DecodeYCbCr2RGB_420( cyuv, bytes, out );
			break;
		default:
			break;
	}
}

/*
* Theora_ReadNextFrame_CIN
*/
uint8_t *Theora_ReadNextFrame_CIN( cinematics_t *cin, bool *redraw ) {
	bool eos;
	bool haveVideo;
	qtheora_info_t *qth = cin->fdata;

	haveVideo = Theora_ReadNextFrame_CIN_( cin, redraw, &eos );
	if( eos ) {
		return NULL;
	}

	if( haveVideo ) {
		// convert YCbCr to RGB
		Theora_DecodeYCbCr2RGB( qth->ti.pixel_fmt, &qth->pub_yuv, 3, cin->vid_buffer );
	}

	return cin->vid_buffer;
}

#endif // THEORA_SOFTWARE_YUV2RGB

/*
* Theora_ReadNextFrameYUV_CIN
*/
cin_yuv_t *Theora_ReadNextFrameYUV_CIN( cinematics_t *cin, bool *redraw ) {
	bool eos;
	qtheora_info_t *qth = cin->fdata;

	Theora_ReadNextFrame_CIN_( cin, redraw, &eos );
	if( eos ) {
		return NULL;
	}

	return cin->width ? &qth->pub_yuv : NULL;
}

/*
* Theora_Init_CIN
*/
bool Theora_Init_CIN( cinematics_t *cin ) {
	int status;
	ogg_page og;
	ogg_packet op;
	int vorbis_p, theora_p;
	qtheora_info_t *qth;

	qth = CIN_Alloc( cin->mempool, sizeof( *qth ) );
	memset( qth, 0, sizeof( *qth ) );
	cin->fdata = ( void * )qth;
	CIN_Free( cin->vid_buffer );
	cin->vid_buffer = NULL;
	cin->width = cin->height = 0;

	if( !theoraLibrary ) {
		return false;
	}

	// start up Ogg stream synchronization layer
	qogg_sync_init( &qth->oy );

	// init supporting Vorbis structures needed in header parsing
	qvorbis_info_init( &qth->vi );
	qvorbis_comment_init( &qth->vc );

	// init supporting Theora structures needed in header parsing
	qth_comment_init( &qth->tc );
	qth_info_init( &qth->ti );

	// Ogg file open; parse the headers
	// Only interested in Vorbis/Theora streams
	vorbis_p = theora_p = 0;
	qth->v_stream = qth->a_stream = false;
	qth->a_eos = qth->v_eos = false;

	status = 0;
	while( !status ) {
		if( !Ogg_LoadBlockToSync( cin ) ) {
			break;
		}

		while( qogg_sync_pageout( &qth->oy, &og ) > 0 ) {
			ogg_stream_state test;

			// is this a mandated initial header? If not, stop parsing
			if( !qogg_page_bos( &og ) ) {
				// don't leak the page; get it into the appropriate stream
				Ogg_LoadPagesToStreams( qth, &og );
				status = 1;
				break;
			}

			qogg_stream_init( &test, qogg_page_serialno( &og ) );
			qogg_stream_pagein( &test, &og );
			qogg_stream_packetout( &test, &op );

			// identify the codec: try theora
			if( !qth->v_stream && qth_decode_headerin( &qth->ti, &qth->tc, &qth->tsi, &op ) >= 0 ) {
				// it is theora
				qth->v_stream = true;
				memcpy( &qth->os_video, &test, sizeof( test ) );
				theora_p = 1;
			} else if( !qth->a_stream && !qvorbis_synthesis_headerin( &qth->vi, &qth->vc, &op ) && !( cin->flags & CIN_NOAUDIO ) ) {
				// it is vorbis
				qth->a_stream = true;
				memcpy( &qth->os_audio, &test, sizeof( test ) );
				vorbis_p = 1;
			} else {
				// whatever it is, we don't care about it
				qogg_stream_clear( &test );
			}
		}
	}

	// we're expecting more header packets
	while( ( theora_p && theora_p < 3 ) || ( vorbis_p && vorbis_p < 3 ) ) {
		// look for further theora headers
		while( theora_p && ( theora_p < 3 ) && ( status = qogg_stream_packetout( &qth->os_video, &op ) ) ) {
			if( status < 0 ) {
				Com_Printf( S_COLOR_YELLOW "File %s: error parsing Theora stream headers; corrupt stream?\n", cin->name );
				return false;
			}
			if( qth_decode_headerin( &qth->ti, &qth->tc, &qth->tsi, &op ) == 0 ) {
				Com_Printf( S_COLOR_YELLOW "File %s: error parsing Theora stream headers; corrupt stream?\n", cin->name );
				return false;
			}
			theora_p++;
		}

		// look for more vorbis header packets
		while( vorbis_p && ( vorbis_p < 3 ) && ( status = qogg_stream_packetout( &qth->os_audio, &op ) ) ) {
			if( status < 0 ) {
				Com_Printf( S_COLOR_YELLOW "File %s: error parsing Vorbis stream headers; corrupt stream?\n", cin->name );
				return false;
			}
			if( qvorbis_synthesis_headerin( &qth->vi, &qth->vc, &op ) != 0 ) {
				Com_Printf( S_COLOR_YELLOW "File %s: error parsing Vorbis stream headers; corrupt stream?\n", cin->name );
				return false;
			}
			vorbis_p++;
			if( vorbis_p == 3 ) {
				break;
			}
		}

		// the header pages/packets will arrive before anything else we
		// care about, or the stream is not obeying spec
		if( qogg_sync_pageout( &qth->oy, &og ) > 0 ) {
			// demux into the appropriate stream
			Ogg_LoadPagesToStreams( qth, &og );
		} else if( !Ogg_LoadBlockToSync( cin ) ) {
			Com_Printf( S_COLOR_YELLOW "File %s: end of file while searching for codec headers\n", cin->name );
			return false;
		}
	}

	// and now we have it all. initialize decoders

	if( theora_p ) {
		qth->tctx = qth_decode_alloc( &qth->ti, qth->tsi );
		qth->th_granulepos = -1;
		qth->th_max_keyframe_interval = 1000 * ( ( 1 << qth->ti.keyframe_granule_shift ) + 1 ) * qth->ti.fps_denominator /
										qth->ti.fps_numerator;

		cin->framerate = (float)qth->ti.fps_numerator / qth->ti.fps_denominator;

		// the aspect ratio of the pixels
		// if either value is zero, the aspect ratio is undefined
		cin->aspect_numerator = qth->ti.aspect_numerator;
		cin->aspect_denominator = qth->ti.aspect_denominator;
		if( !cin->aspect_numerator || !cin->aspect_denominator ) {
			cin->aspect_numerator = cin->aspect_denominator = 1;
		}
	} else {
		// tear down the partial theora setup
		qth->v_stream = false;
		qth_comment_clear( &qth->tc );
		qth_info_clear( &qth->ti );
	}

	qth_setup_free( qth->tsi );

	if( vorbis_p ) {
		qvorbis_synthesis_init( &qth->vd, &qth->vi );

		cin->s_rate = qth->vi.rate;
		cin->s_width = 2;
		cin->s_channels = qth->vi.channels;
		qth->s_rate_msec = (double)cin->s_rate / 1000.0;
		qth->s_samples_read = 0;
		qth->s_samples_need = 0;
	} else {
		// tear down the partial vorbis setup
		qth->a_stream = false;
		qth->s_rate_msec = 0;
		qth->s_samples_read = 0;
		qth->s_samples_need = 0;

		qvorbis_comment_clear( &qth->vc );
		qvorbis_info_clear( &qth->vi );
	}

	if( !qth->v_stream || !cin->framerate ) {
		return false;
	}

	cin->headerlen = trap_FS_Tell( cin->file );
	cin->yuv = true;

	return true;
}

/*
* Theora_HasOggAudio_CIN
*/
bool Theora_HasOggAudio_CIN( cinematics_t *cin ) {
#if 1
	qtheora_info_t *qth = cin->fdata;
	return qth->a_stream;
#else
	return false;
#endif
}

/*
* Theora_Shutdown_CIN
*/
void Theora_Shutdown_CIN( cinematics_t *cin ) {
	qtheora_info_t *qth = cin->fdata;

	if( !theoraLibrary ) {
		return;
	}

	if( qth->v_stream ) {
		qth->v_stream = false;
		qth_info_clear( &qth->ti );
		qth_comment_clear( &qth->tc );
		qth_decode_free( qth->tctx );
	}

	if( qth->a_stream ) {
		qth->a_stream = false;
		qvorbis_dsp_clear( &qth->vd );
		qvorbis_comment_clear( &qth->vc );
		qvorbis_info_clear( &qth->vi );  // must be called last (comment from vorbis example code)
	}

	qogg_stream_clear( &qth->os_audio );
	qogg_stream_clear( &qth->os_video );

	qogg_sync_clear( &qth->oy );
}

/*
* Theora_Reset_CIN
*/
void Theora_Reset_CIN( cinematics_t *cin ) {
	Theora_Shutdown_CIN( cin );

	CIN_Free( cin->fdata );
	cin->fdata = NULL;

	trap_FS_Seek( cin->file, 0, FS_SEEK_SET );

	Theora_Init_CIN( cin );
}

/*
* Theora_NeedNextFrame_CIN
*/
bool Theora_NeedNextFrame_CIN( cinematics_t *cin ) {
	int64_t sys_time;
	qtheora_info_t *qth = cin->fdata;

	sys_time = cin->cur_time - cin->start_time;
	if( qth->a_stream ) {
		qth->s_sound_time = qth->s_samples_read / qth->s_rate_msec;
		if( qth->s_sound_time < cin->s_samples_length ) {
			qth->s_sound_time = 0;
		} else {
			qth->s_sound_time -= cin->s_samples_length;
		}
	} else {
		qth->s_sound_time = sys_time;
	}

	return OggVorbis_NeedAudioData( cin )
		   || OggTheora_NeedVideoData( cin );
}
