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
#include "../../qalgo/q_trie.h"

#define MAX_GLSL_PROGRAMS           1024
#define GLSL_PROGRAMS_HASH_SIZE     256

#define GLSL_DEFAULT_CACHE_FILE_NAME    "glsl/glsl.cache.default"

#define GLSL_CACHE_FILE_NAME            "cache/glsl.cache"
#define GLSL_BINARY_CACHE_FILE_NAME     "cache/glsl.cache.bin"

typedef struct {
	r_glslfeat_t bit;
	const char      *define;
	const char      *suffix;
} glsl_feature_t;

typedef struct glsl_program_s {
	char            *name;
	int type;
	r_glslfeat_t features;
	const char      *string;
	char            *deformsKey;
	struct glsl_program_s *hash_next;

	int object;
	int vertexShader;
	int fragmentShader;

	int binaryCachePos;

	struct loc_s {
		int ModelViewMatrix,
			ModelViewProjectionMatrix,

			ZRange,

			ViewOrigin,
			ViewAxis,

			MirrorSide,

			Viewport,

			LightDir,
			LightAmbient,
			LightDiffuse,
			LightingIntensity,

			TextureMatrix,

			GlossFactors,

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
				ScaleAndEyeDist,
				EyePlane;
		} Fog;

		int ShaderTime,

			ReflectionTexMatrix,
			VectorTexMatrix,

			DeluxemapOffset,
			LightstyleColor[MAX_LIGHTMAPS],

			DynamicLightsMatrix,
			DynamicLightsDiffuseAndInvRadius,
			DynamicLightVector,

			AttrBonesIndices,
			AttrBonesWeights,
			DualQuats,

			InstancePoints,

			WallColor,
			FloorColor,

			BlendMix,
			ColorMod,

			SoftParticlesScale;

		int hdrGamma,
			hdrExposure;

		// builtin uniforms
		struct {
			int ShaderTime,
				ViewOrigin,
				ViewAxis,
				MirrorSide,
				EntityOrigin;
		} builtin;
	} loc;
} glsl_program_t;

trie_t *glsl_cache_trie = NULL;

static bool r_glslprograms_initialized;

static unsigned int r_numglslprograms;
static glsl_program_t r_glslprograms[MAX_GLSL_PROGRAMS];
static glsl_program_t *r_glslprograms_hash[GLSL_PROGRAM_TYPE_MAXTYPE][GLSL_PROGRAMS_HASH_SIZE];

static int r_glslbincache_storemode;

static void RP_GetUniformLocations( glsl_program_t *program );
static void RP_BindAttrbibutesLocations( glsl_program_t *program );

static void *RP_GetProgramBinary( int elem, int *format, unsigned *length );
static int RP_RegisterProgramBinary( int type, const char *name, const char *deformsKey,
									 const deformv_t *deforms, int numDeforms, r_glslfeat_t features,
									 int binaryFormat, unsigned binaryLength, void *binary );

/*
* RP_Init
*/
void RP_Init( void ) {
	int program;

	if( r_glslprograms_initialized ) {
		return;
	}

	memset( r_glslprograms, 0, sizeof( r_glslprograms ) );
	memset( r_glslprograms_hash, 0, sizeof( r_glslprograms_hash ) );

	Trie_Create( TRIE_CASE_INSENSITIVE, &glsl_cache_trie );

	// register base programs
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, DEFAULT_GLSL_MATERIAL_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_DISTORTION, DEFAULT_GLSL_DISTORTION_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_OUTLINE, DEFAULT_GLSL_OUTLINE_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_Q3A_SHADER, DEFAULT_GLSL_Q3A_SHADER_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_CELSHADE, DEFAULT_GLSL_CELSHADE_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_FOG, DEFAULT_GLSL_FOG_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_COLOR_CORRECTION, DEFAULT_GLSL_COLORCORRECTION_PROGRAM, NULL, NULL, 0, 0 );
	RP_RegisterProgram( GLSL_PROGRAM_TYPE_KAWASE_BLUR, DEFAULT_GLSL_KAWASE_BLUR_PROGRAM, NULL, NULL, 0, 0 );

	// check whether compilation of the shader with GPU skinning succeeds, if not, disable GPU bone transforms
	if( glConfig.maxGLSLBones ) {
		program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, DEFAULT_GLSL_MATERIAL_PROGRAM, NULL, NULL, 0, GLSL_SHADER_COMMON_BONE_TRANSFORMS1 );
		if( !program ) {
			glConfig.maxGLSLBones = 0;
		}
	}

	r_glslprograms_initialized = true;
}

/*
* RP_PrecachePrograms
*
* Loads the list of known program permutations from disk file.
*
* Expected file format:
* application_name\n
* version_number\n*
* program_type1 features_lower_bits1 features_higher_bits1 program_name1 binary_offset
* ..
* program_typeN features_lower_bitsN features_higher_bitsN program_nameN binary_offset
*/
void RP_PrecachePrograms( void ) {
	int version;
	char *buffer = NULL, *data, **ptr;
	const char *token;
	int handleBin;
	size_t binaryCacheSize = 0;
	bool isDefaultCache = false;
	char tempbuf[MAX_TOKEN_CHARS];

	R_LoadCacheFile( GLSL_CACHE_FILE_NAME, ( void ** )&buffer );
	if( !buffer ) {
		isDefaultCache = true;
		r_glslbincache_storemode = FS_WRITE;

		// load default glsl cache list, supposedly shipped with the game
		R_LoadFile( GLSL_DEFAULT_CACHE_FILE_NAME, ( void ** )&buffer );
		if( !buffer ) {
			return;
		}
	}

#define CLOSE_AND_DROP_BINARY_CACHE() do { \
		ri.FS_FCloseFile( handleBin ); \
		handleBin = 0; \
		r_glslbincache_storemode = FS_WRITE; \
} while( 0 )

	handleBin = 0;
	if( glConfig.ext.get_program_binary && !isDefaultCache ) {
		r_glslbincache_storemode = FS_APPEND;
		if( ri.FS_FOpenFile( GLSL_BINARY_CACHE_FILE_NAME, &handleBin, FS_READ | FS_CACHE ) != -1 ) {
			unsigned hash;

			version = 0;
			hash = 0;

			ri.FS_Seek( handleBin, 0, FS_SEEK_END );
			binaryCacheSize = ri.FS_Tell( handleBin );
			ri.FS_Seek( handleBin, 0, FS_SEEK_SET );

			ri.FS_Read( &version, sizeof( version ), handleBin );
			ri.FS_Read( &hash, sizeof( hash ), handleBin );

			if( binaryCacheSize < 8 || version != GLSL_BITS_VERSION || hash != glConfig.versionHash ) {
				CLOSE_AND_DROP_BINARY_CACHE();
			}
		}
	}

	data = buffer;
	ptr = &data;

	token = COM_Parse_r( tempbuf, sizeof( tempbuf ), ptr );
	if( strcmp( token, APPLICATION ) ) {
		ri.Com_DPrintf( "Ignoring GLSL cache: unknown application name \"%s\", expected \"%s\"\n",
						token, APPLICATION );
		return;
	}

	token = COM_Parse_r( tempbuf, sizeof( tempbuf ), ptr );
	version = atoi( token );
	if( version != GLSL_BITS_VERSION ) {
		// ignore cache files with mismatching version number
		ri.Com_DPrintf( "Ignoring GLSL cache: found version %i, expected %i\n", version, GLSL_BITS_VERSION );
	} else {
		while( 1 ) {
			int type;
			r_glslfeat_t lb, hb;
			r_glslfeat_t features;
			char name[256];
			void *binary = NULL;
			int binaryFormat = 0;
			unsigned binaryLength = 0;
			int binaryPos = 0;

			// read program type
			token = COM_Parse_r( tempbuf, sizeof( tempbuf ), ptr );
			if( !token[0] ) {
				break;
			}
			type = atoi( token );

			// read lower bits
			token = COM_ParseExt_r( tempbuf, sizeof( tempbuf ), ptr, false );
			if( !token[0] ) {
				break;
			}
			lb = atoi( token );

			// read higher bits
			token = COM_ParseExt_r( tempbuf, sizeof( tempbuf ), ptr, false );
			if( !token[0] ) {
				break;
			}
			hb = atoi( token );

			// read program full name
			token = COM_ParseExt_r( tempbuf, sizeof( tempbuf ), ptr, false );
			if( !token[0] ) {
				break;
			}

			Q_strncpyz( name, token, sizeof( name ) );
			features = ( hb << 32 ) | lb;

			// read optional binary cache
			token = COM_ParseExt_r( tempbuf, sizeof( tempbuf ), ptr, false );
			if( handleBin && token[0] ) {
				binaryPos = atoi( token );
				if( binaryPos ) {
					bool err = false;

					err = !err && ri.FS_Seek( handleBin, binaryPos, FS_SEEK_SET ) < 0;
					err = !err && ri.FS_Read( &binaryFormat, sizeof( binaryFormat ), handleBin ) != sizeof( binaryFormat );
					err = !err && ri.FS_Read( &binaryLength, sizeof( binaryLength ), handleBin ) != sizeof( binaryLength );
					if( err || binaryLength >= binaryCacheSize ) {
						binaryLength = 0;
						CLOSE_AND_DROP_BINARY_CACHE();
					}

					if( binaryLength ) {
						binary = R_Malloc( binaryLength );
						if( binary != NULL && ri.FS_Read( binary, binaryLength, handleBin ) != (int)binaryLength ) {
							R_Free( binary );
							binary = NULL;
							CLOSE_AND_DROP_BINARY_CACHE();
						}
					}
				}
			}

			if( binary ) {
				int elem;

				ri.Com_DPrintf( "Loading binary program %s...\n", name );

				elem = RP_RegisterProgramBinary( type, name, NULL, NULL, 0, features,
												 binaryFormat, binaryLength, binary );

				if( RP_GetProgramObject( elem ) == 0 ) {
					// check whether the program actually exists
					elem = 0;
				}

				if( !elem ) {
					// rewrite this binary cache on exit
					CLOSE_AND_DROP_BINARY_CACHE();
				} else {
					glsl_program_t *program = r_glslprograms + elem - 1;
					program->binaryCachePos = binaryPos;
				}

				R_Free( binary );
				binary = NULL;

				if( elem ) {
					continue;
				}
				// fallthrough to regular registration
			}

			ri.Com_DPrintf( "Loading program %s...\n", name );

			RP_RegisterProgram( type, name, NULL, NULL, 0, features );
		}
	}

#undef CLOSE_AND_DROP_BINARY_CACHE

	R_FreeFile( buffer );

	if( handleBin ) {
		ri.FS_FCloseFile( handleBin );
	}
}


