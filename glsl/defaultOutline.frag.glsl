#include "include/common.glsl"
#include "include/uniforms.glsl"

#ifdef APPLY_FOG
#include "include/fog.glsl"

qf_varying vec2 v_FogCoord;
#endif

uniform float u_OutlineCutOff;

void main(void)
{
#ifdef APPLY_OUTLINES_CUTOFF
	if (u_OutlineCutOff > 0.0 && (gl_FragCoord.z / gl_FragCoord.w > u_OutlineCutOff))
		discard;
#endif

#ifdef APPLY_FOG
	myhalf fogDensity = FogDensity(v_FogCoord);
	qf_FragColor = vec4(vec3(mix(qf_FrontColor.rgb, u_FogColor, fogDensity)), 1.0);
#else
	qf_FragColor = vec4(qf_FrontColor);
#endif
}
