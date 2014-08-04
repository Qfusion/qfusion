#if defined(FOG_GEN_OUTPUT_COLOR)
void FogGen(in vec4 Position, inout myhalf4 outColor, in myhalf2 blendMix)
#elif defined(FOG_GEN_OUTPUT_TEXCOORDS)
void FogGen(in vec4 Position, inout vec2 outTexCoord)
#endif
{
	// side = vec2(inside, outside)
	myhalf2 side = myhalf2(step(u_FogScaleAndEyeDist.y, 0.0), step(0.0, u_FogScaleAndEyeDist.y));
	myhalf FDist = dot(Position.xyz, u_FogEyePlane.xyz) - u_FogEyePlane.w;
	myhalf FVdist = dot(Position.xyz, u_FogPlane.xyz) - u_FogPlane.w;
	myhalf FogDistScale = FVdist / (FVdist - u_FogScaleAndEyeDist.y);

#if defined(FOG_GEN_OUTPUT_COLOR)
	myhalf FogDist = FDist * dot(side, myhalf2(1.0, FogDistScale));
	myhalf FogScale = myhalf(clamp(1.0 - FogDist * u_FogScaleAndEyeDist.x, 0.0, 1.0));
	outColor *= mix(myhalf4(1.0), myhalf4(FogScale), blendMix.xxxy);
#endif

#if defined(FOG_GEN_OUTPUT_TEXCOORDS)
	myhalf FogS = FDist * dot(side, myhalf2(1.0, step(FVdist, 0.0) * FogDistScale));
	myhalf FogT = -FVdist;
	outTexCoord = vec2(FogS * u_FogScaleAndEyeDist.x, FogT * u_FogScaleAndEyeDist.x + 1.5*FOG_TEXCOORD_STEP);
#endif
}
