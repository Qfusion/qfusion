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

#include "qas_precompiled.h"
#include "addon/addon_math.h"
#include "addon/addon_scriptarray.h"
#include "addon/addon_string.h"
#include "addon/addon_dictionary.h"
#include "addon/addon_time.h"
#include "addon/addon_any.h"
#include "addon/addon_vec3.h"
#include "addon/addon_cvar.h"
#include "addon/addon_stringutils.h"

#include <list>

static void *qasAlloc( size_t size )
{
	return QAS_Malloc( size );
}

static void qasFree( void *mem )
{
	QAS_Free( mem );
}

// ============================================================================

// list of contexts in the same engine
typedef std::list<asIScriptContext *> qasContextList;

// engine -> contexts key/value pairs
typedef std::map<asIScriptEngine *, qasContextList> qasEngineContextMap;

qasEngineContextMap contexts;

// ============================================================================

static void qasMessageCallback( const asSMessageInfo *msg )
{
	const char *msg_type;

	switch( msg->type )
	{
		case asMSGTYPE_ERROR:
			msg_type = S_COLOR_RED "ERROR: ";
			break;
		case asMSGTYPE_WARNING:
			msg_type = S_COLOR_YELLOW "WARNING: ";
			break;
		case asMSGTYPE_INFORMATION:
		default:
			msg_type = S_COLOR_CYAN "ANGELSCRIPT: ";
			break;
	}

	Com_Printf( "%s%s %d:%d: %s\n", msg_type, msg->section, msg->row, msg->col, msg->message );
}

static void qasExceptionCallback( asIScriptContext *ctx )
{
	int line, col;
	asIScriptFunction *func;
	const char *sectionName, *exceptionString, *funcDecl;

	line = ctx->GetExceptionLineNumber( &col, &sectionName );
	func = ctx->GetExceptionFunction();
	exceptionString = ctx->GetExceptionString();
	funcDecl = ( func ? func->GetDeclaration( true ) : "" );

	Com_Printf( S_COLOR_RED "ASModule::ExceptionCallback:\n%s %d:%d %s: %s\n", sectionName, line, col, funcDecl, exceptionString );
}

asIScriptEngine *qasCreateEngine( bool *asMaxPortability )
{
	asIScriptEngine *engine;

	// register the global memory allocation and deallocation functions
	asSetGlobalMemoryFunctions( qasAlloc, qasFree );

	// always new

	// ask for angelscript initialization and script engine creation
	engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
	if( !engine )
		return NULL;

	if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) )
	{
		QAS_Printf( "* angelscript library with AS_MAX_PORTABILITY detected\n" );
		engine->Release();
		return NULL;
	}

	*asMaxPortability = false;

	// The script compiler will write any compiler messages to the callback.
	engine->SetMessageCallback( asFUNCTION( qasMessageCallback ), 0, asCALL_CDECL );
	engine->SetEngineProperty( asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1 );

	PreRegisterMathAddon( engine );
	PreRegisterScriptArray( engine, true );
	PreRegisterStringAddon( engine );
	PreRegisterScriptDictionary( engine );
	PreRegisterTimeAddon( engine );
	PreRegisterScriptAny( engine );
	PreRegisterVec3Addon( engine );
	PreRegisterCvarAddon( engine );
	PreRegisterStringUtilsAddon( engine );

	RegisterMathAddon( engine );
	RegisterScriptArray( engine, true );
	RegisterStringAddon( engine );
	RegisterScriptDictionary( engine );
	RegisterTimeAddon( engine );
	RegisterScriptAny( engine );
	RegisterVec3Addon( engine );
	RegisterCvarAddon( engine );
	RegisterStringUtilsAddon( engine );

	return engine;
}

void qasReleaseEngine( asIScriptEngine *engine )
{
	if( !engine )
		return;

	// release all contexts linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ )
	{
		asIScriptContext *ctx = *it;
		ctx->Release();
	}
	ctxList.clear();

	qasEngineContextMap::iterator it = contexts.find( engine );
	if( it != contexts.end() ) {
		contexts.erase( it );
	}

	engine->Release();
}

static asIScriptContext *qasCreateContext( asIScriptEngine *engine )
{
	asIScriptContext *ctx;
	int error;

	if( engine == NULL )
		return NULL;

	// always new
	ctx = engine->CreateContext();
	if( !ctx )
		return NULL;

	// We don't want to allow the script to hang the application, e.g. with an
	// infinite loop, so we'll use the line callback function to set a timeout
	// that will abort the script after a certain time. Before executing the 
	// script the timeOut variable will be set to the time when the script must 
	// stop executing. 

	error = ctx->SetExceptionCallback( asFUNCTION(qasExceptionCallback), NULL, asCALL_CDECL );
	if( error < 0 )
	{
		ctx->Release();
		return NULL;
	}

	qasContextList &ctxList = contexts[engine];
	ctxList.push_back( ctx );

	return ctx;
}

void qasReleaseContext( asIScriptContext *ctx )
{
	if( !ctx )
		return;

	asIScriptEngine *engine = ctx->GetEngine();
	qasContextList &ctxList = contexts[engine];
	ctxList.remove( ctx );

	ctx->Release();
}

asIScriptContext *qasAcquireContext( asIScriptEngine *engine )
{
	if( !engine )
		return NULL;

	// try to reuse any context linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ )
	{
		asIScriptContext *ctx = *it;
		if( ctx->GetState() == asEXECUTION_FINISHED )
			return ctx;
	}

	// if no context was available, create a new one
	return qasCreateContext( engine );
}

asIScriptContext *qasGetActiveContext( void )
{
	return asGetActiveContext();
}

/*************************************
* Array tools
**************************************/

CScriptArrayInterface *qasCreateArrayCpp( unsigned int length, void *ot )
{
	return QAS_NEW(CScriptArray)( length, static_cast<asIObjectType *>( ot ) );
}

void qasReleaseArrayCpp( CScriptArrayInterface *arr )
{
	arr->Release();
}

/*************************************
* Strings
**************************************/

asstring_t *qasStringFactoryBuffer( const char *buffer, unsigned int length )
{
	return objectString_FactoryBuffer( buffer, length );
}

void qasStringRelease( asstring_t *str )
{
	objectString_Release( str );
}

asstring_t *qasStringAssignString( asstring_t *self, const char *string, unsigned int strlen )
{
	return objectString_AssignString( self, string, strlen );
}

/*************************************
* Dictionary
**************************************/

CScriptDictionaryInterface *qasCreateDictionaryCpp( asIScriptEngine *engine )
{
	return QAS_NEW(CScriptDictionary)( engine );
}

void qasReleaseDictionaryCpp( CScriptDictionaryInterface *dict )
{
	dict->Release();
}

/*************************************
* Any
**************************************/

CScriptAnyInterface *qasCreateAnyCpp( asIScriptEngine *engine )
{
	return QAS_NEW(CScriptAny)( engine );
}

void qasReleaseAnyCpp( CScriptAnyInterface *any )
{
	any->Release();
}
