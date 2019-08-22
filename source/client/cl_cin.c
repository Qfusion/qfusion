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

#define SCR_CinematicTime() Sys_Milliseconds()


/*
=================================================================

ROQ PLAYING

=================================================================
*/

/*
* SCR_StopCinematic
*/
void SCR_StopCinematic( void ) {
	if( !cl.cin.h ) {
		return;
	}

	CIN_Close( cl.cin.h );
	memset( &cl.cin, 0, sizeof( cl.cin ) );
}

/*
* SCR_FinishCinematic
*
* Called when either the cinematic completes, or it is aborted
*/
void SCR_FinishCinematic( void ) {
	SCR_PauseCinematic( false );
	CL_Disconnect( NULL );
}

//==========================================================================

/*
* SCR_CinematicRawSamples
*/
static void SCR_CinematicRawSamples( void *unused, unsigned int samples,
									 unsigned int rate, unsigned short width, unsigned short channels, const uint8_t *data ) {
	(void)unused;

	CL_SoundModule_RawSamples( samples, rate, width, channels, data, true );
}

/*
* SCR_CinematicGetRawSamplesLength
*/
static unsigned int SCR_CinematicGetRawSamplesLength( void *unused ) {
	(void)unused;

	return CL_SoundModule_GetRawSamplesLength();
}

/*
* SCR_ReadNextCinematicFrame
*/
static void SCR_ReadNextCinematicFrame( void ) {
	if( cl.cin.yuv ) {
		cl.cin.cyuv = CIN_ReadNextFrameYUV( cl.cin.h,
											&cl.cin.width, &cl.cin.height,
											&cl.cin.aspect_numerator, &cl.cin.aspect_denominator,
											&cl.cin.redraw );

		// so that various cl.cin.pic == NULL checks still make sense
		cl.cin.pic = ( uint8_t * )cl.cin.cyuv;
	} else {
		cl.cin.pic = CIN_ReadNextFrame( cl.cin.h,
										&cl.cin.width, &cl.cin.height,
										&cl.cin.aspect_numerator, &cl.cin.aspect_denominator,
										&cl.cin.redraw );
	}
}

/*
* SCR_AllowCinematicConsole
*/
bool SCR_AllowCinematicConsole( void ) {
	return cl.cin.allowConsole;
}

/*
* SCR_CinematicFramerate
*/
float SCR_CinematicFramerate( void ) {
	return cl.cin.framerate;
}

