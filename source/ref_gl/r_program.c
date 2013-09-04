/*
Copyright (C) 2007 Victor Luchits

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

// r_program.c - OpenGL Shading Language support

#include "r_local.h"
#include "../qcommon/trie.h"

#define MAX_GLSL_PROGRAMS			1024
#define GLSL_PROGRAMS_HASH_SIZE		256

#define GLSL_CACHE_FILE_NAME		"glsl.cache"

typedef struct
{
	r_glslfeat_t	bit;
	const char		*define;
	const char		*suffix;
} glsl_feature_t;

typedef struct glsl_program_s
{
	char			*name;
	int				type;
	r_glslfeat_t	features;
	const char		*string;
	char 			*deformsKey;
	struct glsl_program_s *hash_next;

	int				object;
	int				vertexShader;
	int				fragmentShader;

	struct loc_s {
		int			ModelViewMatrix,
					ModelViewProjectionMatrix,

					ZNear, ZFar,

					ViewOrigin,
					ViewAxis,

					MirrorSide,

					Viewport,

					LightDir,
					LightAmbient,
					LightDiffuse,

					TextureMatrix,

					GlossIntensity,
					GlossExponent,

					OffsetMappingScale,
					OutlineHeight,
					OutlineCutOff,

					FrontPlane,
					TextureParams,

					EntityDist,
					EntityOrigin,
					EntityColor,
					ConstColor,
					RGBGenFuncArgs,
					AlphaGenFuncArgs;

					struct {
						int Plane,
							Color,
							Scale,
							EyePlane,
							EyeDist;
					} Fog;

		int			ShaderTime,

					ReflectionTexMatrix,
					VectorTexMatrix,

					DeluxemapOffset,
					LightstyleColor,

					DynamicLightsRadius[MAX_DLIGHTS],
					DynamicLightsPosition[MAX_DLIGHTS],
					DynamicLightsDiffuse[MAX_DLIGHTS],

					AttrBonesIndices,
					AttrBonesWeights,

					WallColor,
					FloorColor,

					ShadowProjDistance[GLSL_SHADOWMAP_LIMIT],
					ShadowmapTextureParams[GLSL_SHADOWMAP_LIMIT],
					ShadowmapMatrix[GLSL_SHADOWMAP_LIMIT],
					ShadowAlpha,
					
					BlendMix,
					
					SoftParticlesScale;

		// builtin uniforms
		struct {
			int		ShaderTime,
					ViewOrigin,
					ViewAxis,
					MirrorSide,
					EntityOrigin,
					DualQuats,
					InstancePoints;
		} builtin;
	} loc;
} glsl_program_t;

trie_t *glsl_cache_trie = NULL;

static unsigned int r_numglslprograms;
static glsl_program_t r_glslprograms[MAX_GLSL_PROGRAMS];
static glsl_program_t *r_glslprograms_hash[GLSL_PROGRAM_TYPE_MAXTYPE][GLSL_PROGRAMS_HASH_SIZE];

static void RP_GetUniformLocations( glsl_program_t *program );
static void RP_BindAttrbibutesLocations( glsl_program_t *program );

static void RP_PrecachePrograms( void );
static void RP_StorePrecacheList( void );

/*
* RP_Init
*/
void RP_Init( void )
{
	int program;

	memset( r_glslprograms, 0, sizeof( r_glslprograms ) );
	memset( r_glslprograms_hash, 0, sizeof( r_glslprograms_hash ) );

	Trie_Create( TRIE_CASE_INSENSITIVE, &glsl_cache_trie );

	// register base programs
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, DEFAULT_GLSL_MATERIAL_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_DISTORTION, DEFAULT_GLSL_DISTORTION_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_RGB_SHADOW, DEFAULT_GLSL_RGB_SHADOW_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_SHADOWMAP, DEFAULT_GLSL_SHADOWMAP_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_OUTLINE, DEFAULT_GLSL_OUTLINE_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_Q3A_SHADER, DEFAULT_GLSL_Q3A_SHADER_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_CELSHADE, DEFAULT_GLSL_CELSHADE_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_FOG, DEFAULT_GLSL_FOG_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_FXAA, DEFAULT_GLSL_FXAA_PROGRAM, NULL, NULL, 0, 0 );
	
	// check whether compilation of the shader with GPU skinning succeeds, if not, disable GPU bone transforms
	if( glConfig.maxGLSLBones ) {
		program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, DEFAULT_GLSL_MATERIAL_PROGRAM, NULL, NULL, 0, GLSL_SHADER_COMMON_BONE_TRANSFORMS1 );
		if( !program ) {
			glConfig.maxGLSLBones = 0;
		}
	}

	RP_PrecachePrograms();
}

/*
* RP_PrecachePrograms
*
* Loads the list of known program permutations from disk file.
*
* Expected file format:
* application_name\n
* version_number\n*
* program_type1 features_lower_bits1 features_higher_bits1 program_name1
* ..
* program_typeN features_lower_bitsN features_higher_bitsN program_nameN
*/
static void RP_PrecachePrograms( void )
{
#ifdef NDEBUG
	int length;
	int version;
	char *buffer = NULL, *data, **ptr;
	const char *token;
	const char *fileName;

	fileName = GLSL_CACHE_FILE_NAME;

	length = FS_LoadFile( fileName, ( void ** )&buffer, NULL, 0 );
	if( !buffer ) {
		return;
	}

	data = buffer;
	ptr = &data;

	token = COM_Parse( ptr );
	if( strcmp( token, APPLICATION ) ) {
		Com_DPrintf( "Ignoring %s: unknown application name \"%s\", expected \"%s\"\n", token, APPLICATION );
		return;
	}

	token = COM_Parse( ptr );
	version = atoi( token );
	if( version != GLSL_BITS_VERSION ) {
		// ignore cache files with mismatching version number
		Com_DPrintf( "Ignoring %s: found version %i, expcted %i\n", version, GLSL_BITS_VERSION );
	}
	else {
		while( 1 ) {
			int type;
			r_glslfeat_t lb, hb;
			r_glslfeat_t features;
			char name[256];

			// read program type
			token = COM_Parse( ptr );
			if( !token[0] ) {
				break;
			}
			type = atoi( token );

			// read lower bits
			token = COM_ParseExt( ptr, qfalse );
			if( !token[0] ) {
				break;
			}
			lb = atoi( token );

			// read higher bits
			token = COM_ParseExt( ptr, qfalse );
			if( !token[0] ) {
				break;
			}
			hb = atoi( token );

			// read program full name
			token = COM_ParseExt( ptr, qfalse );
			if( !token[0] ) {
				break;
			}

			Q_strncpyz( name, token, sizeof( name ) );
			features = (hb << 32) | lb; 

			Com_DPrintf( "Loading program %s...\n", name );

			RP_RegisterProgram( type, name, NULL, NULL, 0, features );
		}
	}

	FS_FreeFile( buffer );
#endif
}


/*
* RP_StorePrecacheList
*
* Stores the list of known GLSL program permutations to file on the disk.
* File format matches that expected by RP_PrecachePrograms.
*/
static void RP_StorePrecacheList( void )
{
#ifdef NDEBUG
	unsigned int i;
	int handle;
	const char *fileName;
	glsl_program_t *program;

	fileName = GLSL_CACHE_FILE_NAME;
	if( FS_FOpenFile( fileName, &handle, FS_WRITE ) == -1 ) {
		Com_Printf( S_COLOR_YELLOW "Could not open %s for writing.\n", fileName );
		return;
	}

	FS_Printf( handle, "%s\n", APPLICATION );
	FS_Printf( handle, "%i\n", GLSL_BITS_VERSION );

	for( i = 0, program = r_glslprograms; i < r_numglslprograms; i++, program++ ) {
		if( !program->features ) {
			continue;
		}
		if( *program->deformsKey ) {
			continue;
		}

		FS_Printf( handle, "%i %i %i %s\n", 
			program->type, 
			(int)(program->features & ULONG_MAX), 
			(int)((program->features>>32) & ULONG_MAX), 
			program->name );
	}

	FS_FCloseFile( handle );
#endif
}

/*
* RP_CopyString
*/
static char *RP_CopyString( const char *in )
{
	char *out;

	out = R_Malloc( strlen( in ) + 1 );
	strcpy( out, in );

	return out;
}

/*
* RP_DeleteProgram
*/
static void RP_DeleteProgram( glsl_program_t *program )
{
	glsl_program_t *hash_next;

	if( program->vertexShader )
	{
		qglDetachObjectARB( program->object, program->vertexShader );
		qglDeleteObjectARB( program->vertexShader );
		program->vertexShader = 0;
	}

	if( program->fragmentShader )
	{
		qglDetachObjectARB( program->object, program->fragmentShader );
		qglDeleteObjectARB( program->fragmentShader );
		program->fragmentShader = 0;
	}

	if( program->object )
		qglDeleteObjectARB( program->object );

	if( program->name )
		Mem_Free( program->name );
	if( program->deformsKey )
		Mem_Free( program->deformsKey );

	hash_next = program->hash_next;
	memset( program, 0, sizeof( glsl_program_t ) );
	program->hash_next = hash_next;
}

/*
* RP_CompileShader
*/
static int RP_CompileShader( int program, const char *programName, const char *shaderName, 
	int shaderType, const char **strings, int numStrings )
{
	GLhandleARB shader;
	GLint compiled;

	shader = qglCreateShaderObjectARB( (GLenum)shaderType );
	if( !shader )
		return 0;

	// if lengths is NULL, then each string is assumed to be null-terminated
	qglShaderSourceARB( shader, numStrings, strings, NULL );
	qglCompileShaderARB( shader );
	qglGetObjectParameterivARB( shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled );

	if( !compiled )
	{
		char log[4096];

		qglGetInfoLogARB( shader, sizeof( log ) - 1, NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] ) {
			int i;
			for( i = 0; i < numStrings; i++ )
				Com_Printf( "%s\n", strings[i] );
			Com_Printf( S_COLOR_YELLOW "Failed to compile %s shader for program %s:\n%s\n", 
				shaderName, programName, log );
		}

		qglDeleteObjectARB( shader );
		return 0;
	}

	qglAttachObjectARB( program, shader );

	return shader;
}

// ======================================================================================

#define MAX_DEFINES_FEATURES	255

