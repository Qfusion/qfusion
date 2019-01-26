#ifdef FRAGMENT_SHADER

uniform float u_SoftParticlesScale;

float FragmentSoftness(float Depth, sampler2D DepthTexture, in vec2 ScreenCoord, in float NearClip)
{
	vec2 tc = ScreenCoord * u_TextureParams.zw;

	float fragdepth = NearClip / ( 1.0 - qf_texture(DepthTexture, tc).r );
	float partdepth = Depth;
	
	float d = max((fragdepth - partdepth) * u_SoftParticlesScale, 0.0);
	float softness = 1.0 - min(1.0, d);
	
	softness *= softness;
	softness = 1.0 - softness * softness;
	return softness;
}

#endif
