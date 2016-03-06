#ifndef AI_BOT_H
#define AI_BOT_H

#include "static_vector.h"
#include "dangers_detector.h"

class Bot: public Ai
{
public:
    Bot(edict_t *self): Ai(self), dangersDetector(self) {}

    using Ai::SpecialMove;
    void SpecialMove(vec3_t lookdir, vec3_t pathdir, usercmd_t *ucmd);
    void Move(usercmd_t *ucmd);
    void MoveWander(usercmd_t *ucmd);
    void CombatMovement(usercmd_t *ucmd);
    void FindEnemy();
    bool ChangeWeapon(int weapon);
    float ChooseWeapon();
    bool CheckShot(vec3_t point);
    void PredictProjectileShot(vec3_t fire_origin, float projectile_speed, vec3_t target, vec3_t target_velocity);
    bool FireWeapon(usercmd_t *ucmd);
    float PlayerWeight(edict_t *enemy);
    void UpdateStatus();
    void BlockedTimeout();
    void SayVoiceMessages();
    void GhostingFrame();
    void RunFrame();
private:
    DangersDetector dangersDetector;

    Vec3 MakeEvadeDirection(const class Danger &danger);
};



#endif
