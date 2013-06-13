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

#ifndef __TVM_CHASE_H
#define __TVM_CHASE_H

#include "tvm_local.h"

void TVM_Cmd_ChaseNext( edict_t *ent );
void TVM_Cmd_ChasePrev( edict_t *ent );
void TVM_Cmd_ChaseCam( edict_t *ent );
void TVM_Cmd_SwitchChaseCamMode( edict_t *ent );
void TVM_ChasePlayer( edict_t *ent, char *name, int followmode );
void TVM_ChaseClientEndSnapFrame( edict_t *ent );

#endif // __TVM_CHASE_H
