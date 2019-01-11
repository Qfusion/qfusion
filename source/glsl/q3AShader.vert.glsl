#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/rgbgen.glsl"

#include "include/varying_q3a.glsl"

#if defined(APPLY_TC_GEN_VECTOR)
uniform mat4 u_VectorTexMatrix;
#endif

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	myhalf4 inColor = myhalf4(a_Color);

	QF_TransformVerts(Position, Normal, TexCoord);

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

	qf_FrontColor = vec4(outColor);

#if defined(APPLY_CUBEMAP_VERTEX)

#elif !defined(APPLY_CUBEMAP) && !defined(APPLY_SURROUNDMAP)

#if defined(APPLY_TC_GEN_ENV)
		vec3 Projection;

		Projection = u_EntityDist - Position.xyz;
		Projection = normalize(Projection);

		float Depth = dot(Normal.xyz, Projection) * 2.0;
		v_TexCoord = 0.5 + (Normal.yz * Depth - Projection.yz) * vec2(0.5, -0.5);
#elif defined(APPLY_TC_GEN_VECTOR)
		v_TexCoord = vec2(Position * u_VectorTexMatrix); // account for u_VectorTexMatrix being transposed
#elif defined(APPLY_TC_GEN_PROJECTION)
		v_TexCoord = vec2(normalize(u_ModelViewProjectionMatrix * Position) * 0.5 + vec4(0.5));
#else
		v_TexCoord = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);
#endif // defined(APPLY_TC_GEN)

#endif // !defined(APPLY_CUBEMAP) && !defined(APPLY_SURROUNDMAP)

	v_Position = Position.xyz;

#if defined(APPLY_CUBEMAP) || defined(APPLY_DRAWFLAT)
	v_Normal = Normal;
#endif

	gl_Position = u_ModelViewProjectionMatrix * Position;

#if defined(APPLY_SOFT_PARTICLE)
	vec4 modelPos = u_ModelViewMatrix * Position;
	v_Depth = -modelPos.z;
#endif	
}
