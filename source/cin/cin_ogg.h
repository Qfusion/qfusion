/*
Copyright (C) 2012 Victor Luchits

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

#ifndef _CIN_OGG_H_
#define _CIN_OGG_H_

#include "cin_local.h"

#define OGG_FILE_EXTENSIONS ".ogg .ogv"

qboolean Ogg_Init_CIN( cinematics_t *cin );
void Ogg_Shutdown_CIN( cinematics_t *cin );
void Ogg_Reset_CIN( cinematics_t *cin );
qboolean Ogg_NeedNextFrame_CIN( cinematics_t *cin );
qbyte *Ogg_ReadNextFrame_CIN( cinematics_t *cin, qboolean *redraw );

#endif
