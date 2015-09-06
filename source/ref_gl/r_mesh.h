/*
Copyright (C) 2002-2007 Victor Luchits

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
#ifndef R_MESH_H
#define R_MESH_H

struct shader_s;
struct mfog_s;
struct portalSurface_s;

#define MIN_RENDER_MESHES			2048

typedef struct mesh_s
{
	unsigned short		numVerts;
	vec4_t				*xyzArray;
	vec4_t				*normalsArray;
	vec4_t				*sVectorsArray;
	vec2_t				*stArray;
	vec2_t				*lmstArray[MAX_LIGHTMAPS];
	byte_vec4_t			*lmlayersArray[( MAX_LIGHTMAPS + 3 ) / 4];
	byte_vec4_t			*colorsArray[MAX_LIGHTMAPS];

	uint8_t				*blendIndices;
	uint8_t				*blendWeights;

	unsigned short		numElems;
	elem_t				*elems;
} mesh_t;

typedef struct
{
	unsigned int		numVerts, numElems;
	unsigned int		firstVert, firstElem;
} vboSlice_t;

typedef struct
{
	unsigned int		distKey;
	unsigned int		sortKey;
	drawSurfaceType_t	*drawSurf;
} sortedDrawSurf_t;

typedef struct
{
	unsigned int		numDrawSurfs, maxDrawSurfs;
	sortedDrawSurf_t	*drawSurfs;

	unsigned int		maxVboSlices;
	vboSlice_t			*vboSlices;

	unsigned			numSliceVerts, numSliceVertsReal;
	unsigned			numSliceElems, numSliceElemsReal;
} drawList_t;

typedef void (*drawSurf_cb)( const entity_t *, const struct shader_s *, const struct mfog_s *, const struct portalSurface_s *, unsigned int, void * );
typedef void (*batchDrawSurf_cb)( const entity_t *, const struct shader_s *, const struct mfog_s *, const struct portalSurface_s *, unsigned int, void * );

#endif // R_MESH_H
