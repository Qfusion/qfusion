#include "bot.h"
#include "ai_shutdown_hooks_holder.h"
#include "ai_gametype_brain.h"
#include "ai_base_team_brain.h"
#include "aas.h"

ai_weapon_t AIWeapons[WEAP_TOTAL];
const size_t ai_handle_size = sizeof( ai_handle_t );

static bool ai_intialized = false;
static bool aas_system_initialized = false;

StaticVector<int, 16> hubAreas;

//==========================================
// AI_InitLevel
// Inits Map local parameters
//==========================================
void AI_InitLevel( void )
{
    aas_system_initialized = AI_InitAAS();
    if (!aas_system_initialized)
        G_Printf("Can't initialize AAS system\n");
    else if (!AI_LoadLevelAAS(level.mapname))
        G_Printf("Can't load AAS for level %s\n", level.mapname);

    // set up weapon usage weights

    memset( &AIWeapons, 0, sizeof( ai_weapon_t )*WEAP_TOTAL );

    //WEAP_GUNBLADE
    AIWeapons[WEAP_GUNBLADE].aimType = AI_AIMSTYLE_PREDICTION_EXPLOSIVE;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.2f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_SHORT_RANGE] = 0.3f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_MELEE_RANGE] = 0.4f;

    //WEAP_MACHINEGUN
    AIWeapons[WEAP_MACHINEGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.8f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.7f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.1f;

    //WEAP_RIOTGUN
    AIWeapons[WEAP_RIOTGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.8f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.5f;

    //ROCKETLAUNCHER
    AIWeapons[WEAP_ROCKETLAUNCHER].aimType = AI_AIMSTYLE_PREDICTION_EXPLOSIVE;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_LONG_RANGE] = 0.2f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_SHORT_RANGE] = 0.9f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_MELEE_RANGE] = 0.6f;

    //WEAP_GRENADELAUNCHER
    AIWeapons[WEAP_GRENADELAUNCHER].aimType = AI_AIMSTYLE_DROP;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_LONG_RANGE] = 0.0f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.1f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_MELEE_RANGE] = 0.3f;

    //WEAP_PLASMAGUN
    AIWeapons[WEAP_PLASMAGUN].aimType = AI_AIMSTYLE_PREDICTION;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.7f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.4f;

    //WEAP_ELECTROBOLT
    AIWeapons[WEAP_ELECTROBOLT].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_LONG_RANGE] = 0.9f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.7f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_MELEE_RANGE] = 0.3f;

    //WEAP_LASERGUN
    AIWeapons[WEAP_LASERGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.0f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.0f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.7f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.6f;

    //WEAP_INSTAGUN
    AIWeapons[WEAP_INSTAGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.9f;

    NavEntitiesRegistry::Instance()->Init();

    ai_intialized = true;
}

void AI_Shutdown( void )
{
    hubAreas.clear();

    if (!ai_intialized)
        return;

    AI_UnloadLevel();

    AiShutdownHooksHolder::Instance()->InvokeHooks();
    ai_intialized = false;
}

void AI_UnloadLevel()
{
    BOT_RemoveBot("all");
    if (aas_system_initialized)
    {
        AI_ShutdownAAS();
        aas_system_initialized = false;
    }
}

void AI_GametypeChanged(const char *gametype)
{
    AiGametypeBrain::OnGametypeChanged(gametype);
}

void AI_JoinedTeam(edict_t *ent, int team)
{
    AiGametypeBrain::Instance()->OnBotJoinedTeam(ent, team);
}

void AI_CommonFrame()
{
    AI_AASFrame();

    AiGametypeBrain::Instance()->Update();
}

static void FindHubAreas()
{
    if (!hubAreas.empty())
        return;

    // Select not more than hubAreas.capacity() grounded areas that have highest connectivity to other areas.

    struct AreaAndReachCount
    {
        int area, reachCount;
        AreaAndReachCount(int area, int reachCount): area(area), reachCount(reachCount) {}
        // Ensure that area with lowest reachCount will be evicted in pop_heap(), so use >
        bool operator<(const AreaAndReachCount &that) const { return reachCount > that.reachCount; }
    };

    StaticVector<AreaAndReachCount, hubAreas.capacity() + 1> bestAreasHeap;
    for (int i = 1; i < aasworld.numareas; ++i)
    {
        const auto &areaSettings = aasworld.areasettings[i];
        if (!(areaSettings.areaflags & AREA_GROUNDED))
            continue;
        if (areaSettings.areaflags & AREA_DISABLED)
            continue;
        if (areaSettings.contents & (AREACONTENTS_DONOTENTER|AREACONTENTS_LAVA|AREACONTENTS_SLIME|AREACONTENTS_WATER))
            continue;

        // Reject degenerate areas, pass only relatively large areas
        const auto &area = aasworld.areas[i];
        if (area.maxs[0] - area.mins[0] < 128.0f)
            continue;
        if (area.maxs[1] - area.mins[1] < 128.0f)
            continue;

        // Count as useful only several kinds of reachabilities
        int usefulReachCount = 0;
        int reachNum = areaSettings.firstreachablearea;
        int lastReachNum = areaSettings.firstreachablearea + areaSettings.numreachableareas - 1;
        while (reachNum <= lastReachNum)
        {
            const auto &reach = aasworld.reachability[reachNum];
            if (reach.traveltype == TRAVEL_WALK || reach.traveltype == TRAVEL_WALKOFFLEDGE)
                usefulReachCount++;
            ++reachNum;
        }

        // Reject early to avoid more expensive call to push_heap()
        if (!usefulReachCount)
            continue;

        bestAreasHeap.push_back(AreaAndReachCount(i, usefulReachCount));
        std::push_heap(bestAreasHeap.begin(), bestAreasHeap.end());

        // bestAreasHeap size should be always less than its capacity:
        // 1) to ensure that there is a free room for next area;
        // 2) to ensure that hubAreas capacity will not be exceeded.
        if (bestAreasHeap.size() == bestAreasHeap.capacity())
        {
            std::pop_heap(bestAreasHeap.begin(), bestAreasHeap.end());
            bestAreasHeap.pop_back();
        }
    }
    static_assert(bestAreasHeap.capacity() == hubAreas.capacity() + 1, "");
    for (const auto &areaAndReachCount: bestAreasHeap)
        hubAreas.push_back(areaAndReachCount.area);
}

static inline void ExtendDimension(float *mins, float *maxs, int dimension)
{
    float side = maxs[dimension] - mins[dimension];
    if (side < 48.0f)
    {
        maxs[dimension] += 0.5f * (48.0f - side);
        mins[dimension] -= 0.5f * (48.0f - side);
    }
}

static int FindGoalAASArea(edict_t *ent)
{
    Vec3 mins(ent->r.mins), maxs(ent->r.maxs);
    // Extend AABB XY dimensions
    ExtendDimension(mins.Data(), maxs.Data(), 0);
    ExtendDimension(mins.Data(), maxs.Data(), 1);
    // Z needs special extension rules
    float presentHeight = maxs.Z() - mins.Z();
    float playerHeight = playerbox_stand_maxs[2] - playerbox_stand_mins[2];
    if (playerHeight > presentHeight)
        maxs.Z() += playerHeight - presentHeight;

    // Find all areas in bounds
    int areas[16];
    // Convert bounds to absolute ones
    mins += ent->s.origin;
    maxs += ent->s.origin;
    const int numAreas = AAS_BBoxAreas(mins.Data(), maxs.Data(), areas, 16);

    // Find hub areas (or use cached)
    FindHubAreas();

    int bestArea = 0;
    int bestAreaReachCount = 0;
    for (int i = 0; i < numAreas; ++i)
    {
        const int areaNum = areas[i];
        int areaReachCount = 0;
        for (const int hubAreaNum: hubAreas)
        {
            const aas_area_t &hubArea = aasworld.areas[hubAreaNum];
            Vec3 hubAreaPoint(hubArea.center);
            hubAreaPoint.Z() = hubArea.mins[2] + std::min(24.0f, hubArea.maxs[2] - hubArea.mins[2]);
            // Do not waste pathfinder cycles testing for preferred flags that may fail.
            constexpr int travelFlags = Bot::ALLOWED_TRAVEL_FLAGS;
            if (AAS_AreaReachabilityToGoalArea(hubAreaNum, hubAreaPoint.Data(), areaNum, travelFlags))
            {
                areaReachCount++;
                // Thats't enough, do not waste CPU cycles
                if (areaReachCount == 4)
                    return areaNum;
            }
        }
        if (areaReachCount > bestAreaReachCount)
        {
            bestArea = areaNum;
            bestAreaReachCount = areaReachCount;
        }
    }
    if (bestArea)
        return bestArea;

    // Fall back to a default method and hope it succeeds
    return FindAASAreaNum(ent);
}

void AI_AddNavEntity(edict_t *ent, ai_nav_entity_flags flags)
{
    if (!flags)
    {
        G_Printf(S_COLOR_RED "AI_AddNavEntity(): flags are empty");
        return;
    }
    int onlyMutExFlags = flags & (AI_NAV_REACH_AT_TOUCH | AI_NAV_REACH_AT_RADIUS | AI_NAV_REACH_ON_EVENT);
    // Valid mutual exclusive flags give a power of two
    if (onlyMutExFlags & (onlyMutExFlags - 1))
    {
        G_Printf(S_COLOR_RED, "AI_AddNavEntity(): illegal flags %x for nav entity %s", flags, ent->classname);
        return;
    }

    NavEntityFlags navEntityFlags = NavEntityFlags::NONE;
    if (flags & AI_NAV_REACH_AT_TOUCH)
        navEntityFlags = navEntityFlags | NavEntityFlags::REACH_AT_TOUCH;
    if (flags & AI_NAV_REACH_AT_RADIUS)
        navEntityFlags = navEntityFlags | NavEntityFlags::REACH_AT_RADIUS;
    if (flags & AI_NAV_REACH_ON_EVENT)
        navEntityFlags = navEntityFlags | NavEntityFlags::REACH_ON_EVENT;
    if (flags & AI_NAV_REACH_IN_GROUP)
        navEntityFlags = navEntityFlags | NavEntityFlags::REACH_IN_GROUP;
    if (flags & AI_NAV_DROPPED)
        navEntityFlags = navEntityFlags | NavEntityFlags::DROPPED_ENTITY;

    int areaNum = FindGoalAASArea(ent);
    if (areaNum)
    {
        NavEntitiesRegistry::Instance()->AddNavEntity(ent, areaNum, navEntityFlags);
        return;
    }
    constexpr const char *format = S_COLOR_RED "AI_AddNavEntity(): Can't find an area num for %s @ %.3f %.3f %.3f\n";
    G_Printf(format, ent->classname, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
}

void AI_RemoveNavEntity(edict_t *ent)
{
    NavEntity *navEntity = NavEntitiesRegistry::Instance()->NavEntityForEntity(ent);
    // (An nav. item absence is not an error, this function is called for each entity in game)
    if (!navEntity)
        return;

    AiGametypeBrain::Instance()->ClearGoals(navEntity, nullptr);
    NavEntitiesRegistry::Instance()->RemoveNavEntity(navEntity);
}

void AI_NavEntityReached(edict_t *ent)
{
    AiGametypeBrain::Instance()->NavEntityReached(ent);
}

void AI_SetBotAttitude(ai_handle_t *ai, edict_t *ent, int attitude)
{
    if (ai && ent)
        ai->botRef->SetAttitude(ent, attitude);
}

void AI_ClearBotExternalEntityWeights(ai_handle_t *ai)
{
    if (ai)
        ai->botRef->ClearExternalEntityWeights();
}

void AI_SetBotExternalEntityWeight(ai_handle_t *ai, edict_t *ent, float weight)
{
    if (ai && ent)
        ai->botRef->SetExternalEntityWeight(ent, weight);
}

//==========================================
// G_FreeAI
// removes the AI handle from memory
//==========================================
void G_FreeAI( edict_t *ent )
{
    if (!ent->ai)
        return;

    // Invoke an appropriate destructor based on ai instance type, then free memory.
    // It is enough to call G_Free(ent->ai->aiRef), since botRef (if it is present)
    // points to the same block as aiRef does, but to avoid confusion we free pointer aliases explicitly.
    if (ent->ai->botRef)
    {
        AiGametypeBrain::Instance()->OnBotDropped(ent);
        ent->ai->botRef->~Bot();
        G_Free(ent->ai->botRef);
    }
    else
    {
        ent->ai->aiRef->~Ai();
        G_Free(ent->ai->aiRef);
    }
    ent->ai->aiRef = nullptr;
    ent->ai->botRef = nullptr;

    G_Free(ent->ai);
    ent->ai = nullptr;
}

//==========================================
// G_SpawnAI
// allocate ai_handle_t for this entity
//==========================================
void G_SpawnAI( edict_t *ent, float skillLevel )
{
    if (!ent->ai)
        ent->ai = ( ai_handle_t * )G_Malloc( sizeof( ai_handle_t ) );
    else
        memset( &ent->ai, 0, sizeof( ai_handle_t ) );

    if( ent->r.svflags & SVF_FAKECLIENT )
    {
        ent->ai->type = AI_ISBOT;
        void *mem = G_Malloc( sizeof(Bot) );
        ent->ai->botRef = new(mem) Bot( ent, skillLevel );
        ent->ai->aiRef = ent->ai->botRef;
    }
    else
    {
        // TODO: Monster brain is not implemented! This is just a stub!
        ent->ai->type = AI_ISMONSTER;
        void *mem = G_Malloc( sizeof(Ai) );
        ent->ai->botRef = nullptr;
        ent->ai->aiRef = new(mem) Ai( ent, TFL_DEFAULT, TFL_DEFAULT );
    }
}

//==========================================
// AI_GetType
//==========================================
ai_type AI_GetType( const ai_handle_t *ai )
{
    return ai ? ai->type : AI_INACTIVE;
}

void AI_TouchedEntity(edict_t *self, edict_t *ent)
{
    self->ai->aiRef->TouchedEntity(ent);
}

void AI_DamagedEntity(edict_t *self, edict_t *ent, int damage)
{
    if (self->ai->botRef)
        self->ai->botRef->OnEnemyDamaged(ent, damage);
}

void AI_Pain(edict_t *self, edict_t *attacker, int kick, int damage)
{
    if (self->ai->botRef)
        self->ai->botRef->Pain(attacker, kick, damage);
}

void AI_Think(edict_t *self)
{
    if( !self->ai || self->ai->type == AI_INACTIVE )
        return;

    self->ai->aiRef->Update();
}
