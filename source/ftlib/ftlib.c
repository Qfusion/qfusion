/*
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2012 Victor Luchits

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

static unsigned int numFontFamilies;
static qfontfamily_t fontFamilies[FTLIB_MAX_FONT_FAMILIES];

static unsigned int numFontFaces;
static qfontface_t fontFaces[FTLIB_MAX_FONT_FACES];

// ============================================================================

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define QFT_DIR						"fonts"
#define FT_FILE_EXTENSION_TRUETYPE	".ttf"
#define FT_FILE_EXTENSION_OPENTYPE	".otf"

#define TTF_BITMAP_MARGIN		3

FT_Library ftLibrary = NULL;

typedef struct
{
	FT_Face ftface;
	FT_UInt *gindices;
} qttface_t;

/*
* QFT_GetKerning
*/
static short QFT_GetKerning( qfontface_t *qfont, unsigned int char1, unsigned int char2 )
{
	qttface_t *qttf;
	FT_UInt gi1, gi2;
	FT_Vector kvec;

	qttf = ( qttface_t * )qfont->facedata;
	if( char1 < qfont->minChar || char1 > qfont->maxChar ) {
		return 0;
	}
	if( char2 < qfont->minChar || char2 > qfont->maxChar ) {
		return 0;
	}

	gi1 = qttf->gindices[char1];
	gi2 = qttf->gindices[char2];
	if( !gi1 || !gi2 ) {
		return 0;
	}

	FT_Get_Kerning( qttf->ftface, gi1, gi2, FT_KERNING_DEFAULT, &kvec );
	return kvec.x >> 6;
}