static const glsl_feature_t glsl_features_generic[] =
{
	{ GLSL_SHADER_COMMON_GRAYSCALE, "#define APPLY_GRAYSCALE\n", "_grey" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_material[] =
{
	{ GLSL_SHADER_COMMON_GRAYSCALE, "#define APPLY_GRAYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_CONST, "#define APPLY_RGB_CONST\n", "_cc" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },
	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },

	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_CONST, "#define APPLY_ALPHA_CONST\n", "_ac" },
	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_DLIGHTS_32, "#define NUM_DLIGHTS 32\n", "_dl32" },
	{ GLSL_SHADER_COMMON_DLIGHTS_16, "#define NUM_DLIGHTS 16\n", "_dl16" },
	{ GLSL_SHADER_COMMON_DLIGHTS_8, "#define NUM_DLIGHTS 8\n", "_dl8" },
	{ GLSL_SHADER_COMMON_DLIGHTS_4, "#define NUM_DLIGHTS 4\n", "_dl4" },

	{ GLSL_SHADER_COMMON_DRAWFLAT, "#define APPLY_DRAWFLAT\n", "_flat" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },
	{ GLSL_SHADER_COMMON_AUTOPARTICLE, "#define APPLY_AUTOSPRITE\n#define APPLY_AUTOPARTICLE\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n"
		"#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE3, "#define NUM_LIGHTMAPS 4\n", "_ls3" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE2, "#define NUM_LIGHTMAPS 3\n", "_ls2" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE1, "#define NUM_LIGHTMAPS 2\n", "_ls1" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE0, "#define NUM_LIGHTMAPS 1\n", "_ls0" },
	{ GLSL_SHADER_MATERIAL_FB_LIGHTMAP, "#define APPLY_FBLIGHTMAP\n", "_fb" },
	{ GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT, "#define APPLY_DIRECTIONAL_LIGHT\n", "_dirlight" },

	{ GLSL_SHADER_MATERIAL_SPECULAR, "#define APPLY_SPECULAR\n", "_gloss" },
	{ GLSL_SHADER_MATERIAL_OFFSETMAPPING, "#define APPLY_OFFSETMAPPING\n", "_offmap" },
	{ GLSL_SHADER_MATERIAL_RELIEFMAPPING, "#define APPLY_RELIEFMAPPING\n", "_relmap" },
	{ GLSL_SHADER_MATERIAL_AMBIENT_COMPENSATION, "#define APPLY_AMBIENT_COMPENSATION\n", "_amb" },
	{ GLSL_SHADER_MATERIAL_DECAL, "#define APPLY_DECAL\n", "_decal" },
	{ GLSL_SHADER_MATERIAL_DECAL_ADD, "#define APPLY_DECAL_ADD\n", "_add" },
	{ GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY, "#define APPLY_BASETEX_ALPHA_ONLY\n", "_alpha" },
	{ GLSL_SHADER_MATERIAL_CELSHADING, "#define APPLY_CELSHADING\n", "_cel" },
	{ GLSL_SHADER_MATERIAL_HALFLAMBERT, "#define APPLY_HALFLAMBERT\n", "_lambert" },

	{ GLSL_SHADER_MATERIAL_ENTITY_DECAL, "#define APPLY_ENTITY_DECAL\n", "_decal2" },
	{ GLSL_SHADER_MATERIAL_ENTITY_DECAL_ADD, "#define APPLY_ENTITY_DECAL_ADD\n", "_decal2_add" },

	// doesn't make sense without APPLY_DIRECTIONAL_LIGHT
	{ GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT_MIX, "#define APPLY_DIRECTIONAL_LIGHT_MIX\n", "_mix" },
	{ GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT_FROM_NORMAL, "#define APPLY_DIRECTIONAL_LIGHT_FROM_NORMAL\n", "_normlight" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_distortion[] =
{
	{ GLSL_SHADER_COMMON_GRAYSCALE, "#define APPLY_GRAYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_CONST, "#define APPLY_RGB_CONST\n", "_cc" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },
	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },

	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_CONST, "#define APPLY_ALPHA_CONST\n", "_ac" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },
	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },
	{ GLSL_SHADER_COMMON_AUTOPARTICLE, "#define APPLY_AUTOSPRITE\n#define APPLY_AUTOPARTICLE\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n"
			"#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_DISTORTION_DUDV, "#define APPLY_DUDV\n", "_dudv" },
	{ GLSL_SHADER_DISTORTION_EYEDOT, "#define APPLY_EYEDOT\n", "_eyedot" },
	{ GLSL_SHADER_DISTORTION_DISTORTION_ALPHA, "#define APPLY_DISTORTION_ALPHA\n", "_alpha" },
	{ GLSL_SHADER_DISTORTION_REFLECTION, "#define APPLY_REFLECTION\n", "_refl" },
	{ GLSL_SHADER_DISTORTION_REFRACTION, "#define APPLY_REFRACTION\n", "_refr" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_rgbshadow[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_shadowmap[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_SHADOWMAP_DITHER, "#define APPLY_DITHER\n", "_dither" },
	{ GLSL_SHADER_SHADOWMAP_PCF, "#define APPLY_PCF\n", "_pcf" },
	{ GLSL_SHADER_SHADOWMAP_SHADOW2, "#define MAX_SHADOWS 2\n", "_2" },
	{ GLSL_SHADER_SHADOWMAP_SHADOW3, "#define MAX_SHADOWS 3\n", "_3" },
	{ GLSL_SHADER_SHADOWMAP_SHADOW4, "#define MAX_SHADOWS 4\n", "_4" },
	{ GLSL_SHADER_SHADOWMAP_RGB_SHADOW, "#define APPLY_RGB_SHADOW\n", "_rgb" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_outline[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_RGB_GEN_CONST, "#define APPLY_RGB_CONST\n", "_cc" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_CONST, "#define APPLY_ALPHA_CONST\n", "_ac" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_OUTLINE_OUTLINES_CUTOFF, "#define APPLY_OUTLINES_CUTOFF\n", "_outcut" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_dynamiclights[] =
{
	{ GLSL_SHADER_COMMON_DLIGHTS_32, "#undef NUM_DLIGHTS\n#define NUM_DLIGHTS 32\n", "_dl32" },
	{ GLSL_SHADER_COMMON_DLIGHTS_16, "#undef NUM_DLIGHTS\n#define NUM_DLIGHTS 16\n", "_dl16" },
	{ GLSL_SHADER_COMMON_DLIGHTS_8, "#undef NUM_DLIGHTS\n#define NUM_DLIGHTS 8\n", "_dl8" },
	{ GLSL_SHADER_COMMON_DLIGHTS_4, "#undef NUM_DLIGHTS\n#define NUM_DLIGHTS 4\n", "_dl4" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_q3a[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_CONST, "#define APPLY_RGB_CONST\n", "_cc" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },
	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },

	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_CONST, "#define APPLY_ALPHA_CONST\n", "_ac" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },
	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_DLIGHTS_32, "#define NUM_DLIGHTS 32\n", "_dl32" },
	{ GLSL_SHADER_COMMON_DLIGHTS_16, "#define NUM_DLIGHTS 16\n", "_dl16" },
	{ GLSL_SHADER_COMMON_DLIGHTS_8, "#define NUM_DLIGHTS 8\n", "_dl8" },
	{ GLSL_SHADER_COMMON_DLIGHTS_4, "#define NUM_DLIGHTS 4\n", "_dl4" },

	{ GLSL_SHADER_COMMON_DRAWFLAT, "#define APPLY_DRAWFLAT\n", "_flat" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },
	{ GLSL_SHADER_COMMON_AUTOPARTICLE, "#define APPLY_AUTOSPRITE\n#define APPLY_AUTOPARTICLE\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_SOFT_PARTICLE, "#define APPLY_SOFT_PARTICLE\n", "_sp" },

	{ GLSL_SHADER_Q3_TC_GEN_REFLECTION, "#define APPLY_TC_GEN_REFLECTION\n", "_tc_refl" },
	{ GLSL_SHADER_Q3_TC_GEN_PROJECTION, "#define APPLY_TC_GEN_PROJECTION\n", "_tc_proj" },
	{ GLSL_SHADER_Q3_TC_GEN_ENV, "#define APPLY_TC_GEN_ENV\n", "_tc_env" },
	{ GLSL_SHADER_Q3_TC_GEN_VECTOR, "#define APPLY_TC_GEN_VECTOR\n", "_tc_vec" },

	{ GLSL_SHADER_Q3_LIGHTSTYLE3, "#define NUM_LIGHTMAPS 4\n", "_ls3" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE2, "#define NUM_LIGHTMAPS 3\n", "_ls2" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE1, "#define NUM_LIGHTMAPS 2\n", "_ls1" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE0, "#define NUM_LIGHTMAPS 1\n", "_ls0" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_celshade[] =
{
	{ GLSL_SHADER_COMMON_GRAYSCALE, "#define APPLY_GRAYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },
	{ GLSL_SHADER_COMMON_AUTOPARTICLE, "#define APPLY_AUTOSPRITE\n#define APPLY_AUTOPARTICLE\n", "" },

	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_CONST, "#define APPLY_RGB_CONST\n", "_cc" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },
	{ GLSL_SHADER_COMMON_RGB_GEN_DIFFUSELIGHT, "#define APPLY_RGB_GEN_DIFFUSELIGHT\n", "_cl" },

	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_CONST, "#define APPLY_ALPHA_CONST\n", "_ac" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_CELSHADE_DIFFUSE, "#define APPLY_DIFFUSE\n", "_diff" },
	{ GLSL_SHADER_CELSHADE_DECAL, "#define APPLY_DECAL\n", "_decal" },
	{ GLSL_SHADER_CELSHADE_DECAL_ADD, "#define APPLY_DECAL_ADD\n", "_decal" },
	{ GLSL_SHADER_CELSHADE_ENTITY_DECAL, "#define APPLY_ENTITY_DECAL\n", "_edecal" },
	{ GLSL_SHADER_CELSHADE_ENTITY_DECAL_ADD, "#define APPLY_ENTITY_DECAL\n#define APPLY_ENTITY_DECAL_ADD\n", "_add" },
	{ GLSL_SHADER_CELSHADE_STRIPES, "#define APPLY_STRIPES\n", "_stripes" },
	{ GLSL_SHADER_CELSHADE_STRIPES_ADD, "#define APPLY_STRIPES_ADD\n", "_stripes_add" },
	{ GLSL_SHADER_CELSHADE_CEL_LIGHT, "#define APPLY_CEL_LIGHT\n", "_light" },
	{ GLSL_SHADER_CELSHADE_CEL_LIGHT_ADD, "#define APPLY_CEL_LIGHT\n#define APPLY_CEL_LIGHT_ADD\n", "_add" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_fog[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },
	{ GLSL_SHADER_COMMON_AUTOPARTICLE, "#define APPLY_AUTOSPRITE\n#define APPLY_AUTOPARTICLE\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRASNFORMS\n", "_instanced_va" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_fxaa[] =
{
	{ 0, NULL, NULL }
};

static const glsl_feature_t * const glsl_programtypes_features[] =
{
	// GLSL_PROGRAM_TYPE_NONE
	NULL,
	// GLSL_PROGRAM_TYPE_MATERIAL
	glsl_features_material,
	// GLSL_PROGRAM_TYPE_DISTORTION
	glsl_features_distortion,
	// GLSL_PROGRAM_TYPE_RGB_SHADOW
	glsl_features_rgbshadow,
	// GLSL_PROGRAM_TYPE_SHADOWMAP
	glsl_features_shadowmap,
	// GLSL_PROGRAM_TYPE_OUTLINE
	glsl_features_outline,
	// GLSL_PROGRAM_TYPE_DYNAMIC_LIGHTS
	glsl_features_dynamiclights,
	// GLSL_PROGRAM_TYPE_Q3A_SHADER
	glsl_features_q3a,
	// GLSL_PROGRAM_TYPE_CELSHADE
	glsl_features_celshade,
	// GLSL_PROGRAM_TYPE_FOG
	glsl_features_fog,
	// GLSL_PROGRAM_TYPE_FXAA
	glsl_features_fxaa,
};

// ======================================================================================

#ifndef STR_HELPER
#define STR_HELPER( s )					# s
#define STR_TOSTR( x )					STR_HELPER( x )
#endif

#define QF_GLSL_VERSION120 "" \
"#version 120\n" \
"#define QF_GLSLVERSION 120\n"

#define QF_GLSL_VERSION130 "" \
"#version 130\n" \
"#define QF_GLSLVERSION 130\n"

#define QF_GLSL_VERSION140 "" \
"#version 140\n" \
"#define QF_GLSLVERSION 140\n"

#define QF_BUILTIN_GLSL_MACROS "" \
"#if !defined(myhalf)\n" \
"//#if !defined(__GLSL_CG_DATA_TYPES)\n" \
"#define myhalf float\n" \
"#define myhalf2 vec2\n" \
"#define myhalf3 vec3\n" \
"#define myhalf4 vec4\n" \
"//#else\n" \
"//#define myhalf half\n" \
"//#define myhalf2 half2\n" \
"//#define myhalf3 half3\n" \
"//#define myhalf4 half4\n" \
"//#endif\n" \
"#endif\n" \
"\n" \
"#if QF_GLSLVERSION >= 130\n" \
"  precision highp float;\n" \
"# ifdef VERTEX_SHADER\n" \
"   out myhalf4 qf_FrontColor;\n" \
"#  define varying out\n" \
"#  define attribute in\n" \
"#  define gl_FrontColor qf_FrontColor\n" \
"# endif\n" \
"\n" \
"# ifdef FRAGMENT_SHADER\n" \
"   in myhalf4 qf_FrontColor;\n" \
"   out vec4 qf_FragColor;\n" \
"   out myhalf4	qf_FragColor;\n" \
"#  define gl_Color qf_FrontColor\n" \
"#  define gl_FragColor qf_FragColor\n" \
"#  define varying in\n" \
"#  define attribute in\n" \
"#  define texture2D texture\n" \
"#  define textureCube texture\n" \
"#  define shadow2D texture\n" \
"# endif\n" \
"#endif\n" \
"\n"

#define QF_GLSL_PI "" \
"#ifndef M_PI\n" \
"#define M_PI 3.14159265358979323846\n" \
"#endif\n" \
"#ifndef M_TWOPI\n" \
"#define M_TWOPI 6.28318530717958647692\n" \
"#endif\n"

#define QF_BUILTIN_GLSL_CONSTANTS \
QF_GLSL_PI \
"\n" \
"#ifndef MAX_UNIFORM_BONES\n" \
"#define MAX_UNIFORM_BONES " STR_TOSTR( MAX_GLSL_UNIFORM_BONES ) "\n" \
"#endif\n" \
"\n" \
"#ifndef MAX_UNIFORM_INSTANCES\n" \
"#define MAX_UNIFORM_INSTANCES " STR_TOSTR( MAX_GLSL_UNIFORM_INSTANCES ) "\n" \
"#endif\n" \
"\n" \
"#ifndef FOG_TEXCOORD_STEP\n" \
"#define FOG_TEXCOORD_STEP 1.0/256.0\n" \
"#endif\n" \
"\n" \
"#ifndef DRAWFLAT_NORMAL_STEP\n" \
"#define DRAWFLAT_NORMAL_STEP " STR_TOSTR( DRAWFLAT_NORMAL_STEP ) "\n" \
"#endif\n"

#define QF_BUILTIN_GLSL_UNIFORMS \
"uniform vec3 u_QF_ViewOrigin;\n" \
"uniform mat3 u_QF_ViewAxis;\n" \
"uniform float u_QF_MirrorSide;\n" \
"uniform vec3 u_QF_EntityOrigin;\n" \
"uniform float u_QF_ShaderTime;\n"

#define QF_GLSL_WAVEFUNCS \
"\n" \
QF_GLSL_PI \
"\n" \
"#ifndef WAVE_SIN\n" \
"float QF_WaveFunc_Sin(float x)\n" \
"{\n" \
"x -= floor(x);\n" \
"return sin(x * M_TWOPI);\n" \
"}\n" \
"float QF_WaveFunc_Triangle(float x)\n" \
"{\n" \
"x -= floor(x);\n" \
"return step(x, 0.25) * x * 4.0 + (2.0 - 4.0 * step(0.25, x) * step(x, 0.75) * x) + ((step(0.75, x) * x - 0.75) * 4.0 - 1.0);\n" \
"}\n" \
"float QF_WaveFunc_Square(float x)\n" \
"{\n" \
"x -= floor(x);\n" \
"return step(x, 0.5) * 2.0 - 1.0;\n" \
"}\n" \
"float QF_WaveFunc_Sawtooth(float x)\n" \
"{\n" \
"x -= floor(x);\n" \
"return x;\n" \
"}\n" \
"float QF_QF_WaveFunc_InverseSawtooth(float x)\n" \
"{\n" \
"x -= floor(x);\n" \
"return 1.0 - x;\n" \
"}\n" \
"\n" \
"#define WAVE_SIN(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sin((phase)+(time)*(freq))))\n" \
"#define WAVE_TRIANGLE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Triangle((phase)+(time)*(freq))))\n" \
"#define WAVE_SQUARE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Square((phase)+(time)*(freq))))\n" \
"#define WAVE_SAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sawtooth((phase)+(time)*(freq))))\n" \
"#define WAVE_INVERSESAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_QF_WaveFunc_InverseSawtooth((phase)+(time)*(freq))))\n" \
"#endif\n" \
"\n"

#define QF_DUAL_QUAT_TRANSFORM_OVERLOAD "" \
"#if defined(DUAL_QUAT_TRANSFORM_NORMALS)\n" \
"#if defined(DUAL_QUAT_TRANSFORM_TANGENT)\n" \
"void QF_VertexDualQuatsTransform(const int numWeights, inout vec4 Position, inout vec3 Normal, inout vec3 Tangent)\n" \
"#else\n" \
"void QF_VertexDualQuatsTransform(const int numWeights, inout vec4 Position, inout vec3 Normal)\n" \
"#endif\n" \
"#else\n" \
"void QF_VertexDualQuatsTransform(const int numWeights, inout vec4 Position)\n" \
"#endif\n" \
"{\n" \
"int index;\n" \
"vec4 Indices = a_BonesIndices;\n" \
"vec4 Weights = a_BonesWeights;\n" \
"vec4 Indices_2 = Indices * 2.0;\n" \
"vec4 DQReal, DQDual;\n" \
"\n" \
"index = int(Indices_2.x);\n" \
"DQReal = u_QF_DualQuats[index+0];\n" \
"DQDual = u_QF_DualQuats[index+1];\n" \
"\n" \
"if (numWeights > 1)\n" \
"{\n" \
"DQReal *= Weights.x;\n" \
"DQDual *= Weights.x;\n" \
"\n" \
"vec4 DQReal1, DQDual1;\n" \
"float scale;\n" \
"\n" \
"index = int(Indices_2.y);\n" \
"DQReal1 = u_QF_DualQuats[index+0];\n" \
"DQDual1 = u_QF_DualQuats[index+1];\n" \
"// antipodality handling\n" \
"scale = (dot(DQReal1, DQReal) < 0.0 ? -1.0 : 1.0) * Weights.y;\n" \
"DQReal += DQReal1 * scale;\n" \
"DQDual += DQDual1 * scale;\n" \
"\n" \
"if (numWeights > 2)\n" \
"{\n" \
"index = int(Indices_2.z);\n" \
"DQReal1 = u_QF_DualQuats[index+0];\n" \
"DQDual1 = u_QF_DualQuats[index+1];\n" \
"// antipodality handling\n" \
"scale = (dot(DQReal1, DQReal) < 0.0 ? -1.0 : 1.0) * Weights.z;\n" \
"DQReal += DQReal1 * scale;\n" \
"DQDual += DQDual1 * scale;\n" \
"\n" \
"if (numWeights > 3)\n" \
"{\n" \
"index = int(Indices_2.w);\n" \
"DQReal1 = u_QF_DualQuats[index+0];\n" \
"DQDual1 = u_QF_DualQuats[index+1];\n" \
"// antipodality handling\n" \
"scale = (dot(DQReal1, DQReal) < 0.0 ? -1.0 : 1.0) * Weights.w;\n" \
"DQReal += DQReal1 * scale;\n" \
"DQDual += DQDual1 * scale;\n" \
"}\n" \
"}\n" \
"}\n" \
"\n" \
"float len = length(DQReal);\n" \
"DQReal /= len;\n" \
"DQDual /= len;\n" \
"\n" \
"Position.xyz = (cross(DQReal.xyz, cross(DQReal.xyz, Position.xyz) + Position.xyz*DQReal.w + DQDual.xyz) + " \
	"DQDual.xyz*DQReal.w - DQReal.xyz*DQDual.w)*2.0 + Position.xyz;\n" \
"\n" \
"#ifdef DUAL_QUAT_TRANSFORM_NORMALS\n" \
"Normal = cross(DQReal.xyz, cross(DQReal.xyz, Normal) + Normal*DQReal.w)*2.0 + Normal;\n" \
"#endif\n" \
"\n" \
"#ifdef DUAL_QUAT_TRANSFORM_TANGENT\n" \
"Tangent = cross(DQReal.xyz, cross(DQReal.xyz, Tangent) + Tangent*DQReal.w)*2.0 + Tangent;\n" \
"#endif\n" \
"}\n"

// FIXME: version 140 and up?
#define QF_GLSL_DUAL_QUAT_TRANSFORMS \
"#ifdef VERTEX_SHADER\n" \
"attribute vec4 a_BonesIndices;\n" \
"attribute vec4 a_BonesWeights;\n" \
"\n" \
"uniform vec4 u_QF_DualQuats[MAX_UNIFORM_BONES*2];\n" \
"\n" \
QF_DUAL_QUAT_TRANSFORM_OVERLOAD \
"\n" \
"// use defines to overload the transform function\n" \
"\n" \
"#define DUAL_QUAT_TRANSFORM_NORMALS\n" \
QF_DUAL_QUAT_TRANSFORM_OVERLOAD \
"\n" \
"#define DUAL_QUAT_TRANSFORM_TANGENT\n" \
QF_DUAL_QUAT_TRANSFORM_OVERLOAD \
"\n" \
"#endif\n"

#define QF_GLSL_INSTANCED_TRASFORMS \
"#ifdef VERTEX_SHADER\n" \
"#ifdef APPLY_INSTANCED_ATTRIB_TRASNFORMS\n" \
"attribute vec4 a_InstanceQuat;\n" \
"attribute vec4 a_InstancePosAndScale;\n" \
"#elif defined(GL_ARB_draw_instanced)\n" \
"\n" \
"uniform vec4 u_QF_InstancePoints[MAX_UNIFORM_INSTANCES*2];\n" \
"\n" \
"#define a_InstanceQuat u_QF_InstancePoints[gl_InstanceID*2]\n" \
"#define a_InstancePosAndScale u_QF_InstancePoints[gl_InstanceID*2+1]\n" \
"#else\n" \
"uniform vec4 u_QF_InstancePoints[2];\n" \
"#define a_InstanceQuat u_QF_InstancePoints[0]\n" \
"#define a_InstancePosAndScale u_QF_InstancePoints[1]\n" \
"#endif\n" \
"\n" \
"void QF_InstancedTransform(inout vec4 Position, inout vec3 Normal)\n" \
"{\n" \
"Position.xyz = (cross(a_InstanceQuat.xyz, cross(a_InstanceQuat.xyz, Position.xyz) + " \
	"Position.xyz*a_InstanceQuat.w)*2.0 + Position.xyz) *\n" \
" a_InstancePosAndScale.w + a_InstancePosAndScale.xyz;\n" \
"Normal = cross(a_InstanceQuat.xyz, cross(a_InstanceQuat.xyz, Normal) + Normal*a_InstanceQuat.w)*2.0 + Normal;\n" \
"}\n" \
"\n" \
"#endif\n"

/*
* R_GLSLBuildDeformv
* 
* Converts some of the Q3A vertex deforms to a GLSL vertex shader. 
* Supported deforms are: wave, move, bulge.
* NOTE: Autosprite deforms can only be performed in a geometry shader.
* NULL is returned in case an unsupported deform is passed.
*/
static const char *R_GLSLBuildDeformv( const deformv_t *deformv, int numDeforms )
{
	int i;
	int funcType;
	static char program[40*1024];
	static const char * const funcs[] = {
		NULL, "WAVE_SIN", "WAVE_TRIANGLE", "WAVE_SQUARE", "WAVE_SAWTOOTH", "WAVE_INVERSESAWTOOTH", NULL
	};
	static const int numSupportedFuncs = sizeof( funcs ) / sizeof( funcs[0] ) - 1;

	if( !numDeforms ) {
		return NULL;
	}

	program[0] = '\0';
	Q_strncpyz( program, 
		"#define APPLY_DEFORMVERTS\n"
		"\n"
		"#if defined(APPLY_AUTOSPRITE) || defined(APPLY_AUTOSPRITE2)\n"
		"attribute vec4 a_SpritePoint;\n"
		"#else\n"
		"#define a_SpritePoint vec4(0.0)\n"
		"#endif\n"
		"\n"
		"#if defined(APPLY_AUTOSPRITE2)\n"
		"attribute vec3 a_SpriteRightAxis;\n"
		"attribute vec3 a_SpriteUpAxis;\n"
		"#else\n"
		"#define a_SpriteRightAxis vec3(0.0)\n"
		"#define a_SpriteUpAxis vec3(0.0)\n"
		"#endif\n"
		"\n"
		"void QF_DeformVerts(inout vec4 Position, inout vec3 Normal, in vec2 TexCoord)\n"
		"{\n"
		"float t = 0.0;\n"
		"vec3 dist;\n"
		"vec3 right, up, forward, newright;\n"
		"\n"
		"#if defined(WAVE_SIN)\n"
		, sizeof( program ) );

	for( i = 0; i < numDeforms; i++, deformv++ ) {
		switch( deformv->type ) {
			case DEFORMV_WAVE:
				funcType = deformv->func.type;
				if( funcType <= SHADER_FUNC_NONE || funcType > numSupportedFuncs || !funcs[funcType] ) {
					return NULL;
				}

				Q_strncatz( program, va( "Position.xyz += %s(u_QF_ShaderTime,%f,%f,%f+%f*(Position.x+Position.y+Position.z),%f) * Normal.xyz;\n", 
					funcs[funcType], deformv->func.args[0], deformv->func.args[1], deformv->func.args[2], deformv->func.args[3] ? deformv->args[0] : 0.0, deformv->func.args[3] ), 
					sizeof( program ) );
				break;
			case DEFORMV_MOVE:
				funcType = deformv->func.type;
				if( funcType <= SHADER_FUNC_NONE || funcType > numSupportedFuncs || !funcs[funcType] ) {
					return NULL;
				}

				Q_strncatz( program, va( "Position.xyz += %s(u_QF_ShaderTime,%f,%f,%f,%f) * vec3(%f, %f, %f);\n", 
					funcs[funcType], deformv->func.args[0], deformv->func.args[1], deformv->func.args[2], deformv->func.args[3],
						deformv->args[0], deformv->args[1], deformv->args[2] ), 
					sizeof( program ) );
				break;
			case DEFORMV_BULGE:
				Q_strncatz( program, va( 
						"t = sin(TexCoord.s * %f + u_QF_ShaderTime * %f);\n"
						"Position.xyz += max (-1.0 + %f, t) * %f * Normal.xyz;\n", 
						deformv->args[0], deformv->args[2], deformv->args[3], deformv->args[1] ), 
					sizeof( program ) );
				break;
			case DEFORMV_AUTOSPRITE:
				Q_strncatz( program,
						"right = (1.0 - TexCoord.s * 2.0) * u_QF_ViewAxis[1] * u_QF_MirrorSide;\n;"
						"up = (1.0 - TexCoord.t * 2.0) * u_QF_ViewAxis[2];\n"
						"forward = -1 * u_QF_ViewAxis[0];\n"
						"Position.xyz = a_SpritePoint.xyz + (right + up) * a_SpritePoint.w;\n"
						"Normal.xyz = forward;\n",
					sizeof( program ) );
				break;
			case DEFORMV_AUTOPARTICLE:
				Q_strncatz( program,
						"right = (1.0 - TexCoord.s * 2.0) * u_QF_ViewAxis[1] * u_QF_MirrorSide;\n;"
						"up = (1.0 - TexCoord.t * 2.0) * u_QF_ViewAxis[2];\n"
						"forward = -1 * u_QF_ViewAxis[0];\n"
						"// prevent the particle from disappearing at large distances\n"
						"t = (a_SpritePoint.xyz + u_QF_EntityOrigin - u_QF_ViewOrigin) * u_QF_ViewAxis[0];\n"
						"t = 1.5 + step(20.0, t) * t * 0.006;\n"
						"Position.xyz = a_SpritePoint.xyz + (right + up) * t * a_SpritePoint.w;\n"
						"Normal.xyz = forward;\n",
					sizeof( program ) );
				break;
			case DEFORMV_AUTOSPRITE2:
				Q_strncatz( program,
					"// local sprite axes\n"
					"right = a_SpriteRightAxis * u_QF_MirrorSide;\n"
					"up = a_SpriteUpAxis;\n"
					"\n"
					"// mid of quad to camera vector\n"
					"dist = u_QF_ViewOrigin - u_QF_EntityOrigin - a_SpritePoint.xyz;\n"
					"\n"
					"// filter any longest-axis-parts off the camera-direction\n"
					"forward = normalize(dist - up * dot(dist, up));\n"
					"\n"
					"// the right axis vector as it should be to face the camera\n"
					"newright = cross(up, forward);\n"
					"\n"
					"// rotate the quad vertex around the up axis vector\n"
					"t = dot(right, Position.xyz - a_SpritePoint.xyz);\n"
					"Position.xyz += t * (newright - right);\n"
					"Normal.xyz = forward;\n", 
					sizeof( program ) );
				break;
			default:
				return NULL;
		}
	}

	Q_strncatz( program, 
		"#endif\n"
		"}\n"
		"\n"
		, sizeof( program ) );

	return program;
}

//=======================================================================

#define PARSER_MAX_STACKDEPTH	16

typedef struct
{
	const char *topFile;
	qboolean error;

	const char **strings;
	size_t maxStrings;
	size_t numStrings;

	char **buffers;
	size_t maxBuffers;
	size_t numBuffers;
} glslParser_t;

/*
* RP_LoadShaderFromFile_r
*/
static qboolean RP_LoadShaderFromFile_r( glslParser_t *parser, const char *fileName,
	int stackDepth )
{
	char *fileContents;
	char *token, *line;
	char *ptr, *prevPtr;
	char *startBuf;
	char *trieCache;
	trie_error_t trie_error;

	trie_error = Trie_Find( glsl_cache_trie, fileName, TRIE_EXACT_MATCH, &trieCache );
	if( trie_error != TRIE_OK ) {
		FS_LoadFile( fileName, &fileContents, NULL, 0 );

		if( fileContents ) {
			trieCache = RP_CopyString( fileContents );
		}
		else {
			trieCache = NULL;
		}
		Trie_Insert( glsl_cache_trie, fileName, trieCache );
	}
	else {
		if( trieCache ) {
			fileContents = RP_CopyString( trieCache );
		}
		else {
			fileContents = NULL;
		}
	}

	if( !fileContents ) {
		Com_Printf( S_COLOR_YELLOW "Cannot load file '%s'\n", fileName );
		return qtrue;
	}

	if( parser->numBuffers == parser->maxBuffers ) {
		Com_Printf( S_COLOR_YELLOW "numBuffers overflow in '%s' around '%s'\n", parser->topFile, fileName );
		return qtrue;
	}
	parser->buffers[parser->numBuffers++] = fileContents;

	ptr = fileContents;
	startBuf = NULL;

	while( 1 ) {
		prevPtr = ptr;
		token = COM_ParseExt( &ptr, qtrue );
		if( !token[0] ) {
			break;
		}

		line = token;
		if( Q_stricmp( token, "#include" ) ) {
			if( !startBuf ) {
				startBuf = prevPtr;
			}
			// skip to the end of the line
			token = strchr( ptr, '\n' );
			if( !token ) {
				break;
			}
			ptr = token+1;
			continue;
		}

		if( startBuf && prevPtr > startBuf ) {
			// cut the string at the beginning of the #include
			*prevPtr = '\0';

			if( parser->numStrings == parser->maxStrings ) {
				Com_Printf( S_COLOR_YELLOW "numStrings overflow in '%s' around '%s'\n", fileName, line );
				return qtrue;
			}
			parser->strings[parser->numStrings++] = startBuf;
			startBuf = NULL;
		}

		// parse #include argument
		token = COM_Parse( &ptr );
		if( !token[0] ) {
			Com_Printf( S_COLOR_YELLOW "Syntax error in '%s' around '%s'\n", fileName, line );
			return qtrue;
		}

		if( stackDepth == PARSER_MAX_STACKDEPTH ) {
			Com_Printf( S_COLOR_YELLOW "Include stack overflow in '%s' around '%s'\n", fileName, line );
			return qtrue;
		}

		if( !parser->error ) {
			char *tempFilename;
			size_t tempFilenameSize;

			// load files from current directory, unless the path starts
			// with the leading "/". in that case, go back to to top directory

			COM_SanitizeFilePath( token );

			tempFilenameSize = strlen( fileName ) + 1 + strlen( token ) + 1;
			tempFilename = Mem_TempMalloc( tempFilenameSize );

			if( *token != '/' ) {
				Q_strncpyz( tempFilename, fileName, tempFilenameSize );
				COM_StripFilename( tempFilename );
			} else {
				token++;
				Q_strncpyz( tempFilename, parser->topFile, tempFilenameSize );
				COM_StripFilename( tempFilename );
			}

			Q_strncatz( tempFilename, va( "%s%s", *tempFilename ? "/" : "", token ), tempFilenameSize );

			parser->error = RP_LoadShaderFromFile_r( parser, tempFilename, stackDepth+1 );

			Mem_TempFree( tempFilename );

			if( parser->error ) {
				return qtrue;
			}
		}
	}

	if( startBuf ) {
		if( parser->numStrings == parser->maxStrings ) {
			Com_Printf( S_COLOR_YELLOW "numStrings overflow in '%s'\n", fileName, startBuf );
			return qtrue;
		}
		parser->strings[parser->numStrings++] = startBuf;
	}

	return parser->error;
}

/*
* R_ProgramFeatures2Defines
* 
* Return an array of strings for bitflags
*/
static const char **R_ProgramFeatures2Defines( const glsl_feature_t *type_features, r_glslfeat_t features, char *name, size_t size )
{
	int i, p;
	static const char *headers[MAX_DEFINES_FEATURES+1]; // +1 for NULL safe-guard

	for( i = 0, p = 0; features && type_features && type_features[i].bit; i++ )
	{
		if( (features & type_features[i].bit) == type_features[i].bit )
		{
			headers[p++] = type_features[i].define;
			if( name )
				Q_strncatz( name, type_features[i].suffix, size );

			features &= ~type_features[i].bit;

			if( p == MAX_DEFINES_FEATURES )
				break;
		}
	}

	if( p )
	{
		headers[p] = NULL;
		return headers;
	}

	return NULL;
}

/*
* R_Features2HashKey
*/
static int R_Features2HashKey( r_glslfeat_t features )
{
	int hash = 0x7e53a269;

#define ComputeHash(hash,val) hash = -1521134295 * hash + (val), hash += (hash << 10), hash ^= (hash >> 6)

	ComputeHash(hash, (int)(features & 0xFFFFFFFF));
	ComputeHash(hash, (int)((features >> 32ULL) & 0xFFFFFFFF));

	return hash & (GLSL_PROGRAMS_HASH_SIZE - 1);
}

/*
* RP_RegisterProgram
*/
int RP_RegisterProgram( int type, const char *name, const char *deformsKey, const deformv_t *deforms, int numDeforms, r_glslfeat_t features )
{
	unsigned int i;
	int hash;
	int linked, error = 0;
	int shaderTypeIdx, deformvIdx;
	int body_start, num_init_strings, num_shader_strings;
	glsl_program_t *program;
	char fullName[1024];
	char fileName[1024];
	const char **header;
	char *shaderBuffers[100];
	const char *shaderStrings[MAX_DEFINES_FEATURES+100];
	glslParser_t parser;

	if( type <= GLSL_PROGRAM_TYPE_NONE || type >= GLSL_PROGRAM_TYPE_MAXTYPE )
		return 0;

	assert( !deforms || deformsKey );

	// default deformsKey to empty string, easier on checking later
	if( !deforms )
		deformsKey = "";

	hash = R_Features2HashKey( features );
	for( program = r_glslprograms_hash[type][hash]; program; program = program->hash_next )
	{
		if( ( program->features == features ) && !strcmp( program->deformsKey, deformsKey ) ) {
			return ( (program - r_glslprograms) + 1 );
		}
	}

	if( r_numglslprograms == MAX_GLSL_PROGRAMS )
	{
		Com_Printf( S_COLOR_YELLOW "RP_RegisterProgram: GLSL programs limit exceeded\n" );
		return 0;
	}

	// if no string was specified, search for an already registered program of the same type
	// with minimal set of features specified
	if( !name )
	{
		glsl_program_t *parent;

		parent = NULL;
		for( i = 0; i < r_numglslprograms; i++ ) {
			program = r_glslprograms + i;

			if( (program->type == type) && !program->features ) {
				parent = program;
				break;
			}
		}

		if( parent ) {
			if( !name )
				name = parent->name;
		}
		else {
			Com_Printf( S_COLOR_YELLOW "RP_RegisterProgram: failed to find parent for program type %i\n", type );
			return 0;
		}
	}

	memset( &parser, 0, sizeof( parser ) );

	program = r_glslprograms + r_numglslprograms++;
	program->object = qglCreateProgramObjectARB();
	if( !program->object )
	{
		error = 1;
		goto done;
	}

	Q_strncpyz( fullName, name, sizeof( fullName ) );
	header = R_ProgramFeatures2Defines( glsl_programtypes_features[type], features, fullName, sizeof( fullName ) );

	Q_snprintfz( fileName, sizeof( fileName ), "glsl/%s.glsl", name );

	// load
	//

	Com_DPrintf( "Registering GLSL program %s\n", fullName );

	i = 0;
	if( glConfig.shadingLanguageVersion >= 140 ) {
		shaderStrings[i++] = QF_GLSL_VERSION140;
	}
	else if( glConfig.shadingLanguageVersion >= 130 ) {
		shaderStrings[i++] = QF_GLSL_VERSION130;
	}
	else {
		shaderStrings[i++] = QF_GLSL_VERSION120;
	}
	shaderTypeIdx = i;
	shaderStrings[i++] = "\n";
	shaderStrings[i++] = QF_BUILTIN_GLSL_MACROS;
	shaderStrings[i++] = QF_BUILTIN_GLSL_CONSTANTS;
	shaderStrings[i++] = QF_BUILTIN_GLSL_UNIFORMS;	
	shaderStrings[i++] = QF_GLSL_WAVEFUNCS;
	shaderStrings[i++] = QF_GLSL_DUAL_QUAT_TRANSFORMS;
	shaderStrings[i++] = QF_GLSL_INSTANCED_TRASFORMS;

	if( header ) {
		body_start = i;
		for( ; header[i - body_start] && *header[i - body_start]; i++ )
			shaderStrings[i] = ( char * )header[i - body_start];
	}

	deformvIdx = i;
	if( numDeforms ) {
		// forward declare QF_DeformVerts
		shaderStrings[i++] = R_GLSLBuildDeformv( deforms, numDeforms );
	} else {
		shaderStrings[i++] = "\n";
	}

	num_init_strings = i;

	// load program body
	parser.topFile = fileName;
	parser.error = qfalse;

	parser.buffers = &shaderBuffers[0];
	parser.numBuffers = 0;
	parser.maxBuffers = sizeof( shaderBuffers ) / sizeof( shaderBuffers[0] );

	parser.strings = &shaderStrings[num_init_strings];
	parser.numStrings = 0;
	parser.maxStrings = sizeof( shaderStrings ) / sizeof( shaderStrings[0] ) - num_init_strings;

	RP_LoadShaderFromFile_r( &parser, parser.topFile, 1 );

	num_shader_strings = num_init_strings + parser.numStrings;

	// compile
	//

	RP_BindAttrbibutesLocations( program );

	// vertex shader
	shaderStrings[shaderTypeIdx] = "#define VERTEX_SHADER\n";
	program->vertexShader = RP_CompileShader( program->object, fullName, "vertex", GL_VERTEX_SHADER_ARB, 
		shaderStrings, num_shader_strings );
	if( !program->vertexShader )
	{
		error = 1;
		goto done;
	}

	// fragment shader
	shaderStrings[shaderTypeIdx] = "#define FRAGMENT_SHADER\n";
	shaderStrings[deformvIdx] = "\n";
	program->fragmentShader = RP_CompileShader( program->object, fullName, "fragment", GL_FRAGMENT_SHADER_ARB, 
		shaderStrings, num_shader_strings );
	if( !program->fragmentShader )
	{
		error = 1;
		goto done;
	}

	// link
	qglLinkProgramARB( program->object );
	qglGetObjectParameterivARB( program->object, GL_OBJECT_LINK_STATUS_ARB, &linked );
	if( !linked )
	{
		char log[8192];

		qglGetInfoLogARB( program->object, sizeof( log ), NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] )
			Com_Printf( S_COLOR_YELLOW "Failed to link object for program %s:\n%s\n", fullName, log );

		error = 1;
		goto done;
	}

done:
	if( error )
		RP_DeleteProgram( program );

	for( i = 0; i < parser.numBuffers; i++ ) {
		Mem_Free( parser.buffers[i] );
	}

	program->type = type;
	program->features = features;
	program->name = RP_CopyString( name );
	program->deformsKey = RP_CopyString( deformsKey ? deformsKey : "" );

	if( !program->hash_next )
	{
		program->hash_next = r_glslprograms_hash[type][hash];
		r_glslprograms_hash[type][hash] = program;
	}

	if( program->object )
	{
		qglUseProgramObjectARB( program->object );
		RP_GetUniformLocations( program );
	}

	return ( program - r_glslprograms ) + 1;
}

/*
* RP_GetProgramObject
*/
int RP_GetProgramObject( int elem )
{
	return r_glslprograms[elem - 1].object;
}

/*
* RP_ProgramList_f
*/
void RP_ProgramList_f( void )
{
	int i;
	glsl_program_t *program;
	char fullName[1024];

	Com_Printf( "------------------\n" );
	for( i = 0, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ )
	{
		if( !program->name )
			break;

		Q_strncpyz( fullName, program->name, sizeof( fullName ) );
		R_ProgramFeatures2Defines( glsl_programtypes_features[program->type], program->features, fullName, sizeof( fullName ) );

		Com_Printf( " %3i %s", i+1, fullName );
		if( *program->deformsKey ) {
			Com_Printf( " dv:%s", program->deformsKey );
		}
		Com_Printf( "\n" );
	}
	Com_Printf( "%i programs total\n", i );
}

/*
* RP_UpdateShaderUniforms
*/
void RP_UpdateShaderUniforms( int elem, 
	float shaderTime, 
	const vec3_t entOrigin, const vec3_t entDist, const qbyte *entityColor, 
	const qbyte *constColor, const float *rgbGenFuncArgs, const float *alphaGenFuncArgs,
	const mat4_t texMatrix )
{
	GLfloat m[9];
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( entDist ) {
		if( program->loc.EntityOrigin >= 0 )
			qglUniform3fvARB( program->loc.EntityOrigin, 1, entOrigin );
		if( program->loc.builtin.EntityOrigin >= 0 )
			qglUniform3fvARB( program->loc.builtin.EntityOrigin, 1, entOrigin );
	}

	if( program->loc.EntityDist >= 0 && entDist )
		qglUniform3fvARB( program->loc.EntityDist, 1, entDist );
	if( program->loc.EntityColor >= 0 && entityColor )
		qglUniform4fARB( program->loc.EntityColor, entityColor[0] * 1.0/255.0, entityColor[1] * 1.0/255.0, entityColor[2] * 1.0/255.0, entityColor[3] * 1.0/255.0 );

	if( program->loc.ShaderTime >= 0 )
		qglUniform1fARB( program->loc.ShaderTime, shaderTime );
	if( program->loc.builtin.ShaderTime >= 0 )
		qglUniform1fARB( program->loc.builtin.ShaderTime, shaderTime );

	if( program->loc.ConstColor >= 0 && constColor )
		qglUniform4fARB( program->loc.ConstColor, constColor[0] * 1.0/255.0, constColor[1] * 1.0/255.0, constColor[2] * 1.0/255.0, constColor[3] * 1.0/255.0 );
	if( program->loc.RGBGenFuncArgs >= 0 && rgbGenFuncArgs )
		qglUniform4fvARB( program->loc.RGBGenFuncArgs, 1, rgbGenFuncArgs );
	if( program->loc.AlphaGenFuncArgs >= 0 && alphaGenFuncArgs )
		qglUniform4fvARB( program->loc.AlphaGenFuncArgs, 1, alphaGenFuncArgs );

	// FIXME: this looks shit...
	if( program->loc.TextureMatrix >= 0 ) {
		m[0] = texMatrix[0], m[1] = texMatrix[4];
		m[2] = texMatrix[1], m[3] = texMatrix[5];
		m[4] = texMatrix[12], m[5] = texMatrix[13];

		qglUniform2fvARB( program->loc.TextureMatrix, 3, m );
	}
}

/*
* RP_UpdateViewUniforms
*/
void RP_UpdateViewUniforms( int elem, 
	const mat4_t modelviewMatrix, const mat4_t modelviewProjectionMatrix,
	const vec3_t viewOrigin, const mat3_t viewAxis, 
	const float mirrorSide, 
	int viewport[4],
	float zNear, float zFar )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.ModelViewMatrix >= 0 ) {
		qglUniformMatrix4fvARB( program->loc.ModelViewMatrix, 1, GL_FALSE, modelviewMatrix );
	}
	if( program->loc.ModelViewProjectionMatrix >= 0 ) {
		qglUniformMatrix4fvARB( program->loc.ModelViewProjectionMatrix, 1, GL_FALSE, modelviewProjectionMatrix );
	}

	if( program->loc.ZNear >= 0 ) {
		qglUniform1fARB( program->loc.ZNear, zNear );
	}
	if( program->loc.ZFar >= 0 ) {
		qglUniform1fARB( program->loc.ZFar, zFar );
	}

	if( viewOrigin ) {
		if( program->loc.ViewOrigin >= 0 )
			qglUniform3fvARB( program->loc.ViewOrigin, 1, viewOrigin );
		if( program->loc.builtin.ViewOrigin >= 0 )
			qglUniform3fvARB( program->loc.builtin.ViewOrigin, 1, viewOrigin );
	}

	if( viewAxis ) {
		if( program->loc.ViewAxis >= 0 )
			qglUniformMatrix3fvARB( program->loc.ViewAxis, 1, GL_FALSE, viewAxis );
		if( program->loc.builtin.ViewAxis >= 0 )
			qglUniformMatrix3fvARB( program->loc.builtin.ViewAxis, 1, GL_FALSE, viewAxis );
	}

	if( program->loc.Viewport >= 0 ) {
		qglUniform4ivARB( program->loc.Viewport, 1, viewport );
	}

	if( program->loc.MirrorSide >= 0 )
		qglUniform1fARB( program->loc.MirrorSide, mirrorSide );
	if( program->loc.builtin.MirrorSide >= 0 )
		qglUniform1fARB( program->loc.builtin.MirrorSide, mirrorSide );
}

/*
* RP_UpdateBlendMixUniform
*
* The first component corresponds to RGB, the second to ALPHA.
* Whenever the program needs to scale source colors, the mask needs
* to be used in the following manner:
* color *= mix(myhalf4(1.0), myhalf4(scale), u_BlendMix.xxxy);
*/
void RP_UpdateBlendMixUniform( int elem, vec2_t blendMix )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.BlendMix >= 0 ) {
		qglUniform2fvARB( program->loc.BlendMix, 1, blendMix );
	}
}

/*
* RP_UpdateSoftParticlesUniforms
*/
void RP_UpdateSoftParticlesUniforms( int elem, float scale )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.SoftParticlesScale >= 0 ) {
		qglUniform1fARB( program->loc.SoftParticlesScale, scale );
	}
}

/*
* RP_UpdateDiffuseLightUniforms
*/
void RP_UpdateDiffuseLightUniforms( int elem,
	const vec3_t lightDir, const vec4_t lightAmbient, const vec4_t lightDiffuse )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.LightDir >= 0 && lightDir )
		qglUniform3fARB( program->loc.LightDir, lightDir[0], lightDir[1], lightDir[2] );
	if( program->loc.LightAmbient >= 0 && lightAmbient )
		qglUniform3fARB( program->loc.LightAmbient, lightAmbient[0], lightAmbient[1], lightAmbient[2] );
	if( program->loc.LightDiffuse >= 0 && lightDiffuse )
		qglUniform3fARB( program->loc.LightDiffuse, lightDiffuse[0], lightDiffuse[1], lightDiffuse[2] );
}

/*
* RP_UpdateMaterialUniforms
*/
void RP_UpdateMaterialUniforms( int elem,
	float offsetmappingScale, float glossIntensity, float glossExponent )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.GlossIntensity >= 0 )
		qglUniform1fARB( program->loc.GlossIntensity, glossIntensity );
	if( program->loc.GlossExponent >= 0 )
		qglUniform1fARB( program->loc.GlossExponent, glossExponent );
	if( program->loc.OffsetMappingScale >= 0 )
		qglUniform1fARB( program->loc.OffsetMappingScale, offsetmappingScale );
}

