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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include "ggformat.h"

static size_t strlcat( char * dst, const char * src, size_t dsize );
static long long strtonum( const char * numstr, long long minval, long long maxval, const char ** errstrp );

template< typename To, typename From >
inline To checked_cast( const From & from ) {
	To result = To( from );
	GGFORMAT_ASSERT( From( result ) == from );
	return result;
}

struct ShortString {
	char buf[ 16 ];

	ShortString() {
		buf[ 0 ] = '\0';
	}

	void operator+=( int x ) {
		char num[ 16 ];
		snprintf( num, sizeof( num ), "%d", x );
		*this += num;
	}

	void operator+=( const char * str ) {
		strlcat( buf, str, sizeof( buf ) );
	}
};

template< typename T >
static void format_helper( FormatBuffer * fb, const ShortString & fmt, const T & x ) {
	char * dst = fb->buf + fb->len;
	size_t len = fb->capacity - fb->len;

	if( fb->len >= fb->capacity ) {
		dst = NULL;
		len = 0;
	}

#if GGFORMAT_COMPILER_MSVC
#pragma warning( disable : 4996 ) // '_snprintf': This function or variable may be unsafe.
	int printed = _snprintf( NULL, 0, fmt.buf, x );
	_snprintf( dst, len, fmt.buf, x );
#else
	int printed = snprintf( dst, len, fmt.buf, x );
#endif
	fb->len += checked_cast< size_t >( printed );
}

void format( FormatBuffer * fb, double x, const FormatOpts & opts ) {
	ShortString fmt;
	fmt += "%";
	int precision = opts.precision != -1 ? opts.precision : 5;
	if( opts.plus_sign ) fmt += "+";
	if( opts.left_align ) fmt += "-";
	if( opts.zero_pad ) fmt += "0";
	if( opts.width != -1 ) fmt += opts.width + 1 + precision;
	fmt += ".";
	fmt += precision;
	fmt += "f";
	format_helper( fb, fmt, x );
}

void format( FormatBuffer * fb, char x, const FormatOpts & opts ) {
	ShortString fmt;
	fmt += "%";
	if( opts.left_align ) fmt += "-";
	if( opts.width != -1 ) fmt += opts.width;
	fmt += "c";
	format_helper( fb, fmt, x );
}

void format( FormatBuffer * fb, const char * x, const FormatOpts & opts ) {
	ShortString fmt;
	fmt += "%";
	if( opts.left_align ) fmt += "-";
	if( opts.width != -1 ) fmt += opts.width;
	fmt += "s";
	format_helper( fb, fmt, x );
}

void format( FormatBuffer * fb, bool x, const FormatOpts & opts ) {
	format( fb, x ? "true" : "false", opts );
}

template< typename T >
static void int_helper( FormatBuffer * fb, const char * fmt_length, const char * fmt_decimal, const T & x, const FormatOpts & opts ) {
	ShortString fmt;
	fmt += "%";
	if( opts.plus_sign ) fmt += "+";
	if( opts.left_align ) fmt += "-";
	if( opts.zero_pad ) fmt += "0";
	if( opts.width != -1 ) fmt += opts.width;
	if( opts.number_format == FormatOpts::DECIMAL ) {
		fmt += fmt_length;
		fmt += fmt_decimal;
	}
	else if( opts.number_format == FormatOpts::HEX ) {
		fmt += fmt_length;
		fmt += "x";
	}
	else if( opts.number_format == FormatOpts::BINARY ) {
		fmt += "s";
		char binary[ sizeof( x ) * 8 + 1 ];
		binary[ sizeof( x ) * 8 ] = '\0';

		for( size_t i = 0; i < sizeof( x ) * 8; i++ ) {
			// this is UB for signed types, but who cares?
			T bit = x & ( T( 1 ) << ( sizeof( x ) * 8 - i - 1 ) );
			binary[ i ] = bit == 0 ? '0' : '1';
		}

		format_helper( fb, fmt, binary );
		return;
	}
	format_helper( fb, fmt, x );
}

#define INT_OVERLOADS( T, fmt_length ) \
	void format( FormatBuffer * fb, signed T x, const FormatOpts & opts ) { \
		int_helper( fb, fmt_length, "d", x, opts ); \
	} \
	void format( FormatBuffer * fb, unsigned T x, const FormatOpts & opts ) { \
		int_helper( fb, fmt_length, "u", x, opts ); \
	}

INT_OVERLOADS( char, "hh" )
INT_OVERLOADS( short, "h" )
INT_OVERLOADS( int, "" )
INT_OVERLOADS( long, "l" )
INT_OVERLOADS( long long, "ll" )

#undef INT_OVERLOADS

static const char * parse_format_bool( const char * p, const char * one_past_end, char x, bool * out ) {
	if( p >= one_past_end ) return p;
	if( *p != x ) return p;
	*out = true;
	return p + 1;
}

