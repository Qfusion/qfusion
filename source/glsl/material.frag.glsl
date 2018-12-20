#include "include/common.glsl"
#include "include/lightmap.glsl"
#include "include/uniforms.glsl"
#include "include/varying_material.glsl"

#include_if(APPLY_GREYSCALE) "include/greyscale.glsl"
#include_if(APPLY_OFFSETMAPPING) "include/material_offsetmapping.frag.glsl"
#include_if(NUM_LIGHTMAPS) "include/material_lightmaps.frag.glsl"
#include_if(APPLY_DIRECTIONAL_LIGHT) "include/material_dirlight.frag.glsl"

uniform sampler2D u_BaseTexture;
uniform sampler2D u_NormalmapTexture;
uniform sampler2D u_GlossTexture;
#ifdef APPLY_DECAL
uniform sampler2D u_DecalTexture;
#endif

#ifdef APPLY_ENTITY_DECAL
uniform sampler2D u_EntityDecalTexture;
#endif

#if defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)
uniform float u_OffsetMappingScale;
#endif

#ifdef APPLY_DRAWFLAT
uniform myhalf3 u_WallColor;
uniform myhalf3 u_FloorColor;
#endif

uniform myhalf2 u_GlossFactors; // gloss scaling and exponent factors

void main()
{
	myhalf3 surfaceNormal;
	myhalf3 surfaceNormalModelspace;
	myhalf3 weightedDiffuseNormalModelspace;

#if !defined(APPLY_DIRECTIONAL_LIGHT) && !defined(NUM_LIGHTMAPS)
	myhalf4 color = myhalf4 (1.0, 1.0, 1.0, 1.0);
#else
	myhalf4 color = myhalf4 (0.0, 0.0, 0.0, 1.0);
#endif

	myhalf4 decal = myhalf4 (0.0, 0.0, 0.0, 1.0);
	myhalf3 lightColor = myhalf3 (0.0);
	myhalf3 specular = myhalf3 (0.0);

	// get the surface normal
	surfaceNormal = normalize(myhalf3(qf_texture (u_NormalmapTexture, v_TexCoord)) - myhalf3 (0.5));
	surfaceNormalModelspace = normalize(v_StrMatrix * surfaceNormal);

#ifdef APPLY_DIRECTIONAL_LIGHT
	lightColor += DirectionalLightColor(surfaceNormalModelspace, weightedDiffuseNormalModelspace);
#endif

#ifdef NUM_LIGHTMAPS
	lightColor += LightmapColor(surfaceNormalModelspace, weightedDiffuseNormalModelspace);
#endif

#ifdef APPLY_SPECULAR

#ifdef NORMALIZE_DIFFUSE_NORMAL
	myhalf3 specularNormal = normalize (myhalf3 (normalize (weightedDiffuseNormalModelspace)) + myhalf3 (normalize (u_EntityDist - v_Position)));
#else
	myhalf3 specularNormal = normalize (weightedDiffuseNormalModelspace + myhalf3 (normalize (u_EntityDist - v_Position)));
#endif

	myhalf specularProduct = myhalf(dot (surfaceNormalModelspace, specularNormal));
	specular = myhalf3(qf_texture(u_GlossTexture, v_TexCoord).r * u_GlossFactors.x * pow(myhalf(max(specularProduct, 0.0)), u_GlossFactors.y));
#endif // APPLY_SPECULAR

#if defined(APPLY_BASETEX_ALPHA_ONLY) && !defined(APPLY_DRAWFLAT)
	color.rgb = lightColor.rgb + specular.rgb;
	color = min(color, myhalf4(qf_texture(u_BaseTexture, v_TexCoord).a));
#else
	myhalf4 diffuse;

#ifdef APPLY_DRAWFLAT
	myhalf n = myhalf(step(DRAWFLAT_NORMAL_STEP, abs(v_StrMatrix[2].z)));
	diffuse = myhalf4(mix(u_WallColor, u_FloorColor, n), myhalf(qf_texture(u_BaseTexture, v_TexCoord).a));
#else
	diffuse = myhalf4(qf_texture(u_BaseTexture, v_TexCoord));
#endif

#ifdef APPLY_ENTITY_DECAL
	myhalf3 entColor = LinearColor(u_EntityColor.rgb);

#ifdef APPLY_ENTITY_DECAL_ADD
	decal.rgb = myhalf3(qf_texture(u_EntityDecalTexture, v_TexCoord));
	diffuse.rgb += entColor * decal.rgb;
#else
	decal = myhalf4(entColor, 1.0) * myhalf4(qf_texture(u_EntityDecalTexture, v_TexCoord));
	diffuse.rgb = mix(diffuse.rgb, decal.rgb, decal.a);
#endif // APPLY_ENTITY_DECAL_ADD

#endif // APPLY_ENTITY_DECAL

	color = myhalf4(lightColor.rgb * (diffuse.rgb + specular.rgb), diffuse.a);
#endif // defined(APPLY_BASETEX_ALPHA_ONLY) && !defined(APPLY_DRAWFLAT)

#ifdef APPLY_DECAL

#ifdef APPLY_DECAL_ADD
	decal.rgb = myhalf3(qf_FrontColor.rgb) * myhalf3(qf_texture(u_DecalTexture, v_TexCoord));
	color.rgb += decal.rgb;
#else
	decal = myhalf4(qf_FrontColor.rgb, 1.0) * myhalf4(qf_texture(u_DecalTexture, v_TexCoord));
	color.rgb = mix(color.rgb, decal.rgb, decal.a);
#endif // APPLY_DECAL_ADD
	color.a *= myhalf(qf_FrontColor.a);

#else

#if !defined (APPLY_DIRECTIONAL_LIGHT) || !defined(APPLY_DIRECTIONAL_LIGHT_MIX)
# if defined(APPLY_ENV_MODULATE_COLOR)
	color *= myhalf4(qf_FrontColor);
# endif
#else
	color.a *= myhalf(qf_FrontColor.a);
#endif

#endif // APPLY_DECAL

#ifdef QF_ALPHATEST
	QF_ALPHATEST(color.a);
#endif

#ifdef APPLY_GREYSCALE
	color.rgb = Greyscale(color.rgb);
#endif

	qf_FragColor = vec4(sRGBColor(color));
}