/*
* RP_StorePrecacheList
*
* Stores the list of known GLSL program permutations to file on the disk.
* File format matches that expected by RP_PrecachePrograms.
*/
void RP_StorePrecacheList( void ) {
	unsigned int i;
	int handle, handleBin;
	glsl_program_t *program;
	unsigned dummy;

	if( !r_glslprograms_initialized ) {
		return;
	}

	handle = 0;
	if( ri.FS_FOpenFile( GLSL_CACHE_FILE_NAME, &handle, FS_WRITE | FS_CACHE ) == -1 ) {
		Com_Printf( S_COLOR_YELLOW "Could not open %s for writing.\n", GLSL_CACHE_FILE_NAME );
		return;
	}

	handleBin = 0;
	if( glConfig.ext.get_program_binary ) {
		if( ri.FS_FOpenFile( GLSL_BINARY_CACHE_FILE_NAME, &handleBin, r_glslbincache_storemode | FS_CACHE ) == -1 ) {
			Com_Printf( S_COLOR_YELLOW "Could not open %s for writing.\n", GLSL_BINARY_CACHE_FILE_NAME );
		} else if( r_glslbincache_storemode == FS_WRITE ) {
			dummy = 0;
			ri.FS_Write( &dummy, sizeof( dummy ), handleBin );

			dummy = glConfig.versionHash;
			ri.FS_Write( &dummy, sizeof( dummy ), handleBin );
		} else {
			ri.FS_Seek( handleBin, 0, FS_SEEK_END );
		}
	}

	ri.FS_Printf( handle, "%s\n", APPLICATION );
	ri.FS_Printf( handle, "%i\n", GLSL_BITS_VERSION );

	for( i = 0, program = r_glslprograms; i < r_numglslprograms; i++, program++ ) {
		void *binary = NULL;
		int binaryFormat = 0;
		unsigned binaryLength = 0;
		int binaryPos = 0;

		if( *program->deformsKey ) {
			continue;
		}
		if( !program->features ) {
			continue;
		}

		if( handleBin ) {
			if( r_glslbincache_storemode == FS_APPEND && program->binaryCachePos ) {
				// this program is already cached
				binaryPos = program->binaryCachePos;
			} else {
				binary = RP_GetProgramBinary( i + 1, &binaryFormat, &binaryLength );
				if( binary ) {
					binaryPos = ri.FS_Tell( handleBin );
				}
			}
		}

		ri.FS_Printf( handle, "%i %i %i \"%s\" %u\n",
					  program->type,
					  (int)( program->features & ULONG_MAX ),
					  (int)( ( program->features >> 32 ) & ULONG_MAX ),
					  program->name, binaryPos );

		if( binary ) {
			ri.FS_Write( &binaryFormat, sizeof( binaryFormat ), handleBin );
			ri.FS_Write( &binaryLength, sizeof( binaryLength ), handleBin );
			ri.FS_Write( binary, binaryLength, handleBin );
			R_Free( binary );
		}
	}

	ri.FS_FCloseFile( handle );
	ri.FS_FCloseFile( handleBin );

	if( handleBin && ri.FS_FOpenFile( GLSL_BINARY_CACHE_FILE_NAME, &handleBin, FS_UPDATE | FS_CACHE ) != -1 ) {
		dummy = GLSL_BITS_VERSION;
		ri.FS_Write( &dummy, sizeof( dummy ), handleBin );
		ri.FS_FCloseFile( handleBin );
	}
}

/*
* RF_DeleteProgram
*/
static void RF_DeleteProgram( glsl_program_t *program ) {
	glsl_program_t *hash_next;

	if( program->vertexShader ) {
		glDetachShader( program->object, program->vertexShader );
		glDeleteShader( program->vertexShader );
		program->vertexShader = 0;
	}

	if( program->fragmentShader ) {
		glDetachShader( program->object, program->fragmentShader );
		glDeleteShader( program->fragmentShader );
		program->fragmentShader = 0;
	}

	if( program->object ) {
		glDeleteProgram( program->object );
	}

	if( program->name ) {
		R_Free( program->name );
	}
	if( program->deformsKey ) {
		R_Free( program->deformsKey );
	}

	hash_next = program->hash_next;
	memset( program, 0, sizeof( glsl_program_t ) );
	program->hash_next = hash_next;
}

/*
* RF_CompileShader
*/
static int RF_CompileShader( int program, const char *programName, const char *shaderName,
							 int shaderType, const char **strings, int numStrings ) {
	GLuint shader = glCreateShader( (GLenum)shaderType );
	if( !shader ) {
		return 0;
	}

	// if lengths is NULL, then each string is assumed to be null-terminated
	glShaderSource( shader, numStrings, strings, NULL );
	glCompileShader( shader );

	GLint compiled;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );

	if( !compiled ) {
		char log[4096];

		glGetShaderInfoLog( shader, sizeof( log ) - 1, NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] ) {
			Com_Printf( "!!! begin\n" );
			for( int i = 0; i < numStrings; i++ ) {
				Com_Printf( "%s", strings[i] );
			}
			Com_Printf( "\n!!! end\n" );

			Com_Printf( S_COLOR_YELLOW "Failed to compile %s shader above for program %s\n",
						shaderName, programName );
			Com_Printf( "%s", log );
			Com_Printf( "\n" );
		}

		glDeleteShader( shader );
		return 0;
	}

	glAttachShader( program, shader );

	return shader;
}

// ======================================================================================

#define MAX_DEFINES_FEATURES    255

