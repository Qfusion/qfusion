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

static void Diag_Unicast( diag_connection_t *con, qstreambuf_t *stream );
static void Diag_Broadcast( qstreambuf_t *stream );

static void Diag_RespBreakFilters( diag_connection_t *con, qstreambuf_t *stream );
static void Diag_RespCallStack( diag_connection_t *con, qstreambuf_t *stream );
static void Diag_RespVariables( diag_connection_t *con, qstreambuf_t *stream, int level, const char *scope );

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

static int Diag_ReadInt8( uint8_t *buf, uint8_t *end, int *res )
{
	if( buf + 1 > end )
		return 0;
	*res = buf[0];
	return 1;
}

static int Diag_ReadInt32( uint8_t *buf, uint8_t *end, int *res )
{
	if( buf + 4 > end )
		return 0;
	*res = buf[0] | ( buf[1] << 8 ) | ( buf[2] << 16 ) | ( buf[3] << 24 );
	return 4;
}

static int Diag_ReadString( uint8_t *buf, uint8_t *end, char **pres )
{
	int32_t len;
	int32_t off = 0;
	char *res;

	*pres = NULL;
	if( buf + 4 > end )
		return 0;

	off += Diag_ReadInt32( buf, end, &len );

	if( buf + off + len > end )
		return 0;

	res = Mem_TempMalloc( len + 1 );
	memcpy( res, (char *)( buf + off ), len );
	res[len] = 0;
	*pres = res;

	off += len;

	return off;
}

static int Diag_WriteInt8( uint8_t *buf, uint8_t *end, int val )
{
	if( buf + 1 > end )
		return 0;
	buf[0] = (uint8_t)val;
	return 1;
}

static int Diag_WriteInt32( uint8_t *buf, uint8_t *end, int val )
{
	if( buf + 4 > end )
		return 0;
	buf[0] = ( uint8_t )( val & 0xff );
	buf[1] = ( uint8_t )( ( val >> 8 ) & 0xff );
	buf[2] = ( uint8_t )( ( val >> 16 ) & 0xff );
	buf[3] = ( uint8_t )( val >> 24 );
	return 4;
}

static int Diag_WriteString( uint8_t *buf, uint8_t *end, char *str, size_t len )
{
	int32_t off = 0;

	if( !str )
		return 0;

	if( buf + 4 > end )
		return 0;

	off += Diag_WriteInt32( buf, end, len );

	if( buf + off + len > end )
		return 0;

	memcpy( buf + off, str, len );
	off += len;

	return off;
}

static bool Diag_PeekMessage( diag_connection_t *con ) {
	int		 len;
	size_t	 s, off;
	uint8_t *data;
	qstreambuf_t *rb = &con->recvbuf;

	s = rb->datalength( rb );
	if( s < 4 ) {
		return false;
	}

	data = rb->data( rb );
	off = Diag_ReadInt32( data, data + s, &len );
	return s - off >= len;
}

static void Diag_ReadMessage( diag_connection_t *con ) {
	int		 len;
	uint8_t *data, *end;
	size_t	 s, off;

	qstreambuf_t *rb = &con->recvbuf;
	qstreambuf_t  resp;

	s = rb->datalength( rb );
	if( s < 4 ) {
		assert( s >= 4 );
		return;
	}

	QStreamBuf_Init( &resp );

	off = 0;
	data = rb->data( rb );
	end = data + s;

	off += Diag_ReadInt32( data + off, end, &len );

	if( len != 0 ) {
		int mt;

		off += Diag_ReadInt8( data + off, end, &mt );

		switch( mt ) {
			case StartDebugging:
				if( !con->debugging ) {
					con->debugging = true;
					diag_debugging++;
				}
				break;
			case StopDebugging:
			case Disconnect:
				if( con->debugging ) {
					con->debugging = false;
					diag_debugging--;
				}
				break;
			case RequestBreakFilters:
				Diag_RespBreakFilters( con, &resp );
				break;
			case RequestCallStack:
				Diag_RespCallStack( con, &resp );
				break;
			case Pause:
				Diag_Stop( true );
				break;
			case Continue:
				Diag_Stop( false );
				break;
			case RequestVariables:
				{
					char *sep, *str;

					off += Diag_ReadString( data + off, end, &str );

					if( !str ) {
						break;
					}

					sep = strchr( str, ':' );
					if( sep ) {
						*sep = '\0';				
						Diag_RespVariables( con, &resp, atoi( str ), sep + 1 );
					}

					Mem_TempFree( str );
				}
				break;
		}
	}

	rb->consume( rb, 4 + len );

	if( resp.datalength( &resp ) > 0 ) {
		Diag_Unicast( con, &resp );
	}

	resp.clear( &resp );
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

static void Diag_Broadcast( qstreambuf_t *stream )
{
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

static void Diag_EncodeMsg( qstreambuf_t *stream, const char *fmt, ... )
{
	int	   j;
	size_t total_len = 0;

	for( j = 0; j < 2; j++ ) {
		int			i = 0;
		char *		s = NULL;
		size_t		s_len;
		size_t		len = 0, off = 0;
		va_list		argp;
		const char *p;
		uint8_t		*buf = NULL, *end = NULL;

		if( j != 0 ) {
			stream->prepare( stream, total_len );
			buf = stream->buffer( stream );
			end = buf + stream->size( stream );
			off = 0;
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
						off += Diag_WriteInt8( buf + off, end, i );
					}
					break;
				case 's':
					s = va_arg( argp, char * );
					s_len = strlen( s );
					len += 4;
					len += s_len;
					if( j != 0 ) {
						off += Diag_WriteString( buf + off, end, s, s_len );
					}
					break;
				case 'i':
				case 'd':
					i = va_arg( argp, int );
					len += 4;
					if( j != 0 ) {
						off += Diag_WriteInt32( buf + off, end, i );
					}
					break;
			}
		}

		va_end( argp );

		if( j != 0 ) {
			stream->commit( stream, off );
			break;
		}

		total_len = len;
	}
}

