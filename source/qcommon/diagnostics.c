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

#define MAX_DIAGNOSTICS_CONNECTIONS 10

enum DiagMessageType {
	Diagnostics,
	RequestDebugDatabase,
	DebugDatabase,
};

typedef struct com_diag_byteslice_s {
	uint8_t *bytes;
	size_t	 len, cap;

	void ( *reserve )( struct com_diag_byteslice_s *slice, size_t size );
	uint8_t *( *buffer )( struct com_diag_byteslice_s *slice );
	size_t ( *size )( struct com_diag_byteslice_s *slice );
	uint8_t *( *append )( struct com_diag_byteslice_s *slice, uint8_t *b, size_t len );
	void ( *move )( struct com_diag_byteslice_s *slice, size_t p );
	uint8_t *( *read )( struct com_diag_byteslice_s *slice, size_t l );
	void ( *free )( struct com_diag_byteslice_s *slice );
} com_diag_byteslice_t;

typedef struct com_diag_message_s {
	int	  severity;
	int	  line, col;
	char *text;
} com_diag_message_t;

typedef struct {
	unsigned			num_messages;
	com_diag_message_t *messages;
} com_diag_messagelist_t;

typedef struct com_diag_connection_s {
	bool	 open;
	socket_t socket;
	netadr_t address;
	int64_t	 last_active;

	com_diag_byteslice_t *recvbuf, *sendbuf;
	size_t				  recvpos, sendpos;

	struct com_diag_connection_s *next, *prev;
} com_diag_connection_t;

static socket_t com_diagnostics_sock;
static bool		com_diag_initialized;
static trie_t	*com_diag_messages;

static com_diag_connection_t com_diag_connections[MAX_DIAGNOSTICS_CONNECTIONS];
static com_diag_connection_t com_diag_connection_headnode, *com_free_diag_connections;

static cvar_t *com_diagnostics;
static cvar_t *com_diagnostics_port;
static cvar_t *com_diagnostics_timeout;


// ======

static void ByteSlice_Reserve( com_diag_byteslice_t *slice, size_t size )
{
	size_t rem = slice->cap - slice->len;
	if( rem >= size )
		return;

	size -= rem;
	if( slice->cap + size < slice->cap + slice->cap / 2 ) {
		size = slice->cap / 2;
	}

	slice->cap += size;
	if( !slice->bytes )
		slice->bytes = Mem_ZoneMalloc( slice->cap );
	else
		slice->bytes = Mem_Realloc( slice->bytes, slice->cap );
}

static uint8_t *ByteSlice_Buffer( com_diag_byteslice_t *slice )
{
	return slice->bytes + slice->len;
}

static size_t ByteSlice_Size( com_diag_byteslice_t *slice )
{
	return slice->cap - slice->len;
}

static uint8_t *ByteSlice_Append( com_diag_byteslice_t *slice, uint8_t *b, size_t len )
{
	ByteSlice_Reserve( slice, len );
	memcpy( slice->bytes + slice->len, b, len );
	slice->len += len;
	return slice->bytes + slice->len - len;
}

static uint8_t *ByteSlice_Read( com_diag_byteslice_t *slice, size_t l )
{
	uint8_t *b = slice->bytes + slice->len;
	slice->len += l;
	if( slice->len > slice->cap )
		Com_Error( ERR_FATAL, "Buffer overrun" );
	return b;
}

static void ByteSlice_Move( com_diag_byteslice_t *slice, size_t p )
{
	if( slice->len == 0 )
		return;
	if( p > slice->len )
		Com_Error( ERR_FATAL, "Move overrun" );
	memmove( slice->bytes, slice->bytes + p, slice->len - p );
	slice->len -= p;
}

static void ByteSlice_Free( com_diag_byteslice_t *slice )
{
	Mem_ZoneFree( slice->bytes );
	memset( slice, 0, sizeof( *slice ) );
}

