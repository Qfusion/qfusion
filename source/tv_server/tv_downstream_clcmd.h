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

#ifndef __TV_DOWNSTREAM_CLCMD_H
#define __TV_DOWNSTREAM_CLCMD_H

#include "tv_local.h"

void TV_Downstream_New_f( client_t *client );
void TV_Downstream_ExecuteUserCommand( client_t *client, char *s );

#endif // __TV_DOWNSTREAM_CLCMD_H
