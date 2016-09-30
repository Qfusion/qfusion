#include "ai_local.h"
#include "ai.h"
#include "ai_aas_route_cache.h"
#include "ai_objective_based_team_brain.h"
#include "bot.h"
#include "tactical_spots_detector.h"
#include "../g_as_local.h"

static const asEnumVal_t asNavEntityFlagsEnumVals[] =
{
    ASLIB_ENUM_VAL( AI_NAV_REACH_AT_TOUCH ),
    ASLIB_ENUM_VAL( AI_NAV_REACH_AT_RADIUS ),
    ASLIB_ENUM_VAL( AI_NAV_REACH_ON_EVENT ),
    ASLIB_ENUM_VAL( AI_NAV_REACH_IN_GROUP ),
    ASLIB_ENUM_VAL( AI_NAV_DROPPED ),
    ASLIB_ENUM_VAL( AI_NAV_MOVABLE ),
    ASLIB_ENUM_VAL( AI_NAV_NOTIFY_SCRIPT ),

    ASLIB_ENUM_VAL_NULL
};

static const asEnumVal_t asWeaponAimTypeEnumVals[] =
{
    ASLIB_ENUM_VAL( AI_WEAPON_AIM_TYPE_INSTANT_HIT ),
    ASLIB_ENUM_VAL( AI_WEAPON_AIM_TYPE_PREDICTION ),
    ASLIB_ENUM_VAL( AI_WEAPON_AIM_TYPE_PREDICTION_EXPLOSIVE ),
    ASLIB_ENUM_VAL( AI_WEAPON_AIM_TYPE_DROP ),

    ASLIB_ENUM_VAL_NULL
};

const asEnum_t asAIEnums[] =
{
    { "nav_entity_flags_e", asNavEntityFlagsEnumVals },
    { "weapon_aim_type_e", asWeaponAimTypeEnumVals },

    ASLIB_ENUM_VAL_NULL
};

static const asFuncdef_t asAiScriptWeaponDef_Funcdefs[] =
{
    ASLIB_FUNCDEF_NULL
};

static const asBehavior_t asAiScriptWeaponDef_ObjectBehaviors[] =
{
    ASLIB_BEHAVIOR_NULL
};

static const asMethod_t asAiScriptWeaponDef_ObjectMethods[] =
{
    ASLIB_METHOD_NULL
};