static void Diag_FinishEncodeMsg( qstreambuf_t *stream, size_t head_pos, int type )
{
	uint8_t *buf = stream->data( stream ) + head_pos;
	uint8_t *end = buf + 5;
	size_t	 off = 0;

	off += Diag_WriteInt32( buf + off, end, ( stream->datalength( stream ) - head_pos - 5 ) );
	off += Diag_WriteInt8( buf + off, end, type );
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

		Diag_EncodeMsg( &stream, "%s%i", filename, ml->num_messages );

		for( j = 0; j < ml->num_messages; j++ ) {
			diag_message_t *dm = &ml->messages[j];

			Diag_EncodeMsg( &stream, "%s%i%i%i%i", dm->text, dm->line, dm->col, dm->severity > 1, dm->severity == 0 );

			Mem_ZoneFree( dm->text );
		}

		Diag_FinishEncodeMsg( &stream, head_pos, Diagnostics );

		Mem_ZoneFree( ml->messages );
		Mem_ZoneFree( ml );
	}

	Trie_FreeDump( dump );
	Trie_Destroy( diag_messages );
	diag_messages = NULL;

	Diag_Broadcast( &stream );

	stream.clear( &stream );
}

static void Diag_Pause_( const char *sectionName, int line, int col, const char *funcDecl, const char *reason, const char *reasonString )
{
	qstreambuf_t stream;
	size_t		 head_pos;

	if( !diag_initialized ) {
		return;
	}
	if( !diag_debugging ) {
		return;
	}

	Diag_Stop( true );

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_EncodeMsg( &stream, "%s%i%s", reason, 0, reasonString );

	Diag_FinishEncodeMsg( &stream, head_pos, HasStopped );

	Diag_Broadcast( &stream );

	stream.clear( &stream );
}

void Diag_Exception( const char *sectionName, int line, int col, const char *funcDecl, const char *exceptionString )
{
	Diag_Pause_( sectionName, line, col, funcDecl, "exception", exceptionString );
}

static void Diag_HasContinued( void )
{
	qstreambuf_t stream;
	size_t		 head_pos;

	QStreamBuf_Init( &stream );

	head_pos = Diag_BeginEncodeMsg( &stream );

	Diag_FinishEncodeMsg( &stream, head_pos, HasContinued );

	Diag_Broadcast( &stream );

	stream.clear( &stream );
}

void Diag_Stop( bool stop )
{
	if( diag_stopped == stop ) {
		return;
	}
	if( !stop ) {
		Diag_HasContinued();
	}
	diag_stopped = stop;
}

bool Diag_Stopped( void ) {
	if( !diag_debugging ) {
		Diag_Stop( false );
	}
	return diag_stopped;
}

static void Diag_RespBreakFilters( diag_connection_t *con, qstreambuf_t *stream )
{
	size_t		 head_pos;

	head_pos = Diag_BeginEncodeMsg( stream );

	Diag_EncodeMsg( stream, "%i%s%s", 1, "uncaught", "Uncaught Exceptions" );

	Diag_FinishEncodeMsg( stream, head_pos, BreakFilters );
}

static void Diag_RespCallStack( diag_connection_t *con, qstreambuf_t *stream )
{
	size_t		 head_pos;
	int			 i, stack_size;

	angelwrap_stack_frame_t **stack;

	head_pos = Diag_BeginEncodeMsg( stream );

	stack = Com_asGetCallstack();
	stack_size = 0;
	if( stack ) {
		for( i = 0; stack[i]; i++ )
			stack_size++;
	}

	Diag_EncodeMsg( stream, "%i", stack_size );
	for( i = 0; i < stack_size; i++ ){
		Diag_EncodeMsg( stream, "%s%s%i", stack[i]->func, stack[i]->file, stack[i]->line );
		Mem_Free( stack[i]->file );
		Mem_Free( stack[i]->func );
		Mem_Free( stack[i] );
	}
	Mem_Free( stack );

	Diag_FinishEncodeMsg( stream, head_pos, CallStack );
}

static void Diag_RespVariables( diag_connection_t *con, qstreambuf_t *stream, int level, const char *scope )
{
	size_t		 head_pos;
	int			 i, num_vars = 0;

	angelwrap_variable_t **vars;

	head_pos = Diag_BeginEncodeMsg( stream );

	vars = Com_asGetVariables( level, scope );
	num_vars = 0;

	if( vars ) {
		for( i = 0; vars[i]; i++ )
			num_vars++;
	}

	Diag_EncodeMsg( stream, "%i", num_vars );
	for( i = 0; i < num_vars; i++ ) {
		Diag_EncodeMsg( stream, "%s%s%s%i", vars[i]->name, vars[i]->value, vars[i]->type, (int)vars[i]->hasProperties );
		Mem_Free( vars[i]->name );
		Mem_Free( vars[i]->value );
		Mem_Free( vars[i]->type );
		Mem_Free( vars[i] );
	}
	Mem_Free( vars );

	Diag_FinishEncodeMsg( stream, head_pos, Variables );
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
