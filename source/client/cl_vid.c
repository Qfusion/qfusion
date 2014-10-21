/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// qfusion refresh engine.
#include "client.h"
#include "cin.h"
#include "ftlib.h"

cvar_t *vid_ref;
cvar_t *vid_mode;
cvar_t *vid_customwidth;   // custom screen width
cvar_t *vid_customheight;  // custom screen height
cvar_t *vid_xpos;          // X coordinate of window position
cvar_t *vid_ypos;          // Y coordinate of window position
cvar_t *vid_fullscreen;
cvar_t *vid_displayfrequency;
cvar_t *vid_multiscreen_head;
cvar_t *vid_parentwid;		// parent window identifier
cvar_t *win_noalttab;
cvar_t *win_nowinkeys;

// Global variables used internally by this module
viddef_t viddef;             // global video state; used by other modules

ref_export_t re;

#define VID_DEFAULTREF			"ref_gl"
#define VID_DEFAULTMODE			"-2"
#define VID_DEFAULTFALLBACKMODE	"4"

#define VID_NUM_MODES (int)( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

typedef rserr_t (*vid_init_t)( int, int, int, int, int, void *, qboolean, qboolean );

static int      vid_ref_prevmode;
static qboolean vid_ref_modified;
static qboolean vid_ref_verbose;
static qboolean vid_ref_sound_restart;
static qboolean vid_ref_active;
static qboolean vid_initialized;

static void		*vid_ref_libhandle = NULL;
static mempool_t *vid_ref_mempool = NULL;

// These are system specific functions
rserr_t VID_Sys_Init( int x, int y, int width, int height, int displayFrequency, void *parentWindow,
	qboolean fullscreen, qboolean wideScreen, qboolean verbose, void (*initcb)(void) ); // wrapper around R_Init
static rserr_t VID_SetMode( int x, int y, int width, int height, int displayFrequency, void *parentWindow,
	qboolean fullScreen, qboolean wideScreen );
void VID_Front_f( void );
void VID_UpdateWindowPosAndSize( int x, int y );
void VID_EnableAltTab( qboolean enable );
void VID_EnableWinKeys( qboolean enable );

/*
** VID_Restart_f
*
* Console command to re-start the video mode and refresh DLL. We do this
* simply by setting the vid_ref_modified variable, which will
* cause the entire video mode and refresh DLL to be reset on the next frame.
*/
void VID_Restart( qboolean verbose, qboolean soundRestart )
{
	vid_ref_modified = qtrue;
	vid_ref_verbose = verbose;
	vid_ref_sound_restart = soundRestart;
}

void VID_Restart_f( void )
{
	VID_Restart( (Cmd_Argc() >= 2 ? qtrue : qfalse), qfalse );
}


typedef struct vidmode_s
{
	int width, height;
	qboolean wideScreen;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ 320, 240, qfalse },
	{ 400, 300, qfalse },
	{ 512, 384, qfalse },
	{ 640, 480, qfalse },
	{ 800, 600, qfalse },
	{ 960, 720, qfalse },
	{ 1024, 768, qfalse },
	{ 1152, 864, qfalse },
	{ 1280, 960, qfalse },
	{ 1280, 1024, qfalse },
	{ 1600, 1200, qfalse },
	{ 2048, 1536, qfalse },

	{ 800, 480, qtrue },
	{ 856, 480, qtrue },
	{ 1024,	576, qtrue },
	{ 1024, 600, qtrue },
	{ 1280, 720, qtrue },
	{ 1200, 800, qtrue },
	{ 1280, 800, qtrue },
	{ 1360, 768, qtrue },
	{ 1366, 768, qtrue },
	{ 1440,	900, qtrue },
	{ 1600,	900, qtrue },
	{ 1680,	1050, qtrue },
	{ 1920, 1080, qtrue },
	{ 1920,	1200, qtrue },
	{ 2560,	1600, qtrue },

	{ 2400, 600, qfalse },
	{ 3072, 768, qfalse },
	{ 3840, 720, qfalse },
	{ 3840, 1024, qfalse },
	{ 4800, 1200, qfalse },
	{ 6144, 1536, qfalse }
};