static const char * parse_format_int( const char * p, const char * one_past_end, int * out ) {
	char num[ 16 ];
	size_t num_len = 0;

	while( p + num_len < one_past_end && isdigit( p[ num_len ] ) ) {
		num[ num_len ] = p[ num_len ];
		num_len++;
	}
	num[ num_len ] = '\0';

	if( num_len == 0 ) return p;

	*out = int( strtonum( num, 1, 1024, NULL ) );
	GGFORMAT_ASSERT( *out != 0 );

	return p + num_len;
}

static const char * parse_format_precision( const char * p, const char * one_past_end, int * precision ) {
	bool has_a_dot = false;
	const char * after_dot = parse_format_bool( p, one_past_end, '.', &has_a_dot );
	if( !has_a_dot ) return p;
	return parse_format_int( after_dot, one_past_end, precision );
}

static const char * parse_format_number_format( const char * p, const char * one_past_end, FormatOpts::NumberFormat * number_format ) {
	bool hex = false;
	const char * after_hex = parse_format_bool( p, one_past_end, 'x', &hex );

	if( hex ) {
		*number_format = FormatOpts::HEX;
		return after_hex;
	}

	bool bin = false;
	const char * after_bin = parse_format_bool( p, one_past_end, 'b', &bin );

	if( bin ) {
		*number_format = FormatOpts::BINARY;
		return after_bin;
	}

	return p;
}

FormatOpts parse_formatopts( const char * fmt, size_t len ) {
	FormatOpts opts;

	const char * start = fmt;
	const char * one_past_end = start + len;

	start = parse_format_bool( start, one_past_end, '+', &opts.plus_sign );
	start = parse_format_bool( start, one_past_end, '-', &opts.left_align );
	start = parse_format_bool( start, one_past_end, '0', &opts.zero_pad );
	start = parse_format_int( start, one_past_end, &opts.width );
	start = parse_format_precision( start, one_past_end, &opts.precision );
	start = parse_format_number_format( start, one_past_end, &opts.number_format );

	GGFORMAT_ASSERT( start == one_past_end );

	return opts;
}

static bool strchridx( const char * haystack, char needle, size_t * idx, size_t skip = 0 ) {
	*idx = skip;
	while( haystack[ *idx ] != '\0' ) {
		if( haystack[ *idx ] == needle ) {
			return true;
		}
		( *idx )++;
	}
	return false;
}

bool ggformat_find( const char * str, size_t * start, size_t * one_past_end ) {
	size_t open_idx;
	bool has_open = strchridx( str, '{', &open_idx );
	if( has_open && str[ open_idx + 1 ] == '{' ) {
		has_open = false;
	}
	if( !has_open ) open_idx = 0;

	size_t close_idx;
	bool has_close = strchridx( str, '}', &close_idx, open_idx );
	if( has_close && str[ close_idx + 1 ] == '}' ) {
		has_close = false;
	}

	if( has_open ) {
		GGFORMAT_ASSERT( has_close );
		GGFORMAT_ASSERT( open_idx < close_idx );

		*start = open_idx;
		*one_past_end = close_idx;

		return true;
	}

	GGFORMAT_ASSERT( !has_close );
	return false;
}

void ggformat_literals( FormatBuffer * fb, const char * literals, size_t len ) {
	size_t copied_len = 0;
	for( size_t i = 0; i < len; i++ ) {
		if( literals[ i ] == '{' || literals[ i ] == '}' ) {
			i++;
		}
		if( fb->len + copied_len < fb->capacity ) {
			fb->buf[ fb->len + copied_len ] = literals[ i ];
		}
		copied_len++;
	}
	fb->len += copied_len;
	if( fb->capacity > 0 ) {
		fb->buf[ fb->len < fb->capacity - 1 ? fb->len : fb->capacity - 1 ] = '\0';
	}
}

void ggformat_impl( FormatBuffer * fb, const char * fmt ) {
	size_t ignored;
	GGFORMAT_ASSERT( !ggformat_find( fmt, &ignored, &ignored ) );
	ggformat_literals( fb, fmt, strlen( fmt ) );
}

/*
 * Copyright (c) 1998, 2015 Todd C. Miller <Todd.Miller@courtesan.com>
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

static size_t
strlcat(char *dst, const char *src, size_t dsize)
{
	const char *odst = dst;
	const char *osrc = src;
	size_t n = dsize;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end. */
	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = dst - odst;
	n = dsize - dlen;

	if (n-- == 0)
		return(dlen + strlen(src));
	while (*src != '\0') {
		if (n != 0) {
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';

	return(dlen + (src - osrc));    /* count does not include NUL */
}

/*
 * Copyright (c) 2004 Ted Unangst and Todd Miller
 * All rights reserved.
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

#define INVALID         1
#define TOOSMALL        2
#define TOOLARGE        3

static long long
strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp)
{
	long long ll = 0;
	char *ep;
	int error = 0;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,         0 },
		{ "invalid",    EINVAL },
		{ "too small",  ERANGE },
		{ "too large",  ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval)
		error = INVALID;
	else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}
