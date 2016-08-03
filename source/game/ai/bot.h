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
    friend class AiGametypeBrain;
    friend class AiBaseTeamBrain;
    friend class BotBrain;
    friend class AiSquad;
    friend class AiBaseEnemyPool;
public:
    static constexpr auto PREFERRED_TRAVEL_FLAGS =
        TFL_WALK | TFL_WALKOFFLEDGE | TFL_JUMP | TFL_AIR | TFL_TELEPORT | TFL_JUMPPAD;
    static constexpr auto ALLOWED_TRAVEL_FLAGS =
        PREFERRED_TRAVEL_FLAGS | TFL_WATER | TFL_WATERJUMP | TFL_SWIM | TFL_LADDER | TFL_ELEVATOR;

    Bot(edict_t *self, float skillLevel);
    virtual ~Bot() override {}

    void Move(usercmd_t *ucmd);
    void LookAround();
    bool ChangeWeapon(int weapon);
    bool FireWeapon();
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

    void OnRespawn();

    inline float Skill() const { return skillLevel; }
    inline bool IsReady() const { return level.ready[PLAYERNUM(self)]; }

    inline void OnAttachedToSquad(AiSquad *squad)
    {
        botBrain.OnAttachedToSquad(squad);
    }
    inline void OnDetachedFromSquad(AiSquad *squad)
    {
        botBrain.OnDetachedFromSquad(squad);
    }
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
    inline void ClearExternalEntityWeights()
    {
        botBrain.ClearExternalEntityWeights();
    }
    inline void SetExternalEntityWeight(const edict_t *ent, float weight)
    {
        botBrain.SetExternalEntityWeight(ent, weight);
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
protected:
    virtual void Frame() override;
    virtual void Think() override;

    virtual void TouchedGoal(const edict_t *goalUnderlyingEntity) override;
    virtual void TouchedJumppad(const edict_t *jumppad) override;
private:
    void RegisterVisibleEnemies();

    inline bool IsPrimaryAimEnemy(const edict_t *enemy) const { return botBrain.IsPrimaryAimEnemy(enemy); }
    inline bool IsOldHiddenEnemy(const edict_t *enemy) const { return botBrain.IsOldHiddenEnemy(enemy); }

    DangersDetector dangersDetector;
    BotBrain botBrain;

    float skillLevel;

    unsigned nextBlockedEscapeAttemptAt;
    Vec3 blockedEscapeGoalOrigin;

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
    int jumppadLandingAreas[MAX_LANDING_AREAS];
    int jumppadLandingAreasCount;
    Vec3 jumppadTarget;

    Vec3 rocketJumpTarget;
    Vec3 rocketJumpFireTarget;
    bool hasTriggeredRocketJump;
    bool hasCorrectedRocketJump;
    bool wasTriggeredRocketJumpPrevFrame;
    unsigned rocketJumpTimeoutAt;

    bool hasPendingLandingDash;
    bool isOnGroundThisFrame;
    bool wasOnGroundPrevFrame;
    unsigned pendingLandingDashTimeout;
    float requestedViewTurnSpeedMultiplier;

    unsigned combatMovePushTimeout;
    int combatMovePushes[3];

    unsigned vsayTimeout;

    Vec3 pendingLookAtPoint;
    unsigned pendingLookAtPointTimeoutAt;
    bool hasPendingLookAtPoint;
    float lookAtPointTurnSpeedMultiplier;

    Vec3 cachedPredictedTargetOrigin;
    unsigned cachedPredictedTargetValidUntil;
    unsigned cachedPredictedTargetInstanceId;

    // If it is set, the bot should stay on a spot defined by campingSpotOrigin
    bool hasCampingSpot;
    // If it is set, the bot should prefer to look at the campingSpotLookAtPoint while camping
    // Otherwise the bot should spin view randomly
    bool hasCampingLookAtPoint;
    // Maximum bot origin deviation from campingSpotOrigin while strafing when camping a spot
    float campingSpotRadius;
    // 0..1, greater values result in frequent and hectic strafing/camera rotating
    float campingAlertness;
    Vec3 campingSpotOrigin;
    Vec3 campingSpotLookAtPoint;
    Vec3 campingSpotStrafeDir;
    // When to change chosen strafe dir
    unsigned campingSpotStrafeTimeout;
    // When to change randomly chosen look-at-point (if the point is not initially specified)
    unsigned campingSpotLookAtPointTimeout;

    bool isWaitingForItemSpawn;

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

    void SetCampingSpot(const Vec3 &spotOrigin, float spotRadius, float alertness = 0.5f);
    void SetCampingSpot(const Vec3 &spotOrigin, const Vec3 &lookAtPoint, float spotRaduis, float alertness = 0.5f);
    void ClearCampingSpot();

    void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutDuration = 500);

    void ApplyPendingTurnToLookAtPoint();

    // Must be called on each frame
    void MoveFrame(usercmd_t *ucmd, bool inhibitCombat);

    void MoveOnLadder(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveEnteringJumppad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveRidingJummpad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveTriggeredARocketJump(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveOnPlatform(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpot(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpotWithGivenLookAtPoint(const Vec3 &givenLookAtPoint, Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *intendedLookVec, usercmd_t *ucmd);
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

    void SetPendingLandingDash(usercmd_t *ucmd);
    bool TryApplyPendingLandingDash(usercmd_t *ucmd);
    // Returns true if a pending landing dash has timed out
    bool CheckPendingLandingDashTimedOut();

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

    void SetupCoarseFireTarget(vec3_t fire_origin, vec3_t target);
    // Returns true if current look angle worth pressing attack
    bool CheckShot(const vec3_t fire_origin, const vec3_t target);
    // All these methods return suggested accuracy
    float AdjustTarget(int weapon, const firedef_t *firedef, vec_t *fire_origin, vec_t *target);
    float AdjustPredictionExplosiveAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustPredictionAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustDropAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustInstantAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);

    // Returns true is a shootable environment for inflicting a splash damage has been found
    bool AdjustTargetByEnvironment(const firedef_t *firedef, const vec3_t fire_origin, vec3_t target);
    bool AdjustTargetByEnvironmentTracing(float splashRadius, const vec3_t fire_origin, vec3_t target);
    bool AdjustTargetByEnvironmentWithAAS(float splashRadius, const vec3_t fire_origin, vec3_t target, int targetAreaNum);

    inline bool HasCachedTargetOrigin() const
    {
        return EnemyInstanceId() == cachedPredictedTargetInstanceId && cachedPredictedTargetValidUntil > level.time;
    }

    void GetPredictedTargetOrigin(const vec3_t fireOrigin, float projSpeed, vec3_t target);
    void PredictProjectileShot(
        const vec3_t fireOrigin, float projSpeed, vec3_t target, const vec3_t targetVelocity, bool applyTargetGravity);

    void LookAtEnemy(float wfac, const vec3_t fire_origin, vec3_t target);
    bool TryPressAttack();

    inline bool HasEnemy() const { return !botBrain.combatTask.Empty(); }
    inline bool IsEnemyAStaticSpot() const { return botBrain.combatTask.IsTargetAStaticSpot(); }
    inline const edict_t *EnemyTraceKey() const { return botBrain.combatTask.TraceKey(); }
    inline const bool IsEnemyOnGround() const { return botBrain.combatTask.IsOnGround(); }
    inline Vec3 EnemyOrigin() const { return botBrain.combatTask.EnemyOrigin(); }
    inline Vec3 EnemyLookDir() const { return botBrain.combatTask.EnemyLookDir(); }
    inline unsigned EnemyFireDelay() const { return botBrain.combatTask.EnemyFireDelay(); }
    inline Vec3 EnemyVelocity() const { return botBrain.combatTask.EnemyVelocity(); }
    inline Vec3 EnemyMins() const { return botBrain.combatTask.EnemyMins(); }
    inline Vec3 EnemyMaxs() const { return botBrain.combatTask.EnemyMaxs(); }
    inline unsigned EnemyInstanceId() const { return botBrain.combatTask.instanceId; }

    bool MayKeepRunningInCombat() const;

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
