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

#include "r_local.h"
#include "r_imagelib.h"
#include "../qalgo/hash.h"

#if defined ( __MACOSX__ )
#include "libjpeg/jpeglib.h"
#include "png/png.h"
#else
#include "jpeglib.h"
#include "png.h"
#endif

#include <setjmp.h>

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

// basic readpixel/writepixel loop for RLE runs
#define WRITELOOP_COMP_1				\
		for( i = 0; i < size; )		\
		{								\
			header = *pin++;			\
			pixelcount = (header & 0x7f) + 1;	\
			if( header & 0x80 )			\
			{							\
				READPIXEL(pin);			\
				for( j = 0; j < pixelcount; j++ )	\
				{						\
					WRITEPIXEL( pout );	\
				}						\
				i += pixelcount;		\
			}							\
			else						\
			{							\
				for( j = 0; j < pixelcount; j++ )	\
				{						\
					READPIXEL( pin );	\
					WRITEPIXEL( pout );	\
				}						\
				i += pixelcount;		\
			}							\
		}

// readpixel/writepixel loop for RLE runs with memcpy on uncomp run
#define WRITELOOP_COMP_2(chan)			\
		for( i = 0; i < size; )		\
		{								\
			header = *pin++;			\
			pixelcount = (header & 0x7f) + 1;	\
			if( header & 0x80 )			\
			{							\
				READPIXEL(pin);			\
				if( chan == 4 )			\
				{						\
					Q_memset32( pout, blue|(green<<8)|(red<<16)|(alpha<<24), pixelcount );	\
					pout += pixelcount * chan;		\
					i += pixelcount;		\
				}						\
				else					\
				{						\
					for( j = 0; j < pixelcount; j++ )	\
						WRITEPIXEL( pout );				\
					i += pixelcount;					\
				}						\
			}							\
			else						\
			{							\
				memcpy( pout, pin, pixelcount * chan );	\
				pin += pixelcount * chan;		\
				pout += pixelcount * chan;		\
				i += pixelcount;		\
			}							\
		}

#define WRITEPIXEL24(a)			\
		*a++ = blue;				\
		*a++ = green;				\
		*a++ = red;

#define WRITEPIXEL32(a)			\
		WRITEPIXEL24(a);			\
		*a++ = alpha;

#define READPIXEL(a)				\
		blue = *a++;				\
		red = palette[blue][0];		\
		green = palette[blue][1];	\
		blue = palette[blue][2];

#undef WRITEPIXEL
#define WRITEPIXEL	WRITEPIXEL24
static void tga_comp_cm24( qbyte *pout, qbyte *pin, qbyte palette[256][4], int size )
{
	int header, pixelcount;
	int blue, green, red;
	int i, j;

	WRITELOOP_COMP_1;
}

static void tga_cm24( qbyte *pout, qbyte *pin, qbyte palette[256][4], int size )
{
	int blue, green, red;
	int i;

	for( i = 0; i < size; i++ )
	{
		READPIXEL(pin);
		WRITEPIXEL(pout);
	}
}

#undef READPIXEL
#define READPIXEL(a)				\
		blue = *a++;				\
		red = palette[blue][0];		\
		green = palette[blue][1];	\
		blue = palette[blue][2];	\
		alpha = palette[blue][3];

#undef WRITEPIXEL
#define WRITEPIXEL	WRITEPIXEL32
static void tga_comp_cm32( qbyte *pout, qbyte *pin, qbyte palette[256][4], int size )
{
	int header, pixelcount;
	int blue, green, red, alpha;
	int i, j;

	WRITELOOP_COMP_1;
}

static void tga_cm32( qbyte *pout, qbyte *pin, qbyte palette[256][4], int size )
{
	int blue, green, red, alpha;
	int i;

	for( i = 0; i < size; i++ )
	{
		READPIXEL(pin);
		WRITEPIXEL(pout);
	}
}

#undef READPIXEL
#define READPIXEL(a)			\
		blue = *a++;			\
		green = *a++;			\
		red = *a++;

