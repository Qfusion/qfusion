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

#define QFT_DIR						"fonts"
#define QFT_DIR_FALLBACK			"fonts/fallback"
#define FT_FILE_EXTENSION_TRUETYPE	".ttf"
#define FT_FILE_EXTENSION_OPENTYPE	".otf"

static uint8_t *qftGlyphTempBitmap;
static unsigned int qftGlyphTempBitmapHeight;
#define QFT_GLYPH_BITMAP_HEIGHT_INCREMENT 64 // must be a power of two

FT_Library ftLibrary = NULL;

typedef struct
{
	FT_Face ftface;
	unsigned int imageCurX, imageCurY, imageCurLineHeight;
} qftface_t;

typedef struct
{
	qglyph_t qglyph;
	FT_UInt gindex;
} qftglyph_t;

/*
* QFT_AllocGlyphs
*/
void *QFT_AllocGlyphs( qfontface_t *qfont, qwchar first, unsigned int count )
{
	return FTLIB_Alloc( ftlibPool, count * sizeof( qftglyph_t ) );
}

/*
* QFT_LoadGlyph
*/
void QFT_LoadGlyph( qfontface_t *qfont, qwchar num )
{
	( ( qftglyph_t * )( FTLIB_GetGlyph( qfont, num ) ) )->gindex =
		FT_Get_Char_Index( ( ( qftface_t * )( qfont->facedata ) )->ftface, num );
}

/*
* QFT_GetGlyph
*/
qglyph_t *QFT_GetGlyph( qfontface_t *qfont, void *glyphArray, unsigned int numInArray, qwchar num )
{
	return &( ( ( qftglyph_t * )glyphArray )[numInArray].qglyph );
}

/*
* QFT_GetKerning
*/
static int QFT_GetKerning( qfontface_t *qfont, qwchar char1, qwchar char2 )
{
	qftface_t *qttf;
	qglyph_t *g1, *g2;
	FT_UInt gi1, gi2;
	FT_Vector kvec;

	qttf = ( qftface_t * )qfont->facedata;

	g1 = FTLIB_GetGlyph( qfont, char1 );
	if( !g1 ) {
		g1 = FTLIB_LoadGlyph( qfont, char1 );
	}
	gi1 = ( ( qftglyph_t * )g1 )->gindex;
	if( !gi1 ) {
		return 0;
	}

	g2 = FTLIB_GetGlyph( qfont, char2 );
	if( !g2 ) {
		g2 = FTLIB_LoadGlyph( qfont, char2 );
	}
	gi2 = ( ( qftglyph_t * )g2 )->gindex;
	if( !gi2 ) {
		return 0;
	}

	FT_Get_Kerning( qttf->ftface, gi1, gi2, FT_KERNING_DEFAULT, &kvec );
	return kvec.x >> 6;
}

/*
* QFT_UploadRenderedGlyphs
*/
static void QFT_UploadRenderedGlyphs( uint8_t *pic, struct shader_s *shader, int x, int y, int width, int height )
{
	int i;
	const uint8_t *src = pic;
	uint8_t *dest = pic;

	if ( !width || !height )
		return;

	for( i = 0; i < height; i++, src += FTLIB_FONT_IMAGE_WIDTH, dest += width ) {
		memmove( dest, src, width );
	}
	trap_R_ReplaceRawSubPic( shader, x, y, width, height, pic );
}

/*
* QFT_GetRenderableGlyph
*
* Returns the glyph if it needs to be rendered.
*/
static qftglyph_t *QFT_GetRenderableGlyph( qfontface_t *qfont, qfontface_t *mainqfont, qwchar num )
{
	qftglyph_t *qftglyph;

	qftglyph = ( qftglyph_t * )FTLIB_GetGlyph( qfont, num );
	if( qftglyph ) {
		return ( ( !qftglyph->qglyph.shader && qftglyph->gindex ) ? qftglyph : NULL );
	}

	if( mainqfont && ( mainqfont != qfont ) ) {
		qftglyph = ( qftglyph_t * )FTLIB_GetGlyph( mainqfont, num );
		if( qftglyph && qftglyph->gindex ) {
			return NULL;
		}
	}

	qftglyph = ( qftglyph_t * )FTLIB_LoadGlyph( qfont, num );
	return ( qftglyph->gindex ? qftglyph : NULL );
}

