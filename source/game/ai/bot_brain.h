#ifndef AI_ENEMY_POOL_H
#define AI_ENEMY_POOL_H

#include "ai_local.h"
#include "../../gameshared/q_comref.h"
#include <deque>
#include <limits>

template <int Weapon> struct WeaponAmmo
{
    static constexpr int strongAmmoTag = AMMO_NONE;
    static constexpr int weakAmmoTag = AMMO_NONE;
};

template<> struct WeaponAmmo<WEAP_NONE>
{
    static constexpr int strongAmmoTag = AMMO_NONE;
    static constexpr int weakAmmoTag = AMMO_NONE;
};

template<> struct WeaponAmmo<WEAP_GUNBLADE>
{
    static constexpr int strongAmmoTag = AMMO_GUNBLADE;
    static constexpr int weakAmmoTag = AMMO_WEAK_GUNBLADE;
};

template<> struct WeaponAmmo<WEAP_RIOTGUN>
{
    static constexpr int strongAmmoTag = AMMO_SHELLS;
    static constexpr int weakAmmoTag = AMMO_WEAK_SHELLS;
};

template<> struct WeaponAmmo<WEAP_GRENADELAUNCHER>
{
    static constexpr int strongAmmoTag = AMMO_GRENADES;
    static constexpr int weakAmmoTag = AMMO_WEAK_GRENADES;
};

template<> struct WeaponAmmo<WEAP_ROCKETLAUNCHER>
{
    static constexpr int strongAmmoTag = AMMO_ROCKETS;
    static constexpr int weakAmmoTag = AMMO_WEAK_ROCKETS;
};

template<> struct WeaponAmmo<WEAP_PLASMAGUN>
{
    static constexpr int strongAmmoTag = AMMO_PLASMA;
    static constexpr int weakAmmoTag = AMMO_WEAK_PLASMA;
};

template<> struct WeaponAmmo<WEAP_LASERGUN>
{
    static constexpr int strongAmmoTag = AMMO_LASERS;
    static constexpr int weakAmmoTag = AMMO_WEAK_LASERS;
};

template<> struct WeaponAmmo<WEAP_MACHINEGUN>
{
    static constexpr int strongAmmoTag = AMMO_BULLETS;
    static constexpr int weakAmmoTag = AMMO_WEAK_BULLETS;
};

template<> struct WeaponAmmo<WEAP_ELECTROBOLT>
{
    static constexpr int strongAmmoTag = AMMO_BOLTS;
    static constexpr int weakAmmoTag = AMMO_WEAK_BOLTS;
};

inline bool HasQuad(const edict_t *ent)
{
    return ent && ent->r.client && ent->r.client->ps.inventory[POWERUP_QUAD];
}

inline bool HasShell(const edict_t *ent)
{
    return ent && ent->r.client && ent->r.client->ps.inventory[POWERUP_SHELL];
}

inline bool HasPowerups(const edict_t *ent)
{
    if (!ent || !ent->r.client)
        return false;
    return ent->r.client->ps.inventory[POWERUP_QUAD] && ent->r.client->ps.inventory[POWERUP_SHELL];
}

inline bool IsCarrier(const edict_t *ent)
{
    return ent && ent->r.client && ent->s.effects & EF_CARRIER;
}

float DamageToKill(const edict_t *client, float armorProtection, float armorDegradation);

class Enemy
{
public:

    Enemy() : ent(nullptr), lastSeenPosition(1/0.0f, 1/0.0f, 1/0.0f), lastSeenVelocity(1/0.0f, 1/0.0f, 1/0.0f)
    {
        Clear();
    }

    const edict_t *ent;  // If null, the enemy slot is unused

    static constexpr unsigned MAX_TRACKED_POSITIONS = 16;

    float weight;
    float avgPositiveWeight;
    float maxPositiveWeight;
    unsigned positiveWeightsCount;

    unsigned registeredAt;

    void Clear();
    void OnViewed();

    inline const char *Nick() const { return ent->r.client ? ent->r.client->netname : ent->classname; }

    inline bool HasQuad() const { return ::HasQuad(ent); }
    inline bool HasShell() const { return ::HasShell(ent); }
    inline bool HasPowerups() const { return ::HasPowerups(ent); }
    inline bool IsCarrier() const { return ::IsCarrier(ent); }

    template<int Weapon> inline int AmmoReadyToFireCount() const
    {
        if (!ent->r.client)
            return 0;
        const int *inventory = ent->r.client->ps.inventory;
        if (!inventory[Weapon])
            return 0;
        return inventory[WeaponAmmo<Weapon>::strongAmmoTag] + inventory[WeaponAmmo<Weapon>::weakAmmoTag];
    }

