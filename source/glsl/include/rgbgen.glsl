uniform vec4 u_ConstColor;
uniform vec4 u_RGBGenFuncArgs, u_AlphaGenFuncArgs;

vec4 VertexRGBGen(in vec4 Position, in vec3 Normal, in vec4 VertexColor)
{
	vec4 Color;

#if defined(APPLY_RGB_DISTANCERAMP) || defined(APPLY_ALPHA_DISTANCERAMP)
#define DISTANCERAMP(x1,x2,y1,y2) mix(y1, y2, smoothstep(x1, x2, float(dot(u_EntityDist - Position.xyz, Normal))))
#endif

#if defined(APPLY_RGB_VERTEX)
	Color.rgb = LinearColor(VertexColor.rgb);
#else
	Color.rgb = LinearColor(u_ConstColor.rgb) * u_ColorMod;
#endif

#if defined(APPLY_ALPHA_VERTEX)
	Color.a = VertexColor.a;
#else
	Color.a = u_ConstColor.a;
#endif

#ifdef APPLY_RGB_DISTANCERAMP
	Color.rgb *= LinearColor(DISTANCERAMP(u_RGBGenFuncArgs[2], u_RGBGenFuncArgs[3], u_RGBGenFuncArgs[0], u_RGBGenFuncArgs[1]));
#endif

#ifdef APPLY_ALPHA_DISTANCERAMP
	Color.a *= DISTANCERAMP(u_AlphaGenFuncArgs[2], u_AlphaGenFuncArgs[3], u_AlphaGenFuncArgs[0], u_AlphaGenFuncArgs[1]);
#endif

	return Color;
#if defined(DISTANCERAMP)
#undef DISTANCERAMP
#endif
}