/*
** VID_GetModeInfo
*/
qboolean VID_GetModeInfo( int *width, int *height, qboolean *wideScreen, int mode )
{
	if( mode < -1 || mode >= VID_NUM_MODES )
		return qfalse;

	if( mode == -1 )
	{
		*width = vid_customwidth->integer;
		*height = vid_customheight->integer;
		*wideScreen = qfalse;
	}
	else
	{
		*width  = vid_modes[mode].width;
		*height = vid_modes[mode].height;
		*wideScreen = vid_modes[mode].wideScreen;
	}

	return qtrue;
}

/*
** VID_GetModeNum
*
* Find exact match for given width/height
*/
static int VID_GetModeNum( int width, int height )
{
	int i;

	for( i = 0; i < VID_NUM_MODES; i++ )
	{
		if( vid_modes[i].width == width && vid_modes[i].height == height ) {
			return i;
		}
	}

	return -1;
}

/*
** VID_ModeList_f
*/
static void VID_ModeList_f( void )
{
	int i;

	Com_Printf( "Mode -1: for custom width/height (use vid_customwidth and vid_customheight)\n" );
	for( i = 0; i < VID_NUM_MODES; i++ )
		Com_Printf( "Mode %i: %ix%i%s\n", i, vid_modes[i].width, vid_modes[i].height, ( vid_modes[i].wideScreen ? " (wide)" : "" ) );
}

/*
** VID_NewWindow
*/
static void VID_NewWindow( int width, int height )
{
	viddef.width  = width;
	viddef.height = height;
}

static rserr_t VID_Sys_Init_( int x, int y, int width, int height, int displayFrequency,
	void *parentWindow, qboolean fullScreen, qboolean wideScreen )
{
	return VID_Sys_Init( x, y, width, height, displayFrequency, parentWindow, 
		fullScreen, wideScreen, vid_ref_verbose, SCR_BeginLoadingPlaque );
}

/*
** VID_SetMode
*/
static rserr_t VID_SetMode( int x, int y, int width, int height, int displayFrequency,
	void *parentWindow, qboolean fullScreen, qboolean wideScreen )
{
    return re.SetMode( x, y, width, height, displayFrequency, fullScreen, wideScreen );
}

/*
** VID_AppActivate
*/
void VID_AppActivate( qboolean active, qboolean destroy )
{
	re.AppActivate( active, destroy );
}

/*
** VID_RefreshActive
*/
qboolean VID_RefreshActive( void )
{
	return vid_ref_active;
}

/*
** VID_GetWindowWidth
*/
int VID_GetWindowWidth( void )
{
	return viddef.width;
}

/*
** VID_GetWindowHeight
*/
int VID_GetWindowHeight( void )
{
	return viddef.height;
}

