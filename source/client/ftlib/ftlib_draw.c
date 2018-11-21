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
#include "client/renderer/r_local.h"

static fdrawchar_t drawCharIntercept = NULL;

//===============================================================================
//STRINGS HELPERS
//===============================================================================

/*
* FTLIB_GrabChar
*/
static int FTLIB_GrabChar( const char **pstr, wchar_t *wc, int *colorindex, int flags ) {
	if( flags & TEXTDRAWFLAG_NO_COLORS ) {
		wchar_t num = Q_GrabWCharFromUtf8String( pstr );
		*wc = num;
		return num ? GRABCHAR_CHAR : GRABCHAR_END;
	}

	return Q_GrabWCharFromColorString( pstr, wc, colorindex );
}

/*
* FTLIB_FontSize
*/
size_t FTLIB_FontSize( qfontface_t *font ) {
	if( !font ) {
		return 0;
	}
	return font->size;
}

/*
* FTLIB_FontHeight
*/
size_t FTLIB_FontHeight( qfontface_t *font ) {
	if( !font ) {
		return 0;
	}
	return font->height;
}

/*
* FTLIB_StringWidth
* doesn't count invisible characters. Counts up to given length, if any.
*/
size_t FTLIB_StringWidth( const char *str, qfontface_t *font, size_t maxlen, int flags ) {
	const char *s = str, *olds;
	size_t width = 0;
	wchar_t num, prev_num = 0;
	qglyph_t *glyph, *prev_glyph = NULL;
	renderString_f renderString;
	getKerning_f getKerning;
	bool hasKerning;

	if( !str || !font ) {
		return 0;
	}

	renderString = font->f->renderString;
	getKerning = font->f->getKerning;
	hasKerning = ( flags & TEXTDRAWFLAG_KERNING ) && font->hasKerning;

	while( *s && *s != '\n' ) {
		if( maxlen && (size_t)( s - str ) >= maxlen ) { // stop counting at desired len
			return width;
		}

		olds = s;

		switch( FTLIB_GrabChar( &s, &num, NULL, flags ) ) {
			case GRABCHAR_CHAR:
				if( num < ' ' ) {
					break;
				}

				glyph = FTLIB_GetGlyph( font, num );
				if( !glyph ) {
					num = FTLIB_REPLACEMENT_GLYPH;
					glyph = FTLIB_GetGlyph( font, num );
				}

				if( !glyph->shader ) {
					renderString( font, olds );
				}

				if( prev_num && hasKerning ) {
					width += getKerning( font, prev_glyph, glyph );
				}

				width += glyph->x_advance;

				prev_num = num;
				prev_glyph = glyph;
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
size_t FTLIB_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth, int flags ) {
	const char *s, *olds;
	size_t width = 0;
	int gc;
	int advance = 0;
	wchar_t num, prev_num = 0;
	qglyph_t *glyph, *prev_glyph = NULL;
	renderString_f renderString;
	getKerning_f getKerning;
	bool hasKerning;

	if( !str || !font ) {
		return 0;
	}

	renderString = font->f->renderString;
	getKerning = font->f->getKerning;
	hasKerning = ( flags & TEXTDRAWFLAG_KERNING ) && font->hasKerning;

	for( s = str; s; ) {
		olds = s;
		gc = FTLIB_GrabChar( &s, &num, NULL, flags );
		if( gc == GRABCHAR_CHAR ) {
			if( num == '\n' ) {
				break;
			}

			if( num < ' ' ) {
				continue;
			}

			glyph = FTLIB_GetGlyph( font, num );
			if( !glyph ) {
				num = FTLIB_REPLACEMENT_GLYPH;
				glyph = FTLIB_GetGlyph( font, num );
			}

			if( !glyph->shader ) {
				renderString( font, olds );
			}

			advance = glyph->x_advance;
			if( hasKerning && prev_num ) {
				advance += getKerning( font, prev_glyph, glyph );
			}

			if( maxwidth && ( ( width + advance ) > maxwidth ) ) {
				s = olds;
				break;
			}

			width += advance;

			prev_num = num;
			prev_glyph = glyph;
		} else if( gc == GRABCHAR_COLOR ) {
			continue;
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}

	return (unsigned int)( s - str );
}

/*
* FTLIB_FontUnderline
*/
int FTLIB_FontUnderline( qfontface_t *font, int *thickness ) {
	int p = 0, t = 0;
	if( font ) {
		p = font->underlinePosition;
		t = font->underlineThickness;
	}
	if( thickness ) {
		*thickness = t;
	}
	return p;
}

/*
* FTLIB_FontAdvance
*/
size_t FTLIB_FontAdvance( qfontface_t *font ) {
	if( font ) {
		return font->advance;
	}
	return 0;
}

/*
* FTLIB_FontXHeight
*/
size_t FTLIB_FontXHeight( qfontface_t *font ) {
	if( font ) {
		if( !font->xHeight ) {
			qglyph_t *glyph = FTLIB_GetGlyph( font, 'x' );
			if( !glyph ) {
				glyph = FTLIB_GetGlyph( font, FTLIB_REPLACEMENT_GLYPH );
			}
			font->xHeight = glyph->height;
		}
		return font->xHeight;
	}
	return 0;
}

//===============================================================================
//STRINGS DRAWING
//===============================================================================

/*
* FTLIB_SetDrawCharIntercept
*/
fdrawchar_t FTLIB_SetDrawCharIntercept( fdrawchar_t intercept ) {
	fdrawchar_t old = drawCharIntercept;
	drawCharIntercept = intercept;
	return old;
}

/*
* FTLIB_DrawRawChar
*
* Draws one graphics character with 0 being transparent.
* It can be clipped to the top of the screen to allow the console to be
* smoothly scrolled off.
*/
void FTLIB_DrawRawChar( int x, int y, wchar_t num, qfontface_t *font, vec4_t color ) {
	qglyph_t *glyph;
	fdrawchar_t draw = R_DrawStretchPic;

	if( ( num <= ' ' ) || !font || ( y <= -font->height ) ) {
		return;
	}

	glyph = FTLIB_GetGlyph( font, num );
	if( !glyph ) {
		num = FTLIB_REPLACEMENT_GLYPH;
		glyph = FTLIB_GetGlyph( font, num );
	}

	if( !glyph->shader ) {
		font->f->renderString( font, Q_WCharToUtf8Char( num ) );
	}

	if( !glyph->width || !glyph->height ) {
		return;
	}

	if( drawCharIntercept ) {
		draw = drawCharIntercept;
	}

	draw( x + glyph->x_offset, y + font->glyphYOffset + glyph->y_offset,
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
void FTLIB_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color ) {
	qglyph_t *glyph;
	int x2, y2;
	float s1 = 0.0f, t1 = 0.0f, s2 = 1.0f, t2 = 1.0f;
	float tw, th;
	fdrawchar_t draw = R_DrawStretchPic;

	if( ( num <= ' ' ) || !font || ( xmax <= xmin ) || ( ymax <= ymin ) ) {
		return;
	}

	glyph = FTLIB_GetGlyph( font, num );
	if( !glyph ) {
		num = FTLIB_REPLACEMENT_GLYPH;
		glyph = FTLIB_GetGlyph( font, num );
	}

	if( !glyph->shader ) {
		font->f->renderString( font, Q_WCharToUtf8Char( num ) );
	}

	if( !glyph->width || !glyph->height ) {
		return;
	}

	x += glyph->x_offset;
	y += font->glyphYOffset + glyph->y_offset;
	x2 = x + glyph->width;
	y2 = y + glyph->height;
	if( ( x > xmax ) || ( y > ymax ) || ( x2 <= xmin ) || ( y2 <= ymin ) ) {
		return;
	}

	++xmax;
	++ymax;

	if( x < xmin ) {
		s1 = ( xmin - x ) / ( float )glyph->width;
		x = xmin;
	}
	if( y < ymin ) {
		t1 = ( ymin - y ) / ( float )glyph->height;
		y = ymin;
	}
	if( x2 > xmax ) {
		s2 = 1.0f - ( x2 - xmax ) / ( float )glyph->width;
		x2 = xmax;
	}
	if( y2 > ymax ) {
		t2 = 1.0f - ( y2 - ymax ) / ( float )glyph->height;
		y2 = ymax;
	}

	tw = glyph->s2 - glyph->s1;
	th = glyph->t2 - glyph->t1;

	if( drawCharIntercept ) {
		draw = drawCharIntercept;
	}

	draw( x, y, x2 - x, y2 - y,
		  glyph->s1 + tw * s1, glyph->t1 + th * t1,
		  glyph->s1 + tw * s2, glyph->t1 + th * t2,
		  color, glyph->shader );
}

/*
* FTLIB_DrawClampString
*/
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color, int flags ) {
	int xoffset = 0;
	vec4_t scolor;
	int colorindex;
	wchar_t num, prev_num = 0;
	const char *s = str, *olds;
	int gc;
	qglyph_t *glyph, *prev_glyph = NULL;
	renderString_f renderString;
	getKerning_f getKerning;
	bool hasKerning;

	if( !str || !font ) {
		return;
	}
	if( ( xmax <= xmin ) || ( ymax <= ymin ) || ( x > xmax ) || ( y > ymax ) ) {
		return;
	}

	Vector4Copy( color, scolor );

	renderString = font->f->renderString;
	getKerning = font->f->getKerning;
	hasKerning = ( flags & TEXTDRAWFLAG_KERNING ) && font->hasKerning;

	while( 1 ) {
		olds = s;
		gc = FTLIB_GrabChar( &s, &num, &colorindex, flags );
		if( gc == GRABCHAR_CHAR ) {
			if( num == '\n' ) {
				break;
			}

			if( num < ' ' ) {
				continue;
			}

			glyph = FTLIB_GetGlyph( font, num );
			if( !glyph ) {
				num = FTLIB_REPLACEMENT_GLYPH;
				glyph = FTLIB_GetGlyph( font, num );
			}

			if( !glyph->shader ) {
				renderString( font, olds );
			}

			if( prev_num ) {
				xoffset += prev_glyph->x_advance;
				if( hasKerning ) {
					xoffset += getKerning( font, prev_glyph, glyph );
				}
			}

			if( x + xoffset > xmax ) {
				break;
			}

			FTLIB_DrawClampChar( x + xoffset, y, num, xmin, ymin, xmax, ymax, font, scolor );

			prev_num = num;
			prev_glyph = glyph;
		} else if( gc == GRABCHAR_COLOR ) {
			assert( ( unsigned )colorindex < MAX_S_COLORS );
			VectorCopy( color_table[colorindex], scolor );
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}
}

/*
* FTLIB_DrawRawString - Doesn't care about aligning. Returns drawn len.
* It can stop when reaching maximum width when a value has been parsed.
*/
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, int *width, qfontface_t *font, vec4_t color, int flags ) {
	unsigned int xoffset = 0;
	vec4_t scolor;
	const char *s, *olds;
	int gc, colorindex;
	wchar_t num, prev_num = 0;
	qglyph_t *glyph, *prev_glyph = NULL;
	renderString_f renderString;
	getKerning_f getKerning;
	bool hasKerning;

	if( !str || !font ) {
		return 0;
	}

	Vector4Copy( color, scolor );

	renderString = font->f->renderString;
	getKerning = font->f->getKerning;
	hasKerning = ( flags & TEXTDRAWFLAG_KERNING ) && font->hasKerning;

	for( s = str; s; ) {
		olds = s;
		gc = FTLIB_GrabChar( &s, &num, &colorindex, flags );
		if( gc == GRABCHAR_CHAR ) {
			if( num == '\n' ) {
				break;
			}

			if( num < ' ' ) {
				continue;
			}

			glyph = FTLIB_GetGlyph( font, num );
			if( !glyph ) {
				num = FTLIB_REPLACEMENT_GLYPH;
				glyph = FTLIB_GetGlyph( font, num );
			}

			if( !glyph->shader ) {
				renderString( font, olds );
			}

			// ignore kerning at this point so the full width of the previous character will always be returned
			if( maxwidth && ( ( xoffset + glyph->x_advance ) > maxwidth ) ) {
				s = olds;
				break;
			}

			if( hasKerning && prev_num ) {
				xoffset += getKerning( font, prev_glyph, glyph );
			}

			FTLIB_DrawRawChar( x + xoffset, y, num, font, scolor );

			xoffset += glyph->x_advance;

			prev_num = num;
			prev_glyph = glyph;
		} else if( gc == GRABCHAR_COLOR ) {
			assert( ( unsigned )colorindex < MAX_S_COLORS );
			VectorCopy( color_table[colorindex], scolor );
		} else if( gc == GRABCHAR_END ) {
			break;
		} else {
			assert( 0 );
		}
	}

	if( width ) {
		*width = xoffset;
	}

	return ( s - str );
}

/* FTLIB_DrawMultilineString
 *
 * Draws a string with word wrap.
 */
int FTLIB_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, qfontface_t *font, vec4_t color, int flags ) {
	bool ended = false; // whether to stop drawing lines

	// characters and glyphs
	const char *oldstr;
	int gc, colorindex;
	wchar_t num, prev_num;
	qglyph_t *glyph, *prev_glyph;
	int glyph_width;
	renderString_f renderString;
	getKerning_f getKerning;
	bool hasKerning;

	// words
	const char *word = NULL; // beginning of the current word
	int word_chars, space_chars; // length of the current word and number of spaces before it
	int word_width, space_width; // width of the current word and the spaces before it
	vec4_t word_color; // starting color of the current word
	bool in_space; // whether currently in a sequence of spaces

	// line drawing
	const char *line = NULL; // beginning of the line
	int line_chars; // number of characters to draw in this line
	int line_width; // width of the current line
	int line_x; // x position of the current character in line
	vec4_t line_color, line_next_color; // first color in the line
	int line_height; // height of a single line
	int lines = 0; // number of lines drawn - the return value

	if( !str || !font || ( maxwidth <= 0 ) ) {
		return 0;
	}

	halign = halign % 3; // ignore vertical alignment

	renderString = font->f->renderString;
	getKerning = font->f->getKerning;
	hasKerning = ( flags & TEXTDRAWFLAG_KERNING ) && font->hasKerning;

	Vector4Copy( color, line_next_color );
	line_height = FTLIB_FontHeight( font );

	Vector4Copy( color, word_color );

	do {
		// reset
		word_chars = space_chars = 0;
		word_width = space_width = 0;
		in_space = true; // assume starting from a whitespace so preceding whitespaces can be skipped
		line_chars = 0;
		line_width = 0;
		Vector4Copy( line_next_color, line_color );

		// find where to wrap
		prev_num = 0;
		prev_glyph = NULL;
		while( str ) {
			oldstr = str;
			gc = FTLIB_GrabChar( &str, &num, &colorindex, flags );
			if( gc == GRABCHAR_CHAR ) {
				if( num == '\n' ) {
					if( !word_chars ) {
						space_chars = space_width = 0;
					}
					Vector4Copy( color, line_next_color );
					break;
				}

				if( num < ' ' ) {
					continue;
				}

				glyph = FTLIB_GetGlyph( font, num );
				if( !glyph ) {
					num = FTLIB_REPLACEMENT_GLYPH;
					glyph = FTLIB_GetGlyph( font, num );
				}

				if( !glyph->shader ) {
					renderString( font, oldstr );
				}

				if( Q_IsBreakingSpaceChar( num ) ) {
					if( in_space ) {
						if( !line_chars ) {
							continue; // skip preceding whitespaces in a line
						}
					} else {
						in_space = true;

						// reached the space without wrapping - send the current word to the line
						line_chars += space_chars + word_chars;
						word_chars = space_chars = 0;
						line_width += space_width + word_width;
						word_width = space_width = 0;
					}
					space_chars++;
					if( hasKerning && prev_num ) {
						space_width += getKerning( font, prev_glyph, glyph );
					}
					space_width += glyph->x_advance;
				} else {
					in_space = false;

					glyph_width = glyph->x_advance;
					if( hasKerning && prev_num ) {
						glyph_width += getKerning( font, prev_glyph, glyph );
					}

					if( !word_chars ) {
						word = oldstr;
						Vector4Copy( line_next_color, word_color );
					}

					if( line_chars ) {
						// wrap after the previous word, ignoring spaces between the words
						if( ( line_width + space_width + word_width + glyph_width ) > maxwidth ) {
							str = word;
							Vector4Copy( word_color, line_next_color );
							word_chars = space_chars = 0;
							word_width = space_width = 0;
							break;
						}
					} else {
						line = word;
						if( word_chars ) { // always draw at least 1 character in a line
							if( ( word_width + glyph_width ) > maxwidth ) {
								str = oldstr;
								break;
							}
						}
					}

					word_chars++;
					word_width += glyph_width;
				}

				prev_num = num;
				prev_glyph = glyph;
			} else if( gc == GRABCHAR_COLOR ) {
				assert( ( unsigned )colorindex < MAX_S_COLORS );
				VectorCopy( color_table[colorindex], line_next_color );
				if( !line_chars && !word_chars ) {
					Vector4Copy( line_next_color, line_color );
				}
			} else if( gc == GRABCHAR_END ) {
				ended = true;
				break;
			} else {
				assert( 0 );
			}
		}
		// add the remaining part of the word
		line_chars += space_chars + word_chars;
		line_width += space_width + word_width;

		// draw the line
		if( line_chars > 0 ) {
			line_x = x;
			if( halign == ALIGN_CENTER_TOP ) {
				line_x -= line_width >> 1;
			} else if( halign == ALIGN_RIGHT_TOP ) {
				line_x -= line_width;
			}

			prev_num = 0;
			prev_glyph = NULL;
			while( ( line_chars > 0 ) && line ) {
				gc = FTLIB_GrabChar( &line, &num, &colorindex, flags );
				if( gc == GRABCHAR_CHAR ) {
					if( num < ' ' ) {
						continue;
					}

					line_chars--;

					glyph = FTLIB_GetGlyph( font, num );
					if( !glyph ) {
						num = FTLIB_REPLACEMENT_GLYPH;
						glyph = FTLIB_GetGlyph( font, num );
					}

					if( hasKerning && prev_num ) {
						line_x += getKerning( font, prev_glyph, glyph );
					}

					FTLIB_DrawRawChar( line_x, y, num, font, line_color );

					line_x += glyph->x_advance;

					prev_num = num;
					prev_glyph = glyph;
				} else if( gc == GRABCHAR_COLOR ) {
					assert( ( unsigned )colorindex < MAX_S_COLORS );
					VectorCopy( color_table[colorindex], line_color );
				} else if( gc == GRABCHAR_END ) {
					break;
				} else {
					assert( 0 );
				}
			}
		}

		lines++;
		if( ( maxlines > 0 ) && ( lines >= maxlines ) ) {
			break;
		}
		y += line_height;

	} while( !ended );

	return lines;
}
