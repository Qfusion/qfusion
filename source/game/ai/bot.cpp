#include "bot.h"
#include "aas.h"
#include <algorithm>

Bot::Bot(edict_t *self)
    : Ai(self),
      dangersDetector(self),
      enemyPool(self),
      printLink(false),
      isBunnyHopping(false),
      hasTriggeredRj(false),
      rjTimeout(0),
      hasTriggeredJumppad(false),
      jumppadMoveTimeout(0),
      jumppadDestAreaNum(0),
      jumppadReachEndPoint(INFINITY, INFINITY, INFINITY),
      combatMovePushTimeout(0),
      vsayTimeout(level.time + 10000),
      pendingLookAtPoint(0, 0, 0),
      pendingLookAtPointTimeoutAt(0),
      hasPendingLookAtPoint(false),
      lookAtPointTurnSpeedMultiplier(0.5f)
{
    self->r.client->movestyle = GS_NEWBUNNY;
}

void Bot::LookAround()
{
    ApplyPendingTurnToLookAtPoint();

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

void Bot::SetPendingLookAtPoint(const Vec3 &point, float turnSpeedMultiplier, unsigned int timeoutDuration)
{
    pendingLookAtPoint = point;
    pendingLookAtPointTimeoutAt = level.time + timeoutDuration;
    hasPendingLookAtPoint = true;
    lookAtPointTurnSpeedMultiplier = turnSpeedMultiplier;
}

void Bot::ApplyPendingTurnToLookAtPoint()
{
    if (!hasPendingLookAtPoint)
        return;

    Vec3 toPointDir(pendingLookAtPoint);
    toPointDir -= self->s.origin;
    toPointDir.NormalizeFast();

    ChangeAngle(toPointDir, lookAtPointTurnSpeedMultiplier);

    if (pendingLookAtPointTimeoutAt <= level.time)
        hasPendingLookAtPoint = false;
}

void Bot::RegisterVisibleEnemies()
{
    if(G_ISGHOSTING(self) || GS_MatchState() == MATCH_STATE_COUNTDOWN || GS_ShootingDisabled())
        return;


    /*
    FOREACH_GOALENT(goalEnt)
    {
        int i = goalEnt->id;

        if( !goalEnt->ent || !goalEnt->ent->r.inuse )
            continue;

        if(!goalEnt->ent->r.client) // this may be changed, there could be enemies which aren't clients
            continue;

        if (G_ISGHOSTING(goalEnt->ent))
            continue;

        if (self->ai->status.entityWeights[i] <= 0 || goalEnt->ent->flags & (FL_NOTARGET|FL_BUSY) )
            continue;

        if (GS_TeamBasedGametype() && goalEnt->ent->s.team == self->s.team)
            continue;

        if (trap_inPVS(self->s.origin, goalEnt->ent->s.origin) && G_Visible(self, goalEnt->ent))
        {
            enemyPool.OnEnemyViewed(goalEnt->ent);
        }
    }*/

    // Atm clients cannot be goal entities, so instead of iterating all goal ents we iterate just over all clients
    /*
    for (int i = 0; i < gs.maxclients; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (!ent->r.inuse || !ent->r.client)
            continue;
        if (G_ISGHOSTING(ent))
            continue;
        if (ent->flags & (FL_NOTARGET|FL_BUSY))
            continue;
        if (GS_TeamBasedGametype() && ent->s.team == self->s.team)
            continue;

        if (trap_inPVS(self->s.origin, ent->s.origin) && G_Visible(self, ent))
            enemyPool.OnEnemyViewed(ent);
    }*/

    enemyPool.AfterAllEnemiesViewed();
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

    return true;
}

float Bot::ComputeItemWeight(const edict_t *ent, bool onlyGotGB) const
{
    switch (ent->item->type)
    {
        case IT_WEAPON: return ComputeWeaponWeight(ent, onlyGotGB);
        case IT_AMMO: return ComputeAmmoWeight(ent);
        case IT_HEALTH: return ComputeHealthWeight(ent);
        case IT_ARMOR: return ComputeArmorWeight(ent);
        case IT_POWERUP: return ComputePowerupWeight(ent);
    }
    return 0;
}

float Bot::ComputeWeaponWeight(const edict_t *ent, bool onlyGotGB) const
{
    if (Inventory()[ent->item->tag])
    {
        // TODO: Precache
        const gsitem_t *ammo = GS_FindItemByTag(ent->item->ammo_tag);
        if (Inventory()[ammo->tag] >= ammo->inventory_max)
            return 0;

        float ammoQuantityFactor = 1.0f - Inventory()[ammo->tag] / (float)ammo->inventory_max;

        switch (ent->item->tag)
        {
            case WEAP_ELECTROBOLT: return ammoQuantityFactor;
            case WEAP_LASERGUN: return ammoQuantityFactor * 1.1f;
            case WEAP_PLASMAGUN: return ammoQuantityFactor * 1.1f;
            case WEAP_ROCKETLAUNCHER: return ammoQuantityFactor;
            default: return 0.5f * ammoQuantityFactor;
        }
    }

    // We may consider plasmagun in a bot's hand as a top tier weapon too
    const int topTierWeapons[4] = { WEAP_ELECTROBOLT, WEAP_LASERGUN, WEAP_ROCKETLAUNCHER, WEAP_PLASMAGUN };

    // TODO: Precompute
    float topTierWeaponGreed = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        if (!Inventory()[topTierWeapons[i]])
            topTierWeaponGreed += 1.0f;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (topTierWeapons[i] == ent->item->tag)
            return (onlyGotGB ? 1.5f : 0.9f) + (topTierWeaponGreed - 1.0f) / 3.0f;
    }

    return onlyGotGB ? 1.5f : 0.7f;
}

