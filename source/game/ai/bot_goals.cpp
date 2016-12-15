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