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
#include "addon/addon_string.h"
#include <string>
#include <sstream>
#include <vector>

struct mempool_s *angelwrappool;

static angelwrap_api_t angelExport;

struct angelwrap_api_s *QAS_GetAngelExport( void ) {
	return &angelExport;
}

void QAS_InitAngelExport( void ) {
	memset( &angelExport, 0, sizeof( angelExport ) );

	angelExport.angelwrap_api_version = ANGELWRAP_API_VERSION;

	angelExport.asCreateEngine = qasCreateEngine;
	angelExport.asReleaseEngine = qasReleaseEngine;
	angelExport.asWriteEngineDocsToFile = qasWriteEngineDocsToFile;

	angelExport.asAcquireContext = qasAcquireContext;
	angelExport.asReleaseContext = qasReleaseContext;
	angelExport.asGetActiveContext = qasGetActiveContext;

	angelExport.asStringFactoryBuffer = qasStringFactoryBuffer;
	angelExport.asStringRelease = qasStringRelease;
	angelExport.asStringAssignString = qasStringAssignString;

	angelExport.asCreateArrayCpp = qasCreateArrayCpp;
	angelExport.asReleaseArrayCpp = qasReleaseArrayCpp;

	angelExport.asCreateDictionaryCpp = qasCreateDictionaryCpp;
	angelExport.asReleaseDictionaryCpp = qasReleaseDictionaryCpp;

	angelExport.asCreateAnyCpp = qasCreateAnyCpp;
	angelExport.asReleaseAnyCpp = qasReleaseAnyCpp;

	angelExport.asLoadScriptProject = qasLoadScriptProject;
	angelExport.asScriptProjectMTime = qasScriptProjectMTime;
}

int QAS_API( void ) {
	return ANGELWRAP_API_VERSION;
}

int QAS_Init( void ) {
	angelwrappool = QAS_MemAllocPool( "Angelwrap script module" );
	QAS_Printf( "Initializing Angel Script\n" );

	srand( time( NULL ) );

	QAS_InitAngelExport();
	return 1;
}

void *QAS_Malloc( size_t size ) {
	return QAS_MemAlloc( angelwrappool, size );
}

void QAS_Free( void *data ) {
	QAS_MemFree( data );
}

void QAS_ShutDown( void ) {
	QAS_MemFreePool( &angelwrappool );
}

static std::string VarToString( void *value, asUINT typeId, int expandMembers, asIScriptEngine *engine, asUINT stringTypeId )
{
	if( value == 0 )
		return "<null>";

	std::stringstream s;
	if( typeId == asTYPEID_VOID )
		return "<void>";
	else if( typeId == asTYPEID_BOOL )
		return *(bool *)value ? "true" : "false";
	else if( typeId == asTYPEID_INT8 )
		s << (int)*(signed char *)value;
	else if( typeId == asTYPEID_INT16 )
		s << (int)*(signed short *)value;
	else if( typeId == asTYPEID_INT32 )
		s << *(signed int *)value;
	else if( typeId == asTYPEID_INT64 )
#if defined( _MSC_VER ) && _MSC_VER <= 1200
		s << "{...}"; // MSVC6 doesn't like the << operator for 64bit integer
#else
		s << *(asINT64 *)value;
#endif
	else if( typeId == asTYPEID_UINT8 )
		s << (unsigned int)*(unsigned char *)value;
	else if( typeId == asTYPEID_UINT16 )
		s << (unsigned int)*(unsigned short *)value;
	else if( typeId == asTYPEID_UINT32 )
		s << *(unsigned int *)value;
	else if( typeId == asTYPEID_UINT64 )
#if defined( _MSC_VER ) && _MSC_VER <= 1200
		s << "{...}"; // MSVC6 doesn't like the << operator for 64bit integer
#else
		s << *(asQWORD *)value;
#endif
	else if( typeId == asTYPEID_FLOAT )
		s << *(float *)value;
	else if( typeId == asTYPEID_DOUBLE )
		s << *(double *)value;
	else if( typeId == stringTypeId )
		s << "\"" << ( (asstring_t *)value )->buffer << "\"";
	else if( ( typeId & asTYPEID_MASK_OBJECT ) == 0 ) {
		// The type is an enum
		s << *(asUINT *)value;
	} else if( typeId & asTYPEID_SCRIPTOBJECT ) {
		// Dereference handles, so we can see what it points to
		if( typeId & asTYPEID_OBJHANDLE )
			value = *(void **)value;

		// Print the address of the object
		s << "0x" << value;
	} else {
		// Dereference handles, so we can see what it points to
		if( typeId & asTYPEID_OBJHANDLE )
			value = *(void **)value;

		// Print the address for reference types so it will be
		// possible to see when handles point to the same object
		if( engine ) {
			asITypeInfo *type = engine->GetTypeInfoById( typeId );

			if( type ) {
				if( type->GetFlags() & asOBJ_REF )
					s << "0x" << value;

				qasObjToString_t toString = (qasObjToString_t)type->GetUserData( 33 );
				if( toString ) {
					asstring_t *str = toString( value );
					s << ( ( type->GetFlags() & asOBJ_REF )  ? " " : "" ) << str->buffer;
					objectString_Free( str );
				}
			}
		} else {
			s << "{no engine}";
		}
	}

	return s.str();
}

