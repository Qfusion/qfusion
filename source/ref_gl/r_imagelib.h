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

#ifndef R_IMAGELIB_H
#define R_IMAGELIB_H

// odd values include alpha channel
typedef enum {
	IMGCOMP_RGB,
	IMGCOMP_RGBA,
	IMGCOMP_BGR,
	IMGCOMP_BGRA,
} r_imgcomp_t;

typedef struct {
	int width;
	int height;
	int samples;
	r_imgcomp_t comp;
	uint8_t *pixels;
} r_imginfo_t;

void R_Imagelib_Init( void );
void R_Imagelib_Shutdown( void );

r_imginfo_t LoadTGA( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );
bool WriteTGA( const char *name, r_imginfo_t *info );

r_imginfo_t LoadJPG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );
bool WriteJPG( const char *name, r_imginfo_t *info, int quality );

r_imginfo_t LoadPNG( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );
bool WritePNG( const char *name, r_imginfo_t *info );

r_imginfo_t LoadPCX( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );

r_imginfo_t LoadWAL( const char *name, uint8_t *( *allocbuf )( void *, size_t, const char *, int ), void *uptr );

r_imginfo_t LoadSVG( const char *name, int width, int height, 
	uint8_t *( *allocbuf )(void *, size_t, const char *, int), void *uptr );

void DecompressETC1( const uint8_t *in, int width, int height, uint8_t *out, bool bgr );

#endif // R_IMAGELIB_H
