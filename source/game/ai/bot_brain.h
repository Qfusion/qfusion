#ifndef QFUSION_BOT_BRAIN_H
#define QFUSION_BOT_BRAIN_H

#include <stdarg.h>
#include "ai_base_ai.h"
#include "ai_base_brain.h"
#include "ai_base_enemy_pool.h"
#include "bot_items_selector.h"
#include "bot_weapon_selector.h"

class BotBrain: public AiBaseBrain
{
    friend class Bot;
    friend class BotItemsSelector;

    edict_t *bot;

    float baseOffensiveness;
    const float skillLevel;
    const unsigned reactionTime;

    unsigned nextTargetChoiceAt;
    const unsigned targetChoicePeriod;

    BotItemsSelector itemsSelector;

    unsigned prevThinkLevelTime;

    // These values are loaded from cvars for fast access
    const float armorProtection;
    const float armorDegradation;

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

    inline bool WillAdvance() const { abort(); }
    inline bool WillRetreat() const { abort(); }

    inline bool ShouldCloak() const { abort(); }
    inline bool ShouldBeSilent() const { abort(); }
    inline bool ShouldAttack() const { abort(); }

    inline bool ShouldKeepXhairOnEnemy() const { abort(); }

    inline bool WillAttackMelee() const { abort(); }
    inline bool ShouldRushHeadless() const { abort(); }

    void UpdateBlockedAreasStatus();

    void PrepareCurrWorldState(WorldState *worldState) override;

    BotBrain() = delete;
    // Disable copying and moving
    BotBrain(BotBrain &&that) = delete;

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
        void SetBotRoleWeight(const edict_t *bot_, float weight) override {}
        float GetAdditionalEnemyWeight(const edict_t *bot_, const edict_t *enemy) const override { return 0; }
        void OnBotEnemyAssigned(const edict_t *bot_, const Enemy *enemy) override {}
    public:
        EnemyPool(edict_t *bot_, BotBrain *botBrain_, float skill_)
            : AiBaseEnemyPool(skill_), bot(bot_), botBrain(botBrain_)
        {
            SetTag(va("BotBrain(%s)::EnemyPool", bot->r.client->netname));
        }
        virtual ~EnemyPool() override {}
    };

    class AiSquad *squad;
    EnemyPool botEnemyPool;
    AiBaseEnemyPool *activeEnemyPool;

protected:
    virtual void SetFrameAffinity(unsigned modulo, unsigned offset) override
    {
        // Call super method first
        AiBaseBrain::SetFrameAffinity(modulo, offset);
        // Allow bot's own enemy pool to think
        botEnemyPool.SetFrameAffinity(modulo, offset);
    }
    virtual void OnAttitudeChanged(const edict_t *ent, int oldAttitude_, int newAttitude_) override;
public:
    SelectedEnemies &selectedEnemies;
    SelectedWeapons &selectedWeapons;

    // A WorldState cached from the moment of last world state update
    WorldState recentWorldState;

    // Note: saving references to Bot members is the only valid access kind to Bot in this call
    BotBrain(class Bot *bot, float skillLevel_);

    virtual void Frame() override;
    virtual void Think() override;
    virtual void PostThink() override;

    void OnAttachedToSquad(AiSquad *squad_);
    void OnDetachedFromSquad(AiSquad *squad_);

    void OnNewThreat(const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector);
    void OnEnemyRemoved(const Enemy *enemy);

    inline unsigned MaxTrackedEnemies() const { return botEnemyPool.MaxTrackedEnemies(); }

    void OnEnemyViewed(const edict_t *enemy);
    void AfterAllEnemiesViewed() {}
    void UpdateSelectedEnemies();

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
        return selectedEnemies.IsPrimaryEnemy(enemy);
    }

    inline float GetBaseOffensiveness() const { return baseOffensiveness; }

    float GetEffectiveOffensiveness() const;

    inline void SetBaseOffensiveness(float baseOffensiveness_)
    {
        this->baseOffensiveness = baseOffensiveness_;
        clamp(this->baseOffensiveness, 0.0f, 1.0f);
    }

    inline void ClearOverriddenEntityWeights()
    {
        itemsSelector.ClearOverriddenEntityWeights();
    }

    inline void OverrideEntityWeight(const edict_t *ent, float weight)
    {
        itemsSelector.OverrideEntityWeight(ent, weight);
    }
};

#endif
