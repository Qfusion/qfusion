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

#include "snd_local.h"
#include "snd_cmdque.h"

static sndQueue_t *s_cmdQueue;

static struct qthread_s *s_backThread;

static int		s_registration_sequence;
static qboolean	s_registering;

struct mempool_s *soundpool;

cvar_t *developer;

cvar_t *s_volume;
cvar_t *s_musicvolume;
cvar_t *s_testsound;
cvar_t *s_khz;
cvar_t *s_show;
cvar_t *s_mixahead;
cvar_t *s_swapstereo;
cvar_t *s_vorbis;
cvar_t *s_pseudoAcoustics;
cvar_t *s_separationDelay;

sfx_t known_sfx[MAX_SFX];
int num_sfx;

/*
===============================================================================

console functions

===============================================================================
*/

#ifdef ENABLE_PLAY
static void SF_Play_f( void )
{
	int i;
	sfx_t *sfx;

	for( i = 1; i < trap_Cmd_Argc(); i++ )
	{
		sfx = SF_RegisterSound( trap_Cmd_Argv( i ) );
		if( !sfx )
		{
			Com_Printf( "Couldn't play: %s\n", trap_Cmd_Argv( i ) );
			continue;
		}
		S_StartGlobalSound( sfx, S_CHANNEL_AUTO, 1.0 );
	}
}
#endif // ENABLE_PLAY

/*
* SF_SoundList
*/
static void SF_SoundList_f( void )
{
	S_IssueStuffCmd( s_cmdQueue, "soundlist" );
}

/*
* S_Music
*/
static void SF_Music_f( void )
{
	if( trap_Cmd_Argc() < 2 )
	{
		Com_Printf( "music: <introfile|playlist> [loopfile|shuffle]\n" );
		return;
	}

	SF_StartBackgroundTrack( trap_Cmd_Argv( 1 ), trap_Cmd_Argv( 2 ) );
}

/*
* S_SoundInfo_f
*/
static void SF_SoundInfo_f( void )
{
	Com_Printf( "%5d stereo\n", dma.channels - 1 );
	Com_Printf( "%5d samples\n", dma.samples );
	Com_Printf( "%5d samplepos\n", dma.samplepos );
	Com_Printf( "%5d samplebits\n", dma.samplebits );
	Com_Printf( "%5d submission_chunk\n", dma.submission_chunk );
	Com_Printf( "%5d speed\n", dma.speed );
	Com_Printf( "0x%x dma buffer\n", dma.buffer );
}

// =======================================================================
// Load a sound
// =======================================================================

/*
* SF_FindName
*/
static sfx_t *SF_FindName( const char *name )
{
	int i;
	sfx_t *sfx;

	if( !name )
		S_Error( "SF_FindName: NULL" );
	if( !name[0] != '\0' )
	{
		assert( name[0] != '\0' );
		S_Error( "SF_FindName: empty name" );
	}

	if( strlen( name ) >= MAX_QPATH )
		S_Error( "Sound name too long: %s", name );

	// see if already loaded
	for( i = 0; i < num_sfx; i++ )
	{
		if( !strcmp( known_sfx[i].name, name ) )
			return &known_sfx[i];
	}

	// find a free sfx
	for( i = 0; i < num_sfx; i++ )
	{
		if( !known_sfx[i].name[0] )
			break;
	}

	if( i == num_sfx )
	{
		if( num_sfx == MAX_SFX )
			S_Error( "S_FindName: out of sfx_t" );
		num_sfx++;
	}

	sfx = &known_sfx[i];
	memset( sfx, 0, sizeof( *sfx ) );
	Q_strncpyz( sfx->name, name, sizeof( sfx->name ) );
	sfx->isUrl = trap_FS_IsUrl( name );

	return sfx;
}

/*
* SF_BeginRegistration
*/
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

/*
* SF_RegisterSound
*/
sfx_t *SF_RegisterSound( const char *name )
{
	sfx_t *sfx;
	int sfxnum;

	assert( name );

	sfx = SF_FindName( name );
	if( sfx->registration_sequence != s_registration_sequence ) {
		sfx->registration_sequence = s_registration_sequence;

		// evenly balance the load between two threads during registration
		sfxnum = sfx - known_sfx;
		if( !s_registering || sfxnum & 1 ) {
			S_IssueLoadSfxCmd( s_cmdQueue, sfxnum );
		}
		else {
			S_LoadSound( sfx );
		}
	}
	return sfx;
}

/*
* SF_FreeSounds
*/
void SF_FreeSounds( void )
{
	int i;
	sfx_t *sfx;

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	// free all sounds
	for( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ )
	{
		if( !sfx->name[0] ) {
			continue;
		}
		S_Free( sfx->cache );
		memset( sfx, 0, sizeof( *sfx ) );
	}
}

