/*
Copyright (C) 2011 Victor Luchits

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
#include "wswcurl.h"
#include "asyncstream.h"

typedef struct async_stream_handler_s {
	async_stream_read_cb_t read_cb;
	async_stream_done_cb_t done_cb;
	async_stream_header_cb_t header_cb;
	wswcurl_req *request;
	void *privatep;

	struct async_stream_module_s *module;
	struct async_stream_handler_s *next, *prev;
} async_stream_handler_t;

struct async_stream_module_s {
	async_stream_handler_t *root_handler;
	char *name;
	async_stream_alloc_t alloc_f;
	async_stream_free_t free_f;
};

//==============================================

/*
* AsyncStream_InitModule
*/
async_stream_module_t *AsyncStream_InitModule( const char *name, async_stream_alloc_t alloc_f, async_stream_free_t free_f ) {
	size_t name_size;
	async_stream_module_t *module;

	assert( name );
	assert( alloc_f );
	assert( free_f );

	name_size = strlen( name ) + 1;

	// allocate and initialize module, store pointers
	module = ( async_stream_module_t * ) alloc_f( sizeof( *module ) + name_size, __FILE__, __LINE__ );
	module->name = ( char * )( ( uint8_t * )module + sizeof( *module ) );
	Q_strncpyz( module->name, name, name_size );
	module->alloc_f = alloc_f;
	module->free_f = free_f;
	module->root_handler = NULL;

	return module;
}

/*
* AsyncStream_ReadCallback
*/
static size_t AsyncStream_ReadCallback( wswcurl_req *req, const void *buf, size_t numb, float percentage, void *privatep ) {
	async_stream_handler_t *handler;

	handler = ( async_stream_handler_t * )privatep;
	assert( handler );
	if( !handler ) {
		return 0;
	}

	if( handler->read_cb ) {
		int status = wswcurl_get_status( req );
		const char *contentType = wswcurl_get_content_type( req );
		return handler->read_cb( buf, numb, percentage, status, contentType, handler->privatep );
	}

	return numb;
}

/*
* AsyncStream_DoneCallback
*/
static void AsyncStream_DoneCallback( wswcurl_req *req, int status, void *privatep ) {
	async_stream_module_t *module;
	async_stream_handler_t *handler;

	handler = ( async_stream_handler_t * )privatep;
	assert( handler );
	if( !handler ) {
		return;
	}

	module = handler->module;
	assert( module );
	if( !module ) {
		return;
	}

	// unlink from the list
	if( module->root_handler == handler ) {
		module->root_handler = handler->next;
	}
	if( handler->prev ) {
		handler->prev->next = handler->next;
	}
	if( handler->next ) {
		handler->next->prev = handler->prev;
	}

	if( handler->done_cb ) {
		const char *contentType = wswcurl_get_content_type( req );
		handler->done_cb( status, contentType, handler->privatep );
	}

	module->free_f( handler, __FILE__, __LINE__ );

	wswcurl_delete( req );
}

/*
* AsyncStream_HeaderCallback
*/
static void AsyncStream_HeaderCallback( wswcurl_req *req, const char *buf, void *privatep ) {
	async_stream_handler_t *handler;

	handler = ( async_stream_handler_t * )privatep;
	assert( handler );
	if( !handler ) {
		return;
	}

	if( handler->header_cb ) {
		handler->header_cb( buf, handler->privatep );
	}
}

/*
* AsyncStream_UrlEncode
*/
void AsyncStream_UrlEncode( const char *src, char *dst, size_t size ) {
	wswcurl_urlencode( src, dst, size );
}

/*
* AsyncStream_UrlDecode
*/
size_t AsyncStream_UrlDecode( const char *src, char *dst, size_t size ) {
	return Q_urldecode( src, dst, size );
}

/*
* AsyncStream_UrlEncodeUnsafeChars
*/
void AsyncStream_UrlEncodeUnsafeChars( const char *src, char *dst, size_t size ) {
	Q_urlencode_unsafechars( src, dst, size );
}

