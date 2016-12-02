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

const short *WorldState::GetRunAwayTeleportOrigin()
{
    return self->ai->botRef->tacticalSpotsCache.GetRunAwayTeleportOrigin(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetRunAwayJumppadOrigin()
{
    return self->ai->botRef->tacticalSpotsCache.GetRunAwayJumppadOrigin(BotOriginData(), EnemyOriginData());
}

const short *WorldState::GetRunAwayElevatorOrigin()
{
    return self->ai->botRef->tacticalSpotsCache.GetRunAwayElevatorOrigin(BotOriginData(), EnemyOriginData());
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

inline void BotGutsActionsAccessor::SetPendingLookAtPoint(const Vec3 &lookAtPoint,
                                                          float turnSpeedMultiplier,
                                                          unsigned timeoutPeriod)
{
    ent->ai->botRef->pendingLookAtPointState.SetTriggered(lookAtPoint, turnSpeedMultiplier, timeoutPeriod);
}

inline bool BotGutsActionsAccessor::IsPendingLookAtPointValid() const
{
    return ent->ai->botRef->pendingLookAtPointState.IsActive();
}

inline void BotGutsActionsAccessor::InvalidatePendingLookAtPoint()
{
    ent->ai->botRef->pendingLookAtPointState.timeoutAt = 0;
}

int BotGutsActionsAccessor::TravelTimeMillis(const Vec3& from, const Vec3 &to, bool allowUnreachable)
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
    {
        if (allowUnreachable)
            return 0;

        AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find `from` AAS area");
    }

    const int toAreaNum = aasWorld->FindAreaNum(to.Data());
    if (!toAreaNum)
    {
        if (allowUnreachable)
            return 0;

        AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find `to` AAS area");
    }

    AiAasRouteCache *routeCache = ent->ai->botRef->botBrain.RouteCache();
    for (int flags: { ent->ai->botRef->PreferredTravelFlags(), ent->ai->botRef->AllowedTravelFlags() })
    {
        if (int aasTravelTime = routeCache->TravelTimeToGoalArea(fromAreaNum, toAreaNum, flags))
            return 10U * aasTravelTime;
    }

    if (allowUnreachable)
        return 0;

    AI_FailWith("BotGutsActionsAccessor::TravelTimeMillis()", "Can't find travel time %d->%d\n", fromAreaNum, toAreaNum);
}

inline unsigned BotGutsActionsAccessor::NextSimilarWorldStateInstanceId()
{
    return ent->ai->botRef->NextSimilarWorldStateInstanceId();
}

void BotBaseActionRecord::Activate()
{
    AiBaseActionRecord::Activate();
    Tactics().Clear();
}

void BotBaseActionRecord::Deactivate()
{
    AiBaseActionRecord::Deactivate();
    Tactics().Clear();
}

void BotGenericRunToItemActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
}

void BotGenericRunToItemActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

constexpr float GOAL_PICKUP_ACTION_RADIUS = 72.0f;
constexpr float TACTICAL_SPOT_RADIUS = 40.0f;

AiBaseActionRecord::Status BotGenericRunToItemActionRecord::CheckStatus(const WorldState &currWorldState) const
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
    if (currWorldState.DistanceToNavTarget() <= GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Nav target pickup distance has been reached\n");
        return COMPLETED;
    }

    return VALID;
}

