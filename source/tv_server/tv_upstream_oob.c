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

#include "tv_upstream_oob.h"

#include "tv_upstream.h"

typedef struct
{
	char *name;
	void ( *func )( upstream_t *upstream, msg_t *msg );
} upstreamless_cmd_t;

/*
* TV_Upstream_Challenge
*/
static void TV_Upstream_Challenge_f( upstream_t *upstream, msg_t *msg )
{
	assert( upstream );

	// ignore if we get in the wrong time
	if( upstream->state != CA_CONNECTING )
		return;

	upstream->challenge = atoi( Cmd_Argv( 1 ) );
	upstream->connect_time = tvs.realtime;
	TV_Upstream_SendConnectPacket( upstream );
}

/*
* TV_Upstream_ClientConnectPacket
* ClientConnect in client code
*/
static void TV_Upstream_ClientConnectPacket( upstream_t *upstream, msg_t *msg )
{
	if( upstream->state != CA_CONNECTING )
		return;

	Netchan_Setup( &upstream->netchan, upstream->socket, &upstream->serveraddress, Netchan_GamePort() );
	upstream->state = CA_HANDSHAKE;
	TV_Upstream_AddReliableCommand( upstream, "new" );

	Com_Printf( "%s" S_COLOR_WHITE ": Connected\n", upstream->name );
}

/*
* TV_Upstream_Reject
*/
static void TV_Upstream_Reject_f( upstream_t *upstream, msg_t *msg )
{
	int rejecttype, rejectflag;
	char rejectmessage[MAX_STRING_CHARS];

	rejecttype = atoi( MSG_ReadStringLine( msg ) );
	if( rejecttype < 0 || rejecttype >= DROP_TYPE_TOTAL )
		rejecttype = DROP_TYPE_GENERAL;

	rejectflag = atoi( MSG_ReadStringLine( msg ) );

	Q_strncpyz( rejectmessage, MSG_ReadStringLine( msg ), sizeof( rejectmessage ) );

	Com_Printf( "%s" S_COLOR_WHITE ": Upstream refused: %s\n", upstream->name, rejectmessage );
	if( rejectflag & DROP_FLAG_AUTORECONNECT )
		Com_Printf( "Automatic reconnecting allowed.\n" );
	else
		Com_Printf( "Automatic reconnecting not allowed.\n" );

	TV_Upstream_Error( upstream, "Upstream refused: %s", rejectmessage );
}

/*
* List of commands
*/
static upstreamless_cmd_t upstream_upstreamless_cmds[] =
{
	{ "challenge", TV_Upstream_Challenge_f },
	{ "client_connect", TV_Upstream_ClientConnectPacket },
	{ "reject", TV_Upstream_Reject_f },
	{ NULL, NULL }
};

/*
* TV_Upstream_ConnectionlessPacket
*/
void TV_Upstream_ConnectionlessPacket( upstream_t *upstream, msg_t *msg )
{
	upstreamless_cmd_t *cmd;
	char *s, *c;

	MSG_BeginReading( msg );
	MSG_ReadLong( msg );    // skip the -1 marker

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );
	c = Cmd_Argv( 0 );

	for( cmd = upstream_upstreamless_cmds; cmd->name; cmd++ )
	{
		if( !strcmp( c, cmd->name ) )
		{
			cmd->func( upstream, msg );
			return;
		}
	}

	Com_DPrintf( "%s" S_COLOR_WHITE ": Bad upstream connectionless packet: %s\n", upstream->name, c );
}
