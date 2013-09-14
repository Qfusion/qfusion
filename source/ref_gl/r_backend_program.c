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

#define FTABLE_SIZE_POW	12
#define FTABLE_SIZE	( 1<<FTABLE_SIZE_POW )
#define FTABLE_CLAMP( x ) ( ( (int)( ( x )*FTABLE_SIZE ) & ( FTABLE_SIZE-1 ) ) )
#define FTABLE_EVALUATE( table, x ) ( ( table )[FTABLE_CLAMP( x )] )

enum
{
	BUILTIN_GLSLPASS_FOG,
	BUILTIN_GLSLPASS_SHADOWMAP,
	BUILTIN_GLSLPASS_OUTLINE,
	BUILTIN_GLSLPASS_SKYBOX,
	MAX_BUILTIN_GLSLPASSES
};

static float rb_sintable[FTABLE_SIZE];
static float rb_triangletable[FTABLE_SIZE];
static float rb_squaretable[FTABLE_SIZE];
static float rb_sawtoothtable[FTABLE_SIZE];
static float rb_inversesawtoothtable[FTABLE_SIZE];

#define NOISE_SIZE	256
#define NOISE_VAL( a )	  rb_noiseperm[( a ) & ( NOISE_SIZE - 1 )]
#define NOISE_INDEX( x, y, z, t ) NOISE_VAL( x + NOISE_VAL( y + NOISE_VAL( z + NOISE_VAL( t ) ) ) )
#define NOISE_LERP( a, b, w ) ( a * ( 1.0f - w ) + b * w )

static float rb_noisetable[NOISE_SIZE];
static int rb_noiseperm[NOISE_SIZE];

static shaderpass_t r_GLSLpasses[MAX_BUILTIN_GLSLPASSES];

static void RB_SetShaderpassState( int state );

