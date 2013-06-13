// RGB-encoded depth GLSL shader

#include "include/uniforms.glsl"
#include "include/rgbdepth.glsl"

varying float v_Depth;

#ifdef VERTEX_SHADER
// Vertex shader

#include "include/attributes.glsl"
#include "include/vtransform.glsl"

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal;
	vec2 TexCoord = a_TexCoord;

	TransformVerts(Position, Normal, TexCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;
	v_Depth = gl_Position.z;
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER
// Fragment shader

void main(void)
{
	gl_FragColor = encodedepthmacro(v_Depth);
}

#endif // FRAGMENT_SHADER
