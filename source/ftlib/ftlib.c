/*
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2015 Chasseur de bots

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

#include "ftlib_local.h"

static qfontfamily_t *fontFamilies;

// ============================================================================

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H
#include FT_SIZES_H

#define QFT_DIR                     "fonts"
#define QFT_DIR_FALLBACK            "fonts/fallback"
#define QFT_FILE_EXTENSION_TRUETYPE ".ttf"
#define QFT_FILE_EXTENSION_OPENTYPE ".otf"

#define QFTGLYPH_SEARCHED_MAIN      1           // the main font has been searched for the gindex
#define QFTGLYPH_SEARCHED_FALLBACK  ( 1 << 1 )  // the fallback font has been searched for the gindex
#define QFTGLYPH_FROM_FALLBACK      ( 1 << 2 )  // the fallback gindex should be used

static uint8_t *qftGlyphTempBitmap;
static unsigned int qftGlyphTempBitmapHeight;
#define QFT_GLYPH_BITMAP_HEIGHT_INCREMENT 64 // must be a power of two

FT_Library ftLibrary = NULL;

typedef struct qftfallback_s {
	FT_Size ftsize;
	unsigned int size;
	struct qftfallback_s *next;
} qftfallback_t;

typedef struct {
	FT_Byte *file;
	FT_Face ftface;
	qftfallback_t *fallbacks;
} qftfamily_t;

typedef struct {
	unsigned int imageCurX, imageCurY, imageCurLineHeight;

	FT_Size ftsize, ftfallbacksize;
	qfontfamily_t *fallbackFamily;
	bool fallbackLoaded;
} qftface_t;

typedef struct {
	qglyph_t qglyph;
	unsigned int flags;
	FT_UInt gindex;
} qftglyph_t;

static void *q_freetypeLibrary;

#ifdef FREETYPELIB_RUNTIME

static FT_Error ( *q_FT_New_Size )( FT_Face face, FT_Size* size );
static FT_Error (*q_FT_Activate_Size)( FT_Size size );
static FT_Error (*q_FT_Set_Pixel_Sizes)( FT_Face face, FT_UInt pixel_width, FT_UInt pixel_height );
static FT_Error (*q_FT_Done_Size)( FT_Size size );
static FT_UInt (*q_FT_Get_Char_Index)( FT_Face face, FT_ULong charcode );
static FT_Error (*q_FT_Get_Kerning)( FT_Face face, FT_UInt left_glyph, FT_UInt right_glyph, FT_UInt kern_mode, FT_Vector *akerning );
static FT_Error (*q_FT_Load_Glyph)( FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags );
static FT_Error (*q_FT_New_Memory_Face)( FT_Library library, const FT_Byte* file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface );
static FT_Error (*q_FT_Done_Face)( FT_Face face );
static FT_Error (*q_FT_Init_FreeType)( FT_Library  *alibrary );
static FT_Error (*q_FT_Done_FreeType)( FT_Library library );
#ifdef FT_MULFIX_INLINED
#define q_FT_MulFix FT_MulFix
#else
static FT_Long ( *q_FT_MulFix )( FT_Long a, FT_Long b );
#endif

static dllfunc_t freetypefuncs[] =
{
	{ "FT_New_Size", ( void **)&q_FT_New_Size },
	{ "FT_Activate_Size", ( void **)&q_FT_Activate_Size },
	{ "FT_Set_Pixel_Sizes", ( void **)&q_FT_Set_Pixel_Sizes },
	{ "FT_Done_Size", ( void **)&q_FT_Done_Size },
	{ "FT_Get_Char_Index", ( void **)&q_FT_Get_Char_Index },
	{ "FT_Get_Kerning", ( void **)&q_FT_Get_Kerning },
	{ "FT_Load_Glyph", ( void **)&q_FT_Load_Glyph },
	{ "FT_New_Memory_Face", ( void **)&q_FT_New_Memory_Face },
	{ "FT_Done_Face", ( void **)&q_FT_Done_Face },
	{ "FT_Init_FreeType", ( void **)&q_FT_Init_FreeType },
	{ "FT_Done_FreeType", ( void **)&q_FT_Done_FreeType },
#ifndef FT_MULFIX_INLINED
	{ "FT_MulFix", ( void **)&q_FT_MulFix },
#endif
	{ NULL, NULL },
};

#else

#define q_FT_New_Size FT_New_Size
#define q_FT_Activate_Size FT_Activate_Size
#define q_FT_Set_Pixel_Sizes FT_Set_Pixel_Sizes
#define q_FT_Done_Size FT_Done_Size
#define q_FT_Get_Char_Index FT_Get_Char_Index
#define q_FT_Get_Kerning FT_Get_Kerning
#define q_FT_Load_Glyph FT_Load_Glyph
#define q_FT_New_Memory_Face FT_New_Memory_Face
#define q_FT_Done_Face FT_Done_Face
#define q_FT_Init_FreeType FT_Init_FreeType
#define q_FT_Done_FreeType FT_Done_FreeType
#define q_FT_MulFix FT_MulFix

#endif

/*
* QFT_UnloadFreetypeLibrary
*/
static void QFT_UnloadFreetypeLibrary( void ) {
#ifdef FREETYPELIB_RUNTIME
	if( q_freetypeLibrary ) {
		trap_UnloadLibrary( &q_freetypeLibrary );
	}
#endif
	q_freetypeLibrary = NULL;
}

