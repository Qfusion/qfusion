/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "r_local.h"
#include "r_imagelib.h"
#include "../../qalgo/hash.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <setjmp.h>

r_imginfo_t IMG_LoadImage( const char * filename, uint8_t * ( *allocbuf )( void *, size_t, const char *, int ), void * uptr ) {
	( void ) allocbuf;

	r_imginfo_t ret;
	memset( &ret, 0, sizeof( ret ) );

	uint8_t * data;
	size_t size = R_LoadFile( filename, ( void ** ) &data );
	if( data == NULL )
		return ret;

	ret.pixels = stbi_load_from_memory( data, size, &ret.width, &ret.height, &ret.samples, 0 );

	R_FreeFile( data );

	return ret;
}

bool WriteTGA( const char * filename, r_imginfo_t * img ) {
	FS_CreateAbsolutePath( filename );
	return stbi_write_tga( filename, img->width, img->height, img->samples, img->pixels ) != 0;
}

bool WriteJPG( const char * filename, r_imginfo_t * img, int quality ) {
	FS_CreateAbsolutePath( filename );
	return stbi_write_jpg( filename, img->width, img->height, img->samples, img->pixels, quality ) != 0;
}
