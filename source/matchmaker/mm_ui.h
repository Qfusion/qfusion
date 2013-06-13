/*
   Copyright (C) 2008 Will Franklin.

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

#ifndef _MM_UI_H_
#define _MM_UI_H_

typedef enum
{
	STATUS_DISCONNECTED,
	STATUS_CONNECTING,
	STATUS_CONNECTED,
	STATUS_MATCHMAKING
} mm_status_t;

typedef enum
{
	ACTION_CONNECT,
	ACTION_DISCONNECT,
	ACTION_GETCLIENTS,
	ACTION_GETCHANNELS,
	ACTION_ADDMATCH,
	ACTION_JOIN,
	ACTION_DROP,
	ACTION_CHAT,
	ACTION_STATUS
} mm_action_t;

#endif
