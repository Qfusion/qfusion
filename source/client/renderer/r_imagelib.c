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
#include "../../qalgo/hash.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <setjmp.h>

r_imginfo_t IMG_LoadImage( const char * filename, uint8_t * ( *allocbuf )( void *, size_t, const char *, int ), void * uptr ) {
	( void ) allocbuf;

	r_imginfo_t ret;
	memset( &ret, 0, sizeof( ret ) );

	uint8_t * data;
	size_t size = R_LoadFile( filename, ( void ** ) &data );
	if( data == NULL )
		return ret;

	ret.pixels = stbi_load_from_memory( data, size, &ret.width, &ret.height, &ret.samples, 0 );
	ret.comp = ret.samples == 4 ? IMGCOMP_RGBA : IMGCOMP_RGB;

	R_FreeFile( data );

	return ret;
}

bool WriteTGA( const char * filename, r_imginfo_t * img, int quality ) {
	return stbi_write_tga( filename, img->width, img->height, img->samples, img->pixels ) != 0;
}

bool WriteJPG( const char * filename, r_imginfo_t * img, int quality ) {
	return stbi_write_jpg( filename, img->width, img->height, img->samples, img->pixels, quality ) != 0;
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
			*( q++ ) = bound( 0, b + delta, 255 );
			*( q++ ) = bound( 0, g + delta, 255 );
			*( q++ ) = bound( 0, r + delta, 255 );
		} else {
			*( q++ ) = bound( 0, r + delta, 255 );
			*( q++ ) = bound( 0, g + delta, 255 );
			*( q++ ) = bound( 0, b + delta, 255 );
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
	int stride = ALIGN( width, 4 ) * 3;
	uint8_t *uncompressed = alloca( 4 * stride );
	int i, j, rows, rowSize = width * 3, rowSizeAligned = ALIGN( rowSize, 4 );
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