/*
* QFT_LoadFace
*
* FIXME: this is waaaaaaaay too complex...
*/
static qfontface_t *QFT_LoadFace( qfontfamily_t *family, unsigned int size, unsigned int lastChar, 
	const void *data, size_t dataSize )
{
	unsigned int i, j, x, y;
	unsigned int faceIndex;
	int fontHeight;
	unsigned int minChar, maxChar, numGlyphs;
	int lastStartChar;
	int error;
	FT_Face ftface;
	FT_ULong charcode;
	FT_UInt gindex;
	FT_Pos advance;
	const int margin = TTF_BITMAP_MARGIN;
	const int imageWidth = FTLIB_FONT_IMAGE_WIDTH;
	int imageHeight, imagePitch;
	int xOffset, yOffset;
	int maxUp;
	int line, numLines;
	int linesPerImage;
	int numImages, imageNum;
	qbyte *tempRGBA = NULL;
	qboolean clearImage;
	qboolean hasKerning;
	qttface_t *qttf = NULL;
	qfontface_t *qfont = NULL;

	ftface = NULL;

	error = FT_New_Memory_Face( ftLibrary, ( const FT_Byte* )data, dataSize, 0, &ftface );
	if( error != 0 ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Error loading font face '%s': %i\n", family->name, error );
		return NULL;
	}

	// try to find an unused slot
	for( i = 0; i < numFontFaces; i++ ) {
		if( !fontFaces[i].family ) {
			break;
		}
	}

	if( i == FTLIB_MAX_FONT_FACES ) {
		Com_Printf( S_COLOR_YELLOW "Warning: numFontFaces == FTLIB_MAX_FONT_FACES\n" );
		goto done;
	}

	faceIndex = i;

	// set the font size
	FT_Set_Pixel_Sizes( ftface, size, 0 );

	hasKerning = FT_HAS_KERNING( ftface ) ? qtrue : qfalse;

	// go over all characters to find the max ascent values, which
	// corresponds to the amount of space required above the baseline
	maxUp = 0;

	// track available chars
	minChar = FTLIB_LAST_FONT_CHAR + 1;
	maxChar = FTLIB_FIRST_FONT_CHAR - 1;

	numLines = 1;

	advance = 0;
	xOffset = 0;
	for( i = FTLIB_FIRST_FONT_CHAR; i <= lastChar; i++ ) {
		// render glyph for this char
		charcode = i;
		gindex = FT_Get_Char_Index( ftface, charcode );
		if( gindex > 0 ) {
			FT_Load_Glyph( ftface, gindex, FT_LOAD_DEFAULT );
			FT_Render_Glyph( ftface->glyph, FT_RENDER_MODE_NORMAL );

			xOffset += advance;
			advance = ftface->glyph->bitmap.width + margin;

			// if the char doesn't fit, start on the next line
			if( xOffset + advance > imageWidth ) {
				xOffset = 0;
				numLines++;
			}

			if( i < minChar ) {
				minChar = i;
			}
			if( i > maxChar ) {
				maxChar = i;
			}

			// track line and font height
			maxUp = max( maxUp, ftface->glyph->bitmap_top );
		}
	}

	// validate
	if( maxChar < minChar ) { 
		Com_Printf( S_COLOR_YELLOW "Warning: No glyphs found in font face '%s' %i\n", family->name, size );
		goto done;
	}

	if( maxChar < FTLIB_REPLACEMENT_GLYPH ) {
		maxChar = FTLIB_REPLACEMENT_GLYPH;
	}

	// we are going to need this for kerning
	qttf = FTLIB_Alloc( ftlibPool, sizeof( *qttf ) + (maxChar+1) * sizeof( FT_UInt ) );
	qttf->gindices = ( FT_UInt * )( ( qbyte * )qttf + sizeof( *qttf ) );
	qttf->ftface = ftface;

	// failed to find an unused slot, take a new one
	if( faceIndex == numFontFaces ) {
		numFontFaces++;
	}

	// use scaled version of the original design text height (the vertical 
	// distance from one baseline to the next) as font height
	fontHeight = ftface->size->metrics.height >> 6;
	numGlyphs = maxChar + 1;
	linesPerImage = FTLIB_MAX_FONT_IMAGE_HEIGHT / (fontHeight + margin);
	if( linesPerImage < 1 ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Font height limit exceeded for '%s' %i\n", family->name, size );
		goto done;
	}
	numImages = numLines / linesPerImage + 1;

	// store font info
	qfont = & fontFaces[faceIndex];
	qfont->family = family;
	qfont->size = size;
	qfont->height = fontHeight;
	qfont->numGlyphs = numGlyphs;
	qfont->minChar = minChar;
	qfont->maxChar = maxChar;
	qfont->lastChar = lastChar;
	qfont->glyphs = ( qglyph_t *)(FTLIB_Alloc( ftlibPool, numGlyphs * sizeof( *qfont->glyphs ) ));
	qfont->numShaders = numImages;
	qfont->shaders = ( shader_t ** )FTLIB_Alloc( ftlibPool, numImages * sizeof( *qfont->shaders ) );
	qfont->shaderNames = ( char ** )FTLIB_Alloc( ftlibPool, numImages * sizeof( *qfont->shaderNames ) );
	qfont->hasKerning = hasKerning;
	qfont->facedata = ( void * )qttf;
	qfont->getKerning = & QFT_GetKerning;

	// render glyphs onto RGBA images
	imageNum = 0;
	imageHeight = 0;
	imagePitch = imageWidth * 4;
	tempRGBA = ( qbyte * )FTLIB_Alloc( ftlibPool, imagePitch * FTLIB_MAX_FONT_IMAGE_HEIGHT );
	clearImage = qtrue;
	lastStartChar = minChar;

	line = 0;
	advance = 0;
	xOffset = 0;
	yOffset = 0;
	for( i = minChar; i <= maxChar + 1; i++ ) {
		qglyph_t *qglyph = &qfont->glyphs[i];
		qboolean uploadNow = i > maxChar && imageHeight > 0 ? qtrue : qfalse;

upload_image:
		if( uploadNow )
		{
			unsigned int lastEndChar;
			int uploadWidth, uploadHeight;
			char shaderName[MAX_QPATH];
			shader_t *shader;
			
			assert( imageNum < numImages );

			clearImage = qtrue;

			// round to nearest greater power of two
			uploadWidth = imageWidth;
			uploadHeight = 1;
			for( ; uploadHeight < imageHeight; uploadHeight <<= 1 );

			Q_snprintfz( shaderName, sizeof( shaderName ), 
				"%s %i %i (%i)", 
				family->name, size, family->style, imageNum );
			shader = trap_R_RegisterRawPic( shaderName, uploadWidth, uploadHeight, tempRGBA );

			qfont->shaderNames[imageNum] = FTLIB_CopyString( shaderName );
			qfont->shaders[imageNum] = shader;
			imageNum++;

			// update glyphs texture coordinates and set shader pointer
			lastEndChar = min( i, maxChar );
			for( j = lastStartChar; j <= lastEndChar; j++ ) {
				qglyph_t *qglyph = &qfont->glyphs[j];
				qglyph->s1 = (float)qglyph->s1 / (float)uploadWidth;
				qglyph->t1 = (float)qglyph->t1 / (float)uploadHeight;
				qglyph->s2 = (float)qglyph->s2 / (float)uploadWidth;
				qglyph->t2 = (float)qglyph->t2 / (float)uploadHeight;
				qglyph->shader = shader;
			}
		}

		if( i > maxChar ) {
			break;
		}
				
		if( clearImage ) {
			clearImage = qfalse;
			memset( tempRGBA, 0, imagePitch * FTLIB_MAX_FONT_IMAGE_HEIGHT );
			lastStartChar = i;
			imageHeight = 0;
		}

		// render glyph for this char
		charcode = i;
		gindex = FT_Get_Char_Index( ftface, charcode );
		qttf->gindices[charcode] = gindex;
		if( gindex > 0 ) {
			qbyte *dst, *src;
			qbyte *dst_p, *src_p;
			FT_Bitmap *bitmap;
			unsigned int bitmap_width, bitmap_height;
			unsigned int bitmap_left, bitmap_top;
			unsigned int bitmap_pixmode, bitmap_pitch;
			
			FT_Load_Glyph( ftface, gindex, FT_LOAD_DEFAULT );
			FT_Render_Glyph( ftface->glyph, FT_RENDER_MODE_NORMAL );

			// copy bitmap
			bitmap = &ftface->glyph->bitmap;
			bitmap_left = ftface->glyph->bitmap_left;
			bitmap_top = ftface->glyph->bitmap_top;
			bitmap_width = bitmap->width;
			bitmap_height = bitmap->rows;
			bitmap_pixmode = bitmap->pixel_mode;
			bitmap_pitch = bitmap->pitch;

			xOffset += advance;
			advance = bitmap_width + margin;
			if( xOffset + advance > imageWidth ) {
				xOffset = 0;
				yOffset += fontHeight + margin;
				line++;

				if( yOffset + fontHeight + margin > FTLIB_MAX_FONT_IMAGE_HEIGHT ) {
					// ran out of room, start with new image
					advance = 0;
					yOffset = 0;
					uploadNow = qtrue;
					goto upload_image;
				}
			}

			if( !xOffset ) {
				assert( imageNum < numImages );
				imageHeight += fontHeight + margin;
			}

			src = bitmap->buffer;
			dst = tempRGBA + yOffset * imagePitch + xOffset * 4;

			for( y = 0; y < bitmap_height; y++ ) {
				src_p = src;
				dst_p = dst;

				switch( bitmap_pixmode ) {
					case FT_PIXEL_MODE_MONO:
						for( x = 0; x < bitmap_width; x += 8 ) {
							unsigned char ch = *src_p++;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x80) >> 7); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x40) >> 6); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x20) >> 5); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x10) >> 4); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x08) >> 3); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x04) >> 2); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x02) >> 1); dst_p += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = 255 * !!((ch & 0x01) >> 0); dst_p += 4;
						}
						break;
					case FT_PIXEL_MODE_GRAY2:
						for( x = 0; x < bitmap_width; x += 4 ) {
							unsigned char ch = *src_p++;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0xA0) >> 6) * 0x55 ); ch <<= 2; dst += 4;
						}
						break;
					case FT_PIXEL_MODE_GRAY4:
						for( x = 0; x < bitmap_width; x += 2 ) {
							unsigned char ch = *src_p++;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0xF0) >> 4) * 0x11); dst += 4;
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = ( ((ch & 0x0F) ) * 0x11); dst += 4;
						}
						break;
					case FT_PIXEL_MODE_GRAY:
						// in this case pitch should equal width

						// copy the grey value into the alpha bytes
						for( x = 0; x < bitmap_width; x++ ) {
							dst_p[0] = dst_p[1] = dst_p[2] = 255; dst_p[3] = *src_p++; dst_p += 4;
						}
						break;
					default:
						break;
				}

				src += bitmap_pitch;
				dst += imagePitch;
			}

			// store glyph info
			qglyph->width = bitmap_width;
			qglyph->x_advance = (ftface->glyph->advance.x >> 6) + (ftface->glyph->advance.x & 0x3F ? 1 : 0);
			qglyph->s1 = xOffset;
			qglyph->t1 = yOffset;
			qglyph->s2 = xOffset + bitmap_width;
			qglyph->t2 = yOffset + fontHeight;
			qglyph->x_offset = bitmap_left;
			qglyph->y_offset = maxUp - bitmap_top;
		}
	}

	// make sure the replacement glyph is always present
	if( !qfont->glyphs[FTLIB_REPLACEMENT_GLYPH].shader ) {
		qfont->glyphs[FTLIB_REPLACEMENT_GLYPH] = qfont->glyphs['?'];
	}

	// ummm, replace missing glyphs with some sane one
	assert( qfont->glyphs[FTLIB_REPLACEMENT_GLYPH].shader != NULL );	
	for( i = minChar; i <= maxChar; i++ ) {
		qglyph_t *qglyph = &qfont->glyphs[i];
		if( !qglyph->shader ) {
			*qglyph = qfont->glyphs[FTLIB_REPLACEMENT_GLYPH];
		}
	}