static bool ScriptVarHasProperties( void *ptr, int typeId )
{
	if( !( typeId & asTYPEID_SCRIPTOBJECT ) )
		return false;

	// Dereference handles, so we can see what it points to
	if( typeId & asTYPEID_OBJHANDLE )
		ptr = *(void **)ptr;

	asIScriptObject *obj = (asIScriptObject *)ptr;
	return ( obj && obj->GetPropertyCount() > 0 );
}

static void FillScriptVarInfo( void *ptr, int typeId, asIScriptEngine *engine, asUINT stringTypeId, angelwrap_variable_t &var )
{
	var.value = QAS_CopyString( VarToString( ptr, typeId, 0, engine, stringTypeId ).c_str() );
	var.type = QAS_CopyString( engine->GetTypeDeclaration( typeId ) );
	var.hasProperties = ScriptVarHasProperties( ptr, typeId );
}

static void GetScriptVarProperty( asUINT n, void *bPtr, asITypeInfo *type, const char **ppropName, void **pptr, int *ptypeId )
{
	void *ptr = NULL;
	int	  typeId = 0;
	int	  offset = 0;
	bool  isReference = 0;
	int	  compositeOffset = 0;
	bool  isCompositeIndirect = false;
	const char *propName = 0;

	if( n >= type->GetPropertyCount() ) {
		return;
	}

	type->GetProperty( n, &propName, &typeId, 0, 0, &offset, &isReference, 0, &compositeOffset, &isCompositeIndirect );

	ptr = (void *)( ( (asBYTE *)bPtr ) + compositeOffset );
	if( isCompositeIndirect )
		ptr = *(void **)ptr;
	ptr = (void *)( ( (asBYTE *)ptr ) + offset );
	if( isReference )
		ptr = *(void **)ptr;

	*ppropName = propName;
	*pptr = ptr;
	*ptypeId = typeId;
}

