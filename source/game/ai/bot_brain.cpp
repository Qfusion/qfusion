#include "bot.h"
#include "ai_ground_trace_cache.h"
#include "ai_squad_based_team_brain.h"
#include "bot_brain.h"
#include <algorithm>
#include <limits>
#include <stdarg.h>

void BotBrain::EnemyPool::OnEnemyRemoved(const Enemy *enemy)
{
    bot->ai->botRef->OnEnemyRemoved(enemy);
}

void BotBrain::EnemyPool::OnNewThreat(const edict_t *newThreat)
{
    bot->ai->botRef->botBrain.OnNewThreat(newThreat, this);
}

void BotBrain::OnAttachedToSquad(AiSquad *squad)
{
    this->squad = squad;
    activeEnemyPool = squad->EnemyPool();
    ResetCombatTask();
}

void BotBrain::OnDetachedFromSquad(AiSquad *squad)
{
    if (this->squad != squad)
    {
        FailWith("was not attached to squad %s", squad ? squad->Tag() : "???");
    }
    this->squad = nullptr;
    activeEnemyPool = &botEnemyPool;
    ResetCombatTask();
}

void BotBrain::OnNewThreat(const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector)
{
    // Reject threats detected by bot brain if there is active squad.
    // Otherwise there may be two calls for a single or different threats
    // detected by squad and the bot brain enemy pool itself.
    if (squad && threatDetector == &this->botEnemyPool)
        return;

    if (!combatTask.Empty())
    {
        ResetCombatTask();
        nextTargetChoiceAt = level.time + 1;
        nextWeaponChoiceAt = level.time + 1;
    }

    vec3_t botLookDir;
    AngleVectors(self->s.angles, botLookDir, nullptr, nullptr);
    Vec3 toEnemyDir = Vec3(newThreat->s.origin) - self->s.origin;
    float squareDistance = toEnemyDir.SquaredLength();
    if (squareDistance > 1)
    {
        float distance = 1.0f / Q_RSqrt(squareDistance);
        toEnemyDir *= distance;
        if (toEnemyDir.Dot(botLookDir) < 0)
        {
            if (!self->ai->botRef->hasPendingLookAtPoint)
            {
                // Try to guess enemy origin
                toEnemyDir.X() += -0.25f + 0.50f * random();
                toEnemyDir.Y() += -0.10f + 0.20f * random();
                toEnemyDir.NormalizeFast();
                Vec3 threatPoint(self->s.origin);
                threatPoint += distance * toEnemyDir;
                self->ai->botRef->SetPendingLookAtPoint(threatPoint, 1.0f);
            }
        }
    }
}

void BotBrain::OnEnemyRemoved(const Enemy *enemy)
{
    if (enemy == combatTask.aimEnemy)
    {
        ResetCombatTask();
        // This implicitly causes a call to TryFindNewCombatTask() in this or next think frame
        nextTargetChoiceAt = level.time + reactionTime;
    }
}

BotBrain::BotBrain(edict_t *bot, float skillLevel)
    : AiBaseBrain(bot, Bot::PREFERRED_TRAVEL_FLAGS, Bot::ALLOWED_TRAVEL_FLAGS),
      bot(bot),
      baseOffensiveness(0.5f),
      skillLevel(skillLevel),
      reactionTime(320 - From0UpToMax(300, BotSkill())),
      aimTargetChoicePeriod(1000 - From0UpToMax(900, BotSkill())),
      spamTargetChoicePeriod(1333 - From0UpToMax(500, BotSkill())),
      aimWeaponChoicePeriod(1032 - From0UpToMax(500, BotSkill())),
      spamWeaponChoicePeriod(1000 - From0UpToMax(333, BotSkill())),
      combatTaskInstanceCounter(1),
      nextTargetChoiceAt(level.time),
      nextWeaponChoiceAt(level.time),
      nextFastWeaponSwitchActionCheckAt(level.time),
      prevThinkLevelTime(level.time),
      armorProtection(g_armor_protection->value),
      armorDegradation(g_armor_degradation->value),
      weaponScoreRandom(0.0f),
      nextWeaponScoreRandomUpdate(level.time),
      decisionRandom(0.5f),
      nextDecisionRandomUpdate(level.time),
      botEnemyPool(bot, this, BotSkill()),
      specialGoalCombatTaskId(0)
{
    memset(&localNavEntity, 0, sizeof(NavEntity));
    squad = nullptr;
    activeEnemyPool = &botEnemyPool;
    SetTag(bot->r.client->netname);

    // Set a default ignore attitude to a world and non-client entities
    attitude[0] = 0;
    memset(attitude + gs.maxclients, 0, sizeof(attitude));
    // Set a default negative attitude to all clients
    std::fill_n(attitude + 1, gs.maxclients, -1);
    // Save the attitude values as an old attitude values
    static_assert(sizeof(attitude) == sizeof(oldAttitude), "");
    memcpy(oldAttitude, attitude, sizeof(attitude));
}

void BotBrain::PreThink()
{
    AiBaseBrain::PreThink();

    const unsigned levelTime = level.time;

    if (combatTask.aimEnemy)
    {
        if (!combatTask.aimEnemy->IsValid())
        {
            Debug("aiming on an ememy has been invalidated\n");
            ResetCombatTask();
        }
    }
    else if (combatTask.spamEnemy)
    {
        if (!combatTask.spamEnemy->IsValid() || combatTask.spamTimesOutAt <= levelTime)
        {
            Debug("spamming on an ememy has been invalidated\n");
            ResetCombatTask();
        }
    }

    if (nextWeaponScoreRandomUpdate <= levelTime)
    {
        weaponScoreRandom = brandom(0, 1.0f - BotSkill()) - 0.5f;
        nextWeaponScoreRandomUpdate = levelTime + 4000;
    }
    if (nextDecisionRandomUpdate <= levelTime)
    {
        decisionRandom = random();
        nextDecisionRandomUpdate = levelTime + 2500;
    }
}

void BotBrain::PostThink()
{
    AiBaseBrain::PostThink();

    prevThinkLevelTime = level.time;
}

void BotBrain::Frame()
{
    // Call superclass method first
    AiBaseBrain::Frame();

    // Reset offensiveness to a default value
    if (G_ISGHOSTING(self))
        baseOffensiveness = 0.5f;

    botEnemyPool.Update();
}

void BotBrain::Think()
{
    // Call superclass method first
    AiBaseBrain::Think();

    if (nextFastWeaponSwitchActionCheckAt <= level.time)
    {
        if (CheckFastWeaponSwitchAction())
        {
            nextFastWeaponSwitchActionCheckAt = level.time + 500;
            if (nextTargetChoiceAt <= level.time + 64)
                nextTargetChoiceAt += 64 + 1;
            if (nextWeaponChoiceAt <= level.time + 64)
                nextWeaponChoiceAt += 64 + 1;
            return;
        }
    }

    if (combatTask.aimEnemy && (level.time - combatTask.aimEnemy->LastSeenAt()) > reactionTime)
    {
        TryFindNewCombatTask();
    }
    else if (nextTargetChoiceAt > level.time)
    {
        UpdateKeptCurrentCombatTask();
    }
    else
    {
        TryFindNewCombatTask();
    }

    if (!specialGoal && !combatTask.Empty())
        CheckTacticalPosition();
}

void BotBrain::AfterAllEnemiesViewed()
{
    CheckIsInThinkFrame(__FUNCTION__);

    // Stop spamming if we see any enemy in view, choose a target to fight
    if (combatTask.spamEnemy)
    {
        if (activeEnemyPool->WillAssignAimEnemy())
        {
            Debug("should stop spamming at %s, there are enemies in view\n", combatTask.spamEnemy->Nick());
            ResetCombatTask();
            nextTargetChoiceAt = level.time;
        }
    }
}

void BotBrain::OnEnemyViewed(const edict_t *enemy)
{
    botEnemyPool.OnEnemyViewed(enemy);
    if (squad)
        squad->OnBotViewedEnemy(self, enemy);
}

void BotBrain::OnPain(const edict_t *enemy, float kick, int damage)
{
    botEnemyPool.OnPain(self, enemy, kick, damage);
    if (squad)
        squad->OnBotPain(self, enemy, kick, damage);
}

void BotBrain::OnEnemyDamaged(const edict_t *target, int damage)
{
    botEnemyPool.OnEnemyDamaged(self, target, damage);
    if (squad)
        squad->OnBotDamagedEnemy(self, target, damage);
}

void BotBrain::OnGoalCleanedUp(const Goal *goal)
{
    self->ai->botRef->OnGoalCleanedUp(goal);
}

void BotBrain::OnClearSpecialGoalRequested()
{
    AiBaseBrain::OnClearSpecialGoalRequested();
    // Prevent reuse of an old goal for newly set goals
    localSpecialGoal.ResetWithSetter(this);
}

bool BotBrain::ShouldCancelSpecialGoalBySpecificReasons()
{
    if (!specialGoal)
        return false;
    // Can't cancel special goal that was not set by bot itself
    if (specialGoal->Setter() != this)
        return false;

    // Cancel pursuit goal if combat task has changed
    if (combatTask.instanceId != specialGoalCombatTaskId)
        return true;

    // Prefer picking up top-tier items rather than pursuit an enemy
    if ((longTermGoal && longTermGoal->IsTopTierItem()) || (shortTermGoal && shortTermGoal->IsTopTierItem()))
        return true;

    int goalAreaNum = specialGoal->AasAreaNum();
    // Bot is already in a goal area
    if (droppedToFloorAasAreaNum == goalAreaNum)
        return false;

    // Cancel pursuit if its path includes jumppads or elevators, or other vulnerable kinds of movement.
    // Define small set of less-vulnerable travel flags and check an area reachability for these flags.
    int travelFlags = TFL_WALK | TFL_WALKOFFLEDGE | TFL_JUMP | TFL_AIR;
    int reachNum = RouteCache()->ReachabilityToGoalArea(droppedToFloorAasAreaNum, goalAreaNum, travelFlags);
    // If a reachability can't be found, cancel goal
    return reachNum == 0;
}