static const glsl_feature_t glsl_features_empty[] =
{
	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_material[] =
{
	{ GLSL_SHADER_COMMON_GREYSCALE, "#define APPLY_GREYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define QF_NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define QF_NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define QF_NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define QF_NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },
	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },

	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_DLIGHTS, "#define NUM_DLIGHTS 1\n", "_dl" },
	{ GLSL_SHADER_COMMON_DLIGHT_CUBEFILTER, "#define APPLY_DLIGHT_CUBEFILTER\n", "_dlcf" },
	{ GLSL_SHADER_COMMON_DLIGHT_DIRECTIONAL, "#define APPLY_DLIGHT_DIRECTIONAL\n", "_dir" },

	{ GLSL_SHADER_COMMON_DRAWFLAT, "#define APPLY_DRAWFLAT\n", "_flat" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },

	{ GLSL_SHADER_COMMON_LIGHTING, "#define APPLY_LIGHTING\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n"
	  "#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_AFUNC_GE128, "#define QF_ALPHATEST(a) { if ((a) < 0.5) discard; }\n", "_afunc_ge128" },
	{ GLSL_SHADER_COMMON_AFUNC_LT128, "#define QF_ALPHATEST(a) { if ((a) >= 0.5) discard; }\n", "_afunc_lt128" },
	{ GLSL_SHADER_COMMON_AFUNC_GT0, "#define QF_ALPHATEST(a) { if ((a) <= 0.0) discard; }\n", "_afunc_gt0" },

	{ GLSL_SHADER_COMMON_VERTEX_LIGHTING, "#define APPLY_VERTEX_LIGHTING\n", "_bvc" },

	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE3, "#define NUM_LIGHTMAPS 4\n#define qf_lmvec01 vec4\n#define qf_lmvec23 vec4\n", "_ls3" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE2, "#define NUM_LIGHTMAPS 3\n#define qf_lmvec01 vec4\n#define qf_lmvec23 vec2\n", "_ls2" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE1, "#define NUM_LIGHTMAPS 2\n#define qf_lmvec01 vec4\n", "_ls1" },
	{ GLSL_SHADER_MATERIAL_LIGHTSTYLE0, "#define NUM_LIGHTMAPS 1\n#define qf_lmvec01 vec2\n", "_ls0" },
	{ GLSL_SHADER_MATERIAL_LIGHTMAP_ARRAYS, "#define LIGHTMAP_ARRAYS\n", "_lmarray" },
	{ GLSL_SHADER_MATERIAL_LIGHTMAP_BICUBIC, "#define LIGHTMAP_BICUBIC\n", "_bicubic" },
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

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_distortion[] =
{
	{ GLSL_SHADER_COMMON_GREYSCALE, "#define APPLY_GREYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },
	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },

	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n"
	  "#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_DISTORTION_DUDV, "#define APPLY_DUDV\n", "_dudv" },
	{ GLSL_SHADER_DISTORTION_EYEDOT, "#define APPLY_EYEDOT\n", "_eyedot" },
	{ GLSL_SHADER_DISTORTION_DISTORTION_ALPHA, "#define APPLY_DISTORTION_ALPHA\n", "_alpha" },
	{ GLSL_SHADER_DISTORTION_REFLECTION, "#define APPLY_REFLECTION\n", "_refl" },
	{ GLSL_SHADER_DISTORTION_REFRACTION, "#define APPLY_REFRACTION\n", "_refr" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_outline[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define QF_NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define QF_NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define QF_NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define QF_NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_OUTLINE_OUTLINES_CUTOFF, "#define APPLY_OUTLINES_CUTOFF\n", "_outcut" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_q3a[] =
{
	{ GLSL_SHADER_COMMON_GREYSCALE, "#define APPLY_GREYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define QF_NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define QF_NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define QF_NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define QF_NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_RGB_DISTANCERAMP, "#define APPLY_RGB_DISTANCERAMP\n", "_rgb_dr" },
	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },

	{ GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP, "#define APPLY_ALPHA_DISTANCERAMP\n", "_alpha_dr" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_DLIGHTS, "#define NUM_DLIGHTS 1\n", "_dl" },
	{ GLSL_SHADER_COMMON_DLIGHT_CUBEFILTER, "#define APPLY_DLIGHT_CUBEFILTER\n", "_dlcf" },
	{ GLSL_SHADER_COMMON_DLIGHT_DIRECTIONAL, "#define APPLY_DLIGHT_DIRECTIONAL\n", "_ortho" },

	{ GLSL_SHADER_COMMON_DRAWFLAT, "#define APPLY_DRAWFLAT\n", "_flat" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },

	{ GLSL_SHADER_COMMON_LIGHTING, "#define APPLY_LIGHTING\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_SOFT_PARTICLE, "#define APPLY_SOFT_PARTICLE\n", "_sp" },

	{ GLSL_SHADER_COMMON_AFUNC_GE128, "#define QF_ALPHATEST(a) { if ((a) < 0.5) discard; }\n", "_afunc_ge128" },
	{ GLSL_SHADER_COMMON_AFUNC_LT128, "#define QF_ALPHATEST(a) { if ((a) >= 0.5) discard; }\n", "_afunc_lt128" },
	{ GLSL_SHADER_COMMON_AFUNC_GT0, "#define QF_ALPHATEST(a) { if ((a) <= 0.0) discard; }\n", "_afunc_gt0" },

	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COMMON_VERTEX_LIGHTING, "#define APPLY_VERTEX_LIGHTING\n", "_bvc" },

	{ GLSL_SHADER_Q3_TC_GEN_CELSHADE, "#define APPLY_TC_GEN_CELSHADE\n", "_tc_cel" },
	{ GLSL_SHADER_Q3_TC_GEN_PROJECTION, "#define APPLY_TC_GEN_PROJECTION\n", "_tc_proj" },
	{ GLSL_SHADER_Q3_TC_GEN_REFLECTION, "#define APPLY_TC_GEN_REFLECTION\n", "_tc_refl" },
	{ GLSL_SHADER_Q3_TC_GEN_ENV, "#define APPLY_TC_GEN_ENV\n", "_tc_env" },
	{ GLSL_SHADER_Q3_TC_GEN_VECTOR, "#define APPLY_TC_GEN_VECTOR\n", "_tc_vec" },
	{ GLSL_SHADER_Q3_TC_GEN_SURROUND, "#define APPLY_TC_GEN_SURROUND\n", "_tc_surr" },

	{ GLSL_SHADER_Q3_LIGHTSTYLE3, "#define NUM_LIGHTMAPS 4\n#define qf_lmvec01 vec4\n#define qf_lmvec23 vec4\n", "_ls3" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE2, "#define NUM_LIGHTMAPS 3\n#define qf_lmvec01 vec4\n#define qf_lmvec23 vec2\n", "_ls2" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE1, "#define NUM_LIGHTMAPS 2\n#define qf_lmvec01 vec4\n", "_ls1" },
	{ GLSL_SHADER_Q3_LIGHTSTYLE0, "#define NUM_LIGHTMAPS 1\n#define qf_lmvec01 vec2\n", "_ls0" },
	{ GLSL_SHADER_Q3_LIGHTMAP_BICUBIC, "#define LIGHTMAP_BICUBIC\n", "_bicubic" },

	{ GLSL_SHADER_Q3_LIGHTMAP_ARRAYS, "#define LIGHTMAP_ARRAYS\n", "_lmarray" },

	{ GLSL_SHADER_Q3_ALPHA_MASK, "#define APPLY_ALPHA_MASK\n", "_alpha_mask" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_celshade[] =
{
	{ GLSL_SHADER_COMMON_GREYSCALE, "#define APPLY_GREYSCALE\n", "_grey" },

	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define QF_NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define QF_NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define QF_NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define QF_NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },

	{ GLSL_SHADER_COMMON_LIGHTING, "#define APPLY_LIGHTING\n", "" },

	{ GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX, "#define APPLY_RGB_ONE_MINUS_VERTEX\n", "_c1-v" },
	{ GLSL_SHADER_COMMON_RGB_GEN_VERTEX, "#define APPLY_RGB_VERTEX\n", "_cv" },

	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX, "#define APPLY_ALPHA_ONE_MINUS_VERTEX\n", "_a1-v" },
	{ GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX, "#define APPLY_ALPHA_VERTEX\n", "_av" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n#define APPLY_FOG_IN 1\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ GLSL_SHADER_COMMON_AFUNC_GE128, "#define QF_ALPHATEST(a) { if ((a) < 0.5) discard; }\n", "_afunc_ge128" },
	{ GLSL_SHADER_COMMON_AFUNC_LT128, "#define QF_ALPHATEST(a) { if ((a) >= 0.5) discard; }\n", "_afunc_lt128" },
	{ GLSL_SHADER_COMMON_AFUNC_GT0, "#define QF_ALPHATEST(a) { if ((a) <= 0.0) discard; }\n", "_afunc_gt0" },

	{ GLSL_SHADER_CELSHADE_DIFFUSE, "#define APPLY_DIFFUSE\n", "_diff" },
	{ GLSL_SHADER_CELSHADE_DECAL, "#define APPLY_DECAL\n", "_decal" },
	{ GLSL_SHADER_CELSHADE_DECAL_ADD, "#define APPLY_DECAL_ADD\n", "_decal" },
	{ GLSL_SHADER_CELSHADE_ENTITY_DECAL, "#define APPLY_ENTITY_DECAL\n", "_edecal" },
	{ GLSL_SHADER_CELSHADE_ENTITY_DECAL_ADD, "#define APPLY_ENTITY_DECAL_ADD\n", "_add" },
	{ GLSL_SHADER_CELSHADE_STRIPES, "#define APPLY_STRIPES\n", "_stripes" },
	{ GLSL_SHADER_CELSHADE_STRIPES_ADD, "#define APPLY_STRIPES_ADD\n", "_stripes_add" },
	{ GLSL_SHADER_CELSHADE_CEL_LIGHT, "#define APPLY_CEL_LIGHT\n", "_light" },
	{ GLSL_SHADER_CELSHADE_CEL_LIGHT_ADD, "#define APPLY_CEL_LIGHT_ADD\n", "_add" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_fog[] =
{
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS4, "#define QF_NUM_BONE_INFLUENCES 4\n", "_bones4" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS3, "#define QF_NUM_BONE_INFLUENCES 3\n", "_bones3" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS2, "#define QF_NUM_BONE_INFLUENCES 2\n", "_bones2" },
	{ GLSL_SHADER_COMMON_BONE_TRANSFORMS1, "#define QF_NUM_BONE_INFLUENCES 1\n", "_bones1" },

	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COMMON_FOG, "#define APPLY_FOG\n", "_fog" },
	{ GLSL_SHADER_COMMON_FOG_RGB, "#define APPLY_FOG_COLOR\n", "_rgb" },

	{ GLSL_SHADER_COMMON_AUTOSPRITE, "#define APPLY_AUTOSPRITE\n", "" },
	{ GLSL_SHADER_COMMON_AUTOSPRITE2, "#define APPLY_AUTOSPRITE2\n", "" },

	{ GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n", "_instanced" },
	{ GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS, "#define APPLY_INSTANCED_TRANSFORMS\n#define APPLY_INSTANCED_ATTRIB_TRANSFORMS\n", "_instanced_va" },

	{ 0, NULL, NULL }
};

static const glsl_feature_t glsl_features_colcorrection[] =
{
	{ GLSL_SHADER_COMMON_SRGB2LINEAR, "#define APPLY_SRGB2LINEAR\n", "_srgb" },
	{ GLSL_SHADER_COMMON_LINEAR2SRB, "#define APPLY_LINEAR2SRGB\n", "_linear" },

	{ GLSL_SHADER_COLOR_CORRECTION_LUT, "#define APPLY_LUT\n", "_lut" },
	{ GLSL_SHADER_COLOR_CORRECTION_HDR, "#define APPLY_HDR\n", "_hdr" },

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
	// GLSL_PROGRAM_TYPE_OUTLINE
	glsl_features_outline,
	// GLSL_PROGRAM_TYPE_UNUSED
	glsl_features_empty,
	// GLSL_PROGRAM_TYPE_Q3A_SHADER
	glsl_features_q3a,
	// GLSL_PROGRAM_TYPE_CELSHADE
	glsl_features_celshade,
	// GLSL_PROGRAM_TYPE_FOG
	glsl_features_fog,
	// GLSL_PROGRAM_TYPE_COLOR_CORRECTION
	glsl_features_colcorrection,
	// GLSL_PROGRAM_TYPE_KAWASE_BLUR
	glsl_features_empty,
};

// ======================================================================================

#ifndef STR_HELPER
#define STR_HELPER( s )                 # s
#define STR_TOSTR( x )                  STR_HELPER( x )
#endif

#define QF_BUILTIN_GLSL_MACROS "" \
	"#define myhalf float\n" \
	"#define myhalf2 vec2\n" \
	"#define myhalf3 vec3\n" \
	"#define myhalf4 vec4\n" \
	"#define qf_lowp_float float\n" \
	"#define qf_lowp_vec2 vec2\n" \
	"#define qf_lowp_vec3 vec3\n" \
	"#define qf_lowp_vec4 vec4\n" \
	"\n"

#define QF_BUILTIN_GLSL_MACROS_GLSL120 "" \
	"#define qf_varying varying\n" \
	"#define qf_flat_varying varying\n" \
	"#ifdef VERTEX_SHADER\n" \
	"# define qf_FrontColor gl_FrontColor\n" \
	"# define qf_attribute attribute\n" \
	"#endif\n" \
	"#ifdef FRAGMENT_SHADER\n" \
	"# define qf_FrontColor gl_Color\n" \
	"# define qf_FragColor gl_FragColor\n" \
	"# define qf_BrightColor gl_FragData[1]\n" \
	"#endif\n" \
	"#define qf_texture texture2D\n" \
        "#define qf_textureLod texture2DLod\n" \
        "#define qf_textureCube textureCube\n" \
        "#define qf_textureArray texture2DArray\n" \
        "#define qf_texture3D texture3D\n" \
        "#define qf_textureOffset(a,b,c,d) texture2DOffset(a,b,ivec2(c,d))\n" \
	"\n"

#define QF_BUILTIN_GLSL_MACROS_GLSL130 "" \
	"precision highp float;\n" \
        "#ifdef VERTEX_SHADER\n" \
        "  out myhalf4 qf_FrontColor;\n" \
        "# define qf_varying out\n" \
        "# define qf_flat_varying flat out\n" \
        "# define qf_attribute in\n" \
        "#endif\n" \
        "#ifdef FRAGMENT_SHADER\n" \
        "  in myhalf4 qf_FrontColor;\n" \
        "  layout( location = 0 ) out myhalf4 qf_FragColor;\n" \
        "  layout( location = 1 ) out myhalf4 qf_BrightColor;\n" \
        "# define qf_varying in\n" \
        "# define qf_flat_varying flat in\n" \
        "#endif\n" \
        "#define qf_texture texture\n" \
        "#define qf_textureCube texture\n" \
        "#define qf_textureLod textureLod\n" \
        "#define qf_textureArray texture\n" \
        "#define qf_texture3D texture\n" \
        "#define qf_textureOffset(a,b,c,d) textureOffset(a,b,ivec2(c,d))\n" \
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
	"#ifndef MAX_UNIFORM_INSTANCES\n" \
	"#define MAX_UNIFORM_INSTANCES " STR_TOSTR( MAX_GLSL_UNIFORM_INSTANCES ) "\n" \
	"#endif\n" \

#define QF_BUILTIN_GLSL_UNIFORMS \
	"uniform vec3 u_QF_ViewOrigin;\n" \
	"uniform mat3 u_QF_ViewAxis;\n" \
	"uniform float u_QF_MirrorSide;\n" \
	"uniform vec3 u_QF_EntityOrigin;\n" \
	"uniform float u_QF_ShaderTime;\n"

#define QF_BUILTIN_GLSL_QUAT_TRANSFORM_OVERLOAD \
	"#ifdef QF_DUAL_QUAT_TRANSFORM_TANGENT\n" \
	"void QF_VertexDualQuatsTransform_Tangent(inout vec4 Position, inout vec3 Normal, inout vec3 Tangent)\n" \
	"#else\n" \
	"void QF_VertexDualQuatsTransform(inout vec4 Position, inout vec3 Normal)\n" \
	"#endif\n" \
	"{\n" \
	"	ivec4 Indices = ivec4(a_BonesIndices * 2.0);\n" \
	"	vec4 DQReal = u_DualQuats[Indices.x];\n" \
	"	vec4 DQDual = u_DualQuats[Indices.x + 1];\n" \
	"#if QF_NUM_BONE_INFLUENCES >= 2\n" \
	"	DQReal *= a_BonesWeights.x;\n" \
	"	DQDual *= a_BonesWeights.x;\n" \
	"	vec4 DQReal1 = u_DualQuats[Indices.y];\n" \
	"	vec4 DQDual1 = u_DualQuats[Indices.y + 1];\n" \
	"	float Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.y;\n" \
	"	DQReal += DQReal1 * Scale;\n" \
	"	DQDual += DQDual1 * Scale;\n" \
	"#if QF_NUM_BONE_INFLUENCES >= 3\n" \
	"	DQReal1 = u_DualQuats[Indices.z];\n" \
	"	DQDual1 = u_DualQuats[Indices.z + 1];\n" \
	"	Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.z;\n" \
	"	DQReal += DQReal1 * Scale;\n" \
	"	DQDual += DQDual1 * Scale;\n" \
	"#if QF_NUM_BONE_INFLUENCES >= 4\n" \
	"	DQReal1 = u_DualQuats[Indices.w];\n" \
	"	DQDual1 = u_DualQuats[Indices.w + 1];\n" \
	"	Scale = mix(-1.0, 1.0, step(0.0, dot(DQReal1, DQReal))) * a_BonesWeights.w;\n" \
	"	DQReal += DQReal1 * Scale;\n" \
	"	DQDual += DQDual1 * Scale;\n" \
	"#endif // QF_NUM_BONE_INFLUENCES >= 4\n" \
	"#endif // QF_NUM_BONE_INFLUENCES >= 3\n" \
	"	float Len = 1.0 / length(DQReal);\n" \
	"	DQReal *= Len;\n" \
	"	DQDual *= Len;\n" \
	"#endif // QF_NUM_BONE_INFLUENCES >= 2\n" \
	"	Position.xyz += (cross(DQReal.xyz, cross(DQReal.xyz, Position.xyz) + Position.xyz * DQReal.w + DQDual.xyz) +\n" \
	"		DQDual.xyz*DQReal.w - DQReal.xyz*DQDual.w) * 2.0;\n" \
	"	Normal += cross(DQReal.xyz, cross(DQReal.xyz, Normal) + Normal * DQReal.w) * 2.0;\n" \
	"#ifdef QF_DUAL_QUAT_TRANSFORM_TANGENT\n" \
	"	Tangent += cross(DQReal.xyz, cross(DQReal.xyz, Tangent) + Tangent * DQReal.w) * 2.0;\n" \
	"#endif\n" \
	"}\n" \
	"\n"

#define QF_BUILTIN_GLSL_QUAT_TRANSFORM \
	"qf_attribute vec4 a_BonesIndices, a_BonesWeights;\n" \
	"uniform vec4 u_DualQuats[MAX_UNIFORM_BONES*2];\n" \
	"\n" \
	QF_BUILTIN_GLSL_QUAT_TRANSFORM_OVERLOAD \
	"#define QF_DUAL_QUAT_TRANSFORM_TANGENT\n" \
	QF_BUILTIN_GLSL_QUAT_TRANSFORM_OVERLOAD \
	"#undef QF_DUAL_QUAT_TRANSFORM_TANGENT\n"

#define QF_BUILTIN_GLSL_INSTANCED_TRANSFORMS \
	"#if defined(APPLY_INSTANCED_ATTRIB_TRANSFORMS)\n" \
	"qf_attribute vec4 a_InstanceQuat, a_InstancePosAndScale;\n" \
	"#elif defined(GL_ARB_draw_instanced) || (defined(GL_ES) && (__VERSION__ >= 300))\n" \
	"uniform vec4 u_InstancePoints[MAX_UNIFORM_INSTANCES*2];\n" \
	"#define a_InstanceQuat u_InstancePoints[gl_InstanceID*2]\n" \
	"#define a_InstancePosAndScale u_InstancePoints[gl_InstanceID*2+1]\n" \
	"#else\n" \
	"uniform vec4 u_InstancePoints[2];\n" \
	"#define a_InstanceQuat u_InstancePoints[0]\n" \
	"#define a_InstancePosAndScale u_InstancePoints[1]\n" \
	"#endif // APPLY_INSTANCED_ATTRIB_TRANSFORMS\n" \
	"\n" \
	"void QF_InstancedTransform(inout vec4 Position, inout vec3 Normal)\n" \
	"{\n" \
	"	Position.xyz = (cross(a_InstanceQuat.xyz,\n" \
	"		cross(a_InstanceQuat.xyz, Position.xyz) + Position.xyz*a_InstanceQuat.w)*2.0 +\n" \
	"		Position.xyz) * a_InstancePosAndScale.w + a_InstancePosAndScale.xyz;\n" \
	"	Normal = cross(a_InstanceQuat.xyz, cross(a_InstanceQuat.xyz, Normal) + Normal*a_InstanceQuat.w)*2.0 + Normal;\n" \
	"}\n" \
	"\n"

// We have to use these #ifdefs here because #defining prototypes
// of these functions to nothing results in a crash on Intel GPUs.
#define QF_BUILTIN_GLSL_TRANSFORM_VERTS \
	"void QF_TransformVerts(inout vec4 Position, inout vec3 Normal, inout vec2 TexCoord)\n" \
	"{\n" \
	"#	ifdef QF_NUM_BONE_INFLUENCES\n" \
	"		QF_VertexDualQuatsTransform(Position, Normal);\n" \
	"#	endif\n" \
	"#	ifdef QF_APPLY_DEFORMVERTS\n" \
	"		QF_DeformVerts(Position, Normal, TexCoord);\n" \
	"#	endif\n" \
	"#	ifdef APPLY_INSTANCED_TRANSFORMS\n" \
	"		QF_InstancedTransform(Position, Normal);\n" \
	"#	endif\n" \
	"}\n" \
	"\n" \
	"void QF_TransformVerts_Tangent(inout vec4 Position, inout vec3 Normal, inout vec3 Tangent, inout vec2 TexCoord)\n" \
	"{\n" \
	"#	ifdef QF_NUM_BONE_INFLUENCES\n" \
	"		QF_VertexDualQuatsTransform_Tangent(Position, Normal, Tangent);\n" \
	"#	endif\n" \
	"#	ifdef QF_APPLY_DEFORMVERTS\n" \
	"		QF_DeformVerts(Position, Normal, TexCoord);\n" \
	"#	endif\n" \
	"#	ifdef APPLY_INSTANCED_TRANSFORMS\n" \
	"		QF_InstancedTransform(Position, Normal);\n" \
	"#	endif\n" \
	"}\n" \
	"\n"

#define QF_GLSL_WAVEFUNCS \
	"\n" \
	QF_GLSL_PI \
	"\n" \
	"#ifndef WAVE_SIN\n" \
	"float QF_WaveFunc_Sin(float x)\n" \
	"{\n" \
	"return sin(fract(x) * M_TWOPI);\n" \
	"}\n" \
	"float QF_WaveFunc_Triangle(float x)\n" \
	"{\n" \
	"x = fract(x);\n" \
	"return step(x, 0.25) * x * 4.0 + (2.0 - 4.0 * step(0.25, x) * step(x, 0.75) * x) + ((step(0.75, x) * x - 0.75) * 4.0 - 1.0);\n" \
	"}\n" \
	"float QF_WaveFunc_Square(float x)\n" \
	"{\n" \
	"return step(fract(x), 0.5) * 2.0 - 1.0;\n" \
	"}\n" \
	"float QF_WaveFunc_Sawtooth(float x)\n" \
	"{\n" \
	"return fract(x);\n" \
	"}\n" \
	"float QF_WaveFunc_InverseSawtooth(float x)\n" \
	"{\n" \
	"return 1.0 - fract(x);\n" \
	"}\n" \
	"\n" \
	"#define WAVE_SIN(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sin((phase)+(time)*(freq))))\n" \
	"#define WAVE_TRIANGLE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Triangle((phase)+(time)*(freq))))\n" \
	"#define WAVE_SQUARE(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Square((phase)+(time)*(freq))))\n" \
	"#define WAVE_SAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_Sawtooth((phase)+(time)*(freq))))\n" \
	"#define WAVE_INVERSESAWTOOTH(time,base,amplitude,phase,freq) (((base)+(amplitude)*QF_WaveFunc_InverseSawtooth((phase)+(time)*(freq))))\n" \
	"#endif\n" \
	"\n"

#define QF_GLSL_MATH \
	"#define QF_LatLong2Norm(ll) vec3(cos((ll).y) * sin((ll).x), sin((ll).y) * sin((ll).x), cos((ll).x))\n" \
	"\n"

/*
* R_GLSLBuildDeformv
*
* Converts some of the Q3A vertex deforms to a GLSL vertex shader.
* Supported deforms are: wave, move, bulge.
* NOTE: Autosprite deforms can only be performed in a geometry shader.
* NULL is returned in case an unsupported deform is passed.
*/
static const char *R_GLSLBuildDeformv( const deformv_t *deformv, int numDeforms ) {
	int i;
	int funcType;
	char tmp[256];
	static char program[40 * 1024];
	static const char * const funcs[] = {
		NULL, "WAVE_SIN", "WAVE_TRIANGLE", "WAVE_SQUARE", "WAVE_SAWTOOTH", "WAVE_INVERSESAWTOOTH", NULL
	};
	static const int numSupportedFuncs = sizeof( funcs ) / sizeof( funcs[0] ) - 1;

	if( !numDeforms ) {
		return NULL;
	}

	program[0] = '\0';
	Q_strncpyz( program,
				"#define QF_APPLY_DEFORMVERTS\n"
				"#if defined(APPLY_AUTOSPRITE) || defined(APPLY_AUTOSPRITE2)\n"
				"qf_attribute vec4 a_SpritePoint;\n"
				"#else\n"
				"#define a_SpritePoint vec4(0.0)\n"
				"#endif\n"
				"\n"
				"#if defined(APPLY_AUTOSPRITE2)\n"
				"qf_attribute vec4 a_SpriteRightUpAxis;\n"
				"#else\n"
				"#define a_SpriteRightUpAxis vec4(0.0)\n"
				"#endif\n"
				"\n"
				"void QF_DeformVerts(inout vec4 Position, inout vec3 Normal, inout vec2 TexCoord)\n"
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

				Q_strncatz( program, va_r( tmp, sizeof( tmp ), "Position.xyz += %s(u_QF_ShaderTime,%f,%f,%f+%f*(Position.x+Position.y+Position.z),%f) * Normal.xyz;\n",
										   funcs[funcType], deformv->func.args[0], deformv->func.args[1], deformv->func.args[2], deformv->func.args[3] ? deformv->args[0] : 0.0, deformv->func.args[3] ),
							sizeof( program ) );
				break;
			case DEFORMV_MOVE:
				funcType = deformv->func.type;
				if( funcType <= SHADER_FUNC_NONE || funcType > numSupportedFuncs || !funcs[funcType] ) {
					return NULL;
				}

				Q_strncatz( program, va_r( tmp, sizeof( tmp ), "Position.xyz += %s(u_QF_ShaderTime,%f,%f,%f,%f) * vec3(%f, %f, %f);\n",
										   funcs[funcType], deformv->func.args[0], deformv->func.args[1], deformv->func.args[2], deformv->func.args[3],
										   deformv->args[0], deformv->args[1], deformv->args[2] ),
							sizeof( program ) );
				break;
			case DEFORMV_BULGE:
				Q_strncatz( program, va_r( tmp, sizeof( tmp ),
										   "t = sin(TexCoord.s * %f + u_QF_ShaderTime * %f);\n"
										   "Position.xyz += max (-1.0 + %f, t) * %f * Normal.xyz;\n",
										   deformv->args[0], deformv->args[2], deformv->args[3], deformv->args[1] ),
							sizeof( program ) );
				break;
			case DEFORMV_AUTOSPRITE:
				Q_strncatz( program,
							"right = (1.0 + step(0.5, TexCoord.s) * -2.0) * u_QF_ViewAxis[1] * u_QF_MirrorSide;\n;"
							"up = (1.0 + step(0.5, TexCoord.t) * -2.0) * u_QF_ViewAxis[2];\n"
							"forward = -1.0 * u_QF_ViewAxis[0];\n"
							"Position.xyz = a_SpritePoint.xyz + (right + up) * a_SpritePoint.w;\n"
							"Normal.xyz = forward;\n"
							"TexCoord.st = vec2(step(0.5, TexCoord.s),step(0.5, TexCoord.t));\n",
							sizeof( program ) );
				break;
			case DEFORMV_AUTOPARTICLE:
				Q_strncatz( program,
							"right = (1.0 + TexCoord.s * -2.0) * u_QF_ViewAxis[1] * u_QF_MirrorSide;\n;"
							"up = (1.0 + TexCoord.t * -2.0) * u_QF_ViewAxis[2];\n"
							"forward = -1.0 * u_QF_ViewAxis[0];\n"
				            // prevent the particle from disappearing at large distances
							"t = dot(a_SpritePoint.xyz + u_QF_EntityOrigin - u_QF_ViewOrigin, u_QF_ViewAxis[0]);\n"
							"t = 1.5 + step(20.0, t) * t * 0.006;\n"
							"Position.xyz = a_SpritePoint.xyz + (right + up) * t * a_SpritePoint.w;\n"
							"Normal.xyz = forward;\n",
							sizeof( program ) );
				break;
			case DEFORMV_AUTOSPRITE2:
				Q_strncatz( program,
				            // local sprite axes
							"right = QF_LatLong2Norm(a_SpriteRightUpAxis.xy) * u_QF_MirrorSide;\n"
							"up = QF_LatLong2Norm(a_SpriteRightUpAxis.zw);\n"

				            // mid of quad to camera vector
							"dist = u_QF_ViewOrigin - u_QF_EntityOrigin - a_SpritePoint.xyz;\n"

				            // filter any longest-axis-parts off the camera-direction
							"forward = normalize(dist - up * dot(dist, up));\n"

				            // the right axis vector as it should be to face the camera
							"newright = cross(up, forward);\n"

				            // rotate the quad vertex around the up axis vector
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

#define PARSER_MAX_STACKDEPTH   16

typedef struct {
	const char *topFile;
	bool error;

	const char **strings;
	size_t maxStrings;
	size_t numStrings;

	char **buffers;
	size_t maxBuffers;
	size_t numBuffers;
} glslParser_t;

/*
* RF_LoadShaderFromFile_r
*/
static bool RF_LoadShaderFromFile_r( glslParser_t *parser, const char *fileName,
									 int stackDepth, int programType, r_glslfeat_t features ) {
	char *fileContents;
	char *token, *line;
	char *ptr, *prevPtr;
	char *startBuf;
	char *trieCache;
	trie_error_t trie_error;
	char tempbuf[MAX_TOKEN_CHARS];

	trie_error = Trie_Find( glsl_cache_trie, fileName, TRIE_EXACT_MATCH, ( void ** )&trieCache );
	if( trie_error != TRIE_OK ) {
		R_LoadFile( fileName, (void **)&fileContents );

		if( fileContents ) {
			trieCache = R_CopyString( fileContents );
		} else {
			trieCache = NULL;
		}
		Trie_Insert( glsl_cache_trie, fileName, trieCache );
	} else {
		if( trieCache ) {
			fileContents = R_CopyString( trieCache );
		} else {
			fileContents = NULL;
		}
	}

	if( !fileContents ) {
		Com_Printf( S_COLOR_YELLOW "Cannot load file '%s'\n", fileName );
		return true;
	}

	if( parser->numBuffers == parser->maxBuffers ) {
		Com_Printf( S_COLOR_YELLOW "numBuffers overflow in '%s' around '%s'\n", parser->topFile, fileName );
		return true;
	}
	parser->buffers[parser->numBuffers++] = fileContents;

	ptr = fileContents;
	startBuf = NULL;

	while( 1 ) {
		bool include, ignore_include;

		prevPtr = ptr;
		token = COM_ParseExt_r( tempbuf, sizeof( tempbuf ), &ptr, true );
		if( !token[0] ) {
			break;
		}

		include = false;
		ignore_include = false;

		if( !Q_stricmp( token, "#include" ) ) {
			include = true;
		} else if( !Q_strnicmp( token, "#include_if(", 12 ) ) {
			include = true;
			token += 12;

			ignore_include = true;

			if( ( !Q_stricmp( token, "APPLY_FOG)" ) && ( features & GLSL_SHADER_COMMON_FOG ) ) ||

				( !Q_stricmp( token, "NUM_DLIGHTS)" ) && ( features & GLSL_SHADER_COMMON_DLIGHTS ) ) ||

				( !Q_stricmp( token, "APPLY_GREYSCALE)" ) && ( features & GLSL_SHADER_COMMON_GREYSCALE ) ) ||

				( ( programType == GLSL_PROGRAM_TYPE_Q3A_SHADER ) && !Q_stricmp( token, "NUM_LIGHTMAPS)" )
				  && ( features & GLSL_SHADER_Q3_LIGHTSTYLE ) ) ||

				( ( programType == GLSL_PROGRAM_TYPE_MATERIAL ) && !Q_stricmp( token, "NUM_LIGHTMAPS)" )
				  && ( features & GLSL_SHADER_MATERIAL_LIGHTSTYLE ) ) ||

				( ( programType == GLSL_PROGRAM_TYPE_MATERIAL ) && !Q_stricmp( token, "APPLY_OFFSETMAPPING)" )
				  && ( features & ( GLSL_SHADER_MATERIAL_OFFSETMAPPING | GLSL_SHADER_MATERIAL_RELIEFMAPPING ) ) ) ||

				( ( programType == GLSL_PROGRAM_TYPE_MATERIAL ) && !Q_stricmp( token, "APPLY_CELSHADING)" )
				  && ( features & GLSL_SHADER_MATERIAL_CELSHADING ) ) ||

				( ( programType == GLSL_PROGRAM_TYPE_MATERIAL ) && !Q_stricmp( token, "APPLY_DIRECTIONAL_LIGHT)" )
				  && ( features & GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT ) )

				) {
				ignore_include = false;
			}
		}

		line = token;
		if( !include || ignore_include ) {
			if( !ignore_include ) {
				if( !startBuf ) {
					startBuf = prevPtr;
				}
			}

			// skip to the end of the line
			token = strchr( ptr, '\n' );
			if( !token ) {
				break;
			}
			ptr = token + 1;
			continue;
		}

		if( startBuf && prevPtr > startBuf ) {
			// cut the string at the beginning of the #include
			*prevPtr = '\0';

			if( parser->numStrings == parser->maxStrings ) {
				Com_Printf( S_COLOR_YELLOW "numStrings overflow in '%s' around '%s'\n", fileName, line );
				return true;
			}
			parser->strings[parser->numStrings++] = startBuf;
			startBuf = NULL;
		}

		// parse #include argument
		token = COM_Parse_r( tempbuf, sizeof( tempbuf ), &ptr );
		if( !token[0] ) {
			Com_Printf( S_COLOR_YELLOW "Syntax error in '%s' around '%s'\n", fileName, line );
			return true;
		}

		if( stackDepth == PARSER_MAX_STACKDEPTH ) {
			Com_Printf( S_COLOR_YELLOW "Include stack overflow in '%s' around '%s'\n", fileName, line );
			return true;
		}

		if( !parser->error ) {
			char tmp[MAX_TOKEN_CHARS + 2];
			char *tempFilename;
			size_t tempFilenameSize;

			// load files from current directory, unless the path starts
			// with the leading "/". in that case, go back to to top directory

			COM_SanitizeFilePath( token );

			tempFilenameSize = strlen( fileName ) + 1 + strlen( token ) + 1;
			tempFilename = ( char * ) R_Malloc( tempFilenameSize );

			if( *token != '/' ) {
				Q_strncpyz( tempFilename, fileName, tempFilenameSize );
				COM_StripFilename( tempFilename );
			} else {
				token++;
				Q_strncpyz( tempFilename, parser->topFile, tempFilenameSize );
				COM_StripFilename( tempFilename );
			}

			Q_strncatz( tempFilename, va_r( tmp, sizeof( tmp ), "%s%s", *tempFilename ? "/" : "", token ), tempFilenameSize );

			parser->error = RF_LoadShaderFromFile_r( parser, tempFilename, stackDepth + 1, programType, features );

			R_Free( tempFilename );

			if( parser->error ) {
				return true;
			}
		}
	}

	if( startBuf ) {
		if( parser->numStrings == parser->maxStrings ) {
			Com_Printf( S_COLOR_YELLOW "numStrings overflow in '%s'\n", fileName );
			return true;
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
static const char **R_ProgramFeatures2Defines( const glsl_feature_t *type_features, r_glslfeat_t features, char *name, size_t size ) {
	int i, p;
	static const char *headers[MAX_DEFINES_FEATURES + 1]; // +1 for NULL safe-guard

	for( i = 0, p = 0; features && type_features && type_features[i].bit; i++ ) {
		if( ( features & type_features[i].bit ) == type_features[i].bit ) {
			headers[p++] = type_features[i].define;
			if( name ) {
				Q_strncatz( name, type_features[i].suffix, size );
			}

			features &= ~type_features[i].bit;

			if( p == MAX_DEFINES_FEATURES ) {
				break;
			}
		}
	}

	if( p ) {
		headers[p] = NULL;
		return headers;
	}

	return NULL;
}

/*
* R_Features2HashKey
*/
static int R_Features2HashKey( r_glslfeat_t features ) {
	int64_t hash = 0x7e53a269;

#define ComputeHash( hash,val ) hash = -1521134295 * hash + ( val ), hash += ( hash << 10 ), hash ^= ( hash >> 6 )

	ComputeHash( hash, (int)( features & 0xFFFFFFFF ) );
	ComputeHash( hash, (int)( ( features >> 32ULL ) & 0xFFFFFFFF ) );

	return hash & ( GLSL_PROGRAMS_HASH_SIZE - 1 );
}

/*
* RP_RegisterProgramBinary
*/
static int RP_RegisterProgramBinary( int type, const char *name, const char *deformsKey,
									 const deformv_t *deforms, int numDeforms, r_glslfeat_t features,
									 int binaryFormat, unsigned binaryLength, void *binary ) {
	unsigned int i;
	int hash;
	int error = 0;
	int shaderTypeIdx, wavefuncsIdx, deformvIdx, dualQuatsIdx, instancedIdx, vTransformsIdx;
	int body_start, num_init_strings;
	glsl_program_t *program;
	char fullName[1024];
	char fileName[1024];
	const char **header;
	char *shaderBuffers[100];
	const char *shaderStrings[MAX_DEFINES_FEATURES + 100];
	char maxBones[100];
	const char *deformv;
	glslParser_t parser;

	if( type <= GLSL_PROGRAM_TYPE_NONE || type >= GLSL_PROGRAM_TYPE_MAXTYPE ) {
		return 0;
	}

	assert( !deforms || deformsKey );

	// default deformsKey to empty string, easier on checking later
	if( !deforms || !deformsKey ) {
		deformsKey = "";
	}

	hash = R_Features2HashKey( features );
	for( program = r_glslprograms_hash[type][hash]; program; program = program->hash_next ) {
		if( ( program->features == features ) && !strcmp( program->deformsKey, deformsKey ) ) {
			return ( ( program - r_glslprograms ) + 1 );
		}
	}

	if( r_numglslprograms == MAX_GLSL_PROGRAMS ) {
		Com_Printf( S_COLOR_YELLOW "RP_RegisterProgram: GLSL programs limit exceeded\n" );
		return 0;
	}

	// if no string was specified, search for an already registered program of the same type
	// with minimal set of features specified
	if( !name ) {
		glsl_program_t *parent;

		parent = NULL;
		for( i = 0; i < r_numglslprograms; i++ ) {
			program = r_glslprograms + i;

			if( ( program->type == type ) && !program->features ) {
				parent = program;
				break;
			}
		}

		if( parent ) {
			if( !name ) {
				name = parent->name;
			}
		} else {
			Com_Printf( S_COLOR_YELLOW "RP_RegisterProgram: failed to find parent for program type %i\n", type );
			return 0;
		}
	}

	program = r_glslprograms + r_numglslprograms++;
	program->object = glCreateProgram();
	if( !program->object ) {
		error = 1;
		goto done;
	}

	if( glConfig.ext.get_program_binary ) {
		glProgramParameteri( program->object, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
	}

	if( binary ) {
		GLint linked;
		glProgramBinary( program->object, binaryFormat, binary, binaryLength );
		glGetProgramiv( program->object, GL_LINK_STATUS, &linked );
		if( !linked ) {
			error = 1;
		}
		goto done;
	}

	Q_strncpyz( fullName, name, sizeof( fullName ) );
	header = R_ProgramFeatures2Defines( glsl_programtypes_features[type], features, fullName, sizeof( fullName ) );

	// load
	//

	ri.Com_DPrintf( "Registering GLSL program %s\n", fullName );

	i = 0;
	if( glConfig.ext.glsl130 ) {
		shaderStrings[i++] = "#version 130\n";
		shaderStrings[i++] = "#extension GL_ARB_explicit_attrib_location : enable\n";
		shaderStrings[i++] = "#define QF_GLSL_VERSION 130\n";
	}
	else {
		shaderStrings[i++] = "#version 120\n";
		shaderStrings[i++] = "#extension GL_EXT_texture_array : enable\n";
		shaderStrings[i++] = "#define QF_GLSL_VERSION 120\n";
	}
	shaderTypeIdx = i;
	shaderStrings[i++] = "\n";
	shaderStrings[i++] = QF_BUILTIN_GLSL_MACROS;
	if( glConfig.ext.glsl130 )
		shaderStrings[i++] = QF_BUILTIN_GLSL_MACROS_GLSL130;
	else
		shaderStrings[i++] = QF_BUILTIN_GLSL_MACROS_GLSL120;
	shaderStrings[i++] = QF_BUILTIN_GLSL_CONSTANTS;
	Q_snprintfz( maxBones, sizeof( maxBones ),
				 "#define MAX_UNIFORM_BONES %i\n", glConfig.maxGLSLBones );
	shaderStrings[i++] = maxBones;
	shaderStrings[i++] = QF_BUILTIN_GLSL_UNIFORMS;
	wavefuncsIdx = i;
	shaderStrings[i++] = QF_GLSL_WAVEFUNCS;
	shaderStrings[i++] = QF_GLSL_MATH;

	if( header ) {
		body_start = i;
		for( ; header[i - body_start] && *header[i - body_start]; i++ )
			shaderStrings[i] = ( char * )header[i - body_start];
	}

	// forward declare QF_DeformVerts
	deformvIdx = i;
	deformv = R_GLSLBuildDeformv( deforms, numDeforms );
	if( !deformv ) {
		deformv = "\n";
	}
	shaderStrings[i++] = deformv;

	dualQuatsIdx = i;
	if( features & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
		shaderStrings[i++] = QF_BUILTIN_GLSL_QUAT_TRANSFORM;
	} else {
		shaderStrings[i++] = "\n";
	}

	instancedIdx = i;
	if( features & ( GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS | GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS ) ) {
		shaderStrings[i++] = QF_BUILTIN_GLSL_INSTANCED_TRANSFORMS;
	} else {
		shaderStrings[i++] = "\n";
	}

	vTransformsIdx = i;
	shaderStrings[i++] = QF_BUILTIN_GLSL_TRANSFORM_VERTS;

	// setup the parser
	num_init_strings = i;
	memset( &parser, 0, sizeof( parser ) );
	parser.topFile = fileName;
	parser.buffers = &shaderBuffers[0];
	parser.maxBuffers = sizeof( shaderBuffers ) / sizeof( shaderBuffers[0] );
	parser.strings = &shaderStrings[num_init_strings];
	parser.maxStrings = sizeof( shaderStrings ) / sizeof( shaderStrings[0] ) - num_init_strings;

	// compile
	//

	RP_BindAttrbibutesLocations( program );

	// vertex shader
	shaderStrings[shaderTypeIdx] = "#define VERTEX_SHADER\n";
	Q_snprintfz( fileName, sizeof( fileName ), "glsl/%s.vert.glsl", name );
	parser.error = false;
	parser.numBuffers = 0;
	parser.numStrings = 0;
	RF_LoadShaderFromFile_r( &parser, parser.topFile, 1, type, features );
	program->vertexShader = RF_CompileShader( program->object, fullName, "vertex", GL_VERTEX_SHADER,
											  shaderStrings, num_init_strings + parser.numStrings );
	for( i = 0; i < parser.numBuffers; i++ )
		R_Free( parser.buffers[i] );
	if( !program->vertexShader ) {
		error = 1;
		goto done;
	}

	// fragment shader
	shaderStrings[shaderTypeIdx] = "#define FRAGMENT_SHADER\n";
	shaderStrings[wavefuncsIdx] = "\n";
	shaderStrings[deformvIdx] = "\n";
	shaderStrings[dualQuatsIdx] = "\n";
	shaderStrings[instancedIdx] = "\n";
	shaderStrings[vTransformsIdx] = "\n";
	Q_snprintfz( fileName, sizeof( fileName ), "glsl/%s.frag.glsl", name );
	parser.error = false;
	parser.numBuffers = 0;
	parser.numStrings = 0;
	RF_LoadShaderFromFile_r( &parser, parser.topFile, 1, type, features );
	program->fragmentShader = RF_CompileShader( program->object, fullName, "fragment", GL_FRAGMENT_SHADER,
												shaderStrings, num_init_strings + parser.numStrings );
	for( i = 0; i < parser.numBuffers; i++ )
		R_Free( parser.buffers[i] );
	if( !program->fragmentShader ) {
		error = 1;
		goto done;
	}

	// link
	GLint linked;

	glLinkProgram( program->object );
	glGetProgramiv( program->object, GL_LINK_STATUS, &linked );
	if( !linked ) {
		char log[8192];

		glGetProgramInfoLog( program->object, sizeof( log ), NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] ) {
			Com_Printf( S_COLOR_YELLOW "Failed to link object for program %s\n", fullName );
			Com_Printf( "%s", log );
			Com_Printf( "\n" );
		}

		error = 1;
		goto done;
	}

done:
	if( error ) {
		RF_DeleteProgram( program );
	}

	program->type = type;
	program->features = features;
	program->name = R_CopyString( name );
	program->deformsKey = R_CopyString( deformsKey ? deformsKey : "" );

	if( !program->hash_next ) {
		program->hash_next = r_glslprograms_hash[type][hash];
		r_glslprograms_hash[type][hash] = program;
	}

	if( program->object ) {
		glUseProgram( program->object );
		RP_GetUniformLocations( program );
	}

	return ( program - r_glslprograms ) + 1;
}

/*
* RP_RegisterProgram
*/
int RP_RegisterProgram( int type, const char *name, const char *deformsKey,
						const deformv_t *deforms, int numDeforms, r_glslfeat_t features ) {
	return RP_RegisterProgramBinary( type, name, deformsKey, deforms, numDeforms,
									 features, 0, 0, NULL );
}

/*
* RP_GetProgramObject
*/
int RP_GetProgramObject( int elem ) {
	if( elem < 1 ) {
		return 0;
	}
	return r_glslprograms[elem - 1].object;
}

/*
* RP_GetProgramBinary
*
* Retrieves the binary from the program object
*/
static void *RP_GetProgramBinary( int elem, int *format, unsigned *length ) {
	void *binary;
	glsl_program_t *program = r_glslprograms + elem - 1;
	GLenum GLFormat;
	GLint GLlength;

	if( !glConfig.ext.get_program_binary ) {
		return NULL;
	}
	if( !program->object ) {
		return NULL;
	}

	GLint linked;
	glGetProgramiv( program->object, GL_LINK_STATUS, &linked );
	if( !linked ) {
		return NULL;
	}

	glGetProgramiv( program->object, GL_PROGRAM_BINARY_LENGTH, &GLlength );
	if( !GLlength ) {
		return NULL;
	}

	binary = R_Malloc( GLlength );
	glGetProgramBinary( program->object, GLlength, NULL, &GLFormat, binary );

	*format = GLFormat;
	*length = GLlength;

	return binary;
}

/*
* RP_ProgramList_f
*/
void RP_ProgramList_f( void ) {
	int i;
	glsl_program_t *program;
	char fullName[1024];

	Com_Printf( "------------------\n" );
	for( i = 0, program = r_glslprograms; i < MAX_GLSL_PROGRAMS; i++, program++ ) {
		if( !program->name ) {
			break;
		}

		Q_strncpyz( fullName, program->name, sizeof( fullName ) );
		R_ProgramFeatures2Defines( glsl_programtypes_features[program->type], program->features, fullName, sizeof( fullName ) );

		Com_Printf( " %3i %s", i + 1, fullName );
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
							  const vec3_t entOrigin, const vec3_t entDist, const uint8_t *entityColor,
							  const uint8_t *constColor, const float *rgbGenFuncArgs, const float *alphaGenFuncArgs,
							  const mat4_t texMatrix, float colorMod ) {
	GLfloat m[9];
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( entOrigin ) {
		if( program->loc.EntityOrigin >= 0 ) {
			glUniform3fv( program->loc.EntityOrigin, 1, entOrigin );
		}
		if( program->loc.builtin.EntityOrigin >= 0 ) {
			glUniform3fv( program->loc.builtin.EntityOrigin, 1, entOrigin );
		}
	}

	if( program->loc.EntityDist >= 0 && entDist ) {
		glUniform3fv( program->loc.EntityDist, 1, entDist );
	}
	if( program->loc.EntityColor >= 0 && entityColor ) {
		glUniform4f( program->loc.EntityColor, entityColor[0] * 1.0 / 255.0, entityColor[1] * 1.0 / 255.0, entityColor[2] * 1.0 / 255.0, entityColor[3] * 1.0 / 255.0 );
	}

	if( program->loc.ShaderTime >= 0 ) {
		glUniform1f( program->loc.ShaderTime, shaderTime );
	}
	if( program->loc.builtin.ShaderTime >= 0 ) {
		glUniform1f( program->loc.builtin.ShaderTime, shaderTime );
	}

	if( program->loc.ConstColor >= 0 && constColor ) {
		glUniform4f( program->loc.ConstColor, constColor[0] * 1.0 / 255.0, constColor[1] * 1.0 / 255.0, constColor[2] * 1.0 / 255.0, constColor[3] * 1.0 / 255.0 );
	}
	if( program->loc.RGBGenFuncArgs >= 0 && rgbGenFuncArgs ) {
		glUniform4fv( program->loc.RGBGenFuncArgs, 1, rgbGenFuncArgs );
	}
	if( program->loc.AlphaGenFuncArgs >= 0 && alphaGenFuncArgs ) {
		glUniform4fv( program->loc.AlphaGenFuncArgs, 1, alphaGenFuncArgs );
	}

	// FIXME: this looks shit...
	if( program->loc.TextureMatrix >= 0 ) {
		m[0] = texMatrix[0], m[1] = texMatrix[4];
		m[2] = texMatrix[1], m[3] = texMatrix[5];
		m[4] = texMatrix[12], m[5] = texMatrix[13];

		glUniform4fv( program->loc.TextureMatrix, 2, m );
	}

	if( program->loc.LightingIntensity >= 0 ) {
		glUniform1f( program->loc.LightingIntensity, 1.0 );
	}
	if( program->loc.ColorMod >= 0 ) {
		glUniform1f( program->loc.ColorMod, colorMod );
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
							float zNear, float zFar ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.ModelViewMatrix >= 0 ) {
		glUniformMatrix4fv( program->loc.ModelViewMatrix, 1, GL_FALSE, modelviewMatrix );
	}
	if( program->loc.ModelViewProjectionMatrix >= 0 ) {
		glUniformMatrix4fv( program->loc.ModelViewProjectionMatrix, 1, GL_FALSE, modelviewProjectionMatrix );
	}

	if( program->loc.ZRange >= 0 ) {
		glUniform2f( program->loc.ZRange, zNear, zFar );
	}

	if( viewOrigin ) {
		if( program->loc.ViewOrigin >= 0 ) {
			glUniform3fv( program->loc.ViewOrigin, 1, viewOrigin );
		}
		if( program->loc.builtin.ViewOrigin >= 0 ) {
			glUniform3fv( program->loc.builtin.ViewOrigin, 1, viewOrigin );
		}
	}

	if( viewAxis ) {
		if( program->loc.ViewAxis >= 0 ) {
			glUniformMatrix3fv( program->loc.ViewAxis, 1, GL_FALSE, viewAxis );
		}
		if( program->loc.builtin.ViewAxis >= 0 ) {
			glUniformMatrix3fv( program->loc.builtin.ViewAxis, 1, GL_FALSE, viewAxis );
		}
	}

	if( program->loc.Viewport >= 0 ) {
		glUniform4iv( program->loc.Viewport, 1, viewport );
	}

	if( program->loc.MirrorSide >= 0 ) {
		glUniform1f( program->loc.MirrorSide, mirrorSide );
	}
	if( program->loc.builtin.MirrorSide >= 0 ) {
		glUniform1f( program->loc.builtin.MirrorSide, mirrorSide );
	}
}

/*
* RP_UpdateBlendMixUniform
*
* The first component corresponds to RGB, the second to ALPHA.
* Whenever the program needs to scale source colors, the mask needs
* to be used in the following manner:
* color *= mix(myhalf4(1.0), myhalf4(scale), u_BlendMix.xxxy);
*/
void RP_UpdateBlendMixUniform( int elem, vec2_t blendMix ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.BlendMix >= 0 ) {
		glUniform2fv( program->loc.BlendMix, 1, blendMix );
	}
}

/*
* RP_UpdateSoftParticlesUniforms
*/
void RP_UpdateSoftParticlesUniforms( int elem, float scale ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.SoftParticlesScale >= 0 ) {
		glUniform1f( program->loc.SoftParticlesScale, scale );
	}
}

/*
* RP_UpdateDiffuseLightUniforms
*/
void RP_UpdateDiffuseLightUniforms( int elem,
									const vec3_t lightDir, const vec4_t lightAmbient, const vec4_t lightDiffuse ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.LightDir >= 0 && lightDir ) {
		glUniform3fv( program->loc.LightDir, 1, lightDir );
	}
	if( program->loc.LightAmbient >= 0 && lightAmbient ) {
		glUniform3f( program->loc.LightAmbient, lightAmbient[0], lightAmbient[1], lightAmbient[2] );
	}
	if( program->loc.LightDiffuse >= 0 && lightDiffuse ) {
		glUniform3f( program->loc.LightDiffuse, lightDiffuse[0], lightDiffuse[1], lightDiffuse[2] );
	}
	if( program->loc.LightingIntensity >= 0 ) {
		glUniform1f( program->loc.LightingIntensity, r_lighting_intensity->value );
	}
}

/*
* RP_UpdateMaterialUniforms
*/
void RP_UpdateMaterialUniforms( int elem,
								float offsetmappingScale, float glossIntensity, float glossExponent ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.GlossFactors >= 0 ) {
		glUniform2f( program->loc.GlossFactors, glossIntensity, glossExponent );
	}
	if( program->loc.OffsetMappingScale >= 0 ) {
		glUniform1f( program->loc.OffsetMappingScale, offsetmappingScale );
	}
}

/*
* RP_UpdateDistortionUniforms
*/
void RP_UpdateDistortionUniforms( int elem, bool frontPlane ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.FrontPlane >= 0 ) {
		glUniform1f( program->loc.FrontPlane, frontPlane ? 1 : -1 );
	}
}

/*
* RP_UpdateTextureUniforms
*/
void RP_UpdateTextureUniforms( int elem, int TexWidth, int TexHeight ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.TextureParams >= 0 ) {
		glUniform4f( program->loc.TextureParams, TexWidth, TexHeight,
						 TexWidth ? 1.0 / TexWidth : 1.0, TexHeight ? 1.0 / TexHeight : 1.0 );
	}
}

/*
* RP_UpdateOutlineUniforms
*/
void RP_UpdateOutlineUniforms( int elem, float projDistance ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.OutlineHeight >= 0 ) {
		glUniform1f( program->loc.OutlineHeight, projDistance );
	}
	if( program->loc.OutlineCutOff >= 0 ) {
		glUniform1f( program->loc.OutlineCutOff, max( 0, r_outlines_cutoff->value ) );
	}
}

/*
* RP_UpdateFogUniforms
*/
void RP_UpdateFogUniforms( int elem, byte_vec4_t color, float clearDist, float opaqueDist, cplane_t *fogPlane, cplane_t *eyePlane, float eyeDist ) {
	GLfloat fog_color[3] = { 0, 0, 0 };
	glsl_program_t *program = r_glslprograms + elem - 1;

	VectorScale( color, ( 1.0 / 255.0 ), fog_color );

	if( program->loc.Fog.Color >= 0 ) {
		glUniform3fv( program->loc.Fog.Color, 1, fog_color );
	}
	if( program->loc.Fog.ScaleAndEyeDist >= 0 ) {
		glUniform2f( program->loc.Fog.ScaleAndEyeDist, 1.0 / ( opaqueDist - clearDist ), eyeDist );
	}
	if( program->loc.Fog.Plane >= 0 ) {
		glUniform4f( program->loc.Fog.Plane, fogPlane->normal[0], fogPlane->normal[1], fogPlane->normal[2], fogPlane->dist );
	}
	if( program->loc.Fog.EyePlane >= 0 ) {
		glUniform4f( program->loc.Fog.EyePlane, eyePlane->normal[0], eyePlane->normal[1], eyePlane->normal[2], eyePlane->dist );
	}
}

/*
* RP_UpdateLightstyleUniforms
*/
void RP_UpdateLightstyleUniforms( int elem, const superLightStyle_t *superLightStyle ) {
	unsigned i;
	GLfloat rgb[3];
	float deluxemapOffset[( MAX_LIGHTMAPS + 3 ) & ( ~3 )] = { 0 };
	glsl_program_t *program = r_glslprograms + elem - 1;

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) {
		VectorClear( rgb );

		if( superLightStyle ) {
			if( superLightStyle->lightmapStyles[i] != 255 ) {
				VectorCopy( rsc.lightStyles[superLightStyle->lightmapStyles[i]].rgb, rgb );
			} else if( superLightStyle->vertexStyles[i] != 255 ) {
				VectorCopy( rsc.lightStyles[superLightStyle->vertexStyles[i]].rgb, rgb );
			}
			if( program->loc.DeluxemapOffset >= 0 ) {
				deluxemapOffset[i] = superLightStyle->stOffset[i][0];
			}
		}

		if( program->loc.LightstyleColor[i] >= 0 ) {
			glUniform3fv( program->loc.LightstyleColor[i], 1, rgb );
		} else if( program->loc.DeluxemapOffset < 0 ) {
			break;
		}
	}
	
	if( i && ( program->loc.DeluxemapOffset >= 0 ) ) {
		glUniform4fv( program->loc.DeluxemapOffset, ( i + 3 ) / 4, deluxemapOffset );
	}
}

/*
* RP_UpdateRealtimeLightsUniforms
*/
void RP_UpdateRealtimeLightsUniforms( int elem, const vec3_t lightVec, const mat4_t objectToLightMatrix,
	unsigned int numRtLights, rtlight_t **rtlights, unsigned numSurfs, unsigned *surfRtLightBits ) {
	unsigned i;
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( numRtLights ) {	
		vec4_t shaderColor = { 0 };

		for( i = 0; i < numRtLights; i++ ) {
			const rtlight_t *rl = rtlights[i];

			if( program->loc.DynamicLightsDiffuseAndInvRadius < 0 ) {
				break;
			}

			if( program->loc.DynamicLightsMatrix >= 0 ) {
				glUniformMatrix4fv( program->loc.DynamicLightsMatrix, 1, GL_FALSE, objectToLightMatrix );
			}

			if( glConfig.sSRGB ) {
				shaderColor[0] = rl->linearColor[0];
				shaderColor[1] = rl->linearColor[1];
				shaderColor[2] = rl->linearColor[2];
				shaderColor[3] = rl->linearColor[3];
			} else {
				shaderColor[0] = rl->color[0];
				shaderColor[1] = rl->color[1];
				shaderColor[2] = rl->color[2];
				shaderColor[3] = rl->color[3];
			}

			if( rl->style >= 0 && rl->style < MAX_LIGHTSTYLES ) {
				float *rgb = rsc.lightStyles[rl->style].rgb;
				shaderColor[0] *= rgb[0];
				shaderColor[1] *= rgb[1];
				shaderColor[2] *= rgb[2];
			}

			// DynamicLightsDiffuseAndInvRadius is transposed for SIMD, but it's still 4x4
			glUniform4fv( program->loc.DynamicLightsDiffuseAndInvRadius, 1, shaderColor );

			if( program->loc.DynamicLightVector >= 0 ) {
				glUniform3fv( program->loc.DynamicLightVector, 1, lightVec );
			}
		}
	}
}

/*
* RP_UpdateTexGenUniforms
*/
void RP_UpdateTexGenUniforms( int elem, const mat4_t reflectionMatrix, const mat4_t vectorMatrix ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.ReflectionTexMatrix >= 0 ) {
		mat3_t m;
		memcpy( &m[0], &reflectionMatrix[0], 3 * sizeof( vec_t ) );
		memcpy( &m[3], &reflectionMatrix[4], 3 * sizeof( vec_t ) );
		memcpy( &m[6], &reflectionMatrix[8], 3 * sizeof( vec_t ) );
		glUniformMatrix3fv( program->loc.ReflectionTexMatrix, 1, GL_FALSE, m );
	}
	if( program->loc.VectorTexMatrix >= 0 ) {
		glUniformMatrix4fv( program->loc.VectorTexMatrix, 1, GL_FALSE, vectorMatrix );
	}
}

/*
* RP_UpdateBonesUniforms
*
* Set uniform values for animation dual quaternions
*/
void RP_UpdateBonesUniforms( int elem, unsigned int numBones, dualquat_t *animDualQuat ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( numBones > glConfig.maxGLSLBones ) {
		return;
	}
	if( program->loc.DualQuats < 0 ) {
		return;
	}
	glUniform4fv( program->loc.DualQuats, numBones * 2, &animDualQuat[0][0] );
}

/*
* RP_UpdateInstancesUniforms
*
* Set uniform values for instance points (quaternion + xyz + scale)
*/
void RP_UpdateInstancesUniforms( int elem, unsigned int numInstances, instancePoint_t *instances ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( numInstances > MAX_GLSL_UNIFORM_INSTANCES ) {
		numInstances = MAX_GLSL_UNIFORM_INSTANCES;
	}
	if( program->loc.InstancePoints < 0 ) {
		return;
	}
	glUniform4fv( program->loc.InstancePoints, numInstances * 2, &instances[0][0] );
}

/*
* RP_UpdateColorCorrectionUniforms
*/
void RP_UpdateColorCorrectionUniforms( int elem, float hdrGamma, float hdrExposure ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.hdrGamma >= 0 ) {
		glUniform1f( program->loc.hdrGamma, hdrGamma );
	}
	if( program->loc.hdrExposure >= 0 ) {
		glUniform1f( program->loc.hdrExposure, hdrExposure );
	}
}

/*
* RP_UpdateDrawFlatUniforms
*/
void RP_UpdateDrawFlatUniforms( int elem, const vec3_t wallColor, const vec3_t floorColor ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.WallColor >= 0 ) {
		glUniform3f( program->loc.WallColor, wallColor[0], wallColor[1], wallColor[2] );
	}
	if( program->loc.FloorColor >= 0 ) {
		glUniform3f( program->loc.FloorColor, floorColor[0], floorColor[1], floorColor[2] );
	}
}

/*
* RP_UpdateKawaseUniforms
*/
void RP_UpdateKawaseUniforms( int elem, int TexWidth, int TexHeight, int iteration ) {
	glsl_program_t *program = r_glslprograms + elem - 1;

	if( program->loc.TextureParams >= 0 ) {
		glUniform4f( program->loc.TextureParams,
						 TexWidth ? 1.0 / TexWidth : 1.0, TexHeight ? 1.0 / TexHeight : 1.0, (float)iteration, 1.0 );
	}
}

/*
* RP_GetUniformLocations
*/
static void RP_GetUniformLocations( glsl_program_t *program ) {
	memset( &program->loc, -1, sizeof( program->loc ) );

	program->loc.ModelViewMatrix = glGetUniformLocation( program->object, "u_ModelViewMatrix" );
	program->loc.ModelViewProjectionMatrix = glGetUniformLocation( program->object, "u_ModelViewProjectionMatrix" );

	program->loc.ZRange = glGetUniformLocation( program->object, "u_ZRange" );

	program->loc.ViewOrigin = glGetUniformLocation( program->object, "u_ViewOrigin" );
	program->loc.ViewAxis = glGetUniformLocation( program->object, "u_ViewAxis" );

	program->loc.MirrorSide = glGetUniformLocation( program->object, "u_MirrorSide" );

	program->loc.Viewport = glGetUniformLocation( program->object, "u_Viewport" );

	program->loc.LightDir = glGetUniformLocation( program->object, "u_LightDir" );
	program->loc.LightAmbient = glGetUniformLocation( program->object, "u_LightAmbient" );
	program->loc.LightDiffuse = glGetUniformLocation( program->object, "u_LightDiffuse" );
	program->loc.LightingIntensity = glGetUniformLocation( program->object, "u_LightingIntensity" );

	program->loc.TextureMatrix = glGetUniformLocation( program->object, "u_TextureMatrix" );

	int locBaseTexture = glGetUniformLocation( program->object, "u_BaseTexture" );
	int locNormalmapTexture = glGetUniformLocation( program->object, "u_NormalmapTexture" );
	int locGlossTexture = glGetUniformLocation( program->object, "u_GlossTexture" );
	int locDecalTexture = glGetUniformLocation( program->object, "u_DecalTexture" );
	int locEntityDecalTexture = glGetUniformLocation( program->object, "u_EntityDecalTexture" );

	int locDuDvMapTexture = glGetUniformLocation( program->object, "u_DuDvMapTexture" );
	int locReflectionTexture = glGetUniformLocation( program->object, "u_ReflectionTexture" );
	int locRefractionTexture = glGetUniformLocation( program->object, "u_RefractionTexture" );

	int locCelShadeTexture = glGetUniformLocation( program->object, "u_CelShadeTexture" );
	int locCelLightTexture = glGetUniformLocation( program->object, "u_CelLightTexture" );
	int locDiffuseTexture = glGetUniformLocation( program->object, "u_DiffuseTexture" );
	int locStripesTexture = glGetUniformLocation( program->object, "u_StripesTexture" );

	int locDepthTexture = glGetUniformLocation( program->object, "u_DepthTexture" );

	int locBlueNoiseTexture = glGetUniformLocation( program->object, "u_BlueNoiseTexture" );
	int locBlueNoiseTextureSize = glGetUniformLocation( program->object, "u_BlueNoiseTextureSize" );

	int locColorLUT = glGetUniformLocation( program->object, "u_ColorLUT" );
	int locCubeFilter = glGetUniformLocation( program->object, "u_CubeFilter" );

	program->loc.DeluxemapOffset = glGetUniformLocation( program->object, "u_DeluxemapOffset" );

	int locLightmapTexture[MAX_LIGHTMAPS];
	for( unsigned int i = 0; i < MAX_LIGHTMAPS; i++ ) {
		char tmp[1024];
		// arrays of samplers are broken on ARM Mali so get u_LightmapTexture%i instead of u_LightmapTexture[%i]
		locLightmapTexture[i] = glGetUniformLocation( program->object,
			va_r( tmp, sizeof( tmp ), "u_LightmapTexture%i", i ) );

		program->loc.LightstyleColor[i] = glGetUniformLocation( program->object,
			va_r( tmp, sizeof( tmp ), "u_LightstyleColor[%i]", i ) );

		if( locLightmapTexture[i] < 0 && program->loc.LightstyleColor[i] < 0 ) {
			break;
		}
	}

	program->loc.GlossFactors = glGetUniformLocation( program->object, "u_GlossFactors" );

	program->loc.OffsetMappingScale = glGetUniformLocation( program->object, "u_OffsetMappingScale" );

	program->loc.OutlineHeight = glGetUniformLocation( program->object, "u_OutlineHeight" );
	program->loc.OutlineCutOff = glGetUniformLocation( program->object, "u_OutlineCutOff" );

	program->loc.FrontPlane = glGetUniformLocation( program->object, "u_FrontPlane" );

	program->loc.TextureParams = glGetUniformLocation( program->object, "u_TextureParams" );

	program->loc.EntityDist = glGetUniformLocation( program->object, "u_EntityDist" );
	program->loc.EntityOrigin = glGetUniformLocation( program->object, "u_EntityOrigin" );
	program->loc.EntityColor = glGetUniformLocation( program->object, "u_EntityColor" );
	program->loc.ConstColor = glGetUniformLocation( program->object, "u_ConstColor" );
	program->loc.RGBGenFuncArgs = glGetUniformLocation( program->object, "u_RGBGenFuncArgs" );
	program->loc.AlphaGenFuncArgs = glGetUniformLocation( program->object, "u_AlphaGenFuncArgs" );

	program->loc.Fog.Plane = glGetUniformLocation( program->object, "u_FogPlane" );
	program->loc.Fog.Color = glGetUniformLocation( program->object, "u_FogColor" );
	program->loc.Fog.ScaleAndEyeDist = glGetUniformLocation( program->object, "u_FogScaleAndEyeDist" );
	program->loc.Fog.EyePlane = glGetUniformLocation( program->object, "u_FogEyePlane" );

	program->loc.ShaderTime = glGetUniformLocation( program->object, "u_ShaderTime" );

	program->loc.ReflectionTexMatrix = glGetUniformLocation( program->object, "u_ReflectionTexMatrix" );
	program->loc.VectorTexMatrix = glGetUniformLocation( program->object, "u_VectorTexMatrix" );

	program->loc.builtin.ViewOrigin = glGetUniformLocation( program->object, "u_QF_ViewOrigin" );
	program->loc.builtin.ViewAxis = glGetUniformLocation( program->object, "u_QF_ViewAxis" );
	program->loc.builtin.MirrorSide = glGetUniformLocation( program->object, "u_QF_MirrorSide" );
	program->loc.builtin.EntityOrigin = glGetUniformLocation( program->object, "u_QF_EntityOrigin" );
	program->loc.builtin.ShaderTime = glGetUniformLocation( program->object, "u_QF_ShaderTime" );

	// dynamic lights
	program->loc.DynamicLightsDiffuseAndInvRadius = glGetUniformLocation( program->object, "u_DlightDiffuseAndInvRadius" );
	program->loc.DynamicLightsMatrix = glGetUniformLocation( program->object, "u_DlightMatrix" );
	program->loc.DynamicLightVector = glGetUniformLocation( program->object, "u_DlightVector" );

	program->loc.BlendMix = glGetUniformLocation( program->object, "u_BlendMix" );
	program->loc.ColorMod = glGetUniformLocation( program->object, "u_ColorMod" );

	program->loc.SoftParticlesScale = glGetUniformLocation( program->object, "u_SoftParticlesScale" );

	program->loc.DualQuats = glGetUniformLocation( program->object, "u_DualQuats" );

	program->loc.InstancePoints = glGetUniformLocation( program->object, "u_InstancePoints" );

	program->loc.WallColor = glGetUniformLocation( program->object, "u_WallColor" );
	program->loc.FloorColor = glGetUniformLocation( program->object, "u_FloorColor" );

	program->loc.hdrGamma = glGetUniformLocation( program->object, "u_HDRGamma" );
	program->loc.hdrExposure = glGetUniformLocation( program->object, "u_HDRExposure" );

	if( locBaseTexture >= 0 ) {
		glUniform1i( locBaseTexture, 0 );
	}
	if( locDuDvMapTexture >= 0 ) {
		glUniform1i( locDuDvMapTexture, 0 );
	}

	if( locNormalmapTexture >= 0 ) {
		glUniform1i( locNormalmapTexture, 1 );
	}
	if( locGlossTexture >= 0 ) {
		glUniform1i( locGlossTexture, 2 );
	}
	if( locDecalTexture >= 0 ) {
		glUniform1i( locDecalTexture, 3 );
	}
	if( locEntityDecalTexture >= 0 ) {
		glUniform1i( locEntityDecalTexture, 4 );
	}

	if( locReflectionTexture >= 0 ) {
		glUniform1i( locReflectionTexture, 2 );
	}
	if( locRefractionTexture >= 0 ) {
		glUniform1i( locRefractionTexture, 3 );
	}

//	if( locBaseTexture >= 0 )
//		qglUniform1i( locBaseTexture, 0 );
	if( locCelShadeTexture >= 0 ) {
		glUniform1i( locCelShadeTexture, 1 );
	}
	if( locDiffuseTexture >= 0 ) {
		glUniform1i( locDiffuseTexture, 2 );
	}
//	if( locDecalTexture >= 0 )
//		qglUniform1i( locDecalTexture, 3 );
//	if( locEntityDecalTexture >= 0 )
//		qglUniform1i( locEntityDecalTexture, 4 );
	if( locStripesTexture >= 0 ) {
		glUniform1i( locStripesTexture, 5 );
	}
	if( locCelLightTexture >= 0 ) {
		glUniform1i( locCelLightTexture, 6 );
	}

	if( locDepthTexture >= 0 ) {
		glUniform1i( locDepthTexture, 3 );
	}

	for( unsigned int i = 0; i < MAX_LIGHTMAPS && locLightmapTexture[i] >= 0; i++ )
		glUniform1i( locLightmapTexture[i], i + 4 );

	if( locBlueNoiseTexture >= 0 ) {
		assert( locBlueNoiseTextureSize >= 0 );
		glUniform1i( locBlueNoiseTexture, 7 );
		glUniform2f( locBlueNoiseTextureSize, BLUENOISE_TEXTURE_SIZE, BLUENOISE_TEXTURE_SIZE );
	}

	if( locColorLUT >= 0 ) {
		glUniform1i( locColorLUT, 1 );
	}

	if( locCubeFilter >= 0 ) {
		glUniform1i( locCubeFilter, 5 );
	}
}

/*
* RP_BindAttrbibutesLocations
*/
static void RP_BindAttrbibutesLocations( glsl_program_t *program ) {
	glBindAttribLocation( program->object, VATTRIB_POSITION, "a_Position" );
	glBindAttribLocation( program->object, VATTRIB_SVECTOR, "a_SVector" );
	glBindAttribLocation( program->object, VATTRIB_NORMAL, "a_Normal" );
	glBindAttribLocation( program->object, VATTRIB_COLOR0, "a_Color" );
	glBindAttribLocation( program->object, VATTRIB_TEXCOORDS, "a_TexCoord" );

	glBindAttribLocation( program->object, VATTRIB_SPRITEPOINT, "a_SpritePoint" );
	glBindAttribLocation( program->object, VATTRIB_SVECTOR, "a_SpriteRightUpAxis" );

	glBindAttribLocation( program->object, VATTRIB_BONESINDICES, "a_BonesIndices" );
	glBindAttribLocation( program->object, VATTRIB_BONESWEIGHTS, "a_BonesWeights" );

	glBindAttribLocation( program->object, VATTRIB_LMCOORDS01, "a_LightmapCoord01" );
	glBindAttribLocation( program->object, VATTRIB_LMCOORDS23, "a_LightmapCoord23" );

	glBindAttribLocation( program->object, VATTRIB_SURFINDEX, "a_SurfaceIndex" );

	glBindAttribLocation( program->object, VATTRIB_LMLAYERS0123, "a_LightmapLayer0123" );

	glBindAttribLocation( program->object, VATTRIB_INSTANCE_QUAT, "a_InstanceQuat" );
	glBindAttribLocation( program->object, VATTRIB_INSTANCE_XYZS, "a_InstancePosAndScale" );

	/* glBindFragDataLocation( program->object, 0, "qf_FragColor" ); */
	/* glBindFragDataLocation( program->object, 1, "qf_BrightColor" ); */
}

/*
* RP_Shutdown
*/
void RP_Shutdown( void ) {
	unsigned int i;
	glsl_program_t *program;

	glUseProgram( 0 );

	for( i = 0, program = r_glslprograms; i < r_numglslprograms; i++, program++ ) {
		RF_DeleteProgram( program );
	}

	Trie_Destroy( glsl_cache_trie );
	glsl_cache_trie = NULL;

	r_numglslprograms = 0;
	r_glslprograms_initialized = false;
}
