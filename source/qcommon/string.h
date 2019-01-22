#pragma once

#include <stddef.h>
#include <string.h>

#include "ggformat.h"

template< size_t N >
class String {
public:
	STATIC_ASSERT( N > 0 );

	String() {
		clear();
	}

	template< typename... Rest >
	String( const char * fmt, const Rest & ... rest ) {
		format( fmt, rest... );
	}

	operator const char *() const {
		return buf;
	}

	void clear() {
		buf[ 0 ] = '\0';
		length = 0;
	}

	template< typename T >
	void operator+=( const T & x ) {
		append( "{}", x );
	}

	template< typename... Rest >
	void format( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf, N, fmt, rest... );
		length = min( copied, N - 1 );
	}

	template< typename... Rest >
	void append( const char * fmt, const Rest & ... rest ) {
		size_t copied = ggformat( buf + length, N - length, fmt, rest... );
		length += min( copied, N - length - 1 );
	}

	void truncate( size_t n ) {
		if( n >= length )
			return;
		buf[ n ] = '\0';
		length = n;
	}

	char & operator[]( size_t i ) {
		assert( i < N );
		return buf[ i ];
	}

	const char & operator[]( size_t i ) const {
		assert( i < N );
		return buf[ i ];
	}

	const char * c_str() const {
		return buf;
	}

	size_t len() const {
		return length;
	}

	bool operator==( const char * rhs ) const {
		return strcmp( buf, rhs ) == 0;
	}

	bool operator!=( const char * rhs ) const {
		return !( *this == rhs );
	}

private:
	size_t length;
	char buf[ N ];
};

template< size_t N >
void format( FormatBuffer * fb, const String< N > & buf, const FormatOpts & opts ) {
	format( fb, buf.c_str(), opts );
}
