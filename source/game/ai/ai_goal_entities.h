#ifndef QFUSION_AI_GOAL_ENTITIES_H
#define QFUSION_AI_GOAL_ENTITIES_H

#include "ai_local.h"
#include "vec3.h"

enum class GoalFlags
{
    NONE = 0x0,
    REACH_AT_TOUCH = 0x1,
    REACH_ENTITY = 0x2,
    DROPPED_ENTITY = 0x4,
    TACTICAL_SPOT = 0x8
};

inline GoalFlags operator|(const GoalFlags &lhs, const GoalFlags &rhs) { return (GoalFlags)((int)lhs | (int)rhs); }
inline GoalFlags operator&(const GoalFlags &lhs, const GoalFlags &rhs) { return (GoalFlags)((int)lhs & (int)rhs); }

class NavEntity
{
    friend class GoalEntitiesRegistry;
    friend class BotBrain;

    // Numeric id that matches index of corresponding entity in game edicts (if any)
    int id;
    // Id of area this goal is located in
    int aasAreaNum;
    // Contains instance id of associated combat task
    // If current combat task instance id has been changed
    // and this is a special goal, this goal should be invalidated
    unsigned combatTaskInstanceId;
    // If zero (not set), should be computed according to goal-specific rules
    unsigned explicitTimeout;
    // If zero (not set), should be computed according to goal-specific rules
    unsigned explicitSpawnTime;
    // Should be used unless a goal is based on some entity.
    // In that case, origin of entity should be used
    Vec3 explicitOrigin;
    // Misc. goal flags, mainly defining way this goal should be reached
    GoalFlags goalFlags;
    // An entity this goal may be based on
    edict_t *ent;
    // Links for registry goals pool
    NavEntity *prev, *next;

    static constexpr unsigned MAX_NAME_LEN = 64;
    char name[MAX_NAME_LEN];

    // Should initialize all fields since it may be used as a AiBaseBrain subclass field not held by the registry
    NavEntity()
        : explicitOrigin(INFINITY, INFINITY, INFINITY)
    {
        id = 0;
        aasAreaNum = 0;
        combatTaskInstanceId = 0;
        explicitTimeout = 0;
        explicitSpawnTime = 0;
        goalFlags = GoalFlags::NONE;
        ent = nullptr;
        prev = next = nullptr;
        name[0] = '\0';
    }
public:

    inline int Id() const { return id; }
    inline int AasAreaNum() const { return aasAreaNum; }
    inline Vec3 Origin() const
    {
        return IsBasedOnSomeEntity()? Vec3(ent->s.origin) : explicitOrigin;
    }
    inline const gsitem_t *Item() const { return ent->item; }
    inline const char *Name() const { return name; }
    inline bool IsEnabled() const
    {
        return !IsTacticalSpot() ? ent && ent->r.inuse : true;
    }
    inline bool IsDisabled() const { return !IsEnabled(); }
    inline bool IsBasedOnEntity(const edict_t *e) const { return e && this->ent == e; }
    inline bool IsBasedOnSomeEntity() const { return ent != nullptr; }
    inline bool IsClient() const { return ent->r.client != nullptr; }
    inline bool IsSpawnedAtm() const { return ent->r.solid != SOLID_NOT; }
    inline bool ToBeSpawnedLater() const { return ent->r.solid == SOLID_NOT; }
    inline bool IsDroppedEntity() const
    {
        return ent && GoalFlags::NONE != (goalFlags & GoalFlags::DROPPED_ENTITY);
    }
    bool IsTopTierItem() const;
    inline bool IsTacticalSpot() const
    {
        return GoalFlags::NONE != (goalFlags & GoalFlags::TACTICAL_SPOT);
    }
    unsigned Timeout() const;
    inline bool ShouldBeReachedAtTouch() const
    {
        return GoalFlags::NONE != (goalFlags & GoalFlags::REACH_AT_TOUCH);
    }

    // Returns level.time when the item is already spawned
    // Returns zero if spawn time is unknown
    // Returns spawn time when the item is not spawned and spawn time may be predicted
    unsigned SpawnTime() const;
};

class GoalEntitiesRegistry
{
    NavEntity goalEnts[MAX_GOALENTS];
    NavEntity *entGoals[MAX_EDICTS];
    NavEntity *goalEntsFree;
    NavEntity goalEntsHeadnode;

    static GoalEntitiesRegistry instance;

    NavEntity *AllocGoalEntity();
    void FreeGoalEntity(NavEntity *navEntity);
public:
    void Init();

    NavEntity *AddGoalEntity(edict_t *ent, int aasAreaNum, GoalFlags flags = GoalFlags::NONE);
    void RemoveGoalEntity(NavEntity *navEntity);

    inline NavEntity *GoalEntityForEntity(edict_t *ent)
    {
        if (!ent) return nullptr;
        return entGoals[ENTNUM(ent)];
    }

    inline edict_t *GetGoalEntity(int index) { return goalEnts[index].ent; }
    inline int GetNextGoalEnt(int index) { return goalEnts[index].prev->id; }

    class GoalEntitiesIterator
    {
        friend class GoalEntitiesRegistry;
        NavEntity *currEntity;
        inline GoalEntitiesIterator(NavEntity *currEntity): currEntity(currEntity) {}
    public:
        inline NavEntity *operator*() { return currEntity; }
        inline const NavEntity *operator*() const { return currEntity; }
        inline void operator++() { currEntity = currEntity->prev; }
        inline bool operator!=(const GoalEntitiesIterator &that) const { return currEntity != that.currEntity; }
    };
    inline GoalEntitiesIterator begin() { return GoalEntitiesIterator(goalEntsHeadnode.prev); }
    inline GoalEntitiesIterator end() { return GoalEntitiesIterator(&goalEntsHeadnode); }

    static inline GoalEntitiesRegistry *Instance() { return &instance; }
};

#define FOREACH_GOALENT(goalEnt) for (auto *goalEnt : *GoalEntitiesRegistry::Instance())

#endif