done:
	if( tempRGBA ) {
		FTLIB_Free( tempRGBA );
	}
	if( !qfont && qttf ) {
		if( qttf->ftface ) {
			FT_Done_Face( qttf->ftface );
		}
		FTLIB_Free( qttf );
	}
	return qfont;
}

/*
* QFT_UnloadFace
*/
void QFT_UnloadFace( qfontface_t *qfont )
{
	qttface_t *qttf;

	qttf = ( qttface_t * )qfont->facedata;
	if( !qttf ) {
		return;
	}

	if( qttf->ftface ) {
		FT_Done_Face( qttf->ftface );
	}
	FTLIB_Free( qttf );
}

/*
* QFT_LoadFamily
*/
static void QFT_LoadFamily( const char *fileName, const qbyte *data, size_t dataSize, qboolean verbose )
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

	if( numFontFamilies == FTLIB_MAX_FONT_FAMILIES ) {
		if( verbose ) {
			Com_Printf( S_COLOR_YELLOW "Warning: numFontFamilies == FTLIB_MAX_FONT_FAMILIES\n" );
		}
		return;
	}

	qfamily = & fontFamilies[numFontFamilies++];
	qfamily->numFaces = 0;
	qfamily->name = FTLIB_CopyString( familyName );
	qfamily->privatep = FTLIB_Alloc( ftlibPool, dataSize );
	qfamily->privateSize = dataSize;
	qfamily->loadFace = & QFT_LoadFace;
	qfamily->unloadFace = & QFT_UnloadFace;
	qfamily->style = QFONT_STYLE_NONE;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_ITALIC ? QFONT_STYLE_ITALIC : 0;
	qfamily->style |= ftface->style_flags & FT_STYLE_FLAG_BOLD ? QFONT_STYLE_BOLD : 0;
	memcpy( qfamily->privatep, data, dataSize );

	if( verbose ) {
		Com_Printf( "Loaded font '%s %s' from '%s'\n", familyName, styleName, fileName );
	}

	FT_Done_Face( ftface );
}

