/*
Copyright (C) 2014 Victor Luchits

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

typedef struct {
	int width;
	int height;
	int samples;
	uint8_t *pixels;
} r_imginfo_t;

r_imginfo_t IMG_LoadImage( const char * filename, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );

bool WriteTGA( const char * filename, r_imginfo_t *info );
bool WriteJPG( const char * filename, r_imginfo_t *info, int quality );

void DecompressETC1( const uint8_t *in, int width, int height, uint8_t *out, bool bgr );
