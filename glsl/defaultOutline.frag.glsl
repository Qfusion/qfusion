#include "include/common.glsl"
#include "include/uniforms.glsl"
#ifdef APPLY_FOG
#include "include/fog.glsl"
#endif

uniform float u_OutlineCutOff;

void main(void)
{
#ifdef APPLY_OUTLINES_CUTOFF
	if (u_OutlineCutOff > 0.0 && (gl_FragCoord.z / gl_FragCoord.w > u_OutlineCutOff))
		discard;
#endif
	qf_FragColor = vec4(qf_FrontColor);
}
