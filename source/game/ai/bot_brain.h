#ifndef QFUSION_BOT_BRAIN_H
#define QFUSION_BOT_BRAIN_H

#include <stdarg.h>
#include "ai_base_ai.h"
#include "ai_base_brain.h"
#include "ai_base_enemy_pool.h"
#include "bot.h"

class CombatTask
{
    friend class BotBrain;
    // If it is not null, it is a chosen target for shooting.
    const Enemy *aimEnemy;

    // May be null if spamming is not personal for the enemy.
    // Used to cancel spamming if enemy is killed by bot or other player
    // (this means there is no sense to do spamming).
    const Enemy *spamEnemy;
    Vec3 spamSpot;

    int suggestedShootWeapon;
    int suggestedSpamWeapon;
#ifndef _MSC_VER
    inline void FailWith(const char *message) const __attribute__ ((noreturn))
#else
    __declspec(noreturn) inline void FailWith(const char *message) const
#endif
    {
        fputs("CombatTask ", stderr);
        fputs(message, stderr);
        fflush(stderr);
        abort();
    }

    const edict_t *AimEnemyEnt() const
    {
        if (!aimEnemy)
            FailWith("There is no aim enemy");
        if (!aimEnemy->ent)
            FailWith("AimEnemy is present but ent is null (this means this enemy slot has been released)");
        return aimEnemy->ent;
    }

public:

    // When level.time == spamTimesOutAt, stop spamming
    unsigned spamTimesOutAt;

    // Used to distinguish different combat tasks and track combat task changes
    // (e.g. when some parameters that depend of a combat task are cached by some external code
    // and instanceId of the combat task has been changed, its time to invalidate cached parameters)
    // When combat task is empty, instanceId is 0.
    // When combat task is updated, instanceId is set to a number unique for all combat tasks of this bot.
    unsigned instanceId;

    bool spamKeyPoint;
    bool advance;
    bool retreat;
    bool inhibit;

    // Must be set back to false by shooting code.
    bool importantShot;

    CombatTask() : spamSpot(vec3_origin)
    {
        Clear();
    };

    void Clear()
    {
        aimEnemy = nullptr;
        spamEnemy = nullptr;
        spamTimesOutAt = level.time;
        instanceId = 0;
        suggestedShootWeapon = WEAP_NONE;
        suggestedSpamWeapon = WEAP_NONE;
        spamKeyPoint = false;
        advance = false;
        retreat = false;
        inhibit = false;
        importantShot = false;
    }

    bool Empty() const  { return !aimEnemy && !spamEnemy; }

    bool IsTargetAStaticSpot() const { return !aimEnemy; }

    bool IsOnGround() const { return aimEnemy && aimEnemy->ent->groundentity; }

    Vec3 EnemyOrigin() const
    {
        if (aimEnemy) return aimEnemy->LastSeenPosition();
        if (spamEnemy) return spamSpot;
        FailWith("EnemyOrigin(): combat task is empty\n");
    }

    Vec3 EnemyVelocity() const
    {
        if (aimEnemy) return Vec3(AimEnemyEnt()->velocity);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyVelocity(): combat task is empty\n");
    }

    Vec3 EnemyMins() const
    {
        if (aimEnemy) return Vec3(AimEnemyEnt()->r.mins);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyMins(): combat task is empty\n");
    }

    Vec3 EnemyMaxs() const
    {
        if (aimEnemy) return Vec3(AimEnemyEnt()->r.maxs);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyMaxs(): combat task is empty\n");
    }

    Vec3 EnemyLookDir() const
    {
        if (aimEnemy)
        {
            vec3_t forward;
            AngleVectors(AimEnemyEnt()->s.angles, forward, nullptr, nullptr);
            return Vec3(forward);
        }
        FailWith("EnemyLookDir(): aim enemy is not present");
    }

    Vec3 EnemyAngles() const
    {
        if (aimEnemy)
            return Vec3(AimEnemyEnt()->s.angles);
        FailWith("EnemyAngles(): aim enemy is not present");
    }

    unsigned EnemyFireDelay() const
    {
        if (aimEnemy && aimEnemy->ent)
        {
            if (!aimEnemy->ent->r.client)
                return 0;
            return (unsigned)aimEnemy->ent->r.client->ps.stats[STAT_WEAPON_TIME];
        }
        return std::numeric_limits<unsigned>::max();
    }

    const int Weapon()
    {
        if (aimEnemy) return suggestedShootWeapon;
        if (spamEnemy) return suggestedSpamWeapon;
        FailWith("Weapon(): combat task is empty\n");
    }

    const edict_t *TraceKey() const { return aimEnemy ? aimEnemy->ent : nullptr; }
};

struct CombatDisposition
{
    float damageToKill;
    float damageToBeKilled;
    float distance;

    inline float KillToBeKilledDamageRatio() const { return damageToKill / damageToBeKilled; }
};

class BotBrain: public AiBaseBrain
{
    friend class Bot;

    edict_t *bot;
    const float skillLevel;
    const unsigned reactionTime;