float Bot::ComputeAmmoWeight(const edict_t *ent) const
{
    if (Inventory()[ent->item->tag] < ent->item->inventory_max)
    {
        float quantityFactor = 1.0f - Inventory()[ent->item->tag] / (float)ent->item->inventory_max;

        for (int weapon = WEAP_GUNBLADE; weapon < WEAP_TOTAL; weapon++)
        {
            // TODO: Preache
            const gsitem_t *weaponItem = GS_FindItemByTag( weapon );
            if (weaponItem->ammo_tag == ent->item->tag)
            {
                if (Inventory()[weaponItem->tag])
                {
                    switch (weaponItem->tag)
                    {
                        case WEAP_ELECTROBOLT: return quantityFactor;
                        case WEAP_LASERGUN: return quantityFactor * 1.1f;
                        case WEAP_PLASMAGUN: return quantityFactor * 1.1f;
                        case WEAP_ROCKETLAUNCHER: return quantityFactor;
                        default: return 0.5f * quantityFactor;
                    }
                }
                return quantityFactor * 0.33f;
            }
        }
    }
    return 0.0;
}

float Bot::ComputeHealthWeight(const edict_t *ent) const
{
    if (ent->item->tag == HEALTH_MEGA || ent->item->tag == HEALTH_ULTRA)
        return 2.5f;

    if (ent->item->tag == HEALTH_SMALL)
        return 0.2f + 0.3f * (1.0f - self->health / (float)self->max_health);

    return std::max(0.0f, 1.0f - self->health / (float)self->max_health);
}

float Bot::ComputeArmorWeight(const edict_t *ent) const
{
    float currArmor = self->r.client->resp.armor;
    switch (ent->item->tag)
    {
        case ARMOR_RA:
            return currArmor < 150.0f ? 2.0f : 0.0f;
        case ARMOR_YA:
            return currArmor < 125.0f ? 1.7f : 0.0f;
        case ARMOR_GA:
            return currArmor < 100.0f ? 1.4f : 0.0f;
        case ARMOR_SHARD:
        {
            if (currArmor < 25 || currArmor >= 150)
                return 0.4f;
            return 0.25f;
        }
    }
    return 0;
}

