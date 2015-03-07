#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;
#ifdef APPLY_3D_LUT
uniform sampler3D u_ColorLUT;
#else
uniform sampler2D u_ColorLUT;
#endif

void main(void)
{
	vec3 coords = qf_texture(u_BaseTexture, v_TexCoord).rgb * vec3(0.96875) + vec3(0.015625);
#ifdef APPLY_3D_LUT
	qf_FragColor = vec4(qf_texture3D(u_ColorLUT, coords).rgb, 1.0);
#else
	coords *= vec3(0.125, 0.25, 32.0);
	float blueMix = fract(coords.b);
	coords.b = floor(coords.b) * 0.25;
	vec3 color1 = qf_texture(u_ColorLUT, coords.rg + vec2(floor(coords.b) * 0.125, fract(coords.b))).rgb;
	coords.b = min(coords.b + 0.25, 7.75);
	vec3 color2 = qf_texture(u_ColorLUT, coords.rg + vec2(floor(coords.b) * 0.125, fract(coords.b))).rgb;
	qf_FragColor = vec4(mix(color1, color2, blueMix), 1.0);
#endif
}