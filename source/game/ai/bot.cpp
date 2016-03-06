#include "bot.h"
#include <vector>
#include <algorithm>

//==========================================
// BOT_DMclass_FindEnemy
// Scan for enemy (simplifed for now to just pick any visible enemy)
//==========================================
void Bot::FindEnemy()
{
#define WEIGHT_MAXDISTANCE_FACTOR 15000
    nav_ents_t *goalEnt;
    edict_t *bestTarget = NULL;
    float dist, weight, bestWeight = 9999999;
    vec3_t forward, vec;
    int i;

    if( G_ISGHOSTING( self )
        || GS_MatchState() == MATCH_STATE_COUNTDOWN
        || GS_ShootingDisabled() )
    {
        self->ai->enemyReactionDelay = 0;
        self->enemy = self->ai->latched_enemy = NULL;
        return;
    }

    // we also latch NULL enemies, so the bot can loose them
    if( self->ai->enemyReactionDelay > 0 )
    {
        self->ai->enemyReactionDelay -= game.frametime;
        return;
    }

    self->enemy = self->ai->latched_enemy;

    FOREACH_GOALENT( goalEnt )
    {
        i = goalEnt->id;

        if( !goalEnt->ent || !goalEnt->ent->r.inuse )
            continue;

        if( !goalEnt->ent->r.client ) // this may be changed, there could be enemies which aren't clients
            continue;

        if( G_ISGHOSTING( goalEnt->ent ) )
            continue;

        if( self->ai->status.entityWeights[i] <= 0 || goalEnt->ent->flags & (FL_NOTARGET|FL_BUSY) )
            continue;

        if( GS_TeamBasedGametype() && goalEnt->ent->s.team == self->s.team )
            continue;

        dist = DistanceFast( self->s.origin, goalEnt->ent->s.origin );

        // ignore very soft weighted enemies unless they are in your face
        if( dist > 500 && self->ai->status.entityWeights[i] <= 0.1f )
            continue;

        //if( dist > 700 && dist > WEIGHT_MAXDISTANCE_FACTOR * self->ai->status.entityWeights[i] )
        //	continue;

        weight = dist / self->ai->status.entityWeights[i];

        if( weight < bestWeight )
        {
            if( trap_inPVS( self->s.origin, goalEnt->ent->s.origin ) && G_Visible( self, goalEnt->ent ) )
            {
                bool close = dist < 2000 || goalEnt->ent == self->ai->last_attacker;

                if( !close )
                {
                    VectorSubtract( goalEnt->ent->s.origin, self->s.origin, vec );
                    VectorNormalize( vec );
                    close = DotProduct( vec, forward ) > 0.3;
                }

                if( close )
                {
                    bestWeight = weight;
                    bestTarget = goalEnt->ent;
                }
            }
        }
    }

    NewEnemyInView( bestTarget );
#undef WEIGHT_MAXDISTANCE_FACTOR
}

//==========================================
// BOT_DMClass_ChangeWeapon
//==========================================
bool Bot::ChangeWeapon(int weapon)
{
    if( weapon == self->r.client->ps.stats[STAT_PENDING_WEAPON] )
        return false;

    if( !GS_CheckAmmoInWeapon( &self->r.client->ps , weapon ) )
        return false;

    // Change to this weapon
    self->r.client->ps.stats[STAT_PENDING_WEAPON] = weapon;
    self->ai->changeweapon_timeout = level.time + 2000 + ( 4000 * ( 1.0 - self->ai->pers.skillLevel ) );

    return true;
}