/*
* QFT_GlyphNeedsRendering
*
* Returns whether the glyph for the number needs to be rendered.
*/
bool QFT_GlyphNeedsRendering( qfontface_t *qfont, qfontface_t *mainqfont, qwchar num )
{
	return ( QFT_GetRenderableGlyph( qfont, mainqfont, num ) ? true : false );
}

/*
* QFT_RenderString
*/
static void QFT_RenderString( qfontface_t *qfont, qfontface_t *mainqfont, const char *str )
{
	int gc;
	qwchar num;
	qftface_t *qttf = ( qftface_t * )( qfont->facedata );
	qftglyph_t *qftglyph;
	qglyph_t *qglyph;
	FT_Face ftface = qttf->ftface;
	FT_GlyphSlot ftglyph;
	FT_UInt pixelMode;
	int srcStride = 0;
	unsigned int bitmapWidth, bitmapHeight;
	unsigned int tempWidth = 0, tempLineHeight = 0;
	struct shader_s *shader = NULL;
	int shaderNum;
	float invHeight = 1.0f / ( float )( qfont->shaderHeight );
	int x, y;
	uint8_t *src, *dest;

	if( qfont->numShaders ) {
		shader = qfont->shaders[qfont->numShaders - 1];
	}

	for( ; ; ) {
		gc = Q_GrabWCharFromColorString( &str, &num, NULL );
		if( gc == GRABCHAR_END ) {
			QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, tempWidth, tempLineHeight );
			qttf->imageCurX += tempWidth;
			break;
		}

		if( gc != GRABCHAR_CHAR ) {
			continue;
		}

		qftglyph = QFT_GetRenderableGlyph( qfont, mainqfont, num );
		if( !qftglyph ) {
			assert( qftglyph->qglyph.shader || !qftglyph->gindex );
			continue;
		}

		FT_Load_Glyph( ftface, qftglyph->gindex, FT_LOAD_DEFAULT );
		ftglyph = ftface->glyph;
		FT_Render_Glyph( ftglyph, FT_RENDER_MODE_NORMAL );

		pixelMode = ftglyph->bitmap.pixel_mode;
		switch( pixelMode ) {
		case FT_PIXEL_MODE_MONO:
			srcStride = ALIGN( ftglyph->bitmap.width, 8 ) >> 3;
			break;
		case FT_PIXEL_MODE_GRAY:
			srcStride = ftglyph->bitmap.width;
			break;
		default:
			assert( 0 );
		}

		bitmapWidth = ftglyph->bitmap.width + 2;
		bitmapHeight = ftglyph->bitmap.rows + 2;
		if( bitmapWidth > FTLIB_FONT_IMAGE_WIDTH ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Width limit exceeded for '%s' character %i - %i\n",
				qfont->family->name, num, bitmapWidth - 2 );
			bitmapWidth = FTLIB_FONT_IMAGE_WIDTH;
		}
		if( bitmapHeight > qfont->shaderHeight ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Height limit exceeded for '%s' character %i - %i\n",
				qfont->family->name, num, bitmapHeight - 2 );
			bitmapHeight = qfont->shaderHeight;
		}

		if( ( qttf->imageCurX + tempWidth + bitmapWidth ) > FTLIB_FONT_IMAGE_WIDTH ) {
			QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, tempWidth, tempLineHeight );
			tempWidth = 0;
			tempLineHeight = 0;
			qttf->imageCurX = 0;
			qttf->imageCurY += qttf->imageCurLineHeight - 1; // overlap the previous line's margin
			qttf->imageCurLineHeight = 0;
		}

		if( bitmapHeight > qftGlyphTempBitmapHeight ) {
			qftGlyphTempBitmapHeight = ALIGN( bitmapHeight, QFT_GLYPH_BITMAP_HEIGHT_INCREMENT );
			qftGlyphTempBitmap = FTLIB_Realloc( qftGlyphTempBitmap, FTLIB_FONT_IMAGE_WIDTH * qftGlyphTempBitmapHeight );
		}

		if( bitmapHeight > tempLineHeight ) {
			if( bitmapHeight > qttf->imageCurLineHeight ) {
				if( ( qttf->imageCurY + bitmapHeight ) > qfont->shaderHeight ) {
					QFT_UploadRenderedGlyphs( qftGlyphTempBitmap, shader, qttf->imageCurX, qttf->imageCurY, tempWidth, tempLineHeight );
					tempWidth = 0;
					qttf->imageCurX = 0;
					qttf->imageCurY = 0;
					shaderNum = ( qfont->numShaders )++;
					shader = trap_R_RegisterRawPic( FTLIB_FontShaderName( qfont, shaderNum ),
						FTLIB_FONT_IMAGE_WIDTH, qfont->shaderHeight, NULL, 1 );
					if( shaderNum )
						qfont->shaders = FTLIB_Realloc( qfont->shaders, qfont->numShaders * sizeof( struct shader_s * ) );
					else
						qfont->shaders = FTLIB_Alloc( ftlibPool, qfont->numShaders * sizeof( struct shader_s * ) );
					qfont->shaders[shaderNum] = shader;
				}
				qttf->imageCurLineHeight = bitmapHeight;
			}
			tempLineHeight = bitmapHeight;
		}

		qglyph = &( qftglyph->qglyph );
		qglyph->width = bitmapWidth - 2;
		qglyph->height = bitmapHeight - 2;
		qglyph->x_advance = ( ftglyph->advance.x + ( 1 << 5 ) ) >> 6;
		qglyph->x_offset = ftglyph->bitmap_left;
		qglyph->y_offset = -( (int)( ftglyph->bitmap_top ) );
		qglyph->shader = shader;
		qglyph->s1 = ( float )( qttf->imageCurX + tempWidth + 1 ) * ( 1.0f / ( float )FTLIB_FONT_IMAGE_WIDTH );
		qglyph->t1 = ( float )( qttf->imageCurY + 1 ) * invHeight;
		qglyph->s2 = qglyph->s1 + ( float )( qglyph->width ) * ( 1.0f / ( float )FTLIB_FONT_IMAGE_WIDTH );
		qglyph->t2 = qglyph->t1 + ( float )( qglyph->height ) * invHeight;

		src = ftglyph->bitmap.buffer;
		dest = qftGlyphTempBitmap + tempWidth;
		memset( dest, 0, bitmapWidth );
		dest += FTLIB_FONT_IMAGE_WIDTH;
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
			dest += FTLIB_FONT_IMAGE_WIDTH;
		}
		memset( dest, 0, bitmapWidth );

		tempWidth += bitmapWidth - 1; // overlap the previous character's margin
	}
}

