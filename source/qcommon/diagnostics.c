/*
Copyright (C) 2020 Victor Luchits

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

#include "qcommon.h"
#include "../qalgo/q_trie.h"
#include "../angelwrap/qas_public.h"

#define MAX_DIAGNOSTICS_CONNECTIONS 10

enum DiagMessageType {
	Diagnostics,
	RequestDebugDatabase,
	DebugDatabase,

    StartDebugging,
	StopDebugging,
	Pause,
	Continue,

	RequestCallStack,
	CallStack,

	ClearBreakpoints,
	SetBreakpoint,

	HasStopped,
	HasContinued,

	StepOver,
	StepIn,
	StepOut,

	EngineBreak,

	RequestVariables,
	Variables,

	RequestEvaluate,
	Evaluate,
	GoToDefinition,

	BreakOptions,
	RequestBreakFilters,
	BreakFilters,

	Disconnect,
};

typedef struct {
	int	  severity;
	int	  line, col;
	char *text;
} diag_message_t;

typedef struct {
	unsigned			num_messages;
	diag_message_t *messages;
} diag_messagelist_t;

typedef struct diag_connection_s {
	bool	 open;
	socket_t socket;
	netadr_t address;
	int64_t	 last_active;
	bool	 debugging;

	qstreambuf_t recvbuf, sendbuf;

	struct diag_connection_s *next, *prev;
} diag_connection_t;

static socket_t diag_sock;
static bool		diag_initialized;
static trie_t	*diag_messages;
static int		diag_connected;
static bool		diag_stopped;

static diag_connection_t diag_connections[MAX_DIAGNOSTICS_CONNECTIONS];
static diag_connection_t diag_connection_headnode, *com_free_diag_connections;

static cvar_t *com_diagnostics;
static cvar_t *com_diagnostics_port;
static cvar_t *com_diagnostics_timeout;

static void Diag_Unicast( qstreambuf_t *sb, qstreambuf_t *stream );

static diag_connection_t *Diag_AllocConnection( void )
{
	diag_connection_t *con;

	if( com_free_diag_connections ) {
		// take a free connection if possible
		con = com_free_diag_connections;
		com_free_diag_connections = con->next;
	} else {
		return NULL;
	}

	con->debugging = false;
	QStreamBuf_Init( &con->recvbuf );
	QStreamBuf_Init( &con->sendbuf );

	// put at the start of the list
	con->prev = &diag_connection_headnode;
	con->next = diag_connection_headnode.next;
	con->next->prev = con;
	con->prev->next = con;

	diag_connected++;

	return con;
}

static void Diag_FreeConnection( diag_connection_t *con )
{
	diag_connected--;
	if( diag_connected == 0 ) {
		Com_asDiag_Pause( false );
	}

	con->recvbuf.clear( &con->recvbuf );
	con->sendbuf.clear( &con->sendbuf );

	// remove from linked active list
	con->prev->next = con->next;
	con->next->prev = con->prev;

	// insert into linked free list
	con->next = com_free_diag_connections;
	com_free_diag_connections = con;
}

static void Diag_InitSocket( const char *addrstr, netadrtype_t adrtype, socket_t *socket )
{
	netadr_t address;

	address.type = NA_NOTRANSMIT;

	NET_StringToAddress( addrstr, &address );
	NET_SetAddressPort( &address, com_diagnostics_port->integer );

	if( address.type == adrtype ) {
		if( !NET_OpenSocket( socket, SOCKET_TCP, &address, true ) ) {
			Com_Printf( "Error: Couldn't open TCP socket: %s", NET_ErrorString() );
		} else if( !NET_Listen( socket ) ) {
			Com_Printf( "Error: Couldn't listen to TCP socket: %s", NET_ErrorString() );
			NET_CloseSocket( socket );
		} else {
			Com_Printf( "Diagnostics server started on %s\n", NET_AddressToString( &address ) );
		}
	}
}

static void Diag_Listen( socket_t *socket )
{
	int					  ret;
	socket_t			  newsocket = { 0 };
	netadr_t			  newaddress;

	// accept new connections
	while( ( ret = NET_Accept( socket, &newsocket, &newaddress ) ) ) {
		diag_connection_t *con = NULL;

		if( ret == -1 ) {
			Com_Printf( "NET_Accept: Error: %s\n", NET_ErrorString() );
			continue;
		}

		if( NET_IsLocalAddress( &newaddress ) ) {
			Com_DPrintf( "Diag connection accepted from %s\n", NET_AddressToString( &newaddress ) );
			con = Diag_AllocConnection();
		}

		if( !con ) {
			Com_DPrintf( "Diag connection refused for %s\n", NET_AddressToString( &newaddress ) );
			NET_CloseSocket( &newsocket );
			continue;
		}

		con->socket = newsocket;
		con->address = newaddress;
		con->last_active = Sys_Milliseconds();
		con->open = true;
	}
}

static void Diag_ReadFromSocket( socket_t *socket, diag_connection_t *con )
{
	uint8_t			   recvbuf[1024];
	size_t			   recvbuf_size = sizeof( recvbuf );
	size_t			   total_received = 0;
	qstreambuf_t *	   rb = &con->recvbuf, *sb = &con->sendbuf;
	qstreambuf_t	   resp;

	while( !Com_asDiag_PeekMessage( rb ) ) {
		int read = NET_Get( &con->socket, NULL, recvbuf, recvbuf_size );
		if( read < 0 ) {
			con->open = false;
			Com_DPrintf( "Diag connection recv error from %s\n", NET_AddressToString( &con->address ) );
			return;
		}

		if( read == 0 ) {
			con->open = total_received != 0;
			return;
		}

		total_received += read;

		rb->write( rb, recvbuf, read );
	}

	QStreamBuf_Init( &resp );

	while( Com_asDiag_PeekMessage( rb ) ) {
		Com_asDiag_ReadMessage( rb, &resp );
	}
	rb->compact( rb );

	if( resp.datalength( &resp ) > 0 ) {
		Diag_Unicast( sb, &resp );
	}

	resp.clear( &resp );
}

static void Diag_WriteToSocket( socket_t *socket, diag_connection_t *con )
{
	size_t total_sent = 0;
	qstreambuf_t *sb = &con->sendbuf;

	while( sb->datalength( sb ) > 0 ) {
		int sent = NET_Send( &con->socket, sb->data( sb ), sb->datalength( sb ), &con->address );
		if( sent < 0 ) {
			con->open = false;
			Com_DPrintf( "Diag connection send error from %s\n", NET_AddressToString( &con->address ) );
			return;
		}

		if( sent == 0 ) {
			con->open = total_sent == 0;
			break;
		}

		total_sent += sent;
		sb->consume( sb, sent );
	}

	if( !con->open )
		return;

	sb->compact( sb );
}

static void Diag_ExceptionOnSocket( socket_t *socket, diag_connection_t *con ) {
}

static void Diag_Unicast( qstreambuf_t *sb, qstreambuf_t *stream )
{
	sb->write( sb, stream->data( stream ), stream->datalength( stream ) );
}

void Diag_Broadcast( qstreambuf_t *stream )
{
	diag_connection_t *con, *next, *hnode = &diag_connection_headnode;

	if( !diag_initialized )
		return;

	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		Diag_Unicast( &con->sendbuf, stream );
	}
}

void Diag_Init( void ) {
	unsigned int i;

	diag_initialized = false;
	diag_stopped = false;
	diag_connected = 0;

#ifdef PUBLIC_BUILD
	com_diagnostics = Cvar_Get( "com_diagnostics", "0", 0 );
#else
	com_diagnostics = Cvar_Get( "com_diagnostics", "1", 0 );
#endif

	com_diagnostics_port = Cvar_Get( "com_diagnostics_port", "28099", 0 );
	com_diagnostics_timeout = Cvar_Get( "com_diagnostics_timeout", "900", 0 );

	memset( diag_connections, 0, sizeof( diag_connections ) );

	if( !com_diagnostics->integer ) {
		return;
	}

	com_free_diag_connections = diag_connections;
	diag_connection_headnode.prev = &diag_connection_headnode;
	diag_connection_headnode.next = &diag_connection_headnode;
	for( i = 0; i < MAX_DIAGNOSTICS_CONNECTIONS - 2; i++ ) {
		diag_connections[i].next = &diag_connections[i + 1];
	}

	Diag_InitSocket( "", NA_IP, &diag_sock );

	diag_initialized = ( diag_sock.address.type == NA_IP );
}

void Diag_RunFrame( void ) {
	diag_connection_t *con, *next, *hnode = &diag_connection_headnode;
	socket_t *			  sockets[MAX_DIAGNOSTICS_CONNECTIONS + 1];
	void *				  connections[MAX_DIAGNOSTICS_CONNECTIONS];
	int					  num_sockets = 0;

	if( !diag_initialized ) {
		return;
	}

	if( diag_sock.address.type == NA_IP ) {
		Diag_Listen( &diag_sock );
	}

	// handle incoming data
	num_sockets = 0;
	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		sockets[num_sockets] = &con->socket;
		connections[num_sockets] = con;
		num_sockets++;
	}
	sockets[num_sockets] = NULL;

	if( num_sockets != 0 ) {
		NET_Monitor( 0, sockets, 
			(void ( * )( socket_t *, void * ))Diag_ReadFromSocket,
			(void ( * )( socket_t *, void * ))Diag_WriteToSocket,
			(void ( * )( socket_t *, void * ))Diag_ExceptionOnSocket,
			connections );
	} else {
		if( diag_sock.address.type == NA_IP ) {
			sockets[num_sockets++] = &diag_sock;
		}
		sockets[num_sockets] = NULL;
		NET_Sleep( 0, sockets );
	}

	// close dead connections
	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;

		if( con->open ) {
			int64_t timeout = com_diagnostics_timeout->integer;

			if( Sys_Milliseconds() > con->last_active + timeout * 1000 ) {
				con->open = false;
				Com_DPrintf( "Diag connection timeout from %s\n", NET_AddressToString( &con->address ) );
			}
		}

		if( !con->open ) {
			NET_CloseSocket( &con->socket );
			Diag_FreeConnection( con );
		}
	}
}

void Diag_Shutdown( void ) {
	diag_connection_t *con, *next, *hnode;

	// close dead connections
	hnode = &diag_connection_headnode;
	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		if( con->open ) {
			NET_CloseSocket( &con->socket );
			Diag_FreeConnection( con );
		}
	}

	diag_initialized = false;
	diag_stopped = false;
	diag_connected = 0;
}
