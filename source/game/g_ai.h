#pragma once

enum ai_nav_entity_flags {
	// A nav entity should be reached at touch (as regular items like weapons, armors, etc.)
	// A nav entity should be spawned, otherwise an AI may (and usually should) wait for it.
	// This flag cannot be combined with REACH_AT_RADIUS or REACH_ON_EVENT.
	AI_NAV_REACH_AT_TOUCH = 0x1,
	// A nav entity should be reached by proximity. Default proximity raduis is close to a player height.
	// This flag cannot be combined with REACH_AT_TOUCH or REACH_ON_EVENT.
	AI_NAV_REACH_AT_RADIUS = 0x2,
	// A nav entity may be reached on an event (when a callee decides), an AI should wait on the item for it.
	// This flag cannot be combined with REACH_AT_TOUCH or REACH_AT_RADIUS.
	AI_NAV_REACH_ON_EVENT = 0x4,
	// A nav entity may (and should) be reached in group (like a bomb being planted).
	// If this flag is set, all AI's in a squad (if any) will try to reach it.
	// Otherwise a squad will select a goal grabber, and other AIs will try to assist it (like Quad pickup).
	AI_NAV_REACH_IN_GROUP = 0x8,
	// One of the following callbacks:
	// AI_asOnBotTouchedGoal() for REACH_AT_TOUCH
	// AI_asOnBotIsCloseToGoal() for REACH_AT_RADIUS
	// should be called on corresponding event detected by bot native code
	AI_NAV_NOTIFY_SCRIPT = 0x100,
	// A nav entity will disappear at its next think.
	// An AI may skip this goal if it thinks that goal will disappear before it may be reached.
	AI_NAV_DROPPED = 0x1000,
	// A nav entity is movable (e.g. is a player).
	// Use this flag cautious to prevent AI cheating with revealing enemy origin.
	// (A good use case for this flag is a flag carrier of the own team, not the enemy one).
	// Note that all clients are already added as such entities at level start.
	// If you really want to force AI pursue some client, set its external entity weight.
	// Note that client nav entities also have REACH_ON_EVENT flags.
	AI_NAV_MOVABLE = 0x4000,
};
struct ai_handle_t;

void AI_InitLevel();
void AI_Shutdown();
void AI_RemoveBots();

void AI_CommonFrame();

void AI_Respawn( edict_t * ent );

void AI_SpawnBot( const char * teamName );

void AI_RemoveBot( const char * name );

void AI_AddNavEntity( edict_t * ent, ai_nav_entity_flags flags );

void AI_RemoveNavEntity( edict_t * ent );

void AI_RegisterEvent( edict_t * ent, int event, int parm );

void AI_TouchedEntity( edict_t * self, edict_t * ent );

void AI_Think( edict_t * self );
