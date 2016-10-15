#include "bot_actions.h"
#include "bot.h"
#include "ai_ground_trace_cache.h"

typedef WorldState::SatisfyOp SatisfyOp;

// These methods really belong to the bot logic, not the generic AI ones

const short *WorldState::GetSniperRangeTacticalSpot()
{
    return self->ai->botRef->tacticalSpotsCache.GetSniperRangeTacticalSpot(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetFarRangeTacticalSpot()
{
    return self->ai->botRef->tacticalSpotsCache.GetFarRangeTacticalSpot(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetMiddleRangeTacticalSpot()
{
    return self->ai->botRef->tacticalSpotsCache.GetMiddleRangeTacticalSpot(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetCloseRangeTacticalSpot()
{
    return self->ai->botRef->tacticalSpotsCache.GetCloseRangeTacticalSpot(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetCoverSpot()
{
    return self->ai->botRef->tacticalSpotsCache.GetCoverSpot(BotOriginData(), EnemyOriginData());
}

inline void BotGutsActionsAccessor::SetNavTarget(NavTarget *navTarget)
{
    ent->ai->botRef->botBrain.SetNavTarget(navTarget);
}

inline void BotGutsActionsAccessor::ResetNavTarget()
{
    ent->ai->botRef->botBrain.ResetNavTarget();
}

inline const SelectedEnemies &BotGutsActionsAccessor::Enemies() const
{
    return ent->ai->botRef->selectedEnemies;
}

inline const SelectedWeapons &BotGutsActionsAccessor::Weapons() const
{
    return ent->ai->botRef->selectedWeapons;
}

inline SelectedTactics &BotGutsActionsAccessor::Tactics()
{
    return ent->ai->botRef->botBrain.selectedTactics;
}

inline const SelectedTactics &BotGutsActionsAccessor::Tactics() const
{
    return ent->ai->botRef->botBrain.selectedTactics;
}

inline const SelectedNavEntity &BotGutsActionsAccessor::GetGoalNavEntity() const
{
    return ent->ai->botRef->botBrain.GetOrUpdateSelectedNavEntity();
}

inline void BotGutsActionsAccessor::SetCampingSpotWithoutDirection(const Vec3 &spotOrigin, float spotRadius)
{
    ent->ai->botRef->campingSpotState.SetWithoutDirection(spotOrigin, spotRadius, 0.5f);
}

inline void BotGutsActionsAccessor::SetDirectionalCampingSpot(const Vec3 &spotOrigin, const Vec3 &lookAtPoint,
                                                              float spotRadius)
{
    ent->ai->botRef->campingSpotState.SetDirectional(spotOrigin, lookAtPoint, spotRadius, 0.5f);
}

inline void BotGutsActionsAccessor::InvalidateCampingSpot()
{
    ent->ai->botRef->campingSpotState.Invalidate();
}

int BotGutsActionsAccessor::TravelTimeMillis(const Vec3& from, const Vec3 &to)
{
    const AiAasWorld *aasWorld = AiAasWorld::Instance();

    // We try to use the same checks the TacticalSpotsRegistry performs to find spots.
    // If a spot is not reachable, it is an bug,
    // because a reachability must have been checked by the spots registry first in a few preceeding calls.

    int fromAreaNum;
    constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((from - ent->s.origin).SquaredLength() < squareDistanceError)
        fromAreaNum = aasWorld->FindAreaNum(ent);
    else
        fromAreaNum = aasWorld->FindAreaNum(from);

    if (!fromAreaNum)
        AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find `from` AAS area");

    const int toAreaNum = aasWorld->FindAreaNum(to.Data());
    if (!toAreaNum)
        AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find `to` AAS area");

    AiAasRouteCache *routeCache = ent->ai->botRef->botBrain.RouteCache();
    for (int flags: { ent->ai->botRef->PreferredTravelFlags(), ent->ai->botRef->AllowedTravelFlags() })
    {
        if (int aasTravelTime = routeCache->TravelTimeToGoalArea(fromAreaNum, toAreaNum, flags))
            return 10U * aasTravelTime;
    }

    AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find travel time %d->%d\n", fromAreaNum, toAreaNum);
}

void BotGenericRunActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    SetNavTarget(&navTarget);
}

void BotGenericRunActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    ResetNavTarget();
    Tactics().Clear();
}

constexpr float GOAL_PICKUP_ACTION_RADIUS = 72.0f;
constexpr float TACTICAL_SPOT_RADIUS = 40.0f;

AiBaseActionRecord::Status BotGenericRunActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    const auto &selectedNavEntity = GetGoalNavEntity();
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
    if (currWorldState.DistanceToGoalItem() <= GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Nav target pickup distance has been reached\n");
        return COMPLETED;
    }

    return VALID;
}

PlannerNode *BotGenericRunAction::TryApply(const WorldState &worldState)
{
    if (worldState.GoalItemOriginVar().Ignore())
    {
        Debug("Goal item is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItem() <= GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Distance to goal item nav target is too low in the given world state\n");
        return nullptr;
    }

    constexpr float roundingSquareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((worldState.BotOriginVar().Value() - self->s.origin).SquaredLength() > roundingSquareDistanceError)
    {
        Debug("Selected goal item is valid only for current bot origin\n");
        return nullptr;
    }

    const auto &itemNavEntity = GetGoalNavEntity();

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, itemNavEntity.GetNavEntity()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = itemNavEntity.GetCost();

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().BotOriginVar().SetValue(itemNavEntity.GetNavEntity()->Origin());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().ResetTacticalSpots();

    return plannerNode.PrepareActionResult();
}

void BotPickupItemActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    Tactics().shouldMoveCarefully = true;
    SetNavTarget(&navTarget);
}

void BotPickupItemActionRecord::Deactivate()
{
    AiBaseActionRecord::Activate();
    ResetNavTarget();
    Tactics().Clear();
}

AiBaseActionRecord::Status BotPickupItemActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustPickedGoalItemVar())
    {
        Debug("Goal item has been just picked up\n");
        return COMPLETED;
    }

    const SelectedNavEntity &currSelectedNavEntity = GetGoalNavEntity();
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
    if (currWorldState.DistanceToGoalItem() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("The nav target is too far from the bot to pickup it\n");
        return INVALID;
    }
    if (currWorldState.HasThreateningEnemyVar())
    {
        Debug("The bot has threatening enemy\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotPickupItemAction::TryApply(const WorldState &worldState)
{
    if (worldState.GoalItemOriginVar().Ignore())
    {
        Debug("Goal item nav target is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItem() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Distance to goal item nav target is too large to pick up an item in the given world state\n");
        return nullptr;
    }

    if (worldState.HasJustPickedGoalItemVar().Ignore())
    {
        Debug("Has bot picked a goal item is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustPickedGoalItemVar())
    {
        Debug("Bot has just picked a goal item in the given world state\n");
        return nullptr;
    }

    if (worldState.GoalItemWaitTimeVar().Ignore())
    {
        Debug("Goal item wait time is not specified in the given world state\n");
        return nullptr;
    }
    if (worldState.GoalItemWaitTimeVar() > 0)
    {
        Debug("Goal item wait time is non-zero in the given world state\n");
        return nullptr;
    }

    const auto &itemNavEntity = GetGoalNavEntity();
    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, itemNavEntity.GetNavEntity()));
    if (!plannerNode)
        return nullptr;

    // Picking up an item costs almost nothing
    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustPickedGoalItemVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().BotOriginVar().SetValue(itemNavEntity.GetNavEntity()->Origin());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, 12.0f);
    plannerNode.WorldState().ResetTacticalSpots();

    return plannerNode.PrepareActionResult();
}

void BotWaitForItemActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
    Tactics().Clear();
    SetCampingSpotWithoutDirection(navTarget.Origin(), 0.66f * GOAL_PICKUP_ACTION_RADIUS);
}

void BotWaitForItemActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    InvalidateCampingSpot();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotWaitForItemActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustPickedGoalItemVar())
    {
        Debug("Goal item has been just picked up\n");
        return COMPLETED;
    }

    const auto &currSelectedNavEntity = GetGoalNavEntity();
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
    if (currWorldState.DistanceToGoalItem() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Distance to the item is too large to wait for it\n");
        return INVALID;
    }
    if (currWorldState.HasThreateningEnemyVar())
    {
        Debug("The bot has a threatening enemy\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotWaitForItemAction::TryApply(const WorldState &worldState)
{
    if (worldState.GoalItemOriginVar().Ignore())
    {
        Debug("Goal item nav target is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToGoalItem() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Distance to goal item nav target is too large to wait for an item in the given world state\n");
        return nullptr;
    }

    if (worldState.HasJustPickedGoalItemVar().Ignore())
    {
        Debug("Has bot picked a goal item is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustPickedGoalItemVar())
    {
        Debug("Bot has just picked a goal item in the given world state\n");
        return nullptr;
    }

    if (worldState.GoalItemWaitTimeVar().Ignore())
    {
        Debug("Goal item wait time is not specified in the given world state\n");
        return nullptr;
    }
    if (worldState.GoalItemWaitTimeVar() == 0)
    {
        Debug("Goal item wait time is zero in the given world state\n");
        return nullptr;
    }

    const auto &itemNavEntity = GetGoalNavEntity();
    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, itemNavEntity.GetNavEntity()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = worldState.GoalItemWaitTimeVar();

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustPickedGoalItemVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().BotOriginVar().SetValue(itemNavEntity.GetNavEntity()->Origin());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, 12.0f);
    plannerNode.WorldState().ResetTacticalSpots();

    return plannerNode.PrepareActionResult();
}

bool BotCombatActionRecord::CheckCommonCombatConditions(const WorldState &currWorldState) const
{
    if (currWorldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is not specified\n");
        return false;
    }
    if (Enemies().InstanceId() != selectedEnemiesInstanceId)
    {
        Debug("New enemies have been selected\n");
        return false;
    }
    return true;
}

void BotAdvanceToGoodPositionActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    // Set a hint for weapon selection
    Tactics().willAdvance = true;
    SetNavTarget(&navTarget);
}

void BotAdvanceToGoodPositionActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotAdvanceToGoodPositionActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (!CheckCommonCombatConditions(currWorldState))
        return INVALID;

    if ((navTarget.Origin() - self->s.origin).SquaredLength() < TACTICAL_SPOT_RADIUS * TACTICAL_SPOT_RADIUS)
        return COMPLETED;

    return VALID;
}

PlannerNode *BotAdvanceToGoodPositionAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return nullptr;
    }

    const float offensiveness = self->ai->botRef->GetEffectiveOffensiveness();
    Vec3 spotOrigin(0, 0, 0);
    if (worldState.EnemyIsOnSniperRange())
    {
        if (!worldState.HasGoodFarRangeWeaponsVar())
        {
            Debug("Bot doesn't have good far range weapons\n");
            return nullptr;
        }

        if (!worldState.HasGoodSniperRangeWeaponsVar())
        {
            Debug("Bot doesn't have good sniper range weapons and thus can't advance attacking\n");
            return nullptr;
        }

        if (worldState.HasThreateningEnemyVar())
        {
            float minDamageToBeKilled = 75.0f * (1.0f - offensiveness);
            if (worldState.DamageToBeKilled() < minDamageToBeKilled)
                return nullptr;

            if (worldState.EnemyHasGoodFarRangeWeaponsVar())
            {
                if (worldState.KillToBeKilledDamageRatio() > 1.3f)
                    return nullptr;
            }
            else
            {
                if (worldState.KillToBeKilledDamageRatio() > 1.7f)
                    return nullptr;
            }
        }
        else
        {
            if (worldState.EnemyHasGoodFarRangeWeaponsVar())
            {
                if (worldState.KillToBeKilledDamageRatio() > 1.7f)
                    return nullptr;
            }
            else
            {
                float minDamageToBeKilled = 40.0f * (1.0f - offensiveness);
                if (worldState.DamageToBeKilled() < minDamageToBeKilled)
                    return nullptr;
            }
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.FarRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Far range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.FarRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.HasGoodMiddleRangeWeaponsVar())
        {
            Debug("Bot doesn't have good middle range weapons\n");
            return nullptr;
        }

        if (!worldState.HasGoodFarRangeWeaponsVar())
        {
            Debug("Bot doesn't have good far range weapons and thus can't advance attacking\n");
            return nullptr;
        }

        if (worldState.HasThreateningEnemyVar())
        {
            if (worldState.EnemyHasGoodMiddleRangeWeaponsVar())
            {
                if (worldState.KillToBeKilledDamageRatio() > 0.8f + 0.5f * offensiveness)
                    return nullptr;
            }
            else
            {
                float minDamageToBeKilled = 100.0f * (1.0f - offensiveness);
                if (worldState.DamageToBeKilled() < minDamageToBeKilled)
                    return nullptr;
            }
        }
        else
        {
            float minDamageToBeKilled = 75.0f * (1.0f - offensiveness);
            if (worldState.DamageToBeKilled() < minDamageToBeKilled)
                return nullptr;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.MiddleRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Middle range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.MiddleRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.HasGoodCloseRangeWeaponsVar())
        {
            Debug("Bot doesn't have good close range weapons in the given world state\n");
            return nullptr;
        }

        if (!worldState.HasGoodMiddleRangeWeaponsVar())
        {
            Debug("Bot doesn't have good middle range weapons and thus can't advance attacking\n");
            return nullptr;
        }

        float damageRatio = worldState.KillToBeKilledDamageRatio();
        if (worldState.HasThreateningEnemyVar())
        {
            float minDamageToBeKilled = 75.0f * (1.0f - offensiveness);
            if (worldState.DamageToBeKilled() < minDamageToBeKilled)
                return nullptr;

            if (damageRatio > (worldState.EnemyHasGoodCloseRangeWeaponsVar() ? 1.0f : 0.7f))
                return nullptr;
        }
        else
        {
            if (damageRatio > (worldState.EnemyHasGoodCloseRangeWeaponsVar() ? 1.1f : 0.6f))
                return nullptr;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.CloseRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Close range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.CloseRangeTacticalSpotVar().Value();
    }
    else
    {
        Debug("Advancing on a close range does not make sense\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, spotOrigin, Enemies().InstanceId()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = TravelTimeMillis(worldState.BotOriginVar().Value(), spotOrigin);
    plannerNode.WorldState() = worldState;

    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin).SetSatisfyOp(SatisfyOp::EQ, TACTICAL_SPOT_RADIUS);
    plannerNode.WorldState().ResetTacticalSpots();

    // Satisfy conditions for BotKillEnemyGoal
    plannerNode.WorldState().CanHitEnemyVar().SetValue(true);
    plannerNode.WorldState().HasPositionalAdvantageVar().SetValue(true);

    return plannerNode.PrepareActionResult();
}

void BotRetreatToGoodPositionActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    // Set a hint for weapon selection
    Tactics().willRetreat = true;
    SetNavTarget(&navTarget);
}

void BotRetreatToGoodPositionActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotRetreatToGoodPositionActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (!CheckCommonCombatConditions(currWorldState))
        return INVALID;

    if ((navTarget.Origin() - self->s.origin).SquaredLength() < TACTICAL_SPOT_RADIUS * TACTICAL_SPOT_RADIUS)
        return COMPLETED;

    return VALID;
}

PlannerNode *BotRetreatToGoodPositionAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return nullptr;
    }

    Vec3 spotOrigin(0, 0, 0);
    if (worldState.EnemyIsOnSniperRange())
    {
        Debug("Retreating on sniper range does not make sense\n");
        return nullptr;
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.HasThreateningEnemyVar())
        {
            Debug("There is no threatening enemy, and thus retreating on far range does not make sense\n");
            return nullptr;
        }

        if (!worldState.HasGoodSniperRangeWeaponsVar())
        {
            Debug("Bot doesn't have good sniper range weapons\n");
            return nullptr;
        }

        if (worldState.HasGoodFarRangeWeaponsVar())
        {
            if (worldState.EnemyHasGoodFarRangeWeaponsVar())
            {
                if (worldState.KillToBeKilledDamageRatio() < 1)
                    return nullptr;
            }
            else
            {
                if (worldState.KillToBeKilledDamageRatio() < 1.5)
                    return nullptr;
            }
        }
        else
        {
            if (!worldState.EnemyHasGoodFarRangeWeaponsVar())
                return nullptr;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.SniperRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Sniper range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.SniperRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.HasGoodFarRangeWeaponsVar())
        {
            Debug("Bot doesn't have good far range weapons\n");
            return nullptr;
        }

        if (!worldState.HasThreateningEnemyVar())
        {
            if (worldState.HasGoodMiddleRangeWeaponsVar())
            {
                Debug("Bot has good middle range weapons and the enemy is not threatening\n");
                return nullptr;
            }
        }
        else
        {
            if (worldState.HasGoodMiddleRangeWeaponsVar())
            {
                if (worldState.EnemyHasGoodMiddleRangeWeaponsVar())
                {
                    if (worldState.KillToBeKilledDamageRatio() < 0.9)
                        return nullptr;
                }
                else
                {
                    if (worldState.KillToBeKilledDamageRatio() < 1.2)
                        return nullptr;
                }
            }
            else
            {
                if (!worldState.HasGoodMiddleRangeWeaponsVar())
                    return nullptr;
            }
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.FarRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Far range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.FarRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnCloseRange())
    {
        if (worldState.MiddleRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Middle range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.MiddleRangeTacticalSpotVar().Value();
    }

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, spotOrigin, Enemies().InstanceId()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = TravelTimeMillis(worldState.BotOriginVar().Value(), spotOrigin);
    plannerNode.WorldState() = worldState;

    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin).SetSatisfyOp(SatisfyOp::EQ, TACTICAL_SPOT_RADIUS);
    plannerNode.WorldState().ResetTacticalSpots();

    // Satisfy conditions for BotKillEnemyGoal
    plannerNode.WorldState().CanHitEnemyVar().SetValue(true);
    plannerNode.WorldState().HasPositionalAdvantageVar().SetValue(true);

    return plannerNode.PrepareActionResult();
}

void BotSteadyCombatActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    SetNavTarget(&navTarget);
}

void BotSteadyCombatActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotSteadyCombatActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (!CheckCommonCombatConditions(currWorldState))
        return INVALID;

    // Never return COMPLETED value (otherwise the dummy following BotKillEnemyAction may be activated leading to a crash).
    // This action often gets actually deactivated on replanning.

    // Bot often moves out of TACTICAL_SPOT_RADIUS during "camping a spot" movement
    constexpr float invalidationRadius = 1.5f * TACTICAL_SPOT_RADIUS;
    if ((navTarget.Origin() - self->s.origin).SquaredLength() > invalidationRadius * invalidationRadius)
        return INVALID;

    return VALID;
}

PlannerNode *BotSteadyCombatAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return nullptr;
    }

    Vec3 spotOrigin(0, 0, 0);
    float actionPenalty = 0.0f;
    const float offensiveness = self->ai->botRef->GetEffectiveOffensiveness();
    if (worldState.EnemyIsOnSniperRange())
    {
        if (!worldState.HasGoodSniperRangeWeaponsVar())
            return nullptr;

        if (worldState.EnemyHasGoodSniperRangeWeaponsVar())
        {
            if (worldState.DamageToBeKilled() < 80.0f && worldState.KillToBeKilledDamageRatio() > 1)
                return nullptr;
        }
        else
        {
            if (worldState.HasGoodFarRangeWeaponsVar())
                actionPenalty += 0.5f * offensiveness;
            if (worldState.HasGoodMiddleRangeWeaponsVar())
                actionPenalty += 0.5f * offensiveness;
            if (worldState.HasGoodCloseRangeWeaponsVar())
                actionPenalty += 0.5f * offensiveness;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.SniperRangeTacticalSpotVar().IgnoreOrAbsent())
            return nullptr;

        if (worldState.DistanceToSniperRangeTacticalSpot() > 64.0f)
            return nullptr;

        spotOrigin = worldState.SniperRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.HasGoodFarRangeWeaponsVar())
            return nullptr;

        if (worldState.EnemyHasGoodFarRangeWeaponsVar())
        {
            if (worldState.KillToBeKilledDamageRatio() > 1.5f)
                return nullptr;
        }
        else
        {
            if (worldState.HasGoodMiddleRangeWeaponsVar())
                actionPenalty += 0.5f * offensiveness;
            if (worldState.HasGoodCloseRangeWeaponsVar())
                actionPenalty += 0.5f * offensiveness;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.FarRangeTacticalSpotVar().IgnoreOrAbsent())
            return nullptr;

        if (worldState.DistanceToFarRangeTacticalSpot() > 64.0f)
            return nullptr;

        spotOrigin = worldState.FarRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.HasGoodMiddleRangeWeaponsVar())
            return nullptr;

        if (worldState.EnemyHasGoodMiddleRangeWeaponsVar())
        {
            if (worldState.KillToBeKilledDamageRatio() > 1)
                return nullptr;
        }
        else
        {
            if (worldState.HasGoodCloseRangeWeaponsVar())
                actionPenalty += 1.0f * offensiveness;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.MiddleRangeTacticalSpotVar().IgnoreOrAbsent())
            return nullptr;

        if (worldState.DistanceToMiddleRangeTacticalSpot() > 64.0f)
            return nullptr;

        spotOrigin = worldState.MiddleRangeTacticalSpotVar().Value();
    }
    else
    {
        if (!worldState.HasGoodCloseRangeWeaponsVar())
        {
            Debug("Bot does not have good close range weapons\n");
            return nullptr;
        }
        if (worldState.EnemyHasGoodCloseRangeWeaponsVar())
        {
            if (worldState.KillToBeKilledDamageRatio() > 0.8f)
                return nullptr;
        }
        else
        {
            if (worldState.KillToBeKilledDamageRatio() > 1.3f)
                return nullptr;
        }

        // Put this condition last to avoid forcing tactical spot to be computed.
        // Test cheap conditions first for early action rejection.
        if (worldState.CloseRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Close range tactical spot is ignored or is absent\n");
            return nullptr;
        }
        if (worldState.DistanceToCloseRangeTacticalSpot() > 48.0f)
        {
            Debug("Bot is already on the close range tactical spot\n");
            return nullptr;
        }

        spotOrigin = worldState.CloseRangeTacticalSpotVar().Value();
    }

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, spotOrigin, Enemies().InstanceId()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = (1.0f + actionPenalty) * worldState.DamageToKill();

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin);
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(WorldState::SatisfyOp::EQ, TACTICAL_SPOT_RADIUS);
    // These vars should be lazily recomputed for the modified bot origin
    plannerNode.WorldState().ResetTacticalSpots();

    // Satisfy conditions for BotKillEnemyGoal
    plannerNode.WorldState().CanHitEnemyVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().HasPositionalAdvantageVar().SetValue(true).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotGotoAvailableGoodPositionActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    // Enable attacking if view angles fit
    Tactics().shouldAttack = true;
    // Do not try to track enemy by crosshair, adjust view for movement
    // TODO: Bot should try if movement is not affected significantly (e.g while flying with released keys)
    Tactics().shouldKeepXhairOnEnemy = false;
    SetNavTarget(&navTarget);
}

void BotGotoAvailableGoodPositionActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotGotoAvailableGoodPositionActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (!CheckCommonCombatConditions(currWorldState))
        return INVALID;

    if ((navTarget.Origin() - self->s.origin).SquaredLength() < TACTICAL_SPOT_RADIUS * TACTICAL_SPOT_RADIUS)
        return COMPLETED;

    return VALID;
}

PlannerNode *BotGotoAvailableGoodPositionAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return nullptr;
    }

    // Don't check whether enemy is threatening.
    // (Use any chance to get a good position)

    Vec3 spotOrigin(0, 0, 0);
    if (worldState.EnemyIsOnSniperRange())
    {
        if (!worldState.HasGoodSniperRangeWeaponsVar())
        {
            Debug("Bot does not have good sniper range weapons\n");
            return nullptr;
        }
        if (worldState.SniperRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Sniper range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }
        if (worldState.HasThreateningEnemyVar())
        {
            if (worldState.DamageToBeKilled() < 80 && worldState.EnemyHasGoodSniperRangeWeaponsVar())
                return nullptr;
        }
        spotOrigin = worldState.SniperRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.HasGoodFarRangeWeaponsVar())
        {
            Debug("Bot does not have good far range weapons\n");
            return nullptr;
        }
        if (worldState.FarRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Far range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }
        if (worldState.HasThreateningEnemyVar())
        {
            if (worldState.DamageToBeKilled() < 80 && worldState.EnemyHasGoodSniperRangeWeaponsVar())
                return nullptr;
        }
        spotOrigin = worldState.FarRangeTacticalSpotVar().Value();
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.HasGoodMiddleRangeWeaponsVar())
        {
            Debug("Bot does not have good middle range weapons\n");
            return nullptr;
        }
        if (worldState.HasThreateningEnemyVar())
        {
            if (worldState.KillToBeKilledDamageRatio() > (worldState.EnemyHasGoodMiddleRangeWeaponsVar() ? 1.2f : 1.5f))
                return nullptr;
        }
        if (worldState.MiddleRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Middle range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }

        spotOrigin = worldState.MiddleRangeTacticalSpotVar().Value();
    }
    else
    {
        if (!worldState.HasGoodCloseRangeWeaponsVar())
        {
            Debug("Bot does not have good close range weapons\n");
            return nullptr;
        }
        if (worldState.HasThreateningEnemyVar())
        {
            if (worldState.KillToBeKilledDamageRatio() > (worldState.EnemyHasGoodCloseRangeWeaponsVar() ? 1.1f : 1.5f))
                return nullptr;
        }
        if (worldState.CloseRangeTacticalSpotVar().IgnoreOrAbsent())
        {
            Debug("Close range tactical spot is ignored or absent in the given world state\n");
            return nullptr;
        }
        spotOrigin = worldState.CloseRangeTacticalSpotVar().Value();
    }

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, spotOrigin, Enemies().InstanceId()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = TravelTimeMillis(worldState.BotOriginVar().Value(), spotOrigin);

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin);
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, TACTICAL_SPOT_RADIUS);
    plannerNode.WorldState().ResetTacticalSpots();
    // Satisfy conditions for BotKillEnemyGoal
    plannerNode.WorldState().CanHitEnemyVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().HasPositionalAdvantageVar().SetValue(true).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

