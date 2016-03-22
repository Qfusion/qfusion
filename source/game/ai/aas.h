#ifndef QFUSION_AI_AAS_H
#define QFUSION_AI_AAS_H

#include "../../gameshared/q_math.h"
#include "aas/aasfile.h"
#include "aas/botlib.h"
#include "aas/be_aas.h"
#include "aas/be_aas_def.h"
#include "aas/be_aas_funcs.h"
#include "aas/be_aas_route.h"
#include "aas/be_aas_debug.h"

// It does not exposed by aas headers
extern "C" int AAS_AreaRouteToGoalArea(
    int areanum, vec3_t origin, int goalareanum, int travelflags, int *traveltime, int *reachnum);

extern "C" int AAS_AreaReachabilityToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);

bool AI_InitAAS();

bool AI_LoadLevelAAS(const char *mapname);

void AI_ShutdownAAS();

void AI_AASFrame();

#endif
