/*
Copyright (C) 2013 Victor Luchits

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

// sv_web.c -- builtin HTTP server

#include "server.h"
#include "../qcommon/trie.h"

#ifdef HTTP_SUPPORT

#define MAX_INCOMING_HTTP_CONNECTIONS		32

#define MAX_INCOMING_CONTENT_SIZE			0x2800

#define INCOMING_HTTP_CONNECTION_TIMEOUT	15 // seconds

#define HTTP_SENDBUF_SIZE					0x8000
#define HTTP_RECVBUF_SIZE					0x8000

typedef enum
{
	HTTP_CONN_STATE_NONE = 0,
	HTTP_CONN_STATE_RECV = 1,
	HTTP_CONN_STATE_SEND = 2
} sv_http_connstate_t;

typedef enum
{
	HTTP_METHOD_BAD = -1,
	HTTP_METHOD_NONE = 0,
	HTTP_METHOD_GET  = 1,
	HTTP_METHOD_POST = 2,
	HTTP_METHOD_PUT  = 3,
	HTTP_METHOD_HEAD = 4,
} sv_http_query_method_t;

typedef enum
{
	HTTP_RESP_OK = 200,
	HTTP_RESP_BAD_REQUEST = 400,
	HTTP_RESP_FORBIDDEN = 403,
	HTTP_RESP_NOT_FOUND = 404,
} sv_http_response_code_t;

typedef struct sv_http_query_chunk_s
{
	size_t size;
	size_t rem_size;
	size_t max_size;
	char data[0x8000];
	struct sv_http_query_chunk_s *next, *prev;
} sv_http_query_chunk_t;

typedef struct {
	sv_http_query_method_t method;
	qboolean bad;

	char *resource;
	char *http_ver;

	trie_t *headers;
	qboolean got_headers;

	char *body;
	size_t content_length; // including the trailing '\0'
	size_t expected_content_length;

	qboolean close_after_resp;

	size_t size;
	sv_http_query_chunk_t *chunk_tail, *chunk_head;
} sv_http_request_t;

typedef struct {
	sv_http_response_code_t code;

	int file;

	char *body;
	size_t content_length;

	size_t size;
	sv_http_query_chunk_t *chunk_tail, *chunk_head;
} sv_http_response_t;

typedef struct sv_http_connection_s
{
	qboolean open;
	sv_http_connstate_t state;
	qboolean close_after_resp;

	socket_t socket;
	netadr_t address;

	unsigned int last_active;

	sv_http_request_t request;
	sv_http_response_t response;

	struct sv_http_connection_s *next, *prev;
} sv_http_connection_t;

static qboolean sv_http_initialized = qfalse;
static sv_http_connection_t sv_http_connections[MAX_INCOMING_HTTP_CONNECTIONS];
static sv_http_connection_t sv_http_connection_headnode, *sv_free_http_connections;

// ============================================================================

/*
* SV_Web_AllocChunk
*/
static sv_http_query_chunk_t *SV_Web_AllocChunk( void )
{
	sv_http_query_chunk_t *chunk;
	
	chunk = Mem_ZoneMalloc( sizeof( sv_http_query_chunk_t ) );
	chunk->max_size = sizeof( chunk->data );
	chunk->rem_size = chunk->max_size;
	return chunk;
}

/*
* SV_Web_StoreChunk
*
* Stores data stream in chunks
*/
static void SV_Web_StoreChunk( sv_http_query_chunk_t **list, size_t *list_data_size, const char *data, size_t size )
{
	sv_http_query_chunk_t *chunk, *next;
	size_t remaining;

	if( !size ) {
		return;
	}

	remaining = size;

	// copy received data into chunks
	chunk = *list;
	while( remaining > chunk->rem_size ) {
		size_t rem_size = chunk->rem_size;

		memcpy( chunk->data + chunk->size, data, rem_size );
		chunk->rem_size = 0;
		chunk->size = chunk->max_size;
		data += rem_size;
		remaining -= rem_size;

		next = SV_Web_AllocChunk();
		chunk->next = next;
		next->prev = chunk;
		chunk = next;
		*list = chunk;
		*list_data_size += rem_size;
	}

	memcpy( chunk->data + chunk->size, data, remaining );
	chunk->size += remaining;
	chunk->rem_size -= remaining;
	*list_data_size += remaining;
}

