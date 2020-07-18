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
#include "cin.h"
#include "../qcommon/asyncstream.h"

static cgame_export_t *cge;

EXTERN_API_FUNC void *GetCGameAPI( void * );

static mempool_t *cl_gamemodulepool;

static void *module_handle;

static async_stream_module_t *cg_async_stream;

static int cg_load_seq = 1;

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

static inline bool CL_GameModule_CM_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return CM_InPVS( cl.cms, p1, p2 );
}

//======================================================================

#ifndef _MSC_VER
static void CL_GameModule_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static void CL_GameModule_Error( const char *msg );
#endif

/*
* CL_GameModule_Error
*/
static void CL_GameModule_Error( const char *msg ) {
	Com_Error( ERR_DROP, "%s", msg );
}

/*
* CL_GameModule_Print
*/
static void CL_GameModule_Print( const char *msg ) {
	Com_Printf( "%s", msg );
}

/*
* CL_GameModule_PrintToLog
*/
static void CL_GameModule_PrintToLog( const char *msg ) {
	Con_PrintSilent( msg );
}

/*
* CL_GameModule_GetConfigString
*/
static void CL_GameModule_GetConfigString( int i, char *str, int size ) {
	if( i < 0 || i >= MAX_CONFIGSTRINGS ) {
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: i > MAX_CONFIGSTRINGS" );
		return;
	}
	if( !str || size <= 0 ) {
		Com_DPrintf( S_COLOR_RED "CL_GameModule_GetConfigString: NULL string" );
		return;
	}

	Q_strncpyz( str, cl.configstrings[i], size );
}

