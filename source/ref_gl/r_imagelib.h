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

#ifndef __R_IMAGELIB_H__
#define __R_IMAGELIB_H__

// odd values include alpha channel
typedef enum
{
	IMGCOMP_RGB,
	IMGCOMP_RGBA,
	IMGCOMP_BGR,
	IMGCOMP_BGRA,
} r_imgcomp_t;

typedef struct
{
	int width;
	int height;
	int samples;
	r_imgcomp_t comp;
	qbyte *pixels;
} r_imginfo_t;

r_imginfo_t LoadTGA( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr );
qboolean WriteTGA( const char *name, r_imginfo_t *info, int quality );

r_imginfo_t LoadJPG( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr );
qboolean WriteJPG( const char *name, r_imginfo_t *info, int quality );

r_imginfo_t LoadPNG( const char *name, qbyte *(*allocbuf)( void *, size_t, const char *, int ), void *uptr );

#endif // __R_IMAGELIB_H__
