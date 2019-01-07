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

#include "cg_as_local.h"

std::function<void(asIScriptContext *)> cg_empty_as_cb = [](asIScriptContext *ctx) {};

static cg_asApiFuncPtr_t cg_asCGameAPI[] = {
	{ "void CGame::Load()", &cgs.asMain.load, false },

	{ "void CGame::Input::Init()", &cgs.asInput.init, true },
	{ "void CGame::Input::Shutdown()", &cgs.asInput.shutdown, true },
	{ "void CGame::Input::Frame( int64 curTime, int frameTime )", &cgs.asInput.frame, true },
	{ "void CGame::Input::ClearState()", &cgs.asInput.clearState, true },
	{ "bool CGame::Input::KeyEvent( int key, bool down )", &cgs.asInput.keyEvent, false },
	{ "void CGame::Input::MouseMove( int mx, int my )", &cgs.asInput.mouseMove, true },
	{ "uint CGame::Input::GetButtonBits()", &cgs.asInput.getButtonBits, true },
	{ "Vec3 CGame::Input::GetAngularMovement()", &cgs.asInput.getAngularMovement, true },
	{ "Vec3 CGame::Input::GetMovement()", &cgs.asInput.getMovement, true },

	{ "void CGame::Camera::SetupCamera( CGame::Camera::Camera @cam )", &cgs.asCamera.setupCamera, true },
	{ "void CGame::Camera::SetupRefdef( const CGame::Camera::Camera @cam, CGame::Camera::Refdef @rd )", &cgs.asCamera.setupRefdef, true },

	{ nullptr, nullptr, false },
};

//=======================================================================

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
	GS_asRegisterEnums( asEngine, asCGameCameraEnums, "CGame" );

	// register global funcdefs
	GS_asRegisterFuncdefs( asEngine, asCGameCmdFuncdefs, "CGame::Cmd" );

	// first register all class names so methods using custom classes work
	GS_asRegisterObjectClassNames( asEngine, asCGameCameraClassesDescriptors, "CGame::Camera" );

	// register classes
	GS_asRegisterObjectClasses( asEngine, asCGameCameraClassesDescriptors, "CGame::Camera" );

	// register global functions
	GS_asRegisterGlobalFunctions( asEngine, asCGameGlobalFuncs, "CGame" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameCmdGlobalFuncs, "CGame::Cmd" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameInputGlobalFuncs, "CGame::Input" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameCameraGlobalFuncs, "CGame::Camera" );

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
	auto asModule = CG_asLoadScriptModule( CG_SCRIPTS_GAME_MODULE_NAME, "cgame", cg_asCGameAPI );
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
