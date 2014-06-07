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

#include "snd_local.h"
#include "snd_cmdque.h"

static sndQueue_t *s_cmdQueue;

static struct qthread_s *s_backThread;

struct mempool_s *soundpool;

cvar_t *s_volume;
cvar_t *s_musicvolume;
cvar_t *s_openAL_device;

cvar_t *s_doppler;
cvar_t *s_sound_velocity;
cvar_t *s_stereo2mono;

static int s_registration_sequence = 1;
static qboolean s_registering;

static void SF_UnregisterSound( sfx_t *sfx );
static void SF_FreeSound( sfx_t *sfx );

/*
* Commands
*/

#ifdef ENABLE_PLAY
static void SF_Play_f( void )
{
	int i;
	char name[MAX_QPATH];

	i = 1;
	while( i < trap_Cmd_Argc() )
	{
		Q_strncpyz( name, trap_Cmd_Argv( i ), sizeof( name ) );

		S_StartLocalSound( name );
		i++;
	}
}
#endif // ENABLE_PLAY

/*
* SF_Music
*/
static void SF_Music_f( void )
{
	if( trap_Cmd_Argc() == 2 )
	{
		SF_StartBackgroundTrack( trap_Cmd_Argv( 1 ), trap_Cmd_Argv( 1 ) );
	}
	else if( trap_Cmd_Argc() == 3 )
	{
		SF_StartBackgroundTrack( trap_Cmd_Argv( 1 ), trap_Cmd_Argv( 2 ) );
	}
	else
	{
		Com_Printf( "music <intro|playlist> [loop|shuffle]\n" );
		return;
	}
}

/*
* SF_SoundList
*/
static void SF_SoundList_f( void )
{
	S_IssueStuffCmd( s_cmdQueue, "soundlist" );
}

/*
* SF_ListDevices_f
*/
static void SF_ListDevices_f( void )
{
	S_IssueStuffCmd( s_cmdQueue, "devicelist" );
}

/*
* SF_Init
*/
qboolean SF_Init( void *hwnd, int maxEntities, qboolean verbose )
{
	soundpool = S_MemAllocPool( "OpenAL sound module" );

#ifdef OPENAL_RUNTIME
	if( !QAL_Init( ALDRIVER, verbose ) )
	{
#ifdef ALDRIVER_ALT
		if( !QAL_Init( ALDRIVER_ALT, verbose ) )
#endif
		{
			Com_Printf( "Failed to load OpenAL library: %s\n", ALDRIVER );
			return qfalse;
		}
	}
#endif

	s_volume = trap_Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	s_musicvolume = trap_Cvar_Get( "s_musicvolume", "0.2", CVAR_ARCHIVE );
	s_doppler = trap_Cvar_Get( "s_doppler", "1.0", CVAR_ARCHIVE );
	s_sound_velocity = trap_Cvar_Get( "s_sound_velocity", "10976", CVAR_DEVELOPER );
	s_stereo2mono = trap_Cvar_Get ( "s_stereo2mono", "0", CVAR_ARCHIVE );

#ifdef ENABLE_PLAY
	trap_Cmd_AddCommand( "play", SF_Play_f );
#endif
	trap_Cmd_AddCommand( "music", SF_Music_f );
	trap_Cmd_AddCommand( "stopmusic", SF_StopBackgroundTrack );
	trap_Cmd_AddCommand( "prevmusic", SF_PrevBackgroundTrack );
	trap_Cmd_AddCommand( "nextmusic", SF_NextBackgroundTrack );
	trap_Cmd_AddCommand( "pausemusic", SF_PauseBackgroundTrack );
	trap_Cmd_AddCommand( "soundlist", SF_SoundList_f );
	trap_Cmd_AddCommand( "s_devices", SF_ListDevices_f );

	s_cmdQueue = S_CreateSoundQueue();
	if( !s_cmdQueue ) {
		return qfalse;
	}

	trap_Thread_Create( &s_backThread, S_BackgroundUpdateProc, s_cmdQueue );

	S_IssueInitCmd( s_cmdQueue, hwnd, maxEntities, verbose );

	S_FinishSoundQueue( s_cmdQueue );

	if( !alContext ) {
		return qfalse;
	}

	S_InitBuffers();

	return qtrue;
}

