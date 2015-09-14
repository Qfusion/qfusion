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

#include "tv_upstream.h"

#include "tv_upstream_oob.h"
#include "tv_upstream_parse.h"
#include "tv_upstream_demos.h"
#include "tv_downstream.h"

#include <setjmp.h>

// for jumping over upstream handling when it's disconnected
jmp_buf upstream_abortframe;

/*
* TV_UpstreamForText
* Finds relay upstream matching given text
* Text can be either number, name or address
*/
bool TV_UpstreamForText( const char *text, upstream_t **upstream )
{
	int i;
	static char cleanText[MAX_STRING_CHARS];

	assert( text );
	assert( upstream );

	// strip color tokens
	Q_strncpyz( cleanText, COM_RemoveColorTokens( text ), sizeof( cleanText ) );
	text = cleanText;

	*upstream = NULL;

	// lobby
	if( !Q_stricmp( text, "lobby" ) || ( !strcmp( text, va( "%i", atoi( text ) ) ) && atoi( text ) == 0 ) )
	{
		*upstream = NULL;
		return true;
	}

	// by number
	if( !strcmp( text, va( "%i", atoi( text ) ) ) && atoi( text ) > 0 && atoi( text ) <= tvs.numupstreams &&
		tvs.upstreams[atoi( text )-1] )
	{
		*upstream = tvs.upstreams[atoi( text )-1];
		return true;
	}

	// by name
	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		if( !Q_stricmp( text, COM_RemoveColorTokens( tvs.upstreams[i]->name ) ) )
		{
			*upstream = tvs.upstreams[i];
			return true;
		}
	}

	// by servername
	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		if( !Q_stricmp( text, tvs.upstreams[i]->servername ) )
		{
			*upstream = tvs.upstreams[i];
			return true;
		}
	}

	// by address
	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			continue;

		if( !strcmp( text, NET_AddressToString( &tvs.upstreams[i]->serveraddress ) ) )
		{
			*upstream = tvs.upstreams[i];
			return true;
		}
	}

	// nothing found
	return false;
}

/*
* TV_Upstream_Userinfo
*/
char *TV_Upstream_Userinfo( upstream_t *upstream )
{
	char *name;
	char *userinfo;
	relay_t *relay;
	int i, count;

	if( !upstream->userinfo )
		upstream->userinfo = ( char * )Mem_Alloc( upstream->mempool, 
		sizeof( *upstream->userinfo ) * MAX_INFO_STRING );

	userinfo = upstream->userinfo;

	// override the name value
	relay = &upstream->relay;
	if( relay->state >= CA_HANDSHAKE && relay->num_active_specs )
		name = va( "%s" S_COLOR_WHITE "/%u", tv_name->string, relay->num_active_specs );
	else
		name = tv_name->string;
	Info_SetValueForKey( userinfo, "name", name );

	// override password
	Info_SetValueForKey( userinfo, "password", upstream->password ? upstream->password : "" );

	// send self port
	Info_SetValueForKey( userinfo, "tv_port", va( "%hu", NET_GetAddressPort( &tvs.address ) ) );
	Info_SetValueForKey( userinfo, "tv_port6", va( "%hu", NET_GetAddressPort( &tvs.addressIPv6 ) ) );

	// send the number of connected clients and the maximum number of clients
	count = 0;
	for( i = 0; i < tv_maxclients->integer; i++ )
		if( tvs.clients[i].state >= CS_CONNECTED )
			count++;
	Info_SetValueForKey( userinfo, "num_cl", va( "%i", count ) );
	Info_SetValueForKey( userinfo, "max_cl", va( "%i", tv_maxclients->integer ) );

	Info_SetValueForKey( userinfo, "chan", va( "%i", upstream->number + 1 ) );

	return userinfo;
}

