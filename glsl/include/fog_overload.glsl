#if defined(FOG_GEN_OUTPUT_COLOR)
void FogGen(in vec4 Position, inout myhalf4 outColor, in myhalf2 blendMix)
#elif defined(FOG_GEN_OUTPUT_TEXCOORDS)
void FogGen(in vec4 Position, inout vec2 outTexCoord)
#endif
{
	// side = vec2(inside, outside)
	myhalf2 side = myhalf2(step(u_Fog.ScaleAndEyeDist.y, 0.0), step(0.0, u_Fog.ScaleAndEyeDist.y));
	myhalf FDist = dot(Position.xyz, u_Fog.EyePlane.xyz) - u_Fog.EyePlane.w;
	myhalf FVdist = dot(Position.xyz, u_Fog.Plane.xyz) - u_Fog.Plane.w;
	myhalf FogDistScale = FVdist / (FVdist - u_Fog.ScaleAndEyeDist.y);

#if defined(FOG_GEN_OUTPUT_COLOR)
	myhalf FogDist = FDist * dot(side, myhalf2(1.0, FogDistScale));
	myhalf FogScale = myhalf(clamp(1.0 - FogDist * u_Fog.ScaleAndEyeDist.x, 0.0, 1.0));
	outColor *= mix(myhalf4(1.0), myhalf4(FogScale), blendMix.xxxy);
#endif

#if defined(FOG_GEN_OUTPUT_TEXCOORDS)
	myhalf FogS = FDist * dot(side, myhalf2(1.0, step(FVdist, 0.0) * FogDistScale));
	myhalf FogT = -FVdist;
	outTexCoord = vec2(FogS * u_Fog.ScaleAndEyeDist.x, FogT * u_Fog.ScaleAndEyeDist.x + 1.5*FOG_TEXCOORD_STEP);
#endif
}
