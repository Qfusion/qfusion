#ifndef QFUSION_AI_BASE_BRAIN_H
#define QFUSION_AI_BASE_BRAIN_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "ai_frame_aware_updatable.h"
#include "static_vector.h"
#include "ai_aas_route_cache.h"
#include "ai_base_ai.h"

float DamageToKill(float health, float armor, float armorProtection, float armorDegradation);

inline float DamageToKill(float health, float armor)
{
    return DamageToKill(health, armor, g_armor_protection->value, g_armor_degradation->value);
}

class WorldState
{
    friend class FloatVar;
    friend class BoolVar;
public:
    enum class SatisfyOp: unsigned char
    {
        EQ,
        NE,
        GT,
        GE,
        LS,
        LE
    };
private:
    // WorldState operations such as copying and testing for satisfaction must be fast,
    // so vars components are stored in separate arrays for tight data packing.
    // Var types visible for external code are just thin wrappers around pointers to these values.

    static constexpr auto NUM_UNSIGNED_VARS = 1;
    static constexpr auto NUM_FLOAT_VARS = 2;
    static constexpr auto NUM_SHORT_VARS = 3;
    static constexpr auto NUM_BOOL_VARS = 7;

    unsigned unsignedVarsValues[NUM_UNSIGNED_VARS];
    float floatVarsValues[NUM_FLOAT_VARS];
    short shortVarsValues[NUM_SHORT_VARS];
    bool boolVarsValues[NUM_BOOL_VARS];

    SatisfyOp unsignedVarsSatisfyOps[NUM_UNSIGNED_VARS];
    SatisfyOp floatVarsSatisfyOps[NUM_FLOAT_VARS];
    SatisfyOp shortVarsSatisfyOps[NUM_SHORT_VARS];

    bool unsignedVarsIgnoreFlags[NUM_UNSIGNED_VARS];
    bool floatVarsIgnoreFlags[NUM_FLOAT_VARS];
    bool shortVarsIgnoreFlags[NUM_SHORT_VARS];
    bool boolVarsIgnoreFlags[NUM_BOOL_VARS];
public:

#define DECLARE_VAR_BASE_CLASS(className, type, values, mask)                     \
    class className##Base                                                         \
    {                                                                             \
    protected:                                                                    \
        WorldState *parent;                                                       \
        short index;                                                              \
        className##Base(const WorldState *parent_, short index_)                  \
            : parent(const_cast<WorldState *>(parent_)), index(index_) {}         \
    public:                                                                       \
        inline const type &Value() const { return parent->values[index]; }        \
        inline className##Base &SetValue(type value)                              \
        {                                                                         \
            parent->values[index] = value; return *this;                          \
        }                                                                         \
        inline operator type() const { return parent->values[index]; }            \
        inline bool Ignore() const { return parent->mask[index]; }                \
        inline className##Base &SetIgnore(bool ignore)                            \
        {                                                                         \
            parent->mask[index] = ignore; return *this;                           \
        }                                                                         \
    }

#define DECLARE_VAR_CLASS_SATISFY_OPS(className, type, values, ops)  \
    inline WorldState::SatisfyOp SatisfyOp() const                   \
    {                                                                \
        return parent->ops[index];                                   \
    }                                                                \
    inline className &SetSatisfyOp(WorldState::SatisfyOp op)         \
    {                                                                \
        parent->ops[index] = op; return *this;                       \
    }                                                                \
    inline bool IsSatisfiedBy(type value) const                      \
    {                                                                \
        switch (parent->ops[index])                                  \
        {                                                            \
            case WorldState::SatisfyOp::EQ: return Value() == value; \
            case WorldState::SatisfyOp::NE: return Value() != value; \
            case WorldState::SatisfyOp::GT: return Value() > value;  \
            case WorldState::SatisfyOp::GE: return Value() >= value; \
            case WorldState::SatisfyOp::LS: return Value() < value;  \
            case WorldState::SatisfyOp::LE: return Value() <= value; \
        }                                                            \
    }

    DECLARE_VAR_BASE_CLASS(UnsignedVar, unsigned, unsignedVarsValues, unsignedVarsIgnoreFlags);

