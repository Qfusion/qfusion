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

#define STB_IMAGE_IMPLEMENTATION
#define STB_NO_STDIO

#include "stb_image.h"

static const r_imginfo_t empty_imginfo = { 0 };

/*
=========================================================

LIBS LOADING

=========================================================
*/

#define qjpeg_create_compress( cinfo ) \
	qjpeg_CreateCompress( ( cinfo ), JPEG_LIB_VERSION, (size_t) sizeof( struct jpeg_compress_struct ) )
#define qjpeg_create_decompress( cinfo ) \
	qjpeg_CreateDecompress( ( cinfo ), JPEG_LIB_VERSION, (size_t) sizeof( struct jpeg_decompress_struct ) )

void *jpegLibrary = NULL;

#ifdef LIBJPEG_RUNTIME

static boolean ( *qjpeg_resync_to_restart )( j_decompress_ptr, int );
static void (*qjpeg_CreateCompress)( j_compress_ptr, int, size_t );
static void (*qjpeg_CreateDecompress)( j_decompress_ptr, int, size_t );
static int (*qjpeg_read_header)( j_decompress_ptr, boolean );
static boolean (*qjpeg_start_decompress)( j_decompress_ptr cinfo );
static JDIMENSION (*qjpeg_read_scanlines)( j_decompress_ptr, JSAMPARRAY, JDIMENSION );
static struct jpeg_error_mgr *(*qjpeg_std_error)( struct jpeg_error_mgr * err );
static boolean (*qjpeg_finish_decompress)( j_decompress_ptr cinfo );
static void (*qjpeg_destroy_decompress)( j_decompress_ptr cinfo );
static void (*qjpeg_start_compress)( j_compress_ptr, boolean );
static void (*qjpeg_set_defaults)( j_compress_ptr cinfo );
static void (*qjpeg_set_quality)( j_compress_ptr, int, boolean );
static JDIMENSION (*qjpeg_write_scanlines)( j_compress_ptr, JSAMPARRAY scanlines, JDIMENSION );
static void (*qjpeg_finish_compress)( j_compress_ptr cinfo );
static void (*qjpeg_destroy_compress)( j_compress_ptr cinfo );

static dllfunc_t libjpegfuncs[] =
{
	{ "jpeg_resync_to_restart", ( void ** )&qjpeg_resync_to_restart },
	{ "jpeg_CreateCompress", ( void ** )&qjpeg_CreateCompress },
	{ "jpeg_CreateDecompress", ( void ** )&qjpeg_CreateDecompress },
	{ "jpeg_read_header", ( void ** )&qjpeg_read_header },
	{ "jpeg_start_decompress", ( void ** )&qjpeg_start_decompress },
	{ "jpeg_read_scanlines", ( void ** )&qjpeg_read_scanlines },
	{ "jpeg_std_error", ( void ** )&qjpeg_std_error },
	{ "jpeg_finish_decompress", ( void ** )&qjpeg_finish_decompress },
	{ "jpeg_destroy_decompress", ( void ** )&qjpeg_destroy_decompress },
	{ "jpeg_start_compress", ( void ** )&qjpeg_start_compress },
	{ "jpeg_set_defaults", ( void ** )&qjpeg_set_defaults },
	{ "jpeg_set_quality", ( void ** )&qjpeg_set_quality },
	{ "jpeg_write_scanlines", ( void ** )&qjpeg_write_scanlines },
	{ "jpeg_finish_compress", ( void ** )&qjpeg_finish_compress },
	{ "jpeg_destroy_compress", ( void ** )&qjpeg_destroy_compress },
	{ NULL, NULL }
};

#else

#define qjpeg_resync_to_restart jpeg_resync_to_restart
#define qjpeg_CreateCompress jpeg_CreateCompress
#define qjpeg_CreateDecompress jpeg_CreateDecompress
#define qjpeg_read_header jpeg_read_header
#define qjpeg_start_decompress jpeg_start_decompress
#define qjpeg_read_scanlines jpeg_read_scanlines
#define qjpeg_std_error jpeg_std_error
#define qjpeg_finish_decompress jpeg_finish_decompress
#define qjpeg_destroy_decompress jpeg_destroy_decompress
#define qjpeg_start_compress jpeg_start_compress
#define qjpeg_set_defaults jpeg_set_defaults
#define qjpeg_set_quality jpeg_set_quality
#define qjpeg_write_scanlines jpeg_write_scanlines
#define qjpeg_finish_compress jpeg_finish_compress
#define qjpeg_destroy_compress jpeg_destroy_compress

