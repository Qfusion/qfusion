#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "bot_brain.h"
#include "ai_base_ai.h"
#include "vec3.h"

class AiSquad;
class AiBaseEnemyPool;

class Bot: public Ai
{
    friend class AiManager;
    friend class AiBaseTeamBrain;
    friend class BotBrain;
    friend class AiSquad;
    friend class AiBaseEnemyPool;
    friend class FireTargetCache;
public:
    static constexpr auto PREFERRED_TRAVEL_FLAGS =
        TFL_WALK | TFL_WALKOFFLEDGE | TFL_JUMP | TFL_AIR | TFL_TELEPORT | TFL_JUMPPAD;
    static constexpr auto ALLOWED_TRAVEL_FLAGS =
        PREFERRED_TRAVEL_FLAGS | TFL_WATER | TFL_WATERJUMP | TFL_SWIM | TFL_LADDER | TFL_ELEVATOR;

    Bot(edict_t *self, float skillLevel);
    virtual ~Bot() override
    {
        AiAasRouteCache::ReleaseInstance(routeCache);
    }

    void Move(usercmd_t *ucmd, bool beSilent);
    void LookAround();
    void ChangeWeapon(const CombatTask &combatTask);
    void ChangeWeapon(int weapon);
    bool FireWeapon(bool *didBuiltinAttack);
    void Pain(const edict_t *enemy, float kick, int damage)
    {
        botBrain.OnPain(enemy, kick, damage);
    }
    void OnEnemyDamaged(const edict_t *enemy, int damage)
    {
        botBrain.OnEnemyDamaged(enemy, damage);
    }
    virtual void OnBlockedTimeout() override;
    void SayVoiceMessages();
    void GhostingFrame();
    void ActiveFrame();
    void CallGhostingClientThink(usercmd_t *ucmd);
    void CallActiveClientThink(usercmd_t *ucmd);

    void OnRespawn();

    inline float Skill() const { return skillLevel; }
    inline bool IsReady() const { return level.ready[PLAYERNUM(self)]; }

    inline void OnAttachedToSquad(AiSquad *squad)
    {
        botBrain.OnAttachedToSquad(squad);
        isInSquad = true;
    }
    inline void OnDetachedFromSquad(AiSquad *squad)
    {
        botBrain.OnDetachedFromSquad(squad);
        isInSquad = false;
    }
    inline bool IsInSquad() const { return isInSquad; }

    inline unsigned LastAttackedByTime(const edict_t *attacker)
    {
        return botBrain.LastAttackedByTime(attacker);
    }
    inline unsigned LastTargetTime(const edict_t *target)
    {
        return botBrain.LastTargetTime(target);
    }
    inline void OnEnemyRemoved(const Enemy *enemy)
    {
        botBrain.OnEnemyRemoved(enemy);
    }
    inline void OnNewThreat(const edict_t *newThreat, const AiFrameAwareUpdatable *threatDetector)
    {
        botBrain.OnNewThreat(newThreat, threatDetector);
    }

    inline void SetAttitude(const edict_t *ent, int attitude)
    {
        botBrain.SetAttitude(ent, attitude);
    }
    inline void ClearOverriddenEntityWeights()
    {
        botBrain.ClearOverriddenEntityWeights();
    }
    inline void OverrideEntityWeight(const edict_t *ent, float weight)
    {
        botBrain.OverrideEntityWeight(ent, weight);
    }

    inline float GetBaseOffensiveness() const { return botBrain.GetBaseOffensiveness(); }
    inline float GetEffectiveOffensiveness() const { return botBrain.GetEffectiveOffensiveness(); }
    inline void SetBaseOffensiveness(float baseOffensiveness)
    {
        botBrain.SetBaseOffensiveness(baseOffensiveness);
    }

    inline const int *Inventory() const { return self->r.client->ps.inventory; }

    typedef void (*AlertCallback)(void *receiver, Bot *bot, int id, float alertLevel);
    void EnableAutoAlert(int id, const Vec3 &spotOrigin, float spotRadius, AlertCallback callback, void *receiver);
    void DisableAutoAlert(int id);

