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

#ifndef __TVM_CLIP_H
#define __TVM_CLIP_H

#include "tvm_local.h"

int	G_PointContents( tvm_relay_t *relay, vec3_t p );
void	G_Trace( tvm_relay_t *relay, trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask );
#ifdef TVCOLLISION4D
int	G_PointContents4D( tvm_relay_t *relay, vec3_t p, int timeDelta );
void	G_Trace4D( tvm_relay_t *relay, trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask, int timeDelta );
void	GClip_BackUpCollisionFrame( tvm_relay_t *relay );
edict_t	*GClip_FindBoxInRadius4D( tvm_relay_t *relay, edict_t *from, vec3_t org, float rad, int timeDelta );
#endif
void	GClip_ClearWorld( tvm_relay_t *relay );
void	GClip_SetBrushModel( tvm_relay_t *relay, edict_t *ent, char *name );
void	GClip_SetAreaPortalState( tvm_relay_t *relay, edict_t *ent, bool open );
void	GClip_LinkEntity( tvm_relay_t *relay, edict_t *ent );
void	GClip_UnlinkEntity( tvm_relay_t *relay, edict_t *ent );
void	GClip_TouchTriggers( tvm_relay_t *relay, edict_t *ent );
void	GClip_LinearMovement( tvm_relay_t *relay, edict_t *ent );

#endif // __TVM_CLIP_H
