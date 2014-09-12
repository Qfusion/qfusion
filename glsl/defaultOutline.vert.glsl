#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/vtransform.glsl"
#include "include/rgbgen.glsl"

#ifdef APPLY_FOG
#include "include/fog.glsl"

qf_varying vec2 v_FogCoord;
#endif

uniform float u_OutlineHeight;

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	myhalf4 inColor = myhalf4(a_Color);

	TransformVerts(Position, Normal, TexCoord);

	Position += vec4(Normal * u_OutlineHeight, 0.0);
	gl_Position = u_ModelViewProjectionMatrix * Position;

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

#ifdef APPLY_FOG
	FogGen(Position, v_FogCoord);
#endif

	qf_FrontColor = vec4(outColor);
}