/*
* SV_Web_FreeChunk
*/
static void SV_Web_FreeChunk( sv_http_query_chunk_t *chunk )
{
	Mem_Free( chunk );
}

/*
* SV_Web_FreeChunks
*/
static void SV_Web_FreeChunks( sv_http_query_chunk_t **list, size_t *list_data_size )
{
	sv_http_query_chunk_t *chunk;

	while( *list ) {
		chunk = *list;
		*list = chunk->prev;
		SV_Web_FreeChunk( chunk );
	}
	*list_data_size = 0;
}

// ============================================================================

/*
* SV_Web_AllocConnection
*/
static sv_http_connection_t *SV_Web_AllocConnection( void )
{
	sv_http_connection_t *con;

	if( sv_free_http_connections )
	{
		// take a free decal if possible
		con = sv_free_http_connections;
		sv_free_http_connections = con->next;
	}
	else
	{
		return NULL;
	}

	// put at the start of the list
	con->prev = &sv_http_connection_headnode;
	con->next = sv_http_connection_headnode.next;
	con->next->prev = con;
	con->prev->next = con;
	con->state = HTTP_CONN_STATE_NONE;
	con->close_after_resp = qfalse;

	memset( &con->request, 0, sizeof( con->request ) );
	Trie_Create( TRIE_CASE_INSENSITIVE, &con->request.headers );
	con->request.chunk_tail = con->request.chunk_head = SV_Web_AllocChunk();

	memset( &con->response, 0, sizeof( con->response ) );
	con->response.chunk_tail = con->response.chunk_head = SV_Web_AllocChunk();

	return con;
}

/*
* SV_Web_FreeConnection
*/
static void SV_Web_FreeConnection( sv_http_connection_t *con )
{
	sv_http_request_t *request = &con->request;
	sv_http_response_t *response = &con->response;

	// free incoming data
	SV_Web_FreeChunks( &request->chunk_head, &request->size );

	Trie_Destroy( request->headers );

	if( request->resource ) {
		Mem_Free( request->resource );
	}
	if( request->http_ver ) {
		Mem_Free( request->http_ver );
	}
	if( request->body ) {
		Mem_Free( request->body );
	}

	// free outgoing data
	SV_Web_FreeChunks( &request->chunk_head, &request->size );

	if( response->file ) {
		FS_FCloseFile( response->file );
	}
	
	if( response->body ) {
		Mem_Free( response->body );
	}

	con->state = HTTP_CONN_STATE_NONE;

	// remove from linked active list
	con->prev->next = con->next;
	con->next->prev = con->prev;

	// insert into linked free list
	con->next = sv_free_http_connections;
	sv_free_http_connections = con;
}

/*
* SV_Web_InitConnections
*/
static void SV_Web_InitConnections( void )
{
	unsigned int i;

	memset( sv_http_connections, 0, sizeof( sv_http_connections ) );

	// link decals
	sv_free_http_connections = sv_http_connections;
	sv_http_connection_headnode.prev = &sv_http_connection_headnode;
	sv_http_connection_headnode.next = &sv_http_connection_headnode;
	for( i = 0; i < MAX_INCOMING_HTTP_CONNECTIONS - 2; i++ ) {
		sv_http_connections[i].next = &sv_http_connections[i+1];
	}
}

/*
* SV_Web_ShutdownConnections
*/
static void SV_Web_ShutdownConnections( void )
{
	sv_http_connection_t *con, *next, *hnode;

	// close dead connections
	hnode = &sv_http_connection_headnode;
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->next;

		if( con->open ) {
			NET_CloseSocket( &con->socket );
			SV_Web_FreeConnection( con );
		}
	}
}

