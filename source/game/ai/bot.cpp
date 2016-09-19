#include "bot.h"
#include "ai_aas_world.h"
#include <algorithm>

Bot::Bot(edict_t *self, float skillLevel)
    : Ai(self, PREFERRED_TRAVEL_FLAGS, ALLOWED_TRAVEL_FLAGS),
      dangersDetector(self),
      botBrain(self, skillLevel),
      skillLevel(skillLevel),
      nextBlockedEscapeAttemptAt(0),
      blockedEscapeGoalOrigin(INFINITY, INFINITY, INFINITY),
      rocketJumpMovementState(self),
      combatMovePushTimeout(0),
      vsayTimeout(level.time + 10000),
      isWaitingForItemSpawn(false),
      isInSquad(false),
      defenceSpotId(-1),
      offenceSpotId(-1),
      builtinFireTargetCache(self),
      scriptFireTargetCache(self)
{
    // Set the base brain reference in Ai class, it is mandatory
    this->aiBaseBrain = &botBrain;
    // Set the route cache reference in Ai class, it is mandatory
    // Use a separate instance of a route cache
    this->routeCache = AiAasRouteCache::NewInstance();
    self->r.client->movestyle = Skill() > 0.33f ? GS_NEWBUNNY : GS_CLASSICBUNNY;
    SetTag(self->r.client->netname);
}

void Bot::LookAround()
{
    CheckIsInThinkFrame(__FUNCTION__);

    TestClosePlace();

    RegisterVisibleEnemies();

    if (!botBrain.combatTask.Empty())
        ChangeWeapon(botBrain.combatTask);
}

void Bot::ApplyPendingTurnToLookAtPoint()
{
    if (!pendingLookAtPointState.IsActive())
        return;

    Vec3 toPointDir(pendingLookAtPointState.lookAtPoint);
    toPointDir -= self->s.origin;
    toPointDir.NormalizeFast();

    ChangeAngle(toPointDir, pendingLookAtPointState.EffectiveTurnSpeedMultiplier(1.0f));
}

