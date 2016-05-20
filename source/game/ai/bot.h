#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "bot_brain.h"

class Bot: public Ai
{
    friend class AiGametypeBrain;
    friend class AiBaseTeamBrain;
    friend class BotBrain;
public:
    Bot(edict_t *self, float skillLevel);

    void Move(usercmd_t *ucmd);
    void CombatMovement(usercmd_t *ucmd, bool hasDangers);
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

protected:
    virtual void Frame() override;
    virtual void Think() override;

    virtual void TouchedGoal(const edict_t *goalUnderlyingEntity) override;
private:
    bool TacticsToAprioriMovePushes(int *tacticalXMove, int *tacticalYMove);
    std::pair<int, int> ApplyTacticalMove(
        int tacticalMove, bool advance, const MoveTestResult &positiveDirTest, const MoveTestResult &negativeDirTest);

    void RegisterVisibleEnemies();

    DangersDetector dangersDetector;
    BotBrain botBrain;

    float skillLevel;
    bool printLink;

    bool hasTriggeredRj;
    unsigned rjTimeout;

    bool hasTriggeredJumppad;
    // This timeout is set when bot triggers a jumppad (in MoveEnteringJumppad).
    // Bot tries to keep flying even if next reach. cache is empty if the timeout is greater than level time.
    // If there are no cached reach.'s and the timeout is not greater than level time bot tries to find area to land to.
    unsigned jumppadMoveTimeout;
    // Next reach. cache is lost in air.
    // Thus we have to store next areas starting a jumppad movement and try to prefer these areas for landing
    static constexpr int MAX_LANDING_AREAS = 16;
    int jumppadLandingAreas[MAX_LANDING_AREAS];
    int jummpadLandingAreasCount;

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

    void SetCampingSpot(const Vec3 &spotOrigin, float spotRadius, float alertness = 0.5f);
    void SetCampingSpot(const Vec3 &spotOrigin, const Vec3 &lookAtPoint, float spotRaduis, float alertness = 0.5f);
    void ClearCampingSpot();

    void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutDuration = 500);

    void ApplyPendingTurnToLookAtPoint();

    inline const int *Inventory() const { return self->r.client->ps.inventory; }

    // Must be called on each frame
    void MoveFrame(usercmd_t *ucmd, bool inhibitCombat);

    void MoveOnLadder(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveEnteringJumppad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveRidingJummpad(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveOnPlatform(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveStartingARocketjump(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpot(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveCampingASpotWithGivenLookAtPoint(const Vec3 &givenLookAtPoint, Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *intendedLookVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *intendedLookVec, usercmd_t *ucmd);
    bool CheckAndTryAvoidObstacles(Vec3 *intendedLookVec, usercmd_t *ucmd, float speed);
    void StraightenOrInterpolateLookVec(Vec3 *intendedLookVec, float speed);
    void InterpolateLookVec(Vec3 *intendedLookVec, float speed);
    void SetLookVecToPendingReach(Vec3 *intendedLookVec);
    void TryLandOnNearbyAreas(Vec3 *intendedLookVec, usercmd_t *ucmd);
    bool TryLandOnArea(int areaNum, Vec3 *intendedLookVec, usercmd_t *ucmd);
    void CheckTargetProximity();
    inline bool IsCloseToAnyGoal()
    {
        return botBrain.IsCloseToShortTermGoal() || botBrain.IsCloseToLongTermGoal();
    }
    void OnGoalCleanedUp(const NavEntity *goalEnt);

    void SetPendingLandingDash(usercmd_t *ucmd);
    bool TryApplyPendingLandingDash(usercmd_t *ucmd);
    // Returns true if a pending landing dash has timed out
    bool CheckPendingLandingDashTimedOut();

    // Returns true if the bot is at least a bit blocked
    void TryMoveAwayIfBlocked(usercmd_t *ucmd);

    void ApplyEvadeMovePushes(usercmd_t *ucmd);
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
    inline bool IsEnemyStatic() const { return botBrain.combatTask.IsTargetStatic(); }
    inline const edict_t *EnemyTraceKey() const { return botBrain.combatTask.TraceKey(); }
    inline const bool IsEnemyOnGround() const { return botBrain.combatTask.IsOnGround(); }
    inline Vec3 EnemyOrigin() const { return botBrain.combatTask.EnemyOrigin(); }
    inline Vec3 EnemyVelocity() const { return botBrain.combatTask.EnemyVelocity(); }
    inline Vec3 EnemyMins() const { return botBrain.combatTask.EnemyMins(); }
    inline Vec3 EnemyMaxs() const { return botBrain.combatTask.EnemyMaxs(); }
    inline unsigned EnemyInstanceId() const { return botBrain.combatTask.instanceId; }
};

#endif
