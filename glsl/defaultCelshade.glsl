// Qfusion cel-shading shader

#include "include/common.glsl"
#include "include/uniforms.glsl"

#ifdef APPLY_FOG
#include "include/fog.glsl"
#endif
#ifdef APPLY_GREYSCALE
#include "include/greyscale.glsl"
#endif

qf_varying vec2 v_TexCoord;
qf_varying vec3 v_TexCoordCube;

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
qf_varying vec2 v_FogCoord;
#endif

#ifdef VERTEX_SHADER
// Vertex shader
#include "include/attributes.glsl"
#include "include/vtransform.glsl"
#include "include/rgbgen.glsl"

uniform mat4 u_ReflectionTexMatrix;

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	myhalf4 inColor = myhalf4(a_Color);

	TransformVerts(Position, Normal, TexCoord);

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

#ifdef APPLY_FOG
#ifdef APPLY_FOG_COLOR
	FogGen(Position, outColor, u_BlendMix);
#else
	FogGen(Position, v_FogCoord);
#endif
#endif

	qf_FrontColor = vec4(outColor);

	v_TexCoord = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);
	v_TexCoordCube = vec3(u_ReflectionTexMatrix * vec4(reflect(normalize(Position.xyz - u_EntityDist), Normal.xyz), 0.0));

	gl_Position = u_ModelViewProjectionMatrix * Position;
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER
// Fragment shader

uniform sampler2D u_BaseTexture;
uniform samplerCube u_CelShadeTexture;

#ifdef APPLY_DIFFUSE
uniform sampler2D u_DiffuseTexture;
#endif
#ifdef APPLY_DECAL
uniform sampler2D u_DecalTexture;
#endif
#ifdef APPLY_ENTITY_DECAL
uniform sampler2D u_EntityDecalTexture;
#endif
#ifdef APPLY_STRIPES
uniform sampler2D u_StripesTexture;
#endif
#ifdef APPLY_CEL_LIGHT
uniform samplerCube u_CelLightTexture;
#endif

void main(void)
{
	myhalf4 inColor = myhalf4(qf_FrontColor);

	myhalf4 tempColor;

	myhalf4 outColor;
	outColor = myhalf4(qf_texture(u_BaseTexture, v_TexCoord));

#ifdef APPLY_ENTITY_DECAL
#ifdef APPLY_ENTITY_DECAL_ADD
	outColor.rgb += myhalf3(u_EntityColor.rgb) * myhalf3(qf_texture(u_EntityDecalTexture, v_TexCoord));
#else
	tempColor = myhalf4(u_EntityColor.rgb, 1.0) * myhalf4(qf_texture(u_EntityDecalTexture, v_TexCoord));
	outColor.rgb = mix(outColor.rgb, tempColor.rgb, tempColor.a);
#endif
#endif // APPLY_ENTITY_DECAL

#ifdef APPLY_DIFFUSE
	outColor.rgb *= myhalf3(qf_texture(u_DiffuseTexture, v_TexCoord));
#endif

	outColor.rgb *= myhalf3(qf_textureCube(u_CelShadeTexture, v_TexCoordCube));

#ifdef APPLY_STRIPES
#ifdef APPLY_STRIPES_ADD
	outColor.rgb += myhalf3(u_EntityColor.rgb) * myhalf3(qf_texture(u_StripesTexture, v_TexCoord));
#else
	tempColor = myhalf4(u_EntityColor.rgb, 1.0) * myhalf4(qf_texture(u_StripesTexture, v_TexCoord));
	outColor.rgb = mix(outColor.rgb, tempColor.rgb, tempColor.a);
#endif
#endif // APPLY_STRIPES_ADD

#ifdef APPLY_CEL_LIGHT
#ifdef APPLY_CEL_LIGHT_ADD
	outColor.rgb += myhalf3(qf_textureCube(u_CelLightTexture, v_TexCoordCube));
#else
	tempColor = myhalf4(qf_textureCube(u_CelLightTexture, v_TexCoordCube));
	outColor.rgb = mix(outColor.rgb, tempColor.rgb, tempColor.a);
#endif
#endif // APPLY_CEL_LIGHT

#ifdef APPLY_DECAL
#ifdef APPLY_DECAL_ADD
	outColor.rgb += myhalf3(qf_texture(u_DecalTexture, v_TexCoord));
#else
	tempColor = myhalf4(qf_texture(u_DecalTexture, v_TexCoord));
	outColor.rgb = mix(outColor.rgb, tempColor.rgb, tempColor.a);
#endif
#endif // APPLY_DECAL

	outColor = myhalf4(inColor * outColor);

#ifdef APPLY_GREYSCALE
	outColor.rgb = Greyscale(outColor.rgb);
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
	myhalf fogDensity = FogDensity(v_FogCoord);
	outColor.rgb = mix(outColor.rgb, u_Fog.Color, fogDensity);
#endif

	qf_FragColor = vec4(outColor);
}

#endif // FRAGMENT_SHADER
