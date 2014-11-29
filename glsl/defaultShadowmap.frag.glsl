#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

#ifndef NUM_SHADOWS
#define NUM_SHADOWS 1
#endif

qf_varying vec4 v_ShadowProjVector[NUM_SHADOWS];
qf_varying vec4 v_Position[NUM_SHADOWS];

#ifdef APPLY_SHADOW_SAMPLERS
# define SHADOW_SAMPLER sampler2DShadow
# define dshadow2D(t,v) float(qf_shadow(t,v))
#else
# define SHADOW_SAMPLER sampler2D
# ifdef APPLY_RGB_SHADOW_24BIT
#  define dshadow2D(t,v) step(v.z, decodedepthmacro24(qf_texture(t, v.xy)))
# else
#  define dshadow2D(t,v) step(v.z, decodedepthmacro16(qf_texture(t, v.xy)))
# endif
#endif

uniform SHADOW_SAMPLER u_ShadowmapTexture0;
#if NUM_SHADOWS >= 2
uniform SHADOW_SAMPLER u_ShadowmapTexture1;
#if NUM_SHADOWS >= 3
uniform SHADOW_SAMPLER u_ShadowmapTexture2;
#if NUM_SHADOWS >= 4
uniform SHADOW_SAMPLER u_ShadowmapTexture3;
#endif // NUM_SHADOWS >= 4
#endif // NUM_SHADOWS >= 3
#endif // NUM_SHADOWS >= 2

uniform vec4 u_ShadowDir[NUM_SHADOWS];

#ifdef APPLY_SHADOW_NORMAL_CHECK
qf_varying vec3 v_Normal;
#endif

uniform vec4 u_ShadowAlpha[(NUM_SHADOWS + 3) / 4];
uniform vec4 u_ShadowmapTextureParams[NUM_SHADOWS];

uniform vec3 u_ShadowEntityDist[NUM_SHADOWS];

void main(void)
{
	float finalcolor = 1.0;

#if NUM_SHADOWS >= 1
#define SHADOW_INDEX 0
#define SHADOW_INDEX_COMPONENT x
#define SHADOW_TEXTURE u_ShadowmapTexture0
#include "include/shadowmap_inc.glsl"
#undef SHADOW_TEXTURE
#undef SHADOW_INDEX_COMPONENT
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 2
#define SHADOW_INDEX 1
#define SHADOW_INDEX_COMPONENT y
#define SHADOW_TEXTURE u_ShadowmapTexture1
#include "include/shadowmap_inc.glsl"
#undef SHADOW_TEXTURE
#undef SHADOW_INDEX_COMPONENT
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 3
#define SHADOW_INDEX 2
#define SHADOW_INDEX_COMPONENT z
#define SHADOW_TEXTURE u_ShadowmapTexture2
#include "include/shadowmap_inc.glsl"
#undef SHADOW_TEXTURE
#undef SHADOW_INDEX_COMPONENT
#undef SHADOW_INDEX
#endif

#if NUM_SHADOWS >= 4
#define SHADOW_INDEX 3
#define SHADOW_INDEX_COMPONENT w
#define SHADOW_TEXTURE u_ShadowmapTexture3
#include "include/shadowmap_inc.glsl"
#undef SHADOW_TEXTURE
#undef SHADOW_INDEX_COMPONENT
#undef SHADOW_INDEX
#endif

	qf_FragColor = vec4(vec3(finalcolor),1.0);
}
