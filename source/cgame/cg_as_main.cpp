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
#include <string>
#include <vector>

std::function<void( asIScriptContext * )> cg_empty_as_cb = []( asIScriptContext *ctx ) {};

static cg_asApiFuncPtr_t cg_asCGameAPI[] = {
	{ "void CGame::Load()", &cgs.asMain.load, false },
	{ "void CGame::Precache()", &cgs.asMain.precache, false },
	{ "void CGame::Init( const String @serverName, uint playerNum, "
	  "bool demoplaying, const String @demoName, bool pure, "
	  "uint snapFrameTime, int protocol, const String @demoExtension, bool gameStart)",
		&cgs.asMain.init, true },
	{ "void CGame::Frame( int frameTime, int realFrameTime, int64 realTime, int64 serverTime, "
	  "float stereoSeparation, uint extrapolationTime )",
		&cgs.asMain.frame, true },
	{ "bool CGame::AddEntity( int entNum )", &cgs.asMain.addEntity, false },

	{ "void CGame::Input::Init()", &cgs.asInput.init, true },
	{ "void CGame::Input::Shutdown()", &cgs.asInput.shutdown, true },
	{ "void CGame::Input::Frame( int64 inputTime )", &cgs.asInput.frame, true },
	{ "void CGame::Input::ClearState()", &cgs.asInput.clearState, true },
	{ "bool CGame::Input::KeyEvent( int key, bool down )", &cgs.asInput.keyEvent, false },
	{ "void CGame::Input::MouseMove( int mx, int my )", &cgs.asInput.mouseMove, true },
	{ "uint CGame::Input::GetButtonBits()", &cgs.asInput.getButtonBits, true },
	{ "Vec3 CGame::Input::GetAngularMovement()", &cgs.asInput.getAngularMovement, true },
	{ "Vec3 CGame::Input::GetMovement()", &cgs.asInput.getMovement, true },

	{ "void CGame::Camera::SetupCamera( CGame::Camera::Camera @cam )", &cgs.asCamera.setupCamera, true },
	{ "void CGame::Camera::SetupRefdef( CGame::Camera::Camera @cam )", &cgs.asCamera.setupRefdef, true },

	{ "void CGame::HUD::Init()", &cgs.asHUD.init, false },
	{ "bool CGame::HUD::DrawCrosshair()", &cgs.asHUD.drawCrosshair, false },

	{ "void CGame::NewPacketEntityState( const EntityState @ )", &cgs.asGameState.newPacketEntityState, false },
	{ "void CGame::ConfigString( int index, const String @ )", &cgs.asGameState.configString, false },
	{ "void CGame::UpdateEntities()", &cgs.asGameState.updateEntities, false },
	{ "void CGame::EntityEvent( const EntityState @, int ev, int parm, bool predicted )", &cgs.asGameState.entityEvent, false },

	{ nullptr, nullptr, false },
};

//=======================================================================

static const gs_asEnumVal_t asLimitsEnumVals[] = {
	ASLIB_ENUM_VAL( CG_MAX_TOUCHES ),

	ASLIB_ENUM_VAL_NULL,
};

static const gs_asEnum_t asCGameEnums[] = {
	{ "cg_limits_e", asLimitsEnumVals },

	ASLIB_ENUM_VAL_NULL,
};

//======================================================================

static entity_state_t *asSnapshot_getEntityState( int i, snapshot_t *snap )
{
	if( i < 0 || i >= snap->numEntities ) {
		return NULL;
	}
	return &snap->entities[i];
}

static const gs_asFuncdef_t asCGameSnapshotFuncdefs[] = {
	ASLIB_FUNCDEF_NULL,
};

static const gs_asBehavior_t asCGameSnapshotObjectBehaviors[] = {
	ASLIB_BEHAVIOR_NULL,
};

static const gs_asMethod_t asCGameSnapshotMethods[] = {
	{ ASLIB_FUNCTION_DECL( EntityState @, getEntityState, ( int index ) const ), asFUNCTION( asSnapshot_getEntityState ),
		asCALL_CDECL_OBJLAST },

	ASLIB_METHOD_NULL,
};

