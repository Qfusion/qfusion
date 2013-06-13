struct Fog
{
	float EyeDist;
	vec4 EyePlane, Plane;
	myhalf3 Color;
	float Scale;
};

uniform Fog u_Fog;

#define FogDensity(coord) sqrt(clamp((coord)[0],0.0,1.0))*step(FOG_TEXCOORD_STEP,(coord)[1])

#define FOG_GEN_OUTPUT_COLOR
#include "fog_overload.glsl"

#undef FOG_GEN_OUTPUT_COLOR
#define FOG_GEN_OUTPUT_TEXCOORDS
#include "fog_overload.glsl"
