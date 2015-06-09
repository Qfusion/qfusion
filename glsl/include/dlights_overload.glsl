#ifdef DLIGHTS_SURFACE_NORMAL_IN
myhalf3 DynamicLightsSurfaceColor(in vec3 Position, in myhalf3 surfaceNormalModelspace)
#else
myhalf3 DynamicLightsColor(in vec3 Position)
#endif
{
	myhalf3 Color = myhalf3(0.0);

#if NUM_DLIGHTS > 4 // prevent the compiler from possibly handling the NUM_DLIGHTS <= 4 case as a real loop
#if !defined(GL_ES) && (QF_GLSL_VERSION >= 330)
	for (int dlight = 0; dlight < u_NumDynamicLights; dlight += 4)
#else
	for (int dlight = 0; dlight < NUM_DLIGHTS; dlight += 4)
#endif
#else
#define dlight 0
#endif
	{
		myhalf3 STR0 = myhalf3(u_DlightPosition[dlight] - Position);
		myhalf3 STR1 = myhalf3(u_DlightPosition[dlight + 1] - Position);
		myhalf3 STR2 = myhalf3(u_DlightPosition[dlight + 2] - Position);
		myhalf3 STR3 = myhalf3(u_DlightPosition[dlight + 3] - Position);
		myhalf4 distance = myhalf4(length(STR0), length(STR1), length(STR2), length(STR3));
		myhalf4 falloff = clamp(myhalf4(1.0) - distance * u_DlightDiffuseAndInvRadius[dlight + 3], 0.0, 1.0);

		falloff *= falloff;

		#ifdef DLIGHTS_SURFACE_NORMAL_IN
		distance = myhalf4(1.0) / distance;
		falloff *= max(myhalf4(
			dot(STR0 * distance.xxx, surfaceNormalModelspace),
			dot(STR1 * distance.yyy, surfaceNormalModelspace),
			dot(STR2 * distance.zzz, surfaceNormalModelspace),
			dot(STR3 * distance.www, surfaceNormalModelspace)), 0.0);
		#endif

		Color += myhalf3(
			dot(u_DlightDiffuseAndInvRadius[dlight], falloff),
			dot(u_DlightDiffuseAndInvRadius[dlight + 1], falloff),
			dot(u_DlightDiffuseAndInvRadius[dlight + 2], falloff));
	}

	return Color;
#ifdef dlight
#undef dlight
#endif
}
