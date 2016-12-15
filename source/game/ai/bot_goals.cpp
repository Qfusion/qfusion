#include "bot_goals.h"
#include "bot.h"

inline const SelectedNavEntity &BotBaseGoal::SelectedNavEntity() const
{
    return self->ai->botRef->botBrain.selectedNavEntity;
}

inline const SelectedEnemies &BotBaseGoal::SelectedEnemies() const
{
    return self->ai->botRef->selectedEnemies;
}

PlannerNode *BotBaseGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;
    for (AiBaseAction *action: self->ai->botRef->botBrain.actions)
    {
        if (PlannerNode *currTransition = action->TryApply(worldState))
        {
            currTransition->nextTransition = firstTransition;
            firstTransition = currTransition;
        }
    }
    return firstTransition;
}

void BotGrabItemGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (!SelectedNavEntity().IsValid())
        return;
    if (SelectedNavEntity().IsEmpty())
        return;

    this->weight = SelectedNavEntity().PickupGoalWeight();
}

void BotGrabItemGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasJustPickedGoalItemVar().SetValue(true).SetIgnore(false);
}

#define TRY_APPLY_ACTION(actionName)                                                     \
do                                                                                       \
{                                                                                        \
    if (PlannerNode *currTransition = self->ai->botRef->actionName.TryApply(worldState)) \
    {                                                                                    \
        currTransition->nextTransition = firstTransition;                                \
        firstTransition = currTransition;                                                \
    }                                                                                    \
} while (0)

PlannerNode *BotGrabItemGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(genericRunToItemAction);
    TRY_APPLY_ACTION(pickupItemAction);
    TRY_APPLY_ACTION(waitForItemAction);

    return firstTransition;
}

void BotKillEnemyGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (!SelectedEnemies().AreValid())
        return;

    this->weight = 1.5f * self->ai->botRef->GetEffectiveOffensiveness();
    if (currWorldState.HasThreateningEnemyVar())
        this->weight *= 1.75f;
}

void BotKillEnemyGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasJustKilledEnemyVar().SetValue(true).SetIgnore(false);
}

PlannerNode *BotKillEnemyGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(advanceToGoodPositionAction);
    TRY_APPLY_ACTION(retreatToGoodPositionAction);
    TRY_APPLY_ACTION(steadyCombatAction);
    TRY_APPLY_ACTION(gotoAvailableGoodPositionAction);

    TRY_APPLY_ACTION(killEnemyAction);

    return firstTransition;
}

void BotRunAwayGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (!SelectedEnemies().AreValid())
        return;
    if (!SelectedEnemies().AreThreatening())
        return;

    this->weight = 1.5f * (1.0f - self->ai->botRef->GetEffectiveOffensiveness());
    if (currWorldState.HasThreateningEnemyVar())
        this->weight *= 1.75f;
}

void BotRunAwayGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasRunAwayVar().SetValue(true).SetIgnore(false);
}

PlannerNode *BotRunAwayGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(genericRunAvoidingCombatAction);

    TRY_APPLY_ACTION(startGotoCoverAction);
    TRY_APPLY_ACTION(takeCoverAction);

    TRY_APPLY_ACTION(startGotoRunAwayTeleportAction);
    TRY_APPLY_ACTION(doRunAwayViaTeleportAction);

    TRY_APPLY_ACTION(startGotoRunAwayJumppadAction);
    TRY_APPLY_ACTION(doRunAwayViaJumppadAction);

    TRY_APPLY_ACTION(startGotoRunAwayElevatorAction);
    TRY_APPLY_ACTION(doRunAwayViaElevatorAction);

    TRY_APPLY_ACTION(stopRunningAwayAction);

    return firstTransition;
}

void BotReactToDangerGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasReactedToDangerVar().SetIgnore(false).SetValue(true);
}

void BotReactToDangerGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (currWorldState.PotentialDangerDamageVar().Ignore())
        return;

    float weight = 0.15f + 1.75f * currWorldState.PotentialDangerDamageVar() / currWorldState.DamageToBeKilled();
    if (weight > 3.0f)
        weight = 3.0f;
    weight /= 3.0f;
    weight = 3.0f / Q_RSqrt(weight);
    this->weight = weight;
}

PlannerNode *BotReactToDangerGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(dodgeToSpotAction);

    return firstTransition;
}

void BotReactToThreatGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (currWorldState.ThreatPossibleOriginVar().Ignore())
        return;

    float weight = 0.15f + 3.25f * currWorldState.ThreatInflictedDamageVar() / currWorldState.DamageToBeKilled();
    float offensiveness = self->ai->botRef->GetEffectiveOffensiveness();
    if (offensiveness >= 0.5f)
        weight *= (1.0f + (offensiveness - 0.5f));
    if (weight > 1.75f)
        weight = 1.75f;
    weight /= 1.75f;
    weight = 1.75f / Q_RSqrt(weight);
    this->weight = weight;
}

void BotReactToThreatGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasReactedToThreatVar().SetIgnore(false).SetValue(true);
}

PlannerNode *BotReactToThreatGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(turnToThreatOriginAction);

    return firstTransition;
}

void BotReactToEnemyLostGoal::UpdateWeight(const WorldState &currWorldState)
{
    this->weight = 0.0f;

    if (currWorldState.LostEnemyLastSeenOriginVar().Ignore())
        return;

    this->weight = 1.5f * self->ai->botRef->GetEffectiveOffensiveness();
}

void BotReactToEnemyLostGoal::GetDesiredWorldState(WorldState *worldState)
{
    worldState->SetIgnoreAll(true);

    worldState->HasReactedToEnemyLostVar().SetIgnore(false).SetValue(true);
}

PlannerNode *BotReactToEnemyLostGoal::GetWorldStateTransitions(const WorldState &worldState)
{
    PlannerNode *firstTransition = nullptr;

    TRY_APPLY_ACTION(turnToLostEnemyAction);
    TRY_APPLY_ACTION(startLostEnemyPursuitAction);
    TRY_APPLY_ACTION(genericRunAvoidingCombatAction);
    TRY_APPLY_ACTION(stopLostEnemyPursuitAction);

    return firstTransition;
}