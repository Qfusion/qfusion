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

#ifndef GAME_QARCH_H
#define GAME_QARCH_H

#ifdef __cplusplus
extern "C" {
#endif

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
#include <stdbool.h>
#include <stddef.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>


#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
#include <type_traits>
#endif

#ifdef __cplusplus
extern "C" {
#endif


//==============================================

#ifdef _WIN32

// wsw : pb : zlib 1.2.3
//# define ZLIB_WINAPI

#ifdef _MSC_VER

// unknown pragmas are SUPPOSED to be ignored, but....
#pragma warning( disable : 4244 )       // MIPS
#pragma warning( disable : 4136 )       // X86
#pragma warning( disable : 4051 )       // ALPHA

//# pragma warning(disable : 4018)		// signed/unsigned mismatch
//# pragma warning(disable : 4305)		// truncation from const double to float
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

#define HAVE___INLINE

#define HAVE__SNPRINTF

#define HAVE__VSNPRINTF

#define HAVE__STRICMP

#define HAVE_STRTOK_S

#ifdef LCC_WIN32
#ifndef C_ONLY
#define C_ONLY
#endif
#define HAVE_TCHAR
#define HAVE_MMSYSTEM
#define HAVE_DLLMAIN
#else
#define HAVE_WSIPX
#endif

#define LIB_DIRECTORY "libs"
#define LIB_PREFIX ""
#define LIB_SUFFIX ".dll"

#define VID_INITFIRST

#define MUMBLE_SUPPORT
#define OPENAL_RUNTIME

// FIXME: move these to CMakeLists.txt
#define LIBZ_LIBNAME "zlib1.dll"
#define LIBCURL_LIBNAME "libcurl-4.dll|libcurl-3.dll"
#define LIBPNG_LIBNAME "libpng16.dll|libpng16-16.dll|libpng15-15.dll|libpng15.dll|libpng14-14.dll|libpng14.dll|libpng12.dll"
#define LIBJPEG_LIBNAME "libjpeg.dll"
#define LIBOGG_LIBNAME "libogg-0.dll|libogg.dll"
#define LIBVORBIS_LIBNAME "libvorbis-0.dll|libvorbis.dll|vorbis.dll"
#define LIBVORBISFILE_LIBNAME "libvorbisfile-3.dll|libvorbisfile.dll|vorbisfile.dll"
#define LIBTHEORA_LIBNAME "libtheora-0.dll|libtheora.dll"
#define LIBFREETYPE_LIBNAME "libfreetype-6.dll|freetype6.dll"

#ifdef NDEBUG
#define BUILDSTRING "Win32 RELEASE"
#else
#define BUILDSTRING "Win32 DEBUG"
#endif

#define OSNAME "Windows"

#define STEAMQUERY_OS 'w'

#ifdef _M_IX86
#define CPUSTRING "x86"
#elif defined ( __x86_64__ ) || defined( _M_AMD64 )
#define CPUSTRING "x64"
#elif defined ( _M_ALPHA )
#define CPUSTRING "axp"
#endif

// doh, some compilers need a _ prefix for variables so they can be
// used in asm code
#ifdef __GNUC__     // mingw
#define VAR( x )    "_" # x
#else
#define VAR( x )    # x
#endif

#ifdef _MSC_VER
#define HAVE___CDECL
#endif

#ifdef __GNUC__
#define HAVE_TYPEOF
#endif

#include <malloc.h>
#define HAVE__ALLOCA

typedef int socklen_t;

typedef unsigned long ioctl_param_t;

typedef uintptr_t socket_handle_t;

#elif defined ( __APPLE__ ) && defined ( __MACH__ )

#ifndef __MACOSX__
#define __MACOSX__
#endif

#define HAVE_INLINE

#ifndef HAVE_STRCASECMP // SDL_config.h seems to define this too...
#define HAVE_STRCASECMP
#endif

//#define HAVE_TYPEOF

#define MUMBLE_SUPPORT
#define OPENAL_RUNTIME

// FIXME: move these to CMakeLists.txt
#define LIBZ_LIBNAME "libz.dylib"
#define LIBCURL_LIBNAME "libcurl.4.dylib|libcurl.3.dylib|libcurl.2.dylib"
#define LIBPNG_LIBNAME "libpng16.16.dylib|libpng15.15.dylib|libpng14.14.dylib|libpng12.0.dylib"
#define LIBJPEG_LIBNAME "libjpeg.62.dylib"
#define LIBOGG_LIBNAME "libogg.0.dylib|libogg.dylib"
#define LIBVORBIS_LIBNAME "libvorbis.dylib"
#define LIBVORBISFILE_LIBNAME "libvorbisfile.dylib"
#define LIBTHEORA_LIBNAME "libtheora.0.dylib|libtheora.dylib"
#define LIBFREETYPE_LIBNAME "libfreetype.6.dylib|libfreetype.dylib"

#define STEAMQUERY_OS 'o'

#define VAR( x ) # x

#include <alloca.h>

typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#else

#define HAVE_INLINE

#ifndef HAVE_STRCASECMP // SDL_config.h seems to define this too...
#define HAVE_STRCASECMP
#endif

#define HAVE_TYPEOF

#ifndef __ANDROID__
#define MUMBLE_SUPPORT
#define OPENAL_RUNTIME
#endif

// FIXME: move these to CMakeLists.txt
#define LIBZ_LIBNAME "libz.so.1|libz.so"
#define LIBCURL_LIBNAME "libcurl.so.4|libcurl.so.3|libcurl.so"
#define LIBPNG_LIBNAME "libpng16.so.16|libpng15.so.15|libpng14.so.14|libpng12.so.0|libpng.so"
#define LIBJPEG_LIBNAME "libjpeg.so.8|libjpeg.so"
#define LIBOGG_LIBNAME "libogg.so.0|libogg.so"
#define LIBVORBIS_LIBNAME "libvorbis.so.0|libvorbis.so"
#define LIBVORBISFILE_LIBNAME "libvorbisfile.so.3|libvorbisfile.so"
#define LIBTHEORA_LIBNAME "libtheora.so.0|libtheora.so"
#define LIBFREETYPE_LIBNAME "libfreetype.so.6|libfreetype.so"

#if defined ( __ANDROID__ )
#define BUILDSTRING "Android"
#define OSNAME "Android"
#define LIB_DIRECTORY "libs"
#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".so"
#endif

#define STEAMQUERY_OS 'l'

#if defined ( __ANDROID__ )
#ifdef __i386__
#define CPUSTRING "i386"
#elif defined ( __arm__ )
#define CPUSTRING "arm"
#elif defined ( _MIPS_ARCH )
#define CPUSTRING "mips"
#endif
#endif

#define VAR( x ) # x

#if defined ( __linux__ )
#include <alloca.h>
#endif

// wsw : aiwa : 64bit integers and integer-pointer types
typedef int ioctl_param_t;

typedef int socket_handle_t;

#define SOCKET_ERROR ( -1 )
#define INVALID_SOCKET ( -1 )

#endif

//==============================================

#if ( defined __i386__ || defined __x86_64__ ) && defined __GNUC__
#define HAVE__BUILTIN_ATOMIC
#elif ( defined _WIN32 )
#define HAVE__INTERLOCKED_API
#endif

//==============================================

#if !defined( __cplusplus )

#ifdef HAVE___INLINE
#ifndef inline
#define inline __inline
#endif
#elif !defined ( HAVE_INLINE )
#ifndef inline
#define inline
#endif
#endif

#endif

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

#if ( defined ( _M_IX86 ) || defined ( __i386__ ) || defined ( __ia64__ ) ) && !defined ( C_ONLY )
#define id386
#else
#ifdef id386
#undef id386
#endif
#endif

#ifndef BUILDSTRING
#define BUILDSTRING "NON-WIN32"
#endif

#ifndef CPUSTRING
#define CPUSTRING  "NON-WIN32"
#endif

#ifdef HAVE_TCHAR
#include <tchar.h>
#endif

#ifdef HAVE___CDECL
#define qcdecl __cdecl
#else
#define qcdecl
#endif

#if defined ( __GNUC__ )
#define ATTRIBUTE_ALIGNED( x ) __attribute__( ( aligned( x ) ) )
#define ATTRIBUTE_NOINLINE     __attribute__( ( noinline ) )
#define ATTRIBUTE_NAKED
#elif defined ( _MSC_VER )
#define ATTRIBUTE_ALIGNED( x ) __declspec( align( x ) )
#define ATTRIBUTE_NOINLINE
#define ATTRIBUTE_NAKED        __declspec( naked )
#else
#define ATTRIBUTE_ALIGNED( x )
#define ATTRIBUTE_NOINLINE
#define ATTRIBUTE_NAKED
#endif

#ifdef HAVE___STRTOI64
#define strtoll _strtoi64
#define strtoull _strtoi64
#endif

#ifdef ALIGN
#undef ALIGN
#endif

// the ALIGN macro as defined by Linux kernel
#ifdef HAVE_TYPEOF
#define __ALIGN_MASK( x,mask )    ( ( ( x ) + ( mask ) ) & ~( mask ) )
#define ALIGN( x,a )              __ALIGN_MASK( x,( typeof( x ) )( a ) - 1 )
#else
#define ALIGN( x, a ) ( ( ( x ) + ( ( size_t )( a ) - 1 ) ) & ~( ( size_t )( a ) - 1 ) )
#endif

#ifdef _M_AMD64
#define STR_TO_POINTER( str ) (void *)strtoll( str,NULL,0 )
#else
#define STR_TO_POINTER( str ) (void *)strtol( str,NULL,0 )
#endif

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
# define QF_DLL_IMPORT __declspec( dllimport )
# define QF_DLL_EXPORT __declspec( dllexport )
# define QF_DLL_LOCAL
#else
# if __GNUC__ >= 4
#  define QF_DLL_IMPORT __attribute__ ( ( visibility( "default" ) ) )
#  define QF_DLL_EXPORT __attribute__ ( ( visibility( "default" ) ) )
#  define QF_DLL_LOCAL  __attribute__ ( ( visibility( "hidden" ) ) )
# else
#  define QF_DLL_IMPORT
#  define QF_DLL_EXPORT
#  define QF_DLL_LOCAL
# endif
#endif

//==============================================

#ifndef NULL
#define NULL ( (void *)0 )
#endif
#ifdef __cplusplus
};
#endif

#endif // GAME_QARCH_H
