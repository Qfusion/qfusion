/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#pragma once

// global preprocessor defines
#include "config.h"

// q_shared.h -- included first by ALL program modules
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

//==============================================

#ifdef _WIN32

#ifdef _MSC_VER

// unknown pragmas are SUPPOSED to be ignored, but....
#pragma warning( disable : 4244 )       // MIPS
#pragma warning( disable : 4136 )       // X86
#pragma warning( disable : 4051 )       // ALPHA
#pragma warning( disable : 4514 )       // unreferenced inline function has been removed
#pragma warning( disable : 4152 )       // nonstandard extension, function/data pointer conversion in expression
#pragma warning( disable : 4201 )       // nonstandard extension used : nameless struct/union
#pragma warning( disable : 4054 )       // 'type cast' : from function pointer to data pointer
#pragma warning( disable : 4127 )       // conditional expression is constant
#pragma warning( disable : 4100 )       // unreferenced formal parameter
#pragma warning( disable : 4706 )       // assignment within conditional expression
#pragma warning( disable : 4702 )       // unreachable code
#pragma warning( disable : 4306 )       // conversion from 'int' to 'void *' of greater size
#pragma warning( disable : 4305 )       // truncation from 'void *' to 'int'
#pragma warning( disable : 4055 )       // 'type cast' : from data pointer 'void *' to function pointer
#pragma warning( disable : 4204 )       // nonstandard extension used : non-constant aggregate initializer

#if defined _M_AMD64
#pragma warning( disable : 4267 )       // conversion from 'size_t' to whatever, possible loss of data
#endif

#pragma warning( disable : 4838 )       // conversion from 'double' to whatever requires a narrowing conversion
#pragma warning( disable : 4324 )       // structure was padded due to alignment specifier
#endif

#if defined( _MSC_VER ) && defined( _I64_MAX )
# define HAVE___STRTOI64
#endif

#define HAVE__SNPRINTF

#define HAVE__VSNPRINTF

#define HAVE__STRICMP

#define HAVE_STRTOK_S

#define LIB_DIRECTORY "libs"
#define LIB_PREFIX ""
#define LIB_SUFFIX ".dll"

#ifdef NDEBUG
#define BUILDSTRING "Win32 RELEASE"
#else
#define BUILDSTRING "Win32 DEBUG"
#endif

#define OSNAME "Windows"

#include <malloc.h>
#define HAVE__ALLOCA

typedef int socklen_t;

typedef unsigned long ioctl_param_t;

typedef uintptr_t socket_handle_t;

#endif

#define ARCH "x86_64"

//==============================================

#if defined ( __linux__ ) || defined ( __FreeBSD__ )

#define HAVE_INLINE

#ifndef HAVE_STRCASECMP // SDL_config.h seems to define this too...
#define HAVE_STRCASECMP
#endif

#define LIB_DIRECTORY "libs"
#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".so"

#if defined ( __FreeBSD__ )
#define BUILDSTRING "FreeBSD"
#define OSNAME "FreeBSD"
#else
#define BUILDSTRING "Linux"
#define OSNAME "Linux"
#endif

#include <alloca.h>

// wsw : aiwa : 64bit integers and integer-pointer types
typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#endif

//==============================================

#if defined ( __APPLE__ ) && defined ( __MACH__ )

#ifndef __MACOSX__
#define __MACOSX__
#endif

#define HAVE_INLINE

#ifndef HAVE_STRCASECMP // SDL_config.h seems to define this too...
#define HAVE_STRCASECMP
#endif

#define LIB_DIRECTORY "libs"
#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".dylib"

//Mac OSX has universal binaries, no need for cpu dependency
#define BUILDSTRING "MacOSX"
#define OSNAME "MacOSX"

#include <alloca.h>

typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#endif

//==============================================

#ifdef HAVE__SNPRINTF
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#ifdef HAVE__VSNPRINTF
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif

#ifdef HAVE__STRICMP
#ifndef Q_stricmp
#define Q_stricmp( s1, s2 ) _stricmp( ( s1 ), ( s2 ) )
#endif
#ifndef Q_strnicmp
#define Q_strnicmp( s1, s2, n ) _strnicmp( ( s1 ), ( s2 ), ( n ) )
#endif
#endif

#ifdef HAVE_STRCASECMP
#ifndef Q_stricmp
#define Q_stricmp( s1, s2 ) strcasecmp( ( s1 ), ( s2 ) )
#endif
#ifndef Q_strnicmp
#define Q_strnicmp( s1, s2, n ) strncasecmp( ( s1 ), ( s2 ), ( n ) )
#endif
#endif

#ifdef HAVE_STRTOK_S
#ifndef strtok_r
#define strtok_r strtok_s
#endif
#endif

#ifdef HAVE__ALLOCA
#ifndef alloca
#define alloca _alloca
#endif
#endif

#ifndef BUILDSTRING
#define BUILDSTRING "NON-WIN32"
#endif

#if defined ( __GNUC__ )
#define ATTRIBUTE_ALIGNED( x ) __attribute__( ( aligned( x ) ) )
#elif defined ( _MSC_VER )
#define ATTRIBUTE_ALIGNED( x ) __declspec( align( x ) )
#else
#define ATTRIBUTE_ALIGNED( x )
#endif

#ifdef HAVE___STRTOI64
#define strtoll _strtoi64
#define strtoull _strtoi64
#endif

// the ALIGN macro as defined by Linux kernel
#define ALIGN( x, a ) ( ( ( x ) + ( ( size_t )( a ) - 1 ) ) & ~( ( size_t )( a ) - 1 ) )

// The `malloc' attribute is used to tell the compiler that a function
// may be treated as if it were the malloc function.  The compiler
// assumes that calls to malloc result in a pointers that cannot
// alias anything.  This will often improve optimization.
#if defined ( __GNUC__ )
#define ATTRIBUTE_MALLOC __attribute__( ( malloc ) )
#elif defined ( _MSC_VER )
#define ATTRIBUTE_MALLOC __declspec( noalias ) __declspec( restrict )
#else
#define ATTRIBUTE_MALLOC
#endif

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define QF_DLL_EXPORT __declspec( dllexport )
#else
#define QF_DLL_EXPORT __attribute__ ( ( visibility( "default" ) ) )
#endif