static const qfontface_funcs_t qft_face_funcs =
{
	QFT_AllocGlyphs,
	QFT_LoadGlyph,
	QFT_GetGlyph,
	QFT_GlyphNeedsRendering,
	QFT_RenderString,
	QFT_GetKerning
};

/*
* QFT_LoadFace
*/
static qfontface_t *QFT_LoadFace( qfontfamily_t *family, unsigned int size, const void *data, size_t dataSize )
{
	unsigned int i;
	int fontHeight, baseLine;
	int error;
	FT_Face ftface;
	bool hasKerning;
	qftface_t *qttf = NULL;
	qfontface_t *qfont = NULL;
	char renderStr[1024];

	ftface = NULL;

	error = FT_New_Memory_Face( ftLibrary, ( const FT_Byte* )data, dataSize, 0, &ftface );
	if( error != 0 ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Error loading font face '%s': %i\n", family->name, error );
		return NULL;
	}

	// check if the font has the replacement glyph
	if( !FT_Get_Char_Index( ftface, FTLIB_REPLACEMENT_GLYPH ) ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Font face '%s' doesn't have the replacement glyph %i\n",
			family->name, FTLIB_REPLACEMENT_GLYPH );
		FT_Done_Face( qttf->ftface );
		return NULL;
	}

	// set the font size
	FT_Set_Pixel_Sizes( ftface, size, 0 );

	hasKerning = FT_HAS_KERNING( ftface ) ? true : false;

	// we are going to need this for kerning
	qttf = FTLIB_Alloc( ftlibPool, sizeof( *qttf ) );
	qttf->ftface = ftface;

	// use scaled version of the original design text height (the vertical 
	// distance from one baseline to the next) as font height
	fontHeight = ( ftface->size->metrics.height + ( 1 << 5 ) ) >> 6;
	baseLine = ( ftface->size->metrics.height - ftface->size->metrics.ascender + ( 1 << 5 ) ) >> 6;

	// store font info
	qfont = FTLIB_Alloc( ftlibPool, sizeof( qfontface_t ) );
	qfont->family = family;
	qfont->size = size;
	qfont->height = fontHeight;
	qfont->glyphYOffset = fontHeight - baseLine;
	if( fontHeight > 48 ) {
		qfont->shaderHeight = FTLIB_FONT_IMAGE_HEIGHT_LARGE;
	} else if( fontHeight > 24 ) {
		qfont->shaderHeight = FTLIB_FONT_IMAGE_HEIGHT_MEDIUM;
	} else {
		qfont->shaderHeight = FTLIB_FONT_IMAGE_HEIGHT_SMALL;
	}
	qfont->numShaders = 0;
	qfont->hasKerning = hasKerning;
	qfont->f = &qft_face_funcs;
	qfont->facedata = ( void * )qttf;
	qfont->next = family->faces;
	family->faces = qfont;

	qttf->imageCurY = qfont->shaderHeight; // create a new shader the next time anything is rendered

	if( !family->fallback ) {
		// pre-render Latin-1
		for( i = 32; i <= 255; ++i ) {
			utf8Len = Q_WCharToUtf8( pRenderStr, i, sizeof( renderStr ) - renderStrLen );
			pRenderStr += utf8Len;
			renderStrLen += utf8Len;
		}
		QFT_RenderString( qfont, NULL, renderStr );
	}

	return qfont;
}

