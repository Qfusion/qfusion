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

#include "client.h"
#include "cin.h"

#define SCR_CinematicTime() CL_SoundModule_GetRawSamplesTime()


/*
=================================================================

ROQ PLAYING

=================================================================
*/

/*
* SCR_StopCinematic
*/
void SCR_StopCinematic( void )
{
	if( !cl.cin.h )
		return;

	CIN_Close( cl.cin.h );
	memset( &cl.cin, 0, sizeof( cl.cin ) );
}

/*
* SCR_FinishCinematic
*
* Called when either the cinematic completes, or it is aborted
*/
void SCR_FinishCinematic( void )
{
	CL_Disconnect( NULL );
}

//==========================================================================

/*
* SCR_ReadNextCinematicFrame
*/
static void SCR_ReadNextCinematicFrame( void )
{
	cl.cin.pic = CIN_ReadNextFrame( cl.cin.h, 
		&cl.cin.width, &cl.cin.height, 
		&cl.cin.aspect_numerator, &cl.cin.aspect_denominator,
		&cl.cin.redraw );
}

/*
* SCR_AllowCinematicConsole
*/
qboolean SCR_AllowCinematicConsole( void )
{
	return cl.cin.allowConsole;
}

/*
* SCR_RunCinematic
*/
void SCR_RunCinematic( void )
{
	if( cls.state != CA_CINEMATIC ) {
		return;
	}

	if( ( cls.key_dest != key_game && cls.key_dest != key_console )
		|| (cls.key_dest == key_console && !SCR_AllowCinematicConsole()) ) {
		// stop if menu or console is up
		SCR_FinishCinematic();
		return;
	}

	cl.cin.absPrevTime = cl.cin.absCurrentTime;
	cl.cin.absCurrentTime = SCR_CinematicTime();

	if( cl.cin.paused ) {
		return;
	}

	cl.cin.currentTime += cl.cin.absCurrentTime - cl.cin.absPrevTime;

	if( !CIN_NeedNextFrame( cl.cin.h, cl.cin.currentTime ) ) {
		cl.cin.redraw = qfalse;
		return;
	}

	// read next frame
	SCR_ReadNextCinematicFrame();
	if( !cl.cin.pic ) {
		// end of cinematic
		SCR_FinishCinematic();
		return;
	}
}

/*
* SCR_DrawCinematic
*
* Returns true if a cinematic is active, meaning the view rendering should be skipped
*/
qboolean SCR_DrawCinematic( void )
{
	int x, y;
	int w, h;
	qboolean keepRatio;

	if( cls.state != CA_CINEMATIC )
		return qfalse;

	if( !cl.cin.pic )
		return qtrue;

	keepRatio = cl.cin.keepRatio && cl.cin.aspect_numerator!=0 && cl.cin.aspect_denominator!=0;

	// check whether we should keep video aspect ratio upon user's request
	// and make sure that video's aspect ration is really different from that of the screen
	// a/b == c/d => a * d == c * b
	if( keepRatio && (viddef.width * cl.cin.aspect_numerator != viddef.height * cl.cin.aspect_denominator) )
	{
		float aspect;

		// apply the aspect ratio
		aspect = ((float)cl.cin.aspect_numerator * cl.cin.width) / ((float)cl.cin.aspect_denominator * cl.cin.height);

		// shrink either of the dimensions, also keeping it even
		if( aspect > 1 )
		{
			w = viddef.width;
			h = ((int)( (float)viddef.width / aspect ) + 1) & ~1;

			clamp( h, 0, (int)viddef.height );

			x = 0;
			y = Q_rint( (viddef.height - h) * 0.5 );
		}
		else
		{
			// this branch is untested but hopefully works

			w = ((int)( (float)viddef.height * aspect ) + 1) & ~1;
			h = viddef.height;

			clamp( w, 0, (int)viddef.width );

			x = Q_rint( (viddef.width  - w) * 0.5 );
			y = 0;
		}
	}
	else
	{
		// fullscreen (stretches video in case of non-matching aspect ratios)
		w = viddef.width;
		h = viddef.height;
		x = 0;
		y = 0;
	}

	R_DrawStretchRaw( x, y, w, h, cl.cin.width, cl.cin.height, cl.cin.pic );

	cl.cin.redraw = qfalse;
	return qtrue;
}

/*
* SCR_PlayCinematic
*/
static void SCR_PlayCinematic( const char *arg, int flags )
{
	struct cinematics_s *cin;

	CL_SoundModule_Clear();

	cin = CIN_Open( arg, 0, 0 );
	if( !cin )
	{
		Com_Printf( "SCR_PlayCinematic: couldn't find %s\n", arg );
		return;
	}

	CL_Disconnect( NULL );

	cl.cin.h = cin;
	cl.cin.keepRatio = (flags & 1) ? qfalse : qtrue;
	cl.cin.allowConsole = (flags & 2) ? qfalse : qtrue;
	cl.cin.paused = qfalse;
	cl.cin.absStartTime = cl.cin.absCurrentTime = cl.cin.absPrevTime = SCR_CinematicTime();
	cl.cin.currentTime = 0;

	CL_SetClientState( CA_CINEMATIC );

	CL_SoundModule_StopAllSounds();

	SCR_EndLoadingPlaque();

	SCR_ReadNextCinematicFrame();
}

/*
* SCR_PauseCinematic
*/
void SCR_PauseCinematic( void )
{
	if( cls.state != CA_CINEMATIC ) {
		return;
	}

	cl.cin.paused = !cl.cin.paused;
	if( cl.cin.paused ) {
		CL_SoundModule_Clear();
	}
}

// =======================================================================

/*
* CL_PlayCinematic_f
*/
void CL_PlayCinematic_f( void )
{
	if( Cmd_Argc() < 1 )
	{
		Com_Printf( "Usage: %s <name> [flags]\n", Cmd_Argv( 0 ) );
		return;
	}

	SCR_PlayCinematic( Cmd_Argv( 1 ), atoi( Cmd_Argv( 2 ) ) );
}

/*
* CL_PauseCinematic_f
*/
void CL_PauseCinematic_f( void )
{
	if( cls.state != CA_CINEMATIC )
	{
		Com_Printf( "Usage: %s\n", Cmd_Argv( 0 ) );
		return;
	}

	SCR_PauseCinematic();
}

/*
* CL_InitCinematics
*/
void CL_InitCinematics( void )
{
	CIN_LoadLibrary( qtrue );

	Cmd_AddCommand( "cinematic", CL_PlayCinematic_f );
	Cmd_AddCommand( "cinepause", CL_PauseCinematic_f );
}

/*
* CL_ShutdownCinematics
*/
void CL_ShutdownCinematics( void )
{
	Cmd_RemoveCommand( "cinematic" );
	Cmd_RemoveCommand( "cinepause" );

	CIN_UnloadLibrary( qtrue );
}
