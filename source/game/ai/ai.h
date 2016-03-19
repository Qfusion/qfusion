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

typedef enum
{
	AI_INACTIVE,
	AI_ISBOT,
	AI_ISMONSTER,

	AI_MAX_TYPES
} ai_type;

typedef struct ai_handle_s ai_handle_t;
extern const size_t ai_handle_size;

// bot_cmds.c
bool    BOT_Commands( edict_t *ent );
bool    BOT_ServerCommand( void );

// ai_main.c
void        AI_InitLevel( void );
void        AI_Shutdown( void );
// Should be called before all entities (including clients) think
void        AI_CommonFrame( void );
void		AI_AddGoalEntity( edict_t *ent );
void		AI_AddGoalEntityCustom( edict_t *ent );
void		AI_AddNavigatableEntity( edict_t *ent, int node );
void		AI_RemoveGoalEntity( edict_t *ent );
void		AI_InitEntitiesData( void );
void        AI_Think( edict_t *self );
void        G_FreeAI( edict_t *ent );
void        G_SpawnAI( edict_t *ent );
ai_type		AI_GetType( const ai_handle_t *ai );
void		AI_ClearWeights( ai_handle_t *ai );
void		AI_SetGoalWeight( ai_handle_t *ai, int index, float weight );
void		AI_ResetWeights( ai_handle_t *ai );
float		AI_GetItemWeight( const ai_handle_t *ai, const gsitem_t *item );
int			AI_GetRootGoalEnt( void );
int			AI_GetNextGoalEnt( int index );
edict_t		*AI_GetGoalEntity( int index );
void		AI_ReachedEntity( edict_t *self );
void		AI_TouchedEntity( edict_t *self, edict_t *ent );
void        AI_DamagedEntity( edict_t *self, edict_t *ent, int damage );
float		AI_GetCharacterReactionTime( const ai_handle_t *ai );
float		AI_GetCharacterOffensiveness( const ai_handle_t *ai );
float		AI_GetCharacterCampiness( const ai_handle_t *ai );
float		AI_GetCharacterFirerate( const ai_handle_t *ai );

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
