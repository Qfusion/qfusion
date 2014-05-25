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

#ifdef HTTP_SUPPORT

#define MAX_INCOMING_HTTP_CONNECTIONS			48
#define MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR	3

#define MAX_INCOMING_CONTENT_LENGTH				0x2800

#define INCOMING_HTTP_CONNECTION_RECV_TIMEOUT	5 // seconds
#define INCOMING_HTTP_CONNECTION_SEND_TIMEOUT	15 // seconds

typedef enum
{
	HTTP_CONN_STATE_NONE = 0,
	HTTP_CONN_STATE_RECV = 1,
	HTTP_CONN_STATE_RESP = 2,
	HTTP_CONN_STATE_SEND = 3
} sv_http_connstate_t;
	
typedef struct {
	long begin;
	long end;
} sv_http_content_range_t;

typedef struct {
	size_t header_length;
	char header_buf[0x4000];
	size_t header_buf_p;
	qboolean header_done;

	char *content;
	size_t content_p;
	size_t content_length;
	sv_http_content_range_t content_range;
} sv_http_stream_t;

typedef struct {
	http_query_method_t method;
	http_response_code_t error;
	sv_http_stream_t stream;

	char *method_str;
	char *resource;
	char *query_string;
	char *http_ver;

	int clientNum;
	char *clientSession;
	netadr_t realAddr;

	qboolean partial;
	sv_http_content_range_t partial_content_range;

	qboolean got_start_line;
	qboolean close_after_resp;
} sv_http_request_t;

typedef struct {
	http_response_code_t code;
	sv_http_stream_t stream;

	int file;
	size_t file_send_pos;
	size_t file_chunk_size;
	char *filename;
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

	qboolean is_upstream;

	struct sv_http_connection_s *next, *prev;
} sv_http_connection_t;

static qboolean sv_http_initialized = qfalse;
static sv_http_connection_t sv_http_connections[MAX_INCOMING_HTTP_CONNECTIONS];
static sv_http_connection_t sv_http_connection_headnode, *sv_free_http_connections;

static socket_t sv_socket_http;
static socket_t sv_socket_http6;

static netadr_t sv_web_upstream_addr;
static qboolean sv_web_upstream_is_set;

// ============================================================================

/*
* SV_Web_ResetStream
*/
static void SV_Web_ResetStream( sv_http_stream_t *stream )
{
	stream->header_done = qfalse;
	stream->header_length = 0;
	stream->header_buf_p = 0;

	if( stream->content && 
		( stream->content < stream->header_buf 
			|| stream->content >= stream->header_buf + sizeof( stream->header_buf ) ) ) {
		Mem_Free( stream->content );
	}

	stream->content_range.begin = stream->content_range.end = 0;

	stream->content = NULL;
	stream->content_length = 0;
	stream->content_p = 0;
}

/*
* SV_Web_ResetRequest
*/
static void SV_Web_ResetRequest( sv_http_request_t *request )
{
	if( request->method_str ) {
		Mem_Free( request->method_str );
		request->method_str = NULL;
	}
	if( request->resource ) {
		Mem_Free( request->resource );
		request->resource = NULL;
	}
	if( request->http_ver ) {
		Mem_Free( request->http_ver );
		request->http_ver = NULL;
	}

	if( request->clientSession ) {
		Mem_Free( request->clientSession );
		request->clientSession = NULL;
	}

	request->query_string = "";
	SV_Web_ResetStream( &request->stream );
	
	NET_InitAddress( &request->realAddr, NA_NOTRANSMIT );

	request->partial = qfalse;
	request->close_after_resp = qfalse;
	request->got_start_line = qfalse;
	request->error = HTTP_RESP_NONE;
	request->clientNum = -1;
}

/*
* SV_Web_ResetResponse
*/
static void SV_Web_ResetResponse( sv_http_response_t *response )
{
	if( response->filename ) {
		Mem_Free( response->filename );
		response->filename = NULL;
	}
	if( response->file ) {
		FS_FCloseFile( response->file );
		response->file = 0;
	}
	response->file_send_pos = 0;
	response->file_chunk_size = 0;

	SV_Web_ResetStream( &response->stream );

	response->code = HTTP_RESP_NONE;
}

