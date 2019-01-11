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

#include "gameshared/q_shared.h"
#include "qcommon/qfiles.h"
#include "bsp.h"

/*
==============================================================

BSP FORMATS

==============================================================
*/

static const int mod_IBSPQ3Versions[] = { Q3BSPVERSION, RTCWBSPVERSION, 0 };
static const int mod_RBSPQ3Versions[] = { RBSPVERSION, 0 };
static const int mod_FBSPQ3Versions[] = { QFBSPVERSION, 0 };

const bspFormatDesc_t q3BSPFormats[] = {
	{ QFBSPHEADER, mod_FBSPQ3Versions, BSP_RAVEN, LUMP_ENTITIES },
	{ IDBSPHEADER, mod_IBSPQ3Versions, BSP_NONE, LUMP_ENTITIES },
	{ RBSPHEADER, mod_RBSPQ3Versions, BSP_RAVEN, LUMP_ENTITIES },
	{ },
};

/*
* Com_FindBSPFormat
*/
const bspFormatDesc_t *Q_FindBSPFormat( const bspFormatDesc_t *formats, const char *header, int version ) {
	int j;
	const bspFormatDesc_t *bspFormat;

	// check whether any of passed formats matches the header/version combo
	for( bspFormat = formats; bspFormat->header; bspFormat++ ) {
		if( strlen( bspFormat->header ) && strncmp( header, bspFormat->header, strlen( bspFormat->header ) ) ) {
			continue;
		}

		// check versions listed for this header
		for( j = 0; bspFormat->versions[j]; j++ ) {
			if( version == bspFormat->versions[j] ) {
				break;
			}
		}

		// found a match
		if( bspFormat->versions[j] ) {
			return bspFormat;
		}
	}

	return NULL;
}

/*
* Com_FindFormatDescriptor
*/
const modelFormatDescr_t *Q_FindFormatDescriptor( const modelFormatDescr_t *formats, const uint8_t *buf, const bspFormatDesc_t **bspFormat ) {
	int i;
	const modelFormatDescr_t *descr;

	// search for a matching header
	for( i = 0, descr = formats; descr->header; i++, descr++ ) {
		if( descr->header[0] == '*' ) {
			const char *header;
			int version;

			header = ( const char * )buf;
			version = LittleLong( *( (int *)( (uint8_t *)buf + descr->headerLen ) ) );

			// check whether any of specified formats matches the header/version combo
			*bspFormat = Q_FindBSPFormat( descr->bspFormats, header, version );
			if( *bspFormat ) {
				return descr;
			}
		} else {
			if( !strncmp( (const char *)buf, descr->header, descr->headerLen ) ) {
				return descr;
			}
		}
	}

	return NULL;
}
