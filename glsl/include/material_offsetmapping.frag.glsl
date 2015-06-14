#if defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)
// The following reliefmapping and offsetmapping routine was taken from DarkPlaces
// The credit goes to LordHavoc (as always)
vec2 OffsetMapping(sampler2D NormalmapTexture, in vec2 TexCoord, in vec3 EyeVector, in float OffsetMappingScale)
{
#ifdef APPLY_RELIEFMAPPING
	// 14 sample relief mapping: linear search and then binary search
	// this basically steps forward a small amount repeatedly until it finds
	// itself inside solid, then jitters forward and back using decreasing
	// amounts to find the impact
	//vec3 OffsetVector = vec3(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMappingScale) * vec2(-1, 1), -1);
	//vec3 OffsetVector = vec3(normalize(EyeVector.xy) * OffsetMappingScale * vec2(-1, 1), -1);
	vec3 OffsetVector = vec3(normalize(EyeVector).xy * OffsetMappingScale * vec2(-1, 1), -1);
	vec3 RT = vec3(TexCoord, 1);
	OffsetVector *= 0.1;
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector *  step(qf_texture(NormalmapTexture, RT.xy).a, RT.z);
	RT += OffsetVector * (step(qf_texture(NormalmapTexture, RT.xy).a, RT.z)          - 0.5);
	RT += OffsetVector * (step(qf_texture(NormalmapTexture, RT.xy).a, RT.z) * 0.5    - 0.25);
	RT += OffsetVector * (step(qf_texture(NormalmapTexture, RT.xy).a, RT.z) * 0.25   - 0.125);
	RT += OffsetVector * (step(qf_texture(NormalmapTexture, RT.xy).a, RT.z) * 0.125  - 0.0625);
	RT += OffsetVector * (step(qf_texture(NormalmapTexture, RT.xy).a, RT.z) * 0.0625 - 0.03125);
	return RT.xy;
#else
	// 2 sample offset mapping (only 2 samples because of ATI Radeon 9500-9800/X300 limits)
	// this basically moves forward the full distance, and then backs up based
	// on height of samples
	//vec2 OffsetVector = vec2(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMappingScale) * vec2(-1, 1));
	//vec2 OffsetVector = vec2(normalize(EyeVector.xy) * OffsetMappingScale * vec2(-1, 1));
	vec2 OffsetVector = vec2(normalize(EyeVector).xy * OffsetMappingScale * vec2(-1, 1));
	vec2 TexCoord_ = TexCoord + OffsetVector;
	OffsetVector *= 0.5;
	TexCoord_ -= OffsetVector * qf_texture(NormalmapTexture, TexCoord_).a;
	TexCoord_ -= OffsetVector * qf_texture(NormalmapTexture, TexCoord_).a;
	return TexCoord_;
#endif // APPLY_RELIEFMAPPING
}
#endif // defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)