/*
** VID_ChangeMode
*/
static rserr_t VID_ChangeMode( vid_init_t vid_init )
{
	int x, y;
	int w, h;
	int disp_freq;
	qboolean fs, ws;
	qboolean r;
	rserr_t err;
	void *parent_win;

#if 0
	if( vid_ref_active && !Cvar_FlagIsSet( vid_mode->flags, CVAR_LATCH_VIDEO ) ) {
		// try to change video mode without vid_restart
		err = VID_ChangeMode( &VID_SetMode );

		if( err == rserr_restart_required ) {
			// didn't work, mark the cvar as CVAR_LATCH_VIDEO 
			Cvar_Get( vid_mode->name, VID_DEFAULTMODE, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
			Com_Printf( "%s will be changed upon restarting video.\n", vid_mode->name );
	}
#endif

	vid_mode->modified = qfalse;
	vid_fullscreen->modified = qfalse;

	x = vid_xpos->integer;
	y = vid_ypos->integer;
	fs = vid_fullscreen->integer ? qtrue : qfalse;
	disp_freq = vid_displayfrequency->integer;
	parent_win = STR_TO_POINTER( vid_parentwid->string );
	r = VID_GetModeInfo( &w, &h, &ws, vid_mode->integer );

	if( !r ) {
		err = rserr_invalid_mode;
	}
	else {
		err = vid_init( x, y, w, h, disp_freq, parent_win, fs, ws );
	}

	if( err == rserr_ok ) {
		// store fallback mode
		vid_ref_prevmode = vid_mode->integer;
	} else if( err == rserr_restart_required ) {
		return err;
	}
	else {
		Cvar_ForceSet( "vid_fullscreen", "0" );
		vid_fullscreen->modified = qfalse;
		fs = qfalse;

		if( err == rserr_invalid_fullscreen ) {
			Com_Printf( "VID_ChangeMode() - fullscreen unavailable in this mode\n" );

			if( ( err = vid_init( x, y, w, h, disp_freq, parent_win, qfalse, ws ) ) == rserr_ok ) {
				goto done_ok;
			}
		}
		else if( err == rserr_invalid_mode ) {
			Cvar_ForceSet( "vid_mode", va( "%i", vid_ref_prevmode ) );
			vid_mode->modified = qfalse;

			Com_Printf( "VID_ChangeMode() - invalid mode\n" );

			r = VID_GetModeInfo( &w, &h, &ws, vid_mode->integer );
			if( !r ) {
				return rserr_invalid_mode;
			}
		}

		// try setting it back to something safe
		if( ( err = vid_init( x, y, w, h, disp_freq, parent_win, fs, ws ) ) != rserr_ok ) {
			Com_Printf( "VID_ChangeMode() - could not revert to safe mode\n" );
			return err;
		}
	}

done_ok:

	// let the sound and input subsystems know about the new window
	VID_NewWindow( w, h );

	return rserr_ok;
}

/*
** VID_UnloadRefresh
*/
static void VID_UnloadRefresh( void )
{
	if( vid_ref_libhandle ) {
		if( vid_ref_active ) {
			re.Shutdown( qfalse );
			vid_ref_active = qfalse;
		}
		Com_UnloadLibrary( &vid_ref_libhandle );
		Mem_FreePool( &vid_ref_mempool );
	}
}

static void *VID_RefModule_MemAllocExt( mempool_t *pool, size_t size, size_t align, int z, const char *filename, int fileline ) {
	return _Mem_AllocExt( pool, size, align, z, MEMPOOL_REFMODULE, 0, filename, fileline );
}

static void VID_RefModule_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_REFMODULE, 0, filename, fileline );
}

static mempool_t *VID_RefModule_MemAllocPool( mempool_t *parent, const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool( parent ? parent : vid_ref_mempool, name, MEMPOOL_REFMODULE, filename, fileline );
}

static void VID_RefModule_MemFreePool( mempool_t **pool, const char *filename, int fileline ) {
	_Mem_FreePool( pool, MEMPOOL_REFMODULE, 0, filename, fileline );
}

static void VID_RefModule_MemEmptyPool( mempool_t *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool( pool, MEMPOOL_REFMODULE, 0, filename, fileline );
}

