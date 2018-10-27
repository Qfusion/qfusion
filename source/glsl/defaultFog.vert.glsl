#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/fog.glsl"
#include "include/attributes.glsl"

qf_varying vec2 v_FogCoord;

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;

	QF_TransformVerts(Position, Normal, TexCoord);

	FogGenCoord(Position, v_FogCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;
}
