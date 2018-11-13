/*************************************************************************
 *             Easy to use async CURL wrapper for Warsow
 *
 * Author     : Bart Meuris (KoFFiE)
 * E-Mail     : bart.meuris@gmail.com
 *
 * Todo:
 *  - Add wait for download function.
 *  - Add file resuming for downloading maps etc.
 *  - Add timeouts.
 *
 *************************************************************************/

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "curl/curl.h"
#ifdef USE_OPENSSL
#include <openssl/crypto.h>
#endif
#include "wswcurl.h"
#include "qcommon.h"

///////////////////////
#define WMALLOC( x )      _Mem_Alloc( wswcurl_mempool, x, 0, 0, __FILE__, __LINE__ )
#define WREALLOC( x, y )  ( ( x ) ? Mem_Realloc( ( x ), ( y ) ) : WMALLOC( y ) )
#define WFREE( x )        Mem_Free( x )

// Curl setopt wrapper
#define CURLSETOPT( c,r,o,v ) { if( c ) { r = qcurl_easy_setopt( c,o,v ); if( r ) { printf( "\nCURL ERROR: %d: %s\n", r, qcurl_easy_strerror( r ) ); qcurl_easy_cleanup( c ) ; c = NULL; } } }
#define CURLDBG( x )

#define WCONNECTTIMEOUT     0
#define WTIMEOUT            200

// 400k of buffering at max
#define WMAXBUFFERING       102400 * 4
// 100k at least
#define WMINBUFFERING       102400

// the maximum number of curl_multi handles to be processed simultaneously
#define WMAXMULTIHANDLES    4

#define WSTATUS_NONE        0   // not started
#define WSTATUS_STARTED     1   // started
#define WSTATUS_FINISHED    2   // finished
#define WSTATUS_QUEUED      3   //  queued

///////////////////////
// Typedefs
typedef struct chained_buffer_s {

	size_t rxsize;  // size of buffer
	size_t rxoffset;    // read-head
	struct chained_buffer_s *next;
	char data[1];
} chained_buffer_t;

struct wswcurl_req_s {

	int status; // < 0 : error
	char *url;  // Url of the request

	// List of buffers, 1 for each received block
	// singly-linked list with head+tail
	chained_buffer_t *bhead;
	chained_buffer_t *btail;

	size_t rxreturned;  // Amount of bytes returned to callee
	size_t rxreceived;  // Amount of bytes received from stream
	size_t rx_expsize;  // Expected size of the file, 0 if unknown (this is possible!)

	long respcode;  // HTTP response code when request was handled completely.

	char headers_done;
	char paused;
	time_t last_action;
	time_t timeout;
	size_t ignore_bytes;

	// Custom pointer to pass to callback functions below
	void *customp;

	// Callback when done
	wswcurl_done_cb callback_done;
	// Callback when data is available
	wswcurl_read_cb callback_read;
	// Callback when header data is available
	wswcurl_header_cb callback_header;

	CURL *curl;     // Curl handle
	CURLcode res;   // Curl response code

	// Additional header stuff
	struct curl_slist *txhead;

	// Internal form stuff - see http://curl.haxx.se/libcurl/c/curl_formadd.html
	struct curl_httppost *post;
	struct curl_httppost *post_last;

	// Linked list stuff
	struct wswcurl_req_s *next;
	struct wswcurl_req_s *prev;
};

///////////////////////
// Function defines
static int wswcurl_checkmsg();
static size_t wswcurl_write( void *ptr, size_t size, size_t nmemb, void *stream );
static size_t wswcurl_readheader( void *ptr, size_t size, size_t nmemb, void *stream );
static void wswcurl_pause( wswcurl_req *req );
static void wswcurl_unpause( wswcurl_req *req );
static time_t wswcurl_now( void );

///////////////////////
// Local variables
static wswcurl_req *http_requests = NULL; // Linked list of active requests
static wswcurl_req *http_requests_hnode; // The item node in the list
static qmutex_t *http_requests_mutex = NULL;
static CURLM *curlmulti = NULL;     // Curl MULTI handle
static int curlmulti_num_handles = 0;

static struct mempool_s *wswcurl_mempool;
static CURL *curldummy = NULL;
static qmutex_t *curldummy_mutex = NULL;

static cvar_t *http_proxy;
static cvar_t *http_proxyuserpwd;

#ifdef USE_OPENSSL
static qmutex_t **crypto_mutexes = NULL;
static int crypto_num_mutexes = 0;
#endif