static const asProperty_t asAiScriptWeaponDef_Properties[] =
{
    { ASLIB_PROPERTY_DECL(int, weaponNum), ASLIB_FOFFSET(AiScriptWeaponDef, weaponNum) },
    { ASLIB_PROPERTY_DECL(int, tier), ASLIB_FOFFSET(AiScriptWeaponDef, tier) },
    { ASLIB_PROPERTY_DECL(float, minRange), ASLIB_FOFFSET(AiScriptWeaponDef, minRange) },
    { ASLIB_PROPERTY_DECL(float, maxRange), ASLIB_FOFFSET(AiScriptWeaponDef, maxRange) },
    { ASLIB_PROPERTY_DECL(float, bestRange), ASLIB_FOFFSET(AiScriptWeaponDef, bestRange) },
    { ASLIB_PROPERTY_DECL(float, projectileSpeed), ASLIB_FOFFSET(AiScriptWeaponDef, projectileSpeed) },
    { ASLIB_PROPERTY_DECL(float, splashRadius), ASLIB_FOFFSET(AiScriptWeaponDef, splashRadius) },
    { ASLIB_PROPERTY_DECL(float, maxSelfDamage), ASLIB_FOFFSET(AiScriptWeaponDef, maxSelfDamage) },
    { ASLIB_PROPERTY_DECL(weapon_aim_type_e, aimType), ASLIB_FOFFSET(AiScriptWeaponDef, aimType) },
    { ASLIB_PROPERTY_DECL(bool, isContinuousFire), ASLIB_FOFFSET(AiScriptWeaponDef, isContinuousFire) },

    ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asAiScriptWeaponDefClassDescriptor =
{
    "AIScriptWeaponDef",		         /* name */
    asOBJ_VALUE|asOBJ_POD,	             /* object type flags */
    sizeof(AiScriptWeaponDef),	         /* size */
    asAiScriptWeaponDef_Funcdefs,		 /* funcdefs */
    asAiScriptWeaponDef_ObjectBehaviors, /* object behaviors */
    asAiScriptWeaponDef_ObjectMethods,	 /* methods */
    asAiScriptWeaponDef_Properties,		 /* properties */

    NULL, NULL					         /* string factory hack */
};

static const asFuncdef_t asAiDefenceSpot_Funcdefs[] =
{
    ASLIB_FUNCDEF_NULL
};

static constexpr auto DEFAULT_MAX_DEFENDERS = 5;
static constexpr auto DEFAULT_MAX_ATTACKERS = 5;

static void objectAiDefenceSpot_Constructor( AiDefenceSpot *spot, int id, const edict_t *entity, float radius )
{
    spot->id = id;
    spot->entity = entity;
    spot->radius = radius;
    spot->usesAutoAlert = true;
    spot->minDefenders = 1;
    spot->maxDefenders = DEFAULT_MAX_DEFENDERS;
    spot->regularEnemyAlertScale = 1.0f;
    spot->carrierEnemyAlertScale = 1.0f;
}

static const asBehavior_t asAiDefenceSpot_ObjectBehaviors[] =
{
    { asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL(void, f, (int id, const Entity @entity, float radius)), asFUNCTION(objectAiDefenceSpot_Constructor), asCALL_CDECL_OBJFIRST },

    ASLIB_BEHAVIOR_NULL
};

static const asMethod_t asAiDefenceSpot_Methods[] =
{
    ASLIB_METHOD_NULL
};

static const asProperty_t asAiDefenceSpot_Properties[] =
{
    { ASLIB_PROPERTY_DECL(int, id), ASLIB_FOFFSET(AiDefenceSpot, id) },
    { ASLIB_PROPERTY_DECL(const Entity @, entity), ASLIB_FOFFSET(AiDefenceSpot, entity) },
    { ASLIB_PROPERTY_DECL(float, radius), ASLIB_FOFFSET(AiDefenceSpot, radius) },
    { ASLIB_PROPERTY_DECL(bool, usesAutoAlert), ASLIB_FOFFSET(AiDefenceSpot, usesAutoAlert) },
    { ASLIB_PROPERTY_DECL(uint, minDefenders), ASLIB_FOFFSET(AiDefenceSpot, minDefenders) },
    { ASLIB_PROPERTY_DECL(uint, maxDefenders), ASLIB_FOFFSET(AiDefenceSpot, maxDefenders) },
    { ASLIB_PROPERTY_DECL(float, regularEnemyAlertScale), ASLIB_FOFFSET(AiDefenceSpot, regularEnemyAlertScale) },
    { ASLIB_PROPERTY_DECL(float, carrierEnemyAlertScale), ASLIB_FOFFSET(AiDefenceSpot, carrierEnemyAlertScale) },

    ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asAiDefenceSpotClassDescriptor =
{
    "AIDefenceSpot",
    asOBJ_VALUE|asOBJ_POD,
    sizeof(AiDefenceSpot),
    asAiDefenceSpot_Funcdefs,
    asAiDefenceSpot_ObjectBehaviors,
    asAiDefenceSpot_Methods,
    asAiDefenceSpot_Properties,

    NULL, NULL
};

static const asFuncdef_t asAiOffenseSpot_Funcdefs[] =
{
    ASLIB_FUNCDEF_NULL
};

static void objectAiOffenseSpot_Constructor( AiOffenseSpot *spot, int id, const edict_t *entity )
{
    spot->id = id;
    spot->entity = entity;
    spot->minAttackers = 1;
    spot->maxAttackers = DEFAULT_MAX_ATTACKERS;
}

static const asBehavior_t asAiOffenseSpot_ObjectBehaviors[] =
{
    { asBEHAVE_CONSTRUCT, ASLIB_FUNCTION_DECL(void, f, (int id, const Entity @entity)), asFUNCTION(objectAiOffenseSpot_Constructor), asCALL_CDECL_OBJFIRST },

    ASLIB_BEHAVIOR_NULL
};

static const asMethod_t asAiOffenseSpot_Methods[] =
{
    ASLIB_METHOD_NULL
};

static const asProperty_t asAiOffenseSpot_Properties[] =
{
    { ASLIB_PROPERTY_DECL(int, id), ASLIB_FOFFSET(AiOffenseSpot, id) },
    { ASLIB_PROPERTY_DECL(const Entity @, entity), ASLIB_FOFFSET(AiOffenseSpot, entity) },
    { ASLIB_PROPERTY_DECL(uint, minAttackers), ASLIB_FOFFSET(AiOffenseSpot, minAttackers) },
    { ASLIB_PROPERTY_DECL(uint, maxAttackers), ASLIB_FOFFSET(AiOffenseSpot, maxAttackers) },

    ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asAiOffenseSpotClassDescriptor =
{
    "AIOffenseSpot",
    asOBJ_VALUE|asOBJ_POD,
    sizeof(AiOffenseSpot),
    asAiOffenseSpot_Funcdefs,
    asAiOffenseSpot_ObjectBehaviors,
    asAiOffenseSpot_Methods,
    asAiOffenseSpot_Properties,

    NULL, NULL
};

static const asFuncdef_t asbot_Funcdefs[] =
{
    ASLIB_FUNCDEF_NULL
};

static const asBehavior_t asbot_ObjectBehaviors[] =
{
    ASLIB_BEHAVIOR_NULL
};

float AI_GetBotEffectiveOffensiveness(const ai_handle_t *ai)
{
    return ai ? ai->botRef->GetEffectiveOffensiveness() : 0.0f;
}

void AI_SetBotBaseOffensiveness(ai_handle_t *ai, float baseOffensiveness)
{
    if (ai)
        ai->botRef->SetBaseOffensiveness(baseOffensiveness);
}

void AI_SetBotAttitude(ai_handle_t *ai, edict_t *ent, int attitude)
{
    if (ai && ent)
        ai->botRef->SetAttitude(ent, attitude);
}

void AI_ClearBotOverriddenEntityWeights(ai_handle_t *ai)
{
    if (ai)
        ai->botRef->ClearOverriddenEntityWeights();
}

void AI_OverrideBotEntityWeight(ai_handle_t *ai, edict_t *ent, float weight)
{
    if (ai && ent)
        ai->botRef->OverrideEntityWeight(ent, weight);
}

int AI_BotDefenceSpotId(const ai_handle_t *ai)
{
    return ai && ai->botRef ? ai->botRef->DefenceSpotId() : -1;
}

int AI_BotOffenseSpotId(const ai_handle_t *ai)
{
    return ai && ai->botRef ? ai->botRef->OffenseSpotId() : -1;
}

static const asMethod_t asbot_Methods[] =
{
    { ASLIB_FUNCTION_DECL(float, getEffectiveOffensiveness, () const), asFUNCTION(AI_GetBotEffectiveOffensiveness), asCALL_CDECL_OBJFIRST },
    { ASLIB_FUNCTION_DECL(void, setBaseOffensiveness, (float baseOffensiveness)), asFUNCTION(AI_SetBotBaseOffensiveness), asCALL_CDECL_OBJFIRST },

    { ASLIB_FUNCTION_DECL(void, setAttitude, (Entity @ent, int attitude)), asFUNCTION(AI_SetBotAttitude), asCALL_CDECL_OBJFIRST },

    { ASLIB_FUNCTION_DECL(void, clearOverriddenEntityWeights, ()), asFUNCTION(AI_ClearBotOverriddenEntityWeights), asCALL_CDECL_OBJFIRST },
    { ASLIB_FUNCTION_DECL(void, overrideEntityWeight, (Entity @ent, float weight)), asFUNCTION(AI_OverrideBotEntityWeight), asCALL_CDECL_OBJFIRST },

    { ASLIB_FUNCTION_DECL(int, get_defenceSpotId, () const), asFUNCTION(AI_BotDefenceSpotId), asCALL_CDECL_OBJFIRST },
    { ASLIB_FUNCTION_DECL(int, get_offenseSpotId, () const), asFUNCTION(AI_BotOffenseSpotId), asCALL_CDECL_OBJFIRST },

    ASLIB_METHOD_NULL
};

static const asProperty_t asbot_Properties[] =
{
    ASLIB_PROPERTY_NULL
};

static const asClassDescriptor_t asBotClassDescriptor =
{
    "Bot",						/* name */
    asOBJ_REF|asOBJ_NOCOUNT,	/* object type flags */
    sizeof(ai_handle_t),		/* size */
    asbot_Funcdefs,				/* funcdefs */
    asbot_ObjectBehaviors,		/* object behaviors */
    asbot_Methods,				/* methods */
    asbot_Properties,			/* properties */

    NULL, NULL					/* string factory hack */
};

const asClassDescriptor_t *asAIClassesDescriptors[] =
{
    &asAiScriptWeaponDefClassDescriptor,
    &asAiDefenceSpotClassDescriptor,
    &asAiOffenseSpotClassDescriptor,
    &asBotClassDescriptor,

    NULL
};

static inline AiObjectiveBasedTeamBrain *GetObjectiveBasedTeamBrain(const char *caller, int team)
{
    // Make sure that AiBaseTeamBrain::GetBrainForTeam() will not crash for illegal team
    if (team != TEAM_ALPHA && team != TEAM_BETA)
    {
        G_Printf(S_COLOR_RED "%s: illegal team %d\n", caller, team);
        return nullptr;
    }

    AiBaseTeamBrain *baseTeamBrain = AiBaseTeamBrain::GetBrainForTeam(team);
    if (auto *objectiveBasedTeamBrain = dynamic_cast<AiObjectiveBasedTeamBrain*>(baseTeamBrain))
        return objectiveBasedTeamBrain;

    G_Printf(S_COLOR_RED "%s: can't be used in not objective based gametype\n", caller);
    return nullptr;
}

void AI_AddDefenceSpot( int team, const AiDefenceSpot *spot )
{
    if (auto *objectiveBasedTeamBrain = GetObjectiveBasedTeamBrain(__FUNCTION__, team))
        objectiveBasedTeamBrain->AddDefenceSpot(*spot);
}

void AI_RemoveDefenceSpot( int team, int id )
{
    if (auto *objectiveBasedTeamBrain = GetObjectiveBasedTeamBrain(__FUNCTION__, team))
        objectiveBasedTeamBrain->RemoveDefenceSpot(id);
}

void AI_DefenceSpotAlert( int team, int id, float alertLevel, unsigned timeoutPeriod )
{
    if (auto *objectiveBasedTeamBrain = GetObjectiveBasedTeamBrain(__FUNCTION__, team))
        objectiveBasedTeamBrain->SetDefenceSpotAlert(id, alertLevel, timeoutPeriod);
}

void AI_AddOffenseSpot( int team, const AiOffenseSpot *spot )
{
    if (auto *objectiveBasedTeamBrain = GetObjectiveBasedTeamBrain(__FUNCTION__, team))
        objectiveBasedTeamBrain->AddOffenseSpot(*spot);
}

void AI_RemoveOffenseSpot( int team, int id )
{
    if (auto *objectiveBasedTeamBrain = GetObjectiveBasedTeamBrain(__FUNCTION__, team))
        objectiveBasedTeamBrain->RemoveOffenseSpot(id);
}

static int AI_SuggestDefencePlantingSpots(const edict_t *defendedEntity, float searchRadius, vec3_t *spots, int maxSpots)
{
    TacticalSpotsDetector tacticalSpotsDetector;
    // A reachability to a spot from an entity will be checked.
    // Checking a reachability from a planting spot to an entity does not make sense.
    tacticalSpotsDetector.SetCheckToAndBackReachability(false);
    tacticalSpotsDetector.SetDistanceInfluence(0.9f);
    // This means weight never falls off and farther spots have greater weight.
    tacticalSpotsDetector.SetWeightFalloffDistanceRatio(1.0f);
    // Travel time should not affect it significantly (spots planting is a "background task" for bots).
    tacticalSpotsDetector.SetTravelTimeInfluence(0.2f);
    // Avoid planting near ledges (a bot may miss and drop an item from a ledge trying to plant it).
    tacticalSpotsDetector.SetLedgePenalty(10.0f);
    // Avoid planting near walls (enemies can destroy planted items easily using explosives).
    tacticalSpotsDetector.SetWallPenalty(5.0f);
    // Allow planting on ground and a bit below if there are no elevated areas
    tacticalSpotsDetector.SetMinHeightAdvantage(-64.0f);
    // Prefer elevated areas
    tacticalSpotsDetector.SetHeightInfluence(0.9f);
    // Prevent selection of spots that are too close to each other
    tacticalSpotsDetector.SetSpotProximityThreshold(128.0f);
    TacticalSpotsDetector::OriginParams originParams(defendedEntity, searchRadius, AiAasRouteCache::Shared());
    TacticalSpotsDetector::AdvantageProblemParams problemParams(defendedEntity);
    return tacticalSpotsDetector.FindPositionalAdvantageSpots(originParams, problemParams, spots, maxSpots);
}

static CScriptArrayInterface *asFunc_AI_SuggestDefencePlantingSpots(const edict_t *defendedEntity, float radius, int maxSpots)
{
    if (maxSpots > 8)
    {
        G_Printf(S_COLOR_YELLOW "AI_SuggestDefencePlantingSpots(): maxSpots value %d will be limited to 8\n", maxSpots);
        maxSpots = 8;
    }

    vec3_t spots[8];
    unsigned numSpots = (unsigned)AI_SuggestDefencePlantingSpots(defendedEntity, radius, spots, maxSpots);

    auto *ctx = angelExport->asGetActiveContext();
    auto *engine = ctx->GetEngine();
    auto *arrayObjectType = engine->GetObjectTypeById(engine->GetTypeIdByDecl("array<Vec3>"));

    CScriptArrayInterface *result = angelExport->asCreateArrayCpp( numSpots, arrayObjectType );
    for (unsigned i = 0; i < numSpots; ++i)
    {
        asvec3_t *dst = (asvec3_t *)result->At( i );
        VectorCopy(spots[i], dst->v);
    }
    return result;
}

const asglobfuncs_t asAIGlobFuncs[] =
{
    { "void AddNavEntity( Entity @ent, int flags )", asFUNCTION(AI_AddNavEntity), NULL },
    { "void RemoveNavEntity( Entity @ent )", asFUNCTION(AI_RemoveNavEntity), NULL },
    { "void NavEntityReached( Entity @ent )", asFUNCTION(AI_NavEntityReached), NULL },

    { "void AddDefenceSpot(int team, const AIDefenceSpot &in spot )", asFUNCTION(AI_AddDefenceSpot), NULL },
    { "void AddOffenseSpot(int team, const AIOffenseSpot &in spot )", asFUNCTION(AI_AddOffenseSpot), NULL },
    { "void RemoveDefenceSpot(int team, int id)", asFUNCTION(AI_RemoveDefenceSpot), NULL },
    { "void RemoveOffenseSpot(int team, int id)", asFUNCTION(AI_RemoveOffenseSpot), NULL },

    { "void DefenceSpotAlert(int team, int id, float level, uint timeoutPeriod)", asFUNCTION(AI_DefenceSpotAlert), NULL },

    { "array<Vec3> @SuggestDefencePlantingSpots(Entity @defendedEntity, float radius, int maxSpots)", asFUNCTION(asFunc_AI_SuggestDefencePlantingSpots), NULL },

    { NULL }
};

// Forward declarations of ASFunctionsRegistry methods result types
template <typename R> struct ASFunction0;
template <typename R, typename T1> struct ASFunction1;
template <typename R, typename T1, typename T2> struct ASFunction2;
template <typename R, typename T1, typename T2, typename T3> struct ASFunction3;

class ASFunctionsRegistry
{
    friend struct ASUntypedFunction;

    StaticVector<struct ASUntypedFunction *, 32> functions;

    inline void Register(class ASUntypedFunction &function)
    {
        functions.push_back(&function);
    }
public:
    void Load(asIScriptModule *module);
    void Unload();

    template <typename R>
    inline ASFunction0<R> Function0(const char *name, R defaultResult);

    template <typename R, typename T1>
    inline ASFunction1<R, T1> Function1(const char *name, R defaultResult);

    template <typename R, typename T1, typename T2>
    inline ASFunction2<R, T1, T2> Function2(const char *name, R defaultResult);

    template <typename R, typename T1, typename T2, typename T3>
    inline ASFunction3<R, T1, T2, T3> Function3(const char *name, R defaultResult);
};

// This class is a common non-template supertype for concrete types.
// We can't store templates (and pointers to these templates) with different parameters
// in a single array in ASFunctionsRegistry due to C++ template invariance.
struct ASUntypedFunction
{
private:
    const char *const decl;
    asIScriptFunction *funcPtr;
protected:
    inline asIScriptContext *PrepareContext()
    {
        if (!funcPtr || !angelExport)
            return nullptr;

        asIScriptContext *ctx = angelExport->asAcquireContext(GAME_AS_ENGINE());
        int error = ctx->Prepare(funcPtr);
        if (error < 0)
            return nullptr;

        return ctx;
    }

    inline asIScriptContext *CallForContext(asIScriptContext *preparedContext)
    {
        int error = preparedContext->Execute();
        // Put likely case first
        if (!G_ExecutionErrorReport(error))
            return preparedContext;

        GT_asShutdownScript();
        return nullptr;
    }
public:
    inline ASUntypedFunction(const char *decl_, ASFunctionsRegistry &registry): decl(decl_)
    {
        registry.Register(*this);
    }

    void Load(asIScriptModule *module)
    {
        funcPtr = module->GetFunctionByDecl(decl);
        if (!funcPtr)
        {
            if (developer->integer || sv_cheats->integer)
                G_Printf("* The function '%s' was not present in the script.\n", decl);
        }
    }

    inline void Unload() { funcPtr = nullptr; }

    inline bool IsLoaded() const { return funcPtr != nullptr; }
};

void ASFunctionsRegistry::Load(asIScriptModule *module)
{
    for (auto *func: functions)
        func->Load(module);
}

void ASFunctionsRegistry::Unload()
{
    for (auto *func: functions)
        func->Unload();
}

struct Void
{
    static Void VALUE;
};

Void Void::VALUE;

template <typename R> struct ResultGetter
{
    static inline R Get(asIScriptContext *ctx) = delete;
};

template <typename R> struct ResultGetter<const R&>
{
    static inline const R &Get(asIScriptContext *ctx) { return *((R *)ctx->GetReturnObject()); }
};

template <typename R> struct ResultGetter<const R*>
{
    static inline const R *Get(asIScriptContext *ctx) { return (R *)ctx->GetReturnObject(); }
};

template <> struct ResultGetter<bool>
{
    static inline bool Get(asIScriptContext *ctx) { return ctx->GetReturnByte() != 0; }
};

template <> struct ResultGetter<int>
{
    static inline int Get(asIScriptContext *ctx) { return ctx->GetReturnDWord(); }
};

template<> struct ResultGetter<float>
{
    static inline float Get(asIScriptContext *ctx) { return ctx->GetReturnFloat(); }
};

template<> struct ResultGetter<Void>
{
    static inline Void Get(asIScriptContext *ctx) { return Void::VALUE; }
};

template<typename R>
struct ASTypedResultFunction: public ASUntypedFunction
{
    R defaultResult;

    ASTypedResultFunction(const char *decl_, R defaultResult_, ASFunctionsRegistry &registry)
        : ASUntypedFunction(decl_, registry), defaultResult(defaultResult_) {}

protected:
    inline R CallForResult(asIScriptContext *preparedContext)
    {
        if (auto ctx = CallForContext(preparedContext))
            return ResultGetter<R>::Get(ctx);
        return defaultResult;
    }
};

template <typename Arg> struct ArgSetter
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, Arg arg) = delete;
};

template <typename Arg> struct ArgSetter<const Arg &>
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, const Arg &arg)
    {
        ctx->SetArgObject(argNum, const_cast<Arg *>(&arg));
    }
};