/*
* SV_Web_AllocConnection
*/
static sv_http_connection_t *SV_Web_AllocConnection( void )
{
	sv_http_connection_t *con;

	if( sv_free_http_connections )
	{
		// take a free connection if possible
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
	con->is_upstream = qfalse;
	return con;
}

/*
* SV_Web_FreeConnection
*/
static void SV_Web_FreeConnection( sv_http_connection_t *con )
{
	SV_Web_ResetRequest( &con->request );
	SV_Web_ResetResponse( &con->response );

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
		next = con->prev;
		if( con->open ) {
			NET_CloseSocket( &con->socket );
			SV_Web_FreeConnection( con );
		}
	}
}

/*
* SV_Web_ConnectionLimitReached
*/
static unsigned SV_Web_ConnectionLimitReached( const netadr_t *addr )
{
	unsigned cnt;
	const sv_http_connection_t *con, *next;
	const sv_http_connection_t *hnode = &sv_http_connection_headnode;

	if( NET_IsLocalAddress( addr ) ) {
		return qfalse;
	}

	cnt = 0;
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->prev;
		if( NET_CompareAddress( addr, &con->address ) ) {
			if( cnt >= MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR ) {
				return qtrue;
			}
		}
		cnt++;
	}
	return qfalse;
}

/*
* SV_Web_Get
*/
static int SV_Web_Get( sv_http_connection_t *con, void *recvbuf, size_t recvbuf_size )
{
	int read;

	read = NET_Get( &con->socket, NULL, recvbuf, recvbuf_size - 1 );
	if( read < 0 ) {
		con->open = qfalse;
		Com_DPrintf( "HTTP connection recv error from %s\n", NET_AddressToString( &con->address ) );
	}
	return read;
}

/*
* SV_Web_Send
*/
static int SV_Web_Send( sv_http_connection_t *con, void *sendbuf, size_t sendbuf_size )
{
	int sent;

	sent = NET_Send( &con->socket, sendbuf, sendbuf_size, &con->address );
	if( sent < 0 ) {
		Com_DPrintf( "HTTP transmission error to %s\n", NET_AddressToString( &con->address ) );
		con->open = qfalse;
	}
	return sent;
}

// ============================================================================

/*
* SV_Web_ParseStartLine
*/
static void SV_Web_ParseStartLine( sv_http_request_t *request, char *line )
{
	char *ptr;
	char *token, *delim;

	ptr = line;

	token = ptr;
	ptr = strchr( token, ' ' );
	if( !ptr ) {
		request->error = HTTP_RESP_BAD_REQUEST;
		return;
	}
	*ptr = '\0';

	if( !Q_stricmp( token, "GET" ) ) {
		request->method = HTTP_METHOD_GET;
	} else if( !Q_stricmp( token, "POST" ) ) {
		request->method = HTTP_METHOD_POST;
	} else if( !Q_stricmp( token, "PUT" ) ) {
		request->method = HTTP_METHOD_PUT;
	} else if( !Q_stricmp( token, "HEAD" ) ) {
		request->method = HTTP_METHOD_HEAD;
	} else {
		request->error = HTTP_RESP_BAD_REQUEST;
	}

	request->method_str = ZoneCopyString( token );

	token = ptr + 1;
	while( *token <= ' ' || *token == '/' ) {
		token++;
	}
	ptr = strrchr( token, ' ' );
	if( !ptr ) {
		request->error = HTTP_RESP_BAD_REQUEST;
		return;
	}
	*ptr = '\0';

	request->resource = ZoneCopyString( *token ? token : "/" );
	Q_urldecode( request->resource, request->resource, strlen( request->resource ) + 1 );

	// split resource into filepath and query string
	delim = strstr( request->resource, "?" );
	if( delim ) {
		*delim = '\0';
		request->query_string = delim + 1;
	}

	token = ptr + 1;
	while( *token <= ' ' ) {
		token++;
	}
	request->http_ver = ZoneCopyString( token );

	// check for HTTP/1.1 and greater
	if( strncmp( request->http_ver, "HTTP/", 5 ) ) {
		request->error = HTTP_RESP_BAD_REQUEST;
	} else if( (int)(atof( request->http_ver + 5 )*10) < 11 ) {
		request->error = HTTP_RESP_BAD_REQUEST;
	}
}

