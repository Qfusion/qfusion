#ifndef QFUSION_AI_GOAL_ENTITIES_H
#define QFUSION_AI_GOAL_ENTITIES_H

#include "ai_local.h"
#include "vec3.h"

enum class NavEntityFlags: unsigned
{
    NONE = 0x0,
    REACH_AT_TOUCH = 0x1,
    REACH_AT_RADIUS = 0x2,
    REACH_ON_EVENT = 0x4,
    REACH_IN_GROUP = 0x8,
    DROPPED_ENTITY = 0x10,
    NOTIFY_SCRIPT = 0x20,
    MOVABLE = 0x40
};

inline NavEntityFlags operator|(const NavEntityFlags &lhs, const NavEntityFlags &rhs)
{
    return (NavEntityFlags)((unsigned)lhs | (unsigned)(rhs));
}
inline NavEntityFlags operator&(const NavEntityFlags &lhs, const NavEntityFlags &rhs)
{
    return (NavEntityFlags)((unsigned)lhs & (unsigned)(rhs));
}

enum class GoalFlags: unsigned
{
    NONE = 0x0,
    REACH_ON_RADIUS = 0x1,
    REACH_ON_EVENT = 0x2,
    REACH_IN_GROUP = 0x4,
    TACTICAL_SPOT = 0x8
};

inline GoalFlags operator|(const GoalFlags &lhs, const GoalFlags &rhs)
{
    return (GoalFlags)((unsigned)lhs | (unsigned)rhs);
}
inline GoalFlags operator&(const GoalFlags &lhs, const GoalFlags &rhs)
{
    return (GoalFlags)((unsigned)lhs & (unsigned)rhs);
}

// A NavEntity is based on some entity (edict_t) plus some attributes.
// All NavEntities are global for all Ai beings.
// A Goal is often based on a NavEntity
class NavEntity
{
    friend class NavEntitiesRegistry;
    friend class BotBrain;

    // Numeric id that matches index of corresponding entity in game edicts (if any)
    int id;
    // Id of area this goal is located in
    int aasAreaNum;
    // A goal origin, set once on goal addition or updated explicitly for movable goals
    // (It is duplicated from entity origin to prevent cheating with revealing
    // an actual origin of movable entities not marked as movable).
    Vec3 origin;
    // Misc. goal flags, mainly defining way this goal should be reached
    NavEntityFlags flags;
    // An entity this goal is based on
    edict_t *ent;
    // Links for registry goals pool
    NavEntity *prev, *next;
    // All fields are set to zero by NavEntities registry initialization code
    NavEntity(): origin(0, 0, 0) {}

    static constexpr unsigned MAX_NAME_LEN = 128;
    char name[MAX_NAME_LEN];

    inline bool IsFlagSet(NavEntityFlags flag) const
    {
        return NavEntityFlags::NONE != (this->flags & flag);
    }
public:
    inline NavEntityFlags Flags() const { return flags; }
    inline int Id() const { return id; }
    inline int AasAreaNum() const { return aasAreaNum; }
    // A cost influence defines how base entity weight is affected by cost (move duration and wait time).
    // A cost influence is a positive float number usually in 0.5-1.0 range.
    // Lesser cost influence means that an entity weight is less affected by distance.
    float CostInfluence() const;
    inline Vec3 Origin() const { return origin; }
    inline const gsitem_t *Item() const { return ent ? ent->item : nullptr; }
    inline const char *Classname() const { return ent ? ent->classname : nullptr; }
    inline bool IsEnabled() const { return ent && ent->r.inuse; }
    inline bool IsDisabled() const { return !IsEnabled(); }
    inline bool IsBasedOnEntity(const edict_t *e) const { return e && this->ent == e; }
    inline bool IsClient() const { return ent->r.client != nullptr; }
    inline bool IsSpawnedAtm() const { return ent->r.solid != SOLID_NOT; }
    inline bool ToBeSpawnedLater() const { return ent->r.solid == SOLID_NOT; }

    inline bool IsDroppedEntity() const { return IsFlagSet(NavEntityFlags::DROPPED_ENTITY); }

