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
#include "xpm.h"
#include "renderer/r_local.h"
#include "renderer/r_frontend.h"
#include "../sdl/sdl_window.h"

cvar_t *vid_width, *vid_height;
cvar_t *vid_fullscreen;
cvar_t *vid_borderless;
cvar_t *vid_displayfrequency;
cvar_t *vid_parentwid;      // parent window identifier

// Global variables used internally by this module
viddef_t viddef;             // global video state; used by other modules

ref_export_t re;

static int vid_ref_prevwidth, vid_ref_prevheight;
static bool vid_ref_verbose;
static bool vid_ref_sound_restart;
static bool vid_app_active;
static bool vid_app_minimized;

static mempool_t *vid_ref_mempool = NULL;

/*
** VID_Restart_f
*
* Console command to re-start the video mode and refresh DLL. We do this
* simply by setting the vid_ref_modified variable, which will
* cause the entire video mode and refresh DLL to be reset on the next frame.
*/
void VID_Restart( bool verbose, bool soundRestart ) {
	vid_ref_verbose = verbose;
	vid_ref_sound_restart = soundRestart;
}

void VID_Restart_f( void ) {
	VID_Restart( ( Cmd_Argc() >= 2 ? true : false ), false );
}

unsigned int vid_num_modes;
vidmode_t *vid_modes;