    inline int Health() const
    {
        return self->r.client->ps.stats[STAT_HEALTH];
    }
    inline int Armor() const
    {
        return self->r.client->ps.stats[STAT_ARMOR];
    }
    inline bool CanAndWouldDropHealth() const
    {
        return GT_asBotWouldDropHealth(self->r.client);
    }
    inline void DropHealth()
    {
        GT_asBotDropHealth(self->r.client);
    }
    inline bool CanAndWouldDropArmor() const
    {
        return GT_asBotWouldDropArmor(self->r.client);
    }
    inline void DropArmor()
    {
        GT_asBotDropArmor(self->r.client);
    }
    inline bool CanAndWouldCloak() const
    {
        return GT_asBotWouldCloak(self->r.client);
    }
    inline void SetCloakEnabled(bool enabled)
    {
        GT_asSetBotCloakEnabled(self->r.client, enabled);
    }
    inline bool IsCloaking() const
    {
        return GT_asIsEntityCloaking(self);
    }
    inline float PlayerDefenciveAbilitiesRating() const
    {
        return GT_asPlayerDefenciveAbilitiesRating(self->r.client);
    }
    inline float PlayerOffenciveAbilitiesRating() const
    {
        return GT_asPlayerOffensiveAbilitiesRating(self->r.client);
    }
    inline int DefenceSpotId() const { return defenceSpotId; }
    inline int OffenceSpotId() const { return offenceSpotId; }
    inline void ClearDefenceAndOffenceSpots()
    {
        defenceSpotId = -1;
        offenceSpotId = -1;
    }
    inline void SetDefenceSpotId(int spotId)
    {
        defenceSpotId = spotId;
        offenceSpotId = -1;
    }
    inline void SetOffenceSpotId(int spotId)
    {
        defenceSpotId = -1;
        offenceSpotId = spotId;
    }
protected:
    virtual void Frame() override;
    virtual void Think() override;

    virtual void PreFrame() override
    {
        // We should update weapons status each frame since script weapons may be changed each frame.
        // These statuses are used by firing methods, so actual weapon statuses are required.
        UpdateScriptWeaponsStatus();
    }

    virtual void TouchedGoal(const edict_t *goalUnderlyingEntity) override;
    virtual void TouchedJumppad(const edict_t *jumppad) override;
private:
    void RegisterVisibleEnemies();

    inline bool IsPrimaryAimEnemy(const edict_t *enemy) const { return botBrain.IsPrimaryAimEnemy(enemy); }

    DangersDetector dangersDetector;
    BotBrain botBrain;

    float skillLevel;

    unsigned nextBlockedEscapeAttemptAt;
    Vec3 blockedEscapeGoalOrigin;

    struct JumppadMovementState
    {
        // Should be set by Bot::TouchedJumppad() callback (its get called in ClientThink())
        // It gets processed by movement code in next frame
        bool hasTouchedJumppad;
        // If this flag is set, bot is in "jumppad" movement state
        bool hasEnteredJumppad;
        // This timeout is computed and set in Bot::TouchedJumppad().
        // Bot tries to keep flying even if next reach. cache is empty if the timeout is greater than level time.
        // If there are no cached reach.'s and the timeout is not greater than level time bot tries to find area to land to.
        unsigned jumppadMoveTimeout;
        // Next reach. cache is lost in air.
        // Thus we have to store next areas starting a jumppad movement and try to prefer these areas for landing
        static constexpr int MAX_LANDING_AREAS = 16;
        int landingAreas[MAX_LANDING_AREAS];
        int landingAreasCount;
        Vec3 jumppadTarget;

        inline JumppadMovementState()
            : hasTouchedJumppad(false),
              hasEnteredJumppad(false),
              jumppadMoveTimeout(0),
              landingAreasCount(0),
              jumppadTarget(INFINITY, INFINITY, INFINITY) {}

        inline bool IsActive() const
        {
            return hasTouchedJumppad || hasEnteredJumppad;
        }
    };

    JumppadMovementState jumppadMovementState;

    struct RocketJumpMovementState
    {
        const edict_t *self;

        Vec3 jumpTarget;
        Vec3 fireTarget;
        bool hasTriggeredRocketJump;
        bool hasCorrectedRocketJump;
        bool wasTriggeredPrevFrame;
        unsigned timeoutAt;

        RocketJumpMovementState(const edict_t *self)
            : self(self),
              jumpTarget(INFINITY, INFINITY, INFINITY),
              fireTarget(INFINITY, INFINITY, INFINITY),
              hasTriggeredRocketJump(false),
              hasCorrectedRocketJump(false),
              wasTriggeredPrevFrame(false),
              timeoutAt(0) {}

        inline bool IsActive() const
        {
            return hasTriggeredRocketJump;
        }