#endif

/*
* R_Imagelib_UnloadLibjpeg
*/
static void R_Imagelib_UnloadLibjpeg( void ) {
#ifdef LIBJPEG_RUNTIME
	if( jpegLibrary ) {
		ri.Com_UnloadLibrary( &jpegLibrary );
	}
#endif
	jpegLibrary = NULL;
}

/*
* R_Imagelib_LoadLibjpeg
*/
static void R_Imagelib_LoadLibjpeg( void ) {
	R_Imagelib_UnloadLibjpeg();

#ifdef LIBJPEG_RUNTIME
	jpegLibrary = ri.Com_LoadSysLibrary( LIBJPEG_LIBNAME, libjpegfuncs );
#else
	jpegLibrary = (void *)1;
#endif
}

// ======================================================

void *pngLibrary = NULL;

#ifdef LIBPNG_RUNTIME

#ifndef PNGAPI
#define PNGAPI
#endif

static int( PNGAPI * qpng_sig_cmp )( png_bytep, png_size_t, png_size_t );
static png_uint_32( PNGAPI * qpng_access_version_number )( void );
static png_structp( PNGAPI * qpng_create_read_struct )( png_const_charp, png_voidp, png_error_ptr, png_error_ptr );
static png_infop( PNGAPI * qpng_create_info_struct )( png_structp png_ptr );
static void( PNGAPI * qpng_set_read_fn )( png_structp, png_voidp, png_rw_ptr );
static void( PNGAPI * qpng_set_sig_bytes )( png_structp, int );
static void( PNGAPI * qpng_read_info )( png_structp, png_infop );
static png_uint_32( PNGAPI * qpng_get_IHDR )( png_structp, png_infop, png_uint_32 *, png_uint_32 *, int *, int *, int *, int *, int * );
static png_uint_32( PNGAPI * qpng_get_valid )( png_structp, png_infop, png_uint_32 );
static void( PNGAPI * qpng_set_palette_to_rgb )( png_structp );
static void( PNGAPI * qpng_set_gray_to_rgb )( png_structp );
static void( PNGAPI * qpng_set_tRNS_to_alpha )( png_structp );
static void( PNGAPI * qpng_set_expand )( png_structp );
static void( PNGAPI * qpng_read_update_info )( png_structp, png_infop );
static png_uint_32( PNGAPI * qpng_get_rowbytes )( png_structp, png_infop );
static void( PNGAPI * qpng_read_image )( png_structp, png_bytepp );
static void( PNGAPI * qpng_read_end )( png_structp, png_infop );
static void( PNGAPI * qpng_destroy_read_struct )( png_structpp, png_infopp, png_infopp );
static png_voidp( PNGAPI * qpng_get_io_ptr )( png_structp );

// Error handling in libpng pre-1.4 and 1.4+.
// In older versions, the jmp_buf is in the only public field of the struct, which is the first field.
// In 1.4 and newer, it is configured using png_set_longjmp_fn.
typedef void (*qpng_longjmp_ptr)( jmp_buf, int );
static jmp_buf *( PNGAPI * qpng_set_longjmp_fn )( png_structp, qpng_longjmp_ptr, size_t );
#define qpng_jmpbuf( png_ptr ) ( qpng_set_longjmp_fn ? \
								 *qpng_set_longjmp_fn( ( png_ptr ), longjmp, sizeof( jmp_buf ) ) : \
								 *( ( jmp_buf * )png_ptr ) )

