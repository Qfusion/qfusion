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

#pragma once

#define AS_USE_STLNAMES 1

#include "qcommon/qcommon.h"
#include "gameshared/q_angeliface.h"
#include "qas_public.h"

// few fixes regarding Quake and std compatibility
#ifdef min
	#undef min
#endif
#ifdef max
	#undef max
#endif

#include <new>
#include <string>

#if defined ( _WIN32 ) || ( _WIN64 )
#include <string.h>
#endif

#define QAS_SECTIONS_SEPARATOR ';'
#define QAS_FILE_EXTENSION     ".as"

extern struct mempool_s *angelwrappool;

#define QAS_NEW( x )        new( Mem_Alloc( angelwrappool, sizeof( x ) ) )( x )
#define QAS_DELETE( ptr,x ) {void *tmp = ptr; ( ptr )->~x(); Mem_Free( tmp );}

#define QAS_NEWARRAY( x,cnt )  (x*)Mem_Alloc( angelwrappool, sizeof( x ) * cnt )
#define QAS_DELETEARRAY( ptr ) Mem_Free( ptr )

/******* C++ objects *******/
asIScriptEngine *qasCreateEngine( bool *asMaxPortability );
asIScriptContext *qasAcquireContext( asIScriptEngine *engine );
void qasReleaseContext( asIScriptContext *ctx );
void qasReleaseEngine( asIScriptEngine *engine );
asIScriptContext *qasGetActiveContext( void );
void qasWriteEngineDocsToFile( asIScriptEngine *engine, const char *path, bool singleFile, bool markdown, unsigned andMask, unsigned notMask );

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