/*
** VID_LoadRefresh
*/
static qboolean VID_LoadRefresh( const char *name )
{
	static ref_import_t import;
	dllfunc_t funcs[2];
	GetRefAPI_t GetRefAPI_f;

	VID_UnloadRefresh();

	import.Com_Error = &Com_Error;
	import.Com_Printf = &Com_Printf;
	import.Com_DPrintf = &Com_DPrintf;

	import.Sys_Milliseconds = &Sys_Milliseconds;
	import.Sys_Microseconds = &Sys_Microseconds;
	import.Sys_Sleep = &Sys_Sleep;

	import.Cvar_Get = &Cvar_Get;
	import.Cvar_Set = &Cvar_Set;
	import.Cvar_ForceSet = &Cvar_ForceSet;
	import.Cvar_SetValue = &Cvar_SetValue;
	import.Cvar_String = &Cvar_String;
	import.Cvar_Value = &Cvar_Value;

	import.Cmd_Argc = &Cmd_Argc;
	import.Cmd_Argv = &Cmd_Argv;
	import.Cmd_Args = &Cmd_Args;
	import.Cmd_AddCommand = &Cmd_AddCommand;
	import.Cmd_RemoveCommand = &Cmd_RemoveCommand;
	import.Cmd_Execute = &Cbuf_Execute;
	import.Cmd_ExecuteText = &Cbuf_ExecuteText;
	import.Cmd_SetCompletionFunc = &Cmd_SetCompletionFunc;

	import.FS_FOpenFile = &FS_FOpenFile;
	import.FS_FOpenAbsoluteFile = &FS_FOpenAbsoluteFile;
	import.FS_Read = &FS_Read;
	import.FS_Write = &FS_Write;
	import.FS_Printf = &FS_Printf;
	import.FS_Tell = &FS_Tell;
	import.FS_Seek = &FS_Seek;
	import.FS_Eof = &FS_Eof;
	import.FS_Flush = &FS_Flush;
	import.FS_FCloseFile = &FS_FCloseFile;
	import.FS_RemoveFile = &FS_RemoveFile;
	import.FS_GetFileList = &FS_GetFileList;
	import.FS_GetGameDirectoryList = &FS_GetGameDirectoryList;
	import.FS_FirstExtension = &FS_FirstExtension;
	import.FS_MoveFile = &FS_MoveFile;
	import.FS_IsUrl = &FS_IsUrl;
	import.FS_FileMTime = &FS_FileMTime;
	import.FS_RemoveDirectory = &FS_RemoveDirectory;
	import.FS_GameDirectory = &FS_GameDirectory;
	import.FS_WriteDirectory = &FS_WriteDirectory;

	import.CIN_Open = &CIN_Open;
	import.CIN_NeedNextFrame = &CIN_NeedNextFrame;
	import.CIN_ReadNextFrame = &CIN_ReadNextFrame;
	import.CIN_ReadNextFrameYUV = &CIN_ReadNextFrameYUV;
	import.CIN_Reset = &CIN_Reset;
	import.CIN_Close = &CIN_Close;

	import.Mem_AllocPool = &VID_RefModule_MemAllocPool;
	import.Mem_FreePool = &VID_RefModule_MemFreePool;
	import.Mem_EmptyPool = &VID_RefModule_MemEmptyPool;
	import.Mem_AllocExt = &VID_RefModule_MemAllocExt;
	import.Mem_Free = &VID_RefModule_MemFree;
	import.Mem_Realloc = &_Mem_Realloc;
	import.Mem_PoolTotalSize = &Mem_PoolTotalSize;

	import.Thread_Create = QThread_Create;
	import.Thread_Join = QThread_Join;
	import.Thread_Yield = QThread_Yield;
	import.Mutex_Create = QMutex_Create;
	import.Mutex_Destroy = QMutex_Destroy;
	import.Mutex_Lock = QMutex_Lock;
	import.Mutex_Unlock = QMutex_Unlock;

	import.BufQueue_Create = QBufQueue_Create;
	import.BufQueue_Destroy = QBufQueue_Destroy;
	import.BufQueue_Finish = QBufQueue_Finish;
	import.BufQueue_EnqueueCmd = QBufQueue_EnqueueCmd;
	import.BufQueue_ReadCmds = QBufQueue_ReadCmds;

	// load dynamic library
	Com_Printf( "Loading refresh module %s... ", name );
	funcs[0].name = "GetRefAPI";
	funcs[0].funcPointer = (void **) &GetRefAPI_f;
	funcs[1].name = NULL;
	vid_ref_libhandle = Com_LoadLibrary( va( LIB_DIRECTORY "/%s_" ARCH LIB_SUFFIX, name ), funcs );

	if( vid_ref_libhandle ) {
		// load succeeded
		int api_version;
		ref_export_t *rep;

		rep = GetRefAPI_f( &import );
		re = *rep;
		vid_ref_mempool = Mem_AllocPool( NULL, "Refresh" );
		api_version = re.API();

		if( api_version != REF_API_VERSION ) {
			// wrong version
			Com_Printf( "Wrong version: %i, not %i.\n", api_version, REF_API_VERSION );
			VID_UnloadRefresh();
			return qfalse;
		}
	}
	else
	{
		Com_Printf( "Not found %s.\n", va( LIB_DIRECTORY "/%s_" ARCH LIB_SUFFIX, name ) );
		return qfalse;
	}

	return qtrue;
}