static dllfunc_t libpngfuncs[] =
{
	{ "png_sig_cmp", ( void ** )&qpng_sig_cmp },
	{ "png_access_version_number", ( void ** )&qpng_access_version_number },
	{ "png_create_read_struct", ( void ** )&qpng_create_read_struct },
	{ "png_create_info_struct", ( void ** )&qpng_create_info_struct },
	{ "png_set_read_fn", ( void ** )&qpng_set_read_fn },
	{ "png_set_sig_bytes", ( void ** )&qpng_set_sig_bytes },
	{ "png_read_info", ( void ** )&qpng_read_info },
	{ "png_get_IHDR", ( void ** )&qpng_get_IHDR },
	{ "png_get_valid", ( void ** )&qpng_get_valid },
	{ "png_set_palette_to_rgb", ( void ** )&qpng_set_palette_to_rgb },
	{ "png_set_gray_to_rgb", ( void ** )&qpng_set_gray_to_rgb },
	{ "png_set_tRNS_to_alpha", ( void ** )&qpng_set_tRNS_to_alpha },
	{ "png_set_expand", ( void ** )&qpng_set_expand },
	{ "png_read_update_info", ( void ** )&qpng_read_update_info },
	{ "png_get_rowbytes", ( void ** )&qpng_get_rowbytes },
	{ "png_read_image", ( void ** )&qpng_read_image },
	{ "png_read_end", ( void ** )&qpng_read_end },
	{ "png_destroy_read_struct", ( void ** )&qpng_destroy_read_struct },
	{ "png_get_io_ptr", ( void ** )&qpng_get_io_ptr },
	{ NULL, NULL }
};

#else

#define qpng_sig_cmp png_sig_cmp
#define qpng_access_version_number png_access_version_number
#define qpng_create_read_struct png_create_read_struct
#define qpng_create_info_struct png_create_info_struct
#define qpng_jmpbuf png_jmpbuf
#define qpng_set_read_fn png_set_read_fn
#define qpng_set_sig_bytes png_set_sig_bytes
#define qpng_read_info png_read_info
#define qpng_get_IHDR png_get_IHDR
#define qpng_get_valid png_get_valid
#define qpng_set_palette_to_rgb png_set_palette_to_rgb
#define qpng_set_gray_to_rgb png_set_gray_to_rgb
#define qpng_set_tRNS_to_alpha png_set_tRNS_to_alpha
#define qpng_set_expand png_set_expand
#define qpng_read_update_info png_read_update_info
#define qpng_get_rowbytes png_get_rowbytes
#define qpng_read_image png_read_image
#define qpng_read_end png_read_end
#define qpng_destroy_read_struct png_destroy_read_struct
#define qpng_get_io_ptr png_get_io_ptr

#endif

/*
* R_Imagelib_UnloadLibpng
*/
static void R_Imagelib_UnloadLibpng( void ) {
#ifdef LIBPNG_RUNTIME
	if( pngLibrary ) {
		ri.Com_UnloadLibrary( &pngLibrary );
	}
#endif
	pngLibrary = NULL;
}

/*
* R_Imagelib_LoadLibpng
*/
static void R_Imagelib_LoadLibpng( void ) {
	R_Imagelib_UnloadLibpng();

#ifdef LIBPNG_RUNTIME
	pngLibrary = ri.Com_LoadSysLibrary( LIBPNG_LIBNAME, libpngfuncs );
	if( pngLibrary ) {
		*(void **)&qpng_set_longjmp_fn = ri.Com_LibraryProcAddress( pngLibrary, "png_set_longjmp_fn" );
	}
#else
	pngLibrary =  (void *)1;
#endif
}

// ======================================================

/*
* R_Imagelib_Init
*/
void R_Imagelib_Init( void ) {
	R_Imagelib_LoadLibjpeg();
	R_Imagelib_LoadLibpng();
}

/*
* R_Imagelib_Shutdown
*/
void R_Imagelib_Shutdown( void ) {
	R_Imagelib_UnloadLibjpeg();
	R_Imagelib_UnloadLibpng();
}

/*
=========================================================

STB IMAGE LOADING

=========================================================
*/

