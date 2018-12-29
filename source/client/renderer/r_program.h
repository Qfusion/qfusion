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

typedef uint64_t r_glslfeat_t;

#define GLSL_BIT( x )                           ( 1ULL << ( x ) )
#define GLSL_BITS_VERSION                       22

#define DEFAULT_GLSL_MATERIAL_PROGRAM           "material"
#define DEFAULT_GLSL_OUTLINE_PROGRAM            "outline"
#define DEFAULT_GLSL_Q3A_SHADER_PROGRAM         "q3AShader"
#define DEFAULT_GLSL_COLORCORRECTION_PROGRAM    "colorCorrection"
#define DEFAULT_GLSL_KAWASE_BLUR_PROGRAM        "kawaseBlur"

// program types
enum {
	GLSL_PROGRAM_TYPE_NONE,
	GLSL_PROGRAM_TYPE_MATERIAL,
	GLSL_PROGRAM_TYPE_OUTLINE,
	GLSL_PROGRAM_TYPE_Q3A_SHADER,
	GLSL_PROGRAM_TYPE_COLOR_CORRECTION,
	GLSL_PROGRAM_TYPE_KAWASE_BLUR,

	GLSL_PROGRAM_TYPE_MAXTYPE
};

// features common for all program types
#define GLSL_SHADER_COMMON_GREYSCALE            GLSL_BIT( 0 )

#define GLSL_SHADER_COMMON_RGB_GEN_VERTEX       GLSL_BIT( 1 )
#define GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX GLSL_BIT( 2 )
#define GLSL_SHADER_COMMON_RGB_DISTANCERAMP     ( GLSL_SHADER_COMMON_RGB_GEN_VERTEX |\
													GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX )

#define GLSL_SHADER_COMMON_SRGB2LINEAR          GLSL_BIT( 3 )

#define GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX     GLSL_BIT( 4 )
#define GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX GLSL_BIT( 5 )
#define GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP    ( GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX | \
													GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX )

#define GLSL_SHADER_COMMON_BONE_TRANSFORMS1     GLSL_BIT( 6 )
#define GLSL_SHADER_COMMON_BONE_TRANSFORMS2     GLSL_BIT( 7 )
#define GLSL_SHADER_COMMON_BONE_TRANSFORMS3     ( GLSL_SHADER_COMMON_BONE_TRANSFORMS1 | GLSL_SHADER_COMMON_BONE_TRANSFORMS2 )
#define GLSL_SHADER_COMMON_BONE_TRANSFORMS4     GLSL_BIT( 8 )
#define GLSL_SHADER_COMMON_BONE_TRANSFORMS      ( GLSL_SHADER_COMMON_BONE_TRANSFORMS1 | GLSL_SHADER_COMMON_BONE_TRANSFORMS2 \
												  | GLSL_SHADER_COMMON_BONE_TRANSFORMS3 | GLSL_SHADER_COMMON_BONE_TRANSFORMS4 )

#define GLSL_SHADER_COMMON_DRAWFLAT             GLSL_BIT( 12 )

#define GLSL_SHADER_COMMON_AUTOSPRITE           GLSL_BIT( 13 )
#define GLSL_SHADER_COMMON_AUTOSPRITE2          GLSL_BIT( 14 )

#define GLSL_SHADER_COMMON_LIGHTING             GLSL_BIT( 15 )

#define GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS GLSL_BIT( 16 )
#define GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS  GLSL_BIT( 17 )

#define GLSL_SHADER_COMMON_SOFT_PARTICLE        GLSL_BIT( 18 )

#define GLSL_SHADER_COMMON_AFUNC_GT0            GLSL_BIT( 19 )
#define GLSL_SHADER_COMMON_AFUNC_LT128          GLSL_BIT( 20 )
#define GLSL_SHADER_COMMON_AFUNC_GE128          ( GLSL_SHADER_COMMON_AFUNC_GT0 | GLSL_SHADER_COMMON_AFUNC_LT128 )

#define GLSL_SHADER_COMMON_FRAGMENT_HIGHP       GLSL_BIT( 21 )

#define GLSL_SHADER_COMMON_LINEAR2SRB           GLSL_BIT( 22 )

#define GLSL_SHADER_COMMON_VERTEX_LIGHTING		GLSL_BIT( 31 )

// material program type features
#define GLSL_SHADER_MATERIAL_LIGHTSTYLE0        GLSL_BIT( 32 )
#define GLSL_SHADER_MATERIAL_LIGHTSTYLE1        GLSL_BIT( 33 )
#define GLSL_SHADER_MATERIAL_LIGHTSTYLE2        ( GLSL_SHADER_MATERIAL_LIGHTSTYLE0 | GLSL_SHADER_MATERIAL_LIGHTSTYLE1 )
#define GLSL_SHADER_MATERIAL_LIGHTSTYLE3        GLSL_BIT( 34 )
#define GLSL_SHADER_MATERIAL_LIGHTSTYLE             ( ( GLSL_SHADER_MATERIAL_LIGHTSTYLE0 | GLSL_SHADER_MATERIAL_LIGHTSTYLE1 \
														| GLSL_SHADER_MATERIAL_LIGHTSTYLE2 | GLSL_SHADER_MATERIAL_LIGHTSTYLE3 ) )
