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
* G_ResetMapScriptData
*/
static void G_ResetMapScriptData( void ) {
	memset( &level.mapscript, 0, sizeof( level.mapscript ) );
}

/*
* G_asInitializeMapScript
*/
static bool G_asInitializeMapScript( asIScriptModule *asModule ) {
	const char *fdeclstr;

	fdeclstr = "void MAP_Init()";
	level.mapscript.initFunc = asModule->GetFunctionByDecl( fdeclstr );
	if( !level.mapscript.initFunc ) {
		if( developer->integer || sv_cheats->integer ) {
			G_Printf( "* The function '%s' was not present in the map script.\n", fdeclstr );
		}
	}

	fdeclstr = "void MAP_PreThink()";
	level.mapscript.preThinkFunc = asModule->GetFunctionByDecl( fdeclstr );
	if( !level.mapscript.preThinkFunc ) {
		if( developer->integer || sv_cheats->integer ) {
			G_Printf( "* The function '%s' was not present in the map script.\n", fdeclstr );
		}
	}

	fdeclstr = "void MAP_PostThink()";
	level.mapscript.postThinkFunc = asModule->GetFunctionByDecl( fdeclstr );
	if( !level.mapscript.postThinkFunc ) {
		if( developer->integer || sv_cheats->integer ) {
			G_Printf( "* The function '%s' was not present in the map script.\n", fdeclstr );
		}
	}

	fdeclstr = "void MAP_Exit()";
	level.mapscript.exitFunc = asModule->GetFunctionByDecl( fdeclstr );
	if( !level.mapscript.exitFunc ) {
		if( developer->integer || sv_cheats->integer ) {
			G_Printf( "* The function '%s' was not present in the map script.\n", fdeclstr );
		}
	}

	fdeclstr = "const String @MAP_Gametype( const String &gt )";
	level.mapscript.gametypeFunc = asModule->GetFunctionByDecl( fdeclstr );
	if( !level.mapscript.gametypeFunc ) {
		if( developer->integer || sv_cheats->integer ) {
			G_Printf( "* The function '%s' was not present in the map script.\n", fdeclstr );
		}
	}

	return true;
}

/*
* G_asCallMapFunction
*/
static void G_asCallMapFunction( void *func ) {
	int error;
	asIScriptContext *ctx;

	if( !func || !game.asExport ) {
		return;
	}

	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );

	error = ctx->Prepare( static_cast<asIScriptFunction *>( func ) );
	if( error < 0 ) {
		return;
	}

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		G_asShutdownMapScript();
	}
}

/*
* G_asCallMapInit
*/
void G_asCallMapInit( void ) {
	G_asCallMapFunction( level.mapscript.initFunc );
}

/*
* G_asCallMapPreThink
*/
void G_asCallMapPreThink( void ) {
	G_asCallMapFunction( level.mapscript.preThinkFunc );
}

/*
* G_asCallMapPostThink
*/
void G_asCallMapPostThink( void ) {
	G_asCallMapFunction( level.mapscript.postThinkFunc );
}

/*
* G_asCallMapExit
*/
void G_asCallMapExit( void ) {
	G_asCallMapFunction( level.mapscript.exitFunc );
}

/*
* G_asCallMapGametype
*/
const char *G_asCallMapGametype( void ) {
	asstring_t *string;
	int error;
	asIScriptContext *ctx;
	static char gametypeName[MAX_CONFIGSTRING_CHARS];
	asstring_t *s;

	if( !level.mapscript.gametypeFunc ) {
		return "";
	}

	ctx = game.asExport->asAcquireContext( GAME_AS_ENGINE() );

	error = ctx->Prepare( static_cast<asIScriptFunction *>( level.mapscript.gametypeFunc ) );
	if( error < 0 ) {
		return "";
	}

	s = game.asExport->asStringFactoryBuffer( g_gametype->string, strlen( g_gametype->string ) );

	ctx->SetArgObject( 0, s );

	error = ctx->Execute();
	if( G_ExecutionErrorReport( error ) ) {
		GT_asShutdownScript();
	}

	string = ( asstring_t * )ctx->GetReturnObject();
	if( !string || !string->len || !string->buffer ) {
		return "";
	}

	Q_strncpyz( gametypeName, string->buffer, sizeof( gametypeName ) );

	return gametypeName;
}

/*
* G_asLoadMapScript
*/
bool G_asLoadMapScript( const char *mapName ) {
	const char *moduleName = MAP_SCRIPTS_MODULE_NAME;
	asIScriptModule *asModule;

	G_ResetMapScriptData();

	// Load the script
	asModule = G_LoadGameScript( moduleName, MAP_SCRIPTS_DIRECTORY, va( "%s%s", mapName, MAP_SCRIPTS_PROJECT_EXTENSION ) );
	if( !asModule ) {
		return false;
	}

	// Initialize the script
	if( !G_asInitializeMapScript( asModule ) ) {
		G_asShutdownMapScript();
		return false;
	}

	return true;
}

/*
* G_asShutdownMapScript
*/
void G_asShutdownMapScript( void ) {
	int i;
	edict_t *e;

	if( game.asEngine == NULL ) {
		return;
	}

	// release the callback and any other objects obtained from the script engine before releasing the engine
	for( i = 0; i < game.numentities; i++ ) {
		e = &game.edicts[i];

		if( e->scriptSpawned && e->asScriptModule &&
			!strcmp( ( static_cast<asIScriptModule*>( e->asScriptModule ) )->GetName(), MAP_SCRIPTS_MODULE_NAME ) ) {
			G_asReleaseEntityBehaviors( e );
			e->asScriptModule = NULL;
		}
	}

	G_ResetMapScriptData();

	GAME_AS_ENGINE()->DiscardModule( MAP_SCRIPTS_MODULE_NAME );
}
