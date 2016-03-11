#include "bot.h"

//==========================================
// BOT_DMclass_CheckShot
// Checks if shot is blocked (doesn't verify it would hit)
//==========================================
bool Bot::CheckShot(vec3_t point)
{
    trace_t tr;
    vec3_t start, forward, right, offset;

    if( random() > self->ai->pers.cha.firerate )
        return false;

    AngleVectors( self->r.client->ps.viewangles, forward, right, NULL );

    VectorSet( offset, 0, 0, self->viewheight );
    G_ProjectSource( self->s.origin, offset, forward, right, start );

    // blocked, don't shoot
    G_Trace( &tr, start, vec3_origin, vec3_origin, point, self, MASK_AISOLID );
    if( tr.fraction < 0.8f )
    {
        if( tr.ent < 1 || !game.edicts[tr.ent].takedamage || game.edicts[tr.ent].movetype == MOVETYPE_PUSH )
            return false;

        // check if the player we found is at our team
        if( game.edicts[tr.ent].s.team == self->s.team && GS_TeamBasedGametype() )
            return false;
    }

    return true;
}

//==========================================
// BOT_DMclass_PredictProjectileShot
// predict target movement
//==========================================
void Bot::PredictProjectileShot(const vec3_t fire_origin, float projectile_speed, vec3_t target, const vec3_t target_velocity)
{
    vec3_t predictedTarget;
    vec3_t targetMovedir;
    float targetSpeed;
    float predictionTime;
    float distance;
    trace_t	trace;
    int contents;

    if( projectile_speed <= 0.0f )
        return;

    targetSpeed = VectorNormalize2( target_velocity, targetMovedir );

    // ok, this is not going to be 100% precise, since we will find the
    // time our projectile will take to travel to enemy's CURRENT position,
    // and them find enemy's position given his CURRENT velocity and his CURRENT dir
    // after prediction time. The result will be much better if the player
    // is moving to the sides (relative to us) than in depth (relative to us).
    // And, of course, when the player moves in a curve upwards it will totally miss (ie, jumping).

    // but in general it does a great job, much better than any human player :)

    distance = DistanceFast( fire_origin, target );
    predictionTime = distance/projectile_speed;
    VectorMA( target, predictionTime*targetSpeed, targetMovedir, predictedTarget );

    // if this position is inside solid, try finding a position at half of the prediction time
    contents = G_PointContents( predictedTarget );
    if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
    {
        VectorMA( target, ( predictionTime * 0.5f )*targetSpeed, targetMovedir, predictedTarget );
        contents = G_PointContents( predictedTarget );
        if( contents & CONTENTS_SOLID && !( contents & CONTENTS_PLAYERCLIP ) )
            return; // INVALID
    }

    // if we can see this point, we use it, otherwise we keep the current position
    G_Trace( &trace, const_cast<float*>(fire_origin), vec3_origin, vec3_origin, predictedTarget, self, MASK_SHOT );
    if( trace.fraction == 1.0f || ( trace.ent && game.edicts[trace.ent].takedamage ) )
        VectorCopy( predictedTarget, target );
}