#undef WRITEPIXEL
#define WRITEPIXEL	WRITEPIXEL24
static void tga_comp_rgb24( qbyte *pout, qbyte *pin, int size )
{
	int header, pixelcount;
	int blue, green, red;
	int i, j;

	for( i = 0; i < size; )
	{
		header = *pin++;
		pixelcount = (header & 0x7f) + 1;
		if( header & 0x80 )
		{
			READPIXEL(pin);
			for( j = 0; j < pixelcount; j++ )
			{
				WRITEPIXEL( pout );
			}
			i += pixelcount;
		}
		else
		{
			memcpy( pout, pin, pixelcount * 3 );
			pin += pixelcount * 3;
			pout += pixelcount * 3;
			i += pixelcount;
		}
	}
}

static void tga_rgb24( qbyte *pout, qbyte *pin, int size )
{
	memcpy( pout, pin, size * 3 );
}

#undef READPIXEL
#define READPIXEL(a)			\
		blue = *a++;			\
		green = *a++;			\
		red = *a++;				\
		alpha = *a++;			\
		pix = blue | ( green << 8 ) | ( red << 16 ) | ( alpha << 24 );

#undef WRITEPIXEL
#define WRITEPIXEL(a)			\
		*((int*)a) = pix;	\
		a += 4;

static void tga_comp_rgb32( qbyte *pout, qbyte *pin, int size )
{
	int header, pixelcount;
	int blue, green, red, alpha;
	int i, pix;

	for( i = 0; i < size; )
	{
		header = *pin++;
		pixelcount = (header & 0x7f) + 1;
		if( header & 0x80 )
		{
			READPIXEL(pin);
			Q_memset32( pout, pix, pixelcount );
			pout += pixelcount * 4;
			i += pixelcount;
		}
		else
		{
			memcpy( pout, pin, pixelcount * 4 );
			pin += pixelcount * 4;
			pout += pixelcount * 4;
			i += pixelcount;
		}
	}
}

static void tga_rgb32( qbyte *pout, qbyte *pin, int size )
{
	memcpy( pout, pin, size * 4 );
}


#undef READPIXEL
#define READPIXEL(a)			\
	blue = green = red = *a++;

#undef WRITEPIXEL
#define WRITEPIXEL	WRITEPIXEL24
static void tga_comp_grey( qbyte *pout, qbyte *pin, int size )
{
	int header, pixelcount;
	int blue, green, red;
	int i, j;

	WRITELOOP_COMP_1;
}

static void tga_grey( qbyte *pout, qbyte *pin, int size )
{
	int i, a;

	for( i = 0; i < size; i++ )
	{
		a = *pin++;
		*pout++ = a;
		*pout++ = a;
		*pout++ = a;
	}
}