    inline bool MayBeReachedInGroup() const { return IsFlagSet(NavEntityFlags::REACH_IN_GROUP); }

    unsigned MaxWaitDuration() const;

    bool IsTopTierItem() const;

    const char *Name() const { return name; }

    inline void NotifyTouchedByBot(const edict_t *bot)
    {
        if (ShouldNotifyScript())
            GT_asBotTouchedGoal(bot->ai, ent);
    }

    inline void NotifyBotReachedRadius(const edict_t *bot)
    {
        if (ShouldNotifyScript())
            GT_asBotReachedGoalRadius(bot->ai, ent);
    }

    unsigned Timeout() const;

    inline bool ShouldBeReachedAtTouch() const { return IsFlagSet(NavEntityFlags::REACH_AT_TOUCH); }
    inline bool ShouldBeReachedAtRadius() const { return IsFlagSet(NavEntityFlags::REACH_AT_RADIUS); }
    inline bool ShouldBeReachedOnEvent() const { return IsFlagSet(NavEntityFlags::REACH_ON_EVENT); }

    inline bool ShouldNotifyScript() const { return IsFlagSet(NavEntityFlags::NOTIFY_SCRIPT); }

    // Returns level.time when the item is already spawned
    // Returns zero if spawn time is unknown
    // Returns spawn time when the item is not spawned and spawn time may be predicted
    unsigned SpawnTime() const;
};

// A goal may be based on a NavEntity (an item with attributes) or may be an "aritficial" spot
// A goal has a setter, an Ai superclass.
// If a goal user does not own a goal (is not a setter of a goal),
// the goal cannot be canceled by the user, but can time out as any other goal.
class Goal
{
    friend class Ai;

    NavEntity *navEntity;

    const class AiFrameAwareUpdatable *setter;

    GoalFlags flags;

    Vec3 explicitOrigin;

    int explicitAasAreaNum;
    unsigned explicitSpawnTime;
    unsigned explicitTimeout;
    float explicitRadius;

    const char *name;

    void Clear();

    inline bool IsFlagSet(GoalFlags flag) const
    {
        return GoalFlags::NONE != (this->flags & flag);
    }
public:
    Goal(const class AiFrameAwareUpdatable *initialSetter);
    ~Goal();

    // Instead of cleaning the goal completely set a "setter" of this action
    // (otherwise a null setter cannot release the goal ownership)
    void ResetWithSetter(const class AiFrameAwareUpdatable *setter)
    {
        Clear();
        this->setter = setter;
    }

    void SetToNavEntity(NavEntity *navEntity, const AiFrameAwareUpdatable *setter);
    void SetToTacticalSpot(const Vec3 &origin, unsigned timeout, const AiFrameAwareUpdatable *setter);

    inline int AasAreaNum() const
    {
        return navEntity ? navEntity->AasAreaNum() : explicitAasAreaNum;
    }

    // A cost influence defines how base goal weight is affected by cost (move duration and wait time).
    // A cost influence is a positive float number usually in 0.5-1.0 range.
    // Lesser cost influence means that a goal weight is less affected by distance.
    inline float CostInfluence() const
    {
        return navEntity ? navEntity->CostInfluence() : 0.5f;
    }

    inline bool IsBasedOnEntity(const edict_t *ent) const
    {
        return navEntity ? navEntity->IsBasedOnEntity(ent) : false;
    }

    inline bool IsBasedOnNavEntity(const NavEntity *navEntity) const
    {
        return navEntity && this->navEntity == navEntity;
    }

    inline bool IsBasedOnSomeEntity() const { return navEntity != nullptr; }

    inline bool IsDisabled() const
    {
        return navEntity && navEntity->IsDisabled();
    }

    inline bool IsDroppedEntity() const
    {
        return navEntity && navEntity->IsDroppedEntity();
    }

    inline bool IsTacticalSpot() const { return IsFlagSet(GoalFlags::TACTICAL_SPOT); }

    inline bool IsEnabled() const { return !IsDisabled(); }