///////////////////////
// Symbols
static void *curlLibrary = NULL;
#ifdef USE_OPENSSL
static void *cryptoLibrary = NULL;
#endif

#ifdef LIBCURL_RUNTIME

static CURLFORMcode ( *qcurl_formadd )( struct curl_httppost **, struct curl_httppost **, ... );
static CURLcode (*qcurl_easy_setopt)( CURL *, CURLoption, ... );
static CURL *(*qcurl_easy_init)( void );
static char *(*qcurl_easy_escape)( CURL *, const char *, int );
static char *(*qcurl_easy_unescape)( CURL *, const char *, int, int * );
static void (*qcurl_free)( void * );
static CURLM *(*qcurl_multi_init)( void );
static CURLMcode (*qcurl_multi_cleanup)( CURLM * );
static CURLMcode (*qcurl_multi_perform)( CURLM *, int * );
static CURLMcode (*qcurl_multi_add_handle)( CURLM *, CURL * );
static CURLMcode (*qcurl_multi_remove_handle)( CURLM *, CURL * );
static struct curl_slist *(*qcurl_slist_append)( struct curl_slist *, const char * );
static void (*qcurl_slist_free_all)( struct curl_slist * );
static void (*qcurl_formfree)( struct curl_httppost * );
static void (*qcurl_easy_cleanup)( CURL * );
static CURLcode (*qcurl_easy_getinfo)( CURL *, CURLINFO, ... );
static const char *(*qcurl_easy_strerror)( CURLcode );
static CURLMsg *(*qcurl_multi_info_read)( CURLM *, int * );
static CURLcode (*qcurl_easy_pause)( CURL *, int );
static CURLcode (*qcurl_global_init)( long flags );
static void (*qcurl_global_cleanup)( void );

static dllfunc_t libcurlfuncs[] =
{
	{ "curl_formadd", ( void ** )&qcurl_formadd },
	{ "curl_easy_setopt", ( void ** )&qcurl_easy_setopt },
	{ "curl_easy_init", ( void ** )&qcurl_easy_init },
	{ "curl_easy_escape", ( void ** )&qcurl_easy_escape },
	{ "curl_easy_unescape", ( void ** )&qcurl_easy_unescape },
	{ "curl_free", ( void ** )&qcurl_free },
	{ "curl_multi_init", ( void ** )&qcurl_multi_init },
	{ "curl_multi_cleanup", ( void ** )&qcurl_multi_cleanup },
	{ "curl_multi_perform", ( void ** )&qcurl_multi_perform },
	{ "curl_multi_add_handle", ( void ** )&qcurl_multi_add_handle },
	{ "curl_multi_remove_handle", ( void ** )&qcurl_multi_remove_handle },
	{ "curl_slist_append", ( void ** )&qcurl_slist_append },
	{ "curl_slist_free_all", ( void ** )&qcurl_slist_free_all },
	{ "curl_formfree", ( void ** )&qcurl_formfree },
	{ "curl_easy_cleanup", ( void ** )&qcurl_easy_cleanup },
	{ "curl_easy_getinfo", ( void ** )&qcurl_easy_getinfo },
	{ "curl_easy_strerror", ( void ** )&qcurl_easy_strerror },
	{ "curl_multi_info_read", ( void ** )&qcurl_multi_info_read },
	{ "curl_easy_pause", ( void ** )&qcurl_easy_pause },
	{ "curl_global_init", ( void ** )&qcurl_global_init },
	{ "curl_global_cleanup", ( void ** )&qcurl_global_cleanup },

	{ NULL, NULL }
};

#else

#define qcurl_formadd curl_formadd
#define qcurl_easy_setopt curl_easy_setopt
#define qcurl_easy_init curl_easy_init
#define qcurl_easy_escape curl_easy_escape
#define qcurl_easy_unescape curl_easy_unescape
#define qcurl_free curl_free
#define qcurl_multi_init curl_multi_init
#define qcurl_multi_cleanup curl_multi_cleanup
#define qcurl_multi_perform curl_multi_perform
#define qcurl_multi_add_handle curl_multi_add_handle
#define qcurl_multi_remove_handle curl_multi_remove_handle
#define qcurl_slist_append curl_slist_append
#define qcurl_slist_free_all curl_slist_free_all
#define qcurl_formfree curl_formfree
#define qcurl_easy_cleanup curl_easy_cleanup
#define qcurl_easy_getinfo curl_easy_getinfo
#define qcurl_easy_strerror curl_easy_strerror
#define qcurl_multi_info_read curl_multi_info_read
#define qcurl_easy_pause curl_easy_pause
#define qcurl_global_init curl_global_init
#define qcurl_global_cleanup curl_global_cleanup