/*
* RP_UpdateDistortionUniforms
*/
void RP_UpdateDistortionUniforms( int elem, qboolean frontPlane, int TexWidth, int TexHeight )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.FrontPlane >= 0 )
		qglUniform1fARB( program->loc.FrontPlane, frontPlane ? 1 : -1 );
	if( program->loc.TextureParams >= 0 )
		qglUniform4fARB( program->loc.TextureParams, TexWidth, TexHeight, TexWidth ? 1.0 / TexWidth : 1.0, TexHeight ? 1.0 / TexHeight : 1.0 );
}

/*
* RP_UpdateOutlineUniforms
*/
void RP_UpdateOutlineUniforms( int elem, float projDistance )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.OutlineHeight >= 0 )
		qglUniform1fARB( program->loc.OutlineHeight, projDistance );
	if( program->loc.OutlineCutOff >= 0 )
		qglUniform1fARB( program->loc.OutlineCutOff, max( 0, r_outlines_cutoff->value ) );
}

/*
* RP_UpdateFogUniforms
*/
void RP_UpdateFogUniforms( int elem, byte_vec4_t color, float clearDist, float opaqueDist, cplane_t *fogPlane, cplane_t *eyePlane, float eyeDist )
{
	GLfloat fog_color[3] = { 0, 0, 0 };
	glsl_program_t *program = r_glslprograms + elem - 1;

	VectorScale( color, (1.0/255.0), fog_color );

	if( program->loc.Fog.Color >= 0 )
		qglUniform3fvARB( program->loc.Fog.Color, 1, fog_color ); 
	if( program->loc.Fog.Scale >= 0 )
		qglUniform1fARB( program->loc.Fog.Scale, 1.0 / (opaqueDist - clearDist) );
	if( program->loc.Fog.Plane >= 0 )
		qglUniform4fARB( program->loc.Fog.Plane, fogPlane->normal[0], fogPlane->normal[1], fogPlane->normal[2], fogPlane->dist );
	if( program->loc.Fog.EyePlane >= 0 )
		qglUniform4fARB( program->loc.Fog.EyePlane, eyePlane->normal[0], eyePlane->normal[1], eyePlane->normal[2], eyePlane->dist );
	if( program->loc.Fog.EyeDist >= 0 )
		qglUniform1fARB( program->loc.Fog.EyeDist, eyeDist );
}

