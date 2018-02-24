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

#include "r_local.h"

/*
=============================================================

STANDARD PROJECTIVE SHADOW MAPS (SSM)

=============================================================
*/

/*
* R_DrawLights
*/
void R_DrawLights( void ) {
	unsigned i;
	rtlight_t *l;
	float farClip;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return;
	}

	if( !R_PushRefInst() ) {
		return;
	}

	for( i = 0; i < rn.numRealtimeLights; i++ ) {
		l = rn.rtlights[i];

		if( l->frameCount == rsc.frameCount ) {
			continue;
		}

		l->frameCount = rsc.frameCount;

		farClip = l->intensity;

		//rn.renderTarget = shadowmap->fbo;
		rn.farClip = farClip;
		rn.renderFlags = RF_SHADOWMAPVIEW | RF_FLIPFRONTFACE;
		//if( !( shadowmap->flags & IT_DEPTH ) ) {
		//	rn.renderFlags |= RF_SHADOWMAPVIEW_RGB;
		//}
		rn.clipFlags = 31; // clip by far plane too
		rn.meshlist = &r_shadowlist;
		rn.portalmasklist = NULL;
		rn.lod_dist_scale_for_fov = 0;
		rn.rtLight = l;
		VectorCopy( l->origin, rn.lodOrigin );
		VectorCopy( l->origin, rn.viewOrigin );
	}

	R_PopRefInst();
}
