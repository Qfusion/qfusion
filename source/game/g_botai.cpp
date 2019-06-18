#include "g_local.h"
#include "g_as_local.h"

static const gs_asEnumVal_t asNavEntityFlagsEnumVals[] = {
	ASLIB_ENUM_VAL( AI_NAV_REACH_AT_TOUCH ),
	ASLIB_ENUM_VAL( AI_NAV_REACH_AT_RADIUS ),
	ASLIB_ENUM_VAL( AI_NAV_REACH_ON_EVENT ),
	ASLIB_ENUM_VAL( AI_NAV_REACH_IN_GROUP ),
	ASLIB_ENUM_VAL( AI_NAV_DROPPED ),
	ASLIB_ENUM_VAL( AI_NAV_MOVABLE ),
	ASLIB_ENUM_VAL( AI_NAV_NOTIFY_SCRIPT ),

	ASLIB_ENUM_VAL_NULL
};


const gs_asEnum_t asAIEnums[] = {
	{"nav_entity_flags_e", asNavEntityFlagsEnumVals},
	ASLIB_ENUM_VAL_NULL
};

const gs_asClassDescriptor_t *asAIClassesDescriptors[] = {
	NULL
};

const gs_asglobfuncs_t asAIGlobFuncs[] = {
	{ "void AddNavEntity( Entity @ent, int flags )", asFUNCTION(AI_AddNavEntity), NULL },
	{ "void RemoveNavEntity( Entity @ent )", asFUNCTION(AI_RemoveNavEntity), NULL },
	{ "void NavEntityReached( Entity @ent )", asFUNCTION(AI_NavEntityReached), NULL },

	{ NULL }
};

void AI_InitLevel( void ) {}

void AI_Shutdown( void ) {}

void AI_BeforeLevelLevelScriptShutdown( void ) {}

void AI_AfterLevelScriptShutdown() {}

void AI_CommonFrame( void ) {}

void AI_JoinedTeam( edict_t *ent, int team ) {}

void AI_InitGametypeScript( class asIScriptModule *module ) {}

void AI_ResetGametypeScript() {}

void AI_AddNavEntity( edict_t *ent, ai_nav_entity_flags flags ) {}

void AI_RemoveNavEntity( edict_t *ent ) {}

void AI_NavEntityReached( edict_t *ent ) {}

void AI_Think( edict_t *self ) {}
void G_FreeAI( edict_t *ent ) {}

void G_SpawnAI( edict_t *ent, float skillLevel ) {}

ai_type AI_GetType( const ai_handle_t *ai )
{
	return AI_INACTIVE;
}

void AI_TouchedEntity( edict_t *self, edict_t *ent ) {}

void AI_DamagedEntity( edict_t *self, edict_t *ent, int damage ) {}

void AI_Pain( edict_t *self, edict_t *attacker, int kick, int damage ) {}

void AI_RegisterEvent( edict_t *ent, int event, int parm ) {}

void AI_SpawnBot( const char *team ) {}
void AI_RemoveBot( const char *name ) {}
void AI_RemoveBots() {}

void AI_Respawn( edict_t *ent ) {}

void AI_Cheat_NoTarget( edict_t *ent ) {}