/*
* LoadSTB
*/
static r_imginfo_t LoadSTB( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	uint8_t *img;
	uint8_t *png_data;
	size_t png_datasize;
	size_t imgsize;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	if( !pngLibrary ) {
		return empty_imginfo;
	}

	// load the file
	png_datasize = R_LoadFile( name, (void **)&png_data );
	if( !png_data ) {
		return empty_imginfo;
	}

	img = stbi_load_from_memory( png_data, png_datasize, &imginfo.width, &imginfo.height, &imginfo.samples, 0 );
	R_FreeFile( png_data );

	if( !img ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad png file %s: %s\n", name, stbi_failure_reason() );
		return empty_imginfo;
	}

	if( imginfo.samples != 1 && imginfo.samples != 3 && imginfo.samples != 4 ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad png file %s samples: %d\n", name, imginfo.samples );
		return empty_imginfo;
	}

	imgsize = (size_t)imginfo.width * imginfo.height * imginfo.samples;
	imginfo.pixels = allocbuf( uptr, imgsize, __FILE__, __LINE__ );
	memcpy( imginfo.pixels, img, imgsize );
	imginfo.comp = ( imginfo.samples & 1 ? IMGCOMP_RGB : IMGCOMP_RGBA );
	free( img );

	return imginfo;
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

/*
* LoadTGA
*/
r_imginfo_t LoadTGA( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return LoadSTB( name, allocbuf, uptr );
}

/*
* WriteTGA
*/
bool WriteTGA( const char *name, r_imginfo_t *info, int quality ) {
	int file, i, c, temp;
	int width, height, samples;
	uint8_t header[18], *buffer;
	bool bgr;

	if( ri.FS_FOpenAbsoluteFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "WriteTGA: Couldn't create %s\n", name );
		return false;
	}

	width = info->width;
	height = info->height;
	samples = info->samples;
	bgr = ( info->comp == IMGCOMP_BGR || info->comp == IMGCOMP_BGRA );
	buffer = info->pixels;

	memset( header, 0, sizeof( header ) );
	header[2] = 2;  // uncompressed type
	header[12] = width & 255;
	header[13] = width >> 8;
	header[14] = height & 255;
	header[15] = height >> 8;
	header[16] = samples << 3; // pixel size

	ri.FS_Write( header, sizeof( header ), file );

	// swap rgb to bgr
	c = width * height * samples;
	if( !bgr ) {
		for( i = 0; i < c; i += samples ) {
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}
	}
	ri.FS_Write( buffer, c, file );
	ri.FS_FCloseFile( file );

	return true;
}

/*
=========================================================

JPEG LOADING

=========================================================
*/

/*
* LoadJPG
*/
r_imginfo_t LoadJPG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return LoadSTB( name, allocbuf, uptr );
}

#define JPEG_OUTPUT_BUFFER_SIZE     4096

struct q_jpeg_destination_mgr {
	struct jpeg_destination_mgr pub;

	int outfile;
	JOCTET *buffer;
};

static void q_jpg_init_destination( j_compress_ptr cinfo ) {
	struct q_jpeg_destination_mgr *dest = ( struct q_jpeg_destination_mgr * )( cinfo->dest );

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUFFER_SIZE;
}

static boolean q_jpg_empty_output_buffer( j_compress_ptr cinfo ) {
	struct q_jpeg_destination_mgr *dest = ( struct q_jpeg_destination_mgr * )( cinfo->dest );

	if( ri.FS_Write( dest->buffer, JPEG_OUTPUT_BUFFER_SIZE, dest->outfile ) == 0 ) {
		return FALSE;
	}

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUFFER_SIZE;

	return TRUE;
}

static void q_jpg_term_destination( j_compress_ptr cinfo ) {
	struct q_jpeg_destination_mgr *dest = ( struct q_jpeg_destination_mgr * )( cinfo->dest );
	size_t datacount = JPEG_OUTPUT_BUFFER_SIZE - dest->pub.free_in_buffer;

	if( datacount > 0 ) {
		ri.FS_Write( dest->buffer, datacount, dest->outfile );
	}
}

