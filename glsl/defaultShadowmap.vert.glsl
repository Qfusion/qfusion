#include "include/common.glsl"
#include "include/uniforms.glsl"
#include "include/attributes.glsl"
#include "include/vtransform.glsl"

#ifndef NUM_SHADOWS
#define NUM_SHADOWS 1
#endif

qf_varying vec4 v_ShadowProjVector[NUM_SHADOWS];

uniform mat4 u_ShadowmapMatrix[NUM_SHADOWS];

void main(void)
{
	vec4 Position = a_Position;
	vec3 Normal = a_Normal.xyz;
	vec2 TexCoord = a_TexCoord;

	TransformVerts(Position, Normal, TexCoord);

	gl_Position = u_ModelViewProjectionMatrix * Position;

	for (int i = 0; i < NUM_SHADOWS; i++)
	{
		v_ShadowProjVector[i] = u_ShadowmapMatrix[i] * Position;
		// a trick whish allows us not to perform the
		// 'shadowmaptc = (shadowmaptc + vec3 (1.0)) * vec3 (0.5)'
		// computation in the fragment shader
		v_ShadowProjVector[i].xyz = (v_ShadowProjVector[i].xyz + vec3(v_ShadowProjVector[i].w)) * 0.5;
	}
}