/*
* QFT_LoadFreetypeLibrary
*/
static void QFT_LoadFreetypeLibrary( void ) {
	QFT_UnloadFreetypeLibrary();

#ifdef FREETYPELIB_RUNTIME
	q_freetypeLibrary = trap_LoadLibrary( LIBFREETYPE_LIBNAME, freetypefuncs );
#else
	q_freetypeLibrary = (void *)1;
#endif
}

/*
* QFT_AllocGlyphs
*/
static void *QFT_AllocGlyphs( qfontface_t *qfont, wchar_t first, unsigned int count ) {
	return FTLIB_Alloc( ftlibPool, count * sizeof( qftglyph_t ) );
}

/*
* QFT_GetFallbackFace
*/
static qftfallback_t *QFT_GetFallbackFace( qfontfamily_t *qfamily, unsigned int size ) {
	qftfamily_t *qftfamily = ( ( qftfamily_t * )( qfamily->familydata ) );
	qftfallback_t *fallback;

	for( fallback = qftfamily->fallbacks; fallback; fallback = fallback->next ) {
		if( fallback->size == size ) {
			return fallback;
		}
	}

	if( !qftfamily->ftface ) {
		return NULL;
	}

	fallback = FTLIB_Alloc( ftlibPool, sizeof( qftfallback_t ) );

	q_FT_New_Size( qftfamily->ftface, &( fallback->ftsize ) );
	q_FT_Activate_Size( fallback->ftsize );
	q_FT_Set_Pixel_Sizes( qftfamily->ftface, size, 0 );

	fallback->size = size;
	fallback->next = qftfamily->fallbacks;
	qftfamily->fallbacks = fallback;

	return fallback;
}

/*
* QFT_GetGlyph
*/
static qglyph_t *QFT_GetGlyph( qfontface_t *qfont, void *glyphArray, unsigned int numInArray, wchar_t num ) {
	qftglyph_t *qftglyph = &( ( ( qftglyph_t * )glyphArray )[numInArray] );
	qftface_t *qttf = ( ( qftface_t * )( qfont->facedata ) );

	if( !qftglyph->gindex ) {
		if( !( qftglyph->flags & QFTGLYPH_SEARCHED_MAIN ) ) {
			qftglyph->flags |= QFTGLYPH_SEARCHED_MAIN;
			qftglyph->gindex = q_FT_Get_Char_Index( qttf->ftsize->face, num );
			if( qftglyph->gindex ) {
				return &( qftglyph->qglyph );
			}
		}

		if( qttf->fallbackFamily ) {
			if( !qttf->fallbackLoaded ) {
				qftfallback_t *fallback;

				qttf->fallbackLoaded = true;

				fallback = QFT_GetFallbackFace( qttf->fallbackFamily, qfont->size );
				if( !fallback ) {
					return NULL;
				}
				qttf->ftfallbacksize = fallback->ftsize;
				qfont->hasKerning |= ( FT_HAS_KERNING( qttf->ftfallbacksize->face ) ? true : false );
			}

			if( qttf->ftfallbacksize && !( qftglyph->flags & QFTGLYPH_SEARCHED_FALLBACK ) ) {
				qftglyph->flags |= QFTGLYPH_SEARCHED_FALLBACK;
				qftglyph->gindex = q_FT_Get_Char_Index( qttf->ftfallbacksize->face, num );
				if( qftglyph->gindex ) {
					qftglyph->flags |= QFTGLYPH_FROM_FALLBACK;
				}
			}
		}
	}

	return qftglyph->gindex ? &( qftglyph->qglyph ) : NULL;
}

