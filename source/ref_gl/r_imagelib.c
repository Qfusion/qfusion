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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

static const r_imginfo_t empty_imginfo = { 0 };

/*
* R_Imagelib_Init
*/
void R_Imagelib_Init( void ) {
	stbi_flip_vertically_on_write( 1 );
}

/*
* R_Imagelib_Shutdown
*/
void R_Imagelib_Shutdown( void ) {
}

/*
* R_LoadSTB
*/
static r_imginfo_t R_LoadSTB( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	uint8_t *img;
	uint8_t *png_data;
	size_t png_datasize;
	size_t imgsize;
	r_imginfo_t imginfo;

	memset( &imginfo, 0, sizeof( imginfo ) );

	// load the file
	png_datasize = R_LoadFile( name, (void **)&png_data );
	if( !png_data ) {
		return empty_imginfo;
	}

	img = stbi_load_from_memory( png_data, png_datasize, &imginfo.width, &imginfo.height, &imginfo.samples, 0 );
	R_FreeFile( png_data );

	if( !img ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad image file %s: %s\n", name, stbi_failure_reason() );
		return empty_imginfo;
	}

	if( imginfo.samples != 1 && imginfo.samples != 3 && imginfo.samples != 4 ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "Bad image file %s samples: %d\n", name, imginfo.samples );
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
* R_WriteSTBFunc
*/
static void R_WriteSTBFunc( void* context, void* data, int size ) {
	ri.FS_Write( data, size, (int)((intptr_t)context) );
}

/*
* LoadTGA
*/
r_imginfo_t LoadTGA( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return R_LoadSTB( name, allocbuf, uptr );
}

/*
* WriteTGA
*/
bool WriteTGA( const char *name, r_imginfo_t *info ) {
	int file;

	if( ri.FS_FOpenAbsoluteFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "WriteTGA: Couldn't create %s\n", name );
		return false;
	}

	if( !stbi_write_tga_to_func( &R_WriteSTBFunc, (void *)((intptr_t)file), info->width, info->height, info->samples, info->pixels ) ) {
		Com_Printf( "WriteTGA: Couldn't write to %s\n", name );
		ri.FS_FCloseFile( file );
		return false;
	}
	
	ri.FS_FCloseFile( file );
	return true;
}

/*
* LoadJPG
*/
r_imginfo_t LoadJPG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return R_LoadSTB( name, allocbuf, uptr );
}

/*
* WriteJPG
*/
bool WriteJPG( const char *name, r_imginfo_t *info, int quality ) {
	int file;

	if( ri.FS_FOpenAbsoluteFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "WriteJPG: Couldn't create %s\n", name );
		return false;
	}

	if( !stbi_write_jpg_to_func( &R_WriteSTBFunc, (void *)((intptr_t)file), info->width, info->height, info->samples, info->pixels, quality ) ) {
		Com_Printf( "WriteJPG: Couldn't write to %s\n", name );
		ri.FS_FCloseFile( file );
		return false;
	}
	
	ri.FS_FCloseFile( file );
	return true;
}

/*
* LoadPCX
*/
r_imginfo_t LoadPCX( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	uint8_t *raw;
	int x, y;
	int len, columns, rows;
	int dataByte, runLength;
	uint8_t *pal, *pix, *c;
	struct {
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
	} *pcx;
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
	pcx = (void *)raw;

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
* LoadPNG
*/
r_imginfo_t LoadPNG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr ) {
	return R_LoadSTB( name, allocbuf, uptr );
}

/*
* WritePNG
*/
bool WritePNG( const char *name, r_imginfo_t *info )
{
	int file;

	if( ri.FS_FOpenAbsoluteFile( name, &file, FS_WRITE ) == -1 ) {
		Com_Printf( "WritePNG: Couldn't create %s\n", name );
		return false;
	}

	if( !stbi_write_png_to_func( &R_WriteSTBFunc, (void *)( (intptr_t)file ), info->width, info->height, info->samples,
			info->pixels, 0 ) ) {
		Com_Printf( "WritePNG: Couldn't write to %s\n", name );
		ri.FS_FCloseFile( file );
		return false;
	}

	ri.FS_FCloseFile( file );
	return true;
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
