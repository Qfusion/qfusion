#include "bot.h"
#include "aas.h"
#include <algorithm>

Bot::Bot(edict_t *self, float skillLevel)
    : Ai(self, PREFERRED_TRAVEL_FLAGS, ALLOWED_TRAVEL_FLAGS),
      dangersDetector(self),
      botBrain(self, skillLevel),
      skillLevel(skillLevel),
      printLink(false),
      hasTouchedJumppad(false),
      hasEnteredJumppad(false),
      jumppadTarget(INFINITY, INFINITY, INFINITY),
      jumppadMoveTimeout(0),
      jumppadLandingAreasCount(0),
      hasPendingLandingDash(false),
      isOnGroundThisFrame(false),
      wasOnGroundPrevFrame(false),
      pendingLandingDashTimeout(0),
      requestedViewTurnSpeedMultiplier(1.0f),
      combatMovePushTimeout(0),
      vsayTimeout(level.time + 10000),
      pendingLookAtPoint(0, 0, 0),
      pendingLookAtPointTimeoutAt(0),
      hasPendingLookAtPoint(false),
      lookAtPointTurnSpeedMultiplier(0.5f),
      cachedPredictedTargetOrigin(INFINITY, INFINITY, INFINITY),
      cachedPredictedTargetValidUntil(0),
      cachedPredictedTargetInstanceId(0),
      hasCampingSpot(false),
      hasCampingLookAtPoint(false),
      campingSpotRadius(INFINITY),
      campingAlertness(INFINITY),
      campingSpotOrigin(INFINITY, INFINITY, INFINITY),
      campingSpotLookAtPoint(INFINITY, INFINITY, INFINITY),
      campingSpotStrafeDir(INFINITY, INFINITY, INFINITY),
      campingSpotStrafeTimeout(0),
      campingSpotLookAtPointTimeout(0),
      isWaitingForItemSpawn(false)
{
    // Set the base brain reference in Ai class, it is mandatory
    this->aiBaseBrain = &botBrain;
    self->r.client->movestyle = Skill() > 0.33f ? GS_NEWBUNNY : GS_CLASSICBUNNY;
}