void BotBrain::UpdateKeptCurrentCombatTask()
{
    auto *task = &combatTask;
    if (nextWeaponChoiceAt <= level.time)
    {
        bool oldAdvance = task->advance;
        bool oldRetreat = task->retreat;
        bool oldInhibit = task->inhibit;
        if (task->aimEnemy)
        {
            nextWeaponChoiceAt = level.time + aimWeaponChoicePeriod;
            SuggestAimWeaponAndTactics(task);
            Debug("UpdateKeptCombatTask(): has aim enemy, next weapon choice at %09d\n", nextWeaponChoiceAt);
        }
        else if (task->spamEnemy)
        {
            nextWeaponChoiceAt = level.time + spamWeaponChoicePeriod;
            SuggestSpamEnemyWeaponAndTactics(task);
            Debug("UpdateKeptCombatTask(): has spam enemy, next weapon choice at %09d\n", nextWeaponChoiceAt);
        }

        // If tactics has been changed, treat updated combat task as new
        if (task->advance != oldAdvance || task->retreat != oldRetreat || task->inhibit != oldInhibit)
            combatTask.instanceId = NextCombatTaskInstanceId();

        UpdateBlockedAreasStatus();
    }
}

bool BotBrain::CheckFastWeaponSwitchAction()
{
    if (!combatTask.aimEnemy)
        return false;

    if (self->r.client->ps.stats[STAT_WEAPON_TIME] >= 64)
        return false;

    // Easy bots do not perform fast weapon switch actions
    if (BotSkill() < 0.33f)
        return false;

    const Enemy &enemy = *combatTask.aimEnemy;
    CombatDisposition disposition = GetCombatDisposition(*combatTask.aimEnemy);

    bool botMovesFast, enemyMovesFast;

    int chosenWeapon = WEAP_NONE;
    if (disposition.damageToKill < 75)
    {
        Debug("decided to finish %s that can resist %.1f damage units\n", enemy.Nick(), disposition.damageToKill);
        chosenWeapon = SuggestFinishWeapon(enemy, disposition);
        Debug("chose %s to finish %s\n", WeapName(chosenWeapon), enemy.Nick());
    }
    // Try to hit escaping enemies hard in a single shot
    else if (IsEnemyEscaping(enemy, disposition, &botMovesFast, &enemyMovesFast))
    {
        Debug("detected that %s is escaping\n", enemy.Nick());
        chosenWeapon = SuggestHitEscapingEnemyWeapon(enemy, disposition, botMovesFast, enemyMovesFast);
        Debug("chose %s to hit escaping %s\n", WeapName(chosenWeapon), enemy.Nick());
    }
    // Try to hit enemy hard in a single shot before death
    else if (CheckForShotOfDespair(enemy, disposition))
    {
        Debug("decided to do a shot of despair having %.1f damage units to be killed\n", disposition.damageToBeKilled);
        chosenWeapon = SuggestShotOfDespairWeapon(enemy, disposition);
        Debug("chose %s for shot of despair at %s\n", WeapName(chosenWeapon), enemy.Nick());
    }

    if (chosenWeapon != WEAP_NONE)
    {
        combatTask.suggestedShootWeapon = chosenWeapon;
        combatTask.importantShot = true;
        return true;
    }

    return false;
}

void BotBrain::TryFindNewCombatTask()
{
    if (const Enemy *aimEnemy = activeEnemyPool->ChooseAimEnemy(bot))
    {
        combatTask.aimEnemy = aimEnemy;
        combatTask.instanceId = NextCombatTaskInstanceId();
        nextTargetChoiceAt = level.time + aimTargetChoicePeriod;
        activeEnemyPool->EnqueueTarget(aimEnemy->ent);
        UpdateBlockedAreasStatus();

        Debug("TryFindNewCombatTask(): found aim enemy %s, next target choice at %09d\n", aimEnemy->Nick(), nextTargetChoiceAt);
        SuggestAimWeaponAndTactics(&combatTask);
        return;
    }

    if (oldCombatTask.aimEnemy)
    {
        if (bot->ai->botRef->hasPendingLookAtPoint)
        {
            Debug("TryFindNewCombatTask(): bot is already turning to some look-at-point, defer target assignment\n");
        }
        else
        {
            SuggestPointToTurnToWhenEnemyIsLost(oldCombatTask.aimEnemy);
        }
        return;
    }

    if (!HasMoreImportantTasksThanEnemies());
        SuggestPursuitOrSpamTask(&combatTask);
}

bool BotBrain::SuggestPointToTurnToWhenEnemyIsLost(const Enemy *oldEnemy)
{
    unsigned notSeenDuration = level.time - oldEnemy->LastSeenAt();
    if (notSeenDuration > 500)
        return false;

    Vec3 estimatedPos(oldEnemy->LastSeenPosition());
    Vec3 lastSeenVelocityDir(oldEnemy->LastSeenVelocity());
    float lastSeenSqSpeed = lastSeenVelocityDir.SquaredLength();
    if (lastSeenSqSpeed > 1)
        lastSeenVelocityDir *= Q_RSqrt(lastSeenSqSpeed);
    // Extrapolate last seen position using last seen velocity and not seen duration in seconds
    estimatedPos += (notSeenDuration / 1000.0f) * lastSeenVelocityDir;

    float turnSpeedMultiplier = 0.55f + 0.55f * BotSkill();
    bot->ai->botRef->SetPendingLookAtPoint(estimatedPos, turnSpeedMultiplier, 750);

    return true;
}

void BotBrain::SuggestPursuitOrSpamTask(CombatTask *task)
{
    if (GetEffectiveOffensiveness() < 0.25f)
    {
        nextTargetChoiceAt = level.time + spamTargetChoicePeriod / 3;
        return;
    }
    if (const Enemy *bestEnemy = activeEnemyPool->ChooseHiddenEnemy(bot))
    {
        StartSpamAtEnemyOrPursuit(task, bestEnemy);
        nextTargetChoiceAt = level.time + spamTargetChoicePeriod;
    }
    else
    {
        // If not set, bot will repeat try to find target on each next frame
        nextTargetChoiceAt = level.time + spamTargetChoicePeriod / 2;
    }
}

void BotBrain::StartSpamAtEnemyOrPursuit(CombatTask *task, const Enemy *enemy)
{
    const auto disposition = GetCombatDisposition(*enemy);
    float effectiveKillToBeKilledRatio = disposition.KillToBeKilledDamageRatio();
    // If bot is supported by a squad
    if (squad)
        effectiveKillToBeKilledRatio /= 1.5f;

    float offensiveness = disposition.offensiveness;
    // For offensiveness of 0 this should be 0
    // For base offensiveness of 0.5 this should be 0.75f
    // For offensiveness of 1 this should be relatively large (about 5.0f)
    float pursuitThresholdKillToBeKilledRatio = 1.5f;
    if (offensiveness <= 0.5f)
        pursuitThresholdKillToBeKilledRatio *= offensiveness;
    else
        pursuitThresholdKillToBeKilledRatio *= 0.5f * powf(10.0f, 2.0f * (offensiveness - 0.5f));

    if (effectiveKillToBeKilledRatio < pursuitThresholdKillToBeKilledRatio)
    {
        StartPursuit(*enemy);
        return;
    }

    Vec3 spamSpot = enemy->LastSeenPosition();
    task->spamEnemy = enemy;
    task->spamSpot = spamSpot;
    task->instanceId = NextCombatTaskInstanceId();
    SuggestSpamEnemyWeaponAndTactics(task);
    task->spamTimesOutAt = level.time + 1200;
    unsigned timeDelta = level.time - enemy->LastSeenAt();
    constexpr const char *fmt = "starts spamming at %.3f %.3f %.3f with %s where it has seen %s %d ms ago\n";
    Debug(fmt, spamSpot.X(), spamSpot.Y(), spamSpot.Z(), WeapName(task->suggestedSpamWeapon), enemy->Nick(), timeDelta);
}

bool BotBrain::IsGoalATopTierItem() const
{
    if (specialGoal && specialGoal->IsTopTierItem())
        return true;

    if (longTermGoal && longTermGoal->IsTopTierItem())
        return true;

    if (shortTermGoal && shortTermGoal->IsTopTierItem())
        return true;

    return false;
}

unsigned BotBrain::GoalSpawnTime() const
{
    // Note: goals are listed in order of (short-term) priority

    if (specialGoal)
        return specialGoal->SpawnTime();

    if (shortTermGoal)
        return shortTermGoal->SpawnTime();

    if (longTermGoal)
        return longTermGoal->SpawnTime();

    return level.time;
}

bool BotBrain::HasMoreImportantTasksThanEnemies() const
{
    if (specialGoal)
        return true;

    if (longTermGoal && longTermGoal->IsTopTierItem())
        return true;

    if (shortTermGoal && shortTermGoal->IsTopTierItem())
        return true;

    return false;
}

bool BotBrain::StartPursuit(const Enemy &enemy, unsigned timeout)
{
    Debug("decided to pursue %s\n", enemy.Nick());
    Vec3 origin(enemy.ent->s.origin);
    if (!SetTacticalSpot(origin, timeout))
    {
        Debug("Can't set pursuit tactical spot for %s\n", enemy.Nick());
        return false;
    }
    return true;
}

bool BotBrain::SetTacticalSpot(const Vec3 &origin, unsigned timeout)
{
    if (localSpecialGoal.Setter() != this)
    {
        Debug("Can't set tactical spot (a special goal is set by an external AI entity)\n");
        Debug("localSpecialGoal.Setter(): %p, this: %p\n", localSpecialGoal.Setter(), this);
        return false;
    }
    localSpecialGoal.SetToTacticalSpot(origin, timeout, this);
    SetSpecialGoal(&localSpecialGoal);
    specialGoalCombatTaskId = combatTask.instanceId;
    return true;
}

void BotBrain::SetSpecialGoalFromEntity(edict_t *entity, const AiFrameAwareUpdatable *setter)
{
    memset(&localNavEntity, 0, sizeof(NavEntity));
    localNavEntity.ent = entity;
    localNavEntity.aasAreaNum = AasWorld()->FindAreaNum(entity->s.origin);
    localNavEntity.flags = NavEntityFlags::REACH_AT_TOUCH | NavEntityFlags::DROPPED_ENTITY;
    localSpecialGoal.SetToNavEntity(&localNavEntity, setter);
    SetSpecialGoal(&localSpecialGoal);
}

