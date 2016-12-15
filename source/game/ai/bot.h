#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "bot_brain.h"
#include "ai_base_ai.h"
#include "vec3.h"

#include "bot_weapon_selector.h"
#include "bot_fire_target_cache.h"
#include "bot_tactical_spots_cache.h"

#include "bot_goals.h"
#include "bot_actions.h"

class AiSquad;
class AiBaseEnemyPool;

struct AiAlertSpot
{
    int id;
    Vec3 origin;
    float radius;
    float regularEnemyInfluenceScale;
    float carrierEnemyInfluenceScale;

    AiAlertSpot(int id_,
                Vec3 origin_,
                float radius_,
                float regularEnemyInfluenceScale_ = 1.0f,
                float carrierEnemyInfluenceScale_ = 1.0f)
        : id(id_),
          origin(origin_),
          radius(radius_),
          regularEnemyInfluenceScale(regularEnemyInfluenceScale_),
          carrierEnemyInfluenceScale(carrierEnemyInfluenceScale_) {}
};

struct BotInput
{
    usercmd_t ucmd;
    Vec3 intendedLookVec;
    // A copy of self->s.angles for modification
    // We do not want to do deeply hidden angles update in the aiming functions,
    // the BotInput should be only mutable thing in the related code.
    // Should be copied back to self->s.angles if it has been modified when the BotInput gets applied.
    Vec3 alreadyComputedAngles;
    bool fireScriptWeapon;
    bool isUcmdSet;
    bool isLookVecSet;
    bool hasAlreadyComputedAngles;
    bool canOverrideUcmd;
    bool shouldOverrideUcmd;
    bool canOverrideLookVec;
    bool shouldOverrideLookVec;
    bool canOverridePitch;
    bool applyExtraViewPrecision;
    float turnSpeedMultiplier;

    inline BotInput(const edict_t *self)
        : intendedLookVec(NAN, NAN, NAN),
          alreadyComputedAngles(self->s.angles),
          fireScriptWeapon(false),
          isUcmdSet(false),
          isLookVecSet(false),
          hasAlreadyComputedAngles(false),
          canOverrideUcmd(false),
          shouldOverrideUcmd(false),
          canOverrideLookVec(false),
          shouldOverrideLookVec(false),
          canOverridePitch(false),
          applyExtraViewPrecision(false),
          turnSpeedMultiplier(1.0f)
    {
        memset(&ucmd, 0, sizeof(ucmd));
    }

    inline void SetButton(int button, bool isSet)
    {
        if (isSet)
            ucmd.buttons |= button;
        else
            ucmd.buttons &= ~button;
    }

    inline bool IsButtonSet(int button) const { return (ucmd.buttons & button) != 0; }

    inline void SetAttackButton(bool isSet) { SetButton(BUTTON_ATTACK, isSet); }
    inline void SetSpecialButton(bool isSet) { SetButton(BUTTON_SPECIAL, isSet); }
    inline void SetWalkButton(bool isSet) { SetButton(BUTTON_WALK, isSet); }

    inline bool IsAttackButtonSet() const { return IsButtonSet(BUTTON_ATTACK); }
    inline bool IsSpecialButtonSet() const { return IsButtonSet(BUTTON_SPECIAL); }
    inline bool IsWalkButtonSet() const { return IsButtonSet(BUTTON_WALK); }

    inline int ForwardMovement() const { return (int)ucmd.forwardmove; }
    inline int RightMovement() const { return (int)ucmd.sidemove; }
    inline int UpMovement() const { return (int)ucmd.upmove; }

    inline bool IsCrouching() const { return ucmd.upmove < 0; }

    inline void SetForwardMovement(int movement) { ucmd.forwardmove = movement; }
    inline void SetRightMovement(int movement) { ucmd.sidemove = movement; }
    inline void SetUpMovement(int movement) { ucmd.upmove = movement; }

    inline void ClearMovementDirections()
    {
        ucmd.forwardmove = 0;
        ucmd.sidemove = 0;
        ucmd.upmove = 0;
    }
};

