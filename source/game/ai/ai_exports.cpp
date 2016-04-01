#include "bot.h"
#include "ai_shutdown_hooks_holder.h"
#include "aas.h"

cvar_t *sv_botpersonality;

ai_weapon_t AIWeapons[WEAP_TOTAL];
const size_t ai_handle_size = sizeof( ai_handle_t );

static bool ai_intialized = false;
static bool aas_system_initialized = false;
static bool aas_data_loaded = false;

//==========================================
// AI_InitLevel
// Inits Map local parameters
//==========================================
void AI_InitLevel( void )
{
    edict_t	*ent;

    //Init developer mode
    bot_showpath = trap_Cvar_Get( "bot_showpath", "0", 0 );
    bot_showcombat = trap_Cvar_Get( "bot_showcombat", "0", 0 );
    bot_showsrgoal = trap_Cvar_Get( "bot_showsrgoal", "0", 0 );
    bot_showlrgoal = trap_Cvar_Get( "bot_showlrgoal", "0", 0 );
    bot_dummy = trap_Cvar_Get( "bot_dummy", "0", 0 );
    sv_botpersonality =	    trap_Cvar_Get( "sv_botpersonality", "0", CVAR_ARCHIVE );

    aas_system_initialized = AI_InitAAS();
    if (!aas_system_initialized)
        G_Printf("Can't initialize AAS system\n");
    if (aas_system_initialized)
        aas_data_loaded = AI_LoadLevelAAS(level.mapname);

    // count bots
    game.numBots = 0;
    for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ )
    {
        if( !ent->r.inuse || !ent->ai ) continue;
        if( ent->r.svflags & SVF_FAKECLIENT && AI_GetType( ent->ai ) == AI_ISBOT )
            game.numBots++;
    }

    // set up weapon usage weights

    memset( &AIWeapons, 0, sizeof( ai_weapon_t )*WEAP_TOTAL );

    //WEAP_GUNBLADE
    AIWeapons[WEAP_GUNBLADE].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.2f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_SHORT_RANGE] = 0.3f;
    AIWeapons[WEAP_GUNBLADE].RangeWeight[AIWEAP_MELEE_RANGE] = 0.4f;

    //WEAP_MACHINEGUN
    AIWeapons[WEAP_MACHINEGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.8f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.7f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_MACHINEGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.1f;

    //WEAP_RIOTGUN
    AIWeapons[WEAP_RIOTGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.8f;
    AIWeapons[WEAP_RIOTGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.5f;

    //ROCKETLAUNCHER
    AIWeapons[WEAP_ROCKETLAUNCHER].aimType = AI_AIMSTYLE_PREDICTION_EXPLOSIVE;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_LONG_RANGE] = 0.2f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_SHORT_RANGE] = 0.9f;
    AIWeapons[WEAP_ROCKETLAUNCHER].RangeWeight[AIWEAP_MELEE_RANGE] = 0.6f;

    //WEAP_GRENADELAUNCHER
    AIWeapons[WEAP_GRENADELAUNCHER].aimType = AI_AIMSTYLE_DROP;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_LONG_RANGE] = 0.0f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.1f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_GRENADELAUNCHER].RangeWeight[AIWEAP_MELEE_RANGE] = 0.3f;

    //WEAP_PLASMAGUN
    AIWeapons[WEAP_PLASMAGUN].aimType = AI_AIMSTYLE_PREDICTION;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.1f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.5f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.7f;
    AIWeapons[WEAP_PLASMAGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.4f;

    //WEAP_ELECTROBOLT
    AIWeapons[WEAP_ELECTROBOLT].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_LONG_RANGE] = 0.9f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.7f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_SHORT_RANGE] = 0.4f;
    AIWeapons[WEAP_ELECTROBOLT].RangeWeight[AIWEAP_MELEE_RANGE] = 0.3f;

    //WEAP_LASERGUN
    AIWeapons[WEAP_LASERGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.0f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.0f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.7f;
    AIWeapons[WEAP_LASERGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.6f;

    //WEAP_INSTAGUN
    AIWeapons[WEAP_INSTAGUN].aimType = AI_AIMSTYLE_INSTANTHIT;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_LONG_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_MEDIUM_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_SHORT_RANGE] = 0.9f;
    AIWeapons[WEAP_INSTAGUN].RangeWeight[AIWEAP_MELEE_RANGE] = 0.9f;

    ai_intialized = true;
}

void AI_Shutdown( void )
{
    if (!ai_intialized)
        return;
    BOT_RemoveBot("all");

    if (aas_system_initialized)
        AI_ShutdownAAS();
    aas_system_initialized = false;

    AiShutdownHooksHolder::Instance()->InvokeHooks();
    ai_intialized = false;
}

void AI_CommonFrame()
{
    AI_AASFrame();
}

//==========================================
// G_FreeAI
// removes the AI handle from memory
//==========================================
void G_FreeAI( edict_t *ent )
{
    if( !ent->ai ) {
        return;
    }
    if( ent->ai->type == AI_ISBOT ) {
        game.numBots--;
    }

    // Invoke an appropriate destructor based on ai instance type, then free memory.
    // It is enough to call G_Free(ent->ai->aiRef), since botRef (if it is present)
    // points to the same block as aiRef does, but to avoid confusion we free pointer aliases explicitly.
    if (ent->ai->botRef) {
        ent->ai->botRef->~Bot();
        G_Free(ent->ai->botRef);
    } else {
        ent->ai->aiRef->~Ai();
        G_Free(ent->ai->aiRef);
    }
    ent->ai->aiRef = nullptr;
    ent->ai->botRef = nullptr;

    G_Free( ent->ai );
    ent->ai = NULL;
}