/*
* QFT_GetKerning
*/
static int QFT_GetKerning( qfontface_t *qfont, qglyph_t *g1_, qglyph_t *g2_ ) {
	qftglyph_t *g1, *g2;
	FT_UInt gi1, gi2;
	qftface_t *qttf;
	FT_Size ftsize;
	FT_Vector kvec;

	g1 = ( qftglyph_t * )g1_;
	assert( g1 != NULL );
	if( !g1 ) {
		return 0;
	}
	gi1 = ( ( qftglyph_t * )g1 )->gindex;
	if( !gi1 ) {
		return 0;
	}

	g2 = ( qftglyph_t * )g2_;
	assert( g2 != NULL );
	if( !g2 ) {
		return 0;
	}
	gi2 = ( ( qftglyph_t * )g2 )->gindex;
	if( !gi2 ) {
		return 0;
	}

	if( ( g1->flags ^ g2->flags ) & QFTGLYPH_FROM_FALLBACK ) {
		return 0;
	}

	qttf = ( qftface_t * )( qfont->facedata );
	ftsize = ( ( g1->flags & QFTGLYPH_FROM_FALLBACK ) ? qttf->ftfallbacksize : qttf->ftsize );
	q_FT_Activate_Size( ftsize );
	q_FT_Get_Kerning( ftsize->face, gi1, gi2, FT_KERNING_DEFAULT, &kvec );
	return kvec.x >> 6;
}

/*
* QFT_UploadRenderedGlyphs
*/
static void QFT_UploadRenderedGlyphs( uint8_t *pic, struct shader_s *shader, int x, int y, int src_width, int width, int height ) {
	int i;
	const uint8_t *src = pic;
	uint8_t *dest = pic;

	if( !width || !height ) {
		return;
	}

	for( i = 0; i < height; i++, src += src_width, dest += width ) {
		memmove( dest, src, width );
	}
	trap_R_ReplaceRawSubPic( shader, x, y, width, height, pic );
}

