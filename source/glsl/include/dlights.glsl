#if defined(NUM_DLIGHTS)

uniform vec3 u_DlightPosition[NUM_DLIGHTS];
uniform myhalf4 u_DlightDiffuseAndInvRadius[NUM_DLIGHTS];
#if !defined(GL_ES) && (QF_GLSL_VERSION >= 330)
uniform int u_NumDynamicLights;
#endif

#include "dlights_overload.glsl"

#define DLIGHTS_SURFACE_NORMAL_IN
#include "dlights_overload.glsl"

#endif
