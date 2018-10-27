#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"
#include "include/attributes.glsl"

qf_varying float v_Depth;

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;

	QF_TransformVerts(Position, Normal, TexCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;
	v_Depth = gl_Position.z;
}