/*
* SF_EndRegistration
*/
void SF_EndRegistration( void )
{
	int i;
	sfx_t *sfx;

	// wait for the queue to be processed
	S_FinishSoundQueue( s_cmdQueue );

	s_registering = qfalse;

	// free any sounds not from this registration sequence
	for( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ ) {
		if( !sfx->name[0] ) {
			continue;
		}
		if( sfx->registration_sequence != s_registration_sequence ) {
			// we don't need this sound
			S_Free( sfx->cache );
			memset( sfx, 0, sizeof( *sfx ) );
		}
	}
}

/*
* SF_Init
*/
qboolean SF_Init( void *hwnd, int maxEntities, qboolean verbose )
{
	soundpool = S_MemAllocPool( "QF Sound Module" );

	developer = trap_Cvar_Get( "developer", "0", 0 );

	s_volume = trap_Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	s_musicvolume = trap_Cvar_Get( "s_musicvolume", "0.2", CVAR_ARCHIVE );
	s_khz = trap_Cvar_Get( "s_khz", "44", CVAR_ARCHIVE|CVAR_LATCH_SOUND );
	s_mixahead = trap_Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE );
	s_show = trap_Cvar_Get( "s_show", "0", CVAR_CHEAT );
	s_testsound = trap_Cvar_Get( "s_testsound", "0", 0 );
	s_swapstereo = trap_Cvar_Get( "s_swapstereo", "0", CVAR_ARCHIVE );
	s_vorbis = trap_Cvar_Get( "s_vorbis", "1", CVAR_ARCHIVE );
	s_pseudoAcoustics = trap_Cvar_Get( "s_pseudoAcoustics", "0", CVAR_ARCHIVE );
	s_separationDelay = trap_Cvar_Get( "s_separationDelay", "1.0", CVAR_ARCHIVE );

#ifdef ENABLE_PLAY
	trap_Cmd_AddCommand( "play", SF_Play_f );
#endif
	trap_Cmd_AddCommand( "music", SF_Music_f );
	trap_Cmd_AddCommand( "stopsound", SF_StopAllSounds );
	trap_Cmd_AddCommand( "stopmusic", SF_StopBackgroundTrack );
	trap_Cmd_AddCommand( "prevmusic", SF_PrevBackgroundTrack );
	trap_Cmd_AddCommand( "nextmusic", SF_NextBackgroundTrack );
	trap_Cmd_AddCommand( "pausemusic", SF_PauseBackgroundTrack );
	trap_Cmd_AddCommand( "soundlist", SF_SoundList_f );
	trap_Cmd_AddCommand( "soundinfo", SF_SoundInfo_f );

	num_sfx = 0;

	s_registration_sequence = 1;
	s_registering = qfalse;

	s_cmdQueue = S_CreateSoundQueue();
	if( !s_cmdQueue ) {
		return qfalse;
	}

	trap_Thread_Create( &s_backThread, S_BackgroundUpdateProc, s_cmdQueue );

	S_IssueInitCmd( s_cmdQueue, hwnd, maxEntities, verbose );

	S_FinishSoundQueue( s_cmdQueue );

	if( !dma.buffer )
		return qfalse;

	SF_SetAttenuationModel( S_DEFAULT_ATTENUATION_MODEL, 
		S_DEFAULT_ATTENUATION_MAXDISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );

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

	// free all sounds
	SF_FreeSounds();

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
	trap_Cmd_RemoveCommand( "stopsound" );
	trap_Cmd_RemoveCommand( "stopmusic" );
	trap_Cmd_RemoveCommand( "prevmusic" );
	trap_Cmd_RemoveCommand( "nextmusic" );
	trap_Cmd_RemoveCommand( "pausemusic" );
	trap_Cmd_RemoveCommand( "soundlist" );
	trap_Cmd_RemoveCommand( "soundinfo" );

	S_MemFreePool( &soundpool );

	s_registering = qfalse;

	num_sfx = 0;
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
	S_IssueStartFixedSoundCmd( s_cmdQueue, sfx - known_sfx, origin, channel, fvol, attenuation );
}

/*
* SF_StartRelativeSound
*/
void SF_StartRelativeSound( sfx_t *sfx, int entnum, int channel, float fvol, float attenuation )
{
	S_IssueStartRelativeSoundCmd( s_cmdQueue, sfx - known_sfx, entnum, channel, fvol, attenuation );
}

/*
* SF_StartGlobalSound
*/
void SF_StartGlobalSound( sfx_t *sfx, int channel, float fvol )
{
	S_IssueStartGlobalSoundCmd( s_cmdQueue, sfx - known_sfx, channel, fvol );
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

	S_IssueStartLocalSoundCmd( s_cmdQueue, sfx - known_sfx );
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
	S_IssueAddLoopSoundCmd( s_cmdQueue, sfx - known_sfx, entnum, fvol, attenuation );
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
