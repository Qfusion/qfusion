myhalf3 YUV2RGB(vec3 yuv)
{
	// Standard definition TV color matrix
	const mat3 yuv2rgb = mat3(
		1.0, 1.0, 1.0, 
		0.0, -0.343, -0.711,
		1.400, -0.711, 0.0
	);
	const vec3 yuv_sub = vec3(
		0.0, 0.5, 0.5
	);

	return clamp( (yuv2rgb * (yuv - yuv_sub)).xyz, 0.0, 1.0 );
}

myhalf3 YUV2RGB_HDTV(vec3 yuv)
{
	// High Definition TV color matrix
	const mat3 yuv2rgb = mat3(
		1.164, 1.164, 1.164, 
		0.0, -0.213, 2.212,
		1.793, -0.533, 0.0
	);
	const vec3 yuv_sub = vec3(
		0.0625, 0.5, 0.5
	);

	return clamp( (yuv2rgb * (yuv - yuv_sub)).xyz, 0.0, 1.0 );
}
