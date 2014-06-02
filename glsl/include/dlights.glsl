#if defined(NUM_DLIGHTS)

struct DynamicLight
{
	vec3 Position;
	myhalf4 DiffuseAndInvRadius;
};

uniform DynamicLight u_DynamicLights[NUM_DLIGHTS];
#if !defined(GL_ES) && (QF_GLSL_VERSION >= 330)
uniform int u_NumDynamicLights;
#endif

#include "dlights_overload.glsl"

#define DLIGHTS_SURFACE_NORMAL_IN
#include "dlights_overload.glsl"

#endif
