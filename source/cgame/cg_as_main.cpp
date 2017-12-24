/*
Copyright (C) 2017 Victor Luchits

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

#include "cg_local.h"
#include "cg_as_local.h"

std::function<void(asIScriptContext *)> cg_empty_as_cb = [](asIScriptContext *ctx) {};

//=======================================================================

static const gs_asEnumVal_t asLimitsEnumVals[] =
{
	ASLIB_ENUM_VAL( CG_MAX_TOUCHES ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnum_t asCGameEnums[] =
{
	{ "cg_limits_e", asLimitsEnumVals },

	ASLIB_ENUM_VAL_NULL
};

//======================================================================

static void asFunc_Print( const asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	CG_Printf( "%s", str->buffer );
}

static const gs_asglobfuncs_t asCGameGlobalFuncs[] =
{
	{ "void Print( const String &in )", asFUNCTION( asFunc_Print ), NULL },

	{ NULL }
};

//======================================================================

/*
* CG_asInitializeCGameEngineSyntax
*/
static void CG_asInitializeCGameEngineSyntax( asIScriptEngine *asEngine ) {
	CG_Printf( "* Initializing Game module syntax\n" );

	// register shared stuff
	GS_asInitializeEngine( asEngine );

	// register global enums
	GS_asRegisterEnums( asEngine, asCGameEnums, "CGame" );
	GS_asRegisterEnums( asEngine, asCGameInputEnums, "CGame" );

	// register global funcdefs
	GS_asRegisterFuncdefs( asEngine, asCGameCmdFuncdefs, "CGame::Cmd" );

	// first register all class names so methods using custom classes work
	GS_asRegisterObjectClassNames( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );

	// register classes
	GS_asRegisterObjectClasses( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );

	// register global functions
	GS_asRegisterGlobalFunctions( asEngine, asCGameGlobalFuncs, "CGame" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameCmdGlobalFuncs, "CGame::Cmd" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameInputGlobalFuncs, "CGame::Input" );

	// register global properties
}

/*
* CG_asInitScriptEngine
*/
void CG_asInitScriptEngine( void ) {
	bool asGeneric;
	asIScriptEngine *asEngine;

	// initialize the engine
	cgs.asEngine = NULL;
	cgs.asExport = trap_asGetAngelExport();
	if( !cgs.asExport ) {
		CG_Printf( "* Couldn't initialize angelscript, missing symbol.\n" );
		return;
	}

	asEngine = cgs.asExport->asCreateEngine( &asGeneric );
	if( !asEngine ) {
		CG_Printf( "* Couldn't initialize angelscript.\n" );
		return;
	}

	if( asGeneric ) {
		CG_Printf( "* Generic calling convention detected, aborting.\n" );
		CG_asShutdownScriptEngine();
		return;
	}

	cgs.asEngine = asEngine;

	CG_asInitializeCGameEngineSyntax( asEngine );
}

/*
* CG_asShutdownScriptEngine
*/
void CG_asShutdownScriptEngine( void ) {
	auto asEngine = CGAME_AS_ENGINE();
	if( asEngine == NULL ) {
		return;
	}

	CG_asUnloadGameScript();
	CG_asUnloadInputScript();

	cgs.asExport->asReleaseEngine( asEngine );
	cgs.asEngine = NULL;
}

/*
* CG_asExecutionErrorReport
*/
bool CG_asExecutionErrorReport( int error ) {
	return( error != asEXECUTION_FINISHED );
}

/*
* CG_asCallScriptFunc
*/
bool CG_asCallScriptFunc( void *ptr, std::function<void(asIScriptContext *)> setArgs,
	std::function<void(asIScriptContext *)> getResult ) {
	bool ok;

	if( !ptr ) {
		return false;
	}

	auto ctx = cgs.asExport->asAcquireContext( CGAME_AS_ENGINE() );

	auto error = ctx->Prepare( static_cast<asIScriptFunction *>( ptr ) );
	if( error < 0 ) {
		return false;
	}

	// Now we need to pass the parameters to the script function.
	setArgs( ctx );

	error = ctx->Execute();
	ok = CG_asExecutionErrorReport( error ) == false;

	assert( ok == true );

	if( ok ) {
		getResult( ctx );
	}

	return ok;
}

/*
* CG_asUnloadScriptModule
*/
void CG_asUnloadScriptModule( const char *moduleName, cg_asApiFuncPtr_t *api ) {
	auto asEngine = CGAME_AS_ENGINE();
	if( asEngine == NULL ) {
		return;
	}

	if( api ) {
		for( size_t i = 0; api[i].decl != nullptr; i++ ) {
			*api[i].ptr = NULL;
		}
	}

	CG_asReleaseModuleCommands( moduleName );

	asEngine->DiscardModule( moduleName );
}

/*
* CG_asLoadScriptModule
*/
asIScriptModule *CG_asLoadScriptModule( const char *moduleName, const char *filename, 
	cg_asApiFuncPtr_t *api ) {
	auto asEngine = CGAME_AS_ENGINE();
	if( asEngine == NULL ) {
		return NULL;
	}

	asEngine->DiscardModule( moduleName );

	auto asModule = cgs.asExport->asLoadScriptProject( asEngine, moduleName, 
		"progs", "client", filename, ".cp" );
	if( asModule == nullptr ) {
		return nullptr;
	}

	if( !api ) {
		return asModule;
	}

	for( size_t i = 0; api[i].decl != nullptr; i++ ) {
		auto decl = api[i].decl;
		auto ptr = asModule->GetFunctionByDecl( decl );

		if( !ptr && api[i].mandatory ) {
			CG_Printf( S_COLOR_RED "* The function '%s' was not found. Can not continue.\n", decl );
			goto error;
		}
		*api[i].ptr = ptr;
	}

	//
	// execute the optional 'load' function
	//
	if( *api[0].ptr != NULL ) {
		if( !CG_asCallScriptFunc( *api[0].ptr, cg_empty_as_cb, cg_empty_as_cb ) ) {
			goto error;
		}
	}

	return asModule;

error:
	CG_asUnloadScriptModule( moduleName, api );

	return nullptr;
}

//======================================================================

/*
* CG_asLoadGameScript
*/
bool CG_asLoadGameScript( void ) {
	auto asModule = CG_asLoadScriptModule( CG_SCRIPTS_GAME_MODULE_NAME, "cgame", NULL );
	if( asModule == nullptr ) {
		return false;
	}
	return true;
}

/*
* CG_asUnloadGameScript
*/
void CG_asUnloadGameScript( void ) {
	CG_asUnloadScriptModule( CG_SCRIPTS_GAME_MODULE_NAME, NULL );
}