/*
* SV_Web_AnalyzeHeader
*/
static void SV_Web_AnalyzeHeader( sv_http_request_t *request, const char *key, const char *value )
{
	sv_http_stream_t *stream = &request->stream;

	//
	// store valuable information for quicker access
	if( !Q_stricmp( key, "Content-Length" ) ) {
		stream->content_length = atoi( value );
		if( stream->content_length > MAX_INCOMING_CONTENT_LENGTH ) {
			request->error = HTTP_RESP_REQUEST_TOO_LARGE;
		}
	}
	else if( !Q_stricmp( key, "Connection" ) ) {
		if( !Q_stricmp( value, "close" ) ) {
			request->close_after_resp = qtrue;
		}
	}
	else if( !Q_stricmp( key, "Host" ) ) {
		// valid HTTP 1.1 request must contain Host header
		if ( !value || !*value ) {
			request->error = HTTP_RESP_BAD_REQUEST;
		}
	}
	else if( !Q_stricmp( key, "Range" ) 
		&& ( request->method == HTTP_METHOD_GET || request->method == HTTP_METHOD_HEAD ) ) {
		const char *delim = strchr( value, '-' );

		if( Q_strnicmp( value, "bytes=", 6 ) || !delim ) {
			request->error = HTTP_RESP_BAD_REQUEST;
		}
		else {
			qboolean neg_end = qfalse;
			const char *p = value;

			// first byte pos
			while( *p && p < delim ) {
				if( *p >= '0' && *p <= '9' )
					stream->content_range.begin = stream->content_range.begin*10 + *p - '0';
				p++;
			}
			p++;

			// last byte pos
			if( *p == '-' ) {
				if( value != delim ) {
					request->error = HTTP_RESP_REQUESTED_RANGE_NOT_SATISFIABLE;
					return;
				}
				neg_end = qtrue;
				p++;
			}
			while( *p ) {
				if( *p >= '0' && *p <= '9' )
				stream->content_range.end = stream->content_range.end*10 + *p++ - '0';
			}

			// partial content request
			if( neg_end && stream->content_range.end ) {
				// bytes=-100
				request->partial = qtrue;
				stream->content_range.end = -stream->content_range.end;
			} else if( stream->content_range.end >= stream->content_range.begin ) {
				// bytes=200-300
				request->partial = qtrue;
			} else if( stream->content_range.begin >= 0 && *(delim+1) == '\0' ) {
				// bytes=200-
				request->partial = qtrue;
			}

			if( request->partial ) {
				request->partial_content_range = stream->content_range;
			}
		}
	} else if( !Q_stricmp( key, "X-Client" ) ) {
		request->clientNum = atoi( value );
	} else if( !Q_stricmp( key, "X-Session" ) ) {
		request->clientSession = ZoneCopyString( value );
	} else if( !Q_stricmp( key, sv_http_upstream_realip_header->string ) ) {
		NET_StringToAddress( value, &request->realAddr );
	}
}

/*
* SV_Web_ParseHeaderLine
*
* Parses and splits the header line into key-value pair
*/
static void SV_Web_ParseHeaderLine( sv_http_request_t *request, char *line )
{
	char *value;
	size_t offset;
	const char *colon;
	const char *key;

	if( request->error ) {
		return;
	}

	colon = strchr( line, ':' );
	if( !colon ) {
		return;
	}

	offset = colon - line;
	line[offset] = '\0';
	key = Q_trim( line );
	value = line + offset + 1;

	// ltrim
	while( *value <= ' ' ) {
		value++;
	}
	SV_Web_AnalyzeHeader( request, key, value );
}

/*
* SV_Web_ParseHeaders
*/
static size_t SV_Web_ParseHeaders( sv_http_request_t *request, char *data )
{
	char *line, *p;

	line = data;
	while( (p = strstr( line, "\r\n" )) != NULL ) {
		if( p == line ) {
			line = p + 2;
			request->stream.header_done = qtrue;
			break;
		}

		*p = *(p+1) = '\0';

		if( request->got_start_line ) {
			SV_Web_ParseHeaderLine( request, line );
		}
		else {
			SV_Web_ParseStartLine( request, line );
			request->got_start_line = qtrue;
		}

		line = p + 2;
	}
	return (line - data);
}