/*
* QFT_RenderString
*/
static void QFT_RenderString( qfontface_t *qfont, const char *str ) {
	int gc;
	wchar_t num;
	qftface_t *qttf = ( qftface_t * )( qfont->facedata );
	qftglyph_t *qftglyph;
	qglyph_t *qglyph;
	FT_Error fterror;
	FT_Size ftsize;
	FT_GlyphSlot ftglyph;
	FT_UInt pixelMode;
	int srcStride = 0;
	unsigned int bitmapWidth, bitmapHeight;
	unsigned int tempWidth = 0, tempLineHeight = 0;
	struct shader_s *shader = qfont->shaders[qfont->numShaders - 1];
	int shaderNum;
	int x, y;
	uint8_t *src, *dest;

	for( ; ; ) {
		gc = Q_GrabWCharFromColorString( &str, &num, NULL );
		if( gc == GRABCHAR_END ) {
			QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, qfont->shaderWidth, tempWidth, tempLineHeight );
			qttf->imageCurX += tempWidth;
			break;
		}

		if( gc != GRABCHAR_CHAR ) {
			continue;
		}

		qftglyph = ( qftglyph_t * )FTLIB_GetGlyph( qfont, num );
		if( !qftglyph || qftglyph->qglyph.shader ) {
			continue;
		}

		qglyph = &( qftglyph->qglyph );
		if( qglyph->shader ) {
			continue;
		}

		// from now, it is assumed that the current glyph's shader will be valid after this function
		// so if continue is used, any shader, even an empty one, should be assigned to the glyph

		ftsize = ( ( qftglyph->flags & QFTGLYPH_FROM_FALLBACK ) ? qttf->ftfallbacksize : qttf->ftsize );
		q_FT_Activate_Size( ftsize );
		fterror = q_FT_Load_Glyph( ftsize->face, qftglyph->gindex, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL );
		if( fterror ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Failed to load and render glyph %i for '%s', error %i\n",
						num, qfont->family->name, fterror );
			qglyph->shader = shader;
			continue;
		}
		ftglyph = ftsize->face->glyph;

		pixelMode = ftglyph->bitmap.pixel_mode;
		switch( pixelMode ) {
			case FT_PIXEL_MODE_MONO:
				srcStride = Q_ALIGN( ftglyph->bitmap.width, 8 ) >> 3;
				break;
			case FT_PIXEL_MODE_GRAY:
				srcStride = ftglyph->bitmap.width;
				break;
			default:
				assert( 0 );
		}

		bitmapWidth = ftglyph->bitmap.width + 2;
		bitmapHeight = ftglyph->bitmap.rows + 2;
		if( bitmapWidth > qfont->shaderWidth ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Width limit exceeded for '%s' character %i - %i\n",
						qfont->family->name, num, bitmapWidth - 2 );
			bitmapWidth = qfont->shaderWidth;
		}
		if( bitmapHeight > qfont->shaderHeight ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Height limit exceeded for '%s' character %i - %i\n",
						qfont->family->name, num, bitmapHeight - 2 );
			bitmapHeight = qfont->shaderHeight;
		}

		if( ( qttf->imageCurX + tempWidth + bitmapWidth ) > qfont->shaderWidth ) {
			QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, qfont->shaderWidth, tempWidth, tempLineHeight );
			tempWidth = 0;
			tempLineHeight = 0;
			qttf->imageCurX = 0;
			qttf->imageCurY += qttf->imageCurLineHeight - 1; // overlap the previous line's margin
			qttf->imageCurLineHeight = 0;
		}

		if( bitmapHeight > qftGlyphTempBitmapHeight ) {
			qftGlyphTempBitmapHeight = Q_ALIGN( bitmapHeight, QFT_GLYPH_BITMAP_HEIGHT_INCREMENT );
			qftGlyphTempBitmap = FTLIB_Realloc( qftGlyphTempBitmap, FTLIB_FONT_MAX_IMAGE_WIDTH * qftGlyphTempBitmapHeight );
		}

		if( bitmapHeight > tempLineHeight ) {
			if( bitmapHeight > qttf->imageCurLineHeight ) {
				if( ( qttf->imageCurY + bitmapHeight ) > qfont->shaderHeight ) {
					QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, qfont->shaderWidth, tempWidth, tempLineHeight );
					tempWidth = 0;
					qttf->imageCurX = 0;
					qttf->imageCurY = 0;
					shaderNum = ( qfont->numShaders )++;
					shader = trap_R_RegisterRawAlphaMask( FTLIB_FontShaderName( qfont, shaderNum ),
														  qfont->shaderWidth, qfont->shaderHeight, NULL );
					qfont->shaders = FTLIB_Realloc( qfont->shaders, qfont->numShaders * sizeof( struct shader_s * ) );
					qfont->shaders[shaderNum] = shader;
				}
				qttf->imageCurLineHeight = bitmapHeight;
			}
			tempLineHeight = bitmapHeight;
		}

		qglyph->width = bitmapWidth - 2;
		qglyph->height = bitmapHeight - 2;
		qglyph->x_advance = ( ftglyph->advance.x + ( 1 << 5 ) ) >> 6;
		qglyph->x_offset = ftglyph->bitmap_left;
		qglyph->y_offset = -( (int)( ftglyph->bitmap_top ) );
		qglyph->shader = shader;
		qglyph->s1 = ( float )( qttf->imageCurX + tempWidth + 1 ) / ( float )qfont->shaderWidth;
		qglyph->t1 = ( float )( qttf->imageCurY + 1 ) / ( float )qfont->shaderHeight;
		qglyph->s2 = ( float )( qttf->imageCurX + tempWidth + 1 + qglyph->width ) / ( float )qfont->shaderWidth;
		qglyph->t2 = ( float )( qttf->imageCurY + 1 + qglyph->height ) / ( float )qfont->shaderHeight;

		src = ftglyph->bitmap.buffer;
		dest = qftGlyphTempBitmap + tempWidth;
		memset( dest, 0, bitmapWidth );
		dest += qfont->shaderWidth;
		for( y = 0; y < qglyph->height; ++y, src += srcStride ) {
			dest[0] = 0;
			switch( pixelMode ) {
				case FT_PIXEL_MODE_MONO:
					for( x = 0; x < qglyph->width; x++ ) {
						dest[x + 1] = ( ( ( ( unsigned int )( src[x >> 3] ) ) >> ( 7 - ( x & 7 ) ) ) & 1 ) * 255;
					}
					break;
				case FT_PIXEL_MODE_GRAY:
					memcpy( dest + 1, src, qglyph->width );
					break;
				default:
					// shouldn't happen actually, but make it a valid glyph anyway
					if( !y || ( y == qglyph->height ) ) {
						memset( dest + 1, 255, qglyph->width );
					} else {
						dest[1] = dest[qglyph->width] = 255;
						memset( dest + 1, 0, qglyph->width );
					}
					break;
			}
			dest[qglyph->width + 1] = 0;
			dest += qfont->shaderWidth;
		}
		memset( dest, 0, bitmapWidth );

		tempWidth += bitmapWidth - 1; // overlap the previous character's margin
	}
}

static void QFT_SetFallback( qfontface_t *qfont, qfontfamily_t *qfamily ) {
	qftface_t *qttf = ( qftface_t * )( qfont->facedata );

	if( !qttf->fallbackFamily ) {
		qttf->fallbackFamily = qfamily;
	}
}

static const qfontface_funcs_t qft_face_funcs =
{
	QFT_AllocGlyphs,
	QFT_GetGlyph,
	QFT_RenderString,
	QFT_GetKerning,
	QFT_SetFallback
};