//==========================================
// BOT_DMclass_ChooseWeapon
// Choose weapon based on range & weights
//==========================================
float Bot::ChooseWeapon()
{
    float dist;
    int i;
    float best_weight = 0.0;
    gsitem_t *weaponItem;
    int curweapon, weapon_range = 0, best_weapon = WEAP_NONE;

    curweapon = self->r.client->ps.stats[STAT_PENDING_WEAPON];

    // if no enemy, then what are we doing here?
    if( !self->enemy )
    {
        weapon_range = AIWEAP_MEDIUM_RANGE;
        if( curweapon == WEAP_GUNBLADE || curweapon == WEAP_NONE )
            self->ai->changeweapon_timeout = level.time;
    }
    else // Base weapon selection on distance:
    {
        dist = DistanceFast( self->s.origin, self->enemy->s.origin );

        if( dist < 150 )
            weapon_range = AIWEAP_MELEE_RANGE;
        else if( dist < 500 )  // Medium range limit is Grenade launcher range
            weapon_range = AIWEAP_SHORT_RANGE;
        else if( dist < 900 )
            weapon_range = AIWEAP_MEDIUM_RANGE;
        else
            weapon_range = AIWEAP_LONG_RANGE;
    }

    if( self->ai->changeweapon_timeout > level.time )
        return AIWeapons[curweapon].RangeWeight[weapon_range];

    for( i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++ )
    {
        float rangeWeight;

        if( ( weaponItem = GS_FindItemByTag( i ) ) == NULL )
            continue;

        if( !GS_CheckAmmoInWeapon( &self->r.client->ps, i ) )
            continue;

        rangeWeight = AIWeapons[i].RangeWeight[weapon_range] * self->ai->pers.cha.weapon_affinity[i - ( WEAP_GUNBLADE - 1 )];

        // weigh up if having strong ammo
        if( self->r.client->ps.inventory[weaponItem->ammo_tag] )
            rangeWeight *= 1.25;

        // add a small random factor (less random the more skill)
        rangeWeight += brandom( -( 1.0 - self->ai->pers.skillLevel ), 1.0 - self->ai->pers.skillLevel );

        // compare range weights
        if( rangeWeight > best_weight )
        {
            best_weight = rangeWeight;
            best_weapon = i;
        }
    }

    // do the change (same weapon, or null best_weapon is covered at ChangeWeapon)
    if( best_weapon != WEAP_NONE )
        ChangeWeapon(best_weapon);

    return AIWeapons[curweapon].RangeWeight[weapon_range]; // return current
}

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
void Bot::PredictProjectileShot(vec3_t fire_origin, float projectile_speed, vec3_t target, vec3_t target_velocity)
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
    G_Trace( &trace, fire_origin, vec3_origin, vec3_origin, predictedTarget, self, MASK_SHOT );
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
    trace_t	trace;
    bool continuous_fire = false;
    firedef_t *firedef = GS_FiredefForPlayerState( &self->r.client->ps, self->r.client->ps.stats[STAT_WEAPON] );

    if( !self->enemy )
        return false;

    weapon = self->s.weapon;
    if( weapon < 0 || weapon >= WEAP_TOTAL )
        weapon = 0;

    if( !firedef )
        return false;

    // Aim to center of the box
    for( i = 0; i < 3; i++ )
        target[i] = self->enemy->s.origin[i] + ( 0.5f * ( self->enemy->r.maxs[i] + self->enemy->r.mins[i] ) );
    fire_origin[0] = self->s.origin[0];
    fire_origin[1] = self->s.origin[1];
    fire_origin[2] = self->s.origin[2] + self->viewheight;

    if( self->s.weapon == WEAP_LASERGUN || self->s.weapon == WEAP_PLASMAGUN )
        continuous_fire = true;

    if( !continuous_fire && !CheckShot(target) )
        return false;

    // find out our weapon AIM style
    if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION_EXPLOSIVE )
    {
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);

        wfac = WFAC_GENERIC_PROJECTILE * 1.3;

        // aim to the feet when enemy isn't higher
        if( fire_origin[2] > ( target[2] + ( self->enemy->r.mins[2] * 0.8 ) ) )
        {
            vec3_t checktarget;
            VectorSet( checktarget,
                       self->enemy->s.origin[0],
                       self->enemy->s.origin[1],
                       self->enemy->s.origin[2] + self->enemy->r.mins[2] + 4 );

            G_Trace( &trace, fire_origin, vec3_origin, vec3_origin, checktarget, self, MASK_SHOT );
            if( trace.fraction == 1.0f || ( trace.ent > 0 && game.edicts[trace.ent].takedamage ) )
                VectorCopy( checktarget, target );
        }
        else if( !IsStep( self->enemy ) )
            wfac *= 2.5; // more imprecise for air rockets
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_PREDICTION )
    {
        if( self->s.weapon == WEAP_PLASMAGUN )
            wfac = WFAC_GENERIC_PROJECTILE * 0.5;
        else
            wfac = WFAC_GENERIC_PROJECTILE;

        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity);
    }
    else if( AIWeapons[weapon].aimType == AI_AIMSTYLE_DROP )
    {
        //jalToDo
        wfac = WFAC_GENERIC_PROJECTILE;
        // in the lowest skill level, don't predict projectiles
        if( self->ai->pers.skillLevel >= 0.33f )
            PredictProjectileShot(fire_origin, firedef->speed, target, self->enemy->velocity );
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
            if( G_InFront( self, self->enemy ) ) {
                ucmd->buttons |= BUTTON_ATTACK; // could fire, but wants to?
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
        G_PrintChasersf( self, "%s: attacking %s\n", self->ai->pers.netname, self->enemy->r.client ? self->enemy->r.client->netname : self->classname );

    return true;
}

float Bot::PlayerWeight(edict_t *enemy)
{
    bool rage_mode = false;

    if( !enemy || enemy == self )
        return 0;

    if( G_ISGHOSTING( enemy ) || enemy->flags & (FL_NOTARGET|FL_BUSY) )
        return 0;

    if( self->r.client->ps.inventory[POWERUP_QUAD] || self->r.client->ps.inventory[POWERUP_SHELL] )
        rage_mode = true;

    // don't fight against powerups.
    if( enemy->r.client && ( enemy->r.client->ps.inventory[POWERUP_QUAD] || enemy->r.client->ps.inventory[POWERUP_SHELL] ) )
        return 0.2;

    //if not team based give some weight to every one
    if( GS_TeamBasedGametype() && ( enemy->s.team == self->s.team ) )
        return 0;

    // if having EF_CARRIER we can assume it's someone important
    if( enemy->s.effects & EF_CARRIER )
        return 2.0f;

    if( enemy == self->ai->last_attacker )
        return rage_mode ? 4.0f : 1.0f;

    return rage_mode ? 4.0f : 0.3f;
}

//==========================================
// BOT_DMclass_UpdateStatus
// update ai.status values based on bot state,
// so ai can decide based on these settings
//==========================================
void Bot::UpdateStatus()
{
    float LowNeedFactor = 0.5;
    gclient_t *client;
    int i;
    bool onlyGotGB = true;
    edict_t *ent;
    ai_handle_t *ai;
    nav_ents_t *goalEnt;

    client = self->r.client;

    ai = self->ai;

    FOREACH_GOALENT( goalEnt )
    {
        i = goalEnt->id;
        ent = goalEnt->ent;

        // item timing disabled by now
        if( ent->r.solid == SOLID_NOT )
        {
            ai->status.entityWeights[i] = 0;
            continue;
        }

        if( ent->r.client )
        {
            ai->status.entityWeights[i] = PlayerWeight( ent ) * self->ai->pers.cha.offensiveness;
            continue;
        }

        if( ent->item )
        {
            if( ent->r.solid == SOLID_NOT )
            {
                ai->status.entityWeights[i] = 0;
                continue;
            }

            if( ent->item->type & IT_WEAPON )
            {
                if( client->ps.inventory[ent->item->tag] )
                {
                    if( client->ps.inventory[ent->item->ammo_tag] )
                    {
                        // find ammo item for this weapon
                        gsitem_t *ammoItem = GS_FindItemByTag( ent->item->ammo_tag );
                        if( ammoItem->inventory_max )
                        {
                            ai->status.entityWeights[i] *= (0.5 + 0.5 * (1.0 - (float)client->ps.inventory[ent->item->ammo_tag] / ammoItem->inventory_max));
                        }
                        ai->status.entityWeights[i] *= LowNeedFactor;
                    }
                    else
                    {
                        // we need some ammo
                        ai->status.entityWeights[i] *= LowNeedFactor;
                    }
                    onlyGotGB = false;
                }
            }
            else if( ent->item->type & IT_AMMO )
            {
                if( client->ps.inventory[ent->item->tag] >= ent->item->inventory_max )
                {
                    ai->status.entityWeights[i] = 0.0;
                }
                else
                {
#if 0
                    // find weapon item for this ammo
					gsitem_t *weaponItem;
					int weapon;

					for( weapon = WEAP_GUNBLADE; weapon < WEAP_TOTAL; weapon++ )
					{
						weaponItem = GS_FindItemByTag( weapon );
						if( weaponItem->ammo_tag == ent->item->tag )
						{
							if( !client->ps.inventory[weaponItem->tag] )
								self->ai->status.entityWeights[i] *= LowNeedFactor;
						}
					}
#endif
                }
            }
            else if( ent->item->type & IT_ARMOR )
            {
                if ( self->r.client->resp.armor < ent->item->inventory_max || !ent->item->inventory_max )
                {
                    if( ent->item->inventory_max )
                    {
                        if( ( (float)self->r.client->resp.armor / (float)ent->item->inventory_max ) > 0.75 )
                            ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag] * LowNeedFactor;
                    }
                    else
                        ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
                }
                else
                {
                    ai->status.entityWeights[i] = 0;
                }
            }
            else if( ent->item->type & IT_HEALTH )
            {
                if( ent->item->tag == HEALTH_MEGA || ent->item->tag == HEALTH_ULTRA || ent->item->tag == HEALTH_SMALL )
                    ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
                else
                {
                    if( self->health >= self->max_health )
                        ai->status.entityWeights[i] = 0;
                    else
                    {
                        float health_func;

                        health_func = self->health / self->max_health;
                        health_func *= health_func;

                        ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag] + ( 1.1f - health_func );
                    }
                }
            }
            else if( ent->item->type & IT_POWERUP )
            {
                ai->status.entityWeights[i] = self->ai->pers.inventoryWeights[ent->item->tag];
            }
        }
    }

    if( onlyGotGB )
    {
        FOREACH_GOALENT( goalEnt )
        {
            i = goalEnt->id;
            ent = goalEnt->ent;

            if( ent->item && ent->item->type & IT_WEAPON )
                self->ai->status.entityWeights[i] *= 2.0f;
        }
    }
}

