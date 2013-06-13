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

#ifndef __TVM_MAIN_H
#define __TVM_MAIN_H

#include "tvm_local.h"

int TVM_API( void );
void TVM_Init( const char *game, unsigned int maxclients );
void TVM_Shutdown( void );
tvm_relay_t *TVM_InitRelay( relay_t *relay_server, unsigned int snapFrameTime, int playernum );
void TVM_ShutdownRelay( tvm_relay_t *relay );

#endif // __TVM_MAIN_H
