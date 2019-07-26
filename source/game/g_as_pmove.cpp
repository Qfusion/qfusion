/*
 Copyright (C) 2019 Victor Luchits
 
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

#include "g_local.h"
#include "g_as_local.h"

/*
 * G_ResetPMoveScriptData
 */
static void G_ResetPMoveScriptData( void ) {
	memset( &game.pmovescript, 0, sizeof( game.pmovescript ) );
}

/*
 * G_asInitializePMoveScript
 */
static bool G_asInitializePMoveScript( asIScriptModule *asModule ) {
	const char *fdeclstr;
	
	fdeclstr = "void PM::Init()";
	game.pmovescript.initFunc = asModule->GetFunctionByDecl( fdeclstr );
	
	fdeclstr = "void PM::PMove( PMove @ )";
	game.pmovescript.pmoveFunc = asModule->GetFunctionByDecl( fdeclstr );

	return true;
}

/*
 * G_asCallPMoveInitFunction
 */
static void G_asCallPMoveInitFunction( void ) {
	int error;
	asIScriptContext *ctx;
	
	if( !game.pmovescript.initFunc || !game.asExport ) {
		return;
	}
	
	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );
	
	error = ctx->Prepare( static_cast<asIScriptFunction *>( game.pmovescript.initFunc ) );
	if( error < 0 ) {
		return;
	}
	
	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownPMoveScript();
	}
}

/*
 * G_asCallPMovePMoveFunction
 */
void G_asCallPMovePMoveFunction( pmove_t *pmove ) {
	int error;
	asIScriptContext *ctx;
	
	if( !game.pmovescript.pmoveFunc || !game.asExport ) {
		return;
	}
	
	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );
	
	error = ctx->Prepare( static_cast<asIScriptFunction *>( game.pmovescript.pmoveFunc ) );
	if( error < 0 ) {
		return;
	}
	
	ctx->SetArgObject( 0, pmove );
	
	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownPMoveScript();
	}
}

/*
 * G_asLoadPMoveScript
 */
bool G_asLoadPMoveScript( void ) {
	const char *moduleName = PMOVE_SCRIPTS_MODULE_NAME;
	asIScriptModule *asModule;
	
	G_ResetPMoveScriptData();
	
	// Load the script
	asModule = G_LoadGameScript( moduleName, PMOVE_SCRIPTS_DIRECTORY, "pmove", PMOVE_SCRIPTS_PROJECT_EXTENSION );
	if( !asModule ) {
		return false;
	}
	
	// Initialize the script
	if( !G_asInitializePMoveScript( asModule ) ) {
		G_asShutdownPMoveScript();
		return false;
	}

	G_asCallPMoveInitFunction();

	return true;
}

/*
 * G_asShutdownPMoveScript
 */
void G_asShutdownPMoveScript( void ) {
	if( game.asEngine == NULL ) {
		return;
	}
	
	G_ResetPMoveScriptData();
	
	GAME_AS_ENGINE()->DiscardModule( PMOVE_SCRIPTS_MODULE_NAME );
}