#endif

#ifdef USE_OPENSSL

#ifdef LIBCRYPTO_RUNTIME

static int ( *qCRYPTO_num_locks )( void );
static void ( *qCRYPTO_set_locking_callback )( void ( *func )( int mode, int type, const char *file, int line ) );
static dllfunc_t libcryptofuncs[] =
{
	{ "CRYPTO_num_locks", ( void ** )&qCRYPTO_num_locks },
	{ "CRYPTO_set_locking_callback", ( void ** )&qCRYPTO_set_locking_callback },
	{ NULL, NULL }
};

#else

#define qCRYPTO_num_locks CRYPTO_num_locks
#define qCRYPTO_set_locking_callback CRYPTO_set_locking_callback

#endif

#endif

/*
* wswcurl_unloadlib
*/
static void wswcurl_unloadlib( void ) {
#ifdef USE_OPENSSL
#ifdef LIBCRYPTO_RUNTIME
	if( cryptoLibrary ) {
		Com_UnloadLibrary( &cryptoLibrary );
	}
#endif
	cryptoLibrary = NULL;
#endif

#ifdef LIBCURL_RUNTIME
	if( curlLibrary ) {
		Com_UnloadLibrary( &curlLibrary );
	}
#endif
	curlLibrary = NULL;
}

/*
* wswcurl_loadlib
*/
static void wswcurl_loadlib( void ) {
	wswcurl_unloadlib();

#ifdef LIBCURL_RUNTIME
	curlLibrary = Com_LoadSysLibrary( LIBCURL_LIBNAME, libcurlfuncs );
#else
	curlLibrary = (void *)1;
#endif

#ifdef USE_OPENSSL
#ifdef LIBCRYPTO_RUNTIME
	cryptoLibrary = Com_LoadSysLibrary( LIBCRYPTO_LIBNAME, libcryptofuncs );
#else
	cryptoLibrary = (void *)1;
#endif
#endif
}

int wswcurl_formadd( wswcurl_req *req, const char *field, const char *value, ... ) {
	va_list arg;
	char buf[1024];
	if( !req ) {
		return -1;
	}
	if( !field ) {
		return -2;
	}
	if( !value ) {
		return -3;
	}

	va_start( arg, value );
	Q_vsnprintfz( buf, sizeof( buf ), value, arg );
	va_end( arg );
	qcurl_formadd( &req->post, &req->post_last, CURLFORM_COPYNAME, field, CURLFORM_COPYCONTENTS, buf, CURLFORM_END );
	return 0;
}

int wswcurl_formadd_raw( wswcurl_req *req, const char *field, void *data, size_t size ) {
	if( !req ) {
		return -1;
	}
	if( !data ) {
		return -2;
	}
	if( !size ) {
		return -3;
	}

	// TODO: set the Content-type: to some other or just accept base64(URL) encoding here?
	qcurl_formadd( &req->post, &req->post_last,
				   CURLFORM_COPYNAME, field,
				   CURLFORM_COPYCONTENTS, data,
				   CURLFORM_CONTENTSLENGTH, size,
				   CURLFORM_END );
	return 0;
}

int wswcurl_set_postfields( wswcurl_req *req, const char *fields, size_t size ) {
	if( !req ) {
		return -1;
	}
	if( !fields ) {
		return -2;
	}
	if( !size ) {
		return -3;
	}

	// Specify we want to POST data
	qcurl_easy_setopt( req->curl, CURLOPT_POST, 1 );

	// Set the expected POST size
	qcurl_easy_setopt( req->curl, CURLOPT_POSTFIELDSIZE, (long)size );

	// Set the POST data
	qcurl_easy_setopt( req->curl, CURLOPT_POSTFIELDS, fields );

	return 0;
}

void wswcurl_urlencode( const char *src, char *dst, size_t size ) {
	char *curl_esc;

	assert( src );
	assert( dst );

	if( !src || !dst ) {
		return;
	}
	if( !curldummy ) {
		return;
	}

	QMutex_Lock( curldummy_mutex );
	curl_esc = qcurl_easy_escape( curldummy, src, 0 );
	QMutex_Unlock( curldummy_mutex );

	Q_strncpyz( dst, curl_esc, size );
	qcurl_free( curl_esc );
}