    inline int ShellsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_RIOTGUN>(); }
    inline int GrenadesReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_GRENADELAUNCHER>(); }
    inline int RocketsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ROCKETLAUNCHER>(); }
    inline int PlasmasReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_PLASMAGUN>(); }
    inline int BulletsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_MACHINEGUN>(); }
    inline int LasersReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_LASERGUN>(); }
    inline int BoltsReadyToFireCount() const { return AmmoReadyToFireCount<WEAP_ELECTROBOLT>(); }

    inline int PendingWeapon() const
    {
        // TODO: It does not check ammo
        return ent->r.client ? ent->r.client->ps.stats[STAT_PENDING_WEAPON] : WEAP_NONE;
    }

    inline unsigned LastSeenAt() const { return lastSeenAt; }
    inline const Vec3 &LastSeenPosition() const { return lastSeenPosition; }
    inline const Vec3 &LastSeenVelocity() const { return lastSeenVelocity; }

    // TODO: Fuse in a single array of some struct
    // Array of last seen timestamps
    std::deque<unsigned> lastSeenTimestamps;
    // Array of last seen positions
    std::deque<Vec3> lastSeenPositions;
    // Array of last seen enemy velocities
    std::deque<Vec3> lastSeenVelocities;
private:
    // Same as front() of lastSeenPositions, used for faster access
    Vec3 lastSeenPosition;
    // Same as front() of lastSeenVelocities, used for faster access
    Vec3 lastSeenVelocity;
    // Same as front() of lastSeenTimestamps, used for faster access
    unsigned lastSeenAt;
};

class AttackStats
{
    friend class BotBrain;

    // Very close to 4 game seconds
    static constexpr unsigned MAX_KEPT_FRAMES = 64 * 4;

    static_assert((MAX_KEPT_FRAMES & (MAX_KEPT_FRAMES - 1)) == 0, "Should be a power of 2 for fast modulo computation");

    float frameDamages[MAX_KEPT_FRAMES];

    unsigned frameIndex;
    unsigned totalAttacks;
    unsigned lastDamageAt;
    unsigned lastTouchAt;
    float totalDamage;

    const edict_t *ent;

    AttackStats() { Clear(); }
public:

    void Clear()
    {
        ent = nullptr;
        totalDamage = 0;
        totalAttacks = 0;
        lastDamageAt = 0;
        lastTouchAt = level.time;
        frameIndex = 0;
        memset(frameDamages, 0, sizeof(frameDamages));
    }

    // Call it once in a game frame
    void Frame()
    {
        float overwrittenDamage = frameDamages[frameIndex];
        frameIndex = (frameIndex + 1) % MAX_KEPT_FRAMES;
        totalDamage -= overwrittenDamage;
        frameDamages[frameIndex] = 0.0f;
        if (overwrittenDamage > 0)
            totalAttacks--;
    }

    // Call it after Frame() in the same frame
    void OnDamage(float damage)
    {
        frameDamages[frameIndex] = damage;
        totalDamage += damage;
        totalAttacks++;
        lastDamageAt = level.time;
    }

    // Call it after Frame() in the same frame if damage is not registered
    // but you want to mark frame as a frame of activity anyway
    void Touch() { lastTouchAt = level.time; }

    unsigned LastActivityAt() const { return std::max(lastDamageAt, lastTouchAt); }
};

class CombatTask
{
    friend class BotBrain;
    // If it is not null, it is a chosen target for shooting.
    const Enemy *aimEnemy;

    // May be null if spamming is not personal for the enemy.
    // Used to cancel spamming if enemy is killed by bot or other player
    // (this means there is no sense to do spamming).
    const Enemy *spamEnemy;
    // Used to prevent infinite spamming on the same point, should be set in Reset();
    const Enemy *prevSpamEnemy;
    Vec3 spamSpot;