/*
* SCR_RunCinematic
*/
void SCR_RunCinematic( void ) {
	if( cls.state != CA_CINEMATIC ) {
		return;
	}

	if( ( cls.key_dest != key_game && cls.key_dest != key_console )
		|| ( cls.key_dest == key_console && !SCR_AllowCinematicConsole() ) ) {
		// stop if menu or console is up
		SCR_FinishCinematic();
		return;
	}

	if( cl.cin.pause_cnt > 0 ) {
		return;
	}

	// CIN_NeedNextFrame is going to query us for raw samples length
	CIN_AddRawSamplesListener( cl.cin.h, NULL,
							   &SCR_CinematicRawSamples, &SCR_CinematicGetRawSamplesLength );

	if( !CIN_NeedNextFrame( cl.cin.h, SCR_CinematicTime() - cl.cin.startTime ) ) {
		cl.cin.redraw = false;
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
bool SCR_DrawCinematic( void ) {
	int x, y;
	int w, h;
	bool keepRatio;

	if( cls.state != CA_CINEMATIC ) {
		return false;
	}

	if( !cl.cin.pic ) {
		return true;
	}

	keepRatio = cl.cin.keepRatio && cl.cin.aspect_numerator != 0 && cl.cin.aspect_denominator != 0;

	// check whether we should keep video aspect ratio upon user's request
	// and make sure that video's aspect ration is really different from that of the screen
	// a/b == c/d => a * d == c * b
	if( keepRatio && ( viddef.width * cl.cin.aspect_numerator != viddef.height * cl.cin.aspect_denominator ) ) {
		float aspect;

		// apply the aspect ratio
		aspect = ( (float)cl.cin.aspect_numerator * cl.cin.width ) / ( (float)cl.cin.aspect_denominator * cl.cin.height );

		// shrink either of the dimensions, also keeping it even
		if( aspect > 1 ) {
			w = viddef.width;
			h = ( (int)( (float)viddef.width / aspect ) + 1 ) & ~1;

			Q_clamp( h, 0, (int)viddef.height );

			x = 0;
			y = Q_rint( ( viddef.height - h ) * 0.5 );
		} else {
			// this branch is untested but hopefully works

			w = ( (int)( (float)viddef.height * aspect ) + 1 ) & ~1;
			h = viddef.height;

			Q_clamp( w, 0, (int)viddef.width );

			x = Q_rint( ( viddef.width  - w ) * 0.5 );
			y = 0;
		}
	} else {
		// fullscreen (stretches video in case of non-matching aspect ratios)
		w = viddef.width;
		h = viddef.height;
		x = 0;
		y = 0;
	}

	if( cl.cin.yuv ) {
		ref_yuv_t *cyuv = cl.cin.cyuv;

		re.DrawStretchRawYUV( x, y, w, h,
							  (float)( cyuv->x_offset ) / cyuv->image_width,
							  (float)( cyuv->y_offset ) / cyuv->image_height,
							  (float)( cyuv->x_offset + cyuv->width ) / cyuv->image_width,
							  (float)( cyuv->y_offset + cyuv->height ) / cyuv->image_height,
							  cl.cin.redraw ? cyuv->yuv : NULL );
	} else {
		re.DrawStretchRaw( x, y, w, h, cl.cin.width, cl.cin.height,
						   0, 0, 1, 1, cl.cin.redraw ? cl.cin.pic : NULL );
	}

	cl.cin.redraw = false;
	return true;
}

/*
* SCR_PlayCinematic
*/
static void SCR_PlayCinematic( const char *arg, int flags ) {
	struct cinematics_s *cin;
	bool has_ogg;
	bool yuv;
	float framerate;
	size_t name_size = strlen( "video/" ) + strlen( arg ) + 1;
	char *name = alloca( name_size );

	if( strstr( arg, "/" ) == NULL && strstr( arg, "\\" ) == NULL ) {
		Q_snprintfz( name, name_size, "video/%s", arg );
	} else {
		Q_snprintfz( name, name_size, "%s", arg );
	}

	cin = CIN_Open( name, 0, 0, &yuv, &framerate );
	if( !cin ) {
		Com_Printf( "SCR_PlayCinematic: couldn't find %s\n", name );
		return;
	}

	has_ogg = CIN_HasOggAudio( cin );

	CIN_Close( cin );

	SCR_FinishCinematic();

	CL_SoundModule_StopAllSounds( true, true );

	cin = CIN_Open( name, 0, has_ogg ? CIN_NOAUDIO : 0, &yuv, &framerate );
	if( !cin ) {
		Com_Printf( "SCR_PlayCinematic: (FIXME) couldn't find %s\n", name );
		return;
	}

	if( has_ogg ) {
		CL_SoundModule_StartBackgroundTrack( CIN_FileName( cin ), NULL, 4 );
	}

	cl.cin.h = cin;
	cl.cin.keepRatio = ( flags & 1 ) ? false : true;
	cl.cin.allowConsole = ( flags & 2 ) ? false : true;
	cl.cin.startTime = SCR_CinematicTime();
	cl.cin.paused = false;
	cl.cin.pause_cnt = 0;
	cl.cin.yuv = yuv;
	cl.cin.framerate = framerate;

	CL_SetClientState( CA_CINEMATIC );

	SCR_EndLoadingPlaque();

	SCR_RunCinematic();
}

/*
* SCR_PauseCinematic
*/
void SCR_PauseCinematic( bool pause ) {
	if( cls.state != CA_CINEMATIC ) {
		return;
	}

	cl.cin.pause_cnt += pause ? 1 : -1;
	if( cl.cin.pause_cnt == 1 && pause ) {
		cl.cin.pauseTime = SCR_CinematicTime();
		CL_SoundModule_LockBackgroundTrack( true );
		CL_SoundModule_Clear();
	} else if( cl.cin.pause_cnt == 0 && !pause ) {
		cl.cin.startTime += SCR_CinematicTime() - cl.cin.pauseTime;
		cl.cin.pauseTime = 0;
		CL_SoundModule_LockBackgroundTrack( false );
	}
	if( cl.cin.pause_cnt < 0 ) {
		cl.cin.pause_cnt = 0;
	}
}

// =======================================================================

/*
* CL_CinematicsComplete_f
*/
static char **CL_CinematicsComplete_f( const char *partial ) {
	return Cmd_CompleteFileList( partial, "video", NULL, true );
}

/*
* CL_PlayCinematic_f
*/
void CL_PlayCinematic_f( void ) {
	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <name> [flags]\n", Cmd_Argv( 0 ) );
		return;
	}

	SCR_PlayCinematic( Cmd_Argv( 1 ), atoi( Cmd_Argv( 2 ) ) );
}

/*
* CL_PauseCinematic_f
*/
void CL_PauseCinematic_f( void ) {
	if( cls.state != CA_CINEMATIC ) {
		Com_Printf( "Usage: %s\n", Cmd_Argv( 0 ) );
		return;
	}

	cl.cin.paused = !cl.cin.paused;
	SCR_PauseCinematic( cl.cin.paused );
}

/*
* CL_InitCinematics
*/
void CL_InitCinematics( void ) {
	CIN_LoadLibrary( true );

	Cmd_AddCommand( "cinematic", CL_PlayCinematic_f );
	Cmd_AddCommand( "cinepause", CL_PauseCinematic_f );

	Cmd_SetCompletionFunc( "cinematic", CL_CinematicsComplete_f );
}

/*
* CL_ShutdownCinematics
*/
void CL_ShutdownCinematics( void ) {
	Cmd_RemoveCommand( "cinematic" );
	Cmd_RemoveCommand( "cinepause" );

	CIN_UnloadLibrary( true );
}
