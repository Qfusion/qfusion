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
// Should be called when a static level item is about to spawn first time
void AI_AddStaticItem( edict_t *ent );
// Should be called when an item has been dropped
void AI_AddDroppedItem( edict_t *ent );
// Should be called when an item edict is about to be freed
void AI_DeleteItem( edict_t *ent );

void AI_JoinedTeam( edict_t *ent, int team );

void        AI_Think( edict_t *self );
void        G_FreeAI( edict_t *ent );
void        G_SpawnAI( edict_t *ent, float skillLevel = 0.1f );
ai_type		AI_GetType( const ai_handle_t *ai );
void		AI_TouchedEntity( edict_t *self, edict_t *ent );
void        AI_DamagedEntity( edict_t *self, edict_t *ent, int damage );
// For unknown reasons self->pain is not called for bots, this is the workaround
void        AI_Pain( edict_t *self, edict_t *attacker, int kick, int damage );

// bot_spawn.c
void        BOT_SpawnBot( const char *team );
void        BOT_RemoveBot( const char *name );
void        BOT_Respawn( edict_t *ent );

void        AI_Cheat_NoTarget( edict_t *ent );

#endif