/*
* TV_Upstream_Netchan_Transmit
*/
static void TV_Upstream_Netchan_Transmit( upstream_t *upstream, msg_t *msg )
{
	//int zerror;

	// if we got here with unsent fragments, fire them all now
	Netchan_PushAllFragments( &upstream->netchan );

	// do not enable client compression until I fix the compression+fragmentation rare case bug
	/*if( cl_compresspackets->integer ) {
	zerror = Netchan_CompressMessage( msg );
	if( zerror < 0 ) {  // it's compression error, just send uncompressed
	Com_DPrintf( "TV_Upstream_Netchan_Transmit (ignoring compression): Compression error %i\n", zerror );
	}
	}*/

	Netchan_Transmit( &upstream->netchan, msg );
	upstream->lastPacketSentTime = tvs.realtime;
}

/*
* TV_Upstream_ProcessPacket
*/
static bool TV_Upstream_ProcessPacket( netchan_t *netchan, msg_t *msg )
{
	/*int sequence, sequence_ack;*/
	int zerror;

	if( !Netchan_Process( netchan, msg ) )
		return false; // wasn't accepted for some reason

	// now if compressed, expand it
	MSG_BeginReading( msg );
	/*sequence = */MSG_ReadLong( msg );
	/*sequence_ack = */MSG_ReadLong( msg );
	if( msg->compressed )
	{
		zerror = Netchan_DecompressMessage( msg );
		if( zerror < 0 )
		{          // compression error. Drop the packet
			Com_Printf( "Compression error %i. Dropping packet\n", zerror );
			return false;
		}
	}

	return true;
}

/*
* TV_Upstream_WriteUcmdToMessage
*/
static void TV_Upstream_WriteUcmdToMessage( upstream_t *upstream, msg_t *msg )
{
	usercmd_t cmd, nullcmd;

	memset( &nullcmd, 0, sizeof( usercmd_t ) );

	memset( &cmd, 0, sizeof( usercmd_t ) );
	cmd.serverTimeStamp = upstream->serverTime;

	MSG_WriteByte( msg, clc_move );
	MSG_WriteLong( msg, upstream->serverFrame );
	MSG_WriteLong( msg, upstream->framenum );
	MSG_WriteByte( msg, 1 );

	MSG_WriteDeltaUsercmd( msg, &nullcmd, &cmd );
}

/*
* TV_Upstream_SendMessagesToServer
*/
static void TV_Upstream_SendMessagesToServer( upstream_t *upstream, bool sendNow )
{
	msg_t message;
	uint8_t messageData[MAX_MSGLEN];
	bool ucmd = false;

	if( upstream->demo.playing )
		return;
	if( upstream->state == CA_DISCONNECTED || upstream->state == CA_CONNECTING )
		return;

	MSG_Init( &message, messageData, sizeof( messageData ) );
	MSG_Clear( &message );

	if( sendNow || tvs.realtime > upstream->lastPacketSentTime + ( upstream->state < CA_ACTIVE ? 100 : 40 ) )
	{
		// write up the clc commands
		if( upstream->state == CA_ACTIVE )
		{
			if( sendNow || tvs.realtime > upstream->lastUcmdTime + 40 )
			{
				ucmd = true;
				upstream->lastUcmdTime = tvs.realtime;
				TV_Upstream_WriteUcmdToMessage( upstream, &message );
			}
		}

		if( sendNow || tvs.realtime > upstream->lastPacketSentTime + 100 || ucmd )
		{
			if( !upstream->reliable )
			{
				MSG_WriteByte( &message, clc_svcack );
				MSG_WriteLong( &message, (unsigned int)upstream->lastExecutedServerCommand );
			}
		}

		// send a userinfo update if needed
		if( upstream->userinfo_modified )
		{
			upstream->userinfo_modified = false;
			//Com_Printf( "modified!\n" );
			TV_Upstream_AddReliableCommand( upstream, va( "usri \"%s\"", TV_Upstream_Userinfo( upstream ) ) );
		}

		// reliable commands
		TV_Upstream_UpdateReliableCommandsToServer( upstream, &message );

		// only send empty packets to prevent timing out when we are reliable
		// and don't have nothing to send for a long time
		if( message.cursize > 0 || tvs.realtime > upstream->lastPacketSentTime + 1000 )
			TV_Upstream_Netchan_Transmit( upstream, &message );
	}
}