/*
* WriteJPG
*/
bool WriteJPG( const char *name, r_imginfo_t *info, int quality ) {
#if 0
	struct jpeg_compress_struct cinfo;
	struct q_jpeg_error_mgr jerr;
	struct q_jpeg_destination_mgr jdest;
	JOCTET buffer[JPEG_OUTPUT_BUFFER_SIZE];
	JSAMPROW s[1];
	int offset, w3;
	int file;

	if( !jpegLibrary ) {
		Com_Printf( S_COLOR_YELLOW "WriteJPG: libjpeg is not loaded.\n" );
		return false;
	}

	if( ri.FS_FOpenAbsoluteFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( S_COLOR_YELLOW "WriteJPG: Couldn't create %s\n", name );
		return false;
	}

	jdest.pub.init_destination = q_jpg_init_destination;
	jdest.pub.empty_output_buffer = q_jpg_empty_output_buffer;
	jdest.pub.term_destination = q_jpg_term_destination;
	jdest.outfile = file;
	jdest.buffer = buffer;

	// initialize the JPEG compression object
	cinfo.err = qjpeg_std_error( &jerr.pub );
	jerr.pub.error_exit = q_jpg_error_exit;

	// establish the setjmp return context for q_jpg_error_exit to use.
	if( setjmp( jerr.setjmp_buffer ) ) {
		// if we get here, the JPEG code has signaled an error
		goto error;
	}

	qjpeg_create_compress( &cinfo );
	cinfo.dest = &( jdest.pub );

	// setup JPEG parameters
	cinfo.image_width = info->width;
	cinfo.image_height = info->height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = info->samples;

	qjpeg_set_defaults( &cinfo );

	if( ( quality > 100 ) || ( quality <= 0 ) ) {
		quality = 85;
	}

	qjpeg_set_quality( &cinfo, quality, TRUE );

	// If quality is set high, disable chroma subsampling
	if( quality >= 85 ) {
		cinfo.comp_info[0].h_samp_factor = 1;
		cinfo.comp_info[0].v_samp_factor = 1;
	}

	// start compression
	qjpeg_start_compress( &cinfo, true );

	// feed scanline data
	w3 = cinfo.image_width * info->samples;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height ) {
		s[0] = &info->pixels[offset - cinfo.next_scanline * w3];
		qjpeg_write_scanlines( &cinfo, s, 1 );
	}

	// finish compression
	qjpeg_finish_compress( &cinfo );
	qjpeg_destroy_compress( &cinfo );

	ri.FS_FCloseFile( file );

	return true;

error:
	qjpeg_destroy_compress( &cinfo );
	ri.FS_FCloseFile( file );
#endif
	return false;
}

/*
=================================================================

PCX LOADING

=================================================================
*/

