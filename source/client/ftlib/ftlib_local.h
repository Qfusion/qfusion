/*
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

#pragma once

#include "qcommon/qcommon.h"
#include "gameshared/q_arch.h"
#include "gameshared/q_math.h"
#include "gameshared/q_shared.h"
#include "gameshared/q_cvar.h"

#include "ftlib_public.h"

extern struct mempool_s *ftlibPool;
typedef struct shader_s shader_t;

#define FTLIB_REPLACEMENT_GLYPH         '?'

#define FTLIB_FIRST_ASCII_CHAR          ' '
#define FTLIB_LAST_ASCII_CHAR           '~'
#define FTLIB_NUM_ASCII_CHARS           ( FTLIB_LAST_ASCII_CHAR - FTLIB_FIRST_ASCII_CHAR )

#define FTLIB_FONT_MAX_IMAGE_WIDTH      1024
#define FTLIB_FONT_IMAGE_HEIGHT_SMALL   128
#define FTLIB_FONT_IMAGE_HEIGHT_MEDIUM  256
#define FTLIB_FONT_IMAGE_HEIGHT_LARGE   512

struct qglyph_s;
struct qfontface_s;
struct qfontfamily_s;

typedef struct {
	unsigned short width, height;
	unsigned short x_advance;
	short x_offset, y_offset;
	struct shader_s *shader;
	float s1, t1, s2, t2;
} qglyph_t;

typedef void ( *renderString_f )( struct qfontface_s *qfont, const char *str );
typedef int ( *getKerning_f )( struct qfontface_s *qfont,  qglyph_t *g1, qglyph_t *g2 );

typedef struct qfontface_funcs_s {
	// allocates an array of glyphs
	void *( *allocGlyphs )( struct qfontface_s *qfont, wchar_t first, unsigned int count );

	// gets the glyph at the offset in the array, or NULL if it's missing in the font
	qglyph_t *( *getGlyph )( struct qfontface_s *qfont, void *glyphArray, unsigned int numInArray, wchar_t num );

	// renders the glyphs for a UTF-8 string
	renderString_f renderString;

	// offsets between adjacent characters
	getKerning_f getKerning;

	// sets the fallback font family for the font
	void ( *setFallback )( struct qfontface_s *qfont, struct qfontfamily_s *qfamily );
} qfontface_funcs_t;

typedef struct qfontface_s {
	struct qfontfamily_s *family;

	int style;
	unsigned int size;
	int height;
	int glyphYOffset;

	int underlinePosition, underlineThickness;

	// a font may not fit into single image
	unsigned int numShaders;
	shader_t **shaders;
	unsigned int shaderWidth;
	unsigned int shaderHeight;

	// glyphs
	size_t glyphSize;
	void *glyphs[256]; // 256 dynamically allocated blocks of 256 glyphs, each is a { qglyph_t; userdata } struct.

	bool hasKerning;

	const qfontface_funcs_t *f;

	void *facedata;

	struct qfontface_s *next;
} qfontface_t;

typedef struct qfontfamily_funcs_s {
	// method which the loader needs to call to load specific font face
	qfontface_t *( *loadFace )( struct qfontfamily_s *family, unsigned int size );

	// method which the loader needs to call to unload font face
	void ( *unloadFace )( qfontface_t *face );

	// method which the loader needs to call to unload font family private data
	void ( *unloadFamily )( struct qfontfamily_s *family );
} qfontfamily_funcs_t;

typedef struct qfontfamily_s {
	char *name;

	bool fallback;

	unsigned int numFaces;
	int style;

	const qfontfamily_funcs_t *f;

	qfontface_t *faces;

	void *familydata;

	struct qfontfamily_s *next;
} qfontfamily_t;

void FTLIB_InitSubsystems();
void FTLIB_ShutdownSubsystems();

qglyph_t *FTLIB_GetGlyph( qfontface_t *font, wchar_t num );
const char *FTLIB_FontShaderName( qfontface_t *qfont, unsigned int shaderNum );
char *FTLIB_CopyString( const char *in );