/*
* TV_Upstream_CheckForResend
*/
static void TV_Upstream_CheckForResend( upstream_t *upstream )
{
	if( upstream->state != CA_CONNECTING )
		return;

	// resend if we haven't gotten a reply yet
	if( !upstream->reliable )
	{
		if( tvs.realtime - upstream->connect_time < 3000 )
			return;

		upstream->connect_count++;
		upstream->connect_time = tvs.realtime; // for retransmit requests

		Com_Printf( "%s" S_COLOR_WHITE ": Connecting...\n", upstream->name );

		Netchan_OutOfBandPrint( upstream->socket, &upstream->serveraddress, "getchallenge\n" );
	}
	else
	{
		if( tvs.realtime - upstream->connect_time < 3000 )
			return;

#ifdef TCP_ALLOW_TVCONNECT
		if( upstream->socket->type == SOCKET_TCP && !upstream->socket->connected )
		{
			connection_status_t status;

			if( !upstream->connect_count )
			{
				Com_Printf( "%s" S_COLOR_WHITE ": Connecting...\n", upstream->name );
				status = NET_Connect( upstream->socket, &upstream->serveraddress );
			}
			else
			{
				status = NET_CheckConnect( upstream->socket );
			}

			upstream->connect_count++;
			upstream->connect_time = tvs.realtime;

			if( status == CONNECTION_FAILED )
				TV_Upstream_Error( upstream, "Upstream failed: %s", NET_ErrorString() );

			if( status == CONNECTION_INPROGRESS )
				return;

			Netchan_OutOfBandPrint( upstream->socket, &upstream->serveraddress, "getchallenge\n" );
			return;
		}
#endif // TCP_ALLOW_TVCONNECT

		if( tvs.realtime - upstream->connect_time < 10000 )
			return;

		TV_Upstream_Error( upstream, "Upstream timed out" );
	}
}

/*
* TV_Upstream_SavePacket
*/
void TV_Upstream_SavePacket( upstream_t *upstream, msg_t *msg, int timeBias )
{
	packet_t *packet;

	assert( upstream );
	assert( msg && msg->cursize && msg->cursize < MAX_MSGLEN );

	if( upstream->packetqueue_discarded )
	{
		packet = upstream->packetqueue_discarded;
		upstream->packetqueue_discarded = upstream->packetqueue_discarded->next;
		if( packet->msg.maxsize && packet->msg.maxsize < msg->cursize )
		{
			Mem_Free( packet->msg.data );
			packet->msg.data = NULL;
			packet->msg.maxsize = 0;
		}
	}
	else
	{
		packet = Mem_Alloc( upstream->mempool, sizeof( packet_t ) );
	}

	packet->time = tvs.realtime + timeBias;
	packet->next = NULL;

	if( !packet->msg.maxsize )
	{
		packet->msg.maxsize = msg->cursize;
		packet->msg.data = Mem_Alloc( upstream->mempool, packet->msg.maxsize );
	}

	MSG_Clear( &packet->msg );
	MSG_CopyData( &packet->msg, msg->data, msg->cursize );

	if( !upstream->packetqueue )
		upstream->packetqueue = packet;
	if( upstream->packetqueue_head )
		upstream->packetqueue_head->next = packet;
	upstream->packetqueue_head = packet;
}

