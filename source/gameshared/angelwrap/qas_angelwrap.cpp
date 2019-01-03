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

#include "qas_local.h"
#include "addon/addon_math.h"
#include "addon/addon_scriptarray.h"
#include "addon/addon_string.h"
#include "addon/addon_dictionary.h"
#include "addon/addon_time.h"
#include "addon/addon_any.h"
#include "addon/addon_vec3.h"
#include "addon/addon_vec4.h"
#include "addon/addon_cvar.h"
#include "addon/addon_stringutils.h"

#include <list>

static void *qasAlloc( size_t size ) {
	return Mem_Alloc( angelwrappool, size );
}

static void qasFree( void *mem ) {
	Mem_Free( mem );
}

// ============================================================================

// list of contexts in the same engine
typedef std::list<asIScriptContext *> qasContextList;

// engine -> contexts key/value pairs
typedef std::map<asIScriptEngine *, qasContextList> qasEngineContextMap;

qasEngineContextMap contexts;

// ============================================================================

static void qasMessageCallback( const asSMessageInfo *msg ) {
	const char *msg_type;

	switch( msg->type ) {
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

static void qasExceptionCallback( asIScriptContext *ctx ) {
	int line, col;
	asIScriptFunction *func;
	const char *sectionName, *exceptionString, *funcDecl;

	line = ctx->GetExceptionLineNumber( &col, &sectionName );
	func = ctx->GetExceptionFunction();
	exceptionString = ctx->GetExceptionString();
	funcDecl = ( func ? func->GetDeclaration( true ) : "" );

	Com_Printf( S_COLOR_RED "ASModule::ExceptionCallback:\n%s %d:%d %s: %s\n", sectionName, line, col, funcDecl, exceptionString );
}

asIScriptEngine *qasCreateEngine( bool *asMaxPortability ) {
	asIScriptEngine *engine;

	// register the global memory allocation and deallocation functions
	asSetGlobalMemoryFunctions( qasAlloc, qasFree );

	// always new

	// ask for angelscript initialization and script engine creation
	engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
	if( !engine ) {
		return NULL;
	}

	if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) ) {
		Com_Printf( "* angelscript library with AS_MAX_PORTABILITY detected\n" );
		engine->Release();
		return NULL;
	}

	*asMaxPortability = false;

	// The script compiler will write any compiler messages to the callback.
	engine->SetMessageCallback( asFUNCTION( qasMessageCallback ), 0, asCALL_CDECL );
	engine->SetEngineProperty( asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, 1 );
	engine->SetDefaultAccessMask( 0xFFFFFFFF );

	PreRegisterMathAddon( engine );
	PreRegisterScriptArray( engine, true );
	PreRegisterStringAddon( engine );
	PreRegisterScriptDictionary( engine );
	PreRegisterTimeAddon( engine );
	PreRegisterScriptAny( engine );
	PreRegisterVec3Addon( engine );
	PreRegisterVec4Addon( engine );
	PreRegisterCvarAddon( engine );
	PreRegisterStringUtilsAddon( engine );

	RegisterMathAddon( engine );
	RegisterScriptArray( engine, true );
	RegisterStringAddon( engine );
	RegisterScriptDictionary( engine );
	RegisterTimeAddon( engine );
	RegisterScriptAny( engine );
	RegisterVec3Addon( engine );
	RegisterVec4Addon( engine );
	RegisterCvarAddon( engine );
	RegisterStringUtilsAddon( engine );

	engine->SetDefaultAccessMask( 0x1 );

	return engine;
}

