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

#define MAX_DIAGNOSTICS_CONNECTIONS 10

enum DiagMessageType {
	Diagnostics,
	RequestDebugDatabase,
	DebugDatabase,
};

typedef struct com_diag_connection_s {
	bool	 open;
	socket_t socket;
	netadr_t address;
	int64_t	 last_active;

	uint8_t *recvbuf, *sendbuf;
	size_t	 recvbuf_size, sendbuf_size;

	struct com_diag_connection_s *next, *prev;
} com_diag_connection_t;

static socket_t com_diagnostics_sock;
static bool		com_diag_initialized;

static com_diag_connection_t com_diag_connections[MAX_DIAGNOSTICS_CONNECTIONS];
static com_diag_connection_t com_diag_connection_headnode, *com_free_diag_connections;

static cvar_t *com_diagnostics;
static cvar_t *com_diagnostics_port;
static cvar_t *com_diagnostics_timeout;

static com_diag_connection_t *Com_Diag_AllocConnection( void )
{
	com_diag_connection_t *con;

	if( com_free_diag_connections ) {
		// take a free connection if possible
		con = com_free_diag_connections;
		com_free_diag_connections = con->next;
	} else {
		return NULL;
	}

	// put at the start of the list
	con->prev = &com_diag_connection_headnode;
	con->next = com_diag_connection_headnode.next;
	con->next->prev = con;
	con->prev->next = con;
	return con;
}

static void Com_Diag_FreeConnection( com_diag_connection_t *con )
{
	// remove from linked active list
	con->prev->next = con->next;
	con->next->prev = con->prev;

	// insert into linked free list
	con->next = com_free_diag_connections;
	com_free_diag_connections = con;
}

static void Com_Diag_InitSocket( const char *addrstr, netadrtype_t adrtype, socket_t *socket )
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
			Com_Printf( "Web server started on %s\n", NET_AddressToString( &address ) );
		}
	}
}

static void Com_Diag_Listen( socket_t *socket )
{
	int					  ret;
	socket_t			  newsocket = { 0 };
	netadr_t			  newaddress;
	com_diag_connection_t *con;

	// accept new connections
	while( ( ret = NET_Accept( socket, &newsocket, &newaddress ) ) ) {
		if( ret == -1 ) {
			Com_Printf( "NET_Accept: Error: %s\n", NET_ErrorString() );
			continue;
		}

		if( NET_IsLocalAddress( &newaddress ) ) {
			Com_DPrintf( "Diag connection accepted from %s\n", NET_AddressToString( &newaddress ) );
			con = Com_Diag_AllocConnection();
			if( !con ) {
				break;
			}
			con->socket = newsocket;
			con->address = newaddress;
			con->last_active = Sys_Milliseconds();
			con->open = true;
			continue;
		}

		Com_DPrintf( "Diag connection refused for %s\n", NET_AddressToString( &newaddress ) );
		NET_CloseSocket( &newsocket );
	}
}

static bool Com_Diag_PeekMessage( com_diag_connection_t *con ) {
	int len;
	msg_t msg;

	if( con->recvbuf_size < 5 ) {
		return false;
	}

	MSG_Init( &msg, con->recvbuf, con->recvbuf_size );
	MSG_GetSpace( &msg, con->recvbuf_size );

	len = MSG_ReadInt32( &msg );
	return msg.cursize - msg.readcount >= len;
}

static void Com_Diag_ReadMessage( com_diag_connection_t *con ) {
	int	  type, len;
	msg_t msg;

	if( con->recvbuf_size < 5 ) {
		assert( 0 );
		return;
	}

	MSG_Init( &msg, con->recvbuf, con->recvbuf_size );
	MSG_GetSpace( &msg, con->recvbuf_size );

	len = MSG_ReadInt32( &msg );
	type = MSG_ReadUint8( &msg );

	MSG_SkipData( &msg, len - 1 );

	memmove( con->recvbuf, con->recvbuf + msg.readcount, con->recvbuf_size - msg.readcount );
	con->recvbuf = Q_realloc( con->recvbuf, con->recvbuf_size - msg.readcount );
	con->recvbuf_size -= msg.readcount;
}

static void Com_Diag_ReceiveRequest( socket_t *socket, com_diag_connection_t *con )
{
	char 			   recvbuf[1024];
	size_t			   recvbuf_size = sizeof( recvbuf );
	size_t			   total_received = 0;

	while( !Com_Diag_PeekMessage( con ) ) {
		int read = NET_Get( &con->socket, NULL, recvbuf, recvbuf_size );
		if( read < 0 ) {
			con->open = false;
			Com_DPrintf( "HTTP connection recv error from %s\n", NET_AddressToString( &con->address ) );
			return;
		}

		if( read == 0 ) {
			con->open = total_received != 0;
			return;
		}

		total_received += read;

		con->recvbuf = Q_realloc( con->recvbuf, con->recvbuf_size + read );
		memcpy( con->recvbuf + con->recvbuf_size, recvbuf, read );
		con->recvbuf_size += read;
	}

	Com_Diag_ReadMessage( con );
}

static void Com_Diag_WriteResponse( socket_t *socket, com_diag_connection_t *con )
{

}

void Com_InitDiagnostics() {
	unsigned int i;

	com_diag_initialized = false;

#ifdef PUBLIC_BUILD
	com_diagnostics = Cvar_Get( "com_diagnostics", "0", 0 );
#else
	com_diagnostics = Cvar_Get( "com_diagnostics", "1", 0 );
#endif

	com_diagnostics_port = Cvar_Get( "com_diagnostics_port", "28099", 0 );
	com_diagnostics_timeout = Cvar_Get( "com_diagnostics_timeout", "900", 0 );

	memset( com_diag_connections, 0, sizeof( com_diag_connections ) );

	if( !com_diagnostics->integer ) {
		return;
	}

	com_free_diag_connections = com_diag_connections;
	com_diag_connection_headnode.prev = &com_diag_connection_headnode;
	com_diag_connection_headnode.next = &com_diag_connection_headnode;
	for( i = 0; i < MAX_DIAGNOSTICS_CONNECTIONS - 2; i++ ) {
		com_diag_connections[i].next = &com_diag_connections[i + 1];
	}

	Com_Diag_InitSocket( "", NA_IP, &com_diagnostics_sock );

	com_diag_initialized = ( com_diagnostics_sock.address.type == NA_IP );
}

void Com_RunDiagnosticsFrame() {
	com_diag_connection_t *con, *next, *hnode = &com_diag_connection_headnode;
	socket_t *			  sockets[MAX_DIAGNOSTICS_CONNECTIONS + 1];
	void *				  connections[MAX_DIAGNOSTICS_CONNECTIONS];
	int					  num_sockets = 0;

	if( !com_diag_initialized ) {
		return;
	}

	if( com_diagnostics_sock.address.type == NA_IP ) {
		Com_Diag_Listen( &com_diagnostics_sock );
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
			(void ( * )( socket_t *, void * ))Com_Diag_ReceiveRequest,
			(void ( * )( socket_t *, void * ))Com_Diag_WriteResponse, 
			NULL, connections );
	} else {
		if( com_diagnostics_sock.address.type == NA_IP ) {
			sockets[num_sockets++] = &com_diagnostics_sock;
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
			Com_Diag_FreeConnection( con );
		}
	}
}

void Com_ShutdownDiagnostics() {
	com_diag_connection_t *con, *next, *hnode;

	// close dead connections
	hnode = &com_diag_connection_headnode;
	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		if( con->open ) {
			NET_CloseSocket( &con->socket );
			Com_Diag_FreeConnection( con );
		}
	}

	com_diag_initialized = false;
}