/*
* TV_Upstream_FreePackets
*/
static void TV_Upstream_FreePackets( upstream_t *upstream )
{
	packet_t *stop, *iter;

	if( upstream->relay.state == CA_UNINITIALIZED )
	{
		stop = upstream->packetqueue_head;
	}
	else
	{
		if( upstream->relay.packetqueue_pos )
			stop = upstream->relay.packetqueue_pos;
		else
			stop = upstream->packetqueue;
	}

	iter = upstream->packetqueue;
	while( iter != stop )
	{
		upstream->packetqueue = iter->next;

		if( upstream->state == CA_DISCONNECTED )
		{
			if( iter->msg.maxsize )
				Mem_Free( iter->msg.data );
			Mem_Free( iter );
		}
		else
		{
			iter->next = upstream->packetqueue_discarded;
			upstream->packetqueue_discarded = iter;
		}

		iter = upstream->packetqueue;
	}
}

/*
* TV_Upstream_ReadDemoMessage
* Read a packet from the demo file and send it to the messages parser
*/
static void TV_Upstream_ReadDemoMessage( upstream_t *upstream, int timeBias )
{
	static uint8_t msgbuf[MAX_MSGLEN];
	static msg_t demomsg;
	bool init = true;
	int read;

	if( !upstream->demo.filehandle )
	{
		TV_Upstream_Error( upstream, "No demo file handle" );
		return;
	}

	if( upstream->demo.filelen <= 0 )
	{
		TV_Upstream_StartDemo( upstream, upstream->servername, upstream->demo.random );
		return;
	}

	if( init )
	{
		MSG_Init( &demomsg, msgbuf, sizeof( msgbuf ) );
		init = false;
	}

	read = SNAP_ReadDemoMessage( upstream->demo.filehandle, &demomsg );
	if( read == -1 )
	{
		TV_Upstream_StartDemo( upstream, upstream->servername, upstream->demo.random );
		return;
	}

	upstream->demo.filelen -= read;

	TV_Upstream_SavePacket( upstream, &demomsg, timeBias );
	TV_Upstream_ParseServerMessage( upstream, &demomsg );
}

/*
* TV_Upstream_ReadDemoPackets
* See if it's time to read a new demo packet
*/
static void TV_Upstream_ReadDemoPackets( upstream_t *upstream )
{
	unsigned int timeBias;
	unsigned int readCount = 0xFFFF;
	unsigned int prevSnapFrameTime;
	connstate_t prevState;

	if( tvs.realtime < upstream->lastPacketReceivedTime + 1000 )
		return;

	prevState = upstream->state;
	prevSnapFrameTime = upstream->snapFrameTime;

	// read (1000/snapFrameTime) packets each second for smooth playback
	timeBias = 0;
	if( prevSnapFrameTime )
		readCount = (tvs.realtime - upstream->lastPacketReceivedTime) / prevSnapFrameTime;

	while( readCount-- )
	{
		TV_Upstream_ReadDemoMessage( upstream, timeBias );

		// message from next queued demo file
		if( upstream->state < prevState )
			break;

		// if we didn't know the snapFrameTime until now (demo start),
		// calculate the readCount
		if( !prevSnapFrameTime && upstream->snapFrameTime )
		{
			prevSnapFrameTime = upstream->snapFrameTime;
			readCount = 1000 / prevSnapFrameTime;
		}
		timeBias += upstream->snapFrameTime;
	}

	upstream->lastPacketReceivedTime = tvs.realtime;
}