size_t wswcurl_urldecode( const char *src, char *dst, size_t size ) {
	int unesc_len;
	char *curl_unesc;

	assert( src );
	assert( dst );

	if( !src || !dst ) {
		return 0;
	}
	if( !curldummy ) {
		return 0;
	}

	QMutex_Lock( curldummy_mutex );
	curl_unesc = qcurl_easy_unescape( curldummy, src, 0, &unesc_len );
	QMutex_Unlock( curldummy_mutex );

	Q_strncpyz( dst, curl_unesc, size );
	qcurl_free( curl_unesc );

	return (size_t)unesc_len;
}

void wswcurl_start( wswcurl_req *req ) {
	CURLcode res;

	if( !req ) {
		return;
	}
	if( req->status ) {
		return; // was already started

	}
	if( !req->curl ) {
		wswcurl_delete( req );
		return;
	}

	if( req->txhead ) {
		CURLSETOPT( req->curl, res, CURLOPT_HTTPHEADER, req->txhead );
	}
	if( req->post ) {
		CURLSETOPT( req->curl, res, CURLOPT_HTTPPOST, req->post );
		req->post_last = NULL;
	}

	req->status = WSTATUS_QUEUED; // queued
}

size_t wswcurl_getsize( wswcurl_req *req, size_t *rxreceived ) {
	if( rxreceived ) {
		*rxreceived = req->rxreceived;
	}
	if( req->status < 0 ) {
		return 0;
	}
	return req->rx_expsize;
}

void wswcurl_stream_callbacks( wswcurl_req *req, wswcurl_read_cb read_cb, wswcurl_done_cb done_cb,
							   wswcurl_header_cb header_cb, void *customp ) {
	if( !req ) {
		return;
	}
	req->callback_read = read_cb;
	req->callback_done = done_cb;
	req->callback_header = header_cb;
	req->customp = customp;
}

size_t wswcurl_read( wswcurl_req *req, void *buffer, size_t size ) {
	size_t written = 0;
	chained_buffer_t *cb;

	if( ( req->rxreceived - req->rxreturned ) < ( size + WMINBUFFERING ) && req->paused ) {
		wswcurl_unpause( req );
	}

	// hmm, signal an error?
	if( req->status < 0 ) {
		return 0;
	}

	// go through the buffers in chain, dropping them if not needed
	// start from the beginning (chronological order)
	cb = req->bhead;
	while( cb && written < size ) {
		size_t numb = cb->rxsize - cb->rxoffset;
		if( numb + written > size ) {
			numb = size - written;
		}
		if( !numb ) {
			break;
		}

		if( req->ignore_bytes >= numb ) {
			req->ignore_bytes -= numb;
			cb->rxoffset += numb;
			goto advance;
		} else {
			cb->rxoffset += req->ignore_bytes;
			req->ignore_bytes = 0;
		}

		memcpy( ( (char*)buffer ) + written, cb->data + cb->rxoffset, numb );
		written += numb;
		cb->rxoffset += numb;

advance:
		if( cb->rxoffset >= cb->rxsize ) {
			// flush the buffer away
			req->bhead = cb->next;
			if( req->btail == cb ) {
				req->btail = NULL;
			}
			WFREE( cb );
			// advance to the next buffer in chain
			cb = req->bhead;
		}
		// else break;
	}

	if( written < size ) {
		( (char*)buffer )[written] = '\0';
	}

	req->rxreturned += written;

	return written;
}

#ifdef USE_OPENSSL
static void wswcurl_crypto_lockcallback( int mode, int type, const char *file, int line ) {
	( void )file;
	( void )line;
	if( mode & CRYPTO_LOCK ) {
		QMutex_Lock( crypto_mutexes[type] );
	} else {
		QMutex_Unlock( crypto_mutexes[type] );
	}
}
#endif

void wswcurl_init( void ) {
	if( wswcurl_mempool ) {
		return;
	}

	wswcurl_mempool = Mem_AllocPool( NULL, "Curl" );

	// HTTP proxy settings
	http_proxy = Cvar_Get( "http_proxy", "", CVAR_ARCHIVE );
	http_proxyuserpwd = Cvar_Get( "http_proxyuserpwd", "", CVAR_ARCHIVE );

	wswcurl_loadlib();

	if( curlLibrary ) {
		qcurl_global_init( CURL_GLOBAL_ALL );

		curldummy = qcurl_easy_init();
		curlmulti = qcurl_multi_init();
	}

	curldummy_mutex = QMutex_Create();

	http_requests_mutex = QMutex_Create();

#ifdef USE_OPENSSL
	if( cryptoLibrary ) {
		int mutex_num;
		crypto_num_mutexes = qCRYPTO_num_locks();
		crypto_mutexes = WMALLOC( crypto_num_mutexes * sizeof( *crypto_mutexes ) );
		for( mutex_num = 0; mutex_num < crypto_num_mutexes; mutex_num++ )
			crypto_mutexes[mutex_num] = QMutex_Create();
		qCRYPTO_set_locking_callback( wswcurl_crypto_lockcallback );
	}
#endif
}

