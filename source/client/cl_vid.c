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

#define VID_DEFAULTMODE			"-2"
#define VID_DEFAULTFALLBACKMODE	"4"

#define VID_NUM_MODES (int)( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

typedef rserr_t (*vid_init_t)( int, int, int, int, qboolean, qboolean );

static int      vid_ref_prevmode;
static qboolean vid_ref_modified;
static qboolean vid_ref_verbose;
static qboolean vid_ref_sound_restart;
static qboolean vid_ref_active;
static qboolean vid_initialized;

// These are system specific functions
rserr_t VID_Sys_Init( int x, int y, int width, int height, qboolean fullscreen, qboolean wideScreen, qboolean verbose ); // wrapper around R_Init
rserr_t VID_SetMode( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen );
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
* Find the best matching video mode for given width and height
*/
static int VID_GetModeNum( int width, int height )
{
	int i;
	int dx, dy, dist;
	int best = -1, best_dist = 10000000;

	for( i = 0; i < VID_NUM_MODES; i++ )
	{
		dx = vid_modes[i].width - width;
		dy = vid_modes[i].height - height;

		dist = dx * dx + dy * dy;
		if( best < 0 || dist < best_dist )
		{
			best = i;
			best_dist = dist;
		}
	}

	return best;
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

static rserr_t VID_Sys_Init_( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen )
{
	return VID_Sys_Init( x, y, width, height, fullScreen, wideScreen, vid_ref_verbose );
}

/*
** VID_SetMode
*/
rserr_t VID_SetMode( int x, int y, int width, int height, qboolean fullScreen, qboolean wideScreen )
{
    return R_SetMode( x, y, width, height, fullScreen, wideScreen );
}

/*
** VID_ChangeMode
*/
static rserr_t VID_ChangeMode( vid_init_t vid_init )
{
	int x, y;
	int w, h;
	qboolean fs, ws;
	qboolean r;
	rserr_t err;

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
	r = VID_GetModeInfo( &w, &h, &ws, vid_mode->integer );

	if( !r ) {
		err = rserr_invalid_mode;
	}
	else {
		err = vid_init( x, y, w, h, fs, ws );
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

			if( ( err = vid_init( x, y, w, h, qfalse, ws ) ) == rserr_ok ) {
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
		if( ( err = vid_init( x, y, w, h, fs, ws ) ) != rserr_ok ) {
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
** VID_CheckChanges
*
* This function gets called once just before drawing each frame, and its sole purpose in life
* is to check to see if any of the video mode parameters have changed, and if they have to
* update the rendering DLL and/or video mode to match.
*/
void VID_CheckChanges( void )
{
	rserr_t err;

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

	if( vid_ref_modified ) {
		qboolean cgameActive;

		cgameActive = cls.cgameActive;
		cls.disable_screen = qtrue;

		CL_ShutdownMedia( qfalse );

		// stop and free all sounds
		CL_SoundModule_Shutdown( qfalse );

		if( vid_ref_active ) {
			R_Shutdown( qfalse );
			vid_ref_active = qfalse;
		}

		Cvar_GetLatchedVars( CVAR_LATCH_VIDEO );

		// handle vid_mode changes
		if( vid_mode->integer == -2 ) {
			int w, h;
			int mode = -1;

			if( VID_GetScreenSize( &w, &h ) ) {
				mode = VID_GetModeNum( w, h );
			}
			Com_Printf( "Mode %i detected\n", mode );

			if( mode < 0 ) {
				mode = vid_ref_prevmode;
			}
			Cvar_ForceSet( "vid_mode", va( "%i", mode ) );
		}

		if( vid_mode->integer < -1 || vid_mode->integer >= VID_NUM_MODES ) {
			Com_Printf( "Bad mode %i or custom resolution\n", vid_mode->integer );
			Cvar_ForceSet( "vid_fullscreen", "0" );
			Cvar_ForceSet( "vid_mode", VID_DEFAULTFALLBACKMODE );
		}

		err = VID_ChangeMode( &VID_Sys_Init_ );
		if( err != rserr_ok ) {
			Com_Error( ERR_FATAL, "VID_ChangeMode() failed with code %i", err );
		}

		// stop and free all sounds
		CL_SoundModule_Init( vid_ref_verbose || vid_ref_sound_restart );

		CL_InitMedia( vid_ref_verbose || vid_ref_sound_restart );

		cls.disable_screen = qfalse;

		Con_Close();

		if( cgameActive ) {
			CL_GameModule_Init();
			Con_Close();
			CL_UIModule_ForceMenuOff();
			CL_SetKeyDest( key_game );
		}
		else {
			Cbuf_ExecuteText( EXEC_NOW, "menu_force 1\n" );
			CL_SetKeyDest( key_menu );
		}

		vid_ref_active = qtrue;
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

	VID_CheckChanges();
}

/*
** VID_Shutdown
*/
void VID_Shutdown( void )
{
	if( !vid_initialized )
		return;

	if( vid_ref_active )
	{
		R_Shutdown( qtrue );
		vid_ref_active = qfalse;
	}

	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "vid_front" );
	Cmd_RemoveCommand( "vid_modelist" );

	vid_initialized = qfalse;
}
