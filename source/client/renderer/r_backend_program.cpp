/*
Copyright (C) 2011 Victor Luchits

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

#include "r_local.h"
#include "r_backend_local.h"

#define FTABLE_SIZE_POW 12
#define FTABLE_SIZE ( 1 << FTABLE_SIZE_POW )
#define FTABLE_CLAMP( x ) ( ( (int)( ( x ) * FTABLE_SIZE ) & ( FTABLE_SIZE - 1 ) ) )
#define FTABLE_EVALUATE( table, x ) ( ( table )[FTABLE_CLAMP( fmod( x, 1.0 ) )] )

#define DRAWFLAT() ( ( rb.currentModelType == mod_brush ) && ( rb.renderFlags & RF_DRAWFLAT ) && !( rb.currentShader->flags & SHADER_NODRAWFLAT ) )

enum {
	BUILTIN_GLSLPASS_OUTLINE,
	BUILTIN_GLSLPASS_SKYBOX,
	MAX_BUILTIN_GLSLPASSES
};

static float rb_sintable[FTABLE_SIZE];
static float rb_triangletable[FTABLE_SIZE];
static float rb_squaretable[FTABLE_SIZE];
static float rb_sawtoothtable[FTABLE_SIZE];
static float rb_inversesawtoothtable[FTABLE_SIZE];

#define NOISE_SIZE  256
#define NOISE_VAL( a )    rb_noiseperm[( a ) & ( NOISE_SIZE - 1 )]
#define NOISE_INDEX( x, y, z, t ) NOISE_VAL( x + NOISE_VAL( y + NOISE_VAL( z + NOISE_VAL( t ) ) ) )
#define NOISE_LERP( a, b, w ) ( a * ( 1.0f - w ) + b * w )

static float rb_noisetable[NOISE_SIZE];
static int rb_noiseperm[NOISE_SIZE];

static shaderpass_t r_GLSLpasses[MAX_BUILTIN_GLSLPASSES];

static int RB_GetShaderpassState( int state );

static void RB_RenderMeshGLSL_Material( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Outline( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Q3AShader( const shaderpass_t *pass, r_glslfeat_t programFeatures );

/*
* RB_InitBuiltinPasses
*/
static void RB_InitBuiltinPasses( void ) {
	shaderpass_t *pass;

	// init optional GLSL program passes
	memset( r_GLSLpasses, 0, sizeof( r_GLSLpasses ) );

	// outlines
	pass = &r_GLSLpasses[BUILTIN_GLSLPASS_OUTLINE];
	pass->flags = GLSTATE_DEPTHWRITE;
	pass->rgbgen.type = RGB_GEN_OUTLINE;
	pass->alphagen.type = ALPHA_GEN_OUTLINE;
	pass->tcgen = TC_GEN_NONE;
	pass->program_type = GLSL_PROGRAM_TYPE_OUTLINE;

	// skybox
	pass = &r_GLSLpasses[BUILTIN_GLSLPASS_SKYBOX];
	pass->program_type = GLSL_PROGRAM_TYPE_Q3A_SHADER;
	pass->tcgen = TC_GEN_BASE;
	pass->rgbgen.type = RGB_GEN_IDENTITY;
	pass->alphagen.type = ALPHA_GEN_IDENTITY;
}

/*
* RB_InitShading
*/
void RB_InitShading( void ) {
	int i;
	float t;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ ) {
		t = (float)i / (float)FTABLE_SIZE;

		rb_sintable[i] = sin( t * M_TWOPI );

		if( t < 0.25 ) {
			rb_triangletable[i] = t * 4.0;
		} else if( t < 0.75 ) {
			rb_triangletable[i] = 2 - 4.0 * t;
		} else {
			rb_triangletable[i] = ( t - 0.75 ) * 4.0 - 1.0;
		}

		if( t < 0.5 ) {
			rb_squaretable[i] = 1.0f;
		} else {
			rb_squaretable[i] = -1.0f;
		}

		rb_sawtoothtable[i] = t;
		rb_inversesawtoothtable[i] = 1.0 - t;
	}

	// init the noise table
	Q_InitNoiseTable( 1001, rb_noisetable, rb_noiseperm );

	RB_InitBuiltinPasses();
}

/*
* RB_FastSin
*/
static inline float RB_FastSin( float t ) {
	return FTABLE_EVALUATE( rb_sintable, t );
}

/*
* RB_TableForFunc
*/
static float *RB_TableForFunc( unsigned int func ) {
	switch( func ) {
		case SHADER_FUNC_SIN:
			return rb_sintable;
		case SHADER_FUNC_TRIANGLE:
			return rb_triangletable;
		case SHADER_FUNC_SQUARE:
			return rb_squaretable;
		case SHADER_FUNC_SAWTOOTH:
			return rb_sawtoothtable;
		case SHADER_FUNC_INVERSESAWTOOTH:
			return rb_inversesawtoothtable;
		default:
			break;
	}

	return rb_sintable;  // default to sintable
}

/*
* RB_BackendGetNoiseValue
*/
static float RB_BackendGetNoiseValue( float x, float y, float z, float t ) {
	return Q_GetNoiseValueFromTable( rb_noisetable, rb_noiseperm, x, y, z, t );
}