/*
* TV_Upstream_ReadPackets
*/
static void TV_Upstream_ReadPackets( upstream_t *upstream )
{
	msg_t msg;
	uint8_t msgData[MAX_MSGLEN];
	int ret;
	netadr_t address;

	assert( upstream->state > CA_UNINITIALIZED );

#ifdef TCP_ALLOW_TVCONNECT
	if( upstream->socket->type == SOCKET_TCP && !upstream->socket->connected )
		return;
#endif

	MSG_Init( &msg, msgData, sizeof( msgData ) );
	MSG_Clear( &msg );

	while( ( ret = NET_GetPacket( upstream->socket, &address, &msg ) ) != 0 )
	{
		if( ret == -1 )
			TV_Upstream_Error( upstream, "Error receiving packet: %s", NET_ErrorString() );

		if( !NET_CompareAddress( &upstream->serveraddress, &address ) )
			return;

		// remote command packet
		if( *(int *)msg.data == -1 )
		{
			TV_Upstream_ConnectionlessPacket( upstream, &msg );
			//TV_Upstream_SavePacket( upstream, &msg, 0 );
			continue;
		}

		if( upstream->state >= CA_HANDSHAKE )
		{
			//
			// packet from server
			//
			if( !TV_Upstream_ProcessPacket( &upstream->netchan, &msg ) )
				continue;

			TV_Upstream_SavePacket( upstream, &msg, 0 );
			TV_Upstream_ParseServerMessage( upstream, &msg );
			upstream->lastPacketReceivedTime = tvs.realtime;
#ifdef TCP_ALLOW_TVCONNECT
			// we might have just been disconnected
			if( upstream->socket->type == SOCKET_TCP && !upstream->socket->connected )
				break;
#endif
		}
	}

	// check timeout
	if( upstream->state >= CA_HANDSHAKE &&
		upstream->lastPacketReceivedTime + tv_timeout->value * 1000 < tvs.realtime )
	{
		if( ++upstream->timeoutcount > 5 )  // timeoutcount saves debugger
			TV_Upstream_Error( upstream, "Upstream timed out" );
	}
	else
	{
		upstream->timeoutcount = 0;
	}
}

/*
* TV_Upstream_SendConnectPacket
* We have gotten a challenge from the server, so try and connect
*/
void TV_Upstream_SendConnectPacket( upstream_t *upstream )
{
	upstream->userinfo_modified = false;

	Netchan_OutOfBandPrint( upstream->socket, &upstream->serveraddress, "connect %i %i %i \"%s\" %i\n",
		APP_PROTOCOL_VERSION, Netchan_GamePort(), upstream->challenge, TV_Upstream_Userinfo( upstream ), 1 );
}

/*
* TV_Upstream_Error
* Must only be called from inside TV_Upstream_Run
*/
void TV_Upstream_Error( upstream_t *upstream, const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	assert( upstream );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	TV_Upstream_Disconnect( upstream, "%s", msg );
	longjmp( upstream_abortframe, -1 );
}

/*
* TV_Upstream_Disconnect
*/
void TV_Upstream_Disconnect( upstream_t *upstream, const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	assert( upstream );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	Com_Printf( "%s" S_COLOR_WHITE ": Disconnected: %s\n", upstream->name, msg );

	if( upstream->state > CA_CONNECTING )
	{
		int i;

		for( i = 0; i < 3; i++ ) {
			TV_Upstream_AddReliableCommand( upstream, "disconnect" );
			TV_Upstream_SendMessagesToServer( upstream, true );
		}
	}

	if( upstream->individual_socket )
		NET_CloseSocket( upstream->socket );

	if( upstream->demo.recording )
		TV_Upstream_StopDemoRecord( upstream, false, false );

	if( upstream->demo.playing )
		TV_Upstream_StopDemo( upstream );

	upstream->state = CA_DISCONNECTED;
}