//==========================================
// G_SpawnAI
// allocate ai_handle_t for this entity
//==========================================
void G_SpawnAI( edict_t *ent )
{
    if( !ent->ai ) {
        ent->ai = ( ai_handle_t * )G_Malloc( sizeof( ai_handle_t ) );
    }
    else {
        memset( &ent->ai, 0, sizeof( ai_handle_t ) );
    }

    if( ent->r.svflags & SVF_FAKECLIENT ) {
        ent->ai->type = AI_ISBOT;
        void *mem = G_Malloc( sizeof(Bot) );
        ent->ai->botRef = new(mem) Bot( ent );
        ent->ai->aiRef = ent->ai->botRef;
    }
    else {
        ent->ai->type = AI_ISMONSTER;
        void *mem = G_Malloc( sizeof(Ai) );
        ent->ai->botRef = nullptr;
        ent->ai->aiRef = new(mem) Ai( ent );
    }
}

//==========================================
// AI_GetType
//==========================================
ai_type AI_GetType( const ai_handle_t *ai )
{
    return ai ? ai->type : AI_INACTIVE;
}

//==========================================
// AI_ClearWeights
//==========================================
void AI_ClearWeights( ai_handle_t *ai )
{
    memset( ai->status.entityWeights, 0, sizeof( ai->status.entityWeights ) );
}

//==========================================
// AI_SetGoalWeight
//==========================================
void AI_SetGoalWeight( ai_handle_t *ai, int index, float weight )
{
    if( index < 0 || index >= MAX_GOALENTS )
        return;
    ai->status.entityWeights[index] = weight;
}

//==========================================
// AI_ResetWeights
// Init bot weights from bot-class weights.
//==========================================
void AI_ResetWeights( ai_handle_t *ai )
{
    // restore defaults from bot personality
    AI_ClearWeights( ai );

    FOREACH_GOALENT( goalEnt )
    {
        if( goalEnt->ent->item )
            AI_SetGoalWeight( ai, goalEnt->id, AI_GetItemWeight( ai, goalEnt->ent->item ) );
    }
}

//==========================================
// AI_GetItemWeight
//==========================================
float AI_GetItemWeight( const ai_handle_t *ai, const gsitem_t *item )
{
    if( !item )
        return 0;
    return ai->pers.inventoryWeights[item->tag];
}
//==========================================
// AI_GetRootGoalEnt
//==========================================
int AI_GetRootGoalEnt( void )
{
    return -1;
}

//==========================================
// AI_GetNextGoalEnt
//==========================================
int AI_GetNextGoalEnt( int index )
{
    if (!AAS_Initialized() || index < 0 || index >= MAX_GOALENTS )
        return -1;

    return GoalEntitiesRegistry::Instance()->GetNextGoalEnt(index);
}

//==========================================
// AI_GetGoalEntity
//==========================================
edict_t *AI_GetGoalEntity( int index )
{
    if (!AAS_Initialized() || index < 0 || index >= MAX_GOALENTS )
        return NULL;

    return GoalEntitiesRegistry::Instance()->GetGoalEntity(index);
}

/*
* AI_ReachedEntity
* Some nodes are declared so they are never reached until the entity says so.
* This is a entity saying so.
*/
void AI_ReachedEntity(edict_t *self)
{
    // self->ai->aiRef->ReachedEntity();
    // Currently disabled
    abort();
}

/*
* AI_TouchedEntity
* Some AI has touched some entity. Some entities are declared to never be reached until touched.
* See if it's one of them and declare it reached
*/
void AI_TouchedEntity(edict_t *self, edict_t *ent)
{
    self->ai->aiRef->TouchedEntity(ent);
}

void AI_DamagedEntity(edict_t *self, edict_t *ent, int damage)
{
    if (self->ai->botRef)
        self->ai->botRef->OnEnemyDamaged(ent, damage);
}

float AI_GetCharacterReactionTime( const ai_handle_t *ai )
{
    return ai == nullptr ? 0 : ai->pers.cha.reaction_time;
}

float AI_GetCharacterOffensiveness( const ai_handle_t *ai )
{
    return ai == nullptr ? 0 : ai->pers.cha.offensiveness;
}

float AI_GetCharacterCampiness( const ai_handle_t *ai )
{
    return ai == nullptr ? 0 : ai->pers.cha.campiness;
}

float AI_GetCharacterFirerate( const ai_handle_t *ai )
{
    return ai == nullptr ? 0 : ai->pers.cha.firerate;
}

void AI_Think(edict_t *self)
{
    if( !self->ai || self->ai->type == AI_INACTIVE )
        return;

    self->ai->aiRef->Think();
}

//==========================================
// AI_EnemyAdded
// Add the Player to our list
//==========================================
void AI_EnemyAdded( edict_t *ent )
{
    AI_AddGoalEntity( ent );
}

//==========================================
// AI_EnemyRemoved
// Remove the Player from list
//==========================================
void AI_EnemyRemoved( edict_t *ent )
{
    AI_RemoveGoalEntity( ent );
}