template <typename Arg> struct ArgSetter<const Arg *>
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, const Arg *arg)
    {
        ctx->SetArgObject(argNum, const_cast<Arg *>(arg));
    }
};

template <typename Arg> struct ArgSetter<Arg *>
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, Arg *arg)
    {
        ctx->SetArgObject(argNum, arg);
    }
};

template<> struct ArgSetter<bool>
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, bool arg)
    {
        ctx->SetArgByte(argNum, arg ? 1 : 0);
    }
};

template<> struct ArgSetter<int>
{
    static inline void Set(asIScriptContext *ctx, unsigned argNum, int arg)
    {
        ctx->SetArgDWord(argNum, arg);
    }
};

template<typename R>
struct ASFunction0: public ASTypedResultFunction<R>
{
    ASFunction0(const char *decl_, R defaultResult_, ASFunctionsRegistry &registry)
        : ASTypedResultFunction<R>(decl_, defaultResult_, registry) {}

    R operator()()
    {
        if (auto preparedContext = this->PrepareContext())
        {
            return this->CallForResult(preparedContext);
        }
        return this->defaultResult;
    }
};

template<typename R, typename T1>
struct ASFunction1: public ASTypedResultFunction<R>
{
    ASFunction1(const char *decl_, R defaultResult_, ASFunctionsRegistry &registry)
        : ASTypedResultFunction<R>(decl_, defaultResult_, registry) {}