/*
* QFT_LoadFamilyFromFile
*/
static void QFT_LoadFamilyFromFile( const char *name, const char *fileName, qboolean verbose )
{
	int fileNum;
	int length;
	qbyte *buffer;

	if( numFontFamilies == FTLIB_MAX_FONT_FAMILIES ) {
		return;
	}

	length = trap_FS_FOpenFile( fileName, &fileNum, FS_READ );
	if( length < 0 ) {
		return;
	}

	buffer = ( qbyte * )FTLIB_Alloc( ftlibPool, length );
	trap_FS_Read( buffer, length, fileNum );

	QFT_LoadFamily( name, buffer, length, verbose );

	FTLIB_Free( buffer );
}

/*
* QFT_PrecacheFonts
*
* Load fonts given type, storing family name, style, size
*/
static void QFT_PrecacheFontsByExt( qboolean verbose, const char *ext )
{
	int i, j;
	int numfiles;
	char buffer[1024];
	char fileName[1024 + sizeof( QFT_DIR ) + 2];
	char *s;
	size_t length;

	assert( ftLibrary != NULL );
	if( ftLibrary == NULL ) {
		//Com_Printf( S_COLOR_RED "Error: TTF_LoadFonts called prior initializing FreeType\n" );
		return;
	}

	if( ( numfiles = trap_FS_GetFileList( QFT_DIR, ext, NULL, 0, 0, 0 ) ) == 0 ) {
		return;
	}

	i = 0;
	length = 0;
	do {
		if( ( j = trap_FS_GetFileList( QFT_DIR, ext, buffer, sizeof( buffer ), i, numfiles ) ) == 0 ) {
			// can happen if the filename is too long to fit into the buffer or we're done
			i++;
			continue;
		}

		i += j;
		for( s = buffer; j > 0; j--, s += length + 1 ) {
			length = strlen( s );
			Q_strncpyz( fileName, va( "%s/%s", QFT_DIR, s ), sizeof( fileName ) );

			QFT_LoadFamilyFromFile( s, fileName, verbose );
		}
	} while( i < numfiles );
}