/*
* RB_ApplyTCMods
*/
void RB_ApplyTCMods( const shaderpass_t *pass, mat4_t result ) {
	unsigned i;
	const float *table;
	double t1, t2, sint, cost;
	mat4_t m1, m2;
	const tcmod_t *tcmod;

	for( i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++ ) {
		switch( tcmod->type ) {
			case TC_MOD_ROTATE:
				cost = tcmod->args[0] * rb.currentShaderTime;
				sint = RB_FastSin( cost );
				cost = RB_FastSin( cost + 0.25 );
				m2[0] =  cost, m2[1] = sint, m2[12] =  0.5f * ( sint - cost + 1 );
				m2[4] = -sint, m2[5] = cost, m2[13] = -0.5f * ( sint + cost - 1 );
				Matrix4_Copy2D( result, m1 );
				Matrix4_Multiply2D( m2, m1, result );
				break;
			case TC_MOD_SCALE:
				Matrix4_Scale2D( result, tcmod->args[0], tcmod->args[1] );
				break;
			case TC_MOD_TURB:
				t1 = ( 1.0 / 4.0 );
				t2 = tcmod->args[2] + rb.currentShaderTime * tcmod->args[3];
				Matrix4_Scale2D( result,
								 1 + ( tcmod->args[1] * RB_FastSin( t2 ) + tcmod->args[0] ) * t1,
								 1 + ( tcmod->args[1] * RB_FastSin( t2 + 0.25 ) + tcmod->args[0] ) * t1 );
				break;
			case TC_MOD_STRETCH:
				table = RB_TableForFunc( tcmod->args[0] );
				t2 = tcmod->args[3] + rb.currentShaderTime * tcmod->args[4];
				t1 = FTABLE_EVALUATE( table, t2 ) * tcmod->args[2] + tcmod->args[1];
				t1 = t1 ? 1.0f / t1 : 1.0f;
				t2 = 0.5f - 0.5f * t1;
				Matrix4_Stretch2D( result, t1, t2 );
				break;
			case TC_MOD_SCROLL:
				t1 = tcmod->args[0] * rb.currentShaderTime;
				t2 = tcmod->args[1] * rb.currentShaderTime;
				t1 = t1 - floor( t1 );
				t2 = t2 - floor( t2 );
				Matrix4_Translate2D( result, t1, t2 );
				break;
			case TC_MOD_TRANSFORM:
				m2[0] = tcmod->args[0], m2[1] = tcmod->args[2], m2[12] = tcmod->args[4],
				m2[5] = tcmod->args[1], m2[4] = tcmod->args[3], m2[13] = tcmod->args[5];
				Matrix4_Copy2D( result, m1 );
				Matrix4_Multiply2D( m2, m1, result );
				break;
			default:
				break;
		}
	}
}