    const unsigned aimTargetChoicePeriod;
    const unsigned spamTargetChoicePeriod;
    const unsigned aimWeaponChoicePeriod;
    const unsigned spamWeaponChoicePeriod;

    unsigned combatTaskInstanceCounter;

    unsigned nextTargetChoiceAt;
    unsigned nextWeaponChoiceAt;

    unsigned nextFastWeaponSwitchActionCheckAt;

    unsigned prevThinkLevelTime;

    // These values are loaded from cvars for fast access
    const float armorProtection;
    const float armorDegradation;

    // Used in weapon choice. Must be a value in [-0.5, 0.5], high skill bots should have lesser absolute value.
    // Should be updated once per some seconds in PrepareToFrame()
    // If you update this value each frame, you will cause a weapon score "jitter",
    // when bots tries to change weapon infinitely since on each frame different weapon is chosen.
    float weaponScoreRandom;
    unsigned nextWeaponScoreRandomUpdate;

    // 0..1, does not depend of skill
    float decisionRandom;
    unsigned nextDecisionRandomUpdate;

    struct TargetEnvironment
    {
        // Sides are relative to direction from bot origin to target origin
        // Order: top, bottom, front, back, left, right
        trace_t sideTraces[6];

        enum Side { TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT };

        float factor;
        static const float TRACE_DEPTH;
    };

    TargetEnvironment targetEnvironment;

    // This var holds memory referred by AiBaseBrain::specialGoal
    NavEntity localSpecialGoal;

    inline unsigned NextCombatTaskInstanceId() { return combatTaskInstanceCounter++; }

    inline bool BotHasQuad() const { return ::HasQuad(bot); }
    inline bool BotHasShell() const { return ::HasShell(bot); }
    inline bool BotHasPowerups() const { return ::HasPowerups(bot); }
    inline bool BotIsCarrier() const { return ::IsCarrier(bot); }
    float BotSkill() const { return skillLevel; }

    inline float DamageToKill(const edict_t *client) const
    {
        return ::DamageToKill(client, armorProtection, armorDegradation);
    }

    inline const int *Inventory() const { return bot->r.client->ps.inventory; }

    template <int Weapon> inline int AmmoReadyToFireCount() const
    {
        if (!Inventory()[Weapon])
            return 0;
        return Inventory()[WeaponAmmo<Weapon>::strongAmmoTag] + Inventory()[WeaponAmmo<Weapon>::weakAmmoTag];
    }

