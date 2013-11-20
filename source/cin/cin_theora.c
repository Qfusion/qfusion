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

#define OGG_BUFFER_SIZE	4*1024 // 4096

typedef struct
{
	qboolean		 a_stream;
	qboolean		 a_eos;
	qboolean		 v_stream;
	qboolean		 v_eos;

	double			 inv_s_rate;
	unsigned int	 samples_read;

	ogg_sync_state   oy;			/* sync and verify incoming physical bitstream */
	ogg_stream_state os_audio;
	ogg_stream_state os_video;

	vorbis_dsp_state vd;			/* central working state for the packet->PCM decoder */
	vorbis_info      vi;			/* struct that stores all the static vorbis bitstream settings */
	vorbis_comment   vc;			/* struct that stores all the bitstream user comments */

	th_setup_info	*tsi;
	th_dec_ctx		*tctx;
	th_comment		tc;
	th_info			ti;
	ogg_int64_t		th_granulepos;
	th_ycbcr_buffer th_yuv;
	cin_yuv_t		pub_yuv;
} qtheora_info_t;

/*
* Ogg_LoadBlockToSync
*
* Returns number of bytes transferred
*/
static int Ogg_LoadBlockToSync( cinematics_t *cin )
{
	int bytes;
	char *buffer;
	qtheora_info_t *qth = cin->fdata;

	if( trap_FS_Eof( cin->file ) ) {
		return 0;
	}

	buffer = ogg_sync_buffer( &qth->oy, OGG_BUFFER_SIZE );
	bytes = trap_FS_Read( buffer, OGG_BUFFER_SIZE, cin->file );
	ogg_sync_wrote( &qth->oy, bytes );

	return bytes;
}

/*
* Ogg_LoadPagesToStreams
*/
static void Ogg_LoadPagesToStreams( qtheora_info_t *qth, ogg_page *page )
{
	// this can be done blindly; a stream won't accept a page that doesn't belong to it
	if( qth->a_stream ) 
		ogg_stream_pagein( &qth->os_audio, page );
	if( qth->v_stream ) 
		ogg_stream_pagein( &qth->os_video, page );
}


#define RAW_BUFFER_SIZE		8*1024
#define AUDIO_PRELOAD_MSEC	400

/*
* OggVorbis_NeedAudioData
*/
static qboolean OggVorbis_NeedAudioData( cinematics_t *cin )
{
	qtheora_info_t *qth = cin->fdata;

	if( qth->a_stream && !qth->a_eos
		&& ( cin->cur_time + AUDIO_PRELOAD_MSEC > cin->start_time + qth->samples_read * qth->inv_s_rate ) ) {
		return qtrue;
	}
	return qfalse;
}

/*
* OggVorbis_LoadAudioFrame
*
* Returns qtrue if no additional audio packets are needed
*/
static qboolean OggVorbis_LoadAudioFrame( cinematics_t *cin )
{
	int	i, val;
	short* ptr;
	float **pcm;
	float *right,*left;
	int	samples, samplesNeeded;
	qbyte rawBuffer[RAW_BUFFER_SIZE];
	ogg_packet op;
	vorbis_block vb;
	qtheora_info_t *qth = cin->fdata;

	memset( &op, 0, sizeof( op ) );
	memset( &vb, 0, sizeof( vb ) );

	vorbis_block_init( &qth->vd, &vb );

read_samples:
	while ( ( samples = vorbis_synthesis_pcmout( &qth->vd, &pcm ) ) > 0 ) {
		// vorbis -> raw
		ptr = (short *)rawBuffer;

		samplesNeeded = sizeof( rawBuffer ) / (cin->s_width * cin->s_channels);
		if( samplesNeeded > samples )
			samplesNeeded = samples;

		if( cin->s_channels == 1 )
		{
			left = right = pcm[0];
			for( i = 0; i < samplesNeeded; i++ )
			{
				val = (left[i] * 32767.f + 0.5f);
				ptr[0] = bound( -32768, val, 32767 );

				ptr += 1;
			}
		}
		else
		{
			left = pcm[0];
			right = pcm[1];
			for( i = 0; i < samplesNeeded; i++ )
			{
				val = (left[i] * 32767.f + 0.5f);
				ptr[0] = bound( -32768, val, 32767 );

				val = (right[i] * 32767.f + 0.5f);
				ptr[1] = bound( -32768, val, 32767 );

				ptr += cin->s_channels;
			}
		}

		// tell libvorbis how many samples we actually consumed
		vorbis_synthesis_read( &qth->vd, samplesNeeded ); 
		trap_S_RawSamples( i, cin->s_rate, cin->s_width, cin->s_channels, rawBuffer, qfalse );

		qth->samples_read += samplesNeeded;

		if( !OggVorbis_NeedAudioData( cin ) ) {
			vorbis_block_clear( &vb );
			return qtrue;
		}
	}

	if( ogg_stream_packetout( &qth->os_audio, &op ) ) {
		if( op.e_o_s ) {
			// end of stream packet
			qth->a_eos = qfalse;
			trap_S_Clear();
			return qtrue;
		}

		if( vorbis_synthesis( &vb, &op ) == 0 ) {
			vorbis_synthesis_blockin( &qth->vd, &vb );
			goto read_samples;
		}
	}

	vorbis_block_clear( &vb );
	return qfalse;
}

