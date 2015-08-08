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

#ifndef _FTLIB_LOCAL_H_
#define _FTLIB_LOCAL_H_

// it's in qcommon.h too, but we don't include it for modules
typedef struct { char *name; void **funcPointer; } dllfunc_t;

#include "../gameshared/q_arch.h"
#include "../gameshared/q_math.h"
#include "../gameshared/q_shared.h"
#include "../gameshared/q_cvar.h"

#include "ftlib_public.h"
#include "ftlib_syscalls.h"

extern struct mempool_s *ftlibPool;
typedef struct shader_s shader_t;

#define FTLIB_Alloc( pool, size ) trap_MemAlloc( pool, size, __FILE__, __LINE__ )
#define FTLIB_Realloc( data, size ) trap_MemRealloc( data, size, __FILE__, __LINE__ )
#define FTLIB_Free( mem ) trap_MemFree( mem, __FILE__, __LINE__ )
#define FTLIB_AllocPool( name ) trap_MemAllocPool( name, __FILE__, __LINE__ )
#define FTLIB_FreePool( pool ) trap_MemFreePool( pool, __FILE__, __LINE__ )
#define FTLIB_EmptyPool( pool ) trap_MemEmptyPool( pool, __FILE__, __LINE__ )

#define FTLIB_REPLACEMENT_GLYPH			'?'

#define FTLIB_FIRST_ASCII_CHAR			' '
#define FTLIB_LAST_ASCII_CHAR			'~'
#define FTLIB_NUM_ASCII_CHARS			(FTLIB_LAST_ASCII_CHAR-FTLIB_FIRST_ASCII_CHAR)

#define FTLIB_FONT_MAX_IMAGE_WIDTH		1024
#define FTLIB_FONT_IMAGE_HEIGHT_SMALL	128
#define FTLIB_FONT_IMAGE_HEIGHT_MEDIUM	256
#define FTLIB_FONT_IMAGE_HEIGHT_LARGE	512

struct qglyph_s;
struct qfontface_s;
struct qfontfamily_s;

typedef struct
{
	unsigned short width, height;
	unsigned short x_advance;
	short x_offset, y_offset;
	struct shader_s *shader;
	float s1, t1, s2, t2;
} qglyph_t;

typedef void ( *renderString_f )( struct qfontface_s *qfont, const char *str );
typedef int ( *getKerning_f )( struct qfontface_s *qfont,  qglyph_t *g1, qglyph_t *g2 );

typedef struct qfontface_funcs_s
{
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

typedef struct qfontface_s
{
	struct qfontfamily_s *family;

	int style;
	unsigned int size;
	int height;
	int glyphYOffset;

	int underlinePosition, underlineThickness;
	int advance; // maximum character width/advance
	int xHeight; // height of lowercase 'x'

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

typedef struct qfontfamily_funcs_s
{
	// method which the loader needs to call to load specific font face
	qfontface_t *( *loadFace )( struct qfontfamily_s *family, unsigned int size );

	// method which the loader needs to call to unload font face
	void ( *unloadFace )( qfontface_t *face );

	// method which the loader needs to call to unload font family private data
	void ( *unloadFamily )( struct qfontfamily_s *family );
} qfontfamily_funcs_t;

typedef struct qfontfamily_s
{
	char *name;

	bool fallback;

	unsigned int numFaces;
	int style;

	const qfontfamily_funcs_t *f;

	qfontface_t *faces;

	void *familydata;

	struct qfontfamily_s *next;
} qfontfamily_t;

void Com_DPrintf( const char *format, ... );

int FTLIB_API( void );
bool FTLIB_Init( bool verbose );
void FTLIB_Shutdown( bool verbose );

char *FTLIB_CopyString( const char *in );

// ftlib.c
void FTLIB_InitSubsystems( bool verbose );
void FTLIB_ShutdownSubsystems( bool verbose );
void FTLIB_PrecacheFonts( bool verbose );
qfontface_t *FTLIB_RegisterFont( const char *family, const char *fallback, int style, unsigned int size );
void FTLIB_TouchFont( qfontface_t *qfont );
void FTLIB_TouchAllFonts( void );
void FTLIB_FreeFonts( bool verbose );
void FTLIB_PrintFontList( void );
qglyph_t *FTLIB_GetGlyph( qfontface_t *font, wchar_t num );
const char *FTLIB_FontShaderName( qfontface_t *qfont, unsigned int shaderNum );

// ftlib_draw.c
size_t FTLIB_FontSize( qfontface_t *font );
size_t FTLIB_FontHeight( qfontface_t *font );
size_t FTLIB_strWidth( const char *str, qfontface_t *font, size_t maxlen, int flags );
size_t FTLIB_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth, int flags );
int FTLIB_FontUnderline( qfontface_t *font, int *thickness );
size_t FTLIB_FontAdvance( qfontface_t *font );
size_t FTLIB_FontXHeight( qfontface_t *font );
void FTLIB_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color );
void FTLIB_DrawRawChar( int x, int y, wchar_t num, qfontface_t *font, vec4_t color );
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color, int flags );
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, int *width, qfontface_t *font, vec4_t color, int flags );
int FTLIB_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, qfontface_t *font, vec4_t color, int flags );
fdrawchar_t FTLIB_SetDrawIntercept( fdrawchar_t intercept );

#endif