    R operator()(T1 arg1)
    {
        if (auto preparedContext = this->PrepareContext())
        {
            ArgSetter<T1>::Set(preparedContext, 0, arg1);
            return this->CallForResult(preparedContext);
        }
        return this->defaultResult;
    }
};

template<typename R, typename T1, typename T2>
struct ASFunction2: public ASTypedResultFunction<R>
{
    ASFunction2(const char *decl_, R defaultResult_, ASFunctionsRegistry &registry)
        : ASTypedResultFunction<R>(decl_, defaultResult_, registry) {}

    R operator()(T1 arg1, T2 arg2)
    {
        if (auto preparedContext = this->PrepareContext())
        {
            ArgSetter<T1>::Set(preparedContext, 0, arg1);
            ArgSetter<T2>::Set(preparedContext, 1, arg2);
            return this->CallForResult(preparedContext);
        }
        return this->defaultResult;
    }
};

template<typename R, typename T1, typename T2, typename T3>
struct ASFunction3: public ASTypedResultFunction<R>
{
    ASFunction3(const char *decl_, R defaultResult_, ASFunctionsRegistry &registry)
        : ASTypedResultFunction<R>(decl_, defaultResult_, registry) {}

    R operator()(T1 arg1, T2 arg2, T3 arg3)
    {
        if (auto preparedContext = this->PrepareContext())
        {
            ArgSetter<T1>::Set(preparedContext, 0, arg1);
            ArgSetter<T2>::Set(preparedContext, 1, arg2);
            ArgSetter<T3>::Set(preparedContext, 2, arg3);
            return this->CallForResult(preparedContext);
        }
        return this->defaultResult;
    }
};

