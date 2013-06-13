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
#include "cin_ogg.h"

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
} qogg_info_t;

/*
* Ogg_LoadBlockToSync
*
* Returns number of bytes transferred
*/
static int Ogg_LoadBlockToSync( cinematics_t *cin )
{
	int bytes;
	char *buffer;
	qogg_info_t *ogg = cin->fdata;

	if( trap_FS_Eof( cin->file ) ) {
		return 0;
	}

	buffer = ogg_sync_buffer( &ogg->oy, OGG_BUFFER_SIZE );
	bytes = trap_FS_Read( buffer, OGG_BUFFER_SIZE, cin->file );
	ogg_sync_wrote( &ogg->oy, bytes );

	return bytes;
}

/*
* Ogg_LoadPagesToStreams
*/
static void Ogg_LoadPagesToStreams( qogg_info_t *ogg, ogg_page *page )
{
	// this can be done blindly; a stream won't accept a page that doesn't belong to it
	if( ogg->a_stream ) 
		ogg_stream_pagein( &ogg->os_audio, page );
	if( ogg->v_stream ) 
		ogg_stream_pagein( &ogg->os_video, page );
}


#define RAW_BUFFER_SIZE		8*1024
#define AUDIO_PRELOAD_MSEC	400

/*
* Ogg_NeedAudioData
*/
static qboolean Ogg_NeedAudioData( cinematics_t *cin )
{
	qogg_info_t *ogg = cin->fdata;

	if( ogg->a_stream && !ogg->a_eos
		&& ( cin->cur_time + AUDIO_PRELOAD_MSEC > cin->start_time + ogg->samples_read * ogg->inv_s_rate ) ) {
		return qtrue;
	}
	return qfalse;
}

/*
* Ogg_LoadAudioFrame
*
* Returns qtrue if no additional audio packets are needed
*/
static qboolean Ogg_LoadAudioFrame( cinematics_t *cin )
{
	int	i, val;
	short* ptr;
	float **pcm;
	float *right,*left;
	int	samples, samplesNeeded;
	qbyte rawBuffer[RAW_BUFFER_SIZE];
	ogg_packet op;
	vorbis_block vb;
	qogg_info_t *ogg = cin->fdata;

	memset( &op, 0, sizeof( op ) );
	memset( &vb, 0, sizeof( vb ) );

	vorbis_block_init( &ogg->vd, &vb );

read_samples:
	while ( ( samples = vorbis_synthesis_pcmout( &ogg->vd, &pcm ) ) > 0 ) {
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
		vorbis_synthesis_read( &ogg->vd, samplesNeeded ); 
		trap_S_RawSamples( i, cin->s_rate, cin->s_width, cin->s_channels, rawBuffer, qfalse );

		ogg->samples_read += samplesNeeded;

		if( !Ogg_NeedAudioData( cin ) ) {
			vorbis_block_clear( &vb );
			return qtrue;
		}
	}

	if( ogg_stream_packetout( &ogg->os_audio, &op ) ) {
		if( op.e_o_s ) {
			// end of stream packet
			ogg->a_eos = qfalse;
			trap_S_Clear();
			return qtrue;
		}

		if( vorbis_synthesis( &vb, &op ) == 0 ) {
			vorbis_synthesis_blockin( &ogg->vd, &vb );
			goto read_samples;
		}
	}

	vorbis_block_clear( &vb );
	return qfalse;
}

/*
* Ogg_VideoGranuleMsec
*/
static unsigned int Ogg_VideoGranuleMsec( cinematics_t *cin )
{
	qogg_info_t *ogg = cin->fdata;

	if( ogg->th_granulepos >= 0 ) {
		return th_granule_time( ogg->tctx, ogg->th_granulepos ) * 1000.0;
	}
	return 0;
}

