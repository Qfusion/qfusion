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

#include "../ftlib/ftlib_public.h"

void FTLIB_LoadLibrary( qboolean verbose );
void FTLIB_UnloadLibrary( qboolean verbose );
struct qfontface_s *FTLIB_RegisterFont( const char *family, int style, unsigned int size, unsigned int lastChar );
void FTLIB_TouchFont( struct qfontface_s *qfont );
void FTLIB_TouchAllFonts( void );
void FTLIB_PrecacheFonts( qboolean verbose );
void FTLIB_FreeFonts( qboolean verbose );

// drawing functions

size_t FTLIB_FontHeight( struct qfontface_s *font );
size_t FTLIB_StringWidth( const char *str, struct qfontface_s *font, size_t maxlen );
size_t FTLIB_StrlenForWidth( const char *str, struct qfontface_s *font, size_t maxwidth );
void FTLIB_DrawRawChar( int x, int y, qwchar num, struct qfontface_s *font, vec4_t color );
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color );
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, struct qfontface_s *font, vec4_t color );
