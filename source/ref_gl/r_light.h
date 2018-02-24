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

#ifndef R_LIGHT_H
#define R_LIGHT_H

#include "r_public.h"
#include "r_math.h"
#include "r_model.h"
#include "r_surface.h"
#include "r_portals.h"
#include "r_vattribs.h"

// flags for rtlight rendering
#define LIGHTFLAG_NORMALMODE 1
#define LIGHTFLAG_REALTIMEMODE 2

#define DLIGHT_SCALE        0.5f
#define MAX_SUPER_STYLES    128

typedef struct superLightStyle_s {
	vattribmask_t vattribs;
	int lightmapNum[MAX_LIGHTMAPS];
	int lightmapStyles[MAX_LIGHTMAPS];
	int vertexStyles[MAX_LIGHTMAPS];
	float stOffset[MAX_LIGHTMAPS][2];
} superLightStyle_t;

typedef struct {
	int texNum;
	int texLayer;
	float texMatrix[2][2];
} lightmapRect_t;

void        R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius, bool noWorldLight );
float       R_LightExposureForOrigin( const vec3_t origin );
void        R_BuildLightmaps( model_t *mod, int numLightmaps, int w, int h, const uint8_t *data, lightmapRect_t *rects );
void        R_InitLightStyles( model_t *mod );
superLightStyle_t   *R_AddSuperLightStyle( model_t *mod, const int *lightmaps, const uint8_t *lightmapStyles, const uint8_t *vertexStyles, lightmapRect_t **lmRects );
void        R_SortSuperLightStyles( model_t *mod );
void        R_TouchLightmapImages( model_t *mod );

void        R_InitCoronas( void );
void        R_BatchCoronaSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, const portalSurface_t *portalSurface, unsigned int shadowBits, drawSurfaceType_t *drawSurf );
void        R_DrawCoronas( void );
void        R_ShutdownCoronas( void );

#endif // R_LIGHT_H