    class UnsignedVar: public UnsignedVarBase
    {
        friend class WorldState;
        UnsignedVar(const WorldState *parent_, short index_): UnsignedVarBase(parent_, index_) {}
    public:
        DECLARE_VAR_CLASS_SATISFY_OPS(UnsignedVar, unsigned, unsignedVarsValues, unsignedVarsSatisfyOps);
    };

    DECLARE_VAR_BASE_CLASS(FloatVar, float, floatVarsValues, floatVarsIgnoreFlags);

    class FloatVar: public FloatVarBase
    {
        friend class WorldState;
        FloatVar(const WorldState *parent_, short index_): FloatVarBase(parent_, index_) {}
    public:
        DECLARE_VAR_CLASS_SATISFY_OPS(FloatVar, float, floatVarsValues, floatVarsSatisfyOps)
    };

    DECLARE_VAR_BASE_CLASS(ShortVar, short, shortVarsValues, shortVarsIgnoreFlags);

    class ShortVar: public ShortVarBase
    {
        friend class WorldState;
        ShortVar(const WorldState *parent_, short index_): ShortVarBase(parent_, index_) {}
    public:
        DECLARE_VAR_CLASS_SATISFY_OPS(ShortVar, short, shortVarsValues, shortVarsSatisfyOps)
    };

    DECLARE_VAR_BASE_CLASS(BoolVar, bool, boolVarsValues, boolVarsIgnoreFlags);

    class BoolVar: public BoolVarBase
    {
        friend class WorldState;
        BoolVar(const WorldState *parent_, short index_): BoolVarBase(parent_, index_) {}

    public:
        inline bool IsSatisfiedBy(bool value) const
        {
            return Value() == value;
        }
    };

    bool IsSatisfiedBy(const WorldState &that) const;

    uint32_t Hash() const;
    bool operator==(const WorldState &that) const;

    inline void SetIgnoreAll(bool ignore)
    {
        std::fill_n(unsignedVarsIgnoreFlags, NUM_UNSIGNED_VARS, ignore);
        std::fill_n(floatVarsIgnoreFlags, NUM_FLOAT_VARS, ignore);
        std::fill_n(shortVarsIgnoreFlags, NUM_SHORT_VARS, ignore);
        std::fill_n(boolVarsIgnoreFlags, NUM_BOOL_VARS, ignore);
    }

    inline FloatVar DistanceToEnemy() const { return FloatVar(this, 0); }
    inline FloatVar DistanceToGoalItemNavTarget() const { return FloatVar(this, 1); }

    inline UnsignedVar GoalItemWaitTime() const { return UnsignedVar(this, 0); }

    inline ShortVar Health() const { return ShortVar(this, 0); }
    inline ShortVar Armor() const { return ShortVar(this, 1); }
    inline ShortVar RawDamageToKill() const { return ShortVar(this, 2); }

    inline BoolVar HasQuad() const { return BoolVar(this, 0); }
    inline BoolVar HasShell() const { return BoolVar(this, 1); }
    inline BoolVar HasEnemy() const { return BoolVar(this, 2); }
    inline BoolVar EnemyHasQuad() const { return BoolVar(this, 3); }
    inline BoolVar HasThreateningEnemy() const { return BoolVar(this, 4); }
    inline BoolVar HasJustPickedGoalItem() const { return BoolVar(this, 5); }
    inline BoolVar HasGoalItemNavTarget() const { return BoolVar(this, 6); }

    constexpr static float FAR_RANGE_MAX = 2.5f * 900.0f;
    constexpr static float MIDDLE_RANGE_MAX = 900.0f;
    constexpr static float CLOSE_RANGE_MAX = 175.0f;

    inline bool EnemyIsOnSniperRange() const
    {
        return DistanceToEnemy() > FAR_RANGE_MAX;
    }
    inline bool EnemyIsOnFarRange() const
    {
        return DistanceToEnemy() > MIDDLE_RANGE_MAX && DistanceToEnemy() <= FAR_RANGE_MAX;
    }
    inline bool EnemyIsOnMiddleRange() const
    {
        return DistanceToEnemy() > CLOSE_RANGE_MAX && DistanceToEnemy() <= MIDDLE_RANGE_MAX;
    }
    inline bool EnemyIsOnCloseRange() const
    {
        return DistanceToEnemy() <= CLOSE_RANGE_MAX;
    }