/*
* SF_Shutdown
*/
void SF_Shutdown( qboolean verbose )
{
	if( !soundpool ) {
		return;
	}
	
	SF_StopAllSounds();

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	S_ShutdownBuffers();

	// shutdown backend
	S_IssueShutdownCmd( s_cmdQueue, verbose );

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	// wait for the backend thread to die
	trap_Thread_Join( s_backThread );
	s_backThread = NULL;

	S_DestroySoundQueue( &s_cmdQueue );

#ifdef ENABLE_PLAY
	trap_Cmd_RemoveCommand( "play" );
#endif
	trap_Cmd_RemoveCommand( "music" );
	trap_Cmd_RemoveCommand( "stopmusic" );
	trap_Cmd_RemoveCommand( "prevmusic" );
	trap_Cmd_RemoveCommand( "nextmusic" );
	trap_Cmd_RemoveCommand( "pausemusic" );
	trap_Cmd_RemoveCommand( "soundlist" );
	trap_Cmd_RemoveCommand( "s_devices" );

	QAL_Shutdown();

	S_MemFreePool( &soundpool );
}

void SF_BeginRegistration( void )
{
	s_registration_sequence++;
	if( !s_registration_sequence ) {
		s_registration_sequence = 1;
	}
	s_registering = qtrue;

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );
}

void SF_EndRegistration( void )
{
	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	S_ForEachBuffer( SF_UnregisterSound );

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	S_ForEachBuffer( SF_FreeSound );

	s_registering = qfalse;

}

/*
* SF_RegisterSound
*/
sfx_t *SF_RegisterSound( const char *name )
{
	sfx_t *sfx;

	assert( name );

	sfx = S_FindBuffer( name );
	S_IssueLoadSfxCmd( s_cmdQueue, sfx->id );
	sfx->used = trap_Milliseconds();
	sfx->registration_sequence = s_registration_sequence;
	return sfx;
}

/*
* SF_UnregisterSound
*/
static void SF_UnregisterSound( sfx_t *sfx )
{
	if( sfx->filename[0] == '\0' ) {
		return;
	}
	if( sfx->registration_sequence != s_registration_sequence ) {
		S_IssueFreeSfxCmd( s_cmdQueue, sfx->id );
	}
}

/*
* SF_FreeSound
*/
static void SF_FreeSound( sfx_t *sfx )
{
	if( !sfx->registration_sequence ) {
		return;
	}
	if( sfx->registration_sequence != s_registration_sequence ) {
		S_MarkBufferFree( sfx );
	}
}

/*
* SF_Activate
*/
void SF_Activate( qboolean active )
{
	SF_LockBackgroundTrack( !active );

	S_IssueActivateCmd( s_cmdQueue, active );
}

/*
* SF_StartBackgroundTrack
*/
void SF_StartBackgroundTrack( const char *intro, const char *loop )
{
	S_IssueStartBackgroundTrackCmd( s_cmdQueue, intro, loop );
}

/*
* SF_StopBackgroundTrack
*/
void SF_StopBackgroundTrack( void )
{
	S_IssueStopBackgroundTrackCmd( s_cmdQueue );
}

/*
* SF_LockBackgroundTrack
*/
void SF_LockBackgroundTrack( qboolean lock )
{
	S_IssueLockBackgroundTrackCmd( s_cmdQueue, lock );
}

/*
* SF_StopAllSounds
*/
void SF_StopAllSounds( void )
{
	S_IssueStopAllSoundsCmd( s_cmdQueue );
}

/*
* SF_PrevBackgroundTrack
*/
void SF_PrevBackgroundTrack( void )
{
	S_IssueAdvanceBackgroundTrackCmd( s_cmdQueue, -1 );
}

/*
* SF_NextBackgroundTrack
*/
void SF_NextBackgroundTrack( void )
{
	S_IssueAdvanceBackgroundTrackCmd( s_cmdQueue, 1 );
}