void Bot::TouchedGoal(const edict_t *goalUnderlyingEntity)
{
    if (botBrain.HandleGoalTouch(goalUnderlyingEntity))
    {
        // Stop camping a spawn point if the bot did it
        if (isWaitingForItemSpawn)
        {
            campingSpotState.Invalidate();
            isWaitingForItemSpawn = false;
        }
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

    jumppadMovementState.jumppadMoveTimeout = level.time + (unsigned)(1000.0f * relaxedFlightSeconds);
    jumppadMovementState.hasTouchedJumppad = true;
    jumppadMovementState.jumppadTarget = Vec3(jumppad->target_ent->s.origin);
}

void Bot::EnableAutoAlert(int id, const Vec3 &spotOrigin, float spotRadius, AlertCallback callback, void *receiver)
{
    // First check duplicate ids. Fail on error since callers of this method are internal.
    for (unsigned i = 0; i < alertSpots.size(); ++i)
    {
        if (alertSpots[i].id == id)
            FailWith("Duplicated alert spot (id=%d)\n", id);
    }

    if (alertSpots.size() == alertSpots.capacity())
        FailWith("Can't add an alert spot (id=%d)\n: too many spots", id);

    alertSpots.emplace_back(AlertSpot(spotOrigin, id, spotRadius, callback, receiver));
}

void Bot::DisableAutoAlert(int id)
{
    for (unsigned i = 0; i < alertSpots.size(); ++i)
    {
        if (alertSpots[i].id == id)
        {
            alertSpots.erase(alertSpots.begin() + i);
            return;
        }
    }

    FailWith("Can't find alert spot by id %d\n", id);
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
    // Note: non-client entities also may be candidate targets.
    StaticVector<EntAndDistance, MAX_EDICTS> candidateTargets;

    for (int i = 1; i < game.numentities; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (botBrain.MayNotBeFeasibleEnemy(ent))
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

    // Select inPVS/visible targets first to aid instruction cache, do not call callbacks in loop
    StaticVector<edict_t *, MAX_CLIENTS> targetsInPVS;
    StaticVector<edict_t *, MAX_CLIENTS> visibleTargets;

    static_assert(AiBaseEnemyPool::MAX_TRACKED_ENEMIES <= MAX_CLIENTS, "targetsInPVS capacity may be exceeded");

    for (int i = 0, end = std::min(candidateTargets.size(), botBrain.MaxTrackedEnemies()); i < end; ++i)
    {
        edict_t *ent = game.edicts + candidateTargets[i].entNum;
        if (trap_inPVS(self->s.origin, ent->s.origin))
            targetsInPVS.push_back(ent);
    }

    for (auto ent: targetsInPVS)
        if (G_Visible(self, ent))
            visibleTargets.push_back(ent);

    // Call bot brain callbacks on visible targets
    for (auto ent: visibleTargets)
        botBrain.OnEnemyViewed(ent);

    botBrain.AfterAllEnemiesViewed();

    CheckAlertSpots(visibleTargets);
}

void Bot::CheckAlertSpots(const StaticVector<edict_t *, MAX_CLIENTS> &visibleTargets)
{
    float scores[MAX_ALERT_SPOTS];

    // First compute scores (good for instruction cache)
    for (unsigned i = 0; i < alertSpots.size(); ++i)
    {
        float score = 0.0f;
        const auto &alertSpot = alertSpots[i];
        const float squareRadius = alertSpot.radius * alertSpot.radius;
        const float invRadius = 1.0f / alertSpot.radius;
        for (const edict_t *ent: visibleTargets)
        {
            float squareDistance = DistanceSquared(ent->s.origin, alertSpot.origin.Data());
            if (squareDistance > squareRadius)
                continue;
            float distance = Q_RSqrt(squareDistance + 0.001f);
            score += 1.0f - distance * invRadius;
            if (HasPowerups(ent))
                score *= 4.0f;
        }
        // Clamp score by a max value
        clamp_high(score, 3.0f);
        // Convert score to [0, 1] range
        score /= 3.0f;
        // Get a square root of score (values closer to 0 gets scaled more than ones closer to 1)
        score = 1.0f / Q_RSqrt(score + 0.001f);
        // Sanitize
        clamp(score, 0.0f, 1.0f);
        scores[i] = score;
    }

    // Then call callbacks
    const unsigned levelTime = level.time;
    for (unsigned i = 0; i < alertSpots.size(); ++i)
    {
        auto &alertSpot = alertSpots[i];
        unsigned nonReportedFor = levelTime - alertSpot.lastReportedAt;
        if (nonReportedFor >= 1000)
            alertSpot.lastReportedScore = 0.0f;

        // Since scores are sanitized, they are in range [0.0f, 1.0f], and abs(scoreDelta) is in range [-1.0f, 1.0f];
        float scoreDelta = scores[i] - alertSpot.lastReportedScore;
        if (scoreDelta >= 0)
        {
            if (nonReportedFor >= 1000 - scoreDelta * 500)
                alertSpot.Alert(this, scores[i]);
        }
        else
        {
            if (nonReportedFor >= 500 - scoreDelta * 500)
                alertSpot.Alert(this, scores[i]);
        }
    }
}

void Bot::ChangeWeapon(const CombatTask &combatTask)
{
    if (combatTask.CanUseBuiltinWeapon())
        self->r.client->ps.stats[STAT_PENDING_WEAPON] = combatTask.BuiltinWeapon();
    if (combatTask.CanUseScriptWeapon())
        GT_asSelectScriptWeapon(self->r.client, combatTask.ScriptWeapon());
}

void Bot::ChangeWeapon(int weapon)
{
    self->r.client->ps.stats[STAT_PENDING_WEAPON] = weapon;
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

    botBrain.oldCombatTask.Clear();
    botBrain.combatTask.Clear();

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

    // ask for respawn if the minimum bot respawning time passed
    if( level.time > self->deathTimeStamp + 3000 )
        ucmd.buttons = BUTTON_ATTACK;

    CallGhostingClientThink(&ucmd);
}

void Bot::CallGhostingClientThink(usercmd_t *ucmd)
{
    // set approximate ping and show values
    ucmd->serverTimeStamp = game.serverTime;
    ucmd->msec = game.frametime;
    self->r.client->r.ping = 0;

    ClientThink( self, ucmd, 0 );
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

bool Bot::MayKeepRunningInCombat() const
{
    if (!HasEnemy())
        FailWith("MayKeepRunningInCombat(): there is no enemy");

    Vec3 enemyToBotDir = Vec3(self->s.origin) - EnemyOrigin();
    bool enemyMayHit = true;
    if (IsEnemyAStaticSpot())
    {
        enemyMayHit = false;
    }
    else if (EnemyFireDelay() > 300)
    {
        enemyMayHit = false;
    }
    else
    {
        Vec3 enemyLookDir = EnemyLookDir();
        float squaredDistance = enemyToBotDir.SquaredLength();
        if (squaredDistance > 1)
        {
            float distance = 1.0f / Q_RSqrt(squaredDistance);
            enemyToBotDir *= 1.0f / distance;
            // Compute a cosine of angle between enemy look dir and enemy to bot dir
            float cosPhi = enemyLookDir.Dot(enemyToBotDir);
            // Be aware of RL splash on this range
            if (distance < 150.0f)
                enemyMayHit = cosPhi > 0.3;
            else if (cosPhi <= 0.3)
                enemyMayHit = false;
            else
            {
                float cotPhi = Q_RSqrt((1.0f / (cosPhi * cosPhi)) - 1);
                float sideMiss = distance / cotPhi;
                // Use hitbox height plus a bit as a worst case
                float hitboxLargestSectionSide = 8.0f + playerbox_stand_maxs[2] - playerbox_stand_mins[2];
                enemyMayHit = sideMiss < hitboxLargestSectionSide;
            }
        }
    }

    if (enemyMayHit)
        return false;

    vec3_t botLookDir;
    AngleVectors(self->s.angles, botLookDir, nullptr, nullptr);
    // Check whether the bot may hit while running
    return ((-enemyToBotDir).Dot(botLookDir) > 0.99);
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
        GhostingFrame();
    else
        ActiveFrame();
}

void Bot::ActiveFrame()
{
    //get ready if in the game
    if(GS_MatchState() <= MATCH_STATE_WARMUP && !IsReady() && self->r.client->teamstate.timeStamp + 4000 < level.time)
        G_Match_Ready(self);

    ApplyPendingTurnToLookAtPoint();

    const CombatTask &combatTask = botBrain.combatTask;

    bool inhibitShooting, inhibitCombatMove;
    SetCombatInhibitionFlags(&inhibitShooting, &inhibitCombatMove);

    // ucmd modification in FireWeapon() will be overwritten by MoveFrame()
    bool fireButtonPressed = false;
    if (!inhibitShooting)
    {
        SetCloakEnabled(false);
        // If bot fired builtin or script weapon, save builtin fire button status
        FireWeapon(&fireButtonPressed);
    }
    else
    {
        if (!combatTask.Empty())
        {
            SetCloakEnabled(true);
        }
        else if (botBrain.HasGoal())
        {
            if (botBrain.IsCloseToAnyGoal(768.0f, true))
                SetCloakEnabled(true);
            else if (botBrain.IsCloseToAnyGoal(384.0f, false))
                SetCloakEnabled(true);
            else
                SetCloakEnabled(false);
        }
        else
        {
            SetCloakEnabled(false);
        }
    }

    bool beSilent = ShouldBeSilent(inhibitShooting);

    // Do not modify pmove features by beSilent value, features may be changed dynamically by script.
    usercmd_t ucmd;
    memset(&ucmd, 0, sizeof(ucmd));
    MoveFrame(&ucmd, inhibitCombatMove, beSilent);

    if (fireButtonPressed)
        ucmd.buttons |= BUTTON_ATTACK;

    CallActiveClientThink(&ucmd);

    SayVoiceMessages();
}

bool Bot::ShouldBeSilent(bool inhibitShooting) const
{
    const CombatTask &combatTask = botBrain.combatTask;
    if (!inhibitShooting)
        return false;
    // Do not be silent if no enemy has been detected
    if (combatTask.Empty())
        return false;

    if ((combatTask.LastSeenEnemyOrigin() - self->s.origin).SquaredLength() < 384.0f * 384.0f)
    {
        if (CanAndWouldCloak())
            return true;

        // When there is only a single enemy
        if (botBrain.activeEnemyPool->ActiveEnemies().size() < 2)
        {
            Vec3 enemyToBot(self->s.origin);
            enemyToBot -= combatTask.LastSeenEnemyOrigin();
            enemyToBot.NormalizeFast();
            if (enemyToBot.Dot(EnemyLookDir()) < -0)
                return true;
        }
    }

    return false;
}

void Bot::SetCombatInhibitionFlags(bool *inhibitShootingRef, bool *inhibitCombatMoveRef)
{
    // Make reference aliases to avoid pointer/boolean confusing errors
    bool &inhibitCombatMove = *inhibitCombatMoveRef;
    bool &inhibitShooting = *inhibitShootingRef;

    const CombatTask &combatTask = botBrain.combatTask;
    inhibitShooting = combatTask.Empty() || combatTask.inhibit;
    inhibitCombatMove = inhibitShooting;
    if (inhibitCombatMove)
        return;

    if (botBrain.HasGoal() && currAasAreaNum != GoalAreaNum() && !nextReaches.empty())
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
        else if (aasWorld->AreaCrouch(currAasAreaNum))
            inhibitCombatMove = true;
    }
    // Try to move bunnying instead of dodging on ground
    // if the enemy is not looking to bot being able to hit him
    // and the bot is able to hit while moving without changing angle significantly
    if (!inhibitCombatMove && MayKeepRunningInCombat())
        inhibitCombatMove = true;
}

void Bot::CallActiveClientThink(usercmd_t *ucmd)
{
    //set up for pmove
    for (int i = 0; i < 3; i++)
        ucmd->angles[i] = ANGLE2SHORT(self->s.angles[i]) - self->r.client->ps.pmove.delta_angles[i];

    VectorSet(self->r.client->ps.pmove.delta_angles, 0, 0, 0);

    // set approximate ping and show values
    ucmd->msec = game.frametime;
    ucmd->serverTimeStamp = game.serverTime;

    // If this value is modified by ClientThink() callbacks, it will be kept until next frame reaches this line
    jumppadMovementState.hasTouchedJumppad = false;

    ClientThink( self, ucmd, 0 );
    self->nextThink = level.time + 1;
}