void wswcurl_cleanup( void ) {
	if( !wswcurl_mempool ) {
		return;
	}

	while( http_requests ) {
		wswcurl_delete( http_requests );
	}

	if( curldummy ) {
		qcurl_easy_cleanup( curldummy );
		curldummy = NULL;
	}

	if( curlmulti ) {
		qcurl_multi_cleanup( curlmulti );
		curlmulti = NULL;
	}

	QMutex_Destroy( &curldummy_mutex );

	QMutex_Destroy( &http_requests_mutex );

#ifdef USE_OPENSSL
	if( cryptoLibrary ) {
		qCRYPTO_set_locking_callback( NULL );
		if( crypto_num_mutexes && crypto_mutexes ) {
			int mutex_num;
			for( mutex_num = 0; mutex_num < crypto_num_mutexes; mutex_num++ )
				QMutex_Destroy( &crypto_mutexes[mutex_num] );
			WFREE( crypto_mutexes );
			crypto_mutexes = NULL;
		}
		crypto_num_mutexes = 0;
	}
#endif

	if( curlLibrary ) {
		qcurl_global_cleanup();
	}

	wswcurl_unloadlib();

	Mem_FreePool( &wswcurl_mempool );
}

int wswcurl_perform() {
	int ret = 0;
	wswcurl_req *r, *next;

	if( !curlmulti ) {
		return 0;
	}

	// process requests in FIFO manner

	QMutex_Lock( http_requests_mutex );

	// check for timed out requests and requests that need to be paused
	r = http_requests_hnode;
	while( r ) {
		next = r->prev;

		if( r->status == WSTATUS_QUEUED ) {
			// queued
			if( curlmulti_num_handles < WMAXMULTIHANDLES ) {
				if( qcurl_multi_add_handle( curlmulti, r->curl ) ) {
					CURLDBG( ( "OOPS: CURL MULTI ADD HANDLE FAIL!!!" ) );
				}
				r->status = WSTATUS_STARTED;
				r->last_action = wswcurl_now();
				curlmulti_num_handles++;
			} else {
				// stay in queue
			}
		}

		// handle pauses for synchronous requests
		if( r->status == WSTATUS_STARTED && !r->callback_read ) {
			if( r->rxreceived >= r->rxreturned + WMAXBUFFERING ) {
				wswcurl_pause( r );
			}
		}

		// handle timeouts
		if( r->status == WSTATUS_STARTED ) {
			time_t now = wswcurl_now();

			if( r->paused ) {
				// paused
				r->last_action = now;
			} else if( r->timeout && ( r->last_action + r->timeout <= now ) ) {
				// timed out
				r->respcode = -1;
				r->status = -CURLE_OPERATION_TIMEDOUT;
				if( r->callback_done ) {
					r->callback_done( r, r->status, r->customp );
				}
			}
		}

		r = next;
	}

	QMutex_Unlock( http_requests_mutex );

	//CURLDBG(("CURL BEFORE MULTI_PERFORM\n"));
	while( qcurl_multi_perform( curlmulti, &ret ) == CURLM_CALL_MULTI_PERFORM ) {
		CURLDBG( ( "   CURL MULTI LOOP\n" ) );
	}
	ret += wswcurl_checkmsg();
	//CURLDBG(("CURL after checkmsg\n"));

	return ret;
}

int wswcurl_header( wswcurl_req *req, const char *key, const char *value, ... ) {
	char buf[1024], *ptr;
	va_list arg;
	if( req->status ) {
		return -1;
	}
	if( !req->curl ) {
		return -2;
	}

	Q_snprintfz( buf, sizeof( buf ), "%s: ", key );
	ptr = &buf[strlen( buf )];

	va_start( arg, value );
	Q_vsnprintfz( ptr, ( sizeof( buf ) - ( ptr - buf ) ), value, arg );
	va_end( arg );

	req->txhead = qcurl_slist_append( req->txhead, buf );
	return ( req->txhead == NULL );
}

