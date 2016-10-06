#ifndef QFUSION_BOT_ITEMS_SELECTOR_H
#define QFUSION_BOT_ITEMS_SELECTOR_H

#include "ai_local.h"
#include "ai_goal_entities.h"
#include "static_vector.h"

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

    const NavEntity *SuggestGoalNavEntity(const NavEntity *currGoalNavEntity);
};

#endif
