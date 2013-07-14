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

// wswcurl encapsulation "class", which does per-module tracking of
// asynchronous HTTP requess.

// "module" is an object which is decribed by a triplet of: 1) name and 2+3) memory
// allocation and freeing functions. name isn't supposed to be unique,
// it merely exists to make debugging somewhat more convenient

// module owner is supposed to shutdown the module at proper time to interrupt
// all currently running requests and allow resources to be freed.
// the module can not be used to initialize any further requests as it is also
// freed during the shutdown procedure

typedef void *(*async_stream_alloc_t)( size_t size, const char *filename, int fileline );
typedef void (*async_stream_free_t)( void *data, const char *filename, int fileline );

// read and done callbacks
// status - HTTP/FTP response code
// contentType represents value read from the Content-Type: field by libcurl:
// A value of NULL indicates that the server didn't send a valid Content-Type header or that the protocol used doesn't support this.

typedef size_t (*async_stream_read_cb_t)(const void *buf, size_t numb, float percentage, 
	int status, const char *contentType, void *privatep);
typedef void (*async_stream_done_cb_t)(int status, const char *contentType, void *privatep);
typedef void (*async_stream_header_cb_t)(const void *buf, void *privatep);

typedef struct async_stream_module_s async_stream_module_t;

//==============================================

/*
* AsyncStream_InitModule
*/
async_stream_module_t *AsyncStream_InitModule( const char *name, async_stream_alloc_t alloc_f, async_stream_free_t free_f );

/*
* AsyncStream_ShutdownModule
*
* Interrupt all currently running requests, let the module perform the shutdown properly.
* Done-callback gets called with status value of -2 for each request.
*/
void AsyncStream_ShutdownModule( async_stream_module_t *module );

/*
* AsyncStream_UrlDecode
*
* Converts the given null-terminated string to an URL encoded null-terminated string.
*/
void AsyncStream_UrlEncode( const char *src, char *dst, size_t size );

/*
* AsyncStream_UrlDecode
*
* Converts the given URL-encoded string to a null-terminated plain string. Returns 
* total (untruncated) length of the resulting string.
*/
size_t AsyncStream_UrlDecode( const char *src, char *dst, size_t size );

/*
* AsyncStream_UrlEncodeUnsafeChars
*
* Only "unsafe" subset of characters are encoded.
*/
void AsyncStream_UrlEncodeUnsafeChars( const char *src, char *dst, size_t size );

/*
* AsyncStream_PerformRequestExt
*
* Performs asynchronous HTTP request to specified URL. In case of GET method, data
* is appended to the URL, in case of POST it is sent as the post data. Data is assumed
* to be URL-encoded.
* Timeout should be specified in seconds.
* Read callback is called whenever there's data to be read from the request. Done callback
* is called upon completion of the request. A negative value indicates an error (usually a 
* negated CURL error code).
* Caller may also specify a private pointer to be passed to callback functions.
*
* Returns 0 on success.
*/
int AsyncStream_PerformRequestExt( async_stream_module_t *module, const char *url, const char *method, const char *data, 
	const char **headers, int timeout, int resumeFrom, 
	async_stream_read_cb_t read_cb, async_stream_done_cb_t done_cb, async_stream_header_cb_t header_cb,
	void *privatep );

int AsyncStream_PerformRequest( async_stream_module_t *module, const char *url, const char *method, const char *data, 
	const char *referer, int timeout, int resumeFrom, async_stream_read_cb_t read_cb, async_stream_done_cb_t done_cb, void *privatep );