/*
* RB_GetShaderpassColor
*/
void RB_GetShaderpassColor( const shaderpass_t *pass, byte_vec4_t rgba_, float *colorMod ) {
	int c;
	int rgba[4];
	double temp;
	float *table, a;
	vec3_t v;
	const shaderfunc_t *rgbgenfunc = &pass->rgbgen.func;
	const shaderfunc_t *alphagenfunc = &pass->alphagen.func;

	*colorMod = 1;
	Vector4Set( rgba, 255, 255, 255, 255 );

	switch( pass->rgbgen.type ) {
		case RGB_GEN_IDENTITY:
			break;
		case RGB_GEN_CONST:
			rgba[0] = ( int )( pass->rgbgen.args[0] * 255.0f );
			rgba[1] = ( int )( pass->rgbgen.args[1] * 255.0f );
			rgba[2] = ( int )( pass->rgbgen.args[2] * 255.0f );
			break;
		case RGB_GEN_ENTITYWAVE:
		case RGB_GEN_WAVE:
		case RGB_GEN_CUSTOMWAVE:
			if( rgbgenfunc->type == SHADER_FUNC_NONE ) {
				temp = 1;
			} else if( rgbgenfunc->type == SHADER_FUNC_RAMP ) {
				break;
			} else if( rgbgenfunc->args[1] == 0 ) {
				temp = rgbgenfunc->args[0];
			} else {
				if( rgbgenfunc->type == SHADER_FUNC_NOISE ) {
					temp = RB_BackendGetNoiseValue( 0, 0, 0, ( rb.currentShaderTime + rgbgenfunc->args[2] ) * rgbgenfunc->args[3] );
				} else {
					table = RB_TableForFunc( rgbgenfunc->type );
					temp = rb.currentShaderTime * rgbgenfunc->args[3] + rgbgenfunc->args[2];
					temp = FTABLE_EVALUATE( table, temp ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
				}
				temp = temp * rgbgenfunc->args[1] + rgbgenfunc->args[0];
			}

			if( pass->rgbgen.type == RGB_GEN_ENTITYWAVE ) {
				VectorSet( v,
						   rb.entityColor[0] * ( 1.0 / 255.0 ),
						   rb.entityColor[1] * ( 1.0 / 255.0 ),
						   rb.entityColor[2] * ( 1.0 / 255.0 ) );
			} else if( pass->rgbgen.type == RGB_GEN_CUSTOMWAVE ) {
				c = R_GetCustomColor( (int)pass->rgbgen.args[0] );
				VectorSet( v,
						   COLOR_R( c ) * ( 1.0 / 255.0 ),
						   COLOR_G( c ) * ( 1.0 / 255.0 ),
						   COLOR_B( c ) * ( 1.0 / 255.0 ) );
			} else {
				VectorCopy( pass->rgbgen.args, v );
			}

			a = v[0]; rgba[0] = ( int )( a * 255.0f );
			a = v[1]; rgba[1] = ( int )( a * 255.0f );
			a = v[2]; rgba[2] = ( int )( a * 255.0f );
			*colorMod = (float)temp;
			break;
		case RGB_GEN_OUTLINE:
			rgba[0] = rb.entityOutlineColor[0];
			rgba[1] = rb.entityOutlineColor[1];
			rgba[2] = rb.entityOutlineColor[2];
			break;
		case RGB_GEN_ONE_MINUS_ENTITY:
			rgba[0] = 255 - rb.entityColor[0];
			rgba[1] = 255 - rb.entityColor[1];
			rgba[2] = 255 - rb.entityColor[2];
			break;
		case RGB_GEN_ENVIRONMENT:
			rgba[0] = mapConfig.environmentColor[0];
			rgba[1] = mapConfig.environmentColor[1];
			rgba[2] = mapConfig.environmentColor[2];
			break;
		default:
			break;
	}

	switch( pass->alphagen.type ) {
		case ALPHA_GEN_IDENTITY:
			break;
		case ALPHA_GEN_CONST:
			rgba[3] = ( int )( pass->alphagen.args[0] * 255.0f );
			break;
		case ALPHA_GEN_WAVE:
			if( !alphagenfunc || alphagenfunc->type == SHADER_FUNC_NONE ) {
				a = 1;
			} else if( alphagenfunc->type == SHADER_FUNC_RAMP ) {
				break;
			} else {
				if( alphagenfunc->type == SHADER_FUNC_NOISE ) {
					a = RB_BackendGetNoiseValue( 0, 0, 0, ( rb.currentShaderTime + alphagenfunc->args[2] ) * alphagenfunc->args[3] );
				} else {
					table = RB_TableForFunc( alphagenfunc->type );
					a = alphagenfunc->args[2] + rb.currentShaderTime * alphagenfunc->args[3];
					a = FTABLE_EVALUATE( table, a );
				}

				a = a * alphagenfunc->args[1] + alphagenfunc->args[0];
			}

			rgba[3] = ( int )( a * 255.0f );
			break;
		case ALPHA_GEN_ENTITY:
			rgba[3] = rb.entityColor[3];
			break;
		case ALPHA_GEN_OUTLINE:
			rgba[3] = rb.entityOutlineColor[3];
		default:
			break;
	}

	for( c = 0; c < 4; c++ ) {
		rgba_[c] = bound( 0, rgba[c], 255 );
	}
}

/*
* RB_ShaderpassTex
*/
static inline const image_t *RB_ShaderpassTex( const shaderpass_t *pass ) {
	const image_t *tex;

	if( pass->anim_fps && pass->anim_numframes ) {
		return pass->images[(int)( pass->anim_fps * rb.currentShaderTime ) % pass->anim_numframes];
	}

	if( ( pass->flags & SHADERPASS_SKYBOXSIDE ) && rb.skyboxShader && rb.skyboxSide >= 0 ) {
		return rb.skyboxShader->skyParms.images[rb.skyboxSide];
	}

	tex = pass->images[0];

	if( !tex ) {
		return rsh.noTexture;
	}
	if( !tex->missing ) {
		return tex;
	}
	return rsh.greyTexture;
}

//==================================================================================

/*
* RB_RGBAlphaGenToProgramFeatures
*/
static int RB_RGBAlphaGenToProgramFeatures( const colorgen_t *rgbgen, const colorgen_t *alphagen ) {
	r_glslfeat_t programFeatures;

	programFeatures = 0;

	switch( rgbgen->type ) {
		case RGB_GEN_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_VERTEX;
			break;
		case RGB_GEN_WAVE:
		case RGB_GEN_CUSTOMWAVE:
		case RGB_GEN_ENTITYWAVE:
			if( rgbgen->func.type == SHADER_FUNC_RAMP ) {
				programFeatures |= GLSL_SHADER_COMMON_RGB_DISTANCERAMP;
			}
			break;
		case RGB_GEN_IDENTITY:
		default:
			break;
	}

	switch( alphagen->type ) {
		case ALPHA_GEN_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX;
			break;
		case ALPHA_GEN_WAVE:
			if( alphagen->func.type == SHADER_FUNC_RAMP ) {
				programFeatures |= GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP;
			}
			break;
		default:
			break;
	}

	return programFeatures;
}

/*
* RB_BonesTransformsToProgramFeatures
*/
static r_glslfeat_t RB_BonesTransformsToProgramFeatures( void ) {
	// check whether the current model is actually sketetal
	if( rb.currentModelType != mod_skeletal ) {
		return 0;
	}
	// base pose sketetal models aren't animated and rendered as-is
	if( !rb.bonesData.numBones ) {
		return 0;
	}
	return rb.bonesData.maxWeights * GLSL_SHADER_COMMON_BONE_TRANSFORMS1;
}

/*
* RB_AutospriteProgramFeatures
*/
static r_glslfeat_t RB_AutospriteProgramFeatures( void ) {
	r_glslfeat_t programFeatures = 0;
	if( ( rb.currentVAttribs & VATTRIB_AUTOSPRITE2_BIT ) == VATTRIB_AUTOSPRITE2_BIT ) {
		programFeatures |= GLSL_SHADER_COMMON_AUTOSPRITE2;
	} else if( ( rb.currentVAttribs & VATTRIB_AUTOSPRITE_BIT ) == VATTRIB_AUTOSPRITE_BIT ) {
		programFeatures |= GLSL_SHADER_COMMON_AUTOSPRITE;
	}
	return programFeatures;
}

/*
* RB_InstancedArraysProgramFeatures
*/
static r_glslfeat_t RB_InstancedArraysProgramFeatures( void ) {
	r_glslfeat_t programFeatures = 0;
	if( ( rb.currentVAttribs & VATTRIB_INSTANCES_BITS ) == VATTRIB_INSTANCES_BITS ) {
		programFeatures |= GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRANSFORMS;
	} else if( rb.drawElements.numInstances ) {
		programFeatures |= GLSL_SHADER_COMMON_INSTANCED_TRANSFORMS;
	}
	return programFeatures;
}

/*
* RB_AlphatestProgramFeatures
*/
static r_glslfeat_t RB_AlphatestProgramFeatures( const shaderpass_t *pass ) {
	switch( pass->flags & SHADERPASS_ALPHAFUNC ) {
		case SHADERPASS_AFUNC_GT0:
			return GLSL_SHADER_COMMON_AFUNC_GT0;
		case SHADERPASS_AFUNC_LT128:
			return GLSL_SHADER_COMMON_AFUNC_LT128;
		case SHADERPASS_AFUNC_GE128:
			return GLSL_SHADER_COMMON_AFUNC_GE128;
	}
	return 0;
}

/*
* RB_sRGBProgramFeatures
*
* The main framebuffer is always srgb, otherwise assume linear
*/
static r_glslfeat_t RB_sRGBProgramFeatures( const shaderpass_t *pass ) {
	r_glslfeat_t f = 0;

	if( pass->flags & SHADERPASS_NOSRGB ) {
		// don't perform srgb<->linear conversion at all - used for blitting framebuffers
		return 0;
	}

	if( glConfig.sSRGB ) {
		f |= GLSL_SHADER_COMMON_SRGB2LINEAR;

		// ok, so we're getting linear color input while rendering to
		// sRGB framebuffer, convert from linear color space to sRGB
		if( RFB_sRGBColorSpace( RB_BoundFrameBufferObject() ) ) {
			f |= GLSL_SHADER_COMMON_LINEAR2SRB;
		}
	}

	return f;
}

/*
* RB_UpdateCommonUniforms
*/
static void RB_UpdateCommonUniforms( int program, const shaderpass_t *pass, mat4_t texMatrix ) {
	vec3_t entDist, entOrigin;
	byte_vec4_t constColor;
	float colorMod = 1.0f;
	const entity_t *e = rb.currentEntity;
	vec3_t tmp;
	vec2_t blendMix = { 0, 0 };

	// the logic here should match R_TransformForEntity
	if( e->rtype != RT_MODEL ) {
		VectorClear( entOrigin );
		VectorCopy( rb.cameraOrigin, entDist );
	} else {
		VectorCopy( e->origin, entOrigin );
		VectorSubtract( rb.cameraOrigin, e->origin, tmp );
		Matrix3_TransformVector( e->axis, tmp, entDist );
	}

	// calculate constant color
	RB_GetShaderpassColor( pass, constColor, &colorMod );

	// apply modifications to texture coordinates
	if( pass->numtcmods ) {
		RB_ApplyTCMods( pass, texMatrix );
	}

	RP_UpdateViewUniforms( program,
						   rb.modelviewMatrix, rb.modelviewProjectionMatrix,
						   rb.cameraOrigin, rb.cameraAxis,
						   rb.gl.viewport,
						   rb.zNear, rb.zFar
						   );

	if( RB_IsAlphaBlending( rb.gl.state & GLSTATE_SRCBLEND_MASK, rb.gl.state & GLSTATE_DSTBLEND_MASK ) ) {
		blendMix[1] = 1;
		if( rb.alphaHack ) {
			constColor[3] *= rb.hackedAlpha;
		}
	} else {
		blendMix[0] = 1;
		if( rb.alphaHack ) {
			constColor[0] *= rb.hackedAlpha, constColor[1] *= rb.hackedAlpha, constColor[2] *= rb.hackedAlpha;
		}
	}

	RP_UpdateShaderUniforms( program,
							 rb.currentShaderTime,
							 entOrigin, entDist, rb.entityColor,
							 constColor,
							 pass->rgbgen.func.type != SHADER_FUNC_NONE ? pass->rgbgen.func.args : pass->rgbgen.args,
							 pass->alphagen.func.type != SHADER_FUNC_NONE ? pass->alphagen.func.args : pass->alphagen.args,
							 texMatrix, colorMod );

	RP_UpdateBlendMixUniform( program, blendMix );

	RP_UpdateSoftParticlesUniforms( program, r_soft_particles_scale->value );
}

/*
* RB_RenderMeshGLSL_Material
*/
static void RB_RenderMeshGLSL_Material( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int state;
	int program;
	const image_t *base, *normalmap, *glossmap, *decalmap, *entdecalmap;
	vec3_t lightDir = { 0.0f, 0.0f, 0.0f };
	vec4_t ambient = { 0.0f, 0.0f, 0.0f, 0.0f }, diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
	float glossIntensity, glossExponent;
	bool applyDecal;
	mat4_t texMatrix;
	const entity_t *e = rb.currentEntity;

	// handy pointers
	base = RB_ShaderpassTex( pass );
	normalmap = pass->images[1] && !pass->images[1]->missing ? pass->images[1] : rsh.blankBumpTexture;
	glossmap = pass->images[2] && !pass->images[2]->missing ?  pass->images[2] : NULL;
	decalmap = pass->images[3] && !pass->images[3]->missing ?  pass->images[3] : NULL;
	entdecalmap = pass->images[4] && !pass->images[4]->missing ?  pass->images[4] : NULL;

	if( normalmap && !normalmap->loaded ) {
		normalmap = rsh.blankBumpTexture;
	}
	if( glossmap && !glossmap->loaded ) {
		glossmap = NULL;
	}
	if( decalmap && !decalmap->loaded ) {
		decalmap = NULL;
	}
	if( entdecalmap && !entdecalmap->loaded ) {
		entdecalmap = NULL;
	}

	// use blank image if the normalmap is too tiny due to high picmip value
	if( !normalmap || ( normalmap->upload_width < 2 || normalmap->upload_height < 2 ) ) {
		normalmap = rsh.blankBumpTexture;
	}

	if( rb.mode == RB_MODE_POST_LIGHT ) {
		if( !decalmap && !entdecalmap ) {
			return;
		}
		normalmap = rsh.blankBumpTexture;
		glossmap = NULL;
	}

	if( rb.noColorWrite || rb.currentModelType == mod_brush ) {
		// render as plain Q3A shader, which is less computation-intensive
		// TODO: write a depth only shader
		RB_RenderMeshGLSL_Q3AShader( pass, programFeatures );
		return;
	}

	state = RB_GetShaderpassState( pass->flags );
	if( rb.mode == RB_MODE_POST_LIGHT ) {
		state = ( state & ~GLSTATE_DEPTHWRITE ) | GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE;
	}

	if( rb.mode == RB_MODE_DEPTH ) {
		if( !(state & GLSTATE_DEPTHWRITE) ) {
			return;
		}
	}

	glossIntensity = rb.currentShader->glossIntensity ? rb.currentShader->glossIntensity : r_lighting_glossintensity->value;
	glossExponent = rb.currentShader->glossExponent ? rb.currentShader->glossExponent : r_lighting_glossexponent->value;

	applyDecal = decalmap != NULL;
	if( rb.mode == RB_MODE_DIFFUSE ) {
		applyDecal = false;
	}

	if( rb.currentModelType == mod_brush ) {
		if( rb.mode == RB_MODE_DIFFUSE ) {
			return;
		}

		// brush models
		if( DRAWFLAT() ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT | GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY;
		}
	} else if( rb.currentModelType == mod_bad ) {
		// polys
	} else {
		// regular models
	#ifdef HALFLAMBERTLIGHTING
		programFeatures |= GLSL_SHADER_MATERIAL_HALFLAMBERT;
	#endif
	}

	Matrix4_Identity( texMatrix );

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( state );

	RB_BindImage( 0, base );

	// we only send S-vectors to GPU and recalc T-vectors as cross product
	// in vertex shader
	RB_BindImage( 1, normalmap );         // normalmap

	if( glossmap && glossIntensity ) {
		programFeatures |= GLSL_SHADER_MATERIAL_SPECULAR;
		RB_BindImage( 2, glossmap ); // gloss
	}

	if( applyDecal ) {
		programFeatures |= GLSL_SHADER_MATERIAL_DECAL;

		// if no alpha, use additive blending
		if( decalmap->samples & 1 ) {
			programFeatures |= GLSL_SHADER_MATERIAL_DECAL_ADD;
		}

		RB_BindImage( 3, decalmap ); // decal
	}

	if( entdecalmap ) {
		programFeatures |= GLSL_SHADER_MATERIAL_ENTITY_DECAL;

		// if no alpha, use additive blending
		if( entdecalmap->samples & 1 ) {
			programFeatures |= GLSL_SHADER_MATERIAL_ENTITY_DECAL_ADD;
		}

		RB_BindImage( 4, entdecalmap ); // decal
	}

	if( e->flags & RF_FULLBRIGHT || rb.currentModelType == mod_bad ) {
		programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT;
		Vector4Set( ambient, 1, 1, 1, 1 );
		Vector4Set( diffuse, 1, 1, 1, 1 );
	} else {
		bool minLight = ( e->flags & RF_MINLIGHT ) != 0;

		if( minLight ) {
			float ambientL = VectorLength( ambient );

			if( ambientL < rb.minLight ) {
				if( ambientL < 0.001 ) {
					VectorSet( ambient, 1, 1, 1 );
				}

				VectorNormalize( ambient );
				VectorScale( ambient, rb.minLight, ambient );

				programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT;
			}
		}
	}

	RB_BindImage( 7, rsh.blueNoiseTexture );

	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		// update uniforms

		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateMaterialUniforms( program, glossIntensity, glossExponent );

		RP_UpdateDiffuseLightUniforms( program, lightDir, ambient, diffuse );

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		// r_drawflat
		if( programFeatures & GLSL_SHADER_COMMON_DRAWFLAT ) {
			RP_UpdateDrawFlatUniforms( program, rsh.wallColor, rsh.floorColor );
		}

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_Outline
*/
static void RB_RenderMeshGLSL_Outline( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int faceCull;
	int program;
	mat4_t texMatrix;

	if( rb.currentModelType == mod_brush ) {
		programFeatures |= GLSL_SHADER_OUTLINE_OUTLINES_CUTOFF;
	}

	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	// update uniforcms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_OUTLINE, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( !RB_BindProgram( program ) ) {
		return;
	}

	Matrix4_Identity( texMatrix );

	faceCull = rb.gl.faceCull;
	RB_Cull( GL_BACK );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	RB_UpdateCommonUniforms( program, pass, texMatrix );

	RP_UpdateOutlineUniforms( program, rb.currentEntity->outlineHeight * r_outlines_scale->value );

	// submit animation data
	if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
		RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
	}

	RB_DrawElementsReal( &rb.drawElements );

	RB_Cull( faceCull );
}