/*
* CL_GameModule_NET_GetUserCmd
*/
static void CL_GameModule_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	if( cmd ) {
		if( frame < 0 ) {
			frame = 0;
		}

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
static void CL_GameModule_NET_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
	if( incomingAcknowledged ) {
		*incomingAcknowledged = cls.ucmdAcknowledged;
	}
	if( outgoingSequence ) {
		*outgoingSequence = cls.ucmdHead;
	}
	if( outgoingSent ) {
		*outgoingSent = cls.ucmdSent;
	}
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
* CL_GameModule_AsyncStream_Init
*/
static void CL_GameModule_AsyncStream_Init( void ) {
	cg_async_stream = AsyncStream_InitModule( "CGame", CL_GameModule_MemAlloc, CL_GameModule_MemFree );
}

/*
* CL_GameModule_AsyncStream_PerformRequest
*/
static int CL_GameModule_AsyncStream_PerformRequest( const char *url, const char *method,
													 const char *data, int timeout,
													 cg_async_stream_read_cb_t read_cb, cg_async_stream_done_cb_t done_cb, void *privatep ) {
	const char *headers[] = { NULL, NULL, NULL, NULL, NULL };

	assert( cg_async_stream );

	CL_AddSessionHttpRequestHeaders( url, headers );

	return AsyncStream_PerformRequestExt( cg_async_stream, url, method, data, headers, timeout,
										  0, read_cb, done_cb, NULL, privatep );
}

/*
* CL_GameModule_AsyncStream_Shutdown
*/
static void CL_GameModule_AsyncStream_Shutdown( void ) {
	AsyncStream_ShutdownModule( cg_async_stream );
	cg_async_stream = NULL;
}

//==============================================

#define CGAME_L10N_DOMAIN   "cgame"

/*
* CL_GameModule_L10n_LoadLangPOFile
*/
void CL_GameModule_L10n_LoadLangPOFile( const char *filepath ) {
	L10n_LoadLangPOFile( CGAME_L10N_DOMAIN, filepath );
}

/*
* CL_GameModule_TranslateString
*/
const char *CL_GameModule_L10n_TranslateString( const char *string ) {
	return L10n_TranslateString( CGAME_L10N_DOMAIN, string );
}

/*
* CL_GameModule_L10n_ClearDomain
*/
void CL_GameModule_L10n_ClearDomain( void ) {
	L10n_ClearDomain( CGAME_L10N_DOMAIN );
}

/*
* CL_GameModule_S_RawSamples
*/
static void CL_GameModule_S_RawSamples( unsigned int samples, unsigned int rate,
										unsigned short width, unsigned short channels, const uint8_t *data ) {
	CL_SoundModule_RawSamples( samples, rate, width, channels, data, false );
}

//==============================================

// wrap cgame listeners into proxies so that
// cinematics won't attempt to pass samples to
// entities which are not valid anymore due to
// module reload

#define MAX_CGAME_RAW_SAMPLES_LISTENERS 8

typedef struct {
	bool inuse;
	struct cinematics_s *cin;
	void *ptr;
	int load_seq;
	cg_raw_samples_cb_t rs;
	cg_get_raw_samples_cb_t grs;
} cg_raw_samples_listener_t;

cg_raw_samples_listener_t cg_raw_samples_listeners[MAX_CGAME_RAW_SAMPLES_LISTENERS];

/*
* CL_GameModule_RawSamples
*/
static void CL_GameModule_RawSamples( void *ptr, unsigned int samples,
									  unsigned int rate, unsigned short width, unsigned short channels,
									  const uint8_t *data ) {
	cg_raw_samples_listener_t *cglistener;

	// free listener
	cglistener = ( cg_raw_samples_listener_t * )ptr;

	// each listener gets samples passed exactly once
	cglistener->inuse = false;

	if( cglistener->load_seq != cg_load_seq ) {
		return;
	}
	cglistener->rs( cglistener->ptr, samples, rate, width, channels, data );
}

/*
* CL_GameModule_GetRawSamplesLength
*/
static unsigned int CL_GameModule_GetRawSamplesLength( void *ptr ) {
	cg_raw_samples_listener_t *cglistener;

	cglistener = ( cg_raw_samples_listener_t * )ptr;
	if( cglistener->load_seq != cg_load_seq ) {
		return 0;
	}
	return cglistener->grs( cglistener->ptr );
}

/*
* CL_GameModule_AddRawSamplesListener
*/
static bool CL_GameModule_AddRawSamplesListener( struct cinematics_s *cin,
												 void *listener, cg_raw_samples_cb_t rs, cg_get_raw_samples_cb_t grs ) {
	int i;
	cg_raw_samples_listener_t *cglistener, *freel;

	freel = NULL;

	cglistener = cg_raw_samples_listeners;
	for( i = 0; i < MAX_CGAME_RAW_SAMPLES_LISTENERS; i++ ) {
		if( !freel && !cglistener->inuse ) {
			// grab a free one
			freel = cglistener;
		} else if( cglistener->inuse
				   && cglistener->cin == cin
				   && cglistener->ptr == listener
				   && cglistener->rs == rs
				   && cglistener->grs == grs ) {
			// same listener
			return true;
		}
		cglistener++;
	}

	if( !freel ) {
		return false;
	}

	// fill in our proxy
	cglistener = freel;
	cglistener->inuse = true;
	cglistener->cin = cin;
	cglistener->ptr = listener;
	cglistener->load_seq = cg_load_seq;
	cglistener->rs = (cin_raw_samples_cb_t)rs;
	cglistener->grs = (cin_get_raw_samples_cb_t)grs;

	if( !CIN_AddRawSamplesListener( cin, cglistener, &CL_GameModule_RawSamples,
									&CL_GameModule_GetRawSamplesLength ) ) {
		// free listener
		cglistener->inuse = false;
		return false;
	}

	return true;
}

//==============================================

/*
* CL_GameModule_Init
*/
void CL_GameModule_Init( void ) {
	int apiversion;
	int64_t start;
	cgame_import_t import;
	void *( *builtinAPIfunc )( void * ) = NULL;
#ifdef CGAME_HARD_LINKED
	builtinAPIfunc = GetCGameAPI;
#endif

	// stop all playing sounds
	CL_SoundModule_StopAllSounds( true, true );

	CL_GameModule_Shutdown();

	cl_gamemodulepool = _Mem_AllocPool( NULL, "Client Game Progs", MEMPOOL_CLIENTGAME, __FILE__, __LINE__ );

	import.Error = CL_GameModule_Error;
	import.Print = CL_GameModule_Print;
	import.PrintToLog = CL_GameModule_PrintToLog;

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

	import.NET_GetUserCmd = CL_GameModule_NET_GetUserCmd;
	import.NET_GetCurrentUserCmdNum = CL_GameModule_NET_GetCurrentUserCmdNum;
	import.NET_GetCurrentState = CL_GameModule_NET_GetCurrentState;

	import.R_UpdateScreen = SCR_UpdateScreen;
	import.R_GetClippedFragments = re.GetClippedFragments;
	import.R_ClearScene = re.ClearScene;
	import.R_AddEntityToScene = re.AddEntityToScene;
	import.R_AddLightToScene = re.AddLightToScene;
	import.R_AddPolyToScene = re.AddPolyToScene;
	import.R_AddLightStyleToScene = re.AddLightStyleToScene;
	import.R_RenderScene = re.RenderScene;
	import.R_GetSpeedsMessage = re.GetSpeedsMessage;
	import.R_GetAverageFrametime = re.GetAverageFrametime;
	import.R_RegisterWorldModel = re.RegisterWorldModel;
	import.R_ModelBounds = re.ModelBounds;
	import.R_ModelFrameBounds = re.ModelFrameBounds;
	import.R_RegisterModel = re.RegisterModel;
	import.R_RegisterPic = re.RegisterPic;
	import.R_RegisterRawPic = re.RegisterRawPic;
	import.R_RegisterLevelshot = re.RegisterLevelshot;
	import.R_RegisterSkin = re.RegisterSkin;
	import.R_RegisterSkinFile = re.RegisterSkinFile;
	import.R_RegisterLinearPic = re.RegisterLinearPic;
	import.R_LerpTag = re.LerpTag;
	import.R_LightForOrigin = re.LightForOrigin;
	import.R_SetCustomColor = re.SetCustomColor;
	import.R_DrawStretchPic = re.DrawStretchPic;
	import.R_DrawStretchPoly = re.DrawStretchPoly;
	import.R_DrawRotatedStretchPic = re.DrawRotatedStretchPic;
	import.R_Scissor = re.Scissor;
	import.R_GetScissor = re.GetScissor;
	import.R_ResetScissor = re.ResetScissor;
	import.R_GetShaderDimensions = re.GetShaderDimensions;
	import.R_TransformVectorToScreen = re.TransformVectorToScreen;
	import.R_SkeletalGetNumBones = re.SkeletalGetNumBones;
	import.R_SkeletalGetBoneInfo = re.SkeletalGetBoneInfo;
	import.R_SkeletalGetBonePose = re.SkeletalGetBonePose;

	import.R_GetShaderForOrigin = re.GetShaderForOrigin;
	import.R_GetShaderCinematic = re.GetShaderCinematic;

	import.VID_FlashWindow = VID_FlashWindow;

	import.CM_NumInlineModels = CL_GameModule_CM_NumInlineModels;
	import.CM_InlineModel = CL_GameModule_CM_InlineModel;
	import.CM_TransformedBoxTrace = CL_GameModule_CM_TransformedBoxTrace;
	import.CM_RoundUpToHullSize = CL_GameModule_CM_RoundUpToHullSize;
	import.CM_TransformedPointContents = CL_GameModule_CM_TransformedPointContents;
	import.CM_ModelForBBox = CL_GameModule_CM_ModelForBBox;
	import.CM_OctagonModelForBBox = CL_GameModule_CM_OctagonModelForBBox;
	import.CM_InlineModelBounds = CL_GameModule_CM_InlineModelBounds;
	import.CM_InPVS = CL_GameModule_CM_InPVS;

	import.S_RegisterSound = CL_SoundModule_RegisterSound;
	import.S_StartFixedSound = CL_SoundModule_StartFixedSound;
	import.S_StartRelativeSound = CL_SoundModule_StartRelativeSound;
	import.S_StartGlobalSound = CL_SoundModule_StartGlobalSound;
	import.S_StartLocalSound = CL_SoundModule_StartLocalSound;
	import.S_Update = CL_GameModule_SoundUpdate;
	import.S_AddLoopSound = CL_SoundModule_AddLoopSound;
	import.S_StartBackgroundTrack = CL_SoundModule_StartBackgroundTrack;
	import.S_StopBackgroundTrack = CL_SoundModule_StopBackgroundTrack;
	import.S_RawSamples = CL_GameModule_S_RawSamples;
	import.S_PositionedRawSamples = CL_SoundModule_PositionedRawSamples;
	import.S_GetRawSamplesLength = CL_SoundModule_GetRawSamplesLength;
	import.S_GetPositionedRawSamplesLength = CL_SoundModule_GetPositionedRawSamplesLength;
	import.S_SetEntitySpatilization = CL_SoundModule_SetEntitySpatilization;

	import.SCR_RegisterFont = SCR_RegisterFont;
	import.SCR_RegisterSpecialFont = SCR_RegisterSpecialFont;
	import.SCR_DrawString = SCR_DrawString;
	import.SCR_DrawStringWidth = SCR_DrawStringWidth;
	import.SCR_DrawClampString = SCR_DrawClampString;
	import.SCR_DrawMultilineString = SCR_DrawMultilineString;
	import.SCR_DrawRawChar = SCR_DrawRawChar;
	import.SCR_DrawClampChar = SCR_DrawClampChar;
	import.SCR_FontSize = SCR_FontSize;
	import.SCR_FontHeight = SCR_FontHeight;
	import.SCR_FontUnderline = SCR_FontUnderline;
	import.SCR_FontAdvance = SCR_FontAdvance;
	import.SCR_FontXHeight = SCR_FontXHeight;
	import.SCR_SetDrawCharIntercept = SCR_SetDrawCharIntercept;
	import.SCR_DrawChat = Con_DrawChat;
	import.SCR_strWidth = SCR_strWidth;
	import.SCR_StrlenForWidth = SCR_StrlenForWidth;
	import.SCR_ShowOverlay = SCR_ShowOverlay;
	import.SCR_HaveOverlay = SCR_HaveOverlay;
	import.SCR_OverlayHover = SCR_OverlayHover;
	import.SCR_OverlayKeyEvent = SCR_OverlayKeyEvent;
	import.SCR_OverlayMouseMove = SCR_OverlayMouseMove;

	import.AsyncStream_UrlEncode = AsyncStream_UrlEncode;
	import.AsyncStream_UrlDecode = AsyncStream_UrlDecode;
	import.AsyncStream_PerformRequest = CL_GameModule_AsyncStream_PerformRequest;
	import.GetBaseServerURL = CL_GetBaseServerURL;

	import.Mem_Alloc = CL_GameModule_MemAlloc;
	import.Mem_Free = CL_GameModule_MemFree;

	import.L10n_LoadLangPOFile = &CL_GameModule_L10n_LoadLangPOFile;
	import.L10n_TranslateString = &CL_GameModule_L10n_TranslateString;
	import.L10n_ClearDomain = &CL_GameModule_L10n_ClearDomain;

	import.CIN_AddRawSamplesListener = &CL_GameModule_AddRawSamplesListener;

	import.IN_GetThumbsticks = IN_GetThumbsticks;
	import.IN_IME_GetCandidates = IN_IME_GetCandidates;
	import.IN_SupportedDevices = IN_SupportedDevices;

	import.asGetAngelExport = Com_asGetAngelExport;

	if( builtinAPIfunc ) {
		cge = builtinAPIfunc( &import );
	} else {
		cge = (cgame_export_t *)Com_LoadGameLibrary( "cgame", "GetCGameAPI", &module_handle, &import, cls.sv_pure, NULL );
	}
	if( !cge ) {
		Com_Error( ERR_DROP, "Failed to load client game DLL" );
	}

	apiversion = cge->API();
	if( apiversion != CGAME_API_VERSION ) {
		Com_UnloadGameLibrary( &module_handle );
		Mem_FreePool( &cl_gamemodulepool );
		cge = NULL;
		Com_Error( ERR_DROP, "Client game is version %i, not %i", apiversion, CGAME_API_VERSION );
	}

	CL_GameModule_AsyncStream_Init();

	start = Sys_Milliseconds();
	cge->Init( cls.servername, cl.playernum,
			   viddef.width, viddef.height, VID_GetPixelRatio(),
			   cls.demo.playing, cls.demo.playing ? cls.demo.filename : "",
			   cls.sv_pure, cl.snapFrameTime, APP_PROTOCOL_VERSION, APP_DEMO_EXTENSION_STR,
			   cls.mediaRandomSeed, cl.gamestart );

	Com_DPrintf( "CL_GameModule_Init: %.2f seconds\n", (float)( Sys_Milliseconds() - start ) * 0.001f );

	cl.gamestart = false;
	cls.cgameActive = true;
}

/*
* CL_GameModule_Reset
*/
void CL_GameModule_Reset( void ) {
	if( cge ) {
		cge->Reset();
	}
}

/*
* CL_GameModule_Shutdown
*/
void CL_GameModule_Shutdown( void ) {
	if( !cge ) {
		return;
	}

	cg_load_seq++;
	cls.cgameActive = false;

	CL_GameModule_AsyncStream_Shutdown();

	cge->Shutdown();
	Mem_FreePool( &cl_gamemodulepool );
	Com_UnloadGameLibrary( &module_handle );
	cge = NULL;
}

/*
* CL_GameModule_EscapeKey
*/
void CL_GameModule_EscapeKey( void ) {
	if( cge ) {
		cge->EscapeKey();
	} else if( cls.state == CA_CINEMATIC ) {
		SCR_FinishCinematic();
	}
}

/*
* CL_GameModule_GetEntitySoundOrigin
*/
void CL_GameModule_GetEntitySpatilization( int entNum, vec3_t origin, vec3_t velocity ) {
	if( cge ) {
		cge->GetEntitySpatilization( entNum, origin, velocity );
	}
}

/*
* CL_GameModule_ConfigString
*/
void CL_GameModule_ConfigString( int number, const char *value ) {
	if( cge ) {
		cge->ConfigString( number, value );
	}
}

/*
* CL_GameModule_NewSnapshot
*/
bool CL_GameModule_NewSnapshot( int pendingSnapshot ) {
	snapshot_t *currentSnap, *newSnap;

	if( cge ) {
		currentSnap = ( cl.currentSnapNum <= 0 ) ? NULL : &cl.snapShots[cl.currentSnapNum & UPDATE_MASK];
		newSnap = &cl.snapShots[pendingSnapshot & UPDATE_MASK];
		return cge->NewFrameSnapshot( newSnap, currentSnap );
	}

	return false;
}

/*
* CL_GameModule_RenderView
*/
void CL_GameModule_RenderView( float stereo_separation ) {
	if( cge && cls.cgameActive ) {
		cge->RenderView( cls.frametime, cls.realFrameTime, cls.realtime, cl.serverTime, stereo_separation,
						 cl_extrapolate->integer && !cls.demo.playing ? cl_extrapolationTime->integer : 0 );
	}
}

/*
* CL_GameModule_InputFrame
*/
void CL_GameModule_InputFrame( int64_t inputTime ) {
	if( cge ) {
		cge->InputFrame( inputTime );
	}
}

/*
* CL_GameModule_ClearInputState
*/
void CL_GameModule_ClearInputState( void ) {
	if( cge ) {
		cge->ClearInputState();
	}
}

/*
* CL_GameModule_GetButtonBits
*/
unsigned CL_GameModule_GetButtonBits( void ) {
	if( cge ) {
		return cge->GetButtonBits();
	}
	return 0;
}

/*
* CL_GameModule_AddViewAngles
*/
void CL_GameModule_AddViewAngles( vec3_t viewAngles ) {
	if( cge ) {
		cge->AddViewAngles( viewAngles );
	}
}

/*
* CL_GameModule_AddMovement
*/
void CL_GameModule_AddMovement( vec3_t movement ) {
	if( cge ) {
		cge->AddMovement( movement );
	}
}

/*
* CL_GameModule_MouseMove
*/
void CL_GameModule_MouseMove( int dx, int dy ) {
	if( cge ) {
		cge->MouseMove( dx, dy );
	}
}

/*
* CL_GameModule_KeyEvent
*/
bool CL_GameModule_KeyEvent( int key, bool down ) {
	if( cge ) {
		return cge->KeyEvent( key, down );
	}
	return false;
}

/*
* CL_GameModule_TouchEvent
*/
void CL_GameModule_TouchEvent( int id, touchevent_t type, int x, int y, int64_t time ) {
	if( cge ) {
		cge->TouchEvent( id, type, x, y, time );
	}
}

/*
* CL_GameModule_IsTouchDown
*/
bool CL_GameModule_IsTouchDown( int id ) {
	if( cge ) {
		return cge->IsTouchDown( id );
	}

	return false;
}

/*
 * CL_GameModule_HotloadAssets
 */
void CL_GameModule_HotloadAssets( void )
{
	if( cge ) {
		cge->HotloadAssets();
	}
}
