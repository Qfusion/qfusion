#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/rgbgen.glsl"
#include_if(APPLY_FOG) "include/fog.glsl"

#ifdef APPLY_FOG
qf_varying vec2 v_FogCoord;
#endif

uniform float u_OutlineHeight;

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;
	myhalf4 inColor = myhalf4(a_Color);

	QF_TransformVerts(Position, Normal, TexCoord);

	Position += vec4(Normal * u_OutlineHeight, 0.0);
	gl_Position = u_ModelViewProjectionMatrix * Position;

	myhalf4 outColor = VertexRGBGen(Position, Normal, inColor);

#ifdef APPLY_FOG
#if defined(APPLY_FOG_COLOR)
	FogGenColor(Position, outColor, u_BlendMix);
#else
	FogGenCoord(Position, v_FogCoord);
#endif
#endif // APPLY_FOG

	qf_FrontColor = vec4(outColor);
}