// ============================================================================
/*
* SV_Web_ResetRequest
*/
static void SV_Web_ResetRequest( sv_http_request_t *request )
{
	trie_t *trie;

	// free incoming data
	SV_Web_FreeChunks( &request->chunk_head, &request->size );

	trie = request->headers;
	Trie_Clear( request->headers );

	if( request->resource ) {
		Mem_Free( request->resource );
	}
	if( request->http_ver ) {
		Mem_Free( request->http_ver );
	}
	if( request->body ) {
		Mem_Free( request->body );
	}
	
	memset( request, 0, sizeof( *request ) );
	request->headers = trie;
	request->chunk_tail = request->chunk_head = SV_Web_AllocChunk();
}

/*
* SV_Web_ParseRequestResource
*/
static void SV_Web_ParseRequestResource( sv_http_request_t *request, char *data )
{
	const char *ptr;
	char *token;

	ptr = data;

	token = COM_ParseExt( &ptr, qfalse );
	if( !Q_stricmp( token, "GET" ) ) {
		request->method = HTTP_METHOD_GET;
	} else if( !Q_stricmp( token, "POST" ) ) {
		request->method = HTTP_METHOD_POST;
	} else if( !Q_stricmp( token, "PUT" ) ) {
		request->method = HTTP_METHOD_PUT;
	} else if( !Q_stricmp( token, "HEAD" ) ) {
		request->method = HTTP_METHOD_HEAD;
	} else {
		request->bad = qtrue;
	}

	token = COM_ParseExt( &ptr, qfalse );
	while( *token == '/' ) {
		token++;
	}
	request->resource = ZoneCopyString( *token ? token : "/" );

	token = COM_ParseExt( &ptr, qfalse );
	request->http_ver = ZoneCopyString( token );

	// check for HTTP/1.1 and greater
	if( strncmp( request->http_ver, "HTTP/", 5 ) ) {
		request->bad = qtrue;
	} else if( (int)(atof( request->http_ver + 5 )*10) < 11 ) {
		request->bad = qtrue;
	}
}

/*
* SV_Web_ParseRequestHeaders
*/
static void SV_Web_ParseRequestHeaders( sv_http_request_t *request, char *data )
{
	char *line, *value;
	size_t offset;
	qboolean done;
	const char *newline, *colon;
	const char *key;
	trie_error_t trie_error;

	if( request->bad ) {
		return;
	}

	done = qfalse;
	do {
		if( !data || !*data ) {
			break;
		}

		line = data;
		newline = strchr( data, '\n' );
		if( newline ) {
			offset = newline - data;
			data[offset] = '\0';
			data += offset + 1;
		}
		else {
			done = qtrue;
		}

		colon = strchr( line, ':' );
		if( !colon ) {
			continue;
		}

		offset = colon - line;
		line[offset] = '\0';
		key = Q_trim( line );
		value = line + offset + 1;

		// ltrim
		while( *value == ' ' ) {
			value++;
		}

		trie_error = Trie_Insert( request->headers, key, value );
		if( trie_error == TRIE_DUPLICATE_KEY ) {
			void *old_data;
			Trie_Replace( request->headers, key, value, &old_data );
		}
	} while( !done );

	//
	// store valuable information for quicker access
	trie_error = Trie_Find( request->headers, "Content-Length", TRIE_EXACT_MATCH, &value );
	if( trie_error == TRIE_OK ) {
		request->expected_content_length = atoi( value );
	}

	trie_error = Trie_Find( request->headers, "Connection", TRIE_EXACT_MATCH, &value );
	if( trie_error == TRIE_OK ) {
		if( !Q_stricmp( value, "close" ) ) {
			request->close_after_resp = qtrue;
		}
	}

	// valid HTTP 1.1 request must contain Host header
	trie_error = Trie_Find( request->headers, "Host", TRIE_EXACT_MATCH, &value );
	if( trie_error != TRIE_OK || !value || !*value ) {
		request->bad = qtrue;
	}
}

/*
* SV_Web_CompactRequestBody
*/
static void SV_Web_CompactRequestBody( sv_http_request_t *request )
{
	char *data;
	size_t data_size, data_p;
	sv_http_query_chunk_t *chunk;

	data_p = 0;
	data_size = request->size;
	data = Mem_ZoneMalloc( data_size + 1 );

	if( request->size ) {
		while( request->chunk_head ) {
			chunk = request->chunk_head;
			request->chunk_head = chunk->next;

			memcpy( data + data_p, chunk->data, chunk->size );
			data_p += chunk->size;

			SV_Web_FreeChunk( chunk );
		}
		request->size = 0;
		request->chunk_tail = request->chunk_head = SV_Web_AllocChunk();
	}

	data[data_p] = '\0';

	request->body = data;
	request->content_length = data_p;
}

