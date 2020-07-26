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
#include "addon/addon_vec4.h"
#include "addon/addon_cvar.h"
#include "addon/addon_stringutils.h"
#include "addon/addon_mat3.h"

#include <list>
#include <vector>
#include <map>
static void *qasAlloc( size_t size ) {
	return QAS_Malloc( size );
}

static void qasFree( void *mem ) {
	QAS_Free( mem );
}

struct qasNamespaceDump {
	std::string								  name;
	std::vector<asUINT>						  enums;
	std::vector<asUINT>						  classes;
	std::vector<asUINT>						  functions;
	std::vector<asUINT>						  properties;
	std::map<std::string, qasNamespaceDump *> children;

	qasNamespaceDump( std::string name ) : name( name ) {}

	enum class qasDumpType {
		qasEnum,
		qasClass,
		qasFunction,
		qasProperty,
	};

	void listAllChildren( std::vector<qasNamespaceDump *> &res ) const
	{
		for( auto it = children.begin(); it != children.end(); ++it ) {
			it->second->listAllChildren( res );
			res.push_back( it->second );
		}
	}

	void filterObject( std::string ns, asUINT id, qasDumpType dt ) {
		std::string subns, nextns;

		std::size_t delim = ns.find( "::" );
		if( delim != std::string::npos ) {
			subns = ns.substr( 0, delim );
			nextns = ns.substr( subns.length() + 2 );
		}
		if( subns.empty() && !ns.empty() ) {
			subns = ns;
			nextns = "";
		}

		if( !subns.empty() ) {
			qasNamespaceDump *child;

			auto it = children.find( subns );
			if( it == children.end() ) {
				child = QAS_NEW( qasNamespaceDump )( subns );
				children[subns] = child;
			} else {
				child = it->second;
			}

			child->filterObject( nextns, id, dt );
			return;
		}

		switch( dt ) {
			case qasDumpType::qasEnum:
				enums.push_back( id );
				break;
			case qasDumpType::qasClass:
				classes.push_back( id );
				break;
			case qasDumpType::qasFunction:
				functions.push_back( id );
				break;
			case qasDumpType::qasProperty:
				properties.push_back( id );
				break;
		}
	}