/*
* SV_Web_ReceiveRequest
*/
static void SV_Web_ReceiveRequest( socket_t *socket, sv_http_connection_t *con )
{
	int ret = 0;
	char *recvbuf;
	size_t recvbuf_size;
	sv_http_request_t *request = &con->request;
	size_t total_received = 0;

	while( !request->stream.header_done ) {
		char *end;
		size_t rem;
		size_t advance;

		recvbuf = request->stream.header_buf + request->stream.header_buf_p;
		recvbuf_size = sizeof( request->stream.header_buf ) - request->stream.header_buf_p;
		if( recvbuf_size <= 1 ) {
			request->error = HTTP_RESP_BAD_REQUEST;
			break;
		}

		ret = SV_Web_Get( con, recvbuf, recvbuf_size - 1 );
		if( ret <= 0 ) {
			break;
		}

		total_received += ret;

		recvbuf[ret] = '\0';
		advance = SV_Web_ParseHeaders( request, request->stream.header_buf );
		if( !advance ) {
			request->stream.header_buf_p += ret;
			continue;
		}

		end = request->stream.header_buf + advance;
		rem = (request->stream.header_buf_p + ret) - advance;
		memmove( request->stream.header_buf, end, rem );
		request->stream.header_buf_p = rem;
		request->stream.header_length += advance;

		if( request->stream.header_length > MAX_INCOMING_CONTENT_LENGTH ) {
			request->error = HTTP_RESP_REQUEST_TOO_LARGE;
		}

		// request must come from a connected client with a valid session id
		if( !request->error && request->stream.header_done ) {
			// check real IP header value for upstream HTTP connections
			if( con->is_upstream && 
				(request->realAddr.type == NA_NOTRANSMIT || 	SV_Web_ConnectionLimitReached( &request->realAddr )) ) {
				request->error = HTTP_RESP_SERVICE_UNAVAILABLE;
			}
			else if( !SV_ClientAllowHttpRequest( request->clientNum, request->clientSession ) ) {
				request->error = HTTP_RESP_FORBIDDEN;
			}
		}

		if( request->error ) {
			break;
		}

		if( request->stream.header_done ) {
			con->close_after_resp = request->close_after_resp;

			if( request->stream.content_length ) {
				if( request->stream.content_length < sizeof( request->stream.header_buf ) ) {
					request->stream.content = request->stream.header_buf;
					request->stream.content_p = request->stream.header_buf_p;
				}
				else {
					request->stream.content = Mem_ZoneMallocExt( request->stream.content_length + 1, 0 );
					request->stream.content[request->stream.content_length] = 0;
					memcpy( request->stream.content, request->stream.header_buf, request->stream.header_buf_p );
					request->stream.content_p = request->stream.header_buf_p;
				}
			}
		}
	}

	if( request->stream.header_done && !request->error && request->stream.content_length ) {
		while( request->stream.content_length > request->stream.content_p ) {
			recvbuf = request->stream.content + request->stream.content_p;
			recvbuf_size = request->stream.content_length - request->stream.content_p;

			ret = SV_Web_Get( con, recvbuf, recvbuf_size );
			if( ret <= 0 ) {
				break;
			}

			total_received += ret;
			request->stream.content_p += ret;
		}
		if( request->stream.content_p >= request->stream.content_length ) {
			request->stream.content_p = request->stream.content_length;
			request->stream.content[request->stream.content_p] = '\0';
		}
	}

	if( total_received > 0 ) {
		con->last_active = Sys_Milliseconds();
	}

	if( request->error ) {
		con->close_after_resp = qtrue;
		con->state = HTTP_CONN_STATE_RESP;
	}
	else if( request->stream.header_done && request->stream.content_p >= request->stream.content_length ) {
		// yay, fully got the request
		con->state = HTTP_CONN_STATE_RESP;
	}

	if( ret == -1 ) {
		con->open = qfalse;
		Com_DPrintf( "HTTP connection error from %s\n", NET_AddressToString( &con->address ) );
	}
}

// ============================================================================

