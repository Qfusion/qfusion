#ifndef QFUSION_BOT_ACTIONS_H
#define QFUSION_BOT_ACTIONS_H

#include "ai_base_brain.h"

class BotGutsActionsAccessor
{
    // We have to avoid member name collisions in this class children, so use a different name for member
    edict_t *ent;
protected:
    inline BotGutsActionsAccessor(edict_t *self): ent(self)
    {
#ifdef _DEBUG
        if (!self)
            AI_FailWith("BotGutsActionsAccessor()", "Attempt to initialize using a null self pointer\n");
#endif
    }
    inline void SetNavTarget(NavTarget *navTarget);
    inline void ResetNavTarget();

    inline const class SelectedEnemies &Enemies() const;
    inline const class SelectedWeapons &Weapons() const;

    inline struct SelectedTactics &Tactics();
    inline const struct SelectedTactics &Tactics() const;

    inline const class SelectedNavEntity &GetGoalNavEntity() const;

    inline void SetCampingSpotWithoutDirection(const Vec3 &spotOrigin, float spotRadius);
    inline void SetDirectionalCampingSpot(const Vec3 &spotOrigin, const Vec3 &lookAtPoint, float spotRadius);
    inline void InvalidateCampingSpot();

    int TravelTimeMillis(const Vec3 &from, const Vec3 &to);
};

class BotBaseActionRecord: public AiBaseActionRecord, protected BotGutsActionsAccessor
{
public:
    BotBaseActionRecord(PoolBase *pool_, edict_t *self_, const char *name_)
        : AiBaseActionRecord(pool_, self_, name_), BotGutsActionsAccessor(self_) {}
};

class BotBaseAction: public AiBaseAction, protected BotGutsActionsAccessor
{
public:
    BotBaseAction(Ai *ai, const char *name_)
        : AiBaseAction(ai, name_), BotGutsActionsAccessor(ai->self) {}
};

class BotGenericRunActionRecord: public BotBaseActionRecord
{
    NavTarget navTarget;
public:
    BotGenericRunActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : BotBaseActionRecord(pool_, self_, "BotGenericRunActionRecord"), navTarget(NavTarget::Dummy())
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const override;
};

#define DECLARE_ACTION(actionName, poolSize)                                                     \
class actionName: public BotBaseAction                                                           \
{                                                                                                \
    Pool<actionName##Record, poolSize> pool;                                                     \
public:                                                                                          \
    actionName(Ai *ai_): BotBaseAction(ai_, #actionName), pool("Pool<" #actionName "Record>") {} \
    PlannerNode *TryApply(const WorldState &worldState) override;                                \
}

DECLARE_ACTION(BotGenericRunAction, 3);

class BotPickupItemActionRecord: public BotBaseActionRecord
{
    NavTarget navTarget;
public:
    BotPickupItemActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : BotBaseActionRecord(pool_, self_, "BotPickupItemActionRecord"), navTarget(NavTarget::Dummy())
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const;
};

DECLARE_ACTION(BotPickupItemAction, 3);

class BotWaitForItemActionRecord: public BotBaseActionRecord
{
    NavTarget navTarget;
public:
    BotWaitForItemActionRecord(PoolBase *pool_, edict_t *self_, const NavEntity *navEntity_)
        : BotBaseActionRecord(pool_, self_, "BotWaitForItemActionRecord"), navTarget(NavTarget::Dummy())
    {
        navTarget.SetToNavEntity(navEntity_);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const;
};

DECLARE_ACTION(BotWaitForItemAction, 3);

// A dummy action that always terminates combat actions chain but should not actually gets reached.
// This action is used to avoid direct world state satisfaction by temporary actions
// (that leads to premature planning termination).
class BotKillEnemyActionRecord: public BotBaseActionRecord
{
public:
    BotKillEnemyActionRecord(PoolBase *pool_, edict_t *self_)
        : BotBaseActionRecord(pool_, self, "BotKillEnemyActionRecord") {}

    void Activate() override { AiBaseActionRecord::Activate(); }
    void Deactivate() override { AiBaseActionRecord::Deactivate(); }
    Status CheckStatus(const WorldState &currWorldState) const
    {
        Debug("This is a dummy action that is never valid by its nature, should replan\n");
        return INVALID;
    }
};

DECLARE_ACTION(BotKillEnemyAction, 5);

class BotCombatActionRecord: public BotBaseActionRecord
{
protected:
    NavTarget navTarget;
    unsigned selectedEnemiesInstanceId;

    bool CheckCommonCombatConditions(const WorldState &currWorldState) const;
public:
    BotCombatActionRecord(PoolBase *pool_, edict_t *self_, const char *name_,
                          const Vec3 &tacticalSpotOrigin,
                          unsigned selectedEnemiesInstanceId)
        : BotBaseActionRecord(pool_, self_, name_),
          navTarget(NavTarget::Dummy()),
          selectedEnemiesInstanceId(selectedEnemiesInstanceId) {}
};

#define DECLARE_COMBAT_ACTION_RECORD(recordName)                                                                     \
class recordName: public BotCombatActionRecord                                                                       \
{                                                                                                                    \
public:                                                                                                              \
    recordName(PoolBase *pool_, edict_t *self_, const Vec3 &tacticalSpotOrigin, unsigned selectedEnemiesInstanceId_) \
        : BotCombatActionRecord(pool_, self_, #recordName, tacticalSpotOrigin, selectedEnemiesInstanceId_) {}        \
    void Activate() override;                                                                                        \
    void Deactivate() override;                                                                                      \
    Status CheckStatus(const WorldState &currWorldState) const override;                                             \
};

DECLARE_COMBAT_ACTION_RECORD(BotAdvanceToGoodPositionActionRecord);
DECLARE_ACTION(BotAdvanceToGoodPositionAction, 2);

DECLARE_COMBAT_ACTION_RECORD(BotRetreatToGoodPositionActionRecord);
DECLARE_ACTION(BotRetreatToGoodPositionAction, 2);

DECLARE_COMBAT_ACTION_RECORD(BotSteadyCombatActionRecord);
DECLARE_ACTION(BotSteadyCombatAction, 2);

DECLARE_COMBAT_ACTION_RECORD(BotGotoAvailableGoodPositionActionRecord);
DECLARE_ACTION(BotGotoAvailableGoodPositionAction, 2);

class BotRetreatToCoverActionRecord: public BotBaseActionRecord
{
    NavTarget navTarget;
    const unsigned selectedEnemiesInstanceId;
public:
    BotRetreatToCoverActionRecord(PoolBase *pool_, edict_t *self_,
                                  const Vec3 &tacticalSpotOrigin,
                                  unsigned selectedEnemiesInstanceId_)
        : BotBaseActionRecord(pool_, self_, "BotRetreatToCoverActionRecord"),
          navTarget(NavTarget::Dummy()),
          selectedEnemiesInstanceId(selectedEnemiesInstanceId_)
    {
        navTarget.SetToTacticalSpot(tacticalSpotOrigin);
    }

    void Activate() override;
    void Deactivate() override;
    Status CheckStatus(const WorldState &currWorldState) const;
};

DECLARE_ACTION(BotRetreatToCoverAction, 3);

#endif