PlannerNode *BotKillEnemyAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasPositionalAdvantageVar().Ignore())
    {
        Debug("Has bot positional advantage is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasPositionalAdvantageVar())
    {
        Debug("Bot does not have positional advantage in the given world state\n");
        return nullptr;
    }
    if (worldState.CanHitEnemyVar().Ignore())
    {
        Debug("Can bot hit enemy is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.CanHitEnemyVar())
    {
        Debug("Bot can't hit enemy in the given world state\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self));
    if (!plannerNode)
    {
        Debug("Can't allocate planner node\n");
        return nullptr;
    }

    // Set low dummy cost
    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustKilledEnemyVar().SetValue(true).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotRetreatToCoverActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
    // Enable attacking if view angles fit
    Tactics().shouldAttack = true;
    // Do not try to track enemy by crosshair, adjust view for movement
    Tactics().shouldKeepXhairOnEnemy = false;
    // Set a hint for weapon selection
    Tactics().willRetreat = true;
    SetNavTarget(&navTarget);
}

void BotRetreatToCoverActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotRetreatToCoverActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasThreateningEnemyVar().Ignore())
    {
        Debug("A threatening enemy is ignored\n");
        return INVALID;
    }

    // An enemy is still threatening the bot
    if (currWorldState.HasThreateningEnemyVar())
    {
        if (Enemies().InstanceId() != selectedEnemiesInstanceId)
        {
            Debug("New enemies have been selected\n");
            return INVALID;
        }

        // Check whether there is no cover spot anymore.
        // Note: this forces recomputation of an acutal cover spot
        if (currWorldState.CoverSpotVar().IgnoreOrAbsent())
        {
            Debug("Cover spot is ignored or absent\n");
            return INVALID;
        }

        // Cover spot is present and it is different from the nav target
        if ((currWorldState.CoverSpotVar().Value() - navTarget.Origin()).SquaredLength() > 8.0f * 8.0f)
        {
            Debug("Actual cover spot has been changed\n");
            return INVALID;
        }

        if (currWorldState.DistanceToCoverSpot() > 32.0f)
            return VALID;

        return COMPLETED;
    }

    // Continue moving to the nav target. Do not force recomputation of new cover spot.
    if ((currWorldState.BotOriginVar().Value() - navTarget.Origin()).LengthFast() > 32.0f)
        return VALID;

    return COMPLETED;
}

PlannerNode *BotRetreatToCoverAction::TryApply(const WorldState &worldState)
{
    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.EnemyIsOnSniperRange())
    {
        if (!worldState.EnemyHasGoodSniperRangeWeaponsVar())
        {
            Debug("Enemy does not have good sniper range weapons and thus taking cover makes no sense\n");
            return nullptr;
        }
        if (worldState.DamageToBeKilled() > 80)
        {
            Debug("Bot can resist more than 80 damage units on sniper range and thus taking cover makes no sense\n");
            return nullptr;
        }
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.EnemyHasGoodFarRangeWeaponsVar())
        {
            Debug("Enemy does not have good far range weapons and thus taking cover makes no sense\n");
            return nullptr;
        }
        if (worldState.DamageToBeKilled() > 80)
        {
            Debug("Bot can resist more than 80 damage units on far range and thus taking cover makes no sense\n");
            return nullptr;
        }
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.EnemyHasGoodMiddleRangeWeaponsVar())
        {
            if (worldState.HasGoodMiddleRangeWeaponsVar())
            {
                if (worldState.DamageToBeKilled() > 50)
                    return nullptr;
                if (worldState.KillToBeKilledDamageRatio() < 1.3f)
                    return nullptr;
            }
            else
            {
                if (worldState.KillToBeKilledDamageRatio() < 1)
                    return nullptr;
                if (worldState.DamageToBeKilled() > 100)
                    return nullptr;
            }
        }
    }
    else
    {
        if (!worldState.HasGoodCloseRangeWeaponsVar())
        {
            if (!worldState.EnemyHasGoodCloseRangeWeaponsVar())
            {
                Debug("Bot and enemy both do not have good close range weapons\n");
                return nullptr;
            }
        }
        else
        {
            float damageRatio = worldState.KillToBeKilledDamageRatio();
            if (damageRatio < (worldState.EnemyHasGoodCloseRangeWeaponsVar() ? 0.8f : 1.2f))
            {
                Debug("Enemy has %f x weaker HP than bot\n", damageRatio);
                return nullptr;
            }
        }
    }

    if (worldState.CoverSpotVar().Ignore())
    {
        Debug("Cover spot is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.CoverSpotVar().IsPresent())
    {
        Debug("Cover spot is absent in the given world state\n");
        return nullptr;
    }

    Vec3 spotOrigin = worldState.CoverSpotVar().Value();
    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self, spotOrigin, Enemies().InstanceId()));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = TravelTimeMillis(worldState.BotOriginVar().Value(), spotOrigin);

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().EnemyCanHitVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin).SetSatisfyOp(SatisfyOp::EQ, TACTICAL_SPOT_RADIUS);
    plannerNode.WorldState().ResetTacticalSpots();

    return plannerNode.PrepareActionResult();
}