static const gs_asProperty_t asCGameSnapshotProperties[] = {
	{ ASLIB_PROPERTY_DECL( bool, valid ), ASLIB_FOFFSET( snapshot_t, valid ) },
	{ ASLIB_PROPERTY_DECL( int64, serverFrame ), ASLIB_FOFFSET( snapshot_t, serverFrame ) },
	{ ASLIB_PROPERTY_DECL( int64, serverTime ), ASLIB_FOFFSET( snapshot_t, serverTime ) },
	{ ASLIB_PROPERTY_DECL( int64, ucmdExecuted ), ASLIB_FOFFSET( snapshot_t, ucmdExecuted ) },
	{ ASLIB_PROPERTY_DECL( bool, delta ), ASLIB_FOFFSET( snapshot_t, delta ) },
	{ ASLIB_PROPERTY_DECL( bool, allentities ), ASLIB_FOFFSET( snapshot_t, allentities ) },
	{ ASLIB_PROPERTY_DECL( bool, multipov ), ASLIB_FOFFSET( snapshot_t, multipov ) },
	{ ASLIB_PROPERTY_DECL( int64, deltaFrameNum ), ASLIB_FOFFSET( snapshot_t, deltaFrameNum ) },
	{ ASLIB_PROPERTY_DECL( int, numPlayers ), ASLIB_FOFFSET( snapshot_t, numplayers ) },
	{ ASLIB_PROPERTY_DECL( PlayerState @, playerState ), ASLIB_FOFFSET( snapshot_t, playerState ) },
	{ ASLIB_PROPERTY_DECL( int, numEntities ), ASLIB_FOFFSET( snapshot_t, numEntities ) },

	ASLIB_PROPERTY_NULL,
};

static const gs_asClassDescriptor_t asSnapshotClassDescriptor = {
	"Snapshot",						/* name */
	asOBJ_REF | asOBJ_NOCOUNT,		/* object type flags */
	sizeof( snapshot_t ),			/* size */
	asCGameSnapshotFuncdefs,		/* funcdefs */
	asCGameSnapshotObjectBehaviors, /* object behaviors */
	asCGameSnapshotMethods,			/* methods */
	asCGameSnapshotProperties,		/* properties */
	NULL, NULL,						/* string factory hack */
};

//======================================================================