PlannerNode *BotGenericRunToItemAction::TryApply(const WorldState &worldState)
{
    if (worldState.GoalItemWaitTimeVar().Ignore())
    {
        Debug("Goal item is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.NavTargetOriginVar().Ignore())
    {
        Debug("Nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.DistanceToNavTarget() <= GOAL_PICKUP_ACTION_RADIUS)
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
    BotBaseActionRecord::Activate();
    Tactics().shouldMoveCarefully = true;
    SetNavTarget(&navTarget);
}

void BotPickupItemActionRecord::Deactivate()
{
    BotBaseActionRecord::Activate();
    ResetNavTarget();
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
    if (currWorldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
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
    if (worldState.GoalItemWaitTimeVar().Ignore())
    {
        Debug("Goal item is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
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
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
    SetCampingSpotWithoutDirection(navTarget.Origin(), 0.66f * GOAL_PICKUP_ACTION_RADIUS);
}

void BotWaitForItemActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    InvalidateCampingSpot();
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
    if (currWorldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
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
    if (worldState.NavTargetOriginVar().Ignore())
    {
        Debug("Nav target is ignored in the given world state\n");
        return nullptr;
    }

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
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
    BotBaseActionRecord::Activate();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    // Set a hint for weapon selection
    Tactics().willAdvance = true;
    SetNavTarget(&navTarget);
}

void BotAdvanceToGoodPositionActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
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
    BotBaseActionRecord::Activate();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    // Set a hint for weapon selection
    Tactics().willRetreat = true;
    SetNavTarget(&navTarget);
}

void BotRetreatToGoodPositionActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
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
    BotBaseActionRecord::Activate();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    SetNavTarget(&navTarget);
}

void BotSteadyCombatActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
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
    BotBaseActionRecord::Activate();
    // Enable attacking if view angles fit
    Tactics().shouldAttack = true;
    // Do not try to track enemy by crosshair, adjust view for movement
    // TODO: Bot should try if movement is not affected significantly (e.g while flying with released keys)
    Tactics().shouldKeepXhairOnEnemy = false;
    SetNavTarget(&navTarget);
}

void BotGotoAvailableGoodPositionActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
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

    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetValue(NextSimilarWorldStateInstanceId());
    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

bool BotRunAwayAction::CheckCommonRunAwayPreconditions(const WorldState &worldState) const
{
    if (!worldState.HasRunAwayVar().Ignore() && worldState.HasRunAwayVar())
    {
        Debug("Bot has already run away in the given world state\n");
        return false;
    }
    if (!worldState.IsRunningAwayVar().Ignore() && worldState.IsRunningAwayVar())
    {
        Debug("Bot is already running away in the given world state\n");
        return false;
    }

    if (worldState.EnemyOriginVar().Ignore())
    {
        Debug("Enemy is ignored in the given world state\n");
        return false;
    }
    if (worldState.HealthVar().Ignore() || worldState.ArmorVar().Ignore())
    {
        Debug("Health or armor are ignored in the given world state\n");
        return false;
    }

    if (worldState.EnemyIsOnSniperRange())
    {
        if (!worldState.EnemyHasGoodSniperRangeWeaponsVar())
        {
            Debug("Enemy does not have good sniper range weapons and thus taking cover makes no sense\n");
            return false;
        }
        if (worldState.DamageToBeKilled() > 80)
        {
            Debug("Bot can resist more than 80 damage units on sniper range and thus taking cover makes no sense\n");
            return false;
        }
    }
    else if (worldState.EnemyIsOnFarRange())
    {
        if (!worldState.EnemyHasGoodFarRangeWeaponsVar())
        {
            Debug("Enemy does not have good far range weapons and thus taking cover makes no sense\n");
            return false;
        }
        if (worldState.DamageToBeKilled() > 80)
        {
            Debug("Bot can resist more than 80 damage units on far range and thus taking cover makes no sense\n");
            return false;
        }
    }
    else if (worldState.EnemyIsOnMiddleRange())
    {
        if (!worldState.EnemyHasGoodMiddleRangeWeaponsVar())
        {
            if (worldState.HasGoodMiddleRangeWeaponsVar())
            {
                if (worldState.DamageToBeKilled() > 50)
                    return false;
                if (worldState.KillToBeKilledDamageRatio() < 1.3f)
                    return false;
            }
            else
            {
                if (worldState.KillToBeKilledDamageRatio() < 1)
                    return false;
                if (worldState.DamageToBeKilled() > 100)
                    return false;
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
                return false;
            }
        }
        else
        {
            float damageRatio = worldState.KillToBeKilledDamageRatio();
            if (damageRatio < (worldState.EnemyHasGoodCloseRangeWeaponsVar() ? 0.8f : 1.2f))
            {
                Debug("Enemy has %f x weaker HP than bot\n", damageRatio);
                return false;
            }
        }
    }

    return true;
}

void BotGenericRunAvoidingCombatActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = false;
    SetNavTarget(&navTarget);
}

void BotGenericRunAvoidingCombatActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotGenericRunAvoidingCombatActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    // It really gets invalidated on goal reevaluation

    if ((navTarget.Origin() - self->s.origin).LengthFast() <= GOAL_PICKUP_ACTION_RADIUS)
        return COMPLETED;

    return VALID;
}

PlannerNode *BotGenericRunAvoidingCombatAction::TryApply(const WorldState &worldState)
{
    if (worldState.NavTargetOriginVar().Ignore())
    {
        Debug("Nav target is absent in the given world state\n");
        return nullptr;
    }
    if (worldState.DistanceToNavTarget() <= GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too close to the nav target\n");
        return nullptr;
    }

    const Vec3 navTargetOrigin = worldState.NavTargetOriginVar().Value();
    const SelectedNavEntity &selectedNavEntity = GetGoalNavEntity();
    if (selectedNavEntity.IsValid() && !selectedNavEntity.IsEmpty())
    {
        const Vec3 navEntityOrigin = selectedNavEntity.GetNavEntity()->Origin();
        constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
        if ((navEntityOrigin - navTargetOrigin).SquaredLength() < squareDistanceError)
        {
            Debug("Action is not applicable for goal entities (there are specialized actions for these kinds of nav target\n");
            return nullptr;
        }
    }

    // As a contrary to combat actions, illegal travel time (when the destination is not reachable for AAS) is allowed.
    // Combat actions require simple kinds of movement to keep crosshair on enemy.
    // Thus tactical spot should be reachable in common way for combat actions.
    // In case of retreating, some other kinds of movement AAS is not aware of might be used.
    int travelTimeMillis = TravelTimeMillis(worldState.BotOriginVar().Value(), navTargetOrigin, true);

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, navTargetOrigin)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = travelTimeMillis;

    plannerNode.WorldState() = worldState;
    // Move bot origin
    plannerNode.WorldState().BotOriginVar().SetValue(navTargetOrigin);
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().BotOriginVar().SetIgnore(false);
    // Since bot origin has been moved, tactical spots should be recomputed
    plannerNode.WorldState().ResetTacticalSpots();

    return plannerNode.PrepareActionResult();
}

PlannerNode *BotStartGotoCoverAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (!worldState.PendingOriginVar().Ignore())
    {
        Debug("Pending origin is already present in the given world state\n");
        return nullptr;
    }
#ifdef _DEBUG
    // Sanity check
    if (!worldState.HasPendingCoverSpotVar().Ignore() && worldState.HasPendingCoverSpotVar())
    {
        worldState.DebugPrint("Given WS");
        AI_FailWith(this->name, "Pending cover spot is already present in the given world state\n");
    }
#endif

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
    PlannerNodePtr plannerNode = NewNodeForRecord(pool.New(self));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasPendingCoverSpotVar().SetValue(true).SetIgnore(false);
    // Set nav target to the tactical spot
    plannerNode.WorldState().NavTargetOriginVar().SetValue(spotOrigin);
    plannerNode.WorldState().NavTargetOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().NavTargetOriginVar().SetIgnore(false);
    // Set pending origin to the tactical spot
    plannerNode.WorldState().PendingOriginVar().SetValue(spotOrigin);
    plannerNode.WorldState().PendingOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().PendingOriginVar().SetIgnore(false);

    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetValue(NextSimilarWorldStateInstanceId());
    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotTakeCoverActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
    SetNavTarget(&navTarget);
}

void BotTakeCoverActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotTakeCoverActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    static_assert(GOAL_PICKUP_ACTION_RADIUS > TACTICAL_SPOT_RADIUS, "");

    if (selectedEnemiesInstanceId != Enemies().InstanceId())
    {
        Debug("New enemies have been selected\n");
        return INVALID;
    }

    float distanceToActionNavTarget = (navTarget.Origin() - self->s.origin).SquaredLength();
    if (distanceToActionNavTarget > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from nav target\n");
        return INVALID;
    }

    return (distanceToActionNavTarget < TACTICAL_SPOT_RADIUS) ? COMPLETED : VALID;
}

PlannerNode *BotTakeCoverAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (worldState.NavTargetOriginVar().Ignore())
    {
        Debug("Nav target is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasPendingCoverSpotVar().Ignore() || !worldState.HasPendingCoverSpotVar())
    {
        Debug("Has bot pending cover spot is ignored or absent in the given world state\n");
        return nullptr;
    }

    const Vec3 navTargetOrigin = worldState.NavTargetOriginVar().Value();

#ifdef _DEBUG
    // Sanity check
    if (worldState.PendingOriginVar().Ignore())
    {
        worldState.DebugPrint("Given WS");
        AI_FailWith("BotTakeCoverAction", "PendingOriginVar() is ignored in the given world state\n");
    }

    constexpr float distanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((worldState.PendingOriginVar().Value() - navTargetOrigin).SquaredLength() > distanceError)
    {
        worldState.DebugPrint("Given WS");
        AI_FailWith("BotTakeCoverAction", "PendingOrigin and NavTargetOrigin differ in the given world state\n");
    }
#endif

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the nav target (pending cover spot)\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, navTargetOrigin, Enemies().InstanceId())));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasPendingCoverSpotVar().SetIgnore(true);
    plannerNode.WorldState().PendingOriginVar().SetIgnore(true);
    // Bot origin var remains the same (it is close to nav target)
    plannerNode.WorldState().IsRunningAwayVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().CanHitEnemyVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().EnemyCanHitVar().SetValue(false).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

