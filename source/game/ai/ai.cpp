#include "ai_local.h"
#include "ai.h"
#include "vec3.h"
#include <stdarg.h>

AiWeaponAimType WeaponAimType(int weapon)
{
    switch (weapon)
    {
        case WEAP_GUNBLADE:
            return AiWeaponAimType::PREDICTION_EXPLOSIVE;
        case WEAP_GRENADELAUNCHER:
            return AiWeaponAimType::DROP;
        case WEAP_ROCKETLAUNCHER:
            return AiWeaponAimType::PREDICTION_EXPLOSIVE;
        case WEAP_PLASMAGUN:
            return AiWeaponAimType::PREDICTION;
        default:
            return AiWeaponAimType::INSTANT_HIT;
    }
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

