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

cvar_t *vid_ref;
cvar_t *vid_width, *vid_height;
cvar_t *vid_xpos;          // X coordinate of window position
cvar_t *vid_ypos;          // Y coordinate of window position
cvar_t *vid_fullscreen;
cvar_t *vid_borderless;
cvar_t *vid_multiscreen_head;
cvar_t *win_noalttab;
cvar_t *win_nowinkeys;

// Global variables used internally by this module
viddef_t viddef;             // global video state; used by other modules

ref_export_t re;

#define VID_DEFAULTREF          "ref_gl"

typedef rserr_t (*vid_init_t)( int, int, int, int, int, void *, bool );

static int vid_ref_prevwidth, vid_ref_prevheight;
static bool vid_ref_modified;
static bool vid_ref_verbose;
static bool vid_ref_sound_restart;
static bool vid_ref_active;
static bool vid_initialized;
static bool vid_app_active;
static bool vid_app_minimized;
static bool vid_ref_changing;
static int  vid_ref_req_width, vid_ref_req_height;

static void     *vid_ref_libhandle = NULL;
static mempool_t *vid_ref_mempool = NULL;

// These are system specific functions
// wrapper around R_Init
rserr_t VID_Sys_Init(
	const char *applicationName, const char *screenshotsPrefix, int startupColor, const int *iconXPM, bool verbose );
void VID_UpdateWindowPosAndSize( int x, int y );
void VID_EnableAltTab( bool enable );
void VID_EnableWinKeys( bool enable );