/*
* QFT_LoadFace
*/
static qfontface_t *QFT_LoadFace( qfontfamily_t *family, unsigned int size ) {
	unsigned int i;
	int fontHeight;
	float unitScale;
	bool hasKerning;
	qftfamily_t *qftfamily = ( qftfamily_t * )( family->familydata );
	FT_Face ftface = qftfamily->ftface;
	FT_Size ftsize;
	qftface_t *qttf = NULL;
	qfontface_t *qfont = NULL;
	char renderStr[FTLIB_NUM_ASCII_CHARS + 1];
	int shaderWidth, shaderHeight;
	int maxShaderWidth, maxShaderHeight;

	// set the font size
	q_FT_New_Size( ftface, &ftsize );
	q_FT_Activate_Size( ftsize );
	q_FT_Set_Pixel_Sizes( ftface, 0, size );

	hasKerning = FT_HAS_KERNING( ftface ) ? true : false;

	// we are going to need this for kerning
	qttf = FTLIB_Alloc( ftlibPool, sizeof( *qttf ) );
	qttf->ftsize = ftsize;

	// use scaled version of the original design text height (the vertical
	// distance from one baseline to the next) as font height
	fontHeight = ftsize->metrics.height >> 6;
	unitScale = ( float )fontHeight / ( float )ftface->units_per_EM;

	// store font info
	qfont = FTLIB_Alloc( ftlibPool, sizeof( qfontface_t ) );
	qfont->family = family;
	qfont->size = size;
	qfont->height = fontHeight;
	qfont->advance = ( ( q_FT_MulFix( ftface->max_advance_width, ftsize->metrics.x_scale ) ) >> 6 );
	qfont->glyphYOffset = ftsize->metrics.ascender >> 6;
	qfont->underlineThickness = ftface->underline_thickness * unitScale + 0.5f;
	if( qfont->underlineThickness <= 0 ) {
		qfont->underlineThickness = 1;
	}
	qfont->underlinePosition = qfont->glyphYOffset -
							   ( int )( ftface->underline_position * unitScale ) - ( qfont->underlineThickness >> 1 );

	maxShaderWidth = FTLIB_FONT_MAX_IMAGE_WIDTH;
	if( fontHeight > 48 ) {
		maxShaderHeight = FTLIB_FONT_IMAGE_HEIGHT_LARGE;
	} else if( fontHeight > 24 ) {
		maxShaderHeight = FTLIB_FONT_IMAGE_HEIGHT_MEDIUM;
	} else {
		maxShaderHeight = FTLIB_FONT_IMAGE_HEIGHT_SMALL;
	}

	if( ftface->num_glyphs < FTLIB_NUM_ASCII_CHARS ) {
		int pow2;
		int maxAdvanceX, maxAdvanceY;
		int numCols, numRows;

		// calculate estimate on texture size
		maxAdvanceX = ( ( ( q_FT_MulFix( ftface->max_advance_width, ftsize->metrics.x_scale ) + 63 ) & ~63 ) >> 6 ) + 2;
		maxAdvanceY = ( ( ( q_FT_MulFix( ftface->max_advance_height, ftsize->metrics.y_scale ) + 63 ) & ~63 ) >> 6 ) + 2;

		numCols = maxShaderWidth / maxAdvanceX;
		Q_clamp( numCols, 1, ftface->num_glyphs );

		numRows = ftface->num_glyphs / numCols;

		shaderWidth = min( numCols * maxAdvanceX, maxShaderWidth );
		shaderHeight = min( numRows * maxAdvanceY, maxShaderHeight ) ;

		// round to the next power of 2
		for( pow2 = 1; pow2 < shaderWidth; pow2 <<= 1 ) ;
		qfont->shaderWidth = pow2;

		for( pow2 = 1; pow2 < shaderHeight; pow2 <<= 1 ) ;
		qfont->shaderHeight = pow2;
	} else {
		// assume we will eventually need some space to render fallback glyphs
		// for less common chars such as CJK
		qfont->shaderWidth = maxShaderWidth;
		qfont->shaderHeight = maxShaderHeight;
	}

	qfont->numShaders = 1;
	qfont->shaders = FTLIB_Alloc( ftlibPool, sizeof( struct shader_s * ) );
	qfont->shaders[0] = trap_R_RegisterRawAlphaMask( FTLIB_FontShaderName( qfont, 0 ),
													 qfont->shaderWidth, qfont->shaderHeight, NULL );
	qfont->hasKerning = hasKerning;
	qfont->f = &qft_face_funcs;
	qfont->facedata = ( void * )qttf;
	qfont->next = family->faces;
	family->faces = qfont;

	// pre-render 32-126
	for( i = 0; i < FTLIB_NUM_ASCII_CHARS; i++ ) {
		renderStr[i] = FTLIB_FIRST_ASCII_CHAR + i;
	}
	renderStr[i] = '\0';
	QFT_RenderString( qfont, renderStr );

	return qfont;
}

