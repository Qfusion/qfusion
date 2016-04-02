#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "bot_brain.h"

class Bot: public Ai
{
    friend class BotBrain;
public:
    Bot(edict_t *self);

    void Move(usercmd_t *ucmd);
    void CombatMovement(usercmd_t *ucmd);
    void LookAround();
    bool ChangeWeapon(int weapon);
    bool CheckShot(const vec3_t point);
    void PredictProjectileShot(const vec3_t fire_origin, float projectile_speed, vec3_t target, const vec3_t target_velocity);
    bool FireWeapon(usercmd_t *ucmd);
    void Pain(const edict_t *enemy, float kick, int damage)
    {
        botBrain.OnPain(enemy, kick, damage);
    }
    void OnEnemyDamaged(const edict_t *enemy, int damage)
    {
        botBrain.OnEnemyDamaged(enemy, damage);
    }
    void UpdateStatus();
    void BlockedTimeout();
    void SayVoiceMessages();
    void GhostingFrame();
    void RunFrame();

    void OnRespawn();

    inline float Skill() { return self->ai->pers.skillLevel; }

private:
    bool TacticsToAprioriMovePushes(int *tacticalXMove, int *tacticalYMove);
    std::pair<int, int> ApplyTacticalMove(
        int tacticalMove, bool advance, const MoveTestResult &positiveDirTest, const MoveTestResult &negativeDirTest);

    void RegisterVisibleEnemies();

    DangersDetector dangersDetector;
    BotBrain botBrain;
    CombatTask aimTarget;

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

    void SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier = 0.5f, unsigned timeoutDuration = 500);

    void ApplyPendingTurnToLookAtPoint();

    inline const int *Inventory() const { return self->r.client->ps.inventory; }

    void MoveOnLadder(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveEnteringJumppad(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveRidingJummpad(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveRidingPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveEnteringPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveFallingOrJumping(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveStartingARocketjump(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *moveVec, usercmd_t *ucmd);
    void CheckAndTryAvoidObstacles(Vec3 *moveVec, usercmd_t *ucmd, float speed);
    void StraightenOrInterpolateMoveVec(Vec3 *moveVec, float speed);
    bool TryStraightenMoveVec(Vec3 *moveVec, float speed);
    void InterpolateMoveVec(Vec3 *moveVec, float speed);
    void SetMoveVecToPendingReach(Vec3 *moveVec);
    void TryLandOnNearbyAreas(Vec3 *moveVec, usercmd_t *ucmd);
    bool TryLandOnArea(int areaNum, Vec3 *moveVec, usercmd_t *ucmd);
    void CheckTargetReached();

    void SetPendingLandingDash(usercmd_t *ucmd);
    void TryApplyPendingLandingDash(usercmd_t *ucmd);
    void CheckPendingLandingDashTimedOut();

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

    // Returns true if current look angle worth pressing attack
    bool LookAtEnemy(float wfac, const vec3_t fire_origin, vec3_t target);
    void TryPressAttack(usercmd_t *ucmd, bool importantShot);

    // Name clash... we have to use a method name prefix
    inline const CombatTask &GetCombatTask() { return botBrain.combatTask; }
    inline const Enemy *AimEnemy() const { return botBrain.combatTask.aimEnemy; }
    inline const Vec3 &SpamSpot() const { return botBrain.combatTask.spamSpot; }
};

#endif
