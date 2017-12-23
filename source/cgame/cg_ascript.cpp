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
#include "cg_local.h"
#include "angelscript.h"
#include "../gameshared/q_angeliface.h"
#include "../gameshared/gs_ascript.h"

#define CG_SCRIPTS_GAME_MODULE_NAME "cgame"
#define CG_SCRIPTS_INPUT_MODULE_NAME "input"

#define CGAME_AS_ENGINE() static_cast<asIScriptEngine *>( cgs.asEngine )

static std::function<void(asIScriptContext *)> empty_as_cb = [](asIScriptContext *ctx) {};

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
	{ ASLIB_PROPERTY_DECL(const  bool, areaValid ), ASLIB_FOFFSET( cg_touch_t, area_valid ) },

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

static const gs_asClassDescriptor_t * const asCGameInputClassesDescriptors[] =
{
	&asTouchClassDescriptor,

	NULL
};

static const gs_asglobfuncs_t asCGameInputGlobalFuncs[] =
{
	{ "Touch @GetTouch( int id )", asFUNCTION( CG_GetTouch ), NULL },

	{ NULL }
};

//======================================================================

static void asFunc_Print( const asstring_t *str ) {
	if( !str || !str->buffer ) {
		return;
	}

	CG_Printf( "%s", str->buffer );
}

//=======================================================================

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

	// first register all class names so methods using custom classes work
	GS_asRegisterObjectClassNames( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );

	// register classes
	GS_asRegisterObjectClasses( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );

	// register global functions
	GS_asRegisterGlobalFunctions( asEngine, asCGameGlobalFuncs, "CGame" );
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
	"void CGame::Input::Load()", &cgs.asInput.init, false,
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
