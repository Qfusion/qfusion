uniform vec4 u_DeluxemapOffset[(NUM_LIGHTMAPS + 3) / 4]; // s-offset for v_LightmapTexCoord
uniform LightmapSampler u_LightmapTexture0;
#if NUM_LIGHTMAPS >= 2
uniform LightmapSampler u_LightmapTexture1;
#if NUM_LIGHTMAPS >= 3
uniform LightmapSampler u_LightmapTexture2;
#if NUM_LIGHTMAPS >= 4
uniform LightmapSampler u_LightmapTexture3;
#endif // NUM_LIGHTMAPS >= 4
#endif // NUM_LIGHTMAPS >= 3
#endif // NUM_LIGHTMAPS >= 2

// deluxemapping using light vectors in modelspace
myhalf3 LightmapColor(in myhalf3 surfaceNormalModelspace, out myhalf3 weightedDiffuseNormalModelspace)
{
	myhalf3 diffuseNormalModelspace;
	myhalf diffuseProduct;
	myhalf3 color = myhalf3(0.0);
	
	// get light normal
	diffuseNormalModelspace = normalize(myhalf3 (Lightmap(u_LightmapTexture0, v_LightmapTexCoord01.st+vec2(u_DeluxemapOffset[0].x, 0.0), v_LightmapLayer0123.x)) - myhalf3 (0.5));
	// calculate directional shading
	diffuseProduct = float (dot (surfaceNormalModelspace, diffuseNormalModelspace));

#ifdef APPLY_FBLIGHTMAP
	weightedDiffuseNormalModelspace = diffuseNormalModelspace;
	// apply lightmap color
	color.rgb += myhalf3 (max (diffuseProduct, 0.0) * myhalf3 (Lightmap(u_LightmapTexture0, v_LightmapTexCoord01.st, v_LightmapLayer0123.x)));
#else
#define NORMALIZE_DIFFUSE_NORMAL
	weightedDiffuseNormalModelspace = u_LightstyleColor[0] * diffuseNormalModelspace;
	// apply lightmap color
	color.rgb += u_LightstyleColor[0] * myhalf(max (diffuseProduct, 0.0)) * myhalf3 (Lightmap(u_LightmapTexture0, v_LightmapTexCoord01.st, v_LightmapLayer0123.x));
#endif // APPLY_FBLIGHTMAP

#ifdef APPLY_AMBIENT_COMPENSATION
	// compensate for ambient lighting
	color.rgb += myhalf((1.0 - max (diffuseProduct, 0.0))) * u_LightAmbient;
#endif

#if NUM_LIGHTMAPS >= 2
	diffuseNormalModelspace = normalize(myhalf3 (Lightmap(u_LightmapTexture1, v_LightmapTexCoord01.pq+vec2(u_DeluxemapOffset[0].y,0.0), v_LightmapLayer0123.y)) - myhalf3 (0.5));
	diffuseProduct = float (dot (surfaceNormalModelspace, diffuseNormalModelspace));
	weightedDiffuseNormalModelspace += u_LightstyleColor[1] * diffuseNormalModelspace;
	color.rgb += u_LightstyleColor[1] * myhalf(max (diffuseProduct, 0.0)) * myhalf3 (Lightmap(u_LightmapTexture1, v_LightmapTexCoord01.pq, v_LightmapLayer0123.y));
#if NUM_LIGHTMAPS >= 3
	diffuseNormalModelspace = normalize(myhalf3 (Lightmap(u_LightmapTexture2, v_LightmapTexCoord23.st+vec2(u_DeluxemapOffset[0].z,0.0), v_LightmapLayer0123.z)) - myhalf3 (0.5));
	diffuseProduct = float (dot (surfaceNormalModelspace, diffuseNormalModelspace));
	weightedDiffuseNormalModelspace += u_LightstyleColor[2] * diffuseNormalModelspace;
	color.rgb += u_LightstyleColor[2] * myhalf(max (diffuseProduct, 0.0)) * myhalf3 (Lightmap(u_LightmapTexture2, v_LightmapTexCoord23.st, v_LightmapLayer0123.z));
#if NUM_LIGHTMAPS >= 4
	diffuseNormalModelspace = normalize(myhalf3 (Lightmap(u_LightmapTexture3, v_LightmapTexCoord23.pq+vec2(u_DeluxemapOffset[0].w,0.0), v_LightmapLayer0123.w)) - myhalf3 (0.5));
	diffuseProduct = float (dot (surfaceNormalModelspace, diffuseNormalModelspace));
	weightedDiffuseNormalModelspace += u_LightstyleColor[3] * diffuseNormalModelspace;
	color.rgb += u_LightstyleColor[3] * myhalf(max (diffuseProduct, 0.0)) * myhalf3 (Lightmap(u_LightmapTexture3, v_LightmapTexCoord23.pq, v_LightmapLayer0123.w));
#endif // NUM_LIGHTMAPS >= 4
#endif // NUM_LIGHTMAPS >= 3
#endif // NUM_LIGHTMAPS >= 2

	return color;
}