class Bot: public Ai
{
    friend class AiManager;
    friend class AiBaseTeamBrain;
    friend class BotBrain;
    friend class AiSquad;
    friend class AiBaseEnemyPool;
    friend class BotFireTargetCache;
    friend class BotItemsSelector;
    friend class BotWeaponSelector;
    friend class BotBaseGoal;
    friend class BotGutsActionsAccessor;
    friend class BotTacticalSpotsCache;
    friend class WorldState;
public:
    static constexpr auto PREFERRED_TRAVEL_FLAGS =
        TFL_WALK | TFL_WALKOFFLEDGE | TFL_JUMP | TFL_AIR | TFL_TELEPORT | TFL_JUMPPAD;
    static constexpr auto ALLOWED_TRAVEL_FLAGS =
        PREFERRED_TRAVEL_FLAGS | TFL_WATER | TFL_WATERJUMP | TFL_SWIM | TFL_LADDER | TFL_ELEVATOR;

    Bot(edict_t *self_, float skillLevel_);
    virtual ~Bot() override
    {
        AiAasRouteCache::ReleaseInstance(routeCache);
    }

    inline float Skill() const { return skillLevel; }
    inline bool IsReady() const { return level.ready[PLAYERNUM(self)]; }

    void Pain(const edict_t *enemy, float kick, int damage)
    {
        botBrain.OnPain(enemy, kick, damage);
    }
    void OnEnemyDamaged(const edict_t *enemy, int damage)
    {
        botBrain.OnEnemyDamaged(enemy, damage);
    }

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

    void EnableAutoAlert(const AiAlertSpot &alertSpot, AlertCallback callback, void *receiver);
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
    inline int OffenseSpotId() const { return offenseSpotId; }
    inline void ClearDefenceAndOffenceSpots()
    {
        defenceSpotId = -1;
        offenseSpotId = -1;
    }
    inline void SetDefenceSpotId(int spotId)
    {
        defenceSpotId = spotId;
        offenseSpotId = -1;
    }
    inline void SetOffenseSpotId(int spotId)
    {
        defenceSpotId = -1;
        offenseSpotId = spotId;
    }
    inline float Fov() const { return 110.0f + 69.0f * Skill(); }
    inline float FovDotFactor() const { return cosf((float)DEG2RAD(Fov() / 2)); }
protected:
    virtual void Frame() override;
    virtual void Think() override;

    virtual void PreFrame() override
    {
        // We should update weapons status each frame since script weapons may be changed each frame.
        // These statuses are used by firing methods, so actual weapon statuses are required.
        UpdateScriptWeaponsStatus();
    }

    void TouchedNavEntity(const edict_t *underlyingEntity) override
    {
        botBrain.HandleNavTargetTouch(underlyingEntity);
    }
    virtual void TouchedOtherEntity(const edict_t *entity) override;
    void TouchedJumppad(const edict_t *jumppad);
private:
    void RegisterVisibleEnemies();

    inline bool IsPrimaryAimEnemy(const edict_t *enemy) const { return botBrain.IsPrimaryAimEnemy(enemy); }

    DangersDetector dangersDetector;
    BotBrain botBrain;

    float skillLevel;

    SelectedEnemies selectedEnemies;
    SelectedWeapons selectedWeapons;

    BotWeaponSelector weaponsSelector;

    BotTacticalSpotsCache tacticalSpotsCache;

    BotFireTargetCache builtinFireTargetCache;
    BotFireTargetCache scriptFireTargetCache;

    BotGrabItemGoal grabItemGoal;
    BotKillEnemyGoal killEnemyGoal;
    BotRunAwayGoal runAwayGoal;
    BotReactToDangerGoal reactToDangerGoal;
    BotReactToThreatGoal reactToThreatGoal;
    BotReactToEnemyLostGoal reactToEnemyLostGoal;

    BotGenericRunToItemAction genericRunToItemAction;
    BotPickupItemAction pickupItemAction;
    BotWaitForItemAction waitForItemAction;

    BotKillEnemyAction killEnemyAction;
    BotAdvanceToGoodPositionAction advanceToGoodPositionAction;
    BotRetreatToGoodPositionAction retreatToGoodPositionAction;
    BotSteadyCombatAction steadyCombatAction;
    BotGotoAvailableGoodPositionAction gotoAvailableGoodPositionAction;

    BotGenericRunAvoidingCombatAction genericRunAvoidingCombatAction;
    BotStartGotoCoverAction startGotoCoverAction;
    BotTakeCoverAction takeCoverAction;

