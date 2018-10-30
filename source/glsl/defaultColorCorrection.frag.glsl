#include "include/common.glsl"
#include "include/uniforms.glsl"

qf_varying vec2 v_TexCoord;

uniform sampler2D u_BaseTexture;

#ifdef APPLY_LUT
uniform sampler2D u_ColorLUT;
#endif

#ifdef APPLY_BLOOM
uniform sampler2D u_BloomTexture0;
uniform sampler2D u_BloomTexture1;
uniform sampler2D u_BloomTexture2;
uniform sampler2D u_BloomTexture3;
#endif

const float W = 0.90; // the white point
const float B = 0.80; // the bright point for bloom

#ifdef APPLY_HDR

uniform myhalf u_HDRGamma;
uniform myhalf u_HDRExposure;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    vec3 s = ((x*(a*x+b))/(x*(c*x+d)+e));
	return s;
}

vec3 ToneMap(vec3 c)
{
	return ACESFilm(c / 2.0) / ACESFilm(vec3(W) / 2.0);
}

#endif

#ifdef APPLY_BLOOM
vec3 Bloom(vec2 t)
{
	vec3 bloom = qf_texture(u_BloomTexture0, t).rgb +
				 qf_texture(u_BloomTexture1, t).rgb +
				 qf_texture(u_BloomTexture2, t).rgb +
				 qf_texture(u_BloomTexture3, t).rgb;
	return clamp(bloom, 0.0, 1.0);
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
	vec4 texel = texture(u_BaseTexture, v_TexCoord);
	vec3 coords = texel.rgb;

#ifdef APPLY_HDR
	coords = ToneMap(coords * u_HDRExposure);
#endif

#ifdef APPLY_SRGB2LINEAR

#ifdef APPLY_HDR
	coords = pow(coords, vec3(1.0 / u_HDRGamma));
#else
	coords = pow(coords, vec3(1.0 / 2.2));
#endif

#endif

#ifdef APPLY_OVEBRIGHT

	qf_FragColor = vec4(coords, texel.a);
	qf_BrightColor = vec4(coords - clamp(coords, 0.0, B), texel.a);
	
#else

#if defined(APPLY_LUT) && defined(APPLY_SRGB2LINEAR)
	coords = clamp(coords, 0.0, 1.0);
#endif

#ifdef APPLY_LUT
	coords = ColorMap(coords);

#ifdef APPLY_BLOOM
	coords += ColorMap(Bloom(v_TexCoord));
#endif // APPLY_BLOOM

#else

#ifdef APPLY_BLOOM
	coords += Bloom(v_TexCoord);
#endif // APPLY_BLOOM

#endif // APPLY_LUT

	qf_FragColor = vec4(coords, texel.a);
#endif // APPLY_OUT_BLOOM
}
