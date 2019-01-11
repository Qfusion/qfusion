#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/rgbgen.glsl"

#include "include/varying_material.glsl"

void main()
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	myhalf4 inColor = myhalf4(a_Color);
	vec2 TexCoord = a_TexCoord;
	vec3 Tangent = a_SVector.xyz;
	float TangentDir = a_SVector.w;

	QF_TransformVerts_Tangent(Position, Normal, Tangent, TexCoord);

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

	qf_FrontColor = vec4(outColor);

	v_TexCoord = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);

	v_StrMatrix[0] = Tangent;
	v_StrMatrix[2] = Normal;
	v_StrMatrix[1] = TangentDir * cross(Normal, Tangent);

#if defined(APPLY_SPECULAR)
	vec3 EyeVectorWorld = u_ViewOrigin - Position.xyz;
	v_EyeVector = EyeVectorWorld * v_StrMatrix;
#endif

	v_Position = Position.xyz;

	gl_Position = u_ModelViewProjectionMatrix * Position;
}