void qasWriteEngineDocsToFile( asIScriptEngine *engine, const char *path, bool singleFile, bool markdown, unsigned andMask, unsigned notMask ) {
	int i, j, filenum;
	const char *str = 0;    // for temporary strings
	const char *singleFn;
	std::string spath( path );

	if( spath[spath.size() - 1] != '/' ) {
		spath += '/';
	}

	if( markdown ) {
		singleFn = "API.md";
	} else {
		singleFn = "API.h";
	}

	// global file
	std::string global_file( spath );
	if( singleFile ) {
		global_file += singleFn;
	} else {
		global_file += "globals.h";
	}

	if( FS_FOpenFile( global_file.c_str(), &filenum, FS_WRITE ) == -1 ) {
		Com_Printf( "ASModule::dumpAPI: Couldn't write %s.\n", global_file.c_str() );
		return;
	}

	//
	// global enums
	if( markdown ) {
		str = "\r\n## Enums\r\n```c++\r\n";
	} else {
		str = "/**\r\n * Enums\r\n */\r\n";
	}
	FS_Write( str, strlen( str ), filenum );

	int enumCount = engine->GetEnumCount();
	for( i = 0; i < enumCount; i++ ) {
		int enumTypeId;
		const char *enumName = engine->GetEnumByIndex( i, &enumTypeId, NULL, NULL, NULL );

		str = "typedef enum\r\n{\r\n";
		FS_Write( str, strlen( str ), filenum );

		int enumValueCount = engine->GetEnumValueCount( enumTypeId );
		for( j = 0; j < enumValueCount; j++ ) {
			int outValue;
			const char *valueName = engine->GetEnumValueByIndex( enumTypeId, j, &outValue );
			str = va( "\t%s = 0x%x,\r\n", valueName, outValue );
			FS_Write( str, strlen( str ), filenum );
		}

		str = va( "} %s;\r\n\r\n", enumName );
		FS_Write( str, strlen( str ), filenum );
	}

	if( markdown ) {
		str = "```\r\n";
		FS_Write( str, strlen( str ), filenum );
	}

	//
	// global properties
	if( markdown ) {
		str = "\r\n## Global properties\r\n```c++\r\n";
	} else {
		str = "\r\n/**\r\n * Global properties\r\n */\r\n";
	}
	FS_Write( str, strlen( str ), filenum );

	int propertyCount = engine->GetGlobalPropertyCount();
	for( i = 0; i < propertyCount; i++ ) {
		const char *propertyName;
		const char *propertyNamespace;
		int propertyTypeId;
		bool propertyIsConst;
		asDWORD mask;

		if( engine->GetGlobalPropertyByIndex( i, &propertyName, &propertyNamespace, &propertyTypeId, &propertyIsConst, NULL, NULL, &mask ) >= 0 ) {
			if( ( mask & andMask ) == 0 ) {
				continue;
			}
			if( ~( mask & notMask ) == 0 ) {
				continue;
			}

			const char *decl;
			const char *constAttr = propertyIsConst ? "const " : "";

			if( propertyNamespace && *propertyNamespace ) {
				decl = va( "%s%s %s::%s;\r\n", constAttr,
				engine->GetTypeDeclaration( propertyTypeId ), propertyNamespace, propertyName );
			} else {
				decl = va( "%s%s %s;\r\n", constAttr,
					engine->GetTypeDeclaration( propertyTypeId ), propertyName );
			}

			FS_Write( decl, strlen( decl ), filenum );
		}
	}

	if( markdown ) {
		str = "```\r\n";
		FS_Write( str, strlen( str ), filenum );
	}

	//
	// global functions
	if( markdown ) {
		str = "\r\n## Global functions\r\n```c++\r\n";
	} else {
		str = "\r\n/**\r\n * Global functions\r\n */\r\n";
	}
	FS_Write( str, strlen( str ), filenum );

	int functionCount = engine->GetGlobalFunctionCount();
	for( i = 0; i < functionCount; i++ ) {
		asIScriptFunction *func = engine->GetGlobalFunctionByIndex( i );
		if( func ) {
			asDWORD mask = func->GetAccessMask();

			if( ( mask & andMask ) == 0 ) {
				continue;
			}
			if( ~( mask & notMask ) == 0 ) {
				continue;
			}

			const char *decl = va( "%s;\r\n", func->GetDeclaration( false, false, true ) );
			FS_Write( decl, strlen( decl ), filenum );
		}
	}

	if( markdown ) {
		str = "```\r\n";
		FS_Write( str, strlen( str ), filenum );
	}

	FS_FCloseFile( filenum );
	Com_Printf( "Wrote %s\n", global_file.c_str() );

	// classes
	int objectCount = engine->GetObjectTypeCount();
	bool wroteClassesHeader = !singleFile;
	for( i = 0; i < objectCount; i++ ) {
		asIObjectType *objectType = engine->GetObjectTypeByIndex( i );
		std::map<std::string, bool> skipSetters;
		asDWORD mask = objectType->GetAccessMask();

		if( ( mask & andMask ) == 0 ) {
			continue;
		}
		if( ~( mask & notMask ) == 0 ) {
			continue;
		}

		if( objectType ) {
			// class file
			int mode;
			std::string class_file( spath );
			
			if( singleFile ) {
				mode = FS_APPEND;
				class_file += singleFn;
			} else {
				mode = FS_WRITE;
				class_file += objectType->GetName();
				class_file += ".h";
			}

			if( FS_FOpenFile( class_file.c_str(), &filenum, mode ) == -1 ) {
				Com_Printf( "ASModule::dumpAPI: Couldn't write %s.\n", class_file.c_str() );
				continue;
			}

			if( !wroteClassesHeader ) {
				// global functions
				if( markdown ) {
					str = "\r\n## Classes\r\n\r\n";
				} else {
					str = "\r\n/**\r\n * Classes\r\n */\r\n\r\n";
				}
				FS_Write( str, strlen( str ), filenum );

				wroteClassesHeader = true;
			}

			if( markdown ) {
				str = va( "### %s\r\n\r\n```c++\r\n", objectType->GetName() );
			} else {
				str = va( "/**\r\n * %s\r\n */\r\n", objectType->GetName() );
			}
			FS_Write( str, strlen( str ), filenum );
			str = va( "class %s\r\n{\r\npublic:", objectType->GetName() );
			FS_Write( str, strlen( str ), filenum );

			// properties
			str = "\r\n\t/* object properties */\r\n";
			FS_Write( str, strlen( str ), filenum );

			int memberCount = objectType->GetPropertyCount();
			for( j = 0; j < memberCount; j++ ) {
				const char *decl = va( "\t%s;\r\n\r\n", objectType->GetPropertyDeclaration( j ) );
				FS_Write( decl, strlen( decl ), filenum );
			}

			// properties with accessors
			{
				int methodCount = objectType->GetMethodCount();
				for( j = 0; j < methodCount; j++ ) {
					asIScriptFunction *method = objectType->GetMethodByIndex( j );
					const char *methodName = method->GetName();
					if( strncmp( methodName, "get_", 4 ) ) {
						continue;
					}

					std::string setterName = "set_";
					setterName += methodName + 4;
					asIScriptFunction *setter = objectType->GetMethodByName( setterName.c_str() );
					bool readOnly = setter == nullptr;
					skipSetters[setterName] = true;

					const char *typeDecl = engine->GetTypeDeclaration( method->GetReturnTypeId() );
					const char *decl = va( "\t%s%s %s;\r\n\r\n", readOnly ? "const " : "", typeDecl, methodName + 4 );
					FS_Write( decl, strlen( decl ), filenum );
				}
			}

			// behaviours
			str = "\r\n\t/* object behaviors */\r\n";
			FS_Write( str, strlen( str ), filenum );

			int behaviourCount = objectType->GetBehaviourCount();
			for( j = 0; j < behaviourCount; j++ ) {
				// ch : FIXME: obscure function names in behaviours
				asEBehaviours behaviourType;
				asIScriptFunction *function = objectType->GetBehaviourByIndex( j, &behaviourType );
				if( behaviourType == asBEHAVE_ADDREF || behaviourType == asBEHAVE_RELEASE ) {
					continue;
				}

				const char *decl = va( "\t%s;%s\r\n\r\n", function->GetDeclaration( false, false, true ),
					( behaviourType == asBEHAVE_FACTORY ? " /* factory */ " : "" ) );
				FS_Write( decl, strlen( decl ), filenum );
			}

			// methods
			str = "\r\n\t/* object methods */\r\n";
			FS_Write( str, strlen( str ), filenum );

			int methodCount = objectType->GetMethodCount();
			for( j = 0; j < methodCount; j++ ) {
				asIScriptFunction *method = objectType->GetMethodByIndex( j );
				const char *methodName = method->GetName();

				// skip accesors for properties we have already dumped
				if( !strncmp( methodName, "get_", 4 ) ) {
					continue;
				}
				if( skipSetters.find( methodName ) != skipSetters.end() ) {
					continue;
				}

				const char *decl = va( "\t%s;\r\n\r\n", method->GetDeclaration( false, false, true ) );
				FS_Write( decl, strlen( decl ), filenum );
			}

			str = "};\r\n\r\n";
			FS_Write( str, strlen( str ), filenum );

			if( markdown ) {
				str = "```\r\n\r\n";
				FS_Write( str, strlen( str ), filenum );
			}

			FS_FCloseFile( filenum );

			Com_Printf( "Wrote %s\n", class_file.c_str() );
		}
	}
}

