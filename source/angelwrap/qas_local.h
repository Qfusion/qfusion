/*
Copyright (C) 2008 German Garcia

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

#ifndef __QAS_LOCAL_H__
#define __QAS_LOCAL_H__

#define AS_USE_STLNAMES 1

#include "../gameshared/q_arch.h"
#include "../gameshared/q_math.h"
#include "../gameshared/q_shared.h"
#include "../gameshared/q_cvar.h"
#include "../gameshared/q_angeliface.h"
#include "qas_public.h"
#include "qas_syscalls.h"

// few fixes regarding Quake and std compatibility
#ifdef min
	#undef min
#endif
#ifdef max
	#undef max
#endif
#ifdef clamp
#undef clamp
#endif

#include <new>
#include <string>

#if defined ( _WIN32 ) || ( _WIN64 )
#include <string.h>
#endif

#define QAS_SECTIONS_SEPARATOR ';'
#define QAS_FILE_EXTENSION     ".as"

extern struct mempool_s *angelwrappool;

#define QAS_MemAlloc( pool, size ) trap_MemAlloc( pool, size, __FILE__, __LINE__ )
#define QAS_MemFree( mem ) trap_MemFree( mem, __FILE__, __LINE__ )
#define QAS_MemAllocPool( name ) trap_MemAllocPool( name, __FILE__, __LINE__ )
#define QAS_MemFreePool( pool ) trap_MemFreePool( pool, __FILE__, __LINE__ )
#define QAS_MemEmptyPool( pool ) trap_MemEmptyPool( pool, __FILE__, __LINE__ )

#define QAS_Malloc( size ) QAS_MemAlloc( angelwrappool, size )
#define QAS_Free( data ) QAS_MemFree( data )

#define QAS_NEW( x )        new( QAS_Malloc( sizeof( x ) ) )( x )
#define QAS_DELETE( ptr,x ) {void *tmp = ptr; ( ptr )->~x(); QAS_Free( tmp );}

#define QAS_NEWARRAY( x,cnt )  (x*)QAS_Malloc( sizeof( x ) * cnt )
#define QAS_DELETEARRAY( ptr ) QAS_Free( ptr )

int QAS_API( void );
int QAS_Init( void );
void QAS_ShutDown( void );
struct angelwrap_api_s *QAS_GetAngelExport( void );

#ifndef _MSC_VER
void QAS_Printf( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
void QAS_Error( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) ) __attribute__( ( noreturn ) );
#else
void QAS_Printf( _Printf_format_string_ const char *format, ... );
__declspec( noreturn ) void QAS_Error( _Printf_format_string_ const char *format, ... );
#endif

/******* C++ objects *******/
asIScriptEngine *qasCreateEngine( bool *asMaxPortability );
asIScriptContext *qasAcquireContext( asIScriptEngine *engine );
void qasReleaseContext( asIScriptContext *ctx );
void qasReleaseEngine( asIScriptEngine *engine );
asIScriptContext *qasGetActiveContext( void );
void qasWriteEngineDocsToFile( asIScriptEngine *engine, const char *path, const char *contextName, 
	bool markdown, bool singleFile,	unsigned andMask, unsigned notMask );

// array tools
CScriptArrayInterface *qasCreateArrayCpp( unsigned int length, void *ot );
void qasReleaseArrayCpp( CScriptArrayInterface *arr );

// string tools
asstring_t *qasStringFactoryBuffer( const char *buffer, unsigned int length );
void qasStringRelease( asstring_t *str );
asstring_t *qasStringAssignString( asstring_t *self, const char *string, unsigned int strlen );

// dictionary tools
CScriptDictionaryInterface *qasCreateDictionaryCpp( asIScriptEngine *engine );
void qasReleaseDictionaryCpp( CScriptDictionaryInterface *dict );

// any tools
CScriptAnyInterface *qasCreateAnyCpp( asIScriptEngine *engine );
void qasReleaseAnyCpp( CScriptAnyInterface *any );

// projects / bundles
asIScriptModule *qasLoadScriptProject( asIScriptEngine *engine, const char *moduleName, const char *rootDir, const char *dir, const char *filename, const char *ext );

#endif // __QAS_LOCAL_H__
