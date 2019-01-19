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
	BUILTIN_GLSLPASS_FOG,
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
static void RB_RenderMeshGLSL_Distortion( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Shadow( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Outline( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Q3AShader( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_Fog( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_FXAA( const shaderpass_t *pass, r_glslfeat_t programFeatures );
static void RB_RenderMeshGLSL_YUV( const shaderpass_t *pass, r_glslfeat_t programFeatures );

/*
* RB_InitBuiltinPasses
*/
static void RB_InitBuiltinPasses( void ) {
	shaderpass_t *pass;

	// init optional GLSL program passes
	memset( r_GLSLpasses, 0, sizeof( r_GLSLpasses ) );

	// init fog pass
	pass = &r_GLSLpasses[BUILTIN_GLSLPASS_FOG];
	pass->program_type = GLSL_PROGRAM_TYPE_Q3A_SHADER;
	pass->tcgen = TC_GEN_FOG;
	pass->rgbgen.type = RGB_GEN_FOG;
	pass->alphagen.type = ALPHA_GEN_IDENTITY;
	pass->flags = GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	pass->program_type = GLSL_PROGRAM_TYPE_FOG;

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
* RB_TransformFogPlanes
*/
static float RB_TransformFogPlanes( const mfog_t *fog, vec3_t fogNormal,
									vec_t *fogDist, vec3_t vpnNormal, vec_t *vpnDist ) {
	cplane_t *fogPlane;
	shader_t *fogShader;
	vec3_t viewtofog;
	float dist, scale;
	const entity_t *e = rb.currentEntity;

	assert( fog );
	assert( fogNormal && fogDist );
	assert( vpnNormal && vpnDist );

	fogPlane = fog->visibleplane;
	fogShader = fog->shader;

	// distance to fog
	dist = PlaneDiff( rb.cameraOrigin, fog->visibleplane );
	scale = e->scale;

	if( rb.currentShader->flags & SHADER_SKY ) {
		if( dist > 0 ) {
			VectorMA( rb.cameraOrigin, -dist, fogPlane->normal, viewtofog );
		} else {
			VectorCopy( rb.cameraOrigin, viewtofog );
		}
	} else if( e->rtype == RT_MODEL ) {
		VectorCopy( e->origin, viewtofog );
	} else {
		VectorClear( viewtofog );
	}

	// some math tricks to take entity's rotation matrix into account
	// for fog texture coordinates calculations:
	// M is rotation matrix, v is vertex, t is transform vector
	// n is plane's normal, d is plane's dist, r is view origin
	// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
	// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)
	Matrix3_TransformVector( e->axis, fogPlane->normal, fogNormal );
	VectorScale( fogNormal, scale, fogNormal );
	*fogDist = ( fogPlane->dist - DotProduct( viewtofog, fogPlane->normal ) );

	Matrix3_TransformVector( e->axis, rb.cameraAxis, vpnNormal );
	VectorScale( vpnNormal, scale, vpnNormal );
	*vpnDist = ( ( rb.cameraOrigin[0] - viewtofog[0] ) * rb.cameraAxis[AXIS_FORWARD + 0] +
				 ( rb.cameraOrigin[1] - viewtofog[1] ) * rb.cameraAxis[AXIS_FORWARD + 1] +
				 ( rb.cameraOrigin[2] - viewtofog[2] ) * rb.cameraAxis[AXIS_FORWARD + 2] ) +
			   fogShader->fog_clearDist;

	return dist;
}

/*
* RB_VertexTCCelshadeMatrix
*/
void RB_VertexTCCelshadeMatrix( mat4_t matrix ) {
	vec3_t dir;
	mat4_t m;
	const entity_t *e = rb.currentEntity;

	if( e->model != NULL && !( rb.renderFlags & RF_SHADOWMAPVIEW ) ) {
		R_LightForOrigin( e->lightingOrigin, dir, NULL, NULL, e->model->radius * e->scale, rb.noWorldLight, false );

		Matrix4_Identity( m );

		// rotate direction
		Matrix3_TransformVector( e->axis, dir, &m[0] );
		VectorNormalize( &m[0] );

		MakeNormalVectors( &m[0], &m[4], &m[8] );
		Matrix4_Transpose( m, matrix );
	}
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
				if( pass->program_type != GLSL_PROGRAM_TYPE_DISTORTION ) { // HACK HACK HACK
					t1 = t1 - floor( t1 );
					t2 = t2 - floor( t2 );
				}
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
		case RGB_GEN_ONE_MINUS_ENTITY:
			rgba[0] = 255 - rb.entityColor[0];
			rgba[1] = 255 - rb.entityColor[1];
			rgba[2] = 255 - rb.entityColor[2];
			break;
		case RGB_GEN_FOG:
			rgba[0] = rb.texFog->shader->fog_color[0];
			rgba[1] = rb.texFog->shader->fog_color[1];
			rgba[2] = rb.texFog->shader->fog_color[2];
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

	if( pass->flags & SHADERPASS_PORTALMAP ) {
		return rb.currentPortalSurface && rb.currentPortalSurface->texures[0] ?
			   rb.currentPortalSurface->texures[0] : rsh.blackTexture;
	}

	if( ( pass->flags & SHADERPASS_SKYBOXSIDE ) && rb.skyboxShader && rb.skyboxSide >= 0 ) {
		return rb.skyboxShader->skyParms.images[rb.skyboxSide];
	}

	if( pass->cin ) {
		tex = R_GetCinematicImage( pass->cin );
	} else {
		tex = pass->images[0];
	}

	if( !tex ) {
		return rsh.noTexture;
	}
	if( !tex->missing ) {
		return tex;
	}
	return r_usenotexture->integer == 0 ? rsh.greyTexture : rsh.noTexture;
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
		case RGB_GEN_ONE_MINUS_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX;
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
		case ALPHA_GEN_ONE_MINUS_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX;
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
* RB_RtLightsToProgramFeatures
*/
static r_glslfeat_t RB_RtlightbitsToProgramFeatures( void ) {
	r_glslfeat_t bits;
	unsigned int numRtlights = rb.numRealtimeLights;

	bits = 0;

	if( rb.numRealtimeLights > 0 && rb.rtlights[0]->shadowSize > 0 && rsh.shadowmapAtlasTexture ) {
		bits |= GLSL_SHADER_COMMON_REALTIME_SHADOWS;

		if( glConfig.ext.shadow ) {
			bits |= GLSL_SHADER_COMMON_SHADOWMAP_SAMPLERS;
		} else if( glConfig.ext.rgb8_rgba8 ) {
			bits |= GLSL_SHADER_COMMON_RGBSHADOW_24BIT;
		}
		if( r_shadows_pcf->integer > 1 ) {
			bits |= GLSL_SHADER_COMMON_SHADOWMAP_PCF2;
		} else if( r_shadows_pcf->integer > 0 ) {
			bits |= GLSL_SHADER_COMMON_SHADOWMAP_PCF;
		}
	}

	if( !numRtlights ) {
		return bits;
	}

	if( rb.rtlights[0]->cubemapFilter ) {
		if( !rb.rtlights[0]->cubemapFilter->missing )
			bits |= GLSL_SHADER_COMMON_DLIGHT_CUBEFILTER;
	}

	if( rb.rtlights[0]->directional )
		bits |= GLSL_SHADER_COMMON_DLIGHT_DIRECTIONAL;

	return bits|GLSL_SHADER_COMMON_DLIGHTS;
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
* RB_FogProgramFeatures
*/
static r_glslfeat_t RB_FogProgramFeatures( const shaderpass_t *pass, const mfog_t *fog ) {
	r_glslfeat_t programFeatures = 0;
	if( fog ) {
		programFeatures |= GLSL_SHADER_COMMON_FOG;
		if( fog == rb.colorFog ) {
			programFeatures |= GLSL_SHADER_COMMON_FOG_RGB;
		}
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
						   rb.renderFlags & RF_MIRRORVIEW ? -1 : 1,
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
* RB_UpdateFogUniforms
*/
static void RB_UpdateFogUniforms( int program, const mfog_t *fog ) {
	float dist;
	cplane_t fogPlane, vpnPlane;

	assert( fog != NULL );
	if( !fog ) {
		return;
	}

	dist = RB_TransformFogPlanes( fog, fogPlane.normal, &fogPlane.dist, vpnPlane.normal, &vpnPlane.dist );

	RP_UpdateFogUniforms( program, fog->shader->fog_color, fog->shader->fog_clearDist,
						  fog->shader->fog_dist, &fogPlane, &vpnPlane, dist );
}

/*
* RB_RenderMeshGLSL_Material
*/
static void RB_RenderMeshGLSL_Material( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int i;
	int state;
	int program;
	const image_t *base, *normalmap, *glossmap, *decalmap, *entdecalmap;
	vec3_t lightDir = { 0.0f, 0.0f, 0.0f };
	vec4_t ambient = { 0.0f, 0.0f, 0.0f, 0.0f }, diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
	float offsetmappingScale, glossIntensity, glossExponent;
	const superLightStyle_t *lightStyle = NULL;
	const mfog_t *fog = rb.fog;
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

	if( rb.noColorWrite || ( rb.currentModelType == mod_brush && !mapConfig.deluxeMappingEnabled )
	    /*|| ( normalmap == rsh.blankBumpTexture && !glossmap && !decalmap && !entdecalmap )*/ ) {
		// render as plain Q3A shader, which is less computation-intensive
		RB_RenderMeshGLSL_Q3AShader( pass, programFeatures );
		return;
	}

	if( normalmap->samples == 4 ) {
		offsetmappingScale = r_offsetmapping_scale->value * rb.currentShader->offsetmappingScale;
	} else { // no alpha in normalmap, don't bother with offset mapping
		offsetmappingScale = 0;
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
	if( rb.mode == RB_MODE_LIGHT || rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE ) {
		applyDecal = false;
	}

	// possibly apply the "texture" fog inline
	if( fog == rb.texFog ) {
		if( rb.currentShader->numpasses == 1 ) {
			rb.texFog = NULL;
		} else {
			fog = NULL;
		}
	}
	programFeatures |= RB_FogProgramFeatures( pass, fog );

	if( rb.currentModelType == mod_brush ) {
		if( rb.mode == RB_MODE_DIFFUSE ) {
			return;
		}

		lightStyle = rb.superLightStyle;

		// brush models
		if( !( r_offsetmapping->integer & 1 ) ) {
			offsetmappingScale = 0;
		}
		if( rb.renderFlags & RF_LIGHTMAP ) {
			programFeatures |= GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY;
		}
		if( DRAWFLAT() ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT | GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY;
		}
	} else if( rb.currentModelType == mod_bad ) {
		// polys
		if( !( r_offsetmapping->integer & 2 ) ) {
			offsetmappingScale = 0;
		}
	} else {
		// regular models
		if( !( r_offsetmapping->integer & 4 ) ) {
			offsetmappingScale = 0;
		}
	#ifdef CELSHADEDMATERIAL
		programFeatures |= GLSL_SHADER_MATERIAL_CELSHADING;
	#endif
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

		if( rb.renderFlags & RF_LIGHTMAP ) {
			decalmap = rsh.blackTexture;
			programFeatures |= GLSL_SHADER_MATERIAL_DECAL_ADD;
		} else {
			// if no alpha, use additive blending
			if( decalmap->samples & 1 ) {
				programFeatures |= GLSL_SHADER_MATERIAL_DECAL_ADD;
			}
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

	if( offsetmappingScale > 0 ) {
		programFeatures |= r_offsetmapping_reliefmapping->integer ?
						   GLSL_SHADER_MATERIAL_RELIEFMAPPING : GLSL_SHADER_MATERIAL_OFFSETMAPPING;
	}


	programFeatures |= GLSL_SHADER_COMMON_LIGHTING;

	if( rb.mode == RB_MODE_POST_LIGHT ) {
		// only apply decals
		programFeatures &= ~GLSL_SHADER_COMMON_LIGHTING;
	} else if( e->flags & RF_FULLBRIGHT || rb.currentModelType == mod_bad ) {
		programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT;
		Vector4Set( ambient, 1, 1, 1, 1 );
		Vector4Set( diffuse, 1, 1, 1, 1 );
	} else if( rb.mode == RB_MODE_LIGHT ) {
		programFeatures |= RB_RtlightbitsToProgramFeatures();

		if( programFeatures & GLSL_SHADER_COMMON_DLIGHT_CUBEFILTER ) {
			RB_BindImage( 5, rb.rtlights[0]->cubemapFilter );
		}
	} else {
		bool minLight = ( e->flags & RF_MINLIGHT ) != 0;

		if( lightStyle ) {
			// world surface
			if( pass->rgbgen.type == RGB_GEN_VERTEX || pass->rgbgen.type == RGB_GEN_ONE_MINUS_VERTEX ) {
				// vertex lighting
				programFeatures |= GLSL_SHADER_COMMON_VERTEX_LIGHTING;
			} else if( lightStyle->lightmapNum[0] >= 0 ) {
				// bind lightmap textures and set program's features for lightstyles
				for( i = 0; i < MAX_LIGHTMAPS && lightStyle->lightmapStyles[i] != 255; i++ )
					RB_BindImage( i + 4, rsh.worldBrushModel->lightmapImages[lightStyle->lightmapNum[i]] );

				programFeatures |= ( i * GLSL_SHADER_MATERIAL_LIGHTSTYLE0 );

				if( mapConfig.lightmapArrays ) {
					programFeatures |= GLSL_SHADER_MATERIAL_LIGHTMAP_ARRAYS;
				}

				if( i == 1 ) {
					vec_t *rgb = rsc.lightStyles[lightStyle->lightmapStyles[0]].rgb;

					// GLSL_SHADER_MATERIAL_FB_LIGHTMAP indicates that there's no need to renormalize
					// the lighting vector for specular (saves 3 adds, 3 muls and 1 normalize per pixel)
					if( rgb[0] == 1 && rgb[1] == 1 && rgb[2] == 1 ) {
						programFeatures |= GLSL_SHADER_MATERIAL_FB_LIGHTMAP;
					}
				}

				if( !VectorCompare( mapConfig.ambient, vec3_origin ) ) {
					VectorCopy( mapConfig.ambient, ambient );
					programFeatures |= GLSL_SHADER_MATERIAL_AMBIENT_COMPENSATION;
				}
			}
		} else {
			vec3_t temp;

			// get weighted incoming direction of world and dynamic lights
			// for 'lightmap' mode, only consider contributions from the lightgrid
			R_LightForOrigin( e->lightingOrigin, temp, ambient, diffuse,
				e->model->radius * e->scale, rb.noWorldLight, rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE );

			// rotate direction
			Matrix3_TransformVector( e->axis, temp, lightDir );

			programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT;
		}

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

	if( programFeatures & GLSL_SHADER_COMMON_REALTIME_SHADOWS ) {
		RB_BindImage( 7, rsh.shadowmapAtlasTexture );
	}

	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		// update uniforms

		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateMaterialUniforms( program, offsetmappingScale, glossIntensity, glossExponent );

		RP_UpdateDiffuseLightUniforms( program, lightDir, ambient, diffuse );

		if( programFeatures & GLSL_SHADER_COMMON_FOG ) {
			RB_UpdateFogUniforms( program, fog );
		}

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RP_UpdateLightstyleUniforms( program, lightStyle );

		// dynamic lights
		if( rb.numRealtimeLights > 0 ) {
			RP_UpdateRealtimeLightsUniforms( program, rb.lightDir, rb.objectToLightMatrix,
				rb.numRealtimeLights, rb.rtlights, 0, NULL );
		}

		// r_drawflat
		if( programFeatures & GLSL_SHADER_COMMON_DRAWFLAT ) {
			RP_UpdateDrawFlatUniforms( program, rsh.wallColor, rsh.floorColor );
		}

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_Distortion
*/
static void RB_RenderMeshGLSL_Distortion( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int i;
	int width = 1, height = 1;
	int program;
	image_t *portaltexture[2];
	bool frontPlane;
	mat4_t texMatrix;
	const image_t *dudvmap, *normalmap;

	if( !rb.currentPortalSurface ) {
		return;
	}

	for( i = 0; i < 2; i++ ) {
		portaltexture[i] = rb.currentPortalSurface->texures[i];
		if( !portaltexture[i] ) {
			portaltexture[i] = rsh.blackTexture;
		} else {
			width = portaltexture[i]->upload_width;
			height = portaltexture[i]->upload_height;
		}
	}

	dudvmap = pass->images[0] && !pass->images[0]->missing ? pass->images[0] : rsh.blankBumpTexture;
	normalmap = pass->images[1] && !pass->images[1]->missing ? pass->images[1] : rsh.blankBumpTexture;

	if( dudvmap != rsh.blankBumpTexture ) {
		programFeatures |= GLSL_SHADER_DISTORTION_DUDV;
	}
	if( portaltexture[0] != rsh.blackTexture ) {
		programFeatures |= GLSL_SHADER_DISTORTION_REFLECTION;
	}
	if( portaltexture[1] != rsh.blackTexture ) {
		programFeatures |= GLSL_SHADER_DISTORTION_REFRACTION;
	}

	frontPlane = ( PlaneDiff( rb.cameraOrigin, &rb.currentPortalSurface->untransformed_plane ) > 0 ? true : false );

	if( frontPlane ) {
		if( pass->alphagen.type != ALPHA_GEN_IDENTITY ) {
			programFeatures |= GLSL_SHADER_DISTORTION_DISTORTION_ALPHA;
		}
	}

	Matrix4_Identity( texMatrix );

	RB_BindImage( 0, dudvmap );

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	programFeatures |= RB_FogProgramFeatures( pass, rb.fog );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	if( normalmap != rsh.blankBumpTexture ) {
		// eyeDot
		programFeatures |= GLSL_SHADER_DISTORTION_EYEDOT;

		RB_BindImage( 1, normalmap );
	}

	RB_BindImage( 2, portaltexture[0] );           // reflection
	RB_BindImage( 3, portaltexture[1] );           // refraction

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_DISTORTION, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateDistortionUniforms( program, frontPlane );

		RP_UpdateTextureUniforms( program, width, height );

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_Shadow
*/
static void RB_RenderMeshGLSL_Shadow( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int program;
	mat4_t texMatrix;
	const image_t *base;

	if( glConfig.ext.shadow ) {
		programFeatures |= GLSL_SHADER_COMMON_SHADOWMAP_SAMPLERS;
	} else if( glConfig.ext.rgb8_rgba8 ) {
		programFeatures |= GLSL_SHADER_COMMON_RGBSHADOW_24BIT;
	}

	if( pass->flags & SHADERPASS_ALPHAFUNC ) {
		base = RB_ShaderpassTex( pass );
	} else {
		base = rsh.whiteTexture;
	}

	RB_BindImage( 0, base );

	Matrix4_Identity( texMatrix );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_SHADOW, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RB_DrawElementsReal( &rb.drawElements );
	}
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
	const mfog_t *fog = rb.fog;
	bool isWorldSurface = rb.currentModelType == mod_brush ? true : false;
	const superLightStyle_t *lightStyle = rb.superLightStyle;
	const superLightStyle_t *realLightStyle = rb.realSuperLightStyle;
	const entity_t *e = rb.currentEntity;
	bool isLightmapped = false, isWorldVertexLight = false, applyLighting = false;
	vec3_t lightDir;
	vec4_t lightAmbient, lightDiffuse;
	mat4_t texMatrix, genVectors;
	bool noDlight = ( ( rb.surfFlags & (SURF_SKY|SURF_NODLIGHT) ) != 0 );

	if( isWorldSurface ) {
		if( rb.mode == RB_MODE_DIFFUSE ) {
			return;
		}

		if( rgbgen == RGB_GEN_VERTEX || rgbgen == RGB_GEN_EXACT_VERTEX ) {
			// vertex-lit world surface
			isWorldVertexLight = ( realLightStyle && realLightStyle->vertexStyles[0] != 255 ) || ( noDlight == false );
		} else if( ( rgbgen == RGB_GEN_IDENTITY
		  || rgbgen == RGB_GEN_CONST
		  || rgbgen == RGB_GEN_WAVE
		  || rgbgen == RGB_GEN_CUSTOMWAVE
		  || rgbgen == RGB_GEN_VERTEX
		  || rgbgen == RGB_GEN_ONE_MINUS_VERTEX
		  || rgbgen == RGB_GEN_EXACT_VERTEX ) &&
		( rb.currentShader->flags & SHADER_LIGHTMAP ) &&
		( pass->flags & GLSTATE_BLEND_ADD ) != GLSTATE_BLEND_ADD &&
		( pass->flags & ( GLSTATE_SRCBLEND_SRC_ALPHA ) ) != GLSTATE_SRCBLEND_SRC_ALPHA ) {
			// lightmapped surface pass
			isLightmapped = ( realLightStyle && realLightStyle->lightmapStyles[0] != 255 ) || ( noDlight == false );
		}
	}

	// possibly apply the fog inline
	if( fog == rb.texFog ) {
		if( rb.currentShader->numpasses == 1 || ( isLightmapped && rb.currentShader->numpasses == 2 ) ) {
			rb.texFog = NULL;
		} else {
			fog = NULL;
		}
	}
	programFeatures |= RB_FogProgramFeatures( pass, fog );

	// diffuse lighting for entities
	if( !isWorldSurface && rgbgen == RGB_GEN_LIGHTING_DIFFUSE ) {
		vec3_t temp = { 0.1f, 0.2f, 0.7f };
		float radius = 1;

		applyLighting = true;

		if( e->flags & RF_FULLBRIGHT ) {
			VectorSet( lightDir, 0, 0, 0 );
			Vector4Set( lightAmbient, 1, 1, 1, 1 );
			Vector4Set( lightDiffuse, 1, 1, 1, 1 );
		} else {
			if( e != rsc.worldent && e->model != NULL ) {
				radius = e->model->radius;
			}

			// get weighted incoming direction of world and dynamic lights
			R_LightForOrigin( e->lightingOrigin, temp, lightAmbient, lightDiffuse, radius * e->scale, 
				rb.noWorldLight, rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE );

			if( e->flags & RF_MINLIGHT ) {
				if( lightAmbient[0] <= 0.1f || lightAmbient[1] <= 0.1f || lightAmbient[2] <= 0.1f ) {
					VectorSet( lightAmbient, 0.1f, 0.1f, 0.1f );
				}
			}

			// rotate direction
			Matrix3_TransformVector( e->axis, temp, lightDir );
		}
	} else {
		VectorSet( lightDir, 0, 0, 0 );
		Vector4Set( lightAmbient, 1, 1, 1, 1 );
		Vector4Set( lightDiffuse, 1, 1, 1, 1 );
	}

	image = RB_ShaderpassTex( pass );
	if( rb.triangleOutlines || rb.noColorWrite || rb.mode == RB_MODE_DECALS ) {
		applyLighting = false;
	} else if( !applyLighting ) {
		applyLighting = ( isLightmapped || isWorldVertexLight );
	}

	if( !applyLighting && ( rb.mode == RB_MODE_LIGHT || rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE ) ) {
		return;
	}
	if( applyLighting && ( rb.mode == RB_MODE_POST_LIGHT ) ) {
		return;
	}

	if( applyLighting ) {
		if( DRAWFLAT() ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT;
		}
		if( rb.renderFlags & RF_LIGHTMAP ) {
			image = rsh.whiteTexture;
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

	if( ( isLightmapped || isWorldVertexLight ) &&
		!rb.doneDepthPass &&
		!( state & GLSTATE_DEPTHWRITE ) &&
		Shader_DepthWrite( rb.currentShader ) ) {
		if( !( pass->flags & SHADERPASS_ALPHAFUNC ) ) {
			state &= ~GLSTATE_BLEND_MASK;
		}
		state |= GLSTATE_DEPTHWRITE;
	}

	if( rb.mode == RB_MODE_DEPTH ) {
		if( !(state & GLSTATE_DEPTHWRITE) ) {
			return;
		}
	}

	RB_SetState( RB_GetShaderpassState( state ) );

	RB_BindImage( 0, image );

	if( programFeatures & GLSL_SHADER_COMMON_SOFT_PARTICLE ) {
		RB_BindImage( 3, rb.st.screenDepthTexCopy );
	}

	if( applyLighting ) {
		programFeatures |= GLSL_SHADER_COMMON_LIGHTING;

		if( rb.mode == RB_MODE_LIGHT ) {
			programFeatures |= RB_RtlightbitsToProgramFeatures();

			if( programFeatures & GLSL_SHADER_COMMON_DLIGHT_CUBEFILTER ) {
				RB_BindImage( 5, rb.rtlights[0]->cubemapFilter );
			}
		} else {
			if( isWorldVertexLight ) {
				programFeatures |= GLSL_SHADER_COMMON_VERTEX_LIGHTING;
			}

			if( isLightmapped && lightStyle && lightStyle->lightmapStyles[0] != 255 ) {
				int i;

				// bindr lightmap textures and set program's features for lightstyles
				for( i = 0; i < MAX_LIGHTMAPS && lightStyle->lightmapStyles[i] != 255; i++ )
					RB_BindImage( i + 4, rsh.worldBrushModel->lightmapImages[lightStyle->lightmapNum[i]] ); // lightmap
				programFeatures |= ( i * GLSL_SHADER_Q3_LIGHTSTYLE0 );
				if( mapConfig.lightmapArrays ) {
					programFeatures |= GLSL_SHADER_Q3_LIGHTMAP_ARRAYS;
				}
			}
		}
	}

	if( programFeatures & GLSL_SHADER_COMMON_REALTIME_SHADOWS ) {
		RB_BindImage( 7, rsh.shadowmapAtlasTexture );
	}

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_Q3A_SHADER, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateTexGenUniforms( program, texMatrix, genVectors );

		if( isWorldSurface || rgbgen == RGB_GEN_LIGHTING_DIFFUSE ) {
			RP_UpdateDiffuseLightUniforms( program, lightDir, lightAmbient, lightDiffuse );
		}

		if( programFeatures & GLSL_SHADER_COMMON_FOG ) {
			RB_UpdateFogUniforms( program, fog );
		}

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RP_UpdateLightstyleUniforms( program, lightStyle );

		// dynamic lights
		if( applyLighting ) {
			RP_UpdateRealtimeLightsUniforms( program, rb.lightDir, rb.objectToLightMatrix,
				rb.numRealtimeLights, rb.rtlights, 0, NULL );
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
* RB_RenderMeshGLSL_Fog
*/
static void RB_RenderMeshGLSL_Fog( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int program;
	const mfog_t *fog = rb.fog;
	mat4_t texMatrix = { 0 };

	programFeatures |= GLSL_SHADER_COMMON_FOG;

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_FOG, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RB_UpdateFogUniforms( program, fog );

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_FXAA
*/
static void RB_RenderMeshGLSL_FXAA( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	bool fxaa3 = false;
	int program;
	const image_t *image = pass->images[0];
	mat4_t texMatrix;

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	Matrix4_Identity( texMatrix );

	RB_BindImage( 0, image );

	if( glConfig.ext.gpu_shader5 ) {
		fxaa3 = true;
	}
#ifdef GL_ES_VERSION_2_0
	if( glConfig.shadingLanguageVersion >= 310 ) {
		fxaa3 = true;
	}
#endif
	if( fxaa3 ) {
		programFeatures |= GLSL_SHADER_FXAA_FXAA3;
	}

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_FXAA, NULL,
								  rb.currentShader->deformsKey, rb.currentShader->deforms,
								  rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateTextureUniforms( program, image->upload_width, image->upload_height );

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_YUV
*/
static void RB_RenderMeshGLSL_YUV( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int program;
	mat4_t texMatrix = { 0 };

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetState( RB_GetShaderpassState( pass->flags ) );

	RB_BindImage( 0, pass->images[0] );
	RB_BindImage( 1, pass->images[1] );
	RB_BindImage( 2, pass->images[2] );

	// update uniforms
	program = RB_RegisterProgram( GLSL_PROGRAM_TYPE_YUV, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, 0 );

	if( RB_BindProgram( program ) ) {
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RB_DrawElementsReal( &rb.drawElements );
	}
}

/*
* RB_RenderMeshGLSL_ColorCorrection
*/
static void RB_RenderMeshGLSL_ColorCorrection( const shaderpass_t *pass, r_glslfeat_t programFeatures ) {
	int i;
	int program;
	mat4_t texMatrix;

	programFeatures &= ~GLSL_SHADER_COMMON_SRGB2LINEAR;
	if( pass->images[1] ) { // lut
		programFeatures |= GLSL_SHADER_COLOR_CORRECTION_LUT;
	}
	if( pass->images[2] ) { // output bloom
		programFeatures |= GLSL_SHADER_COLOR_CORRECTION_OVERBRIGHT;
	}
	if( pass->images[3] ) { // apply bloom
		programFeatures |= GLSL_SHADER_COLOR_CORRECTION_BLOOM;
	}

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
	if( pass->images[1] ) {
		RB_BindImage( 1, pass->images[1] );
	}
	for( i = 0; i < NUM_BLOOM_LODS; i++ ) {
		if( pass->images[3 + i] ) {
			RB_BindImage( 2 + i, pass->images[3 + i] );
		}
	}

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
#ifdef GL_ES_VERSION_2_0
	if( glConfig.ext.fragment_precision_high ) {
		features |= GLSL_SHADER_COMMON_FRAGMENT_HIGHP;
	}
#endif

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
		case GLSL_PROGRAM_TYPE_DISTORTION:
			RB_RenderMeshGLSL_Distortion( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_SHADOW:
			RB_RenderMeshGLSL_Shadow( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_Q3A_SHADER:
			RB_RenderMeshGLSL_Q3AShader( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_FOG:
			RB_RenderMeshGLSL_Fog( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_FXAA:
			RB_RenderMeshGLSL_FXAA( pass, features );
			break;
		case GLSL_PROGRAM_TYPE_YUV:
			RB_RenderMeshGLSL_YUV( pass, features );
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
	if( rb.superLightStyle ) {
		vattribs |= rb.superLightStyle->vattribs;
	}
	if( rb.bonesData.numBones ) {
		vattribs |= VATTRIB_BONES_BITS;
	}
	if( DRAWFLAT() ) {
		vattribs |= VATTRIB_NORMAL_BIT;
	}
	if( rb.numRealtimeLights ) {
		vattribs |= VATTRIB_NORMAL_BIT;
	}

	rb.currentVAttribs = vattribs;
}

/*
* RB_BindShader
*/
void RB_BindShader( const entity_t *e, const shader_t *shader, const mfog_t *fog ) {
	if( rb.mode == RB_MODE_BLACK_GT ) {
		shader = rsh.whiteShader;
	}

	if( !rb.dirtyUniformState ) {
		if( rb.currentEntity == e && rb.currentShader == shader && rb.fog == fog ) {
			return;
		}
	}

	rb.currentShader = shader;
	rb.fog = fog;
	rb.texFog = rb.colorFog = NULL;

	rb.doneDepthPass = false;
	rb.dirtyUniformState = true;

	rb.currentEntity = e ? e : &rb.nullEnt;
	rb.currentModelType = rb.currentEntity->rtype == RT_MODEL && rb.currentEntity->model ? rb.currentEntity->model->type : mod_bad;
	rb.superLightStyle = NULL;

	rb.bonesData.numBones = 0;
	rb.bonesData.maxWeights = 0;

	rb.currentPortalSurface = NULL;

	rb.skyboxShader = NULL;
	rb.skyboxSide = -1;

	rb.surfFlags = SURF_NODLIGHT;

	if( rb.numRealtimeLights )
	{
		vec3_t tvec;
		rtlight_t *rl = rb.rtlights[0];
		if( rl->directional ) {
			VectorNegate( &rl->axis[AXIS_FORWARD], tvec );
		} else {
			VectorSubtract( rl->origin, rb.currentEntity->origin, tvec );
		}
		Matrix3_TransformVector( rb.currentEntity->axis, tvec, rb.lightDir );
	}

	if( !e ) {
		rb.currentShaderTime = rb.nullEnt.shaderTime * 0.001;
		rb.alphaHack = false;
		rb.greyscale = false;
		rb.noDepthTest = false;
		rb.noColorWrite = false;
		rb.depthEqual = false;
	} else {
		Vector4Copy( rb.currentEntity->shaderRGBA, rb.entityColor );
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
	if( rb.mode == RB_MODE_LIGHT || rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE ) {
		rb.depthEqual = true;
	}

	if( fog && fog->shader && !rb.noColorWrite ) {
		// should we fog the geometry with alpha texture or scale colors?
		if( !rb.alphaHack && Shader_UseTextureFog( shader ) ) {
			rb.texFog = fog;
		} else {
			// use scaling of colors
			rb.colorFog = fog;
		}
	}

	RB_UpdateVertexAttribs();
}

/*
* RB_SetLightstyle
*/
void RB_SetLightstyle( const superLightStyle_t *lightStyle, const superLightStyle_t *realLightStyle ) {
	if( rb.triangleOutlines ) {
		rb.superLightStyle = NULL;
		return;
	}

	if( rb.superLightStyle == lightStyle && rb.realSuperLightStyle == realLightStyle ) {
		return;
	}

	assert( rb.currentShader != NULL );
	rb.superLightStyle = lightStyle;
	rb.realSuperLightStyle = realLightStyle;
	rb.dirtyUniformState = true;

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
* RB_SetPortalSurface
*/
void RB_SetPortalSurface( const portalSurface_t *portalSurface ) {
	if( rb.currentPortalSurface == portalSurface ) {
		return;
	}
	assert( rb.currentShader != NULL );
	rb.currentPortalSurface = portalSurface;
	rb.dirtyUniformState = true;
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
* RB_SetRtLightParams
*/
void RB_SetRtLightParams( unsigned numRtLights, rtlight_t **rtlights, unsigned numSurfs, unsigned *surfRtLightBits ) {
	if( r_lighting_maxglsldlights->integer >= 0 && numRtLights > (unsigned)r_lighting_maxglsldlights->integer ) {
		numRtLights = r_lighting_maxglsldlights->integer;
	}
	if( numRtLights > MAX_DRAWSURF_RTLIGHTS ) {
		numRtLights = MAX_DRAWSURF_RTLIGHTS;
	}
	if( numSurfs > MAX_DRAWSURF_SURFS ) {
		numSurfs = MAX_DRAWSURF_SURFS;
	}

	if( rb.triangleOutlines || ( rb.renderFlags & RF_SHADOWMAPVIEW ) ) {
		numSurfs = 0;
		numRtLights = 0;
	}

	if( rb.numRealtimeLights == 0 && numRtLights == 0 ) {
		return;
	}

	rb.numRealtimeLights = numRtLights;
	if( rtlights )
		memcpy( rb.rtlights, rtlights, numRtLights * sizeof( *rtlights ) );

	rb.dirtyUniformState = true;

	RB_UpdateVertexAttribs();
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
		qglUseProgram( 0 );
		return 0;
	}

	object = RP_GetProgramObject( program );
	if( object ) {
		qglUseProgram( object );
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
	// for depth texture we render light's view to, ignore passes that do not write into depth buffer
	if( ( rb.renderFlags & RF_SHADOWMAPVIEW ) && !( pass->flags & GLSTATE_DEPTHWRITE ) ) {
		return;
	}

	if( rb.renderFlags & RF_SHADOWMAPVIEW ) {
		RB_RenderMeshGLSLProgrammed( pass, GLSL_PROGRAM_TYPE_SHADOW );
	} else if( pass->program_type ) {
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
	if( !gl_cull->integer || rb.currentEntity->rtype == RT_SPRITE ) {
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

	if( rb.mode == RB_MODE_LIGHT || rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE ) {
		state &= ~(GLSTATE_DEPTHWRITE|GLSTATE_DSTBLEND_MASK);
		if( !(state & GLSTATE_OFFSET_FILL) ) {
			state |= GLSTATE_DEPTHFUNC_EQ;
		}
		state |= GLSTATE_SRCBLEND_SRC_ALPHA | GLSTATE_DSTBLEND_ONE;
	} else if( rb.mode == RB_MODE_BLACK_GT ) {
		state &= ~(GLSTATE_BLEND_MASK|GLSTATE_DEPTHWRITE);
		state |= GLSTATE_DEPTHFUNC_GT | GLSTATE_SRCBLEND_ZERO | GLSTATE_DSTBLEND_ZERO;
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
#ifndef GL_ES_VERSION_2_0
	static shaderpass_t r_triLinesPass;
	static vec4_t r_triLinesColor;
	shaderpass_t *pass;

	if( RB_CleanSinglePass() ) {
		return;
	}

	Vector4Copy( RB_TriangleLinesColor(), r_triLinesColor );

	if( !rb.currentShader->numpasses ) {
		// happens on fog volumes
		pass = &r_GLSLpasses[BUILTIN_GLSLPASS_FOG];
	} else {
		pass = &rb.currentShader->passes[0];
	}

	// set some flags
	rb.colorFog = rb.texFog = NULL;
	rb.superLightStyle = NULL;

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
#endif
}

/*
* RB_DrawShadedElements
*/
void RB_DrawShadedElements( void ) {
	unsigned i;
	shaderpass_t *pass;

	if( ( rb.mode == RB_MODE_DEPTH ) && !( rb.currentShader->flags & SHADER_DEPTHWRITE ) ) {
		return;
	}
	if( RB_CleanSinglePass() ) {
		return;
	}

	RB_SetShaderState();

	for( i = 0, pass = rb.currentShader->passes; i < rb.currentShader->numpasses; i++, pass++ ) {
		if( rb.mode == RB_MODE_DECALS ) {
			int state = RB_GetShaderpassState( pass->flags );
			if( !(state & GLSTATE_BLEND_MASK) ) {
				continue;
			}
		}

		if( ( pass->flags & SHADERPASS_DETAIL ) && !r_detailtextures->integer ) {
			continue;
		}

		if( pass->flags & SHADERPASS_LIGHTMAP ) {
			continue;
		}

		RB_RenderPass( pass );
	}

	if( rb.mode == RB_MODE_DEPTH || rb.mode == RB_MODE_TRIANGLE_OUTLINES || rb.mode == RB_MODE_LIGHT || rb.mode == RB_MODE_LIGHTMAP || rb.mode == RB_MODE_DIFFUSE ) {
		goto end;
	}

	// fog
	if( rb.texFog && rb.texFog->shader ) {
		shaderpass_t *fogPass = &r_GLSLpasses[BUILTIN_GLSLPASS_FOG];

		fogPass->images[0] = rsh.whiteTexture;
		if( !rb.currentShader->numpasses || rb.currentShader->fog_dist || ( rb.currentShader->flags & SHADER_SKY ) ) {
			fogPass->flags &= ~GLSTATE_DEPTHFUNC_EQ;
		} else {
			fogPass->flags |= GLSTATE_DEPTHFUNC_EQ;
		}
		RB_RenderPass( fogPass );
	}

end:
	rb.dirtyUniformState = rb.donePassesTotal != 1;
}
