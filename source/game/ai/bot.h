#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "enemy_pool.h"
#include "../../gameshared/q_comref.h"

class Bot: public Ai
{
    friend class EnemyPool;
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
        enemyPool.OnPain(enemy, kick, damage);
    }
    void OnEnemyDamaged(const edict_t *enemy, int damage)
    {
        enemyPool.OnEnemyDamaged(enemy, damage);
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
    EnemyPool enemyPool;
    CombatTask aimTarget;

    bool printLink;

    bool isBunnyHopping;
    bool hasTriggeredRj;
    unsigned rjTimeout;

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

    float ComputeItemWeight(const edict_t *ent, bool onlyGotGB) const;
    float ComputeWeaponWeight(const edict_t *ent, bool onlyGotGB) const;
    float ComputeAmmoWeight(const edict_t *ent) const;
    float ComputeArmorWeight(const edict_t *ent) const;
    float ComputeHealthWeight(const edict_t *ent) const;
    float ComputePowerupWeight(const edict_t *ent) const;

    void MoveOnLadder(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveOnJumppad(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveRidingPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveEnteringPlatform(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveFallingOrJumping(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveStartingAJump(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveStartingARocketjump(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveSwimming(Vec3 *moveVec, usercmd_t *ucmd);
    void MoveGenericRunning(Vec3 *moveVec, usercmd_t *ucmd);

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
    inline const CombatTask &GetCombatTask() { return enemyPool.combatTask; }
    inline const Enemy *AimEnemy() const { return enemyPool.combatTask.aimEnemy; }
    inline const Vec3 &SpamSpot() const { return enemyPool.combatTask.spamSpot; }
};

#endif
