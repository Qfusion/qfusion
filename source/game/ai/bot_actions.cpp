#include "bot_actions.h"
#include "bot.h"

typedef WorldState::SatisfyOp SatisfyOp;

void BotGenericRunActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    self->ai->botRef->botBrain.selectedTactics.Clear();
    self->ai->botRef->botBrain.SetNavTarget(&navTarget);
}

void BotGenericRunActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    self->ai->botRef->botBrain.ResetNavTarget();
    self->ai->botRef->botBrain.selectedTactics.Clear();
}

AiBaseActionRecord::Status BotGenericRunActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    const auto &selectedNavEntity = self->ai->botRef->botBrain.GetOrUpdateSelectedNavEntity();
    if (!navTarget.IsBasedOnNavEntity(selectedNavEntity.GetNavEntity()))
    {
        Debug("Nav target does no longer match selected nav entity\n");
        return INVALID;
    }
    if (navTarget.SpawnTime() == 0)
    {
        Debug("Illegal nav target spawn time (looks like it has been invalidated)\n");
        return INVALID;
    }
    if (currWorldState.DistanceToGoalItemNavTarget() <= 72.0f)
    {
        Debug("Nav target pickup distance has been reached\n");
        return COMPLETED;
    }

    return VALID;
}

PlannerNode *BotGenericRunAction::TryApply(const WorldState &worldState)
{
    if (worldState.HasGoalItemNavTarget().Ignore())
    {
        Debug("Goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasGoalItemNavTarget())
    {
        Debug("Goal item nav target is not present in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItemNavTarget().Ignore())
    {
        Debug("Distance to goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.DistanceToGoalItemNavTarget() <= 72.0f)
    {
        Debug("Distance to goal item nav target is too low in the given world state\n");
        return nullptr;
    }

    const auto &itemNavEntity = self->ai->botRef->botBrain.selectedNavEntity;
    auto *actionRecord = pool.New(self, itemNavEntity.GetNavEntity());
    if (!actionRecord)
        return nullptr;

    auto *plannerNode = NewPlannerNode();
    if (!plannerNode)
    {
        actionRecord->DeleteSelf();
        return nullptr;
    }

    plannerNode->worldState = worldState;
    plannerNode->worldState.DistanceToGoalItemNavTarget().SetValue(71.0f).SetIgnore(false);
    plannerNode->worldStateHash = plannerNode->worldStateHash;
    plannerNode->transitionCost = itemNavEntity.GetCost();
    plannerNode->actionRecord = actionRecord;

    return plannerNode;
}

void BotPickupItemActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    self->ai->botRef->botBrain.selectedTactics.Clear();
    self->ai->botRef->botBrain.selectedTactics.shouldMoveCarefully = true;
    self->ai->botRef->botBrain.SetNavTarget(&navTarget);
}

void BotPickupItemActionRecord::Deactivate()
{
    AiBaseActionRecord::Activate();
    self->ai->botRef->botBrain.ResetNavTarget();
    self->ai->botRef->botBrain.selectedTactics.Clear();
}

AiBaseActionRecord::Status BotPickupItemActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustPickedGoalItem())
    {
        Debug("Goal item has been just picked up\n");
        return COMPLETED;
    }

    const SelectedNavEntity &currSelectedNavEntity = self->ai->botRef->botBrain.GetOrUpdateSelectedNavEntity();
    if (!navTarget.IsBasedOnNavEntity(currSelectedNavEntity.GetNavEntity()))
    {
        Debug("Nav target does no longer match current selected nav entity\n");
        return INVALID;
    }
    if (!navTarget.SpawnTime())
    {
        Debug("Illegal nav target spawn time (looks like it has been invalidated)\n");
        return INVALID;
    }
    if (navTarget.SpawnTime() - level.time > 0)
    {
        Debug("The nav target requires waiting for it\n");
        return INVALID;
    }
    if (currWorldState.DistanceToGoalItemNavTarget() > 72.0f)
    {
        Debug("The nav target is too far from the bot to pickup it\n");
        return INVALID;
    }
    if (currWorldState.HasThreateningEnemy())
    {
        Debug("The bot has threatening enemy\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotPickupItemAction::TryApply(const WorldState &worldState)
{
    if (worldState.HasGoalItemNavTarget().Ignore())
    {
        Debug("Goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasGoalItemNavTarget())
    {
        Debug("Goal item nav target is absent in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItemNavTarget().Ignore())
    {
        Debug("Distance to goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.DistanceToGoalItemNavTarget() > 72.0f)
    {
        Debug("Distance to goal item nav target is too large to pick up an item in the given world state\n");
        return nullptr;
    }

    if (worldState.HasJustPickedGoalItem().Ignore())
    {
        Debug("Has bot picked a goal item is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustPickedGoalItem())
    {
        Debug("Bot has just picked a goal item in the given world state\n");
        return nullptr;
    }

    if (worldState.GoalItemWaitTime().Ignore())
    {
        Debug("Goal item wait time is not specified in the given world state\n");
        return nullptr;
    }
    if (worldState.GoalItemWaitTime() > 0)
    {
        Debug("Goal item wait time is non-zero in the given world state\n");
        return nullptr;
    }

    const auto &itemNavEntity = self->ai->botRef->botBrain.selectedNavEntity;
    auto *actionRecord = pool.New(self, itemNavEntity.GetNavEntity());
    // Can't allocate a record
    if (!actionRecord)
        return nullptr;

    PlannerNode *plannerNode = NewPlannerNode();
    // Can't allocate planner node
    if (!plannerNode)
    {
        actionRecord->DeleteSelf();
        return nullptr;
    }

    plannerNode->worldState = worldState;
    plannerNode->worldState.HasJustPickedGoalItem().SetValue(true).SetIgnore(false);
    plannerNode->worldStateHash = plannerNode->worldState.Hash();
    // Picking up an item costs almost nothing
    plannerNode->transitionCost = 1.0f;
    plannerNode->actionRecord = actionRecord;
    return plannerNode;
}

void BotWaitForItemActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    self->ai->botRef->botBrain.SetNavTarget(&navTarget);
    self->ai->botRef->botBrain.selectedTactics.Clear();
    self->ai->botRef->campingSpotState.SetWithoutDirection(navTarget.Origin(), 40.0f, 0.75f);
}

void BotWaitForItemActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    self->ai->botRef->campingSpotState.Invalidate();
    self->ai->botRef->botBrain.selectedTactics.Clear();
    self->ai->botRef->botBrain.ResetNavTarget();
}

AiBaseActionRecord::Status BotWaitForItemActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustPickedGoalItem())
    {
        Debug("Goal item has been just picked up\n");
        return COMPLETED;
    }

    const auto &currSelectedNavEntity = self->ai->botRef->botBrain.GetOrUpdateSelectedNavEntity();
    if (!navTarget.IsBasedOnNavEntity(currSelectedNavEntity.GetNavEntity()))
    {
        Debug("Nav target does no longer match current selected nav entity\n");
        return INVALID;
    }
    if (!navTarget.SpawnTime())
    {
        Debug("Illegal nav target spawn time (looks like it has been invalidated)\n");
        return INVALID;
    }
    // Wait duration is too long (more than it was estimated)
    unsigned waitDuration = navTarget.SpawnTime() - level.time;
    if (navTarget.SpawnTime() - level.time > navTarget.MaxWaitDuration())
    {
        constexpr auto *format = "Wait duration %d is too long (the maximum allowed value for the nav target is %d)\n";
        Debug(format, waitDuration, navTarget.MaxWaitDuration());
        return INVALID;
    }
    if (currWorldState.DistanceToGoalItemNavTarget() > 72.0f)
    {
        Debug("Distance to the item is too large to wait for it\n");
        return INVALID;
    }
    if (currWorldState.HasThreateningEnemy())
    {
        Debug("The bot has a threatening enemy\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotWaitForItemAction::TryApply(const WorldState &worldState)
{
    if (worldState.HasGoalItemNavTarget().Ignore())
    {
        Debug("Goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasGoalItemNavTarget())
    {
        Debug("Goal item nav target is absent in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItemNavTarget().Ignore())
    {
        Debug("Distance to goal item nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.DistanceToGoalItemNavTarget() > 72.0f)
    {
        Debug("Distance to goal item nav target is too large to wait for an item in the given world state\n");
        return nullptr;
    }

    if (worldState.HasJustPickedGoalItem().Ignore())
    {
        Debug("Has bot picked a goal item is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustPickedGoalItem())
    {
        Debug("Bot has just picked a goal item in the given world state\n");
        return nullptr;
    }

    if (worldState.GoalItemWaitTime().Ignore())
    {
        Debug("Goal item wait time is not specified in the given world state\n");
        return nullptr;
    }
    if (worldState.GoalItemWaitTime() == 0)
    {
        Debug("Goal item wait time is zero in the given world state\n");
        return nullptr;
    }

    const auto &itemNavEntity = self->ai->botRef->botBrain.selectedNavEntity;
    auto *actionRecord = pool.New(self, itemNavEntity.GetNavEntity());
    // Can't allocate a record
    if (!actionRecord)
        return nullptr;

    PlannerNode *plannerNode = NewPlannerNode();
    if (!plannerNode)
    {
        actionRecord->DeleteSelf();
        return nullptr;
    }

    plannerNode->worldState = worldState;
    plannerNode->worldState.HasJustPickedGoalItem().SetValue(true).SetIgnore(false);
    plannerNode->worldStateHash = plannerNode->worldState.Hash();
    plannerNode->transitionCost = worldState.GoalItemWaitTime();
    plannerNode->actionRecord = actionRecord;
    return plannerNode;
}
