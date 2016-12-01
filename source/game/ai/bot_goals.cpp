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

    float entRawWeight = SelectedNavEntity().GetRawWeight();
    if (entRawWeight <= 0.0f)
        return;

    float goalWeight = 1.5f * BoundedFraction(entRawWeight, 5.0f);

    this->weight = goalWeight;
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