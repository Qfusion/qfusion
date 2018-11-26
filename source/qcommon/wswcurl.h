#pragma once

#define WSWCURL_STATUS_NONE     0
#define WSWCURL_STATUS_RUNNING  1
#define WSWCURL_STATUS_FINISHED 2

typedef struct wswcurl_req_s wswcurl_req;

typedef void (*wswcurl_done_cb)( struct wswcurl_req_s *req, int status, void *customp );
typedef size_t (*wswcurl_read_cb)( struct wswcurl_req_s *req, const void *buf, size_t numb,
								   float percentage, void *customp );
typedef void (*wswcurl_header_cb)( struct wswcurl_req_s *req, const char *buf, void *customp );

/**
 * initializes the memory pool, clears buffers, counters, etc
 */
void wswcurl_init( void );
/**
 * Creates a new curl environment for a specific url.
 * Note that no request is made, this just prepares the request.
 * This request can be modified by various functions available here.
 * The function can dynamicly format the url like printf. Note that the maximum URL size is 4kb
 */
wswcurl_req *wswcurl_create( const char *iface, const char *furl, ... );
/**
 * Starts previously created wswcurl_req request
 */
void wswcurl_start( wswcurl_req *req );
/**
 * Blocking call that waits to get the header, then tries to
 * read the content-length. returns 0 if no-can-do
 */
size_t wswcurl_getsize( wswcurl_req *req, size_t *rxreceived );
/**
 * Sets callbacks to read the data in streaming-mode, no data will be stored by wswcurl
 */
void wswcurl_stream_callbacks( wswcurl_req *req, wswcurl_read_cb read_cb, wswcurl_done_cb done_cb,
							   wswcurl_header_cb header_cb, void *customp );
/**
 * Read 'size' bytes to buffer. Blocking call that waits for as long as buffer is filled
 * or EOF is reached. Returns the number of bytes written.
 * Note that this doesnt require done_cb, stream is done when less bytes are returned
 * than 'size'
 */
size_t wswcurl_read( wswcurl_req *req, void *buffer, size_t size );
/**
 * Lets curl handle all connection, treats all messages and returns how many connections are still active.
 * NOT thread-safe.
 */
int wswcurl_perform( void );
/**
 * Cancels and removes a http request
 */
void wswcurl_delete( wswcurl_req *req );
/**
 * Cleans up the warsow curl environment, canceling any existing connections
 */
void wswcurl_cleanup();
/**
 * Sets request timeout (in seconds)
 */
void wswcurl_set_timeout( wswcurl_req *req, int timeout );
/**
 * Offset in bytes to start the download from
 */
void wswcurl_set_resume_from( wswcurl_req *req, long resume );

/**
 * Add a header field in the format of "key: value" to the header
 * The value field acts like printf
 */
int wswcurl_header( wswcurl_req *req, const char *key, const char *value, ... );
/**
 * Add a form field to the request. Adding this makes the request a POST request.
 * The value field acts like printf.
 */
int wswcurl_formadd( wswcurl_req *req, const char *field, const char *value, ... );
/**
 * Add a raw field with size defined to the request. Adding this makes the request a POST request.
 */
int wswcurl_formadd_raw( wswcurl_req *req, const char *field, void *data, size_t size );
/**
 * Sets raw POST request data, expected to be in URL-encoded format.
 */
int wswcurl_set_postfields( wswcurl_req *req, const char *fields, size_t size );
/**
 * Converts the given null-terminated string to an URL encoded null-terminated string.
 */
void wswcurl_urlencode( const char *src, char *dst, size_t size );
/**
 * Checks if a handle is still known in the wswcurl pool.
 */
int wswcurl_isvalidhandle( wswcurl_req *req );
/**
 * Returns current position in stream.
 */
int wswcurl_tell( wswcurl_req *req );
/**
 * Returns 1 whether the request has been completed and there are no bytes buffered.
 */
int wswcurl_eof( wswcurl_req *req );
/**
 *
 */
void wswcurl_ignore_bytes( wswcurl_req *req, size_t nbytes );

const char *wswcurl_get_content_type( wswcurl_req *req );
const char *wswcurl_getip( wswcurl_req *req );
const char *wswcurl_errorstr( int status );
const char *wswcurl_get_url( const wswcurl_req *req );
const char *wswcurl_get_effective_url( wswcurl_req *req );
int wswcurl_get_status( const wswcurl_req *req );