typedef struct {
	char manufacturer;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char palette[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
	unsigned char data;         // unbounded
} pcx_t;

/*
* LoadPCX
*/
r_imginfo_t LoadPCX( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	uint8_t *raw;
	pcx_t *pcx;
	int x, y;
	int len, columns, rows;
	int dataByte, runLength;
	uint8_t *pal, *pix, *c;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	//
	// load the file
	//
	len = R_LoadFile( name, (void **)&raw );
	if( !raw ) {
		return imginfo;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

	pcx->xmin = LittleShort( pcx->xmin );
	pcx->ymin = LittleShort( pcx->ymin );
	pcx->xmax = LittleShort( pcx->xmax );
	pcx->ymax = LittleShort( pcx->ymax );
	pcx->hres = LittleShort( pcx->hres );
	pcx->vres = LittleShort( pcx->vres );
	pcx->bytes_per_line = LittleShort( pcx->bytes_per_line );
	pcx->palette_type = LittleShort( pcx->palette_type );

	raw = &pcx->data;

	if( pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| len < 768 ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad pcx file %s\n", name );
		R_FreeFile( pcx );
		return imginfo;
	}

	columns = pcx->xmax + 1;
	rows = pcx->ymax + 1;
	pix = allocbuf( uptr, columns * rows * 3 + 768, __FILE__, __LINE__ );
	pal = pix + columns * rows * 3;
	memcpy( pal, (uint8_t *)pcx + len - 768, 768 );

	c = pix;
	for( y = 0; y < rows; y++ ) {
		for( x = 0; x < columns; ) {
			dataByte = *raw++;

			if( ( dataByte & 0xC0 ) == 0xC0 ) {
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			} else {
				runLength = 1;
			}

			while( runLength-- > 0 ) {
				c[0] = pal[dataByte * 3 + 0];
				c[1] = pal[dataByte * 3 + 1];
				c[2] = pal[dataByte * 3 + 2];
				x++;
				c += 3;
			}
		}
	}

	R_FreeFile( pcx );

	if( raw - (uint8_t *)pcx > len ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "PCX file %s was malformed", name );
		return imginfo;
	}

	imginfo.comp = IMGCOMP_RGB;
	imginfo.width = rows;
	imginfo.height = columns;
	imginfo.samples = 3;
	imginfo.pixels = pix;
	return imginfo;
}

/*
=========================================================

WAL LOADING

=========================================================
*/

/*
* LoadWAL
*/
r_imginfo_t LoadWAL( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	unsigned int i;
	unsigned int p, s, *trans;
	unsigned int rows, columns;
	int samples;
	uint8_t *buffer, *data, *imgbuf;
	q2miptex_t *mt;
	const unsigned *table8to24 = R_LoadPalette( IT_WAL );
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	// load the file
	R_LoadFile( name, (void **)&buffer );
	if( !buffer ) {
		return imginfo;
	}

	mt = ( q2miptex_t * )buffer;
	rows = LittleLong( mt->width );
	columns = LittleLong( mt->height );
	data = buffer + LittleLong( mt->offsets[0] );
	s = LittleLong( mt->width ) * LittleLong( mt->height );

	// determine the number of channels
	for( i = 0; i < s && data[i] != 255; i++ ) ;
	samples = ( i < s ) ? 4 : 3;

	imgbuf = allocbuf( uptr, s * samples, __FILE__, __LINE__ );
	trans = ( unsigned int * )imgbuf;

	if( samples == 4 ) {
		for( i = 0; i < s; i++ ) {
			p = data[i];
			trans[i] = table8to24[p];

			if( p == 255 ) {
				// transparent, so scan around for another color
				// to avoid alpha fringes
				// FIXME: do a full flood fill so mips work...
				if( i > rows && data[i - rows] != 255 ) {
					p = data[i - rows];
				} else if( i < s - rows && data[i + rows] != 255 ) {
					p = data[i + rows];
				} else if( i > 0 && data[i - 1] != 255 ) {
					p = data[i - 1];
				} else if( i < s - 1 && data[i + 1] != 255 ) {
					p = data[i + 1];
				} else {
					p = 0;
				}

				// copy rgb components
				( (uint8_t *)&trans[i] )[0] = ( (uint8_t *)&table8to24[p] )[0];
				( (uint8_t *)&trans[i] )[1] = ( (uint8_t *)&table8to24[p] )[1];
				( (uint8_t *)&trans[i] )[2] = ( (uint8_t *)&table8to24[p] )[2];
			}
		}
	} else {
		// copy rgb components
		for( i = 0; i < s; i++ ) {
			p = data[i];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[0];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[1];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[2];
		}
	}

	R_FreeFile( mt );

	imginfo.comp = ( samples == 3 ? IMGCOMP_RGB : IMGCOMP_RGBA );
	imginfo.width = rows;
	imginfo.height = columns;
	imginfo.samples = samples;
	imginfo.pixels = (void *)trans;
	return imginfo;
}

/*
=========================================================

PNG LOADING

=========================================================
*/

typedef struct {
	uint8_t *data;
	size_t size;
	size_t curptr;
} q_png_iobuf_t;

static void q_png_error_fn( png_structp png_ptr, const char *message ) {
	ri.Com_DPrintf( "q_png_error_fn: error: %s\n", message );
}

static void q_png_warning_fn( png_structp png_ptr, const char *message ) {
	ri.Com_DPrintf( "q_png_warning_fn: warning: %s\n", message );
}

//LordHavoc: removed __cdecl prefix, added overrun protection, and rewrote this to be more efficient
static void q_png_user_read_fn( png_structp png_ptr, unsigned char *data, size_t length ) {
	q_png_iobuf_t *io = (q_png_iobuf_t *)qpng_get_io_ptr( png_ptr );
	size_t rem = io->size - io->curptr;

	if( length > rem ) {
		ri.Com_DPrintf( "q_png_user_read_fn: overrun by %i bytes\n", (int)( length - rem ) );

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
r_imginfo_t LoadPNG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return LoadSTB( name, allocbuf, uptr );
}

/*
=========================================================

ETC COMPRESSION

=========================================================
*/

static const int q_etc1_modifierTable[] =
{
	2, 8, -2, -8,
	5, 17, -5, -17,
	9, 29, -9, -29,
	13, 42, -13, -42,
	18, 60, -18, -60,
	24, 80, -24, -80,
	33, 106, -33, -106,
	47, 183, -47, -183
};

static const int q_etc1_lookup[] = { 0, 1, 2, 3, -4, -3, -2, -1 };

static void q_etc1_subblock( uint8_t *out, int stride, bool bgr, int r, int g, int b,
							 const int *table, unsigned int low, bool second, bool flipped ) {
	int baseX = 0, baseY = 0;
	int i;
	int x, y;
	int k, delta;
	uint8_t *q;
	if( second ) {
		if( flipped ) {
			baseY = 2;
		} else {
			baseX = 2;
		}
	}
	for( i = 0; i < 8; ++i ) {
		if( flipped ) {
			x = baseX + ( i >> 1 );
			y = baseY + ( i & 1 );
		} else {
			x = baseX + ( i >> 2 );
			y = baseY + ( i & 3 );
		}
		k = y + ( x * 4 );
		delta = table[( ( low >> k ) & 1 ) | ( ( low >> ( k + 15 ) ) & 2 )];
		q = out + 3 * x + stride * y;
		if( bgr ) {
			*( q++ ) = Q_bound( 0, b + delta, 255 );
			*( q++ ) = Q_bound( 0, g + delta, 255 );
			*( q++ ) = Q_bound( 0, r + delta, 255 );
		} else {
			*( q++ ) = Q_bound( 0, r + delta, 255 );
			*( q++ ) = Q_bound( 0, g + delta, 255 );
			*( q++ ) = Q_bound( 0, b + delta, 255 );
		}
	}
}

static void q_etc1_block( const uint8_t *in, uint8_t *out, int stride, bool bgr ) {
	unsigned int high = ( in[0] << 24 ) | ( in[1] << 16 ) | ( in[2] << 8 ) | in[3];
	unsigned int low = ( in[4] << 24 ) | ( in[5] << 16 ) | ( in[6] << 8 ) | in[7];
	int r1, r2, g1, g2, b1, b2;
	bool flipped = ( bool )( high & 1 );

	if( high & 2 ) {
		int rBase, gBase, bBase;

		rBase = ( high >> 27 ) & 31;
		r1 = ( rBase << 3 ) | ( rBase >> 2 );
		rBase = ( rBase + ( q_etc1_lookup[( high >> 24 ) & 7] ) ) & 31;
		r2 = ( rBase << 3 ) | ( rBase >> 2 );

		gBase = ( high >> 19 ) & 31;
		g1 = ( gBase << 3 ) | ( gBase >> 2 );
		gBase = ( gBase + ( q_etc1_lookup[( high >> 16 ) & 7] ) ) & 31;
		g2 = ( gBase << 3 ) | ( gBase >> 2 );

		bBase = ( high >> 11 ) & 31;
		b1 = ( bBase << 3 ) | ( bBase >> 2 );
		bBase = ( bBase + ( q_etc1_lookup[( high >> 8 ) & 7] ) ) & 31;
		b2 = ( bBase << 3 ) | ( bBase >> 2 );
	} else {
		r1 = ( ( high >> 24 ) & 0xf0 ) | ( ( high >> 28 ) & 0xf );
		r2 = ( ( high >> 20 ) & 0xf0 ) | ( ( high >> 24 ) & 0xf );
		g1 = ( ( high >> 16 ) & 0xf0 ) | ( ( high >> 20 ) & 0xf );
		g2 = ( ( high >> 12 ) & 0xf0 ) | ( ( high >> 16 ) & 0xf );
		b1 = ( ( high >> 8 ) & 0xf0 ) | ( ( high >> 12 ) & 0xf );
		b2 = ( ( high >> 4 ) & 0xf0 ) | ( ( high >> 8 ) & 0xf );
	}

	q_etc1_subblock( out, stride, bgr, r1, g1, b1,
					 q_etc1_modifierTable + ( ( high >> 3 ) & ( 7 << 2 ) ),
					 low, false, flipped );
	q_etc1_subblock( out, stride, bgr, r2, g2, b2,
					 q_etc1_modifierTable + ( high & ( 7 << 2 ) ),
					 low, true, flipped );
}

void DecompressETC1( const uint8_t *in, int width, int height, uint8_t *out, bool bgr ) {
	int stride = Q_ALIGN( width, 4 ) * 3;
	uint8_t *uncompressed = alloca( 4 * stride );
	int i, j, rows, rowSize = width * 3, rowSizeAligned = Q_ALIGN( rowSize, 4 );
	for( i = 0; i < height; i += 4 ) {
		for( j = 0; j < width; j += 4 ) {
			q_etc1_block( in, uncompressed + j * 3, stride, bgr );
			in += 8;
		}
		rows = height - i;
		if( rows > 4 ) {
			rows = 4;
		}
		for( j = 0; j < rows; ++j ) {
			memcpy( out, uncompressed + j * stride, rowSize );
			out += rowSizeAligned;
		}
	}
}
