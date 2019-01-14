#include "include/common.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"
#include "include/uniforms.glsl"
#include_if(APPLY_GREYSCALE) "include/greyscale.glsl"

#include "include/varying_q3a.glsl"

#if defined(APPLY_CUBEMAP) || defined(APPLY_CUBEMAP_VERTEX) || defined(APPLY_SURROUNDMAP)
uniform samplerCube u_BaseTexture;
#else
uniform sampler2D u_BaseTexture;
#endif

#ifdef APPLY_DRAWFLAT
uniform vec3 u_WallColor;
uniform vec3 u_FloorColor;
#endif

#if defined(APPLY_SOFT_PARTICLE)
#include "include/softparticle.glsl"
uniform sampler2D u_DepthTexture;
#endif

void main(void)
{
#ifndef APPLY_DRAWFLAT
	vec4 color = vec4(qf_FrontColor);
#else
	vec4 color = vec4(1.0);
#endif
	vec4 diffuse;

#if defined(APPLY_CUBEMAP)
	diffuse = vec4(qf_textureCube(u_BaseTexture, reflect(v_Position - u_EntityDist, normalize(v_Normal))));
#elif defined(APPLY_CUBEMAP_VERTEX)
	diffuse = vec4(qf_textureCube(u_BaseTexture, v_TexCoord));
#elif defined(APPLY_SURROUNDMAP)
	diffuse = vec4(qf_textureCube(u_BaseTexture, v_Position - u_EntityDist));
#else
	diffuse = vec4(qf_texture(u_BaseTexture, v_TexCoord));
#endif

#ifdef APPLY_DRAWFLAT
	float n = float(step(DRAWFLAT_NORMAL_STEP, abs(v_Normal.z)));
	diffuse.rgb = vec3(mix(LinearColor(u_WallColor), LinearColor(u_FloorColor), n));
#endif

#ifdef APPLY_ALPHA_MASK
	color.a *= diffuse.a;
#else
	color *= diffuse;
#endif

#ifdef APPLY_GREYSCALE
	color.rgb = Greyscale(color.rgb);
#endif

#if defined(APPLY_SOFT_PARTICLE)
	float softness = FragmentSoftness(v_Depth, u_DepthTexture, gl_FragCoord.xy, u_ZRange);
	color *= mix(vec4(1.0), vec4(softness), u_BlendMix.xxxy);
#endif

#ifdef QF_ALPHATEST
	QF_ALPHATEST(color.a);
#endif

#ifdef APPLY_DRAWFLAT
	color.rgb = apply_fog( color.rgb, length( v_Position - u_ViewOrigin ) );
	color.rgb += dither();
#endif

	qf_FragColor = vec4(sRGBColor(color));
}
