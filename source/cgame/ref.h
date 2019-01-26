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
#pragma once

// FIXME: move these to r_local.h?
#define MAX_DLIGHTS             32
#define MAX_ENTITIES            2048
#define MAX_POLY_VERTS          3000
#define MAX_POLYS               2048

// entity_state_t->renderfx flags
#define RF_MINLIGHT             0x1       // always have some light (viewmodel)
#define RF_FULLBRIGHT           0x2       // always draw full intensity
#define RF_FRAMELERP            0x4
#define RF_NOSHADOW             0x8

#define RF_WEAPONMODEL          0x20     // only draw through eyes and depth hack
#define RF_FORCENOLOD           0x80
#define RF_NOPORTALENTS         0x100
#define RF_ALPHAHACK            0x200   // force alpha blending on opaque passes, read alpha from entity
#define RF_GREYSCALE            0x400
#define RF_NODEPTHTEST          0x800
#define RF_NOCOLORWRITE         0x1000

// refdef flags
#define RDF_UNDERWATER          0x1     // warp the screen as apropriate
#define RDF_NOWORLDMODEL        0x2     // used for player configuration screen
#define RDF_OLDAREABITS         0x4     // forces R_MarkLeaves if not set
#define RDF_WORLDOUTLINES       0x8     // draw cell outlines for world surfaces
#define RDF_CROSSINGWATER       0x10    // potentially crossing water surface
#define RDF_USEORTHO            0x20    // use orthographic projection
#define RDF_BLURRED             0x40

// skm flags
#define SKM_ATTACHMENT_BONE     1

typedef struct orientation_s {
	mat3_t axis;
	vec3_t origin;
} orientation_t;

typedef struct bonepose_s {
	dualquat_t dualquat;
} bonepose_t;

typedef struct fragment_s {
	int firstvert;
	int numverts;                       // can't exceed MAX_POLY_VERTS
	vec3_t normal;
} fragment_t;

typedef struct poly_s {
	int numverts;
	vec4_t *verts;
	vec4_t *normals;
	vec2_t *stcoords;
	byte_vec4_t *colors;
	int numelems;
	unsigned short *elems;
	struct shader_s *shader;
	int renderfx;
} poly_t;

typedef enum {
	RT_MODEL,
	RT_SPRITE,

	NUM_RTYPES
} refEntityType_t;

typedef struct entity_s {
	refEntityType_t rtype;
	union {
		int flags;
		int renderfx;
	};

	struct model_s *model;              // opaque type outside refresh

	/*
	** most recent data
	*/
	mat3_t axis;
	vec3_t origin, origin2;
	vec3_t lightingOrigin;
	int frame;
	bonepose_t *boneposes;              // pretransformed boneposes for current frame

	/*
	** previous data for lerping
	*/
	int oldframe;
	bonepose_t *oldboneposes;           // pretransformed boneposes for old frame
	float backlerp;                     // 0.0 = current, 1.0 = old

	/*
	** texturing
	*/
	struct skinfile_s *customSkin;      // registered .skin file
	struct shader_s *customShader;      // NULL for inline skin

	/*
	** misc
	*/
	int64_t shaderTime;
	union {
		byte_vec4_t color;
		uint8_t shaderRGBA[4];
	};

	float scale;
	float radius;                       // used as RT_SPRITE's radius
	float rotation;

	float outlineHeight;
	union {
		byte_vec4_t outlineColor;
		uint8_t outlineRGBA[4];
	};
} entity_t;

typedef struct refdef_s {
	int x, y, width, height;            // viewport, in virtual screen coordinates
	int scissor_x, scissor_y, scissor_width, scissor_height;
	int ortho_x, ortho_y;
	float fov_x, fov_y;
	vec3_t vieworg;
	mat3_t viewaxis;
	float blend[4];                     // rgba 0-1 full screen blend
	int64_t time;                       // time is used for timing offsets
	int rdflags;                        // RDF_UNDERWATER, etc
	uint8_t *areabits;                  // if not NULL, only areas with set bits will be drawn
	float minLight;                     // minimum value of ambient lighting applied to RF_MINLIGHT entities
} refdef_t;
