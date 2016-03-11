#include "bot.h"
#include <algorithm>

void Bot::LookAround()
{
    TestClosePlace();

    RegisterVisibleEnemies();

    enemyPool.UpdateCombatTask();

    if (enemyPool.combatTask.aimEnemy)
        ChangeWeapon(enemyPool.combatTask.suggestedShootWeapon);
    else if (enemyPool.combatTask.spamEnemy)
        ChangeWeapon(enemyPool.combatTask.suggestedSpamWeapon);

    // Try to keep compatibility with other code, especially scripts
    if (enemyPool.combatTask.aimEnemy)
        self->enemy = const_cast<edict_t*>(enemyPool.combatTask.aimEnemy->ent);
    else
        self->enemy = nullptr;
}

void Bot::RegisterVisibleEnemies()
{
    nav_ents_t *goalEnt;

    if(G_ISGHOSTING(self) || GS_MatchState() == MATCH_STATE_COUNTDOWN || GS_ShootingDisabled())
    {
        self->ai->enemyReactionDelay = 0;
        self->enemy = self->ai->latched_enemy = NULL;
        return;
    }

    FOREACH_GOALENT( goalEnt )
    {
        int i = goalEnt->id;

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

        if( trap_inPVS( self->s.origin, goalEnt->ent->s.origin ) && G_Visible( self, goalEnt->ent ) )
        {
            enemyPool.OnEnemyViewed( goalEnt->ent );
        }
    }
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
            ai->status.entityWeights[i] = enemyPool.PlayerAiWeight( ent );
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
    bool inhibitCombat = false;
    int i;

    enemyPool.PrepareToFrame();

    if( G_ISGHOSTING( self ) )
    {
        enemyPool.combatTask.Reset();
        enemyPool.combatTask.prevSpamEnemy = nullptr;

        GhostingFrame();

        enemyPool.FinishFrame();
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
        LookAround();

        const CombatTask &combatTask = enemyPool.combatTask;

        inhibitCombat = ( CurrentLinkType() & (LINK_JUMPPAD|LINK_JUMP|LINK_ROCKETJUMP) ) != 0;

        if( (combatTask.aimEnemy || combatTask.spamEnemy) && !inhibitCombat )
        {
            if( FireWeapon( &ucmd ) )
            {
                if (!combatTask.spamEnemy)
                    self->ai->state_combat_timeout = level.time + AI_COMBATMOVE_TIMEOUT;
            }
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

    enemyPool.FinishFrame();
}


