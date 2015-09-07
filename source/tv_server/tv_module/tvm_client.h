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

#ifndef __TVM_CLIENT_H
#define __TVM_CLIENT_H

#include "tvm_local.h"

// client data that stays across multiple level loads
// reseted only on connect
typedef struct
{
	char userinfo[MAX_INFO_STRING];
	char netname[MAX_INFO_VALUE];

	bool connected;     // a loadgame will leave valid entities that just don't have a connection yet
	bool connecting;    // so whe know when a player is in the process of connecting for scoreboard prints

	bool multiview;

	short cmd_angles[3];            // angles sent over in the last command
} client_persistant_t;

typedef struct
{
	bool active;                // so target can remember the position when not chasing
	bool teamonly;
	int target;
	int mode;                       // 3rd or 1st person
	int range;
	int followmode;
} chasecam_t;

struct gclient_s
{
	// known to server
	player_state_t ps;          // communicated by server to clients
	client_shared_t	r;

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================

	client_persistant_t pers;           // cleared on connect
	chasecam_t chase;

	pmove_state_t old_pmove;            // for detecting out-of-pmove changes
	int buttons;
	uint8_t plrkeys;                      // used for displaying key icons
	int timeDelta;                      // time offset to adjust for shots collision (antilag)
};

void TVM_ClientEndSnapFrame( edict_t *ent );

bool TVM_ClientIsZoom( edict_t *ent );

void TVM_ClientBegin( tvm_relay_t *relay, edict_t *ent );
void TVM_ClientUserinfoChanged( tvm_relay_t *relay, edict_t *ent, char *userinfo );
bool TVM_CanConnect( tvm_relay_t *relay, char *userinfo );
void TVM_ClientConnect( tvm_relay_t *relay, edict_t *ent, char *userinfo );
void TVM_ClientDisconnect( tvm_relay_t *relay, edict_t *ent );
bool TVM_ClientMultiviewChanged( tvm_relay_t *relay, edict_t *ent, bool multiview );
void TVM_ClientThink( tvm_relay_t *relay, edict_t *ent, usercmd_t *ucmd, int timeDelta );

#endif // __TVM_CLIENT_H
