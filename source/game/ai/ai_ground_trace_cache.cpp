#include "ai_shutdown_hooks_holder.h"
#include "ai_ground_trace_cache.h"
#include "static_vector.h"
#include "ai_local.h"

struct CachedTrace
{
    trace_t trace;
    int64_t computedAt;
    float depth;
};

AiGroundTraceCache::AiGroundTraceCache()
{
    data = G_Malloc(MAX_EDICTS * sizeof(CachedTrace));
    memset(data, 0, MAX_EDICTS * sizeof(CachedTrace));
}

AiGroundTraceCache::~AiGroundTraceCache()
{
    if (data)
        G_Free(data);
}

static StaticVector<AiGroundTraceCache, 1> instanceHolder;

AiGroundTraceCache* AiGroundTraceCache::Instance()
{
    if (instanceHolder.empty())
    {
        instanceHolder.emplace_back(AiGroundTraceCache());
        AiShutdownHooksHolder::Instance()->RegisterHook([&]{ instanceHolder.clear(); });
    }
    return &instanceHolder.front();
}

void AiGroundTraceCache::GetGroundTrace(const edict_s *ent, float depth, trace_t *trace, uint64_t maxMillisAgo)
{
    edict_t *entRef = const_cast<edict_t *>(ent);
    CachedTrace *cachedTrace = (CachedTrace *)data + ENTNUM(entRef);

    if (cachedTrace->computedAt + maxMillisAgo >= level.time)
    {
        if (cachedTrace->depth >= depth)
        {
            trace->startsolid = cachedTrace->trace.startsolid;
            if (cachedTrace->trace.fraction == 1.0f)
            {
                trace->fraction = 1.0f;
                return;
            }
            float cachedHitDepth = cachedTrace->depth * cachedTrace->trace.fraction;
            if (cachedHitDepth > depth)
            {
                trace->fraction = 1.0f;
                return;
            }
            // Copy trace data
            *trace = cachedTrace->trace;
            // Recalculate result fraction
            trace->fraction = cachedHitDepth / depth;
            return;
        }
    }

    vec3_t end = { ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - depth };
    G_Trace(&cachedTrace->trace, entRef->s.origin, nullptr, nullptr, end, entRef, MASK_AISOLID);
    // Copy trace data
    *trace = cachedTrace->trace;
    cachedTrace->depth = depth;
    cachedTrace->computedAt = level.time;
    return;
}

// Uses the same algorithm as GetGroundTrace() but avoids trace result copying and thus a is a bit faster.
bool AiGroundTraceCache::TryDropToFloor(const struct edict_s *ent, float depth, vec3_t result, uint64_t maxMillisAgo)
{
    edict_t *entRef = const_cast<edict_t *>(ent);
    CachedTrace *cachedTrace = (CachedTrace *)data + ENTNUM(entRef);

    VectorCopy(ent->s.origin, result);

    if (cachedTrace->computedAt + maxMillisAgo >= level.time)
    {
        if (cachedTrace->depth >= depth)
        {
            if (cachedTrace->trace.fraction == 1.0f)
                return false;
            float cachedHitDepth = cachedTrace->depth * cachedTrace->trace.fraction;
            if (cachedHitDepth > depth)
                return false;

            VectorCopy(cachedTrace->trace.endpos, result);
            result[2] += 16.0f; // Add some delta
            return true;
        }
    }

    vec3_t end = { ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - depth };
    G_Trace(&cachedTrace->trace, entRef->s.origin, nullptr, nullptr, end, entRef, MASK_AISOLID);
    cachedTrace->depth = depth;
    cachedTrace->computedAt = level.time;
    if (cachedTrace->trace.fraction == 1.0f)
        return false;

    VectorCopy(cachedTrace->trace.endpos, result);
    result[2] += 16.0f;
    return true;
}
