/*
Copyright (C) 2002-2003 Victor Luchits

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
#include "client.h"

static cgame_export_t *cge;

EXTERN_API_FUNC void *GetCGameAPI( void * );

static mempool_t *cl_gamemodulepool;

static void *module_handle;

//======================================================================

// CL_GameModule versions of the CM functions passed to the game module
// they only add sv.cms as the first parameter

//======================================================================

static inline int CL_GameModule_CM_NumInlineModels( void ) {
	return CM_NumInlineModels( cl.cms );
}

static inline int CL_GameModule_CM_TransformedPointContents( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles ) {
	return CM_TransformedPointContents( cl.cms, p, cmodel, origin, angles );
}

static inline void CL_GameModule_CM_TransformedBoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
	struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles ) {
	CM_TransformedBoxTrace( cl.cms, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline void CL_GameModule_CM_RoundUpToHullSize( vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel ) {
	CM_RoundUpToHullSize( cl.cms, mins, maxs, cmodel );
}

static inline struct cmodel_s *CL_GameModule_CM_InlineModel( int num ) {
	return CM_InlineModel( cl.cms, num );
}

static inline void CL_GameModule_CM_InlineModelBounds( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	CM_InlineModelBounds( cl.cms, cmodel, mins, maxs );
}

static inline struct cmodel_s *CL_GameModule_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CM_ModelForBBox( cl.cms, mins, maxs );
}

static inline struct cmodel_s *CL_GameModule_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CM_OctagonModelForBBox( cl.cms, mins, maxs );
}
//======================================================================

/*
* CL_GameModule_Error
*/
static void CL_GameModule_Error( const char *msg ) {
	Com_Error( ERR_DROP, "%s", msg );
}

/*
* CL_GameModule_Print
*/
static void CL_GameModule_Print( const char *msg )
{
	Com_Printf( "%s", msg );
}

/*
* CL_GameModule_PrintToLog
*/
static void CL_GameModule_PrintToLog( const char *msg )
{
	Con_PrintSilent( msg );
}

/*
* CL_GameModule_GetConfigString
*/
static void CL_GameModule_GetConfigString( int i, char *str, int size )
{
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
	if( !str || size <= 0 )
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: NULL string" );

	Q_strncpyz( str, cl.configstrings[i], size );
}

/*
* CL_GameModule_NET_GetUserCmd
*/
static void CL_GameModule_NET_GetUserCmd( int frame, usercmd_t *cmd )
{
	if( cmd )
	{
		if( frame < 0 )
			frame = 0;

		*cmd = cl.cmds[frame & CMD_MASK];
	}
}

/*
* CL_GameModule_NET_GetCurrentUserCmdNum
*/
static int CL_GameModule_NET_GetCurrentUserCmdNum( void ) {
	return cls.ucmdHead;
}

/*
* CL_GameModule_NET_GetCurrentState
*/
static void CL_GameModule_NET_GetCurrentState( int *incomingAcknowledged, int *outgoingSequence, int *outgoingSent )
{
	if( incomingAcknowledged )
#ifdef TCP_ALLOW_CONNECT
		*incomingAcknowledged = cls.ucmdHead;
#else
		*incomingAcknowledged = cls.ucmdAcknowledged;
#endif
	if( outgoingSequence )
		*outgoingSequence = cls.ucmdHead;
	if( outgoingSent )
		*outgoingSent = cls.ucmdSent;
}

/*
* CL_GameModule_RefreshMouseAngles
*/
static void CL_GameModule_RefreshMouseAngles( void )
{
	CL_UpdateCommandInput(); // force a mouse refresh when requesting the in-work user command
}

/*
* CL_GameModule_R_RegisterWorldModel
*/
static void CL_GameModule_R_RegisterWorldModel( const char *model ) {
	R_RegisterWorldModel( model, cl.cms ? CM_PVSData( cl.cms ) : NULL );
}

/*
* CL_GameModule_MemAlloc
*/
static void *CL_GameModule_MemAlloc( size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( cl_gamemodulepool, size, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
* CL_GameModule_MemFree
*/
static void CL_GameModule_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_CLIENTGAME, 0, filename, fileline );
}