/*
* QFT_UnloadFace
*/
static void QFT_UnloadFace( qfontface_t *qfont ) {
	qftface_t *qttf;

	qttf = ( qftface_t * )qfont->facedata;
	if( !qttf ) {
		return;
	}

	q_FT_Done_Size( qttf->ftsize );

	FTLIB_Free( qttf );
}

static void QFT_UnloadFamily( qfontfamily_t *qfamily ) {
	qftfamily_t *qftfamily = ( qftfamily_t * )( qfamily->familydata );
	qftfallback_t *fallback, *nextfallback;

	if( !qftfamily ) {
		return;
	}

	for( fallback = qftfamily->fallbacks; fallback; fallback = nextfallback ) {
		nextfallback = fallback->next;
		if( fallback->ftsize ) {
			q_FT_Done_Size( fallback->ftsize );
		}
		FTLIB_Free( fallback );
	}

	if( qftfamily->ftface ) {
		q_FT_Done_Face( qftfamily->ftface );
		qftfamily->ftface = NULL;
	}

	FTLIB_Free( qftfamily->file );
}

static const qfontfamily_funcs_t qft_family_funcs =
{
	QFT_LoadFace,
	QFT_UnloadFace,
	QFT_UnloadFamily
};

/*
* QFT_LoadFamily
*/
static bool QFT_LoadFamily( const char *fileName, uint8_t *data, size_t dataSize, bool verbose, bool fallback ) {
	FT_Face ftface;
	int error;
	const char *familyName;
	const char *styleName;
	qfontfamily_t *qfamily;
	qftfamily_t *qftfamily;

	ftface = NULL;
	error = q_FT_New_Memory_Face( ftLibrary, ( const FT_Byte* )data, dataSize, 0, &ftface );
	if( error != 0 ) {
		if( verbose ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Error loading font face '%s': %i\n", fileName, error );
		}
		return false;
	}

	familyName = ftface->family_name;
	styleName = ftface->style_name;

	// check if the font has the replacement glyph
	if( !q_FT_Get_Char_Index( ftface, FTLIB_REPLACEMENT_GLYPH ) ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Font face '%s' doesn't have the replacement glyph %i\n",
					familyName, FTLIB_REPLACEMENT_GLYPH );
		q_FT_Done_Face( ftface );
		return false;
	}

	// exit if this is not a scalable font
	if( !( ftface->face_flags & FT_FACE_FLAG_SCALABLE ) || !( ftface->face_flags & FT_FACE_FLAG_HORIZONTAL ) ) {
		if( verbose ) {
			Com_Printf( S_COLOR_YELLOW "Warning: '%s' is not a scalable font face\n", familyName );
		}
		q_FT_Done_Face( ftface );
		return false;
	}

	qftfamily = FTLIB_Alloc( ftlibPool, sizeof( qftfamily_t ) );
	qftfamily->ftface = ftface;
	qftfamily->file = data;

	qfamily = FTLIB_Alloc( ftlibPool, sizeof( qfontfamily_t ) );
	qfamily->numFaces = 0;
	qfamily->name = FTLIB_CopyString( familyName );
	qfamily->f = &qft_family_funcs;
	qfamily->style = QFONT_STYLE_NONE;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_ITALIC ? QFONT_STYLE_ITALIC : 0;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_BOLD ? QFONT_STYLE_BOLD : 0;
	qfamily->fallback = fallback;
	qfamily->familydata = qftfamily;
	qfamily->next = fontFamilies;
	fontFamilies = qfamily;

	if( verbose ) {
		Com_Printf( "Loaded font '%s %s' from '%s'\n", familyName, styleName, fileName );
	}
	return true;
}

/*
* QFT_LoadFamilyFromFile
*/
static void QFT_LoadFamilyFromFile( const char *name, const char *fileName, bool verbose, bool fallback ) {
	int fileNum;
	int length;
	uint8_t *buffer;

	length = trap_FS_FOpenFile( fileName, &fileNum, FS_READ );
	if( length < 0 ) {
		return;
	}

	buffer = ( uint8_t * )FTLIB_Alloc( ftlibPool, length );
	trap_FS_Read( buffer, length, fileNum );

	if( !QFT_LoadFamily( name, buffer, length, verbose, fallback ) ) {
		FTLIB_Free( buffer );
	}

	trap_FS_FCloseFile( fileNum );
}