PlannerNode *BotStartGotoRunAwayTeleportAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (worldState.HasJustTeleportedVar().Ignore())
    {
        Debug("Has bot just teleported is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustTeleportedVar())
    {
        Debug("Bot has just teleported in the given world state. Avoid chain teleportations\n");
        return nullptr;
    }
    if (!worldState.PendingOriginVar().Ignore())
    {
        Debug("The pending origin is already present in the given world state\n");
        return nullptr;
    }
    if (worldState.RunAwayTeleportOriginVar().IgnoreOrAbsent())
    {
        Debug("A teleport for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasPendingRunAwayTeleportVar().SetValue(true).SetIgnore(false);
    // Set nav target to the teleport origin
    plannerNode.WorldState().NavTargetOriginVar().SetValue(worldState.RunAwayTeleportOriginVar().Value());
    plannerNode.WorldState().NavTargetOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().NavTargetOriginVar().SetIgnore(false);
    // Set pending origin to the teleport destination
    plannerNode.WorldState().PendingOriginVar().SetValue(worldState.RunAwayTeleportOriginVar().Value2());
    plannerNode.WorldState().PendingOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().PendingOriginVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotDoRunAwayViaTeleportActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
}

void BotDoRunAwayViaTeleportActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

PlannerNode *BotDoRunAwayViaTeleportAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (!worldState.HasJustTeleportedVar().Ignore() && worldState.HasJustTeleportedVar())
    {
        Debug("Bot has just teleported in the given world state. Avoid chain teleportations\n");
        return nullptr;
    }
    if (worldState.HasPendingRunAwayTeleportVar().Ignore() || !worldState.HasPendingRunAwayTeleportVar())
    {
        Debug("Has bot a pending teleport for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

#ifdef _DEBUG
    // Sanity check
    if (worldState.NavTargetOriginVar().Ignore())
    {
        worldState.DebugPrint("Goal WS");
        AI_FailWith(this->name, "Nav target origin is ignored in the given world state\n");
    }
    if (worldState.PendingOriginVar().Ignore())
    {
        worldState.DebugPrint("Goal WS");
        AI_FailWith(this->name, "Pending origin is ignored in the given world state\n");
    }
#endif

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the nav target (teleport origin)\n");
        return nullptr;
    }

    Vec3 teleportOrigin = worldState.NavTargetOriginVar().Value();
    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, teleportOrigin, Enemies().InstanceId())));
    if (!plannerNode)
        return nullptr;

    // Teleportation costs almost nothing
    plannerNode.Cost() = 1;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustTeleportedVar().SetValue(false).SetIgnore(false);
    // Set bot origin to the teleport destination
    plannerNode.WorldState().BotOriginVar().SetValue(worldState.PendingOriginVar().Value());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    // Reset pending origin
    plannerNode.WorldState().PendingOriginVar().SetIgnore(true);
    plannerNode.WorldState().HasPendingRunAwayTeleportVar().SetIgnore(true);
    // Tactical spots should be recomputed after teleportation
    plannerNode.WorldState().ResetTacticalSpots();

    plannerNode.WorldState().IsRunningAwayVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().CanHitEnemyVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().EnemyCanHitVar().SetValue(false).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

AiBaseActionRecord::Status BotDoRunAwayViaTeleportActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustTeleportedVar().Ignore())
    {
        Debug("Has bot just teleported is ignored\n");
        return INVALID;
    }
    if (currWorldState.HasJustTeleportedVar())
        return COMPLETED;

    if (currWorldState.HasThreateningEnemyVar().Ignore())
    {
        Debug("A threatening enemy is ignored\n");
        return INVALID;
    }
    if (!currWorldState.HasThreateningEnemyVar())
    {
        Debug("A threatening enemy is absent\n");
        return INVALID;
    }
    if (selectedEnemiesInstanceId != Enemies().InstanceId())
    {
        Debug("New enemies have been selected\n");
        return INVALID;
    }
    // Use the same radius as for goal items pickups
    // (running actions for picking up an item and running away might be shared)
    if ((navTarget.Origin() - self->s.origin).SquaredLength() > GOAL_PICKUP_ACTION_RADIUS * GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the teleport trigger\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotStartGotoRunAwayJumppadAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (!worldState.HasJustTouchedJumppadVar().Ignore() && worldState.HasJustTouchedJumppadVar())
    {
        Debug("Bot has just touched the jumppad in the given world state\n");
        return nullptr;
    }
    if (!worldState.PendingOriginVar().Ignore())
    {
        Debug("The pending origin is already present in the given world state\n");
        return nullptr;
    }
    if (worldState.RunAwayJumppadOriginVar().IgnoreOrAbsent())
    {
        Debug("A jumppad for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasPendingRunAwayJumppadVar().SetValue(true).SetIgnore(false);
    // Set nav target to the jumppad origin
    plannerNode.WorldState().NavTargetOriginVar().SetValue(worldState.RunAwayJumppadOriginVar().Value());
    plannerNode.WorldState().NavTargetOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().NavTargetOriginVar().SetIgnore(false);
    // Set pending origin to the jumppad destination
    plannerNode.WorldState().PendingOriginVar().SetValue(worldState.RunAwayJumppadOriginVar().Value2());
    plannerNode.WorldState().PendingOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().PendingOriginVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotDoRunAwayViaJumppadActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
}

void BotDoRunAwayViaJumppadActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotDoRunAwayViaJumppadActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    if (currWorldState.HasJustTouchedJumppadVar().Ignore())
    {
        Debug("Has just touched jumppad is ignored\n");
        return INVALID;
    }
    if (currWorldState.HasJustTouchedJumppadVar())
        return COMPLETED;

    if (currWorldState.HasThreateningEnemyVar().Ignore())
    {
        Debug("A threatening enemy is ignored\n");
        return INVALID;
    }
    if (!currWorldState.HasThreateningEnemyVar())
    {
        Debug("A threatening enemy is absent\n");
        return INVALID;
    }
    if (selectedEnemiesInstanceId != Enemies().InstanceId())
    {
        Debug("New enemies have been selected\n");
        return INVALID;
    }
    // Use the same radius as for goal items pickups
    // (running actions for picking up an item and running away might be shared)
    if ((navTarget.Origin() - self->s.origin).SquaredLength() > GOAL_PICKUP_ACTION_RADIUS * GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the jumppad trigger\n");
        return INVALID;
    }

    return VALID;
}

PlannerNode *BotDoRunAwayViaJumppadAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (!worldState.HasJustTouchedJumppadVar().Ignore() && worldState.HasJustTouchedJumppadVar())
    {
        Debug("Has bot just touched a jumppad is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasPendingRunAwayJumppadVar().Ignore() || !worldState.HasPendingRunAwayJumppadVar())
    {
        Debug("Has bot a pending jumppad for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

#ifdef _DEBUG
    // Sanity check
    if (worldState.NavTargetOriginVar().Ignore())
    {
        worldState.DebugPrint("Goal WS");
        AI_FailWith(this->name, "Nav target origin is ignored in the given world state\n");
    }
    if (worldState.PendingOriginVar().Ignore())
    {
        worldState.DebugPrint("Goal WS");
        AI_FailWith(this->name, "Pending origin is ignored in the given world state\n");
    }
#endif

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the nav target (jumppad origin)");
        return nullptr;
    }

    Vec3 jumppadOrigin = worldState.NavTargetOriginVar().Value();
    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, jumppadOrigin, Enemies().InstanceId())));
    if (!plannerNode)
        return nullptr;

    // Use distance from jumppad origin to target as an estimation for travel time millis
    plannerNode.Cost() = (jumppadOrigin - worldState.PendingOriginVar().Value()).LengthFast();

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustTouchedJumppadVar().SetValue(true).SetIgnore(false);
    // Set bot origin to the jumppad destination
    plannerNode.WorldState().BotOriginVar().SetValue(worldState.PendingOriginVar().Value());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    // Reset pending origin
    plannerNode.WorldState().PendingOriginVar().SetIgnore(true);
    plannerNode.WorldState().HasPendingRunAwayJumppadVar().SetIgnore(true);
    // Tactical spots should be recomputed for the new bot origin
    plannerNode.WorldState().ResetTacticalSpots();

    plannerNode.WorldState().IsRunningAwayVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().CanHitEnemyVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().EnemyCanHitVar().SetValue(false).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

PlannerNode *BotStartGotoRunAwayElevatorAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (worldState.HasJustEnteredElevatorVar().Ignore())
    {
        Debug("Has bot just entered an elevator is ignored in the given world state\n");
        return nullptr;
    }
    if (worldState.HasJustEnteredElevatorVar())
    {
        Debug("Bot has just entered an elevator in the given world state\n");
        return nullptr;
    }
    if (!worldState.PendingOriginVar().Ignore())
    {
        Debug("Pending origin is already present in the given world state\n");
        return nullptr;
    }
    if (worldState.RunAwayElevatorOriginVar().IgnoreOrAbsent())
    {
        Debug("An elevator for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasPendingRunAwayElevatorVar().SetValue(true).SetIgnore(false);
    // Set nav target to the elevator origin
    plannerNode.WorldState().NavTargetOriginVar().SetValue(worldState.RunAwayElevatorOriginVar().Value());
    plannerNode.WorldState().NavTargetOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().NavTargetOriginVar().SetIgnore(false);
    // Set pending origin to the elevator destination
    plannerNode.WorldState().PendingOriginVar().SetValue(worldState.RunAwayElevatorOriginVar().Value2());
    plannerNode.WorldState().PendingOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    plannerNode.WorldState().PendingOriginVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotDoRunAwayViaElevatorActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
}

void BotDoRunAwayViaElevatorActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotDoRunAwayViaElevatorActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    // Checking of this action record differs from other run away action record.
    // We want the bot to stand on a platform until it finishes its movement.

    // We do not want to invalidate an action due to being a bit in air above the platform, don't check self->groundentity
    trace_t selfTrace;
    AiGroundTraceCache::Instance()->GetGroundTrace(self, 64.0f, &selfTrace);

    if (selfTrace.fraction == 1.0f)
    {
        Debug("Bot is too high above the ground (if any)\n");
        return INVALID;
    }
    if (selfTrace.ent <= gs.maxclients || game.edicts[selfTrace.ent].use != Use_Plat)
    {
        Debug("Bot is not above a platform\n");
        return INVALID;
    }

    // If there are no valid enemies, just keep standing on the platform
    if (Enemies().AreValid())
    {
        trace_t enemyTrace;
        AiGroundTraceCache::Instance()->GetGroundTrace(Enemies().Ent(), 128.0f, &enemyTrace);
        if (enemyTrace.fraction != 1.0f && enemyTrace.ent == selfTrace.ent)
        {
            Debug("Enemy is on the same platform!\n");
            return INVALID;
        }
    }

    if (game.edicts[selfTrace.ent].moveinfo.state == STATE_TOP)
        return COMPLETED;

    return VALID;
}

PlannerNode *BotDoRunAwayViaElevatorAction::TryApply(const WorldState &worldState)
{
    if (!CheckCommonRunAwayPreconditions(worldState))
        return nullptr;

    if (!worldState.HasJustEnteredElevatorVar().Ignore() && worldState.HasJustEnteredElevatorVar())
    {
        Debug("Bot has just entered elevator in the given world state\n");
        return nullptr;
    }
    if (worldState.HasPendingRunAwayElevatorVar().Ignore() || !worldState.HasPendingRunAwayElevatorVar())
    {
        Debug("Has bot a pending elevator for running away is ignored or absent in the given world state\n");
        return nullptr;
    }

#ifdef _DEBUG
    // Sanity check
    if (worldState.NavTargetOriginVar().Ignore())
    {
        worldState.DebugPrint("Given WS");
        AI_FailWith(this->name, "Nav target origin is ignored in the given world state\n");
    }
    if (worldState.PendingOriginVar().Ignore())
    {
        worldState.DebugPrint("Given WS");
        AI_FailWith(this->name, "Pending origin is ignored in the given world state\n");
    }
#endif

    if (worldState.DistanceToNavTarget() > GOAL_PICKUP_ACTION_RADIUS)
    {
        Debug("Bot is too far from the nav target (elevator origin)\n");
        return nullptr;
    }

    Vec3 elevatorOrigin = worldState.NavTargetOriginVar().Value();
    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, elevatorOrigin, Enemies().InstanceId())));
    if (!plannerNode)
        return nullptr;

    float elevatorDistance = (elevatorOrigin - worldState.PendingOriginVar().Value()).LengthFast();
    // Assume that elevator speed is 400 units per second
    float speedInUnitsPerMillis = 400 / 1000.0f;
    plannerNode.Cost() = elevatorDistance / speedInUnitsPerMillis;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustEnteredElevatorVar().SetValue(true).SetIgnore(false);
    // Set bot origin to the elevator destination
    plannerNode.WorldState().BotOriginVar().SetValue(worldState.PendingOriginVar().Value());
    plannerNode.WorldState().BotOriginVar().SetSatisfyOp(SatisfyOp::EQ, GOAL_PICKUP_ACTION_RADIUS);
    // Reset pending origin
    plannerNode.WorldState().PendingOriginVar().SetIgnore(true);
    plannerNode.WorldState().HasPendingRunAwayElevatorVar().SetIgnore(true);
    // Tactical spots should be recomputed for the new bot origin
    plannerNode.WorldState().ResetTacticalSpots();

    plannerNode.WorldState().IsRunningAwayVar().SetValue(true).SetIgnore(false);
    plannerNode.WorldState().CanHitEnemyVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().EnemyCanHitVar().SetValue(false).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

