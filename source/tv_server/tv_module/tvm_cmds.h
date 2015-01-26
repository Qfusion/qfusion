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

#ifndef __TVM_CMDS_H
#define __TVM_CMDS_H

#include "tvm_local.h"

bool TVM_ClientCommand( tvm_relay_t *relay, edict_t *ent );
void TVM_RemoveGameCommands( tvm_relay_t *relay );
void TVM_AddGameCommands( tvm_relay_t *relay );

#endif // __TVM_CMDS_H
