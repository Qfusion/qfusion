// shadowmapping GLSL shader

#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

#ifndef NUM_SHADOWS
#define NUM_SHADOWS 1
#endif

qf_varying vec4 v_ShadowProjVector[NUM_SHADOWS];

#ifdef VERTEX_SHADER
// Vertex shader

#include "include/attributes.glsl"
#include "include/vtransform.glsl"

uniform mat4 u_ShadowmapMatrix[NUM_SHADOWS];

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;

	TransformVerts(Position, Normal, TexCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;

	for (int i = 0; i < NUM_SHADOWS; i++)
	{
		v_ShadowProjVector[i] = u_ShadowmapMatrix[i] * Position;
		// a trick whish allows us not to perform the
		// 'shadowmaptc = (shadowmaptc + vec3 (1.0)) * vec3 (0.5)'
		// computation in the fragment shader
		v_ShadowProjVector[i].xyz = (v_ShadowProjVector[i].xyz + vec3(v_ShadowProjVector[i].w)) * 0.5;
	}
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER
// Fragment shader

#ifdef APPLY_RGB_SHADOW
uniform sampler2D u_ShadowmapTexture[NUM_SHADOWS];
# define dshadow2D(t,v) step(v.z, decodedepthmacro(qf_texture(t, v.xy)))
#else
uniform sampler2DShadow u_ShadowmapTexture[NUM_SHADOWS];
# define dshadow2D(t,v) float(qf_shadow(t,v))
#endif

uniform float u_ShadowAlpha;
uniform float u_ShadowProjDistance[NUM_SHADOWS];
uniform vec4 u_ShadowmapTextureParams[NUM_SHADOWS];

void main(void)
{
	float finalcolor = 1.0;

#if NUM_SHADOWS >= 1
#define SHADOW_INDEX 0
#include "include/shadowmap_inc.glsl"
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 2
#define SHADOW_INDEX 1
#include "include/shadowmap_inc.glsl"
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 3
#define SHADOW_INDEX 2
#include "include/shadowmap_inc.glsl"
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 4
#define SHADOW_INDEX 3
#include "include/shadowmap_inc.glsl"
#undef SHADOW_INDEX
#endif

	qf_FragColor = vec4(vec3(finalcolor),1.0);
}

#endif // FRAGMENT_SHADER