static void ListScriptVarProperties( void *ptr, int typeId,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine, std::vector<std::string> &path )
{
	const char *propName = 0;
	void *		pPtr = 0;
	int			pTypeId = 0;

	while( !path.empty() ) {
		if( ( typeId & asTYPEID_SCRIPTOBJECT ) != asTYPEID_SCRIPTOBJECT ) {
			return;
		}
		if( typeId & asTYPEID_OBJHANDLE ) {
			ptr = *(void **)ptr;
		}

		asITypeInfo *type = engine->GetTypeInfoById( typeId );

		asUINT n;
		for( n = 0; n < type->GetPropertyCount(); n++ ) {
			GetScriptVarProperty( n, ptr, type, &propName, &pPtr, &pTypeId );
			if( propName != path[0] )
				continue;

			ptr = pPtr;
			typeId = pTypeId;
			path.erase( path.begin() );
			break;
		}

		if( n == type->GetPropertyCount() ) {
			return;
		}
	}

	if( ( typeId & asTYPEID_SCRIPTOBJECT ) != asTYPEID_SCRIPTOBJECT ) {
		return;
	}
	if( typeId & asTYPEID_OBJHANDLE ) {
		ptr = *(void **)ptr;
	}

	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	asITypeInfo *type = engine->GetTypeInfoById( typeId );
	for( asUINT n = 0; n < type->GetPropertyCount(); n++ ) {
		GetScriptVarProperty( n, ptr, type, &propName, &pPtr, &pTypeId );

		angelwrap_variable_t var;
		var.name = QAS_CopyString( propName );
		FillScriptVarInfo( pPtr, pTypeId, engine, stringTypeId, var );
		res.push_back( var );
	}
}

static void ListScriptFuncLocals( asIScriptContext *ctx, int stackLevel, asIScriptFunction *func,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine )
{
	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	for( asUINT n = func->GetVarCount(); n-- > 0; ) {
		if( ctx->IsVarInScope( n, stackLevel ) ) {
			void *ptr = ctx->GetAddressOfVar( n, stackLevel );
			int	  typeId = ctx->GetVarTypeId( n, stackLevel );

			angelwrap_variable_t var;
			var.name = QAS_CopyString( ctx->GetVarName( n, stackLevel ) );
			FillScriptVarInfo( ptr, typeId, engine, stringTypeId, var );
			res.push_back( var );
		}
	}
}

static void ListScriptFuncLocalVarProperties( asIScriptContext *ctx, int stackLevel, asIScriptFunction *func,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine, std::vector<std::string> &path )
{
	if( path.empty() ) {
		return;
	}

	for( asUINT n = func->GetVarCount(); n-- > 0; ) {
		if( !ctx->IsVarInScope( n, stackLevel ) ) {
			continue;
		}
		if( path[0] != ctx->GetVarName( n, stackLevel ) ) {
			continue;
		}

		void *ptr = ctx->GetAddressOfVar( n, stackLevel );
		int	  typeId = ctx->GetVarTypeId( n, stackLevel );
		path.erase( path.begin() );

		ListScriptVarProperties( ptr, typeId, res, engine, path );
		return;
	}
}

static void ListScriptModuleGlobals( asIScriptModule *mod,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine )
{
	asUINT stringTypeId = engine->GetStringFactoryReturnTypeId();

	for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ ) {
		void *		ptr = mod->GetAddressOfGlobalVar( n );
		int			typeId = 0;
		const char *name;

		mod->GetGlobalVar( n, &name, NULL, &typeId );

		angelwrap_variable_t var;
		var.name = QAS_CopyString( name );
		FillScriptVarInfo( ptr, typeId, engine, stringTypeId, var );
		res.push_back( var );
	}
}

static void ListScriptModuleGlobalVarProperties( asIScriptModule *mod,
	std::vector<angelwrap_variable_t> &res, asIScriptEngine *engine, std::vector<std::string> &path )
{
	for( asUINT n = 0; n < mod->GetGlobalVarCount(); n++ ) {
		void *		ptr = mod->GetAddressOfGlobalVar( n );
		int			typeId = 0;
		const char *name;

		mod->GetGlobalVar( n, &name, NULL, &typeId );
		if( path[0] != name ) {
			continue;
		}

		path.erase( path.begin() );
		ListScriptVarProperties( ptr, typeId, res, engine, path );
		return;
	}
}

