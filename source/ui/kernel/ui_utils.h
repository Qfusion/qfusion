#pragma once
#ifndef __UI_UTILS_H__
#define __UI_UTILS_H__

#include "kernel/ui_common.h"
#include "kernel/ui_syscalls.h"
#include <string>
#include <utility>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>

namespace WSWUI
{
// container is of type that stores Element*, implements push_back and clear
// and its range can be used in std::for_each.
// predicate looks like bool(Element*) and should return true if it wants
// the element given as parameter to be included on the list
template<typename T, typename Function>
void collectChildren( Rml::Core::Element *elem, T &container, Function predicate = unary_true<Rml::Core::Element*> ) {
	// insert top-first
	Rml::Core::Element *child = elem->GetFirstChild();
	while( child ) {
		if( predicate( child ) ) {
			container.push_back( child );
		}
		// recurse
		collectChildren( child, container, predicate );
		// and iterate
		child = child->GetNextSibling();
	}
}

// as above but start collecting from the given element
template<typename T, typename Function>
void collectElements( Rml::Core::Element *elem, T &container, Function predicate = unary_true<Rml::Core::Element*> ) {
	// insert bottom-first
	Rml::Core::Element *child = elem->GetFirstChild();
	while( child ) {
		collectElements( child, container, predicate );
		child = child->GetNextSibling();
	}
	container.push_back( elem );
}

// inline action for elements, Function should take 1 parameter, the element itself
template<typename Function>
void foreachElem( Rml::Core::Element *elem, Function function, bool doRoot = true ) {
	if( doRoot ) {
		function( elem );
	}

	Rml::Core::Element *child = elem->GetFirstChild();
	while( child ) {
		function( child );
		foreachElem( child, function, false );
		child = child->GetNextSibling();
	}
}

// it returns a list of files contained into the path with specified extension. If
// keep_extension is true, files will have extension text
template<typename C>
void getFileList( C &filesList, const std::string &path, const std::string &extension, bool keep_extension = false ) {
	int i, k;
	char listbuf[1024];
	char    *ptr;
	int ptrlen;
	int numOfFiles;

	numOfFiles = trap::FS_GetFileList( path.c_str(), extension.c_str(), NULL, 0, 0, 0 );

	// DAMN: remember this.
	i = 0;

	do {
		if( ( k = trap::FS_GetFileList( path.c_str(), extension.c_str(), listbuf, sizeof( listbuf ), i, numOfFiles ) ) == 0 ) {
			i++;     // can happen if the filename is too long to fit into the buffer or we're done
			continue;
		}

		i += k;

		for( ptr = listbuf; k > 0; k--, ptr += ptrlen + 1 ) {
			ptrlen = strlen( ptr );

			// remove "/" from path name
			if( ptr[ptrlen - 1] == '/' ) {
				ptr[ptrlen - 1] = '\0';
			}

			// remove hidden directories
			if( ptr[0] == '.' ) {
				continue;
			}

			// invalid directories
			if( !strcmp( ptr, "." ) || !strcmp( ptr,".." ) ) {
				continue;
			}

			if( !keep_extension ) {
				COM_StripExtension( ptr );
			}

			filesList.push_back( ptr );
		}
	} while( i < numOfFiles );
}

template <class T>
std::string toString( const T& t ) {
	std::ostringstream oss;
	oss << t;
	return oss.str();
}

// simple benchmark timer class
struct BenchmarkTimer {
	uint64_t start;
	// constructor grabs current time automatically
	BenchmarkTimer() : start( trap::Microseconds() ) {}
	// fetch current timedelta
	unsigned int operator()() {
		return (unsigned int)( trap::Microseconds() - start );
	}
	// reset timer to current time
	void reset() {
		start = trap::Microseconds();
	}
};

//======================================

/* this allows you to push items to vector like this
    vector<int> ivec;
    pusher( ivec )( 1 )( 2 )( 3 )( 4 );
*/
template<typename T>
struct pusher_type {
	T &container;
	pusher_type( T &_container ) : container( _container ) {}
	template<typename U>
	pusher_type &operator()( U item ) {
		container.push_back( item );
		return *this;
	}
	// explicit ref version
	template<typename U>
	pusher_type &operator()( U &item ) {
		container.push_back( item );
		return *this;
	}
};

template<typename T>
pusher_type<T> pusher( T &container ) {
	return pusher_type<T>( container );
}

//======================================

/* same thing but for an associated container that requires 2 components
    map<string, int> simap;
    mapper( simap )( "a", 1 )( "b", 2 )( "c", 3 );
*/
template<typename T>
struct mapper_type {
	T &container;
	mapper_type( T &_container ) : container( _container ) {}
	// TODO/FIXME: explicit by-value/by-ref versions
	template<typename U, typename V>
	mapper_type &operator()( const U &key, V &item ) {
		container.insert( std::pair<U,V>( key, item ) );
		return *this;
	}
};

template<typename T>
mapper_type<T> mapper( T &container ) {
	return mapper_type<T>( container );
}
	
struct ElementOwner {
	Rml::Core::ElementPtr elem;
	ElementOwner(Rml::Core::Element *elem_) : elem(Rml::Core::ElementPtr(elem_)) { }
};

//======================================

extern std::string rgb2hex( const char *rgbstr );
extern std::string hex2rgb( const char *hexstr );

extern const char *int_to_addr( uint64_t r );
extern uint64_t addr_to_int( const std::string &adr );

//======================================

extern void tokenize( const std::string &str, char sep, std::vector<std::string> &tokens );
}

#endif
