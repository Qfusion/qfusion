#ifndef AI_BOT_H
#define AI_BOT_H

#include "ai_local.h"

class Bot: public Ai
{
public:
    Bot(edict_t *self): Ai(self) {}

    using Ai::SpecialMove;
    void SpecialMove(vec3_t lookdir, vec3_t pathdir, usercmd_t *ucmd);
    void Move(usercmd_t *ucmd);
    void MoveWander(usercmd_t *ucmd);
    bool FindRocket(vec3_t away_from_rocket);
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

};

#endif