/*
* Ogg_NeedVideoData
*/
static qboolean Ogg_NeedVideoData( cinematics_t *cin )
{
	unsigned int realframe;
	qogg_info_t *ogg = cin->fdata;

	if( ogg->a_stream && !ogg->a_eos ) {
		// sync to audio
		if( cin->cur_time > cin->start_time + Ogg_VideoGranuleMsec( cin ) ) {
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
#define Ogg_InitYCbCrTable() \
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
* Ogg_DecodeYCbCr2RGB_420
*/
static void Ogg_DecodeYCbCr2RGB_420( th_ycbcr_buffer yuv, int x_offset, int y_offset, unsigned int width, unsigned int height, int bytes, qbyte *out )
{
	int 
		yStride = yuv[0].stride,
		uStride = yuv[1].stride,
		vStride = yuv[2].stride,
		outStride = width * bytes;
	qbyte 
		*yData = yuv[0].data + (x_offset     ) + yStride * (y_offset     ), 
		*uData = yuv[1].data + (x_offset >> 1) + uStride * (y_offset >> 1),
		*vData = yuv[2].data + (x_offset >> 1) + vStride * (y_offset >> 1);
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

	Ogg_InitYCbCrTable();

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
* Ogg_DecodeYCbCr2RGB_422
*/
static void Ogg_DecodeYCbCr2RGB_422( th_ycbcr_buffer yuv, int x_offset, int y_offset, unsigned int width, unsigned int height, int bytes, qbyte *out )
{
	int 
		yStride = yuv[0].stride,
		uStride = yuv[1].stride,
		vStride = yuv[2].stride,
		outStride = width * bytes;
	qbyte 
		*yData = yuv[0].data + (x_offset     ) + yStride * (y_offset), 
		*uData = yuv[1].data + (x_offset >> 1) + uStride * (y_offset),
		*vData = yuv[2].data + (x_offset >> 1) + vStride * (y_offset);
	qbyte
		*yRow  = yData,
		*uRow  = uData,
		*vRow  = vData;
	qbyte
		*oRow  = out;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Ogg_InitYCbCrTable();

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
* Ogg_DecodeYCbCr2RGB_444
*/
static void Ogg_DecodeYCbCr2RGB_444( th_ycbcr_buffer yuv, int x_offset, int y_offset, unsigned int width, unsigned int height, int bytes, qbyte *out )
{
	int 
		yStride = yuv[0].stride,
		uStride = yuv[1].stride,
		vStride = yuv[2].stride,
		outStride = width * bytes;
	qbyte 
		*yData = yuv[0].data + x_offset + yStride * y_offset, 
		*uData = yuv[1].data + x_offset + uStride * y_offset,
		*vData = yuv[2].data + x_offset + vStride * y_offset;
	qbyte
		*yRow  = yData,
		*uRow  = uData,
		*vRow  = vData;
	qbyte
		*oRow  = out;
	unsigned int xPos, yPos;
	int y = 0, u = 0, v = 0, c[3];

	Ogg_InitYCbCrTable();

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
* Ogg_DecodeYCbCr2RGB
*/
static void Ogg_DecodeYCbCr2RGB( th_pixel_fmt pfmt, th_ycbcr_buffer yuv, int x_offset, int y_offset, unsigned int width, unsigned int height, int bytes, qbyte *out )
{
	if( pfmt == (th_pixel_fmt)TH_PF_444 ) {
		Ogg_DecodeYCbCr2RGB_444( yuv, x_offset, y_offset, width, height, bytes, out );
	}
	else if( pfmt == (th_pixel_fmt)TH_PF_422 ) {
		Ogg_DecodeYCbCr2RGB_422( yuv, x_offset, y_offset, width, height, bytes, out );
	}
	else if( pfmt == (th_pixel_fmt)TH_PF_420 ) {
		Ogg_DecodeYCbCr2RGB_420( yuv, x_offset, y_offset, width, height, bytes, out );
	}
}

/*
* Ogg_LoadVideoFrame
*
* Return qtrue if a new video frame has been successfully loaded
*/
static qboolean Ogg_LoadVideoFrame( cinematics_t *cin )
{
	ogg_packet op;
	qogg_info_t *ogg = cin->fdata;
	th_ycbcr_buffer yuv;

	memset( &op, 0, sizeof( op ) );

	while( ogg_stream_packetout( &ogg->os_video, &op ) )
	{
		int width, height;

		if( op.e_o_s ) {
			// we've encountered end of stream packet
			ogg->v_eos = qtrue;
			break;
		}

        if( op.granulepos >= 0 ){
			th_decode_ctl( ogg->tctx, TH_DECCTL_SET_GRANPOS, &op.granulepos, sizeof( op.granulepos ) );
        }

		if( th_decode_packetin( ogg->tctx, &op, &ogg->th_granulepos ) != 0 ) {
			// bad packet
			continue;
		}
		if( th_packet_isheader( &op ) ) {
			// header packet, skip
			continue;
		}

		cin->frame = th_granule_frame( ogg->tctx, ogg->th_granulepos );

		if( cin->cur_time > cin->start_time + Ogg_VideoGranuleMsec( cin ) ) {
			Com_DPrintf( "Dropped frame: %i\n", cin->frame );
			continue;
		}

		if( th_decode_ycbcr_out( ogg->tctx, yuv ) != 0 ) {
			// error
			continue;
		}

		// effective width and height
		width  = ogg->ti.pic_width & ~1;
		height = ogg->ti.pic_height & ~1;

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

		// convert YCbCr to RGB
		Ogg_DecodeYCbCr2RGB( ogg->ti.pixel_fmt, yuv, ogg->ti.pic_x & ~1, ogg->ti.pic_y & ~1, width, height, 4, cin->vid_buffer );

		return qtrue;
	}

	return qfalse;
}

/*
* Ogg_ReadNextFrame_CIN
*/
qbyte *Ogg_ReadNextFrame_CIN( cinematics_t *cin, qboolean *redraw )
{
	unsigned int bytes, pages = 0;
	qboolean redraw_ = qfalse;
	qogg_info_t *ogg = cin->fdata;
	qboolean haveAudio = qfalse, haveVideo = qfalse;

	while( 1 )
	{
		ogg_page og;
		qboolean needAudio, needVideo;

		needAudio = !haveAudio && Ogg_NeedAudioData( cin );
		needVideo = !haveVideo && Ogg_NeedVideoData( cin );
		redraw_ = redraw_ || needVideo;

		if( !needAudio && !needVideo ) {
			break;
		}

		if( needAudio ) {
			haveAudio = Ogg_LoadAudioFrame( cin );
			needAudio = !haveAudio;
		}
		if( needVideo ) {
			haveVideo = Ogg_LoadVideoFrame( cin );
			needVideo = !haveVideo;
		}

		if( ogg->v_eos ) {
			// end of video stream
			return NULL;
		}

		if( !needAudio && !needVideo ) {
			break;
		}

		bytes = Ogg_LoadBlockToSync( cin ); // returns 0 if EOF

		// process all read pages
		pages = 0;
		while( ogg_sync_pageout( &ogg->oy, &og ) > 0 ) {
			pages++;
			Ogg_LoadPagesToStreams( ogg, &og );
		}

		if( !bytes && !pages ) {
			// end of FILE, no pages remaining
			return NULL;
		}
	}

	*redraw = redraw_;
	return cin->vid_buffer;
}

/*
* Ogg_Init_CIN
*/
qboolean Ogg_Init_CIN( cinematics_t *cin )
{
	int status;
	ogg_page	og;
	ogg_packet	op;
	int vorbis_p, theora_p;
	qogg_info_t *ogg;

	ogg = CIN_Alloc( cin->mempool, sizeof( *ogg ) );
	cin->fdata = ( void * )ogg;
	memset( ogg, 0, sizeof( *ogg ) );

	// start up Ogg stream synchronization layer
	ogg_sync_init( &ogg->oy );

	// init supporting Vorbis structures needed in header parsing
	vorbis_info_init( &ogg->vi );
	vorbis_comment_init( &ogg->vc );

	// init supporting Theora structures needed in header parsing
	th_comment_init( &ogg->tc );
	th_info_init( &ogg->ti );

	// Ogg file open; parse the headers
	// Only interested in Vorbis/Theora streams
	vorbis_p = theora_p = 0;
	ogg->v_stream = ogg->a_stream = qfalse;
	ogg->a_eos = ogg->v_eos = qfalse;

	status = 0;
	while( !status )
	{
		if( !Ogg_LoadBlockToSync( cin ) )
			break;

		while( ogg_sync_pageout( &ogg->oy, &og ) > 0 )
		{
			ogg_stream_state test;

			// is this a mandated initial header? If not, stop parsing 
			if( !ogg_page_bos( &og ) ) {
				// don't leak the page; get it into the appropriate stream 
				Ogg_LoadPagesToStreams( ogg, &og );
				status = 1;
				break;
			}

			ogg_stream_init( &test, ogg_page_serialno(&og ) );
			ogg_stream_pagein( &test, &og );
			ogg_stream_packetout( &test, &op );

			// identify the codec: try theora
			if( !ogg->v_stream && th_decode_headerin( &ogg->ti, &ogg->tc, &ogg->tsi, &op ) >= 0 )
			{
				// it is theora
				ogg->v_stream = qtrue;
				memcpy( &ogg->os_video, &test, sizeof( test ) );
				theora_p = 1;
			}
			else if( !ogg->a_stream && !(cin->flags & CIN_NOAUDIO) && !vorbis_synthesis_headerin( &ogg->vi, &ogg->vc, &op ) )
			{
				// it is vorbis
				ogg->a_stream = qtrue;
				memcpy( &ogg->os_audio, &test, sizeof( test ) );
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
		while( theora_p && (theora_p < 3) && (status = ogg_stream_packetout( &ogg->os_video, &op ) ) )
		{
			if( status < 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Theora stream headers; corrupt stream?\n" );
				return qfalse;
			}
			if( th_decode_headerin( &ogg->ti, &ogg->tc, &ogg->tsi, &op ) == 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Theora stream headers; corrupt stream?\n" );
				return qfalse;
			}
			theora_p++;
		}

		// look for more vorbis header packets
		while( vorbis_p && (vorbis_p < 3) && (status = ogg_stream_packetout( &ogg->os_audio, &op ) ) )
		{
			if( status < 0 )
			{
				Com_Printf( S_COLOR_YELLOW, "File %s: error parsing Vorbis stream headers; corrupt stream?\n" );
				return qfalse;
			}
			if( vorbis_synthesis_headerin( &ogg->vi, &ogg->vc, &op ) != 0 )
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
		if( ogg_sync_pageout( &ogg->oy, &og ) > 0 )
		{
			// demux into the appropriate stream
			Ogg_LoadPagesToStreams( ogg, &og );
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
		ogg->tctx = th_decode_alloc( &ogg->ti, ogg->tsi );
		ogg->th_granulepos = -1;

		cin->framerate = (float)ogg->ti.fps_numerator / ogg->ti.fps_denominator;

		// the aspect ratio of the pixels
		// if either value is zero, the aspect ratio is undefined
		cin->aspect_numerator = ogg->ti.aspect_numerator;
		cin->aspect_denominator = ogg->ti.aspect_denominator;
		if( !cin->aspect_numerator || !cin->aspect_denominator ) {
			cin->aspect_numerator = cin->aspect_denominator = 1;
		}
	}
	else
	{
		// tear down the partial theora setup
		ogg->v_stream = qfalse;
		th_comment_clear( &ogg->tc );
		th_info_clear( &ogg->ti );
	}

	th_setup_free( ogg->tsi );

	if( vorbis_p )
	{
		vorbis_synthesis_init( &ogg->vd, &ogg->vi );

		cin->s_rate = ogg->vi.rate;
		cin->s_width = 2;
		cin->s_channels = ogg->vi.channels;
		ogg->inv_s_rate = 1000.0 / (double)cin->s_rate;
		ogg->samples_read = 0;
	}
	else
	{
		// tear down the partial vorbis setup
		ogg->a_stream = qfalse;
		ogg->inv_s_rate = 0;
		ogg->samples_read = 0;

		vorbis_comment_clear( &ogg->vc );
		vorbis_info_clear( &ogg->vi );
	}

	if( !ogg->v_stream || !cin->framerate ) {
		return qfalse;
	}

	cin->headerlen = trap_FS_Tell( cin->file );

	return qtrue;
}

/*
* Ogg_Shutdown_CIN
*/
void Ogg_Shutdown_CIN( cinematics_t *cin )
{
	qogg_info_t *ogg = cin->fdata;

	if( ogg->v_stream )
	{
		ogg->v_stream = qfalse;
		th_info_clear( &ogg->ti );
		th_comment_clear( &ogg->tc );
		th_decode_free( ogg->tctx );
	}

	if( ogg->a_stream )
	{
		ogg->a_stream = qfalse;
		vorbis_dsp_clear( &ogg->vd );
		vorbis_comment_clear( &ogg->vc );
		vorbis_info_clear( &ogg->vi );  // must be called last (comment from vorbis example code)
	}

	ogg_stream_clear( &ogg->os_audio );
	ogg_stream_clear( &ogg->os_video );

	ogg_sync_clear( &ogg->oy );	
}

/*
* Ogg_Reset_CIN
*/
void Ogg_Reset_CIN( cinematics_t *cin )
{
	Ogg_Shutdown_CIN( cin );

	CIN_Free( cin->fdata );
	cin->fdata = NULL;

	trap_FS_Seek( cin->file, 0, FS_SEEK_SET );

	Ogg_Init_CIN( cin );
}

/*
* Ogg_NeedNextFrame_CIN
*/
qboolean Ogg_NeedNextFrame_CIN( cinematics_t *cin )
{
	return Ogg_NeedAudioData( cin ) || Ogg_NeedVideoData( cin );
}