/*
** VID_CheckChanges
*
* This function gets called once just before drawing each frame, and its sole purpose in life
* is to check to see if any of the video mode parameters have changed, and if they have to
* update the rendering DLL and/or video mode to match.
*/
void VID_CheckChanges( void )
{
	rserr_t err;
	qboolean vid_ref_was_active = vid_ref_active;
	qboolean verbose = vid_ref_verbose || vid_ref_sound_restart;

	if( win_noalttab->modified ) {
		VID_EnableAltTab( win_noalttab->integer ? qfalse : qtrue );
		win_noalttab->modified = qfalse;
	}

	if( win_nowinkeys->modified ) {
		VID_EnableWinKeys( win_nowinkeys->integer ? qfalse : qtrue );
		win_nowinkeys->modified = qfalse;
	}

	if( vid_fullscreen->modified ) {
		if( vid_ref_active ) {
			// try to change video mode without vid_restart
			err = VID_ChangeMode( &VID_SetMode );

			if( err == rserr_restart_required ) {
				// didn't work, mark the cvar as CVAR_LATCH_VIDEO 
				Com_Printf( "Changing vid_fullscreen requires restarting video.\n", vid_fullscreen->name );
				Cvar_ForceSet( vid_fullscreen->name, va( "%i", !vid_fullscreen->integer ) );
			}
		}

		vid_fullscreen->modified = qfalse;
	}

	if( vid_ref->modified ) {
		vid_ref_modified = qtrue;
		vid_ref->modified = qfalse;
	}

	if( vid_ref_modified ) {
		qboolean cgameActive;

		cgameActive = cls.cgameActive;
		cls.disable_screen = 1;

		CL_ShutdownMedia();

		// stop and free all sounds
		CL_SoundModule_Shutdown( qfalse );

		FTLIB_FreeFonts( qfalse );

		L10n_ClearDomains();

		Cvar_GetLatchedVars( CVAR_LATCH_VIDEO );

load_refresh:
		if( !VID_LoadRefresh( vid_ref->string ) ) {
			// reset to default
			if( !Q_stricmp( vid_ref->string, VID_DEFAULTREF ) ) {
				Sys_Error( "Failed to load default refresh DLL" );
			}
			Cvar_ForceSet( vid_ref->name, VID_DEFAULTREF );
			vid_ref->modified = qfalse;
			goto load_refresh;
		}

		// handle vid_mode changes
		if( vid_mode->integer == -2 ) {
			int w, h;

			if( VID_GetDisplaySize( &w, &h ) ) {
				int mode = VID_GetModeNum( w, h );

				Com_Printf( "Mode %i detected\n", mode );

				if( mode < 0 ) {
					Cvar_ForceSet( "vid_mode", "-1" );
					Cvar_ForceSet( "vid_customwidth", va( "%i", w ) );
					Cvar_ForceSet( "vid_customheight", va( "%i", h ) );
				}
				else {
					Cvar_ForceSet( "vid_mode", va( "%i", mode ) );
				}
			}
			else {
				Cvar_ForceSet( "vid_mode", va( "%i", vid_ref_prevmode ) );
			}
		}

		if( vid_mode->integer < -1 || vid_mode->integer >= VID_NUM_MODES ) {
			Com_Printf( "Bad mode %i or custom resolution\n", vid_mode->integer );
			Cvar_ForceSet( "vid_fullscreen", "0" );
			Cvar_ForceSet( "vid_mode", VID_DEFAULTFALLBACKMODE );
		}

		err = VID_ChangeMode( &VID_Sys_Init_ );
		if( err != rserr_ok ) {
			Sys_Error( "VID_ChangeMode() failed with code %i", err );
		}
		vid_ref_active = qtrue;

		// stop and free all sounds
		CL_SoundModule_Init( verbose );

		re.BeginRegistration();
		CL_SoundModule_BeginRegistration();

		FTLIB_PrecacheFonts( verbose );

		// load common localization strings
		L10n_LoadLangPOFile( "common", "l10n" );

		if( vid_ref_was_active ) {
			IN_Restart();
		}

		CL_InitMedia();

		Con_Close();

		if( cgameActive ) {
			CL_GameModule_Init();
			Con_Close();
			CL_UIModule_ForceMenuOff();
			CL_SetKeyDest( key_game );
		}
		else {
			CL_UIModule_ForceMenuOn();
			CL_SetKeyDest( key_menu );
		}

		re.EndRegistration();
		CL_SoundModule_EndRegistration();

		vid_ref_modified = qfalse;
		vid_ref_verbose = qtrue;
	}

	/*
	** update our window position
	*/
	if( vid_xpos->modified || vid_ypos->modified )
	{
		if( !vid_fullscreen->integer )
			VID_UpdateWindowPosAndSize( vid_xpos->integer, vid_ypos->integer );
		vid_xpos->modified = qfalse;
		vid_ypos->modified = qfalse;
	}
}