    inline float DamageToBeKilled() const
    {
        float damageToBeKilled = ::DamageToKill(Health(), Armor());
        if (HasShell())
            damageToBeKilled *= 4.0f;
        if (EnemyHasQuad())
            damageToBeKilled /= 4.0f;
        return damageToBeKilled;
    }

    inline float DamageToKill() const
    {
        float damageToKill = RawDamageToKill();
        if (HasQuad())
            damageToKill /= 4.0f;
        return damageToKill;
    }

    inline float KillToBeKilledDamageRatio() const
    {
        return DamageToKill() / DamageToBeKilled();
    }
};

class AiBaseGoal
{
    friend class Ai;
    friend class AiBaseBrain;

    static inline void Register(Ai *ai, AiBaseGoal *goal);
protected:
    edict_t *self;
    const char *name;
    const unsigned updatePeriod;

    float weight;
public:
    // Don't pass self as a constructor argument (self->ai ptr might not been set yet)
    inline AiBaseGoal(Ai *ai, const char *name_, unsigned updatePeriod_)
        : self(ai->self), name(name_), updatePeriod(updatePeriod_), weight(0.0f)
    {
        Register(ai, this);
    }

    virtual ~AiBaseGoal() {};

    virtual void UpdateWeight(const WorldState &worldState) = 0;
    virtual void GetDesiredWorldState(WorldState *worldState) = 0;

    inline bool IsRelevant() const { return weight > 0; }

    // More important goals are first after sorting goals array
    inline bool operator<(const AiBaseGoal &that) const
    {
        return this->weight > that.weight;
    }

    inline const char *Name() const { return name; }
    inline unsigned UpdatePeriod() const { return updatePeriod; }
};

class alignas(8) PoolBase
{
    friend class PoolItem;

    char *basePtr;
    const char *tag;
    unsigned itemSize;

    static constexpr auto FREE_LIST = 0;
    static constexpr auto USED_LIST = 1;

    short listFirst[2];

#ifdef _DEBUG
    inline const char *ListName(short index)
    {
        switch (index)
        {
            case FREE_LIST: return "FREE";
            case USED_LIST: return "USED";
            default: abort();
        }
    }
#endif

    inline class PoolItem &ItemAt(short index)
    {
        return *(PoolItem *)(basePtr + itemSize * index);
    }
    inline short IndexOf(const class PoolItem *item) const
    {
        return (short)(((const char *)item - basePtr) / itemSize);
    }

    inline void Link(short itemIndex, short listIndex);
    inline void Unlink(short itemIndex, short listIndex);

protected:
    void *Alloc();
    void Free(class PoolItem *poolItem);

#ifndef _MSC_VER
    inline void Debug(const char *format, ...) const __attribute__((format(printf, 2, 3)))
#else
    inline void Debug(_Printf_format_string_ const char *format, ...) const
#endif
    {
        va_list va;
        va_start(va, format);
        AI_Debugv(tag, format, va);
        va_end(va);
    }
public:
    PoolBase(char *basePtr_, const char *tag_, unsigned itemSize_, unsigned itemsCount);

    void Clear();
};

class alignas(8) PoolItem
{
    friend class PoolBase;
    PoolBase *pool;
    short prevInList;
    short nextInList;
public:
    PoolItem(PoolBase *pool_): pool(pool_) {}
    virtual ~PoolItem() {}

    inline void DeleteSelf()
    {
        this->~PoolItem();
        pool->Free(this);
    }
};

template<class Item, unsigned N> class alignas(8) Pool: public PoolBase
{
    static constexpr unsigned ChunkSize()
    {
        return (sizeof(Item) % 8) ? sizeof(Item) + 8 - (sizeof(Item) % 8) : sizeof(Item);
    }

    alignas(8) char buffer[N * ChunkSize()];
public:
    Pool(const char *tag_): PoolBase(buffer, tag_, sizeof(Item), N) {}

    inline Item *New()
    {
        if (void *mem = Alloc())
            return new(mem) Item(this);
        return nullptr;
    }

    template <typename Arg1>
    inline Item *New(Arg1 arg1)
    {
        if (void *mem = Alloc())
            return new(mem) Item(this, arg1);
        return nullptr;
    }

    template <typename Arg1, typename Arg2>
    inline Item *New(Arg1 arg1, Arg2 arg2)
    {
        if (void *mem = Alloc())
            return new(mem) Item(this, arg1, arg2);
        return nullptr;
    };

    template <typename Arg1, typename Arg2, typename Arg3>
    inline Item *New(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        if (void *mem = Alloc())
            return new(mem) Item(this, arg1, arg2, arg3);
        return nullptr;
    };

    template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    inline Item *New(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        if (void *mem = Alloc())
            return new(mem) Item(this, arg1, arg2, arg3, arg4);
        return nullptr;
    };
};

