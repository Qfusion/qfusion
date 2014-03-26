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

//===============================================================================
//STRINGS HELPERS
//===============================================================================

/*
* FTLIB_fontHeight
*/
size_t FTLIB_fontHeight( qfontface_t *font )
{
	if( !font ) {
		return 0;
	}
	return font->height;
}

/*
* FTLIB_strWidth
* doesn't count invisible characters. Counts up to given length, if any.
*/
size_t FTLIB_strWidth( const char *str, qfontface_t *font, size_t maxlen )
{
	const char *s = str;
	size_t width = 0;
	qwchar num, prev_num = 0;

	if( !str )
		return 0;

	if( !font )
		return 0;

	while( *s && *s != '\n' )
	{
		if( maxlen && (size_t)( s - str ) >= maxlen )  // stop counting at desired len
			return width;

		switch( Q_GrabWCharFromColorString( &s, &num, NULL ) )
		{
		case GRABCHAR_CHAR:
			if( num < ' ' )
				break;
			if( num < font->minChar || num > font->maxChar )
				num = FTLIB_REPLACEMENT_GLYPH;

			if( prev_num ) {
				if( font->hasKerning ) {
					width += font->getKerning( font, prev_num, num );
				}
			}

			width += font->glyphs[num].x_advance;
			break;

		case GRABCHAR_COLOR:
			break;

		case GRABCHAR_END:
			return width;

		default:
			assert( 0 );
		}
	}

	return width;
}

/*
* FTLIB_StrlenForWidth
* returns the len allowed for the string to fit inside a given width when using a given font.
*/
size_t FTLIB_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth )
{
	const char *s, *olds;
	size_t width = 0;
	int gc;
	int advance = 0;
	qwchar num, prev_num = 0;

	if( !str )
		return 0;

	if( !font )
		return 0;

	s = str;

	while( s )
	{
		olds = s;
		gc = Q_GrabWCharFromColorString( &s, &num, NULL );
		if( gc == GRABCHAR_CHAR )
		{
			if( num == '\n' )
				break;
			if( num < ' ' )
				continue;
			if( num < font->minChar || num > font->maxChar )
				num = FTLIB_REPLACEMENT_GLYPH;

			advance = font->glyphs[num].x_advance;
			if( prev_num ) {
				if( font->hasKerning ) {
					advance += font->getKerning( font, prev_num, num );
				}
			}

			if( maxwidth && ( ( width + advance ) > maxwidth ) )
			{
				s = olds;
				break;
			}

			width += advance;
		}
		else if( gc == GRABCHAR_COLOR )
			continue;
		else if( gc == GRABCHAR_END )
			break;
		else
			assert( 0 );
	}

	return (unsigned int)( s - str );
}

//===============================================================================
//STRINGS DRAWING
//===============================================================================

/*
* FTLIB_DrawRawChar
* 
* Draws one graphics character with 0 being transparent.
* It can be clipped to the top of the screen to allow the console to be
* smoothly scrolled off.
*/
void FTLIB_DrawRawChar( int x, int y, qwchar num, qfontface_t *font, vec4_t color )
{
	qglyph_t *glyph;

	if( !font )
		return;
	if( y <= -font->height )
		return; // totally off screen

	if( num <= ' ' )
		return;
	if( num < font->minChar || num > font->maxChar )
		num = FTLIB_REPLACEMENT_GLYPH;

	glyph = &font->glyphs[num];
	trap_R_DrawStretchPic( x + glyph->x_offset, y + glyph->y_offset, 
		glyph->width, font->height,
		glyph->s1, glyph->t1, glyph->s2, glyph->t2,
		color, glyph->shader );
}

/*
* FTLIB_DrawClampString
*/
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color )
{
	int xoffset = 0;
	vec4_t scolor;
	int colorindex;
	qwchar num, prev_num = 0;
	const char *s = str;
	int gc;
	struct { int x, y, w, h; } oldscissor;

	if( !str )
		return;
	if( !font )
		return;
	if( xmax <= xmin || ymax <= ymin || x > xmax || y > ymax )
		return;

	trap_R_GetScissor( &oldscissor.x, &oldscissor.y, &oldscissor.w, &oldscissor.h );
	trap_R_Scissor( 
		max( xmin, oldscissor.x ),
		max( ymin, oldscissor.y ), 
		min( xmax - xmin + 1, oldscissor.w ), 
		min( ymax - ymin + 1, oldscissor.h ) );

	Vector4Copy( color, scolor );

	while( 1 )
	{
		gc = Q_GrabWCharFromColorString( &s, &num, &colorindex );
		if( gc == GRABCHAR_CHAR )
		{
			if( num == '\n' )
				break;
			if( num < font->minChar || num > font->maxChar )
				continue;

			if( prev_num ) {
				xoffset += font->glyphs[prev_num].x_advance;
				if( font->hasKerning ) {
					xoffset += font->getKerning( font, prev_num, num );
				}
			}

			if( x + xoffset > xmax )
				break;

			FTLIB_DrawRawChar( x + xoffset, y, num, font, scolor );

			prev_num = num;
		}
		else if( gc == GRABCHAR_COLOR )
		{
			assert( ( unsigned )colorindex < MAX_S_COLORS );
			VectorCopy( color_table[colorindex], scolor );
		}
		else if( gc == GRABCHAR_END )
			break;
		else
			assert( 0 );
	}

	trap_R_Scissor( oldscissor.x, oldscissor.y, oldscissor.w, oldscissor.h );
}

/*
* FTLIB_DrawRawString - Doesn't care about aligning. Returns drawn len.
* It can stop when reaching maximum width when a value has been parsed.
*/
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, qfontface_t *font, vec4_t color )
{
	unsigned int xoffset = 0;
	vec4_t scolor;
	const char *s, *olds;
	int gc, colorindex;
	qwchar num, prev_num = 0;

	if( !str )
		return 0;
	if( !font )
		return 0;

	Vector4Copy( color, scolor );

	s = str;

	while( s )
	{
		olds = s;
		gc = Q_GrabWCharFromColorString( &s, &num, &colorindex );
		if( gc == GRABCHAR_CHAR )
		{
			if( num == '\n' )
				break;
			if( num < font->minChar || num > font->maxChar )
				continue;

			if( maxwidth && ( ( xoffset + font->glyphs[num].x_advance ) > maxwidth ) )
			{
				s = olds;
				break;
			}

			if( prev_num ) {
				xoffset += font->glyphs[prev_num].x_advance;
				if( font->hasKerning ) {
					xoffset += font->getKerning( font, prev_num, num );
				}
			}

			FTLIB_DrawRawChar( x + xoffset, y, num, font, scolor );

			prev_num = num;
		}
		else if( gc == GRABCHAR_COLOR )
		{
			assert( ( unsigned )colorindex < MAX_S_COLORS );
			VectorCopy( color_table[colorindex], scolor );
		}
		else if( gc == GRABCHAR_END )
			break;
		else
			assert( 0 );
	}

	return ( s - str );
}