PlannerNode *BotStopRunningAwayAction::TryApply(const WorldState &worldState)
{
    if (worldState.IsRunningAwayVar().Ignore())
    {
        Debug("Is bot running away is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.IsRunningAwayVar())
    {
        Debug("Bot is not running away in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasRunAwayVar().Ignore() && worldState.HasRunAwayVar())
    {
        Debug("Bot has already run away in the given world state\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 1.0f;

    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().HasJustTeleportedVar().SetIgnore(true);
    plannerNode.WorldState().HasJustTouchedJumppadVar().SetIgnore(true);
    plannerNode.WorldState().HasJustEnteredElevatorVar().SetIgnore(true);

    plannerNode.WorldState().IsRunningAwayVar().SetValue(false).SetIgnore(false);
    plannerNode.WorldState().HasRunAwayVar().SetValue(true).SetIgnore(false);

    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetValue(NextSimilarWorldStateInstanceId());
    plannerNode.WorldState().SimilarWorldStateInstanceIdVar().SetIgnore(false);

    return plannerNode.PrepareActionResult();
}

void BotDodgeToSpotActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetNavTarget(&navTarget);
    timeoutAt = level.time + 350;
    Tactics().shouldAttack = true;
    Tactics().shouldKeepXhairOnEnemy = true;
}

void BotDodgeToSpotActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    ResetNavTarget();
}

AiBaseActionRecord::Status BotDodgeToSpotActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    // If the bot has reached the spot, consider the action completed
    // (use a low threshold because dodging is a precise movement)
    if ((navTarget.Origin() - self->s.origin).SquaredLength() < 16 * 16)
        return COMPLETED;

    // Return INVALID if has not reached the spot when the action timed out
    return timeoutAt > level.time ? VALID : INVALID;
}

PlannerNode *BotDodgeToSpotAction::TryApply(const WorldState &worldState)
{
    if (worldState.PotentialDangerDamageVar().Ignore())
    {
        Debug("Potential danger damage is ignored in the given world state\n");
        return nullptr;
    }

#ifndef _DEBUG
    // Sanity check
    if (worldState.DangerHitPointVar().Ignore())
    {
        Debug("Danger hit point is ignored in the given world state\n");
        abort();
    }
    if (worldState.DangerDirectionVar().Ignore())
    {
        Debug("Danger direction is ignored in the given world state\n");
        abort();
    }
#endif

    constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((worldState.BotOriginVar().Value() - self->s.origin).SquaredLength() > squareDistanceError)
    {
        Debug("The action can be applied only to the current bot origin\n");
        return nullptr;
    }

    if (worldState.DodgeDangerSpotVar().Ignore())
    {
        Debug("Spot for dodging a danger is ignored in the given world state, can't dodge\n");
        return nullptr;
    }

    const Vec3 spotOrigin = worldState.DodgeDangerSpotVar().Value();
    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, spotOrigin)));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = TravelTimeMillis(worldState.BotOriginVar().Value(), spotOrigin);
    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().BotOriginVar().SetValue(spotOrigin);
    plannerNode.WorldState().PotentialDangerDamageVar().SetIgnore(true);
    plannerNode.WorldState().DangerHitPointVar().SetIgnore(true);
    plannerNode.WorldState().DangerDirectionVar().SetIgnore(true);
    plannerNode.WorldState().HasReactedToDangerVar().SetIgnore(false).SetValue(true);

    return plannerNode.PrepareActionResult();
}