/*
** VID_Restart_f
*
* Console command to re-start the video mode and refresh DLL. We do this
* simply by setting the vid_ref_modified variable, which will
* cause the entire video mode and refresh DLL to be reset on the next frame.
*/
void VID_Restart( bool verbose, bool soundRestart ) {
	vid_ref_changing = false;
	vid_ref_modified = true;
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
** VID_NewWindow
*/
static void VID_NewWindow( int width, int height ) {
	viddef.width  = width;
	viddef.height = height;
}

static rserr_t VID_Sys_Init_( bool verbose ) {
	rserr_t res;
#include APP_XPM_ICON
	int *xpm_icon;

	xpm_icon = XPM_ParseIcon( sizeof( app256x256_xpm ) / sizeof( app256x256_xpm[0] ), app256x256_xpm );

	res = VID_Sys_Init( APPLICATION_UTF8, APP_SCREENSHOTS_PREFIX, APP_STARTUP_COLOR, xpm_icon, verbose );

	free( xpm_icon );

	return res;
}

/*
** VID_AppActivate
*/
void VID_AppActivate( bool active, bool minimize, bool destroy ) {
	vid_app_active = active;
	vid_app_minimized = minimize;
	
	Com_SetAppActive( vid_app_active );

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
** VID_RefreshIsActive
*/
bool VID_RefreshIsActive( void ) {
	return vid_ref_active;
}

/*
** VID_GetWindowWidth
*/
int VID_GetWindowWidth( void ) {
	return vid_ref_changing ? vid_ref_req_width : viddef.width;
}

/*
** VID_GetWindowHeight
*/
int VID_GetWindowHeight( void ) {
	return vid_ref_changing ? vid_ref_req_height : viddef.height;
}

/*
** VID_ChangeMode_
*/
static rserr_t VID_ChangeMode_( void ) {
	int x, y;
	int w, h;
	bool fs;
	bool borderless;
	rserr_t err;
	bool stereo;

	vid_fullscreen->modified = false;

	borderless = vid_borderless->integer != 0;
	stereo = Cvar_Value( "cl_stereo" ) != 0;
	fs = vid_fullscreen->integer != 0;
	if( borderless && fs ) {
		x = 0;
		y = 0;
		if( !VID_GetDefaultMode( &w, &h ) ) {
			w = vid_modes[0].width;
			h = vid_modes[0].height;
		}
	} else {
		x = vid_xpos->integer;
		y = vid_ypos->integer;
		w = vid_width->integer;
		h = vid_height->integer;
	}

	vid_ref_req_width = w;
	vid_ref_req_height = h;

	if( vid_ref_active && ( w != (int)viddef.width || h != (int)viddef.height ) ) {
		return rserr_restart_required;
	}

	err = re.SetMode( x, y, w, h, fs, stereo, borderless );

	if( err == rserr_ok ) {
		// store fallback mode
		vid_ref_prevwidth = w;
		vid_ref_prevheight = h;
	} else if( err == rserr_restart_required ) {
		return err;
	} else {
		/* Try to recover from all possible kinds of mode-related failures.
		 *
		 * rserr_invalid_fullscreen may be returned only if fullscreen is requested, but at this
		 * point the system may not be totally sure whether the requested mode is windowed-only
		 * or totally unsupported, so there's a possibility of rserr_invalid_mode as well.
		 *
		 * However, the previously working mode may be windowed-only, but the user may request
		 * fullscreen, so this case is handled too.
		 *
		 * In the end, in the worst case, the windowed safe mode will be selected, and the system
		 * should not return rserr_invalid_fullscreen or rserr_invalid_mode anymore.
		 */

		if( err == rserr_invalid_fullscreen ) {
			Com_Printf( "VID_ChangeMode() - fullscreen unavailable in this mode\n" );

			Cvar_ForceSet( vid_fullscreen->name, "0" );
			vid_fullscreen->modified = false;
			fs = false;

			err = re.SetMode( x, y, w, h, false, stereo, borderless );
		}

		if( err == rserr_invalid_mode ) {
			Com_Printf( "VID_ChangeMode() - invalid mode\n" );

			w = vid_ref_prevwidth;
			h = vid_ref_prevheight;
			Cvar_ForceSet( vid_width->name, va( "%i", w ) );
			Cvar_ForceSet( vid_height->name, va( "%i", h ) );

			// try setting it back to something safe
			err = re.SetMode( x, y, w, h, fs, stereo, borderless );
			if( err == rserr_invalid_fullscreen ) {
				Com_Printf( "VID_ChangeMode() - could not revert to safe fullscreen mode\n" );

				Cvar_ForceSet( vid_fullscreen->name, "0" );
				vid_fullscreen->modified = false;
				fs = false;

				err = re.SetMode( x, y, w, h, false, stereo, borderless );
			}
			if( err != rserr_ok ) {
				Com_Printf( "VID_ChangeMode() - could not revert to safe mode\n" );
			}
		}
	}

	if( err == rserr_ok ) {
		// let the sound and input subsystems know about the new window
		VID_NewWindow( w, h );
	}

	return err;
}

/*
** VID_ChangeMode
*/
static rserr_t VID_ChangeMode( void ) {
	rserr_t err;

	vid_ref_changing = true;
	err = VID_ChangeMode_();
	vid_ref_changing = false;
	return err;
}

/*
** VID_UnloadRefresh
*/
static void VID_UnloadRefresh( void ) {
	if( vid_ref_libhandle ) {
		if( vid_ref_active ) {
			re.Shutdown( false );
			vid_ref_active = false;
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

static struct cinematics_s *VID_RefModule_CIN_Open( const char *name, int64_t start_time, bool *yuv, float *framerate ) {
	return CIN_Open( name, start_time, CIN_LOOP, yuv, framerate );
}

/*
** VID_LoadRefresh
*/
static bool VID_LoadRefresh( const char *name ) {
	static ref_import_t import;
	size_t file_size;
	char *file;
	dllfunc_t funcs[2];
	GetRefAPI_t GetRefAPI_f;

	VID_UnloadRefresh();

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

	import.BufPipe_Create = QBufPipe_Create;
	import.BufPipe_Destroy = QBufPipe_Destroy;
	import.BufPipe_Finish = QBufPipe_Finish;
	import.BufPipe_WriteCmd = QBufPipe_WriteCmd;
	import.BufPipe_ReadCmds = QBufPipe_ReadCmds;
	import.BufPipe_Wait = QBufPipe_Wait;

	file_size = strlen( LIB_DIRECTORY "/" LIB_PREFIX ) + strlen( name ) + strlen( LIB_SUFFIX ) + 1;
	file = Mem_TempMalloc( file_size );
	Q_snprintfz( file, file_size, LIB_DIRECTORY "/" LIB_PREFIX "%s" LIB_SUFFIX, name );

	// load dynamic library
	Com_Printf( "Loading refresh module %s... ", name );
	funcs[0].name = "GetRefAPI";
	funcs[0].funcPointer = (void **) &GetRefAPI_f;
	funcs[1].name = NULL;
	vid_ref_libhandle = Com_LoadLibrary( file, funcs );

	Mem_TempFree( file );

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
			return false;
		}
	} else {
		Com_Printf( "Not found %s.\n", va( LIB_DIRECTORY "/" LIB_PREFIX "%s" LIB_SUFFIX, name ) );
		return false;
	}

	Com_Printf( "\n" );
	return true;
}

/*
** VID_CheckChanges
*
* This function gets called once just before drawing each frame, and its sole purpose in life
* is to check to see if any of the video mode parameters have changed, and if they have to
* update the rendering DLL and/or video mode to match.
*/
void VID_CheckChanges( void ) {
	rserr_t err;
	bool verbose = vid_ref_verbose || vid_ref_sound_restart;

	if( win_noalttab->modified ) {
		VID_EnableAltTab( win_noalttab->integer ? false : true );
		win_noalttab->modified = false;
	}

	if( win_nowinkeys->modified ) {
		VID_EnableWinKeys( win_nowinkeys->integer ? false : true );
		win_nowinkeys->modified = false;
	}

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
		if( !VID_LoadRefresh( vid_ref->string ) ) {
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

		err = VID_Sys_Init_( vid_ref_verbose );
		if( err != rserr_ok ) {
			Sys_Error( "VID_Init() failed with code %i", err );
		}

		err = VID_ChangeMode();
		if( err != rserr_ok ) {
			Sys_Error( "VID_ChangeMode() failed with code %i", err );
		}

		vid_ref_active = true;

		FTLIB_PrecacheFonts( verbose );

		// load common localization strings
		L10n_LoadLangPOFile( "common", "l10n" );

		// stop and free all sounds
		CL_SoundModule_Init( verbose );

		if( cls.state != CA_UNINITIALIZED ) {
			re.BeginRegistration();
			CL_SoundModule_BeginRegistration();

			IN_Restart();

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
		}

		vid_ref_modified = false;
		vid_ref_verbose = true;
	}

	/*
	** update our window position
	*/
	if( !vid_fullscreen->integer ) {
		if( vid_xpos->modified || vid_ypos->modified ) {
			// make sure the titlebar is not completely hidden away
			if( vid_xpos->integer < 8 ) {
				Cvar_ForceSet( vid_xpos->name, "8" );
			}
			if( vid_ypos->integer < 8 ) {
				Cvar_ForceSet( vid_ypos->name, "8" );
			}

			VID_UpdateWindowPosAndSize( vid_xpos->integer, vid_ypos->integer );
	
			vid_xpos->modified = false;
			vid_ypos->modified = false;
		}
	}
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

/**
 * Initializes the list of video modes
 */
void VID_InitModes( void ) {
	unsigned int numModes, i;
	int prevWidth = 0, prevHeight = 0;

	numModes = VID_GetSysModes( vid_modes );
	if( !numModes ) {
		Sys_Error( "Failed to get video modes" );
	}

	vid_modes = Mem_ZoneMalloc( numModes * sizeof( vidmode_t ) );
	VID_GetSysModes( vid_modes );
	qsort( vid_modes, numModes, sizeof( vidmode_t ), ( int ( * )( const void *, const void * ) )VID_CompareModes );

	// Remove duplicate modes in case the sys code failed to do so.
	vid_num_modes = 0;
	for( i = 0; i < numModes; i++ ) {
		int width = vid_modes[i].width, height = vid_modes[i].height;
		if( ( width == prevWidth ) && ( height == prevHeight ) ) {
			continue;
		}

		vid_modes[vid_num_modes++] = vid_modes[i];
		prevWidth = width;
		prevHeight = height;
	}
}

/*
** VID_Init
*/
void VID_Init( void ) {
	if( vid_initialized ) {
		return;
	}

	VID_InitModes();

	// Create the video variables so we know how to start the graphics drivers
	vid_ref = Cvar_Get( "vid_ref", VID_DEFAULTREF, CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_width = Cvar_Get( "vid_width", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_height = Cvar_Get( "vid_height", "0", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_xpos = Cvar_Get( "vid_xpos", "0", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get( "vid_ypos", "0", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "1", CVAR_ARCHIVE );
	vid_borderless = Cvar_Get( "vid_borderless", "1", CVAR_ARCHIVE | CVAR_LATCH_VIDEO );
	vid_multiscreen_head = Cvar_Get( "vid_multiscreen_head", "-1", CVAR_ARCHIVE );

	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );
	win_nowinkeys = Cvar_Get( "win_nowinkeys", "0", CVAR_ARCHIVE );

	// Add some console commands that we want to handle
	Cmd_AddCommand( "vid_restart", VID_Restart_f );
	Cmd_AddCommand( "vid_modelist", VID_ModeList_f );

	// make sure the titlebar is not completely hidden away
	if( vid_xpos->integer < 8 ) {
		Cvar_ForceSet( vid_xpos->name, "8" );
	}
	if( vid_ypos->integer < 8 ) {
		Cvar_ForceSet( vid_ypos->name, "8" );
	}

	// Start the graphics mode and load refresh DLL
	vid_ref_modified = true;
	vid_ref_active = false;
	vid_ref_verbose = true;
	vid_initialized = true;
	vid_ref_sound_restart = false;
	vid_fullscreen->modified = false;
	vid_borderless->modified = false;
	vid_xpos->modified = true;
	vid_ypos->modified = true;
	vid_ref_prevwidth = vid_modes[0].width; // the smallest mode is the "safe mode"
	vid_ref_prevheight = vid_modes[0].height;

	FTLIB_LoadLibrary( false );

	VID_CheckChanges();
}

/*
** VID_Shutdown
*/
void VID_Shutdown( void ) {
	if( !vid_initialized ) {
		return;
	}

	VID_UnloadRefresh();

	FTLIB_UnloadLibrary( false );

	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "vid_modelist" );

	Mem_ZoneFree( vid_modes );

	vid_initialized = false;
}