#define GLSL_SHADER_MATERIAL_SPECULAR           GLSL_BIT( 35 )
#define GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT  GLSL_BIT( 36 )
#define GLSL_SHADER_MATERIAL_FB_LIGHTMAP        GLSL_BIT( 37 )
#define GLSL_SHADER_MATERIAL_AMBIENT_COMPENSATION GLSL_BIT( 38 )
#define GLSL_SHADER_MATERIAL_DECAL              GLSL_BIT( 39 )
#define GLSL_SHADER_MATERIAL_DECAL_ADD          GLSL_BIT( 40 )
#define GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY GLSL_BIT( 41 )
#define GLSL_SHADER_MATERIAL_HALFLAMBERT        GLSL_BIT( 42 )
#define GLSL_SHADER_MATERIAL_ENTITY_DECAL       GLSL_BIT( 43 )
#define GLSL_SHADER_MATERIAL_ENTITY_DECAL_ADD   GLSL_BIT( 44 )
#define GLSL_SHADER_MATERIAL_LIGHTMAP_ARRAYS    GLSL_BIT( 45 )
#define GLSL_SHADER_MATERIAL_LIGHTMAP_BICUBIC   GLSL_BIT( 46 )

// q3a shader features
#define GLSL_SHADER_Q3_TC_GEN_ENV               GLSL_BIT( 32 )
#define GLSL_SHADER_Q3_TC_GEN_VECTOR            GLSL_BIT( 33 )
#define GLSL_SHADER_Q3_TC_GEN_REFLECTION        ( GLSL_SHADER_Q3_TC_GEN_ENV | GLSL_SHADER_Q3_TC_GEN_VECTOR )
#define GLSL_SHADER_Q3_TC_GEN_PROJECTION        GLSL_BIT( 34 )
#define GLSL_SHADER_Q3_TC_GEN_SURROUND          GLSL_BIT( 35 )
#define GLSL_SHADER_Q3_LIGHTSTYLE0              GLSL_BIT( 36 )
#define GLSL_SHADER_Q3_LIGHTSTYLE1              GLSL_BIT( 37 )
#define GLSL_SHADER_Q3_LIGHTSTYLE2              ( GLSL_SHADER_Q3_LIGHTSTYLE0 | GLSL_SHADER_Q3_LIGHTSTYLE1 )
#define GLSL_SHADER_Q3_LIGHTSTYLE3              GLSL_BIT( 38 )
#define GLSL_SHADER_Q3_LIGHTSTYLE               ( ( GLSL_SHADER_Q3_LIGHTSTYLE0 | GLSL_SHADER_Q3_LIGHTSTYLE1 \
													| GLSL_SHADER_Q3_LIGHTSTYLE2 | GLSL_SHADER_Q3_LIGHTSTYLE3 ) )
#define GLSL_SHADER_Q3_LIGHTMAP_ARRAYS          GLSL_BIT( 39 )
#define GLSL_SHADER_Q3_ALPHA_MASK               GLSL_BIT( 40 )
#define GLSL_SHADER_Q3_LIGHTMAP_BICUBIC         GLSL_BIT( 41 )

// outlines
#define GLSL_SHADER_OUTLINE_OUTLINES_CUTOFF     GLSL_BIT( 32 )

// hdr/color-correction
#define GLSL_SHADER_COLOR_CORRECTION_LUT        GLSL_BIT( 32 )
#define GLSL_SHADER_COLOR_CORRECTION_HDR        GLSL_BIT( 33 )

void RP_Init( void );
void RP_Shutdown( void );
void RP_PrecachePrograms( void );
void RP_StorePrecacheList( void );

void RP_ProgramList_f( void );

int RP_RegisterProgram( int type, const char *name, const char *deformsKey,
						const deformv_t *deforms, int numDeforms, r_glslfeat_t features );
int RP_GetProgramObject( int elem );

void RP_UpdateShaderUniforms( int elem,
							  float shaderTime,
							  const vec3_t entOrigin, const vec3_t entDist, const uint8_t *entityColor,
							  const uint8_t *constColor, const float *rgbGenFuncArgs, const float *alphaGenFuncArgs,
							  const mat4_t texMatrix, float colorMod );

void RP_UpdateViewUniforms( int elem,
							const mat4_t modelviewMatrix, const mat4_t modelviewProjectionMatrix,
							const vec3_t viewOrigin, const mat3_t viewAxis,
							const float mirrorSide,
							int viewport[4],
							float zNear, float zFar );

void RP_UpdateBlendMixUniform( int elem, vec2_t blendMask );

void RP_UpdateSoftParticlesUniforms( int elem, float scale );

void RP_UpdateMaterialUniforms( int elem, float glossIntensity, float glossExponent );

void RP_UpdateTextureUniforms( int elem, int TexWidth, int TexHeight );

void RP_UpdateOutlineUniforms( int elem, float projDistance );

void RP_UpdateDiffuseLightUniforms( int elem,
									const vec3_t lightDir, const vec4_t lightAmbient, const vec4_t lightDiffuse );

void RP_UpdateLightstyleUniforms( int elem, const superLightStyle_t *superLightStyle );

void RP_UpdateTexGenUniforms( int elem, const mat4_t reflectionMatrix, const mat4_t vectorMatrix );

void RP_UpdateBonesUniforms( int elem, unsigned int numBones, dualquat_t *animDualQuat );

void RP_UpdateInstancesUniforms( int elem, unsigned int numInstances, instancePoint_t *instances );

void RP_UpdateDrawFlatUniforms( int elem, const vec3_t wallColor, const vec3_t floorColor );

void RP_UpdateColorCorrectionUniforms( int elem, float hdrGamme, float hdrExposure );

void RP_UpdateKawaseUniforms( int elem, int TexWidth, int TexHeight, int iteration );
