#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

#ifndef NUM_SHADOWS
#define NUM_SHADOWS 1
#endif

qf_varying vec4 v_ShadowProjVector[NUM_SHADOWS];

#ifdef APPLY_SHADOW_SAMPLERS
uniform sampler2DShadow u_ShadowmapTexture[NUM_SHADOWS];
# define dshadow2D(t,v) float(qf_shadow(t,v))
#else
uniform sampler2D u_ShadowmapTexture[NUM_SHADOWS];
# ifdef APPLY_SHADOW_16BIT
#  define dshadow2D(t,v) step(v.z, decodedepthmacro16(qf_texture(t, v.xy)))
# else
#  define dshadow2D(t,v) step(v.z, decodedepthmacro(qf_texture(t, v.xy)))
# endif
#endif

uniform float u_ShadowAlpha;
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
