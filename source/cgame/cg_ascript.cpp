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

#include <functional>
#include <map>
#include <string>
#include "cg_local.h"
#include "angelscript.h"
#include "../gameshared/q_angeliface.h"
#include "../gameshared/gs_ascript.h"

#define CG_SCRIPTS_GAME_MODULE_NAME "cgame"
#define CG_SCRIPTS_INPUT_MODULE_NAME "input"

#define CGAME_AS_ENGINE() static_cast<asIScriptEngine *>( cgs.asEngine )

typedef std::map<std::string, asIScriptFunction *> scriptCommandMap_t;
typedef std::map<std::string, scriptCommandMap_t> moduleCommands_t;

static std::function<void(asIScriptContext *)> empty_as_cb = [](asIScriptContext *ctx) {};
static moduleCommands_t moduleConsoleCmds;

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

static const gs_asEnumVal_t asTouchareaEnumVals[] =
{
	ASLIB_ENUM_VAL( TOUCHAREA_NONE ),
	ASLIB_ENUM_VAL( TOUCHAREA_HUD ),
	ASLIB_ENUM_VAL( TOUCHAREA_SUB_SHIFT ),
	ASLIB_ENUM_VAL( TOUCHAREA_MASK ),

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
	{ "cg_toucharea_e", asTouchareaEnumVals },
	{ "cg_limits_e", asLimitsEnumVals },

	ASLIB_ENUM_VAL_NULL
};

//=======================================================================

static const gs_asFuncdef_t astouch_Funcdefs[] =
{
	ASLIB_FUNCDEF_NULL
};

static const gs_asBehavior_t astouch_ObjectBehaviors[] =
{
	ASLIB_BEHAVIOR_NULL
};

static const gs_asMethod_t astouch_Methods[] =
{
	ASLIB_METHOD_NULL
};

static const gs_asProperty_t astouch_Properties[] =
{
	{ ASLIB_PROPERTY_DECL( const bool, down ), ASLIB_FOFFSET( cg_touch_t, down ) },
	{ ASLIB_PROPERTY_DECL( const int, x ), ASLIB_FOFFSET( cg_touch_t, x ) },
	{ ASLIB_PROPERTY_DECL( const int, y ), ASLIB_FOFFSET( cg_touch_t, y ) },
	{ ASLIB_PROPERTY_DECL( const int64, time ), ASLIB_FOFFSET( cg_touch_t, time ) },
	{ ASLIB_PROPERTY_DECL( const int, area ), ASLIB_FOFFSET( cg_touch_t, area ) },
	{ ASLIB_PROPERTY_DECL( const  bool, areaValid ), ASLIB_FOFFSET( cg_touch_t, area_valid ) },

	ASLIB_PROPERTY_NULL
};

static const gs_asClassDescriptor_t asTouchClassDescriptor =
{
	"Touch",                    /* name */
	asOBJ_REF | asOBJ_NOCOUNT,  /* object type flags */
	sizeof( cg_touch_t ),       /* size */
	astouch_Funcdefs,           /* funcdefs */
	astouch_ObjectBehaviors,    /* object behaviors */
	astouch_Methods,            /* methods */
	astouch_Properties,         /* properties */
	NULL, NULL                  /* string factory hack */
};

//=======================================================================

static asvec4_t CG_asInputGetThumbsticks( void ) {
	asvec4_t sticks;
	trap_IN_GetThumbsticks( sticks.v );
	return sticks;
}

static const gs_asClassDescriptor_t * const asCGameInputClassesDescriptors[] =
{
	&asTouchClassDescriptor,

	NULL
};

static const gs_asglobfuncs_t asCGameInputGlobalFuncs[] =
{
	{ "Touch @GetTouch( int id )", asFUNCTION( CG_GetTouch ), NULL },
	{ "Vec4 GetThumbsticks()", asFUNCTION( CG_asInputGetThumbsticks ), NULL },

	{ NULL }
};

//======================================================================

static void asFunc_RemoveCommand( const asstring_t *cmd );
static void asFunc_AddCommand( const asstring_t *cmd, asIScriptFunction *f );

static void asFunc_Print( const asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	CG_Printf( "%s", str->buffer );
}

