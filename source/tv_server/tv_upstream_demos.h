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

#ifndef __TV_UPSTREAM_DEMOS_H
#define __TV_UPSTREAM_DEMOS_H

#include "tv_local.h"

bool TV_Upstream_IsAutoRecordable( upstream_t *upstream );
void TV_Upstream_AutoRecordAction( upstream_t *upstream, const char *action );
void TV_Upstream_WriteDemoMessage( upstream_t *upstream, msg_t *msg );
void TV_Upstream_StartDemoRecord( upstream_t *upstream, const char *demoname, bool silent );
void TV_Upstream_StopDemoRecord( upstream_t *upstream, bool silent, bool cancel );

void TV_Upstream_StartDemo( upstream_t *upstream, const char *demoname, bool randomize );
void TV_Upstream_StopDemo( upstream_t *upstream );

#endif // __TV_UPSTREAM_DEMOS_H
