/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
--------------------------------------------------------------
The ACE Bot is a product of Steve Yeager, and is available from
the ACE Bot homepage, at http://www.axionfx.com/ace.

This program is a modification of the ACE Bot, and is therefore
in NO WAY supported by Steve Yeager.
*/

#ifndef QFUSION_AI_H
#define QFUSION_AI_H

typedef enum
{
	AI_INACTIVE,
	AI_ISBOT,
	AI_ISMONSTER,

	AI_MAX_TYPES
} ai_type;

typedef struct ai_handle_s ai_handle_t;
extern const size_t ai_handle_size;

typedef enum
{
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
	AI_NAV_MOVABLE = 0x4000
} ai_nav_entity_flags;

// Should be called before static entities spawn
void AI_InitLevel( void );
// Should be called before level and entities data cleanup
void AI_Shutdown( void );
// Should be called before level restart
void AI_UnloadLevel();
// Should be called when current gametype has been changed on has been set up first time
void AI_GametypeChanged( const char *gametype );
// Should be called before all entities (including AI's and clients) think
void AI_CommonFrame( void );
// Should be called when an AI joins a team
void AI_JoinedTeam( edict_t *ent, int team );

// These functions are exported to the script API

// Should be called when a static item is spawned or a dropped item stopped its movement
void AI_AddNavEntity( edict_t *ent, ai_nav_entity_flags flags );
// Should be called when an item edict is about to be freed
void AI_RemoveNavEntity(edict_t *ent);
// Should be called when an item has been considered to be reached by this function caller
// (A corresponding nav entity should be added with a AI_NAV_REACH_ON_EVENT flag)
void AI_NavEntityReached( edict_t *ent );

void AI_AddDefenceSpot( int team, int id, edict_t *ent, float radius );
void AI_RemoveDefenceSpot( int team, int id );

void AI_DefenceSpotAlert( int team, int id, float alertLevel, unsigned timeoutPeriod );

// If bots of a team see an enemy in defence spot bounds, they call an alert.
// Enemy detection uses "fair" vision of bots, it does not cheat with wallhacking.
// Bots will decide what alert level and timeout should be used.
void AI_EnableDefenceSpotAutoAlert( int team, int id );
void AI_DisableDefenceSpotAutoAlert( int team, int id );

void AI_AddOffenceSpot( int team, int id, edict_t *ent );
void AI_RemoveOffenceSpot( int team, int id );

// Bot methods accessible from scripts

// An offensiveness is a value in range [0.0f, 1.0f].
// An offensiveness affects whether and when a bot will pursue enemies, ignore enemies or retreat from enemies.
// Base offensiveness may be modified by provided setter.
// For example, bots on its own flag base should have high offensiveness to pursue an invader,
// and a flag base invader should have low offensiveness to prefer stealing a flag rather than fighting.
// If a value set is outside of the valid offensiveness range, it will be clamped.
// Effective offensiveness is computed based on the base offensiveness and carrier/supporter bot status.
// (a carrier automatically obtain zero offensiveness, and its supporters get a maximal one)
// Effective offensiveness cannot be overridden but may be retrieved using the provided getter.
// Base offensiveness is reset to its default value 0.5f on each death.
float AI_GetBotBaseOffensiveness( ai_handle_t *ai );
float AI_GetBotEffectiveOffensiveness( ai_handle_t *ai );
void AI_SetBotBaseOffensiveness( ai_handle_t *ai, float baseOffensiveness );

// Negative attitude means that the ent is an enemy.
// Positive attitude means that the ent is a mate.
// Zero attitude means bot should ignore the ent.
// A magnitude of an attitude is currently ignored.
// Note that attitude cannot override game teams (setting non-positive attitude in a team-based GT will be ignored).
// This function is useful for masking potential enemies for custom GTs like "headhunt" or "hot potato".
void AI_SetBotAttitude( ai_handle_t *ai, edict_t *ent, int attitude );

// Clears all external entity weights for a bot that are used in search for a goal.
// This means all internal weights are not overridden by external weights anymore.
// Note that internal weights are not affected by this call.
void AI_ClearBotExternalEntityWeights( ai_handle_t *ai );
// Sets a weight for the ent that is used in search for a goal.
// Note that if there is no nav entity corresponding to the ent, this function has no effect.
// If a zero weight is set, an internal weight computed by hardcoded bot logic is used.
// If a weight is negative, the ent will be ignored in search for a goal.
void AI_SetBotExternalEntityWeight( ai_handle_t *ai, edict_t *ent, float weight );

bool GT_asBotWouldDropHealth( const gclient_t *client );
void GT_asBotDropHealth( gclient_t *client );
bool GT_asBotWouldDropArmor( const gclient_t *client );
void GT_asBotDropArmor( gclient_t *client );

bool GT_asBotWouldCloak( const gclient_t *client );
void GT_asSetBotCloakEnabled( gclient_t *client, bool enabled );
// Useful for testing any entity for cloaking
bool GT_asIsEntityCloaking( const edict_t *ent );

void GT_asBotTouchedGoal( const ai_handle_t *bot, const edict_t *goalEnt );
void GT_asBotReachedGoalRadius( const ai_handle_t *bot, const edict_t *goalEnt );

// These functions return a score in range [0, 1].
// Default score should be 0.5f, and it should be returned
// when a GT script does not provide these function counterparts.
// Note that offence and defence score are not complementary but independent.
float GT_asPlayerOffenciveAbilitiesScore(const gclient_t *client);
float GT_asPlayerDefenciveAbilitiesScore(const gclient_t *client);

void        AI_Think( edict_t *self );
void        G_FreeAI( edict_t *ent );
void        G_SpawnAI( edict_t *ent, float skillLevel = 0.1f );
ai_type		AI_GetType( const ai_handle_t *ai );
void		AI_TouchedEntity( edict_t *self, edict_t *ent );
void        AI_DamagedEntity( edict_t *self, edict_t *ent, int damage );
// For unknown reasons self->pain is not called for bots, this is the workaround
void        AI_Pain( edict_t *self, edict_t *attacker, int kick, int damage );

void        AI_SpawnBot(const char *team);
void        AI_RemoveBot(const char *name);
void        AI_RemoveBots();
void        AI_Respawn(edict_t *ent);

void        AI_Cheat_NoTarget( edict_t *ent );

#endif