/*
* AsyncStream_PerformRequestExt
*/
int AsyncStream_PerformRequestExt( async_stream_module_t *module, const char *url, const char *method,
								   const char *data,
								   const char **headers, int timeout, int resumeFrom,
								   async_stream_read_cb_t read_cb, async_stream_done_cb_t done_cb, async_stream_header_cb_t header_cb,
								   void *privatep ) {
	const char *postfields;
	wswcurl_req *request;
	async_stream_handler_t *handler;

	assert( module );
	if( !module ) {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest: no module specified.\n" );
		return -1000;
	}

	if( !url ) {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest(\"%s\"): missing URL parameter.\n", module->name );
		return -1;
	}
	if( !method ) {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest(\"%s\"): missing method parameter.\n", module->name );
		return -2;
	}
	if( !data ) {
		data = "";
	}

	request = NULL;
	postfields = NULL;

	if( !Q_stricmp( method, "GET" ) ) {
		const char *sep;

		// append data to query string
		sep = strchr( url, '?' );
		request = wswcurl_create( NULL, "%s%s%s", url, sep ? "&" : "?", data );
		postfields = NULL;
	} else if( !Q_stricmp( method, "POST" ) ) {
		request = wswcurl_create( NULL, "%s", url );
		postfields = data;
	} else {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest(\"%s\"): unsupported method \"%s\".\n", module->name, method );
		return -3;
	}

	if( !request ) {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest(\"%s\"): failed to create HTTP request instance.\n", module->name );
		return -100;
	}

	// allocate local handler
	if( module->alloc_f ) {
		handler = ( async_stream_handler_t * ) module->alloc_f( sizeof( *handler ), __FILE__, __LINE__ );
	} else {
		handler = NULL;
	}

	if( !handler ) {
		Com_Printf( S_COLOR_YELLOW "AsyncStream_PerformRequest(\"%s\"): failed to allocate handler.\n", module->name );
		return -100;
	}

	// initialize
	handler->read_cb = read_cb;
	handler->done_cb = done_cb;
	handler->header_cb = header_cb;
	handler->privatep = privatep;
	handler->request = request;
	handler->module = module;

	// add to linked list
	handler->prev = NULL;
	handler->next = module->root_handler;
	if( handler->next ) {
		handler->next->prev = handler;
	}
	module->root_handler = handler;

	// set CURL opts and callbacks
	if( postfields ) {
		wswcurl_set_postfields( request, postfields, strlen( postfields ) );
	}
	if( headers ) {
		int i, j;

		for( i = 0, j = 1; headers[i] && headers[j]; i += 2, j += 2 ) {
			wswcurl_header( request, headers[i], headers[j] );
		}
	}
	if( resumeFrom ) {
		wswcurl_set_resume_from( request, resumeFrom );
	}

	wswcurl_set_timeout( request, timeout );
	wswcurl_stream_callbacks( request, AsyncStream_ReadCallback, AsyncStream_DoneCallback,
							  AsyncStream_HeaderCallback, handler );

	// start
	wswcurl_start( request );

	return 0;
}

/*
* AsyncStream_PerformRequest
*/
int AsyncStream_PerformRequest( async_stream_module_t *module, const char *url, const char *method, const char *data,
								const char *referer, int timeout, int resumeFrom, async_stream_read_cb_t read_cb, async_stream_done_cb_t done_cb,
								void *privatep ) {
	const char *headers[3] = { NULL, NULL, NULL };

	headers[0] = "Referer";
	headers[1] = referer;

	return AsyncStream_PerformRequestExt( module, url, method, data, headers,
										  timeout, resumeFrom, read_cb, done_cb, NULL, privatep );
}

/*
* AsyncStream_ShutdownModule
*/
void AsyncStream_ShutdownModule( async_stream_module_t *module ) {
	async_stream_free_t free_f;

	//assert( module );
	if( !module ) {
		return;
	}

	free_f = module->free_f;
	while( module->root_handler ) {
		AsyncStream_DoneCallback( module->root_handler->request, -2, module->root_handler );
	}

	free_f( module, __FILE__, __LINE__ );
}
