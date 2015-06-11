#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/rgbgen.glsl"

qf_varying vec4 v_TexCoord;
qf_varying vec4 v_ProjVector;
#ifdef APPLY_EYEDOT
qf_varying vec3 v_EyeVector;
#endif

#ifdef APPLY_EYEDOT
uniform float u_FrontPlane;
#endif

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	vec3 Tangent = a_SVector.xyz;
	float TangentDir = a_SVector.w;
	myhalf4 inColor = myhalf4(a_Color);

	QF_TransformVerts(Position, Normal, TexCoord);

	qf_FrontColor = vec4(VertexRGBGen(Position, Normal, inColor));

	v_TexCoord.st = TextureMatrix2x3Mul(u_TextureMatrix, TexCoord);

	vec4 textureMatrix3_[2];
	textureMatrix3_[0] =  u_TextureMatrix[0];
	textureMatrix3_[1] = -u_TextureMatrix[1];
	v_TexCoord.pq = TextureMatrix2x3Mul(textureMatrix3_, TexCoord);

#ifdef APPLY_EYEDOT
	mat3 v_StrMatrix;
	v_StrMatrix[0] = Tangent;
	v_StrMatrix[2] = Normal;
	v_StrMatrix[1] = TangentDir * cross(Normal, Tangent);

	vec3 EyeVectorWorld = (u_ViewOrigin - Position.xyz) * u_FrontPlane;
	v_EyeVector = EyeVectorWorld * v_StrMatrix;
#endif

	gl_Position = u_ModelViewProjectionMatrix * Position;
	v_ProjVector = gl_Position;
}