	void dump( asIScriptEngine *engine, int filenum, bool markdown )
	{
		asUINT		i, j;
		const char *str = 0; // for temporary strings

		if( !name.empty() ) {
			str = va( "\r\nnamespace %s\r\n{\r\n", name.c_str() );
			trap_FS_Write( str, strlen( str ), filenum );
		}

		for( auto it = children.begin(); it != children.end(); ++it ) {
			it->second->dump( engine, filenum, markdown );
		}

		//
		// global enums
		asUINT enumCount = enums.size();
		if( enumCount > 0 ) {
			if( markdown ) {
				str = "\r\n## Enums\r\n```c++\r\n";
			} else {
				str = "\r\n/**\r\n * Enums\r\n */\r\n";
			}
			trap_FS_Write( str, strlen( str ), filenum );

			for( i = 0; i < enumCount; i++ ) {
				int			enumTypeId;
				const char *ns;
				const char *enumName = engine->GetEnumByIndex( enums[i], &enumTypeId, &ns, NULL, NULL );

				str = va( "\r\nenum %s\r\n{\r\n", enumName );
				trap_FS_Write( str, strlen( str ), filenum );

				asUINT enumValueCount = engine->GetEnumValueCount( enumTypeId );
				for( j = 0; j < enumValueCount; j++ ) {
					int			outValue;
					const char *valueName = engine->GetEnumValueByIndex( enumTypeId, j, &outValue );
					str = va( "\t%s = 0x%x,\r\n", valueName, outValue );
					trap_FS_Write( str, strlen( str ), filenum );
				}

				str = "}\r\n";
				trap_FS_Write( str, strlen( str ), filenum );
			}

			if( markdown ) {
				str = "```\r\n";
				trap_FS_Write( str, strlen( str ), filenum );
			}
		}

		//
		// global properties
		asUINT propertyCount = properties.size();
		if( propertyCount > 0 ) {
			if( markdown ) {
				str = "\r\n## Global properties\r\n```c++\r\n";
			} else {
				str = "\r\n/**\r\n * Global properties\r\n */\r\n";
			}
			trap_FS_Write( str, strlen( str ), filenum );

			for( i = 0; i < propertyCount; i++ ) {
				const char *propertyName;
				const char *propertyNamespace;
				int			propertyTypeId;
				bool		propertyIsConst;
				asDWORD		mask;

				if( engine->GetGlobalPropertyByIndex( properties[i], &propertyName, &propertyNamespace, &propertyTypeId,
						&propertyIsConst, NULL, NULL, &mask ) >= 0 ) {
					const char *decl;
					const char *constAttr = propertyIsConst ? "const " : "";

					decl = va( "%s%s %s;\r\n", constAttr, engine->GetTypeDeclaration( propertyTypeId ), propertyName );

					trap_FS_Write( decl, strlen( decl ), filenum );
				}
			}

			if( markdown ) {
				str = "```\r\n";
				trap_FS_Write( str, strlen( str ), filenum );
			}
		}

		//
		// global functions

		asUINT functionCount = functions.size();
		if( functionCount > 0 ) {
			if( markdown ) {
				str = "\r\n## Global functions\r\n```c++\r\n";
			} else {
				str = "\r\n/**\r\n * Global functions\r\n */\r\n";
			}
			trap_FS_Write( str, strlen( str ), filenum );

			for( i = 0; i < functionCount; i++ ) {
				asIScriptFunction *func = engine->GetGlobalFunctionByIndex( functions[i] );
				if( func ) {
					const char *decl;
					decl = va( "%s {}\r\n", func->GetDeclaration( false, false, true ) );
					trap_FS_Write( decl, strlen( decl ), filenum );
				}
			}

			if( markdown ) {
				str = "```\r\n";
				trap_FS_Write( str, strlen( str ), filenum );
			}
		}

		// classes
		asUINT objectCount = classes.size();
		if( markdown ) {
			str = "\r\n## Classes\r\n";
		} else {
			str = "\r\n/**\r\n * Classes\r\n */\r\n";
		}
		if( objectCount > 0 ) {
			for( i = 0; i < objectCount; i++ ) {
				asIObjectType *objectType = engine->GetObjectTypeByIndex( classes[i] );
				if( !objectType ) {
					continue;
				}

				if( markdown ) {
					str = va( "### %s\r\n\r\n```c++\r\n", objectType->GetName() );
				} else {
					str = va( "/**\r\n * %s\r\n */\r\n", objectType->GetName() );
				}

				trap_FS_Write( str, strlen( str ), filenum );

				str = va( "class %s\r\n{", objectType->GetName() );
				trap_FS_Write( str, strlen( str ), filenum );

				asUINT memberCount = objectType->GetPropertyCount();
				if( memberCount > 0 ) {
					// properties
					str = "\r\n\t/* properties */\r\n";
					trap_FS_Write( str, strlen( str ), filenum );

					for( j = 0; j < memberCount; j++ ) {
						bool isPrivate;

						objectType->GetProperty( j, NULL, NULL, &isPrivate );
						if( isPrivate ) {
							continue;
						}

						const char *decl = va( "\t%s;\r\n", objectType->GetPropertyDeclaration( j ) );
						trap_FS_Write( decl, strlen( decl ), filenum );
					}
				}

				asUINT behaviourCount = objectType->GetBehaviourCount();
				if( behaviourCount > 0 ) {
					// behaviours
					str = "\r\n\t/* behaviors */\r\n";
					trap_FS_Write( str, strlen( str ), filenum );
					for( j = 0; j < behaviourCount; j++ ) {
						// ch : FIXME: obscure function names in behaviours
						asEBehaviours behaviourType;

						asIScriptFunction *function = objectType->GetBehaviourByIndex( j, &behaviourType );
						if( behaviourType == asBEHAVE_ADDREF || behaviourType == asBEHAVE_RELEASE ) {
							continue;
						}
						if( behaviourType >= asBEHAVE_FIRST_GC && behaviourType <= asBEHAVE_LAST_GC ) {
							continue;
						}
						if( behaviourType == asBEHAVE_TEMPLATE_CALLBACK ) {
							continue;
						}

						const char *decl = va( "\t%s {}\r\n", function->GetDeclaration( false, false, true ) );
						trap_FS_Write( decl, strlen( decl ), filenum );
					}
				}

				asUINT factoryCount = objectType->GetFactoryCount();
				if( factoryCount > 0 ) {
					// factories
					str = "\r\n\t/* factories */\r\n";
					trap_FS_Write( str, strlen( str ), filenum );
					for( j = 0; j < factoryCount; j++ ) {
						asIScriptFunction *function = objectType->GetFactoryByIndex( j );
						const char *	   decl = va( "\t%s {}\r\n", function->GetDeclaration( false, false, true ) );
						trap_FS_Write( decl, strlen( decl ), filenum );
					}
				}

				asUINT methodCount = objectType->GetMethodCount();
				if( methodCount > 0 ) {
					// methods
					str = "\r\n\t/* methods */\r\n";
					trap_FS_Write( str, strlen( str ), filenum );
					for( j = 0; j < methodCount; j++ ) {
						asIScriptFunction *method = objectType->GetMethodByIndex( j );
						const char *	   decl = va( "\t%s {}\r\n", method->GetDeclaration( false, false, true ) );
						trap_FS_Write( decl, strlen( decl ), filenum );
					}
				}

				str = "\r\n}\r\n\r\n";
				trap_FS_Write( str, strlen( str ), filenum );

				if( markdown ) {
					str = "```\r\n\r\n";
					trap_FS_Write( str, strlen( str ), filenum );
				}
			}
		}

		if( !name.empty() ) {
			str = "\r\n}\r\n\r\n";
			trap_FS_Write( str, strlen( str ), filenum );
		}
	}
};