/*
** VID_GetModeInfo
*/
bool VID_GetModeInfo( int *width, int *height, unsigned int mode ) {
	if( mode >= vid_num_modes ) {
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

	for( i = 0; i < vid_num_modes; i++ )
		Com_Printf( "* %ix%i\n", vid_modes[i].width, vid_modes[i].height );
}

/*
** VID_AppActivate
*/
void VID_AppActivate( bool active, bool minimize, bool destroy ) {
	vid_app_active = active;
	vid_app_minimized = minimize;
	re.AppActivate( active, minimize, destroy );
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

// this is shit and should not exist but i am sick of working on this
void Retarded_SetWindowSize( int w, int h ) {
	viddef.width = w;
	viddef.height = h;
	glConfig.width = w;
	glConfig.height = h;
}

static bool VID_ChangeMode() {
	VideoMode prev_mode = VID_GetCurrentVideoMode();

	VideoMode new_mode;
	new_mode.width = vid_width->integer;
	new_mode.height = vid_height->integer;
	new_mode.frequency = vid_displayfrequency->integer;
	new_mode.fullscreen = FullScreenMode_Windowed;
	if( vid_fullscreen->integer != 0 )
		new_mode.fullscreen = vid_borderless->integer == 0 ? FullScreenMode_Fullscreen : FullScreenMode_FullscreenBorderless;

	bool ok = VID_SetVideoMode( new_mode );
	if( ok )
		return true;

	ok = VID_SetVideoMode( prev_mode );
	if( !ok )
		VID_SetVideoMode( VID_GetDefaultVideoMode() );

	return false;
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

static struct cinematics_s *VID_RefModule_CIN_Open( const char *name, int64_t start_time, bool *yuv, float *framerate ) {
	return CIN_Open( name, start_time, CIN_LOOP, yuv, framerate );
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

	import.Com_LoadSysLibrary = Com_LoadSysLibrary;
	import.Com_UnloadLibrary = Com_UnloadLibrary;
	import.Com_LibraryProcAddress = Com_LibraryProcAddress;

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
	import.FS_MediaDirectory = &FS_MediaDirectory;
	import.FS_AddFileToMedia = &FS_AddFileToMedia;

	import.CIN_Open = &VID_RefModule_CIN_Open;
	import.CIN_NeedNextFrame = &CIN_NeedNextFrame;
	import.CIN_ReadNextFrame = &CIN_ReadNextFrame;
	import.CIN_ReadNextFrameYUV = &CIN_ReadNextFrameYUV;
	import.CIN_Reset = &CIN_Reset;
	import.CIN_Close = &CIN_Close;

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

	vid_modes = Mem_ZoneMalloc( n * sizeof( vidmode_t ) );

	for( int i = 0; i < n; i++ ) {
		VideoMode mode = VID_GetVideoMode( i );
		vid_modes[ i ].width = mode.width;
		vid_modes[ i ].height = mode.height;
	}

	qsort( vid_modes, n, sizeof( vidmode_t ), ( int ( * )( const void *, const void * ) )VID_CompareModes );

	// Remove duplicate modes in case the sys code failed to do so.
	vid_num_modes = 0;
	int prevWidth = 0, prevHeight = 0;
	for( int i = 0; i < n; i++ ) {
		int width = vid_modes[i].width, height = vid_modes[i].height;
		if( ( width == prevWidth ) && ( height == prevHeight ) ) {
			continue;
		}

		vid_modes[vid_num_modes++] = vid_modes[i];
		prevWidth = width;
		prevHeight = height;
	}
}

#if 0

/*
** VID_CheckChanges
*
* This function gets called once just before drawing each frame, and its sole purpose in life
* is to check to see if any of the video mode parameters have changed, and if they have to
* update the rendering DLL and/or video mode to match.
*/
void VID_CheckChanges( void ) {
	rserr_t err;
	bool vid_ref_was_active = vid_ref_active;
	bool verbose = vid_ref_verbose || vid_ref_sound_restart;

	if( vid_fullscreen->modified ) {
		if( vid_ref_active ) {
			// try to change video mode without vid_restart
			err = VID_ChangeMode();

			if( err == rserr_restart_required ) {
				vid_ref_modified = true;
			}
		}

		vid_fullscreen->modified = false;
	}

	if( vid_ref->modified ) {
		vid_ref_modified = true;
		vid_ref->modified = false;
	}

	if( vid_ref_modified ) {
		bool cgameActive;
		char num[16];

		cgameActive = cls.cgameActive;
		cls.disable_screen = 1;

		CL_ShutdownMedia();

		// stop and free all sounds
		CL_SoundModule_Shutdown( false );

		FTLIB_FreeFonts( false );

		L10n_ClearDomains();

		Cvar_GetLatchedVars( CVAR_LATCH_VIDEO );

load_refresh:
		if( !VID_LoadRefresh() ) {
			// reset to default
			if( !Q_stricmp( vid_ref->string, VID_DEFAULTREF ) ) {
				Sys_Error( "Failed to load default refresh DLL" );
			}
			Cvar_ForceSet( vid_ref->name, VID_DEFAULTREF );
			vid_ref->modified = false;
			goto load_refresh;
		}

		// handle vid size changes
		if( ( vid_width->integer <= 0 ) || ( vid_height->integer <= 0 ) ) {
			// set the mode to the default
			int w, h;
			if( !VID_GetDefaultMode( &w, &h ) ) {
				w = vid_modes[0].width;
				h = vid_modes[0].height;
			}
			Q_snprintfz( num, sizeof( num ), "%i", w );
			Cvar_ForceSet( vid_width->name, num );
			Q_snprintfz( num, sizeof( num ), "%i", h );
			Cvar_ForceSet( vid_height->name, num );
		}

		if( vid_width->integer > vid_modes[vid_num_modes - 1].width ) { // modes are sorted by width
			Q_snprintfz( num, sizeof( num ), "%i", vid_modes[vid_num_modes - 1].width );
			Cvar_ForceSet( vid_width->name, num );
		}

		{
			int maxh = 0;
			unsigned int i;
			for( i = 0; i < vid_num_modes; i++ ) {
				clamp_low( maxh, vid_modes[i].height );
			}
			if( vid_height->integer > maxh ) {
				Q_snprintfz( num, sizeof( num ), "%i", maxh );
				Cvar_ForceSet( vid_height->name, num );
			}
		}

		if( vid_fullscreen->integer ) {
			// snap to the closest fullscreen resolution, width has priority over height
			int tw = vid_width->integer, th = vid_height->integer, w = vid_modes[0].width, h;
			int minwdiff = abs( w - tw ), minhdiff;
			unsigned int i, hfirst = 0;

			if( minwdiff ) {
				for( i = 1; i < vid_num_modes; i++ ) {
					const vidmode_t *mode = &( vid_modes[i] );
					int diff = abs( mode->width - tw );
					// select the bigger mode if the diff from the smaller and the larger is equal - use < for the smaller one
					if( diff <= minwdiff ) {
						if( mode->width != w ) { // don't advance hfirst when searching for the larger mode
							hfirst = i;
							w = mode->width;
						}
						minwdiff = diff;
					}
					if( !diff || ( diff > minwdiff ) ) {
						break;
					}
				}
			}

			h = vid_modes[hfirst].height;
			minhdiff = abs( h - th );
			if( minhdiff ) {
				for( i = hfirst + 1; i < vid_num_modes; i++ ) {
					const vidmode_t *mode = &( vid_modes[i] );
					int diff;
					if( mode->width != w ) {
						break;
					}
					diff = abs( mode->height - th );
					if( diff <= minhdiff ) {
						h = mode->height;
						minhdiff = diff;
					}
					if( !diff || ( diff > minhdiff ) ) {
						break;
					}
				}
			}

			if( minwdiff ) {
				Q_snprintfz( num, sizeof( num ), "%i", w );
				Cvar_ForceSet( vid_width->name, num );
			}
			if( minhdiff ) {
				Q_snprintfz( num, sizeof( num ), "%i", h );
				Cvar_ForceSet( vid_height->name, num );
			}
		}

		return re.Init( applicationName, screenshotsPrefix, startupColor, 0, iconXPM, verbose );
		err = VID_Sys_Init_( STR_TO_POINTER( vid_parentwid->string ), vid_ref_verbose );
		if( err != rserr_ok ) {
			Sys_Error( "VID_Init() failed with code %i", err );
		}

		err = VID_ChangeMode();
		if( err != rserr_ok ) {
			Sys_Error( "VID_ChangeMode() failed with code %i", err );
		}

		// stop and free all sounds
		CL_SoundModule_Init( verbose );

		RF_BeginRegistration();
		CL_SoundModule_BeginRegistration();

		FTLIB_PrecacheFonts( verbose );

		// load common localization strings
		L10n_LoadLangPOFile( "common", "l10n" );

		if( vid_ref_was_active ) {
			IN_Restart();
		}

		CL_InitMedia();

		cls.disable_screen = 0;

		Con_Close();

		if( cgameActive ) {
			CL_GameModule_Init();
			Con_Close();
			CL_UIModule_ForceMenuOff();
			CL_SetKeyDest( key_game );

			// this will precache game assets
			SCR_UpdateScreen();
		} else {
			CL_UIModule_ForceMenuOn();
			CL_SetKeyDest( key_menu );
		}

		re.EndRegistration();
		CL_SoundModule_EndRegistration();

		vid_ref_modified = false;
		vid_ref_verbose = true;
	}
}

#endif

void VID_Init() {
	VID_InitModes();

	/* Create the video variables so we know how to start the graphics drivers */
	vid_width = Cvar_Get( "vid_width", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_height = Cvar_Get( "vid_height", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_borderless = Cvar_Get( "vid_borderless", "1", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_displayfrequency = Cvar_Get( "vid_displayfrequency", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_parentwid = Cvar_Get( "vid_parentwid", "0", CVAR_NOSET );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand( "vid_restart", VID_Restart_f );
	Cmd_AddCommand( "vid_modelist", VID_ModeList_f );

	/* Start the graphics mode and load refresh DLL */
	vid_ref_verbose = true;
	vid_ref_sound_restart = false;

	Cvar_GetLatchedVars( CVAR_LATCH_VIDEO );

	// if they haven't set vid_width/vid_height set them to the default
	if( vid_width->integer <= 0 || vid_height->integer <= 0 ) {
		VideoMode def = VID_GetDefaultVideoMode();

		char buf[ 16 ];
		Q_snprintfz( buf, sizeof( buf ), "%i", def.width );
		Cvar_ForceSet( vid_width->name, buf );
		Q_snprintfz( buf, sizeof( buf ), "%i", def.height );
		Cvar_ForceSet( vid_height->name, buf );
	}

	// TODO: this code tries to pick a video mode

	/* if( vid_width->integer > vid_modes[vid_num_modes - 1].width ) { // modes are sorted by width */
	/* 	Q_snprintfz( num, sizeof( num ), "%i", vid_modes[vid_num_modes - 1].width ); */
	/* 	Cvar_ForceSet( vid_width->name, num ); */
	/* } */
        /*  */
	/* { */
	/* 	int maxh = 0; */
	/* 	unsigned int i; */
	/* 	for( i = 0; i < vid_num_modes; i++ ) { */
	/* 		clamp_low( maxh, vid_modes[i].height ); */
	/* 	} */
	/* 	if( vid_height->integer > maxh ) { */
	/* 		Q_snprintfz( num, sizeof( num ), "%i", maxh ); */
	/* 		Cvar_ForceSet( vid_height->name, num ); */
	/* 	} */
	/* } */

	/* if( vid_fullscreen->integer ) { */
	/* 	// snap to the closest fullscreen resolution, width has priority over height */
	/* 	int tw = vid_width->integer, th = vid_height->integer, w = vid_modes[0].width, h; */
	/* 	int minwdiff = abs( w - tw ), minhdiff; */
	/* 	unsigned int i, hfirst = 0; */
        /*  */
	/* 	if( minwdiff ) { */
	/* 		for( i = 1; i < vid_num_modes; i++ ) { */
	/* 			const vidmode_t *mode = &( vid_modes[i] ); */
	/* 			int diff = abs( mode->width - tw ); */
	/* 			// select the bigger mode if the diff from the smaller and the larger is equal - use < for the smaller one */
	/* 			if( diff <= minwdiff ) { */
	/* 				if( mode->width != w ) { // don't advance hfirst when searching for the larger mode */
	/* 					hfirst = i; */
	/* 					w = mode->width; */
	/* 				} */
	/* 				minwdiff = diff; */
	/* 			} */
	/* 			if( !diff || ( diff > minwdiff ) ) { */
	/* 				break; */
	/* 			} */
	/* 		} */
	/* 	} */
        /*  */
	/* 	h = vid_modes[hfirst].height; */
	/* 	minhdiff = abs( h - th ); */
	/* 	if( minhdiff ) { */
	/* 		for( i = hfirst + 1; i < vid_num_modes; i++ ) { */
	/* 			const vidmode_t *mode = &( vid_modes[i] ); */
	/* 			int diff; */
	/* 			if( mode->width != w ) { */
	/* 				break; */
	/* 			} */
	/* 			diff = abs( mode->height - th ); */
	/* 			if( diff <= minhdiff ) { */
	/* 				h = mode->height; */
	/* 				minhdiff = diff; */
	/* 			} */
	/* 			if( !diff || ( diff > minhdiff ) ) { */
	/* 				break; */
	/* 			} */
	/* 		} */
	/* 	} */
        /*  */
	/* 	if( minwdiff ) { */
	/* 		Q_snprintfz( num, sizeof( num ), "%i", w ); */
	/* 		Cvar_ForceSet( vid_width->name, num ); */
	/* 	} */
	/* 	if( minhdiff ) { */
	/* 		Q_snprintfz( num, sizeof( num ), "%i", h ); */
	/* 		Cvar_ForceSet( vid_height->name, num ); */
	/* 	} */
	/* } */

	VideoMode mode; // TODO!!!
	mode.width = 800;
	mode.height = 600;
	mode.frequency = 144;
	mode.fullscreen = FullScreenMode_Windowed;

	VID_WindowInit( mode, 8 );

	if( !VID_LoadRefresh() ) {
		Sys_Error( "Failed to load renderer" );
	}

	rserr_t err = R_Init( vid_ref_verbose );
	if( err != rserr_ok ) {
		Sys_Error( "VID_Init() failed with code %i", err );
	}

	CL_SoundModule_Init( vid_ref_sound_restart );

	RF_BeginRegistration();
	CL_SoundModule_BeginRegistration();

	FTLIB_LoadLibrary( false );
	FTLIB_PrecacheFonts( vid_ref_verbose );

	L10n_LoadLangPOFile( "common", "l10n" );

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
	CL_SoundModule_EndRegistration();

	vid_ref_verbose = true;
}

void VID_Shutdown() {
	CL_ShutdownMedia();

	// stop and free all sounds
	CL_SoundModule_Shutdown( false );

	FTLIB_FreeFonts( false );

	L10n_ClearDomains();

	RF_Shutdown( false );
	Mem_FreePool( &vid_ref_mempool );

	FTLIB_UnloadLibrary( false );

	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "vid_modelist" );

	Mem_ZoneFree( vid_modes );
}