    inline bool IsTopTierItem()
    {
        return navEntity && navEntity->IsTopTierItem();
    }

    inline unsigned MaxWaitDuration() const
    {
        return navEntity ? navEntity->MaxWaitDuration() : 0;
    }

    inline bool MayBeReachedInGroup() const
    {
        if (navEntity)
            return navEntity->MayBeReachedInGroup();
        return IsFlagSet(GoalFlags::REACH_IN_GROUP);
    }

    inline const char *Name() const { return name ? name : "???"; }

    inline Vec3 Origin() const
    {
        return navEntity ? navEntity->Origin() : explicitOrigin;
    }

    inline float RadiusOrDefault(float defaultRadius) const
    {
        if (ShouldBeReachedAtRadius())
            return explicitRadius;
        return defaultRadius;
    }

    inline bool ShouldBeReachedAtTouch() const
    {
        return navEntity ? navEntity->ShouldBeReachedAtTouch(): false;
    }

    inline bool ShouldBeReachedAtRadius() const
    {
        if (navEntity)
            return navEntity->ShouldBeReachedAtRadius();
        return IsFlagSet(GoalFlags::REACH_ON_RADIUS);
    }

    inline bool ShouldBeReachedOnEvent() const
    {
        if (navEntity)
            return navEntity->ShouldBeReachedOnEvent();
        return IsFlagSet(GoalFlags::REACH_ON_EVENT);
    }

    inline bool ShouldNotifyScript() const
    {
        return navEntity ? navEntity->ShouldNotifyScript() : false;
    }

    inline void NotifyTouchedByBot(const edict_t *bot) const
    {
        if (navEntity)
            navEntity->NotifyTouchedByBot(bot);
    }

    inline void NotifyBotReachedRadius(const edict_t *bot) const
    {
        if (navEntity)
            navEntity->NotifyBotReachedRadius(bot);
    }

    // Returns level.time when the item is already spawned
    // Returns zero if spawn time is unknown
    // Returns spawn time when the item is not spawned and spawn time may be predicted
    inline unsigned SpawnTime() const
    {
        return navEntity ? navEntity->SpawnTime() : explicitSpawnTime;
    }

    inline unsigned Timeout() const
    {
        return navEntity ? navEntity->Timeout() : explicitTimeout;
    }

    // Hack for incomplete type
    inline const decltype(setter) Setter() const { return setter; }
};

class NavEntitiesRegistry
{
    NavEntity navEntities[MAX_NAVENTS];
    NavEntity *entityToNavEntity[MAX_EDICTS];
    NavEntity *freeNavEntity;
    NavEntity headnode;

    static NavEntitiesRegistry instance;

    NavEntity *AllocNavEntity();
    void FreeNavEntity(NavEntity *navEntity);
public:
    void Init();
    void Update();

    NavEntity *AddNavEntity(edict_t *ent, int aasAreaNum, NavEntityFlags flags);
    void RemoveNavEntity(NavEntity *navEntity);

    inline NavEntity *NavEntityForEntity(edict_t *ent)
    {
        if (!ent) return nullptr;
        return entityToNavEntity[ENTNUM(ent)];
    }

    class GoalEntitiesIterator
    {
        friend class NavEntitiesRegistry;
        NavEntity *currEntity;
        inline GoalEntitiesIterator(NavEntity *currEntity): currEntity(currEntity) {}
    public:
        inline NavEntity *operator*() { return currEntity; }
        inline const NavEntity *operator*() const { return currEntity; }
        inline void operator++() { currEntity = currEntity->prev; }
        inline bool operator!=(const GoalEntitiesIterator &that) const { return currEntity != that.currEntity; }
    };
    inline GoalEntitiesIterator begin() { return GoalEntitiesIterator(headnode.prev); }
    inline GoalEntitiesIterator end() { return GoalEntitiesIterator(&headnode); }

    static inline NavEntitiesRegistry *Instance() { return &instance; }
};

#define FOREACH_NAVENT(navEnt) for (auto *navEnt : *NavEntitiesRegistry::Instance())

#endif