// ============================================================================

// list of contexts in the same engine
typedef std::list<asIScriptContext *> qasContextList;

// engine -> contexts key/value pairs
typedef std::map<asIScriptEngine *, qasContextList> qasEngineContextMap;

qasEngineContextMap contexts;

// ============================================================================

static void qasMessageCallback( const asSMessageInfo *msg ) {
	const char *msg_type;
	int			severity = 0;

	switch( msg->type ) {
		case asMSGTYPE_ERROR:
			severity = 2;
			msg_type = S_COLOR_RED "ERROR: ";
			break;
		case asMSGTYPE_WARNING:
			severity = 1;
			msg_type = S_COLOR_YELLOW "WARNING: ";
			break;
		case asMSGTYPE_INFORMATION:
		default:
			severity = 0;
			msg_type = S_COLOR_CYAN "ANGELSCRIPT: ";
			break;
	}

	trap_Diag_Message( severity, msg->section, msg->row, msg->col, msg->message );

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
		QAS_Printf( "* angelscript library with AS_MAX_PORTABILITY detected\n" );
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
	PreRegisterMat3Addon( engine );

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
	RegisterMat3Addon( engine );

	engine->SetDefaultAccessMask( 0x1 );

	return engine;
}

void qasWriteEngineDocsToFile( asIScriptEngine *engine, const char *path, const char *contextName, 
	bool markdown, bool singleFile, unsigned andMask, unsigned notMask )
{
	asUINT		i;
	int			filenum;
	const char *str = 0;    // for temporary strings
	std::string singleFn;
	std::string spath( path );

	if( spath[spath.size() - 1] != '/' ) {
		spath += '/';
	}
	
	singleFn = std::string( contextName ) + ( markdown ? ".md" : ".as" );

	// global file
	std::string global_file( spath );
	global_file += singleFn;

	if( trap_FS_FOpenFile( global_file.c_str(), &filenum, FS_WRITE ) == -1 ) {
		Com_Printf( "ASModule::dumpAPI: Couldn't write %s.\n", global_file.c_str() );
		return;
	}

	// funcdefs
	asUINT funcdefCount = engine->GetFuncdefCount();
	if( funcdefCount > 0 ) {
		if( markdown ) {
			str = "\r\n## Funcdefs\r\n```c++\r\n\r\n";
		} else {
			str = "/**\r\n * Funcdefs\r\n */\r\n\r\n";
		}
		trap_FS_Write( str, strlen( str ), filenum );

		for( i = 0; i < funcdefCount; i++ ) {
			auto *f = engine->GetFuncdefByIndex( i );
			str = va( "funcdef %s;\r\n", f->GetDeclaration( false, false, true ) );
			trap_FS_Write( str, strlen( str ), filenum );
		}

		if( markdown ) {
			str = "```\r\n";
			trap_FS_Write( str, strlen( str ), filenum );
		}
	}

	qasNamespaceDump gns( "" );

	// enums
	asUINT enumCount = engine->GetEnumCount();
	if( enumCount > 0 ) {
		for( i = 0; i < enumCount; i++ ) {
			const char *ns;
			engine->GetEnumByIndex( i, NULL, &ns, NULL, NULL );
			gns.filterObject( ns ? ns : "", i, qasNamespaceDump::qasDumpType::qasEnum );
		}
	}

	// properties
	asUINT propertyCount = engine->GetGlobalPropertyCount();
	if( propertyCount > 0 ) {
		for( i = 0; i < propertyCount; i++ ) {
			const char *ns;
			asDWORD mask;

			if( engine->GetGlobalPropertyByIndex( i, NULL, &ns, NULL, NULL, NULL, NULL, &mask ) >= 0 ) {
				if( ( mask & andMask ) == 0 ) {
					continue;
				}
				if( ~( mask & notMask ) == 0 ) {
					continue;
				}
				gns.filterObject( ns ? ns : "", i, qasNamespaceDump::qasDumpType::qasProperty );
			}
		}
	}

	// functions
	asUINT functionCount = engine->GetGlobalFunctionCount();
	if( functionCount > 0 ) {
		for( i = 0; i < functionCount; i++ ) {
			asIScriptFunction *func = engine->GetGlobalFunctionByIndex( i );
			if( !func ) {
				continue;
			}

			asDWORD mask = func->GetAccessMask();
			if( ( mask & andMask ) == 0 ) {
				continue;
			}
			if( ~( mask & notMask ) == 0 ) {
				continue;
			}

			const char *ns = func->GetNamespace();
			gns.filterObject( ns ? ns : "", i, qasNamespaceDump::qasDumpType::qasFunction );
		}
	}

	// classes
	asUINT objectCount = engine->GetObjectTypeCount();
	for( i = 0; i < objectCount; i++ ) {
		asIObjectType *objectType = engine->GetObjectTypeByIndex( i );
		if( !objectType ) {
			continue;
		}
		if( ( objectType->GetFlags() & asOBJ_SCRIPT_OBJECT ) != 0 ) {
			continue;
		}

		asDWORD mask = objectType->GetAccessMask();
		if( ( mask & andMask ) == 0 ) {
			continue;
		}
		if( ~( mask & notMask ) == 0 ) {
			continue;
		}

		const char *ns = objectType->GetNamespace();
		gns.filterObject( ns ? ns : "", i, qasNamespaceDump::qasDumpType::qasClass );
	}

	gns.dump( engine, filenum, markdown );

	std::vector<qasNamespaceDump *> children;
	gns.listAllChildren( children );
	for( auto it = children.begin(); it != children.end(); ++it ) {
		QAS_DELETE( *it, qasNamespaceDump );
	}

	trap_FS_FCloseFile( filenum );

	Com_Printf( "Wrote %s\n", global_file.c_str() );
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
static bool qasLoadScriptSection(
	const char *rootDir, const char *dir, const char *script, int sectionNum, char **psectionName, char **pdata, time_t *mtime )
{
	char filename[MAX_QPATH];
	char  fullpath[4096];
	int length, filenum;
	char *sectionName;

	sectionName = COM_ListNameForPosition( script, sectionNum, QAS_SECTIONS_SEPARATOR );
	if( !sectionName ) {
		return false;
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

	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );

	if( length == -1 ) {
		QAS_Printf( "Couldn't find script section: '%s'\n", filename );
		return false;
	}

	*mtime = trap_FS_SysMTime( filenum );

	if( psectionName ) {
		fullpath[0] = '\0';

		// use full path name to the .as file as the section name
		// if the real file is not an AS script (most likely a pak file), then
		// revert to the relative path name
		if( trap_FS_FGetFullPathName( filenum, fullpath, sizeof( fullpath ) ) > 0 ) {
			if( strlen( fullpath ) <= strlen( QAS_FILE_EXTENSION )
				|| Q_stricmp( fullpath + strlen( fullpath ) - strlen( QAS_FILE_EXTENSION ), QAS_FILE_EXTENSION ) ) {
				fullpath[0] = '\0';
			}
		}

		if( fullpath[0] == '\0' ) {
			Q_strncpyz( fullpath, filename, sizeof( fullpath ) );
		}

		*psectionName = ( char * )QAS_Malloc( strlen( fullpath ) + 1 );
		memcpy( *psectionName, fullpath, strlen( fullpath ) + 1 );
	}

	if( pdata ) {
		// load the script data into memory
		char *data = (char *)qasAlloc( length + 1 );
		trap_FS_Read( data, length, filenum );
		*pdata = data;
		QAS_Printf( "* Loaded script section '%s'\n", filename );
	}

	trap_FS_FCloseFile( filenum );
	return true;
}

/*
* qasParseScriptProject
*/
static bool qasParseScriptProject( asIScriptModule *asModule, const char *rootDir, const char *dir,
	const char *filename, time_t *mtime, std::vector<std::string> &files )
{
	char			 filepath[MAX_QPATH];
	int				 length, filenum;
	char *			 script;
	int				 numSections, sNum;
	char *			 section, *fileName = NULL;
	char **			 psection, **pFileName;
	time_t			 smtime;

	Q_snprintfz( filepath, sizeof( filepath ), "%s/%s/%s", rootDir, dir, filename );

	length = trap_FS_FOpenFile( filepath, &filenum, FS_READ );
	if( length == -1 ) {
		QAS_Printf( "qasParseScriptProject: Couldn't find '%s'.\n", filepath );
		return NULL;
	}

	if( !length ) {
		QAS_Printf( "qasParseScriptProject: '%s' is empty.\n", filepath );
		trap_FS_FCloseFile( filenum );
		return NULL;
	}

	*mtime = trap_FS_SysMTime( filenum );

	// load the script data into memory
	script = (char *)qasAlloc( length + 1 );
	trap_FS_Read( script, length, filenum );
	trap_FS_FCloseFile( filenum );

	// count referenced script sections
	for( numSections = 0; ( section = COM_ListNameForPosition( script, numSections, QAS_SECTIONS_SEPARATOR ) ) != NULL; numSections++ ) ;

	if( !numSections ) {
		qasFree( script );
		QAS_Printf( S_COLOR_RED "* Error: script '%s' has no sections\n", filename );
		return false;
	}

	psection = asModule ? &section : NULL;
	pFileName = asModule ? &fileName : NULL;

	// load up the script sections
	for( sNum = 0; qasLoadScriptSection( rootDir, dir, script, sNum, pFileName, psection, &smtime ); sNum++ ) {
		if( smtime > *mtime ) {
			*mtime = smtime;
		}

		if( !asModule || !fileName ) {
			continue;
		}

		files.push_back( fileName );

		int error = asModule->AddScriptSection( fileName, section, strlen( section ) );

		qasFree( section );
		
		if( error ) {
			QAS_Printf( S_COLOR_RED "* Failed to add file %s with error %i\n", fileName, error );
			qasFree( fileName );
			qasFree( script );
			return false;
		}

		qasFree( fileName );
	}

	qasFree( script );

	if( sNum != numSections ) {
		QAS_Printf( S_COLOR_RED "* Error: couldn't load all script sections.\n" );
		return false;
	}

	return true;
}

/*
 * qasLoadScriptProject
 */
asIScriptModule *qasLoadScriptProject(
	asIScriptEngine *engine, const char *moduleName, const char *rootDir, const char *dir, const char *filename, time_t *mtime )
{
	asIScriptModule *asModule;
	time_t			 mtime_ = -1;
	std::vector<std::string> files;

	if( engine == NULL ) {
		QAS_Printf( S_COLOR_RED "qasBuildGameScript: Angelscript API unavailable\n" );
		return NULL;
	}

	// load up the script sections
	asModule = engine->GetModule( moduleName, asGM_CREATE_IF_NOT_EXISTS );
	if( asModule == NULL ) {
		QAS_Printf( S_COLOR_RED "qasLoadScriptProject: GetModule '%s' failed\n", moduleName );
		return NULL;
	}

	// Initialize the script
	if( !qasParseScriptProject( asModule, rootDir, dir, filename, &mtime_, files ) ) {
		engine->DiscardModule( moduleName );
		return NULL;
	}

	QAS_Printf( "* Initializing script '%s'\n", filename );

	char **diagnames = ( char ** )QAS_Malloc( sizeof( char * ) * (files.size() + 1) );
	for( int i = 0; i < files.size(); i++ ) {
		const char *fn = files[i].c_str();
		size_t		fn_size = strlen( fn ) + 1;

		diagnames[i] = (char *)QAS_Malloc( fn_size );
		memcpy( diagnames[i], fn, fn_size );
	}

	trap_Diag_Begin( ( const char **)diagnames );

	for( int i = 0; i < files.size(); i++ ) {
		QAS_Free( diagnames[i] );
	}
	QAS_Free( diagnames );

	int error = asModule->Build();

	trap_Diag_End();

	if( error ) {
		QAS_Printf( S_COLOR_RED "* Failed to build script '%s'\n", filename );
		engine->DiscardModule( moduleName );
		return NULL;
	}

	if( mtime ) {
		*mtime = mtime_;
	}

	return asModule;
}

/*
 * qasScriptProjectMTime
 */
time_t qasScriptProjectMTime( const char *rootDir, const char *dir, const char *filename )
{
	time_t mtime;
	std::vector<std::string> files;

	// Initialize the script
	if( !qasParseScriptProject( NULL, rootDir, dir, filename, &mtime, files ) ) {
		return -1;
	}

	return mtime;
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
