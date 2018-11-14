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

#define MAX_GLIMAGES        8192
#define IMAGES_HASH_SIZE    64

typedef struct {
	int ctx;
	int side;
} loaderCbInfo_t;

static image_t r_images[MAX_GLIMAGES];
static image_t r_images_hash_headnode[IMAGES_HASH_SIZE], *r_free_images;
static qmutex_t *r_imagesLock;

static int r_unpackAlignment[NUM_QGL_CONTEXTS];

static unsigned *r_8to24table[2];

static mempool_t *r_imagesPool;
static char *r_imagePathBuf, *r_imagePathBuf2;
static size_t r_sizeof_imagePathBuf, r_sizeof_imagePathBuf2;

#undef ENSUREBUFSIZE
#define ENSUREBUFSIZE( buf,need ) \
	if( r_sizeof_ ## buf < need ) \
	{ \
		if( r_ ## buf ) { \
			R_Free( r_ ## buf );} \
		r_sizeof_ ## buf += ( ( ( need ) & ( MAX_QPATH - 1 ) ) + 1 ) * MAX_QPATH; \
		r_ ## buf = R_MallocExt( r_imagesPool, r_sizeof_ ## buf, 0, 0 ); \
	}

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;

static int gl_filter_depth = GL_LINEAR;

static int gl_anisotropic_filter = 0;
static void R_InitImageLoader( int id );
static void R_ShutdownImageLoader( int id );
static bool R_LoadAsyncImageFromDisk( image_t *image );

typedef struct {
	char *name;
	int minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

#define NUM_GL_MODES ( sizeof( modes ) / sizeof( glmode_t ) )


/*
* R_AllocTextureNum
*/
static void R_AllocTextureNum( image_t *tex ) {
	qglGenTextures( 1, &tex->texnum );
}

/*
* R_FreeTextureNum
*/
static void R_FreeTextureNum( image_t *tex ) {
	if( !tex->texnum ) {
		return;
	}

	qglDeleteTextures( 1, &tex->texnum );
	tex->texnum = 0;

	RB_FlushTextureCache();
}

/*
* R_TextureTarget
*/
int R_TextureTarget( int flags, int *uploadTarget ) {
	int target, target2;

	if( flags & IT_CUBEMAP ) {
		target = GL_TEXTURE_CUBE_MAP_ARB;
		target2 = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
	} else if( flags & IT_ARRAY ) {
		target = target2 = GL_TEXTURE_2D_ARRAY_EXT;
	} else if( flags & IT_3D ) {
		target = target2 = GL_TEXTURE_3D_EXT;
	} else {
		target = target2 = GL_TEXTURE_2D;
	}

	if( uploadTarget ) {
		*uploadTarget = target2;
	}
	return target;
}

/*
* R_BindImage
*/
static void R_BindImage( const image_t *tex ) {
	qglBindTexture( R_TextureTarget( tex->flags, NULL ), tex->texnum );
	RB_FlushTextureCache();
}

/*
* R_UnbindImage
*/
static void R_UnbindImage( const image_t *tex ) {
	qglBindTexture( R_TextureTarget( tex->flags, NULL ), 0 );
	RB_FlushTextureCache();
}

/*
* R_TextureMode
*/
void R_TextureMode( char *string ) {
	int i;
	image_t *glt;
	int target;

	for( i = 0; i < NUM_GL_MODES; i++ ) {
		if( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}

	if( i == NUM_GL_MODES ) {
		Com_Printf( "R_TextureMode: bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for( i = 1, glt = r_images; i < MAX_GLIMAGES; i++, glt++ ) {
		if( !glt->texnum ) {
			continue;
		}
		if( glt->flags & ( IT_NOFILTERING | IT_DEPTH ) ) {
			continue;
		}

		target = R_TextureTarget( glt->flags, NULL );

		R_BindImage( glt );

		if( !( glt->flags & IT_NOMIPMAP ) ) {
			qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		} else {
			qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
			qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
}

/*
* R_UnpackAlignment
*/
static void R_UnpackAlignment( int ctx, int value ) {
	if( r_unpackAlignment[ctx] == value ) {
		return;
	}

	r_unpackAlignment[ctx] = value;
	qglPixelStorei( GL_UNPACK_ALIGNMENT, value );
}

/*
* R_AnisotropicFilter
*/
void R_AnisotropicFilter( int value ) {
	int i, old;
	image_t *glt;

	if( !glConfig.ext.texture_filter_anisotropic ) {
		return;
	}

	old = gl_anisotropic_filter;
	gl_anisotropic_filter = bound( 1, value, glConfig.maxTextureFilterAnisotropic );
	if( gl_anisotropic_filter == old ) {
		return;
	}

	// change all the existing mipmap texture objects
	for( i = 1, glt = r_images; i < MAX_GLIMAGES; i++, glt++ ) {
		if( !glt->texnum ) {
			continue;
		}
		if( ( glt->flags & ( IT_NOFILTERING | IT_DEPTH | IT_NOMIPMAP ) ) ) {
			continue;
		}

		R_BindImage( glt );

		qglTexParameteri( R_TextureTarget( glt->flags, NULL ), GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic_filter );
	}
}

/*
* R_PrintImageList
*/
void R_PrintImageList( const char *mask, bool ( *filter )( const char *mask, const char *value ) ) {
	int i, bpp, bytes;
	int numImages;
	image_t *image;
	double texels = 0, add, total_bytes = 0;

	Com_Printf( "------------------\n" );

	numImages = 0;
	for( i = 0, image = r_images; i < MAX_GLIMAGES; i++, image++ ) {
		if( !image->texnum ) {
			continue;
		}
		if( !image->upload_width || !image->upload_height || !image->layers ) {
			continue;
		}
		if( filter && !filter( mask, image->name ) ) {
			continue;
		}
		if( !image->loaded || image->missing ) {
			continue;
		}

		add = image->upload_width * image->upload_height * image->layers;
		if( !( image->flags & ( IT_DEPTH | IT_NOFILTERING | IT_NOMIPMAP ) ) ) {
			add = (unsigned)floor( add / 0.75 );
		}
		if( image->flags & IT_CUBEMAP ) {
			add *= 6;
		}
		texels += add;

		bpp = image->samples;
		if( image->flags & IT_DEPTH ) {
			bpp = 0; // added later
		} else if( ( image->flags & IT_FRAMEBUFFER ) && !glConfig.ext.rgb8_rgba8 ) {
			bpp = 2;
		}

		if( image->flags & ( IT_DEPTH | IT_DEPTHRB ) ) {
			if( image->flags & IT_STENCIL ) {
				bpp += 4;
			} else if( glConfig.ext.depth24 ) {
				bpp += 3;
			} else {
				bpp += 2;
			}
		}

		bytes = add * bpp;
		total_bytes += bytes;

		Com_Printf( " %iW x %iH", image->upload_width, image->upload_height );
		if( image->layers > 1 ) {
			Com_Printf( " x %iL", image->layers );
		}
		Com_Printf( " x %s%iBPP: %s%s%s %.1f KB\n", image->flags & IT_SRGB ? "s" : "", bpp, image->name, image->extension,
					( ( image->flags & ( IT_NOMIPMAP | IT_NOFILTERING ) ) ? "" : " (mip)" ), bytes / 1024.0 );

		numImages++;
	}

	Com_Printf( "Total texels count (counting mipmaps, approx): %.0f\n", texels );
	Com_Printf( "%i RGBA images, totalling %.3f megabytes\n", numImages, total_bytes / 1048576.0 );
}

/*
=================================================================

TEMPORARY IMAGE BUFFERS

=================================================================
*/

enum {
	TEXTURE_LOADING_BUF0,TEXTURE_LOADING_BUF1,TEXTURE_LOADING_BUF2,TEXTURE_LOADING_BUF3,TEXTURE_LOADING_BUF4,TEXTURE_LOADING_BUF5,
	TEXTURE_RESAMPLING_BUF0,TEXTURE_RESAMPLING_BUF1,TEXTURE_RESAMPLING_BUF2,TEXTURE_RESAMPLING_BUF3,TEXTURE_RESAMPLING_BUF4,TEXTURE_RESAMPLING_BUF5,
	TEXTURE_LINE_BUF,
	TEXTURE_CUT_BUF,
	TEXTURE_FLIPPING_BUF0,TEXTURE_FLIPPING_BUF1,TEXTURE_FLIPPING_BUF2,TEXTURE_FLIPPING_BUF3,TEXTURE_FLIPPING_BUF4,TEXTURE_FLIPPING_BUF5,

	NUM_IMAGE_BUFFERS
};

static uint8_t *r_screenShotBuffer;
static size_t r_screenShotBufferSize;

static uint8_t *r_imageBuffers[NUM_QGL_CONTEXTS][NUM_IMAGE_BUFFERS];
static size_t r_imageBufSize[NUM_QGL_CONTEXTS][NUM_IMAGE_BUFFERS];

#define R_PrepareImageBuffer( ctx,buffer,size ) _R_PrepareImageBuffer( ctx,buffer,size,__FILE__,__LINE__ )

/*
* R_PrepareImageBuffer
*/
static uint8_t *_R_PrepareImageBuffer( int ctx, int buffer, size_t size,
									   const char *filename, int fileline ) {
	if( r_imageBufSize[ctx][buffer] < size ) {
		r_imageBufSize[ctx][buffer] = size;
		if( r_imageBuffers[ctx][buffer] ) {
			R_Free( r_imageBuffers[ctx][buffer] );
		}
		r_imageBuffers[ctx][buffer] = R_MallocExt( r_imagesPool, size, 0, 1 );
	}

	memset( r_imageBuffers[ctx][buffer], 255, size );

	return r_imageBuffers[ctx][buffer];
}

/*
* R_FreeImageBuffers
*/
void R_FreeImageBuffers( void ) {
	int i, j;

	for( i = 0; i < NUM_QGL_CONTEXTS; i++ )
		for( j = 0; j < NUM_IMAGE_BUFFERS; j++ ) {
			if( r_imageBuffers[i][j] ) {
				R_Free( r_imageBuffers[i][j] );
				r_imageBuffers[i][j] = NULL;
			}
			r_imageBufSize[i][j] = 0;
		}
}

/*
* R_SwapBlueRed
*/
static void R_SwapBlueRed( uint8_t *data, int width, int height, int samples, int alignment ) {
	int i, j, k, padding;

	padding = ALIGN( width * samples, alignment ) - width * samples;
	for( i = 0; i < height; i++, data += padding ) {
		for( j = 0; j < width; j++, data += samples ) {
			k = data[0];
			data[0] = data[2];
			data[2] = k;
			// data[0] ^= data[2];
			// data[2] = data[0] ^ data[2];
			// data[0] ^= data[2];
		}
	}
}

/*
* R_EndianSwap16BitImage
*/
static void R_EndianSwap16BitImage( unsigned short *data, int width, int height ) {
	int i;
	while( height-- > 0 ) {
		for( i = 0; i < width; i++, data++ )
			*data = ( ( *data & 255 ) << 8 ) | ( *data >> 8 );
		data += width & 1; // 4 unpack alignment
	}
}

/*
* R_AllocImageBufferCb
*/
static uint8_t *_R_AllocImageBufferCb( void *ptr, size_t size, const char *filename, int linenum ) {
	loaderCbInfo_t *cbinfo = ptr;
	return _R_PrepareImageBuffer( cbinfo->ctx, cbinfo->side, size, filename, linenum );
}

/*
* R_ReadImageFromDisk
*/
static int R_ReadImageFromDisk( int ctx, char *pathname, size_t pathname_size,
								uint8_t **pic, int *width, int *height, int *flags, int side ) {
	const char *extension;
	int samples;

	*pic = NULL;
	*width = *height = 0;
	samples = 0;

	extension = ri.FS_FirstExtension( pathname, IMAGE_EXTENSIONS, NUM_IMAGE_EXTENSIONS - 1 ); // last is KTX
	if( extension ) {
		r_imginfo_t imginfo;
		loaderCbInfo_t cbinfo = { ctx, side };

		COM_ReplaceExtension( pathname, extension, pathname_size );

		if( !Q_stricmp( extension, ".jpg" ) ) {
			imginfo = LoadJPG( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		} else if( !Q_stricmp( extension, ".tga" ) ) {
			imginfo = LoadTGA( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		} else if( !Q_stricmp( extension, ".png" ) ) {
			imginfo = LoadPNG( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		} else if( !Q_stricmp( extension, ".pcx" ) ) {
			imginfo = LoadPCX( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		} else if( !Q_stricmp( extension, ".wal" ) ) {
			imginfo = LoadWAL( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		} else {
			return 0;
		}

		if( imginfo.samples >= 3 ) {
			if( ( ( imginfo.comp & ~1 ) == IMGCOMP_BGR ) && ( !glConfig.ext.bgra || !flags ) ) {
				R_SwapBlueRed( imginfo.pixels, imginfo.width, imginfo.height, imginfo.samples, 1 );
				imginfo.comp = IMGCOMP_RGB | ( imginfo.comp & 1 );
			}
		}

		*pic = imginfo.pixels;
		*width = imginfo.width;
		*height = imginfo.height;
		samples = imginfo.samples;
		if( flags ) {
			if( ( imginfo.samples >= 3 ) && ( ( imginfo.comp & ~1 ) == IMGCOMP_BGR ) ) {
				*flags |= IT_BGRA;
			}
			if( imginfo.samples > 0 && !Q_stricmp( extension, ".wal" ) ) {
				*flags = ( *flags | IT_WAL ) & ~IT_SRGB;
			}
		}
	}

	return samples;
}

/*
* R_ScaledImageSize
*/
static int R_ScaledImageSize( int width, int height, int *scaledWidth, int *scaledHeight, int flags, int mips, int minmipsize, bool forceNPOT ) {
	int maxSize;
	int mip = 0;
	int clampedWidth, clampedHeight;
	bool makePOT;

	if( flags & ( IT_FRAMEBUFFER | IT_DEPTH ) ) {
		maxSize = glConfig.maxRenderbufferSize;
	} else if( flags & IT_CUBEMAP ) {
		maxSize = glConfig.maxTextureCubemapSize;
	} else if( flags & IT_3D ) {
		maxSize = glConfig.maxTexture3DSize;
	} else {
		maxSize = glConfig.maxTextureSize;
	}

	makePOT = !glConfig.ext.texture_non_power_of_two && !forceNPOT;
#ifdef GL_ES_VERSION_2_0
	makePOT = makePOT && ( ( flags & ( IT_CLAMP | IT_NOMIPMAP ) ) != ( IT_CLAMP | IT_NOMIPMAP ) );
#endif
	if( makePOT ) {
		int potWidth, potHeight;

		for( potWidth = 1; potWidth < width; potWidth <<= 1 ) ;
		for( potHeight = 1; potHeight < height; potHeight <<= 1 ) ;

		if( ( width != potWidth ) || ( height != potHeight ) ) {
			mips = 1;
		}

		width = potWidth;
		height = potHeight;
	}

	if( !( flags & IT_NOPICMIP ) ) {
		// let people sample down the sky textures for speed
		int picmip = ( flags & IT_SKY ) ? r_skymip->integer : r_picmip->integer;
		while( ( mip < picmip ) && ( ( width > minmipsize ) || ( height > minmipsize ) ) ) {
			++mip;
			width >>= 1;
			height >>= 1;
			if( !width ) {
				width = 1;
			}
			if( !height ) {
				height = 1;
			}
		}
	}

	// try to find the smallest supported texture size from mipmaps
	clampedWidth = width;
	clampedHeight = height;
	while( ( clampedWidth > maxSize ) || ( clampedHeight > maxSize ) ) {
		++mip;
		clampedWidth >>= 1;
		clampedHeight >>= 1;
		if( !clampedWidth ) {
			clampedWidth = 1;
		}
		if( !clampedHeight ) {
			clampedHeight = 1;
		}
	}

	if( mip >= mips ) {
		// the smallest size is not in mipmaps, so ignore mipmaps and aspect ratio and simply clamp
		*scaledWidth = min( width, maxSize );
		*scaledHeight = min( height, maxSize );
		return -1;
	}

	*scaledWidth = clampedWidth;
	*scaledHeight = clampedHeight;
	return mip;
}

/*
* R_FlipTexture
*/
static void R_FlipTexture( const uint8_t *in, uint8_t *out, int width, int height,
						   int samples, bool flipx, bool flipy, bool flipdiagonal ) {
	int i, x, y;
	const uint8_t *p, *line;
	int row_inc = ( flipy ? -samples : samples ) * width, col_inc = ( flipx ? -samples : samples );
	int row_ofs = ( flipy ? ( height - 1 ) * width * samples : 0 ), col_ofs = ( flipx ? ( width - 1 ) * samples : 0 );

	if( !in ) {
		return;
	}

	if( flipdiagonal ) {
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	} else {
		for( y = 0, line = in + row_ofs; y < height; y++, line += row_inc )
			for( x = 0, p = line + col_ofs; x < width; x++, p += col_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
}

/*
* R_CutImage
*/
static void R_CutImage( const uint8_t *in, int inwidth, int height, uint8_t *out, int x, int y, int outwidth, int outheight, int samples ) {
	int i;
	uint8_t *iout;

	if( x + outwidth > inwidth ) {
		outwidth = inwidth - x;
	}
	if( y + outheight > height ) {
		outheight = height - y;
	}

	x *= samples;
	inwidth *= samples;
	outwidth *= samples;

	for( i = 0, iout = (uint8_t *)out; i < outheight; i++, iout += outwidth ) {
		const uint8_t *iin = (uint8_t *)in + ( y + i ) * inwidth + x;
		memcpy( iout, iin, outwidth );
	}
}

/*
* R_ResampleTexture
*/
static void R_ResampleTexture( int ctx, const uint8_t *in, int inwidth, int inheight, uint8_t *out,
							   int outwidth, int outheight, int samples, int alignment ) {
	int i, j, k;
	int inwidthS, outwidthS;
	unsigned int frac, fracstep;
	const uint8_t *inrow, *inrow2, *pix1, *pix2, *pix3, *pix4;
	unsigned *p1, *p2;
	uint8_t *opix;

	if( inwidth == outwidth && inheight == outheight ) {
		memcpy( out, in, inheight * ALIGN( inwidth * samples, alignment ) );
		return;
	}

	p1 = ( unsigned * )R_PrepareImageBuffer( ctx, TEXTURE_LINE_BUF, outwidth * sizeof( *p1 ) * 2 );
	p2 = p1 + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ ) {
		p1[i] = samples * ( frac >> 16 );
		frac += fracstep;
	}

	frac = 3 * ( fracstep >> 2 );
	for( i = 0; i < outwidth; i++ ) {
		p2[i] = samples * ( frac >> 16 );
		frac += fracstep;
	}

	inwidthS = ALIGN( inwidth * samples, alignment );
	outwidthS = ALIGN( outwidth * samples, alignment );
	for( i = 0; i < outheight; i++, out += outwidthS ) {
		inrow = in + inwidthS * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidthS * (int)( ( i + 0.75 ) * inheight / outheight );
		for( j = 0; j < outwidth; j++ ) {
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			opix = out + j * samples;

			for( k = 0; k < samples; k++ )
				opix[k] = ( pix1[k] + pix2[k] + pix3[k] + pix4[k] ) >> 2;
		}
	}
}

/*
* R_ResampleTexture16
*
* Assumes 16-bit unpack alignment
*/
static void R_ResampleTexture16( int ctx, const unsigned short *in, int inwidth, int inheight,
								 unsigned short *out, int outwidth, int outheight, int rMask, int gMask, int bMask, int aMask ) {
	int i, j;
	int inwidthA, outwidthA;
	unsigned int frac, fracstep;
	const unsigned short *inrow, *inrow2, *pix1, *pix2, *pix3, *pix4;
	unsigned *p1, *p2;
	unsigned short *opix;

	if( inwidth == outwidth && inheight == outheight ) {
		memcpy( out, in, inheight * ALIGN( inwidth * sizeof( unsigned short ), 4 ) );
		return;
	}

	p1 = ( unsigned * )R_PrepareImageBuffer( ctx, TEXTURE_LINE_BUF, outwidth * sizeof( *p1 ) * 2 );
	p2 = p1 + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ ) {
		p1[i] = frac >> 16;
		frac += fracstep;
	}

	frac = 3 * ( fracstep >> 2 );
	for( i = 0; i < outwidth; i++ ) {
		p2[i] = frac >> 16;
		frac += fracstep;
	}

	inwidthA = ALIGN( inwidth, 2 );
	outwidthA = ALIGN( outwidth, 2 );
	for( i = 0; i < outheight; i++, out += outwidthA ) {
		inrow = in + inwidthA * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidthA * (int)( ( i + 0.75 ) * inheight / outheight );
		for( j = 0; j < outwidth; j++ ) {
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			opix = out + j;

			*opix = ( ( ( ( *pix1 & rMask ) + ( *pix2 & rMask ) + ( *pix3 & rMask ) + ( *pix4 & rMask ) ) >> 2 ) & rMask ) |
					( ( ( ( *pix1 & gMask ) + ( *pix2 & gMask ) + ( *pix3 & gMask ) + ( *pix4 & gMask ) ) >> 2 ) & gMask ) |
					( ( ( ( *pix1 & bMask ) + ( *pix2 & bMask ) + ( *pix3 & bMask ) + ( *pix4 & bMask ) ) >> 2 ) & bMask ) |
					( ( ( ( *pix1 & aMask ) + ( *pix2 & aMask ) + ( *pix3 & aMask ) + ( *pix4 & aMask ) ) >> 2 ) & aMask );
		}
	}
}

/*
* R_MipMap
*
* Operates in place, quartering the size of the texture
*/
static void R_MipMap( uint8_t *in, int width, int height, int samples, int alignment ) {
	int i, j, k;
	int instride = ALIGN( width * samples, alignment );
	int outwidth, outheight, outpadding;
	uint8_t *out = in;
	uint8_t *next;
	int inofs;

	outwidth = width >> 1;
	outheight = height >> 1;
	if( !outwidth ) {
		outwidth = 1;
	}
	if( !outheight ) {
		outheight = 1;
	}
	outpadding = ALIGN( outwidth * samples, alignment ) - outwidth * samples;

	for( i = 0; i < outheight; i++, in += instride * 2, out += outpadding ) {
		next = ( ( ( i << 1 ) + 1 ) < height ) ? ( in + instride ) : in;
		for( j = 0, inofs = 0; j < outwidth; j++, inofs += samples ) {
			if( ( ( j << 1 ) + 1 ) < width ) {
				for( k = 0; k < samples; ++k, ++inofs )
					*( out++ ) = ( in[inofs] + in[inofs + samples] + next[inofs] + next[inofs + samples] ) >> 2;
			} else {
				for( k = 0; k < samples; ++k, ++inofs )
					*( out++ ) = ( in[inofs] + next[inofs] ) >> 1;
			}
		}
	}
}

/*
* R_MipMap16
*
* Operates in place, quartering the size of the 16-bit texture, assumes unpack alignment of 4
*/
static void R_MipMap16( unsigned short *in, int width, int height, int rMask, int gMask, int bMask, int aMask ) {
	int i, j;
	int instride = ALIGN( width, 2 );
	int outwidth, outheight, outpadding;
	unsigned short *out = in;
	unsigned short *next;
	int col, p[4];

	outwidth = width >> 1;
	outheight = height >> 1;
	if( !outwidth ) {
		outwidth = 1;
	}
	if( !outheight ) {
		outheight = 1;
	}
	outpadding = outwidth & 1;

	for( i = 0; i < outheight; i++, in += instride * 2, out += outpadding ) {
		next = ( ( ( i << 1 ) + 1 ) < height ) ? ( in + instride ) : in;
		for( j = 0; j < outwidth; j++ ) {
			col = j << 1;
			p[0] = in[col];
			p[1] = next[col];
			if( ( col + 1 ) < width ) {
				p[2] = in[col + 1];
				p[3] = next[col + 1];
				*( out++ ) =    ( ( ( ( p[0] & rMask ) + ( p[1] & rMask ) + ( p[2] & rMask ) + ( p[3] & rMask ) ) >> 2 ) & rMask ) |
							 ( ( ( ( p[0] & gMask ) + ( p[1] & gMask ) + ( p[2] & gMask ) + ( p[3] & gMask ) ) >> 2 ) & gMask ) |
							 ( ( ( ( p[0] & bMask ) + ( p[1] & bMask ) + ( p[2] & bMask ) + ( p[3] & bMask ) ) >> 2 ) & bMask ) |
							 ( ( ( ( p[0] & aMask ) + ( p[1] & aMask ) + ( p[2] & aMask ) + ( p[3] & aMask ) ) >> 2 ) & aMask );
			} else {
				*( out++ ) =    ( ( ( ( p[0] & rMask ) + ( p[1] & rMask ) ) >> 1 ) & rMask ) |
							 ( ( ( ( p[0] & gMask ) + ( p[1] & gMask ) ) >> 1 ) & gMask ) |
							 ( ( ( ( p[0] & bMask ) + ( p[1] & bMask ) ) >> 1 ) & bMask ) |
							 ( ( ( ( p[0] & aMask ) + ( p[1] & aMask ) ) >> 1 ) & aMask );
			}
		}
	}
}

/*
* R_TextureInternalFormat
*/
#ifndef GL_ES_VERSION_2_0
static int R_TextureInternalFormat( int samples, int flags, int pixelType ) {
	bool sRGB = ( flags & IT_SRGB ) != 0;

	if( !( flags & IT_NOCOMPRESS ) && r_texturecompression->integer && glConfig.ext.texture_compression ) {
		if( sRGB ) {
			if( samples == 4 ) {
				return GL_COMPRESSED_SRGB_ALPHA_EXT;
			}
			if( samples == 3 ) {
				return GL_COMPRESSED_SRGB_EXT;
			}
			if( samples == 2 ) {
				return GL_COMPRESSED_SLUMINANCE_EXT;
			}
			if( ( samples == 1 ) && !( flags & IT_ALPHAMASK ) ) {
				return GL_COMPRESSED_SLUMINANCE_ALPHA_EXT;
			}
		} else {
			if( samples == 4 ) {
				return GL_COMPRESSED_RGBA_ARB;
			}
			if( samples == 3 ) {
				return GL_COMPRESSED_RGB_ARB;
			}
			if( samples == 2 ) {
				return GL_COMPRESSED_LUMINANCE_ALPHA_ARB;
			}
			if( ( samples == 1 ) && !( flags & IT_ALPHAMASK ) ) {
				return GL_COMPRESSED_LUMINANCE_ARB;
			}
		}
	}

	if( samples == 3 ) {
		if( sRGB ) {
			return GL_SRGB_EXT;
		}
		return GL_RGB;
	}

	if( samples == 2 ) {
		if( sRGB ) {
			return GL_SLUMINANCE_ALPHA_EXT;
		}
		return GL_LUMINANCE_ALPHA;
	}

	if( samples == 1 ) {
		if( sRGB ) {
			return ( ( flags & IT_ALPHAMASK ) ? GL_ALPHA : GL_SLUMINANCE_EXT );
		}
		return ( ( flags & IT_ALPHAMASK ) ? GL_ALPHA : GL_LUMINANCE );
	}

	if( sRGB ) {
		return GL_SRGB8_ALPHA8_EXT;
	}
	return GL_RGBA;
}
#endif

/*
* R_TextureFormat
*/
static void R_TextureFormat( int flags, int samples, int *comp, int *format, int *type ) {
	if( flags & IT_DEPTH ) {
		if( flags & IT_STENCIL ) {
			*comp = GL_DEPTH24_STENCIL8_EXT;
			*format = GL_DEPTH_STENCIL_EXT;
			*type = GL_UNSIGNED_INT_24_8_EXT;
		} else {
			*comp = *format = GL_DEPTH_COMPONENT;
			if( glConfig.ext.depth24 ) {
				*type = GL_UNSIGNED_INT;
			} else {
				*type = GL_UNSIGNED_SHORT;
				if( glConfig.ext.depth_nonlinear ) {
					*comp = GL_DEPTH_COMPONENT16_NONLINEAR_NV;
				}
			}
		}
	} else if( flags & IT_FRAMEBUFFER ) {
		if( samples == 4 ) {
			*comp = *format = GL_RGBA;
			*type = glConfig.ext.rgb8_rgba8 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT_4_4_4_4;
		} else {
			*comp = *format = GL_RGB;
			*type = glConfig.ext.rgb8_rgba8 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT_5_6_5;
		}

		if( flags & IT_FLOAT ) {
			*type = GL_FLOAT;
			if( samples == 4 ) {
				*comp = GL_RGBA16F_ARB;
			} else if( samples == 3 ) {
				*comp = GL_RGB16F_ARB;
			}
		}
	} else {
		if( samples == 4 ) {
			*format = ( flags & IT_BGRA ? GL_BGRA_EXT : GL_RGBA );
		} else if( samples == 3 ) {
			*format = ( flags & IT_BGRA ? GL_BGR_EXT : GL_RGB );
		} else if( samples == 2 ) {
			*format = GL_LUMINANCE_ALPHA;
		} else if( flags & IT_ALPHAMASK ) {
			*format = GL_ALPHA;
		} else {
			*format = GL_LUMINANCE;
		}

		if( flags & IT_FLOAT ) {
			*type = GL_FLOAT;
			if( samples == 4 ) {
				*comp = GL_RGBA16F_ARB;
			} else if( samples == 3 ) {
				*comp = GL_RGB16F_ARB;
			} else if( samples == 2 ) {
				*comp = GL_LUMINANCE_ALPHA16F_ARB;
			} else if( flags & IT_ALPHAMASK ) {
				*comp = GL_ALPHA16F_ARB;
			} else {
				*comp = GL_LUMINANCE16F_ARB;
			}
		} else {
			*type = GL_UNSIGNED_BYTE;
			*comp = *format;

#ifndef GL_ES_VERSION_2_0
			if( !( flags & IT_3D ) ) {
				*comp = R_TextureInternalFormat( samples, flags, GL_UNSIGNED_BYTE );
			}
#endif
		}
	}
}

/*
* R_SetupTexParameters
*/
static void R_SetupTexParameters( int flags, int upload_width, int upload_height, int minmipsize ) {
	int target = R_TextureTarget( flags, NULL );
	int wrap = GL_REPEAT;

	if( flags & IT_NOFILTERING ) {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	} else if( flags & IT_DEPTH ) {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_depth );

		if( glConfig.ext.texture_filter_anisotropic ) {
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
		}
	} else if( !( flags & IT_NOMIPMAP ) ) {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic ) {
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic_filter );
		}

		if( minmipsize > 1 ) {
			int mipwidth = upload_width, mipheight = upload_height, mip = 0;
			while( ( mipwidth > minmipsize ) || ( mipheight > minmipsize ) ) {
				++mip;
				mipwidth >>= 1;
				mipheight >>= 1;
				if( !mipwidth ) {
					mipwidth = 1;
				}
				if( !mipheight ) {
					mipheight = 1;
				}
			}
			qglTexParameteri( target, GL_TEXTURE_MAX_LOD_SGIS, mip );
			qglTexParameteri( target, GL_TEXTURE_MAX_LEVEL_SGIS, mip );
		}
	} else {
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic ) {
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
		}
	}

	// clamp if required
	if( flags & IT_CLAMP ) {
		if( glConfig.ext.texture_edge_clamp ) {
			wrap = GL_CLAMP_TO_EDGE;
		}
#ifndef GL_ES_VERSION_2_0
		else {
			wrap = GL_CLAMP;
		}
#endif
	}
	qglTexParameteri( target, GL_TEXTURE_WRAP_S, wrap );
	qglTexParameteri( target, GL_TEXTURE_WRAP_T, wrap );
	if( flags & IT_3D ) {
		qglTexParameteri( target, GL_TEXTURE_WRAP_R_EXT, wrap );
	}

	if( ( flags & IT_DEPTH ) && ( flags & IT_DEPTHCOMPARE ) && glConfig.ext.shadow ) {
		qglTexParameteri( target, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB );
		qglTexParameteri( target, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL );
	}
}

/*
* R_Upload32
*/
static void R_Upload32( int ctx, uint8_t **data, int layer,
						int x, int y, int width, int height,
						int flags, int minmipsize, int *upload_width, int *upload_height, int samples,
						bool subImage, bool noScale ) {
	int i, comp, format, type;
	int target;
	int numTextures;
	uint8_t *scaled = NULL;
	int scaledWidth, scaledHeight;

	assert( samples );

	R_ScaledImageSize( width, height, &scaledWidth, &scaledHeight, flags, 1, minmipsize,
					   ( subImage && noScale ) ? true : false );

	R_TextureTarget( flags, &target );

	// don't ever bother with > maxSize textures
	if( flags & IT_CUBEMAP ) {
		numTextures = 6;
	} else {
		if( flags & ( IT_LEFTHALF | IT_RIGHTHALF ) ) {
			// assume width represents half of the original image width
			uint8_t *temp = R_PrepareImageBuffer( ctx, TEXTURE_CUT_BUF, width * height * samples );
			if( flags & IT_LEFTHALF ) {
				R_CutImage( *data, width * 2, height, temp, 0, 0, width, height, samples );
			} else {
				R_CutImage( *data, width * 2, height, temp, width, 0, width, height, samples );
			}
			data = &r_imageBuffers[ctx][TEXTURE_CUT_BUF];
		}

		if( flags & ( IT_FLIPX | IT_FLIPY | IT_FLIPDIAGONAL ) ) {
			uint8_t *temp = R_PrepareImageBuffer( ctx, TEXTURE_FLIPPING_BUF0, width * height * samples );
			R_FlipTexture( data[0], temp, width, height, samples,
						   ( flags & IT_FLIPX ) ? true : false,
						   ( flags & IT_FLIPY ) ? true : false,
						   ( flags & IT_FLIPDIAGONAL ) ? true : false );
			data = &r_imageBuffers[ctx][TEXTURE_FLIPPING_BUF0];
		}

		numTextures = 1;
	}

	if( upload_width ) {
		*upload_width = scaledWidth;
	}
	if( upload_height ) {
		*upload_height = scaledHeight;
	}

	R_TextureFormat( flags, samples, &comp, &format, &type );

	if( !( flags & ( IT_ARRAY | IT_3D ) ) ) { // set in R_Create3DImage
		R_SetupTexParameters( flags, scaledWidth, scaledHeight, minmipsize );
	}

	R_UnpackAlignment( ctx, 1 );

	if( ( scaledWidth == width ) && ( scaledHeight == height ) && ( flags & IT_NOMIPMAP ) ) {
		if( flags & ( IT_ARRAY | IT_3D ) ) {
			for( i = 0; i < numTextures; i++, target++ )
				qglTexSubImage3DEXT( target, 0, 0, 0, layer, scaledWidth, scaledHeight, 1, format, type, data[i] );
		} else if( subImage ) {
			for( i = 0; i < numTextures; i++, target++ )
				qglTexSubImage2D( target, 0, x, y, scaledWidth, scaledHeight, format, type, data[i] );
		} else {
			for( i = 0; i < numTextures; i++, target++ )
				qglTexImage2D( target, 0, comp, scaledWidth, scaledHeight, 0, format, type, data[i] );
		}
	} else {
		for( i = 0; i < numTextures; i++, target++ ) {
			uint8_t *mip;

			if( !scaled ) {
				scaled = R_PrepareImageBuffer( ctx, TEXTURE_RESAMPLING_BUF0,
											   scaledWidth * scaledHeight * samples );
			}

			// resample the texture
			mip = scaled;
			if( data[i] ) {
				R_ResampleTexture( ctx, data[i], width, height, (uint8_t *)mip, scaledWidth, scaledHeight, samples, 1 );
			} else {
				mip = NULL;
			}

			if( flags & ( IT_ARRAY | IT_3D ) ) {
				qglTexSubImage3DEXT( target, 0, 0, 0, layer, scaledWidth, scaledHeight, 1, format, type, mip );
			} else if( subImage ) {
				qglTexSubImage2D( target, 0, x, y, scaledWidth, scaledHeight, format, type, mip );
			} else {
				qglTexImage2D( target, 0, comp, scaledWidth, scaledHeight, 0, format, type, mip );
			}

			// mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip ) {
				int w, h;
				int miplevel = 0;

				w = scaledWidth;
				h = scaledHeight;
				while( w > minmipsize || h > minmipsize ) {
					R_MipMap( mip, w, h, samples, 1 );

					w >>= 1;
					h >>= 1;
					if( w < 1 ) {
						w = 1;
					}
					if( h < 1 ) {
						h = 1;
					}
					miplevel++;

					if( flags & ( IT_ARRAY | IT_3D ) ) {
						qglTexSubImage3DEXT( target, miplevel, 0, 0, layer, w, h, 1, format, type, mip );
					} else if( subImage ) {
						qglTexSubImage2D( target, miplevel, x, y, w, h, format, type, mip );
					} else {
						qglTexImage2D( target, miplevel, comp, w, h, 0, format, type, mip );
					}
				}
			}
		}
	}
}

/*
* R_PixelFormatSize
*/
static int R_PixelFormatSize( int format, int type ) {
	switch( type ) {
		case GL_UNSIGNED_BYTE:
			switch( format ) {
				case GL_RGBA:
				case GL_BGRA_EXT:
					return 4;
				case GL_RGB:
				case GL_BGR_EXT:
					return 3;
				case GL_LUMINANCE_ALPHA:
					return 2;
				case GL_ALPHA:
				case GL_LUMINANCE:
					return 1;
			}
			break;
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
		case GL_UNSIGNED_SHORT_5_6_5:
			return 2;
		case 0: // 4x4 block sizes
			switch( format ) {
				case GL_ETC1_RGB8_OES:
					return 8;
			}
			break;
	}
	return 0;
}

/*
=================================================================

PALETTES

=================================================================
*/

/*
* R_LoadQuake1Palette
*/
static void R_LoadQuake1Palette( unsigned *out ) {
	uint8_t *raw;
	int r, g, b;
	unsigned v;
	int i, len;
	const uint8_t *pal;
	static const uint8_t host_quakepal[768] =
#include "../qcommon/quake1pal.h"
	;

	// get the palette
	len = R_LoadFile( "gfx/palette.lmp", (void **)&raw );
	pal = ( raw && len >= 768 ) ? raw : host_quakepal;

	for( i = 0; i < 256; i++ ) {
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = COLOR_RGBA( r, g, b, 255 );
		out[i] = LittleLong( v );
	}

	R_FreeFile( raw );

	out[255] = 0;   // 255 is transparent
}

/*
* R_LoadQuake2Palette
*/
static void R_LoadQuake2Palette( unsigned *out ) {
	int i;
	uint8_t     *pal;
	int r, g, b;
	unsigned v;
	loaderCbInfo_t cbinfo = { QGL_CONTEXT_MAIN, 0 };
	r_imginfo_t imginfo;

	imginfo = LoadPCX( "pics/colormap.pcx", _R_AllocImageBufferCb, (void *)&cbinfo );
	if( !imginfo.pixels ) {
		return;
	}

	pal = imginfo.pixels + imginfo.width * imginfo.height * imginfo.samples;
	for( i = 0; i < 256; i++ ) {
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = COLOR_RGBA( r, g, b, 255 );
		out[i] = LittleLong( v );
	}

	out[255] &= LittleLong( 0xffffff ); // 255 is transparent
}

/*
* R_LoadPalette
*
* Loads Q1 or Q2 palette from disk if not already loaded.
*/
unsigned *R_LoadPalette( int flags ) {
	assert( ( flags & ( IT_MIPTEX | IT_WAL ) ) != 0 );

	if( flags & IT_MIPTEX ) {
		if( !r_8to24table[0] ) {
			r_8to24table[0] = R_MallocExt( r_imagesPool, sizeof( unsigned ) * 256, 0, 1 );
			R_LoadQuake1Palette( r_8to24table[0] );
		}
		return r_8to24table[0];
	}

	if( flags & IT_WAL ) {
		if( !r_8to24table[1] ) {
			r_8to24table[1] = R_MallocExt( r_imagesPool, sizeof( unsigned ) * 256, 0, 1 );
			R_LoadQuake2Palette( r_8to24table[1] );
		}
		return r_8to24table[1];
	}

	return NULL;
}

/*
* R_MipCount
*/
static int R_MipCount( int width, int height, int minmipsize ) {
	int mips = 1;
	while( ( width > minmipsize ) || ( height > minmipsize ) ) {
		++mips;
		width >>= 1;
		height >>= 1;
		if( !width ) {
			width = 1;
		}
		if( !height ) {
			height = 1;
		}
	}
	return mips;
}

/*
* R_UploadMipmapped
*
* Loads a 16/24/32-bit image or cubemap with mipmaps.
* If the image is a cubemap, each face is data[mip * 6 + face].
* Otherwise, it's data[mip].
*/
static void R_UploadMipmapped( int ctx, uint8_t **data,
							   int width, int height, int mipLevels, int flags, int minmipsize,
							   int *upload_width, int *upload_height,
							   int format, int type ) {
	int i, j;
	int pixelSize = R_PixelFormatSize( format, type );
	int rMask = 0, gMask = 0, bMask = 0, aMask = 0;
	int scaledWidth, scaledHeight;
	int mip;
	uint8_t *scaled[6] = { NULL };
	int faces, faceSize = 0;
	int target, comp;
	int mips;
	uint8_t *face;
	int oldWidth = 0, oldHeight = 0;

	switch( type ) {
		case GL_UNSIGNED_SHORT_4_4_4_4:
			rMask = 15 << 12;
			gMask = 15 << 8;
			bMask = 15 << 4;
			aMask = 15;
			break;
		case GL_UNSIGNED_SHORT_5_5_5_1:
			rMask = 31 << 11;
			gMask = 31 << 6;
			bMask = 31 << 1;
			aMask = 1;
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			rMask = 31 << 11;
			gMask = 63 << 5;
			bMask = 31;
			break;
	}

	R_TextureTarget( flags, &target );

	faces = ( flags & IT_CUBEMAP ) ? 6 : 1;

	mip = R_ScaledImageSize( width, height, &scaledWidth, &scaledHeight, flags, mipLevels, minmipsize, false );

	if( upload_width ) {
		*upload_width = scaledWidth;
	}
	if( upload_height ) {
		*upload_height = scaledHeight;
	}

	if( mip < 0 ) {
		faceSize = ALIGN( scaledWidth * pixelSize, 4 ) * scaledHeight;

		for( i = 0; i < faces; i++ )
			scaled[i] = R_PrepareImageBuffer( ctx, TEXTURE_RESAMPLING_BUF0 + i, faceSize );

		// find the mip with the size closest to the target
		for( mip = 0; mip < ( mipLevels - 1 ); mip++ ) {
			if( ( max( width >> 1, 1 ) < scaledWidth ) || ( max( height >> 1, 1 ) < scaledHeight ) ) {
				break;
			}
			width >>= 1;
			height >>= 1;
			if( !width ) {
				width = 1;
			}
			if( !height ) {
				height = 1;
			}
		}

		if( type == GL_UNSIGNED_BYTE ) {
			for( i = 0; i < faces; i++ ) {
				R_ResampleTexture( ctx, data[mip * faces + i], width, height,
								   scaled[i], scaledWidth, scaledHeight, pixelSize, 4 );
			}
		} else {
			for( i = 0; i < faces; i++ ) {
				R_ResampleTexture16( ctx, ( unsigned short * )( data[mip * faces + i] ), width, height,
									 ( unsigned short * )( scaled[i] ), scaledWidth, scaledHeight, rMask, gMask, bMask, aMask );
			}
		}
		data = scaled;
		mip = 0;
		mipLevels = 1;
	}

#ifdef GL_ES_VERSION_2_0
	comp = format;
#else
	comp = R_TextureInternalFormat( pixelSize, flags, type );
#endif

	R_SetupTexParameters( flags, scaledWidth, scaledHeight, minmipsize );

	R_UnpackAlignment( ctx, 4 );

	mips = ( flags & IT_NOMIPMAP ) ? 1 : R_MipCount( scaledWidth, scaledHeight, minmipsize );
	for( i = 0; ( i < mips ) && ( mip < mipLevels ); i++, mip++ ) {
		faceSize = ALIGN( scaledWidth * pixelSize, 4 ) * scaledHeight; // will be used for the first remaining mipmap
		for( j = 0; j < faces; j++ )
			qglTexImage2D( target + j, i, comp, scaledWidth, scaledHeight, 0, format, type, data[mip * faces + j] );
		oldWidth = scaledWidth;
		oldHeight = scaledHeight;
		scaledWidth >>= 1;
		scaledHeight >>= 1;
		if( !scaledWidth ) {
			scaledWidth = 1;
		}
		if( !scaledHeight ) {
			scaledHeight = 1;
		}
	}

	for( ; i < mips; i++ ) {
		for( j = 0; j < faces; j++ ) {
			if( !( scaled[j] ) ) {
				scaled[j] = R_PrepareImageBuffer( ctx, TEXTURE_RESAMPLING_BUF0 + j, faceSize );
				memcpy( scaled[j], data[( mip - 1 ) * faces + j], faceSize );
			}
			face = scaled[j];
			if( type == GL_UNSIGNED_BYTE ) {
				R_MipMap( face, oldWidth, oldHeight, pixelSize, 4 );
			} else {
				R_MipMap16( ( unsigned short * )face, oldWidth, oldHeight, rMask, gMask, bMask, aMask );
			}
			qglTexImage2D( target + j, i, comp, scaledWidth, scaledHeight, 0, format, type, face );
		}

		oldWidth = scaledWidth;
		oldHeight = scaledHeight;
		scaledWidth >>= 1;
		scaledHeight >>= 1;
		if( !scaledWidth ) {
			scaledWidth = 1;
		}
		if( !scaledHeight ) {
			scaledHeight = 1;
		}
	}
}

/*
* R_IsKTXFormatValid
*/
static bool R_IsKTXFormatValid( int format, int type ) {
	switch( type ) {
		case GL_UNSIGNED_BYTE:
			switch( format ) {
				case GL_RGBA:
				case GL_BGRA_EXT:
				case GL_RGB:
				case GL_BGR_EXT:
				case GL_LUMINANCE_ALPHA:
				case GL_ALPHA:
				case GL_LUMINANCE:
					return true;
			}
			return false;
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
			return ( format == GL_RGBA ) ? true : false;
		case GL_UNSIGNED_SHORT_5_6_5:
			return ( format == GL_RGB ) ? true : false;
		case 0:
			return ( format == GL_ETC1_RGB8_OES ) ? true : false;
	}
	return false;
}

typedef struct ktx_header_s {
	char identifier[12];
	int endianness;
	int type;
	int typeSize;
	int format;
	int internalFormat;
	int baseInternalFormat;
	int pixelWidth;
	int pixelHeight;
	int pixelDepth;
	int numberOfArrayElements;
	int numberOfFaces;
	int numberOfMipmapLevels;
	int bytesOfKeyValueData;
} ktx_header_t;

/*
* R_LoadKTX
*/
static bool R_LoadKTX( int ctx, image_t *image, const char *pathname ) {
	int i, j;
	uint8_t *buffer;
	ktx_header_t *header;
	bool swapEndian;
	uint8_t *data;
	int numFaces = ( ( image->flags & IT_CUBEMAP ) ? 6 : 1 ), numMips;

	if( image->flags & ( IT_FLIPX | IT_FLIPY | IT_FLIPDIAGONAL ) ) {
		return false;
	}

	R_LoadFile( pathname, ( void ** )&buffer );
	if( !buffer ) {
		return false;
	}

	header = ( ktx_header_t * )buffer;
	if( memcmp( header->identifier, "\xABKTX 11\xBB\r\n\x1A\n", 12 ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Bad file identifier: %s\n", pathname );
		goto error;
	}

	swapEndian = ( header->endianness == 0x01020304 ) ? true : false;
	if( swapEndian ) {
		for( i = 3; i < 16; ++i )
			( ( int * )header )[i] = LongSwap( ( ( int * )header )[i] );
	}

	if( header->format && ( header->format != header->baseInternalFormat ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Pixel format doesn't match internal format: %s\n", pathname );
		goto error;
	}
	if( !R_IsKTXFormatValid( header->format ? header->baseInternalFormat : header->internalFormat, header->type ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Unsupported pixel format: %s\n", pathname );
		goto error;
	}
	if( ( header->pixelWidth < 1 ) || ( header->pixelHeight < 0 ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Zero texture size: %s\n", pathname );
		goto error;
	}
	if( !header->pixelHeight ) {
		header->pixelHeight = 1;
	}
	if( !header->type && ( ( header->pixelWidth & ( header->pixelWidth - 1 ) ) || ( header->pixelHeight & ( header->pixelHeight - 1 ) ) ) ) {
		// NPOT compressed textures may crash on certain drivers/GPUs
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Compressed image must be power-of-two: %s\n", pathname );
		goto error;
	}
	if( ( image->flags & IT_CUBEMAP ) && ( header->pixelWidth != header->pixelHeight ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Not square cubemap image: %s\n", pathname );
		goto error;
	}
	if( ( header->pixelDepth > 1 ) || ( header->numberOfArrayElements > 1 ) ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: 3D textures and texture arrays are not supported: %s\n", pathname );
		goto error;
	}
	if( header->numberOfFaces != numFaces ) {
		ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Bad number of cubemap faces: %s\n", pathname );
		goto error;
	}
	if( header->numberOfMipmapLevels < 1 ) {
		header->numberOfMipmapLevels = 1;
	}

	numMips = R_MipCount( header->pixelWidth, header->pixelHeight, image->minmipsize );

	data = buffer + sizeof( ktx_header_t ) + header->bytesOfKeyValueData;

	R_BindImage( image );

	if( header->type == 0 ) {
		int mips = numMips;
		int mip;
		int scaledWidth, scaledHeight;

		if( ( header->numberOfMipmapLevels == 1 ) && ( image->flags & IT_NOMIPMAP ) ) {
			mips = 1;
		} else if( header->numberOfMipmapLevels < mips ) {
			ri.Com_DPrintf( S_COLOR_YELLOW "R_LoadKTX: Compressed image has too few mip levels: %s\n", pathname );
			goto error;
		}

		mip = R_ScaledImageSize( header->pixelWidth, header->pixelHeight, &scaledWidth, &scaledHeight,
								 image->flags, mips, image->minmipsize, false );

		image->upload_width = scaledWidth;
		image->upload_height = scaledHeight;

		// If different compression formats are added, make this more general-purpose!

		if( !glConfig.ext.texture_compression || !( glConfig.ext.compressed_ETC1_RGB8_texture || glConfig.ext.ES3_compatibility ) || ( mip < 0 ) ) {
			int inSize = ( ( ALIGN( header->pixelWidth, 4 ) * ALIGN( header->pixelHeight, 4 ) ) >> 4 ) * 8;
			int outSize = ALIGN( header->pixelWidth * 3, 4 ) * header->pixelHeight;
			uint8_t *in = data + sizeof( int );
			uint8_t *decompressed[6];

			for( i = 0; i < numFaces; ++i ) {
				decompressed[i] = R_PrepareImageBuffer( ctx, TEXTURE_LOADING_BUF0 + i, outSize );
				DecompressETC1( in, header->pixelWidth, header->pixelHeight, decompressed[i], glConfig.ext.bgra ? true : false );
				in += inSize;
			}

			R_UploadMipmapped( ctx, decompressed, header->pixelWidth, header->pixelHeight, 1,
							   image->flags, image->minmipsize, &image->upload_width, &image->upload_height,
							   glConfig.ext.bgra ? GL_BGR_EXT : GL_RGB, GL_UNSIGNED_BYTE );
		} else {
			int target;
			int compressedFormat;
			size_t faceSize;

			if( glConfig.ext.ES3_compatibility && ( image->flags & IT_SRGB ) ) {
				compressedFormat = GL_COMPRESSED_SRGB8_ETC2;
			} else {
				compressedFormat = glConfig.ext.ES3_compatibility ? GL_COMPRESSED_RGB8_ETC2 : GL_ETC1_RGB8_OES;
			}

			R_TextureTarget( image->flags, &target );

			R_SetupTexParameters( image->flags, scaledWidth, scaledHeight, image->minmipsize );

			for( i = 0; i < mip; ++i ) {
				data += sizeof( int ) + numFaces * ( (
														 ALIGN( max( header->pixelWidth >> i, 1 ), 4 ) *
														 ALIGN( max( header->pixelHeight >> i, 1 ), 4 )
														 ) >> 4 ) * 8;
			}

			mips -= mip;
			for( i = 0; i < mips; ++i ) {
				faceSize = ( ( ALIGN( scaledWidth, 4 ) * ALIGN( scaledHeight, 4 ) ) >> 4 ) * 8;
				data += sizeof( int );
				for( j = 0; j < numFaces; ++j ) {
					qglCompressedTexImage2DARB( target + j, i, compressedFormat,
												scaledWidth, scaledHeight, 0, faceSize, data );
					data += faceSize;
				}
				scaledWidth >>= 1;
				scaledHeight >>= 1;
				if( !scaledWidth ) {
					scaledWidth = 1;
				}
				if( !scaledHeight ) {
					scaledHeight = 1;
				}
			}
		}

		image->samples = 3;
	} else {
		int mips = ( image->flags & IT_NOMIPMAP ) ? 1 : min( header->numberOfMipmapLevels, numMips );
		uint8_t *images[32 * 6];
		int mipWidth = header->pixelWidth, mipHeight = header->pixelHeight;
		size_t pixelSize = 2;
		size_t faceSize;

		switch( header->baseInternalFormat ) {
			case GL_RGBA:
				image->samples = 4;
				break;
			case GL_BGRA_EXT:
				image->samples = 4;
				image->flags |= IT_BGRA;
				break;
			case GL_RGB:
				image->samples = 3;
				break;
			case GL_BGR_EXT:
				image->samples = 3;
				image->flags |= IT_BGRA;
				break;
			case GL_LUMINANCE_ALPHA:
				image->samples = 2;
				break;
			case GL_LUMINANCE:
				image->samples = 1;
				break;
			case GL_ALPHA:
				image->samples = 1;
				image->flags |= IT_ALPHAMASK;
				break;
		}

		if( header->type == GL_UNSIGNED_BYTE ) {
			pixelSize = image->samples;
		}

		for( i = 0; i < mips; i++ ) {
			faceSize = ALIGN( max( header->pixelWidth >> i, 1 ) * pixelSize, 4 ) * max( header->pixelHeight >> i, 1 );
			data += sizeof( int );
			for( j = 0; j < numFaces; j++ )
				images[i * numFaces + j] = data + faceSize * j;
			data += faceSize * numFaces;
		}

		if( !glConfig.ext.bgra &&
			( ( header->baseInternalFormat == GL_BGR_EXT ) || ( header->baseInternalFormat == GL_BGRA_EXT ) ) ) {
			for( i = 0; i < mips; i++ ) {
				for( j = 0; j < numFaces; j++ ) {
					R_SwapBlueRed( images[i * numFaces + j], mipWidth, mipHeight,
								   ( header->baseInternalFormat == GL_BGR_EXT ) ? 3 : 4, 4 );
				}
				mipWidth >>= 1;
				mipHeight >>= 1;
				if( !mipWidth ) {
					mipWidth = 1;
				}
				if( !mipHeight ) {
					mipHeight = 1;
				}
			}
			header->baseInternalFormat = ( ( header->baseInternalFormat == GL_BGR_EXT ) ? GL_RGB : GL_RGBA );
		} else if( swapEndian && (
					   ( header->type == GL_UNSIGNED_SHORT_4_4_4_4 ) || ( header->type == GL_UNSIGNED_SHORT_5_5_5_1 ) ||
					   ( header->type == GL_UNSIGNED_SHORT_5_6_5 ) ) ) {
			for( i = 0; i < mips; i++ ) {
				for( j = 0; j < numFaces; j++ )
					R_EndianSwap16BitImage( ( unsigned short * )( images[i * numFaces + j] ), mipWidth, mipHeight );
				mipWidth >>= 1;
				mipHeight >>= 1;
				if( !mipWidth ) {
					mipWidth = 1;
				}
				if( !mipHeight ) {
					mipHeight = 1;
				}
			}
		}

		R_UploadMipmapped( ctx, images, header->pixelWidth, header->pixelHeight, mips, image->flags, image->minmipsize,
						   &image->upload_width, &image->upload_height, header->baseInternalFormat, header->type );
	}

	Q_strncpyz( image->extension, ".ktx", sizeof( image->extension ) );
	image->width = header->pixelWidth;
	image->height = header->pixelHeight;

	R_FreeFile( buffer );
	R_DeferDataSync();
	return true;

error: // must not be reached after actually starting uploading the texture
	R_FreeFile( buffer );
	return false;
}

/*
=========================================================

MIPTEX LOADING

=========================================================
*/

/*
* LoadMipTex
*/
static int LoadMipTex( uint8_t **pic, int width, int height, int flags ) {
	unsigned int i;
	unsigned int s, *trans;
	uint8_t *imgbuf, *data;
	const unsigned *table8to24 = R_LoadPalette( IT_MIPTEX );

	data = *pic;
	s = width * height;
	imgbuf = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, s * 4 );
	*pic = imgbuf;
	trans = ( unsigned int * )imgbuf;

	if( flags & IT_SKY ) {
		unsigned j;
		unsigned r, g, b;
		unsigned transpix;
		unsigned rgba;
		int halfwidth = width >> 1;

		// a sky texture is 256*128, with the right side being a masked overlay
		r = g = b = 0;
		for( i = 0; i < (unsigned)height; i++ ) {
			for( j = 0; j < (unsigned)halfwidth; j++ ) {
				uint8_t p = data[i * width + halfwidth + j];
				rgba = table8to24[p];
				trans[i * width + halfwidth + j] = rgba;
				r += COLOR_R( rgba );
				g += COLOR_G( rgba );
				b += COLOR_B( rgba );
			}
		}

		// make an average value for the back to avoid
		// a fringe on the top level
		transpix = COLOR_RGBA( r / ( halfwidth * height ), g / ( halfwidth * height ), b / ( halfwidth * height ), 0 );

		for( i = 0; i < (unsigned)height; i++ ) {
			for( j = 0; j < (unsigned)halfwidth; j++ ) {
				uint8_t p = data[i * width + j];
				trans[i * width + j] = p ? table8to24[p] : transpix;
			}
		}
		return 4;
	} else if( flags & IT_MIPTEX_FULLBRIGHT ) {
		// this is a fullbright mask, so make all non-fullbright
		// colors transparent
		for( i = 0; i < s; i++ ) {
			uint8_t p = data[i];
			if( p < 224 ) {
				trans[i] = 0; // transparent
			} else {
				trans[i] = table8to24[p];   // fullbright
			}
		}
		return 4;
	} else {
		// copy rgb components
		for( i = 0; i < s; i++ ) {
			uint8_t p = data[i];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[0];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[1];
			*imgbuf++ = ( (uint8_t *)&table8to24[p] )[2];
		}
		return 3;
	}
}

/*
* R_LoadImageFromDisk
*/
static bool R_LoadImageFromDisk( int ctx, image_t *image ) {
	int flags = image->flags;
	size_t len = strlen( image->name );
	char pathname[1024];
	size_t pathsize = sizeof( pathname );
	int width = 1, height = 1, samples = 1;
	bool loaded = false;

	if( len >= pathsize - 7 ) {
		return false;
	}

	memcpy( pathname, image->name, len + 1 );

	Q_strncatz( pathname, ".ktx", pathsize );
	if( R_LoadKTX( ctx, image, pathname ) ) {
		return true;
	}
	pathname[len] = 0;

	if( flags & IT_CUBEMAP ) {
		int i, j, k;
		uint8_t *pic[6];
		struct cubemapSufAndFlip {
			char *suf; int flags;
		} cubemapSides[2][6] = {
			{
				{ "px", 0 },
				{ "nx", 0 },
				{ "py", 0 },
				{ "ny", 0 },
				{ "pz", 0 },
				{ "nz", 0 }
			},
			{
				{ "rt", IT_FLIPX | IT_FLIPDIAGONAL }, 
				{ "lf", IT_FLIPY | IT_FLIPDIAGONAL },
				{ "ft", IT_FLIPX | IT_FLIPY },
				{ "bk", 0 },
				{ "up", IT_FLIPX | IT_FLIPDIAGONAL },
				{ "dn", IT_FLIPX | IT_FLIPDIAGONAL }
			}
		};
		int lastSize = 0;

		for( k = 0; k < 2; k++ ) {
			pathname[len] = '_';

			for( i = 0; i < 2; i++ ) {
				for( j = 0; j < 6; j++ ) {
					int cbflags = cubemapSides[i][j].flags;

					pathname[len + k + 0] = cubemapSides[i][j].suf[0];
					pathname[len + k + 1] = cubemapSides[i][j].suf[1];
					pathname[len + k + 2] = 0;

					Q_strncatz( pathname, ".tga", pathsize );
					samples = R_ReadImageFromDisk( ctx, pathname, pathsize,
												   &( pic[j] ), &width, &height, &flags, j );
					if( pic[j] ) {
						if( width != height ) {
							ri.Com_DPrintf( S_COLOR_YELLOW "Not square cubemap image %s\n", pathname );
							break;
						}
						if( !j ) {
							lastSize = width;
						} else if( lastSize != width ) {
							ri.Com_DPrintf( S_COLOR_YELLOW "Different cubemap image size: %s\n", pathname );
							break;
						}
						if( cbflags & ( IT_FLIPX | IT_FLIPY | IT_FLIPDIAGONAL ) ) {
							uint8_t *temp = R_PrepareImageBuffer( ctx,
								TEXTURE_FLIPPING_BUF0 + j, width * height * samples );
							R_FlipTexture( pic[j], temp, width, height, samples,
										   ( cbflags & IT_FLIPX ) ? true : false,
										   ( cbflags & IT_FLIPY ) ? true : false,
										   ( cbflags & IT_FLIPDIAGONAL ) ? true : false );
							pic[j] = temp;
						}
						continue;
					}
					break;
				}
				if( j == 6 ) {
					break;
				}
			}
			if( i != 2 ) {
				break;
			}
		}

		if( k != 2 ) {
			image->width = width;
			image->height = height;
			image->samples = samples;

			R_BindImage( image );

			R_Upload32( ctx, pic, 0, 0, 0, width, height, flags, image->minmipsize, &image->upload_width,
				&image->upload_height, samples, false, false );

			image->error = qglGetError();
			Q_strncpyz( image->extension, &pathname[len + k + 2], sizeof( image->extension ) );
			loaded = true;
		} else {
			ri.Com_DPrintf( S_COLOR_YELLOW "Missing image: %s\n", image->name );
		}
	} else {
		uint8_t *pic = NULL;

		Q_strncatz( pathname, ".tga", pathsize );
		samples = R_ReadImageFromDisk( ctx, pathname, pathsize, &pic, &width, &height, &flags, 0 );

		if( pic ) {
			image->width = width;
			image->height = height;
			image->samples = samples;

			R_BindImage( image );

			R_Upload32( ctx, &pic, 0, 0, 0, width, height, flags, image->minmipsize, &image->upload_width,
						&image->upload_height, samples, false, false );

			image->error = qglGetError();
			Q_strncpyz( image->extension, &pathname[len], sizeof( image->extension ) );
			loaded = true;
		} else {
			ri.Com_DPrintf( S_COLOR_YELLOW "Missing image: %s\n", image->name );
		}
	}

	if( loaded ) {
		// Update IT_LOADFLAGS that may be set by R_ReadImageFromDisk.
		image->flags = flags;
		R_DeferDataSync();
	}

	return loaded;
}

/*
* R_LinkPic
*/
static image_t *R_LinkPic( unsigned int hash ) {
	image_t *image;

	if( !r_free_images ) {
		return NULL;
	}

	ri.Mutex_Lock( r_imagesLock );

	hash = hash % IMAGES_HASH_SIZE;
	image = r_free_images;
	r_free_images = image->next;

	// link to the list of active images
	image->prev = &r_images_hash_headnode[hash];
	image->next = r_images_hash_headnode[hash].next;
	image->next->prev = image;
	image->prev->next = image;

	ri.Mutex_Unlock( r_imagesLock );

	return image;
}

/*
* R_UnlinkPic
*/
static void R_UnlinkPic( image_t *image ) {
	ri.Mutex_Lock( r_imagesLock );

	// remove from linked active list
	image->prev->next = image->next;
	image->next->prev = image->prev;

	// insert into linked free list
	image->next = r_free_images;
	r_free_images = image;

	ri.Mutex_Unlock( r_imagesLock );
}

/*
* R_AllocImage
*/
static image_t *R_CreateImage( const char *name, int width, int height, int layers, int flags, int minmipsize, int tags, int samples ) {
	image_t *image;
	int name_len = strlen( name );
	unsigned hash;

	hash = COM_SuperFastHash( ( const uint8_t *)name, name_len, name_len );

	image = R_LinkPic( hash );
	if( !image ) {
		ri.Com_Error( ERR_DROP, "R_LoadImage: r_numImages == MAX_GLIMAGES" );
		return NULL;
	}

	image->name = R_MallocExt( r_imagesPool, name_len + 1, 0, 1 );
	strcpy( image->name, name );
	image->width = width;
	image->height = height;
	image->layers = layers;
	image->flags = flags;
	image->minmipsize = minmipsize;
	image->samples = samples;
	image->fbo = 0;
	image->texnum = 0;
	image->registrationSequence = rsh.registrationSequence;
	image->tags = tags;
	image->loaded = true;
	image->missing = false;
	image->extension[0] = '\0';
	image->error = GL_NO_ERROR;

	R_AllocTextureNum( image );

	return image;
}

/*
* R_LoadImage
*/
image_t *R_LoadImage( const char *name, uint8_t **pic, int width, int height, int flags, int minmipsize, int tags, int samples ) {
	image_t *image;
	uint8_t **data = pic;

	if( !glConfig.sSRGB ) {
		flags &= ~IT_SRGB;
	}

	if( flags & IT_MIPTEX ) {
		samples = LoadMipTex( data, width, height, flags );
	}
	if( !( flags & IT_CUBEMAP ) && ( flags & ( IT_LEFTHALF | IT_RIGHTHALF ) ) ) {
		width /= 2;
	}

	image = R_CreateImage( name, width, height, 1, flags, minmipsize, tags, samples );

	R_BindImage( image );

	R_Upload32( QGL_CONTEXT_MAIN, pic, 0, 0, 0, width, height, flags, minmipsize,
				&image->upload_width, &image->upload_height, image->samples, false, false );

	image->error = qglGetError();
	return image;
}

/*
* R_Create3DImage
*/
image_t *R_Create3DImage( const char *name, int width, int height, int layers, int flags, int tags, int samples, bool array ) {
	image_t *image;
	int scaledWidth, scaledHeight;
	int target, comp, format, type;

	assert( array ? ( layers <= glConfig.maxTextureLayers ) : ( layers <= glConfig.maxTexture3DSize ) );

	flags |= ( array ? IT_ARRAY : IT_3D );
	if( !glConfig.sSRGB ) {
		flags &= ~IT_SRGB;
	}

	image = R_CreateImage( name, width, height, layers, flags, 1, tags, samples );
	R_BindImage( image );

	R_ScaledImageSize( width, height, &scaledWidth, &scaledHeight, flags, 1, 1, false );
	image->upload_width = scaledWidth;
	image->upload_height = scaledHeight;

	R_SetupTexParameters( flags, scaledWidth, scaledHeight, 1 );

	R_TextureTarget( flags, &target );
	R_TextureFormat( flags, samples, &comp, &format, &type );

	qglTexImage3DEXT( target, 0, comp, scaledWidth, scaledHeight, layers, 0, format, type, NULL );

	if( !( flags & IT_NOMIPMAP ) ) {
		int miplevel = 0;
		while( scaledWidth > 1 || scaledHeight > 1 ) {
			scaledWidth >>= 1;
			scaledHeight >>= 1;
			if( scaledWidth < 1 ) {
				scaledWidth = 1;
			}
			if( scaledHeight < 1 ) {
				scaledHeight = 1;
			}
			qglTexImage3DEXT( target, miplevel++, comp, scaledWidth, scaledHeight, layers, 0, format, type, NULL );
		}
	}

	return image;
}

/*
* R_FreeImage
*/
static void R_FreeImage( image_t *image ) {
	R_UnbindImage( image );

	R_FreeTextureNum( image );

	R_Free( image->name );

	image->name = NULL;
	image->texnum = 0;
	image->registrationSequence = 0;

	R_UnlinkPic( image );
}

/*
* R_ReplaceImage
*
* FIXME: not thread-safe!
*/
void R_ReplaceImage( image_t *image, uint8_t **pic, int width, int height, int flags, int minmipsize, int samples ) {
	assert( image );
	assert( image->texnum );

	if( !glConfig.sSRGB ) {
		flags &= ~IT_SRGB;
	}

	R_BindImage( image );

	if( image->width != width || image->height != height || image->samples != samples ) {
		R_Upload32( QGL_CONTEXT_MAIN, pic, 0, 0, 0, width, height, flags, minmipsize,
					&( image->upload_width ), &( image->upload_height ), samples, false, false );
	} else {
		R_Upload32( QGL_CONTEXT_MAIN, pic, 0, 0, 0, width, height, flags, minmipsize,
					&( image->upload_width ), &( image->upload_height ), samples, true, false );
	}

	image->error = qglGetError();
	image->flags = flags;
	image->width = width;
	image->height = height;
	image->layers = 1;
	image->minmipsize = minmipsize;
	image->samples = samples;
	image->registrationSequence = rsh.registrationSequence;

	if( !( image->flags & IT_NO_DATA_SYNC ) ) {
		R_DeferDataSync();
	}
}

/*
* R_ReplaceSubImage
*/
void R_ReplaceSubImage( image_t *image, int layer, int x, int y, uint8_t **pic, int width, int height ) {
	assert( image );
	assert( image->texnum );

	R_BindImage( image );

	R_Upload32( QGL_CONTEXT_MAIN, pic, layer, x, y, width, height, image->flags, image->minmipsize,
				NULL, NULL, image->samples, true, true );

	image->error = qglGetError();
	image->registrationSequence = rsh.registrationSequence;

	if( !( image->flags & IT_NO_DATA_SYNC ) ) {
		R_DeferDataSync();
	}
}

/*
* R_ReplaceImageLayer
*/
void R_ReplaceImageLayer( image_t *image, int layer, uint8_t **pic ) {
	assert( image );
	assert( image->texnum );

	R_BindImage( image );

	R_Upload32( QGL_CONTEXT_MAIN, pic, layer, 0, 0, image->width, image->height, image->flags, image->minmipsize,
				NULL, NULL, image->samples, true, false );

	image->error = qglGetError();
	image->registrationSequence = rsh.registrationSequence;

	if( !( image->flags & IT_NO_DATA_SYNC ) ) {
		R_DeferDataSync();
	}
}

/*
* R_FindImage
*
* Finds and loads the given image. IT_SYNC images are loaded synchronously.
* For synchronous missing images, NULL is returned.
*/
image_t *R_FindImage( const char *name, const char *suffix, int flags, int minmipsize, int tags ) {
	int i, lastDot, lastSlash, searchFlags;
	unsigned int len, key;
	image_t *image, *hnode;
	char *pathname;
	uint8_t *empty_data[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	bool loaded;

	if( !name || !name[0] ) {
		return NULL; //	ri.Com_Error (ERR_DROP, "R_FindImage: NULL name");

	}
	ENSUREBUFSIZE( imagePathBuf, strlen( name ) + ( suffix ? strlen( suffix ) : 0 ) + 5 );
	pathname = r_imagePathBuf;

	lastDot = -1;
	lastSlash = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i]; i++ ) {
		if( name[i] == '.' ) {
			lastDot = len;
		}
		if( name[i] == '\\' ) {
			pathname[len] = '/';
		} else {
			pathname[len] = tolower( name[i] );
		}
		if( pathname[len] == '/' ) {
			lastSlash = len;
		}
		len++;
	}

	if( len < 5 ) {
		return NULL;
	}

	// don't confuse paths such as /ui/xyz.cache/123 with file extensions
	if( lastDot < lastSlash ) {
		lastDot = -1;
	}

	if( lastDot != -1 ) {
		len = lastDot;
	}

	if( suffix ) {
		for( i = 0; suffix[i]; i++ )
			pathname[len++] = tolower( suffix[i] );
	}

	pathname[len] = 0;

	if( !glConfig.sSRGB ) {
		flags &= ~IT_SRGB;
	}
	searchFlags = flags & ~IT_LOADFLAGS;

	// look for it
	key = COM_SuperFastHash( ( const uint8_t *)pathname, len, len ) % IMAGES_HASH_SIZE;
	hnode = &r_images_hash_headnode[key];
	for( image = hnode->prev; image != hnode; image = image->prev ) {
		if( ( ( image->flags & ~IT_LOADFLAGS ) == searchFlags ) &&
			!strcmp( image->name, pathname ) && ( image->minmipsize == minmipsize ) ) {
			R_TouchImage( image, tags );
			return image;
		}
	}

	pathname[len] = 0;

	//
	// load the pic from disk
	//
	image = R_LoadImage( pathname, empty_data, 1, 1, flags, minmipsize, tags, 1 );

	if( !( image->flags & IT_SYNC ) ) {
		if( R_LoadAsyncImageFromDisk( image ) ) {
			return image;
		}
	}

	loaded = R_LoadImageFromDisk( QGL_CONTEXT_MAIN, image );
	R_UnbindImage( image );

	if( !loaded ) {
		R_FreeImage( image );
		image = NULL;
	} else {
		// Make sure the image is updated on all contexts.
		if( glConfig.multithreading ) {
			qglFinish();
		}
		image->loaded = true;
	}

	return image;
}

/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

/*
* R_ScreenShot
*/
void R_ScreenShot( const char *filename, int x, int y, int width, int height, int quality,
				   bool flipx, bool flipy, bool flipdiagonal, bool silent ) {
	size_t size, buf_size;
	uint8_t *buffer, *flipped, *rgb, *rgba;
	r_imginfo_t imginfo;
	const char *extension;

	if( !R_IsRenderingToScreen() ) {
		return;
	}

	extension = COM_FileExtension( filename );
	if( !extension ) {
		Com_Printf( "R_ScreenShot: Invalid filename\n" );
		return;
	}

	size = width * height * 3;
	buf_size = width * height * 4 * 2; // reserve extra space for flipped images, hence x2
	if( buf_size > r_screenShotBufferSize ) {
		if( r_screenShotBuffer ) {
			R_Free( r_screenShotBuffer );
		}
		r_screenShotBuffer = R_MallocExt( r_imagesPool, buf_size, 0, 1 );
		r_screenShotBufferSize = buf_size;
	}

	buffer = r_screenShotBuffer;
	if( flipx || flipy || flipdiagonal ) {
		flipped = buffer + size;
	} else {
		flipped = NULL;
	}

	imginfo.width = width;
	imginfo.height = height;
	imginfo.samples = 3;
	imginfo.pixels = flipped ? flipped : buffer;
	imginfo.comp = Q_stricmp( extension, ".jpg" ) ? IMGCOMP_BGR : IMGCOMP_RGB;

	qglReadPixels( 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer );

	rgb = rgba = buffer;
	if( imginfo.comp == IMGCOMP_BGR ) {
		while( ( size_t )( rgb - buffer ) < size ) {
			int b = rgba[0]; // the first 3 pixels of rgb and rgba are aliased
			rgb[0] = rgba[2];
			rgb[1] = rgba[1];
			rgb[2] = b;
			rgb += 3;
			rgba += 4;
		}
	} else {
		while( ( size_t )( rgb - buffer ) < size ) {
			*( rgb++ ) = *( rgba++ );
			*( rgb++ ) = *( rgba++ );
			*( rgb++ ) = *( rgba++ );
			rgba++;
		}
	}

	if( flipped ) {
		R_FlipTexture( buffer, flipped, width, height, 3,
					   flipx, flipy, flipdiagonal );
	}

	if( !Q_stricmp( extension, ".jpg" ) ) {
		if( WriteJPG( filename, &imginfo, quality ) && !silent ) {
			Com_Printf( "Wrote %s\n", filename );
		}

	} else {
		if( WriteTGA( filename, &imginfo, 100 ) && !silent ) {
			Com_Printf( "Wrote %s\n", filename );
		}
	}
}

//=======================================================

/*
* R_InitNoTexture
*/
static void R_InitNoTexture( int *w, int *h, int *flags, int *samples ) {
	int x, y;
	uint8_t *data;
	uint8_t dottexture[8][8] =
	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 1, 1, 1, 1, 0, 0, 0 },
		{ 0, 0, 1, 1, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
	};

	//
	// also use this for bad textures, but without alpha
	//
	*w = *h = 8;
	*flags = IT_SRGB;
	*samples = 3;

	// ch : check samples
	data = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 8 * 8 * 3 );
	for( x = 0; x < 8; x++ ) {
		for( y = 0; y < 8; y++ ) {
			data[( y * 8 + x ) * 3 + 0] = dottexture[x & 3][y & 3] * 127;
			data[( y * 8 + x ) * 3 + 1] = dottexture[x & 3][y & 3] * 127;
			data[( y * 8 + x ) * 3 + 2] = dottexture[x & 3][y & 3] * 127;
		}
	}
}

/*
* R_InitSolidColorTexture
*/
static uint8_t *R_InitSolidColorTexture( int *w, int *h, int *flags, int *samples, int color, bool srgb ) {
	uint8_t *data;

	//
	// solid color texture
	//
	*w = *h = 1;
	*flags = IT_NOPICMIP | IT_NOCOMPRESS;
	*samples = 3;
	if( srgb ) {
		*flags |= IT_SRGB;
	}

	// ch : check samples
	data = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 1 * 1 * 3 );
	data[0] = data[1] = data[2] = color;
	return data;
}

/*
* R_InitParticleTexture
*/
static void R_InitParticleTexture( int *w, int *h, int *flags, int *samples ) {
	int x, y;
	int dx2, dy, d;
	float dd2;
	uint8_t *data;

	//
	// particle texture
	//
	*w = *h = 16;
	*flags = IT_NOPICMIP | IT_NOMIPMAP | IT_SRGB;
	*samples = 4;

	data = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 16 * 16 * 4 );
	for( x = 0; x < 16; x++ ) {
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ ) {
			dy = y - 8;
			dd2 = dx2 + dy * dy;
			d = 255 - 35 * sqrt( dd2 );
			data[( y * 16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}
}

/*
* R_InitWhiteTexture
*/
static void R_InitWhiteTexture( int *w, int *h, int *flags, int *samples ) {
	R_InitSolidColorTexture( w, h, flags, samples, 255, true );
}

/*
* R_InitWhiteCubemapTexture
*/
static void R_InitWhiteCubemapTexture( int *w, int *h, int *flags, int *samples ) {
	int i;

	*w = *h = 1;
	*flags = IT_NOPICMIP | IT_NOCOMPRESS | IT_CUBEMAP | IT_SRGB;
	*samples = 3;

	for( i = 0; i < 6; i++ ) {
		uint8_t *data;
		data = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0 + i, 1 * 1 * 3 );
		data[0] = data[1] = data[2] = 255;
	}
}

/*
* R_InitBlackTexture
*/
static void R_InitBlackTexture( int *w, int *h, int *flags, int *samples ) {
	R_InitSolidColorTexture( w, h, flags, samples, 0, true );
}

/*
* R_InitGreyTexture
*/
static void R_InitGreyTexture( int *w, int *h, int *flags, int *samples ) {
	R_InitSolidColorTexture( w, h, flags, samples, 127, true );
}

/*
* R_InitBlankBumpTexture
*/
static void R_InitBlankBumpTexture( int *w, int *h, int *flags, int *samples ) {
	uint8_t *data = R_InitSolidColorTexture( w, h, flags, samples, 128, false );

/*
    data[0] = 128;	// normal X
    data[1] = 128;	// normal Y
*/
	data[2] = 255;  // normal Z
	data[3] = 128;  // height
}

/*
* R_InitCoronaTexture
*/
static void R_InitCoronaTexture( int *w, int *h, int *flags, int *samples ) {
	int x, y, a;
	float dx, dy;
	uint8_t *data;

	//
	// light corona texture
	//
	*w = *h = 32;
	*flags = IT_SPECIAL | IT_SRGB;
	*samples = 4;

	data = R_PrepareImageBuffer( QGL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 32 * 32 * 4 );
	for( y = 0; y < 32; y++ ) {
		dy = ( y - 15.5f ) * ( 1.0f / 16.0f );
		for( x = 0; x < 32; x++ ) {
			dx = ( x - 15.5f ) * ( 1.0f / 16.0f );
			a = (int)( ( ( 1.0f / ( dx * dx + dy * dy + 0.2f ) ) - ( 1.0f / ( 1.0f + 0.2 ) ) ) * 32.0f / ( 1.0f / ( 1.0f + 0.2 ) ) );
			clamp( a, 0, 255 );
			data[( y * 32 + x ) * 4 + 0] = data[( y * 32 + x ) * 4 + 1] = data[( y * 32 + x ) * 4 + 2] = a;
		}
	}
}

/*
* R_GetRenderBufferSize
*/
void R_GetRenderBufferSize( const int inWidth, const int inHeight,
							const int inLimit, const int flags, int *outWidth, int *outHeight ) {
	int limit;
	int width_, height_;
	bool npot;

	// limit the texture size to either screen resolution in case we can't use FBO
	// or hardware limits and ensure it's a POW2-texture if we don't support such textures
	limit = glConfig.maxRenderbufferSize;
	if( inLimit ) {
		limit = min( limit, inLimit );
	}
	if( limit < 1 ) {
		limit = 1;
	}
	width_ = height_ = limit;

	npot = glConfig.ext.texture_non_power_of_two;
#ifdef GL_ES_VERSION_2_0
	npot = npot || ( ( flags & ( IT_CLAMP | IT_NOMIPMAP ) ) == ( IT_CLAMP | IT_NOMIPMAP ) );
#endif
	if( npot ) {
		width_ = min( inWidth, limit );
		height_ = min( inHeight, limit );
	} else {
		int d;

		// calculate the upper bound and make sure it's not a pow of 2
		d = min( limit, inWidth );
		if( ( d & ( d - 1 ) ) == 0 ) {
			d--;
		}
		for( width_ = 2; width_ <= d; width_ <<= 1 ) ;

		d = min( limit, inHeight );
		if( ( d & ( d - 1 ) ) == 0 ) {
			d--;
		}
		for( height_ = 2; height_ <= d; height_ <<= 1 ) ;

		if( inLimit ) {
			while( width_ > inLimit || height_ > inLimit ) {
				width_ >>= 1;
				height_ >>= 1;
			}
		}
	}

	*outWidth = width_;
	*outHeight = height_;
}

/*
* R_InitViewportTexture
*/
void R_InitViewportTexture( image_t **texture, const char *name, int id,
							int viewportWidth, int viewportHeight, int size, int flags, int tags, int samples ) {
	int width, height;
	image_t *t;

	R_GetRenderBufferSize( viewportWidth, viewportHeight, size, flags, &width, &height );

	// create a new texture or update the old one
	if( !( *texture ) || ( *texture )->width != width || ( *texture )->height != height ) {
		uint8_t *data = NULL;

		if( !*texture ) {
			char uploadName[128];

			Q_snprintfz( uploadName, sizeof( uploadName ), "***%s_%i***", name, id );
			t = *texture = R_LoadImage( uploadName, &data, width, height, flags, 1, tags, samples );
		} else {
			t = *texture;
			t->width = width;
			t->height = height;

			R_BindImage( t );

			R_Upload32( QGL_CONTEXT_MAIN, &data, 0, 0, 0, width, height, flags, 1,
						&t->upload_width, &t->upload_height, t->samples, false, false );
			t->error = qglGetError();
		}

		// update FBO, if attached
		if( t->fbo ) {
			RFB_UnregisterObject( t->fbo );
			t->fbo = 0;
		}

		if( t->error != GL_NO_ERROR ) {
			return;
		}

		if( t->flags & IT_FRAMEBUFFER ) {
			t->fbo = RFB_RegisterObject( t->upload_width, t->upload_height, 
				( tags & IMAGE_TAG_BUILTIN ) != 0,
				( flags & IT_DEPTHRB ) != 0, 
				( flags & IT_STENCIL ) != 0, 
				false, 0, false, 
				( flags & IT_SRGB ) != 0
			);
			RFB_AttachTextureToObject( t->fbo, ( t->flags & IT_DEPTH ) != 0, 0, t );
		}
	}
}

/*
* R_GetPortalTextureId
*/
static int R_GetPortalTextureId( const int viewportWidth, const int viewportHeight,
								 const int flags, unsigned frameNum ) {
	int i;
	int best = -1;
	int realwidth, realheight;
	int realflags = IT_SPECIAL | IT_FRAMEBUFFER | IT_DEPTHRB | flags;
	image_t *image;

	R_GetRenderBufferSize( viewportWidth, viewportHeight, r_portalmaps_maxtexsize->integer,
						   flags, &realwidth, &realheight );

	for( i = 0; i < MAX_PORTAL_TEXTURES; i++ ) {
		image = rsh.portalTextures[i];
		if( !image ) {
			return i;
		}

		if( image->framenum == frameNum ) {
			// the texture is used in the current scene
			continue;
		}

		if( image->width == realwidth &&
			image->height == realheight &&
			image->flags == realflags &&
		    image->error == GL_NO_ERROR ) {
			// 100% match
			return i;
		}

		if( best < 0 ) {
			// in case we don't get a 100% matching texture later,
			// reuse this one
			best = i;
		}
	}

	return best;
}

/*
* R_GetPortalTexture
*/
image_t *R_GetPortalTexture( int viewportWidth, int viewportHeight,
							 int flags, unsigned frameNum ) {
	int id;

	if( glConfig.stencilBits ) {
		flags |= IT_STENCIL;
	}

	id = R_GetPortalTextureId( viewportWidth, viewportHeight, flags, frameNum );
	if( id < 0 || id >= MAX_PORTAL_TEXTURES ) {
		return NULL;
	}

	R_InitViewportTexture( &rsh.portalTextures[id], "r_portaltexture", id,
						   viewportWidth, viewportHeight, r_portalmaps_maxtexsize->integer,
						   IT_SPECIAL | IT_FRAMEBUFFER | IT_DEPTHRB | flags, IMAGE_TAG_GENERIC,
						   glConfig.forceRGBAFramebuffers ? 4 : 3 );

	if( rsh.portalTextures[id] ) {
		rsh.portalTextures[id]->framenum = frameNum;
	}

	return rsh.portalTextures[id];
}

/*
* R_GetShadowmapAtlasTexture
*/
image_t *R_GetShadowmapAtlasTexture( void ) {
	int flags;
	int samples;
	int size;
	int err = GL_NO_ERROR;

	size = max( r_shadows_texturesize->integer, SHADOWMAP_MIN_ATLAS_SIZE );

	if( glConfig.ext.shadow ) {
		// render to depthbuffer, GL_ARB_shadow path
		flags = IT_DEPTH;
		samples = 1;
	} else {
		flags = IT_NOFILTERING;
		samples = 3;
	}

	// FIXME: should we generalize this loop for other textures as well?
	while( size >= SHADOWMAP_MIN_ATLAS_SIZE ) {
		image_t *t;
		R_InitViewportTexture( &rsh.shadowmapAtlasTexture, "r_shadowmap", 0,
			size, size, size,
			IT_SPECIAL | IT_FRAMEBUFFER | IT_DEPTHCOMPARE | flags, IMAGE_TAG_BUILTIN, samples );

		t = rsh.shadowmapAtlasTexture;
		if( t && t->error == GL_OUT_OF_MEMORY ) {
			size >>= 1;
			continue;
		}
		break;
	}

	return rsh.shadowmapAtlasTexture;
}

/*
* R_InitStretchRawImages
*/
static void R_InitStretchRawImages( void ) {
	rsh.rawTexture = R_CreateImage( "*** raw ***", 0, 0, 1, IT_SPECIAL | IT_SRGB, 1, IMAGE_TAG_BUILTIN, 3 );
	rsh.rawYUVTextures[0] = R_CreateImage( "*** rawyuv0 ***", 0, 0, 1, IT_SPECIAL, 1, IMAGE_TAG_BUILTIN, 1 );
	rsh.rawYUVTextures[1] = R_CreateImage( "*** rawyuv1 ***", 0, 0, 1, IT_SPECIAL, 1, IMAGE_TAG_BUILTIN, 1 );
	rsh.rawYUVTextures[2] = R_CreateImage( "*** rawyuv2 ***", 0, 0, 1, IT_SPECIAL, 1, IMAGE_TAG_BUILTIN, 1 );
}

/*
* R_InitScreenImagePair
*/
static void R_InitScreenImagePair( const char *name, image_t **color, image_t **depth, 
	int orFlags, int lod, int andFlags ) {
	char tn[128];
	int flags, colorFlags, depthFlags;
	int width = glConfig.width >> lod;
	int height = glConfig.height >> lod;

	assert( !depth || glConfig.ext.depth_texture );

	if( !glConfig.stencilBits ) {
		andFlags &= ~IT_STENCIL;
		orFlags &= ~IT_STENCIL;
	}
	if( width < 1 ) {
		width = 1;
	}
	if( height < 1 ) {
		height = 1;
	}

	flags = IT_SPECIAL;
	flags |= orFlags;

	colorFlags = flags | IT_FRAMEBUFFER;
	depthFlags = flags | ( IT_DEPTH | IT_NOFILTERING );
	if( !depth ) {
		colorFlags |= IT_DEPTHRB;
	}
	if( flags & IT_STENCIL ) {
		if( depth ) {
			depthFlags |= IT_STENCIL;
		} else {
			colorFlags |= IT_STENCIL;
		}
	}
	if( flags & IT_FLOAT ) {
		colorFlags |= IT_FLOAT;
	}
	colorFlags &= andFlags;

	if( color ) {
		R_InitViewportTexture( color, name,
							   0, width, height, 0, colorFlags, IMAGE_TAG_BUILTIN,
							   glConfig.forceRGBAFramebuffers ? 4 : 3 );
	}
	if( depth && color && *color ) {
		R_InitViewportTexture( depth, va_r( tn, sizeof( tn ), "%s_depth", name ),
							   0, width, height, 0, depthFlags, IMAGE_TAG_BUILTIN, 1 );

		if( colorFlags & IT_FRAMEBUFFER ) {
			RFB_AttachTextureToObject( ( *color )->fbo, true, 0, *depth );
		}
	}
}

/*
* R_InitBuiltinScreenImageSet
*
 * Screen textures may only be used in or referenced from the rendering context/thread.
*/
static void R_InitBuiltinScreenImageSet( refScreenTexSet_t *st, int flags ) {
	int i, j;
	char name[128];
	bool useFloat;
	const char *postfix;
	
	flags &= (IT_FLOAT|IT_SRGB);
	useFloat = (flags & IT_FLOAT) != 0;
	postfix = useFloat ? "16f" : "";

	if( glConfig.ext.depth_texture && glConfig.ext.fragment_precision_high && glConfig.ext.framebuffer_blit ) {
		Q_snprintfz( name, sizeof( name ), "r_screenTex%s", postfix );
		R_InitScreenImagePair( name, &st->screenTex, &st->screenDepthTex, IT_STENCIL|flags, 0, ~0 );

		// stencil is required in the copy for depth/stencil formats to match when blitting.
		Q_snprintfz( name, sizeof( name ), "r_screenTexCopy%s", postfix );
		R_InitScreenImagePair( name, &st->screenTexCopy, &st->screenDepthTexCopy, IT_STENCIL|flags, 0, ~0 );
	}

	for( j = 0; j < 2; j++ ) {
		Q_snprintfz( name, sizeof( name ), "rsh.screenPP%sCopy%i", postfix, j );
		R_InitScreenImagePair( name, &st->screenPPCopies[j], NULL, flags, 0, ~0 );
	}

	if( !useFloat && glConfig.ext.draw_buffers ) {
		Q_snprintfz( name, sizeof( name ), "rsh.screenTexOverbright%s", postfix );
		R_InitScreenImagePair( name, &st->screenOverbrightTex, NULL, 0, 0, ~IT_FRAMEBUFFER );

		if( st->screenOverbrightTex ) {
			for( i = 0; i < NUM_BLOOM_LODS; i++ ) {
				for( j = 0; j < 2; j++ ) {
					Q_snprintfz( name, sizeof( name ), "rsh.screenTexBloomLod%s_%i_%i", postfix, i, j );
					R_InitScreenImagePair( name, &st->screenBloomLodTex[i][j], NULL, 0, i + 1, ~0 );
				}
			}
		}
	}
}

/*
* R_ReleaseBuiltinScreenImageSet
*/
static void R_ReleaseBuiltinScreenImageSet( refScreenTexSet_t *st ) {
	int i, j;

	if( st->screenTex ) {
		R_FreeImage( st->screenTex );
		st->screenTex = NULL;
	}
	if( st->screenDepthTex ) {
		R_FreeImage( st->screenDepthTex );
		st->screenDepthTex = NULL;
	}

	if( st->screenTexCopy ) {
		R_FreeImage( st->screenTexCopy );
		st->screenTexCopy = NULL;
	}
	if( st->screenDepthTexCopy ) {
		R_FreeImage( st->screenDepthTexCopy );
		st->screenDepthTexCopy = NULL;
	}

	for( j = 0; j < 2; j++ ) {
		if( st->screenPPCopies[j] ) {
			R_FreeImage( st->screenPPCopies[j] );
			st->screenPPCopies[j] = NULL;
		}
	}

	if( st->screenOverbrightTex ) {
		R_FreeImage( st->screenOverbrightTex );
		st->screenOverbrightTex = NULL;
	}

	for( i = 0; i < NUM_BLOOM_LODS; i++ ) {
		for( j = 0; j < 2; j++ ) {
			if( st->screenBloomLodTex[j][j] ) {
				R_FreeImage( st->screenBloomLodTex[i][j] );
				st->screenBloomLodTex[i][j] = NULL;
			}
		}
	}
}

/*
* R_RegisterMultisampleTarget
*/
int R_RegisterMultisampleTarget( refScreenTexSet_t *st, int samples, bool useFloat, bool sRGB ) {
	int width, height;

	if( samples <= 0 ) {
		return 0;
	}

	if( !st->multisampleTarget || RFB_GetSamples( st->multisampleTarget ) != samples ) {
		R_GetRenderBufferSize( glConfig.width, glConfig.height, 0, IT_SPECIAL, &width, &height );

		if( st->multisampleTarget ) {
			RFB_UnregisterObject( st->multisampleTarget );
		}

		st->multisampleTarget = RFB_RegisterObject( width, height, true, true, 
			glConfig.stencilBits != 0, true, samples, useFloat, sRGB );
	}

	return st->multisampleTarget;
}

/*
* R_InitBuiltinScreenImages
*/
void R_InitBuiltinScreenImages( void ) {
	R_InitBuiltinScreenImageSet( &rsh.st, 0 );

	if( glConfig.ext.texture_float ) {
		R_InitBuiltinScreenImageSet( &rsh.stf, IT_FLOAT );
	}

	R_InitScreenImagePair( "r_2Dtex", &rsh.st2D.screenTex, &rsh.st2D.screenDepthTex, IT_SRGB, 0, ~0 );
}

/*
* R_ReleaseBuiltinScreenImages
*/
void R_ReleaseBuiltinScreenImages( void ) {
	R_ReleaseBuiltinScreenImageSet( &rsh.st );
	R_ReleaseBuiltinScreenImageSet( &rsh.stf );
	R_ReleaseBuiltinScreenImageSet( &rsh.st2D );
}

/*
* R_InitBuiltinImages
*/
static void R_InitBuiltinImages( void ) {
	int w, h, flags, samples;
	image_t *image;
	const struct {
		char *name;
		image_t **image;
		void ( *init )( int *w, int *h, int *flags, int *samples );
	}
	textures[] =
	{
		{ "***r_notexture***", &rsh.noTexture, &R_InitNoTexture },
		{ "***r_whitetexture***", &rsh.whiteTexture, &R_InitWhiteTexture },
		{ "***r_whitecubemaptexture***", &rsh.whiteCubemapTexture, &R_InitWhiteCubemapTexture },
		{ "***r_blacktexture***", &rsh.blackTexture, &R_InitBlackTexture },
		{ "***r_greytexture***", &rsh.greyTexture, &R_InitGreyTexture },
		{ "***r_blankbumptexture***", &rsh.blankBumpTexture, &R_InitBlankBumpTexture },
		{ "***r_particletexture***", &rsh.particleTexture, &R_InitParticleTexture },
		{ "***r_coronatexture***", &rsh.coronaTexture, &R_InitCoronaTexture },
		{ NULL, NULL, NULL }
	};
	size_t i, num_builtin_textures = sizeof( textures ) / sizeof( textures[0] ) - 1;

	for( i = 0; i < num_builtin_textures; i++ ) {
		textures[i].init( &w, &h, &flags, &samples );

		image = R_LoadImage( textures[i].name, r_imageBuffers[QGL_CONTEXT_MAIN], w, h, flags, 1, IMAGE_TAG_BUILTIN, samples );

		if( textures[i].image ) {
			*( textures[i].image ) = image;
		}
	}
}

/*
* R_ReleaseBuiltinImages
*/
static void R_ReleaseBuiltinImages( void ) {
	rsh.rawTexture = NULL;
	rsh.rawYUVTextures[0] = rsh.rawYUVTextures[1] = rsh.rawYUVTextures[2] = NULL;
	rsh.noTexture = NULL;
	rsh.whiteTexture = rsh.blackTexture = rsh.greyTexture = NULL;
	rsh.whiteCubemapTexture = NULL;
	rsh.blankBumpTexture = NULL;
	rsh.particleTexture = NULL;
	rsh.coronaTexture = NULL;
	rsh.shadowmapAtlasTexture = NULL;
}

//=======================================================

/*
* R_InitImages
*/
void R_InitImages( void ) {
	int i;

	if( r_imagesPool ) {
		return;
	}

	R_Imagelib_Init();

	r_imagesPool = R_AllocPool( r_mempool, "Images" );
	r_imagesLock = ri.Mutex_Create();

	r_unpackAlignment[QGL_CONTEXT_MAIN] = 4;
	qglPixelStorei( GL_PACK_ALIGNMENT, 1 );

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	r_8to24table[0] = r_8to24table[1] = NULL;

	memset( r_images, 0, sizeof( r_images ) );

	// link images
	r_free_images = r_images;
	for( i = 0; i < IMAGES_HASH_SIZE; i++ ) {
		r_images_hash_headnode[i].prev = &r_images_hash_headnode[i];
		r_images_hash_headnode[i].next = &r_images_hash_headnode[i];
	}
	for( i = 0; i < MAX_GLIMAGES - 1; i++ ) {
		r_images[i].next = &r_images[i + 1];
	}

	for( i = 0; i < NUM_LOADER_THREADS; i++ ) {
		R_InitImageLoader( i );
	}

	R_InitStretchRawImages();
	R_InitBuiltinImages();
}

/*
* R_TouchImage
*/
void R_TouchImage( image_t *image, int tags ) {
	if( !image ) {
		return;
	}

	image->tags |= tags;

	if( image->registrationSequence == rsh.registrationSequence ) {
		return;
	}

	image->registrationSequence = rsh.registrationSequence;
	if( image->fbo ) {
		RFB_TouchObject( image->fbo );
	}
}

/*
* R_FreeUnusedImagesByTags
*/
void R_FreeUnusedImagesByTags( int tags ) {
	int i;
	image_t *image;
	int keeptags = ~tags;

	for( i = 0, image = r_images; i < MAX_GLIMAGES; i++, image++ ) {
		if( !image->name ) {
			// free image
			continue;
		}
		if( image->registrationSequence == rsh.registrationSequence ) {
			// we need this image
			continue;
		}

		image->tags &= keeptags;
		if( image->tags ) {
			// still used for a different purpose
			continue;
		}

		R_FreeImage( image );
	}
}

/*
* R_FreeUnusedImages
*/
void R_FreeUnusedImages( void ) {
	R_FreeUnusedImagesByTags( ~IMAGE_TAG_BUILTIN );

	R_FinishLoadingImages();

	memset( rsh.portalTextures, 0, sizeof( image_t * ) * MAX_PORTAL_TEXTURES );
}

/*
* R_ShutdownImages
*/
void R_ShutdownImages( void ) {
	int i;
	image_t *image;

	if( !r_imagesPool ) {
		return;
	}

	for( i = 0; i < NUM_LOADER_THREADS; i++ ) {
		R_ShutdownImageLoader( i );
	}

	R_ReleaseBuiltinImages();

	for( i = 0, image = r_images; i < MAX_GLIMAGES; i++, image++ ) {
		if( !image->name ) {
			// free texture
			continue;
		}
		R_FreeImage( image );
	}

	R_FreeImageBuffers();

	if( r_imagePathBuf ) {
		R_Free( r_imagePathBuf );
	}
	if( r_imagePathBuf2 ) {
		R_Free( r_imagePathBuf2 );
	}

	if( r_8to24table[0] ) {
		R_Free( r_8to24table[0] );
	}
	if( r_8to24table[1] ) {
		R_Free( r_8to24table[1] );
	}
	r_8to24table[0] = r_8to24table[1] = NULL;

	ri.Mutex_Destroy( &r_imagesLock );

	R_FreePool( &r_imagesPool );

	r_screenShotBuffer = NULL;
	r_screenShotBufferSize = 0;

	memset( rsh.portalTextures, 0, sizeof( rsh.portalTextures ) );

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	R_Imagelib_Shutdown();
}

// ============================================================================

enum {
	CMD_LOADER_INIT,
	CMD_LOADER_SHUTDOWN,
	CMD_LOADER_LOAD_PIC,
	CMD_LOADER_DATA_SYNC,

	NUM_LOADER_CMDS
};

typedef struct {
	int id;
	int self;
} loaderInitCmd_t;

typedef struct {
	int id;
	int self;
	int pic;
} loaderPicCmd_t;

typedef unsigned (*queueCmdHandler_t)( const void * );

static qbufPipe_t *loader_queue[NUM_LOADER_THREADS] = { NULL };
static qthread_t *loader_thread[NUM_LOADER_THREADS] = { NULL };

static void *loader_gl_context[NUM_LOADER_THREADS] = { NULL };
static void *loader_gl_surface[NUM_LOADER_THREADS] = { NULL };

static void *R_ImageLoaderThreadProc( void *param );

/*
* R_IssueInitLoaderCmd
*/
static void R_IssueInitLoaderCmd( int id ) {
	loaderInitCmd_t cmd;
	cmd.id = CMD_LOADER_INIT;
	cmd.self = id;
	ri.BufPipe_WriteCmd( loader_queue[id], &cmd, sizeof( cmd ) );
}

/*
* R_IssueShutdownLoaderCmd
*/
static void R_IssueShutdownLoaderCmd( int id ) {
	int cmd;
	cmd = CMD_LOADER_SHUTDOWN;
	ri.BufPipe_WriteCmd( loader_queue[id], &cmd, sizeof( cmd ) );
}

/*
* R_IssueLoadPicLoaderCmd
*/
static void R_IssueLoadPicLoaderCmd( int id, int pic ) {
	loaderPicCmd_t cmd;
	cmd.id = CMD_LOADER_LOAD_PIC;
	cmd.self = id;
	cmd.pic = pic;
	ri.BufPipe_WriteCmd( loader_queue[id], &cmd, sizeof( cmd ) );
}

/*
* R_IssueDataSyncLoaderCmd
*/
static void R_IssueDataSyncLoaderCmd( int id ) {
	int cmd;
	cmd = CMD_LOADER_DATA_SYNC;
	ri.BufPipe_WriteCmd( loader_queue[id], &cmd, sizeof( cmd ) );
}

/*
* R_InitImageLoader
*/
static void R_InitImageLoader( int id ) {
	if( !glConfig.multithreading ) {
		loader_gl_context[id] = NULL;
		loader_gl_surface[id] = NULL;
		return;
	}

	if( !GLimp_SharedContext_Create( &loader_gl_context[id], &loader_gl_surface[id] ) ) {
		return;
	}

	loader_queue[id] = ri.BufPipe_Create( 0x40000, 1 );
	loader_thread[id] = ri.Thread_Create( R_ImageLoaderThreadProc, loader_queue[id] );

	R_IssueInitLoaderCmd( id );

	// wait for the thread to complete context setup
	ri.BufPipe_Finish( loader_queue[id] );
}

/*
* R_FinishLoadingImages
*/
void R_FinishLoadingImages( void ) {
	int i;

	for( i = 0; i < NUM_LOADER_THREADS; i++ ) {
		if( loader_gl_context[i] ) {
			R_IssueDataSyncLoaderCmd( i );
		}
	}

	for( i = 0; i < NUM_LOADER_THREADS; i++ ) {
		if( loader_gl_context[i] ) {
			ri.BufPipe_Finish( loader_queue[i] );
		}
	}
}

/*
* R_LoadAsyncImageFromDisk
*/
static bool R_LoadAsyncImageFromDisk( image_t *image ) {
	int pic;
	int id;

	if( loader_gl_context[0] == NULL ) {
		return false;
	}

	pic = image - r_images;
	id = pic % NUM_LOADER_THREADS;
	if( loader_gl_context[id] == NULL ) {
		id = 0;
	}

	image->loaded = false;
	image->missing = false;

	// Unbind and finish so that the image resource becomes available in the loader's context.
	// Not doing finish (or only doing flush instead) causes missing textures on Nvidia and possibly other GPUs,
	// since the loader thread is woken up pretty much instantly, and the GL calls that initialize the texture
	// may still be processed or only queued in the main thread while the loader is trying to load the image.
	R_UnbindImage( image );
	qglFinish();

	R_IssueLoadPicLoaderCmd( id, pic );
	return true;
}

/*
* R_ShutdownImageLoader
*/
static void R_ShutdownImageLoader( int id ) {
	void *context = loader_gl_context[id];
	void *surface = loader_gl_surface[id];

	loader_gl_context[id] = NULL;
	loader_gl_surface[id] = NULL;
	if( !context ) {
		return;
	}

	R_IssueShutdownLoaderCmd( id );

	ri.BufPipe_Finish( loader_queue[id] );

	ri.Thread_Join( loader_thread[id] );
	loader_thread[id] = NULL;

	ri.BufPipe_Destroy( &loader_queue[id] );

	GLimp_SharedContext_Destroy( context, surface );
}

//

/*
* R_HandleInitLoaderCmd
*/
static unsigned R_HandleInitLoaderCmd( void *pcmd ) {
	loaderInitCmd_t *cmd = pcmd;

	GLimp_MakeCurrent( loader_gl_context[cmd->self], loader_gl_surface[cmd->self] );
	r_unpackAlignment[QGL_CONTEXT_LOADER + cmd->self] = 4;

	return sizeof( *cmd );
}

/*
* R_HandleShutdownLoaderCmd
*/
static unsigned R_HandleShutdownLoaderCmd( void *pcmd ) {
	GLimp_MakeCurrent( NULL, NULL );

	return 0;
}

/*
* R_HandleLoadPicLoaderCmd
*/
static unsigned R_HandleLoadPicLoaderCmd( void *pcmd ) {
	loaderPicCmd_t *cmd = pcmd;
	image_t *image = r_images + cmd->pic;
	bool loaded;

	loaded = R_LoadImageFromDisk( QGL_CONTEXT_LOADER + cmd->self, image );
	R_UnbindImage( image );

	if( !loaded ) {
		image->missing = true;
	} else {
		// Make sure:
		// - The GL calls are submitted, otherwise they may stay in the queue until some other textures are loaded.
		// - image->loaded is set when the image is actually uploaded, so the renderer doesn't try to use it while
		//   it's still being uploaded, causing transient or even permanent glitches.
		if( !rsh.registrationOpen ) {
			qglFinish();
		}
		image->loaded = true;
	}

	return sizeof( *cmd );
}

/*
* R_HandleDataSyncLoaderCmd
*/
static unsigned R_HandleDataSyncLoaderCmd( void *pcmd ) {
	int *cmd = pcmd;

	qglFinish();

	return sizeof( *cmd );
}

/*
*R_ImageLoaderCmdsWaiter
*/
static int R_ImageLoaderCmdsWaiter( qbufPipe_t *queue, queueCmdHandler_t *cmdHandlers, bool timeout ) {
	return ri.BufPipe_ReadCmds( queue, cmdHandlers );
}

/*
* R_ImageLoaderThreadProc
*/
static void *R_ImageLoaderThreadProc( void *param ) {
	qbufPipe_t *cmdQueue = param;
	queueCmdHandler_t cmdHandlers[NUM_LOADER_CMDS] =
	{
		(queueCmdHandler_t)R_HandleInitLoaderCmd,
		(queueCmdHandler_t)R_HandleShutdownLoaderCmd,
		(queueCmdHandler_t)R_HandleLoadPicLoaderCmd,
		(queueCmdHandler_t)R_HandleDataSyncLoaderCmd,
	};

	ri.BufPipe_Wait( cmdQueue, R_ImageLoaderCmdsWaiter, cmdHandlers, Q_THREADS_WAIT_INFINITE );

	return NULL;
}
