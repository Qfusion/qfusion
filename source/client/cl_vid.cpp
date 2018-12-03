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
#include "xpm.h"
#include "ftlib/ftlib_public.h"
#include "renderer/r_local.h"
#include "renderer/r_frontend.h"
#include "sdl/sdl_window.h"

static cvar_t *vid_width, *vid_height;
static cvar_t *vid_fullscreen;
static cvar_t *vid_borderless;
static cvar_t *vid_displayfrequency;

static cvar_t *vid_vsync;
static bool force_vsync;

// Global variables used internally by this module
viddef_t viddef;             // global video state; used by other modules

ref_export_t re;

static bool vid_app_active;
static bool vid_app_minimized;

static mempool_t *vid_ref_mempool = NULL;


static unsigned int num_vid_modes;
static vidmode_t *vid_modes;

/*
** VID_GetModeInfo
*/
bool VID_GetModeInfo( int *width, int *height, unsigned int mode ) {
	if( mode >= num_vid_modes ) {
		return false;
	}

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;
	return true;
}

/*
** VID_ModeList_f
*/
static void VID_ModeList_f( void ) {
	unsigned int i;

	for( i = 0; i < num_vid_modes; i++ )
		Com_Printf( "* %ix%i\n", vid_modes[i].width, vid_modes[i].height );
}

/*
** VID_AppActivate
*/
void VID_AppActivate( bool active, bool minimize ) {
	vid_app_active = active;
	vid_app_minimized = minimize;
	re.AppActivate( active, minimize );
}

/*
** VID_AppIsActive
*/
bool VID_AppIsActive( void ) {
	return vid_app_active;
}

/*
** VID_AppIsMinimized
*/
bool VID_AppIsMinimized( void ) {
	return vid_app_minimized;
}

/*
** VID_GetWindowWidth
*/
int VID_GetWindowWidth( void ) {
	return viddef.width;
}

/*
** VID_GetWindowHeight
*/
int VID_GetWindowHeight( void ) {
	return viddef.height;
}

/*
** VID_LoadRefresh
*/
static bool VID_LoadRefresh() {
	static ref_import_t import;

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
	import.FS_RemoveDirectory = &FS_RemoveDirectory;
	import.FS_GameDirectory = &FS_GameDirectory;
	import.FS_WriteDirectory = &FS_WriteDirectory;

	import.Thread_Create = QThread_Create;
	import.Thread_Join = QThread_Join;
	import.Thread_Yield = QThread_Yield;
	import.Mutex_Create = QMutex_Create;
	import.Mutex_Destroy = QMutex_Destroy;
	import.Mutex_Lock = QMutex_Lock;
	import.Mutex_Unlock = QMutex_Unlock;

	import.BufPipe_Create = QBufPipe_Create;
	import.BufPipe_Destroy = QBufPipe_Destroy;
	import.BufPipe_Finish = QBufPipe_Finish;
	import.BufPipe_WriteCmd = QBufPipe_WriteCmd;
	import.BufPipe_ReadCmds = QBufPipe_ReadCmds;
	import.BufPipe_Wait = QBufPipe_Wait;

	// load succeeded
	ref_export_t * rep = GetRefAPI( &import );
	re = *rep;
	vid_ref_mempool = Mem_AllocPool( NULL, "Refresh" );

	Com_Printf( "\n" );
	return true;
}

/*
** VID_CompareModes
*/
static int VID_CompareModes( const vidmode_t *first, const vidmode_t *second ) {
	if( first->width == second->width ) {
		return first->height - second->height;
	}

	return first->width - second->width;
}

static void VID_InitModes() {
	int n = VID_GetNumVideoModes();
	if( n == 0 ) {
		Sys_Error( "Failed to get video modes" );
	}

	vid_modes = ( vidmode_t * ) Mem_ZoneMalloc( n * sizeof( vidmode_t ) );

	for( int i = 0; i < n; i++ ) {
		VideoMode mode = VID_GetVideoMode( i );
		vid_modes[ i ].width = mode.width;
		vid_modes[ i ].height = mode.height;
	}

	qsort( vid_modes, n, sizeof( vidmode_t ), ( int ( * )( const void *, const void * ) )VID_CompareModes );

	// Remove duplicate modes in case the sys code failed to do so.
	num_vid_modes = 0;
	int prevWidth = 0, prevHeight = 0;
	for( int i = 0; i < n; i++ ) {
		int width = vid_modes[i].width, height = vid_modes[i].height;
		if( ( width == prevWidth ) && ( height == prevHeight ) ) {
			continue;
		}

		vid_modes[num_vid_modes++] = vid_modes[i];
		prevWidth = width;
		prevHeight = height;
	}
}

static void VID_PickNearestVideoMode( int * w, int * h ) {
	assert( num_vid_modes > 0 );

	int mindiff = INT_MAX;
	int minidx = 0;

	for( unsigned int i = 0; i < num_vid_modes; i++ ) {
		int diff = abs( vid_modes[ i ].width - *w ) + abs( vid_modes[ i ].height - *h );
		if( diff < mindiff ) {
			mindiff = diff;
			minidx = i;
		}
	}

	*w = vid_modes[ minidx ].width;
	*h = vid_modes[ minidx ].height;
}

static VideoMode VID_MakeVideoMode() {
	int w = vid_width->integer;
	int h = vid_height->integer;

	if( vid_fullscreen->integer )
		VID_PickNearestVideoMode( &w, &h );

	// set video mode
	VideoMode mode;
	mode.width = w;
	mode.height = h;
	mode.frequency = vid_displayfrequency->integer;
	mode.fullscreen = FullScreenMode_Windowed;
	if( vid_fullscreen->integer )
		mode.fullscreen = vid_borderless->integer ? FullScreenMode_FullscreenBorderless : FullScreenMode_Fullscreen;

	return mode;
}