/*
** VID_Init
*/
void VID_Init( void )
{
	if( vid_initialized )
		return;

	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get( "vid_ref", VID_DEFAULTREF, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_mode = Cvar_Get( "vid_mode", VID_DEFAULTMODE, CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_customwidth = Cvar_Get( "vid_customwidth", "1024", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_customheight = Cvar_Get( "vid_customheight", "768", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_xpos = Cvar_Get( "vid_xpos", "0", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get( "vid_ypos", "0", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE );
	vid_displayfrequency = Cvar_Get( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCH_VIDEO );
	vid_multiscreen_head = Cvar_Get( "vid_multiscreen_head", "-1", CVAR_ARCHIVE );
	vid_parentwid = Cvar_Get( "vid_parentwid", "0", CVAR_NOSET );

	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );
	win_nowinkeys = Cvar_Get( "win_nowinkeys", "0", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand( "vid_restart", VID_Restart_f );
	Cmd_AddCommand( "vid_front", VID_Front_f );
	Cmd_AddCommand( "vid_modelist", VID_ModeList_f );

	/* Start the graphics mode and load refresh DLL */
	vid_ref_modified = qtrue;
	vid_ref_active = qfalse;
	vid_ref_verbose = qtrue;
	vid_initialized = qtrue;
	vid_ref_sound_restart = qfalse;
	vid_fullscreen->modified = qfalse;
	vid_ref_prevmode = atoi( VID_DEFAULTFALLBACKMODE );

	FTLIB_LoadLibrary( qfalse );

	VID_CheckChanges();
}

/*
** VID_Shutdown
*/
void VID_Shutdown( void )
{
	if( !vid_initialized )
		return;

	VID_UnloadRefresh();

	FTLIB_UnloadLibrary( qfalse );

	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "vid_front" );
	Cmd_RemoveCommand( "vid_modelist" );

	vid_initialized = qfalse;
}