/*
* RB_TcGenToProgramFeatures
*/
r_glslfeat_t RB_TcGenToProgramFeatures( int tcgen, vec_t *tcgenVec, mat4_t texMatrix, mat4_t genVectors ) {
	r_glslfeat_t programFeatures = 0;

	Matrix4_Identity( texMatrix );

	switch( tcgen ) {
		case TC_GEN_ENVIRONMENT:
			programFeatures |= GLSL_SHADER_Q3_TC_GEN_ENV;
			break;
		case TC_GEN_VECTOR:
			Matrix4_Identity( genVectors );
			Vector4Copy( &tcgenVec[0], &genVectors[0] );
			Vector4Copy( &tcgenVec[4], &genVectors[4] );
			programFeatures |= GLSL_SHADER_Q3_TC_GEN_VECTOR;
			break;
		case TC_GEN_PROJECTION:
			programFeatures |= GLSL_SHADER_Q3_TC_GEN_PROJECTION;
			break;
		case TC_GEN_REFLECTION:
			programFeatures |= GLSL_SHADER_Q3_TC_GEN_REFLECTION;
			break;
		case TC_GEN_SURROUND:
			programFeatures |= GLSL_SHADER_Q3_TC_GEN_SURROUND;
			break;
		default:
			break;
	}

	return programFeatures;
}

