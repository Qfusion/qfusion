#define DRAWFLAT_NORMAL_STEP	0.5		// floor or ceiling if < abs(normal.z)

float LinearFromsRGB(float c)
{
	if (c <= 0.04045)
		return c * (1.0 / 12.92);
	return float(pow((c + 0.055)*(1.0/1.055), 2.4));
}

vec3 LinearFromsRGB(vec3 v)
{
	return vec3(LinearFromsRGB(v.r), LinearFromsRGB(v.g), LinearFromsRGB(v.b));
}

vec4 LinearFromsRGB(vec4 v)
{
	return vec4(LinearFromsRGB(v.r), LinearFromsRGB(v.g), LinearFromsRGB(v.b), v.a);
}

float sRGBFromLinear(float c)
{
	if (c < 0.0031308)
		return c * 12.92;
	return 1.055 * pow(c, 1.0/2.4) - 0.055;
}

vec3 sRGBFromLinear(vec3 v)
{
	return vec3(sRGBFromLinear(v.r), sRGBFromLinear(v.g), sRGBFromLinear(v.b));
}

vec4 sRGBFromLinear(vec4 v)
{
	return vec4(sRGBFromLinear(v.r), sRGBFromLinear(v.g), sRGBFromLinear(v.b), v.a);
}

float LinearizeDepth(float ndc, float nearclip)
{
	return nearclip / (1.0 - ndc);
}

#ifdef APPLY_SRGB2LINEAR
# define LinearColor(c) LinearFromsRGB(c)
#else
# define LinearColor(c) (c)
#endif

#ifdef APPLY_LINEAR2SRGB
# define sRGBColor(c) sRGBFromLinear(c)
#else
# define sRGBColor(c) (c)
#endif

#if defined(APPLY_RGB_DISTANCERAMP) || defined(APPLY_RGB_CONST) || defined(APPLY_RGB_VERTEX)
#define APPLY_ENV_MODULATE_COLOR
#else

#if defined(APPLY_ALPHA_DISTANCERAMP) || defined(APPLY_ALPHA_CONST) || defined(APPLY_ALPHA_VERTEX)
#define APPLY_ENV_MODULATE_COLOR
#endif

#endif