/*
* TV_Upstream_Shutdown
*/
void TV_Upstream_Shutdown( upstream_t *upstream, const char *format, ... )
{
	va_list	argptr;
	char msg[1024];
	int i;
	packet_t *remove;
	mempool_t *mempool;

	assert( upstream );
	assert( format );

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	if( upstream->relay.state != CA_UNINITIALIZED )
		TV_Relay_Shutdown( &upstream->relay, "Upstream shutting down" );

	if( upstream->state != CA_DISCONNECTED )
		TV_Upstream_Disconnect( upstream, "Upstream shutting down" );

	Com_Printf( "%s" S_COLOR_WHITE ": Shutdown: %s\n", upstream->name, msg );

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( tvs.upstreams[i] == upstream )
		{
			tvs.upstreams[i] = NULL;
			break;
		}
	}
	assert( i < tvs.numupstreams );

	mempool = upstream->mempool;

	while( upstream->packetqueue_discarded )
	{
		remove = upstream->packetqueue_discarded;
		upstream->packetqueue_discarded = remove->next;
		if( remove->msg.maxsize )
			Mem_Free( remove->msg.data );
		Mem_Free( remove );
	}

	while( upstream->packetqueue )
	{
		remove = upstream->packetqueue;
		upstream->packetqueue = remove->next;
		if( remove->msg.maxsize )
			Mem_Free( remove->msg.data );
		Mem_Free( remove );
	}

	if( upstream->password )
		Mem_Free( upstream->password );

	if( upstream->userinfo )
		Mem_Free( upstream->userinfo );

	if( upstream->servername )
		Mem_Free( upstream->servername );
	if( upstream->customname )
		Mem_Free( upstream->customname );
	if( upstream->backupname )
		Mem_Free( upstream->backupname );
	Mem_Free( upstream->name );
	Mem_Free( upstream );

	Mem_FreePool( &mempool );
}


/*
* TV_Upstream_ClearState
*/
void TV_Upstream_ClearState( upstream_t *upstream )
{
	upstream->lastExecutedServerCommand = 0;
	upstream->reliableAcknowledge = 0;
	upstream->reliableSequence = 0;
	upstream->reliableSent = 0;
	memset( upstream->reliableCommands, 0, sizeof( upstream->reliableCommands ) );

	upstream->lastPacketSentTime = 0;
	upstream->lastPacketReceivedTime = 0;
	upstream->lastUcmdTime = 0;

	upstream->framenum = 0;
	upstream->serverTime = 0;
	upstream->serverFrame = 0;
}

/*
* TV_Upstream_AddReliableCommand
*/
void TV_Upstream_AddReliableCommand( upstream_t *upstream, const char *cmd )
{
	int index;

	assert( upstream && upstream->state >= CA_HANDSHAKE );
	assert( cmd );

	if( upstream->reliableSequence > MAX_RELIABLE_COMMANDS + upstream->reliableAcknowledge )
	{
		// so we don't get recursive error from disconnect commands
		upstream->reliableAcknowledge = upstream->reliableSequence;
		TV_Upstream_Error( upstream, "Client command overflow" );
	}

	upstream->reliableSequence++;
	index = upstream->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( upstream->reliableCommands[index], cmd, sizeof( upstream->reliableCommands[index] ) );
}

/*
* TV_Upstream_UpdateReliableCommandsToServer
* Add the pending commands to the message
*/
void TV_Upstream_UpdateReliableCommandsToServer( upstream_t *upstream, msg_t *msg )
{
	unsigned int i;

	// write any unacknowledged clientCommands
	for( i = upstream->reliableAcknowledge + 1; i <= upstream->reliableSequence; i++ )
	{
		if( !strlen( upstream->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )] ) )
			continue;

		MSG_WriteByte( msg, clc_clientcommand );
		if( !upstream->reliable )
			MSG_WriteLong( msg, i );
		MSG_WriteString( msg, upstream->reliableCommands[i & ( MAX_RELIABLE_COMMANDS-1 )] );
	}

	upstream->reliableSent = upstream->reliableSequence;
	if( upstream->reliable )
		upstream->reliableAcknowledge = upstream->reliableSent;
}

/*
* TV_Upstream_Run
*/
void TV_Upstream_Run( upstream_t *upstream, int msec )
{
	if( setjmp( upstream_abortframe ) )  // disconnect while running
		return;

	if( upstream->state > CA_DISCONNECTED )
	{
		if( upstream->demo.playing )
		{
			TV_Upstream_ReadDemoPackets( upstream );
			if( upstream->state == CA_ACTIVE )
				upstream->framenum++;
		}
		else
		{
			TV_Upstream_ReadPackets( upstream );

			if( upstream->state == CA_CONNECTING )
				TV_Upstream_CheckForResend( upstream );
			else if( upstream->state == CA_ACTIVE )
				upstream->framenum++;

			if( upstream->netchan.unsentFragments )
				Netchan_TransmitNextFragment( &upstream->netchan );
			else
				TV_Upstream_SendMessagesToServer( upstream, false );
		}
	}
	else
	{
		upstream->framenum = 0;
	}

	if( upstream->relay.state != CA_UNINITIALIZED )
	{
		TV_Relay_Run( &upstream->relay, msec );
		TV_Upstream_FreePackets( upstream );
	}

	if( upstream->relay.state == CA_UNINITIALIZED )
		TV_Upstream_Shutdown( upstream, "Relay was shutdown" );
}

