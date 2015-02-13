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

/*
* FTLIB_FontForChar
*
* Gets the font that should render the specified glyph.
*/
static qfontface_t *FTLIB_FontForChar( qfontface_t *font, qwchar num, bool *render ) {
	qfontface_t *mainfont = font;
	qglyph_t *glyph;

	if( ( num < ' ' ) || ( num > 0xffff ) ) {
		return NULL;
	}

	while( font ) {
		// the glyph has already been rendered
		glyph = FTLIB_GetGlyph( font, num );
		if( glyph && glyph->shader ) {
			if( render ) {
				*render = false;
			}
			return font;
		}

		// a new glyph can be rendered?
		if( font->f->glyphNeedsRendering( font, mainfont, num ) ) {
			if( render ) {
				*render = true;
			}
			return font;
		}

		// try the next font in the fallback chain
		font = font->fallback;
	}

	if( render ) {
		*render = false;
	}
	return NULL;
}

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
	const char *s = str, *olds;
	size_t width = 0;
	qwchar num, prev_num = 0;
	qfontface_t *dfont, *prev_dfont = NULL;
	bool render;

	if( !str || !font )
		return 0;

	while( *s && *s != '\n' )
	{
		if( maxlen && (size_t)( s - str ) >= maxlen )  // stop counting at desired len
			return width;

		olds = s;

		switch( Q_GrabWCharFromColorString( &s, &num, NULL ) )
		{
		case GRABCHAR_CHAR:
			if( num < ' ' )
				break;

			dfont = FTLIB_FontForChar( font, num, &render );
			if( !dfont )
			{
				dfont = font;
				num = FTLIB_REPLACEMENT_GLYPH;
			}

			if( render )
				dfont->f->renderString( dfont, font, olds );

			if( prev_num && ( dfont == prev_dfont ) )
			{
				if( dfont->hasKerning )
					width += dfont->f->getKerning( dfont, prev_num, num );
			}

			width += FTLIB_GetGlyph( dfont, num )->x_advance;

			prev_num = num;
			prev_dfont = dfont;
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
	qfontface_t *dfont, *prev_dfont = NULL;
	bool render;

	if( !str || !font )
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

			dfont = FTLIB_FontForChar( font, num, &render );
			if( !dfont )
			{
				dfont = font;
				num = FTLIB_REPLACEMENT_GLYPH;
			}

			if( render )
				dfont->f->renderString( dfont, font, olds );

			advance = FTLIB_GetGlyph( dfont, num )->x_advance;
			if( prev_num && ( dfont == prev_dfont ) )
			{
				if( dfont->hasKerning )
					advance += dfont->f->getKerning( dfont, prev_num, num );
			}

			if( maxwidth && ( ( width + advance ) > maxwidth ) )
			{
				s = olds;
				break;
			}

			width += advance;

			prev_num = num;
			prev_dfont = dfont;
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
	qfontface_t *dfont;
	bool render;

	if( ( num <= ' ' ) || !font )
		return;

	dfont = FTLIB_FontForChar( font, num, &render );
	if( !dfont )
	{
		dfont = font;
		num = FTLIB_REPLACEMENT_GLYPH;
	}

	if( y <= -dfont->height )
		return; // totally off screen

	if( render )
		dfont->f->renderString( dfont, font, Q_WCharToUtf8Char( num ) );

	glyph = FTLIB_GetGlyph( dfont, num );
	trap_R_DrawStretchPic( x + glyph->x_offset, y + dfont->glyphYOffset + glyph->y_offset, 
		glyph->width, glyph->height,
		glyph->s1, glyph->t1, glyph->s2, glyph->t2,
		color, glyph->shader );
}

/*
* FTLIB_DrawClampChar
* 
* Draws one graphics character with 0 being transparent.
* Clipped to [xmin, ymin; xmax, ymax].
*/
void FTLIB_DrawClampChar( int x, int y, qwchar num, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color )
{
	qglyph_t *glyph;
	qfontface_t *dfont;
	bool render;
	int x2, y2;
	float s1 = 0.0f, t1 = 0.0f, s2 = 1.0f, t2 = 1.0f;
	float tw, th;

	if( ( num <= ' ' ) || !font || ( xmax <= xmin ) || ( ymax <= ymin ) )
		return;

	dfont = FTLIB_FontForChar( font, num, &render );
	if( !dfont )
	{
		dfont = font;
		num = FTLIB_REPLACEMENT_GLYPH;
	}

	if( render )
		dfont->f->renderString( dfont, font, Q_WCharToUtf8Char( num ) );

	glyph = FTLIB_GetGlyph( dfont, num );
	if( !glyph->width || !glyph->height )
		return;

	x += glyph->x_offset;
	y += dfont->glyphYOffset + glyph->y_offset;
	x2 = x + glyph->width;
	y2 = y + glyph->height;
	if( ( x > xmax ) || ( y > ymax ) || ( x2 <= xmin ) || ( y2 <= ymin ) )
		return;

	++xmax;
	++ymax;

	if( x < xmin )
	{
		s1 = ( xmin - x ) / ( float )glyph->width;
		x = xmin;
	}
	if( y < ymin )
	{
		t1 = ( ymin - y ) / ( float )glyph->height;
		y = ymin;
	}
	if( x2 > xmax )
	{
		s2 = 1.0f - ( x2 - xmax ) / ( float )glyph->width;
		x2 = xmax;
	}
	if( y2 > ymax )
	{
		t2 = 1.0f - ( y2 - ymax ) / ( float )glyph->height;
		y2 = ymax;
	}

	tw = glyph->s2 - glyph->s1;
	th = glyph->t2 - glyph->t1;

	trap_R_DrawStretchPic( x, y, x2 - x, y2 - y,
		glyph->s1 + tw * s1, glyph->t1 + th * t1,
		glyph->s1 + tw * s2, glyph->t1 + th * t2,
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
	qfontface_t *dfont, *prev_dfont = NULL;
	bool render;
	const char *s = str, *olds;
	int gc;

	if( !str || !font || ( xmax <= xmin ) || ( ymax <= ymin ) || ( x > xmax ) || ( y > ymax ) )
		return;

	Vector4Copy( color, scolor );

	while( 1 )
	{
		olds = s;
		gc = Q_GrabWCharFromColorString( &s, &num, &colorindex );
		if( gc == GRABCHAR_CHAR )
		{
			if( num == '\n' )
				break;

			if( num < ' ' )
				continue;

			dfont = FTLIB_FontForChar( font, num, &render );
			if( !dfont )
			{
				dfont = font;
				num = FTLIB_REPLACEMENT_GLYPH;
			}

			if( render )
				dfont->f->renderString( dfont, font, olds );

			if( prev_num )
			{
				xoffset += FTLIB_GetGlyph( prev_dfont, prev_num )->x_advance;
				if( ( dfont == prev_dfont ) && dfont->hasKerning )
					xoffset += dfont->f->getKerning( dfont, prev_num, num );
			}

			if( x + xoffset > xmax )
				break;

			FTLIB_DrawClampChar( x + xoffset, y, num, xmin, ymin, xmax, ymax, dfont, scolor );

			prev_num = num;
			prev_dfont = dfont;
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
	qfontface_t *dfont, *prev_dfont = NULL;
	bool render;

	if( !str || !font )
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

			if( num < ' ' )
				continue;

			dfont = FTLIB_FontForChar( font, num, &render );
			if( !dfont )
			{
				dfont = font;
				num = FTLIB_REPLACEMENT_GLYPH;
			}

			if( render )
				dfont->f->renderString( dfont, font, olds );

			if( maxwidth && ( ( xoffset + FTLIB_GetGlyph( dfont, num )->x_advance ) > maxwidth ) )
			{
				s = olds;
				break;
			}

			if( prev_num )
			{
				xoffset += FTLIB_GetGlyph( prev_dfont, prev_num )->x_advance;
				if( ( dfont == prev_dfont ) && dfont->hasKerning )
					xoffset += dfont->f->getKerning( dfont, prev_num, num );
			}

			FTLIB_DrawRawChar( x + xoffset, y, num, dfont, scolor );

			prev_num = num;
			prev_dfont = dfont;
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