void BotTurnToThreatOriginActionRecord::Activate()
{
    BotBaseActionRecord::Activate();
    SetPendingLookAtPoint(threatPossibleOrigin, 3.0f, 350);
}

void BotTurnToThreatOriginActionRecord::Deactivate()
{
    BotBaseActionRecord::Deactivate();
    InvalidatePendingLookAtPoint();
}

AiBaseActionRecord::Status BotTurnToThreatOriginActionRecord::CheckStatus(const WorldState &currWorldState) const
{
    vec3_t lookDir;
    AngleVectors(self->s.angles, lookDir, nullptr, nullptr);

    Vec3 toThreatDir(threatPossibleOrigin);
    toThreatDir -= self->s.origin;
    toThreatDir.NormalizeFast();

    if (toThreatDir.Dot(lookDir) > 0.3f)
        return COMPLETED;

    return IsPendingLookAtPointValid() ? VALID: INVALID;
}

PlannerNode *BotTurnToThreatOriginAction::TryApply(const WorldState &worldState)
{
    if (worldState.ThreatPossibleOriginVar().Ignore())
    {
        Debug("Threat possible origin is ignored in the given world state\n");
        return nullptr;
    }
    if (!worldState.HasReactedToThreatVar().Ignore() && worldState.HasReactedToThreatVar())
    {
        Debug("Bot has already reacted to threat in the given world state\n");
        return nullptr;
    }

    constexpr float squareDistanceError = WorldState::OriginVar::MAX_ROUNDING_SQUARE_DISTANCE_ERROR;
    if ((worldState.BotOriginVar().Value() - self->s.origin).SquaredLength() > squareDistanceError)
    {
        Debug("The action can be applied only to the current bot origin\n");
        return nullptr;
    }

    PlannerNodePtr plannerNode(NewNodeForRecord(pool.New(self, worldState.ThreatPossibleOriginVar().Value())));
    if (!plannerNode)
        return nullptr;

    plannerNode.Cost() = 500;
    plannerNode.WorldState() = worldState;
    plannerNode.WorldState().ThreatPossibleOriginVar().SetIgnore(true);
    // If a bot has reacted to threat, he can't hit current enemy (if any)
    plannerNode.WorldState().CanHitEnemyVar().SetValue(false);
    plannerNode.WorldState().HasReactedToThreatVar().SetValue(true).SetIgnore(false);

    return plannerNode.PrepareActionResult();
}