/*
* CL_GameModule_SoundUpdate
*/
static void CL_GameModule_SoundUpdate( const vec3_t origin, const vec3_t velocity,
	const mat3_t axis, const char *identity ) {
	CL_SoundModule_Update( origin, velocity, axis, identity, CL_WriteAvi() && cls.demo.avi_audio );
}

//==============================================

/*
* CL_GameModule_Init
*/
void CL_GameModule_Init( void )
{
	int apiversion, oldState;
	unsigned int start;
	cgame_import_t import;
	void *( *builtinAPIfunc )(void *) = NULL;
#ifdef CGAME_HARD_LINKED
	builtinAPIfunc = GetCGameAPI;
#endif

	// stop all playing sounds
	CL_SoundModule_StopAllSounds();

	CL_GameModule_Shutdown();

	// disable reading of client game module chat cvars
	Cvar_ForceSet( "con_chatCGame", "0" );

	cl_gamemodulepool = _Mem_AllocPool( NULL, "Client Game Progs", MEMPOOL_CLIENTGAME, __FILE__, __LINE__ );

	import.Error = CL_GameModule_Error;
	import.Print = CL_GameModule_Print;
	import.PrintToLog = CL_GameModule_PrintToLog;

	import.Dynvar_Create = Dynvar_Create;
	import.Dynvar_Destroy = Dynvar_Destroy;
	import.Dynvar_Lookup = Dynvar_Lookup;
	import.Dynvar_GetName = Dynvar_GetName;
	import.Dynvar_GetValue = Dynvar_GetValue;
	import.Dynvar_SetValue = Dynvar_SetValue;
	import.Dynvar_AddListener = Dynvar_AddListener;
	import.Dynvar_RemoveListener = Dynvar_RemoveListener;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_SetValue = Cvar_SetValue;
	import.Cvar_ForceSet = Cvar_ForceSet;
	import.Cvar_String = Cvar_String;
	import.Cvar_Value = Cvar_Value;

	import.Cmd_TokenizeString = Cmd_TokenizeString;
	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;

	import.Cmd_AddCommand = Cmd_AddCommand;
	import.Cmd_RemoveCommand = Cmd_RemoveCommand;
	import.Cmd_ExecuteText = Cbuf_ExecuteText;
	import.Cmd_Execute = Cbuf_Execute;
	import.Cmd_SetCompletionFunc = Cmd_SetCompletionFunc;

	import.FS_FOpenFile = FS_FOpenFile;
	import.FS_Read = FS_Read;
	import.FS_Write = FS_Write;

	import.FS_Print = FS_Print;
	import.FS_Tell = FS_Tell;
	import.FS_Seek = FS_Seek;
	import.FS_Eof = FS_Eof;
	import.FS_Flush = FS_Flush;
	import.FS_FCloseFile = FS_FCloseFile;
	import.FS_RemoveFile = FS_RemoveFile;
	import.FS_GetFileList = FS_GetFileList;
	import.FS_FirstExtension = FS_FirstExtension;
	import.FS_IsPureFile = FS_IsPureFile;
	import.FS_MoveFile = FS_MoveFile;
	import.FS_IsUrl = FS_IsUrl;
	import.FS_FileMTime = FS_BaseFileMTime;
	import.FS_RemoveDirectory = FS_RemoveDirectory;

	import.Key_GetBindingBuf = Key_GetBindingBuf;
	import.Key_KeynumToString = Key_KeynumToString;

	import.GetConfigString = CL_GameModule_GetConfigString;
	import.Milliseconds = Sys_Milliseconds;
	import.DownloadRequest = CL_DownloadRequest;

	import.Hash_BlockChecksum = Com_MD5Digest32;
	import.Hash_SuperFastHash = Com_SuperFastHash;

	import.NET_GetUserCmd = CL_GameModule_NET_GetUserCmd;
	import.NET_GetCurrentUserCmdNum = CL_GameModule_NET_GetCurrentUserCmdNum;
	import.NET_GetCurrentState = CL_GameModule_NET_GetCurrentState;
	import.RefreshMouseAngles = CL_GameModule_RefreshMouseAngles;

	import.R_UpdateScreen = SCR_UpdateScreen;
	import.R_GetClippedFragments = R_GetClippedFragments;
	import.R_ClearScene = R_ClearScene;
	import.R_AddEntityToScene = R_AddEntityToScene;
	import.R_AddLightToScene = R_AddLightToScene;
	import.R_AddPolyToScene = R_AddPolyToScene;
	import.R_AddLightStyleToScene = R_AddLightStyleToScene;
	import.R_RenderScene = R_RenderScene;
	import.R_SpeedsMessage = R_SpeedsMessage;
	import.R_RegisterWorldModel = CL_GameModule_R_RegisterWorldModel;
	import.R_ModelBounds = R_ModelBounds;
	import.R_ModelFrameBounds = R_ModelFrameBounds;
	import.R_RegisterModel = R_RegisterModel;
	import.R_RegisterPic = R_RegisterPic;
	import.R_RegisterRawPic = R_RegisterRawPic;
	import.R_RegisterLevelshot = R_RegisterLevelshot;
	import.R_RegisterSkin = R_RegisterSkin;
	import.R_RegisterSkinFile = R_RegisterSkinFile;
	import.R_LerpTag = R_LerpTag;
	import.R_LightForOrigin = R_LightForOrigin;
	import.R_SetCustomColor = R_SetCustomColor;
	import.R_DrawStretchPic = R_DrawStretchPic;
	import.R_DrawStretchPoly = R_DrawStretchPoly;
	import.R_DrawRotatedStretchPic = R_DrawRotatedStretchPic;
	import.R_SetScissorRegion = R_SetScissorRegion;
	import.R_GetScissorRegion = R_GetScissorRegion;
	import.R_GetShaderDimensions = R_GetShaderDimensions;
	import.R_TransformVectorToScreen = R_TransformVectorToScreen;
	import.R_SkeletalGetNumBones = R_SkeletalGetNumBones;
	import.R_SkeletalGetBoneInfo = R_SkeletalGetBoneInfo;
	import.R_SkeletalGetBonePose = R_SkeletalGetBonePose;

	import.VID_FlashWindow = VID_FlashWindow;

	import.CM_NumInlineModels = CL_GameModule_CM_NumInlineModels;
	import.CM_InlineModel = CL_GameModule_CM_InlineModel;
	import.CM_TransformedBoxTrace = CL_GameModule_CM_TransformedBoxTrace;
	import.CM_RoundUpToHullSize = CL_GameModule_CM_RoundUpToHullSize;
	import.CM_TransformedPointContents = CL_GameModule_CM_TransformedPointContents;
	import.CM_ModelForBBox = CL_GameModule_CM_ModelForBBox;
	import.CM_OctagonModelForBBox = CL_GameModule_CM_OctagonModelForBBox;
	import.CM_InlineModelBounds = CL_GameModule_CM_InlineModelBounds;

	import.S_RegisterSound = CL_SoundModule_RegisterSound;
	import.S_StartFixedSound = CL_SoundModule_StartFixedSound;
	import.S_StartRelativeSound = CL_SoundModule_StartRelativeSound;
	import.S_StartGlobalSound = CL_SoundModule_StartGlobalSound;
	import.S_Update = CL_GameModule_SoundUpdate;
	import.S_AddLoopSound = CL_SoundModule_AddLoopSound;
	import.S_StartBackgroundTrack = CL_SoundModule_StartBackgroundTrack;
	import.S_StopBackgroundTrack = CL_SoundModule_StopBackgroundTrack;

	import.SCR_RegisterFont = SCR_RegisterFont;
	import.SCR_DrawString = SCR_DrawString;
	import.SCR_DrawStringWidth = SCR_DrawStringWidth;
	import.SCR_DrawClampString = SCR_DrawClampString;
	import.SCR_strHeight = SCR_strHeight;
	import.SCR_strWidth = SCR_strWidth;
	import.SCR_StrlenForWidth = SCR_StrlenForWidth;

	import.Mem_Alloc = CL_GameModule_MemAlloc;
	import.Mem_Free = CL_GameModule_MemFree;

	cge = (cgame_export_t *)Com_LoadGameLibrary( "cgame", "GetCGameAPI", &module_handle, &import, builtinAPIfunc, cls.sv_pure, NULL );
	if( !cge )
		Com_Error( ERR_DROP, "Failed to load client game DLL" );

	AC_LoadLibrary( (void *) &import, (void *) cge, ANTICHEAT_CLIENT );	// impulZ: Refire AC Init

	apiversion = cge->API();
	if( apiversion != CGAME_API_VERSION )
	{
		Com_UnloadGameLibrary( &module_handle );
		Mem_FreePool( &cl_gamemodulepool );
		cge = NULL;
		Com_Error( ERR_DROP, "Client game is version %i, not %i", apiversion, CGAME_API_VERSION );
	}

	oldState = cls.state;
	cls.state = CA_LOADING;

	start = Sys_Milliseconds();
	cge->Init( cls.servername, cl.playernum, viddef.width, viddef.height, 
		cls.demo.playing, cls.demo.playing ? cls.demo.filename : "",
		cls.sv_pure, cl.snapFrameTime, APP_PROTOCOL_VERSION, cls.mediaRandomSeed );

	Com_DPrintf( "CL_GameModule_Init: %.2f seconds\n", (float)( Sys_Milliseconds() - start ) * 0.001f );

	cls.state = oldState;
	cls.cgameActive = qtrue;

	// check memory integrity
	Mem_CheckSentinelsGlobal();

	Sys_SendKeyEvents(); // pump message loop
}

