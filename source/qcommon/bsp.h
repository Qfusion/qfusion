/*
Copyright (C) 2013 Victor Luchits

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

#ifndef __BSP_H__
#define __BSP_H__

/*
==============================================================

BSP FORMATS

==============================================================
*/

typedef void ( *modelLoader_t )( void *param0, void *param1, void *param2, void *param3 );

#define BSP_NONE		0
#define BSP_RAVEN		1
#define BSP_NOAREAS		2

typedef struct
{
	const char *header;
	const int *versions;
	int lightmapWidth;
	int lightmapHeight;
	int flags;
	int entityLumpNum;
} bspFormatDesc_t;

typedef struct
{
	const char *header;
	int headerLen;
	const bspFormatDesc_t *bspFormats;
	int maxLods;
	modelLoader_t loader;
} modelFormatDescr_t;

extern const bspFormatDesc_t q3BSPFormats[];
extern const bspFormatDesc_t q2BSPFormats[];
extern const bspFormatDesc_t q1BSPFormats[];

const bspFormatDesc_t *Q_FindBSPFormat( const bspFormatDesc_t *formats, const char *header, int version );
const modelFormatDescr_t *Q_FindFormatDescriptor( const modelFormatDescr_t *formats, const uint8_t *buf, const bspFormatDesc_t **bspFormat );

#endif // __BSP_H__
