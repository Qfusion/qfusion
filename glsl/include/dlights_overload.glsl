#ifdef DLIGHTS_SURFACE_NORMAL_IN
myhalf3 DynamicLightsSummaryColor(in vec3 Position, in myhalf3 surfaceNormalModelspace)
#else
myhalf3 DynamicLightsSummaryColor(in vec3 Position)
#endif
{
	myhalf3 Color = myhalf3(0.0);

#if QF_GLSL_VERSION >= 330
	for (int i = 0; i < u_NumDynamicLights; i++)
#else
	for (int i = 0; i < NUM_DLIGHTS; i++)
#endif
	{
		myhalf3 STR = myhalf3(u_DynamicLights[i].Position - Position);
		myhalf distance = length(STR);
		myhalf falloff = clamp(1.0 - distance / u_DynamicLights[i].Radius, 0.0, 1.0);

		falloff *= falloff;

		#ifdef DLIGHTS_SURFACE_NORMAL_IN
		falloff *= myhalf(max(dot(normalize(STR), surfaceNormalModelspace), 0.0));
		#endif

		Color += falloff * u_DynamicLights[i].Diffuse;
	}

	return Color;
}