angelwrap_variable_t **QAS_asGetVariables( int stackLevel, const char *scope_ )
{
	asIScriptContext *ctx = qasGetActiveContext();

	if( ctx == 0 ) {
		ctx = qasGetExceptionContext();
	}
	if( ctx == 0 ) {
		return NULL;
	}

	asIScriptEngine *  engine = ctx->GetEngine();
	asIScriptFunction *func = ctx->GetFunction( stackLevel );
	asIScriptModule *  mod = func->GetModule();

	std::string scope( scope_ );
	std::vector<std::string> path;
	size_t pos = scope.find( "%", 1 );
	if( pos != std::string::npos ) {
		pos = scope.find( ".", pos + 1 );
	}

	if( pos != std::string::npos ) {
		std::string s = scope.substr( pos + 1 );
		for( pos = s.find( "." ); pos != std::string::npos; ) {
			path.push_back( s.substr( 0, pos ) );
			s.erase( 0, pos + 1 );
			pos = s.find( "." );
		}

		if( !s.empty() ) {
			path.push_back( s );
		}
	}

	std::vector<angelwrap_variable_t> res;

	if( scope.rfind( "%local%" ) == 0 ) {
		if( path.empty() ) {
			ListScriptFuncLocals( ctx, stackLevel, func, res, engine );
		} else {
			ListScriptFuncLocalVarProperties( ctx, stackLevel, func, res, engine, path );
		}
	} else if( scope.rfind( "%this%" ) == 0 ) {
		path.clear();

		if( func->GetObjectType() ) {
			ListScriptVarProperties( ctx->GetThisPointer( stackLevel ),
				ctx->GetThisTypeId( stackLevel ), res, engine, path );
		}
	} else if( scope.rfind( "%module%" ) == 0 ) {
		if( mod ) {
			if( path.empty() ) {
				ListScriptModuleGlobals( mod, res, engine );
			} else {
				ListScriptModuleGlobalVarProperties( mod, res, engine, path );
			}
		}
	}

	angelwrap_variable_t **cres = ( angelwrap_variable_t ** )QAS_Malloc( ( res.size() + 1 ) * sizeof( angelwrap_variable_t * ) );
	for( size_t n = 0; n < res.size(); n++ ) {
		cres[n] = (angelwrap_variable_t *)QAS_Malloc( sizeof( angelwrap_variable_t ) );
		*( cres[n] ) = res[n];
	}
	cres[res.size()] = NULL;

	return cres;
}

angelwrap_stack_frame_t **QAS_GetCallstack( void )
{
	asIScriptContext *ctx = qasGetActiveContext();

	if( ctx == 0 ) {
		ctx = qasGetExceptionContext();
	}
	if( ctx == 0 ) {
		return NULL;
	}

	asUINT ss = ctx->GetCallstackSize();

	angelwrap_stack_frame_t **stack =
		(angelwrap_stack_frame_t **)QAS_Malloc( sizeof( angelwrap_stack_frame_t * ) * ( ss + 1 ) );
	for( asUINT n = 0; n < ss; n++ ) {
		const char *file = 0;
		int			lineNbr = ctx->GetLineNumber( n, 0, &file );
		const char *decl = ctx->GetFunction( n )->GetDeclaration();

		stack[n] = (angelwrap_stack_frame_t *)QAS_Malloc( sizeof( angelwrap_stack_frame_t ) );
		stack[n]->file = QAS_CopyString( file );
		stack[n]->line = lineNbr;
		stack[n]->func = QAS_CopyString( decl );
	}
	stack[ss] = NULL;

	return stack;
}

char *_QAS_CopyString( const char *in, const char *filename, int fileline )
{
	char *out;

	out = (char *)trap_MemAlloc( angelwrappool, strlen( in ) + 1, filename, fileline );
	strcpy( out, in );
	return out;
}

void QAS_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void QAS_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

#ifndef ANGELWRAP_HARD_LINKED

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void Com_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}
#endif