template <typename R>
ASFunction0<R> ASFunctionsRegistry::Function0(const char *decl, R defaultResult)
{
    return ASFunction0<R>(decl, defaultResult, *this);
}

template <typename R, typename T1>
ASFunction1<R, T1> ASFunctionsRegistry::Function1(const char *decl, R defaultResult)
{
    return ASFunction1<R, T1>(decl, defaultResult, *this);
};

template <typename R, typename T1, typename T2>
ASFunction2<R, T1, T2> ASFunctionsRegistry::Function2(const char *decl, R defaultResult)
{
    return ASFunction2<R, T1, T2>(decl, defaultResult, *this);
};

template <typename R, typename T1, typename T2, typename T3>
ASFunction3<R, T1, T2, T3> ASFunctionsRegistry::Function3(const char *decl, R defaultResult)
{
    return ASFunction3<R, T1, T2, T3>(decl, defaultResult, *this);
};

static ASFunctionsRegistry gtAIFunctionsRegistry;

void AI_InitGametypeScript(asIScriptModule *module)
{
    gtAIFunctionsRegistry.Load(module);
}

void AI_ResetGametypeScript()
{
    gtAIFunctionsRegistry.Unload();
}

static auto botWouldDropHealthFunc =
    gtAIFunctionsRegistry.Function1<bool, const gclient_t*>("bool GT_BotWouldDropHealth( const Client @client )", false);