//==========================================
// BOT_DMclass_FireWeapon
// Fire if needed
//==========================================
bool Bot::FireWeapon(usercmd_t *ucmd)
{
#define WFAC_GENERIC_PROJECTILE 300.0
#define WFAC_GENERIC_INSTANT 150.0
    float firedelay;
    vec3_t target;
    int weapon, i;
    float wfac;
    vec3_t fire_origin;
    trace_t trace;
    bool continuous_fire = false;
    firedef_t *firedef = GS_FiredefForPlayerState(&self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON]);

    CombatTask &combatTask = enemyPool.combatTask;

    if (!combatTask.aimEnemy && !combatTask.spamEnemy)
        return false;

    weapon = self->s.weapon;
    if (weapon < 0 || weapon >= WEAP_TOTAL)
        weapon = 0;

    if (!firedef)
        return false;

    if (combatTask.aimEnemy)
    {
        const edict_t *enemy = combatTask.aimEnemy->ent;
        for (i = 0; i < 3; i++)
        {
            target[i] = enemy->s.origin[i] + (0.5f * (enemy->r.maxs[i] + enemy->r.mins[i]));
        }
    }
    else
    {
        VectorCopy(combatTask.spamSpot.vec, target);
    }

    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;

    if (self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN)
        continuous_fire = true;

    edict_t *targetEnt;
    edict_t dummyEnt;
    if (combatTask.aimEnemy)
    {
        targetEnt = const_cast<edict_t *>(combatTask.aimEnemy->ent);
    }
    else
    {
        VectorCopy(combatTask.spamSpot.vec, dummyEnt.s.origin);
        targetEnt = &dummyEnt;
    }

    bool isInFront = Ai::IsInFront(targetEnt);
    bool mayHit = CheckShot(target);

    if( !continuous_fire && !(isInFront && mayHit))
        return false;

    // find out our weapon AIM style
    if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION_EXPLOSIVE )
    {
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
        {
            if (combatTask.aimEnemy)
                PredictProjectileShot(fire_origin, firedef->speed, target, combatTask.aimEnemy->ent->velocity);
            else
                PredictProjectileShot(fire_origin, firedef->speed, target, vec3_origin);
        }

        wfac = WFAC_GENERIC_PROJECTILE * 1.3;

        if (combatTask.aimEnemy)
        {
            // aim to the feet when enemy isn't higher
            if (fire_origin[2] > (target[2] + (self->enemy->r.mins[2] * 0.8)))
            {
                vec3_t checktarget;
                VectorSet(checktarget,
                          self->enemy->s.origin[0],
                          self->enemy->s.origin[1],
                          self->enemy->s.origin[2] + self->enemy->r.mins[2] + 4);

                G_Trace(&trace, fire_origin, vec3_origin, vec3_origin, checktarget, self, MASK_SHOT);
                if (trace.fraction == 1.0f || (trace.ent > 0 && game.edicts[trace.ent].takedamage))
                    VectorCopy(checktarget, target);
            }
            else if (!IsStep(self->enemy))
                wfac *= 2.5; // more imprecise for air rockets
        }
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION )
    {
        if( self->s.weapon == WEAP_PLASMAGUN )
            wfac = WFAC_GENERIC_PROJECTILE * 0.5;
        else
            wfac = WFAC_GENERIC_PROJECTILE;

        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
        {
            if (combatTask.aimEnemy)
                PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);
            else
                PredictProjectileShot(fire_origin, firedef->speed, target, vec3_origin);
        }
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_DROP )
    {
        //jalToDo
        wfac = WFAC_GENERIC_PROJECTILE;
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
        {
            const float *velocity = combatTask.aimEnemy ? combatTask.aimEnemy->ent->velocity : vec3_origin;
            PredictProjectileShot(fire_origin, firedef->speed, target, velocity);
        }
    }
    else // AI_AIMSTYLE_INSTANTHIT
    {
        if( self->s.weapon == WEAP_ELECTROBOLT )
            wfac = WFAC_GENERIC_INSTANT;
        else if( self->s.weapon == WEAP_LASERGUN )
            wfac = WFAC_GENERIC_INSTANT * 1.5;
        else
            wfac = WFAC_GENERIC_INSTANT;
    }

    wfac = 25 + wfac * ( 1.0f - self->ai->pers.skillLevel );

    // look to target
    VectorSubtract( target, fire_origin, self->ai->move_vector );

    if( self->r.client->ps.weaponState == WEAPON_STATE_READY ||
        self->r.client->ps.weaponState == WEAPON_STATE_REFIRE ||
        self->r.client->ps.weaponState == WEAPON_STATE_REFIRESTRONG )
    {
        // in continuous fire weapons don't add delays
        if( self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN )
            firedelay = 1.0f;
        else
            firedelay = ( 1.0f - self->ai->pers.skillLevel ) - ( random()-0.25f );

        if( firedelay > 0.0f )
        {
            if (mayHit)
            {
                ucmd->buttons |= BUTTON_ATTACK;
            }
            // mess up angles only in the attacking frames
            if( self->r.client->ps.weaponState == WEAPON_STATE_READY ||
                self->r.client->ps.weaponState == WEAPON_STATE_REFIRE ||
                self->r.client->ps.weaponState == WEAPON_STATE_REFIRESTRONG )
            {
                if( (self->s.weapon == WEAP_LASERGUN) || (self->s.weapon == WEAP_PLASMAGUN) ) {
                    target[0] += sinf( (float)level.time/100.0) * wfac;
                    target[1] += cosf( (float)level.time/100.0) * wfac;
                }
                else
                {
                    target[0] += ( random()-0.5f ) * wfac;
                    target[1] += ( random()-0.5f ) * wfac;
                }
            }
        }
    }

    //update angles
    VectorSubtract( target, fire_origin, self->ai->move_vector );
    ChangeAngle();

    if( nav.debugMode && bot_showcombat->integer )
    {
        if (combatTask.aimEnemy)
        {
            const edict_t *enemy = combatTask.aimEnemy->ent;
            const char *enemyName = enemy->r.client ? enemy->r.client->netname : enemy->classname;
            G_PrintChasersf(self, "%s: attacking %s\n", self->ai->pers.netname, enemyName);
        }
    }
    return true;
}