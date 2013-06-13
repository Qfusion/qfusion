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
// vid_null.c -- null video driver to aid porting efforts
// this assumes that one of the refs is statically linked to the executable

#include "../client/client.h"

viddef_t viddef;             // global video state
qboolean vid_ref_active;
qboolean vid_ref_modified;

/*
   ==========================================================================

   DIRECT LINK GLUE

   ==========================================================================
 */

void VID_NewWindow( int width, int height )
{
	viddef.width = width;
	viddef.height = height;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int width, height;
	int mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240", 320, 240, 0 },
	{ "Mode 1: 400x300", 400, 300, 1 },
	{ "Mode 2: 512x384", 512, 384, 2 },
	{ "Mode 3: 640x480", 640, 480, 3 },
	{ "Mode 4: 800x600", 800, 600, 4 },
	{ "Mode 5: 960x720", 960, 720, 5 },
	{ "Mode 6: 1024x768", 1024, 768, 6 },
	{ "Mode 7: 1152x864", 1152, 864, 7 },
	{ "Mode 8: 1280x800", 1280, 800, 8 },
	{ "Mode 9: 1280x960", 1280, 960, 9 },
	{ "Mode 10: 1280x1024", 1280, 1024, 10 },
	{ "Mode 11: 1600x1200", 1600, 1200, 11 },
	{ "Mode 12: 2048x1536", 2048, 1536, 12 }
};
#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if( mode < 0 || mode >= VID_NUM_MODES )
		return qfalse;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return qtrue;
}

static void VID_Restart_f( void )
{
	VID_Restart( (Cmd_Argc() >= 2 ? qtrue : qfalse) );
}

void VID_Init( void )
{
	viddef.width = 320;
	viddef.height = 240;

	Cmd_AddCommand( "vid_restart", VID_Restart_f );

	vid_ref_active = qfalse;
	vid_ref_modified = qtrue;

	VID_CheckChanges();
}

void VID_Shutdown( void )
{
	if( vid_ref_active )
	{
		R_Shutdown();
		vid_ref_active = qfalse;
	}

	Cmd_RemoveCommand( "vid_restart" );
}

void VID_CheckChanges( void )
{
	if( vid_ref_modified )
	{
		if( vid_ref_active )
		{
			R_Shutdown();
			vid_ref_active = qfalse;
		}

		if( R_Init( NULL, NULL, NULL, 0, 0, 0, 0, qfalse ) == -1 )
			Com_Error( ERR_FATAL, "Couldn't start refresh" );

		vid_ref_active = qtrue;
		vid_ref_modified = qfalse;
	}
}

void VID_Restart( void )
{
	vid_ref_modified = qtrue;
}