//==========================================
// BOT_DMclass_VSAYmessages
//==========================================
void Bot::SayVoiceMessages()
{
    if( GS_MatchState() != MATCH_STATE_PLAYTIME )
        return;
    if( level.gametype.dummyBots || bot_dummy->integer )
        return;

    if( self->snap.damageteam_given > 25 )
    {
        if( rand() & 1 )
        {
            if( rand() & 1 )
            {
                G_BOTvsay_f( self, "oops", true );
            }
            else
            {
                G_BOTvsay_f( self, "sorry", true );
            }
        }
        return;
    }

    if( self->ai->vsay_timeout > level.time )
        return;

    if( GS_MatchDuration() && game.serverTime + 4000 > GS_MatchEndTime() )
    {
        self->ai->vsay_timeout = game.serverTime + ( 1000 + (GS_MatchEndTime() - game.serverTime) );
        if( rand() & 1 )
            G_BOTvsay_f( self, "goodgame", false );
        return;
    }

    self->ai->vsay_timeout = level.time + ( ( 8+random()*12 ) * 1000 );

    // the more bots, the less vsays to play
    if( random() > 0.1 + 1.0f / game.numBots )
        return;

    if( GS_TeamBasedGametype() && !GS_InvidualGameType() )
    {
        if( self->health < 20 && random() > 0.3 )
        {
            G_BOTvsay_f( self, "needhealth", true );
            return;
        }

        if( ( self->s.weapon == 0 || self->s.weapon == 1 ) && random() > 0.7 )
        {
            G_BOTvsay_f( self, "needweapon", true );
            return;
        }

        if( self->r.client->resp.armor < 10 && random() > 0.8 )
        {
            G_BOTvsay_f( self, "needarmor", true );
            return;
        }
    }

    // NOT team based here

    if( random() > 0.2 )
        return;

    switch( (int)brandom( 1, 8 ) )
    {
        default:
            break;
        case 1:
            G_BOTvsay_f( self, "roger", false );
            break;
        case 2:
            G_BOTvsay_f( self, "noproblem", false );
            break;
        case 3:
            G_BOTvsay_f( self, "yeehaa", false );
            break;
        case 4:
            G_BOTvsay_f( self, "yes", false );
            break;
        case 5:
            G_BOTvsay_f( self, "no", false );
            break;
        case 6:
            G_BOTvsay_f( self, "booo", false );
            break;
        case 7:
            G_BOTvsay_f( self, "attack", false );
            break;
        case 8:
            G_BOTvsay_f( self, "ok", false );
            break;
    }
}


