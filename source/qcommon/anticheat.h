/*
Copyright (C) 2008 Chasseur de bots

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
#ifndef AC_INTERFACE_H
#define AC_INTERFACE_H

#include "../qcommon/qcommon.h"

typedef struct ac_import_s
{
	bool (*SV_SendMessageToClient)( void *client, msg_t *msg );
	void (*SV_ParseClientMessage)( void *client, msg_t *msg );
	void (*CL_ParseServerMessage)( msg_t *msg );
	void (*CL_Netchan_Transmit)( msg_t *msg );

	void (*MSG_Init)( msg_t *buf, uint8_t *data, size_t length );
	void (*MSG_Clear)( msg_t *buf );
	void *(*MSG_GetSpace)( msg_t *buf, size_t length );
	void (*MSG_WriteData)( msg_t *msg, const void *data, size_t length );
	void (*MSG_CopyData)( msg_t *buf, const void *data, size_t length );
	void (*MSG_WriteChar)( msg_t *sb, int c );
	void (*MSG_WriteByte)( msg_t *sb, int c );
	void (*MSG_WriteShort)( msg_t *sb, int c );
	void (*MSG_WriteInt3)( msg_t *sb, int c );
	void (*MSG_WriteLong)( msg_t *sb, int c );
	void (*MSG_WriteFloat)( msg_t *sb, float f );
	void (*MSG_WriteString)( msg_t *sb, const char *s );
	void (*MSG_WriteDeltaUsercmd)( msg_t *sb, struct usercmd_s *from, struct usercmd_s *cmd );
	void (*MSG_WriteDeltaEntity)( struct entity_state_s *from, struct entity_state_s *to, msg_t *msg, bool force, bool newentity );
	void (*MSG_WriteDir)( msg_t *sb, vec3_t vector );
	void (*MSG_BeginReading)( msg_t *sb );
	int (*MSG_ReadChar)( msg_t *msg );
	int (*MSG_ReadByte)( msg_t *msg );
	int (*MSG_ReadShort)( msg_t *sb );
	int (*MSG_ReadInt3)( msg_t *sb );
	int (*MSG_ReadLong)( msg_t *sb );
	float (*MSG_ReadFloat)( msg_t *sb );
	char *(*MSG_ReadString)( msg_t *sb );
	char *(*MSG_ReadStringLine)( msg_t *sb );
	void (*MSG_ReadDeltaUsercmd)( msg_t *sb, struct usercmd_s *from, struct usercmd_s *cmd );
	void (*MSG_ReadDir)( msg_t *sb, vec3_t vector );
	void (*MSG_ReadData)( msg_t *sb, void *buffer, size_t length );
	int (*MSG_SkipData)( msg_t *sb, size_t length );

	void *imports;
	void *exports;
} ac_import_t;

/*
typedef struct ac_export_s {
	// impulZ: this is not as easy as it seems learn_more =)
} ac_export_t;
*/

#endif