/*
* QFT_PrecacheFonts
*
* Load fonts given type, storing family name, style, size
*/
static void QFT_PrecacheFontsByExt( bool verbose, const char *ext, bool fallback ) {
	int i, j;
	const char *dir = ( fallback ? QFT_DIR_FALLBACK : QFT_DIR );
	int numfiles;
	char buffer[1024];
	char fileName[1024];
	char *s;
	size_t length;

	assert( ftLibrary != NULL );
	if( ftLibrary == NULL ) {
		//Com_Printf( S_COLOR_RED "Error: TTF_LoadFonts called prior initializing FreeType\n" );
		return;
	}

	if( ( numfiles = trap_FS_GetFileList( dir, ext, NULL, 0, 0, 0 ) ) == 0 ) {
		return;
	}

	i = 0;
	length = 0;
	do {
		if( ( j = trap_FS_GetFileList( dir, ext, buffer, sizeof( buffer ), i, numfiles ) ) == 0 ) {
			// can happen if the filename is too long to fit into the buffer or we're done
			i++;
			continue;
		}

		i += j;
		for( s = buffer; j > 0; j--, s += length + 1 ) {
			length = strlen( s );
			Q_strncpyz( fileName, va( "%s/%s", dir, s ), sizeof( fileName ) );

			QFT_LoadFamilyFromFile( s, fileName, verbose, fallback );
		}
	} while( i < numfiles );
}

/*
* QFT_PrecacheFonts
*/
static void QFT_PrecacheFonts( bool verbose ) {
	QFT_PrecacheFontsByExt( verbose, QFT_FILE_EXTENSION_TRUETYPE, false );
	QFT_PrecacheFontsByExt( verbose, QFT_FILE_EXTENSION_OPENTYPE, false );
	QFT_PrecacheFontsByExt( verbose, QFT_FILE_EXTENSION_TRUETYPE, true );
	QFT_PrecacheFontsByExt( verbose, QFT_FILE_EXTENSION_OPENTYPE, true );
}

/*
* QFT_Init
*/
static void QFT_Init( bool verbose ) {
	int error;

	QFT_LoadFreetypeLibrary();

	if( q_freetypeLibrary ) {
		assert( ftLibrary == NULL );
		error = q_FT_Init_FreeType( &ftLibrary );
	} else {
		error = 1;
	}

	if( error != 0 ) {
		ftLibrary = NULL;
		if( verbose ) {
			Com_Printf( S_COLOR_RED "Error initializing FreeType library: %i\n", error );
		}
	}

	assert( !qftGlyphTempBitmap );
	qftGlyphTempBitmap = FTLIB_Alloc( ftlibPool, FTLIB_FONT_MAX_IMAGE_WIDTH * QFT_GLYPH_BITMAP_HEIGHT_INCREMENT );
	qftGlyphTempBitmapHeight = QFT_GLYPH_BITMAP_HEIGHT_INCREMENT;
}

/*
* QFT_Shutdown
*/
static void QFT_Shutdown( void ) {
	if( ftLibrary != NULL ) {
		q_FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}

	if( qftGlyphTempBitmap ) {
		FTLIB_Free( qftGlyphTempBitmap );
		qftGlyphTempBitmap = NULL;
		qftGlyphTempBitmapHeight = 0;
	}

	QFT_UnloadFreetypeLibrary();
}

// ============================================================================

/*
* FTLIB_InitSubsystems
*/
void FTLIB_InitSubsystems( bool verbose ) {
	QFT_Init( verbose );
}

/*
* FTLIB_PrecacheFonts
*/
void FTLIB_PrecacheFonts( bool verbose ) {
	QFT_PrecacheFonts( verbose );
}

/*
* FTLIB_GetRegisterFontFamily
*/
static qfontfamily_t *FTLIB_GetRegisterFontFamily( const char *family, int style, unsigned int size, bool fallback ) {
	qfontfamily_t *qfamily, *best;
	int bestStyle;

	best = NULL;
	bestStyle = QFONT_STYLE_MASK + 1;
	for( qfamily = fontFamilies; qfamily; qfamily = qfamily->next ) {
		if( ( qfamily->fallback != fallback ) || Q_stricmp( qfamily->name, family ) ) {
			continue;
		}
		if( qfamily->style == style ) {
			best = qfamily;
			break;
		}
		if( qfamily->style < bestStyle ) {
			best = qfamily;
		}
	}

	qfamily = best;
	if( qfamily == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Unknown font family '%s'\n", family );
	}

	return qfamily;
}