/*
* QFT_PrecacheFonts
*/
static void QFT_PrecacheFonts( qboolean verbose )
{
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_TRUETYPE );
	QFT_PrecacheFontsByExt( verbose, FT_FILE_EXTENSION_OPENTYPE );
}

/*
* QFT_Init
*/
static void QFT_Init( qboolean verbose )
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
}

// ============================================================================

/*
* FTLIB_InitSubsystems
*/
void FTLIB_InitSubsystems( qboolean verbose )
{
	QFT_Init( verbose );
}

/*
* FTLIB_PrecacheFonts
*/
void FTLIB_PrecacheFonts( qboolean verbose )
{
	numFontFaces = 0;
	numFontFamilies = 0;

	QFT_PrecacheFonts( verbose );
}

/*
* FTLIB_RegisterFont
*/
qfontface_t *FTLIB_RegisterFont( const char *family, int style, unsigned int size, unsigned int lastChar )
{
	unsigned int i;
	qfontfamily_t *qfamily, *best;
	int bestStyle;
	qfontface_t *qface;

	assert( family != NULL );
	if( !family || !*family ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Empty family name passed\n" );
		return NULL;
	}

	if( lastChar > FTLIB_LAST_FONT_CHAR ) {
		lastChar = FTLIB_LAST_FONT_CHAR;
	}

	best = NULL;
	bestStyle = QFONT_STYLE_MASK + 1;
	for( i = 0; i < numFontFamilies; i++ ) {
		qfamily = &fontFamilies[i];

		if( qfamily->style == style && !Q_stricmp( qfamily->name, family ) ) {
			best = qfamily;
			break;
		}
		else if( qfamily->style < bestStyle && !Q_stricmp( qfamily->name, family ) ) {
			best = qfamily;
		}
	}

	qfamily = best;
	if( qfamily == NULL ) {
		Com_Printf( S_COLOR_YELLOW "Warning: Unknown font family '%s'\n", family );
		return NULL;
	}

	// find the best matching font style of the same size
	for( i = 0; i < qfamily->numFaces; i++ ) {
		qface = qfamily->faces[i];
		if( qface->size == size && qface->lastChar >= lastChar ) {
			// exact match
			FTLIB_TouchFont( qface );
			return qface;
		}
	}

	qface = qfamily->loadFace( qfamily, size, lastChar, qfamily->privatep, qfamily->privateSize );
	if( qface ) {
		qfamily->faces[qfamily->numFaces++] = qface;
		if( qface->hasKerning && !qface->getKerning ) {
			qface->hasKerning = qfalse;
		}
	}
	return qface;
}

