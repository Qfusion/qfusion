#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;
#ifdef APPLY_3D_TEXTURE
uniform sampler3D u_CorrectionTexture;
#else
uniform sampler2D u_CorrectionTexture;
#endif

void main(void)
{
	vec3 coords = qf_texture(u_BaseTexture, v_TexCoord).rgb * vec3(0.96875) + vec3(0.015625);
#ifdef APPLY_3D_TEXTURE
	qf_FragColor = vec4(qf_texture3D(u_CorrectionTexture, coords).rgb, 1.0);
#else
	coords.yz *= vec2(0.03125, 32.0);
	coords.y += floor(coords.z) * 0.03125;
	vec3 c1 = qf_texture(u_CorrectionTexture, coords.xy).rgb;
	vec3 c2 = qf_texture(u_CorrectionTexture, coords.xy + vec2(0.0, 0.03125 * step(coords.z, 31.0))).rgb;
	qf_FragColor = vec4(mix(c1, c2, fract(coords.z) * 0.03125), 1.0);
#endif
}