/*
* SV_Web_GetHeaders
*/
static void SV_Web_GetHeaders( sv_http_request_t *request )
{
	char *data;
	size_t data_size;
	char *sep, *sep_p;
	char *headers, *body;

	if( !request->size ) {
		return;
	}

	SV_Web_CompactRequestBody( request );

	data = request->body;
	data_size = request->content_length;

	request->body = NULL;
	request->content_length = 0;

	sep = "\r\n\r\n";
	sep_p = strstr( data, sep );
	if( !sep ) {
		sep = "\n\n";
		sep_p = strstr( data, sep );
	}

	if( !sep_p ) {
		Mem_ZoneFree( data );
		return;
	}

	*sep_p = '\0';

	SV_Web_ParseRequestResource( request, data );

	headers = data;
	SV_Web_ParseRequestHeaders( request, headers );

	body = sep_p + strlen( sep );
	SV_Web_StoreChunk( &request->chunk_tail, &request->size, body, data_size - (body - data) );

	Mem_ZoneFree( data );
}

/*
* SV_Web_ReceiveRequest
*/
static void SV_Web_ReceiveRequest( sv_http_connection_t *con )
{
	int ret;
	netadr_t address;
	char data[HTTP_RECVBUF_SIZE+3], *recvbuf;
	sv_http_query_chunk_t *chunk;
	sv_http_request_t *request;

	request = &con->request;
	chunk = request->chunk_head;

	data[0] = '\0';
	if( request->size >= 2 ) {
		if( chunk->size < 2 ) {
			chunk = request->chunk_head->prev;
			data[0] = chunk->data[chunk->size-1];
			chunk = request->chunk_head;
			data[1] = chunk->data[0];
		}
		else {
			data[0] = chunk->data[chunk->size-2];
			data[1] = chunk->data[chunk->size-1];
		}
		recvbuf = data + 2;
	}
	else if( request->size ) {
		data[0] = chunk->data[0];
		recvbuf = data + 1;
	}
	else {
		recvbuf = data;
	}

	while( (ret = NET_Get( &con->socket, &address, recvbuf, HTTP_RECVBUF_SIZE - 1 )) > 0 )
	{
		con->last_active = Sys_Milliseconds();

		recvbuf[ret] = '\0';
		SV_Web_StoreChunk( &request->chunk_tail, &request->size, recvbuf, ret );
	}

	if( !request->got_headers ) {
		qboolean got_headers = (strstr( data, "\r\n\r\n" ) || strstr( data, "\n\n" ) ? qtrue : qfalse);

		if( got_headers ) {
			SV_Web_GetHeaders( request );
			request->got_headers = qtrue;

			con->close_after_resp = request->close_after_resp;
		}
	}

	if( request->size > MAX_INCOMING_CONTENT_SIZE ) {
		request->bad = qtrue;
		con->state = HTTP_CONN_STATE_SEND;
	}
	else if( request->got_headers && request->size >= request->expected_content_length ) {
		// yay, parsed request
		SV_Web_CompactRequestBody( request );
		con->state = HTTP_CONN_STATE_SEND;
	}

	if( ret == -1 ) {
		con->open = qfalse;
		Com_DPrintf( "HTTP connection error from %s\n", NET_AddressToString( &con->address ) );
	}
}

// ============================================================================

/*
* SV_Web_AddToResponse
*/
static void SV_Web_AddToResponse( sv_http_response_t *response, const void *data, size_t size )
{
	SV_Web_StoreChunk( &response->chunk_tail, &response->size, data, size );
}

/*
* SV_Web_ResponseCodeMessage
*/
static const char *SV_Web_ResponseCodeMessage( sv_http_response_code_t code )
{
	switch( code ) {
		case HTTP_RESP_OK: return "OK";
		case HTTP_RESP_BAD_REQUEST: return "Bad Request";
		case HTTP_RESP_NOT_FOUND: return "Not Found";
		default: return "Unknown Error";
	}
}