/*
* QFT_UnloadFace
*/
void QFT_UnloadFace( qfontface_t *qfont )
{
	qftface_t *qttf;

	qttf = ( qftface_t * )qfont->facedata;
	if( !qttf ) {
		return;
	}

	if( qttf->ftface ) {
		FT_Done_Face( qttf->ftface );
	}
	FTLIB_Free( qttf );
}

static const qfontfamily_funcs_t qft_family_funcs =
{
	QFT_LoadFace,
	QFT_UnloadFace
};

/*
* QFT_LoadFamily
*/
static void QFT_LoadFamily( const char *fileName, const uint8_t *data, size_t dataSize, bool verbose, bool fallback )
{
	FT_Face ftface;
	int error;
	const char *familyName;
	const char *styleName;
	qfontfamily_t *qfamily;

	ftface = NULL;
	error = FT_New_Memory_Face( ftLibrary, ( const FT_Byte* )data, dataSize, 0, &ftface );
	if( error != 0 ) {
		if( verbose ) {
			Com_Printf( S_COLOR_YELLOW "Warning: Error loading font face '%s': %i\n", fileName, error );
		}
		return;
	}

	familyName = ftface->family_name;
	styleName = ftface->style_name;

	// exit if this is not a scalable font
	if( !(ftface->face_flags & FT_FACE_FLAG_SCALABLE) || !(ftface->face_flags & FT_FACE_FLAG_HORIZONTAL) ) {
		if( verbose ) {
			Com_Printf( S_COLOR_YELLOW "Warning: '%s' is not a scalable font face\n", familyName );
		}
		return;
	}

	qfamily = FTLIB_Alloc( ftlibPool, sizeof( qfontfamily_t ) );
	qfamily->numFaces = 0;
	qfamily->name = FTLIB_CopyString( familyName );
	qfamily->privatep = FTLIB_Alloc( ftlibPool, dataSize );
	qfamily->privateSize = dataSize;
	qfamily->f = &qft_family_funcs;
	qfamily->style = QFONT_STYLE_NONE;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_ITALIC ? QFONT_STYLE_ITALIC : 0;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_BOLD ? QFONT_STYLE_BOLD : 0;
	qfamily->fallback = fallback;
	qfamily->next = fontFamilies;
	fontFamilies = qfamily;
	memcpy( qfamily->privatep, data, dataSize );

	if( verbose ) {
		Com_Printf( "Loaded font '%s %s' from '%s'\n", familyName, styleName, fileName );
	}

	FT_Done_Face( ftface );
}

