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

#define	MAX_GLIMAGES	    4096
#define IMAGES_HASH_SIZE    64

typedef struct
{
	int ctx;
	int side;
} loaderCbInfo_t;

static image_t images[MAX_GLIMAGES];
static image_t images_hash_headnode[IMAGES_HASH_SIZE], *free_images;
static unsigned int image_cur_hash;

static int *r_8to24table;

static mempool_t *r_imagesPool;
static char *r_imagePathBuf, *r_imagePathBuf2;
static size_t r_sizeof_imagePathBuf, r_sizeof_imagePathBuf2;

#undef ENSUREBUFSIZE
#define ENSUREBUFSIZE(buf,need) \
	if( r_sizeof_ ##buf < need ) \
	{ \
		if( r_ ##buf ) \
			R_Free( r_ ##buf ); \
		r_sizeof_ ##buf += (((need) & (MAX_QPATH-1))+1) * MAX_QPATH; \
		r_ ##buf = R_MallocExt( r_imagesPool, r_sizeof_ ##buf, 0, 0 ); \
	}

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;

static int gl_filter_depth = GL_LINEAR;

static int gl_anisotropic_filter = 0;

static void *gl_loader_context = NULL;

static void R_InitImageLoader( void );
static void R_ShutdownImageLoader( void );
static void R_LoadAsyncImageFromDisk( image_t *image );

typedef struct
{
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
* R_TextureMode
*/
void R_TextureMode( char *string )
{
	int i;
	image_t	*glt;

	for( i = 0; i < NUM_GL_MODES; i++ )
	{
		if( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if( i == NUM_GL_MODES )
	{
		Com_Printf( "R_TextureMode: bad filter name\n" );
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for( i = 1, glt = images; i < MAX_GLIMAGES; i++, glt++ )
	{
		if( !glt->texnum ) {
			continue;
		}
		if( glt->flags & (IT_NOFILTERING|IT_DEPTH) ) {
			continue;
		}

		RB_BindTexture( 0, glt );

		if( !( glt->flags & IT_NOMIPMAP ) )
		{
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
		else 
		{
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max );
		}
	}
}

/*
* R_AnisotropicFilter
*/
void R_AnisotropicFilter( int value )
{
	int i, old;
	image_t	*glt;

	if( !glConfig.ext.texture_filter_anisotropic )
		return;

	old = gl_anisotropic_filter;
	gl_anisotropic_filter = bound( 1, value, glConfig.maxTextureFilterAnisotropic );
	if( gl_anisotropic_filter == old )
		return;

	// change all the existing mipmap texture objects
	for( i = 1, glt = images; i < MAX_GLIMAGES; i++, glt++ )
	{
		if( !glt->texnum ) {
			continue;
		}
		if( (glt->flags & (IT_NOFILTERING|IT_DEPTH|IT_NOMIPMAP)) ) {
			continue;
		}

		RB_BindTexture( 0, glt );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic_filter );
	}
}

/*
* R_PrintImageList
*/
void R_PrintImageList( const char *mask, qboolean (*filter)( const char *mask, const char *value) )
{
	int i, bytes;
	int numImages;
	image_t	*image;
	double texels = 0, add, total_bytes = 0;

	Com_Printf( "------------------\n" );

	numImages = 0;
	for( i = 0, image = images; i < MAX_GLIMAGES; i++, image++ )
	{
		if( !image->texnum ) {
			continue;
		}
		if( !image->upload_width || !image->upload_height ) {
			continue;
		}
		if( filter && !filter( mask, image->name ) ) {
			continue;
		}
		if( !image->loaded || image->missing ) {
			continue;
		}

		add = image->upload_width * image->upload_height;
		if( !(image->flags & (IT_DEPTH|IT_NOFILTERING|IT_NOMIPMAP)) )
			add = (unsigned)floor( add / 0.75 );
		if( image->flags & IT_CUBEMAP )
			add *= 6;

		texels += add;
		bytes = add * (image->flags & IT_LUMINANCE ? 1 : 4);
		total_bytes += bytes;

		Com_Printf( " %4i %4i: %s%s%s %.1f KB\n", image->upload_width, image->upload_height,
			image->name, image->extension, ((image->flags & (IT_NOMIPMAP|IT_NOFILTERING)) ? "" : " (mip)"), bytes / 1024.0 );

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

enum
{
	TEXTURE_LOADING_BUF0,TEXTURE_LOADING_BUF1,TEXTURE_LOADING_BUF2,TEXTURE_LOADING_BUF3,TEXTURE_LOADING_BUF4,TEXTURE_LOADING_BUF5,
	TEXTURE_RESAMPLING_BUF,
	TEXTURE_LINE_BUF,
	TEXTURE_CUT_BUF,
	TEXTURE_FLIPPING_BUF0,TEXTURE_FLIPPING_BUF1,TEXTURE_FLIPPING_BUF2,TEXTURE_FLIPPING_BUF3,TEXTURE_FLIPPING_BUF4,TEXTURE_FLIPPING_BUF5,

	NUM_IMAGE_BUFFERS
};

static qbyte *r_screenShotBuffer;
static size_t r_screenShotBufferSize;

static qbyte *r_imageBuffers[NUM_GL_CONTEXTS][NUM_IMAGE_BUFFERS];
static size_t r_imageBufSize[NUM_GL_CONTEXTS][NUM_IMAGE_BUFFERS];

#define R_PrepareImageBuffer(ctx,buffer,size) _R_PrepareImageBuffer(ctx,buffer,size,__FILE__,__LINE__)

/*
* R_PrepareImageBuffer
*/
static qbyte *_R_PrepareImageBuffer( int ctx, int buffer, size_t size, 
	const char *filename, int fileline )
{
	if( r_imageBufSize[ctx][buffer] < size )
	{
		r_imageBufSize[ctx][buffer] = size;
		if( r_imageBuffers[ctx][buffer] )
			R_Free( r_imageBuffers[ctx][buffer] );
		r_imageBuffers[ctx][buffer] = R_MallocExt( r_imagesPool, size, 0, 1 );
	}

	memset( r_imageBuffers[ctx][buffer], 255, size );

	return r_imageBuffers[ctx][buffer];
}

/*
* R_FreeImageBuffers
*/
void R_FreeImageBuffers( void )
{
	int i, j;

	for( i = 0; i < NUM_GL_CONTEXTS; i++ )
		for( j = 0; j < NUM_IMAGE_BUFFERS; j++ )
		{
			if( r_imageBuffers[i][j] )
			{
				R_Free( r_imageBuffers[i][j] );
				r_imageBuffers[i][j] = NULL;
			}
			r_imageBufSize[i][j] = 0;
		}
}

/*
* R_SwapBlueRed
*/
static void R_SwapBlueRed( qbyte *data, int width, int height, int samples )
{
	int i, j, size;

	size = width * height;
	for( i = 0; i < size; i++, data += samples )
	{
		j = data[0];
		data[0] = data[2];
		data[2] = j;
		// data[0] ^= data[2];
		// data[2] = data[0] ^ data[2];
		// data[0] ^= data[2];
	}
}

/*
* R_AllocImageBufferCb
*/
static qbyte *_R_AllocImageBufferCb( void *ptr, size_t size, const char *filename, int linenum )
{
	loaderCbInfo_t *cbinfo = ptr;
	return _R_PrepareImageBuffer( cbinfo->ctx, cbinfo->side, size, filename, linenum );
}

/*
* R_ReadImageFromDisk
*/
static int R_ReadImageFromDisk( int ctx, char *pathname, size_t pathname_size, 
	qbyte **pic, int *width, int *height, int *flags, int side )
{
	const char *extension;
	int samples;

	*pic = NULL;
	*width = *height = 0;
	samples = 0;

	extension = ri.FS_FirstExtension( pathname, IMAGE_EXTENSIONS, NUM_IMAGE_EXTENSIONS );
	if( extension )
	{
		r_imginfo_t imginfo;
		loaderCbInfo_t cbinfo = { ctx, side };

		COM_ReplaceExtension( pathname, extension, pathname_size );

		if( !Q_stricmp( extension, ".jpg" ) )
			imginfo = LoadJPG( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		else if( !Q_stricmp( extension, ".tga" ) )
			imginfo = LoadTGA( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		else if( !Q_stricmp( extension, ".png" ) )
			imginfo = LoadPNG( pathname, _R_AllocImageBufferCb, (void *)&cbinfo );
		else
			return 0;

		if( imginfo.samples )
		{
			if( ( (imginfo.comp & ~1) == IMGCOMP_BGR ) && ( !glConfig.ext.bgra || !flags ) )
			{
				R_SwapBlueRed( imginfo.pixels, imginfo.width, imginfo.height, imginfo.samples );
				imginfo.comp = IMGCOMP_RGB | (imginfo.comp & 1);
			}
		}

		*pic = imginfo.pixels;
		*width = imginfo.width;
		*height = imginfo.height;
		samples = imginfo.samples;
		if( flags )
			*flags |= ( (imginfo.comp & ~1) == IMGCOMP_BGR ) ? IT_BGRA : 0;
	}

	return samples;
}

/*
* R_FlipTexture
*/
static void R_FlipTexture( const qbyte *in, qbyte *out, int width, int height, 
	int samples, qboolean flipx, qboolean flipy, qboolean flipdiagonal )
{
	int i, x, y;
	const qbyte *p, *line;
	int row_inc = ( flipy ? -samples : samples ) * width, col_inc = ( flipx ? -samples : samples );
	int row_ofs = ( flipy ? ( height - 1 ) * width * samples : 0 ), col_ofs = ( flipx ? ( width - 1 ) * samples : 0 );

	if( !in )
		return;

	if( flipdiagonal )
	{
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
	else
	{
		for( y = 0, line = in + row_ofs; y < height; y++, line += row_inc )
			for( x = 0, p = line + col_ofs; x < width; x++, p += col_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
}

/*
* R_ResampleTexture
*/
static void R_ResampleTexture( int ctx, const qbyte *in, int inwidth, int inheight, qbyte *out, 
	int outwidth, int outheight, int samples )
{
	int i, j, k;
	int inwidthS, outwidthS;
	unsigned int frac, fracstep;
	const qbyte *inrow, *inrow2, *pix1, *pix2, *pix3, *pix4;
	unsigned *p1, *p2;
	qbyte *opix;

	if( inwidth == outwidth && inheight == outheight )
	{
		memcpy( out, in, inwidth * inheight * samples );
		return;
	}

	p1 = ( unsigned * )R_PrepareImageBuffer( ctx, TEXTURE_LINE_BUF, outwidth * sizeof( *p1 ) * 2 );
	p2 = p1 + outwidth;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for( i = 0; i < outwidth; i++ )
	{
		p1[i] = samples * ( frac >> 16 );
		frac += fracstep;
	}

	frac = 3 * ( fracstep >> 2 );
	for( i = 0; i < outwidth; i++ )
	{
		p2[i] = samples * ( frac >> 16 );
		frac += fracstep;
	}

	inwidthS = inwidth * samples;
	outwidthS = outwidth * samples;
	for( i = 0; i < outheight; i++, out += outwidthS )
	{
		inrow = in + inwidthS * (int)( ( i + 0.25 ) * inheight / outheight );
		inrow2 = in + inwidthS * (int)( ( i + 0.75 ) * inheight / outheight );
		for( j = 0; j < outwidth; j++ )
		{
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
* R_HeightmapToNormalmap
*/
static int R_HeightmapToNormalmap( const qbyte *in, qbyte *out, int width, int height, float bumpScale, int samples )
{
	int x, y;
	vec3_t n;
	float ibumpScale;
	const qbyte *p0, *p1, *p2;

	if( !bumpScale )
		bumpScale = 1.0f;
	bumpScale *= max( 0, r_lighting_bumpscale->value );
	ibumpScale = ( 255.0 * 3.0 ) / bumpScale;

	memset( out, 255, width * height * 4 );
	for( y = 0; y < height; y++ )
	{
		for( x = 0; x < width; x++, out += 4 )
		{
			p0 = in + ( y * width + x ) * samples;
			p1 = ( x == width - 1 ) ? p0 - x * samples : p0 + samples;
			p2 = ( y == height - 1 ) ? in + x * samples : p0 + width * samples;

			n[0] = ( p0[0] + p0[1] + p0[2] ) - ( p1[0] + p1[1] + p1[2] );
			n[1] = ( p2[0] + p2[1] + p2[2] ) - ( p0[0] + p0[1] + p0[2] );
			n[2] = ibumpScale;
			VectorNormalize( n );

			out[0] = ( n[0] + 1 ) * 127.5f;
			out[1] = ( n[1] + 1 ) * 127.5f;
			out[2] = ( n[2] + 1 ) * 127.5f;
			out[3] = ( p0[0] + p0[1] + p0[2] ) / 3;
		}
	}

	return 4;
}

/*
* R_MipMap
* 
* Operates in place, quartering the size of the texture
* note: if given odd width/height this discards the last row/column of
* pixels, rather than doing a proper box-filter scale down (LordHavoc)
*/
static void R_MipMap( qbyte *in, int width, int height, int samples )
{
	int i, j, k, samples2;
	qbyte *out;

	// width <<= 2;
	width *= samples;
	height >>= 1;
	samples2 = samples << 1;


	out = in;
	for( i = 0; i < height; i++, in += width )
	{
		for( j = 0; j < width; j += samples2, out += samples, in += samples2 )
		{
			for( k = 0; k < samples; k++ )
				out[k] = ( in[k] + in[k+samples] + in[width+k] + in[width+k+samples] )>>2;
		}
	}
}

/*
* R_TextureFormat
*/
#ifndef GL_ES_VERSION_2_0
static int R_TextureFormat( int samples, qboolean noCompress )
{
	int bits = r_texturebits->integer;

	if( r_texturecompression->integer && glConfig.ext.texture_compression && !noCompress )
	{
		if( samples == 3 )
			return GL_COMPRESSED_RGB_ARB;
		return GL_COMPRESSED_RGBA_ARB;
	}

	if( samples == 3 )
	{
		if( bits == 16 )
			return GL_RGB5;
		else if( bits == 32 )
			return GL_RGB8;
		return GL_RGB;
	}

	if( bits == 16 )
		return GL_RGBA4;
	else if( bits == 32 )
		return GL_RGBA8;
	return GL_RGBA;
}
#endif

/*
* R_Upload32
*/
static void R_Upload32( int ctx, qbyte **data, int width, int height, int flags, 
	int *upload_width, int *upload_height, int samples,
	qboolean subImage, qboolean noScale )
{
	int i, comp, format, type;
	int target, target2;
	int numTextures;
	qbyte *scaled = NULL;
	int scaledWidth, scaledHeight;

	assert( samples );

	// we can't properly mipmap a NPT-texture in software
	if( ( glConfig.ext.texture_non_power_of_two && ( flags & IT_NOMIPMAP ) )
		|| ( subImage && noScale ) )
	{
		scaledWidth = width;
		scaledHeight = height;
	}
	else
	{
		for( scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1 );
		for( scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1 );
	}

	if( !( flags & IT_NOPICMIP ) ) {
		if( flags & IT_SKY ) {
			// let people sample down the sky textures for speed
			scaledWidth >>= r_skymip->integer;
			scaledHeight >>= r_skymip->integer;
		}
		else {
			// let people sample down the world textures for speed
			scaledWidth >>= r_picmip->integer;
			scaledHeight >>= r_picmip->integer;
		}
	}

	// don't ever bother with > maxSize textures
	if( flags & IT_CUBEMAP )
	{
		numTextures = 6;
		target = GL_TEXTURE_CUBE_MAP_ARB;
		target2 = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
		clamp( scaledWidth, 1, glConfig.maxTextureCubemapSize );
		clamp( scaledHeight, 1, glConfig.maxTextureCubemapSize );
	}
	else
	{
		if( flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
		{
			qbyte *temp = R_PrepareImageBuffer( ctx, TEXTURE_FLIPPING_BUF0, width * height * samples );
			R_FlipTexture( data[0], temp, width, height, samples, 
				(flags & IT_FLIPX) ? qtrue : qfalse, 
				(flags & IT_FLIPY) ? qtrue : qfalse, 
				(flags & IT_FLIPDIAGONAL) ? qtrue : qfalse );
			data = &r_imageBuffers[ctx][TEXTURE_FLIPPING_BUF0];
		}

		numTextures = 1;
		target = GL_TEXTURE_2D;
		target2 = GL_TEXTURE_2D;
		clamp( scaledWidth, 1, glConfig.maxTextureSize );
		clamp( scaledHeight, 1, glConfig.maxTextureSize );
	}

	if( upload_width )
		*upload_width = scaledWidth;
	if( upload_height )
		*upload_height = scaledHeight;

	if( flags & IT_DEPTH )
	{
		comp = GL_DEPTH_COMPONENT;
		format = GL_DEPTH_COMPONENT;
		type = glConfig.ext.depth24 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	}
	else if( flags & IT_LUMINANCE )
	{
		comp = GL_LUMINANCE;
		format = GL_LUMINANCE;
		type = GL_UNSIGNED_BYTE;
	}
	else
	{
		if( samples == 4 )
			format = ( flags & IT_BGRA ? GL_BGRA_EXT : GL_RGBA );
		else
			format = ( flags & IT_BGRA ? GL_BGR_EXT : GL_RGB );
#ifdef GL_ES_VERSION_2_0
		comp = format;
#else
		comp = R_TextureFormat( samples, flags & IT_NOCOMPRESS ? qtrue : qfalse );
#endif
		type = GL_UNSIGNED_BYTE;
	}

	if( flags & IT_NOFILTERING )
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	else if( flags & IT_DEPTH )
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_depth );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_depth );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}
	else if( !( flags & IT_NOMIPMAP ) )
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_min );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic_filter );
	}
	else
	{
		qglTexParameteri( target, GL_TEXTURE_MIN_FILTER, gl_filter_max );
		qglTexParameteri( target, GL_TEXTURE_MAG_FILTER, gl_filter_max );

		if( glConfig.ext.texture_filter_anisotropic )
			qglTexParameteri( target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );
	}

	// clamp if required
	if( !( flags & IT_CLAMP ) )
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_REPEAT );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_REPEAT );
	}
	else if( glConfig.ext.texture_edge_clamp )
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
#ifndef GL_ES_VERSION_2_0
	else
	{
		qglTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP );
	}
#endif

	if( ( scaledWidth == width ) && ( scaledHeight == height ) && ( flags & IT_NOMIPMAP ) )
	{
		if( subImage )
		{
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, type, data[i] );
		}
		else
		{
			for( i = 0; i < numTextures; i++, target2++ )
				qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, type, data[i] );
		}
	}
	else
	{
		for( i = 0; i < numTextures; i++, target2++ )
		{
			qbyte *mip;

			if( !scaled )
				scaled = R_PrepareImageBuffer( ctx, TEXTURE_RESAMPLING_BUF, 
				scaledWidth * scaledHeight * samples );

			// resample the texture
			mip = scaled;
			if( data && data[i] )
				R_ResampleTexture( ctx, data[i], width, height, (qbyte*)mip, scaledWidth, scaledHeight, samples );
			else
				mip = NULL;

			if( subImage )
				qglTexSubImage2D( target2, 0, 0, 0, scaledWidth, scaledHeight, format, type, mip );
			else
				qglTexImage2D( target2, 0, comp, scaledWidth, scaledHeight, 0, format, type, mip );

			// mipmaps generation
			if( !( flags & IT_NOMIPMAP ) && mip )
			{
				int w, h;
				int miplevel = 0;

				w = scaledWidth;
				h = scaledHeight;
				while( w > 1 || h > 1 )
				{
					R_MipMap( mip, w, h, samples );

					w >>= 1;
					h >>= 1;
					if( w < 1 )
						w = 1;
					if( h < 1 )
						h = 1;
					miplevel++;

					if( subImage )
						qglTexSubImage2D( target2, miplevel, 0, 0, w, h, format, type, mip );
					else
						qglTexImage2D( target2, miplevel, comp, w, h, 0, format, type, mip );
				}
			}
		}
	}
}

static void R_LoadImageFromDisk( int ctx, image_t *image )
{
	int flags = image->flags;
	char *pathname = image->name;
	size_t pathsize = image->name_size;
	size_t len = strlen( pathname );
	const char *extension = "";
	int width = 1, height = 1, samples = 1;

	if( flags & IT_CUBEMAP )
	{
		int i, j;
		qbyte *pic[6];
		struct cubemapSufAndFlip
		{
			char *suf; int flags;
		} cubemapSides[2][6] = {
			{ 
				{ "px", 0 }, { "nx", 0 }, { "py", 0 },
				{ "ny", 0 }, { "pz", 0 }, { "nz", 0 } 
			},
			{
				{ "rt", IT_FLIPDIAGONAL }, { "lf", IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL }, { "bk", IT_FLIPY },
				{ "ft", IT_FLIPX }, { "up", IT_FLIPDIAGONAL }, { "dn", IT_FLIPDIAGONAL }
			}
		};
		int lastSize = 0;

		pathname[len] = '_';
		for( i = 0; i < 2; i++ )
		{
			for( j = 0; j < 6; j++ )
			{
				pathname[len+1] = cubemapSides[i][j].suf[0];
				pathname[len+2] = cubemapSides[i][j].suf[1];
				pathname[len+3] = 0;

				Q_strncatz( pathname, extension, pathsize );
				samples = R_ReadImageFromDisk( ctx, pathname, pathsize, 
					&(pic[j]), &width, &height, &flags, j );
				if( pic[j] )
				{
					if( width != height )
					{
						ri.Com_DPrintf( S_COLOR_YELLOW "Not square cubemap image %s\n", pathname );
						break;
					}
					if( !j )
					{
						lastSize = width;
					}
					else if( lastSize != width )
					{
						ri.Com_DPrintf( S_COLOR_YELLOW "Different cubemap image size: %s\n", pathname );
						break;
					}
					if( cubemapSides[i][j].flags & ( IT_FLIPX|IT_FLIPY|IT_FLIPDIAGONAL ) )
					{
						int flags = cubemapSides[i][j].flags;
						qbyte *temp = R_PrepareImageBuffer( ctx,
							TEXTURE_FLIPPING_BUF0+j, width * height * samples );
						R_FlipTexture( pic[j], temp, width, height, 4, 
							(flags & IT_FLIPX) ? qtrue : qfalse, 
							(flags & IT_FLIPY) ? qtrue : qfalse, 
							(flags & IT_FLIPDIAGONAL) ? qtrue : qfalse );
						pic[j] = temp;
					}
					continue;
				}
				break;
			}
			if( j == 6 )
				break;
		}

		if( i != 2 )
		{
			pathname[len] = 0;
			image->width = width;
			image->height = height;
			image->samples = samples;

			RB_BindContextTexture( 0, image );

			R_Upload32( ctx, pic, width, height, flags, &image->upload_width, 
				&image->upload_height, samples, qfalse, qfalse );

			image->extension[0] = '.';
			Q_strncpyz( &image->extension[1], &pathname[len+4], sizeof( image->extension )-1 );
			image->loaded = qtrue;
		}
		else
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "Missing image: %s\n", image->name );
			image->missing = qtrue;
		}
	}
	else
	{
		qbyte *pic = NULL;

		Q_strncatz( pathname, extension, pathsize );
		samples = R_ReadImageFromDisk( ctx, pathname, pathsize, &pic, &width, &height, &flags, 0 );

		if( pic )
		{
			qbyte *temp;

			if( flags & IT_HEIGHTMAP )
			{
				temp = R_PrepareImageBuffer( ctx, TEXTURE_FLIPPING_BUF0, width * height * 4 );
				samples = R_HeightmapToNormalmap( pic, temp, width, height, 1, samples );
				pic = temp;
			}

			RB_BindContextTexture( 0, image );

			pathname[len] = 0;
			image->width = width;
			image->height = height;
			image->samples = samples;
			R_Upload32( ctx, &pic, width, height, flags, &image->upload_width, 
				&image->upload_height, samples, qfalse, qfalse );
			image->extension[0] = '.';
			Q_strncpyz( &image->extension[1], &pathname[len+1], sizeof( image->extension )-1 );
			image->loaded = qtrue;
		}
		else
		{
			ri.Com_DPrintf( S_COLOR_YELLOW "Missing image: %s\n", image->name );
			image->missing = qtrue;
		}
	}
}

/*
* R_LinkPic
*/
static image_t *R_LinkPic( void )
{
	image_t *image;
	unsigned int hash = image_cur_hash;

	if( !free_images ) {
		return NULL;
	}

	image = free_images;
	free_images = image->next;

	// link to the list of active images
	image->prev = &images_hash_headnode[hash];
	image->next = images_hash_headnode[hash].next;
	image->next->prev = image;
	image->prev->next = image;
	return image;
}

/*
* R_UnlinkPic
*/
static void R_UnlinkPic( image_t *image )
{
	// remove from linked active list
	image->prev->next = image->next;
	image->next->prev = image->prev;

	// insert into linked free list
	image->next = free_images;
	free_images = image;
}

/*
* R_LoadImage
*/
image_t *R_LoadImage( const char *name, qbyte **pic, int width, int height, int flags, int samples )
{
	image_t *image;
	int name_len = strlen( name );

	if( image_cur_hash >= IMAGES_HASH_SIZE )
		image_cur_hash = COM_SuperFastHash( ( const qbyte *)name, name_len, name_len ) % IMAGES_HASH_SIZE;

	image = R_LinkPic();
	if( !image ) {
		ri.Com_Error( ERR_DROP, "R_LoadImage: r_numImages == MAX_GLIMAGES" );
	}

	image->name_size = name_len + 15;
	image->name = R_MallocExt( r_imagesPool, image->name_size, 0, 1 );
	strcpy( image->name, name );
	image->width = width;
	image->height = height;
	image->flags = flags;
	image->samples = samples;
	image->fbo = 0;
	image->texnum = 0;
	image->registrationSequence = rsh.registrationSequence;
	image->loaded = qtrue;
	image->missing = qfalse;

	RB_AllocTextureNum( image );

	RB_BindTexture( 0, image );

	R_Upload32( GL_CONTEXT_MAIN, pic, width, height, flags, 
		&image->upload_width, &image->upload_height, image->samples, qfalse, qfalse );

	image_cur_hash = IMAGES_HASH_SIZE+1;
	return image;
}

/*
* R_FreeImage
*/
static void R_FreeImage( image_t *image )
{
	RB_FreeTextureNum( image );

	R_Free( image->name );

	image->name = NULL;
	image->texnum = 0;
	image->registrationSequence = 0;

	R_UnlinkPic( image );
}

/*
* R_ReplaceImage
*/
void R_ReplaceImage( image_t *image, qbyte **pic, int width, int height, int flags, int samples )
{
	assert( image );
	assert( image->texnum );

	RB_BindTexture( 0, image );

	if( image->width != width || image->height != height )
		R_Upload32( GL_CONTEXT_MAIN, pic, width, height, flags, 
		&(image->upload_width), &(image->upload_height), samples, qfalse, qfalse );
	else
		R_Upload32( GL_CONTEXT_MAIN, pic, width, height, flags, 
		&(image->upload_width), &(image->upload_height), samples, qtrue, qfalse );

	image->flags = flags;
	image->width = width;
	image->height = height;
	image->samples = samples;
	image->registrationSequence = rsh.registrationSequence;
}

/*
* R_ReplaceSubImage
*
* FIXME: add x,y?
*/
void R_ReplaceSubImage( image_t *image, qbyte **pic, int width, int height )
{
	int w, h;

	assert( image );
	assert( image->texnum );

	RB_BindTexture( 0, image );

	R_Upload32( GL_CONTEXT_MAIN, pic, width, height, image->flags,
		&w, &h, image->samples, qtrue, qtrue );

	image->registrationSequence = rsh.registrationSequence;
}

/*
* R_FindImage
* 
* Finds and loads the given image. IT_SYNC images are loaded synchronously.
* For synchronous missing images, NULL is returned.
*/
image_t	*R_FindImage( const char *name, const char *suffix, int flags, float bumpScale )
{
	int i, lastDot, lastSlash;
	unsigned int len, key;
	image_t	*image, *hnode;
	char *pathname;
	qbyte *empty_data[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

	if( !name || !name[0] )
		return NULL; //	ri.Com_Error (ERR_DROP, "R_FindImage: NULL name");

	ENSUREBUFSIZE( imagePathBuf, strlen( name ) + (suffix ? strlen( suffix ) : 0) + 5 );
	pathname = r_imagePathBuf;

	lastDot = -1;
	lastSlash = -1;
	for( i = ( name[0] == '/' || name[0] == '\\' ), len = 0; name[i]; i++ )
	{
		if( name[i] == '.' )
			lastDot = len;
		if( name[i] == '\\' )
			pathname[len] = '/';
		else
			pathname[len] = tolower( name[i] );
		if( pathname[len] == '/' )
			lastSlash = len;
		len++;
	}

	if( len < 5 )
		return NULL;

	// don't confuse paths such as /ui/xyz.cache/123 with file extensions
	if( lastDot < lastSlash ) {
		lastDot = -1;
	}

	if( lastDot != -1 )
		len = lastDot;

	if( suffix )
	{
		for( i = 0; suffix[i]; i++ )
			pathname[len++] = tolower( suffix[i] );
	}

	pathname[len] = 0;

	// look for it
	key = image_cur_hash = COM_SuperFastHash( ( const qbyte *)pathname, len, len ) % IMAGES_HASH_SIZE;
	hnode = &images_hash_headnode[key];
	if( flags & IT_HEIGHTMAP )
	{
		for( image = hnode->prev; image != hnode; image = image->prev )
		{
			if( ( ( image->flags & flags ) == flags ) && ( image->bumpScale == bumpScale ) && !strcmp( image->name, pathname ) ) {
				R_TouchImage( image );
				return image;
			}
		}
	}
	else
	{
		for( image = hnode->prev; image != hnode; image = image->prev )
		{
			if( ( ( image->flags & flags ) == flags ) && !strcmp( image->name, pathname ) ) {
				R_TouchImage( image );
				return image;
			}
		}
	}

	pathname[len] = 0;

	//
	// load the pic from disk
	//
	image = R_LoadImage( pathname, empty_data, 1, 1, flags, 1 );

	if( !( image->flags & IT_SYNC ) && ( gl_loader_context != NULL ) ) {
		R_LoadAsyncImageFromDisk( image );
		return image;
	}

	R_LoadImageFromDisk( GL_CONTEXT_MAIN, image );
	if( image->missing ) {
		R_FreeImage( image );
		image = NULL;
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
	qboolean flipx, qboolean flipy, qboolean flipdiagonal, qboolean silent )
{
	size_t size, buf_size;
	qbyte *buffer, *flipped;
	r_imginfo_t imginfo;
	const char *extension;

	if( !COM_ValidateRelativeFilename( filename ) )
	{
		Com_Printf( "R_ScreenShot: Invalid filename\n" );
		return;
	}

	extension = COM_FileExtension( filename );
	if( !extension )
	{
		Com_Printf( "R_ScreenShot: Invalid filename\n" );
		return;
	}

	size = width * height * 3;
	buf_size = size * 2;
	if( size > r_screenShotBufferSize ) {
		if( r_screenShotBuffer ) {
			R_Free( r_screenShotBuffer );
		}
		r_screenShotBuffer = R_MallocExt( r_imagesPool, buf_size, 0, 1 );
		r_screenShotBufferSize = buf_size;
	}

	buffer = r_screenShotBuffer;
	if( flipx || flipy || flipdiagonal ) {
		flipped = buffer + size;
	}
	else {
		flipped = NULL;
	}

	imginfo.width = width;
	imginfo.height = height;
	imginfo.samples = 3;
	imginfo.pixels = flipped ? flipped : buffer;

	if( !Q_stricmp( extension, ".jpg" ) ) {
		imginfo.comp = IMGCOMP_RGB;
	} else {
		imginfo.comp = glConfig.ext.bgra ? IMGCOMP_BGR : IMGCOMP_RGB;
	}

	qglReadPixels( 0, 0, width, height, 
		imginfo.comp == IMGCOMP_BGR ? GL_BGR_EXT : GL_RGB, GL_UNSIGNED_BYTE, buffer );

	if( flipped ) {
		R_FlipTexture( buffer, flipped, width, height, 3, 
			flipx, flipy, flipdiagonal ); 
	}

	if( !Q_stricmp( extension, ".jpg" ) ) {
		if( WriteJPG( filename, &imginfo, quality ) && !silent )
			Com_Printf( "Wrote %s\n", filename );

	} else {
		if( WriteTGA( filename, &imginfo, 100 ) && !silent )
			Com_Printf( "Wrote %s\n", filename );
	}
}

//=======================================================

/*
* R_InitNoTexture
*/
static void R_InitNoTexture( int *w, int *h, int *flags, int *samples )
{
	int x, y;
	qbyte *data;
	qbyte dottexture[8][8] =
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
	*flags = 0;
	*samples = 3;

	// ch : check samples
	data = R_PrepareImageBuffer( GL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 8 * 8 * 3 );
	for( x = 0; x < 8; x++ )
	{
		for( y = 0; y < 8; y++ )
		{
			data[( y*8 + x )*3+0] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*3+1] = dottexture[x&3][y&3]*127;
			data[( y*8 + x )*3+2] = dottexture[x&3][y&3]*127;
		}
	}
}

/*
* R_InitSolidColorTexture
*/
static qbyte *R_InitSolidColorTexture( int *w, int *h, int *flags, int *samples, int color )
{
	qbyte *data;

	//
	// solid color texture
	//
	*w = *h = 1;
	*flags = IT_NOPICMIP|IT_NOCOMPRESS;
	*samples = 3;

	// ch : check samples
	data = R_PrepareImageBuffer( GL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 1 * 1 * 3 );
	data[0] = data[1] = data[2] = color;
	return data;
}

/*
* R_InitParticleTexture
*/
static void R_InitParticleTexture( int *w, int *h, int *flags, int *samples )
{
	int x, y;
	int dx2, dy, d;
	float dd2;
	qbyte *data;

	//
	// particle texture
	//
	*w = *h = 16;
	*flags = IT_NOPICMIP|IT_NOMIPMAP;
	*samples = 4;

	data = R_PrepareImageBuffer( GL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 16 * 16 * 4 );
	for( x = 0; x < 16; x++ )
	{
		dx2 = x - 8;
		dx2 = dx2 * dx2;

		for( y = 0; y < 16; y++ )
		{
			dy = y - 8;
			dd2 = dx2 + dy * dy;
			d = 255 - 35 * sqrt( dd2 );
			data[( y*16 + x ) * 4 + 3] = bound( 0, d, 255 );
		}
	}
}

/*
* R_InitWhiteTexture
*/
static void R_InitWhiteTexture( int *w, int *h, int *flags, int *samples )
{
	R_InitSolidColorTexture( w, h, flags, samples, 255 );
}

/*
* R_InitBlackTexture
*/
static void R_InitBlackTexture( int *w, int *h, int *flags, int *samples )
{
	R_InitSolidColorTexture( w, h, flags, samples, 0 );
}

/*
* R_InitGreyTexture
*/
static void R_InitGreyTexture( int *w, int *h, int *flags, int *samples )
{
	R_InitSolidColorTexture( w, h, flags, samples, 127 );
}

/*
* R_InitBlankBumpTexture
*/
static void R_InitBlankBumpTexture( int *w, int *h, int *flags, int *samples )
{
	qbyte *data = R_InitSolidColorTexture( w, h, flags, samples, 128 );

/*
	data[0] = 128;	// normal X
	data[1] = 128;	// normal Y
*/
	data[2] = 255;	// normal Z
	data[3] = 128;	// height
}

/*
* R_InitCoronaTexture
*/
static void R_InitCoronaTexture( int *w, int *h, int *flags, int *samples )
{
	int x, y, a;
	float dx, dy;
	qbyte *data;

	//
	// light corona texture
	//
	*w = *h = 32;
	*flags = IT_NOMIPMAP|IT_NOPICMIP|IT_NOCOMPRESS|IT_CLAMP;
	*samples = 4;

	data = R_PrepareImageBuffer( GL_CONTEXT_MAIN, TEXTURE_LOADING_BUF0, 32 * 32 * 4 );
	for( y = 0; y < 32; y++ )
	{
		dy = ( y - 15.5f ) * ( 1.0f / 16.0f );
		for( x = 0; x < 32; x++ )
		{
			dx = ( x - 15.5f ) * ( 1.0f / 16.0f );
			a = (int)( ( ( 1.0f / ( dx * dx + dy * dy + 0.2f ) ) - ( 1.0f / ( 1.0f + 0.2 ) ) ) * 32.0f / ( 1.0f / ( 1.0f + 0.2 ) ) );
			clamp( a, 0, 255 );
			data[( y*32+x )*4+0] = data[( y*32+x )*4+1] = data[( y*32+x )*4+2] = a;
		}
	}
}

/*
* R_GetViewportTextureSize
*/
static void R_GetViewportTextureSize( const int viewportWidth, const int viewportHeight, 
	const int size, int *width, int *height )
{
	int limit;
	int width_, height_;

	// limit the texture size to either screen resolution in case we can't use FBO
	// or hardware limits and ensure it's a POW2-texture if we don't support such textures
	limit = glConfig.maxTextureSize;
	if( size )
		limit = min( limit, size );
	if( limit < 1 )
		limit = 1;
	width_ = height_ = limit;

	if( glConfig.ext.texture_non_power_of_two )
	{
		width_ = min( viewportWidth, limit );
		height_ = min( viewportHeight, limit );
	}
	else
	{
		int d;

		// calculate the upper bound and make sure it's not a pow of 2
		d = min( limit, viewportWidth );
		if( ( d & (d-1) ) == 0 ) d--;
		for( width_ = 2; width_ <= d; width_ <<= 1 );

		d = min( limit, viewportHeight );
		if( ( d & (d-1) ) == 0 ) d--;
		for( height_ = 2; height_ <= d; height_ <<= 1 );

		if( size ) {
			while( width_ > size || height_ > size ) {
				width_ >>= 1;
				height_ >>= 1;
			}
		}
	}

	*width = width_;
	*height = height_;
}

/*
* R_InitViewportTexture
*/
void R_InitViewportTexture( image_t **texture, const char *name, int id, 
	int viewportWidth, int viewportHeight, int size, int flags, int samples )
{
	int width, height;
	image_t *t;

	if( !glConfig.ext.framebuffer_object ) {
		*texture = NULL;
		return;
	}

	R_GetViewportTextureSize( viewportWidth, viewportHeight, size, &width, &height );

	// create a new texture or update the old one
	if( !( *texture ) || ( *texture )->width != width || ( *texture )->height != height )
	{
		qbyte *data = NULL;

		if( !*texture ) {
			char uploadName[128];

			Q_snprintfz( uploadName, sizeof( uploadName ), "***%s_%i***", name, id );
			t = *texture = R_LoadImage( uploadName, &data, width, height, flags, samples );
		}
		else { 
			t = *texture;
			RB_BindTexture( 0, t );
			t->width = width;
			t->height = height;
			R_Upload32( GL_CONTEXT_MAIN, &data, width, height, flags, 
				&t->upload_width, &t->upload_height, t->samples, qfalse, qfalse );
		}

		// update FBO, if attached
		if( t->fbo ) {
			RFB_UnregisterObject( t->fbo );
			t->fbo = 0;
		}
		if( t->flags & IT_FRAMEBUFFER ) {
			t->fbo = RFB_RegisterObject( t->upload_width, t->upload_height );
			RFB_AttachTextureToObject( t->fbo, t );
		}
	}
}

/*
* R_GetPortalTextureId
*/
static int R_GetPortalTextureId( const int viewportWidth, const int viewportHeight, 
	const int flags, unsigned frameNum )
{
	int i;
	int best = -1;
	int realwidth, realheight;
	int realflags = IT_PORTALMAP|IT_FRAMEBUFFER|flags;
	image_t *image;

	R_GetViewportTextureSize( viewportWidth, viewportHeight, r_portalmaps_maxtexsize->integer, 
		&realwidth, &realheight );

	for( i = 0; i < MAX_PORTAL_TEXTURES; i++ )
	{
		image = rsh.portalTextures[i];
		if( !image )
			return i;

		if( image->framenum == frameNum ) {
			// the texture is used in the current scene
			continue;
		}

		if( image->width == realwidth && 
			image->height == realheight && 
			image->flags == realflags ) {
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
	int flags, unsigned frameNum )
{
	int id;

	id = R_GetPortalTextureId( viewportWidth, viewportHeight, flags, frameNum );
	if( id < 0 || id >= MAX_PORTAL_TEXTURES ) {
		return NULL;
	}

	R_InitViewportTexture( &rsh.portalTextures[id], "r_portaltexture", id, 
		viewportWidth, viewportHeight, r_portalmaps_maxtexsize->integer, 
		IT_PORTALMAP|IT_FRAMEBUFFER|flags, 3 );

	if( rsh.portalTextures[id] ) {
		rsh.portalTextures[id]->framenum = frameNum;
	}

	return rsh.portalTextures[id];
}

/*
* R_GetShadowmapTexture
*/
image_t *R_GetShadowmapTexture( int id, int viewportWidth, int viewportHeight, int flags )
{
	int samples;

	if( id < 0 || id >= MAX_SHADOWGROUPS ) {
		return NULL;
	}

	if( glConfig.ext.shadow ) {
		// render to depthbuffer, GL_ARB_shadow path
		flags |= IT_DEPTH;
		samples = 1;
	} else {
		flags |= IT_NOFILTERING;
		samples = 3;
	}

	R_InitViewportTexture( &rsh.shadowmapTextures[id], "r_shadowmap", id, 
		viewportWidth, viewportHeight, r_shadows_maxtexsize->integer, 
		IT_SHADOWMAP|IT_FRAMEBUFFER|flags, samples );

	return rsh.shadowmapTextures[id];
}

/*
* R_InitStretchRawTexture
*/
static void R_InitStretchRawTexture( void )
{
	const char * const name = "*** raw ***";
	int name_len = strlen( name );
	image_t *rawtexture;

	// reserve a dummy texture slot
	image_cur_hash = COM_SuperFastHash( ( const qbyte *)name, name_len, name_len ) % IMAGES_HASH_SIZE;
	rawtexture = R_LinkPic();

	assert( rawtexture );
	if( !rawtexture ) {
		ri.Com_Error( ERR_FATAL, "Failed to register cinematic texture" );
	}

	rawtexture->name = R_MallocExt( r_imagesPool, name_len + 1, 0, 1 );
	rawtexture->flags = IT_CINEMATIC;
	strcpy( rawtexture->name, name );
	RB_AllocTextureNum( rawtexture );
	rawtexture->loaded = qtrue;
	rawtexture->missing = qfalse;

	rsh.rawTexture = rawtexture;
}

/*
* R_InitStretchRawYUVTextures
*/
static void R_InitStretchRawYUVTextures( void )
{
	int i;
	image_t *rawtexture;
	const char * const name[3] = { "*** rawyuv0 ***", "*** rawyuv1 ***", "*** rawyuv2 ***" };

	for( i = 0; i < 3; i++ ) {
		// reserve a dummy texture slot
		int name_len = strlen( name[i] );

		image_cur_hash = COM_SuperFastHash( ( const qbyte *)name, name_len, name_len ) % IMAGES_HASH_SIZE;
		rawtexture = R_LinkPic();

		assert( rawtexture );
		if( !rawtexture ) {
			ri.Com_Error( ERR_FATAL, "Failed to register cinematic texture" );
		}

		rawtexture->name = R_MallocExt( r_imagesPool, name_len + 1, 0, 1 );
		rawtexture->flags = IT_CINEMATIC|IT_LUMINANCE;
		strcpy( rawtexture->name, name[i] );
		RB_AllocTextureNum( rawtexture );
		rawtexture->loaded = qtrue;
		rawtexture->missing = qfalse;

		rsh.rawYUVTextures[i] = rawtexture;
	}
}

/*
* R_InitScreenTexturesPair
*/
static void R_InitScreenTexturesPair( const char *name, image_t **color, 
	image_t **depth, int samples, qboolean noFilter )
{
	int flags;

	flags = IT_NOCOMPRESS|IT_NOPICMIP|IT_NOMIPMAP|IT_CLAMP;
	if( noFilter ) {
		flags |= IT_NOFILTERING;
	}

	if( color ) {
		R_InitViewportTexture( color, name, 0, 
			glConfig.width, glConfig.height, 0, 
			flags|IT_FRAMEBUFFER, samples );
	}
	if( depth && *color ) {
		R_InitViewportTexture( depth, va( "%s_depth", name ), 0,
			glConfig.width, glConfig.height, 0, 
			flags|IT_DEPTH, 1 );

		RFB_AttachTextureToObject( (*color)->fbo, *depth );
	}
}

/*
* R_InitScreenTextures
*/
static void R_InitScreenTextures( void )
{
	R_InitScreenTexturesPair( "r_screentex", &rsh.screenTexture, 
		&rsh.screenDepthTexture, 3, qtrue ); 

	R_InitScreenTexturesPair( "r_screentexcopy", &rsh.screenTextureCopy, 
		&rsh.screenDepthTextureCopy, 3, qtrue );

	R_InitScreenTexturesPair( "rsh.screenFxaaCopy", &rsh.screenFxaaCopy, 
		NULL, 3, qfalse );

	R_InitScreenTexturesPair( "rsh.screenWeaponTexture", &rsh.screenWeaponTexture, 
		NULL, 4, qtrue );
}

/*
* R_InitBuiltinTextures
*/
static void R_InitBuiltinTextures( void )
{
	int w, h, flags, samples;
	image_t *image;
	const struct
	{
		char *name;
		image_t	**image;
		void ( *init )( int *w, int *h, int *flags, int *samples );
	}
	textures[] =
	{
		{ "***r_notexture***", &rsh.noTexture, &R_InitNoTexture },
		{ "***r_whitetexture***", &rsh.whiteTexture, &R_InitWhiteTexture },
		{ "***r_blacktexture***", &rsh.blackTexture, &R_InitBlackTexture },
		{ "***r_greytexture***", &rsh.greyTexture, &R_InitGreyTexture },
		{ "***r_blankbumptexture***", &rsh.blankBumpTexture, &R_InitBlankBumpTexture },
		{ "***r_particletexture***", &rsh.particleTexture, &R_InitParticleTexture },
		{ "***r_coronatexture***", &rsh.coronaTexture, &R_InitCoronaTexture },
		{ NULL, NULL, NULL }
	};
	size_t i, num_builtin_textures = sizeof( textures ) / sizeof( textures[0] ) - 1;

	for( i = 0; i < num_builtin_textures; i++ )
	{
		textures[i].init( &w, &h, &flags, &samples );

		image = R_LoadImage( textures[i].name, r_imageBuffers[0], w, h, flags, samples );

		if( textures[i].image )
			*( textures[i].image ) = image;
	}
}

/*
* R_TouchBuiltinTextures
*/
static void R_TouchBuiltinTextures( void )
{
	R_TouchImage( rsh.rawTexture );
	R_TouchImage( rsh.rawYUVTextures[0] );
	R_TouchImage( rsh.rawYUVTextures[1] );
	R_TouchImage( rsh.rawYUVTextures[2] );
	R_TouchImage( rsh.noTexture );
	R_TouchImage( rsh.whiteTexture );
	R_TouchImage( rsh.blackTexture ); 
	R_TouchImage( rsh.greyTexture );
	R_TouchImage( rsh.blankBumpTexture ); 
	R_TouchImage( rsh.particleTexture ); 
	R_TouchImage( rsh.coronaTexture ); 
	R_TouchImage( rsh.screenTexture ); 
	R_TouchImage( rsh.screenDepthTexture );
	R_TouchImage( rsh.screenTextureCopy ); 
	R_TouchImage( rsh.screenDepthTextureCopy );
	R_TouchImage( rsh.screenFxaaCopy );
	R_TouchImage( rsh.screenWeaponTexture );
}

/*
* R_ReleaseBuiltinTextures
*/
static void R_ReleaseBuiltinTextures( void )
{
	rsh.rawTexture = NULL;
	rsh.rawYUVTextures[0] = rsh.rawYUVTextures[1] = rsh.rawYUVTextures[2] = NULL;
	rsh.noTexture = NULL;
	rsh.whiteTexture = rsh.blackTexture = rsh.greyTexture = NULL;
	rsh.blankBumpTexture = NULL;
	rsh.particleTexture = NULL;
	rsh.coronaTexture = NULL;
	rsh.screenTexture = rsh.screenDepthTexture = NULL;
	rsh.screenTextureCopy = rsh.screenDepthTextureCopy = NULL;
	rsh.screenFxaaCopy = NULL;
	rsh.screenWeaponTexture = NULL;
}

//=======================================================

/*
* R_InitImages
*/
void R_InitImages( void )
{
	int i;

	// allow any alignment
	qglPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	qglPixelStorei( GL_PACK_ALIGNMENT, 1 );

	r_imagesPool = R_AllocPool( r_mempool, "Images" );
	image_cur_hash = IMAGES_HASH_SIZE+1;

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;

	r_8to24table = NULL;

	memset( images, 0, sizeof( images ) );

	// link images
	free_images = images;
	for( i = 0; i < IMAGES_HASH_SIZE; i++ ) {
		images_hash_headnode[i].prev = &images_hash_headnode[i];
		images_hash_headnode[i].next = &images_hash_headnode[i];
	}
	for( i = 0; i < MAX_GLIMAGES - 1; i++ ) {
		images[i].next = &images[i+1];
	}

	R_InitImageLoader();

	R_InitStretchRawTexture();
	R_InitStretchRawYUVTextures();
	R_InitBuiltinTextures();
	R_InitScreenTextures();
}

/*
* R_TouchImage
*/
void R_TouchImage( image_t *image )
{
	if( !image ) {
		return;
	}
	if( image->registrationSequence == rsh.registrationSequence ) {
		return;
	}

	image->registrationSequence = rsh.registrationSequence;
	if( image->fbo ) {
		RFB_TouchObject( image->fbo );
	}
}

/*
* R_FreeUnusedImages
*/
void R_FreeUnusedImages( void )
{
	int i;
	image_t *image;

	R_TouchBuiltinTextures();
	
	R_FinishLoadingImages();

	for( i = 0, image = images; i < MAX_GLIMAGES; i++, image++ ) {
		if( !image->name ) {
			// free image
			continue;
		}
		if( image->registrationSequence == rsh.registrationSequence ) {
			// we need this image
			continue;
		}
		R_FreeImage( image );
	}

	for( i = 0; i < MAX_PORTAL_TEXTURES; i++ ) {
		if( rsh.portalTextures[i] && rsh.portalTextures[i]->registrationSequence != rsh.registrationSequence ) {
			rsh.portalTextures[i] = NULL;
		}
	}

	for( i = 0; i < MAX_SHADOWGROUPS; i++ ) {
		if( rsh.shadowmapTextures[i] && rsh.shadowmapTextures[i]->registrationSequence != rsh.registrationSequence ) {
			rsh.shadowmapTextures[i] = NULL;
		}
	}
}

/*
* R_ShutdownImages
*/
void R_ShutdownImages( void )
{
	int i;
	image_t *image;

	if( !r_imagesPool )
		return;

	R_ShutdownImageLoader();

	R_ReleaseBuiltinTextures();

	for( i = 0, image = images; i < MAX_GLIMAGES; i++, image++ ) {
		if( !image->name ) {
			// free texture
			continue;
		}
		R_FreeImage( image );
	}

	R_FreeImageBuffers();

	if( r_imagePathBuf )
		R_Free( r_imagePathBuf );
	if( r_imagePathBuf2 )
		R_Free( r_imagePathBuf2 );

	if( r_8to24table )
	{
		R_Free( r_8to24table );
		r_8to24table = NULL;
	}

	R_FreePool( &r_imagesPool );

	r_screenShotBuffer = NULL;
	r_screenShotBufferSize = 0;

	memset( rsh.portalTextures, 0, sizeof( rsh.portalTextures ) );
	memset( rsh.shadowmapTextures, 0, sizeof( rsh.shadowmapTextures ) );

	r_imagePathBuf = r_imagePathBuf2 = NULL;
	r_sizeof_imagePathBuf = r_sizeof_imagePathBuf2 = 0;
}

// ============================================================================

enum
{
	CMD_LOADER_INIT,
	CMD_LOADER_SHUTDOWN,
	CMD_LOADER_LOAD_PIC,

	NUM_LOADER_CMDS
};

typedef struct
{
	int id;
	int pic;
} loaderPicCmd_t;

typedef unsigned (*queueCmdHandler_t)( const void * );

static qbufQueue_t *loader_queue;
static qthread_t *loader_thread = NULL;

static void *R_ImageLoaderThreadProc( void *param );

/*
* R_IssueInitLoaderCmd
*/
static void R_IssueInitLoaderCmd( void )
{
	int cmd = CMD_LOADER_INIT;
	ri.BufQueue_EnqueueCmd( loader_queue, &cmd, sizeof( cmd ) );
}

/*
* R_IssueShutdownLoaderCmd
*/
static void R_IssueShutdownLoaderCmd( void )
{
	int cmd = CMD_LOADER_SHUTDOWN;
	ri.BufQueue_EnqueueCmd( loader_queue, &cmd, sizeof( cmd ) );
}

/*
* R_IssueLoadPicLoaderCmd
*/
static void R_IssueLoadPicLoaderCmd( int pic )
{
	loaderPicCmd_t cmd;
	cmd.id = CMD_LOADER_LOAD_PIC;
	cmd.pic = pic;
	ri.BufQueue_EnqueueCmd( loader_queue, &cmd, sizeof( cmd ) );
}

/*
* R_InitImageLoader
*/
static void R_InitImageLoader( void )
{
	gl_loader_context = GLimp_SharedContext_Create();
	if( !gl_loader_context ) {
		return;
	}

	loader_queue = ri.BufQueue_Create( 0x100000, 1 );
	ri.Thread_Create( &loader_thread, R_ImageLoaderThreadProc, loader_queue );

	R_IssueInitLoaderCmd();
}

/*
* R_FinishLoadingImages
*/
void R_FinishLoadingImages( void )
{
	if( !gl_loader_context ) {
		return;
	}
	ri.BufQueue_Finish( loader_queue );
}

/*
* R_LoadAsyncImageFromDisk
*/
static void R_LoadAsyncImageFromDisk( image_t *image )
{
	image->loaded = qfalse;
	image->missing = qfalse;
	R_IssueLoadPicLoaderCmd( image - images );
}

/*
* R_ShutdownImageLoader
*/
static void R_ShutdownImageLoader( void )
{
	if( !gl_loader_context ) {
		return;
	}

	R_IssueShutdownLoaderCmd();

	ri.BufQueue_Finish( loader_queue );

	ri.Thread_Join( loader_thread );
	loader_thread = NULL;

	ri.BufQueue_Destroy( &loader_queue );

	GLimp_SharedContext_Destroy( gl_loader_context );
	gl_loader_context = NULL;
}

//

/*
* R_HandleInitLoaderCmd
*/
static unsigned R_HandleInitLoaderCmd( void *pcmd )
{
	GLimp_SharedContext_MakeCurrent( gl_loader_context );

	RB_SelectContextTexture( 0 );

	return sizeof( int );
}

/*
* R_HandleShutdownLoaderCmd
*/
static unsigned R_HandleShutdownLoaderCmd( void *pcmd )
{
	GLimp_SharedContext_MakeCurrent( NULL );

	return 0;
}

/*
* R_HandleLoadPicLoaderCmd
*/
static unsigned R_HandleLoadPicLoaderCmd( void *pcmd )
{
	loaderPicCmd_t *cmd = pcmd;
	image_t *image = images + cmd->pic;
	R_LoadImageFromDisk( GL_CONTEXT_LOADER, image );
	return sizeof( *cmd );
}

/*
* R_ImageLoaderThreadProc
*/
static void *R_ImageLoaderThreadProc( void *param )
{
	qbufQueue_t *cmdQueue = param;
	queueCmdHandler_t cmdHandlers[NUM_LOADER_CMDS] = 
	{
		(queueCmdHandler_t)R_HandleInitLoaderCmd,
		(queueCmdHandler_t)R_HandleShutdownLoaderCmd,
		(queueCmdHandler_t)R_HandleLoadPicLoaderCmd
	};

	while ( 1 ){
		int read = ri.BufQueue_ReadCmds( cmdQueue, cmdHandlers );
		
		if( read < 0 ) {
			// shutdown
			break;
		}

		ri.Sys_Sleep( 2 );
	}
 
	return NULL;	
}