/*
* SV_Web_RespondToQuery
*/
static void SV_Web_RespondToQuery( sv_http_connection_t *con )
{
	char headers[0x8000];
	sv_http_request_t *request = &con->request;
	sv_http_response_t *response = &con->response;

	if( request->bad ) {
		response->code = HTTP_RESP_BAD_REQUEST;
		con->close_after_resp = qtrue;
	}
	else if( request->method == HTTP_METHOD_GET ) {
		response->body = NULL;
		response->content_length = FS_FOpenBaseFile( request->resource, &response->file, FS_READ );
		if( !response->file ) {
			response->code = HTTP_RESP_NOT_FOUND;
		}
		else {
			response->code = HTTP_RESP_OK;
		}
	}

	if( response->code != HTTP_RESP_OK ) {
		response->body = ZoneCopyString( 
			va( "%i %s\n", response->code, SV_Web_ResponseCodeMessage( response->code ) ) );
		response->content_length = strlen( response->body );
	}

	Q_snprintfz( headers, sizeof( headers ), 
		"%s %i %s\r\nServer: " APPLICATION " v" APP_VERSION_STR "\r\nContent-Length: %i\r\n", 
		con->request.http_ver, response->code, SV_Web_ResponseCodeMessage( response->code ), response->content_length );

	if( response->file ) {
		Q_strncatz( headers, 
			va( "Content-Disposition: attachment; filename=\"%s\"", COM_FileBase( request->resource ) ),
			sizeof( headers ) );
	}

	Q_strncatz( headers, "\r\n", sizeof( headers ) );

	SV_Web_AddToResponse( response, headers, strlen( headers ) );
	if( response->body ) {
		SV_Web_AddToResponse( response, response->body, response->content_length );
	}
}

/*
* SV_Web_SendResponse
*/
static void SV_Web_SendResponse( sv_http_connection_t *con )
{
	size_t total_sent = 0;
	sv_http_query_chunk_t *chunk;
	sv_http_response_t *response = &con->response;

send_data:
	while( response->chunk_head && response->size ) {
		int sent;

		chunk = response->chunk_head;
		response->chunk_head = chunk->next;

		sent = NET_Send( &con->socket, chunk->data, chunk->size, &con->address );
		if( sent < 0 ) {
			Com_DPrintf( "HTTP transmission error to %s\n", NET_AddressToString( &con->address ) );
			con->open = qfalse;
			break;
		}

		total_sent += sent;
		response->size -= sent;
		con->last_active = Sys_Milliseconds();

		SV_Web_FreeChunk( chunk );
		if( !sent ) {
			break;
		}
		if( total_sent >= HTTP_SENDBUF_SIZE ) {
			break;
		}
	}

	if( !response->chunk_head ) {
		response->chunk_tail = response->chunk_head = SV_Web_AllocChunk();
	}

	// send at least HTTP_SENDBUF_SIZE bytes, if possible
	if( total_sent < HTTP_SENDBUF_SIZE ) {
		// read from file
		if( response->file ) {
			qbyte buf[HTTP_SENDBUF_SIZE];
			int read, max_read;
			
			max_read = sizeof( buf ) - total_sent;
			read = FS_Read( buf, max_read, response->file );
			if( read < max_read ) {
				FS_FCloseFile( response->file );
				response->file = 0;
			}

			if( read ) {
				SV_Web_AddToResponse( response, buf, read );
				goto send_data;
			}
		}
	}

	if( !response->size && !response->file ) {
		con->state = HTTP_CONN_STATE_RECV;
	}
}

