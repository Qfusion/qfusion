/*
 * ggformat v1.0
 *
 * Copyright (c) 2017 Michael Savage <mike@mikejsavage.co.uk>
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>

/*
 * prototypes of the functions you should be calling
 */

/*
 * `ggformat` writes at most `len` bytes to `buf`, and that always includes a
 * null terminator. It returns the number of bytes that would have been written
 * if `buf` were large enough, not including the null terminator, and can be
 * larger than `len` (just like sprintf).
*/
template< typename... Rest >
size_t ggformat( char * buf, size_t len, const char * fmt, const Rest & ... rest );

/*
 * `ggprint_to_file` does what you would expect, and `ggprint` writes to
 * standard output. Both return `true` on success, or `false` if the write
 * fails.
 */
template< typename... Rest >
bool ggprint_to_file( FILE * file, const char * fmt, const Rest & ... rest );

template< typename... Rest >
bool ggprint( const char * fmt, const Rest & ... rest );

/*
 * structures and functions used for formatting specific data types
 */

struct FormatOpts {
	enum NumberFormat { DECIMAL, HEX, BINARY };

	int width = -1;
	int precision = -1;
	bool plus_sign = false;
	bool left_align = false;
	bool zero_pad = false;
	NumberFormat number_format = DECIMAL;
};

struct FormatBuffer {
	FormatBuffer( char * b, size_t c ) {
		buf = b;
		capacity = c;
		len = 0;
	}

	char * buf;
	size_t capacity;
	size_t len;
};

/*
 * format implementations for primitive types
 */

void format( FormatBuffer * fb, signed char x, const FormatOpts & opts );
void format( FormatBuffer * fb, short x, const FormatOpts & opts );
void format( FormatBuffer * fb, int x, const FormatOpts & opts );
void format( FormatBuffer * fb, long x, const FormatOpts & opts );
void format( FormatBuffer * fb, long long x, const FormatOpts & opts );
void format( FormatBuffer * fb, unsigned char x, const FormatOpts & opts );
void format( FormatBuffer * fb, unsigned short x, const FormatOpts & opts );
void format( FormatBuffer * fb, unsigned int x, const FormatOpts & opts );
void format( FormatBuffer * fb, unsigned long x, const FormatOpts & opts );
void format( FormatBuffer * fb, unsigned long long x, const FormatOpts & opts );
void format( FormatBuffer * fb, double x, const FormatOpts & opts );
void format( FormatBuffer * fb, bool x, const FormatOpts & opts );
void format( FormatBuffer * fb, char x, const FormatOpts & opts );
void format( FormatBuffer * fb, const char * x, const FormatOpts & opts = FormatOpts() );

/*
 * nasty implementation details that have to go in the header
 */

#define GGFORMAT_ASSERT( p ) \
	do { \
		if( !( p ) ) { \
			fprintf( stderr, "assertion failed: %s\n", #p ); \
			abort(); \
		} \
	} while( 0 )

#if defined( _MSC_VER )
#  define GGFORMAT_COMPILER_MSVC 1
#elif defined( __clang__ )
#  define GGFORMAT_COMPILER_CLANG 1
#elif defined( __GNUC__ )
#  define GGFORMAT_COMPILER_GCC 1
#else
#  error new compiler
#endif

// this is optional but helps compile times
#if GGFORMAT_COMPILER_MSVC
#  define GGFORMAT_DISABLE_OPTIMISATIONS() __pragma( optimize( "", off ) )
#  define GGFORMAT_ENABLE_OPTIMISATIONS() __pragma( optimize( "", on ) )
#elif GGFORMAT_COMPILER_GCC
#  define GGFORMAT_DISABLE_OPTIMISATIONS() \
        _Pragma( "GCC push_options" ) \
        _Pragma( "GCC optimize (\"O0\")" )
#  define GGFORMAT_ENABLE_OPTIMISATIONS() _Pragma( "GCC pop_options" )
#elif GGFORMAT_COMPILER_CLANG
#  define GGFORMAT_DISABLE_OPTIMISATIONS() _Pragma( "clang optimize off" )
#  define GGFORMAT_ENABLE_OPTIMISATIONS() _Pragma( "clang optimize on" )
#else
#  error new compiler
#endif

FormatOpts parse_formatopts( const char * fmt, size_t len );
void ggformat_impl( FormatBuffer * fb, const char * fmt );
bool ggformat_find( const char * str, size_t * start, size_t * one_past_end );
void ggformat_literals( FormatBuffer * fb, const char * literals, size_t len );

GGFORMAT_DISABLE_OPTIMISATIONS();

template< typename T, typename... Rest >
void ggformat_impl( FormatBuffer * fb, const char * fmt, const T & first, const Rest & ... rest ) {
	size_t start, one_past_end;
	bool has_fmt = ggformat_find( fmt, &start, &one_past_end );
	GGFORMAT_ASSERT( has_fmt );

	ggformat_literals( fb, fmt, start );

	FormatOpts opts = parse_formatopts( fmt + start + 1, one_past_end - start - 1 );
	format( fb, first, opts );

	ggformat_impl( fb, fmt + one_past_end + 1, rest... );
}

template< typename... Rest >
size_t ggformat( char * buf, size_t len, const char * fmt, const Rest & ... rest ) {
	FormatBuffer fb( buf, len );
	ggformat_impl( &fb, fmt, rest... );
	return fb.len;
}

template< typename... Rest >
bool ggprint_to_file( FILE * file, const char * fmt, const Rest & ... rest ) {
	char buf[ 4096 ];
	FormatBuffer fb( buf, sizeof( buf ) );
	ggformat_impl( &fb, fmt, rest... );

	if( fb.len < fb.capacity ) {
		size_t written = fwrite( buf, 1, fb.len, file );
		return written == fb.len;
	}

	char * large_buf = ( char * ) malloc( fb.len + 1 );
	GGFORMAT_ASSERT( large_buf != NULL );
	FormatBuffer new_fb( large_buf, fb.len + 1 );
	ggformat_impl( &new_fb, fmt, rest... );
	size_t written = fwrite( large_buf, 1, fb.len, file );
	free( large_buf );

	return written == fb.len;
}

template< typename... Rest >
bool ggprint( const char * fmt, const Rest & ... rest ) {
	return ggprint_to_file( stdout, fmt, rest... );
}

GGFORMAT_ENABLE_OPTIMISATIONS();