static com_diag_byteslice_t *Diag_NewByteSlice( void )
{
	com_diag_byteslice_t *slice = Mem_ZoneMalloc( sizeof( *slice ) );
	slice->append = ByteSlice_Append;
	slice->reserve = ByteSlice_Reserve;
	slice->buffer = ByteSlice_Buffer;
	slice->size = ByteSlice_Size;
	slice->read = ByteSlice_Read;
	slice->free = ByteSlice_Free;
	slice->move = ByteSlice_Move;
	return slice;
}

// ======

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

	con->recvbuf = Diag_NewByteSlice();
	con->sendbuf = Diag_NewByteSlice();
	con->recvpos = 0;
	con->sendpos = 0;

	// put at the start of the list
	con->prev = &com_diag_connection_headnode;
	con->next = com_diag_connection_headnode.next;
	con->next->prev = con;
	con->prev->next = con;
	return con;
}

static void Com_Diag_FreeConnection( com_diag_connection_t *con )
{
	if( con->recvbuf ) {
		con->recvbuf->free( con->recvbuf );
		Mem_ZoneFree( con->recvbuf );
		con->recvbuf = NULL;
	}
	if( con->sendbuf ) {
		con->sendbuf->free( con->sendbuf );
		Mem_ZoneFree( con->sendbuf );
		con->sendbuf = NULL;
	}

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
	int		 len;
	msg_t	 msg;
	size_t	 s;
	uint8_t *b;

	if( con->recvbuf->len <= con->recvpos ) {
		return false;
	}

	s = con->recvbuf->len - con->recvpos;
	b = con->recvbuf->bytes + con->recvpos;
	if( s < 4 ) {
		return false;
	}

	MSG_Init( &msg, b, s );
	MSG_GetSpace( &msg, msg.maxsize );

	len = MSG_ReadInt32( &msg );
	return msg.cursize - msg.readcount >= len;
}

static void Com_Diag_ReadMessage( com_diag_connection_t *con ) {
	int		 len;
	msg_t	 msg;
	size_t	 s;
	uint8_t *b;

	if( con->recvbuf->len <= con->recvpos ) {
		assert( con->recvbuf->len > con->recvpos );
		return;
	}

	s = con->recvbuf->len - con->recvpos;
	b = con->recvbuf->bytes + con->recvpos;
	if( s < 4 ) {
		assert( s >= 4 );
		return;
	}

	MSG_Init( &msg, b, s );
	MSG_GetSpace( &msg, msg.maxsize );

	len = MSG_ReadInt32( &msg );
	MSG_SkipData( &msg, len );

	con->recvpos += s;
}

static void Com_Diag_ReceiveRequest( socket_t *socket, com_diag_connection_t *con )
{
	uint8_t			   recvbuf[1024];
	size_t			   recvbuf_size = sizeof( recvbuf );
	size_t			   total_received = 0;

	while( !Com_Diag_PeekMessage( con ) ) {
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

		con->recvbuf->append( con->recvbuf, recvbuf, read );
	}

	while( Com_Diag_PeekMessage( con ) ) {
		Com_Diag_ReadMessage( con );
	}

	con->recvbuf->move( con->recvbuf, con->recvpos );
	con->recvpos = 0;
}

static void Com_Diag_WriteResponse( socket_t *socket, com_diag_connection_t *con )
{
	size_t total_sent = 0;

	while( con->sendpos < con->sendbuf->len ) {
		int sent = NET_Send(
			&con->socket, con->sendbuf->bytes + con->sendpos, con->sendbuf->len - con->sendpos, &con->address );
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
		con->sendpos += sent;
	}

	if( !con->open )
		return;

	con->sendbuf->move( con->sendbuf, con->sendpos );
	con->sendpos = 0;
}

