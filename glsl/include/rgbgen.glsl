uniform myhalf4 u_ConstColor;
uniform myhalf4 u_RGBGenFuncArgs, u_AlphaGenFuncArgs;

myhalf4 VertexRGBGen(in vec4 Position, in vec3 Normal, in myhalf4 VertexColor)
{
#if defined(APPLY_RGB_DISTANCERAMP) || defined(APPLY_ALPHA_DISTANCERAMP)
#define DISTANCERAMP(x1,x2,y1,y2) mix(y1, y2, smoothstep(x1, x2, myhalf(dot(u_EntityDist - Position.xyz, Normal))))
#endif

#if defined(APPLY_RGB_CONST) && defined(APPLY_ALPHA_CONST)
	myhalf4 Color = u_ConstColor;
#else
	myhalf4 Color = myhalf4(1.0);

#if defined(APPLY_RGB_CONST)
	Color.rgb = u_ConstColor.rgb;
#elif defined(APPLY_RGB_VERTEX)
	Color.rgb = VertexColor.rgb;
#elif defined(APPLY_RGB_ONE_MINUS_VERTEX)
	Color.rgb = myhalf3(1.0) - VertexColor.rgb;
#elif defined(APPLY_RGB_GEN_DIFFUSELIGHT)
	Color.rgb = myhalf3(u_LightAmbient + max(dot(u_LightDir, Normal), 0.0) * u_LightDiffuse);
#endif

#if defined(APPLY_ALPHA_CONST)
	Color.a = u_ConstColor.a;
#elif defined(APPLY_ALPHA_VERTEX)
	Color.a = VertexColor.a;
#elif defined(APPLY_ALPHA_ONE_MINUS_VERTEX)
	Color.a = 1.0 - VertexColor.a;
#endif

#endif

#ifdef APPLY_RGB_DISTANCERAMP
	Color.rgb *= DISTANCERAMP(u_RGBGenFuncArgs[2], u_RGBGenFuncArgs[3], u_RGBGenFuncArgs[0], u_RGBGenFuncArgs[1]);
#endif

#ifdef APPLY_ALPHA_DISTANCERAMP
	Color.a *= DISTANCERAMP(u_AlphaGenFuncArgs[2], u_AlphaGenFuncArgs[3], u_AlphaGenFuncArgs[0], u_AlphaGenFuncArgs[1]);
#endif

	return Color;
#if defined(DISTANCERAMP)
#undef DISTANCERAMP
#endif
}