class AiBaseActionRecord: public PoolItem
{
    friend class AiBaseAction;
protected:
    edict_t *self;
    const char *name;

#ifndef _MSC_VER
    inline void Debug(const char *format, ...) const __attribute__((format(printf, 2, 3)))
#else
    inline void Debug(_Printf_format_string_ const char *format, ...) const
#endif
    {
        va_list va;
        va_start(va, format);
        AI_Debugv(name, format, va);
        va_end(va);
    }
public:
    AiBaseActionRecord *nextInPlan;

    inline AiBaseActionRecord(PoolBase *pool_, edict_t *self_, const char *name_)
        : PoolItem(pool_), self(self_), name(name_), nextInPlan(nullptr) {}

    virtual ~AiBaseActionRecord() {}

    virtual void Activate()
    {
        Debug("About to activate\n");
    };
    virtual void Deactivate()
    {
        Debug("About to deactivate\n");
    };

    virtual const char *Name() const { return name; }

    enum Status
    {
        INVALID,
        VALID,
        COMPLETED
    };

    virtual Status CheckStatus(const WorldState &currWorldState) const = 0;
};

struct PlannerNode: PoolItem
{
    // World state after applying an action
    WorldState worldState;
    // An action record to apply
    AiBaseActionRecord *actionRecord;
    // Used to reconstruct a plan
    PlannerNode *parent;
    // Next in linked list of transitions for current node
    PlannerNode *nextTransition;

    // AStar edge "distance"
    float transitionCost;
    // AStar node G
    float costSoFar;
    // Priority queue parameter
    float heapCost;
    // Utility for retrieval an actual index in heap array by a node value
    unsigned heapArrayIndex;

    // Utilities for storing the node in a hash set
    PlannerNode *prevInHashBin;
    PlannerNode *nextInHashBin;
    uint32_t worldStateHash;

    inline PlannerNode(PoolBase *pool): PoolItem(pool) {}

    ~PlannerNode() override
    {
        if (actionRecord)
            actionRecord->DeleteSelf();

        // Prevent use-after-free
        actionRecord = nullptr;
        parent = nullptr;
        nextTransition = nullptr;
        prevInHashBin = nullptr;
        nextInHashBin = nullptr;
    }
};

class AiBaseAction
{
    friend class Ai;
    friend class AiBaseBrain;

    static inline void Register(Ai *ai, AiBaseAction *action);
protected:
    edict_t *self;
    const char *name;

#ifndef _MSC_VER
    inline void Debug(const char *format, ...) const __attribute__((format(printf, 2, 3)))
#else
    inline void Debug(_Printf_format_string_ const char *format, ...) const
#endif
    {
        va_list va;
        va_start(va, format);
        AI_Debugv(name, format, va);
        va_end(va);
    }

    inline PlannerNode *NewPlannerNode();
public:
    // Don't pass self as a constructor argument (self->ai ptr might not been set yet)
    inline AiBaseAction(Ai *ai, const char *name_): self(ai->self), name(name_)
    {
        Register(ai, this);
    }

    virtual ~AiBaseAction() {}

    virtual PlannerNode *TryApply(const WorldState &worldState) = 0;
};

class AiBaseBrain: public AiFrameAwareUpdatable
{
    friend class Ai;
    friend class AiManager;
    friend class AiBaseTeamBrain;
    friend class AiBaseGoal;
    friend class AiBaseAction;
    friend class AiBaseActionRecord;
protected:
    edict_t *self;

    NavTarget *navTarget;
    AiBaseActionRecord *planHead;
    AiBaseGoal *activeGoal;
    unsigned nextActiveGoalUpdateAt;

    const NavTarget *lastReachedNavTarget;
    unsigned lastNavTargetReachedAt;

    unsigned prevThinkAt;

    float decisionRandom;
    unsigned nextDecisionRandomUpdateAt;

    static constexpr unsigned MAX_GOALS = 12;
    StaticVector<AiBaseGoal *, MAX_GOALS> goals;