/*
* LoadTGA
*/
r_imginfo_t LoadTGA( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr )
{
	int i, j, columns, rows, samples;
	qbyte *buf_p, *buffer, *pixbuf, *targa_rgba;
	qbyte palette[256][4];
	TargaHeader targa_header;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	//
	// load the file
	//
	R_LoadFile( name, (void **)&buffer );
	if( !buffer )
		return imginfo;

	buf_p = buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = buf_p[0] + buf_p[1] * 256;
	buf_p += 2;
	targa_header.colormap_length = buf_p[0] + buf_p[1] * 256;
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.y_origin = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.width = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.height = LittleShort( *( (short *)buf_p ) );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;
	if( targa_header.id_length != 0 )
		buf_p += targa_header.id_length; // skip TARGA image comment

	samples = 3;
	if( targa_header.image_type == 1 || targa_header.image_type == 9 )
	{
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 )
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 1 and 9" );
			R_FreeFile( buffer );
			return imginfo;
		}
		if( targa_header.colormap_length != 256 )
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit colormaps are supported for type 1 and 9" );
			R_FreeFile( buffer );
			return imginfo;
		}
		if( targa_header.colormap_index )
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: colormap_index is not supported for type 1 and 9" );
			R_FreeFile( buffer );
			return imginfo;
		}
		if( targa_header.colormap_size == 24 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		}
		else if( targa_header.colormap_size == 32 )
		{
			samples = 4;

			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		}
		else
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9" );
			R_FreeFile( buffer );
			return imginfo;
		}
	}
	else if( targa_header.image_type == 2 || targa_header.image_type == 10 )
	{
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 )
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 32 or 24 bit images supported for type 2 and 10" );
			R_FreeFile( buffer );
			return imginfo;
		}

		samples = targa_header.pixel_size >> 3;
	}
	else if( targa_header.image_type == 3 || targa_header.image_type == 11 )
	{
		// uncompressed grayscale
		if( targa_header.pixel_size != 8 )
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "LoadTGA: Only 8 bit images supported for type 3 and 11" );
			R_FreeFile( buffer );
			return imginfo;
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;
	targa_rgba = allocbuf( uptr, columns * rows * samples, __FILE__, __LINE__ );
	pixbuf = targa_rgba;

	switch( targa_header.image_type )
	{
		case 1:
			// uncompressed colourmapped
			if( targa_header.colormap_size == 24 )
				tga_cm24( pixbuf, buf_p, palette, rows * columns );
			else
				tga_cm32( pixbuf, buf_p, palette, rows * columns );
			break;

		case 9:
			// compressed colourmapped
			if( targa_header.colormap_size == 24 )
				tga_comp_cm24( pixbuf, buf_p, palette, rows * columns );
			else
				tga_comp_cm32( pixbuf, buf_p, palette, rows * columns );
			break;

		case 2:
			// uncompressed RGB
			if( targa_header.pixel_size == 24 )
				tga_rgb24( pixbuf, buf_p, rows * columns );
			else
				tga_rgb32( pixbuf, buf_p, rows * columns );
			break;

		case 10:
			// compressed RGB
			if( targa_header.pixel_size == 24 )
				tga_comp_rgb24( pixbuf, buf_p, rows * columns );
			else
				tga_comp_rgb32( pixbuf, buf_p, rows * columns );
			break;

		case 3:
			// uncompressed grayscale
			tga_grey( pixbuf, buf_p, rows * columns );
			break;

		case 11:
			// compressed grayscale
			tga_comp_grey( pixbuf, buf_p, rows * columns );
			break;
	}

	// this could be optimized out easily in uncompressed versions
	if( ! (targa_header.attributes & 0x20) )
	{
		// Flip the image vertically
		int rowsize = columns * samples;
		qbyte *row1, *row2;
		qbyte *tmpLine = malloc( rowsize );

		for( i = 0, j = rows - 1; i < j; i++, j-- )
		{
			row1 = targa_rgba + i * rowsize;
			row2 = targa_rgba + j * rowsize;
			memcpy( tmpLine, row1, rowsize );
			memcpy( row1, row2, rowsize );
			memcpy( row2, tmpLine, rowsize );
		}

		free( tmpLine );
	}

	R_FreeFile( buffer );

	imginfo.comp = (samples == 4 ? IMGCOMP_BGRA : IMGCOMP_BGR);
	imginfo.width = columns;
	imginfo.height = rows;
	imginfo.pixels = targa_rgba;
	imginfo.samples = samples;
	return imginfo;
}

#undef WRITEPIXEL24
#undef WRITEPIXEL32
#undef WRITEPIXEL
#undef READPIXEL
#undef WRITELOOP_COMP_1
#undef WRITELOOP_COMP_2

/*
* WriteTGA
*/
qboolean WriteTGA( const char *name, r_imginfo_t *info, int quality )
{
	int file, i, c, temp;
	int width, height, samples;
	qbyte header[18], *buffer;
	qboolean bgr;

	if( ri.FS_FOpenFile( name, &file, FS_WRITE ) == -1 )
	{
		Com_Printf( "WriteTGA: Couldn't create %s\n", name );
		return qfalse;
	}

	width = info->width;
	height = info->height;
	samples = info->samples;
	bgr = (info->comp == IMGCOMP_BGR || info->comp == IMGCOMP_BGRA);
	buffer = info->pixels;

	memset( header, 0, sizeof( header ) );
	header[2] = 2;  // uncompressed type
	header[12] = width&255;
	header[13] = width>>8;
	header[14] = height&255;
	header[15] = height>>8;
	header[16] = samples<<3; // pixel size

	ri.FS_Write( header, sizeof( header ), file );

	// swap rgb to bgr
	c = width*height*samples;
	if( !bgr )
	{
		for( i = 0; i < c; i += samples )
		{
			temp = buffer[i];
			buffer[i] = buffer[i+2];
			buffer[i+2] = temp;
		}
	}
	ri.FS_Write( buffer, c, file );
	ri.FS_FCloseFile( file );

	return qtrue;
}

/*
=========================================================

JPEG LOADING

=========================================================
*/

struct q_jpeg_error_mgr {
	struct jpeg_error_mgr pub;		// "public" fields
	jmp_buf setjmp_buffer;			// for return to caller
};

