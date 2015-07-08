#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;
uniform sampler2D u_ColorLUT;

void main(void)
{
	vec3 coords = qf_texture(u_BaseTexture, v_TexCoord).rgb;
	coords.rg = coords.rg * vec2(0.9375) + vec2(0.03125);
	coords *= vec3(1.0, 0.0625, 15.0);
	float blueMix = fract(coords.b);
	vec2 blueOffset = vec2(0.0, floor(coords.b) * 0.0625);
	vec3 color1 = qf_texture(u_ColorLUT, coords.rg + blueOffset).rgb;
	blueOffset.y = min(blueOffset.y + 0.0625, 0.9375);
	vec3 color2 = qf_texture(u_ColorLUT, coords.rg + blueOffset).rgb;
	qf_FragColor = vec4(mix(color1, color2, blueMix), 1.0);
}
