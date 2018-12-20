#include "include/common.glsl"
#include "include/uniforms.glsl"

uniform float u_OutlineCutOff;

void main(void)
{
#ifdef APPLY_OUTLINES_CUTOFF
	if (u_OutlineCutOff > 0.0 && (gl_FragCoord.z / gl_FragCoord.w > u_OutlineCutOff))
		discard;
#endif

	vec4 color = vec4( 1.0 ); //qf_FrontColor;

	qf_FragColor = sRGBColor(color);
}