//==========================================
// BOT_DMClass_BlockedTimeout
// the bot has been blocked for too long
//==========================================
void Bot::BlockedTimeout()
{
    if( level.gametype.dummyBots || bot_dummy->integer ) {
        self->ai->blocked_timeout = level.time + 15000;
        return;
    }
    self->health = 0;
    self->ai->blocked_timeout = level.time + 15000;
    self->die( self, self, self, 100000, vec3_origin );
    G_Killed( self, self, self, 999, vec3_origin, MOD_SUICIDE );
    self->nextThink = level.time + 1;
}

//==========================================
// BOT_DMclass_DeadFrame
// ent is dead = run this think func
//==========================================
void Bot::GhostingFrame()
{
    usercmd_t ucmd;

    ClearGoal();

    self->ai->blocked_timeout = level.time + 15000;
    self->nextThink = level.time + 100;

    // wait 4 seconds after entering the level
    if( self->r.client->level.timeStamp + 4000 > level.time || !level.canSpawnEntities )
        return;

    if( self->r.client->team == TEAM_SPECTATOR )
    {
        // try to join a team
        // note that G_Teams_JoinAnyTeam is quite slow so only call it per frame
        if( !self->r.client->queueTimeStamp && self == level.think_client_entity )
            G_Teams_JoinAnyTeam( self, false );

        if( self->r.client->team == TEAM_SPECTATOR ) // couldn't join, delay the next think
            self->nextThink = level.time + 2000 + (int)( 4000 * random() );
        else
            self->nextThink = level.time + 1;
        return;
    }

    memset( &ucmd, 0, sizeof( ucmd ) );

    // set approximate ping and show values
    ucmd.serverTimeStamp = game.serverTime;
    ucmd.msec = game.frametime;
    self->r.client->r.ping = 0;

    // ask for respawn if the minimum bot respawning time passed
    if( level.time > self->deathTimeStamp + 3000 )
        ucmd.buttons = BUTTON_ATTACK;

    ClientThink( self, &ucmd, 0 );
}


