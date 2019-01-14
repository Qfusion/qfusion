vec3 DirectionalLightColor(in vec3 surfaceNormalModelspace, out vec3 weightedDiffuseNormalModelspace)
{
	vec3 diffuseNormalModelspace;
	vec3 color = vec3(0.0);

#ifdef APPLY_DIRECTIONAL_LIGHT_FROM_NORMAL
	diffuseNormalModelspace = v_StrMatrix[2];
#else
	diffuseNormalModelspace = u_LightDir;
#endif // APPLY_DIRECTIONAL_LIGHT_FROM_NORMAL

	weightedDiffuseNormalModelspace = diffuseNormalModelspace;

#if defined(APPLY_DIRECTIONAL_LIGHT_MIX)
	color.rgb += qf_FrontColor.rgb;
#else
	float diffuseProduct = float(dot(surfaceNormalModelspace, diffuseNormalModelspace));

#ifdef APPLY_HALFLAMBERT
	diffuseProduct = clamp(diffuseProduct, 0.0, 1.0) * 0.5 + 0.5;
	diffuseProduct *= diffuseProduct;
#endif // APPLY_HALFLAMBERT

	vec3 diffuse = LinearColor(u_LightDiffuse.rgb) * float(max (diffuseProduct, 0.0)) + LinearColor(u_LightAmbient.rgb);
	color.rgb += diffuse;

#endif // APPLY_DIRECTIONAL_LIGHT_MIX

	return color;
}