static void q_jpg_error_exit(j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];

	// cinfo->err really points to a my_error_mgr struct, so coerce pointer
	struct q_jpeg_error_mgr *qerr = (struct q_jpeg_error_mgr *) cinfo->err;

    // create the message
	qerr->pub.format_message( cinfo, buffer );
	ri.Com_DPrintf( "q_jpg_error_exit: %s\n", buffer );

	// Return control to the setjmp point
	longjmp(qerr->setjmp_buffer, 1);
}

static void q_jpg_noop( j_decompress_ptr cinfo )
{
}

static boolean q_jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
	ri.Com_DPrintf( "Premature end of jpeg file\n" );
	return 1;
}

static void q_jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
	cinfo->src->next_input_byte += (size_t) num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

void q_jpeg_mem_src( j_decompress_ptr cinfo, unsigned char *mem, unsigned long len )
{
	cinfo->src = (struct jpeg_source_mgr *)
		( *cinfo->mem->alloc_small )( (j_common_ptr) cinfo,
		JPOOL_PERMANENT,
		sizeof( struct jpeg_source_mgr ) );
	cinfo->src->init_source = q_jpg_noop;
	cinfo->src->fill_input_buffer = q_jpg_fill_input_buffer;
	cinfo->src->skip_input_data = q_jpg_skip_input_data;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;
	cinfo->src->term_source = q_jpg_noop;
	cinfo->src->bytes_in_buffer = len;
	cinfo->src->next_input_byte = mem;
}

/*
* LoadJPG
*/
r_imginfo_t LoadJPG( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr )
{
	unsigned int i, length, samples, widthXsamples;
	qbyte *img, *scan, *buffer, *line, *jpg_rgb;
	struct q_jpeg_error_mgr jerr;
	struct jpeg_decompress_struct cinfo;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	// load the file
	length = R_LoadFile( name, (void **)&buffer );
	if( !buffer )
		return imginfo;

	cinfo.err = jpeg_std_error( &jerr.pub );
	jerr.pub.error_exit = q_jpg_error_exit;

	// establish the setjmp return context for q_jpg_error_exit to use.
	if( setjmp( jerr.setjmp_buffer ) ) {
		// if we get here, the JPEG code has signaled an error
		goto error;
	}

	jpeg_create_decompress( &cinfo );
	q_jpeg_mem_src( &cinfo, buffer, length );
	jpeg_read_header( &cinfo, TRUE );
	jpeg_start_decompress( &cinfo );
	samples = cinfo.output_components;

	if( samples != 3 && samples != 1 )
	{
error:
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
		jpeg_destroy_decompress( &cinfo );
		R_FreeFile( buffer );
		return imginfo;
	}

	jpg_rgb = img = allocbuf( uptr, cinfo.output_width * cinfo.output_height * 3, __FILE__, __LINE__ );
	widthXsamples = cinfo.output_width * samples;
	line = malloc( widthXsamples );

	while( cinfo.output_scanline < cinfo.output_height )
	{
		scan = line;
		if( !jpeg_read_scanlines( &cinfo, &scan, 1 ) )
		{
			Com_Printf( S_COLOR_YELLOW "Bad jpeg file %s\n", name );
			jpeg_destroy_decompress( &cinfo );
			R_FreeFile( buffer );
			return imginfo;
		}

		if( samples == 1 )
		{
			for( i = 0; i < cinfo.output_width; i++, img += 3 )
				img[0] = img[1] = img[2] = *scan++;
		}
		else
		{
			memcpy( img, scan, widthXsamples );
			img += widthXsamples;
		}
	}

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	R_FreeFile( buffer );
	free( line );

	imginfo.comp = IMGCOMP_RGB;
	imginfo.width = cinfo.output_width;
	imginfo.height = cinfo.output_height;
	imginfo.pixels = jpg_rgb;
	imginfo.samples = 3;
	return imginfo;
}

#define JPEG_OUTPUT_BUFFER_SIZE		4096

static void q_jpg_init_destination(j_compress_ptr cinfo)
{
}

