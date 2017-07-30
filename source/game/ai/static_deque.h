#ifndef QFUSION_STATIC_DEQUE_H
#define QFUSION_STATIC_DEQUE_H

#include <utility>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "ai_local.h"

#ifdef _MSC_VER
#pragma warning( disable : 4324 )       // structure was padded due to alignment specifier
#endif

template<typename T, unsigned N>
class alignas ( 16 )StaticDeque
{
	friend class iterator;
	friend class const_iterator;

public:
	typedef unsigned size_type;
	typedef T &reference;
	typedef const T &const_reference;

private:
	// Disable moving, and, implicitly, copying
	StaticDeque( StaticDeque<T, N> && that ) = delete;

	static constexpr size_type RealChunkSize() {
		return ( sizeof( T ) % alignof( T ) ) ? sizeof( T ) + alignof( T ) - ( sizeof( T ) % alignof( T ) ) : sizeof( T );
	}

	alignas( 16 ) char staticBuffer[RealChunkSize() * ( N + 1 )];

	T *basePointer;
	size_type count;

	unsigned beforeFirst;
	unsigned afterLast;

#ifndef _MSC_VER
	inline static void fail_with( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute( ( noreturn ) )
#else
	inline static void fail_with( const char *format, ... )
#endif
	{
		va_list va;
		va_start( va, format );
		AI_FailWithv( "StaticDeque::fail_with()", format, va );
		va_end( va );
	}

	inline static unsigned next_index( unsigned index ) {
		return ( index + 1 ) % ( N + 1 );
	}
	inline static unsigned prev_index( unsigned index ) {
		return index ? index - 1 : N;
	}
	inline void check_capacity_overflow( const char *function ) const {
#ifdef _DEBUG
		if( size() == capacity() ) {
			fail_with( "%s: capacity overflow", function );
		}
#endif
	}
	inline void check_capacity_underflow( const char *function ) const {
#ifdef _DEBUG
		if( empty() ) {
			fail_with( "%s: capacity underflow", function );
		}
#endif
	}

public:
	inline StaticDeque() : basePointer( (T*)staticBuffer ), count( 0 ), beforeFirst( 0 ), afterLast( 0 ) {
	}
	inline ~StaticDeque()
	{
		if( count ) {
			for( unsigned index = next_index( beforeFirst ); index != afterLast; index = next_index( index ) )
				basePointer[index].~T();
		}
	}

	inline void clear() {
		for( unsigned index = next_index( beforeFirst ); index != afterLast; index = next_index( index ) )
			basePointer[index].~T();
		count = 0;
		beforeFirst = 0;
		afterLast = 0;
	}

	inline size_type size() const { return count; }
	inline constexpr size_type capacity() const { return N; }
	inline bool empty() const { return count == 0; }

	inline reference &front() {
		check_capacity_underflow( __FUNCTION__ );
		return basePointer[next_index( beforeFirst )];
	}
	inline const_reference &front() const {
		check_capacity_underflow( __FUNCTION__ );
		return basePointer[next_index( beforeFirst )];
	}

	inline reference &back() {
		check_capacity_underflow( __FUNCTION__ );
		return basePointer[prev_index( afterLast )];
	}
	inline const_reference &back() const {
		check_capacity_underflow( __FUNCTION__ );
		return basePointer[prev_index( afterLast )];
	}

	class iterator
	{
		friend class StaticDeque;
		// Put a pointer first for compact alignment
		StaticDeque<T, N> &deque;
		unsigned index;

		iterator( StaticDeque<T, N> &deque_, unsigned index_ ) : deque( deque_ ), index( index_ ) {}

public:
		inline iterator &operator++() {
			index = next_index( index );
			return *this;
		}
		inline iterator operator++( int ) {
			auto result = *this;
			++*this;
			return result;
		}
		inline iterator &operator--() {
			index = prev_index( index );
			return *this;
		}
		inline iterator operator--( int ) {
			auto result = *this;
			++*this;
			return result;
		}
		inline bool operator==( const iterator &that ) const {
#ifdef _DEBUG
			if( &deque != &that.deque ) {
				fail_with( "Attempt to test for equality iterators of different containers" );
			}
#endif
			return index == that.index;
		}
		inline bool operator!=( const iterator &that ) const {
			return !( *this == that );
		}
		inline typename StaticDeque<T, N>::reference operator*() {
			return deque.basePointer[index];
		};
		inline typename StaticDeque<T, N>::const_reference operator*() const {
			return deque.basePointer[index];
		};
	};

	class const_iterator
	{
		friend class StaticDeque;
		// Put a pointer first for compact alignment
		const StaticDeque<T, N> &deque;
		unsigned index;

		const_iterator( const StaticDeque<T, N> &deque_, unsigned index_ ) : deque( deque_ ), index( index_ ) {}

public:
		inline const_iterator &operator++() {
			index = next_index( index );
			return *this;
		}
		inline const_iterator operator++( int ) {
			auto result = *this;
			++*this;
			return result;
		}
		inline const_iterator &operator--() {
			index = prev_index( index );
			return *this;
		}
		inline const_iterator operator--( int ) {
			auto result = *this;
			++*this;
			return result;
		}
		inline bool operator==( const const_iterator &that ) const {
#ifdef _DEBUG
			if( &deque != &that.deque ) {
				fail_with( "Attempt to test for equality iterators of different containers" );
			}
#endif
			return index == that.index;
		}
		inline bool operator!=( const const_iterator &that ) const {
			return !( *this == that );
		}
		inline typename StaticDeque<T, N>::const_reference operator*() const {
			return deque.basePointer[index];
		};
	};

	inline iterator begin() {
		return iterator( *this, count != 0 ? next_index( beforeFirst ) : beforeFirst );
	}

	inline iterator end() {
		return iterator( *this, afterLast );
	}

	inline const_iterator begin() const {
		return const_iterator( *this, count != 0 ? next_index( beforeFirst ) : beforeFirst );
	}

	inline const_iterator end() const {
		return const_iterator( *this, afterLast );
	}

	inline const_iterator cbegin() const { return begin(); }
	inline const_iterator cend() const { return end(); }

	inline void push_front( const T &elem ) {
		check_capacity_overflow( __FUNCTION__ );
		new ( basePointer + beforeFirst )T( elem );
		beforeFirst = prev_index( beforeFirst );
		++count;
		if( count == 1 ) {
			afterLast = next_index( afterLast );
		}
	}
	inline void emplace_front( T && elem ) {
		check_capacity_overflow( __FUNCTION__ );
		new ( basePointer + beforeFirst )T( std::forward<T>( elem ) );
		beforeFirst = prev_index( beforeFirst );
		++count;
		if( count == 1 ) {
			afterLast = next_index( afterLast );
		}
	}

	inline void push_back( const T &elem ) {
		check_capacity_overflow( __FUNCTION__ );
		new ( basePointer + afterLast )T( elem );
		afterLast = next_index( afterLast );
		++count;
		if( count == 1 ) {
			beforeFirst = prev_index( beforeFirst );
		}
	}
	inline void emplace_back( T &&elem ) {
		check_capacity_overflow( __FUNCTION__ );
		new ( basePointer + afterLast )T( std::forward<T>( elem ) );
		afterLast = next_index( afterLast );
		++count;
		if( count == 1 ) {
			beforeFirst = prev_index( beforeFirst );
		}
	}

	inline void pop_front() {
		check_capacity_underflow( __FUNCTION__ );
		beforeFirst = next_index( beforeFirst );
		( basePointer + beforeFirst )->~T();
		--count;
		if( count == 0 ) {
			afterLast = prev_index( afterLast );
		}
	}
	inline void pop_back() {
		check_capacity_underflow( __FUNCTION__ );
		afterLast = prev_index( afterLast );
		( basePointer + afterLast )->~T();
		--count;
		if( count == 0 ) {
			beforeFirst = next_index( beforeFirst );
		}
	}
};

#endif
