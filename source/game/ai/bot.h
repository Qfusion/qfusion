#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"
#include "enemy_pool.h"

class Bot: public Ai
{
public:
    Bot(edict_t *self): Ai(self), dangersDetector(self), enemyPool(self), printLink(false) {}

    using Ai::SpecialMove;
    void SpecialMove(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    void Move(usercmd_t *ucmd);
    void MoveWander(usercmd_t *ucmd);
    void CombatMovement(usercmd_t *ucmd);
    void LookAround();
    bool ChangeWeapon(int weapon);
    bool CheckShot(vec3_t point);
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

    bool MoveOnLadder(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveOnJumppad(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveRidingPlatform(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveEnteringPlatform(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveFallingOrJumping(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveStartingAJump(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveStartingARocketjump(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd);
    bool MoveLikeHavingShortGoal(const vec3_t lookdir, const vec3_t pathdir, usercmd_t *ucmd, bool specialMovement);
    void TryMoveAwayIfBlocked(usercmd_t *ucmd);

    void ApplyEvadeMovePushes(usercmd_t *ucmd);
    bool MayApplyCombatDash();
    Vec3 MakeEvadeDirection(const class Danger &danger);
};

#endif
