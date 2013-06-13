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

#include "AStar.h"

typedef struct
{
	unsigned int moveTypesMask; // moves the bot can perform at this moment
	float entityWeights[MAX_EDICTS];
} ai_status_t;

typedef struct
{
	const char *name;
	float default_yaw_speed;
	float reaction_time;		
	float combatmove_timeout;
	float yaw_accel;
	float offensiveness;
	float campiness;
	float firerate;
	float armor_grabber;
	float health_grabber;

	float weapon_affinity[WEAP_TOTAL];
} ai_character;

typedef struct
{
	const char *netname;
	float skillLevel;       // Affects AIM and fire rate (fraction of 1)
	unsigned int moveTypesMask;      // bot can perform these moves, to check against required moves for a given path
	float inventoryWeights[MAX_ITEMS];

	//class based functions
	void ( *UpdateStatus )( edict_t *ent );
	void ( *RunFrame )( edict_t *ent );
	void ( *blockedTimeout )( edict_t *ent );

	ai_character cha;
} ai_pers_t;

typedef enum
{
	AI_INACTIVE,
	AI_ISBOT,
	AI_ISMONSTER,

	AI_MAX_TYPES
} ai_type;


typedef struct
{
	ai_pers_t pers;         // persistant definition (class?)
	ai_status_t status;     //player (bot, NPC) status for AI frame

	ai_type	type;

	unsigned int state_combat_timeout;

	// movement
	vec3_t move_vector;
	unsigned int blocked_timeout;
	unsigned int changeweapon_timeout;
	unsigned int statusUpdateTimeout;

	unsigned int combatmovepush_timeout;
	int combatmovepushes[3];

	// nodes
	int current_node;
	int goal_node;
	int next_node;
	unsigned int node_timeout;
	struct nav_ents_s *goalEnt;

	unsigned int longRangeGoalTimeout;
	unsigned int shortRangeGoalTimeout;
	int tries;

	struct astarpath_s path; //jabot092

	int nearest_node_tries;     //for increasing radius of search with each try

	//vsay
	unsigned int vsay_timeout;
	edict_t	*vsay_goalent;

	edict_t	*latched_enemy;
	int enemyReactionDelay;
	//int				rethinkEnemyDelay;
	bool notarget;  // bots can not see this entity
	bool rj_triggered;
	bool dont_jump;
	bool camp_item;
	bool rush_item;
	float speed_yaw, speed_pitch;
	bool is_bunnyhop;

	int asFactored, asRefCount;

} ai_handle_t;


// bot_cmds.c
bool    BOT_Commands( edict_t *ent );
bool    BOT_ServerCommand( void );

// ai_main.c
void        AI_InitLevel( void );
void		AI_AddGoalEntity( edict_t *ent );
void		AI_AddGoalEntityCustom( edict_t *ent );
void		AI_RemoveGoalEntity( edict_t *ent );
void		AI_InitEntitiesData( void );
void        AI_Think( edict_t *self );
void        G_FreeAI( edict_t *ent );
void        G_SpawnAI( edict_t *ent );
void AI_ResetWeights( ai_handle_t *ai );
edict_t *AI_GetGoalEnt( int index );
int AI_GetGoalEntNode( int index );
void AI_ReachedEntity( edict_t *self );
void AI_TouchedEntity( edict_t *self, edict_t *ent );

// ai_items.c
void        AI_EnemyAdded( edict_t *ent );
void        AI_EnemyRemoved( edict_t *ent );

// bot_spawn.c
void        BOT_SpawnBot( const char *team );
void        BOT_RemoveBot( const char *name );
void        BOT_Respawn( edict_t *ent );

// ai_tools.c
void        AIDebug_ToogleBotDebug( void );

void        AITools_Frame( void );
void        AITools_DropNodes( edict_t *ent );
void        AITools_InitEditnodes( void );
void        AITools_InitMakenodes( void );
void        AITools_AddBotRoamNode_Cmd( void );
void        AITools_AddNode_Cmd( void );
void		Cmd_SaveNodes_f( void );

void        AI_Cheat_NoTarget( edict_t *ent );