        inline bool HasBeenJustTriggered() const
        {
            return hasTriggeredRocketJump && !wasTriggeredPrevFrame;
        }

        void TryInvalidate()
        {
            if (hasTriggeredRocketJump)
            {
                if (self->groundentity || (jumpTarget - self->s.origin).SquaredLength() < 48 * 48)
                    hasTriggeredRocketJump = false;
            }
        }

        void SetTriggered(const Vec3 &jumpTarget, const Vec3 &fireTarget, unsigned timeoutPeriod)
        {
            this->jumpTarget = jumpTarget;
            this->fireTarget = fireTarget;
            hasTriggeredRocketJump = true;
            hasCorrectedRocketJump = false;
            timeoutAt = level.time + timeoutPeriod;
        }
    };

    RocketJumpMovementState rocketJumpMovementState;

    struct PendingLandingDashState
    {
        bool isTriggered;
        bool isOnGroundThisFrame;
        bool wasOnGroundPrevFrame;
        unsigned timeoutAt;

        inline PendingLandingDashState()
            : isTriggered(false),
              isOnGroundThisFrame(false),
              wasOnGroundPrevFrame(false),
              timeoutAt(0) {}

        inline bool IsActive() const
        {
            return isTriggered;
        }

        inline bool MayApplyDash() const
        {
            return !wasOnGroundPrevFrame && isOnGroundThisFrame;
        }

        inline void Invalidate()
        {
            isTriggered = false;
        }

        void TryInvalidate()
        {
            if (IsActive())
            {
                if (timeoutAt < level.time)
                    Invalidate();
                else if (isOnGroundThisFrame && wasOnGroundPrevFrame)
                    Invalidate();
            }
        }

        inline void SetTriggered(unsigned timeoutPeriod)
        {
            isTriggered = true;
            timeoutAt = level.time + timeoutPeriod;
        }

        inline float EffectiveTurnSpeedMultiplier(float baseTurnSpeedMultiplier) const
        {
            return isTriggered ? 1.35f : baseTurnSpeedMultiplier;
        }
    };

    PendingLandingDashState pendingLandingDashState;

    unsigned combatMovePushTimeout;
    int combatMovePushes[3];

    unsigned vsayTimeout;

    struct PendingLookAtPointState
    {
        Vec3 lookAtPoint;
        unsigned timeoutAt;
        float turnSpeedMultiplier;
        bool isTriggered;

        inline PendingLookAtPointState()
            : lookAtPoint(INFINITY, INFINITY, INFINITY),
              timeoutAt(0),
              turnSpeedMultiplier(1.0f),
              isTriggered(false) {}

        inline bool IsActive() const
        {
            return isTriggered && timeoutAt > level.time;
        }

        inline void SetTriggered(const Vec3 &lookAtPoint, float turnSpeedMultiplier = 0.5f, unsigned timeoutPeriod = 500)
        {
            this->lookAtPoint = lookAtPoint;
            this->turnSpeedMultiplier = turnSpeedMultiplier;
            this->timeoutAt = level.time + timeoutPeriod;
            this->isTriggered = true;
        }

        inline float EffectiveTurnSpeedMultiplier(float baseTurnSpeedMultiplier) const
        {
            return isTriggered ? turnSpeedMultiplier : baseTurnSpeedMultiplier;
        }
    };

    PendingLookAtPointState pendingLookAtPointState;

    struct CampingSpotState
    {
        bool isTriggered;
        // If it is set, the bot should prefer to look at the campingSpotLookAtPoint while camping
        // Otherwise the bot should spin view randomly
        bool hasLookAtPoint;
        // Maximum bot origin deviation from campingSpotOrigin while strafing when camping a spot
        float spotRadius;
        // 0..1, greater values result in frequent and hectic strafing/camera rotating
        float alertness;
        Vec3 spotOrigin;
        Vec3 lookAtPoint;
        Vec3 strafeDir;
        // When to change chosen strafe dir
        unsigned strafeTimeoutAt;
        // When to change randomly chosen look-at-point (if the point is not initially specified)
        unsigned lookAtPointTimeoutAt;

        inline CampingSpotState()
            : isTriggered(false),
              hasLookAtPoint(false),
              spotRadius(INFINITY),
              alertness(INFINITY),
              spotOrigin(INFINITY, INFINITY, INFINITY),
              lookAtPoint(INFINITY, INFINITY, INFINITY),
              strafeDir(INFINITY, INFINITY, INFINITY),
              strafeTimeoutAt(0),
              lookAtPointTimeoutAt(0) {}