bool GT_asBotWouldDropHealth(const gclient_t *client)
{
    return botWouldDropHealthFunc(client);
}

static auto botDropHealthFunc =
    gtAIFunctionsRegistry.Function1<Void, gclient_t*>("void GT_BotDropHealth( Client @client )", Void::VALUE);

void GT_asBotDropHealth( gclient_t *client )
{
    botDropHealthFunc(client);
}

static auto botWouldDropArmorFunc =
    gtAIFunctionsRegistry.Function1<bool, const gclient_t*>("bool GT_BotWouldDropArmor( const Client @client )", false);

bool GT_asBotWouldDropArmor( const gclient_t *client )
{
    return botWouldDropArmorFunc(client);
}

static auto botDropArmorFunc =
    gtAIFunctionsRegistry.Function1<Void, gclient_t*>("void GT_BotDropArmor( Client @client )", Void::VALUE);

void GT_asBotDropArmor( gclient_t *client )
{
    botDropArmorFunc(client);
}

static auto botWouldCloakFunc =
    gtAIFunctionsRegistry.Function1<bool, const gclient_t*>("bool GT_BotWouldCloak( const Client @client )", false);

bool GT_asBotWouldCloak( const gclient_t *client )
{
    return botWouldCloakFunc(client);
}

static auto setBotCloakEnabledFunc =
    gtAIFunctionsRegistry.Function2<Void, gclient_t*, bool>(
        "void GT_SetBotCloakEnabled( Client @client, bool enabled )", Void::VALUE);