void BotBrain::CheckTacticalPosition()
{
    trace_t trace;

    // First, trace whether a bot may hit an enemy from current position
    // (Do a trace from bot origin to enemy origin)
    Vec3 fireOrigin(self->s.origin);
    fireOrigin.Z() += self->viewheight;
    G_Trace(&trace, fireOrigin.Data(), nullptr, nullptr, combatTask.EnemyOrigin().Data(), self, MASK_AISOLID);

    // Bot may hit from the current position
    if (trace.fraction == 1.0f || game.edicts + trace.ent == combatTask.TraceKey())
        return;

    // Avoid another trace call by doing a cheap velocity test first
    // (Predicting origin makes sense for relatively high speed, otherwise the origin is almost the same)
    if (VectorLengthSquared(self->velocity) > 200 * 200)
    {
        // Predict origin for 0.5 s
        float t = 0.5f;
        fireOrigin += t * Vec3(self->velocity);
        // TODO: Check ground trace
        if (!self->groundentity)
            fireOrigin.Z() -= 0.5f * level.gravity * t * t;
        G_Trace(&trace, fireOrigin.Data(), nullptr, nullptr, combatTask.EnemyOrigin().Data(), self, MASK_AISOLID);

        // Bot would be able to hit
        if (trace.fraction == 1.0f || game.edicts + trace.ent == combatTask.TraceKey())
            return;
    }

    combatTask.inhibit = true;
    combatTask.instanceId = NextCombatTaskInstanceId();
    SetTacticalSpot(combatTask.EnemyOrigin(), 350 + (unsigned)(250 * GetBaseOffensiveness()));
}

void BotBrain::UpdateBlockedAreasStatus()
{
    if (!combatTask.retreat)
    {
        // Reset all possibly blocked areas
        RouteCache()->SetDisabledRegions(nullptr, nullptr, 0);
        return;
    }

    StaticVector<Vec3, EnemyPool::MAX_TRACKED_ENEMIES> mins;
    StaticVector<Vec3, EnemyPool::MAX_TRACKED_ENEMIES> maxs;

    AiGroundTraceCache *groundTraceCache = AiGroundTraceCache::Instance();
    for (const Enemy *enemy: activeEnemyPool->ActiveEnemies())
    {
        if (!enemy->IsValid())
            continue;

        // TODO: This may act as cheating since actual enemy origin is used.
        // This is kept for conformance to following ground trace check.
        float squareDistance = DistanceSquared(self->s.origin, enemy->ent->s.origin);
        // (Otherwise all nearby paths are treated as blocked by enemy)
        if (squareDistance < 72.0f)
            continue;
        float distance = 1.0f / Q_RSqrt(squareDistance);
        float side = 72.0f + 192.0f * BoundedFraction(distance - 72.0f, 384.0f);

        // Try to drop an enemy origin to floor
        // TODO: AiGroundTraceCache interface forces using an actual enemy origin
        // and not last seen one, so this may act as cheating.
        vec3_t origin;
        // If an enemy is close to ground (an origin may and has been dropped to floor)
        if (groundTraceCache->TryDropToFloor(enemy->ent, 128.0f, origin, level.time - enemy->LastSeenAt()))
        {
            // Do not use bounds lower than origin[2] (except some delta)
            mins.push_back(Vec3(-side, -side, -8.0f) + origin);
            maxs.push_back(Vec3(+side, +side, 128.0f) + origin);
        }
        else
        {
            // Use a bit lower bounds (an enemy is likely to fall down)
            mins.push_back(Vec3(-side, -side, -192.0f) + origin);
            maxs.push_back(Vec3(+side, +side, +108.0f) + origin);
        }
    }

    RouteCache()->SetDisabledRegions(&mins[0], &maxs[0], mins.size());
}

// Old weapon selection code with some style and C to C++ fixes
int BotBrain::SuggestEasyBotsWeapon(const Enemy &enemy)
{
	float best_weight = 0.0;
	int weapon_range = 0, best_weapon = WEAP_NONE;

	const float	dist = DistanceFast(bot->s.origin, enemy.ent->s.origin);

    if (dist < 150)
        weapon_range = AIWEAP_MELEE_RANGE;
    else if (dist < 500)  // Medium range limit is Grenade launcher range
        weapon_range = AIWEAP_SHORT_RANGE;
    else if (dist < 900)
        weapon_range = AIWEAP_MEDIUM_RANGE;
    else
        weapon_range = AIWEAP_LONG_RANGE;

	for (int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++)
	{
        gsitem_t *weaponItem;
		float rangeWeight;

		if((weaponItem = GS_FindItemByTag(i)) == nullptr)
			continue;

		if(!GS_CheckAmmoInWeapon( &bot->r.client->ps, i ))
			continue;

		rangeWeight = AIWeapons[i].RangeWeight[weapon_range];

		// weigh up if having strong ammo
		if( bot->r.client->ps.inventory[weaponItem->ammo_tag] )
			rangeWeight *= 1.25;

		// add a small random factor (less random the more skill)
		rangeWeight += brandom(-(1.0f - BotSkill()), 1.0f - BotSkill());

		// compare range weights
		if(rangeWeight > best_weight)
		{
			best_weight = rangeWeight;
			best_weapon = i;
		}
	}

    return best_weapon;
}

static constexpr float CLOSE_RANGE = 150.0f;

inline float GetLaserRange()
{
    const auto lgDef = GS_GetWeaponDef(WEAP_LASERGUN);
    return (lgDef->firedef.timeout + lgDef->firedef.timeout) / 2.0f;
}

CombatDisposition BotBrain::GetCombatDisposition(const Enemy &enemy)
{
    float damageToBeKilled = DamageToKill(bot);
    if (BotHasShell())
        damageToBeKilled *= 4.0f;
    if (enemy.HasQuad())
        damageToBeKilled /= 4.0f;
    float damageToKillEnemy = DamageToKill(enemy.ent);
    if (enemy.HasShell())
        damageToKillEnemy *= 4.0f;

    float distance = (enemy.LastSeenPosition() - self->s.origin).LengthFast();

    bool isOutnumbered = false;
    const auto &activeEnemies = activeEnemyPool->ActiveEnemies();
    if (!BotHasPowerups() && activeEnemies.size() > 1)
    {
        // Start from second active enemy
        for (unsigned i = 1; i < activeEnemies.size(); ++i)
        {
            const Enemy *e = activeEnemies[i];
            if (e == &enemy)
                continue;
            if (!e->ent)
                continue;
            if (DistanceSquared(e->ent->s.origin, enemy.ent->s.origin) < 192 * 192)
                damageToKillEnemy += DamageToKill(activeEnemies[i]->ent);
        }
        if (damageToKillEnemy > 1.5 * damageToBeKilled)
            isOutnumbered = true;
    }

    CombatDisposition disposition;
    disposition.damageToBeKilled = damageToBeKilled;
    disposition.damageToKill = damageToKillEnemy;
    disposition.distance = distance;
    disposition.offensiveness = GetEffectiveOffensiveness();
    disposition.isOutnumbered = isOutnumbered;
    return disposition;
}

void BotBrain::SuggestAimWeaponAndTactics(CombatTask *task)
{
    const Enemy &enemy = *task->aimEnemy;
    if (BotSkill() < 0.33f)
    {
        task->suggestedShootWeapon = SuggestEasyBotsWeapon(enemy);
        return;
    }

    Vec3 botOrigin(bot->s.origin);
    TestTargetEnvironment(botOrigin, enemy.LastSeenPosition(), enemy.ent);

    if (GS_Instagib())
    {
        task->suggestedShootWeapon = SuggestInstagibWeapon(enemy);
        return;
    }

    CombatDisposition disposition = GetCombatDisposition(enemy);

    if (BotHasPowerups())
    {
        task->suggestedShootWeapon = SuggestQuadBearerWeapon(enemy);
        if (!BotIsCarrier() && task->suggestedShootWeapon != WEAP_GUNBLADE)
        {
            task->advance = true;
            task->retreat = false;
            StartPursuit(enemy);
        }
        else
        {
            task->advance = false;
            task->retreat = disposition.isOutnumbered;
        }
        return;
    }

    const float lgRange = GetLaserRange();

    if (disposition.distance > lgRange * 2.0f)
        SuggestSniperRangeWeaponAndTactics(task, disposition);
    else if (disposition.distance > lgRange)
        SuggestFarRangeWeaponAndTactics(task, disposition);
    else if (disposition.distance > CLOSE_RANGE)
        SuggestMiddleRangeWeaponAndTactics(task, disposition);
    else
        SuggestCloseRangeWeaponAndTactics(task, disposition);

    if (task->suggestedShootWeapon == WEAP_NONE)
        task->suggestedShootWeapon = WEAP_GUNBLADE;

    bool oldAdvance = task->advance;
    bool oldRetreat = task->retreat;

    if (task->advance)
    {
        // Prefer to pickup an item rather than pursuit an enemy
        // if the item is valuable, even if the bot is outnumbered
        if (HasMoreImportantTasksThanEnemies())
        {
            if (!specialGoal || !specialGoal->IsTacticalSpot())
            {
                task->advance = false;
                task->retreat = false;
            }
        }
    }

    // Treat task as a new if tactics has been changed
    if (oldAdvance != task->advance || oldRetreat != task->retreat)
        combatTask.instanceId = NextCombatTaskInstanceId();
}

