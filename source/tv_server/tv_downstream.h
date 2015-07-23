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

#ifndef __TV_DOWNSTREAM_H
#define __TV_DOWNSTREAM_H

#include "tv_local.h"

#define	MAX_MASTERS 16 // max recipients for heartbeat packets
#define	HEARTBEAT_SECONDS 300

void TV_Downstream_Msg( client_t *client, relay_t *relay, client_t *who, bool chat, const char *format, ... );
void TV_Downstream_ClientResetCommandBuffers( client_t *client, bool resetReliable );
char *TV_Downstream_FixName( const char *orginal_name, client_t *client );
bool TV_Downstream_ChangeStream( client_t *client, relay_t *relay );
void TV_Downstream_AddGameCommand( relay_t *relay, client_t *client, const char *cmd );
void TV_Downstream_UserinfoChanged( client_t *cl );
void TV_Downstream_AddServerCommand( client_t *client, const char *cmd );
void TV_Downstream_SendServerCommand( client_t *cl, const char *format, ... );
void TV_Downstream_AddReliableCommandsToMessage( client_t *client, msg_t *msg );
void TV_Downstream_InitClientMessage( client_t *client, msg_t *msg, uint8_t *data, size_t size );
bool TV_Downstream_SendMessageToClient( client_t *client, msg_t *msg );
void TV_Downstream_DropClient( client_t *drop, int type, const char *format, ... );
void TV_Downstream_ReadPackets( void );
void TV_Downstream_CheckTimeouts( void );
bool TV_Downstream_SendClientsFragments( void );
void TV_Downstream_SendClientMessages( void );
void TV_Downstream_ExecuteClientThinks( relay_t *relay, client_t *client );
void TV_Downstream_InitMaster( void );
void TV_Downstream_MasterHeartbeat( void );
void TV_Downstream_MasterSendQuit( void );
bool TV_Downstream_IsMaster( const netadr_t *address, bool *isSteam );

#endif // __TV_DOWNSTREAM_H