/*
* SF_PauseBackgroundTrack
*/
void SF_PauseBackgroundTrack( void )
{
	S_IssuePauseBackgroundTrackCmd( s_cmdQueue );
}

/*
* SF_BeginAviDemo
*/
void SF_BeginAviDemo( void )
{
	S_IssueAviDemoCmd( s_cmdQueue, qtrue );
}

/*
* SF_StopAviDemo
*/
void SF_StopAviDemo( void )
{
	S_IssueAviDemoCmd( s_cmdQueue, qfalse );
}

/*
* SF_SetAttenuationModel
*/
void SF_SetAttenuationModel( int model, float maxdistance, float refdistance )
{
	S_IssueSetAttenuationCmd( s_cmdQueue, model, maxdistance, refdistance );
}

/*
* SF_SetEntitySpatialization
*/
void SF_SetEntitySpatialization( int entnum, const vec3_t origin, const vec3_t velocity )
{
	S_IssueSetEntitySpatializationCmd( s_cmdQueue, entnum, origin, velocity );
}

/*
* SF_StartFixedSound
*/
void SF_StartFixedSound( sfx_t *sfx, const vec3_t origin, int channel, float fvol, float attenuation )
{
	S_IssueStartFixedSoundCmd( s_cmdQueue, sfx->id, origin, channel, fvol, attenuation );
}

/*
* SF_StartRelativeSound
*/
void SF_StartRelativeSound( sfx_t *sfx, int entnum, int channel, float fvol, float attenuation )
{
	S_IssueStartRelativeSoundCmd( s_cmdQueue, sfx->id, entnum, channel, fvol, attenuation );
}

/*
* SF_StartGlobalSound
*/
void SF_StartGlobalSound( sfx_t *sfx, int channel, float fvol )
{
	S_IssueStartGlobalSoundCmd( s_cmdQueue, sfx->id, channel, fvol );
}

/*
* SF_StartLocalSound
*/
void SF_StartLocalSound( const char *sound )
{
	sfx_t *sfx;

	sfx = SF_RegisterSound( sound );
	if( !sfx )
	{
		Com_Printf( "S_StartLocalSound: can't cache %s\n", sound );
		return;
	}

	S_IssueStartLocalSoundCmd( s_cmdQueue, sfx->id );
}

/*
* SF_Clear
*/
void SF_Clear( void )
{
	S_IssueClearCmd( s_cmdQueue );
}

/*
* SF_AddLoopSound
*/
void SF_AddLoopSound( sfx_t *sfx, int entnum, float fvol, float attenuation )
{
	S_IssueAddLoopSoundCmd( s_cmdQueue, sfx->id, entnum, fvol, attenuation );
}

/*
* SF_Update
*/
void SF_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, qboolean avidump )
{
	S_IssueSetListenerCmd( s_cmdQueue, origin, velocity, axis, avidump );
}

/*
* SF_RawSamples
*/
void SF_RawSamples( unsigned int samples, unsigned int rate, unsigned short width, 
	unsigned short channels, const qbyte *data, qboolean music )
{
	size_t data_size = samples * width * channels;
	qbyte *data_copy = S_Malloc( data_size );

	memcpy( data_copy, data, data_size );

	S_IssueRawSamplesCmd( s_cmdQueue, samples, rate, width, channels, data_copy, music );
}

/*
* SF_PositionedRawSamples
*/
void SF_PositionedRawSamples( int entnum, float fvol, float attenuation, 
	unsigned int samples, unsigned int rate, 
	unsigned short width, unsigned short channels, const qbyte *data )
{
	size_t data_size = samples * width * channels;
	qbyte *data_copy = S_Malloc( data_size );

	memcpy( data_copy, data, data_size );

	S_IssuePositionedRawSamplesCmd( s_cmdQueue, entnum, fvol, attenuation, 
		samples, rate, width, channels, data_copy );
}

// =====================================================================

/*
* S_API
*/
int S_API( void )
{
	return SOUND_API_VERSION;
}

/*
* S_Error
*/
void S_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

#ifndef SOUND_HARD_LINKED

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void Com_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

#if defined ( HAVE_DLLMAIN )
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif

#endif