/*
* FTLIB_RegisterFont
*/
qfontface_t *FTLIB_RegisterFont( const char *family, const char *fallback, int style, unsigned int size ) {
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	assert( family != NULL );
	if( !family || !*family ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Tried to register an empty font family\n" );
		return NULL;
	}

	qfamily = FTLIB_GetRegisterFontFamily( family, style, size, false );
	if( !qfamily ) {
		return NULL;
	}

	// find the best matching font style of the same size
	for( qface = qfamily->faces; qface; qface = qface->next ) {
		if( qface->size == size ) {
			// exact match
			FTLIB_TouchFont( qface );
			break;
		}
	}

	if( !qface ) {
		qface = qfamily->f->loadFace( qfamily, size );
	}

	if( !qface ) {
		return NULL;
	}

	if( qface->hasKerning && !qface->f->getKerning ) {
		qface->hasKerning = false;
	}

	if( fallback && *fallback && qface->f->setFallback ) {
		qfamily = FTLIB_GetRegisterFontFamily( fallback, style, size, true );
		if( qfamily ) {
			qface->f->setFallback( qface, qfamily );
		}
	}

	return qface;
}

/*
* FTLIB_TouchFont
*/
void FTLIB_TouchFont( qfontface_t *qfont ) {
	unsigned int i;

	for( i = 0; i < qfont->numShaders; i++ ) {
		trap_R_RegisterPic( FTLIB_FontShaderName( qfont, i ) );
	}
}

/*
* FTLIB_TouchAllFonts
*/
void FTLIB_TouchAllFonts( void ) {
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	// touch all font families
	for( qfamily = fontFamilies; qfamily; qfamily = qfamily->next ) {
		// touch all faces for this family
		for( qface = qfamily->faces; qface; qface = qface->next ) {
			FTLIB_TouchFont( qface );
		}
	}
}

/*
* FTLIB_FreeFonts
*/
void FTLIB_FreeFonts( bool verbose ) {
	unsigned int i;
	qfontfamily_t *qfamily, *nextqfamily;
	qfontface_t *qface, *nextqface;

	// unload all font families
	for( qfamily = fontFamilies; qfamily; qfamily = nextqfamily ) {
		nextqfamily = qfamily->next;

		// unload all faces for this family
		for( qface = qfamily->faces; qface; qface = nextqface ) {
			nextqface = qface->next;

			if( qfamily->f->unloadFace ) {
				qfamily->f->unloadFace( qface );
			}

			if( qface->shaders ) {
				FTLIB_Free( qface->shaders );
			}

			for( i = 0; i < ( sizeof( qface->glyphs ) / sizeof( qface->glyphs[0] ) ); i++ ) {
				if( qface->glyphs[i] ) {
					FTLIB_Free( qface->glyphs[i] );
				}
			}

			FTLIB_Free( qface );
		}

		if( qfamily->f->unloadFamily ) {
			qfamily->f->unloadFamily( qfamily );
		}
		if( qfamily->name ) {
			FTLIB_Free( qfamily->name );
		}

		FTLIB_Free( qfamily );
	}

	fontFamilies = NULL;
}

/*
* FTLIB_ShutdownSubsystems
*/
void FTLIB_ShutdownSubsystems( bool verbose ) {
	QFT_Shutdown();
}

/*
* FTLIB_PrintFontList
*/
void FTLIB_PrintFontList( void ) {
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	Com_Printf( "Font families:\n" );

	for( qfamily = fontFamilies; qfamily; qfamily = qfamily->next ) {
		Com_Printf( "%s%s%s%s\n", qfamily->name,
					qfamily->fallback ? " (fallback)" : "",
					qfamily->style & QFONT_STYLE_ITALIC ? " (italic)" : "",
					qfamily->style & QFONT_STYLE_BOLD ? " (bold)" : "" );

		// print all faces for this family
		for( qface = qfamily->faces; qface; qface = qface->next ) {
			Com_Printf( "* size: %ipt, height: %ipx, images: %i (%ix%i)\n",
						qface->size, qface->height, qface->numShaders, qface->shaderWidth, qface->shaderHeight );
		}
	}
}

/*
* FTLIB_GetGlyph
*
* Gets a pointer to the glyph for its charcode, loads it if needed, or returns NULL if it's missing.
*/
qglyph_t *FTLIB_GetGlyph( qfontface_t *font, wchar_t num ) {
	void *glyphs;

	if( ( num < ' ' ) || ( num > 0xffff ) ) {
		return NULL;
	}

	glyphs = font->glyphs[num >> 8];
	if( !glyphs ) {
		glyphs = font->f->allocGlyphs( font, num & 0xff00, 256 );
		font->glyphs[num >> 8] = glyphs;
	}

	return font->f->getGlyph( font, glyphs, num & 255, num );
}

/*
* FTLIB_FontShaderName
*
* Gets the name of the shader containing the glyphs for the font.
*/
const char *FTLIB_FontShaderName( qfontface_t *qfont, unsigned int shaderNum ) {
	static char name[MAX_QPATH];

	Q_snprintfz( name, sizeof( name ), "Font %s %i %i %i",
				 qfont->family->name, qfont->size, qfont->family->style, shaderNum );

	return name;
}