void qasReleaseEngine( asIScriptEngine *engine ) {
	if( !engine ) {
		return;
	}

	// release all contexts linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ ) {
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

static asIScriptContext *qasCreateContext( asIScriptEngine *engine ) {
	asIScriptContext *ctx;
	int error;

	if( engine == NULL ) {
		return NULL;
	}

	// always new
	ctx = engine->CreateContext();
	if( !ctx ) {
		return NULL;
	}

	// We don't want to allow the script to hang the application, e.g. with an
	// infinite loop, so we'll use the line callback function to set a timeout
	// that will abort the script after a certain time. Before executing the
	// script the timeOut variable will be set to the time when the script must
	// stop executing.

	error = ctx->SetExceptionCallback( asFUNCTION( qasExceptionCallback ), NULL, asCALL_CDECL );
	if( error < 0 ) {
		ctx->Release();
		return NULL;
	}

	qasContextList &ctxList = contexts[engine];
	ctxList.push_back( ctx );

	return ctx;
}

void qasReleaseContext( asIScriptContext *ctx ) {
	if( !ctx ) {
		return;
	}

	asIScriptEngine *engine = ctx->GetEngine();
	qasContextList &ctxList = contexts[engine];
	ctxList.remove( ctx );

	ctx->Release();
}

asIScriptContext *qasAcquireContext( asIScriptEngine *engine ) {
	if( !engine ) {
		return NULL;
	}

	// try to reuse any context linked to this engine
	qasContextList &ctxList = contexts[engine];
	for( qasContextList::iterator it = ctxList.begin(); it != ctxList.end(); it++ ) {
		asIScriptContext *ctx = *it;
		if( ctx->GetState() == asEXECUTION_FINISHED ) {
			return ctx;
		}
	}

	// if no context was available, create a new one
	return qasCreateContext( engine );
}

asIScriptContext *qasGetActiveContext( void ) {
	return asGetActiveContext();
}

/*************************************
* Scripts
**************************************/

/*
* qasLoadScriptSection
*/
static char *qasLoadScriptSection( const char *rootDir, const char *dir, const char *script, int sectionNum ) {
	char filename[MAX_QPATH];
	uint8_t *data;
	int length, filenum;
	char *sectionName;

	sectionName = COM_ListNameForPosition( script, sectionNum, QAS_SECTIONS_SEPARATOR );
	if( !sectionName ) {
		return NULL;
	}

	COM_StripExtension( sectionName );

	while( *sectionName == '\n' || *sectionName == ' ' || *sectionName == '\r' )
		sectionName++;

	if( sectionName[0] == '/' ) {
		Q_snprintfz( filename, sizeof( filename ), "%s%s%s", rootDir, sectionName, QAS_FILE_EXTENSION );
	} else {
		Q_snprintfz( filename, sizeof( filename ), "%s/%s/%s%s", rootDir, dir, sectionName, QAS_FILE_EXTENSION );
	}
	Q_strlwr( filename );

	length = FS_FOpenFile( filename, &filenum, FS_READ );

	if( length == -1 ) {
		Com_Printf( "Couldn't find script section: '%s'\n", filename );
		return NULL;
	}

	//load the script data into memory
	data = ( uint8_t * )qasAlloc( length + 1 );
	FS_Read( data, length, filenum );
	FS_FCloseFile( filenum );

	return (char *)data;
}

/*
* qasBuildScriptProject
*/
static asIScriptModule *qasBuildScriptProject( asIScriptEngine *asEngine, const char *moduleName, const char *rootDir, const char *dir, const char *scriptName, const char *script ) {
	int error;
	int numSections, sectionNum;
	char *section;
	asIScriptModule *asModule;

	if( asEngine == NULL ) {
		Com_Printf( S_COLOR_RED "qasBuildGameScript: Angelscript API unavailable\n" );
		return NULL;
	}

	// count referenced script sections
	for( numSections = 0; ( section = COM_ListNameForPosition( script, numSections, QAS_SECTIONS_SEPARATOR ) ) != NULL; numSections++ ) ;

	if( !numSections ) {
		Com_Printf( S_COLOR_RED "* Error: script '%s' has no sections\n", scriptName );
		return NULL;
	}

	// load up the script sections

	asModule = asEngine->GetModule( moduleName, asGM_CREATE_IF_NOT_EXISTS );
	if( asModule == NULL ) {
		Com_Printf( S_COLOR_RED "qasBuildGameScript: GetModule '%s' failed\n", moduleName );
		return NULL;
	}

	for( sectionNum = 0; ( section = qasLoadScriptSection( rootDir, dir, script, sectionNum ) ) != NULL; sectionNum++ ) {
		const char *sectionName = COM_ListNameForPosition( script, sectionNum, QAS_SECTIONS_SEPARATOR );
		error = asModule->AddScriptSection( sectionName, section, strlen( section ) );

		qasFree( section );

		if( error ) {
			Com_Printf( S_COLOR_RED "* Failed to add the script section %s with error %i\n", sectionName, error );
			asEngine->DiscardModule( moduleName );
			return NULL;
		}
	}

	if( sectionNum != numSections ) {
		Com_Printf( S_COLOR_RED "* Error: couldn't load all script sections.\n" );
		asEngine->DiscardModule( moduleName );
		return NULL;
	}

	error = asModule->Build();
	if( error ) {
		Com_Printf( S_COLOR_RED "* Failed to build script '%s'\n", scriptName );
		asEngine->DiscardModule( moduleName );
		return NULL;
	}

	return asModule;
}

/*
* qasLoadScriptProject
*/
asIScriptModule *qasLoadScriptProject( asIScriptEngine *engine, const char *moduleName, const char *rootDir, const char *dir, const char *filename, const char *ext ) {
	int length, filenum;
	char *data;
	char filepath[MAX_QPATH];
	asIScriptModule *asModule;

	Q_snprintfz( filepath, sizeof( filepath ), "%s/%s/%s", rootDir, dir, filename );
	COM_DefaultExtension( filepath, ext, sizeof( filepath ) );

	length = FS_FOpenFile( filepath, &filenum, FS_READ );

	if( length == -1 ) {
		Com_Printf( "qasLoadScriptProject: Couldn't find '%s'.\n", filepath );
		return NULL;
	}

	if( !length ) {
		Com_Printf( "qasLoadScriptProject: '%s' is empty.\n", filepath );
		FS_FCloseFile( filenum );
		return NULL;
	}

	//load the script data into memory
	data = ( char * )qasAlloc( length + 1 );
	FS_Read( data, length, filenum );
	FS_FCloseFile( filenum );

	// Initialize the script
	asModule = qasBuildScriptProject( engine, moduleName, rootDir, dir, filepath, data );
	if( !asModule ) {
		qasFree( data );
		return NULL;
	}

	qasFree( data );
	return asModule;
}

/*************************************
* Array tools
**************************************/

CScriptArrayInterface *qasCreateArrayCpp( unsigned int length, void *ot ) {
	return QAS_NEW( CScriptArray )( length, static_cast<asIObjectType *>( ot ) );
}

void qasReleaseArrayCpp( CScriptArrayInterface *arr ) {
	arr->Release();
}

/*************************************
* Strings
**************************************/

asstring_t *qasStringFactoryBuffer( const char *buffer, unsigned int length ) {
	return objectString_FactoryBuffer( buffer, length );
}

void qasStringRelease( asstring_t *str ) {
	objectString_Release( str );
}

asstring_t *qasStringAssignString( asstring_t *self, const char *string, unsigned int strlen ) {
	return objectString_AssignString( self, string, strlen );
}

/*************************************
* Dictionary
**************************************/

CScriptDictionaryInterface *qasCreateDictionaryCpp( asIScriptEngine *engine ) {
	return QAS_NEW( CScriptDictionary )( engine );
}

void qasReleaseDictionaryCpp( CScriptDictionaryInterface *dict ) {
	dict->Release();
}

/*************************************
* Any
**************************************/

CScriptAnyInterface *qasCreateAnyCpp( asIScriptEngine *engine ) {
	return QAS_NEW( CScriptAny )( engine );
}

void qasReleaseAnyCpp( CScriptAnyInterface *any ) {
	any->Release();
}