//==========================================
// BOT_DMclass_RunFrame
// States Machine & call client movement
//==========================================
void Bot::RunFrame()
{
    usercmd_t ucmd;
    float weapon_quality;
    bool inhibitCombat = false;
    int i;

    if( G_ISGHOSTING( self ) )
    {
        GhostingFrame();
        return;
    }

    memset( &ucmd, 0, sizeof( ucmd ) );

    //get ready if in the game
    if( GS_MatchState() <= MATCH_STATE_WARMUP && !level.ready[PLAYERNUM(self)]
        && self->r.client->teamstate.timeStamp + 4000 < level.time )
        G_Match_Ready( self );

    if( level.gametype.dummyBots || bot_dummy->integer )
    {
        self->r.client->level.last_activity = level.time;
    }
    else
    {
        FindEnemy();

        weapon_quality = ChooseWeapon();

        inhibitCombat = ( CurrentLinkType() & (LINK_JUMPPAD|LINK_JUMP|LINK_ROCKETJUMP) ) != 0 ? true : false;

        if( self->enemy && weapon_quality >= 0.3 && !inhibitCombat ) // don't fight with bad weapons
        {
            if( FireWeapon( &ucmd ) )
                self->ai->state_combat_timeout = level.time + AI_COMBATMOVE_TIMEOUT;
        }

        if( inhibitCombat )
            self->ai->state_combat_timeout = 0;

        if( self->ai->state_combat_timeout > level.time )
        {
            CombatMovement( &ucmd );
        }
        else
        {
            Move( &ucmd );
        }

        //set up for pmove
        for( i = 0; i < 3; i++ )
            ucmd.angles[i] = ANGLE2SHORT( self->s.angles[i] ) - self->r.client->ps.pmove.delta_angles[i];

        VectorSet( self->r.client->ps.pmove.delta_angles, 0, 0, 0 );
    }

    // set approximate ping and show values
    ucmd.msec = game.frametime;
    ucmd.serverTimeStamp = game.serverTime;

    ClientThink( self, &ucmd, 0 );
    self->nextThink = level.time + 1;

    SayVoiceMessages();
}