/*
* OggTheora_VideoGranuleMsec
*/
static unsigned int OggTheora_VideoGranuleMsec( cinematics_t *cin )
{
	qtheora_info_t *qth = cin->fdata;

	if( qth->th_granulepos >= 0 ) {
		return th_granule_time( qth->tctx, qth->th_granulepos ) * 1000.0;
	}
	return 0;
}

/*
* OggTheora_NeedVideoData
*/
static qboolean OggTheora_NeedVideoData( cinematics_t *cin )
{
	unsigned int realframe;
	qtheora_info_t *qth = cin->fdata;

	if( qth->a_stream && !qth->a_eos ) {
		// sync to audio
		if( cin->cur_time > cin->start_time + OggTheora_VideoGranuleMsec( cin ) ) {
			return qtrue;
		}
		return qfalse;
	}

	// sync to sys timer and framerate
	realframe = ( cin->cur_time - cin->start_time ) * cin->framerate / 1000.0;
	if( realframe > cin->frame ) {
		return qtrue;
	}

	return qfalse;
}

// taken from http://www.gamedev.ru/code/articles/?id=4252&page=3
#define Theora_InitYCbCrTable() \
	int cc = 0; \
	int b0_[256], b1_[256], b2_[256], b3_[256]; \
 \
	for( cc = 0; cc < 256; cc++ ) \
		b0_[cc] = ( 113443 * (cc-128) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b1_[cc] = (  45744 * (cc-128) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b2_[cc] = (  22020 * (cc-128) + 32768 ) >> 16; \
	for( cc = 0; cc < 256; cc++ ) \
		b3_[cc] = ( 113508 * (cc-128) + 32768 ) >> 16; \

/*
* Theora_DecodeYCbCr2RGB_420
*/
static void Theora_DecodeYCbCr2RGB_420( cin_yuv_t *cyuv, int bytes, qbyte *out )
{
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
	qbyte 
		*yData = cyuv->yuv[0].data + (x_offset     ) + yStride * (y_offset     ), 
		*uData = cyuv->yuv[1].data + (x_offset >> 1) + uStride * (y_offset >> 1),
		*vData = cyuv->yuv[2].data + (x_offset >> 1) + vStride * (y_offset >> 1);
	qbyte
		*yRow  = yData,
		*yRow2 = yData + yStride,
		*uRow  = uData,
		*vRow  = vData;
	qbyte
		*oRow  = out,
		*oRow2 = out + outStride;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Theora_InitYCbCrTable();

	yStride += yStride - width;
	uStride += 0 - (width >> 1);
	vStride += 0 - (width >> 1);
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
static void Theora_DecodeYCbCr2RGB_422( cin_yuv_t *cyuv, int bytes, qbyte *out )
{
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
	qbyte 
		*yData = cyuv->yuv[0].data + (x_offset     ) + yStride * (y_offset), 
		*uData = cyuv->yuv[1].data + (x_offset >> 1) + uStride * (y_offset),
		*vData = cyuv->yuv[2].data + (x_offset >> 1) + vStride * (y_offset);
	qbyte
		*yRow  = yData,
		*uRow  = uData,
		*vRow  = vData;
	qbyte
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
static void Theora_DecodeYCbCr2RGB_444( cin_yuv_t *cyuv, int bytes, qbyte *out )
{
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
	qbyte 
		*yData = cyuv->yuv[0].data + x_offset + yStride * y_offset, 
		*uData = cyuv->yuv[1].data + x_offset + uStride * y_offset,
		*vData = cyuv->yuv[2].data + x_offset + vStride * y_offset;
	qbyte
		*yRow  = yData,
		*uRow  = uData,
		*vRow  = vData;
	qbyte
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
static void Theora_DecodeYCbCr2RGB( th_pixel_fmt pfmt, cin_yuv_t *cyuv, int bytes, qbyte *out )
{
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
* OggTheora_LoadVideoFrame
*
* Return qtrue if a new video frame has been successfully loaded
*/
static qboolean OggTheora_LoadVideoFrame( cinematics_t *cin )
{
	int i;
	ogg_packet op;
	qtheora_info_t *qth = cin->fdata;
	th_ycbcr_buffer yuv;

	memset( &op, 0, sizeof( op ) );

	while( ogg_stream_packetout( &qth->os_video, &op ) )
	{
		int width, height;

		if( op.e_o_s ) {
			// we've encountered end of stream packet
			qth->v_eos = qtrue;
			break;
		}

        if( op.granulepos >= 0 ){
			th_decode_ctl( qth->tctx, TH_DECCTL_SET_GRANPOS, &op.granulepos, sizeof( op.granulepos ) );
        }

		if( th_decode_packetin( qth->tctx, &op, &qth->th_granulepos ) != 0 ) {
			// bad packet
			continue;
		}
		if( th_packet_isheader( &op ) ) {
			// header packet, skip
			continue;
		}

		cin->frame = th_granule_frame( qth->tctx, qth->th_granulepos );

		if( cin->cur_time > cin->start_time + OggTheora_VideoGranuleMsec( cin ) ) {
			Com_DPrintf( "Dropped frame: %i\n", cin->frame );
			continue;
		}

		if( th_decode_ycbcr_out( qth->tctx, yuv ) != 0 ) {
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

			// default to 255 for alpha
			size = cin->width * cin->height * 4;
			cin->vid_buffer = CIN_Alloc( cin->mempool, size );
			memset( cin->vid_buffer, 0xFF, size );
		}

		return qtrue;
	}

	return qfalse;
}

/*
* Theora_ReadNextFrame_CIN_
*/
static qboolean Theora_ReadNextFrame_CIN_( cinematics_t *cin, qboolean *redraw, qboolean *eos )
{
	unsigned int bytes, pages = 0;
	qboolean redraw_ = qfalse;
	qtheora_info_t *qth = cin->fdata;
	qboolean haveAudio = qfalse, haveVideo = qfalse;

	*eos = qfalse;

	while( 1 )
	{
		ogg_page og;
		qboolean needAudio, needVideo;

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
			*eos = qtrue;
			return qfalse;
		}

		if( !needAudio && !needVideo ) {
			break;
		}

		bytes = Ogg_LoadBlockToSync( cin ); // returns 0 if EOF

		// process all read pages
		pages = 0;
		while( ogg_sync_pageout( &qth->oy, &og ) > 0 ) {
			pages++;
			Ogg_LoadPagesToStreams( qth, &og );
		}

		if( !bytes && !pages ) {
			// end of FILE, no pages remaining
			*eos = qtrue;
			return qfalse;
		}
	}

	*redraw = redraw_;
	return haveVideo;
}

/*
* Theora_ReadNextFrame_CIN
*/
qbyte *Theora_ReadNextFrame_CIN( cinematics_t *cin, qboolean *redraw )
{
	qboolean eos;
	qboolean haveVideo;
	qtheora_info_t *qth = cin->fdata;

	haveVideo = Theora_ReadNextFrame_CIN_( cin, redraw, &eos );
	if( eos ) {
		return NULL;
	}

	if( haveVideo ) {
		// convert YCbCr to RGB	
		Theora_DecodeYCbCr2RGB( qth->ti.pixel_fmt, &qth->pub_yuv, 4, cin->vid_buffer );
	}

	return cin->vid_buffer;
}

/*
* Theora_ReadNextFrameYUV_CIN
*/
cin_yuv_t *Theora_ReadNextFrameYUV_CIN( cinematics_t *cin, qboolean *redraw )
{
	qboolean eos;
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
qboolean Theora_Init_CIN( cinematics_t *cin )
{
	int status;
	ogg_page	og;
	ogg_packet	op;
	int vorbis_p, theora_p;
	qtheora_info_t *qth;

	qth = CIN_Alloc( cin->mempool, sizeof( *qth ) );
	cin->fdata = ( void * )qth;
	memset( qth, 0, sizeof( *qth ) );

	// start up Ogg stream synchronization layer
	ogg_sync_init( &qth->oy );

	// init supporting Vorbis structures needed in header parsing
	vorbis_info_init( &qth->vi );
	vorbis_comment_init( &qth->vc );

	// init supporting Theora structures needed in header parsing
	th_comment_init( &qth->tc );
	th_info_init( &qth->ti );

	// Ogg file open; parse the headers
	// Only interested in Vorbis/Theora streams
	vorbis_p = theora_p = 0;
	qth->v_stream = qth->a_stream = qfalse;
	qth->a_eos = qth->v_eos = qfalse;

	status = 0;
	while( !status )
	{
		if( !Ogg_LoadBlockToSync( cin ) )
			break;

		while( ogg_sync_pageout( &qth->oy, &og ) > 0 )
		{
			ogg_stream_state test;

			// is this a mandated initial header? If not, stop parsing 
			if( !ogg_page_bos( &og ) ) {
				// don't leak the page; get it into the appropriate stream 
				Ogg_LoadPagesToStreams( qth, &og );
				status = 1;
				break;
			}

			ogg_stream_init( &test, ogg_page_serialno(&og ) );
			ogg_stream_pagein( &test, &og );
			ogg_stream_packetout( &test, &op );

			// identify the codec: try theora
			if( !qth->v_stream && th_decode_headerin( &qth->ti, &qth->tc, &qth->tsi, &op ) >= 0 )
			{
				// it is theora
				qth->v_stream = qtrue;
				memcpy( &qth->os_video, &test, sizeof( test ) );
				theora_p = 1;
			}
			else if( !qth->a_stream && (cin->flags & CIN_AUDIO) && !vorbis_synthesis_headerin( &qth->vi, &qth->vc, &op ) )
			{
				// it is vorbis
				qth->a_stream = qtrue;
				memcpy( &qth->os_audio, &test, sizeof( test ) );
				vorbis_p = 1;
			}
			else
			{
				// whatever it is, we don't care about it
				ogg_stream_clear( &test );
			}
		}
	}

	// we're expecting more header packets
	while( (theora_p && theora_p < 3) || (vorbis_p && vorbis_p < 3) )
	{
		// look for further theora headers
		while( theora_p && (theora_p < 3) && (status = ogg_stream_packetout( &qth->os_video, &op ) ) )
		{
			if( status < 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Theora stream headers; corrupt stream?\n" );
				return qfalse;
			}
			if( th_decode_headerin( &qth->ti, &qth->tc, &qth->tsi, &op ) == 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Theora stream headers; corrupt stream?\n" );
				return qfalse;
			}
			theora_p++;
		}

		// look for more vorbis header packets
		while( vorbis_p && (vorbis_p < 3) && (status = ogg_stream_packetout( &qth->os_audio, &op ) ) )
		{
			if( status < 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Vorbis stream headers; corrupt stream?\n" );
				return qfalse;
			}
			if( vorbis_synthesis_headerin( &qth->vi, &qth->vc, &op ) != 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Vorbis stream headers; corrupt stream?\n" );
				return qfalse;
			}
			vorbis_p++;
			if( vorbis_p == 3 )
				break;
		}

		// the header pages/packets will arrive before anything else we 
		// care about, or the stream is not obeying spec
		if( ogg_sync_pageout( &qth->oy, &og ) > 0 )
		{
			// demux into the appropriate stream
			Ogg_LoadPagesToStreams( qth, &og );
		}
		else if( !Ogg_LoadBlockToSync( cin ) )
		{
			Com_Printf( S_COLOR_YELLOW "File %s: end of file while searching for codec headers\n", cin->name );
			return qfalse;
		}
	}

	// and now we have it all. initialize decoders

	if( theora_p )
	{
		qth->tctx = th_decode_alloc( &qth->ti, qth->tsi );
		qth->th_granulepos = -1;

		cin->framerate = (float)qth->ti.fps_numerator / qth->ti.fps_denominator;

		// the aspect ratio of the pixels
		// if either value is zero, the aspect ratio is undefined
		cin->aspect_numerator = qth->ti.aspect_numerator;
		cin->aspect_denominator = qth->ti.aspect_denominator;
		if( !cin->aspect_numerator || !cin->aspect_denominator ) {
			cin->aspect_numerator = cin->aspect_denominator = 1;
		}
	}
	else
	{
		// tear down the partial theora setup
		qth->v_stream = qfalse;
		th_comment_clear( &qth->tc );
		th_info_clear( &qth->ti );
	}

	th_setup_free( qth->tsi );

	if( vorbis_p )
	{
		vorbis_synthesis_init( &qth->vd, &qth->vi );

		cin->s_rate = qth->vi.rate;
		cin->s_width = 2;
		cin->s_channels = qth->vi.channels;
		qth->inv_s_rate = 1000.0 / (double)cin->s_rate;
		qth->samples_read = 0;
	}
	else
	{
		// tear down the partial vorbis setup
		qth->a_stream = qfalse;
		qth->inv_s_rate = 0;
		qth->samples_read = 0;

		vorbis_comment_clear( &qth->vc );
		vorbis_info_clear( &qth->vi );
	}

	if( !qth->v_stream || !cin->framerate ) {
		return qfalse;
	}

	cin->headerlen = trap_FS_Tell( cin->file );
	cin->yuv = qtrue;

	return qtrue;
}

/*
* Theora_Shutdown_CIN
*/
void Theora_Shutdown_CIN( cinematics_t *cin )
{
	qtheora_info_t *qth = cin->fdata;

	if( qth->v_stream )
	{
		qth->v_stream = qfalse;
		th_info_clear( &qth->ti );
		th_comment_clear( &qth->tc );
		th_decode_free( qth->tctx );
	}

	if( qth->a_stream )
	{
		qth->a_stream = qfalse;
		vorbis_dsp_clear( &qth->vd );
		vorbis_comment_clear( &qth->vc );
		vorbis_info_clear( &qth->vi );  // must be called last (comment from vorbis example code)
	}

	ogg_stream_clear( &qth->os_audio );
	ogg_stream_clear( &qth->os_video );

	ogg_sync_clear( &qth->oy );	
}

/*
* Theora_Reset_CIN
*/
void Theora_Reset_CIN( cinematics_t *cin )
{
	Theora_Shutdown_CIN( cin );

	CIN_Free( cin->fdata );
	cin->fdata = NULL;

	trap_FS_Seek( cin->file, 0, FS_SEEK_SET );

	Theora_Init_CIN( cin );
}

/*
* Theora_NeedNextFrame_CIN
*/
qboolean Theora_NeedNextFrame_CIN( cinematics_t *cin )
{
	return OggVorbis_NeedAudioData( cin ) || OggTheora_NeedVideoData( cin );
}