static boolean q_jpg_write_buf_to_disk(j_compress_ptr cinfo)
{
	int written = JPEG_OUTPUT_BUFFER_SIZE - cinfo->dest->free_in_buffer;
	qbyte *buffer = ( qbyte * )cinfo->dest->next_output_byte - written;
	int file = *((int *)(buffer + JPEG_OUTPUT_BUFFER_SIZE));
	if( ri.FS_Write( buffer, JPEG_OUTPUT_BUFFER_SIZE, file ) == 0 ) {
		return FALSE;
	}
	return TRUE;
}

static boolean q_jpg_empty_output_buffer(j_compress_ptr cinfo)
{
	boolean res = q_jpg_write_buf_to_disk( cinfo );
	cinfo->dest->next_output_byte -= JPEG_OUTPUT_BUFFER_SIZE - cinfo->dest->free_in_buffer;
	cinfo->dest->free_in_buffer = JPEG_OUTPUT_BUFFER_SIZE;
	return res;
}

static void q_jpg_term_destination(j_compress_ptr cinfo)
{
	q_jpg_write_buf_to_disk( cinfo );
}

/*
* WriteJPG
*/
qboolean WriteJPG( const char *name, r_imginfo_t *info, int quality )
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	struct jpeg_destination_mgr jdest;
	char buf[JPEG_OUTPUT_BUFFER_SIZE + sizeof( int )];
	JSAMPROW s[1];
	int offset, w3;
	int file;

	if( ri.FS_FOpenFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "WriteJPG: Couldn't create %s\n", name );
		return qfalse;
	}

	// hack file handle into output buffer
	*((int *)(buf + JPEG_OUTPUT_BUFFER_SIZE)) = file;

	jdest.init_destination = q_jpg_init_destination;
	jdest.empty_output_buffer = q_jpg_empty_output_buffer;
	jdest.term_destination = q_jpg_term_destination;
	jdest.next_output_byte = (JOCTET *)buf;
	jdest.free_in_buffer = JPEG_OUTPUT_BUFFER_SIZE;

	// initialize the JPEG compression object
	jpeg_create_compress( &cinfo );
	cinfo.err = jpeg_std_error( &jerr );
	cinfo.dest = &jdest;

	// setup JPEG parameters
	cinfo.image_width = info->width;
	cinfo.image_height = info->height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = info->samples;

	jpeg_set_defaults( &cinfo );

	if( ( quality > 100 ) || ( quality <= 0 ) )
		quality = 85;

	jpeg_set_quality( &cinfo, quality, TRUE );

	// If quality is set high, disable chroma subsampling 
	if( quality >= 85 )
	{
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}

	// start compression
	jpeg_start_compress( &cinfo, qtrue );

	// feed scanline data
	w3 = cinfo.image_width * info->samples;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height )
	{
		s[0] = &info->pixels[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines( &cinfo, s, 1 );
	}

	// finish compression
	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	ri.FS_FCloseFile( file );

	return qtrue;
}

/*
=========================================================

PNG LOADING

=========================================================
*/

typedef struct {
	qbyte *data;
	size_t size;
	size_t curptr;
} q_png_iobuf_t;

static void q_png_error_fn( png_structp png_ptr, const char *message )
{
    ri.Com_DPrintf( "q_png_error_fn: error: %s\n", message );
}

static void q_png_warning_fn( png_structp png_ptr, const char *message )
{
    ri.Com_DPrintf( "q_png_warning_fn: warning: %s\n", message );
}

//LordHavoc: removed __cdecl prefix, added overrun protection, and rewrote this to be more efficient
static void q_png_user_read_fn( png_structp png_ptr, unsigned char *data, size_t length )
{
	q_png_iobuf_t *io = (q_png_iobuf_t *)png_get_io_ptr( png_ptr );
	size_t rem = io->size - io->curptr;

	if( length > rem ) {
        ri.Com_DPrintf( "q_png_user_read_fn: overrun by %i bytes\n", (int)(length - rem) );

        // a read going past the end of the file, fill in the remaining bytes
        // with 0 just to be consistent
        memset( data + rem, 0, length - rem );
        length = rem;
    }

	memcpy( data, io->data + io->curptr, length );
    io->curptr += length;
}

