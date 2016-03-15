#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "enemy_pool.h"

class Bot: public Ai
{
    friend class EnemyPool;
public:
    Bot(edict_t *self);

    using Ai::SpecialMove;
    void Move(usercmd_t *ucmd);
    void MoveWander(usercmd_t *ucmd);
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

    Vec3 pendingLookAtPoint;
    unsigned pendingLookAtPointTimeoutAt;
    bool hasPendingLookAtPoint;

    void ApplyPendingTurnToLookAtPoint();

    bool MoveOnLadder(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveOnJumppad(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveRidingPlatform(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveEnteringPlatform(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveFallingOrJumping(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveStartingAJump(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveStartingARocketjump(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveSwimming(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveGenericRunning(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    void TryMoveBunnyHopping(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
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