/*
* RB_InitBuiltinPasses
*/
static void RB_InitBuiltinPasses( void )
{
	shaderpass_t *pass;

	// init optional GLSL program passes
	memset( r_GLSLpasses, 0, sizeof( r_GLSLpasses ) );

	// init fog pass
	pass = &r_GLSLpasses[BUILTIN_GLSLPASS_FOG];
	pass->program_type = GLSL_PROGRAM_TYPE_Q3A_SHADER;
	pass->tcgen = TC_GEN_FOG;
	pass->rgbgen.type = RGB_GEN_FOG;
	pass->alphagen.type = ALPHA_GEN_IDENTITY;
	pass->flags = GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	pass->program_type = GLSL_PROGRAM_TYPE_FOG;

	// shadowmap
	pass = &r_GLSLpasses[BUILTIN_GLSLPASS_SHADOWMAP];
	pass->flags = GLSTATE_DEPTHFUNC_EQ /*|GLSTATE_OFFSET_FILL*/|GLSTATE_SRCBLEND_ZERO|GLSTATE_DSTBLEND_SRC_COLOR;
	pass->tcgen = TC_GEN_NONE;
	pass->rgbgen.type = RGB_GEN_IDENTITY;
	pass->alphagen.type = ALPHA_GEN_IDENTITY;
	pass->program_type = GLSL_PROGRAM_TYPE_SHADOWMAP;

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
void RB_InitShading( void )
{
	int i;
	float t;

	// build lookup tables
	for( i = 0; i < FTABLE_SIZE; i++ ) {
		t = (float)i / (float)FTABLE_SIZE;

		rb_sintable[i] = sin( t * M_TWOPI );

		if( t < 0.25 )
			rb_triangletable[i] = t * 4.0;
		else if( t < 0.75 )
			rb_triangletable[i] = 2 - 4.0 * t;
		else
			rb_triangletable[i] = ( t - 0.75 ) * 4.0 - 1.0;

		if( t < 0.5 )
			rb_squaretable[i] = 1.0f;
		else
			rb_squaretable[i] = -1.0f;

		rb_sawtoothtable[i] = t;
		rb_inversesawtoothtable[i] = 1.0 - t;
	}

	// init the noise table
	srand( 1001 );

	for( i = 0; i < NOISE_SIZE; i++ ) {
		rb_noisetable[i] = (float)( ( ( rand() / (float)RAND_MAX ) * 2.0 - 1.0 ) );
		rb_noiseperm[i] = (unsigned char)( rand() / (float)RAND_MAX * 255 );
	}

	RB_InitBuiltinPasses();
}

/*
* RB_FastSin
*/
static inline float RB_FastSin( float t )
{
	return FTABLE_EVALUATE( rb_sintable, t );
}

/*
* RB_TableForFunc
*/
static float *RB_TableForFunc( unsigned int func )
{
	switch( func )
	{
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
static float RB_BackendGetNoiseValue( float x, float y, float z, float t )
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4], back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = fast_ftol( floor( x ) );
	fx = x - ix;
	iy = fast_ftol( floor( y ) );
	fy = y - iy;
	iz = fast_ftol( floor( z ) );
	fz = z - iz;
	it = fast_ftol( floor( t ) );
	ft = t - it;

	for( i = 0; i < 2; i++ )
	{
		front[0] = rb_noisetable[NOISE_INDEX( ix, iy, iz, it + i )];
		front[1] = rb_noisetable[NOISE_INDEX( ix+1, iy, iz, it + i )];
		front[2] = rb_noisetable[NOISE_INDEX( ix, iy+1, iz, it + i )];
		front[3] = rb_noisetable[NOISE_INDEX( ix+1, iy+1, iz, it + i )];

		back[0] = rb_noisetable[NOISE_INDEX( ix, iy, iz + 1, it + i )];
		back[1] = rb_noisetable[NOISE_INDEX( ix+1, iy, iz + 1, it + i )];
		back[2] = rb_noisetable[NOISE_INDEX( ix, iy+1, iz + 1, it + i )];
		back[3] = rb_noisetable[NOISE_INDEX( ix+1, iy+1, iz + 1, it + i )];

		fvalue = NOISE_LERP( NOISE_LERP( front[0], front[1], fx ), NOISE_LERP( front[2], front[3], fx ), fy );
		bvalue = NOISE_LERP( NOISE_LERP( back[0], back[1], fx ), NOISE_LERP( back[2], back[3], fx ), fy );
		value[i] = NOISE_LERP( fvalue, bvalue, fz );
	}

	finalvalue = NOISE_LERP( value[0], value[1], ft );

	return finalvalue;
}

/*
* RB_TransformFogPlanes
*/
void RB_TransformFogPlanes( const mfog_t *fog, vec3_t fogNormal, vec_t *fogDist, vec3_t vpnNormal, vec_t *vpnDist )
{
	cplane_t *fogPlane;
	shader_t *fogShader;
	vec3_t viewtofog;
	float dist;
	const entity_t *e = rb.currentEntity;

	assert( fog );
	assert( fogNormal && fogDist );
	assert( vpnNormal && vpnDist );

	fogPlane = fog->visibleplane;
	fogShader = fog->shader;

	// distance to fog
	dist = ri.fog_dist_to_eye[fog-r_worldbrushmodel->fogs];

	if( rb.currentShader->flags & SHADER_SKY )
	{
		if( dist > 0 )
			VectorMA( ri.viewOrigin, -dist, fogPlane->normal, viewtofog );
		else
			VectorCopy( ri.viewOrigin, viewtofog );
	}
	else
	{
		VectorCopy( e->origin, viewtofog );
	}

	// some math tricks to take entity's rotation matrix into account
	// for fog texture coordinates calculations:
	// M is rotation matrix, v is vertex, t is transform vector
	// n is plane's normal, d is plane's dist, r is view origin
	// (M*v + t)*n - d = (M*n)*v - ((d - t*n))
	// (M*v + t - r)*n = (M*n)*v - ((r - t)*n)
	Matrix3_TransformVector( e->axis, fogPlane->normal, fogNormal );
	VectorScale( fogNormal, e->scale, fogNormal );
	*fogDist = ( fogPlane->dist - DotProduct( viewtofog, fogPlane->normal ) );

	Matrix3_TransformVector( e->axis, ri.viewAxis, vpnNormal );
	VectorScale( vpnNormal, e->scale, vpnNormal );
	*vpnDist = ( ( ri.viewOrigin[0] - viewtofog[0] ) * ri.viewAxis[AXIS_FORWARD+0] + 
		( ri.viewOrigin[1] - viewtofog[1] ) * ri.viewAxis[AXIS_FORWARD+1] + 
		( ri.viewOrigin[2] - viewtofog[2] ) * ri.viewAxis[AXIS_FORWARD+2] ) + 
		fogShader->fog_clearDist;
}

/*
* RB_VertexTCCelshadeMatrix
*/
void RB_VertexTCCelshadeMatrix( mat4_t matrix )
{
	vec3_t dir;
	mat4_t m;
	const entity_t *e = rb.currentEntity;

	if( e->model != NULL && !( ri.params & RP_SHADOWMAPVIEW ) )
	{
		R_LightForOrigin( e->lightingOrigin, dir, NULL, NULL, e->model->radius * e->scale );

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
void RB_ApplyTCMods( const shaderpass_t *pass, mat4_t result )
{
	int i;
	const float *table;
	double t1, t2, sint, cost;
	mat4_t m1, m2;
	const tcmod_t *tcmod;

	for( i = 0, tcmod = pass->tcmods; i < pass->numtcmods; i++, tcmod++ )
	{
		switch( tcmod->type )
		{
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
			if( pass->program_type != GLSL_PROGRAM_TYPE_TURBULENCE )
			{
				t1 = ( 1.0 / 4.0 );
				t2 = tcmod->args[2] + rb.currentShaderTime * tcmod->args[3];
				Matrix4_Scale2D( result, 1 + ( tcmod->args[1] * RB_FastSin( t2 ) + tcmod->args[0] ) * t1, 1 + ( tcmod->args[1] * RB_FastSin( t2 + 0.25 ) + tcmod->args[0] ) * t1 );
			}
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
			if( pass->program_type != GLSL_PROGRAM_TYPE_DISTORTION )
			{	// HACK HACK HACK
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
void RB_GetShaderpassColor( const shaderpass_t *pass, byte_vec4_t rgba_ )
{
	int c;
	long rgba[4];
	double temp;
	float *table, a;
	vec3_t v;
	const shaderfunc_t *rgbgenfunc = pass->rgbgen.func;
	const shaderfunc_t *alphagenfunc = pass->alphagen.func;

	Vector4Set( rgba, 255, 255, 255, 255 );

	switch( pass->rgbgen.type )
	{
	case RGB_GEN_IDENTITY:
		break;
	case RGB_GEN_CONST:
		rgba[0] = fast_ftol( pass->rgbgen.args[0] * 255.0f );
		rgba[1] = fast_ftol( pass->rgbgen.args[1] * 255.0f );
		rgba[2] = fast_ftol( pass->rgbgen.args[2] * 255.0f );
		break;
	case RGB_GEN_ENTITYWAVE:
	case RGB_GEN_WAVE:
	case RGB_GEN_CUSTOMWAVE:
		
		if( !rgbgenfunc || rgbgenfunc->type == SHADER_FUNC_NONE )
		{
			temp = 1;
		}
		else if( rgbgenfunc->type == SHADER_FUNC_RAMP )
		{
			break;
		}
		else if( rgbgenfunc->args[1] == 0 )
		{
			temp = rgbgenfunc->args[0];
		}
		else
		{
			if( rgbgenfunc->type == SHADER_FUNC_NOISE )
			{
				temp = RB_BackendGetNoiseValue( 0, 0, 0, ( rb.currentShaderTime + rgbgenfunc->args[2] ) * rgbgenfunc->args[3] );
			}
			else
			{
				table = RB_TableForFunc( rgbgenfunc->type );
				temp = rb.currentShaderTime * rgbgenfunc->args[3] + rgbgenfunc->args[2];
				temp = FTABLE_EVALUATE( table, temp ) * rgbgenfunc->args[1] + rgbgenfunc->args[0];
			}
			temp = temp * rgbgenfunc->args[1] + rgbgenfunc->args[0];
		}
	
		if( pass->rgbgen.type == RGB_GEN_ENTITYWAVE )
		{
			VectorSet( v,
				rb.entityColor[0] * (1.0/255.0),
				rb.entityColor[1] * (1.0/255.0),
				rb.entityColor[2] * (1.0/255.0) );
		}
		else if( pass->rgbgen.type == RGB_GEN_CUSTOMWAVE )
		{
			c = R_GetCustomColor( (int)pass->rgbgen.args[0] );
			VectorSet( v,
				COLOR_R( c ) * (1.0 / 255.0),
				COLOR_G( c ) * (1.0 / 255.0),
				COLOR_B( c ) * (1.0 / 255.0) );
		}
		else
		{
			VectorCopy( pass->rgbgen.args, v );
		}

		a = v[0] * temp; rgba[0] = fast_ftol( a * 255.0f );
		a = v[1] * temp; rgba[1] = fast_ftol( a * 255.0f );
		a = v[2] * temp; rgba[2] = fast_ftol( a * 255.0f );
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

	switch( pass->alphagen.type )
	{
	case ALPHA_GEN_IDENTITY:
		break;
	case ALPHA_GEN_CONST:
		rgba[3] = fast_ftol( pass->alphagen.args[0] * 255.0f );
		break;
	case ALPHA_GEN_WAVE:
		if( !alphagenfunc || alphagenfunc->type == SHADER_FUNC_NONE )
		{
			a = 1;
		}
		else if( alphagenfunc->type == SHADER_FUNC_RAMP )
		{
			break;
		}
		else
		{
			if( alphagenfunc->type == SHADER_FUNC_NOISE )
			{
				a = RB_BackendGetNoiseValue( 0, 0, 0, ( rb.currentShaderTime + alphagenfunc->args[2] ) * alphagenfunc->args[3] );
			}
			else
			{
				table = RB_TableForFunc( alphagenfunc->type );
				a = alphagenfunc->args[2] + rb.currentShaderTime * alphagenfunc->args[3];
				a = FTABLE_EVALUATE( table, a );
			}

			a = a * alphagenfunc->args[1] + alphagenfunc->args[0];
		}

		rgba[3] = fast_ftol( a * 255.0f );
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

//==================================================================================

/*
* RB_RGBAlphaGenToProgramFeatures
*/
static int RB_RGBAlphaGenToProgramFeatures( const colorgen_t *rgbgen, const colorgen_t *alphagen )
{
	r_glslfeat_t programFeatures;

	programFeatures = 0;

	switch( rgbgen->type )
	{
		case RGB_GEN_VERTEX:
		case RGB_GEN_EXACT_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_VERTEX;
			break;
		case RGB_GEN_ONE_MINUS_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_ONE_MINUS_VERTEX;
			break;
		case RGB_GEN_WAVE:
		case RGB_GEN_CUSTOMWAVE:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_CONST;
			if( rgbgen->func && rgbgen->func->type == SHADER_FUNC_RAMP ) {
				programFeatures |= GLSL_SHADER_COMMON_RGB_DISTANCERAMP;
			}
			break;
		default:
			programFeatures |= GLSL_SHADER_COMMON_RGB_GEN_CONST;
			break;
	}

	switch( alphagen->type )
	{
		case ALPHA_GEN_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_VERTEX;
			break;
		case ALPHA_GEN_ONE_MINUS_VERTEX:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_ONE_MINUS_VERTEX;
			break;
		case ALPHA_GEN_WAVE:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_CONST;
			if( alphagen->func && alphagen->func->type == SHADER_FUNC_RAMP ) {
				programFeatures |= GLSL_SHADER_COMMON_ALPHA_DISTANCERAMP;
			}
			break;
		default:
			programFeatures |= GLSL_SHADER_COMMON_ALPHA_GEN_CONST;
			break;
	}

	return programFeatures;
}

/*
* RB_BonesTransformsToProgramFeatures
*/
static r_glslfeat_t RB_BonesTransformsToProgramFeatures( void )
{
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
* RB_DlightbitsToProgramFeatures
*/
static r_glslfeat_t RB_DlightbitsToProgramFeatures( unsigned int dlightBits )
{
	int numDlights;

	if( !dlightBits ) {
		return 0;
	}
	
	numDlights = Q_bitcount( dlightBits );
	if( r_lighting_maxglsldlights->integer && numDlights > r_lighting_maxglsldlights->integer ) {
		numDlights = r_lighting_maxglsldlights->integer;
	}

	if( numDlights <= 4 ) {
		return GLSL_SHADER_COMMON_DLIGHTS_4;
	}
	if( numDlights <= 8 ) {
		return GLSL_SHADER_COMMON_DLIGHTS_8;
	}
	if( numDlights <= 16 ) {
		return GLSL_SHADER_COMMON_DLIGHTS_16;
	}
	return GLSL_SHADER_COMMON_DLIGHTS_32;
}

/*
* RB_AutospriteProgramFeatures
*/
static r_glslfeat_t RB_AutospriteProgramFeatures( void )
{
	r_glslfeat_t programFeatures = 0;
	if( ( rb.currentVAttribs & VATTRIB_AUTOSPRITE2_BIT ) == VATTRIB_AUTOSPRITE2_BIT ) {
		programFeatures |= GLSL_SHADER_COMMON_AUTOSPRITE2;
	}
	else if( ( rb.currentVAttribs & VATTRIB_AUTOSPRITE_BIT ) == VATTRIB_AUTOSPRITE_BIT ) {
		programFeatures |= GLSL_SHADER_COMMON_AUTOSPRITE;
	}
	return programFeatures;
}

/*
* RB_InstancedArraysProgramFeatures
*/
static r_glslfeat_t RB_InstancedArraysProgramFeatures( void )
{
	r_glslfeat_t programFeatures = 0;
	if( ( rb.currentVAttribs & VATTRIB_INSTANCES_BIT ) == VATTRIB_INSTANCES_BIT ) {
		programFeatures |= GLSL_SHADER_COMMON_INSTANCED_ATTRIB_TRASNFORMS;
	} else if( rb.drawElements.numInstances ) {
		programFeatures |= GLSL_SHADER_COMMON_INSTANCED_TRASNFORMS;
	}
	return programFeatures;
}

/*
* RB_FogProgramFeatures
*/
static r_glslfeat_t RB_FogProgramFeatures( const shaderpass_t *pass, const mfog_t *fog )
{
	r_glslfeat_t programFeatures = 0;
	if( fog )
	{
		programFeatures |= GLSL_SHADER_COMMON_FOG;
		if( fog == rb.colorFog ) {
			programFeatures |= GLSL_SHADER_COMMON_FOG_RGB;
		}
	}
	return programFeatures;
}

/*
* RB_UpdateCommonUniforms
*/
static void RB_UpdateCommonUniforms( int program, const shaderpass_t *pass, mat4_t texMatrix )
{
	vec3_t entDist, entOrigin;
	byte_vec4_t constColor;
	const entity_t *e = rb.currentEntity;
	vec3_t tmp;
	vec2_t blendMix = { 0, 0 };

	VectorCopy( e->origin, entOrigin );
	VectorSubtract( ri.viewOrigin, e->origin, tmp );
	Matrix3_TransformVector( e->axis, tmp, entDist );

	// calculate constant color
	RB_GetShaderpassColor( pass, constColor );

	// apply modifications to texture coordinates
	if( pass->numtcmods )
	{
		RB_ApplyTCMods( pass, texMatrix );
	}

	RP_UpdateViewUniforms( program,
		rb.modelviewMatrix, rb.modelviewProjectionMatrix,
		ri.viewOrigin, ri.viewAxis, 
		ri.params & RP_MIRRORVIEW ? -1 : 1,
		ri.viewport,
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
			constColor[0] *= rb.hackedAlpha,constColor[1] *= rb.hackedAlpha, constColor[2] *= rb.hackedAlpha;
		}
	}

	RP_UpdateShaderUniforms( program, 
		rb.currentShaderTime, 
		entOrigin, entDist, rb.entityColor,
		constColor, 
		pass->rgbgen.func ? pass->rgbgen.func->args : pass->rgbgen.args, 
		pass->alphagen.func ? pass->alphagen.func->args : pass->alphagen.args,
		texMatrix );

	RP_UpdateBlendMixUniform( program, blendMix );

	RP_UpdateSoftParticlesUniforms( program, r_soft_particles_scale->value );
}
/*

* RB_UpdateFogUniforms
*/
static void RB_UpdateFogUniforms( int program, const mfog_t *fog )
{
	cplane_t fogPlane, vpnPlane;

	RB_TransformFogPlanes( fog, fogPlane.normal, &fogPlane.dist, vpnPlane.normal, &vpnPlane.dist );

	RP_UpdateFogUniforms( program, fog->shader->fog_color, fog->shader->fog_clearDist,
		fog->shader->fog_dist, &fogPlane, &vpnPlane, ri.fog_dist_to_eye[fog-r_worldbrushmodel->fogs] );
}

/*
* RB_RenderMeshGLSL_Material
*/
static void RB_RenderMeshGLSL_Material( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int i;
	int program;
	image_t *base, *normalmap, *glossmap, *decalmap, *entdecalmap;
	vec3_t lightDir = { 0.0f, 0.0f, 0.0f };
	vec4_t ambient = { 0.0f, 0.0f, 0.0f, 0.0f }, diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
	float offsetmappingScale, glossIntensity, glossExponent;
	const superLightStyle_t *lightStyle = NULL;
	const mfog_t *fog = rb.fog;
	qboolean applyDecal;
	mat4_t texMatrix;

	// handy pointers
	base = pass->anim_frames[0];
	normalmap = pass->anim_frames[1];
	glossmap = pass->anim_frames[2];
	decalmap = pass->anim_frames[3];
	entdecalmap = pass->anim_frames[4];

	assert( normalmap );

	if( normalmap->samples == 4 )
		offsetmappingScale = r_offsetmapping_scale->value * rb.currentShader->offsetmappingScale;
	else	// no alpha in normalmap, don't bother with offset mapping
		offsetmappingScale = 0;

	glossIntensity = rb.currentShader->glossIntensity ? rb.currentShader->glossIntensity : r_lighting_glossintensity->value;
	glossExponent = rb.currentShader->glossExponent ? rb.currentShader->glossExponent : r_lighting_glossexponent->value;

	applyDecal = decalmap != NULL;

	// possibly apply the "texture" fog inline
	if( fog == rb.texFog ) {
		if( ( rb.currentShader->numpasses == 1 ) && !rb.currentShadowBits ) {
			rb.texFog = NULL;
		}
		else {
			fog = NULL;
		}
	}
	programFeatures |= RB_FogProgramFeatures( pass, fog );

	if( rb.currentModelType == mod_brush )
	{
		// brush models
		if( !( r_offsetmapping->integer & 1 ) ) {
			offsetmappingScale = 0;
		}
		if( ri.params & RP_LIGHTMAP ) {
			programFeatures |= GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY;
		}
		if( ( ri.params & RP_DRAWFLAT ) && !( rb.currentShader->flags & SHADER_NODRAWFLAT ) ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT|GLSL_SHADER_MATERIAL_BASETEX_ALPHA_ONLY;
		}
	}
	else if( rb.currentModelType == mod_bad )
	{
		// polys
		if( !( r_offsetmapping->integer & 2 ) )
			offsetmappingScale = 0;
	}
	else
	{
		// regular models
		if( !( r_offsetmapping->integer & 4 ) )
			offsetmappingScale = 0;
	#ifdef CELSHADEDMATERIAL
		programFeatures |= GLSL_SHADER_MATERIAL_CELSHADING;
	#endif
	#ifdef HALFLAMBERTLIGHTING
		programFeatures |= GLSL_SHADER_MATERIAL_HALFLAMBERT;
	#endif
	}

	// add dynamic lights
	if( rb.currentDlightBits ) {
		programFeatures |= RB_DlightbitsToProgramFeatures( rb.currentDlightBits );
	}

	Matrix4_Identity( texMatrix );

	RB_BindTexture( 0, base );

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	// we only send S-vectors to GPU and recalc T-vectors as cross product
	// in vertex shader
	RB_BindTexture( 1, normalmap );         // normalmap

	if( glossmap && glossIntensity )
	{
		programFeatures |= GLSL_SHADER_MATERIAL_SPECULAR;
		RB_BindTexture( 2, glossmap ); // gloss
	}

	if( applyDecal )
	{
		programFeatures |= GLSL_SHADER_MATERIAL_DECAL;

		if( ri.params & RP_LIGHTMAP ) {
			decalmap = r_blacktexture;
			programFeatures |= GLSL_SHADER_MATERIAL_DECAL_ADD;
		}
		else {
			// if no alpha, use additive blending
			if( decalmap->samples == 3 )
				programFeatures |= GLSL_SHADER_MATERIAL_DECAL_ADD;
		}

		RB_BindTexture( 3, decalmap ); // decal
	}

	if( entdecalmap )
	{
		programFeatures |= GLSL_SHADER_MATERIAL_ENTITY_DECAL;

		// if no alpha, use additive blending
		if( entdecalmap->samples == 3 )
			programFeatures |= GLSL_SHADER_MATERIAL_ENTITY_DECAL_ADD;

		RB_BindTexture( 4, entdecalmap ); // decal
	}

	if( offsetmappingScale > 0 )
		programFeatures |= r_offsetmapping_reliefmapping->integer ? GLSL_SHADER_MATERIAL_RELIEFMAPPING : GLSL_SHADER_MATERIAL_OFFSETMAPPING;

	if( rb.currentModelType == mod_brush )
	{
		// world surface
		if( rb.superLightStyle && rb.superLightStyle->lightmapNum[0] >= 0 )
		{
			lightStyle = rb.superLightStyle;

			// bind lightmap textures and set program's features for lightstyles
			for( i = 0; i < MAX_LIGHTMAPS && lightStyle->lightmapStyles[i] != 255; i++ )
				RB_BindTexture( i+4, r_worldbrushmodel->lightmapImages[lightStyle->lightmapNum[i]] );

			programFeatures |= ( i * GLSL_SHADER_MATERIAL_LIGHTSTYLE0 );

			if( i == 1 && !mapConfig.lightingIntensity )
			{
				vec_t *rgb = rsc.lightStyles[lightStyle->lightmapStyles[0]].rgb;

				// GLSL_SHADER_MATERIAL_FB_LIGHTMAP indicates that there's no need to renormalize
				// the lighting vector for specular (saves 3 adds, 3 muls and 1 normalize per pixel)
				if( rgb[0] == 1 && rgb[1] == 1 && rgb[2] == 1 )
					programFeatures |= GLSL_SHADER_MATERIAL_FB_LIGHTMAP;
			}

			if( !VectorCompare( mapConfig.ambient, vec3_origin ) )
			{
				VectorCopy( mapConfig.ambient, ambient );
				programFeatures |= GLSL_SHADER_MATERIAL_AMBIENT_COMPENSATION;
			}
		}
		else
		{
			// vertex lighting
			programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT|GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT_MIX;
		}
	}
	else
	{
		vec3_t temp;

		programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT;

		if( rb.currentModelType == mod_bad )
		{
			programFeatures |= GLSL_SHADER_MATERIAL_DIRECTIONAL_LIGHT_FROM_NORMAL;

			VectorSet( lightDir, 0, 0, 0 );
			Vector4Set( ambient, 0, 0, 0, 0 );
			Vector4Set( diffuse, 1, 1, 1, 1 );
		}
		else
		{
			const entity_t *e = rb.currentEntity;

			if( e->flags & RF_FULLBRIGHT )
			{
				Vector4Set( ambient, 1, 1, 1, 1 );
				Vector4Set( diffuse, 1, 1, 1, 1 );
			}
			else
			{
				if( e->model && e != rsc.worldent )
				{
					// get weighted incoming direction of world and dynamic lights
					R_LightForOrigin( e->lightingOrigin, temp, ambient, diffuse, 
						e->model->radius * e->scale );
				}
				else
				{
					VectorSet( temp, 0.1f, 0.2f, 0.7f );
				}

				if( e->flags & RF_MINLIGHT )
				{
					if( ambient[0] <= 0.1f || ambient[1] <= 0.1f || ambient[2] <= 0.1f )
						VectorSet( ambient, 0.1f, 0.1f, 0.1f );
				}

				// rotate direction
				Matrix3_TransformVector( e->axis, temp, lightDir );
			}
		}
	}

	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_MATERIAL, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
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

		// dynamic lights
		RP_UpdateDynamicLightsUniforms( program, lightStyle, rb.currentEntity->origin, rb.currentEntity->axis, 
			rb.currentDlightBits );

		// r_drawflat
		if( programFeatures & GLSL_SHADER_COMMON_DRAWFLAT ) {
			RP_UpdateDrawFlatUniforms( program, rf.wallColor, rf.floorColor );
		}

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_Distortion
*/
static void RB_RenderMeshGLSL_Distortion( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int i;
	int width = 1, height = 1;
	int program;
	image_t *portaltexture[2];
	qboolean frontPlane;
	mat4_t texMatrix;

	if( !rb.currentPortalSurface ) {
		return;
	}

	for( i = 0; i < 2; i++ ) {
		portaltexture[i] = rb.currentPortalSurface->texures[i];
		if( !portaltexture[i] ) {
			portaltexture[i] = r_blacktexture;
		} else {
			width = portaltexture[i]->upload_width;
			height = portaltexture[i]->upload_height;
		}
	}

	if( pass->anim_frames[0] != r_blankbumptexture )
		programFeatures |= GLSL_SHADER_DISTORTION_DUDV;
	if( portaltexture[0] != r_blacktexture )
		programFeatures |= GLSL_SHADER_DISTORTION_REFLECTION;
	if( portaltexture[1] != r_blacktexture )
		programFeatures |= GLSL_SHADER_DISTORTION_REFRACTION;

	frontPlane = (PlaneDiff( ri.viewOrigin, &rb.currentPortalSurface->untransformed_plane ) > 0 ? qtrue : qfalse);

	if( frontPlane )
	{
		if( pass->alphagen.type != ALPHA_GEN_IDENTITY )
			programFeatures |= GLSL_SHADER_DISTORTION_DISTORTION_ALPHA;
	}

	Matrix4_Identity( texMatrix );

	RB_BindTexture( 0, pass->anim_frames[0] );  // dudvmap
	
	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	programFeatures |= RB_FogProgramFeatures( pass, rb.fog );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	if( pass->anim_frames[1] )
	{
		// eyeDot
		programFeatures |= GLSL_SHADER_DISTORTION_EYEDOT;

		RB_BindTexture( 1, pass->anim_frames[1] ); // normalmap
	}

	RB_BindTexture( 2, portaltexture[0] );           // reflection
	RB_BindTexture( 3, portaltexture[1] );           // refraction

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_DISTORTION, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateDistortionUniforms( program, frontPlane, width, height );

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_ShadowmapBatch
*
* Renders a batch of shadowmap groups in one pass
*/
static void RB_RenderMeshGLSL_ShadowmapArray( const shaderpass_t *pass, r_glslfeat_t programFeatures, 
		int numShadows, const shadowGroup_t **shadowGroups, int *scissor )
{
	int i;
	int program;
	mat4_t texMatrix;
	const shadowGroup_t *group;
	const image_t *shadowmap;

	assert( numShadows <= GLSL_SHADOWMAP_LIMIT );

	if( numShadows > GLSL_SHADOWMAP_LIMIT ) {
		numShadows = GLSL_SHADOWMAP_LIMIT;
	}

	// this will tell the program how many shaders we want to render
	if( numShadows > 1 ) {
		programFeatures |= GLSL_SHADER_SHADOWMAP_SHADOW2 << (numShadows - 2);
	}
	if( !glConfig.ext.shadow ) {
		// pack depth into RGB triplet of the colorbuffer
		programFeatures |= GLSL_SHADER_SHADOWMAP_RGB_SHADOW;
	}

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_SHADOWMAP, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( !RB_BindProgram( program ) )
		return;

	for( i = 0; i < numShadows; i++ ) {
		group = shadowGroups[i];
		shadowmap = group->shadowmap;

		RB_BindTexture( i, shadowmap );

		if( glConfig.ext.shadow ) {
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB );
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL );
		}
	}

	Matrix4_Identity( texMatrix );

	RB_Scissor( ri.refdef.x + scissor[0], ri.refdef.y + scissor[1], scissor[2] - scissor[0], scissor[3] - scissor[1] );

	RB_SetShaderpassState( pass->flags );

	RB_UpdateCommonUniforms( program, pass, texMatrix );

	RP_UpdateShadowsUniforms( program, numShadows, shadowGroups, rb.objectMatrix );

	// submit animation data
	if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
		RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
	}

	RB_DrawElementsReal();

	for( i--; i >= 0; i-- ) {
		RB_SelectTextureUnit( i );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE );
	}
}

/*
* RB_RenderMeshGLSL_RGBShadow
*/
static void RB_RenderMeshGLSL_RGBShadow( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int program;
	mat4_t texMatrix;

	Matrix4_Identity( texMatrix );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_RGB_SHADOW, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_Shadowmap
*
* Batch shadow groups so we can render up to 4 in one pass. 
* The downside of this approach is that scissoring won't be as useful.
*/
static void RB_RenderMeshGLSL_Shadowmap( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	unsigned int i, j;
	int scissor[4], old_scissor[4];
	int numShadows;
	shadowGroup_t *group, *shadowGroups[GLSL_SHADOWMAP_LIMIT];

	if( r_shadows_pcf->integer )
		programFeatures |= GLSL_SHADER_SHADOWMAP_PCF;
	if( r_shadows_dither->integer )
		programFeatures |= GLSL_SHADER_SHADOWMAP_DITHER;

	Vector4Copy( ri.scissor, old_scissor );

	numShadows = 0;
	for( i = 0; i < rsc.numShadowGroups; i++ )
	{
		vec3_t bbox[8];
		vec_t *visMins, *visMaxs;
		int groupScissor[4] = { 0, 0, 0, 0 };

		group = rsc.shadowGroups + i;
		if( !( rb.currentShadowBits & group->bit ) ) {
			continue;
		}

		// project the bounding box on to screen then use scissor test
		// so that fragment shader isn't run for unshadowed regions

		visMins = group->visMins;
		visMaxs = group->visMaxs;

		for( j = 0; j < 8; j++ ) {
			vec_t *corner = bbox[j];

			corner[0] = ( ( j & 1 ) ? visMins[0] : visMaxs[0] );
			corner[1] = ( ( j & 2 ) ? visMins[1] : visMaxs[1] );
			corner[2] = ( ( j & 4 ) ? visMins[2] : visMaxs[2] );
		}

		if( !R_ScissorForBounds( bbox, 
			&groupScissor[0], &groupScissor[1], &groupScissor[2], &groupScissor[3] ) )
			continue;

		// compute scissor in absolute coordinates
		if( !numShadows ) {
			Vector4Copy( groupScissor, scissor );
			scissor[2] += scissor[0];
			scissor[3] += scissor[1];
		}
		else {
			scissor[2] = max( scissor[2], groupScissor[0] + groupScissor[2] );
			scissor[3] = max( scissor[3], groupScissor[1] + groupScissor[3] );
			scissor[0] = min( scissor[0], groupScissor[0] );
			scissor[1] = min( scissor[1], groupScissor[1] );
		}

		shadowGroups[numShadows++] = group;
		if( numShadows >= GLSL_SHADOWMAP_LIMIT ) {
			RB_RenderMeshGLSL_ShadowmapArray( pass, programFeatures, numShadows, 
				(const shadowGroup_t **)shadowGroups, scissor );
			numShadows = 0;
		}
	}

	if( numShadows > 0 ) {
		RB_RenderMeshGLSL_ShadowmapArray( pass, programFeatures, numShadows, 
			(const shadowGroup_t **)shadowGroups, scissor );
	}

	RB_Scissor( old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3] );
}

/*
* RB_RenderMeshGLSL_Outline
*/
static void RB_RenderMeshGLSL_Outline( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int faceCull;
	int program;
	mat4_t texMatrix;
	const mfog_t *fog = rb.fog;

	if( fog ) {
		programFeatures |= GLSL_SHADER_COMMON_FOG;
	}
	if( rb.currentModelType == mod_brush ) {
		programFeatures |= GLSL_SHADER_OUTLINE_OUTLINES_CUTOFF;
	}
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_OUTLINE, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( !RB_BindProgram( program ) )
		return;

	Matrix4_Identity( texMatrix );

	faceCull = rb.gl.faceCull;
	RB_Cull( GL_BACK );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	RB_UpdateCommonUniforms( program, pass, texMatrix );

	RP_UpdateOutlineUniforms( program, rb.currentEntity->outlineHeight * r_outlines_scale->value );

	if( programFeatures & GLSL_SHADER_COMMON_FOG ) {
		RB_UpdateFogUniforms( program, fog );
	}

	// submit animation data
	if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
		RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
	}

	RB_DrawElementsReal();

	RB_Cull( faceCull );
}

/*
* RB_TcGenToProgramFeatures
*/
r_glslfeat_t RB_TcGenToProgramFeatures( int tcgen, vec_t *tcgenVec, mat4_t texMatrix, mat4_t genVectors )
{
	r_glslfeat_t programFeatures = 0;

	Matrix4_Identity( texMatrix );

	switch( tcgen )
	{
	case TC_GEN_ENVIRONMENT:
		programFeatures |= GLSL_SHADER_Q3_TC_GEN_ENV;
		break;
	case TC_GEN_VECTOR:
		Matrix4_Identity( genVectors );
		Vector4Copy( &tcgenVec[0], &genVectors[0] );
		Vector4Copy( &tcgenVec[4], &genVectors[4] );
		programFeatures |= GLSL_SHADER_Q3_TC_GEN_VECTOR;
		return qfalse;
	case TC_GEN_PROJECTION:
		programFeatures |= GLSL_SHADER_Q3_TC_GEN_PROJECTION;
		break;
	case TC_GEN_REFLECTION_CELSHADE:
		RB_VertexTCCelshadeMatrix( texMatrix );
	case TC_GEN_REFLECTION:
		programFeatures |= GLSL_SHADER_Q3_TC_GEN_REFLECTION;
		break;
	default:
		break;
	}

	return programFeatures;
}

/*
* RB_ShaderpassTex
*/
static inline const image_t *RB_ShaderpassTex( const shaderpass_t *pass )
{
	if( pass->anim_fps )
		return pass->anim_frames[(int)( pass->anim_fps * rb.currentShaderTime ) % pass->anim_numframes];
	if( pass->flags & SHADERPASS_PORTALMAP )
		return rb.currentPortalSurface && rb.currentPortalSurface->texures[0] ? 
			rb.currentPortalSurface->texures[0] : r_blacktexture;
	if( ( pass->flags & SHADERPASS_SKYBOXSIDE ) && rb.skyboxShader && rb.skyboxSide >= 0 )
		return rb.skyboxShader->skyboxImages[rb.skyboxSide];
	return ( pass->anim_frames[0] ? pass->anim_frames[0] : r_notexture );
}

/*
* RB_RenderMeshGLSL_Q3AShader
*/
static void RB_RenderMeshGLSL_Q3AShader( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int state;
	int program;
	const mfog_t *fog = rb.fog;
	qboolean isWorldSurface = rb.currentModelType == mod_brush ? qtrue : qfalse;
	const superLightStyle_t *lightStyle = NULL;
	const entity_t *e = rb.currentEntity;
	qboolean isLightmapped = qfalse, isWorldVertexLight = qfalse;
	vec3_t lightDir;
	vec4_t lightAmbient, lightDiffuse;
	mat4_t texMatrix, genVectors;

	// lightmapped surface pass
	if( isWorldSurface && 
		rb.superLightStyle && 
		rb.superLightStyle->lightmapNum[0] >= 0	&& 
		pass->rgbgen.type == RGB_GEN_IDENTITY && 
		(rb.currentShader->flags & SHADER_LIGHTMAP) && 
		(pass->flags & GLSTATE_BLEND_ADD) != GLSTATE_BLEND_ADD ) {
		lightStyle = rb.superLightStyle;
		isLightmapped = qtrue;
	}

	// vertex-lit world surface
	if( isWorldSurface
		&& ( pass->rgbgen.type == RGB_GEN_VERTEX || pass->rgbgen.type == RGB_GEN_EXACT_VERTEX )
		&& ( rb.superLightStyle != NULL ) ) {
		isWorldVertexLight = qtrue;
	}
	else {
		isWorldVertexLight = qfalse;
	}

	// possibly apply the fog inline
	if( fog == rb.texFog ) {
		if( rb.currentShadowBits ) {
			fog = NULL;
		}
		else if( rb.currentShader->numpasses == 1 || ( isLightmapped && rb.currentShader->numpasses == 2 ) ) {
			rb.texFog = NULL;
		}
		else {
			fog = NULL;
		}
	}
	programFeatures |= RB_FogProgramFeatures( pass, fog );

	// diffuse lighting for entities
	if( !isWorldSurface && pass->rgbgen.type == RGB_GEN_LIGHTING_DIFFUSE && !(e->flags & RF_FULLBRIGHT) ) {
		vec3_t temp = { 0.1f, 0.2f, 0.7f };
		float radius = 1;

		if( e != rsc.worldent && e->model != NULL ) {
			radius = e->model->radius;
		}

		// get weighted incoming direction of world and dynamic lights
		R_LightForOrigin( e->lightingOrigin, temp, lightAmbient, lightDiffuse, radius * e->scale );

		if( e->flags & RF_MINLIGHT ) 	{
			if( lightAmbient[0] <= 0.1f || lightAmbient[1] <= 0.1f || lightAmbient[2] <= 0.1f ) {
				VectorSet( lightAmbient, 0.1f, 0.1f, 0.1f );
			}
		}

		// rotate direction
		Matrix3_TransformVector( e->axis, temp, lightDir );
	}
	else {
		VectorSet( lightDir, 0, 0, 0 );
		Vector4Set( lightAmbient, 1, 1, 1, 1 );
		Vector4Set( lightDiffuse, 1, 1, 1, 1 );
	}

	if( isLightmapped || isWorldVertexLight ) {
		// add dynamic lights
		if( rb.currentDlightBits ) {
			programFeatures |= RB_DlightbitsToProgramFeatures( rb.currentDlightBits );
		}
		if( ( ri.params & RP_DRAWFLAT ) && !( rb.currentShader->flags & SHADER_NODRAWFLAT ) ) {
			programFeatures |= GLSL_SHADER_COMMON_DRAWFLAT;
		}
	}

	RB_BindTexture( 0, RB_ShaderpassTex( pass ) );

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	programFeatures |= RB_TcGenToProgramFeatures( pass->tcgen, pass->tcgenVec, texMatrix, genVectors );

	// set shaderpass state (blending, depthwrite, etc)
	state = pass->flags;

	// possibly force depthwrite and give up blending when doing a lightmapped pass
	if( isLightmapped && 
		!rb.doneDepthPass &&
		!(state & GLSTATE_DEPTHWRITE) &&
		(rb.currentShader->flags & SHADER_DEPTHWRITE) ) {
		if( !(pass->flags & GLSTATE_ALPHAFUNC) ) {
			state &= ~( GLSTATE_SRCBLEND_MASK|GLSTATE_DSTBLEND_MASK );
		}
		state |= GLSTATE_DEPTHWRITE;
	}

	RB_SetShaderpassState( state );

	if( programFeatures & GLSL_SHADER_COMMON_SOFT_PARTICLE ) {
		RB_BindTexture( 3, r_screendepthtexturecopy );
	}

	if( isLightmapped ) {
		int i;

		// bind lightmap textures and set program's features for lightstyles
		for( i = 0; i < MAX_LIGHTMAPS && lightStyle->lightmapStyles[i] != 255; i++ )
			RB_BindTexture( i+4, r_worldbrushmodel->lightmapImages[lightStyle->lightmapNum[i]] ); // lightmap
		programFeatures |= ( i * GLSL_SHADER_Q3_LIGHTSTYLE0 );
	}

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_Q3A_SHADER, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateTexGenUniforms( program, texMatrix, genVectors );

		RP_UpdateDiffuseLightUniforms( program, lightDir, lightAmbient, lightDiffuse );

		if( programFeatures & GLSL_SHADER_COMMON_FOG ) {
			RB_UpdateFogUniforms( program, fog );
		}

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		// dynamic lights
		if( isLightmapped || isWorldVertexLight ) {
			RP_UpdateDynamicLightsUniforms( program, lightStyle, e->origin, e->axis, rb.currentDlightBits );
		}

		// r_drawflat
		if( programFeatures & GLSL_SHADER_COMMON_DRAWFLAT ) {
			RP_UpdateDrawFlatUniforms( program, rf.wallColor, rf.floorColor );
		}

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_Celshade
*/
static void RB_RenderMeshGLSL_Celshade( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int program;
	image_t *base, *shade, *diffuse, *decal, *entdecal, *stripes, *light;
	const mfog_t *fog = rb.fog;
	mat4_t reflectionMatrix;
	mat4_t texMatrix;

	base = pass->anim_frames[0];
	shade = pass->anim_frames[1];
	diffuse = pass->anim_frames[2];
	decal = pass->anim_frames[3];
	entdecal = pass->anim_frames[4];
	stripes = pass->anim_frames[5];
	light = pass->anim_frames[6];

	Matrix4_Identity( texMatrix );

	RB_BindTexture( 0, base );

	RB_VertexTCCelshadeMatrix( reflectionMatrix );

	// possibly apply the "texture" fog inline
	if( fog == rb.texFog ) {
		if( ( rb.currentShader->numpasses == 1 ) && !rb.currentShadowBits ) {
			rb.texFog = NULL;
		}
		else {
			fog = NULL;
		}
	}
	programFeatures |= RB_FogProgramFeatures( pass, fog );

	// convert rgbgen and alphagen to GLSL feature defines
	programFeatures |= RB_RGBAlphaGenToProgramFeatures( &pass->rgbgen, &pass->alphagen );

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	// bind white texture for shadow map view
#define CELSHADE_BIND(tmu,tex,feature,canAdd) \
	if( tex ) { \
		if( ri.params & RP_SHADOWMAPVIEW ) { \
			tex = r_whitetexture; \
		} else {\
			programFeatures |= feature; \
			if( canAdd && tex->samples == 3 ) programFeatures |= ((feature) << 1); \
		} \
		RB_BindTexture(tmu, tex); \
	}

	CELSHADE_BIND( 1, shade, 0, qfalse );
	CELSHADE_BIND( 2, diffuse, GLSL_SHADER_CELSHADE_DIFFUSE, qfalse );
	CELSHADE_BIND( 3, decal, GLSL_SHADER_CELSHADE_DECAL, qtrue );
	CELSHADE_BIND( 4, entdecal, GLSL_SHADER_CELSHADE_ENTITY_DECAL, qtrue );
	CELSHADE_BIND( 5, stripes, GLSL_SHADER_CELSHADE_STRIPES, qtrue );
	CELSHADE_BIND( 6, light, GLSL_SHADER_CELSHADE_CEL_LIGHT, qtrue );

#undef CELSHADE_BIND

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_CELSHADE, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RP_UpdateTexGenUniforms( program, reflectionMatrix, texMatrix );

		if( programFeatures & GLSL_SHADER_COMMON_FOG ) {
			RB_UpdateFogUniforms( program, fog );
		}

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_Fog
*/
static void RB_RenderMeshGLSL_Fog( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int program;
	const mfog_t *fog = rb.fog;
	mat4_t texMatrix = { 0 };

	programFeatures |= GLSL_SHADER_COMMON_FOG;

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_FOG, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RB_UpdateFogUniforms( program, fog );

		// submit animation data
		if( programFeatures & GLSL_SHADER_COMMON_BONE_TRANSFORMS ) {
			RP_UpdateBonesUniforms( program, rb.bonesData.numBones, rb.bonesData.dualQuats );
		}

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSL_FXAA
*/
static void RB_RenderMeshGLSL_FXAA( const shaderpass_t *pass, r_glslfeat_t programFeatures )
{
	int program;
	mat4_t texMatrix = { 0 };

	// set shaderpass state (blending, depthwrite, etc)
	RB_SetShaderpassState( pass->flags );

	RB_BindTexture( 0, pass->anim_frames[0] );

	// update uniforms
	program = RP_RegisterProgram( GLSL_PROGRAM_TYPE_FXAA, NULL,
		rb.currentShader->deformsKey, rb.currentShader->deforms, rb.currentShader->numdeforms, programFeatures );
	if( RB_BindProgram( program ) )
	{
		RB_UpdateCommonUniforms( program, pass, texMatrix );

		RB_DrawElementsReal();
	}
}

/*
* RB_RenderMeshGLSLProgrammed
*/
void RB_RenderMeshGLSLProgrammed( const shaderpass_t *pass, int programType )
{
	r_glslfeat_t features = 0;

	if( rb.greyscale || pass->flags & SHADERPASS_GREYSCALE ) {
		features |= GLSL_SHADER_COMMON_GREYSCALE;
	}

	features |= RB_BonesTransformsToProgramFeatures();
	features |= RB_AutospriteProgramFeatures();
	features |= RB_InstancedArraysProgramFeatures();
	
	if( ( rb.currentShader->flags & SHADER_SOFT_PARTICLE ) && ri.fbDepthAttachment ) {
		features |= GLSL_SHADER_COMMON_SOFT_PARTICLE;
	}

	switch( programType )
	{
	case GLSL_PROGRAM_TYPE_MATERIAL:
		RB_RenderMeshGLSL_Material( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_DISTORTION:
		RB_RenderMeshGLSL_Distortion( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_RGB_SHADOW:
		RB_RenderMeshGLSL_RGBShadow( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_SHADOWMAP:
		RB_RenderMeshGLSL_Shadowmap( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_OUTLINE:
		RB_RenderMeshGLSL_Outline( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_Q3A_SHADER:
		RB_RenderMeshGLSL_Q3AShader( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_CELSHADE:
		RB_RenderMeshGLSL_Celshade( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_FOG:
		RB_RenderMeshGLSL_Fog( pass, features );
		break;
	case GLSL_PROGRAM_TYPE_FXAA:
		RB_RenderMeshGLSL_FXAA( pass, features );
		break;
	default:
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Unknown GLSL program type %i\n", programType );
		return;
	}
}

//==================================================================================

/*
* RB_UpdateVertexAttribs
*/
static void RB_UpdateVertexAttribs( void )
{
	vattribmask_t vattribs = rb.currentShader->vattribs;
	if( rb.superLightStyle ) {
		vattribs |= rb.superLightStyle->vattribs;
	}
	if( rb.bonesData.numBones ) {
		vattribs |= VATTRIB_BONES_BIT;
	}
	if( rb.currentEntity->outlineHeight ) {
		vattribs |= VATTRIB_NORMAL_BIT;
	}
	rb.currentVAttribs = vattribs;
}

/*
* RB_BindShader
*/
void RB_BindShader( const entity_t *e, const shader_t *shader, const mfog_t *fog )
{
	rb.currentShader = shader;
	rb.fog = fog;
	rb.texFog = rb.colorFog = NULL;

	if( fog && fog->shader ) {
		// should we fog the geometry with alpha texture or scale colors?
		if( Shader_UseTextureFog( shader ) ) {
			rb.texFog = fog;
		} else {
			// use scaling of colors
			rb.colorFog = fog;
		}
	}

	rb.doneDepthPass = qfalse;
	rb.dirtyUniformState = qtrue;

	rb.currentEntity = e ? e : &rb.nullEnt;
	rb.currentModelType = rb.currentEntity->model ? rb.currentEntity->model->type : mod_bad;
	rb.currentDlightBits = 0;
	rb.currentShadowBits = 0;
	rb.superLightStyle = NULL;

	rb.bonesData.numBones = 0;
	rb.bonesData.maxWeights = 0;

	rb.currentPortalSurface = NULL;

	rb.skyboxShader = NULL;
	rb.skyboxSide = -1;

	if( !e ) {
		rb.currentEntity = &rb.nullEnt;
		rb.currentShaderTime = rb.nullEnt.shaderTime * 0.001;
		rb.alphaHack = qfalse;
		rb.greyscale = qfalse;
	} else {
		Vector4Copy( rb.currentEntity->shaderRGBA, rb.entityColor );
		Vector4Copy( rb.currentEntity->outlineColor, rb.entityOutlineColor );
		if( rb.currentEntity->shaderTime > rb.time )
			rb.currentShaderTime = 0;
		else
			rb.currentShaderTime = (rb.time - rb.currentEntity->shaderTime) * 0.001;
		rb.alphaHack = e->renderfx & RF_ALPHAHACK ? qtrue : qfalse;
		rb.hackedAlpha = e->shaderRGBA[3] / 255.0;
		rb.greyscale = e->renderfx & RF_GREYSCALE ? qtrue : qfalse;
	}

	RB_UpdateVertexAttribs();
}

/*
* RB_SetLightstyle
*/
void RB_SetLightstyle( const superLightStyle_t *lightStyle )
{
	assert( rb.currentShader != NULL );
	rb.superLightStyle = lightStyle;
	rb.dirtyUniformState = qtrue;

	RB_UpdateVertexAttribs();
}

/*
* RB_SetDlightBits
*/
void RB_SetDlightBits( unsigned int dlightBits )
{
	assert( rb.currentShader != NULL );
	rb.currentDlightBits = dlightBits;
	rb.dirtyUniformState = qtrue;
}

/*
* RB_SetShadowBits
*/
void RB_SetShadowBits( unsigned int shadowBits )
{
	assert( rb.currentShader != NULL );
	rb.currentShadowBits = shadowBits;
	rb.dirtyUniformState = qtrue;
}

/*
* RB_SetAnimData
*/
void RB_SetBonesData( int numBones, const dualquat_t *dualQuats, int maxWeights )
{
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

	rb.dirtyUniformState = qtrue;

	RB_UpdateVertexAttribs();
}

/*
* RB_SetPortalSurface
*/
void RB_SetPortalSurface( const portalSurface_t *portalSurface )
{
	assert( rb.currentShader != NULL );
	rb.currentPortalSurface = portalSurface;
	rb.dirtyUniformState = qtrue;
}

/*
* RB_SetSkyboxShader
*/
void RB_SetSkyboxShader( const shader_t *shader )
{
	rb.skyboxShader = shader;
	rb.dirtyUniformState = qtrue;
}

/*
* RB_SetSkyboxSide
*/
void RB_SetSkyboxSide( int side )
{
	if( side < 0 || side >= 6 ) {
		rb.skyboxSide = -1;
	} else {
		rb.skyboxSide = side;
	}
	rb.dirtyUniformState = qtrue;
}

/*
* RB_SetInstanceData
*
* Internal backend function, only used by RB_DrawElementsReal to upload
* instance data
*/
void RB_SetInstanceData( int numInstances, instancePoint_t *instances )
{
	if( !rb.currentProgram ) {
		return;
	}
	RP_UpdateInstancesUniforms( rb.currentProgram, numInstances, instances );
}

/*
* RB_SetZClip
*/
void RB_SetZClip( float zNear, float zFar )
{
	rb.zNear = zNear;
	rb.zFar = zFar;
}

/*
* RB_BindProgram
*/
int RB_BindProgram( int program )
{
	int object;

	if( program == rb.currentProgram ) {
		return rb.currentProgramObject;
	}

	rb.currentProgram = program;
	if( !program ) {
		rb.currentProgramObject = 0;
		qglUseProgramObjectARB( 0 );
		return 0;
	}

	object = RP_GetProgramObject( program );
	if( object ) {
		qglUseProgramObjectARB( object );
	}
	rb.currentProgramObject = object;
	rb.dirtyUniformState = qtrue;
	return object;
}

/*
* RB_RenderPass
*/
static void RB_RenderPass( const shaderpass_t *pass )
{
	// for depth texture we render light's view to, ignore passes that do not write into depth buffer
	if( ( ri.params & RP_SHADOWMAPVIEW ) && !( pass->flags & GLSTATE_DEPTHWRITE ) )
		return;

	if( ( ri.params & RP_SHADOWMAPVIEW ) && !glConfig.ext.shadow ) {
		RB_RenderMeshGLSLProgrammed( pass, GLSL_PROGRAM_TYPE_RGB_SHADOW );
	} else if( pass->program_type ) {
		RB_RenderMeshGLSLProgrammed( pass, pass->program_type );
	} else {
		RB_RenderMeshGLSLProgrammed( pass, GLSL_PROGRAM_TYPE_Q3A_SHADER );
	}

	if( rb.dirtyUniformState ) {
		rb.donePassesTotal = 0;
		rb.dirtyUniformState = qfalse;
	}

	if( rb.gl.state & GLSTATE_DEPTHWRITE ) {
		rb.doneDepthPass = qtrue;
	}

	rb.donePassesTotal++;
}

/*
* RB_SetShaderStateMask
*/
void RB_SetShaderStateMask( int ANDmask, int ORmask )
{
	rb.shaderStateANDmask = ANDmask;
	rb.shaderStateORmask = ORmask;
}

/*
* RB_SetShaderState
*/
static void RB_SetShaderState( void )
{
	int state;
	int shaderFlags = rb.currentShader->flags;

	// Face culling
	if( !gl_cull->integer )
		RB_Cull( 0 );
	else if( shaderFlags & SHADER_CULL_FRONT )
		RB_Cull( GL_FRONT );
	else if( shaderFlags & SHADER_CULL_BACK )
		RB_Cull( GL_BACK );
	else
		RB_Cull( 0 );

	state = 0;

	if( shaderFlags & SHADER_POLYGONOFFSET )
	{
		state |= GLSTATE_OFFSET_FILL;
		RB_PolygonOffset( -1.0, -2.0f );
	}
	else if( ri.params & RP_SHADOWMAPVIEW )
	{
		state |= GLSTATE_OFFSET_FILL;
		RB_PolygonOffset( 4.0f, 1.0f );
	}

	if( shaderFlags & SHADER_NO_DEPTH_TEST )
		state |= GLSTATE_NO_DEPTH_TEST;

	rb.currentShaderState = (state & rb.shaderStateANDmask) | rb.shaderStateORmask;
}

/*
* RB_SetShaderpassState
*/
static void RB_SetShaderpassState( int state )
{
	state |= rb.currentShaderState;
	if( rb.alphaHack ) {
		if( !(state &GLSTATE_SRCBLEND_MASK) && !(state & GLSTATE_DSTBLEND_MASK) ) {
			// force alpha blending
			state = (state & ~ GLSTATE_DEPTHWRITE)|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		}
	}
	RB_SetState( state );
}

/*
* RB_CleanSinglePass
*
* Attempts to reuse current GLSL state: since the dirty flag
* is not set and there have been no uniform updates, we can simply
* call glDrawElements with fresh vertex data
*/
static qboolean RB_CleanSinglePass( void )
{
	// reuse current GLSL state (same program bound, same uniform values)
	if( !rb.dirtyUniformState && rb.donePassesTotal == 1 ) {
		RB_DrawElementsReal();
		return qtrue;
	}
	return qfalse;
}

/*
* RB_TriangleLinesColor
*/
static inline const vec_t *RB_TriangleLinesColor( void )
{
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
void RB_DrawOutlinedElements( void )
{
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
	}
	else {
		pass = &rb.currentShader->passes[0];
	}

	// set some flags
	rb.currentShadowBits = 0;
	rb.currentDlightBits = 0;
	rb.colorFog = rb.texFog = NULL;

	// copy and override
	r_triLinesPass = *pass;
	r_triLinesPass.rgbgen.type = RGB_GEN_CONST;
	r_triLinesPass.rgbgen.args = &r_triLinesColor[0];
	r_triLinesPass.alphagen.type = ALPHA_GEN_CONST;
	r_triLinesPass.alphagen.args = &r_triLinesColor[3];
	r_triLinesPass.flags = 0;
	r_triLinesPass.anim_frames[0] = r_whitetexture;
	r_triLinesPass.anim_fps = 0;
	r_triLinesPass.anim_numframes = 0;
	r_triLinesPass.program_type = GLSL_PROGRAM_TYPE_Q3A_SHADER;

	RB_SetShaderState();

	RB_RenderPass( &r_triLinesPass );
}

/*
* RB_DrawShadedElements
*/
void RB_DrawShadedElements( void )
{
	int i;
	qboolean addGLSLOutline = qfalse;
	shaderpass_t *pass;

	if( RB_CleanSinglePass() ) {
		return;
	}

	if( ENTITY_OUTLINE( rb.currentEntity ) && !(ri.params & RP_CLIPPLANE)
		&& ( rb.currentShader->sort == SHADER_SORT_OPAQUE ) && ( rb.currentShader->flags & SHADER_CULL_FRONT )
		&& !( ri.params & RP_SHADOWMAPVIEW ) )
	{
		addGLSLOutline = qtrue;
	}

	RB_SetShaderState();

	// accumulate passes for dynamic merging
	for( i = 0, pass = rb.currentShader->passes; i < rb.currentShader->numpasses; i++, pass++ )
	{
		if( ( pass->flags & SHADERPASS_DETAIL ) && !r_detailtextures->integer )
			continue;
		if( ( pass->flags & SHADERPASS_LIGHTMAP ) )
			continue;
		RB_RenderPass( pass );
	}

	// shadow map
	if( rb.currentShadowBits && ( rb.currentShader->sort >= SHADER_SORT_OPAQUE )
		&& ( rb.currentShader->sort <= SHADER_SORT_ALPHATEST ) )
		RB_RenderPass( &r_GLSLpasses[BUILTIN_GLSLPASS_SHADOWMAP] );

	// outlines
	if( addGLSLOutline )
		RB_RenderPass( &r_GLSLpasses[BUILTIN_GLSLPASS_OUTLINE] );

	// fog
	if( rb.texFog && rb.texFog->shader )
	{
		shaderpass_t *fogPass = &r_GLSLpasses[BUILTIN_GLSLPASS_FOG];

		fogPass->anim_frames[0] = r_whitetexture;
		if( !rb.currentShader->numpasses || rb.currentShader->fog_dist || ( rb.currentShader->flags & SHADER_SKY ) )
			fogPass->flags &= ~GLSTATE_DEPTHFUNC_EQ;
		else
			fogPass->flags |= GLSTATE_DEPTHFUNC_EQ;
		RB_RenderPass( fogPass );
	}
}
