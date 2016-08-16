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
    const Enemy *enemy;

    int suggestedWeapon;
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

    const edict_t *EnemyEnt() const
    {
        if (!enemy)
            FailWith("There is no aim enemy");
        if (!enemy->ent)
            FailWith("AimEnemy is present but ent is null (this means this enemy slot has been released)");
        return enemy->ent;
    }

public:
    // Used to distinguish different combat tasks and track combat task changes
    // (e.g. when some parameters that depend of a combat task are cached by some external code
    // and instanceId of the combat task has been changed, its time to invalidate cached parameters)
    // When combat task is empty, instanceId is 0.
    // When combat task is updated, instanceId is set to a number unique for all combat tasks of this bot.
    unsigned instanceId;

    bool advance;
    bool retreat;
    bool inhibit;

    // Must be set back to false by shooting code.
    bool importantShot;

    CombatTask()
    {
        Clear();
    };

    void Clear()
    {
        enemy = nullptr;
        instanceId = 0;
        suggestedWeapon = WEAP_NONE;
        suggestedSpamWeapon = WEAP_NONE;
        advance = false;
        retreat = false;
        inhibit = false;
        importantShot = false;
    }

    bool Empty() const  { return !enemy; }

    bool IsTargetAStaticSpot() const { return !enemy; }

    bool IsOnGround() const { return enemy && enemy->ent->groundentity; }

    Vec3 EnemyOrigin() const
    {
        if (enemy) return enemy->LastSeenPosition();
        FailWith("EnemyOrigin(): combat task is empty\n");
    }

    Vec3 EnemyVelocity() const
    {
        if (enemy) return Vec3(EnemyEnt()->velocity);
        FailWith("EnemyVelocity(): combat task is empty\n");
    }

    Vec3 EnemyMins() const
    {
        if (enemy) return Vec3(EnemyEnt()->r.mins);
        FailWith("EnemyMins(): combat task is empty\n");
    }

    Vec3 EnemyMaxs() const
    {
        if (enemy) return Vec3(EnemyEnt()->r.maxs);
        FailWith("EnemyMaxs(): combat task is empty\n");
    }

    Vec3 EnemyLookDir() const
    {
        if (enemy)
        {
            vec3_t forward;
            AngleVectors(EnemyEnt()->s.angles, forward, nullptr, nullptr);
            return Vec3(forward);
        }
        FailWith("EnemyLookDir(): aim enemy is not present");
    }

    Vec3 EnemyAngles() const
    {
        if (enemy)
            return Vec3(EnemyEnt()->s.angles);
        FailWith("EnemyAngles(): aim enemy is not present");
    }

    unsigned EnemyFireDelay() const
    {
        if (enemy && enemy->ent)
        {
            if (!enemy->ent->r.client)
                return 0;
            return (unsigned)enemy->ent->r.client->ps.stats[STAT_WEAPON_TIME];
        }
        return std::numeric_limits<unsigned>::max();
    }

    int Weapon() const
    {
        if (enemy) return suggestedWeapon;
        FailWith("Weapon(): combat task is empty\n");
    }

    const edict_t *TraceKey() const { return enemy ? enemy->ent : nullptr; }
};

struct CombatDisposition
{
    float damageToKill;
    float damageToBeKilled;
    float distance;
    float offensiveness;
    bool isOutnumbered;

    inline float KillToBeKilledDamageRatio() const { return damageToKill / damageToBeKilled; }
};

class BotBrain: public AiBaseBrain
{
    friend class Bot;

    edict_t *bot;
    float baseOffensiveness;
    const float skillLevel;
    const unsigned reactionTime;

    const unsigned aimTargetChoicePeriod;
    const unsigned idleTargetChoicePeriod;
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
    int SuggestFinishWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    bool IsEnemyEscaping(const Enemy &enemy, const CombatDisposition &disposition,
                         bool *botMovesFast, bool *enemyMovesFast);
    int SuggestHitEscapingEnemyWeapon(const Enemy &enemy, const CombatDisposition &disposition,
                                      bool botMovesFast, bool enemyMovesFast);
    bool CheckForShotOfDespair(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestShotOfDespairWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestQuadBearerWeapon(const Enemy &enemy);
    bool SuggestPointToTurnToWhenEnemyIsLost(const Enemy *oldEnemy);
    void SuggestPursuitTask(CombatTask *task);
    void TryStartPursuit(CombatTask *task, const Enemy *enemy);
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

    virtual void OnGoalCleanedUp(const Goal *goalEnt) override;
    virtual void OnClearSpecialGoalRequested() override;

    bool IsGoalATopTierItem() const;
    unsigned GoalSpawnTime() const;
    bool HasMoreImportantTasksThanEnemies() const;
    bool StartPursuit(const Enemy &enemy, unsigned timeout = 1000);
    bool SetTacticalSpot(const Vec3 &origin, unsigned timeout = 1000);
    virtual bool ShouldCancelSpecialGoalBySpecificReasons() override;

    void CheckTacticalPosition();
    void UpdateBlockedAreasStatus();

    inline bool HasSpecialGoal() const { return specialGoal != nullptr; }

    bool IsSpecialGoalSetBy(const AiFrameAwareUpdatable *setter) const
    {
        return specialGoal->Setter() == setter;
    }

    unsigned specialGoalCombatTaskId;

    NavEntity localNavEntity;

    void SetSpecialGoalFromEntity(edict_t *entity, const AiFrameAwareUpdatable *setter);

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

    signed char attitude[MAX_EDICTS];
    // Used to detect attitude change
    signed char oldAttitude[MAX_EDICTS];
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
    void AfterAllEnemiesViewed() {}
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
        return combatTask.enemy && combatTask.enemy->ent == enemy;
    }

    void SetAttitude(const edict_t *ent, int attitude);

    inline float GetBaseOffensiveness() const { return baseOffensiveness; }

    float GetEffectiveOffensiveness() const;

    inline void SetBaseOffensiveness(float baseOffensiveness)
    {
        this->baseOffensiveness = baseOffensiveness;
        clamp(baseOffensiveness, 0.0f, 1.0f);
    }

    // Helps to reject non-feasible enemies quickly.
    // A false result does not guarantee that enemy is feasible.
    // A true result guarantees that enemy is not feasible.
    bool MayNotBeFeasibleEnemy(const edict_t *ent) const;

    virtual void UpdatePotentialGoalsWeights() override;
};

#endif
