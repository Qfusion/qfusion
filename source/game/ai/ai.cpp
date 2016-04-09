#include "bot.h"
#include "aas.h"
#include "../../gameshared/q_collision.h"

#include <stdarg.h>

Ai::Ai(edict_t *self, int allowedAasTravelFlags, int preferredAasTravelFlags)
    : EdictRef(self),
      aiBaseBrain(nullptr), // Must be set in a subclass constructor
      currAasAreaNum(0),
      goalAasAreaNum(0),
      goalTargetPoint(0, 0, 0),
      allowedAasTravelFlags(allowedAasTravelFlags),
      preferredAasTravelFlags(preferredAasTravelFlags),
      distanceToNextReachStart(std::numeric_limits<float>::infinity()),
      distanceToNextReachEnd(std::numeric_limits<float>::infinity()),
      blockedTimeout(level.time + 15000),
      aiYawSpeed(0.0f),
      aiPitchSpeed(0.0f)
{
    // TODO: Modify preferred aas travel flags if there is no selfdamage for excessive rocketjumping
}

void Ai::Debug(const char *format, ...) const
{
    va_list va;
    va_start(va, format);
    AI_Debugv(Nick(), format, va);
    va_end(va);
}

void Ai::FailWith(const char *format, ...) const
{
    va_list va;
    va_start(va, format);
    AI_Debugv(Nick(), format, va);
    va_end(va);
    abort();
}

static void EscapePercent(const char *string, char *buffer, int bufferLen)
{
    int j = 0;
    for (const char *s = string; *s && j < bufferLen - 1; ++s)
    {
        if (*s != '%')
            buffer[j++] = *s;
        else if (j < bufferLen - 2)
            buffer[j++] = '%', buffer[j++] = '%';
    }
    buffer[j] = '\0';
}

void AI_Debug(const char *nick, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    AI_Debugv(nick, format, va);
    va_end(va);
}

void AI_Debugv(const char *nick, const char *format, va_list va)
{
    char concatBuffer[1024];

    int prefixLen = sprintf(concatBuffer, "t=%09d %s: ", level.time, nick);

    Q_vsnprintfz(concatBuffer + prefixLen, 1024 - prefixLen, format, va);

    // concatBuffer may contain player names such as "%APPDATA%"
    char outputBuffer[2048];
    EscapePercent(concatBuffer, outputBuffer, 2048);
    G_Printf(outputBuffer);
    printf(outputBuffer);
}

template<typename AASFn>
static int FindAASParamToGoalArea(
    AASFn fn, int fromAreaNum, const vec_t *origin, int goalAreaNum, const edict_t *ignoreInTrace, int travelFlags)
{
    int param = fn(fromAreaNum, const_cast<float*>(origin), goalAreaNum, travelFlags);
    if (param)
        return param;

    float DEPTH_LIMIT = 1.5f * (playerbox_stand_maxs[2] - playerbox_stand_mins[2]);
    float distanceToGround = FindDistanceToGround(origin, ignoreInTrace, DEPTH_LIMIT);
    // Distance to ground is significantly greater than player height
    if (distanceToGround > DEPTH_LIMIT)
        return 0;

    // Add some epsilon offset from ground
    vec3_t projectedOrigin = { origin[0], origin[1], origin[2] - distanceToGround + 1.0f };
    return fn(fromAreaNum, projectedOrigin, goalAreaNum, travelFlags);
}

int FindAASReachabilityToGoalArea(
    int fromAreaNum, const vec3_t fromOrigin, int goalAreaNum, const edict_t *ignoreInTrace, int travelFlags)
{
    return ::FindAASParamToGoalArea(
        AAS_AreaReachabilityToGoalArea, fromAreaNum, fromOrigin, goalAreaNum, ignoreInTrace, travelFlags);
}

int FindAASTravelTimeToGoalArea(
    int fromAreaNum, const vec3_t fromOrigin , int goalAreaNum, const edict_t *ignoreInTrace, int travelFlags)
{
    return ::FindAASParamToGoalArea(
        AAS_AreaTravelTimeToGoalArea, fromAreaNum, fromOrigin, goalAreaNum, ignoreInTrace, travelFlags);
}

float FindSquareDistanceToGround(const vec3_t origin, const edict_t *ignoreInTrace, float traceDepth)
{
    vec3_t end = { origin[0], origin[1], origin[2] - traceDepth };

    trace_t trace;
    edict_t *self = const_cast<edict_t*>(ignoreInTrace);
    G_Trace(&trace, const_cast<float*>(origin), playerbox_stand_mins, playerbox_stand_maxs, end, self, MASK_AISOLID);

    // We do not use trace.fraction to avoid floating point computation issues (avoid 0.000001 * 999999)
    return trace.fraction != 1.0f ? DistanceSquared(origin, trace.endpos) : INFINITY;
}

float FindDistanceToGround(const vec3_t origin, const edict_t *ignoreInTrace, float traceDepth)
{
    float squareDistance = FindSquareDistanceToGround(origin, ignoreInTrace, traceDepth);
    if (squareDistance == 0)
        return 0;
    if (squareDistance == INFINITY)
        return INFINITY;
    return 1.0f / Q_RSqrt(squareDistance);
}