/*
* SV_Web_Init
*/
void SV_Web_Init( void )
{
	netadr_t address;
	netadr_t ipv6_address;

	sv_http_initialized = qfalse;

	SV_Web_InitConnections();

	if( !sv_http->integer ) {
		return;
	}

	address.type = NA_NOTRANSMIT;
	ipv6_address.type = NA_NOTRANSMIT;

	NET_StringToAddress( sv_ip->string, &address );
	NET_SetAddressPort( &address, sv_http_port->integer );

	if( dedicated->integer || sv_maxclients->integer > 1 )
	{
		if( !NET_OpenSocket( &svs.socket_http, SOCKET_TCP, &address, qtrue ) )
		{
			Com_Printf( "Error: Couldn't open TCP socket: %s", NET_ErrorString() );
		}
		else if( !NET_Listen( &svs.socket_http ) )
		{
			Com_Printf( "Error: Couldn't listen to TCP socket: %s", NET_ErrorString() );
			NET_CloseSocket( &svs.socket_http );
		}
		else
		{
			Com_Printf( "Web server started on %s\n", NET_AddressToString( &address ) );
		}
	}

	sv_http_initialized = (svs.socket_http.type == NA_IP || svs.socket_http6.type == NA_IP6);
}

/*
* SV_Web_Frame
*/
void SV_Web_Frame( void )
{
	int ret;
	socket_t socket;
	netadr_t address;
	sv_http_connection_t *con, *next, *hnode;

	if( !sv_http_initialized ) {
		return;
	}

	// accept new connections
	while( ( ret = NET_Accept( &svs.socket_http, &socket, &address ) ) )
	{
		int i;
		client_t *cl;

		if( ret == -1 )
		{
			Com_Printf( "NET_Accept: Error: %s", NET_ErrorString() );
			continue;
		}

		// only accept connections from existing clients
		con = NULL;
		for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		{
			// TODO: only allow 2 simultaneous connections from each client
#if 0
	 		if( cl->state == CS_FREE )
				continue;

			if( NET_CompareAddress( &address, &cl->netchan.remoteAddress ) )
#endif
			{
				Com_DPrintf( "HTTP connection accepted from %s\n", NET_AddressToString( &address ) );
				con = SV_Web_AllocConnection();
				con->socket = socket;
				con->address = address;
				con->last_active = Sys_Milliseconds();
				con->open = qtrue;
				con->state = HTTP_CONN_STATE_RECV;
				break;
			}
		}

		if( !con ) {
			Com_DPrintf( "Refused HTTP connection from %s\n", NET_AddressToString( &address ) );
			NET_CloseSocket( &socket );
		}
	}

	// handle incoming data
	hnode = &sv_http_connection_headnode;
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->next;

		switch( con->state ) {
			case HTTP_CONN_STATE_RECV:
				SV_Web_ReceiveRequest( con );

				if( con->state == HTTP_CONN_STATE_SEND ) {
					SV_Web_RespondToQuery( con );
				}
				break;
			case HTTP_CONN_STATE_SEND:
				SV_Web_SendResponse( con );

				if( con->state != HTTP_CONN_STATE_SEND ) {
					if( con->close_after_resp ) {
						con->open = qfalse;
					}
					else {
						SV_Web_ResetRequest( &con->request );
					}
				}
				break;
			default:
				Com_DPrintf( "Bad connection state: %i\n", con->state );
				con->open = qfalse;
				break;
		}
	}

	// close dead connections
	hnode = &sv_http_connection_headnode;
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->next;

		if( Sys_Milliseconds() > con->last_active + INCOMING_HTTP_CONNECTION_TIMEOUT*1000 ) {
			con->open = qfalse;
			Com_DPrintf( "HTTP connection timeout from %s\n", NET_AddressToString( &con->address ) );
		}

		if( !con->open ) {
			NET_CloseSocket( &con->socket );
			SV_Web_FreeConnection( con );
		}
	}
}

/*
* SV_Web_Shutdown
*/
void SV_Web_Shutdown( void )
{
	if( !sv_http_initialized ) {
		return;
	}

	SV_Web_ShutdownConnections();

	NET_CloseSocket( &svs.socket_http );
	NET_CloseSocket( &svs.socket_http6 );

	sv_http_initialized = qfalse;
}

#else

/*
* SV_Web_Init
*/
void SV_Web_Init( void )
{
}

/*
* SV_Web_Frame
*/
void SV_Web_Frame( void )
{
}

/*
* SV_Web_Shutdown
*/
void SV_Web_Shutdown( void )
{
}


#endif // HTTP_SUPPORT
