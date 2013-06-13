/*
   Copyright (C) 2007 Will Franklin

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

// this file cuts out all the server stuff. we dont need it!

#include "../qcommon/qcommon.h"

void SV_Init( void )
{
}

void SV_Shutdown( const char *finalmsg )
{
}

void SV_ShutdownGame( const char *finalmsg, qboolean reconnect )
{
}

void SV_Frame( int realmsec, int gamemsec )
{
}

qboolean SV_SendMessageToClient( struct client_s *client, msg_t *msg )
{
	return qtrue;
}

void SV_ParseClientMessage( struct client_s *client, msg_t *msg )
{
}