void BotBrain::SuggestSniperRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition)
{
    const Enemy &enemy = *task->aimEnemy;

    int chosenWeapon = WEAP_NONE;

    // Spam plasma from long range to blind enemy
    if (enemy.PendingWeapon() == WEAP_ELECTROBOLT && decisionRandom < BotSkill() / 3.0f)
    {
        if (PlasmasReadyToFireCount())
            chosenWeapon = WEAP_PLASMAGUN;
    }
    if (chosenWeapon == WEAP_NONE)
    {
        if (disposition.damageToKill > 50.0f)
        {
            if (BoltsReadyToFireCount())
                chosenWeapon = WEAP_ELECTROBOLT;
            else if (BulletsReadyToFireCount())
                chosenWeapon = WEAP_MACHINEGUN;
        }
        else
        {
            if (BulletsReadyToFireCount())
                chosenWeapon = WEAP_MACHINEGUN;
            else if (BoltsReadyToFireCount())
                chosenWeapon = WEAP_ELECTROBOLT;
        }
    }
    // Still not chosen
    if (chosenWeapon == WEAP_NONE)
    {
        if (disposition.damageToKill < 25.0f && ShellsReadyToFireCount())
            chosenWeapon = WEAP_RIOTGUN;
    }

    task->retreat = false;
    float ratio = disposition.KillToBeKilledDamageRatio();
    if (ratio < 2 || (ratio > 0.75 && decisionRandom < 0.5))
        task->advance = random() < disposition.offensiveness;
    else
        task->inhibit = true;

    Debug("(sniper range)... : chose %s \n", GS_GetWeaponDef(chosenWeapon)->name);

    task->suggestedShootWeapon = chosenWeapon;
}

struct WeaponAndScore
{
    int weapon;
    float score;
    WeaponAndScore(int weapon = WEAP_NONE, float score = 0.0f)
    {
        this->weapon = weapon;
        this->score = score;
    }
};

int BotBrain::ChooseWeaponByScores(struct WeaponAndScore *begin, struct WeaponAndScore *end)
{
    int weapon = WEAP_NONE;
    const int pendingWeapon = bot->r.client->ps.stats[STAT_PENDING_WEAPON];
    float pendingWeaponScore = std::numeric_limits<float>::infinity();
    for (WeaponAndScore *it = begin; it != end; ++it)
    {
        if (pendingWeapon == it->weapon)
        {
            pendingWeaponScore = it->score + weaponScoreRandom;
            break;
        }
    }
    float maxScore = 0.0f;
    if (pendingWeaponScore != std::numeric_limits<float>::infinity())
    {
        float weightDiffThreshold = 0.1f;
        // Do not switch too often continuous fire weapons
        if (pendingWeapon == WEAP_PLASMAGUN || pendingWeapon == WEAP_MACHINEGUN)
            weightDiffThreshold += 0.2f;
        else if (pendingWeapon == WEAP_LASERGUN)
            weightDiffThreshold += 0.3f;
        for (WeaponAndScore *it = begin; it != end; ++it)
        {
            float currScore = it->score + weaponScoreRandom;
            if (maxScore < currScore)
            {
                // Do not change weapon if its score is almost equal to current one to avoid weapon choice "jitter"
                // when a bot tries to change weapon infinitely when weapon scores are close to each other
                if (pendingWeapon == it->weapon || fabsf(currScore - pendingWeaponScore) > weightDiffThreshold)
                {
                    maxScore = currScore;
                    weapon = it->weapon;
                }
            }
        }
    }
    else
    {
        for (WeaponAndScore *it = begin; it != end; ++it)
        {
            float currScore = it->score + weaponScoreRandom;
            if (maxScore < currScore)
            {
                maxScore = currScore;
                weapon = it->weapon;
            }
        }
    }

    return weapon;
}

void BotBrain::SuggestFarRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition)
{
    const Enemy &enemy = *task->aimEnemy;

    bool botHasMidRangeWeapons = LasersReadyToFireCount() >= 15 ||
        RocketsReadyToFireCount() >= 3 || PlasmasReadyToFireCount() >= 20;
    bool enemyHasMidRangeWeapons = enemy.LasersReadyToFireCount() >= 15 ||
        enemy.RocketsReadyToFireCount() >= 2 || enemy.PlasmasReadyToFireCount() > 15;

    if (botHasMidRangeWeapons && !enemyHasMidRangeWeapons)
        task->advance = disposition.offensiveness > 0;
    if (!botHasMidRangeWeapons && enemyHasMidRangeWeapons)
        task->retreat = disposition.offensiveness <= 0.5f;

    if (disposition.KillToBeKilledDamageRatio() > 1.5f * (2.0f * disposition.offensiveness))
    {
        task->advance = false;
        task->retreat = true;
    }
    else if (disposition.KillToBeKilledDamageRatio() < 0.75f * (2.0f * disposition.offensiveness))
    {
        task->advance = true;
        task->retreat = false;
    }

    // First, try to choose a long range weapon of EB, MG, PG and RG
    enum { EB, MG, PG, RG };
    WeaponAndScore weaponScores[4] =
    {
        WeaponAndScore(WEAP_ELECTROBOLT, 1.0f * BoundedFraction(BoltsReadyToFireCount(), 2.0f)),
        WeaponAndScore(WEAP_MACHINEGUN, 1.0f * BoundedFraction(BulletsReadyToFireCount(), 10.0f)),
        WeaponAndScore(WEAP_PLASMAGUN, 0.8f * BoundedFraction(PlasmasReadyToFireCount(), 15.0f)),
        WeaponAndScore(WEAP_RIOTGUN, 0.6f * BoundedFraction(ShellsReadyToFireCount(), 2))
    };

    weaponScores[EB].score += BotSkill() / 3;

    // Counteract EB with PG or MG
    if (enemy.PendingWeapon() == WEAP_ELECTROBOLT)
    {
        weaponScores[PG].score *= 1.3f;
        weaponScores[MG].score *= 1.2f;
    }
    // Counteract PG with MG or EB
    if (enemy.PendingWeapon() == WEAP_PLASMAGUN)
    {
        weaponScores[EB].score *= 1.4f;
        weaponScores[MG].score *= 1.2f;
    }
    // Counteract MG with PG or EB
    if (enemy.PendingWeapon() == WEAP_MACHINEGUN)
    {
        weaponScores[PG].score *= 1.2f;
        weaponScores[EB].score *= 1.3f;
    }

    // Do not use plasma on fast-moving side-to-side enemies
    Vec3 targetMoveDir = Vec3(enemy.ent->velocity);
    float enemySpeed = targetMoveDir.SquaredLength();
    if (enemySpeed > 0.1f)
    {
        enemySpeed = 1.0f / Q_RSqrt(enemySpeed);
    }
    if (enemySpeed > DEFAULT_DASHSPEED)
    {
        targetMoveDir *= 1.0f / enemySpeed;
        Vec3 botToTargetDir = enemy.LastSeenPosition() - bot->s.origin;
        botToTargetDir.NormalizeFast();

        float speedFactor = BoundedFraction(enemySpeed - DEFAULT_DASHSPEED, 1000.0f - DEFAULT_DASHSPEED);
        float dirFactor = fabsf(botToTargetDir.Dot(targetMoveDir));
        // If enemy moves fast but on botToTargetDir line, pg score is unaffected
        weaponScores[PG].score *= 1.0f - (1.0f - dirFactor) * speedFactor;
    }

    if (squad)
    {
        // In squad prefer MG to gun down an enemy together
        weaponScores[MG].score *= 1.75f;
    }

    int chosenWeapon = ChooseWeaponByScores(weaponScores, weaponScores + 4);

    if (chosenWeapon == WEAP_NONE)
    {
        // Bot needs to have lots of rocket since most of rockets will not hit at this distance
        float rocketScore = targetEnvironment.factor * std::min(6, RocketsReadyToFireCount()) / 6.0f;
        if (rocketScore > 0.4f)
        {
            chosenWeapon = WEAP_ROCKETLAUNCHER;
            if (!task->retreat)
                task->advance = true;
        }
    }
    Debug("(far range)... chose %s\n", GS_GetWeaponDef(chosenWeapon)->name);

    task->suggestedShootWeapon = chosenWeapon;
}