void Com_Diag_Begin( const char **filenames )
{
	int i;

	if( !com_diag_initialized )
		return;

	assert( com_diag_messages == NULL );

	Trie_Create( TRIE_CASE_INSENSITIVE, &com_diag_messages );

	for( i = 0; filenames[i]; i++ ) {
		Trie_Insert( com_diag_messages, filenames[i],
			Mem_ZoneMalloc( sizeof( com_diag_messagelist_t ) ) );
	}
}

void Com_Diag_Message( int severity, const char *filename, int line, int col, const char *text ) {
	com_diag_message_t *	newmsg;
	com_diag_messagelist_t *ml;

	if( !com_diag_initialized )
		return;
	if( Trie_Find( com_diag_messages, filename, TRIE_EXACT_MATCH, (void **)&ml ) == TRIE_KEY_NOT_FOUND )
		return;

	if( !ml->messages )
		ml->messages = Mem_ZoneMalloc( sizeof( *newmsg ) );
	else
		ml->messages = Mem_Realloc( ml->messages, sizeof( *newmsg ) * (ml->num_messages+1) );
	ml->num_messages++;

	newmsg = &ml->messages[ml->num_messages - 1];
	newmsg->line = line;
	newmsg->col = col;
	newmsg->text = ZoneCopyString( text );
	newmsg->severity = severity;
}

void Com_Diag_End( void )
{
	unsigned int		  i, j;
	msg_t				  msg;
	struct trie_dump_s *  dump;
	com_diag_byteslice_t *slice;
	com_diag_connection_t *con, *next, *hnode = &com_diag_connection_headnode;

	if( !com_diag_initialized )
		return;

	assert( com_diag_messages != NULL );
	Trie_Dump( com_diag_messages, "", TRIE_DUMP_BOTH, &dump );

	slice = Diag_NewByteSlice();

	for( i = 0; i < dump->size; i++ ) {
		const char *			filename = dump->key_value_vector[i].key;
		size_t					filename_len = strlen( filename );
		com_diag_messagelist_t *ml = dump->key_value_vector[i].value;
		size_t					head;

		slice->reserve( slice, 5 );
		// skip
		head = slice->len;
		slice->read( slice, 5 );

		slice->reserve( slice, 4 + filename_len + 4 );

		MSG_Init( &msg, slice->buffer( slice ), slice->size( slice ) );
		MSG_WriteInt32( &msg, ( filename_len ) );
		MSG_WriteData( &msg, filename, filename_len );
		MSG_WriteInt32( &msg, ( ml->num_messages ) );

		slice->read( slice, msg.cursize );

		for( j = 0; j < ml->num_messages; j++ ) {
			com_diag_message_t *dm = &ml->messages[j];
			size_t				text_len = strlen( dm->text );

			slice->reserve( slice, 4 + text_len + 4 + 4 + 4 + 4 );

			MSG_Init( &msg, slice->buffer( slice ), slice->size( slice ) );
			MSG_WriteInt32( &msg, ( text_len ) );
			MSG_WriteData( &msg, dm->text, text_len );
			MSG_WriteInt32( &msg, ( dm->line ) );
			MSG_WriteInt32( &msg, ( dm->col ) );
			MSG_WriteInt32( &msg, ( dm->severity > 1 ) );
			MSG_WriteInt32( &msg, ( dm->severity == 0 ) );

			slice->read( slice, msg.cursize );

			Mem_ZoneFree( dm->text );
		}

		MSG_Init( &msg, slice->bytes + head, 5 );
		MSG_WriteInt32( &msg, ( slice->len - head - 5 ) );
		MSG_WriteInt8( &msg, Diagnostics );

		Mem_ZoneFree( ml->messages );
		Mem_ZoneFree( ml );
	}

	Trie_FreeDump( dump );
	Trie_Destroy( com_diag_messages );
	com_diag_messages = NULL;

	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		con->sendbuf->append( con->sendbuf, slice->bytes, slice->len );
	}

	slice->free( slice );
	Mem_ZoneFree( slice );
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