/*
* QFT_LoadFamilyFromFile
*/
static void QFT_LoadFamilyFromFile( const char *name, const char *fileName, bool verbose, bool fallback )
{
	int fileNum;
	int length;
	uint8_t *buffer;

	length = trap_FS_FOpenFile( fileName, &fileNum, FS_READ );
	if( length < 0 ) {
		return;
	}

	buffer = ( uint8_t * )FTLIB_Alloc( ftlibPool, length );
	trap_FS_Read( buffer, length, fileNum );

	QFT_LoadFamily( name, buffer, length, verbose, fallback );

	FTLIB_Free( buffer );

	trap_FS_FCloseFile( fileNum );
}

/*
* QFT_PrecacheFonts
*
* Load fonts given type, storing family name, style, size
*/
static void QFT_PrecacheFontsByExt( bool verbose, const char *ext, bool fallback )
{
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
static void QFT_PrecacheFonts( bool verbose )
{
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_TRUETYPE, false );
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_OPENTYPE, false );
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_TRUETYPE, true );
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_OPENTYPE, true );
}

/*
* QFT_Init
*/
static void QFT_Init( bool verbose )
{
	int error;

	assert( ftLibrary == NULL );

	error = FT_Init_FreeType( &ftLibrary );
	if( error != 0 ) {
		ftLibrary = NULL;
		if( verbose ) {
			Com_Printf( S_COLOR_RED "Error initializing FreeType library: %i\n", error );
		}
	}

	assert( !qftGlyphTempBitmap );
	qftGlyphTempBitmap = FTLIB_Alloc( ftlibPool, FTLIB_FONT_IMAGE_WIDTH * QFT_GLYPH_BITMAP_HEIGHT_INCREMENT );
	qftGlyphTempBitmapHeight = QFT_GLYPH_BITMAP_HEIGHT_INCREMENT;
}

/*
* QFT_Shutdown
*/
static void QFT_Shutdown( void )
{
	if( ftLibrary != NULL ) {
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}

	if( qftGlyphTempBitmap ) {
		FTLIB_Free( qftGlyphTempBitmap );
		qftGlyphTempBitmap = NULL;
		qftGlyphTempBitmapHeight = 0;
	}
}

// ============================================================================

/*
* FTLIB_InitSubsystems
*/
void FTLIB_InitSubsystems( bool verbose )
{
	QFT_Init( verbose );
}

/*
* FTLIB_PrecacheFonts
*/
void FTLIB_PrecacheFonts( bool verbose )
{
	QFT_PrecacheFonts( verbose );
}