    static constexpr unsigned MAX_ACTIONS = 36;
    StaticVector<AiBaseAction *, MAX_ACTIONS> actions;

    static constexpr unsigned MAX_PLANNER_NODES = 384;
    Pool<PlannerNode, MAX_PLANNER_NODES> plannerNodesPool;

    signed char attitude[MAX_EDICTS];
    // Used to detect attitude change
    signed char oldAttitude[MAX_EDICTS];

    int CurrAasAreaNum() const { return self->ai->aiRef->currAasAreaNum; };
    int DroppedToFloorAasAreaNum() const { return self->ai->aiRef->droppedToFloorAasAreaNum; }
    Vec3 DroppedToFloorOrigin() const { return self->ai->aiRef->droppedToFloorOrigin; }

    int PreferredAasTravelFlags() const { return self->ai->aiRef->preferredAasTravelFlags; }
    int AllowedAasTravelFlags() const { return self->ai->aiRef->allowedAasTravelFlags; }

    const AiAasWorld *AasWorld() const { return self->ai->aiRef->aasWorld; }
    AiAasRouteCache *RouteCache() { return self->ai->aiRef->routeCache; }
    const AiAasRouteCache *RouteCache() const { return self->ai->aiRef->routeCache; }

    AiBaseBrain(edict_t *self);

    virtual void PrepareCurrWorldState(WorldState *worldState) = 0;

    bool UpdateGoalAndPlan(const WorldState &currWorldState);

    bool FindNewGoalAndPlan(const WorldState &currWorldState);

    AiBaseActionRecord *BuildPlan(AiBaseGoal *goal, const WorldState &startWorldState);

    PlannerNode *GetWorldStateTransitions(const WorldState &from) const;

    AiBaseActionRecord *ReconstructPlan(PlannerNode *lastNode) const;

    void SetGoalAndPlan(AiBaseGoal *goal_, AiBaseActionRecord *planHead_);

    inline void SetNavTarget(NavTarget *navTarget)
    {
        this->navTarget = navTarget;
        self->ai->aiRef->OnNavTargetSet(navTarget);
    }

    inline void ResetNavTarget()
    {
        this->navTarget = nullptr;
        self->ai->aiRef->OnNavTargetReset();
    }

    int FindAasParamToGoalArea(int goalAreaNum, int (AiAasRouteCache::*pathFindingMethod)(int, int, int) const) const;

    int FindReachabilityToGoalArea(int goalAreaNum) const;
    int FindTravelTimeToGoalArea(int goalAreaNum) const;

    virtual void PreThink() override;

    virtual void Think() override;

    virtual void PostThink() override
    {
        prevThinkAt = level.time;
    }

    virtual void OnAttitudeChanged(const edict_t *ent, int oldAttitude_, int newAttitude_) {}

public:
    virtual ~AiBaseBrain() override {}

    void SetAttitude(const edict_t *ent, int attitude);

    inline bool HasNavTarget() const { return navTarget != nullptr; }

    inline bool HasPlan() const { return planHead != nullptr; }

    void ClearGoalAndPlan();

    void DeletePlan(AiBaseActionRecord *head);

    bool IsCloseToNavTarget(float proximityThreshold) const
    {
        return DistanceSquared(self->s.origin, navTarget->Origin().Data()) < proximityThreshold * proximityThreshold;
    }

    int NavTargetAasAreaNum() const { return navTarget->AasAreaNum(); }
    Vec3 NavTargetOrigin() const { return navTarget->Origin(); }

    bool HandleNavTargetTouch(const edict_t *ent);
    bool TryReachNavTargetByProximity();

    // Helps to reject non-feasible enemies quickly.
    // A false result does not guarantee that enemy is feasible.
    // A true result guarantees that enemy is not feasible.
    bool MayNotBeFeasibleEnemy(const edict_t *ent) const;
};

inline void AiBaseGoal::Register(Ai *ai, AiBaseGoal *goal)
{
    ai->aiBaseBrain->goals.push_back(goal);
}

inline void AiBaseAction::Register(Ai *ai, AiBaseAction *action)
{
    ai->aiBaseBrain->actions.push_back(action);
}

inline PlannerNode *AiBaseAction::NewPlannerNode()
{
    return self->ai->aiRef->aiBaseBrain->plannerNodesPool.New();
}

#endif