void GT_asSetBotCloakEnabled(gclient_t *client, bool enabled)
{
    setBotCloakEnabledFunc(client, enabled);
}

static auto isEntityCloakingFunc =
    gtAIFunctionsRegistry.Function1<bool, const edict_t*>("bool GT_IsEntityCloaking( const Entity @ent )", false);

bool GT_asIsEntityCloaking(const edict_t *ent)
{
    return isEntityCloakingFunc(ent);
}

static auto botTouchedGoalFunc =
    gtAIFunctionsRegistry.Function2<Void, const ai_handle_t *, const edict_t *>(
        "void GT_BotTouchedGoal( const Bot @bot, const Entity @goalEnt )", Void::VALUE);

void GT_asBotTouchedGoal(const ai_handle_t *bot, const edict_t *goalEnt)
{
    botTouchedGoalFunc(bot, goalEnt);
}

static auto botReachedGoalRadiusFunc =
    gtAIFunctionsRegistry.Function2<Void, const ai_handle_t *, const edict_t *>(
        "void GT_BotReachedGoalRadius( const Bot @bot, const Entity @goalEnt )", Void::VALUE);

void GT_asBotReachedGoalRadius(const ai_handle_t *bot, const edict_t *goalEnt)
{
    botReachedGoalRadiusFunc(bot, goalEnt);
}

