#include "include/common.glsl"
#include "include/uniforms.glsl"
#if defined(NUM_DLIGHTS)
#include "include/dlights.glsl"
#endif
#ifdef APPLY_FOG
#include "include/fog.glsl"
#endif
#ifdef APPLY_GREYSCALE
#include "include/greyscale.glsl"
#endif

#include "include/varying_q3a.glsl"

#ifdef APPLY_CUBEMAP
uniform samplerCube u_BaseTexture;
#else
uniform sampler2D u_BaseTexture;
#endif

#ifdef APPLY_DRAWFLAT
uniform myhalf3 u_WallColor;
uniform myhalf3 u_FloorColor;
#endif

#ifdef NUM_LIGHTMAPS
uniform sampler2D u_LightmapTexture[NUM_LIGHTMAPS];
#endif

#if defined(APPLY_SOFT_PARTICLE)
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

void main(void)
{
	myhalf4 color;

#ifdef NUM_LIGHTMAPS
	color = myhalf4(0.0, 0.0, 0.0, qf_FrontColor.a);
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[0], v_LightmapTexCoord01.st)) * u_LightstyleColor[0];
#if NUM_LIGHTMAPS >= 2
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[1], v_LightmapTexCoord01.pq)) * u_LightstyleColor[1];
#if NUM_LIGHTMAPS >= 3
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[2], v_LightmapTexCoord23.st)) * u_LightstyleColor[2];
#if NUM_LIGHTMAPS >= 4
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[3], v_LightmapTexCoord23.pq)) * u_LightstyleColor[3];
#endif // NUM_LIGHTMAPS >= 4
#endif // NUM_LIGHTMAPS >= 3
#endif // NUM_LIGHTMAPS >= 2
#else
	color = myhalf4(qf_FrontColor);
#endif // NUM_LIGHTMAPS

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
	myhalf fogDensity = FogDensity(v_FogCoord);
#endif

#if defined(NUM_DLIGHTS)
	color.rgb += DynamicLightsSummaryColor(v_Position);
#endif

	myhalf4 diffuse;

#ifdef APPLY_CUBEMAP
	diffuse = myhalf4(qf_textureCube(u_BaseTexture, v_TexCoord));
#else
	diffuse = myhalf4(qf_texture(u_BaseTexture, v_TexCoord));
#endif

#ifdef APPLY_DRAWFLAT
	myhalf n = myhalf(step(DRAWFLAT_NORMAL_STEP, abs(v_NormalZ)));
	diffuse.rgb = myhalf3(mix(u_WallColor, u_FloorColor, n));
#endif

	color *= diffuse;

#ifdef NUM_LIGHTMAPS
	// so that team-colored shaders work
	color *= myhalf4(qf_FrontColor);
#endif

#ifdef APPLY_GREYSCALE
	color.rgb = Greyscale(color.rgb);
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
	color.rgb = mix(color.rgb, u_FogColor, fogDensity);
#endif

#if defined(APPLY_SOFT_PARTICLE)
	myhalf softness = FragmentSoftness(v_Depth, u_DepthTexture, gl_FragCoord.xy, u_ZRange);
	color *= mix(myhalf4(1.0), myhalf4(softness), u_BlendMix.xxxy);
#endif

#ifdef QF_ALPHATEST
	QF_ALPHATEST(color.a);
#endif

#ifdef APPLY_BLEND
	qf_FragColor = vec4(color);
#else
	qf_FragColor = vec4(vec3(color), 1.0);
#endif
}