/*
* RB_RenderMeshGLSL_Q3AShader
*/
static void RB_RenderMeshGLSL_Q3AShader( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int state;
	int program;
	int rgbgen = pass->rgbgen.type;
	const image_t *image;
	bool isWorldSurface = rb.currentModelType == mod_brush;
	bool isWorldVertexLight = false;
	bool applyLighting;
	vec3_t lightDir;
	vec4_t lightAmbient, lightDiffuse;
	mat4_t texMatrix, genVectors;
	bool noDlight = ( rb.surfFlags & (SURF_SKY|SURF_NODLIGHT) ) != 0;

	if( isWorldSurface ) {
		if( rb.mode == RB_MODE_DIFFUSE ) {
			return;
		}

		isWorldVertexLight = !noDlight;
	}

	// diffuse lighting for entities
	VectorSet( lightDir, 0, 0, 0 );
	Vector4Set( lightAmbient, 1, 1, 1, 1 );
	Vector4Set( lightDiffuse, 1, 1, 1, 1 );

	image = RB_ShaderpassTex( pass );
	if( rb.triangleOutlines || rb.noColorWrite || rb.mode == RB_MODE_DECALS ) {
		applyLighting = false;
	} else {
		applyLighting = isWorldVertexLight;
	}

	if( !applyLighting && rb.mode == RB_MODE_DIFFUSE ) {
		return;
	}
	if( applyLighting && rb.mode == RB_MODE_POST_LIGHT ) {
		return;
	}

	if( applyLighting ) {
		if( DRAWFLAT() ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT;
		}
	}

	if( image->flags & IT_ALPHAMASK ) {
		programFeatures |= GLSL_SHADER_Q3_ALPHA_MASK;
	}

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	programFeatures |= RB_TcGenToProgramFeatures( pass->tcgen, pass->tcgenVec, texMatrix, genVectors );

	// set shaderpass state (blending, depthwrite, etc)
	state = pass->flags;

	if( isWorldVertexLight && !rb.doneDepthPass && !( state & GLSTATE_DEPTHWRITE ) && Shader_DepthWrite( rb.currentShader ) ) {
		if( !( pass->flags & SHADERPASS_ALPHAFUNC ) ) {
			state &= ~GLSTATE_BLEND_MASK;
		}
		state |= GLSTATE_DEPTHWRITE;
	}

	if( rb.mode == RB_MODE_DEPTH ) {
		if( !( state & GLSTATE_DEPTHWRITE ) ) {
			return;
		}
	}

	RB_SetState( RB_GetShaderpassState( state ) );

	RB_BindImage( 0, image );

	if( programFeatures & GLSL_SHADER_COMMON_SOFT_PARTICLE ) {
		RB_BindImage( 3, rb.st.screenDepthTexCopy );
	}

	RB_BindImage( 7, rsh.blueNoiseTexture );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_Q3A_SHADER, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateTexGenUniforms( program, genVectors );

		if( isWorldSurface || rgbgen == RGB_GEN_LIGHTING_DIFFUSE ) {
			RP_UpdateDiffuseLightUniforms( program, lightDir, lightAmbient, lightDiffuse );
		}

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		// r_drawflat
		if( programFeatures & GLSL_SHADER_COMMON_DRAWFLAT ) {
			RP_UpdateDrawFlatUniforms( program, rsh.wallColor, rsh.floorColor );
		}

		if( programFeatures & GLSL_SHADER_COMMON_SOFT_PARTICLE ) {
			RP_UpdateTextureUniforms( program,
									  rb.st.screenDepthTex->upload_width, rb.st.screenDepthTex->upload_height );
		}

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_ColorCorrection
*/
static void RB_RenderMeshGLSL_ColorCorrection( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int program;
	mat4_t texMatrix;

	programFeatures &= ~GLSL_SHADER_COMMON_SRGB2LINEAR;

	if( pass->images[0]->flags & IT_FLOAT ) {
		if( glConfig.sSRGB ) {
			programFeatures |= GLSL_SHADER_COMMON_SRGB2LINEAR;
		}
		if( r_hdr->integer ) {
			programFeatures |= GLSL_SHADER_COLOR_CORRECTION_HDR;
		}
	}

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	Matrix4_Identity( texMatrix );

	RB_BindImage( 0, pass->images[0] );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_COLOR_CORRECTION, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateColorCorrectionUniforms( program, r_hdr_gamma->value, rb.hdrExposure );

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_KawaseBlur
*/
static void RB_RenderMeshGLSL_KawaseBlur( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int program;
	mat4_t texMatrix = { 0 };

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	RB_BindImage( 0, pass->images[0] );

	Matrix4_Identity( texMatrix );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_KAWASE_BLUR, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateKawaseUniforms( program, pass->images[0]->upload_width, pass->images[0]->upload_height, pass->anim_numframes );

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSLProgrammed
*/
void RB_RenderMeshGLSLProgrammed( const shaderpass_t *pass, int programType ) {
	r_glslfeat_t features = 0;

	if( rb.greyscale || pass->flags & SHADERPASS_GREYSCALE ) {
		features |= GLSL_SHADER_COMMON_GREYSCALE;
	}

	features |= RB_BonesTransformsToProgramFeatures();
	features |= RB_AutospriteProgramFeatures();
	features |= RB_InstancedArraysProgramFeatures();
	features |= RB_AlphatestProgramFeatures( pass );
	features |= RB_sRGBProgramFeatures( pass );

	if( ( rb.currentShader->flags & SHADER_SOFT_PARTICLE )
		&& rb.st.screenDepthTexCopy
		&& ( rb.renderFlags & RF_SOFT_PARTICLES ) ) {
		features |= GLSL_SHADER_COMMON_SOFT_PARTICLE;
	}

	switch( programType ) {
		case GLSL_PROGRAM_TYPE_MATERIAL:
			RB_RenderMeshGLSL_Material( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_OUTLINE:
			RB_RenderMeshGLSL_Outline( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_Q3A_SHADER:
			RB_RenderMeshGLSL_Q3AShader( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_COLOR_CORRECTION:
			RB_RenderMeshGLSL_ColorCorrection( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_KAWASE_BLUR:
			RB_RenderMeshGLSL_KawaseBlur( pass, features );
			break;
		default:
			ri.Com_DPrintf( S_COLOR_YELLOW "WARNING: Unknown GLSL program type %i\n", programType );
			return;
	}
}

//==================================================================================

/*
* RB_UpdateVertexAttribs
*/
static void RB_UpdateVertexAttribs( void ) {
	vattribmask_t vattribs = 0;
	
	if( rb.currentShader ) {
		vattribs |= rb.currentShader->vattribs;
	}
	if( rb.bonesData.numBones ) {
		vattribs |= VATTRIB_BONES_BITS;
	}
	if( rb.currentEntity && rb.currentEntity->outlineHeight ) {
		vattribs |= VATTRIB_NORMAL_BIT;
	}
	if( DRAWFLAT() ) {
		vattribs |= VATTRIB_NORMAL_BIT;
	}

	rb.currentVAttribs = vattribs;
}

/*
* RB_BindShader
*/
void RB_BindShader( const entity_t *e, const shader_t *shader ) {
	if( !rb.dirtyUniformState ) {
		if( rb.currentEntity == e && rb.currentShader == shader ) {
			return;
		}
	}

	rb.currentShader = shader;

	rb.doneDepthPass = false;
	rb.dirtyUniformState = true;

	rb.currentEntity = e ? e : &rb.nullEnt;
	rb.currentModelType = rb.currentEntity->rtype == RT_MODEL && rb.currentEntity->model ? rb.currentEntity->model->type : mod_bad;

	rb.bonesData.numBones = 0;
	rb.bonesData.maxWeights = 0;

	rb.skyboxShader = NULL;
	rb.skyboxSide = -1;

	rb.surfFlags = SURF_NODLIGHT;

	if( !e ) {
		rb.currentShaderTime = rb.nullEnt.shaderTime * 0.001;
		rb.alphaHack = false;
		rb.greyscale = false;
		rb.noDepthTest = false;
		rb.noColorWrite = false;
		rb.depthEqual = false;
	} else {
		Vector4Copy( rb.currentEntity->shaderRGBA, rb.entityColor );
		Vector4Copy( rb.currentEntity->outlineColor, rb.entityOutlineColor );
		if( rb.currentEntity->shaderTime > rb.time ) {
			rb.currentShaderTime = 0;
		} else {
			rb.currentShaderTime = ( rb.time - rb.currentEntity->shaderTime ) * 0.001;
		}
		rb.alphaHack = e->renderfx & RF_ALPHAHACK ? true : false;
		rb.hackedAlpha = e->shaderRGBA[3] / 255.0;
		rb.greyscale = e->renderfx & RF_GREYSCALE ? true : false;
		rb.noDepthTest = e->renderfx & RF_NODEPTHTEST && e->rtype == RT_SPRITE ? true : false;
		rb.noColorWrite = e->renderfx & RF_NOCOLORWRITE ? true : false;
		rb.depthEqual = rb.alphaHack && !( e->renderfx & RF_WEAPONMODEL );
	}

	if( rb.mode == RB_MODE_DEPTH ) {
		rb.noColorWrite = true;
	}
	if( rb.mode == RB_MODE_DIFFUSE ) {
		rb.depthEqual = true;
	}

	RB_UpdateVertexAttribs();
}

/*
* RB_SetAnimData
*/
void RB_SetBonesData( int numBones, dualquat_t *dualQuats, int maxWeights ) {
	assert( rb.currentShader != NULL );

	if( numBones > MAX_GLSL_UNIFORM_BONES ) {
		numBones = MAX_GLSL_UNIFORM_BONES;
	}
	if( maxWeights > 4 ) {
		maxWeights = 4;
	}

	rb.bonesData.numBones = numBones;
	memcpy( rb.bonesData.dualQuats, dualQuats, numBones * sizeof( *dualQuats ) );
	rb.bonesData.maxWeights = maxWeights;

	rb.dirtyUniformState = true;

	RB_UpdateVertexAttribs();
}

/*
* RB_SetSkyboxShader
*/
void RB_SetSkyboxShader( const shader_t *shader ) {
	if( rb.skyboxShader == shader ) {
		return;
	}
	rb.skyboxShader = shader;
	rb.dirtyUniformState = true;
}

/*
* RB_SetSkyboxSide
*/
void RB_SetSkyboxSide( int side ) {
	if( side < 0 || side >= 6 ) {
		side = -1;
	}

	if( rb.skyboxSide == side ) {
		return;
	}

	rb.skyboxSide = side;
	rb.dirtyUniformState = true;
}

/*
* RB_SetInstanceData
*
* Internal backend function, only used by RB_DrawElementsReal to upload
* instance data
*/
void RB_SetInstanceData( int numInstances, instancePoint_t *instances ) {
	if( !rb.currentProgram ) {
		return;
	}
	RP_UpdateInstancesUniforms( rb.currentProgram, numInstances, instances );
}

/*
* RB_SetZClip
*/
void RB_SetZClip( float zNear, float zFar ) {
	rb.zNear = zNear;
	rb.zFar = zFar;
	rb.dirtyUniformState = true;
}

/*
* RB_SetLightParams
*/
void RB_SetLightParams( float minLight, bool noWorldLight, float hdrExposure ) {
	rb.minLight = minLight;
	rb.noWorldLight = noWorldLight;
	rb.hdrExposure = hdrExposure;
	rb.dirtyUniformState = true;
}

/*
* RB_SetScreenImageSet
*/
void RB_SetScreenImageSet( const refScreenTexSet_t *st ) {
	if( st )
		rb.st = *st;
}

/*
* RB_RegisterProgram
*/
int RB_RegisterProgram( int type, const char *name, const char *deformsKey,
						const deformv_t *deforms, int numDeforms, r_glslfeat_t features ) {
	int program;
	bool noDeforms = !deformsKey || !*deformsKey;

	if( rb.currentRegProgramType == type && noDeforms && rb.currentRegProgramFeatures == features ) {
		return rb.currentRegProgram;
	}

	program = RP_RegisterProgram( type, name, deformsKey, deforms, numDeforms, features );
	if( noDeforms ) {
		rb.currentRegProgram = program;
		rb.currentRegProgramType = type;
		rb.currentRegProgramFeatures = features;
	}

	return program;
}

/*
* RB_BindProgram
*/
int RB_BindProgram( int program ) {
	int object;

	if( program == rb.currentProgram ) {
		return rb.currentProgramObject;
	}

	rb.currentProgram = program;
	if( !program ) {
		rb.currentProgramObject = 0;
		glUseProgram( 0 );
		return 0;
	}

	object = RP_GetProgramObject( program );
	if( object ) {
		glUseProgram( object );
	}
	rb.currentProgramObject = object;
	rb.dirtyUniformState = true;
	rb.stats.c_totalPrograms++;
	return object;
}

/*
* RB_RenderPass
*/
static void RB_RenderPass( const shaderpass_t *pass ) {
	if( pass->program_type ) {
		RB_RenderMeshGLSLProgrammed( pass, pass->program_type );
	} else {
		RB_RenderMeshGLSLProgrammed( pass, GLSL_PROGRAM_TYPE_Q3A_SHADER );
	}
}

/*
* RB_SetShaderStateMask
*/
void RB_SetShaderStateMask( int ANDmask, int ORmask ) {
	rb.shaderStateANDmask = ANDmask;
	rb.shaderStateORmask = ORmask;
}

/*
* RB_SetShaderState
*/
static void RB_SetShaderState( void ) {
	int state;
	int shaderFlags = rb.currentShader->flags;

	// Face culling
	if( rb.currentEntity->rtype == RT_SPRITE ) {
		RB_Cull( 0 );
	} else if( shaderFlags & SHADER_CULL_FRONT ) {
		RB_Cull( GL_FRONT );
	} else if( shaderFlags & SHADER_CULL_BACK ) {
		RB_Cull( GL_BACK );
	} else {
		RB_Cull( 0 );
	}

	state = 0;

	if( shaderFlags & SHADER_POLYGONOFFSET ) {
		state |= GLSTATE_OFFSET_FILL;
	}
	if( shaderFlags & SHADER_STENCILTEST ) {
		state |= GLSTATE_STENCIL_TEST;
	}

	if( rb.noDepthTest ) {
		state |= GLSTATE_NO_DEPTH_TEST;
	}

	rb.donePassesTotal = 0;
	rb.currentShaderState = ( state & rb.shaderStateANDmask ) | rb.shaderStateORmask;
}

/*
* RB_GetShaderpassState
*/
static int RB_GetShaderpassState( int state ) {
	state |= rb.currentShaderState;

	if( rb.mode == RB_MODE_DIFFUSE ) {
		state &= ~(GLSTATE_DEPTHWRITE|GLSTATE_DSTBLEND_MASK);
		if( !(state & GLSTATE_OFFSET_FILL) ) {
			state |= GLSTATE_DEPTHFUNC_EQ;
		}
		state |= GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE;
	} else {
		if( rb.alphaHack && !( state & GLSTATE_BLEND_MASK ) ) {
			// force alpha blending
			state = ( state & ~GLSTATE_DEPTHWRITE ) | GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		}
		if( rb.noColorWrite ) {
			state |= GLSTATE_NO_COLORWRITE;
		}
		if( rb.depthEqual && ( state & GLSTATE_DEPTHWRITE ) ) {
			state |= GLSTATE_DEPTHFUNC_EQ;
		}
	}
	return state;
}

/*
* RB_CleanSinglePass
*
* Attempts to reuse current GLSL state: since the dirty flag
* is not set and there have been no uniform updates, we can simply
* call glDrawElements with fresh vertex data
*/
static bool RB_CleanSinglePass( void ) {
	// reuse current GLSL state (same program bound, same uniform values)
	if( !rb.dirtyUniformState && rb.donePassesTotal == 1 ) {
		RB_DrawElementsReal( &rb.drawElements );
		rb.donePassesTotal = 1;
		return true;
	}
	return false;
}

/*
* RB_TriangleLinesColor
*/
static inline const vec_t *RB_TriangleLinesColor( void ) {
	if( r_showtris->integer != 2 ) {
		return colorWhite;
	}

	if( rb.currentModelType == mod_brush ) {
		return colorWhite;
	}
	if( rb.currentModelType != mod_bad ) {
		return colorRed;
	}
	if( rb.currentEntity != rsc.worldent ) {
		return colorBlue;
	}
	return colorGreen;
}

/*
* RB_DrawOutlinedElements
*/
void RB_DrawOutlinedElements( void ) {
	static shaderpass_t r_triLinesPass;
	static vec4_t r_triLinesColor;
	shaderpass_t *pass;

	if( RB_CleanSinglePass() ) {
		return;
	}

	Vector4Copy( RB_TriangleLinesColor(), r_triLinesColor );

	pass = &rb.currentShader->passes[0];

	// copy and override
	r_triLinesPass = *pass;
	r_triLinesPass.rgbgen.type = RGB_GEN_CONST;
	r_triLinesPass.rgbgen.args = &r_triLinesColor[0];
	r_triLinesPass.alphagen.type = ALPHA_GEN_CONST;
	r_triLinesPass.alphagen.args = &r_triLinesColor[3];
	r_triLinesPass.flags = 0;
	r_triLinesPass.images[0] = rsh.whiteTexture;
	r_triLinesPass.anim_fps = 0;
	r_triLinesPass.anim_numframes = 0;
	r_triLinesPass.program_type = GLSL_PROGRAM_TYPE_Q3A_SHADER;

	RB_SetShaderState();

	RB_RenderPass( &r_triLinesPass );
}

/*
* RB_DrawShadedElements
*/
void RB_DrawShadedElements( void ) {
	unsigned i;
	bool addGLSLOutline = false;
	shaderpass_t *pass;

	if( ( rb.mode == RB_MODE_DEPTH ) && !( rb.currentShader->flags & SHADER_DEPTHWRITE ) ) {
		return;
	}
	if( RB_CleanSinglePass() ) {
		return;
	}

	if( rb.currentEntity->outlineHeight && rb.currentShader->sort == SHADER_SORT_OPAQUE && Shader_CullFront( rb.currentShader ) ) {
		addGLSLOutline = true;
	}

	RB_SetShaderState();

	for( i = 0, pass = rb.currentShader->passes; i < rb.currentShader->numpasses; i++, pass++ ) {
		if( rb.mode == RB_MODE_DECALS ) {
			int state = RB_GetShaderpassState( pass->flags );
			if( !(state & GLSTATE_BLEND_MASK) ) {
				continue;
			}
		}

		if( pass->flags & SHADERPASS_DETAIL ) {
			continue;
		}

		RB_RenderPass( pass );
	}

	if( rb.mode == RB_MODE_DEPTH || rb.mode == RB_MODE_TRIANGLE_OUTLINES || rb.mode == RB_MODE_DIFFUSE ) {
		goto end;
	}

	// outlines
	if( addGLSLOutline ) {
		RB_RenderPass( &r_GLSLpasses[BUILTIN_GLSLPASS_OUTLINE] );
	}

end:
	rb.dirtyUniformState = rb.donePassesTotal != 1;
}
