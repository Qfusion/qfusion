myhalf3 YUV2RGB(vec3 yuv)
{
	mat3 yuv2rgb = mat3(
		1.0, 1.0, 1.0, 
		0.0, -0.344, 1.772,
		1.403, -0.714, 0.0
	);
	return clamp( (yuv2rgb * (yuv - vec3(0.0, 0.5, 0.5))).xyz, 0.0, 1.0 );
}