/*
* TV_Upstream_SetName
*/
void TV_Upstream_SetName( upstream_t *upstream, const char *name )
{
	const char *customname = upstream->customname;

	assert( name && *name );

	// if name is different from custom name, store it anyways
	// in case the custom name is reset later
	if( name != customname )
	{
		const char *backupname = upstream->backupname;
		if( !backupname || strcmp( backupname, name ) )
		{
			if( backupname )
				Mem_TempFree( upstream->backupname );
			upstream->backupname = TV_Upstream_CopyString( upstream, name );
		}
	}

	// if custom name is set, override
	if( customname && *customname )
		name = customname;

	if( upstream->name )
	{
		if( !strcmp( upstream->name, name ) )
			return;
		Mem_Free( upstream->name );
	}
	upstream->name = TV_Upstream_CopyString( upstream, name );

	if( upstream->relay.state != CA_UNINITIALIZED )
		TV_Relay_NameNotify( &upstream->relay, NULL );
}

/*
* TV_Upstream_SetAudioTrack
*/
void TV_Upstream_SetAudioTrack( upstream_t *upstream, const char *track )
{
	char *oldtrack;

	oldtrack = upstream->audiotrack;
	if( !oldtrack ) {
		oldtrack = "";
	}

	if( !Q_stricmp( oldtrack, track ) ) {
		// no change
		return;
	}

	if( upstream->audiotrack ) {
		Mem_Free( upstream->audiotrack );
	}

	if( !track[0] ) {
		// no track override
		upstream->audiotrack = NULL;
	}
	else {
		upstream->audiotrack = TV_Upstream_CopyString( upstream, track );
	}

	TV_Relay_SetAudioTrack( &upstream->relay, upstream->audiotrack );
}

/*
* TV_Upstream_Connect
*/
void TV_Upstream_Connect( upstream_t *upstream, const char *servername, const char *password, socket_type_t type, netadr_t *address )
{
	netadr_t socketaddress;

	assert( upstream && upstream->state <= CA_DISCONNECTED );
	assert( servername );
	assert( address );

	if( upstream->demo.playing )
		return;

	switch( type )
	{
	case SOCKET_UDP:
		NET_InitAddress( &socketaddress, address->type );
		if( !NET_OpenSocket( &upstream->socket_real, SOCKET_UDP, &socketaddress, false ) )
		{
			Com_Printf( "Error: Couldn't open UDP socket: %s\n", NET_ErrorString() );
			return;
		}
		upstream->socket = &upstream->socket_real;
		upstream->reliable = false;
		upstream->individual_socket = true;
		break;

#ifdef TCP_ALLOW_TVCONNECT
	case SOCKET_TCP:
		NET_InitAddress( &socketaddress, address->type );
		if( !NET_OpenSocket( &upstream->socket_real, SOCKET_TCP, &socketaddress, false ) )
		{
			Com_Printf( "Error: Couldn't open TCP socket: %s\n", NET_ErrorString() );
			return;
		}
		NET_SetSocketNoDelay( &upstream->socket_real, 1 );
		upstream->socket = &upstream->socket_real;
		upstream->reliable = true;
		upstream->individual_socket = true;
		break;
#endif

	case SOCKET_LOOPBACK:
	default:
		assert( false );
	}

	upstream->serveraddress = *address;
	if( NET_GetAddressPort( &upstream->serveraddress ) == 0 )
		NET_SetAddressPort( &upstream->serveraddress, PORT_SERVER );

	if( upstream->servername )
		Mem_Free( upstream->servername );

	if( upstream->password )
	{
		Mem_Free( upstream->password );
		upstream->password = NULL;
	}

	if( upstream->userinfo )
	{
		Mem_Free( upstream->userinfo );
		upstream->userinfo = NULL;
	}

	upstream->servername = TV_Upstream_CopyString( upstream, servername );
	if( password && *password )
		upstream->password = TV_Upstream_CopyString( upstream, password );

	upstream->state = CA_CONNECTING;
	upstream->connect_time = -99999; // CL_CheckForResend() will fire immediately
	upstream->connect_count = 0;
	upstream->rejected = false;
	upstream->lastPacketReceivedTime = tvs.realtime; // reset the timeout limit
	upstream->multiview = false;
	upstream->precacheDone = false;
}