    int suggestedShootWeapon;
    int suggestedSpamWeapon;
#if defined(__GNUC__) || defined(__clang__)
    void FailWith(const char *message) const __attribute__ ((noreturn))
#else
    void FailWith(const char *message) const
#endif
    {
        G_Printf(message);
        abort();
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

    // This call completely cleans object state
    void Clear()
    {
        aimEnemy = nullptr;
        spamEnemy = nullptr;
        prevSpamEnemy = nullptr;
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

    // This call cleans all fields except prevSpamEnemy which is set to currSpamEnemy
    void Reset()
    {
        const Enemy *currSpamEnemy = spamEnemy;
        Clear();
        prevSpamEnemy = currSpamEnemy;
    }

    bool Empty() const  { return !aimEnemy && !spamEnemy; }

    bool IsTargetStatic() const { return !aimEnemy; }

    Vec3 EnemyOrigin() const
    {
        if (aimEnemy) return aimEnemy->LastSeenPosition();
        if (spamEnemy) return spamSpot;
        FailWith("EnemyOrigin(): combat task is empty\n");
    }

    Vec3 EnemyVelocity() const
    {
        if (aimEnemy) return Vec3(aimEnemy->ent->velocity);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyVelocity(): combat task is empty\n");
    }

    Vec3 EnemyMins() const
    {
        if (aimEnemy) return Vec3(aimEnemy->ent->r.mins);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyMins(): combat task is empty\n");
    }

    Vec3 EnemyMaxs() const
    {
        if (aimEnemy) return Vec3(aimEnemy->ent->r.maxs);
        if (spamEnemy) return Vec3(0, 0, 0);
        FailWith("EnemyMaxs(): combat task is empty\n");
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
    float skillLevel;

    static constexpr unsigned MAX_TRACKED_ENEMIES = 10;
    static constexpr unsigned MAX_TRACKED_ATTACKERS = 5;
    static constexpr unsigned MAX_TRACKED_TARGETS = 5;
    // Ensure we always will have at least 3 free slots for new enemies
    // (quad/shell owners and carrier) FOR MAXIMAL SKILL
    static_assert(MAX_TRACKED_ATTACKERS + 3 <= MAX_TRACKED_ENEMIES, "Leave at least 3 free slots for ordinary enemies");
    static_assert(MAX_TRACKED_TARGETS + 3 <= MAX_TRACKED_ENEMIES, "Leave at least 3 free slots for ordinary enemies");

    static constexpr unsigned NOT_SEEN_TIMEOUT = 10000;
    static constexpr unsigned ATTACKER_TIMEOUT = 3000;
    static constexpr unsigned TARGET_TIMEOUT = 3000;

    StaticVector<Enemy, MAX_TRACKED_ENEMIES> enemies;

    unsigned trackedEnemiesCount;
    const unsigned maxTrackedEnemies;
    const unsigned maxTrackedAttackers;
    const unsigned maxTrackedTargets;

    const unsigned reactionTime;

    const unsigned aimTargetChoicePeriod;
    const unsigned spamTargetChoicePeriod;
    const unsigned aimWeaponChoicePeriod;
    const unsigned spamWeaponChoicePeriod;

    unsigned combatTaskInstanceCounter;

    unsigned frameAffinityModulo;
    unsigned frameAffinityOffset;

    unsigned nextTargetChoiceAt;
    unsigned nextWeaponChoiceAt;

    StaticVector<AttackStats, MAX_TRACKED_ATTACKERS> attackers;
    StaticVector<AttackStats, MAX_TRACKED_TARGETS> targets;

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

    float ComputeRawWeight(const edict_t *enemy);

    void UpdateWeight(Enemy &enemy);

    void TryPushNewEnemy(const edict_t *enemy);

    void EmplaceEnemy(const edict_t *enemy, int slot);

    inline unsigned NextCombatTaskInstanceId() { return combatTaskInstanceCounter++; }

    inline const char *BotNick() const { return bot->r.client->netname; }

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

    // Returns zero if ent not found
    unsigned LastAttackedByTime(const edict_t *ent) const;
    unsigned LastTargetTime(const edict_t *ent) const;
    bool HasAnyDetectedEnemiesInView() const;
    void EnqueueAttacker(const edict_t *attacker, int damage);
    void EnqueueTarget(const edict_t *target);
    void SuggestAimWeaponAndTactics(CombatTask *task);
    void SuggestSniperRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestFarRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestMiddleRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    void SuggestCloseRangeWeaponAndTactics(CombatTask *task, const CombatDisposition &disposition);
    int SuggestInstagibWeapon(const Enemy &enemy);
    int SuggestEasyBotsWeapon(const Enemy &enemy);
    void SuggestSpamEnemyWeaponAndTactics(CombatTask *task);
    int SuggestFinishWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    bool IsEnemyEscaping(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestHitEscapingEnemyWeapon(const Enemy &enemy, const CombatDisposition &disposition);
    int SuggestQuadBearerWeapon(const Enemy &enemy);
    bool SuggestPointToTurnToWhenEnemyIsLost(const Enemy *oldEnemy);
    void SuggestSpamTask(CombatTask *task, const Vec3 &botOrigin, const Vec3 &botViewDirection);
    void StartSpamAtEnemy(CombatTask *task, const Enemy *enemy);
    int ChooseWeaponByScores(struct WeaponAndScore *begin, struct WeaponAndScore *end);
    void TestTargetEnvironment(const Vec3 &botOrigin, const Vec3 &targetOrigin, const edict_t *traceKey);
    void RemoveEnemy(Enemy &enemy);
    void UpdateKeptCurrentCombatTask();
    void TryFindNewCombatTask();

    float ComputeItemWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeWeaponWeight(const gsitem_t *item, bool onlyGotGB) const;
    float ComputeAmmoWeight(const gsitem_t *item) const;
    float ComputeArmorWeight(const gsitem_t *item) const;
    float ComputeHealthWeight(const gsitem_t *item) const;
    float ComputePowerupWeight(const gsitem_t *item) const;

    BotBrain() = delete;
    // Disable copying and moving
    BotBrain(BotBrain &&that) = delete;
public:
    CombatTask combatTask;

    BotBrain(edict_t *bot, float skillLevel);

    virtual void PreThink() override;
    virtual void PostThink() override;

    void OnEnemyViewed(const edict_t *enemy);
    // Call it after all calls to OnEnemyViewed()
    void AfterAllEnemiesViewed();
    void OnPain(const edict_t *enemy, float kick, int damage);
    void OnEnemyDamaged(const edict_t *target, int damage);

    void UpdateCombatTask();

    virtual void UpdatePotentialGoalsWeights() override;
};

#endif //QFUSION_ENEMY_POOL_H