        inline bool IsActive() const
        {
            return isTriggered;
        }

        inline void SetWithoutDirection(const Vec3 &spotOrigin, float spotRadius, float alertness)
        {
            isTriggered = true;
            hasLookAtPoint = false;
            this->spotOrigin = spotOrigin;
            this->spotRadius = spotRadius;
            this->alertness = alertness;
            strafeTimeoutAt = 0;
            lookAtPointTimeoutAt = 0;
        }

        inline void SetDirectional(const Vec3 &spotOrigin, const Vec3 &lookAtPoint, float spotRadius, float alertness)
        {
            isTriggered = true;
            hasLookAtPoint = true;
            this->spotOrigin = spotOrigin;
            this->lookAtPoint = lookAtPoint;
            this->spotRadius = spotRadius;
            this->alertness = alertness;
            strafeTimeoutAt = 0;
            lookAtPointTimeoutAt = 0;
        }

        inline void Invalidate()
        {
            isTriggered = false;
        }
    };

    CampingSpotState campingSpotState;

    bool isWaitingForItemSpawn;

    bool isInSquad;

    int defenceSpotId;
    int offenceSpotId;

    struct AlertSpot
    {
        Vec3 origin;
        int id;
        float radius;
        unsigned lastReportedAt;
        float lastReportedScore;
        AlertCallback callback;
        void *receiver;

        AlertSpot(const Vec3 &origin, int id, float radius, AlertCallback callback, void *receiver)
            : origin(origin),
              id(id),
              radius(radius),
              lastReportedAt(0),
              lastReportedScore(0.0f),
              callback(callback),
              receiver(receiver) {};

        inline void Alert(Bot *bot, float score)
        {
            callback(receiver, bot, id, score);
            lastReportedAt = level.time;
            lastReportedScore = score;
        }
    };

    static constexpr unsigned MAX_ALERT_SPOTS = 3;
    StaticVector<AlertSpot, MAX_ALERT_SPOTS> alertSpots;

    void CheckAlertSpots(const StaticVector<edict_t *, MAX_CLIENTS> &visibleTargets);

    static constexpr unsigned MAX_SCRIPT_WEAPONS = 3;
    StaticVector<ai_script_weapon_def_t, MAX_SCRIPT_WEAPONS> scriptWeaponDefs;
    StaticVector<int, MAX_SCRIPT_WEAPONS> scriptWeaponCooldown;

    void UpdateScriptWeaponsStatus();

