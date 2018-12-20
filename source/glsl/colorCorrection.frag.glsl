#include "include/common.glsl"
#include "include/dither.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;

#ifdef APPLY_LUT
uniform sampler2D u_ColorLUT;
#endif

const float W = 0.90; // the white point
const float B = 0.80; // the bright point for bloom

#ifdef APPLY_HDR

uniform myhalf u_HDRGamma;
uniform myhalf u_HDRExposure;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    vec3 s = ((x*(a*x+b))/(x*(c*x+d)+e));
	return s;
}

vec3 ToneMap(vec3 c)
{
	return ACESFilm(c / 2.0) / ACESFilm(vec3(W) / 2.0);
}

#endif

#ifdef APPLY_LUT

vec3 ColorMap(vec3 c)
{
	vec3 coords = c;
	coords.rg = coords.rg * vec2(0.9375) + vec2(0.03125);
	coords *= vec3(1.0, 0.0625, 15.0);
	float blueMix = fract(coords.b);
	vec2 blueOffset = vec2(0.0, floor(coords.b) * 0.0625);
	vec3 color1 = qf_texture(u_ColorLUT, coords.rg + blueOffset).rgb;
	blueOffset.y = min(blueOffset.y + 0.0625, 0.9375);
	vec3 color2 = qf_texture(u_ColorLUT, coords.rg + blueOffset).rgb;
	coords = mix(color1, color2, blueMix);
	return coords;
}

#endif

void main(void)
{
	vec4 texel = qf_texture(u_BaseTexture, v_TexCoord);
	vec3 color = texel.rgb;

#ifdef APPLY_HDR
	color = ToneMap(color * u_HDRExposure);
#endif

#ifdef APPLY_SRGB2LINEAR

#ifdef APPLY_HDR
	color = pow(color, vec3(1.0 / u_HDRGamma));
#else
	color = pow(color, vec3(1.0 / 2.2));
#endif

#endif

#if defined(APPLY_LUT) && defined(APPLY_SRGB2LINEAR)
	color = clamp(color, 0.0, 1.0);
#endif

#ifdef APPLY_LUT
	color = ColorMap(color);
#endif // APPLY_LUT

	color += dither();

	qf_FragColor = vec4(color, texel.a);
}
