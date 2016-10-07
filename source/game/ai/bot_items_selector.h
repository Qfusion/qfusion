#ifndef QFUSION_BOT_ITEMS_SELECTOR_H
#define QFUSION_BOT_ITEMS_SELECTOR_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "static_vector.h"

class SelectedNavEntity
{
    friend class BotBrain;
    friend class BotItemsSelector;

    const NavEntity *navEntity;
    float cost;
    unsigned selectedAt;
    unsigned timeoutAt;

    inline SelectedNavEntity(const NavEntity *navEntity_, float cost_, unsigned timeoutAt_)
        : navEntity(navEntity_), cost(cost_), selectedAt(level.time), timeoutAt(timeoutAt_) {}

    inline void CheckValid() const
    {
        if (!IsValid()) abort();
    }
public:
    inline bool IsEmpty() const { return navEntity == nullptr; }
    // Empty one is considered valid (until it times out)
    inline bool IsValid() const { return timeoutAt > level.time; }
    inline void Invalidate()
    {
        navEntity = nullptr;
        cost = std::numeric_limits<float>::max();
        timeoutAt = level.time;
    }
    // Avoid class/method name clash by using Get prefix
    inline const NavEntity *GetNavEntity() const
    {
        CheckValid();
        return navEntity;
    }
    inline float GetCost() const
    {
        CheckValid();
        return cost;
    }
};

class BotItemsSelector
{
    edict_t *self;

    float internalEntityWeights[MAX_EDICTS];
    float overriddenEntityWeights[MAX_EDICTS];

    inline float GetEntityWeight(int entNum)
    {
        float overriddenWeight = overriddenEntityWeights[entNum];
        if (overriddenWeight != 0)
            return overriddenWeight;
        return internalEntityWeights[entNum];
    }

    inline const int *Inventory() const { return self->r.client->ps.inventory; }

    void UpdateInternalItemsWeights();

    float ComputeItemWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeWeaponWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeAmmoWeight(const gsitem_t *item) const;
    float ComputeArmorWeight(const gsitem_t *item) const;
    float ComputeHealthWeight(const gsitem_t *item) const;
    float ComputePowerupWeight(const gsitem_t *item) const;

    inline void Debug(const char *format, ...)
    {
        va_list va;
        va_start(va, format);
        AI_Debugv(self->r.client->netname, format, va);
        va_end(va);
    }
public:
    inline BotItemsSelector(edict_t *self_): self(self_) {}

    inline void ClearOverriddenEntityWeights()
    {
        memset(overriddenEntityWeights, 0, sizeof(overriddenEntityWeights));
    }

    // This weight overrides internal one computed by this brain itself.
    inline void OverrideEntityWeight(const edict_t *ent, float weight)
    {
        overriddenEntityWeights[ENTNUM(const_cast<edict_t*>(ent))] = weight;
    }

    SelectedNavEntity SuggestGoalNavEntity(const SelectedNavEntity &currSelectedNavEntity);
};

#endif
