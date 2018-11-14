/*
Copyright (C) 2013 Victor Luchits

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

#ifndef R_IMAGE_H
#define R_IMAGE_H

enum {
	IT_NONE,
	IT_CLAMP             = 1 << 0,
	IT_NOMIPMAP          = 1 << 1,
	IT_SKY               = 1 << 2,
	IT_CUBEMAP           = 1 << 3,
	IT_FLIPX             = 1 << 4,
	IT_FLIPY             = 1 << 5,
	IT_FLIPDIAGONAL      = 1 << 6,     // when used alone, equals to rotating 90 CW and flipping X; with FLIPX|Y, 90 CCW and flipping X
	IT_DEPTH             = 1 << 7,
	IT_NORMALMAP         = 1 << 8,
	IT_FRAMEBUFFER       = 1 << 9,
	IT_DEPTHRB           = 1 << 10,    // framebuffer has a depth renderbuffer
	IT_NOFILTERING       = 1 << 11,
	IT_ALPHAMASK         = 1 << 12,    // image only contains an alpha mask
	IT_BGRA              = 1 << 13,
	IT_SYNC              = 1 << 14,    // load image synchronously
	IT_DEPTHCOMPARE      = 1 << 15,
	IT_ARRAY             = 1 << 16,
	IT_3D                = 1 << 17,
	IT_STENCIL           = 1 << 18,    // for IT_DEPTH or IT_DEPTHRB textures, whether there's stencil
	IT_NO_DATA_SYNC      = 1 << 19,    // owned by the drawing thread, do not sync in the frontend thread
	IT_FLOAT             = 1 << 20,
	IT_SRGB              = 1 << 21,
	IT_MIPTEX            = 1 << 22,
	IT_MIPTEX_FULLBRIGHT = 1 << 23,
	IT_LEFTHALF          = 1 << 24,
	IT_RIGHTHALF         = 1 << 25,
};

/**
 * These flags don't effect the actual usage and purpose of the image.
 * They are ignored when searching for an image.
 * The loader threads may modify these flags (but no other flags),
 * so they must not be used for anything that has a long-term effect.
 */
#define IT_LOADFLAGS        ( IT_ALPHAMASK | IT_BGRA | IT_SYNC | IT_SRGB )

#define IT_SPECIAL          ( IT_CLAMP | IT_NOMIPMAP )
#define IT_SKYFLAGS         ( IT_SKY | IT_NOMIPMAP | IT_CLAMP | IT_SYNC )

/**
 * Image usage tags, to allow certain images to be freed separately.
 */
enum {
	IMAGE_TAG_GENERIC   = 1 << 0      // Images that don't fall into any other category.
	,IMAGE_TAG_BUILTIN  = 1 << 1      // Internal ref images that must not be released.
	,IMAGE_TAG_WORLD    = 1 << 2      // World textures.
};

typedef struct image_s {
	char            *name;                      // game path, not including extension
	int registrationSequence;
	volatile bool loaded;
	volatile bool missing;

	char extension[8];                          // file extension
	int flags;
	unsigned texnum;                              // gl texture binding
	int width, height;                          // source image
	int layers;                                 // texture array size
	int upload_width,
		upload_height;                          // after power of two and picmip
	int minmipsize;                             // size of the smallest mipmap that should be used
	int samples;
	int fbo;                                    // frame buffer object texture is attached to
	unsigned int framenum;                      // rf.frameCount texture was updated (rendered to)
	int tags;                                   // usage tags of the image
	struct image_s  *next, *prev;
} image_t;

void R_InitImages( void );
int R_TextureTarget( int flags, int *uploadTarget );
void R_TouchImage( image_t *image, int tags );
void R_FreeUnusedImagesByTags( int tags );
void R_FreeUnusedImages( void );
void R_InitBuiltinScreenImages( void );
void R_ReleaseBuiltinScreenImages( void );
void R_ShutdownImages( void );
void R_GetRenderBufferSize( const int inWidth, const int inHeight,
							const int inLimit, const int flags, int *outWidth, int *outHeight );
void R_InitViewportTexture( image_t **texture, const char *name, int id,
							int viewportWidth, int viewportHeight, int size, int flags, int tags, int samples );
image_t *R_GetPortalTexture( int viewportWidth, int viewportHeight, int flags, unsigned frameNum );
void R_InitDrawFlatTexture( void );
void R_FreeImageBuffers( void );

void R_PrintImageList( const char *pattern, bool ( *filter )( const char *filter, const char *value ) );
void R_ScreenShot( const char *filename, int x, int y, int width, int height, int quality,
				   bool flipx, bool flipy, bool flipdiagonal, bool silent );

void R_AnisotropicFilter( int value );

image_t *R_LoadImage( const char *name, uint8_t **pic, int width, int height, int flags, int minmipsize, int tags, int samples );
image_t *R_FindImage( const char *name, const char *suffix, int flags, int minmipsize, int tags );
image_t *R_Create3DImage( const char *name, int width, int height, int layers, int flags, int tags, int samples, bool array );
void R_ReplaceImage( image_t *image, uint8_t **pic, int width, int height, int flags, int minmipsize, int samples );
void R_ReplaceSubImage( image_t *image, int layer, int x, int y, uint8_t **pic, int width, int height );
void R_ReplaceImageLayer( image_t *image, int layer, uint8_t **pic );
unsigned *R_LoadPalette( int flags );

#endif // R_IMAGE_H