/*
* RP_UpdateDynamicLightsUniforms
*/
unsigned int RP_UpdateDynamicLightsUniforms( int elem, const superLightStyle_t *superLightStyle, 
	const vec3_t entOrigin, const mat3_t entAxis, unsigned int dlightbits )
{
	int i, n;
	dlight_t *dl;
	float colorScale = mapConfig.mapLightColorScale;
	vec3_t dlorigin, tvec;
	glsl_program_t *program = r_glslprograms + elem - 1;
	qboolean identityAxis = Matrix3_Compare( entAxis, axis_identity );

	if( superLightStyle ) {
		int i;
		GLfloat rgb[3];

		for( i = 0; i < MAX_LIGHTMAPS && superLightStyle->lightmapStyles[i] != 255; i++ ) {
			VectorCopy( rsc.lightStyles[superLightStyle->lightmapStyles[i]].rgb, rgb );
			if( mapConfig.lightingIntensity )
				VectorScale( rgb, mapConfig.lightingIntensity, rgb );

			if( program->loc.LightstyleColor >= 0 )	
				qglUniform3fvARB( program->loc.LightstyleColor+i, 1, rgb );
			if( program->loc.DeluxemapOffset >= 0 )	
				qglUniform1fARB( program->loc.DeluxemapOffset+i, superLightStyle->stOffset[i][0] );
		}
	}

	if( dlightbits ) {
		n = 0;
		for( i = 0; i < MAX_DLIGHTS; i++ ) {
			dl = rsc.dlights + i;
			if( !dl->intensity ) {
				continue;
			}
			if( program->loc.DynamicLightsRadius[n] < 0 ) {
				break;
			}

			VectorSubtract( dl->origin, entOrigin, dlorigin );
			if( !identityAxis ) {
				VectorCopy( dlorigin, tvec );
				Matrix3_TransformVector( entAxis, tvec, dlorigin );
			}

			qglUniform1fARB( program->loc.DynamicLightsRadius[n], dl->intensity );
			qglUniform3fARB( program->loc.DynamicLightsPosition[n], dlorigin[0], dlorigin[1], dlorigin[2] );
			qglUniform3fARB( program->loc.DynamicLightsDiffuse[n], dl->color[0] * colorScale, dl->color[1] * colorScale, dl->color[2] * colorScale );

			n++;
			dlightbits &= ~(1<<i);
			if( !dlightbits ) {
				break;
			}
		}

		for( ; n < MAX_DLIGHTS; n++ ) {
			if( program->loc.DynamicLightsRadius[n] < 0 ) {
				break;
			}
			qglUniform1fARB( program->loc.DynamicLightsRadius[n], 1 );
			qglUniform3fARB( program->loc.DynamicLightsDiffuse[n], 0, 0, 0 );
		}
	}
	
	return 0;
}

