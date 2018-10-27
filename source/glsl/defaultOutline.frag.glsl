#include "include/common.glsl"
#include "include/uniforms.glsl"
#include_if(APPLY_FOG) "include/fog.glsl"

#ifdef APPLY_FOG
qf_varying vec2 v_FogCoord;
#endif

uniform float u_OutlineCutOff;

void main(void)
{
	vec4 color;

#ifdef APPLY_OUTLINES_CUTOFF
	if (u_OutlineCutOff > 0.0 && (gl_FragCoord.z / gl_FragCoord.w > u_OutlineCutOff))
		discard;
#endif

#if defined(APPLY_FOG) && !defined(APPLY_FOG_COLOR)
	myhalf fogDensity = FogDensity(v_FogCoord);
	color = vec4(vec3(mix(qf_FrontColor.rgb, LinearColor(u_FogColor), fogDensity)), 1.0);
#else
	color = qf_FrontColor;
#endif

	qf_FragColor = sRGBColor(color);
}