    inline int ShellsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_RIOTGUN>(); }
    inline int GrenadesReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_GRENADELAUNCHER>(); }
    inline int RocketsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ROCKETLAUNCHER>(); }
    inline int PlasmasReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_PLASMAGUN>(); }
    inline int BulletsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_MACHINEGUN>(); }
    inline int LasersReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_LASERGUN>(); }
    inline int BoltsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ELECTROBOLT>(); }

    void SuggestAimWeaponAndTactics(CombatTask *task);
    void SuggestSniperRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestFarRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestMiddleRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestCloseRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    int SuggestInstagibWeapon(const Enemy &enemy);
    int SuggestEasyBotsWeapon(const Enemy &enemy);
    void SuggestSpamEnemyWeaponAndTactics(CombatTask *task);
    int SuggestFinishWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    bool IsEnemyEscaping(const Enemy &enemy, const CombatDisposition &disposition,
                         bool *botMovesFast, bool *enemyMovesFast);
    int SuggestHitEscapingEnemyWeapon(const Enemy &enemy, const CombatDisposition &disposition,
                                      bool botMovesFast, bool enemyMovesFast);
    bool CheckForShotOfDespair(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestShotOfDespairWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestQuadBearerWeapon(const Enemy &enemy);
    bool SuggestPointToTurnToWhenEnemyIsLost(const Enemy *oldEnemy);
    void SuggestPursuitOrSpamTask(CombatTask *task);
    void StartSpamAtEnemyOrPursuit(CombatTask *task, const Enemy *enemy);
    int ChooseWeaponByScores(struct WeaponAndScore *begin, struct WeaponAndScore *end);
    void TestTargetEnvironment(const Vec3 &botOrigin, const Vec3 &targetOrigin, const edict_t *traceKey);
    void UpdateKeptCurrentCombatTask();
    void TryFindNewCombatTask();
    bool CheckFastWeaponSwitchAction();
    CombatDisposition GetCombatDisposition(const Enemy &enemy);

    float ComputeItemWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeWeaponWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeAmmoWeight(const gsitem_t *item) const;
    float ComputeArmorWeight(const gsitem_t *item) const;
    float ComputeHealthWeight(const gsitem_t *item) const;
    float ComputePowerupWeight(const gsitem_t *item) const;

    virtual void OnGoalCleanedUp(const NavEntity *goalEnt) override;
    virtual bool MayNotBeFeasibleGoal(const NavEntity *goalEnt) override;
    virtual void OnClearSpecialGoalRequested() override;

    bool MayPathToAreaBeBlocked(int goalAreaNum) const;


    bool HasMoreImportantTasksThanEnemies() const;
    bool StartPursuit(const Enemy &enemy, unsigned timeout = 1000);
    bool SetTacticalSpot(const Vec3 &origin, unsigned timeout = 1000);
    virtual bool ShouldCancelSpecialGoalBySpecificReasons() override;

    void CheckTacticalPosition();

    inline bool HasSpecialGoal() const { return specialGoal != nullptr; }

    bool IsSpecialGoalSetBy(const AiFrameAwareUpdatable *setter) const
    {
        // If special goal setter is defined
        if (specialGoal->setter)
        {
            // Pointers should match exactly
            if (specialGoal->setter == setter)
                return true;
        }
        else
        {
            // If there is no special goal setter, treat it as set by bot or bot's brain
            if (setter == (AiFrameAwareUpdatable*)bot->ai->botRef)
                return true;
            if (setter == this)
                return true;
        }
        return false;
    }

    inline void SetSpecialGoalFromEntity(edict_t *entity, const AiFrameAwareUpdatable *setter)
    {
        localSpecialGoal.Clear();
        localSpecialGoal.goalFlags = GoalFlags::DROPPED_ENTITY | GoalFlags::REACH_ENTITY | GoalFlags::REACH_AT_TOUCH;
        localSpecialGoal.ent = entity;
        localSpecialGoal.aasAreaNum = AAS_PointAreaNum(entity->s.origin);
        localSpecialGoal.setter = setter;
        SetSpecialGoal(&localSpecialGoal);
    }

    BotBrain() = delete;
    // Disable copying and moving
    BotBrain(BotBrain &&that) = delete;

    void ResetCombatTask()
    {
        oldCombatTask = combatTask;
        combatTask.Clear();
        nextTargetChoiceAt = level.time;
    }

    class EnemyPool: public AiBaseEnemyPool
    {
        friend class BotBrain;
        edict_t *bot;
        BotBrain *botBrain;
    protected:
        void OnNewThreat(const edict_t *newThreat) override;
        bool CheckHasQuad() const override { return ::HasQuad(bot); }
        bool CheckHasShell() const override { return ::HasShell(bot); }
        float ComputeDamageToBeKilled() const override { return DamageToKill(bot); }
        void OnEnemyRemoved(const Enemy *enemy) override;
        void TryPushNewEnemy(const edict_t *enemy) override { TryPushEnemyOfSingleBot(bot, enemy); }
        void SetBotRoleWeight(const edict_t *bot, float weight) override {}
        float GetAdditionalEnemyWeight(const edict_t *bot, const edict_t *enemy) const override { return 0; }
        void OnBotEnemyAssigned(const edict_t *bot, const Enemy *enemy) override {}
    public:
        EnemyPool(edict_t *bot, BotBrain *botBrain, float skill)
            : AiBaseEnemyPool(skill), bot(bot), botBrain(botBrain)
        {
            SetTag(va("BotBrain(%s)::EnemyPool", bot->r.client->netname));
        }
        virtual ~EnemyPool() override {}
    };

    class AiSquad *squad;
    EnemyPool botEnemyPool;
    AiBaseEnemyPool *activeEnemyPool;

    CombatTask oldCombatTask;
protected:
    virtual void SetFrameAffinity(unsigned modulo, unsigned offset) override
    {
        // Call super method first
        AiBaseBrain::SetFrameAffinity(modulo, offset);
        // Allow bot's own enemy pool to think
        botEnemyPool.SetFrameAffinity(modulo, offset);
    }
public:
    CombatTask combatTask;

    BotBrain(edict_t *bot, float skillLevel);

    virtual void Frame() override;
    virtual void Think() override;
    virtual void PreThink() override;
    virtual void PostThink() override;

    void OnAttachedToSquad(AiSquad *squad);
    void OnDetachedFromSquad(AiSquad *squad);

    void OnNewThreat(const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector);
    void OnEnemyRemoved(const Enemy *enemy);

    inline unsigned MaxTrackedEnemies() const { return botEnemyPool.MaxTrackedEnemies(); }

    void OnEnemyViewed(const edict_t *enemy);
    // Call it after all calls to OnEnemyViewed()
    void AfterAllEnemiesViewed();
    void OnPain(const edict_t *enemy, float kick, int damage);
    void OnEnemyDamaged(const edict_t *target, int damage);

    // In these calls use not active but bot's own enemy pool
    // (this behaviour is expected by callers, otherwise referring to a squad enemy pool is enough)
    inline unsigned LastAttackedByTime(const edict_t *attacker) const
    {
        return botEnemyPool.LastAttackedByTime(attacker);
    }
    inline unsigned LastTargetTime(const edict_t *target) const
    {
        return botEnemyPool.LastTargetTime(target);
    }

    inline bool IsPrimaryAimEnemy(const edict_t *enemy) const
    {
        return combatTask.aimEnemy && combatTask.aimEnemy->ent == enemy;
    }
    inline bool IsOldHiddenEnemy(const edict_t *enemy) const
    {
        return combatTask.spamEnemy && combatTask.spamEnemy->ent == enemy;
    }

    virtual void UpdatePotentialGoalsWeights() override;
};

#endif
