/*
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
#define FTLIB_Free( mem ) trap_MemFree( mem, __FILE__, __LINE__ )
#define FTLIB_AllocPool( name ) trap_MemAllocPool( name, __FILE__, __LINE__ )
#define FTLIB_FreePool( pool ) trap_MemFreePool( pool, __FILE__, __LINE__ )
#define FTLIB_EmptyPool( pool ) trap_MemEmptyPool( pool, __FILE__, __LINE__ )

#define FTLIB_REPLACEMENT_GLYPH		127

#define FTLIB_FONT_IMAGE_WIDTH		1024
#define FTLIB_MAX_FONT_IMAGE_HEIGHT	1024

#define FTLIB_MAX_FONT_FAMILIES		64
#define FTLIB_MAX_FONT_FACES		128

#define FTLIB_FIRST_FONT_CHAR		32
#define FTLIB_LAST_FONT_CHAR		0x9FCC
#define FTLIB_MAX_FONT_CHARS		( FTLIB_LAST_FONT_CHAR - FTLIB_FIRST_FONT_CHAR + 1 )

typedef struct
{
	unsigned short width;
	unsigned short x_advance;
	short x_offset, y_offset;
	struct shader_s	*shader;
	float s1, t1, s2, t2;
} qglyph_t;

typedef struct qfontface_s
{
	struct qfontfamily_s *family;

	int style;
	unsigned int size;
	int height;

	// a font may not fit into single image
	unsigned int numShaders;
	shader_t **shaders;
	char **shaderNames;

	// range of characters contained within the font
	unsigned int minChar, maxChar;

	// registration char
	unsigned int lastChar;

	// glyphs
	unsigned int numGlyphs;
	qglyph_t *glyphs;

	qboolean hasKerning;

	// offsets between adjacent characters
	short ( *getKerning )( struct qfontface_s *, unsigned int char1, unsigned int char2 );

	void *facedata;
} qfontface_t;

typedef struct qfontfamily_s
{
	char *name;

	unsigned int numFaces;
	int style;

	void *privatep;
	size_t privateSize;

	// method which the loader needs to call to load specific font face
	qfontface_t *( *loadFace )( struct qfontfamily_s *, unsigned int , unsigned int, const void *, size_t );

	// method which the loader needs to call to unload font face
	void ( *unloadFace )( qfontface_t * );

	qfontface_t *faces[FTLIB_MAX_FONT_FACES];
} qfontfamily_t;

void Com_DPrintf( const char *format, ... );

int FTLIB_API( void );
qboolean FTLIB_Init( qboolean verbose );
void FTLIB_Shutdown( qboolean verbose );

char *FTLIB_CopyString( const char *in );

// ftlib.c
void FTLIB_InitSubsystems( qboolean verbose );
void FTLIB_ShutdownSubsystems( qboolean verbose );
void FTLIB_PrecacheFonts( qboolean verbose );
qfontface_t *FTLIB_RegisterFont( const char *family, int style, unsigned int size, unsigned int lastChar );
void FTLIB_TouchFont( qfontface_t *qfont );
void FTLIB_TouchAllFonts( void );
void FTLIB_FreeFonts( qboolean verbose );
void FTLIB_PrintFontList( void );

// ftlib_draw.c
size_t FTLIB_fontHeight( qfontface_t *font );
size_t FTLIB_strWidth( const char *str, qfontface_t *font, size_t maxlen );
size_t FTLIB_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth );
void FTLIB_DrawRawChar( int x, int y, qwchar num, qfontface_t *font, vec4_t color );
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color );
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, qfontface_t *font, vec4_t color );

#endif
