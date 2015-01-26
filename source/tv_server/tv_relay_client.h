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

#ifndef __TV_RELAY_CLIENT_H
#define __TV_RELAY_CLIENT_H

#include "tv_local.h"

void TV_Relay_SendClientMessages( relay_t *relay );
void TV_Relay_ReconnectClients( relay_t *relay );
void TV_Relay_ClientUserinfoChanged( relay_t *relay, client_t *client );
void TV_Relay_ClientBegin( relay_t *relay, client_t *client );
void TV_Relay_ClientDisconnect( relay_t *relay, client_t *client );
bool TV_Relay_CanConnect( relay_t *relay, client_t *client, char *userinfo );
void TV_Relay_ClientConnect( relay_t *relay, client_t *client );
bool TV_Relay_ClientCommand_f( relay_t *relay, client_t *client );

void TV_Relay_BuildClientFrameSnap( relay_t *relay, client_t *client );

#endif // __TV_RELAY_CLIENT_H
