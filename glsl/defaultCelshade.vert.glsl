#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/vtransform.glsl"
#include "include/rgbgen.glsl"

#ifdef APPLY_FOG
#include "include/fog.glsl"
#endif

qf_varying vec2 v_TexCoord;
qf_varying vec3 v_TexCoordCube;

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
qf_varying vec2 v_FogCoord;
#endif

uniform mat3 u_ReflectionTexMatrix;

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
	v_TexCoordCube = u_ReflectionTexMatrix * reflect(normalize(Position.xyz - u_EntityDist), Normal.xyz);

	gl_Position = u_ModelViewProjectionMatrix * Position;
}