float Bot::ComputePowerupWeight(const edict_t *ent) const
{
    // TODO: Make it dependent of current health/armor status;
    return 3.5f;
}

//==========================================
// BOT_DMclass_UpdateStatus
// update ai.status values based on bot state,
// so ai can decide based on these settings
//==========================================
void Bot::UpdateStatus()
{
    // Compute it once, not on each loop step
    bool onlyGotGB = true;
    for (int weapon = WEAP_GUNBLADE + 1; weapon < WEAP_TOTAL; ++weapon)
    {
        if (Inventory()[weapon])
        {
            onlyGotGB = false;
            break;
        }
    }

    FOREACH_GOALENT(goalEnt)
    {
        self->ai->status.entityWeights[goalEnt->id] = 0;

        // item timing disabled by now
        if (goalEnt->ent->r.solid == SOLID_NOT)
            continue;

        // Picking clients as goal entities is currently disabled
        if (goalEnt->ent->r.client)
            continue;

        if (goalEnt->ent->item)
            self->ai->status.entityWeights[goalEnt->id] = ComputeItemWeight(goalEnt->ent, onlyGotGB);
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

    if (vsayTimeout > level.time)
        return;

    if( GS_MatchDuration() && game.serverTime + 4000 > GS_MatchEndTime() )
    {
        vsayTimeout = game.serverTime + ( 1000 + (GS_MatchEndTime() - game.serverTime) );
        if( rand() & 1 )
            G_BOTvsay_f( self, "goodgame", false );
        return;
    }

    vsayTimeout = level.time + ( ( 8+random()*12 ) * 1000 );

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
        blockedTimeout = level.time + BLOCKED_TIMEOUT;
        return;
    }
    self->health = 0;
    blockedTimeout = level.time + BLOCKED_TIMEOUT;
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

    if (HasGoal())
        ClearGoal();

    blockedTimeout = level.time + BLOCKED_TIMEOUT;
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

void Bot::OnRespawn()
{
    statusUpdateTimeout = 0;
    stateCombatTimeout = 0;
    combatMovePushTimeout = 0;
}

//==========================================
// BOT_DMclass_RunFrame
// States Machine & call client movement
//==========================================
void Bot::RunFrame()
{
    enemyPool.PrepareToFrame();

    if (G_ISGHOSTING(self))
    {
        enemyPool.combatTask.Reset();
        enemyPool.combatTask.prevSpamEnemy = nullptr;

        GhostingFrame();

        enemyPool.FinishFrame();
        return;
    }

    usercmd_t ucmd;
    memset(&ucmd, 0, sizeof(ucmd));

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

        bool inhibitCombat = false;
        if (currAasAreaNum != goalAasAreaNum && !nextReaches.empty())
        {
            if (IsCloseToReachStart())
            {
                int travelType = nextReaches.front().traveltype;
                if (travelType == TRAVEL_ROCKETJUMP || travelType == TRAVEL_JUMPPAD)
                    inhibitCombat = true;
                else if (travelType == TRAVEL_CROUCH || travelType == TRAVEL_LADDER)
                    inhibitCombat = true;
            }
            else if (currAasAreaTravelFlags & (TFL_CROUCH|TFL_AIR))
                inhibitCombat = true;
        }

        if ((combatTask.aimEnemy || combatTask.spamEnemy) && !inhibitCombat)
        {
            if (FireWeapon(&ucmd))
            {
                if (!combatTask.spamEnemy)
                    stateCombatTimeout = level.time + AI_COMBATMOVE_TIMEOUT;
            }
        }

        if (inhibitCombat)
            stateCombatTimeout = 0;

        if (stateCombatTimeout > level.time)
        {
            CombatMovement(&ucmd);
        }
        else
        {
            Move(&ucmd);
        }

        //set up for pmove
        for (int i = 0; i < 3; i++)
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


