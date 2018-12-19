#define DRAWFLAT_NORMAL_STEP	0.5		// floor or ceiling if < abs(normal.z)

myhalf LinearFromsRGB(myhalf c)
{
	if (c <= 0.04045)
		return c * (1.0 / 12.92);
	return myhalf(pow((c + 0.055)*(1.0/1.055), 2.4));
}

myhalf3 LinearFromsRGB(myhalf3 v)
{
	return myhalf3(LinearFromsRGB(v.r), LinearFromsRGB(v.g), LinearFromsRGB(v.b));
}

myhalf4 LinearFromsRGB(myhalf4 v)
{
	return myhalf4(LinearFromsRGB(v.r), LinearFromsRGB(v.g), LinearFromsRGB(v.b), v.a);
}

myhalf sRGBFromLinear(myhalf c)
{
	if (c < 0.0031308)
		return c * 12.92;
	return 1.055 * pow(c, 1.0/2.4) - 0.055;
}

myhalf3 sRGBFromLinear(myhalf3 v)
{
	return myhalf3(sRGBFromLinear(v.r), sRGBFromLinear(v.g), sRGBFromLinear(v.b));
}

myhalf4 sRGBFromLinear(myhalf4 v)
{
	return myhalf4(sRGBFromLinear(v.r), sRGBFromLinear(v.g), sRGBFromLinear(v.b), v.a);
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

#if defined(APPLY_RGB_DISTANCERAMP) || defined(APPLY_RGB_CONST) || defined(APPLY_RGB_VERTEX) || defined(APPLY_RGB_ONE_MINUS_VERTEX) || defined(APPLY_RGB_GEN_DIFFUSELIGHT)
#define APPLY_ENV_MODULATE_COLOR
#else

#if defined(APPLY_ALPHA_DISTANCERAMP) || defined(APPLY_ALPHA_CONST) || defined(APPLY_ALPHA_VERTEX) || defined(APPLY_ALPHA_ONE_MINUS_VERTEX)
#define APPLY_ENV_MODULATE_COLOR
#endif

#endif