/*
* FTLIB_TouchFont
*/
void FTLIB_TouchFont( qfontface_t *qfont )
{
	unsigned int i;

	if( !qfont->shaders ) {
		return;
	}

	for( i = 0; i < qfont->numShaders; i++ ) {
		trap_R_RegisterPic( qfont->shaderNames[i] ); 
	}
}

/*
* FTLIB_TouchAllFonts
*/
void FTLIB_TouchAllFonts( void )
{
	unsigned int i, j;
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	// unload all font families
	for( i = 0; i < numFontFamilies; i++ ) {
		qfamily = &fontFamilies[i];

		// unload all faces for this family
		for( j = 0; j < qfamily->numFaces; j++ ) {
			qface = qfamily->faces[j];

			FTLIB_TouchFont( qface );
		}
	}
}

/*
* FTLIB_FreeFonts
*/
void FTLIB_FreeFonts( qboolean verbose )
{
	unsigned int i, j, k;
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	// unload all font families
	for( i = 0; i < numFontFamilies; i++ ) {
		qfamily = &fontFamilies[i];

		// unload all faces for this family
		for( j = 0; j < qfamily->numFaces; j++ ) {
			qface = qfamily->faces[j];

			if( qfamily->unloadFace ) {
				qfamily->unloadFace( qface );
			}

			if( qface->shaderNames ) {
				for( k = 0; k < qface->numShaders; k++ ) {
					FTLIB_Free( qface->shaderNames[k] );
				}
				FTLIB_Free( qface->shaderNames );
			}
			if( qface->shaders ) {
				FTLIB_Free( qface->shaders );
			}
			if( qface->glyphs ) {
				FTLIB_Free( qface->glyphs );
			}
		}

		if( qfamily->name ) {
			FTLIB_Free( qfamily->name );
		}
		if( qfamily->privatep ) {
			FTLIB_Free( qfamily->privatep );
		}
	}

	memset( &fontFaces, 0, sizeof( fontFaces ) );
	numFontFaces = 0;

	memset( &fontFamilies, 0, sizeof( fontFamilies ) );
	numFontFamilies = 0;
}

/*
* FTLIB_ShutdownSubsystems
*/
void FTLIB_ShutdownSubsystems( qboolean verbose )
{
	QFT_Shutdown();
}

/*
* FTLIB_PrintFontList
*/
void FTLIB_PrintFontList( void )
{
	unsigned int i, j;
	qfontfamily_t *qfamily;
	qfontface_t *qface;

	Com_Printf( "Font families:\n" );

	for( i = 0; i < numFontFamilies; i++ ) {
		qfamily = &fontFamilies[i];

		Com_Printf( "%s %s %s\n", qfamily->name, 
			qfamily->style & QFONT_STYLE_ITALIC ? "italic" : "",
			qfamily->style & QFONT_STYLE_BOLD ? "bold" : "" );

		// unload all faces for this family
		for( j = 0; j < qfamily->numFaces; j++ ) {
			qface = qfamily->faces[j];

			Com_Printf( "  face %i: size:%ipt, glyphs:%i, height:%ipx, images:%i\n", 
				j, qface->size, qface->numGlyphs, qface->height, qface->numShaders );
		}
	}
}