/*
* SV_Web_ResponseCodeMessage
*/
static const char *SV_Web_ResponseCodeMessage( http_response_code_t code )
{
	switch( code ) {
		case HTTP_RESP_OK: return "OK";
		case HTTP_RESP_PARTIAL_CONTENT: return "Partial Content";
		case HTTP_RESP_BAD_REQUEST: return "Bad Request";
		case HTTP_RESP_FORBIDDEN: return "Forbidden";
		case HTTP_RESP_NOT_FOUND: return "Not Found";
		case HTTP_RESP_REQUEST_TOO_LARGE: return "Request Entity Too Large";
		case HTTP_RESP_REQUESTED_RANGE_NOT_SATISFIABLE: return "Requested range not satisfiable";
		case HTTP_RESP_SERVICE_UNAVAILABLE: return "Service unavailable";
		default: return "Unknown Error";
	}
}

/*
* SV_Web_RouteRequest
*/
static void SV_Web_RouteRequest( const sv_http_request_t *request, sv_http_response_t *response, 
	char **content, size_t *content_length )
{
	const char *resource = request->resource;
	const char *query_string = request->query_string;

	*content = NULL;
	*content_length = 0;

	if( !resource ) {
		response->code = HTTP_RESP_BAD_REQUEST;
	}
	else if( !Q_strnicmp( resource, "game/", 5 ) ) {
		// request to game module
		if( ge ) {
			response->code = ge->WebRequest( request->method, resource + 5, query_string, content, content_length );
		}
		else {
			response->code = HTTP_RESP_NOT_FOUND;
		}
	} else if( !Q_strnicmp( resource, "files/", 6 ) ) {
		const char *filename, *extension;
		
		filename = resource + 6;
		response->filename = ZoneCopyString( filename );
		
		if( request->method == HTTP_METHOD_GET || request->method == HTTP_METHOD_HEAD ) {
			// check for malicious URL's
			if( !sv_uploads_http->integer || !COM_ValidateRelativeFilename( filename ) ) {
				response->code = HTTP_RESP_FORBIDDEN;
				return;
			}

			// only serve GET requests for pack and demo files
			extension = COM_FileExtension( filename );
			if( !extension || !*extension || 
				!(FS_CheckPakExtension( filename ) || !Q_stricmp( extension, APP_DEMO_EXTENSION_STR ) ) ) {
				response->code = HTTP_RESP_FORBIDDEN;
				return;
			}

			*content_length = FS_FOpenBaseFile( filename, &response->file, FS_READ );
			if( !response->file ) {
				response->code = HTTP_RESP_NOT_FOUND;
				*content_length = 0;
			}
			else {
				response->code = HTTP_RESP_OK;
			}
		}
		else {
			response->code = HTTP_RESP_BAD_REQUEST;
		}
	}
	else {
		response->code = HTTP_RESP_NOT_FOUND;
	}
}

