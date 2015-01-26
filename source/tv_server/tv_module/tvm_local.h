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

*/

#ifndef __TVM_LOCAL_H
#define __TVM_LOCAL_H

#include "../../gameshared/q_arch.h"
#include "../../gameshared/q_math.h"
#include "../../gameshared/q_shared.h"
#include "../../gameshared/q_cvar.h"
#include "../../gameshared/q_dynvar.h"
#include "../../gameshared/q_comref.h"
#include "../../gameshared/q_collision.h"

/*
* shared with server and module
*/
#include "tvm_public.h"

struct tvm_relay_s
{
	relay_t	*server;

	// entities from the remote server
	int maxclients;
	int maxentities;
	int numentities;
	unsigned int snapFrameTime;

	edict_t	*edicts;            // [maxentities]
	gclient_t *clients;         // [maxclients]

	// local entities for this relay
	int local_maxclients;
	int local_maxentities;
	int local_numentities;

	edict_t	*local_edicts;      // [local_maxclients]
	gclient_t *local_clients;   // [local_maxclients]

	char *map_entities;         // raw string containing the unparsed entities
	char mapname[MAX_CONFIGSTRING_CHARS];

	unsigned int serverTime;        // time in the server
	snapshot_t frame;
	game_state_t gameState;
	char configStrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];
	bool configStringsOverwritten[MAX_CONFIGSTRINGS];

	int playernum;

	struct
	{
		int frags;
		int health;
		int last_killer;
	} stats;

	struct
	{
		int quad, shell, regen, enemy_flag;
	} effects;
};

typedef struct
{
	unsigned int realtime;          // actual time
	int maxclients;
} tv_module_locals_t;

/*
* edict stuff
*/

#define	FOFS( x ) (size_t)&( ( (edict_t *)0 )->x )

typedef enum
{
	MOVETYPE_NONE,      // never moves
	MOVETYPE_PLAYER,    // never moves (but is moved by pmove)
	MOVETYPE_NOCLIP,    // origin and angles change with no interaction
	MOVETYPE_PUSH,      // no clip to world, push on box contact
	MOVETYPE_STOP,      // no clip to world, stops on box contact
	MOVETYPE_FLY,
	MOVETYPE_TOSS,      // gravity
	MOVETYPE_FLYMISSILE, // extra size to monsters
	MOVETYPE_BOUNCE,
	MOVETYPE_BOUNCEGRENADE
} movetype_t;

typedef struct snap_edict_s
{
	int buttons;
} snap_edict_t;

struct edict_s
{
	entity_state_t s;
	entity_shared_t	r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	bool local;             // from local_edicts, not edicts
	tvm_relay_t *relay;

	snap_edict_t snap; // information that is cleared each frame snap

	movetype_t movetype;
	int timeDelta;
	int viewheight;             // height above origin where eyesight is determined
	vec3_t velocity;

	edict_t	*groundentity;
	int groundentity_linkcount;

	int watertype;
	int waterlevel;
};

/*
* trap_* functions
*/

#include "tvm_syscalls.h"

/*
* mem stuff
*/

#define TVM_Malloc( relay_server, size ) trap_MemAlloc( relay_server, size, __FILE__, __LINE__ )
#define TVM_Free( data ) trap_MemFree( data, __FILE__, __LINE__ )

/*
* tv module wide things
*/

extern tv_module_locals_t tvm;

extern cvar_t *developer;
extern cvar_t *tv_chasemode;

#define ENTNUM( x ) ( ( x->local ? ( x ) - x->relay->local_edicts : ( x ) - x->relay->edicts ) )
#define PLAYERNUM( x ) ( ( x->local ? ( x ) - x->relay->local_edicts : ( x ) - x->relay->edicts - 1 ) )

// from tvm_main.c
void TVM_Printf( const char *format, ... );
void TVM_Error( const char *format, ... );
void TVM_RelayError( tvm_relay_t *relay, const char *format, ... );

#endif // __TVM_LOCAL_H