static int wswcurl_debug_callback( CURL *curl, curl_infotype infotype, char *buf, size_t buf_size, void *userp ) {
	char *temp;

	if( infotype != CURLINFO_TEXT ) {
		return 0;
	}

	temp = Mem_TempMalloc( buf_size + 1 );
	memcpy( temp, buf, buf_size );

	Com_Printf( "%s\n", temp );

	Mem_TempFree( temp );

	return 0;
}

wswcurl_req *wswcurl_create( const char *iface, const char *furl, ... ) {
	wswcurl_req *retreq;
	CURL *curl;
	CURLcode res;
	char url[4 * 1024]; // 4kb url buffer?
	va_list arg;
	const char *proxy = http_proxy->string;
	const char *proxy_userpwd = http_proxyuserpwd->string;

	if( !curlLibrary ) {
		Com_Printf( "!!! WARNING: external library is missing (libcurl).\n" );
		return NULL;
	}

	// Prepare url formatting with variable arguments
	va_start( arg, furl );
	Q_vsnprintfz( url, sizeof( url ), furl, arg );
	va_end( arg );

	// Initialize structure
	if( !( curl = qcurl_easy_init() ) ) {
		return NULL;
	}

	// allocate, copy
	retreq = ( wswcurl_req * )WMALLOC( sizeof( wswcurl_req ) );
	memset( retreq, 0, sizeof( *retreq ) );

	retreq->curl = curl;
	retreq->url = ( char* )WMALLOC( strlen( url ) + 1 );
	memcpy( retreq->url, url, strlen( url ) + 1 );

	CURLSETOPT( curl, res, CURLOPT_URL, retreq->url );
	CURLSETOPT( curl, res, CURLOPT_WRITEFUNCTION, wswcurl_write );
	CURLSETOPT( curl, res, CURLOPT_NOPROGRESS, 1 );
	CURLSETOPT( curl, res, CURLOPT_FOLLOWLOCATION, 1 );
	CURLSETOPT( curl, res, CURLOPT_HEADERFUNCTION, wswcurl_readheader );
	CURLSETOPT( curl, res, CURLOPT_CONNECTTIMEOUT, WCONNECTTIMEOUT );
#if defined( APPLICATION ) && defined( APP_VERSION_STR ) && defined( OSNAME ) && defined( ARCH )
	CURLSETOPT( curl, res, CURLOPT_USERAGENT, APPLICATION "/" APP_VERSION_STR " (compatible; N; " OSNAME "; " ARCH ")" );
#endif
	CURLSETOPT( curl, res, CURLOPT_WRITEDATA, ( void * )retreq );
	CURLSETOPT( curl, res, CURLOPT_WRITEHEADER, ( void * )retreq );
	CURLSETOPT( curl, res, CURLOPT_PRIVATE, ( void * )retreq );
	if( iface && *iface ) {
		CURLSETOPT( curl, res, CURLOPT_INTERFACE, ( void * )iface );
	}
	CURLSETOPT( curl, res, CURLOPT_NOSIGNAL, 1 );

	if( developer->integer ) {
		CURLSETOPT( curl, res, CURLOPT_DEBUGFUNCTION, &wswcurl_debug_callback );
		CURLSETOPT( curl, res, CURLOPT_DEBUGDATA, ( void * )retreq );
		CURLSETOPT( curl, res, CURLOPT_VERBOSE, 1 );
	}

	// HTTP proxy settings
	if( proxy && *proxy ) {
		CURLSETOPT( curl, res, CURLOPT_PROXYTYPE, CURLPROXY_HTTP );

		CURLSETOPT( curl, res, CURLOPT_PROXY, proxy );
		if( proxy_userpwd && *proxy_userpwd ) {
			CURLSETOPT( curl, res, CURLOPT_PROXYUSERPWD, proxy_userpwd );
		}
	}

	wswcurl_set_timeout( retreq, WTIMEOUT );

	// link
	QMutex_Lock( http_requests_mutex );

	retreq->prev = NULL;
	retreq->next = http_requests;
	if( retreq->next ) {
		retreq->next->prev = retreq;
	} else {
		http_requests_hnode = retreq;
	}
	http_requests = retreq;

	CURLDBG( ( va( "   CURL CREATE %s\n", url ) ) );

	QMutex_Unlock( http_requests_mutex );

	return retreq;
}

void wswcurl_set_timeout( wswcurl_req *req, int timeout ) {
	//CURLcode res;
	//CURLSETOPT( req->curl, res, CURLOPT_TIMEOUT, WTIMEOUT );
	req->timeout = timeout;
}