/*
* RP_UpdateTexGenUniforms
*/
void RP_UpdateTexGenUniforms( int elem, const mat4_t reflectionMatrix, const mat4_t vectorMatrix )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.ReflectionTexMatrix >= 0 )
		qglUniformMatrix4fvARB( program->loc.ReflectionTexMatrix, 1, GL_FALSE, reflectionMatrix );
	if( program->loc.VectorTexMatrix >= 0 )
		qglUniformMatrix4fvARB( program->loc.VectorTexMatrix, 1, GL_FALSE, vectorMatrix );
}

/*
* RP_UpdateShadowsUniforms
*/
void RP_UpdateShadowsUniforms( int elem, int numShadows, const shadowGroup_t **groups, const mat4_t objectMatrix )
{
	int i;
	const shadowGroup_t *group;
	mat4_t matrix;
	glsl_program_t *program = r_glslprograms + elem - 1;

	assert( groups != NULL );
	assert( numShadows <= GLSL_SHADOWMAP_LIMIT );

	if( numShadows > GLSL_SHADOWMAP_LIMIT ) {
		numShadows = GLSL_SHADOWMAP_LIMIT;
	}

	for( i = 0; i < numShadows; i++ ) {
		group = groups[i];

		if( program->loc.ShadowProjDistance[i] >= 0 ) {
			qglUniform1fARB( program->loc.ShadowProjDistance[i], group->projDist );
		}

		if( program->loc.ShadowmapTextureParams[i] >= 0 ) {
			qglUniform4fARB( program->loc.ShadowmapTextureParams[i], 
				group->viewportSize[0], group->viewportSize[1], 
				1.0f / group->textureSize[0], 1.0 / group->textureSize[1] );
		}

		if( program->loc.ShadowmapMatrix[i] >= 0 ) {
			Matrix4_Multiply( group->cameraProjectionMatrix, objectMatrix, matrix );
			qglUniformMatrix4fvARB( program->loc.ShadowmapMatrix[i], 1, GL_FALSE, matrix );
		}

		if( program->loc.ShadowAlpha >= 0 ) {
			qglUniform1fARB( program->loc.ShadowAlpha, group->alpha );
		}
	}
}

