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

#include "tv_local.h"

#include "tv_downstream_parse.h"

#include "tv_relay.h"
#include "tv_downstream.h"
#include "tv_downstream_clcmd.h"

/*
* TV_Downstream_ParseMoveCommand
*/
static void TV_Downstream_ParseMoveCommand( client_t *client, msg_t *msg )
{
	unsigned int i, ucmdHead, ucmdFirst;
	usercmd_t nullcmd;
	int lastframe, ucmdCount;

	lastframe = MSG_ReadLong( msg );

	// read the id of the first ucmd we will receive
	ucmdHead = MSG_ReadLong( msg );
	// read the number of ucmds we will receive
	ucmdCount = MSG_ReadByte( msg );

	if( ucmdCount > CMD_MASK )
	{
		TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "Ucmd overflow" );
		return;
	}

	ucmdFirst = ucmdHead - ucmdCount;
	client->UcmdReceived = ucmdHead < 1 ? 0 : ucmdHead - 1;

	// read the user commands
	for( i = ucmdFirst; i < ucmdHead; i++ )
	{
		if( i == ucmdFirst )
		{              // first one isn't delta compressed
			memset( &nullcmd, 0, sizeof( nullcmd ) );
			MSG_ReadDeltaUsercmd( msg, &nullcmd, &client->ucmds[i & CMD_MASK] );
		}
		else
		{
			MSG_ReadDeltaUsercmd( msg, &client->ucmds[( i-1 ) & CMD_MASK], &client->ucmds[i & CMD_MASK] );
		}
	}

	if( client->state != CS_SPAWNED )
	{
		client->lastframe = -1;
		return;
	}

	// calc ping
	if( lastframe != client->lastframe )
	{
		client->lastframe = lastframe;
		if( client->lastframe > 0 )
		{
			// this is more accurate. A little bit hackish, but more accurate
			client->frame_latency[client->lastframe&( LATENCY_COUNTS-1 )] = tvs.realtime - ( client->ucmds[client->UcmdReceived & CMD_MASK].serverTimeStamp + 50 ); // FIXME
		}
	}
}

/*
* TV_Downstream_ParseClientMessage
* The current message is parsed for the given client
*/
void TV_Downstream_ParseClientMessage( client_t *client, msg_t *msg )
{
	int c;
	unsigned cmdNum;
	char *s;

	assert( client );
	assert( msg );

	while( 1 )
	{
		if( msg->readcount > msg->cursize )
		{
			TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "bad message from client" );
			break;
		}

		c = MSG_ReadByte( msg );
		if( c == -1 )
		{
			break;
		}

		switch( c )
		{
		case clc_nop:
			break;

		case clc_move:
			TV_Downstream_ParseMoveCommand( client, msg );
			break;

		case clc_svcack:
			if( client->reliable )
			{
				TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "svack from reliable client" );
				return;
			}
			cmdNum = MSG_ReadLong( msg );
			if( cmdNum < client->reliableAcknowledge || cmdNum > client->reliableSent )
			{
				//					TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "bad server command acknowledged" );
				return;
			}
			client->reliableAcknowledge = cmdNum;
			break;

		case clc_clientcommand:
			if( !client->reliable )
			{
				cmdNum = MSG_ReadLong( msg );
				if( cmdNum <= client->clientCommandExecuted )
				{
					s = MSG_ReadString( msg ); // read but ignore
					continue;
				}
				client->clientCommandExecuted = cmdNum;
			}
			s = MSG_ReadString( msg );
			TV_Downstream_ExecuteUserCommand( client, s );
			if( client->state == CS_ZOMBIE )
				return; // disconnect command
			break;

		case clc_extension:
			if( 1 )
			{
				int ext, len;

				ext = MSG_ReadByte( msg );		// extension id
				MSG_ReadByte( msg );			// version number
				len = MSG_ReadShort( msg );		// command length

				switch( ext )
				{
				default:
					// unsupported
					MSG_SkipData( msg, len );
					break;
				}
			}
			break;

		default:
			TV_Downstream_DropClient( client, DROP_TYPE_GENERAL, "unknown command char" );
			break;
		}
	}
}