    BotStartGotoRunAwayTeleportAction startGotoRunAwayTeleportAction;
    BotDoRunAwayViaTeleportAction doRunAwayViaTeleportAction;
    BotStartGotoRunAwayJumppadAction startGotoRunAwayJumppadAction;
    BotDoRunAwayViaJumppadAction doRunAwayViaJumppadAction;
    BotStartGotoRunAwayElevatorAction startGotoRunAwayElevatorAction;
    BotDoRunAwayViaElevatorAction doRunAwayViaElevatorAction;
    BotStopRunningAwayAction stopRunningAwayAction;

    BotDodgeToSpotAction dodgeToSpotAction;

    BotTurnToThreatOriginAction turnToThreatOriginAction;

    BotTurnToLostEnemyAction turnToLostEnemyAction;
    BotStartLostEnemyPursuitAction startLostEnemyPursuitAction;
    BotStopLostEnemyPursuitAction stopLostEnemyPursuitAction;

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
        unsigned startLandingAt;
        // Next reach. cache is lost in air.
        // Thus we have to store next areas starting a jumppad movement and try to prefer these areas for landing
        static constexpr int MAX_LANDING_AREAS = 16;
        int landingAreas[MAX_LANDING_AREAS];
        int landingAreasCount;
        Vec3 jumppadTarget;

        inline JumppadMovementState()
            : hasTouchedJumppad(false),
              hasEnteredJumppad(false),
              startLandingAt(0),
              landingAreasCount(0),
              jumppadTarget(INFINITY, INFINITY, INFINITY) {}

        inline bool IsActive() const
        {
            return hasTouchedJumppad || hasEnteredJumppad;
        }

        inline bool ShouldPerformLanding() const
        {
            return startLandingAt <= level.time;
        }