static auto playerOffensiveAbilitiesRatingFunc =
    gtAIFunctionsRegistry.Function1<float, const gclient_t*>(
        "float GT_PlayerOffensiveAbilitiesRating( const Client @client )", 0.5f);

float GT_asPlayerOffensiveAbilitiesRating(const gclient_t *client)
{
    return playerOffensiveAbilitiesRatingFunc(client);
}

static auto playerDefenciveAbilitiesRatingFunc =
    gtAIFunctionsRegistry.Function1<float, const gclient_t*>(
        "float GT_PlayerDefenciveAbilitiesRating( const Client @client )", 0.5f);

float GT_asPlayerDefenciveAbilitiesRating(const gclient_t *client)
{
    return playerDefenciveAbilitiesRatingFunc(client);
}

static auto getScriptWeaponsNumFunc =
    gtAIFunctionsRegistry.Function1<int, const gclient_t*>(
        "int GT_GetScriptWeaponsNum( const Client @client )", 0);

int GT_asGetScriptWeaponsNum(const gclient_t *client)
{
    return getScriptWeaponsNumFunc(client);
}

static auto getScriptWeaponDefFunc =
    gtAIFunctionsRegistry.Function3<bool, const gclient_t*, int, AiScriptWeaponDef *>(
        "bool GT_GetScriptWeaponDef( const Client @client, int weaponNum, AIScriptWeaponDef &out weaponDef )", false);

bool GT_asGetScriptWeaponDef(const gclient_t *client, int weaponNum, AiScriptWeaponDef *weaponDef)
{
    return getScriptWeaponDefFunc(client, weaponNum, weaponDef);
}

static auto getScriptWeaponCooldownFunc =
    gtAIFunctionsRegistry.Function2<int, const gclient_t*, int>(
        "int GT_GetScriptWeaponCooldown( const Client @client, int weaponNum )", INT_MAX);

int GT_asGetScriptWeaponCooldown(const gclient_t *client, int weaponNum)
{
    return getScriptWeaponCooldownFunc(client, weaponNum);
}

static auto selectScriptWeaponFunc =
    gtAIFunctionsRegistry.Function2<bool, gclient_t*, int>(
        "bool GT_SelectScriptWeapon( Client @client, int weaponNum )", false);

bool GT_asSelectScriptWeapon(gclient_t *client, int weaponNum)
{
    return selectScriptWeaponFunc(client, weaponNum);
}

static auto fireScriptWeaponFunc =
    gtAIFunctionsRegistry.Function2<bool, gclient_t*, int>(
        "bool GT_FireScriptWeapon( Client @client, int weaponNum )", false);

bool GT_asFireScriptWeapon(gclient_t *client, int weaponNum)
{
    return fireScriptWeaponFunc(client, weaponNum);
}
