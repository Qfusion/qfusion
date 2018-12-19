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
#pragma once

#define MAX_DRAWSURF_SURFS		128 // limit the number of surfaces to a sane 8-bit integer

typedef enum {
	ST_NONE,
	ST_BSP,
	ST_SKY,
	ST_ALIAS,
	ST_SKELETAL,
	ST_SPRITE,
	ST_POLY,
	ST_NULLMODEL,

	ST_MAX_TYPES,

	ST_END = INT_MAX        // ensures that sizeof( surfaceType_t ) == sizeof( int )
} drawSurfaceType_t;

typedef struct {
	drawSurfaceType_t type;

	unsigned int visFrame;          // should be drawn when node is crossed

	unsigned int numVerts;
	unsigned int numElems;

	unsigned int numWorldSurfaces;

	unsigned int firstVboVert, firstVboElem;

	unsigned int numInstances;

	int superLightStyle;

	unsigned int numLightmaps;

	unsigned int surfFlags;

	int vbo;

	unsigned int *worldSurfaces; // [numSurfaces]

	instancePoint_t *instances;

	struct shader_s *shader;

	void *listSurf;                 // only valid if visFrame == rf.frameCount
} drawSurfaceBSP_t;

typedef struct {
	drawSurfaceType_t type;

	float skyMins[2][6];
	float skyMaxs[2][6];
} drawSurfaceSky_t;

typedef struct {
	drawSurfaceType_t type;

	struct maliasmesh_s *mesh;

	struct model_s *model;
} drawSurfaceAlias_t;

typedef struct {
	drawSurfaceType_t type;

	struct mskmesh_s *mesh;

	struct model_s *model;
} drawSurfaceSkeletal_t;

typedef struct {
	drawSurfaceType_t type;

	int renderfx;

	int numElems;
	int numVerts;

	vec4_t *xyzArray;
	vec4_t *normalsArray;
	vec2_t *stArray;
	byte_vec4_t *colorsArray;
	elem_t *elems;
	struct shader_s *shader;
} drawSurfacePoly_t;