void BotBrain::SuggestMiddleRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition)
{
    const Enemy &enemy = *task->aimEnemy;
    const float distance = disposition.distance;
    const float lgRange = GetLaserRange();
    // Should be equal to max mid range distance - min mid range distance
    const float midRangeLen = lgRange - CLOSE_RANGE;
    // Relative distance from min mid range distance
    const float midRangeDistance = distance - CLOSE_RANGE;

    int chosenWeapon = WEAP_NONE;

    // First, give a bot some apriori tactics based on health status
    if (disposition.KillToBeKilledDamageRatio() < 0.75f * (2.0f * disposition.offensiveness))
        task->advance = true;
    else if (disposition.KillToBeKilledDamageRatio() > 1.5f * (2.0f * disposition.offensiveness))
        task->retreat = true;

    enum { RL, LG, PG, MG, RG, GL };
    WeaponAndScore weaponScores[6];
    weaponScores[RL].weapon = WEAP_ROCKETLAUNCHER;
    weaponScores[LG].weapon = WEAP_LASERGUN;
    weaponScores[PG].weapon = WEAP_PLASMAGUN;
    weaponScores[MG].weapon = WEAP_MACHINEGUN;
    weaponScores[RG].weapon = WEAP_RIOTGUN;
    weaponScores[GL].weapon = WEAP_GRENADELAUNCHER;

    weaponScores[RL].score = 1.5f * BoundedFraction(RocketsReadyToFireCount(), 3.0f);
    weaponScores[LG].score = 1.5f * BoundedFraction(LasersReadyToFireCount(), 15.0f);
    weaponScores[PG].score = 0.7f * BoundedFraction(PlasmasReadyToFireCount(), 15.0f);
    weaponScores[MG].score = 1.0f * BoundedFraction(BulletsReadyToFireCount(), 15.0f);
    weaponScores[RG].score = 0.7f * BoundedFraction(ShellsReadyToFireCount(), 3.0f);
    weaponScores[GL].score = 0.5f * BoundedFraction(GrenadesReadyToFireCount(), 5.0f);

    if (squad)
    {
        // In squad prefer continuous fire weapons to burn an enemy quick together
        float boost = 1.5f;
        weaponScores[LG].score *= boost;
        weaponScores[PG].score *= boost;
        weaponScores[MG].score *= boost;
    }

    if (task->advance)
    {
        weaponScores[RL].score *= 1.3f;
        weaponScores[GL].score *= 0.7f;
        weaponScores[RG].score *= 1.1f;
    }
    if (task->retreat)
    {
        // Plasma is a great defensive weapon
        weaponScores[PG].score *= 1.5f;
        weaponScores[LG].score *= 1.1f;
        weaponScores[MG].score *= 1.1f;
        weaponScores[GL].score *= 1.1f;
    }

    // 1 on mid range bound, 0 on close range bound
    float distanceFactor = (distance - CLOSE_RANGE) / (lgRange - CLOSE_RANGE);

    weaponScores[RL].score *= 1.0f - distanceFactor;
    weaponScores[LG].score *= 0.7f + 0.3f * distanceFactor;
    weaponScores[PG].score *= 1.0f - 0.4f * distanceFactor;
    weaponScores[MG].score *= 0.3f + 0.7f * distanceFactor;
    weaponScores[RG].score *= 1.0f - 0.7f * distanceFactor;
    // GL score is maximal in the middle on mid-range zone and is zero on the zone bounds
    weaponScores[GL].score *= 1.0f - fabsf(midRangeDistance - midRangeLen / 2.0f) / midRangeDistance;

    weaponScores[RL].score *= targetEnvironment.factor;
    weaponScores[LG].score *= 1.0f - 0.4f * targetEnvironment.factor;
    weaponScores[PG].score *= 0.5f + 0.5f * targetEnvironment.factor;
    weaponScores[MG].score *= 1.0f - 0.4f * targetEnvironment.factor;
    weaponScores[RG].score *= 1.0f - 0.5f * targetEnvironment.factor;
    weaponScores[GL].score *= targetEnvironment.factor;

    chosenWeapon = ChooseWeaponByScores(weaponScores, weaponScores + 6);

    // Correct tactics aposteriori based on weapons choice
    switch (chosenWeapon)
    {
        case WEAP_ROCKETLAUNCHER:
            if (!task->retreat)
                task->advance = true;
            break;
        case WEAP_LASERGUN:
            // If enemy may hit a bot with mid-range weapons that may affect bot LG accuracy by knockback
            if (enemy.RocketsReadyToFireCount() || enemy.PlasmasReadyToFireCount() || enemy.ShellsReadyToFireCount())
            {
                // Try to stay on a distance that is close to laser length (+ some delta for maneuver), but not more
                if (distance + 125.0f > lgRange)
                    task->advance = true;
                else if (distance + 250.0f < lgRange)
                    task->retreat = true;
            }
            else
            {
                if (!task->retreat)
                    task->advance = true;
            }
            break;
        case WEAP_PLASMAGUN:
            // Come closer to enemy, except enemies having RL in good for RL environment
            if (!enemy.RocketsReadyToFireCount() || targetEnvironment.factor < 0.4)
            {
                if (!task->retreat)
                    task->advance = true;
            }
            break;
        case WEAP_MACHINEGUN:
            // Don't fight with MG versus mid-range weapons
            if (enemy.LasersReadyToFireCount() > 5 || enemy.PlasmasReadyToFireCount() > 5 ||
                enemy.ShellsReadyToFireCount() || enemy.RocketsReadyToFireCount())
            {
                task->retreat = true;
            }
            // Don't come close to enemy except he is sniping
            else if (distance < 1.75f * CLOSE_RANGE)
            {
                if (enemy.PendingWeapon() == WEAP_ELECTROBOLT)
                    task->advance = true;
                else
                    task->retreat = true;
            }
            break;
        case WEAP_RIOTGUN:
            // Don't fight with RG versus stream-like weapons
            if (enemy.LasersReadyToFireCount() > 5 || enemy.PlasmasReadyToFireCount() > 5)
            {
                if (!task->advance)
                    task->retreat = true;
            }
            // Come closer to enemy, except having RL enemy in good for RL environment
            else if (!enemy.RocketsReadyToFireCount() || targetEnvironment.factor < 0.5f)
            {
                if (!task->retreat)
                    task->advance = true;
            }
            break;
        case WEAP_GRENADELAUNCHER:
            // Don't fight with GL versus stream-like weapons
            if (enemy.LasersReadyToFireCount() > 5 || enemy.PlasmasReadyToFireCount() > 5)
                task->retreat = true;
            // Don't come close to enemy except very good for GL environment
            else if (distance < lgRange / 2.0f && targetEnvironment.factor < 0.75f)
                task->retreat = true;
            else if (!task->retreat)
                task->advance = true;
            break;
        default:
            if (BoltsReadyToFireCount())
            {
                chosenWeapon = WEAP_ELECTROBOLT;
                // Retreat except, maybe, case when enemies are weak
                if (!task->advance)
                    task->retreat = true;
            }
            break;
    }

#ifdef _DEBUG
    float rl = weaponScores[RL].score;
    float lg = weaponScores[LG].score;
    float pg = weaponScores[PG].score;
    float mg = weaponScores[MG].score;
    float rg = weaponScores[RG].score;
    float gl = weaponScores[GL].score;
    const char *format = "(mid range) raw scores: RL %1.2f LG %1.2f PG %1.2f MG %1.2f RG %1.2f GL %1.2f chose %s\n";
    Debug(format, rl, lg, pg, mg, rg, gl, GS_GetWeaponDef(chosenWeapon)->name);
#endif

    task->suggestedShootWeapon = chosenWeapon;
}

void BotBrain::SuggestCloseRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition)
{
    int chosenWeapon = WEAP_NONE;

    int lasersCount = LasersReadyToFireCount();
    int rocketsCount = RocketsReadyToFireCount();
    int plasmasCount = PlasmasReadyToFireCount();

    float distanceFactor = BoundedFraction(disposition.distance, CLOSE_RANGE);

    if (g_allow_selfdamage->integer)
    {
        if (ShellsReadyToFireCount() && (disposition.damageToKill < 90 || weaponScoreRandom < 0.4))
        {
            chosenWeapon = WEAP_RIOTGUN;
            task->advance = disposition.offensiveness >= 0.5f;
        }
        else if (rocketsCount > 0 && disposition.damageToBeKilled > 100.0f - 75.0f * distanceFactor)
        {
            chosenWeapon = WEAP_ROCKETLAUNCHER;
            if (distanceFactor < 0.5)
                task->retreat = disposition.offensiveness <= 0.5f;
        }
        else if (plasmasCount > 10 && disposition.damageToBeKilled > 75.0f - 50.0f * distanceFactor)
        {
            chosenWeapon = WEAP_PLASMAGUN;
            if (distanceFactor < 0.5)
                task->retreat = true;
        }
        else if (lasersCount > 10)
        {
            chosenWeapon = WEAP_LASERGUN;
            task->retreat = true;
        }
    }
    else
    {
        if (rocketsCount)
        {
            chosenWeapon = WEAP_ROCKETLAUNCHER;
            task->advance = disposition.offensiveness >= 0.5f;
        }
        else if (lasersCount > 10)
        {
            chosenWeapon = WEAP_LASERGUN;
            // Other players will tend to attack with explosives, so bot has to retreat
            task->retreat = true;
        }
    }
    // Still not chosen
    if (chosenWeapon == WEAP_NONE)
    {
        int shellsCount = ShellsReadyToFireCount();
        if (shellsCount > 0)
            chosenWeapon = WEAP_RIOTGUN;
            // High-skill bots have high accuracy, so they may fight quite well with EB on short range.
        else if (random() < BotSkill() && BoltsReadyToFireCount())
        {
            chosenWeapon = WEAP_ELECTROBOLT;
            task->retreat = true;
        }
        else if (BulletsReadyToFireCount() > 0)
        {
            chosenWeapon = WEAP_MACHINEGUN;
            task->retreat = true;
        }
        else if (lasersCount > 0)
        {
            chosenWeapon = WEAP_LASERGUN;
            task->retreat = true;
        }
        else if (plasmasCount > 0)
        {
            chosenWeapon = WEAP_PLASMAGUN;
            task->retreat = true;
        }
        else
        {
            chosenWeapon = WEAP_GUNBLADE;
            if (disposition.KillToBeKilledDamageRatio() < 0.33f)
                task->advance = true;
            else
                task->retreat = disposition.offensiveness <= 0.5f;
        }
    }

    Debug("(close range) : chose %s \n", GS_GetWeaponDef(chosenWeapon)->name);

    task->suggestedShootWeapon = chosenWeapon;
}