/*
* RP_UpdateBonesUniforms
* 
* Set uniform values for animation dual quaternions
*/
void RP_UpdateBonesUniforms( int elem, unsigned int numBones, const dualquat_t *animDualQuat )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( numBones > glConfig.maxGLSLBones ) {
		return;
	}
	if( !program->loc.builtin.DualQuats < 0 ) {
		return;
	}
	qglUniform4fvARB( program->loc.builtin.DualQuats, numBones * 2, &animDualQuat[0][0] );
}

/*
* RP_UpdateInstancesUniforms
* 
* Set uniform values for instance points (quaternion + xyz + scale)
*/
void RP_UpdateInstancesUniforms( int elem, unsigned int numInstances, const instancePoint_t *instances )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( numInstances > MAX_GLSL_UNIFORM_INSTANCES ) {
		numInstances = MAX_GLSL_UNIFORM_INSTANCES;
	}
	if( !program->loc.builtin.InstancePoints < 0 ) {
		return;
	}
	qglUniform4fvARB( program->loc.builtin.InstancePoints, numInstances * 2, &instances[0][0] );
}


/*
* RP_UpdateDrawFlatUniforms
*/
void RP_UpdateDrawFlatUniforms( int elem, const vec3_t wallColor, const vec3_t floorColor )
{
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.WallColor >= 0 )
		qglUniform3fARB( program->loc.WallColor, wallColor[0], wallColor[1], wallColor[2] );
	if( program->loc.FloorColor >= 0 )
		qglUniform3fARB( program->loc.FloorColor, floorColor[0], floorColor[1], floorColor[2] );
}

