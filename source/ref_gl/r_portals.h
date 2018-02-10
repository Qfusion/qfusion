/*
Copyright (C) 2017 Victor Luchits

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

#ifndef R_PORTALS_H
#define R_PORTALS_H

#include "r_public.h"
#include "r_math.h"
#include "r_mesh.h"
#include "r_image.h"
#include "r_shader.h"

typedef struct portalSurface_s {
	const entity_t  *entity;
	cplane_t plane, untransformed_plane;
	const shader_t  *shader;
	vec3_t mins, maxs, centre;
	image_t         *texures[2];            // front and back portalmaps
	skyportal_t     *skyPortal;
} portalSurface_t;

extern drawList_t r_portallist, r_skyportallist;

portalSurface_t *R_AddPortalSurface( const entity_t *ent, const shader_t *shader, void *drawSurf );
portalSurface_t *R_AddSkyportalSurface( const entity_t *ent, const shader_t *shader, void *drawSurf );
void R_UpdatePortalSurface( portalSurface_t *portalSurface, const mesh_t *mesh,
	const vec3_t mins, const vec3_t maxs, const shader_t *shader, void *drawSurf );
void R_DrawPortals( void );

#endif // R_PORTALS_H