void wswcurl_set_resume_from( wswcurl_req *req, long resume ) {
	CURLcode code;

	if( !req || !req->curl ) {
		return;
	}

	code = qcurl_easy_setopt( req->curl, CURLOPT_RESUME_FROM, resume );
	if( code != CURLE_OK ) {
		Com_Printf( "Failed to set file resume from length\n" );
	}
}

void wswcurl_delete( wswcurl_req *req ) {
	if( ( !req ) || ( !wswcurl_isvalidhandle( req ) ) ) {
		return;
	}

	// if (req->callback_done && req->active )
	//	req->callback_done ( req, 0, req->customp );

	if( req->txhead ) {
		qcurl_slist_free_all( req->txhead );
		req->txhead = NULL;
	}

	if( req->post ) {
		qcurl_formfree( req->post );
		req->post      = NULL;
		req->post_last = NULL;
	}

	if( req->url ) {
		WFREE( req->url );
	}

	if( req->bhead ) {
		chained_buffer_t *cb = req->bhead;
		chained_buffer_t *next = cb->next;

		while( cb ) {
			WFREE( cb );
			cb = next;
			if( cb ) {
				next = cb->next;
			}
		}
	}

	// remove from list
	QMutex_Lock( http_requests_mutex );

	if( req->curl ) {
		if( curlmulti && req->status && req->status != WSTATUS_QUEUED ) {
			qcurl_multi_remove_handle( curlmulti, req->curl );
			curlmulti_num_handles--;
		}
		qcurl_easy_cleanup( req->curl );
		req->curl = NULL;
	}

	if( http_requests_hnode == req ) {
		http_requests_hnode = req->prev;
	}
	if( http_requests == req ) {
		http_requests = req->next;
	}
	if( req->prev ) {
		req->prev->next = req->next;
	}
	if( req->next ) {
		req->next->prev = req->prev;
	}

	QMutex_Unlock( http_requests_mutex );

	WFREE( req );
}

int wswcurl_isvalidhandle( wswcurl_req *req ) {
	wswcurl_req *r = http_requests;
	while( r != NULL ) {
		if( r == req ) {
			return 1;
		}
		r = r->next;
	}

	return 0;
}

const char *wswcurl_get_content_type( wswcurl_req *req ) {
	char *content_type = NULL;
	qcurl_easy_getinfo( req->curl, CURLINFO_CONTENT_TYPE, &content_type );
	return content_type;
}

const char *wswcurl_getip( wswcurl_req *req ) {
	char *ipstr = NULL;
	qcurl_easy_getinfo( req->curl, CURLINFO_PRIMARY_IP, &ipstr );
	return ipstr;
}

const char *wswcurl_errorstr( int status ) {
	return qcurl_easy_strerror( ( CURLcode ) - status );
}

const char *wswcurl_get_url( const wswcurl_req *req ) {
	return req->url;
}

const char *wswcurl_get_effective_url( wswcurl_req *req ) {
	char *last_url = NULL;
	qcurl_easy_getinfo( req->curl, CURLINFO_EFFECTIVE_URL, &last_url );
	return last_url;
}

int wswcurl_get_status( const wswcurl_req *req ) {
	return req->respcode;
}

///////////////////////
// static functions

// Some versions of CURL don't report the correct exepected size when following redirects
// This manual interpretation of the expected size fixes this.
static size_t wswcurl_readheader( void *ptr, size_t size, size_t nmemb, void *stream ) {
	char buf[1024], *str;
	int slen;
	wswcurl_req *req = (wswcurl_req*)stream;

	memset( buf, 0, sizeof( buf ) );
	memcpy( buf, ptr, ( size * nmemb ) > ( sizeof( buf ) - 1 ) ? ( sizeof( buf ) - 1 ) : ( size * nmemb ) );
	str = buf;
	while( *str ) {
		if( ( *str >= 'a' ) && ( *str <= 'z' ) ) {
			*str -= 'a' - 'A';
		}
		str++;
	}

	if( ( str = (char*)strstr( buf, "CONTENT-LENGTH:" ) ) ) {
		int length;

		while( *str && ( *str != ':' ) ) {
			str++;
		}
		str++;

		while( *str && ( *str == ' ' ) ) {
			str++;
		}
		slen = strlen( str ) - 1;

		while( ( slen > 0 ) && ( ( str[slen] == '\n' ) || ( str[slen] == '\r' ) || ( str[slen] == ' ' ) ) ) {
			str[slen] = '\0';
			slen = strlen( str ) - 1;
		}

		length = atoi( str );
		if( length >= 0 ) {
			req->rx_expsize = length;
		}
	} else if( ( str  = (char*)strstr( buf, "TRANSFER-ENCODING:" ) ) ) {
		req->rx_expsize = 0;
	}

	qcurl_easy_getinfo( req->curl, CURLINFO_RESPONSE_CODE, &( req->respcode ) );

	// call header callback function
	if( req->callback_header ) {
		req->callback_header( req, buf, req->customp );
	}

	req->last_action = wswcurl_now();
	return size * nmemb;
}