        inline void Invalidate()
        {
            hasTouchedJumppad = false;
            hasEnteredJumppad = false;
        }
    };

    JumppadMovementState jumppadMovementState;

    struct RocketJumpMovementState
    {
        const edict_t *self;

        Vec3 jumpTarget;
        Vec3 fireTarget;
        bool hasPendingRocketJump;
        bool hasTriggeredRocketJump;
        bool hasCorrectedRocketJump;
        unsigned timeoutAt;

        RocketJumpMovementState(const edict_t *self_)
            : self(self_),
              jumpTarget(INFINITY, INFINITY, INFINITY),
              fireTarget(INFINITY, INFINITY, INFINITY),
              hasPendingRocketJump(false),
              hasTriggeredRocketJump(false),
              hasCorrectedRocketJump(false),
              timeoutAt(0) {}

        inline bool IsActive() const
        {
            return hasPendingRocketJump || hasTriggeredRocketJump || hasCorrectedRocketJump;
        }

        inline void TryInvalidate()
        {
            if (IsActive())
            {
                if (self->groundentity || (jumpTarget - self->s.origin).SquaredLength() < 48 * 48)
                    Invalidate();
            }
        }

        void Invalidate()
        {
            hasPendingRocketJump = false;
            hasTriggeredRocketJump = false;
            hasCorrectedRocketJump = false;
        }

        void SetPending(const Vec3 &jumpTarget_, const Vec3 &fireTarget_, unsigned timeoutPeriod)
        {
            this->jumpTarget = jumpTarget_;
            this->fireTarget = fireTarget_;
            hasPendingRocketJump = true;
            hasTriggeredRocketJump = false;
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

        inline void SetTriggered(const Vec3 &lookAtPoint_, float turnSpeedMultiplier_ = 0.5f, unsigned timeoutPeriod = 500)
        {
            this->lookAtPoint = lookAtPoint_;
            this->turnSpeedMultiplier = turnSpeedMultiplier_;
            this->timeoutAt = level.time + timeoutPeriod;
            this->isTriggered = true;
        }

        inline float EffectiveTurnSpeedMultiplier(float baseTurnSpeedMultiplier) const
        {
            return isTriggered ? turnSpeedMultiplier : baseTurnSpeedMultiplier;
        }

        inline void Invalidate()
        {
            isTriggered = false;
            timeoutAt = 0;
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

        inline void SetWithoutDirection(const Vec3 &spotOrigin_, float spotRadius_, float alertness_)
        {
            isTriggered = true;
            hasLookAtPoint = false;
            this->spotOrigin = spotOrigin_;
            this->spotRadius = spotRadius_;
            this->alertness = alertness_;
            strafeTimeoutAt = 0;
            lookAtPointTimeoutAt = 0;
        }

        inline void SetDirectional(const Vec3 &spotOrigin_, const Vec3 &lookAtPoint_, float spotRadius_, float alertness_)
        {
            isTriggered = true;
            hasLookAtPoint = true;
            this->spotOrigin = spotOrigin_;
            this->lookAtPoint = lookAtPoint_;
            this->spotRadius = spotRadius_;
            this->alertness = alertness_;
            strafeTimeoutAt = 0;
            lookAtPointTimeoutAt = 0;
        }

        inline void Invalidate()
        {
            isTriggered = false;
        }
    };

    CampingSpotState campingSpotState;

    bool isInSquad;

    int defenceSpotId;
    int offenseSpotId;

    struct AlertSpot: public AiAlertSpot
    {
        unsigned lastReportedAt;
        float lastReportedScore;
        AlertCallback callback;
        void *receiver;

        AlertSpot(const AiAlertSpot &spot, AlertCallback callback_, void *receiver_)
            : AiAlertSpot(spot),
              lastReportedAt(0),
              lastReportedScore(0.0f),
              callback(callback_),
              receiver(receiver_) {};

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

    StaticVector<AiScriptWeaponDef, MAX_SCRIPT_WEAPONS> scriptWeaponDefs;
    StaticVector<int, MAX_SCRIPT_WEAPONS> scriptWeaponCooldown;

    unsigned lastTouchedTeleportAt;
    unsigned lastTouchedJumppadAt;
    unsigned lastTouchedElevatorAt;

    unsigned similarWorldStateInstanceId;

    inline unsigned NextSimilarWorldStateInstanceId()
    {
        return ++similarWorldStateInstanceId;
    }

    class AimingRandomHolder
    {
        unsigned valuesTimeoutAt[3];
        float values[3];
    public:
        inline AimingRandomHolder()
        {
            std::fill_n(valuesTimeoutAt, 3, 0);
            std::fill_n(values, 3, 0.5f);
        }
        inline float GetCoordRandom(int coordNum)
        {
            if (valuesTimeoutAt[coordNum] <= level.time)
            {
                values[coordNum] = random();
                valuesTimeoutAt[coordNum] = level.time + 128 + From0UpToMax(256, random());
            }
            return values[coordNum];
        }
    };

    AimingRandomHolder aimingRandomHolder;

    void UpdateScriptWeaponsStatus();

    void Move(BotInput *input, bool mayHitWhileRunning);
    void LookAround();
    void ChangeWeapons(const SelectedWeapons &selectedWeapons);
    void ChangeWeapon(int weapon);
    void FireWeapon(BotInput *input);
    virtual void OnBlockedTimeout() override;
    void SayVoiceMessages();
    void GhostingFrame();
    void ActiveFrame();
    void CallGhostingClientThink(BotInput *input);
    void CallActiveClientThink(BotInput *input);

    void OnRespawn();

    inline bool HasPendingLookAtPoint() const
    {
        return pendingLookAtPointState.IsActive();
    }
    inline void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutPeriod = 500)
    {
        pendingLookAtPointState.SetTriggered(point, turnSpeedMultiplier, timeoutPeriod);
    }
    void ApplyPendingTurnToLookAtPoint(BotInput *input);
    void ApplyInput(BotInput *input);

    // Must be called on each frame
    void MoveFrame(BotInput *input);

    void MoveOnLadder(BotInput *input);
    void MoveEnteringJumppad(BotInput *input);
    void MoveRidingJummpad(BotInput *input);
    void MoveInRocketJumpState(BotInput *input, bool mayHitWhileMoving);
    void MoveOnPlatform(BotInput *input);
    void MoveCampingASpot(BotInput *input);
    void MoveCampingASpotWithGivenLookAtPoint(const Vec3 &givenLookAtPoint, BotInput *input);
    void MoveSwimming(BotInput *input);
    void MoveGenericRunning(BotInput *input, bool mayHitWhileRunning);
    Vec3 GetCheatingAcceleratedVelocity(float velocity2DDirDotToTarget2DDir, bool hasObstacles) const;
    Vec3 GetCheatingCorrectedVelocity(float velocity2DDirDotToTarget2DDir, Vec3 toTargetDir2D) const;
    bool CanFlyAboveGroundRelaxed() const;
    bool CheckAndTryAvoidObstacles(BotInput *input, float speed);
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
    void TryLandOnNearbyAreas(BotInput *input);
    bool TryLandOnArea(int areaNum, BotInput *input);
    void CheckTargetProximity();

    inline bool IsCloseToNavTarget()
    {
        return botBrain.IsCloseToNavTarget(96.0f);
    }

    bool MaySetPendingLandingDash();
    void SetPendingLandingDash(BotInput *input);
    void ApplyPendingLandingDash(BotInput *input);

    bool TryRocketJumpShortcut(BotInput *input);
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
    // Tries to select an appropriate weapon and schedule a pending rocketjump.
    // Assumes that targetOrigin and fireTarget are checked.
    // Returns false if a rocketjump cannot be triggered.
    bool TrySetPendingWeaponJump(const Vec3 &targetOrigin, const Vec3 &fireTarget);
    // Triggers a jump/dash and fire actions. It is assumed to be called from MoveInRocketJumpState()
    // Assumes that targetOrigin and fireTarget are checked.
    // Make sure you have selected an appropriate weapon and its ready to fire before you call it.
    void TriggerWeaponJump(BotInput *input, const Vec3 &targetOrigin, const Vec3 &fireTarget);

    void CombatMovement(BotInput *input);
    void UpdateCombatMovePushes();
    bool MayApplyCombatDash();
    void ApplyCheatingGroundAcceleration(const BotInput *input);

    // Returns true if current look angle worth pressing attack
    bool CheckShot(const AimParams &aimParams, const BotInput *input,
                   const SelectedEnemies &selectedEnemies, const GenericFireDef &fireDef);

    void LookAtEnemy(float accuracy, const vec3_t fire_origin, vec3_t target, BotInput *input);
    void PressAttack(const GenericFireDef *fireDef, const GenericFireDef *builtinFireDef,
                     const GenericFireDef *scriptFireDef, BotInput *input);

    bool MayHitWhileRunning() const;
    void CheckTurnToBackwardsMovement(BotInput *input) const;
    void SetDefaultBotInput(BotInput *input) const;

    inline bool HasEnemy() const { return selectedEnemies.AreValid(); }
    inline bool IsEnemyAStaticSpot() const { return selectedEnemies.IsStaticSpot(); }
    inline const edict_t *EnemyTraceKey() const { return selectedEnemies.TraceKey(); }
    inline const bool IsEnemyOnGround() const { return selectedEnemies.OnGround(); }
    inline Vec3 EnemyOrigin() const { return selectedEnemies.LastSeenOrigin(); }
    inline Vec3 EnemyLookDir() const { return selectedEnemies.LookDir(); }
    inline unsigned EnemyFireDelay() const { return selectedEnemies.FireDelay(); }
    inline Vec3 EnemyVelocity() const { return selectedEnemies.LastSeenVelocity(); }
    inline Vec3 EnemyMins() const { return selectedEnemies.Mins(); }
    inline Vec3 EnemyMaxs() const { return selectedEnemies.Maxs(); }

    inline bool WillAdvance() const { return botBrain.WillAdvance(); }
    inline bool WillRetreat() const { return botBrain.WillRetreat(); }

    inline bool ShouldCloak() const { return botBrain.ShouldCloak(); }
    inline bool ShouldBeSilent() const { return botBrain.ShouldBeSilent(); }
    inline bool ShouldMoveCarefully() const { return botBrain.ShouldMoveCarefully(); }

    inline bool ShouldAttack() const { return botBrain.ShouldAttack(); }
    inline bool ShouldKeepXhairOnEnemy() const { return botBrain.ShouldKeepXhairOnEnemy(); }

    inline bool WillAttackMelee() const { return botBrain.WillAttackMelee(); }
    inline bool ShouldRushHeadless() const { return botBrain.ShouldRushHeadless(); }
};

#endif