void Bot::LookAround()
{
    CheckIsInThinkFrame(__FUNCTION__);

    TestClosePlace();

    RegisterVisibleEnemies();

    if (!botBrain.combatTask.Empty())
        ChangeWeapon(botBrain.combatTask.Weapon());
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

void Bot::SetCampingSpot(const Vec3 &spotOrigin, float spotRadius, float alertness)
{
    hasCampingSpot = true;
    hasCampingLookAtPoint = false;
    campingSpotRadius = spotRadius;
    campingAlertness = alertness;
    campingSpotOrigin = spotOrigin;
    campingSpotStrafeTimeout = 0;
    // Select some random look-at-point on first call of MoveCampingASpot()
    campingSpotLookAtPointTimeout = 0;
}

void Bot::SetCampingSpot(const Vec3 &spotOrigin, const Vec3 &lookAtPoint, float spotRaduis, float alertness)
{
    hasCampingSpot = true;
    hasCampingLookAtPoint = true;
    campingSpotRadius = spotRaduis;
    campingAlertness = alertness;
    campingSpotOrigin = spotOrigin;
    campingSpotLookAtPoint = lookAtPoint;
    campingSpotStrafeTimeout = 0;
}

void Bot::ClearCampingSpot()
{
    hasCampingSpot = false;
    hasCampingLookAtPoint = false;
    campingSpotRadius = INFINITY;
    campingAlertness = INFINITY;
    campingSpotOrigin = Vec3(INFINITY, INFINITY, INFINITY);
    campingSpotLookAtPoint = Vec3(INFINITY, INFINITY, INFINITY);
}

void Bot::TouchedGoal(const edict_t *goalUnderlyingEntity)
{
    // Stop camping a spawn point if the bot did it
    if (isWaitingForItemSpawn)
    {
        ClearCampingSpot();
        isWaitingForItemSpawn = false;
    }
}

void Bot::TouchedJumppad(const edict_t *jumppad)
{
    // jumppad->s.origin2 contains initial push velocity
    Vec3 pushDir(jumppad->s.origin2);
    pushDir.NormalizeFast();

    float relaxedFlightSeconds = 0;
    float zDotFactor = pushDir.Dot(&axis_identity[AXIS_UP]);
    if (zDotFactor > 0)
    {
        // Start to find landing area when vertical velocity is close to zero.
        // This may be wrong for Q3-like horizontal jumppads,
        // but its unlikely to see this kind of triggers in the QF game.
        relaxedFlightSeconds = 0.90f * jumppad->s.origin2[2] / (level.gravity + 0.0001f);
    }
    // Otherwise (for some weird jumppad that pushes dow) start to find landing area immediately

    jumppadMoveTimeout = level.time + (unsigned)(1000.0f * relaxedFlightSeconds);
    hasTouchedJumppad = true;
    jumppadTarget = Vec3(jumppad->target_ent->s.origin);
}

void Bot::RegisterVisibleEnemies()
{
    if(G_ISGHOSTING(self) || GS_MatchState() == MATCH_STATE_COUNTDOWN || GS_ShootingDisabled())
        return;

    CheckIsInThinkFrame(__FUNCTION__);

    // Compute look dir before loop
    vec3_t lookDir;
    AngleVectors(self->s.angles, lookDir, nullptr, nullptr);

    float fov = 110.0f + 69.0f * Skill();
    float dotFactor = cosf((float)DEG2RAD(fov / 2));

    struct EntAndDistance
    {
        int entNum;
        float distance;

        EntAndDistance(int entNum, float distance): entNum(entNum), distance(distance) {}
        bool operator<(const EntAndDistance &that) const { return distance < that.distance; }
    };

    // Do not call inPVS() and G_Visible() for potential targets inside a loop for all clients.
    // In worst case when all bots may see each other we get N^2 traces and PVS tests
    // First, select all candidate targets along with distance to a bot.
    // Then choose not more than BotBrain::maxTrackedEnemies nearest enemies for calling OnEnemyViewed()
    // It may cause data loss (far enemies may have higher logical priority),
    // but in a common good case (when there are few visible enemies) it preserves data,
    // and in the worst case mentioned above it does not act weird from player POV and prevents server hang up.
    StaticVector<EntAndDistance, MAX_CLIENTS> candidateTargets;

    // Atm clients cannot be goal entities, so instead of iterating all goal ents we iterate just over all clients
    for (int i = 0; i < gs.maxclients; ++i)
    {
        edict_t *ent = PLAYERENT(i);
        if (!ent->r.inuse || !ent->r.client)
            continue;
        if (G_ISGHOSTING(ent))
            continue;
        if (ent->flags & (FL_NOTARGET|FL_BUSY))
            continue;
        if (GS_TeamBasedGametype() && ent->s.team == self->s.team)
            continue;
        if (ent == self)
            continue;

        // Reject targets quickly by fov
        Vec3 toTarget(ent->s.origin);
        toTarget -= self->s.origin;
        float squareDistance = toTarget.SquaredLength();
        if (squareDistance < 1)
            continue;
        float invDistance = Q_RSqrt(squareDistance);
        toTarget *= invDistance;
        if (toTarget.Dot(lookDir) < dotFactor)
            continue;

        // It seams to be more instruction cache-friendly to just add an entity to a plain array
        // and sort it once after the loop instead of pushing an entity in a heap on each iteration
        candidateTargets.emplace_back(EntAndDistance(ENTNUM(ent), 1.0f / invDistance));
    }

    std::sort(candidateTargets.begin(), candidateTargets.end());

    for (int i = 0, end = std::min(candidateTargets.size(), botBrain.maxTrackedEnemies); i < end; ++i)
    {
        edict_t *ent = game.edicts + candidateTargets[i].entNum;
        if (trap_inPVS(self->s.origin, ent->s.origin) && G_Visible(self, ent))
            botBrain.OnEnemyViewed(ent);
    }

    botBrain.AfterAllEnemiesViewed();
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

//==========================================
// BOT_DMclass_VSAYmessages
//==========================================
void Bot::SayVoiceMessages()
{
    if( GS_MatchState() != MATCH_STATE_PLAYTIME )
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
void Bot::OnBlockedTimeout()
{
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

    Ai::ClearAllGoals();

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
    // Ai status will be updated implicitly (since a bot stopped ghosting)
    combatMovePushTimeout = 0;
    ResetNavigation();
}

void Bot::Think()
{
    // Call superclass method first
    Ai::Think();

    if (IsGhosting())
        return;

    LookAround();
}

//==========================================
// BOT_DMclass_RunFrame
// States Machine & call client movement
//==========================================
void Bot::Frame()
{
    // Call superclass method first
    Ai::Frame();

    if (IsGhosting())
    {
        botBrain.combatTask.Clear();

        GhostingFrame();

        return;
    }

    usercmd_t ucmd;
    memset(&ucmd, 0, sizeof(ucmd));

    //get ready if in the game
    if(GS_MatchState() <= MATCH_STATE_WARMUP && !IsReady() && self->r.client->teamstate.timeStamp + 4000 < level.time)
        G_Match_Ready(self);

    ApplyPendingTurnToLookAtPoint();

    const CombatTask &combatTask = botBrain.combatTask;

    bool inhibitShooting = combatTask.Empty();
    bool inhibitCombatMove = inhibitShooting || combatTask.inhibit;
    if (!inhibitCombatMove)
    {
        if (currAasAreaNum != goalAasAreaNum && !nextReaches.empty())
        {
            if (IsCloseToReachStart())
            {
                int travelType = nextReaches.front().traveltype;
                if (travelType == TRAVEL_ROCKETJUMP || travelType == TRAVEL_JUMPPAD)
                    inhibitCombatMove = true;
                else if (travelType == TRAVEL_CROUCH)
                    inhibitCombatMove = true;
                else if (travelType == TRAVEL_LADDER)
                    inhibitCombatMove = inhibitShooting = true;
            }
            else if (currAasAreaTravelFlags & (TFL_CROUCH))
                inhibitCombatMove = true;
        }
        // Try to move bunnying instead of dodging on ground
        // if the enemy is not looking to bot being able to hit him
        // and the bot is able to hit while moving without changing angle significantly
        if (!inhibitCombatMove && !combatTask.IsTargetStatic())
        {
            Vec3 enemyLookDir = combatTask.EnemyLookDir();
            Vec3 enemyToBotDir = Vec3(self->s.origin) - combatTask.EnemyOrigin();
            float squaredDistance = enemyToBotDir.SquaredLength();
            // If the enemy is far enough
            if (squaredDistance > 200.0f * 200.0f)
            {
                enemyToBotDir *= Q_RSqrt(squaredDistance);
                // Check whether the enemy may not hit the bot
                if (enemyLookDir.Dot(enemyToBotDir) < 0.9)
                {
                    vec3_t botLookDir;
                    AngleVectors(self->s.angles, botLookDir, nullptr, nullptr);
                    // Check whether the bot may hit while running
                    if ((-enemyToBotDir).Dot(botLookDir) > 0.99)
                        inhibitCombatMove = true;
                }
            }
        }
    }

    // Do not modify the ucmd in FireWeapon(), it will be overwritten by MoveFrame()
    bool fireButtonPressed = false;
    if (!inhibitShooting)
    {
        if (FireWeapon())
        {
            fireButtonPressed = true;
        }
    }

    MoveFrame(&ucmd, inhibitCombatMove);

    if (fireButtonPressed)
        ucmd.buttons |= BUTTON_ATTACK;

    //set up for pmove
    for (int i = 0; i < 3; i++)
        ucmd.angles[i] = ANGLE2SHORT(self->s.angles[i]) - self->r.client->ps.pmove.delta_angles[i];

    VectorSet(self->r.client->ps.pmove.delta_angles, 0, 0, 0);

    // set approximate ping and show values
    ucmd.msec = game.frametime;
    ucmd.serverTimeStamp = game.serverTime;

    // If this value is modified by ClientThink() callbacks, it will be kept until next frame reaches this line
    hasTouchedJumppad = false;

    ClientThink( self, &ucmd, 0 );
    self->nextThink = level.time + 1;

    SayVoiceMessages();
}


