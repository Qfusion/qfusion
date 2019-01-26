/*
Copyright (C) 1997-2001 Id Software, Inc.
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
#pragma once

#include "glad.h"

//====================================================================

#define MAX_TEXTURE_UNITS               8

#define MAX_GLSL_UNIFORM_BONES          100
#define MAX_GLSL_UNIFORM_INSTANCES      40

#define GAMMARAMP_STRIDE                4096

//====================================================================

enum {
	GLSTATE_NONE = 0,

	//
	// glBlendFunc args
	//
	GLSTATE_SRCBLEND_ZERO                   = 1,
	GLSTATE_SRCBLEND_ONE                    = 2,
	GLSTATE_SRCBLEND_DST_COLOR              = 1 | 2,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR    = 4,
	GLSTATE_SRCBLEND_SRC_ALPHA              = 1 | 4,
	GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA    = 2 | 4,
	GLSTATE_SRCBLEND_DST_ALPHA              = 1 | 2 | 4,
	GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA    = 8,

	GLSTATE_DSTBLEND_ZERO                   = 16,
	GLSTATE_DSTBLEND_ONE                    = 32,
	GLSTATE_DSTBLEND_SRC_COLOR              = 16 | 32,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR    = 64,
	GLSTATE_DSTBLEND_SRC_ALPHA              = 16 | 64,
	GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA    = 32 | 64,
	GLSTATE_DSTBLEND_DST_ALPHA              = 16 | 32 | 64,
	GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA    = 128,

	GLSTATE_NO_COLORWRITE                   = 0x100,
	GLSTATE_ALPHAWRITE                      = 0x200,

	GLSTATE_DEPTHWRITE                      = 0x400,
	GLSTATE_DEPTHFUNC_EQ                    = 0x800,
	GLSTATE_DEPTHFUNC_GT                    = 0x1000,

	GLSTATE_OFFSET_FILL                     = 0x2000,
	GLSTATE_NO_DEPTH_TEST                   = 0x4000,

	GLSTATE_STENCIL_TEST                    = 0x8000,

	GLSTATE_ALPHATEST                       = 0x10000,

	GLSTATE_MARK_END                        = 0x20000 // SHADERPASS_MARK_BEGIN
};

#define GLSTATE_MASK        ( GLSTATE_MARK_END - 1 )

// #define SHADERPASS_SRCBLEND_MASK (((GLSTATE_SRCBLEND_DST_ALPHA)<<1)-GLSTATE_SRCBLEND_ZERO)
#define GLSTATE_SRCBLEND_MASK   0xF
// #define SHADERPASS_DSTBLEND_MASK (((GLSTATE_DSTBLEND_DST_ALPHA)<<1)-GLSTATE_DSTBLEND_ZERO)
#define GLSTATE_DSTBLEND_MASK   0xF0
#define GLSTATE_BLEND_MASK      ( GLSTATE_SRCBLEND_MASK | GLSTATE_DSTBLEND_MASK )

#define GLSTATE_BLEND_ADD       ( GLSTATE_SRCBLEND_ONE | GLSTATE_DSTBLEND_ONE )

//====================================================================

typedef struct {
	bool texture_filter_anisotropic;
	bool texture_compression;
	bool get_program_binary;
	bool texture_sRGB;
	bool texture_sRGB_decode;

	bool khr_debug;
	bool amd_debug;
	bool nvidia_meminfo;
	bool ati_meminfo;

	bool glsl130;

	bool ARB_half_float_pixel;
	bool ARB_half_float_vertex;
	bool ARB_texture_float;
	bool ARB_draw_instanced;
	bool ARB_instanced_arrays;
	bool ARB_framebuffer_object;
	bool ARB_texture_swizzle;
	bool EXT_texture_array;
	bool ARB_texture_rg;
	bool ARB_vertex_array_object;
	bool ARB_explicit_attrib_location;
} glextinfo_t;

typedef struct {
	const char      *rendererString;
	const char      *vendorString;
	const char      *versionString;
	unsigned versionHash;

	int width, height;
	bool fullScreen;
	bool borderless;

	int stencilBits;

	bool hwGamma;
	unsigned short gammaRampSize;
	unsigned short originalGammaRamp[3 * GAMMARAMP_STRIDE];

	float depthEpsilon;

	int maxTextureSize;
	int maxTextureUnits;
	int maxTextureCubemapSize;
	int maxTexture3DSize;
	int maxTextureLayers;
	int maxTextureFilterAnisotropic;
	int maxRenderbufferSize;
	int maxVertexUniformComponents;
	int maxVertexAttribs;
	int maxFragmentUniformComponents;
	int maxFramebufferSamples;

	bool forceRGBAFramebuffers;             // PowerVR hack - its blending interprets alpha in RGB FBs as 0, not 1
	bool sSRGB;

	glextinfo_t ext;
} glconfig_t;

extern glconfig_t glConfig;

void    VID_Swap(); // TODO: this doesn't belong here
bool    VID_GetGammaRamp( size_t stride, unsigned short *psize, unsigned short *ramp ); // TODO: this doesn't belong here
void    VID_SetGammaRamp( size_t stride, unsigned short size, unsigned short *ramp );