/*
* TV_Upstream_Reconnect_f
*/
void TV_Upstream_Reconnect_f( upstream_t *upstream )
{
	char *servername, *password;
	socket_type_t type;
	netadr_t address;

	assert( upstream );

	servername = TempCopyString( upstream->servername );
	password = upstream->password ? TempCopyString( upstream->password ) : NULL;

	type = upstream->socket->type;
	address = upstream->serveraddress;

	if( upstream->state > CA_CONNECTING )
	{
		int i;

		for( i = 0; i < 3; i++ ) {
			TV_Upstream_AddReliableCommand( upstream, "disconnect" );
			TV_Upstream_SendMessagesToServer( upstream, true );
		}
	}

	if( upstream->individual_socket )
		NET_CloseSocket( upstream->socket );

	if( upstream->demo.recording )
		TV_Upstream_StopDemoRecord( upstream, false, false );

	if( upstream->demo.playing )
		TV_Upstream_StopDemo( upstream );

	if( upstream->servername )
	{
		Mem_Free( upstream->servername );
		upstream->servername = NULL;
	}

	if( upstream->password )
	{
		Mem_Free( upstream->password );
		upstream->password = NULL;
	}

	if( upstream->userinfo )
	{
		Mem_Free( upstream->userinfo );
		upstream->userinfo = NULL;
	}

	TV_Upstream_ClearState( upstream );

	upstream->framenum = 0;
	upstream->state = CA_DISCONNECTED;

	TV_Upstream_Connect( upstream, servername, password, type, &address );

	Mem_TempFree( servername );
	Mem_TempFree( password );
}

/*
* TV_Upstream_New
*/
upstream_t *TV_Upstream_New( const char *servername, const char *customname, int delay )
{
	int i;
	upstream_t *upstream;
	mempool_t *mempool;

	for( i = 0; i < tvs.numupstreams; i++ )
	{
		if( !tvs.upstreams[i] )
			break;
	}

	if( i == tvs.numupstreams )
	{
		tvs.numupstreams++;
		if( !tvs.upstreams )
			tvs.upstreams = Mem_Alloc( tv_mempool, sizeof( upstream_t * ) * tvs.numupstreams );
		else
			tvs.upstreams = Mem_Realloc( tvs.upstreams, sizeof( upstream_t * ) * tvs.numupstreams );
		i = tvs.numupstreams-1;
	}

	mempool = Mem_AllocPool( tv_mempool, servername );

	tvs.upstreams[i] = Mem_Alloc( mempool, sizeof( upstream_t ) );
	upstream = tvs.upstreams[i];
	memset( upstream, 0, sizeof( *upstream ) );

	upstream->mempool = mempool;
	upstream->state = CA_DISCONNECTED;
	upstream->number = i;
	if( customname && *customname )
		upstream->customname = TV_Upstream_CopyString( upstream, customname );

	TV_Upstream_SetName( upstream, servername );

	TV_Relay_Init( &upstream->relay, upstream, delay );

	return upstream;
}
