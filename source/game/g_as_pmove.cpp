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
	
	fdeclstr = "void PM::Load()";
	game.pmovescript.loadFunc = asModule->GetFunctionByDecl( fdeclstr );
	
	fdeclstr = "void PM::PMove( PMove @, PlayerState @ps, UserCmd cmd )";
	game.pmovescript.pmoveFunc = asModule->GetFunctionByDecl( fdeclstr );

	fdeclstr = "Vec3 PM::GetViewAnglesClamp( const PlayerState @ps )";
	game.pmovescript.vaClampFunc = asModule->GetFunctionByDecl( fdeclstr );

	return true;
}

/*
 * G_asCallPMoveLoad
 */
static void G_asCallPMoveLoad( void ) {
	int error;
	asIScriptContext *ctx;
	
	if( !game.pmovescript.loadFunc || !game.asExport ) {
		return;
	}
	
	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );
	
	error = ctx->Prepare( static_cast<asIScriptFunction *>( game.pmovescript.loadFunc ) );
	if( error < 0 ) {
		return;
	}
	
	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownPMoveScript();
	}
}

/*
 * G_asCallPMovePMove
 */
void G_asCallPMovePMove( pmove_t *pmove, player_state_t *ps, usercmd_t *cmd ) {
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
	ctx->SetArgObject( 1, ps );
	ctx->SetArgObject( 2, cmd );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownPMoveScript();
	}
}

/*
 * G_asCallPMoveGetViewAnglesClamp
 */
void G_asCallPMoveGetViewAnglesClamp( const player_state_t *ps, vec3_t vaclamp ) {
	int error;
	asIScriptContext *ctx;

	if( !game.pmovescript.vaClampFunc || !game.asExport ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );

	error = ctx->Prepare( static_cast<asIScriptFunction *>( game.pmovescript.vaClampFunc ) );
	if( error < 0 ) {
		return;
	}

	ctx->SetArgObject( 0, const_cast<player_state_t *>(ps) );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownPMoveScript();
		return;
	}

	const asvec3_t *va = ( const asvec3_t * )ctx->GetReturnAddress();
	VectorCopy( va->v, vaclamp );
}

/*
 * G_asLoadPMoveScript
 */
bool G_asLoadPMoveScript( void ) {
	const char *moduleName = PMOVE_SCRIPTS_MODULE_NAME;
	asIScriptModule *asModule;

	G_ResetPMoveScriptData();

	// Load the script
	asModule = G_LoadGameScript( moduleName, PMOVE_SCRIPTS_DIRECTORY, "pmove" PMOVE_SCRIPTS_PROJECT_EXTENSION );
	if( !asModule ) {
		return false;
	}

	// Initialize the script
	if( !G_asInitializePMoveScript( asModule ) ) {
		G_asShutdownPMoveScript();
		return false;
	}

	G_asCallPMoveLoad();

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

