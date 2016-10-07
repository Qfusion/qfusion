#include "bot_goals.h"

void BotGrabItemGoal::UpdateWeight(const WorldState &currWorldState)
{
    weight = 0.1f;
}

void BotGrabItemGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasJustPickedGoalItem().SetValue(true).SetIgnore(false);
}