    inline bool HasPendingLookAtPoint() const
    {
        return pendingLookAtPointState.IsActive();
    }
    inline void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutPeriod = 500)
    {
        pendingLookAtPointState.SetTriggered(point, turnSpeedMultiplier, timeoutPeriod);
    }
    void ApplyPendingTurnToLookAtPoint();

    // Must be called on each frame
    void MoveFrame(usercmd_t *ucmd, bool inhibitCombat, bool beSilent);

    void MoveOnLadder(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveEnteringJumppad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveRidingJummpad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveTriggeredARocketJump(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveOnPlatform(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpot(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpotWithGivenLookAtPoint(const Vec3 &givenLookAtPoint, Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *intendedLookVec, usercmd_t *ucmd, bool beSilent);
    bool CheckAndTryAvoidObstacles(Vec3 *intendedLookVec, usercmd_t *ucmd, float speed);
    // Tries to straighten look vec first.
    // If the straightening failed, tries to interpolate it.
    // Also, handles case of empty reachabilities chain in goal area and outside it.
    // Returns true if look vec has been straightened (and is directed to an important spot).
    bool StraightenOrInterpolateLookVec(Vec3 *intendedLookVec, float speed);
    // Returns true if the intendedLookVec has been straightened
    // Otherwise InterpolateLookVec() should be used
    bool TryStraightenLookVec(Vec3 *intendedLookVec);
    // Interpolates intendedLookVec for the pending areas chain
    void InterpolateLookVec(Vec3 *intendedLookVec, float speed);
    void SetLookVecToPendingReach(Vec3 *intendedLookVec);
    void TryLandOnNearbyAreas(Vec3 *intendedLookVec, usercmd_t *ucmd);
    bool TryLandOnArea(int areaNum, Vec3 *intendedLookVec, usercmd_t *ucmd);
    void CheckTargetProximity();
    inline bool IsCloseToAnyGoal()
    {
        return botBrain.IsCloseToAnyGoal();
    }
    void OnGoalCleanedUp(const Goal *goal);

    bool MaySetPendingLandingDash();
    void SetPendingLandingDash(usercmd_t *ucmd);
    void ApplyPendingLandingDash(usercmd_t *ucmd);

    bool TryRocketJumpShortcut(usercmd_t *ucmd);
    // A bot should aim to fireTarget while doing a RJ
    // A bot should look on targetOrigin in flight
    // Return false if targets can't be adjusted (and a RJ should be rejected).
    bool AdjustDirectRocketJumpToAGoalTarget(Vec3 *targetOrigin, Vec3 *fireTarget) const;
    // Should be called when a goal does not seem to be reachable for RJ on the distance to a goal.
    bool AdjustRocketJumpTargetForPathShortcut(Vec3 *targetOrigin, Vec3 *fireTarget) const;
    // Should be called when a goal seems to be reachable for RJ on the distance to a goal,
    // but direct rocketjump to a goal is blocked by obstacles.
    // Returns area num of found area (if any)
    int TryFindRocketJumpAreaCloseToGoal(const Vec3 &botToGoalDir2D, float botToGoalDist2D) const;
    // Tries to select an appropriate weapon and trigger a rocketjump.
    // Assumes that targetOrigin and fireTarget are checked.
    // Returns false if a rocketjump cannot be triggered.
    bool TryTriggerWeaponJump(usercmd_t *ucmd, const Vec3 &targetOrigin, const Vec3 &fireTarget);
    // Triggers a jump/dash and fire actions, and schedules trajectory correction to fireTarget to a next frame.
    // Assumes that targetOrigin and fireTarget are checked.
    // Make sure you have selected an appropriate weapon and its ready to fire before you call it.
    void TriggerWeaponJump(usercmd_t *ucmd, const Vec3 &targetOrigin, const Vec3 &fireTarget);

    void TryEscapeIfBlocked(usercmd_t *ucmd);

    void CombatMovement(usercmd_t *ucmd, bool hasDangers);
    void UpdateCombatMovePushes();
    void MakeEvadeMovePushes(usercmd_t *ucmd);
    bool MayApplyCombatDash();
    Vec3 MakeEvadeDirection(const Danger &danger);
    void ApplyCheatingGroundAcceleration(const usercmd_t *ucmd);

    class GenericFireDef
    {
        const firedef_t *builtinFireDef;
        const ai_script_weapon_def_t *scriptWeaponDef;
        int weaponNum;

    public:
        GenericFireDef(int weaponNum, const firedef_t *builtinFireDef, const ai_script_weapon_def_t *scriptWeaponDef)
        {
            this->builtinFireDef = builtinFireDef;
            this->scriptWeaponDef = scriptWeaponDef;
            this->weaponNum = weaponNum;
        }

        inline int WeaponNum() const { return weaponNum; }
        inline bool IsBuiltin() const { return builtinFireDef != nullptr; }

        inline ai_weapon_aim_type AimType() const
        {
            return builtinFireDef ? BuiltinWeaponAimType(weaponNum) : scriptWeaponDef->aimType;
        }
        inline float ProjectileSpeed() const
        {
            return builtinFireDef ? builtinFireDef->speed : scriptWeaponDef->projectileSpeed;
        }
        inline float SplashRadius() const
        {
            return builtinFireDef ? builtinFireDef->splash_radius : scriptWeaponDef->splashRadius;
        }
        inline bool IsContinuousFire() const
        {
            return builtinFireDef ? IsBuiltinWeaponContinuousFire(weaponNum) : scriptWeaponDef->isContinuousFire;
        }
    };

    struct AimParams
    {
        vec3_t fireOrigin;
        vec3_t fireTarget;
        float suggestedBaseAccuracy;

        float EffectiveAccuracy(float skill, bool importantShot) const
        {
            float accuracy = suggestedBaseAccuracy;
            accuracy *= (1.0f - 0.75f * skill);
            if (importantShot && skill > 0.33f)
                accuracy *= (1.13f - skill);

            return accuracy;
        }
    };

    class FireTargetCache
    {
        struct CachedFireTarget
        {
            Vec3 origin;
            unsigned combatTaskInstanceId;
            unsigned invalidAt;

            CachedFireTarget()
                : origin(0, 0, 0), combatTaskInstanceId(0), invalidAt(0) {}

            inline bool IsValidFor(const CombatTask &combatTask) const
            {
                return combatTaskInstanceId == combatTask.instanceId && invalidAt > level.time;
            }

            inline void SetFor(const CombatTask &combatTask, const vec3_t origin)
            {
                VectorCopy(origin, this->origin.Data());
            }

            inline void SetFor(const CombatTask &combatTask, const Vec3 &origin)
            {
                this->origin = origin;
            }
        };

        CachedFireTarget cachedFireTarget;
        const edict_t *bot;

        void SetupCoarseFireTarget(const CombatTask &combatTask, vec3_t fire_origin, vec3_t target);

        void AdjustPredictionExplosiveAimTypeParams(const CombatTask &combatTask, const GenericFireDef &fireDef,
                                                    AimParams *aimParams);
        void AdjustPredictionAimTypeParams(const CombatTask &combatTask, const GenericFireDef &fireDef,
                                           AimParams *aimParams);
        void AdjustDropAimTypeParams(const CombatTask &combatTask, const GenericFireDef &fireDef,
                                     AimParams *aimParams);
        void AdjustInstantAimTypeParams(const CombatTask &combatTask, const GenericFireDef &fireDef,
                                        AimParams *aimParams);

        // Returns true if a shootable environment for inflicting a splash damage has been found
        bool AdjustTargetByEnvironment(const CombatTask &combatTask, float splashRadius, AimParams *aimParams);
        bool AdjustTargetByEnvironmentTracing(const CombatTask &combatTask, float splashRadius, AimParams *aimParams);
        bool AdjustTargetByEnvironmentWithAAS(const CombatTask &combatTask, float splashRadius, int areaNum,
                                              AimParams *aimParams);

        void GetPredictedTargetOrigin(const CombatTask &combatTask, float projectileSpeed, AimParams *aimParams);
        void PredictProjectileShot(const CombatTask &combatTask, float projectileSpeed, AimParams *aimParams,
                                   bool applyTargetGravity);
    public:
        FireTargetCache(const edict_t *bot) : bot(bot) {}

        void AdjustAimParams(const CombatTask &combatTask, const GenericFireDef &fireDef, AimParams *aimParams);
    };

    FireTargetCache builtinFireTargetCache;
    FireTargetCache scriptFireTargetCache;

    // Returns true if current look angle worth pressing attack
    bool CheckShot(const AimParams &aimParams, const CombatTask &combatTask, const GenericFireDef &fireDef);

    void LookAtEnemy(float accuracy, const vec3_t fire_origin, vec3_t target);
    bool TryPressAttack(const GenericFireDef *fireDef, const GenericFireDef *builtinFireDef,
                        const GenericFireDef *scriptFireDef, bool *didBuiltinAttack);

    inline bool HasEnemy() const { return !botBrain.combatTask.Empty(); }
    inline bool IsEnemyAStaticSpot() const { return botBrain.combatTask.IsTargetAStaticSpot(); }
    inline const edict_t *EnemyTraceKey() const { return botBrain.combatTask.TraceKey(); }
    inline const bool IsEnemyOnGround() const { return botBrain.combatTask.IsOnGround(); }
    inline Vec3 EnemyOrigin() const { return botBrain.combatTask.LastSeenEnemyOrigin(); }
    inline Vec3 EnemyLookDir() const { return botBrain.combatTask.EnemyLookDir(); }
    inline unsigned EnemyFireDelay() const { return botBrain.combatTask.EnemyFireDelay(); }
    inline Vec3 EnemyVelocity() const { return botBrain.combatTask.EnemyVelocity(); }
    inline Vec3 EnemyMins() const { return botBrain.combatTask.EnemyMins(); }
    inline Vec3 EnemyMaxs() const { return botBrain.combatTask.EnemyMaxs(); }
    inline unsigned EnemyInstanceId() const { return botBrain.combatTask.instanceId; }

    bool MayKeepRunningInCombat() const;
    void SetCombatInhibitionFlags(bool *inhibitShooting, bool *inhibitCombatMove);
    bool ShouldBeSilent(bool inhibitShooting) const;

    inline bool HasSpecialGoal() const { return botBrain.HasSpecialGoal(); }
    inline bool IsSpecialGoalSetBy(const AiFrameAwareUpdatable *setter) const
    {
        return botBrain.IsSpecialGoalSetBy(setter);
    }
    inline void SetSpecialGoalFromEntity(edict_t *ent, const AiFrameAwareUpdatable *setter)
    {
        botBrain.SetSpecialGoalFromEntity(ent, setter);
    }
};

#endif
