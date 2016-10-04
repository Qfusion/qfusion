#ifndef QFUSION_AI_BASE_BRAIN_H
#define QFUSION_AI_BASE_BRAIN_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "ai_frame_aware_updatable.h"
#include "static_vector.h"
#include "ai_aas_route_cache.h"
#include "ai_base_ai.h"

class WorldState
{
public:
    bool IsSatisfiedBy(const WorldState &worldState) const;

    uint32_t Hash() const;
    bool operator==(const WorldState &that) const;

    bool EnemyIsOnSniperRange() const;
    bool EnemyIsOnFarRange() const;
    bool EnemyIsOnMiddleRange() const;
    bool EnemyIsOnCloseRange() const;

    float DistanceToEnemy() const;

    float DamageToBeKilled() const;
    float DamageToKill() const;
    float KillToBeKilledDamageRatio();
};

class AiBaseGoal
{
    friend class Ai;
    friend class AiBaseBrain;

    static void Register(edict_t *owner, AiBaseGoal *self);
protected:
    edict_t *owner;

    float weight;
public:
    AiBaseGoal(edict_t *owner_) : owner(owner_), weight(0.0f)
    {
        Register(owner, this);
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
};

class alignas(8) PoolBase
{
    friend class PoolItem;

    char *basePtr;
    unsigned itemSize;
    short firstFree;
    short firstUsed;

    inline class PoolItem &ItemAt(short index)
    {
        return *(PoolItem *)(basePtr + itemSize * index);
    }
    inline short IndexOf(const class PoolItem *item) const
    {
        return (short)(((const char *)item - basePtr) / itemSize);
    }

    inline void Link(short itemIndex, short *listHead);
    inline void Unlink(short itemIndex, short *listHead);

protected:
    void *Alloc();
    void Free(class PoolItem *poolItem);

public:
    PoolBase(char *basePtr_, unsigned itemSize_, unsigned itemsCount);

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
    Pool(): PoolBase(buffer, sizeof(Item), N) {}

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
    edict_t *owner;
public:
    AiBaseActionRecord *nextInPlan;

    AiBaseActionRecord(PoolBase *pool_, edict_t *owner_)
        : PoolItem(pool_), owner(owner_), nextInPlan(nullptr) {}

    virtual ~AiBaseActionRecord() {}

    virtual void Activate() {};
    virtual void Deactivate() {};

    enum class Status
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
    PlannerNode *nextInPlan;
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
        nextInPlan = nullptr;
        nextTransition = nullptr;
        prevInHashBin = nullptr;
        nextInHashBin = nullptr;
    }
};

class AiBaseAction
{
    friend class Ai;
    friend class AiBaseBrain;

    static void Register(edict_t *owner, AiBaseAction *self);
protected:
    edict_t *owner;
public:
    AiBaseAction(edict_t *owner): owner(owner)
    {
        Register(owner, this);
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

    float decisionRandom;
    unsigned nextDecisionRandomUpdateAt;

    static constexpr unsigned MAX_GOALS = 12;
    StaticVector<AiBaseGoal *, MAX_GOALS> goals;

    static constexpr unsigned MAX_ACTIONS = 36;
    StaticVector<AiBaseAction *, MAX_ACTIONS> actions;

    Pool<PlannerNode, 384> plannerNodesPool;

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

    void UpdateGoalsAndPlan(const WorldState &currWorldState);

    AiBaseActionRecord *BuildPlan(AiBaseGoal *goal, const WorldState &startWorldState);

    PlannerNode *GetWorldStateTransitions(const WorldState &from) const;

    AiBaseActionRecord *ReconstructPlan(PlannerNode *startNode) const;

    inline void SetPlan(AiBaseActionRecord *planHead_)
    {
        if (this->planHead) abort();
        this->planHead = planHead_;
        this->planHead->Activate();
    }

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

    virtual void OnAttitudeChanged(const edict_t *ent, int oldAttitude_, int newAttitude_) {}

public:
    virtual ~AiBaseBrain() override {}

    void SetAttitude(const edict_t *ent, int attitude);

    inline bool HasNavTarget() const { return navTarget != nullptr; }

    inline bool HasPlan() const { return planHead != nullptr; }

    void ClearPlan();

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

#endif
