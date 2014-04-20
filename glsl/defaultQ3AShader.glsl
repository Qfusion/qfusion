// Q3A GLSL shader

#include "include/common.glsl"
#include "include/uniforms.glsl"

#if defined(NUM_DLIGHTS)
#if defined(FRAGMENT_SHADER)
#include "include/dlights.glsl"
#endif
#endif

#ifdef APPLY_FOG
#include "include/fog.glsl"
#endif

#ifdef APPLY_GREYSCALE
#include "include/greyscale.glsl"
#endif

qf_varying vec3 v_Position;

#ifdef APPLY_DRAWFLAT
qf_varying myhalf v_NormalZ;
#endif

#ifdef APPLY_TC_GEN_REFLECTION
#define APPLY_CUBEMAP
#endif

#ifdef APPLY_CUBEMAP
qf_varying vec3 v_TexCoord;
#else
qf_varying vec2 v_TexCoord;
#endif

#ifdef NUM_LIGHTMAPS
qf_varying vec2 v_LightmapTexCoord[NUM_LIGHTMAPS];
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
qf_varying vec2 v_FogCoord;
#endif

#if defined(APPLY_SOFT_PARTICLE)
qf_varying float v_Depth;
#endif

#ifdef VERTEX_SHADER
// Vertex shader

#include "include/attributes.glsl"
#include "include/vtransform.glsl"
#include "include/rgbgen.glsl"

#if defined(APPLY_TC_GEN_REFLECTION)
uniform mat4 u_ReflectionTexMatrix;
#elif defined(APPLY_TC_GEN_VECTOR)
uniform mat4 u_VectorTexMatrix;
#endif

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	myhalf4 inColor = myhalf4(a_Color);

	TransformVerts(Position, Normal, TexCoord);

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

#ifdef APPLY_FOG
#if defined(APPLY_FOG_COLOR)
	FogGen(Position, outColor, u_BlendMix);
#else
	FogGen(Position, v_FogCoord);
#endif
#endif // APPLY_FOG

	qf_FrontColor = vec4(outColor);

#if defined(APPLY_TC_GEN_ENV)
	vec3 Projection;

	Projection = u_EntityDist - Position.xyz;
	Projection = normalize(Projection);

	float Depth = dot(Normal.xyz, Projection) * 2.0;
	v_TexCoord = vec2(0.5 + (Normal.y * Depth - Projection.y) * 0.5, 0.5 - (Normal.z * Depth - Projection.z) * 0.5);
#elif defined(APPLY_TC_GEN_VECTOR)
	v_TexCoord = vec2(u_VectorTexMatrix * Position);
#elif defined(APPLY_TC_GEN_REFLECTION)
	v_TexCoord = vec3(u_ReflectionTexMatrix * vec4(reflect(normalize(Position.xyz - u_EntityDist), Normal.xyz), 0.0));
#elif defined(APPLY_TC_GEN_PROJECTION)
	v_TexCoord = vec2(normalize(u_ModelViewProjectionMatrix * Position) * 0.5 + vec4(0.5));
#else
	v_TexCoord = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);
#endif // defined(APPLY_TC_GEN_ENV)

	v_Position = Position.xyz;

#ifdef APPLY_DRAWFLAT
	v_NormalZ = Normal.z;
#endif

#ifdef NUM_LIGHTMAPS
	v_LightmapTexCoord[0] = a_LightmapCoord0;
#if NUM_LIGHTMAPS >= 2
	v_LightmapTexCoord[1] = a_LightmapCoord1;
#if NUM_LIGHTMAPS >= 3
	v_LightmapTexCoord[2] = a_LightmapCoord2;
#if NUM_LIGHTMAPS >= 4
	v_LightmapTexCoord[3] = a_LightmapCoord3;
#endif // NUM_LIGHTMAPS >= 4
#endif // NUM_LIGHTMAPS >= 3
#endif // NUM_LIGHTMAPS >= 2
#endif // NUM_LIGHTMAPS

	gl_Position = u_ModelViewProjectionMatrix * Position;

#if defined(APPLY_SOFT_PARTICLE)
	vec4 modelPos = u_ModelViewMatrix * Position;
	v_Depth = -modelPos.z;
#endif	
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER
// Fragment shader

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
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[0], v_LightmapTexCoord[0])) * u_LightstyleColor[0];
#if NUM_LIGHTMAPS >= 2
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[1], v_LightmapTexCoord[1])) * u_LightstyleColor[1];
#if NUM_LIGHTMAPS >= 3
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[2], v_LightmapTexCoord[2])) * u_LightstyleColor[2];
#if NUM_LIGHTMAPS >= 4
	color.rgb += myhalf3(qf_texture(u_LightmapTexture[3], v_LightmapTexCoord[3])) * u_LightstyleColor[3];
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
	color.rgb = mix(color.rgb, u_Fog.Color, fogDensity);
#endif

#if defined(APPLY_SOFT_PARTICLE)
	myhalf softness = FragmentSoftness(v_Depth, u_DepthTexture, gl_FragCoord.xy, u_Viewport, u_ZNear, u_ZFar, u_SoftParticlesScale);
	color *= mix(myhalf4(1.0), myhalf4(softness), u_BlendMix.xxxy);
#endif

	qf_FragColor = vec4(color);
}

#endif // FRAGMENT_SHADER