/*
* SV_Web_RespondToQuery
*/
static void SV_Web_RespondToQuery( sv_http_connection_t *con )
{
	char err_body[1024];
	char *content = NULL;
	size_t header_length = 0;
	size_t content_length = 0;
	sv_http_request_t *request = &con->request;
	sv_http_response_t *response = &con->response;
	sv_http_stream_t *resp_stream = &response->stream;

	if( request->error ) {
		response->code = request->error;
	}
	else {
		SV_Web_RouteRequest( request, response, &content, &content_length );

		if( response->file ) {
			Com_Printf( "HTTP serving file '%s' to '%s'\n", response->filename, NET_AddressToString( &con->address ) );
		}

		// serve range requests
		if( request->partial && response->file ) {
			// seek to first byte pos and clamp the last byte pos to content length
			if( request->partial_content_range.begin > 0 ) { 
				FS_Seek( response->file, request->partial_content_range.begin, FS_SEEK_SET );
				// range.end may be set to 0 for 'bytes=100-' style requests
				response->stream.content_range.end = request->partial_content_range.end 
					? request->partial_content_range.end : content_length;

			} else if( request->partial_content_range.end < 0 ) {
				// seek to N last bytes in the file
				FS_Seek( response->file, request->partial_content_range.end, FS_SEEK_END );
				response->stream.content_range.end = -request->partial_content_range.end;
			}

			// Content-Range header values
			response->stream.content_range.begin = FS_Tell( response->file );
			response->stream.content_range.end = min( (int)content_length, response->stream.content_range.end );
			response->code = HTTP_RESP_PARTIAL_CONTENT;
		}

		if( request->method == HTTP_METHOD_HEAD && response->file ) {
			FS_FCloseFile( response->file );
			response->file = 0;
		}
	}

	Q_snprintfz( resp_stream->header_buf, sizeof( resp_stream->header_buf ), 
		"%s %i %s\r\nServer: " APPLICATION " v" APP_VERSION_STR "\r\n", 
		request->http_ver, response->code, SV_Web_ResponseCodeMessage( response->code ) );

	Q_strncatz( resp_stream->header_buf, "Accept-Ranges: bytes\r\n", 
			sizeof( resp_stream->header_buf ) );

	if( response->code == HTTP_RESP_REQUESTED_RANGE_NOT_SATISFIABLE ) {
		int file_length = FS_FOpenBaseFile( request->resource, NULL, FS_READ );

		// in accordance with RFC 2616, send the Content-Range entity header,
		// specifying the length of the resource
		if( file_length < 0 ) {
			Q_strncatz( resp_stream->header_buf, "Content-Range: bytes */*\r\n",
				sizeof( resp_stream->header_buf ) );
		}
		else {
			Q_strncatz( resp_stream->header_buf, va( "Content-Range: bytes */%i\r\n", content_length ),
				sizeof( resp_stream->header_buf ) );
		}
	}
	else if( response->code == HTTP_RESP_PARTIAL_CONTENT ) {
		Q_strncatz( resp_stream->header_buf, va( "Content-Range: bytes %i-%i/%i\r\n", 
			response->stream.content_range.begin, response->stream.content_range.end, content_length ),
			sizeof( resp_stream->header_buf ) );
		content_length = response->stream.content_range.end - response->stream.content_range.begin;
	}

	if( response->code >= HTTP_RESP_BAD_REQUEST || !content_length ) {
		// error response or empty response: just return response code + description
		Q_strncatz( resp_stream->header_buf, "Content-Type: text/plain\r\n",
				sizeof( resp_stream->header_buf ) );

		Q_snprintfz( err_body, sizeof( err_body ), 
			va( "%i %s\n", response->code, SV_Web_ResponseCodeMessage( response->code ) ) );
		content = err_body;
		content_length = strlen( err_body );
	}

	// resource length
	Q_strncatz( resp_stream->header_buf, va( "Content-Length: %i\r\n", content_length ),
			sizeof( resp_stream->header_buf ) );

	if( response->file ) {
		Q_strncatz( resp_stream->header_buf, 
			va( "Content-Disposition: attachment; filename=\"%s\"\r\n", COM_FileBase( response->filename ) ),
			sizeof( resp_stream->header_buf ) );
	}

	Q_strncatz( resp_stream->header_buf, "\r\n", sizeof( resp_stream->header_buf ) );

	header_length = strlen( resp_stream->header_buf );
	if( content && content_length ) {
		if( content_length + header_length < sizeof( resp_stream->header_buf ) ) {
			resp_stream->content = resp_stream->header_buf + header_length;
		}
		else {
			resp_stream->content = Mem_ZoneMallocExt( content_length, 0 );
		}
		memcpy( resp_stream->content, content, content_length );
	}
	resp_stream->header_length = header_length;
	resp_stream->content_length = content_length;
}

