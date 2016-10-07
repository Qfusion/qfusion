#ifndef QFUSION_BOT_ACTIONS_H
#define QFUSION_BOT_ACTIONS_H

#include "ai_base_brain.h"

class BotGenericRunActionRecord: public AiBaseActionRecord
{
    NavTarget navTarget;
public:
    BotGenericRunActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : AiBaseActionRecord(pool_, self_, "BotGenericRunActionRecord")
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const override;
};

class BotGenericRunAction: public AiBaseAction
{
    Pool<BotGenericRunActionRecord, 3> pool;
public:
    BotGenericRunAction(Ai *ai): AiBaseAction(ai, "BotGenericRunAction"), pool("Pool<BotGenericRunActionRecord>") {}
    PlannerNode *TryApply(const WorldState &worldState) override;
};

class BotPickupItemActionRecord: public AiBaseActionRecord
{
    NavTarget navTarget;
public:
    BotPickupItemActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : AiBaseActionRecord(pool_, self_, "BotPickupItemActionRecord")
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const;
};

class BotPickupItemAction: public AiBaseAction
{
    Pool<BotPickupItemActionRecord, 3> pool;
public:
    BotPickupItemAction(Ai *ai): AiBaseAction(ai, "BotPickupItemAction"), pool("Pool<BotPickupItemActionRecord>") {}
    PlannerNode *TryApply(const WorldState &worldState) override;
};

class BotWaitForItemActionRecord: public AiBaseActionRecord
{
    NavTarget navTarget;
public:
    BotWaitForItemActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : AiBaseActionRecord(pool_, self_, "BotWaitForItemActionRecord")
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const;
};

class BotWaitForItemAction: public AiBaseAction
{
    Pool<BotWaitForItemActionRecord, 3> pool;
public:
    BotWaitForItemAction(Ai *ai) : AiBaseAction(ai, "BotWaitForItemAction"), pool("Pool<BotWaitForItemActionRecord>") {}
    PlannerNode *TryApply(const WorldState &currWorldState) override;
};

#endif
