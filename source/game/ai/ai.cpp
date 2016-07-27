#include "ai_local.h"
#include "ai.h"
#include "ai_ground_trace_cache.h"
#include "vec3.h"
#include <stdarg.h>

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

void AI_FailWith(const char *tag, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    AI_FailWithv(tag, format, va);
    va_end(va);
}

void AI_FailWithv(const char *tag, const char *format, va_list va)
{
    AI_Debugv(tag, format, va);
    fflush(stdout);
    abort();
}

static int FindAASAreaNum(const vec3_t mins, const vec3_t maxs)
{
    const vec_t *bounds[2] = { maxs, mins };
    // Test all AABB vertices
    vec3_t origin = { 0, 0, 0 };
    for (int i = 0; i < 8; ++i)
    {
        origin[0] = bounds[(i >> 0) & 1][0];
        origin[1] = bounds[(i >> 1) & 1][1];
        origin[2] = bounds[(i >> 2) & 1][2];
        int areaNum = AAS_PointAreaNum(origin);
        if (areaNum)
            return areaNum;
    }
    return 0;
}

int FindAASAreaNum(const vec3_t origin)
{
    int areaNum = AAS_PointAreaNum(const_cast<float*>(origin));
    if (areaNum)
        return areaNum;

    vec3_t mins = { -8, -8, 0 };
    VectorAdd(mins, origin, mins);
    vec3_t maxs = { +8, +8, 16 };
    VectorAdd(maxs, origin, maxs);
    return FindAASAreaNum(mins, maxs);
}

int FindAASAreaNum(const edict_t *ent)
{
    // Reject degenerate case
    if (ent->r.absmin[0] == ent->r.absmax[0] &&
        ent->r.absmin[1] == ent->r.absmax[1] &&
        ent->r.absmin[2] == ent->r.absmax[2])
        return FindAASAreaNum(Vec3(ent->s.origin));

    Vec3 testedOrigin(ent->s.origin);
    int areaNum = AAS_PointAreaNum(testedOrigin.Data());
    if (areaNum)
        return areaNum;

    return FindAASAreaNum(ent->r.absmin, ent->r.absmax);
}