/*
* FTLIB_RegisterFont_
*/
static qfontface_t *FTLIB_RegisterFont_( const char *family, int style, unsigned int size, bool fallback )
{
	qfontfamily_t *qfamily, *best;
	int bestStyle;
	qfontface_t *qface;

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
		return NULL;
	}

	// find the best matching font style of the same size
	for( qface = qfamily->faces; qface; qface = qface->next ) {
		if( qface->size == size ) {
			// exact match
			FTLIB_TouchFont( qface );
			return qface;
		}
	}

	qface = qfamily->f->loadFace( qfamily, size, qfamily->privatep, qfamily->privateSize );
	if( qface ) {
		if( qface->hasKerning && !qface->f->getKerning ) {
			qface->hasKerning = false;
		}
	}
	return qface;
}

/*
* FTLIB_RegisterFont
*/
qfontface_t *FTLIB_RegisterFont( const char *family, const char *fallback, int style, unsigned int size )
{
	qfontface_t *qface;

	assert( family != NULL );
	if( !family || !*family ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Tried to register an empty font family\n" );
		return NULL;
	}

	qface = FTLIB_RegisterFont_( family, style, size, false );
	if( fallback && qface && !qface->fallback ) {
		qface->fallback = FTLIB_RegisterFont_( fallback, style, size, true );
	}

	return qface;
}

/*
* FTLIB_TouchFont
*/
void FTLIB_TouchFont( qfontface_t *qfont )
{
	unsigned int i;

	for( i = 0; i < qfont->numShaders; i++ ) {
		trap_R_RegisterPic( FTLIB_FontShaderName( qfont, i ) ); 
	}
}

/*
* FTLIB_TouchAllFonts
*/
void FTLIB_TouchAllFonts( void )
{
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
void FTLIB_FreeFonts( bool verbose )
{
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

		if( qfamily->name ) {
			FTLIB_Free( qfamily->name );
		}
		if( qfamily->privatep ) {
			FTLIB_Free( qfamily->privatep );
		}

		FTLIB_Free( qfamily );
	}

	fontFamilies = NULL;
}

/*
* FTLIB_ShutdownSubsystems
*/
void FTLIB_ShutdownSubsystems( bool verbose )
{
	QFT_Shutdown();
}

/*
* FTLIB_PrintFontList
*/
void FTLIB_PrintFontList( void )
{
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
				qface->size, qface->height, qface->numShaders, FTLIB_FONT_IMAGE_WIDTH, qface->shaderHeight );
		}
	}
}

/*
* FTLIB_LoadGlyph
*
* Allocates memory to store a new glyph.
*/
qglyph_t *FTLIB_LoadGlyph( qfontface_t *font, qwchar num )
{
	void *glyphs = font->glyphs[num >> 8];
	qglyph_t *qglyph;

	if( !glyphs ) {
		glyphs = font->f->allocGlyphs( font, num & 0xff00, 256 );
		font->glyphs[num >> 8] = glyphs;
	}

	qglyph = font->f->getGlyph( font, glyphs, num & 255, num );
	qglyph->loaded = 1;
	if( font->f->loadGlyph ) {
		font->f->loadGlyph( font, num );
	}

	return qglyph;
}

/*
* FTLIB_GetGlyph
*
* Gets a pointer to the glyph for its charcode.
*/
qglyph_t *FTLIB_GetGlyph( qfontface_t *font, qwchar num )
{
	qglyph_t *qglyph;

	if( !font->glyphs[num >> 8] )
		return NULL;

	qglyph = font->f->getGlyph( font, font->glyphs[num >> 8], num & 255, num );
	return qglyph->loaded ? qglyph : NULL;
}

/*
* FTLIB_FontShaderName
*
* Gets the name of the shader containing the glyphs for the font.
*/
const char *FTLIB_FontShaderName( qfontface_t *qfont, unsigned int shaderNum )
{
	static char name[MAX_QPATH];

	Q_snprintfz( name, sizeof( name ), "Font %s %i %i %i",
		qfont->family->name, qfont->size, qfont->family->style, shaderNum );

	return name;
}
