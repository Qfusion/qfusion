#ifndef QFUSION_BOT_GOALS_H
#define QFUSION_BOT_GOALS_H

#include "ai_base_brain.h"

class BotBaseGoal: public AiBaseGoal
{
public:
    BotBaseGoal(Ai *ai_, const char *name_, unsigned updatePeriod_)
        : AiBaseGoal(ai_, name_, updatePeriod_) {}

protected:
    inline const class SelectedNavEntity &SelectedNavEntity() const;
    inline const class SelectedEnemies &SelectedEnemies() const;
};

class BotGrabItemGoal: public BotBaseGoal
{
public:
    BotGrabItemGoal(Ai *ai_): BotBaseGoal(ai_, "BotGrabItemGoal", 1500) {}

    void UpdateWeight(const WorldState &currWorldState) override;
    void GetDesiredWorldState(WorldState *worldState) override;
};

class BotKillEnemyGoal: public BotBaseGoal
{
public:
    BotKillEnemyGoal(Ai *ai_): BotBaseGoal(ai_, "BotKillEnemyGoal", 750) {}

    void UpdateWeight(const WorldState &currWorldState) override;
    void GetDesiredWorldState(WorldState *worldState) override;
};

class BotRunAwayGoal: public BotBaseGoal
{
public:
    BotRunAwayGoal(Ai *ai_): BotBaseGoal(ai_, "BotRunAwayGoal", 750) {}

    void UpdateWeight(const WorldState &currWorldState) override;
    void GetDesiredWorldState(WorldState *worldState) override;
};

#endif