int BotBrain::SuggestFinishWeapon(const Enemy &enemy, const CombatDisposition &disposition)
{
    if (disposition.distance < CLOSE_RANGE)
    {
        if (g_allow_selfdamage->integer && disposition.damageToBeKilled < 75)
        {
            if (LasersReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
                return WEAP_LASERGUN;
            if (ShellsReadyToFireCount() > 0)
                return WEAP_RIOTGUN;
            if (BulletsReadyToFireCount() > disposition.damageToKill * 0.3f * 10)
                return WEAP_MACHINEGUN;
            if (PlasmasReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
                return WEAP_PLASMAGUN;
            // Hard bots do not do this high risk action
            if (BotSkill() < 0.66f && RocketsReadyToFireCount())
                return WEAP_ROCKETLAUNCHER;
        }
        else
        {
            if (RocketsReadyToFireCount() && targetEnvironment.factor > 0)
                return WEAP_ROCKETLAUNCHER;
            if (ShellsReadyToFireCount())
                return WEAP_RIOTGUN;
            if (PlasmasReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
                return WEAP_PLASMAGUN;
            if (LasersReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
                return WEAP_LASERGUN;
            if (BulletsReadyToFireCount() > disposition.damageToKill * 0.3f * 10)
                return WEAP_MACHINEGUN;
        }
        return WEAP_GUNBLADE;
    }
    const float lgRange = GetLaserRange();
    if (disposition.distance < lgRange)
    {
        if (disposition.distance < lgRange / 2 && targetEnvironment.factor > 0.6f && RocketsReadyToFireCount())
            return WEAP_ROCKETLAUNCHER;
        if (BoltsReadyToFireCount() && disposition.damageToKill > 30 && enemy.PendingWeapon() == WEAP_LASERGUN)
            return WEAP_ELECTROBOLT;
        if (LasersReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
            return WEAP_LASERGUN;
        if (ShellsReadyToFireCount())
            return WEAP_RIOTGUN;
        if (BulletsReadyToFireCount() > disposition.damageToKill * 0.3f * 10)
            return WEAP_MACHINEGUN;
        if (PlasmasReadyToFireCount() > disposition.damageToKill * 0.3f * 14)
            return WEAP_PLASMAGUN;

        // Surprise...
        if (random() < 0.15f && BotSkill() > 0.5f && (GrenadesReadyToFireCount() && targetEnvironment.factor > 0.5f))
            return WEAP_GRENADELAUNCHER;

        return WEAP_GUNBLADE;
    }

    if (BulletsReadyToFireCount() > disposition.damageToKill * 0.3f * 10)
        return WEAP_MACHINEGUN;
    if (BoltsReadyToFireCount())
        return WEAP_ELECTROBOLT;
    if (ShellsReadyToFireCount())
        return WEAP_RIOTGUN;

    return WEAP_GUNBLADE;
}

static bool IsEscapingFromStandingEntity(const edict_t *escaping, const edict_t *standing, float escapingVelocitySqLen)
{
    // Too low relative speed with almost standing enemy
    if (escaping->speed < DEFAULT_DASHSPEED * 1.35f)
        return false;

    Vec3 escapingVelocityDir(escaping->velocity);
    escapingVelocityDir *= Q_RSqrt(escapingVelocitySqLen);

    Vec3 escapingToStandingDir(standing->s.origin);
    escapingToStandingDir -= escaping->s.origin;

    float len = escapingToStandingDir.SquaredLength();
    if (len < 1)
        return false;

    escapingToStandingDir *= Q_RSqrt(len);
    return escapingToStandingDir.Dot(escapingVelocityDir) < -0.5f;
}

bool BotBrain::IsEnemyEscaping(const Enemy &enemy, const CombatDisposition &disposition,
                               bool *botMovesFast, bool *enemyMovesFast)
{
    // Very basic. Todo: Check env. behind an enemy or the bot, is it really tries to escape or just pushed on a wall

    float botVelocitySqLen = VectorLengthSquared(bot->velocity);
    float enemyVelocitySqLen = VectorLengthSquared(enemy.ent->velocity);

    // Enemy is moving fast
    if (enemyVelocitySqLen >= DEFAULT_DASHSPEED * DEFAULT_DASHSPEED)
    {
        // Both entities are moving fast
        if (botVelocitySqLen >= DEFAULT_DASHSPEED * DEFAULT_DASHSPEED)
        {
            Vec3 botVelocityDir(bot->velocity);
            Vec3 enemyVelocityDir(enemy.ent->velocity);
            enemyVelocityDir *= Q_RSqrt(enemyVelocitySqLen);
            botVelocityDir *= Q_RSqrt(botVelocitySqLen);
            if (botVelocityDir.Dot(enemyVelocityDir) < -0.5f)
            {
                *botMovesFast = true;
                *enemyMovesFast = true;
                return true;
            }
            return false;
        }
        // Bot is standing or walking, direction of its speed does not matter
        if (IsEscapingFromStandingEntity(enemy.ent, bot, enemyVelocitySqLen))
        {
            *botMovesFast = false;
            *enemyMovesFast = true;
            return true;
        }
        return false;
    }

    // Enemy is standing or walking, direction of its speed does not matter
    if (IsEscapingFromStandingEntity(bot, enemy.ent, botVelocitySqLen))
    {
        *botMovesFast = true;
        *enemyMovesFast = false;
        return true;
    }
    return false;
}

int BotBrain::SuggestHitEscapingEnemyWeapon(const Enemy &enemy, const CombatDisposition &disposition,
                                            bool botMovesFast, bool enemyMovesFast)
{
    if (disposition.distance < CLOSE_RANGE)
    {
        Debug("(hit escaping) too small distance %.1f to change weapon, too risky\n", disposition.distance);
        return WEAP_NONE;
    }


    if (disposition.distance < GetLaserRange())
    {
        // If target will be lost out of sight, its worth to do a fast weapon switching
        // Extrapolate bot origin for 0.5 seconds
        Vec3 extrapolatedBotOrigin = 0.5f * Vec3(self->velocity) + self->s.origin;
        Vec3 predictedBotOrigin(extrapolatedBotOrigin);
        // Extrapolate enemy origin for 0.5 seconds
        Vec3 extrapolatedEnemyOrigin = 0.5f * Vec3(enemy.ent->velocity) + enemy.ent->s.origin;
        Vec3 predictedEnemyOrigin(extrapolatedEnemyOrigin);

        trace_t trace;
        // Predict bot position after 0.5 seconds
        G_Trace(&trace, self->s.origin, playerbox_stand_mins, playerbox_stand_maxs,
                extrapolatedBotOrigin.Data(), self, MASK_AISOLID);
        if (trace.fraction != 1.0f)
        {
            predictedBotOrigin = Vec3(trace.endpos);
            // Compensate Z for ground trace hit point
            if (trace.endpos[2] > extrapolatedBotOrigin.Z())
                predictedBotOrigin.Z() += std::min(24.0f, trace.endpos[2] = extrapolatedEnemyOrigin.Z());
        }
        // Predict enemy origin after 0.5 seconds
        G_Trace(&trace, const_cast<float*>(enemy.ent->s.origin), playerbox_stand_mins, playerbox_stand_maxs,
                extrapolatedEnemyOrigin.Data(), const_cast<edict_t*>(enemy.ent), MASK_AISOLID);
        if (trace.fraction != 1.0f)
        {
            predictedEnemyOrigin = Vec3(trace.endpos);
            if (trace.endpos[2] > extrapolatedEnemyOrigin.Z())
                predictedEnemyOrigin.Z() += std::min(24.0f, trace.endpos[2] - extrapolatedEnemyOrigin.Z());
        }

        // Check whether bot may hit enemy after 0.5s
        G_Trace(&trace, predictedBotOrigin.Data(), nullptr, nullptr, predictedEnemyOrigin.Data(), self, MASK_AISOLID);
        // Still may hit, keep using current weapon
        if (trace.fraction == 1.0f || enemy.ent == game.edicts + trace.ent)
            return WEAP_NONE;

        TestTargetEnvironment(Vec3(self->s.origin), enemy.LastSeenPosition(), enemy.ent);
        // Hit fast-moving enemy using EB
        if (BoltsReadyToFireCount() && (enemyMovesFast || targetEnvironment.factor < 0.5f))
            return WEAP_ELECTROBOLT;
        // Bot moves fast or target environment is good for explosives. Choose RL, GL or GB
        if (RocketsReadyToFireCount())
            return WEAP_ROCKETLAUNCHER;
        if (GrenadesReadyToFireCount())
            return WEAP_GRENADELAUNCHER;
        return WEAP_GUNBLADE;
    }

    TestTargetEnvironment(Vec3(self->s.origin), enemy.LastSeenPosition(), enemy.ent);

    enum { EB, RL, GB, MAX_WEAPONS };
    WeaponAndScore weaponScores[MAX_WEAPONS] =
    {
        WeaponAndScore(WEAP_ELECTROBOLT, BoltsReadyToFireCount() > 0),
        WeaponAndScore(WEAP_ROCKETLAUNCHER, RocketsReadyToFireCount() > 0),
        WeaponAndScore(WEAP_GUNBLADE, 0.8f)
    };

    weaponScores[EB].score *= 1.0f + 0.33f * BotSkill();
    if (enemyMovesFast)
        weaponScores[EB].score += 0.33f;

    weaponScores[RL].score *= 1.33f * (0.3f + 0.7f * targetEnvironment.factor);
    weaponScores[GB].score *= 0.6f + 0.4f * targetEnvironment.factor;
    if (botMovesFast)
        weaponScores[GB].score += 0.33f;

    weaponScores[EB].score *= 0.3f + 0.7f * BoundedFraction(disposition.distance, 2000.0f);
    weaponScores[RL].score *= 1.0f - BoundedFraction(disposition.distance, 1500.0f);
    weaponScores[GB].score *= 1.0f - 0.3f * BoundedFraction(disposition.distance, 2500.0f);

    // We are sure that weapon switch not only costs nothing, but even is intended, so do not call ChooseWeaponByScore()
    int weapon = WEAP_NONE;
    float maxScore = 0.0f;
    for (int i = 0; i < MAX_WEAPONS; ++i)
    {
        if (maxScore < weaponScores[i].score)
        {
            maxScore = weaponScores[i].score;
            weapon = weaponScores[i].weapon;
        }
    }

    constexpr const char *format = "(hit escaping) raw scores: EB %.2f RL %.2f GB %.2f chose %s\n";
    Debug(format, weaponScores[EB].score, weaponScores[RL].score, weaponScores[GB].score, WeapName(weapon));

    return weapon;
}

bool BotBrain::CheckForShotOfDespair(const Enemy &enemy, const CombatDisposition &disposition)
{
    if (BotHasPowerups())
        return false;

    // Restrict weapon switch time even more compared to generic fast switch action
    if (self->r.client->ps.stats[STAT_WEAPON_TIME] > 16)
        return false;

    float adjustedDamageToBeKilled = disposition.damageToBeKilled * (enemy.HasQuad() ? 0.25f : 1.0f);
    if (adjustedDamageToBeKilled > 25)
        return false;

    if (disposition.damageToKill < 35)
        return false;

    const float lgRange = GetLaserRange();

    if (disposition.distance > lgRange)
        return false;

    switch (enemy.PendingWeapon())
    {
        case WEAP_LASERGUN:
            return true;
        case WEAP_PLASMAGUN:
            return random() > disposition.distance / lgRange;
        case WEAP_ROCKETLAUNCHER:
            return random() > disposition.distance / lgRange;
        case WEAP_MACHINEGUN:
            return true;
        default:
            return false;
    }
}

int BotBrain::SuggestShotOfDespairWeapon(const Enemy &enemy, const CombatDisposition &disposition)
{
    // Prevent negative scores from self-damage suicide.
    int score = self->r.client->ps.stats[STAT_SCORE];
    if (level.gametype.inverseScore)
        score *= -1;

    if (score <= 0)
    {
        if (BoltsReadyToFireCount() > 0)
            return WEAP_ELECTROBOLT;
        if (ShellsReadyToFireCount() > 0)
            return WEAP_RIOTGUN;
        return WEAP_NONE;
    }

    const float lgRange = GetLaserRange();

    enum { EB, RG, RL, GB, GL, WEIGHTS_COUNT };

    WeaponAndScore scores[WEIGHTS_COUNT] =
    {
        WeaponAndScore(WEAP_ELECTROBOLT, BoltsReadyToFireCount() > 0),
        WeaponAndScore(WEAP_RIOTGUN, ShellsReadyToFireCount() > 0),
        WeaponAndScore(WEAP_ROCKETLAUNCHER, RocketsReadyToFireCount() > 0),
        WeaponAndScore(WEAP_GUNBLADE, 0.8f),
        WeaponAndScore(WEAP_GRENADELAUNCHER, 0.7f)
    };

    TestTargetEnvironment(Vec3(self->s.origin), Vec3(enemy.ent->s.origin), enemy.ent);

    // Do not touch hitscan weapons scores, we are not going to do a continuous fight
    scores[RL].score *= targetEnvironment.factor;
    scores[GL].score *= targetEnvironment.factor;
    scores[GB].score *= 0.5f + 0.5f * targetEnvironment.factor;

    // Since shots of despair are done in LG range, do not touch GB
    scores[RL].score *= 1.0f - 0.750f * disposition.distance / lgRange;
    scores[GL].score *= 1.0f - 0.999f * disposition.distance / lgRange;

    // Add extra scores for very close shots (we are not going to prevent bot suicide)
    if (disposition.distance < 150)
    {
        scores[RL].score *= 2.0f;
        scores[GL].score *= 2.0f;
    }

    // Prioritize EB for relatively far shots
    if (disposition.distance > lgRange * 0.66)
        scores[EB].score *= 1.5f;

    // Counteract some weapons with their antipodes
    switch (enemy.PendingWeapon())
    {
        case WEAP_LASERGUN:
        case WEAP_PLASMAGUN:
            scores[RL].score *= 1.75f;
            scores[GL].score *= 1.35f;
            break;
        case WEAP_ROCKETLAUNCHER:
            scores[RG].score *= 2.0f;
            break;
        default: // Shut up inspections
            break;
    }

    // Do not call ChoseWeaponByScores() which gives current weapon a slight priority, weapon switch is intended
    float bestScore = 0;
    int bestWeapon = WEAP_NONE;
    for (const auto &weaponAndScore: scores)
    {
        if (bestScore < weaponAndScore.score)
        {
            bestScore = weaponAndScore.score;
            bestWeapon = weaponAndScore.weapon;
        }
    }

    return bestWeapon;
}

int BotBrain::SuggestQuadBearerWeapon(const Enemy &enemy)
{
    float distance = (enemy.LastSeenPosition() - bot->s.origin).LengthFast();
    auto lgDef = GS_GetWeaponDef(WEAP_LASERGUN);
    auto lgRange = (lgDef->firedef.timeout + lgDef->firedef_weak.timeout) / 2.0f;
    int lasersCount = 0;
    if (Inventory()[WEAP_LASERGUN] && distance < lgRange)
    {
        if ((lgDef->firedef.timeout + lgDef->firedef_weak.timeout) / 2 < distance)
        {
            lasersCount = Inventory()[AMMO_LASERS] + Inventory()[AMMO_WEAK_LASERS];
            if (lasersCount > 7)
                return WEAP_LASERGUN;
        }
    }
    int bulletsCount = BulletsReadyToFireCount();
    if (bulletsCount > 10)
        return WEAP_MACHINEGUN;
    int plasmasCount = PlasmasReadyToFireCount();
    if (plasmasCount > 10)
        return WEAP_PLASMAGUN;
    if (ShellsReadyToFireCount())
        return WEAP_RIOTGUN;
    if (Inventory()[WEAP_ROCKETLAUNCHER])
    {
        if (RocketsReadyToFireCount() > 0 && distance > CLOSE_RANGE && distance < lgRange * 1.25f)
            return WEAP_ROCKETLAUNCHER;
    }
    if (lasersCount > 0 && distance < lgRange)
        return WEAP_LASERGUN;
    if (bulletsCount > 0)
        return WEAP_MACHINEGUN;
    if (plasmasCount > 0)
        return WEAP_PLASMAGUN;
    if (Inventory()[WEAP_GRENADELAUNCHER])
    {
        if (GrenadesReadyToFireCount() > 0 && distance > CLOSE_RANGE && distance < lgRange)
        {
            float deltaZ = bot->s.origin[2] - enemy.LastSeenPosition().Z();
            if (deltaZ < -250.0f && random() > 0.5f)
                return WEAP_GRENADELAUNCHER;
        }
    }
    return WEAP_GUNBLADE;
}

int BotBrain::SuggestInstagibWeapon(const Enemy &enemy)
{
    // Prefer hitscan weapons
    if (BulletsReadyToFireCount())
        return WEAP_MACHINEGUN;
    if (Inventory()[WEAP_LASERGUN])
    {
        auto lgDef = GS_GetWeaponDef(WEAP_LASERGUN);
        float squaredDistance = (enemy.LastSeenPosition() - bot->s.origin).SquaredLength();
        if (Inventory()[AMMO_LASERS] && squaredDistance < lgDef->firedef.timeout * lgDef->firedef.timeout)
            return WEAP_LASERGUN;
        if (Inventory()[AMMO_WEAK_LASERS] && squaredDistance < lgDef->firedef_weak.timeout * lgDef->firedef_weak.timeout)
            return WEAP_LASERGUN;
    }
    if (ShellsReadyToFireCount())
        return WEAP_RIOTGUN;

    if (Inventory()[WEAP_PLASMAGUN])
    {
        float squaredDistance = (enemy.LastSeenPosition() - bot->s.origin).SquaredLength();
        if (squaredDistance < 1000 && PlasmasReadyToFireCount())
            return WEAP_PLASMAGUN;
    }
    if (Inventory()[WEAP_INSTAGUN])
        return WEAP_INSTAGUN;
    if (BoltsReadyToFireCount())
        return WEAP_ELECTROBOLT;
    return WEAP_GUNBLADE;
}

void BotBrain::SuggestSpamEnemyWeaponAndTactics(CombatTask *task)
{
    const Enemy &enemy = *task->spamEnemy;

#ifdef _DEBUG
    {
        const Vec3 &position = enemy.LastSeenPosition();
        constexpr const char *format = "SuggestSpamWeapon...(): %s has been last seen at %f %f %f\n";
        Debug(format, enemy.Nick(), position.X(), position.Y(), position.Z());
    }
#endif
    Vec3 botToSpotVec = enemy.LastSeenPosition();
    botToSpotVec -= bot->s.origin;
    float distance = botToSpotVec.LengthFast();

    TestTargetEnvironment(Vec3(bot->s.origin), enemy.LastSeenPosition(), nullptr);

    enum { PG, MG, RL, GL, GB, WEIGHTS_COUNT };
    const float distanceBounds[WEIGHTS_COUNT] = { 5000.0f, 2000.0f, 1500.0f, 1100.0f, 3000.0f };
    float distanceFactors[WEIGHTS_COUNT];
    for (int i = 0; i < WEIGHTS_COUNT; ++i)
    {
        distanceFactors[i] = BoundedFraction(distance, distanceBounds[i]);
    }

    WeaponAndScore scores[5] =
    {
        WeaponAndScore(WEAP_PLASMAGUN, 1.0f * BoundedFraction(PlasmasReadyToFireCount(), 30)),
        WeaponAndScore(WEAP_MACHINEGUN, 0.7f * BoundedFraction(BulletsReadyToFireCount(), 30)),
        WeaponAndScore(WEAP_ROCKETLAUNCHER, 1.2f * BoundedFraction(RocketsReadyToFireCount(), 5)),
        WeaponAndScore(WEAP_GRENADELAUNCHER, 1.0f * BoundedFraction(GrenadesReadyToFireCount(), 3)),
        WeaponAndScore(WEAP_GUNBLADE, 0.5f)
    };

    // All scores decrease linearly with distance (except for MG)
    scores[PG].score *= 1.0f - 0.8f * distanceFactors[PG];
    scores[MG].score *= distanceFactors[MG];
    scores[RL].score *= 0.5f - 0.5f * distanceFactors[RL];
    scores[GL].score *= 1.0f - distanceFactors[GL];
    scores[GB].score *= 1.0f - 0.5f * distanceFactors[GB];

    scores[PG].score *= 0.7f + 0.3f * targetEnvironment.factor;
    scores[MG].score *= 1.0f - targetEnvironment.factor;
    scores[RL].score *= 0.2f + 0.8f * targetEnvironment.factor;
    scores[GL].score *= targetEnvironment.factor;
    scores[GB].score *= 0.4f + 0.6 * targetEnvironment.factor;

    float deltaZ = bot->s.origin[2] - enemy.LastSeenPosition().Z();
    float deltaZBound = g_gravity->value / 2;
    // 0 for enemies that are deltaZBound or more units higher, 1 for enemies that are deltaZBound or more lower
    float gravityFactor = (std::min(deltaZ, deltaZBound) / deltaZBound + 1.0f) / 2.0f;
    scores[GL].score *= 1.5f * gravityFactor;

    // Don't hit itself
    if (distance < 200.0f)
        scores[RL].score = 0;
    if (distance < 250.0f)
        scores[GB].score = 0;
    if (distance < 50.0f)
        scores[PG].score = 0;
    if (distance < 70.0f)
        scores[GB].score = 0;

    int weapon = ChooseWeaponByScores(scores, scores + 5);

    // Do not spam if there are no spam weapons and the enemy is close, choose a weapon to be ready to fight
    if (weapon <= WEAP_GUNBLADE && level.time - enemy.LastSeenAt() < 1000)
    {
        task->inhibit = true;

        if (distance < GetLaserRange())
        {
            if (LasersReadyToFireCount() > 15)
                weapon = WEAP_LASERGUN;
            else if (ShellsReadyToFireCount() > 0)
                weapon = WEAP_RIOTGUN;
        }
        else if (BoltsReadyToFireCount() > 0)
            weapon = WEAP_ELECTROBOLT;
        else if (PlasmasReadyToFireCount() > 0)
            weapon = WEAP_PLASMAGUN;
    }

    constexpr const char *fmt = "spam raw weapon scores: PG %.2f MG %.2f RL %.2f GL %.2f GB %.2f chose %s\n";
    Debug(fmt, scores[PG].score, scores[MG].score, scores[RL].score, scores[GL].score, scores[GB].score, WeapName(weapon));

    task->suggestedSpamWeapon = weapon;
}

const float BotBrain::TargetEnvironment::TRACE_DEPTH = 250.0f;

void BotBrain::TestTargetEnvironment(const Vec3 &botOrigin, const Vec3 &targetOrigin, const edict_t *traceKey)
{
    Vec3 forward = targetOrigin - botOrigin;
    forward.Z() = 0;
    float frontSquareLen = forward.SquaredLength();
    if (frontSquareLen > 1)
    {
        // Normalize
        forward *= Q_RSqrt(frontSquareLen);
    }
    else
    {
        // Pick dummy horizontal direction
        forward = Vec3(&axis_identity[AXIS_FORWARD]);
    }
    Vec3 right = Vec3(&axis_identity[AXIS_UP]).Cross(forward);

    // Now botToTarget is a normalized horizontal part of botToTarget - botOrigin

    edict_t *passedict = const_cast<edict_t*>(traceKey);
    float *start = const_cast<float*>(botOrigin.Data());
    trace_t *traces = targetEnvironment.sideTraces;

    const float TRACE_DEPTH = TargetEnvironment::TRACE_DEPTH;

    vec_t offsets[6 * 3];
    VectorSet(offsets + 3 * TargetEnvironment::TOP, 0, 0, +TRACE_DEPTH);
    VectorSet(offsets + 3 * TargetEnvironment::BOTTOM, 0, 0, -TRACE_DEPTH);
    VectorSet(offsets + 3 * TargetEnvironment::FRONT, -TRACE_DEPTH, 0, 0);
    VectorSet(offsets + 3 * TargetEnvironment::BACK, +TRACE_DEPTH, 0, 0);
    VectorSet(offsets + 3 * TargetEnvironment::LEFT, 0, -TRACE_DEPTH, 0);
    VectorSet(offsets + 3 * TargetEnvironment::RIGHT, 0, +TRACE_DEPTH, 0);

    vec3_t mins = { -32, -32, -32 };
    vec3_t maxs = { +32, +32, +32 };

    float factor = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        vec3_t end;
        trace_t *trace = traces + i;
        G_ProjectSource(start, offsets + 3 * i, forward.Data(), right.Data(), end);
        G_Trace(trace, start, mins, maxs, end, passedict, MASK_AISOLID);
        // Give some non-zero score by the fact that trace is detected a hit itself
        if (trace->fraction < 1.0f)
            factor += 1.0f / 6.0f;
        // Compute a dot product between a bot-to-target direction and trace-point-to-target direction
        // If the dot product is close to 1, a bot may shoot almost perpendicular to the traced point.
        // If trace point is itself close to a target, bot may inflict enemy lots of damage by rockets.
        Vec3 hitPointToTarget = targetOrigin - trace->endpos;
        if (trace->fraction > 0.01f)
        {
            // Normalize
            hitPointToTarget *= 1.0f / (TRACE_DEPTH * trace->fraction);
            factor += 1.5f * (1.0f - trace->fraction) * fabsf(hitPointToTarget.Dot(forward));
        }
        else
        {
            // The target is very close to a traced solid surface anyway, use 1 instead of a dot product.
            factor += 1.5f * (1.0f - trace->fraction);
        }
    }

    targetEnvironment.factor = std::min(1.0f, factor / 6.0f);
}

void BotBrain::SetAttitude(const edict_t *ent, int attitude)
{
    int entNum = ENTNUM(const_cast<edict_t*>(ent));
    oldAttitude[entNum] = this->attitude[entNum];
    this->attitude[entNum] = (signed char)attitude;

    if (oldAttitude[entNum] < 0 && attitude >= 0)
    {
        botEnemyPool.Forget(ent);
        if (squad)
            activeEnemyPool->Forget(ent);
    }
}

float BotBrain::GetEffectiveOffensiveness() const
{
    if (!squad)
    {
        if (combatTask.aimEnemy && IsCarrier(combatTask.aimEnemy->ent))
            return 0.65f + 0.35f * decisionRandom;
        return baseOffensiveness;
    }
    return squad->IsSupporter(self) ? 1.0f : 0.0f;
}

bool BotBrain::MayNotBeFeasibleEnemy(const edict_t *ent) const
{
    // Only valid clients may be feasible enemies
    if (!ent->r.inuse || !ent->r.client)
        return true;
    // Skip non-spawned clients
    if (G_ISGHOSTING(ent))
        return true;
    // Skip chatting clients or "notarget" cheat users
    if (ent->flags & (FL_NOTARGET|FL_BUSY))
        return true;
    // Skip teammates. Note that team overrides attitude
    if (GS_TeamBasedGametype() && ent->s.team == self->s.team)
        return true;
    // Skip entities that has a non-negative bot attitude
    if (attitude[ENTNUM(const_cast<edict_t*>(ent))] >= 0)
        return true;
    // Skip the bot itself
    if (ent == self)
        return true;

    return false;
}

void BotBrain::UpdatePotentialGoalsWeights()
{
    // Compute it once, not on each loop step
    bool onlyGotGB = true;
    for (int weapon = WEAP_GUNBLADE + 1; weapon < WEAP_TOTAL; ++weapon)
    {
        if (Inventory()[weapon])
        {
            onlyGotGB = false;
            break;
        }
    }

    // Weights are set to zero by caller code.
    // Only non-zero weights should be set.
    FOREACH_NAVENT(goalEnt)
    {
        // Picking clients as goal entities is currently disabled
        if (goalEnt->IsClient())
            continue;

        if (goalEnt->Item())
            internalEntityWeights[goalEnt->Id()] = ComputeItemWeight(goalEnt->Item(), onlyGotGB);
    }
}

float BotBrain::ComputeItemWeight(const gsitem_t *item, bool onlyGotGB) const
{
    switch (item->type)
    {
        case IT_WEAPON: return ComputeWeaponWeight(item, onlyGotGB);
        case IT_AMMO: return ComputeAmmoWeight(item);
        case IT_HEALTH: return ComputeHealthWeight(item);
        case IT_ARMOR: return ComputeArmorWeight(item);
        case IT_POWERUP: return ComputePowerupWeight(item);
    }
    return 0;
}

float BotBrain::ComputeWeaponWeight(const gsitem_t *item, bool onlyGotGB) const
{
    if (Inventory()[item->tag])
    {
        // TODO: Precache
        const gsitem_t *ammo = GS_FindItemByTag(item->ammo_tag);
        if (Inventory()[ammo->tag] >= ammo->inventory_max)
            return 0;

        float ammoQuantityFactor = 1.0f - Inventory()[ammo->tag] / (float)ammo->inventory_max;

        switch (item->tag)
        {
            case WEAP_ELECTROBOLT: return ammoQuantityFactor;
            case WEAP_LASERGUN: return ammoQuantityFactor * 1.1f;
            case WEAP_PLASMAGUN: return ammoQuantityFactor * 1.1f;
            case WEAP_ROCKETLAUNCHER: return ammoQuantityFactor;
            default: return 0.5f * ammoQuantityFactor;
        }
    }

    // We may consider plasmagun in a bot's hand as a top tier weapon too
    const int topTierWeapons[4] = { WEAP_ELECTROBOLT, WEAP_LASERGUN, WEAP_ROCKETLAUNCHER, WEAP_PLASMAGUN };

    // TODO: Precompute
    float topTierWeaponGreed = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        if (!Inventory()[topTierWeapons[i]])
            topTierWeaponGreed += 1.0f;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (topTierWeapons[i] == item->tag)
            return (onlyGotGB ? 1.5f : 0.9f) + (topTierWeaponGreed - 1.0f) / 3.0f;
    }

    return onlyGotGB ? 1.5f : 0.7f;
}

float BotBrain::ComputeAmmoWeight(const gsitem_t *item) const
{
    if (Inventory()[item->tag] < item->inventory_max)
    {
        float quantityFactor = 1.0f - Inventory()[item->tag] / (float)item->inventory_max;

        for (int weapon = WEAP_GUNBLADE; weapon < WEAP_TOTAL; weapon++)
        {
            // TODO: Preache
            const gsitem_t *weaponItem = GS_FindItemByTag( weapon );
            if (weaponItem->ammo_tag == item->tag)
            {
                if (Inventory()[weaponItem->tag])
                {
                    switch (weaponItem->tag)
                    {
                        case WEAP_ELECTROBOLT: return quantityFactor;
                        case WEAP_LASERGUN: return quantityFactor * 1.1f;
                        case WEAP_PLASMAGUN: return quantityFactor * 1.1f;
                        case WEAP_ROCKETLAUNCHER: return quantityFactor;
                        default: return 0.5f * quantityFactor;
                    }
                }
                return quantityFactor * 0.33f;
            }
        }
    }
    return 0.0;
}

float BotBrain::ComputeHealthWeight(const gsitem_t *item) const
{
    if (item->tag == HEALTH_MEGA || item->tag == HEALTH_ULTRA)
        return 2.5f;

    if (item->tag == HEALTH_SMALL)
        return 0.2f + 0.3f * (1.0f - self->health / (float)self->max_health);

    return std::max(0.0f, 1.0f - self->health / (float)self->max_health);
}

float BotBrain::ComputeArmorWeight(const gsitem_t *item) const
{
    float currArmor = bot->r.client->resp.armor;
    switch (item->tag)
    {
        case ARMOR_RA:
            return currArmor < 150.0f ? 2.0f : 0.0f;
        case ARMOR_YA:
            return currArmor < 125.0f ? 1.7f : 0.0f;
        case ARMOR_GA:
            return currArmor < 100.0f ? 1.4f : 0.0f;
        case ARMOR_SHARD:
        {
            if (currArmor < 25 || currArmor >= 150)
                return 0.4f;
            return 0.25f;
        }
    }
    return 0;
}

float BotBrain::ComputePowerupWeight(const gsitem_t *item) const
{
    // TODO: Make it dependent of current health/armor status;
    return 3.5f;
}