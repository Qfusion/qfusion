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
    bool CheckShot(const vec3_t point);
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

private:
    bool TacticsToAprioriMovePushes(int *tacticalXMove, int *tacticalYMove);
    std::pair<int, int> ApplyTacticalMove(
        int tacticalMove, bool advance, const MoveTestResult &positiveDirTest, const MoveTestResult &negativeDirTest);

    void RegisterVisibleEnemies();

    DangersDetector dangersDetector;
    BotBrain botBrain;

    float skillLevel;
    bool printLink;

    bool isBunnyHopping;
    bool hasTriggeredRj;
    unsigned rjTimeout;

    bool hasTriggeredJumppad;
    // This timeout is set when bot triggers a jumppad (in MoveEnteringJumppad).
    // Bot tries to keep flying even if next reach. cache is empty if the timeout is greater than level time.
    // If there are no cached reach.'s and the timeout is not greater than level time bot tries to find area to land to.
    unsigned jumppadMoveTimeout;
    // We have to store next areas props since reach. cache is likely to be lost in flight
    int jumppadDestAreaNum;
    Vec3 jumppadReachEndPoint;

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

    Vec3 cachedMoveVec;
    bool hasCachedMoveVec;

    Vec3 cachedPredictedTargetOrigin;
    unsigned cachedPredictedTargetValidUntil;
    unsigned cachedPredictedTargetInstanceId;

    void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutDuration = 500);

    void ApplyPendingTurnToLookAtPoint();

    inline const int *Inventory() const { return self->r.client->ps.inventory; }

    // Must be called on each frame
    void MoveFrame(usercmd_t *ucmd, bool inhibitCombat);

    void MoveOnLadder(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveEnteringJumppad(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveRidingJummpad(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveRidingPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveEnteringPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveFallingOrJumping(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveStartingARocketjump(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *moveVec, usercmd_t *ucmd);
    bool CheckAndTryAvoidObstacles(Vec3 *moveVec, usercmd_t *ucmd, float speed);
    void StraightenOrInterpolateMoveVec(Vec3 *moveVec, float speed);
    bool TryStraightenMoveVec(Vec3 *moveVec, float speed);
    void InterpolateMoveVec(Vec3 *moveVec, float speed);
    void SetMoveVecToPendingReach(Vec3 *moveVec);
    void TryLandOnNearbyAreas(Vec3 *moveVec, usercmd_t *ucmd);
    bool TryLandOnArea(int areaNum, Vec3 *moveVec, usercmd_t *ucmd);
    void CheckTargetReached();
    inline bool IsCloseToAnyGoal()
    {
        return botBrain.IsCloseToShortTermGoal() || botBrain.IsCloseToLongTermGoal();
    }

    void SetPendingLandingDash(usercmd_t *ucmd);
    bool TryApplyPendingLandingDash(usercmd_t *ucmd);
    // Returns true if a pending landing dash has timed out
    bool CheckPendingLandingDashTimedOut();

    // Returns true if the bot is at least a bit blocked
    void TryMoveAwayIfBlocked(usercmd_t *ucmd);

    void ApplyEvadeMovePushes(usercmd_t *ucmd);
    bool MayApplyCombatDash();
    Vec3 MakeEvadeDirection(const class Danger &danger);

    void SetupCoarseFireTarget(vec3_t fire_origin, vec3_t target);
    void CheckEnemyInFrontAndMayBeHit(const vec3_t target, bool *inFront, bool *mayHitApriory);
    // All these methods return suggested accuracy
    float AdjustTarget(int weapon, const firedef_t *firedef, vec_t *fire_origin, vec_t *target);
    float AdjustPredictionExplosiveAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustPredictionAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustDropAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);
    float AdjustInstantAimStyleTarget(const firedef_t *firedef, vec3_t fire_origin, vec3_t target);

    inline bool HasCachedTargetOrigin() const
    {
        return EnemyInstanceId() == cachedPredictedTargetInstanceId && cachedPredictedTargetValidUntil > level.time;
    }

    void GetPredictedTargetOrigin(const vec3_t fireOrigin, float projSpeed, vec3_t target);
    void PredictProjectileShot(
        const vec3_t fireOrigin, float projSpeed, vec3_t target, const vec3_t targetVelocity, bool applyTargetGravity);

    // Returns true if current look angle worth pressing attack
    bool LookAtEnemy(float wfac, const vec3_t fire_origin, vec3_t target);
    bool TryPressAttack(bool importantShot);

    inline bool HasEnemy() const { return !botBrain.combatTask.Empty(); }
    inline bool IsEnemyStatic() const { return botBrain.combatTask.IsTargetStatic(); }
    inline const edict_t *EnemyTraceKey() const { return botBrain.combatTask.TraceKey(); }
    inline Vec3 EnemyOrigin() const { return botBrain.combatTask.EnemyOrigin(); }
    inline Vec3 EnemyVelocity() const { return botBrain.combatTask.EnemyVelocity(); }
    inline Vec3 EnemyMins() const { return botBrain.combatTask.EnemyMins(); }
    inline Vec3 EnemyMaxs() const { return botBrain.combatTask.EnemyMaxs(); }
    inline unsigned EnemyInstanceId() const { return botBrain.combatTask.instanceId; }
};

#endif