/*
* CL_GameModule_Reset
*/
void CL_GameModule_Reset( void )
{
	if( cge )
		cge->Reset();
}

/*
* CL_GameModule_Shutdown
*/
void CL_GameModule_Shutdown( void )
{
	if( !cge )
		return;

	cls.cgameActive = qfalse;

	cge->Shutdown();
	Mem_FreePool( &cl_gamemodulepool );
	Com_UnloadGameLibrary( &module_handle );
	cge = NULL;
}

/*
* CL_GameModule_EscapeKey
*/
void CL_GameModule_EscapeKey( void )
{
	if( cge )
		cge->EscapeKey();
	else if( cls.state == CA_CINEMATIC )
		SCR_FinishCinematic();
}

/*
* CL_GameModule_GetEntitySoundOrigin
*/
void CL_GameModule_GetEntitySpatilization( int entNum, vec3_t origin, vec3_t velocity )
{
	if( cge )
		cge->GetEntitySpatilization( entNum, origin, velocity );
}

/*
* CL_GameModule_ConfigString
*/
void CL_GameModule_ConfigString( int number, const char *value )
{
	if( cge )
		cge->ConfigString( number, value );
}

/*
* CL_GameModule_SetSensitivityScale
*/
float CL_GameModule_SetSensitivityScale( const float sens )
{
	if( cge )
		return cge->SetSensitivityScale( sens );
	else
		return 1.0f;
}

/*
* CL_GameModule_NewSnapshot
*/
qboolean CL_GameModule_NewSnapshot( int pendingSnapshot )
{
	snapshot_t	*currentSnap, *newSnap;

	if( cge )
	{
		currentSnap = ( cl.currentSnapNum <= 0 ) ? NULL : &cl.snapShots[cl.currentSnapNum & UPDATE_MASK];
		newSnap = &cl.snapShots[pendingSnapshot & UPDATE_MASK];
		cge->NewFrameSnapshot( newSnap, currentSnap );
		return qtrue;
	}

	return qfalse;
}

/*
* CL_GameModule_RenderView
*/
void CL_GameModule_RenderView( float stereo_separation )
{
	if( cge )
		cge->RenderView( cls.frametime, cls.realframetime, cls.realtime, cl.serverTime, stereo_separation, 
		cl_extrapolate->integer && !cls.demo.playing ? cl_extrapolationTime->integer : 0 );
}