static void asFunc_CmdProxyFunc( void ) {
	std::string cmdName = trap_Cmd_Argv( 0 );

	for( moduleCommands_t::const_iterator it = moduleConsoleCmds.begin(); it != moduleConsoleCmds.end(); ++it ) {
		auto &cmds = it->second;
		auto fit = cmds.find( cmdName );
		if( fit != cmds.end() ) {
			auto f = fit->second;
			auto ctx = cgs.asExport->asAcquireContext( CGAME_AS_ENGINE() );
			auto error = ctx->Prepare( f );
			if( error < 0 ) {
				return;
			}
			error = ctx->Execute();
		}
	}

	return;
}

static void asFunc_RemoveCommand( const asstring_t *cmd ) {
	asIScriptContext *ctx = cgs.asExport->asGetActiveContext();
	std::string moduleName = ctx->GetFunction()->GetModuleName();
	std::string cmdName = cmd->buffer;

	auto &cmds = moduleConsoleCmds[moduleName];
	auto fit = cmds.find( cmdName );
	if( fit != cmds.end() ) {
		fit->second->Release();
		cmds.erase( fit );
		trap_Cmd_RemoveCommand( cmd->buffer );
		return;
	}

	CG_Printf( S_COLOR_YELLOW "RemoveCommand: cmd '%s' doesn't exist in module '%s'\n", cmd->buffer, moduleName.c_str() );
}

static void asFunc_AddCommand( const asstring_t *cmd, asIScriptFunction *f ) {
	std::string moduleName = f->GetModuleName();
	std::string cmdName = cmd->buffer;

	auto &cmds = moduleConsoleCmds[moduleName];
	auto oldf = cmds.find( cmdName );
	if( oldf != cmds.end() ) {
		oldf->second->Release();
	}

	f->AddRef();
	cmds[cmdName] = f;

	trap_Cmd_AddCommand( cmd->buffer, &asFunc_CmdProxyFunc );
}

static asstring_t *asFunc_CmdArgv( int index ) {
	const char *buf = trap_Cmd_Argv( index );
	asstring_t *data = cgs.asExport->asStringFactoryBuffer( buf, strlen( buf ) );
	return data;
}

static asstring_t *asFunc_CmdArgs( void ) {
	const char *buf = trap_Cmd_Args();
	asstring_t *data = cgs.asExport->asStringFactoryBuffer( buf, strlen( buf ) );
	return data;
}

static const gs_asFuncdef_t asCGameCmdFuncdefs[] =
{
	{ "void CmdFunction()" },

	ASLIB_FUNCDEF_NULL
};

static const gs_asglobfuncs_t asCGameCmdGlobalFuncs[] =
{
	{ "void AddCommand( const String &in, CmdFunction @f )", asFUNCTION( asFunc_AddCommand ), NULL },
	{ "void RemoveCommand( const String &in )", asFUNCTION( asFunc_RemoveCommand ), NULL },
	{ "uint Argc()", asFUNCTION( trap_Cmd_Argc ), NULL },
	{ "const String @Argv( uint index )", asFUNCTION( asFunc_CmdArgv ), NULL },
	{ "const String @Args()", asFUNCTION( asFunc_CmdArgs ), NULL },

	{ NULL }
};

