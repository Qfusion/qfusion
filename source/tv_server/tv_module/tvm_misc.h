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

#ifndef __TVM_MISC_H
#define __TVM_MISC_H

#include "tvm_local.h"

void TVM_PrintMsg( tvm_relay_t *relay, edict_t *ent, const char *format, ... );
void TVM_CenterPrintMsg( tvm_relay_t *relay, edict_t *ent, const char *format, ... );

edict_t *TVM_FindLocal( tvm_relay_t *relay, const edict_t *start, size_t fieldofs, const char *match );
bool TVM_AllowDownload( tvm_relay_t *relay, edict_t *ent, const char *requestname, const char *uploadname );
bool TVM_ConfigString( tvm_relay_t *relay, int number, const char *value );

void TVM_SetAudoTrack( tvm_relay_t *relay, const char *track );

#endif // __TVM_MISC_H