/*
* SV_Web_SendResponse
*/
static size_t SV_Web_SendResponse( sv_http_connection_t *con )
{
	int sent;
	char *sendbuf;
	size_t sendbuf_size;
	size_t total_sent = 0;
	sv_http_response_t *response = &con->response;
	sv_http_stream_t *stream = &response->stream;

	while( !stream->header_done ) {
		sendbuf = stream->header_buf + stream->header_buf_p;
		sendbuf_size = stream->header_length - stream->header_buf_p;

		sent = SV_Web_Send( con, sendbuf, sendbuf_size );
		if( sent <= 0 ) {
			break;
		}

		stream->header_buf_p += sent;
		if( stream->header_buf_p >= stream->header_length ) {
			stream->header_done = qtrue;
		}

		total_sent += sent;
	}

	if( stream->header_done && stream->content_length ) {
		while( stream->content_p < stream->content_length ) {
			if( response->file ) {
				if( response->file_send_pos >= response->file_chunk_size ) {
					// read from file
					int read_size;
					
					if( FS_Eof( response->file ) ){
						Com_DPrintf( "HTTP file streaming error: premature EOF on %s to %s\n", 
							response->filename, NET_AddressToString( &con->address ) );
						con->open = qfalse;
						break;
					}

					read_size = min( sizeof( stream->header_buf ), stream->content_length - stream->content_p );
					read_size = FS_Read( stream->header_buf, read_size, response->file );

					response->file_chunk_size = read_size;
					response->file_send_pos = 0;
				}
				sendbuf = stream->header_buf + response->file_send_pos;
				sendbuf_size = response->file_chunk_size - response->file_send_pos;
			}
			else if( stream->content ) {
				sendbuf = stream->content + stream->content_p;
				sendbuf_size = stream->content_length - stream->content_p;
			}
			else {
				break;
			}

			sent = SV_Web_Send( con, sendbuf, sendbuf_size );
			if( sent <= 0 ) {
				break;
			}

			stream->content_p += sent;
			total_sent += sent;
			if( response->file ) {
				response->file_send_pos += sent;
			}
		}
	}

	if( total_sent > 0 ) {
		con->last_active = Sys_Milliseconds();
	}

	// if done sending content body, make the transition to recieving state
	if( stream->header_done 
		&& (!stream->content || stream->content_p >= stream->content_length) 
		&& !response->file ) {
		con->state = HTTP_CONN_STATE_RECV;
	}

	return total_sent;
}

/*
* SV_Web_InitSocket
*/
static void SV_Web_InitSocket( const char *addrstr, netadrtype_t adrtype, socket_t *socket )
{
	netadr_t address;

	address.type = NA_NOTRANSMIT;

	NET_StringToAddress( addrstr, &address );
	NET_SetAddressPort( &address, sv_http_port->integer );

	if( address.type == adrtype )
	{
		if( !NET_OpenSocket( socket, SOCKET_TCP, &address, qtrue ) )
		{
			Com_Printf( "Error: Couldn't open TCP socket: %s", NET_ErrorString() );
		}
		else if( !NET_Listen( socket ) )
		{
			Com_Printf( "Error: Couldn't listen to TCP socket: %s", NET_ErrorString() );
			NET_CloseSocket( socket );
		}
		else
		{
			Com_Printf( "Web server started on %s\n", NET_AddressToString( &address ) );
		}
	}
}

/*
* SV_Web_Listen
*/
static void SV_Web_Listen( socket_t *socket )
{
	int i;
	int ret;
	socket_t newsocket;
	netadr_t newaddress;
	sv_http_connection_t *con;

	// accept new connections
	while( ( ret = NET_Accept( socket, &newsocket, &newaddress ) ) )
	{
		client_t *cl;
		qboolean block;
		qboolean is_upstream;

		if( ret == -1 )
		{
			Com_Printf( "NET_Accept: Error: %s\n", NET_ErrorString() );
			continue;
		}

		is_upstream = sv_web_upstream_is_set 
			&& NET_CompareBaseAddress( &newaddress, &sv_web_upstream_addr );
		block = qfalse;

		if( !NET_IsLocalAddress( &newaddress ) && !is_upstream )
		{
			// only accept connections from connected clients
			block = qtrue;
			for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
			{
	 			if( cl->state < CS_FREE )
					continue;
				if( NET_CompareBaseAddress( &newaddress, &cl->netchan.remoteAddress ) ) {		
					// only accept up to three HTTP connections per address
					block = SV_Web_ConnectionLimitReached( &newaddress );
					break;
				}
			}
		}
		
		if( !block ) {
			Com_DPrintf( "HTTP connection accepted from %s\n", NET_AddressToString( &newaddress ) );
			con = SV_Web_AllocConnection();
			if( !con ) {
				break;
			}
			con->socket = newsocket;
			con->address = newaddress;
			con->last_active = Sys_Milliseconds();
			con->open = qtrue;
			con->state = HTTP_CONN_STATE_RECV;
			con->is_upstream = is_upstream;
			continue;
		}

		Com_DPrintf( "HTTP connection refused for %s\n", NET_AddressToString( &newaddress ) );
		NET_CloseSocket( &newsocket );
	}
}

