#ifdef FRAGMENT_SHADER

myhalf FragmentSoftness(float Depth, sampler2D DepthTexture, in vec2 ScreenCoord, in ivec4 Viewport, in float ZNear, in float ZFar, myhalf Scale)
{
	vec2 tc = ScreenCoord * u_TextureParams.zw;

	myhalf fragdepth = ZNear*ZFar/(ZFar - qf_texture(DepthTexture, tc).r*(ZFar-ZNear));
	myhalf partdepth = Depth;
	
	myhalf d = max((fragdepth - partdepth) * Scale, 0.0);
	myhalf softness = 1.0 - min(1.0, d);
	
	softness *= softness;
	softness = 1.0 - softness * softness;
	return softness;
}

#endif