/*
* RP_GetUniformLocations
*/
static void RP_GetUniformLocations( glsl_program_t *program )
{
	unsigned int i;
	int		locBaseTexture,
			locNormalmapTexture,
			locGlossTexture,
			locDecalTexture,
			locEntityDecalTexture,
			locLightmapTexture[MAX_LIGHTMAPS],
			locDuDvMapTexture,
			locReflectionTexture,
			locRefractionTexture,
			locShadowmapTexture[GLSL_SHADOWMAP_LIMIT],
			locCelShadeTexture,
			locCelLightTexture,
			locDiffuseTexture,
			locStripesTexture,
			locDepthTexture;

	memset( &program->loc, -1, sizeof( program->loc ) );

	program->loc.ModelViewMatrix = qglGetUniformLocationARB( program->object, "u_ModelViewMatrix" );
	program->loc.ModelViewProjectionMatrix = qglGetUniformLocationARB( program->object, "u_ModelViewProjectionMatrix" );

	program->loc.ZNear = qglGetUniformLocationARB( program->object, "u_ZNear" );
	program->loc.ZFar = qglGetUniformLocationARB( program->object, "u_ZFar" );

	program->loc.ViewOrigin = qglGetUniformLocationARB( program->object, "u_ViewOrigin" );
	program->loc.ViewAxis = qglGetUniformLocationARB( program->object, "u_ViewAxis" );

	program->loc.MirrorSide = qglGetUniformLocationARB( program->object, "u_MirrorSide" );

	program->loc.Viewport = qglGetUniformLocationARB( program->object, "u_Viewport" );

	program->loc.LightDir = qglGetUniformLocationARB( program->object, "u_LightDir" );
	program->loc.LightAmbient = qglGetUniformLocationARB( program->object, "u_LightAmbient" );
	program->loc.LightDiffuse = qglGetUniformLocationARB( program->object, "u_LightDiffuse" );

	program->loc.TextureMatrix = qglGetUniformLocationARB( program->object, "u_TextureMatrix" );
	
	locBaseTexture = qglGetUniformLocationARB( program->object, "u_BaseTexture" );
	locNormalmapTexture = qglGetUniformLocationARB( program->object, "u_NormalmapTexture" );
	locGlossTexture = qglGetUniformLocationARB( program->object, "u_GlossTexture" );
	locDecalTexture = qglGetUniformLocationARB( program->object, "u_DecalTexture" );
	locEntityDecalTexture = qglGetUniformLocationARB( program->object, "u_EntityDecalTexture" );

	locDuDvMapTexture = qglGetUniformLocationARB( program->object, "u_DuDvMapTexture" );
	locReflectionTexture = qglGetUniformLocationARB( program->object, "u_ReflectionTexture" );
	locRefractionTexture = qglGetUniformLocationARB( program->object, "u_RefractionTexture" );

	for( i = 0; i < GLSL_SHADOWMAP_LIMIT; i++ ) {
		locShadowmapTexture[i] = qglGetUniformLocationARB( program->object, va( "u_ShadowmapTexture[%i]", i ) );
		if( locShadowmapTexture[i] < 0 )
			break;
	}

	locCelShadeTexture = qglGetUniformLocationARB( program->object, "u_CelShadeTexture" );
	locCelLightTexture = qglGetUniformLocationARB( program->object, "u_CelLightTexture" );
	locDiffuseTexture = qglGetUniformLocationARB( program->object, "u_DiffuseTexture" );
	locStripesTexture = qglGetUniformLocationARB( program->object, "u_StripesTexture" );

	locDepthTexture = qglGetUniformLocationARB( program->object, "u_DepthTexture" );

	program->loc.LightstyleColor = qglGetUniformLocationARB( program->object, "u_LightstyleColor" );
	program->loc.DeluxemapOffset = qglGetUniformLocationARB( program->object, "u_DeluxemapOffset" );

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		locLightmapTexture[i] = qglGetUniformLocationARB( program->object, va( "u_LightmapTexture[%i]", i ) );
		if( locLightmapTexture[i] < 0 )
			break;
	}

	program->loc.GlossIntensity = qglGetUniformLocationARB( program->object, "u_GlossIntensity" );
	program->loc.GlossExponent = qglGetUniformLocationARB( program->object, "u_GlossExponent" );

	program->loc.OffsetMappingScale = qglGetUniformLocationARB( program->object, "u_OffsetMappingScale" );

	program->loc.OutlineHeight = qglGetUniformLocationARB( program->object, "u_OutlineHeight" );
	program->loc.OutlineCutOff = qglGetUniformLocationARB( program->object, "u_OutlineCutOff" );

	program->loc.FrontPlane = qglGetUniformLocationARB( program->object, "u_FrontPlane" );

	program->loc.TextureParams = qglGetUniformLocationARB( program->object, "u_TextureParams" );

	program->loc.EntityDist = qglGetUniformLocationARB( program->object, "u_EntityDist" );
	program->loc.EntityOrigin = qglGetUniformLocationARB( program->object, "u_EntityOrigin" );
	program->loc.EntityColor = qglGetUniformLocationARB( program->object, "u_EntityColor" );
	program->loc.ConstColor = qglGetUniformLocationARB( program->object, "u_ConstColor" );
	program->loc.RGBGenFuncArgs = qglGetUniformLocationARB( program->object, "u_RGBGenFuncArgs" );
	program->loc.AlphaGenFuncArgs = qglGetUniformLocationARB( program->object, "u_AlphaGenFuncArgs" );

	program->loc.Fog.Plane = qglGetUniformLocationARB( program->object, "u_Fog.Plane" );
	program->loc.Fog.Color = qglGetUniformLocationARB( program->object, "u_Fog.Color" );
	program->loc.Fog.Scale = qglGetUniformLocationARB( program->object, "u_Fog.Scale" );
	program->loc.Fog.EyePlane = qglGetUniformLocationARB( program->object, "u_Fog.EyePlane" );
	program->loc.Fog.EyeDist = qglGetUniformLocationARB( program->object, "u_Fog.EyeDist" );

	program->loc.ShaderTime = qglGetUniformLocationARB( program->object, "u_ShaderTime" );

	program->loc.ReflectionTexMatrix = qglGetUniformLocationARB( program->object, "u_ReflectionTexMatrix" );
	program->loc.VectorTexMatrix = qglGetUniformLocationARB( program->object, "u_VectorTexMatrix" );

	program->loc.builtin.ViewOrigin = qglGetUniformLocationARB( program->object, "u_QF_ViewOrigin" );
	program->loc.builtin.ViewAxis = qglGetUniformLocationARB( program->object, "u_QF_ViewAxis" );
	program->loc.builtin.MirrorSide = qglGetUniformLocationARB( program->object, "u_QF_MirrorSide" );
	program->loc.builtin.EntityOrigin = qglGetUniformLocationARB( program->object, "u_QF_EntityOrigin" );
	program->loc.builtin.ShaderTime = qglGetUniformLocationARB( program->object, "u_QF_ShaderTime" );
	program->loc.builtin.DualQuats = qglGetUniformLocationARB( program->object, "u_QF_DualQuats" );
	program->loc.builtin.InstancePoints = qglGetUniformLocationARB( program->object, "u_QF_InstancePoints" );

	// dynamic lights
	for( i = 0; i < MAX_DLIGHTS; i++ ) {
		int locR, locP, locD;

		locR = qglGetUniformLocationARB( program->object, va( "u_DynamicLights[%i].Radius", i ) );
		locP = qglGetUniformLocationARB( program->object, va( "u_DynamicLights[%i].Position", i ) );
		locD = qglGetUniformLocationARB( program->object, va( "u_DynamicLights[%i].Diffuse", i ) );

		if( locR < 0 || locP < 0 || locD < 0 ) {
			program->loc.DynamicLightsRadius[i] = program->loc.DynamicLightsPosition[i] = 
				program->loc.DynamicLightsDiffuse[i] = -1;
			break;
		}

		program->loc.DynamicLightsRadius[i] = locR;
		program->loc.DynamicLightsPosition[i] = locP;
		program->loc.DynamicLightsDiffuse[i] = locD;
	}

	// shadowmaps
	for( i = 0; i < GLSL_SHADOWMAP_LIMIT; i++ ) {
		program->loc.ShadowmapTextureParams[i] = 
			qglGetUniformLocationARB( program->object, va( "u_ShadowmapTextureParams[%i]", i ) );
		if( program->loc.ShadowmapTextureParams[i] < 0 )
			break;

		program->loc.ShadowmapMatrix[i] = 
			qglGetUniformLocationARB( program->object, va( "u_ShadowmapMatrix[%i]", i ) );
		program->loc.ShadowProjDistance[i] = 
			qglGetUniformLocationARB( program->object, va( "u_ShadowProjDistance[%i]", i ) );
	}

	program->loc.ShadowAlpha = qglGetUniformLocationARB( program->object, "u_ShadowAlpha" );

	program->loc.BlendMix = qglGetUniformLocationARB( program->object, "u_BlendMix" );

	program->loc.SoftParticlesScale = qglGetUniformLocationARB( program->object, "u_SoftParticlesScale" );
	
	program->loc.WallColor = qglGetUniformLocationARB( program->object, "u_WallColor" );
	program->loc.FloorColor = qglGetUniformLocationARB( program->object, "u_FloorColor" );

	if( locBaseTexture >= 0 )
		qglUniform1iARB( locBaseTexture, 0 );
	if( locDuDvMapTexture >= 0 )
		qglUniform1iARB( locDuDvMapTexture, 0 );

	if( locNormalmapTexture >= 0 )
		qglUniform1iARB( locNormalmapTexture, 1 );
	if( locGlossTexture >= 0 )
		qglUniform1iARB( locGlossTexture, 2 );
	if( locDecalTexture >= 0 )
		qglUniform1iARB( locDecalTexture, 3 );
	if( locEntityDecalTexture >= 0 )
		qglUniform1iARB( locEntityDecalTexture, 4 );

	if( locReflectionTexture >= 0 )
		qglUniform1iARB( locReflectionTexture, 2 );
	if( locRefractionTexture >= 0 )
		qglUniform1iARB( locRefractionTexture, 3 );

	for( i = 0; i < GLSL_SHADOWMAP_LIMIT && locShadowmapTexture[i] >= 0; i++ )
		qglUniform1iARB( locShadowmapTexture[i], i );

