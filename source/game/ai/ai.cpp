#include "bot.h"
#include "aas.h"

#include <stdarg.h>

Ai::Ai(edict_t *self)
    : EdictRef(self),
      currAasAreaNum(0),
      goalAasAreaNum(0),
      goalAasAreaNodeFlags(0),
      goalTargetPoint(0, 0, 0),
      nextAasAreaNum(0),
      allowedAasTravelFlags(TFL_DEFAULT),
      preferredAasTravelFlags(TFL_DEFAULT),
      nextAreaReachNum(0),
      distanceToNextReachStart(std::numeric_limits<float>::infinity()),
      distanceToNextReachEnd(std::numeric_limits<float>::infinity()),
      statusUpdateTimeout(0),
      blockedTimeout(level.time + 15000),
      stateCombatTimeout(0),
      longRangeGoalTimeout(0),
      shortRangeGoalTimeout(0),
      aiYawSpeed(0.0f),
      aiPitchSpeed(0.0f)
{
    memset(_private, 0, sizeof(_private));
    nextAreaReach = (aas_reachability_t *) _private;

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