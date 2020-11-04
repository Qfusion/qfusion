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
static int		diag_debugging;
static bool		diag_stopped;

static diag_connection_t diag_connections[MAX_DIAGNOSTICS_CONNECTIONS];
static diag_connection_t diag_connection_headnode, *com_free_diag_connections;

static cvar_t *com_diagnostics;
static cvar_t *com_diagnostics_port;
static cvar_t *com_diagnostics_timeout;

static void Diag_RespBreakFilters( diag_connection_t *con );
static void Diag_RespCallStack( diag_connection_t *con );

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
	return con;
}

static void Diag_FreeConnection( diag_connection_t *con )
{
	con->debugging = false;
	if( con->debugging ) {
		diag_debugging--;
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

static bool Diag_PeekMessage( diag_connection_t *con ) {
	int		 len;
	msg_t	 msg;
	size_t	 s;
	qstreambuf_t *rb = &con->recvbuf;

	s = rb->datalength( rb );
	if( s < 4 ) {
		return false;
	}

	MSG_Init( &msg, rb->data( rb ), s );
	MSG_GetSpace( &msg, msg.maxsize );

	len = MSG_ReadInt32( &msg );
	return msg.cursize - msg.readcount >= len;
}

static void Diag_ReadMessage( diag_connection_t *con ) {
	int		 len;
	msg_t	 msg;
	size_t	 s, rc;
	qstreambuf_t *rb = &con->recvbuf;

	s = rb->datalength( rb );
	if( s < 4 ) {
		assert( s >= 4 );
		return;
	}

	MSG_Init( &msg, rb->data( rb ), s );
	MSG_GetSpace( &msg, msg.maxsize );

	len = MSG_ReadInt32( &msg );
	rc = msg.readcount;

	if( len != 0 ) {
		int mt = MSG_ReadUint8( &msg );
		switch( mt ) {
			case StartDebugging:
				if( !con->debugging ) {
					con->debugging = true;
					diag_debugging++;
				}
				break;
			case StopDebugging:
				if( con->debugging ) {
					con->debugging = false;
					diag_debugging--;
				}
				break;
			case RequestBreakFilters:
				Diag_RespBreakFilters( con );
				break;
			case RequestCallStack:
				Diag_RespCallStack( con );
				break;
			case Continue:
				diag_stopped = false;
				break;
		}
	}

	MSG_SkipData( &msg, len - ( msg.readcount - rc ) );

	rb->consume( rb, s );
}

static void Diag_ReadFromSocket( socket_t *socket, diag_connection_t *con )
{
	uint8_t			   recvbuf[1024];
	size_t			   recvbuf_size = sizeof( recvbuf );
	size_t			   total_received = 0;
	qstreambuf_t *rb = &con->recvbuf;

	while( !Diag_PeekMessage( con ) ) {
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

	while( Diag_PeekMessage( con ) ) {
		Diag_ReadMessage( con );
	}

	rb->compact( rb );
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

static void Diag_Unicast( diag_connection_t *con, qstreambuf_t *stream )
{
	qstreambuf_t *sb = &con->sendbuf;
	sb->write( sb, stream->data( stream ), stream->datalength( stream ) );
}

static void Diag_Broadcast( qstreambuf_t *stream ) {
	diag_connection_t *con, *next, *hnode = &diag_connection_headnode;

	for( con = hnode->prev; con != hnode; con = next ) {
		next = con->prev;
		Diag_Unicast( con, stream );
	}
}

void Diag_BeginBuild( const char **filenames )
{
	int i;

	if( !diag_initialized )
		return;

	if( diag_messages != NULL ) {
		assert( diag_messages == NULL );
		return;
	}

	Trie_Create( TRIE_CASE_INSENSITIVE, &diag_messages );

	for( i = 0; filenames[i]; i++ ) {
		Trie_Insert( diag_messages, filenames[i],
			Mem_ZoneMalloc( sizeof( diag_messagelist_t ) ) );
	}
}

static size_t Diag_BeginEncodeMsg( qstreambuf_t *stream )
{
	size_t head_pos;

	stream->prepare( stream, 5 );
	head_pos = stream->datalength( stream );
	stream->commit( stream, 5 );

	return head_pos;
}

static void Diag_EncodeMsg( qstreambuf_t *stream, msg_t *msg, const char *fmt, ... )
{
	int	   j;
	size_t total_len = 0;

	for( j = 0; j < 2; j++ ) {
		int			i = 0;
		char *		s = NULL;
		size_t		s_len;
		size_t		len = 0;
		va_list		argp;
		const char *p;

		if( j != 0 ) {
			stream->prepare( stream, total_len );
			MSG_Init( msg, stream->buffer( stream ), stream->size( stream ) );
		}

		va_start( argp, fmt );

		for( p = fmt; *p != '\0'; p++ ) {
			if( *p != '%' ) {
				continue;
			}

			switch( *++p ) {
				case 'c':
					i = va_arg( argp, int );
					len += 1;
					if( j != 0 ) {
						MSG_WriteUint8( msg, i );
					}
					break;
				case 's':
					s = va_arg( argp, char * );
					s_len = strlen( s );
					len += 4;
					len += s_len;
					if( j != 0 ) {
						MSG_WriteInt32( msg, s_len );
						MSG_WriteData( msg, s, s_len );
					}
					break;
				case 'i':
				case 'd':
					i = va_arg( argp, int );
					len += 4;
					if( j != 0 ) {
						MSG_WriteInt32( msg, i );
					}
					break;
			}
		}

		va_end( argp );

		if( j != 0 ) {
			stream->commit( stream, msg->cursize );
			break;
		}

		total_len = len;
	}
}

static void Diag_FinishEncodeMsg( qstreambuf_t *stream, msg_t *msg, size_t head_pos, int type )
{
	MSG_Init( msg, stream->data( stream ) + head_pos, 5 );
	MSG_WriteInt32( msg, ( stream->datalength( stream ) - head_pos - 5 ) );
	MSG_WriteInt8( msg, type );
}

void Diag_Message( int severity, const char *filename, int line, int col, const char *text ) {
	diag_message_t *	newmsg;
	diag_messagelist_t *ml;

	if( !diag_initialized )
		return;
	if( diag_messages == NULL )
		return;
	if( Trie_Find( diag_messages, filename, TRIE_EXACT_MATCH, (void **)&ml ) == TRIE_KEY_NOT_FOUND )
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

void Diag_EndBuild( void )
{
	unsigned int		i, j;
	msg_t				msg;
	struct trie_dump_s *dump;
	qstreambuf_t		stream;

	if( !diag_initialized )
		return;

	if( diag_messages == NULL ) {
		assert( diag_messages != NULL );
		return;
	}

	Trie_Dump( diag_messages, "", TRIE_DUMP_BOTH, &dump );

	QStreamBuf_Init( &stream );

	for( i = 0; i < dump->size; i++ ) {
		const char *			filename = dump->key_value_vector[i].key;
		diag_messagelist_t *ml = dump->key_value_vector[i].value;
		size_t					head_pos;

		head_pos = Diag_BeginEncodeMsg( &stream );

		Diag_EncodeMsg( &stream, &msg, "%s%i", filename, ml->num_messages );

		for( j = 0; j < ml->num_messages; j++ ) {
			diag_message_t *dm = &ml->messages[j];

			Diag_EncodeMsg( &stream, &msg, "%s%i%i%i%i", dm->text, dm->line, dm->col, dm->severity > 1, dm->severity == 0 );

			Mem_ZoneFree( dm->text );
		}

		Diag_FinishEncodeMsg( &stream, &msg, head_pos, Diagnostics );

		Mem_ZoneFree( ml->messages );
		Mem_ZoneFree( ml );
	}

	Trie_FreeDump( dump );
	Trie_Destroy( diag_messages );
	diag_messages = NULL;

	Diag_Broadcast( &stream );

	stream.clear( &stream );
}

void Diag_Exception( const char *sectionName, int line, int col, const char *funcDecl, const char *exceptionString )
{
	qstreambuf_t	   stream;
	msg_t			   msg;
	size_t			   head_pos;

	if( !diag_initialized ) {
		return;
	}
	if( !diag_debugging ) {
		return;
	}

	diag_stopped = true;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_EncodeMsg( &stream, &msg, "%s%i%s", "exception", 0, exceptionString );

	Diag_FinishEncodeMsg( &stream, &msg, head_pos, HasStopped );

	Diag_Broadcast( &stream );

	stream.clear( &stream );
}

bool Diag_Stopped( void ) {
	if( !diag_debugging ) {
		diag_stopped = false;
	}
	return diag_stopped;
}

static void Diag_RespBreakFilters( diag_connection_t *con )
{
	qstreambuf_t stream;
	msg_t		 msg;
	size_t		 head_pos;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_EncodeMsg( &stream, &msg, "%i%s%s", 1, "uncaught", "Uncaught Exceptions" );

	Diag_FinishEncodeMsg( &stream, &msg, head_pos, BreakFilters );

	Diag_Unicast( con, &stream );

	stream.clear( &stream );
}

static void Diag_RespCallStack( diag_connection_t *con )
{
	qstreambuf_t stream;
	msg_t		 msg;
	size_t		 head_pos;
	int			 i, stack_size;

	angelwrap_stack_frame_t **stack;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	stack = Com_asGetCallstack();
	stack_size = 0;
	if( stack ) {
		for( i = 0; stack[i]; i++ )
			stack_size++;
	}

	Diag_EncodeMsg( &stream, &msg, "%i", stack_size );
	for( i = 0; i < stack_size; i++ ){
		Diag_EncodeMsg( &stream, &msg, "%s%s%i", stack[i]->func, stack[i]->file, stack[i]->line );
		Mem_Free( stack[i]->file );
		Mem_Free( stack[i]->func );
		Mem_Free( stack[i] );
	}
	Mem_Free( stack );

	Diag_FinishEncodeMsg( &stream, &msg, head_pos, CallStack );

	Diag_Unicast( con, &stream );

	stream.clear( &stream );
}

void Diag_Init( void ) {
	unsigned int i;

	diag_initialized = false;
	diag_stopped = false;
	diag_debugging = 0;

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
			NULL, connections );
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
	diag_debugging = 0;
}