//	if( locBaseTexture >= 0 )
//		qglUniform1iARB( locBaseTexture, 0 );
	if( locCelShadeTexture >= 0 )
		qglUniform1iARB( locCelShadeTexture, 1 );
	if( locDiffuseTexture >= 0 )
		qglUniform1iARB( locDiffuseTexture, 2 );
//	if( locDecalTexture >= 0 )
//		qglUniform1iARB( locDecalTexture, 3 );
//	if( locEntityDecalTexture >= 0 )
//		qglUniform1iARB( locEntityDecalTexture, 4 );
	if( locStripesTexture >= 0 )
		qglUniform1iARB( locStripesTexture, 5 );
	if( locCelLightTexture >= 0 )
		qglUniform1iARB( locCelLightTexture, 6 );

	if( locDepthTexture >= 0 )
		qglUniform1iARB( locDepthTexture, 3 );

	for( i = 0; i < MAX_LIGHTMAPS && locLightmapTexture[i] >= 0; i++ )
		qglUniform1iARB( locLightmapTexture[i], i+4 );
}

/*
* RP_BindAttrbibutesLocations
*/
static void RP_BindAttrbibutesLocations( glsl_program_t *program )
{
	qglBindAttribLocationARB( program->object, VATTRIB_POSITION, "a_Position" ); 
	qglBindAttribLocationARB( program->object, VATTRIB_SVECTOR, "a_SVector" ); 
	qglBindAttribLocationARB( program->object, VATTRIB_NORMAL, "a_Normal" ); 
	qglBindAttribLocationARB( program->object, VATTRIB_COLOR, "a_Color" );
	qglBindAttribLocationARB( program->object, VATTRIB_TEXCOORDS, "a_TexCoord" );

	qglBindAttribLocationARB( program->object, VATTRIB_SPRITEPOINT, "a_SpritePoint" );
	qglBindAttribLocationARB( program->object, VATTRIB_SPRITERAXIS, "a_SpriteRightAxis" );
	qglBindAttribLocationARB( program->object, VATTRIB_SPRITEUAXIS, "a_SpriteUpAxis" );

	qglBindAttribLocationARB( program->object, VATTRIB_BONESINDICES, "a_BonesIndices" );
	qglBindAttribLocationARB( program->object, VATTRIB_BONESWEIGHTS, "a_BonesWeights" );

	qglBindAttribLocationARB( program->object, VATTRIB_LMCOORDS, "a_LightmapCoord0" );
	qglBindAttribLocationARB( program->object, VATTRIB_LMCOORDS1, "a_LightmapCoord1" );
	qglBindAttribLocationARB( program->object, VATTRIB_LMCOORDS2, "a_LightmapCoord2" );
	qglBindAttribLocationARB( program->object, VATTRIB_LMCOORDS3, "a_LightmapCoord3" );

	qglBindAttribLocationARB( program->object, VATTRIB_COLOR1, "a_Color1" );
	qglBindAttribLocationARB( program->object, VATTRIB_COLOR2, "a_Color2" );
	qglBindAttribLocationARB( program->object, VATTRIB_COLOR3, "a_Color3" );

	qglBindAttribLocationARB( program->object, VATTRIB_INSTANCE_QUAT, "a_InstanceQuat" );
	qglBindAttribLocationARB( program->object, VATTRIB_INSTANCE_XYZS, "a_InstancePosAndScale" );

	if( glConfig.shadingLanguageVersion >= 130 ) {
		qglBindFragDataLocation( program->object, 0, "qf_FragColor" );
	}
}

/*
* RP_Shutdown
*/
void RP_Shutdown( void )
{
	unsigned int i;
	glsl_program_t *program;

	RP_StorePrecacheList();

	for( i = 0, program = r_glslprograms; i < r_numglslprograms; i++, program++ )
		RP_DeleteProgram( program );

	Trie_Destroy( glsl_cache_trie );
	glsl_cache_trie = NULL;

	r_numglslprograms = 0;
}
