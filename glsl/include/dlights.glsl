#if defined(NUM_DLIGHTS)

struct DynamicLight
{
	myhalf Radius;
	vec3 Position;
	myhalf3 Diffuse;
};

uniform DynamicLight u_DynamicLights[NUM_DLIGHTS];
uniform int u_NumDynamicLights;

#include "dlights_overload.glsl"

#define DLIGHTS_SURFACE_NORMAL_IN
#include "dlights_overload.glsl"

#endif