/*
* SV_Web_Init
*/
void SV_Web_Init( void )
{
	sv_http_initialized = qfalse;

	SV_Web_InitConnections();

	if( !sv_http->integer ) {
		return;
	}

	SV_Web_InitSocket( sv_http_ip->string[0] == '\0' ? sv_ip->string : sv_http_ip->string, NA_IP, &sv_socket_http );
	SV_Web_InitSocket( sv_http_ipv6->string[0] == '\0' ? sv_ip6->string : sv_http_ipv6->string, NA_IP6, &sv_socket_http6 );

	sv_http_initialized = (sv_socket_http.address.type == NA_IP || sv_socket_http6.address.type == NA_IP6);
}

/*
* SV_Web_Frame
*/
void SV_Web_Frame( void )
{
	sv_http_connection_t *con, *next, *hnode = &sv_http_connection_headnode;
	socket_t *sockets[MAX_INCOMING_HTTP_CONNECTIONS+1];
	void *connections[MAX_INCOMING_HTTP_CONNECTIONS];
	int num_sockets = 0;

	if( !sv_http_initialized ) {
		return;
	}

	sv_web_upstream_is_set = sv_http_upstream_ip->string[0] != '\0' && sv_http_upstream_baseurl->string[0] != '\0';
	NET_StringToAddress( sv_http_upstream_ip->string, &sv_web_upstream_addr );

	// accept new connections
	if( sv_socket_http.address.type == NA_IP ) {
		SV_Web_Listen( &sv_socket_http );
	}
	if( sv_socket_http6.address.type == NA_IP6 ) {
		SV_Web_Listen( &sv_socket_http6 );
	}

	// handle incoming data
	num_sockets = 0;
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->prev;
		switch( con->state ) {
			case HTTP_CONN_STATE_RECV:
				sockets[num_sockets] = &con->socket;
				connections[num_sockets] = con;
				num_sockets++;
				break;
			default:
				break;
		}
	}
	sockets[num_sockets] = NULL;

	NET_Monitor( 0, sockets, (void (*)(socket_t *, void*))SV_Web_ReceiveRequest, NULL, connections );

	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->prev;

		switch( con->state ) {
			case HTTP_CONN_STATE_RECV:
				break;
			case HTTP_CONN_STATE_RESP:
				con->state = HTTP_CONN_STATE_SEND;
				SV_Web_RespondToQuery( con );

			case HTTP_CONN_STATE_SEND:
				SV_Web_SendResponse( con );

				if( con->state == HTTP_CONN_STATE_RECV ) {
					SV_Web_ResetResponse( &con->response );
					if( con->close_after_resp ) {
						con->open = qfalse;
					}
					else {
						SV_Web_ResetRequest( &con->request );
					}
				}
				break;
			default:
				Com_DPrintf( "Bad connection state %i\n", con->state );
				con->open = qfalse;
				break;
		}
	}

	// close dead connections
	for( con = hnode->prev; con != hnode; con = next )
	{
		next = con->prev;

		if( con->open ) {
			unsigned int timeout = 0;

			switch( con->state ) {
				case HTTP_CONN_STATE_RECV:
					timeout = INCOMING_HTTP_CONNECTION_RECV_TIMEOUT;
					break;
				case HTTP_CONN_STATE_SEND:
					timeout = INCOMING_HTTP_CONNECTION_SEND_TIMEOUT;
					break;
				default:
					break;
			}

			if( Sys_Milliseconds() > con->last_active + timeout*1000 ) {
				con->open = qfalse;
				Com_DPrintf( "HTTP connection timeout from %s\n", NET_AddressToString( &con->address ) );
			}
		}

		if( !con->open ) {
			NET_CloseSocket( &con->socket );
			SV_Web_FreeConnection( con );
		}
	}
}

/*
* SV_Web_Running
*/
qboolean SV_Web_Running( void )
{
	return sv_http_initialized;
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

	NET_CloseSocket( &sv_socket_http );
	NET_CloseSocket( &sv_socket_http6 );

	sv_http_initialized = qfalse;
}

/*
* SV_Web_UpstreamBaseUrl
*/
const char *SV_Web_UpstreamBaseUrl( void )
{
	return sv_http_upstream_baseurl->string;
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

/*
* SV_Web_Running
*/
qboolean SV_Web_Running( void )
{
	return qfalse;
}

/*
* SV_Web_UpstreamBaseUrl
*/
const char *SV_Web_UpstreamBaseUrl( void )
{
	return "";
}

#endif // HTTP_SUPPORT
