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

#pragma once

#include "r_public.h"
#include "r_math.h"
#include "r_model.h"
#include "r_surface.h"
#include "r_portals.h"
#include "r_vattribs.h"
#include "r_image.h"

#define MAX_SUPER_STYLES			128

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

typedef struct
{
	int rowY;
	int currentX;
} lightmapAllocRow_t;

typedef struct
{
	int width;
	int height;
	int currentY;
	lightmapAllocRow_t *rows;
} lightmapAllocState_t;

void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius, bool noWorldLight, bool noDLight );
void R_BuildLightmaps( model_t *mod, int numLightmaps, int w, int h, const uint8_t *data, lightmapRect_t *rects );
void R_InitLightStyles( model_t *mod );
int R_AddSuperLightStyle( model_t *mod, const int *lightmaps, const uint8_t *lightmapStyles, const uint8_t *vertexStyles, lightmapRect_t **lmRects );
void R_SortSuperLightStyles( model_t *mod );
void R_TouchLightmapImages( model_t *mod );

void R_AllocLightmap_Init( lightmapAllocState_t *state, int width, int height );
void R_AllocLightmap_Reset( lightmapAllocState_t *state );
void R_AllocLightmap_Free( lightmapAllocState_t *state );
bool R_AllocLightmap_Block( lightmapAllocState_t *state, int blockwidth, int blockheight, int *outx, int *outy );
