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
#ifndef __REF_H
#define __REF_H

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
#define RF_VIEWERMODEL          0x10     // don't draw through eyes, only mirrors
#define RF_WEAPONMODEL          0x20     // only draw through eyes and depth hack
#define RF_CULLHACK             0x40
#define RF_FORCENOLOD           0x80
#define RF_NOPORTALENTS         0x100
#define RF_ALPHAHACK            0x200   // force alpha blending on opaque passes, read alpha from entity
#define RF_GREYSCALE            0x400
#define RF_NODEPTHTEST          0x800
#define RF_NOCOLORWRITE         0x1000

// refdef flags
#define RDF_UNDERWATER          0x1     // warp the screen as apropriate
#define RDF_NOWORLDMODEL        0x2     // used for player configuration screen
#define RDF_UNUSED              0x4
#define RDF_PORTALINVIEW        0x8     // cull entities using vis too because pvs\areabits are merged serverside
#define RDF_SKYPORTALINVIEW     0x10    // draw skyportal instead of regular sky
#define RDF_FLIPPED             0x20
#define RDF_WORLDOUTLINES       0x40    // draw cell outlines for world surfaces
#define RDF_CROSSINGWATER       0x80    // potentially crossing water surface
#define RDF_USEORTHO            0x100   // use orthographic projection
#define RDF_BLURRED             0x200

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
	int fognum;                         // -1 - no fog
	                                    //  0 - determine fog in R_AddPolyToScene
	                                    // >0 - valid fog volume number returned by R_GetClippedFragments
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
	int fognum;
	int renderfx;
} poly_t;

typedef struct {
	float rgb[3];                       // 0.0 - 2.0
} lightstyle_t;

typedef struct {
	float fov;
	float scale;
	vec3_t vieworg;
	vec3_t viewanglesOffset;
	bool noEnts;
} skyportal_t;

typedef enum {
	RT_MODEL,
	RT_SPRITE,
	RT_PORTALSURFACE,

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
	skyportal_t skyportal;
	uint8_t *areabits;                  // if not NULL, only areas with set bits will be drawn
	float weaponAlpha;
	float minLight;                     // minimum value of ambient lighting applied to RF_MINLIGHT entities
	struct shader_s *colorCorrection;   // post processing color correction lookup table to apply
} refdef_t;

typedef struct {
	// MUST MATCH cin_img_plane_t
	//===============================

	// the width of this plane
	// note that row data has to be continous
	// so for planes where stride != image_width,
	// the width should be max (stride, image_width)
	int width;

	// the height of this plane
	int height;

	// the offset in bytes between successive rows
	int stride;

	// pointer to the beginning of the first row
	unsigned char *data;
} ref_img_plane_t;

typedef struct {
	int image_width;
	int image_height;

	int width;
	int height;

	// cropping factors
	int x_offset;
	int y_offset;
	ref_img_plane_t yuv[3];

	// EVERYTHING ABOVE MATCH cin_yuv_t
	//===============================
} ref_yuv_t;

#endif // __REF_H
