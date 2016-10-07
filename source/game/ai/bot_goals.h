#ifndef QFUSION_BOT_GOALS_H
#define QFUSION_BOT_GOALS_H

#include "ai_base_brain.h"

class BotGrabItemGoal: public AiBaseGoal
{
public:
    BotGrabItemGoal(Ai *ai_): AiBaseGoal(ai_) {}

    void UpdateWeight(const WorldState &currWorldState) override;
    void GetDesiredWorldState(WorldState *worldState) override;
};

#endif