/*
* LoadPNG
*/
r_imginfo_t LoadPNG( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr )
{
	qbyte *img;
	qbyte *png_data;
	size_t png_datasize;
	q_png_iobuf_t io;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 p_width, p_height;
	int p_bit_depth, p_color_type, p_interlace_type;
	int samples;
	size_t y, row_bytes;
	unsigned char **row_pointers;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	// load the file
	png_datasize = R_LoadFile( name, (void **)&png_data );
	if( !png_data )
		return imginfo;

	if( png_sig_cmp( png_data, 0, png_datasize ) ) {
error:
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad png file %s\n", name );

		if( png_ptr != NULL ) {
			png_destroy_write_struct( &png_ptr, NULL );
		}
		R_FreeFile( png_data );
        return imginfo;
	}
	
	// create and initialize the png_struct with the desired error handler
	// functions. We also supply the  the compiler header file version, so 
	// that we know if the application was compiled with a compatible 
	// version of the library. REQUIRED
	png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, q_png_error_fn, q_png_warning_fn );
	if( png_ptr == NULL ) {
		goto error;
	}

	// allocate/initialize the image information data. REQUIRED
	info_ptr = png_create_info_struct( png_ptr );
	if( info_ptr == NULL ) {
		goto error;
	}

	// set error handling if you are using the setjmp/longjmp method (this is
	// the normal method of doing things with libpng). REQUIRED unless you
	// set up your own error handlers in the png_create_read_struct() earlier.
	if( setjmp( png_jmpbuf( png_ptr ) ) ) {
		goto error;
	}

	io.curptr = 0;
	io.data = png_data;
	io.size = png_datasize;

	// if you are using replacement read functions, instead of calling
	// png_init_io() here you would call:
	png_set_read_fn( png_ptr, (void *)&io, q_png_user_read_fn );
	// where user_io_ptr is a structure you want available to the callbacks

	png_set_sig_bytes( png_ptr, 0 );

	// the call to png_read_info() gives us all of the information from the
	// PNG file before the first IDAT (image data chunk). REQUIRED
	png_read_info( png_ptr, info_ptr );

	png_get_IHDR( png_ptr, info_ptr, &p_width, &p_height, &p_bit_depth, &p_color_type,
		&p_interlace_type, NULL, NULL );

	if( p_color_type & PNG_COLOR_MASK_ALPHA ) {
		samples = 4;
	}
	else {
		samples = 3;
		// add filler (or alpha) byte (before/after each RGB triplet)
		// png_set_filler( png_ptr, 255, 1 );
	}

	// expand paletted colors into true RGB triplets
	if( p_color_type == PNG_COLOR_TYPE_PALETTE ) {
        png_set_palette_to_rgb( png_ptr );
	}

	// expand grayscale images to RGB triplets
	if( p_color_type == PNG_COLOR_TYPE_GRAY || p_color_type == PNG_COLOR_TYPE_GRAY_ALPHA ) {
        png_set_gray_to_rgb( png_ptr );
	}

	// expand paletted or RGB images with transparency to full alpha channels
	// so the data will be available as RGBA quartets.
	if( png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ) ) {
        png_set_tRNS_to_alpha( png_ptr );
	}

	// expand grayscale images to the full 8 bits
	if( p_bit_depth < 8 ) {
        png_set_expand( png_ptr );
	}

	// optional call to gamma correct and add the background to the palette
	// and update info structure. REQUIRED if you are expecting libpng to
	// update the palette for you (ie you selected such a transform above).
    png_read_update_info( png_ptr, info_ptr );

	// allocate the memory to hold the image using the fields of info_ptr

	row_bytes = png_get_rowbytes( png_ptr, info_ptr );
	row_pointers = malloc( p_height * sizeof( *row_pointers ) );

	img = allocbuf( uptr, p_height * row_bytes, __FILE__, __LINE__ );

	for( y = 0; y < p_height; y++ ) {
		row_pointers[y] = img + y * row_bytes;
	}

	// now it's time to read the image
	png_read_image( png_ptr, row_pointers );

	// read rest of file, and get additional chunks in info_ptr - REQUIRED
    png_read_end( png_ptr, info_ptr );

	// clean up after the read, and free any memory allocated - REQUIRED
    png_destroy_read_struct( &png_ptr, &info_ptr, 0 );

	free( row_pointers );

	R_FreeFile( png_data );

	imginfo.comp = (samples == 4 ? IMGCOMP_RGBA : IMGCOMP_RGB);
	imginfo.width = p_width;
	imginfo.height = p_height;
	imginfo.samples = samples;
	imginfo.pixels = img;
	return imginfo;
}
