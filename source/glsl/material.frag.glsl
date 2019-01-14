#include "include/common.glsl"
#include "include/dither.glsl"
#include "include/fog.glsl"
#include "include/uniforms.glsl"
#include "include/varying_material.glsl"

#include_if(APPLY_GREYSCALE) "include/greyscale.glsl"
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

#ifdef APPLY_DRAWFLAT
uniform vec3 u_WallColor;
uniform vec3 u_FloorColor;
#endif

uniform vec2 u_GlossFactors; // gloss scaling and exponent factors

void main()
{
	vec3 surfaceNormal;
	vec3 surfaceNormalModelspace;
	vec3 weightedDiffuseNormalModelspace;

	vec4 color = vec4 (0.0, 0.0, 0.0, 1.0);

	vec4 decal = vec4 (0.0, 0.0, 0.0, 1.0);
	vec3 lightColor = vec3 (0.0);
	vec3 specular = vec3 (0.0);

	// get the surface normal
	surfaceNormal = normalize(vec3(qf_texture (u_NormalmapTexture, v_TexCoord)) - vec3 (0.5));
	surfaceNormalModelspace = normalize(v_StrMatrix * surfaceNormal);

#ifdef APPLY_DIRECTIONAL_LIGHT
	lightColor += DirectionalLightColor(surfaceNormalModelspace, weightedDiffuseNormalModelspace);
#endif

#ifdef APPLY_SPECULAR

#ifdef NORMALIZE_DIFFUSE_NORMAL
	vec3 specularNormal = normalize (vec3 (normalize (weightedDiffuseNormalModelspace)) + vec3 (normalize (u_EntityDist - v_Position)));
#else
	vec3 specularNormal = normalize (weightedDiffuseNormalModelspace + vec3 (normalize (u_EntityDist - v_Position)));
#endif

	float specularProduct = float(dot (surfaceNormalModelspace, specularNormal));
	specular = vec3(qf_texture(u_GlossTexture, v_TexCoord).r * u_GlossFactors.x * pow(float(max(specularProduct, 0.0)), u_GlossFactors.y));
#endif // APPLY_SPECULAR

#if defined(APPLY_BASETEX_ALPHA_ONLY) && !defined(APPLY_DRAWFLAT)
	color.rgb = lightColor.rgb + specular.rgb;
	color = min(color, vec4(qf_texture(u_BaseTexture, v_TexCoord).a));
#else
	vec4 diffuse;

#ifdef APPLY_DRAWFLAT
	float n = float(step(DRAWFLAT_NORMAL_STEP, abs(v_StrMatrix[2].z)));
	diffuse = vec4(mix(u_WallColor, u_FloorColor, n), float(qf_texture(u_BaseTexture, v_TexCoord).a));
#else
	diffuse = vec4(qf_texture(u_BaseTexture, v_TexCoord));
#endif

#ifdef APPLY_ENTITY_DECAL
	vec3 entColor = LinearColor(u_EntityColor.rgb);

#ifdef APPLY_ENTITY_DECAL_ADD
	decal.rgb = vec3(qf_texture(u_EntityDecalTexture, v_TexCoord));
	diffuse.rgb += entColor * decal.rgb;
#else
	decal = vec4(entColor, 1.0) * vec4(qf_texture(u_EntityDecalTexture, v_TexCoord));
	diffuse.rgb = mix(diffuse.rgb, decal.rgb, decal.a);
#endif // APPLY_ENTITY_DECAL_ADD

#endif // APPLY_ENTITY_DECAL

	color = vec4(lightColor.rgb * (diffuse.rgb + specular.rgb), diffuse.a);
#endif // defined(APPLY_BASETEX_ALPHA_ONLY) && !defined(APPLY_DRAWFLAT)

#ifdef APPLY_DECAL

#ifdef APPLY_DECAL_ADD
	decal.rgb = vec3(qf_FrontColor.rgb) * vec3(qf_texture(u_DecalTexture, v_TexCoord));
	color.rgb += decal.rgb;
#else
	decal = vec4(qf_FrontColor.rgb, 1.0) * vec4(qf_texture(u_DecalTexture, v_TexCoord));
	color.rgb = mix(color.rgb, decal.rgb, decal.a);
#endif // APPLY_DECAL_ADD
	color.a *= float(qf_FrontColor.a);

#else

#if !defined (APPLY_DIRECTIONAL_LIGHT) || !defined(APPLY_DIRECTIONAL_LIGHT_MIX)
# if defined(APPLY_ENV_MODULATE_COLOR)
	color *= vec4(qf_FrontColor);
# endif
#else
	color.a *= float(qf_FrontColor.a);
#endif

#endif // APPLY_DECAL

#ifdef QF_ALPHATEST
	QF_ALPHATEST(color.a);
#endif

#ifdef APPLY_GREYSCALE
	color.rgb = Greyscale(color.rgb);
#endif

#ifdef APPLY_DRAWFLAT
	color.rgb = apply_fog( color.rgb, length( v_Position - u_ViewOrigin ) );
	color.rgb += dither();
#endif

	qf_FragColor = vec4(sRGBColor(color));
}