static const gs_asClassDescriptor_t asModelHandleClassDescriptor = {
	"ModelHandle",								   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,		   /* object behaviors */
	NULL,			   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

static const gs_asClassDescriptor_t asSoundHandleClassDescriptor = {
	"SoundHandle",								   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,		   /* object behaviors */
	NULL,			   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

static const gs_asClassDescriptor_t asShaderHandleClassDescriptor = {
	"ShaderHandle",								   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,		   /* object behaviors */
	NULL,			   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

static const gs_asClassDescriptor_t asSkinHandleClassDescriptor = {
	"SkinHandle",			   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),		   /* size */
	NULL,					   /* funcdefs */
	NULL,					   /* object behaviors */
	NULL,					   /* methods */
	NULL,					   /* properties */
	NULL, NULL				   /* string factory hack */
};

static const gs_asClassDescriptor_t asFontHandleClassDescriptor = {
	"FontHandle",								   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,		   /* object behaviors */
	NULL,			   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

static const gs_asClassDescriptor_t asModelSkeletonClassDescriptor = {
	"ModelSkeleton",							   /* name */
	asOBJ_REF | asOBJ_NOCOUNT, /* object type flags */
	sizeof( void * ),							   /* size */
	NULL,										   /* funcdefs */
	NULL,		   /* object behaviors */
	NULL,										   /* methods */
	NULL,										   /* properties */
	NULL, NULL									   /* string factory hack */
};

const gs_asClassDescriptor_t *const asCGameClassesDescriptors[] = {
	&asModelHandleClassDescriptor,
	&asSoundHandleClassDescriptor,
	&asShaderHandleClassDescriptor,
	&asFontHandleClassDescriptor,
	&asSnapshotClassDescriptor,
	&asModelSkeletonClassDescriptor,
	&asSkinHandleClassDescriptor,

	NULL,
};

//======================================================================

static void asFunc_Print( const asstring_t *str )
{
	if( !str || !str->buffer ) {
		return;
	}

	CG_Printf( "%s", str->buffer );
}

static void *asFunc_RegisterModel( const asstring_t *str )
{
	return CG_RegisterModel( str->buffer );
}

static void *asFunc_RegisterSound( const asstring_t *str )
{
	return CG_RegisterSfx( str->buffer );
}

static void *asFunc_RegisterShader( const asstring_t *str )
{
	return CG_RegisterShader( str->buffer );
}

static void *asFunc_RegisterFont( const asstring_t *str, int style, unsigned size )
{
	return trap_SCR_RegisterFont( str->buffer, style, size );
}

static int asFunc_ExtrapolationTime( void )
{
	return cgs.extrapolationTime;
}

static int asFunc_SnapFrameTime( void )
{
	return cgs.snapFrameTime;
}

static void *asFunc_SkeletonForModel( struct model_s *m )
{
	return CG_SkeletonForModel( m );
}

static int asFunc_TeamColorForEntity( int entNum ) {
	union {
		int			color;
		uint8_t		shaderRGBA[4];
	} res;

	CG_TeamColorForEntity( entNum, res.shaderRGBA );

	return res.color;
}

static int asFunc_PlayerColorForEntity( int entNum )
{
	union {
		int		color;
		uint8_t shaderRGBA[4];
	} res;

	CG_PlayerColorForEntity( entNum, res.shaderRGBA );

	return res.color;
}

static bool asFunc_IsViewerEntity( int entNum )
{
	return ISVIEWERENTITY( entNum );
}

static asstring_t *asFunc_GetConfigString( int index )
{
	if( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return NULL;
	}
	const char *cs = cgs.configStrings[ index ];
	return cgs.asExport->asStringFactoryBuffer( cs, strlen( cs ) );
}

static const gs_asglobfuncs_t asCGameGlobalFuncs[] = {
	{ "void Print( const String &in )", asFUNCTION( asFunc_Print ), NULL },
	{ "int get_ExtrapolationTime()", asFUNCTION( asFunc_ExtrapolationTime ), NULL },
	{ "int get_SnapFrameTime()", asFUNCTION( asFunc_SnapFrameTime ), NULL },
	{ "void AddEntityToSolidList( int number )", asFUNCTION( CG_AddEntityToSolidList ), NULL },
	{ "void AddEntityToTriggerList( int number )", asFUNCTION( CG_AddEntityToTriggerList ), NULL },

	{ "ModelHandle @RegisterModel( const String &in )", asFUNCTION( asFunc_RegisterModel ), NULL },
	{ "SoundHandle @RegisterSound( const String &in )", asFUNCTION( asFunc_RegisterSound ), NULL },
	{ "ShaderHandle @RegisterShader( const String &in )", asFUNCTION( asFunc_RegisterShader ), NULL },
	{ "FontHandle @RegisterFont( const String &in, int style, uint size )", asFUNCTION( asFunc_RegisterFont ), NULL },
	{ "ModelSkeleton @SkeletonForModel( ModelHandle @ )", asFUNCTION( asFunc_SkeletonForModel ), NULL },

	{ "int TeamColorForEntity( int entNum )", asFUNCTION( asFunc_TeamColorForEntity ), NULL },
	{ "int PlayerColorForEntity( int entNum )", asFUNCTION( asFunc_PlayerColorForEntity ), NULL },
	{ "bool IsViewerEntity( int entNum )", asFUNCTION( asFunc_IsViewerEntity ), NULL },
	{ "String @GetConfigString( int entNum )", asFUNCTION( asFunc_GetConfigString ), NULL },

	{ NULL },
};

static auto cg_predictedPlayerStatePtr = &cg.predictedPlayerState;
static auto cg_snapshotPtr = &cg.frame, cg_oldSnapshotPtr = &cg.oldFrame;

static const gs_asglobproperties_t asCGameGlobalProperties[] = {
	{ "PlayerState @PredictedPlayerState", &cg_predictedPlayerStatePtr },
	{ "Snapshot @Snap", &cg_snapshotPtr },
	{ "Snapshot @OldSnap", &cg_oldSnapshotPtr },

	{ NULL },
};

//======================================================================

/*
 * CG_asInitializeCGameEngineSyntax
 */
static void CG_asInitializeCGameEngineSyntax( asIScriptEngine *asEngine )
{
	CG_Printf( "* Initializing CGame module syntax\n" );

	// register shared stuff
	GS_asInitializeEngine( asEngine );

	// register global enums
	GS_asRegisterEnums( asEngine, asCGameEnums, "CGame" );
	GS_asRegisterEnums( asEngine, asCGameInputEnums, "CGame" );
	GS_asRegisterEnums( asEngine, asCGameCameraEnums, "CGame" );
	GS_asRegisterEnums( asEngine, asCGameScreenEnums, "CGame" );
	GS_asRegisterEnums( asEngine, asCGameRefSceneEnums, "CGame" );

	// register global funcdefs
	GS_asRegisterFuncdefs( asEngine, asCGameCmdFuncdefs, "CGame::Cmd" );

	// first register all class names so methods using custom classes work
	GS_asRegisterObjectClassNames( asEngine, asCGameClassesDescriptors, "CGame" );
	GS_asRegisterObjectClassNames( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );
	GS_asRegisterObjectClassNames( asEngine, asCGameCameraClassesDescriptors, "CGame::Camera" );
	GS_asRegisterObjectClassNames( asEngine, asCGameRefSceneClassesDescriptors, "CGame::Scene" );

	// register classes
	GS_asRegisterObjectClasses( asEngine, asCGameClassesDescriptors, "CGame" );
	GS_asRegisterObjectClasses( asEngine, asCGameInputClassesDescriptors, "CGame::Input" );
	GS_asRegisterObjectClasses( asEngine, asCGameCameraClassesDescriptors, "CGame::Camera" );
	GS_asRegisterObjectClasses( asEngine, asCGameRefSceneClassesDescriptors, "CGame::Scene" );

	// register global functions
	GS_asRegisterGlobalFunctions( asEngine, asCGameGlobalFuncs, "CGame" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameCmdGlobalFuncs, "CGame::Cmd" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameInputGlobalFuncs, "CGame::Input" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameCameraGlobalFuncs, "CGame::Camera" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameRefSceneGlobalFuncs, "CGame::Scene" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameScreenGlobalFuncs, "CGame::Screen" );
	GS_asRegisterGlobalFunctions( asEngine, asCGameSoundGlobalFuncs, "CGame::Sound" );

	// register global properties
	GS_asRegisterGlobalProperties( asEngine, asCGameGlobalProperties, "CGame" );
}

/*
 * CG_asInitScriptEngine
 */
void CG_asInitScriptEngine( void )
{
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
void CG_asShutdownScriptEngine( void )
{
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
bool CG_asExecutionErrorReport( int error )
{
	return ( error != asEXECUTION_FINISHED );
}

/*
 * CG_asCallScriptFunc
 */
bool CG_asCallScriptFunc(
	void *ptr, std::function<void( asIScriptContext * )> setArgs, std::function<void( asIScriptContext * )> getResult )
{
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

	if( ok ) {
		getResult( ctx );
	}

	return ok;
}

/*
 * CG_asUnloadScriptModule
 */
void CG_asUnloadScriptModule( const char *moduleName, cg_asApiFuncPtr_t *api )
{
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
asIScriptModule *CG_asLoadScriptModule(
	const char *moduleName, const char *dir, const char *filename, cg_asApiFuncPtr_t *api, time_t *mtime )
{
	auto asEngine = CGAME_AS_ENGINE();
	if( asEngine == NULL ) {
		return NULL;
	}

	std::string tempModuleName( moduleName );
	tempModuleName += va( "_%d", rand() );

	auto asModule = cgs.asExport->asLoadScriptProject( asEngine, tempModuleName.c_str(), "progs", dir, filename, mtime );
	if( asModule == nullptr ) {
		return nullptr;
	}

	if( !api ) {
		return asModule;
	}

	std::vector<asIScriptFunction *> funcs;
	for( size_t i = 0; api[i].decl != nullptr; i++ ) {
		auto decl = api[i].decl;
		auto ptr = asModule->GetFunctionByDecl( decl );

		if( !ptr && api[i].mandatory ) {
			CG_Printf( S_COLOR_RED "* The function '%s' was not found. Can't continue.\n", decl );
			goto error;
		}

		funcs.push_back( ptr );
	}

	CG_asUnloadScriptModule( moduleName, api );

	for( size_t i = 0; api[i].decl != nullptr; i++ ) {
		*api[i].ptr = funcs[i];
	}

	asModule->SetName( moduleName );

	//
	// execute the optional 'load' function
	//
	if( funcs[0] != NULL ) {
		if( !CG_asCallScriptFunc( funcs[0], cg_empty_as_cb, cg_empty_as_cb ) ) {
			CG_asUnloadScriptModule( moduleName, api );
			return nullptr;
		}
	}

	return asModule;

error:
	asEngine->DiscardModule( tempModuleName.c_str() );
	return nullptr;
}

/*
 * CG_asDumpAPI
 */
void CG_asDumpAPI( void )
{
	cgs.asExport->asWriteEngineDocsToFile(
		CGAME_AS_ENGINE(), va( "AS_API/v%.g/", trap_Cvar_Value( "version" ) ), "cgame", false, false, ~(unsigned)0, 0 );
}

//======================================================================

/*
 * CG_asLoadGameScript
 */
bool CG_asLoadGameScript( void )
{
	return CG_asLoadScriptModule( CG_SCRIPTS_GAME_MODULE_NAME, "client", "cgame.cp", cg_asCGameAPI, &cgs.asMain.mtime ) != nullptr;
}

/*
 * CG_asReloadGameScript
 */
bool CG_asReloadGameScript( void )
{
	time_t mtime = cgs.asExport->asScriptProjectMTime( "progs", "client", "cgame.cp" );
	if( mtime == -1 || mtime <= cgs.asMain.mtime ) {
		return false;
	}
	return CG_asLoadGameScript();
}

/*
 * CG_asUnloadGameScript
 */
void CG_asUnloadGameScript( void )
{
	CG_asUnloadScriptModule( CG_SCRIPTS_GAME_MODULE_NAME, cg_asCGameAPI );
}

//======================================================================

static cg_asApiFuncPtr_t cg_asPmoveAPI[] = {
	{ "void PM::Load()", &cgs.asPMove.load, false },
	{ "void PM::PMove( PMove @pm, PlayerState @playerState, UserCmd cmd )", &cgs.asPMove.pmove, true },
	{ "Vec3 PM::GetViewAnglesClamp( const PlayerState @playerState )", &cgs.asPMove.vaClamp, false },

	{ nullptr, nullptr, false },
};

bool CG_asLoadPMoveScript( void )
{
	return CG_asLoadScriptModule( CG_SCRIPTS_PMOVE_MODULE_NAME, PMOVE_SCRIPTS_DIRECTORY,
			   "pmove" PMOVE_SCRIPTS_PROJECT_EXTENSION, cg_asPmoveAPI, &cgs.asPMove.mtime ) != nullptr;
}

void CG_asUnloadPMoveScript( void )
{
	CG_asUnloadScriptModule( CG_SCRIPTS_PMOVE_MODULE_NAME, cg_asPmoveAPI );
}

void CG_asPMove( pmove_t *pm, player_state_t *ps, usercmd_t *cmd )
{
	CG_asCallScriptFunc(
		cgs.asPMove.pmove,
		[pm, ps, cmd]( asIScriptContext *ctx ) {
			ctx->SetArgObject( 0, pm );
			ctx->SetArgObject( 1, ps );
			ctx->SetArgObject( 2, cmd );
		},
		cg_empty_as_cb );
}

void CG_asGetViewAnglesClamp( const player_state_t *ps, vec3_t vaclamp )
{
	CG_asCallScriptFunc(
		cgs.asPMove.vaClamp,
		[ps]( asIScriptContext *ctx ) { ctx->SetArgObject( 0, const_cast<player_state_t *>( ps ) ); },
		[vaclamp]( asIScriptContext *ctx ) {
			const asvec3_t *va = (const asvec3_t *)ctx->GetReturnAddress();
			VectorCopy( va->v, vaclamp );
		} );
}

void CG_asPrecache( void )
{
	if( !cgs.asMain.precache ) {
		return;
	}
	CG_asCallScriptFunc( cgs.asMain.precache, cg_empty_as_cb, cg_empty_as_cb );
}

void CG_asInit( const char *serverName, unsigned int playerNum, bool demoplaying, const char *demoName, 
	bool pure, unsigned snapFrameTime, int protocol, const char *demoExtension, bool gameStart )
{
	if( !cgs.asMain.init ) {
		return;
	}
	CG_asCallScriptFunc( cgs.asMain.init, 
		[serverName, playerNum, demoplaying, demoName, pure, snapFrameTime, 
		protocol, demoExtension, gameStart]( asIScriptContext *ctx ) 
		{
			ctx->SetArgObject( 0, cgs.asExport->asStringFactoryBuffer( serverName, strlen( serverName ) ) );
			ctx->SetArgDWord( 1, playerNum );
			ctx->SetArgByte( 2, demoplaying );
			ctx->SetArgObject( 3, cgs.asExport->asStringFactoryBuffer( demoName, strlen( demoName ) ) );
			ctx->SetArgByte( 4, pure );
			ctx->SetArgDWord( 5, snapFrameTime );
			ctx->SetArgDWord( 6, protocol );
			ctx->SetArgObject( 7, cgs.asExport->asStringFactoryBuffer( demoExtension, strlen( demoExtension ) ) );
			ctx->SetArgByte( 8, gameStart );
		},
		cg_empty_as_cb );
}

void CG_asFrame( int frameTime, int realFrameTime, int64_t realTime, int64_t serverTime, float stereoSeparation,
	unsigned extrapolationTime )
{
	if( !cgs.asMain.frame ) {
		return;
	}
	CG_asCallScriptFunc(
		cgs.asMain.frame,
		[frameTime, realFrameTime, realTime, serverTime, stereoSeparation, extrapolationTime]( asIScriptContext *ctx ) {
			ctx->SetArgDWord( 0, frameTime );
			ctx->SetArgDWord( 1, realFrameTime );
			ctx->SetArgQWord( 2, realTime );
			ctx->SetArgQWord( 3, serverTime );
			ctx->SetArgFloat( 4, stereoSeparation );
			ctx->SetArgDWord( 5, extrapolationTime );
		},
		cg_empty_as_cb );
}

bool CG_asAddEntity( int entNum )
{
	uint8_t res = 0;

	if( !cgs.asMain.addEntity ) {
		return false;
	}

	CG_asCallScriptFunc(
		cgs.asMain.addEntity, 
		[entNum]( asIScriptContext *ctx ) { ctx->SetArgDWord( 0, entNum ); },
		[&res]( asIScriptContext *ctx ) { res = ctx->GetReturnByte(); }
	);

	return res == 0 ? false : true;
}

void CG_asNewPacketEntityState( entity_state_t *state )
{
	if( !cgs.asGameState.newPacketEntityState ) {
		return;
	}
	CG_asCallScriptFunc(
		cgs.asGameState.newPacketEntityState,
		[state]( asIScriptContext *ctx ) {
			ctx->SetArgObject( 0, state );
		},
		cg_empty_as_cb );
}

void CG_asConfigString( int index, const char *str )
{
	if( !cgs.asGameState.configString ) {
		return;
	}
	CG_asCallScriptFunc(
		cgs.asGameState.configString, [index, str]( asIScriptContext *ctx ) { 
			ctx->SetArgDWord( 0, index );
			ctx->SetArgObject( 1, cgs.asExport->asStringFactoryBuffer( str, strlen( str ) ) );
		},
		cg_empty_as_cb );
}

void CG_asUpdateEntities( void )
{
	if( !cgs.asGameState.updateEntities ) {
		return;
	}
	CG_asCallScriptFunc( cgs.asGameState.updateEntities, cg_empty_as_cb, cg_empty_as_cb );
}

void CG_asEntityEvent( entity_state_t *ent, int ev, int parm, bool predicted )
{
	if( !cgs.asGameState.entityEvent ) {
		return;
	}
	CG_asCallScriptFunc(
		cgs.asGameState.entityEvent,
		[ent, ev, parm, predicted]( asIScriptContext *ctx ) {
			ctx->SetArgObject( 0, ent );
			ctx->SetArgDWord( 1, ev );
			ctx->SetArgDWord( 2, parm );
			ctx->SetArgByte( 3, predicted );
		},
		cg_empty_as_cb );
}