static size_t wswcurl_write( void *ptr, size_t size, size_t nmemb, void *stream ) {
	float progress;
	long numb;
	wswcurl_req *req = (wswcurl_req*)stream;

	if( !req->headers_done ) {
		req->headers_done = 1;
	}

	numb = size * nmemb;
	req->rxreceived += numb;
	req->last_action = wswcurl_now();

	progress = !req->rx_expsize ? 0.0 : (float)( ( (double)req->rxreceived / (double)req->rx_expsize ) * 100.0 );
	clamp( progress, 0, 100 );

	if( req->callback_read ) {
		return req->callback_read( req, ptr, numb, progress, req->customp );
	} else {
		chained_buffer_t *cb;

		// Allocate new buffer
		cb = ( chained_buffer_t* )WMALLOC( sizeof( *cb ) + numb );
		memset( cb, 0, sizeof( *cb ) );

		// Stick the buffer to the end of the chain
		if( req->btail ) {
			req->btail->next = cb;
		}
		req->btail = cb;
		if( !req->bhead ) {
			req->bhead = cb;
		}

		memcpy( cb->data, ptr, numb );
		cb->data[numb] = '\0';
		cb->rxsize = numb;
	}

	return numb;
}

static int wswcurl_checkmsg( void ) {
	int cnt = 0;
	CURLMsg *msg;
	wswcurl_req *r;
	int ret = 0;
	char *info;

	do {
		msg = qcurl_multi_info_read( curlmulti, &cnt );
		if( !msg || !msg->easy_handle ) {
			continue;
		}

		// Treat received message.
		qcurl_easy_getinfo( msg->easy_handle, CURLINFO_PRIVATE, &info );
		r = ( wswcurl_req * )( ( void * )info );

		if( !r ) {
			//Com_Printf("OOPS: Message from unknown source!\n");
			continue; // Not for us - oops :)
		}

		if( msg->msg != CURLMSG_DONE ) {
			//Com_Printf("Other message for %s: %d\n", r->url, msg->msg);
			continue;
		}

		ret++;

		if( msg->data.result == CURLE_OK ) {
			// Done!
			r->status = WSTATUS_FINISHED;
			qcurl_easy_getinfo( r->curl, CURLINFO_RESPONSE_CODE, &( r->respcode ) );

			if( r->callback_done ) {
				r->callback_done( r, r->respcode, r->customp );
			}
		} else {
			// failed, store and pass to callback negative status value
			r->status = (int)msg->data.result * -1;
			r->respcode = -1;

			if( r->callback_done ) {
				r->callback_done( r, r->status, r->customp );
			}
		}
	} while( cnt && msg );

	return ret;
}

static void wswcurl_pause( wswcurl_req *req ) {
	if( req->paused ) {
		return;
	}

	qcurl_easy_pause( req->curl, CURLPAUSE_RECV );
	req->paused = 1;
}

static void wswcurl_unpause( wswcurl_req *req ) {
	if( !req->paused ) {
		return;
	}

	qcurl_easy_pause( req->curl, CURLPAUSE_CONT );
	req->paused = 0;
}

int wswcurl_tell( wswcurl_req *req ) {
	return req->rxreturned;
}

void wswcurl_ignore_bytes( wswcurl_req *req, size_t nbytes ) {
	req->ignore_bytes = nbytes;
}

static int wswcurl_remaining( wswcurl_req *req ) {
	if( req->rx_expsize ) {
		return req->rx_expsize - req->rxreturned;
	}
	return req->rxreceived - req->rxreturned;
}

int wswcurl_eof( wswcurl_req *req ) {
	return ( req->status == WSTATUS_FINISHED || req->status < 0 ) // request completed
		   && !wswcurl_remaining( req );
}

static time_t wswcurl_now( void ) {
	return time( NULL );
}