//======================================================================

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
* CG_asCallScriptFunc
*/
static bool CG_asCallScriptFunc( void *ptr, std::function<void(asIScriptContext *)> setArgs,
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
static void CG_asUnloadScriptModule( const char *moduleName, cg_asApiFuncPtr_t *api ) {
	auto asEngine = CGAME_AS_ENGINE();
	if( asEngine == NULL ) {
		return;
	}

	for( size_t i = 0; api[i].decl != nullptr; i++ ) {
		*api[i].ptr = NULL;
	}

	auto &cmds = moduleConsoleCmds[moduleName];
	for( scriptCommandMap_t::const_iterator fit = cmds.begin(); fit != cmds.end(); ++fit ) {
		fit->second->Release();
	}
	cmds.clear();

	asEngine->DiscardModule( moduleName );
}

/*
* CG_asLoadScriptModule
*/
static asIScriptModule *CG_asLoadScriptModule( const char *moduleName, const char *filename, cg_asApiFuncPtr_t *api ) {
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

		if( !ptr ) {
			CG_Printf( S_COLOR_RED "* The function '%s' was not found. Can not continue.\n", decl );
			goto error;
		}
		*api[i].ptr = ptr;
	}

	//
	// execute the optional 'load' function
	//
	if( *api[0].ptr != NULL ) {
		if( !CG_asCallScriptFunc( *api[0].ptr, empty_as_cb, empty_as_cb ) ) {
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

//======================================================================

static cg_asApiFuncPtr_t cg_asInputAPI[] = {
	"void CGame::Input::Load()", &cgs.asInput.load, false,
	"void CGame::Input::Init()", &cgs.asInput.init, true,
	"void CGame::Input::Shutdown()", &cgs.asInput.shutdown, true,
	"void CGame::Input::Frame( int frameTime )", &cgs.asInput.frame, true,
	"void CGame::Input::ClearState()", &cgs.asInput.clearState, true,
	"void CGame::Input::MouseMove( int mx, int my )", &cgs.asInput.mouseMove, true,
	"uint CGame::Input::GetButtonBits()", &cgs.asInput.getButtonBits, true,
	"Vec3 CGame::Input::AddViewAngles( const Vec3 angles )", &cgs.asInput.addViewAngles, true,
	"Vec3 CGame::Input::AddMovement( const Vec3 move )", &cgs.asInput.addMovement, true,
	nullptr, nullptr, false,
};

/*
* CG_asLoadInputScript
*/
bool CG_asLoadInputScript( void ) {
	auto asModule = CG_asLoadScriptModule( CG_SCRIPTS_INPUT_MODULE_NAME, "input", cg_asInputAPI );
	if( asModule == nullptr ) {
		return false;
	}
	return true;
}

/*
* CG_asUnloadInputScript
*/
void CG_asUnloadInputScript( void ) {
	CG_asUnloadScriptModule( CG_SCRIPTS_INPUT_MODULE_NAME, cg_asInputAPI );
}

/*
* CG_asInputInit
*/
void CG_asInputInit( void ) {
	CG_asCallScriptFunc( cgs.asInput.init, empty_as_cb, empty_as_cb );
}

/*
* CG_asInputShutdown
*/
void CG_asInputShutdown( void ) {
	CG_asCallScriptFunc( cgs.asInput.shutdown, empty_as_cb, empty_as_cb );
}

/*
* CG_asInputFrame
*/
void CG_asInputFrame( int frameTime ) {
	CG_asCallScriptFunc( cgs.asInput.frame, [frameTime](asIScriptContext *ctx)
		{
			ctx->SetArgDWord( 0, frameTime );
		},
		empty_as_cb
	);
}

/*
* CG_asInputClearState
*/
void CG_asInputClearState( void ) {
	CG_asCallScriptFunc( cgs.asInput.clearState, empty_as_cb, empty_as_cb );
}

/*
* CG_asInputMouseMove
*/
void CG_asInputMouseMove( int mx, int my ) {
	CG_asCallScriptFunc( cgs.asInput.mouseMove, [mx, my](asIScriptContext *ctx)
	{
		ctx->SetArgDWord( 0, mx );
		ctx->SetArgDWord( 1, my );
	},
		empty_as_cb
	);
}

/*
* CG_asGetButtonBits
*/
unsigned CG_asGetButtonBits( void ) {
	unsigned res = 0;

	CG_asCallScriptFunc( cgs.asInput.getButtonBits, empty_as_cb,
		[&res](asIScriptContext *ctx)
		{
			res = ctx->GetReturnDWord();
		}
	);

	return res;
}

/*
* CG_asAddViewAngles
*/
void CG_asAddViewAngles( vec3_t viewAngles ) {
	CG_asCallScriptFunc( cgs.asInput.addViewAngles,
		[viewAngles](asIScriptContext *ctx)
		{
			asvec3_t va;
			VectorCopy( viewAngles, va.v );
			ctx->SetArgObject( 0, &va );
		},
		[viewAngles](asIScriptContext *ctx)
		{
			const asvec3_t *va = ( const asvec3_t * )ctx->GetReturnAddress();
			VectorCopy( va->v, viewAngles );
		}
	);
}

/*
* CG_asAddMovement
*/
void CG_asAddMovement( vec3_t movement ) {
	CG_asCallScriptFunc( cgs.asInput.addViewAngles,
		[movement](asIScriptContext *ctx)
		{
			asvec3_t mv;
			VectorCopy( movement, mv.v );
			ctx->SetArgObject( 0, &mv );
		},
		[movement](asIScriptContext *ctx)
		{
			const asvec3_t *mv = ( const asvec3_t * )ctx->GetReturnAddress();
			VectorCopy( mv->v, movement );
		}
	);
}
