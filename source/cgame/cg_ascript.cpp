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
#include "angelscript.h"
#include "../gameshared/q_angeliface.h"
#include "../gameshared/gs_ascript.h"

#define CG_SCRIPTS_GAME_MODULE_NAME "cgame"
#define CG_SCRIPTS_INPUT_MODULE_NAME "input"

#define CGAME_AS_ENGINE() static_cast<asIScriptEngine *>( cgs.asEngine )

//=======================================================================

typedef struct {
	const char * const decl;
	void **ptr;
	bool mandatory;
} cg_asApiFuncPtr_t;

static const gs_asEnumVal_t asTouchpadEnumVals[] =
{
	ASLIB_ENUM_VAL( TOUCHPAD_MOVE ),
	ASLIB_ENUM_VAL( TOUCHPAD_VIEW ),
	ASLIB_ENUM_VAL( TOUCHPAD_COUNT ),

	ASLIB_ENUM_VAL_NULL
};

static const gs_asEnumVal_t asLimitsEnumVals[] =
{
	ASLIB_ENUM_VAL( CG_MAX_TOUCHES ),

	ASLIB_ENUM_VAL_NULL
};

//=======================================================================

static const gs_asEnum_t asCGameEnums[] =
{
	{ "cg_touchpad_e", asTouchpadEnumVals },
	{ "cg_limits_e", asLimitsEnumVals },

	ASLIB_ENUM_VAL_NULL
};

/*
* CG_asInitializeCGameEngineSyntax
*/
static void CG_asInitializeCGameEngineSyntax( asIScriptEngine *asEngine ) {
	CG_Printf( "* Initializing Game module syntax\n" );

	// register shared stuff
	GS_asInitializeEngine( asEngine );

	// register global enums
	GS_asRegisterEnums( asEngine, asCGameEnums );

	// first register all class names so methods using custom classes work

	// register classes

	// register global functions

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
	auto asEngine = static_cast<asIScriptEngine *>( cgs.asEngine );
	if( asEngine == NULL ) {
		return;
	}

	asEngine->DiscardModule( CG_SCRIPTS_GAME_MODULE_NAME );
	asEngine->DiscardModule( CG_SCRIPTS_INPUT_MODULE_NAME );

	cgs.asExport->asReleaseEngine( asEngine );
	cgs.asEngine = NULL;
}

/*
* CG_asExecutionErrorReport
*/
static bool CG_asExecutionErrorReport( int error ) {
	return( error != asEXECUTION_FINISHED );
}

/*
* CG_asLoadScriptModule
*/
static asIScriptModule *CG_asLoadScriptModule( const char *moduleName, const char *filename, cg_asApiFuncPtr_t *api ) {
	auto asEngine = static_cast<asIScriptEngine *>( cgs.asEngine );
	if( asEngine == NULL ) {
		return NULL;
	}

	asEngine->DiscardModule( moduleName );

	auto asModule = cgs.asExport->asLoadScriptProject( CGAME_AS_ENGINE(), moduleName, 
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

		if( !ptr ) {
			CG_Printf( S_COLOR_RED "* The function '%s' was not found. Can not continue.\n", decl );
			goto error;
		}
		*api[i].ptr = ptr;
	}

	//
	// execute the init function
	//
	auto ctx = cgs.asExport->asAcquireContext( CGAME_AS_ENGINE() );

	auto error = ctx->Prepare( static_cast<asIScriptFunction *>( *api[0].ptr ) );
	if( error < 0 ) {
		goto error;
	}

	error = ctx->Execute();
	if( CG_asExecutionErrorReport( error ) ) {
		goto error;
	}

	return asModule;

error:
	for( size_t i = 0; api[i].decl != nullptr; i++ ) {
		*api[i].ptr = NULL;
	}
	asEngine->DiscardModule( moduleName );

	return nullptr;
}

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
* CG_asLoadInputScript
*/
bool CG_asLoadInputScript( void ) {
	cg_asApiFuncPtr_t api[] = {
		"void IN_Init()", &cgs.asInput.init, true,
		"void IN_Shutdown()", &cgs.asInput.shutdown, true,
		"void IN_Frame( int frameTime )", &cgs.asInput.frame, true,
		"void IN_ClearState()", &cgs.asInput.clearState, true,
		"void IN_MouseMove( int mx, int my )", &cgs.asInput.mouseMove, true,
		"uint IN_GetButtonBits()", &cgs.asInput.getButtonBits, true,
		"void IN_AddViewAngles( const Vec3 &in angles )", &cgs.asInput.addViewAngles, true,
		"void IN_AddMovement( const Vec3 &in move )", &cgs.asInput.addMovement, true,
		nullptr, nullptr, false,
	};

	auto asModule = CG_asLoadScriptModule( CG_SCRIPTS_INPUT_MODULE_NAME, "input", api );
	if( asModule == nullptr ) {
		return false;
	}

	return true;
}