// this is shit and should not exist but i am sick of working on this
void Retarded_SetWindowSize( int w, int h ) {
	viddef.width = w;
	viddef.height = h;
	glConfig.width = w;
	glConfig.height = h;
}

static bool VID_ChangeMode() {
	VideoMode prev_mode = VID_GetCurrentVideoMode();

	VideoMode new_mode = VID_MakeVideoMode();
	if( new_mode.width == 0 || new_mode.height == 0 )
		return false;

	bool ok = VID_SetVideoMode( new_mode );
	if( ok ) {
		Retarded_SetWindowSize( new_mode.width, new_mode.height );
		CL_GameModule_ResizeWindow( new_mode.width, new_mode.height );
		RF_ResizeFramebuffers();
		return true;
	}

	ok = VID_SetVideoMode( prev_mode );
	if( !ok )
		VID_SetVideoMode( VID_GetDefaultVideoMode() );

	return false;
}

/*
** VID_CheckChanges
*
* This function gets called once just before drawing each frame, and its sole purpose in life
* is to check to see if any of the video mode parameters have changed, and if they have to
* update the rendering DLL and/or video mode to match.
*/
void VID_CheckChanges( void ) {
	if( vid_vsync->modified ) {
		VID_EnableVsync( force_vsync || vid_vsync->integer != 0 ? VsyncEnabled_Enabled : VsyncEnabled_Disabled );
		vid_vsync->modified = false;
	}

	bool mode_changed = vid_fullscreen->modified || vid_borderless->modified || vid_width->modified || vid_height->modified;
	if( !mode_changed )
		return;

	VID_ChangeMode();

	vid_fullscreen->modified = false;
	vid_borderless->modified = false;
	vid_width->modified = false;
	vid_height->modified = false;
}

void VID_Init() {
	VID_InitModes();

	/* Create the video variables so we know how to start the graphics drivers */
	vid_width = Cvar_Get( "vid_width", "0", CVAR_ARCHIVE );
	vid_height = Cvar_Get( "vid_height", "0", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE );
	vid_borderless = Cvar_Get( "vid_borderless", "1", CVAR_ARCHIVE );
	vid_displayfrequency = Cvar_Get( "vid_displayfrequency", "0", CVAR_ARCHIVE );

	vid_vsync = Cvar_Get( "vid_vsync", "0", CVAR_ARCHIVE );
	force_vsync = false;

	vid_fullscreen->modified = false;
	vid_borderless->modified = false;
	vid_width->modified = false;
	vid_height->modified = false;

	/* Add some console commands that we want to handle */
	Cmd_AddCommand( "vid_modelist", VID_ModeList_f );

	/* Cvar_GetLatchedVars( CVAR_LATCH_VIDEO ); */

	// if they haven't set vid_width/vid_height just make it fullscreen
	bool unset = false;
	if( vid_width->integer <= 0 || vid_height->integer <= 0 ) {
		Cvar_ForceSet( vid_fullscreen->name, "1" );
		unset = true;
	}

	VideoMode mode = VID_MakeVideoMode();
	VID_WindowInit( mode, 8 );

	if( unset ) {
		VideoMode def = VID_GetCurrentVideoMode();

		Cvar_ForceSet( vid_fullscreen->name, def.fullscreen == FullScreenMode_Windowed ? "0" : "1" );
		if( vid_fullscreen->integer ) {
			Cvar_ForceSet( vid_borderless->name, def.fullscreen == FullScreenMode_Fullscreen ? "0" : "1" );
		}

		char buf[ 16 ];
		Q_snprintfz( buf, sizeof( buf ), "%i", def.width );
		Cvar_ForceSet( vid_width->name, buf );
		Q_snprintfz( buf, sizeof( buf ), "%i", def.height );
		Cvar_ForceSet( vid_height->name, buf );
		Q_snprintfz( buf, sizeof( buf ), "%i", def.frequency );
		Cvar_ForceSet( vid_displayfrequency->name, buf );

		mode = def;
	}

	CL_Profiler_InitGL();

	Retarded_SetWindowSize( mode.width, mode.height ); // TODO: this should get the window size

	if( !VID_LoadRefresh() ) {
		Sys_Error( "Failed to load renderer" );
	}

	rserr_t err = R_Init( true );
	if( err != rserr_ok ) {
		Sys_Error( "VID_Init() failed with code %i", err );
	}

	CL_SoundModule_Init( false );

	RF_BeginRegistration();

	FTLIB_LoadLibrary( false );
	FTLIB_PrecacheFonts( true );

	CL_InitMedia();

	cls.disable_screen = 0;

	Con_Close();

	// TODO: what is this?
	if( cls.cgameActive ) {
		CL_GameModule_Init();
		CL_UIModule_ForceMenuOff();
		CL_SetKeyDest( key_game );

		// this will precache game assets
		SCR_UpdateScreen();
	} else {
		CL_UIModule_ForceMenuOn();
		CL_SetKeyDest( key_menu );
	}

	RF_EndRegistration();
}

void CL_ForceVsync( bool force ) {
	if( force != force_vsync ) {
		force_vsync = force;
		vid_vsync->modified = true;
	}
}

void VID_Shutdown() {
	CL_ShutdownMedia();

	FTLIB_FreeFonts( false );

	RF_Shutdown( false );
	Mem_FreePool( &vid_ref_mempool );

	FTLIB_Shutdown();

	Cmd_RemoveCommand( "vid_modelist" );

	Mem_ZoneFree( vid_modes );

	VID_WindowShutdown();

	CL_Profiler_ShutdownGL();
}
