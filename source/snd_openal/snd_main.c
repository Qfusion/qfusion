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

struct mempool_s *soundpool;

cvar_t *s_volume;
cvar_t *s_musicvolume;
cvar_t *s_openAL_device;

static cvar_t *s_doppler;
static cvar_t *s_sound_velocity;
cvar_t *s_stereo2mono;

int s_attenuation_model = 0;
float s_attenuation_maxdistance = 0;
float s_attenuation_refdistance = 0;

static qboolean snd_shutdown_bug = qfalse;
static ALCdevice *alDevice = NULL;
static ALCcontext *alContext = NULL;

/*
* Commands
*/

#ifdef ENABLE_PLAY
static void S_Play( void )
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
* S_Music
*/
static void S_Music( void )
{
	if( trap_Cmd_Argc() == 2 )
	{
		S_StartBackgroundTrack( trap_Cmd_Argv( 1 ), trap_Cmd_Argv( 1 ) );
	}
	else if( trap_Cmd_Argc() == 3 )
	{
		S_StartBackgroundTrack( trap_Cmd_Argv( 1 ), trap_Cmd_Argv( 2 ) );
	}
	else
	{
		Com_Printf( "music <intro|playlist> [loop|shuffle]\n" );
		return;
	}
}

/*
* S_StopMusic
*/
static void S_StopMusic( void )
{
	S_StopBackgroundTrack();
}

/*
* S_PrevMusic
*/
static void S_PrevMusic( void )
{
	S_PrevBackgroundTrack();
}

/*
* S_NextMusic
*/
static void S_NextMusic( void )
{
	S_NextBackgroundTrack();
}

/*
* S_PauseMusic
*/
static void S_PauseMusic( void )
{
	S_PauseBackgroundTrack();
}

/*
* S_BeginAviDemo
*/
void S_BeginAviDemo( void )
{
}

/*
* S_StopAviDemo
*/
void S_StopAviDemo( void )
{
}

/*
* S_ListDevices
*/
static void S_ListDevices( void )
{
	char *device, *defaultDevice, *curDevice;

	Com_Printf( "Available OpenAL devices:\n" );

	defaultDevice = ( char * )qalcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );
	curDevice = ( char * )qalcGetString( alDevice, ALC_DEVICE_SPECIFIER );
	device = ( char * )qalcGetString( NULL, ALC_DEVICE_SPECIFIER );

	for( ; *device; device += strlen( device ) + 1 )
	{
		if( defaultDevice && !strcmp( device, defaultDevice ) )
			Com_Printf( "(def) : " );
		else if( curDevice && !strcmp( device, curDevice ) )
			Com_Printf( "(cur) : " );
		else
			Com_Printf( "      : " );

		Com_Printf( "%s\n", device );
	}
}

/*
* S_SoundFormat
*/
ALuint S_SoundFormat( int width, int channels )
{
	if( width == 1 )
	{
		if( channels == 1 )
			return AL_FORMAT_MONO8;
		else if( channels == 2 )
			return AL_FORMAT_STEREO8;
	}
	else if( width == 2 )
	{
		if( channels == 1 )
			return AL_FORMAT_MONO16;
		else if( channels == 2 )
			return AL_FORMAT_STEREO16;
	}

	Com_Printf( "Unknown sound format: %i channels, %i bits.\n", channels, width * 8 );
	return AL_FORMAT_MONO16;
}

/*
* S_GetBufferLength
*
* Returns buffer length expressed in milliseconds
*/
ALuint S_GetBufferLength( ALuint buffer )
{
    ALint size, bits, channels, freq;

    qalGetBufferi( buffer, AL_SIZE, &size );
    qalGetBufferi( buffer, AL_BITS, &bits );
    qalGetBufferi( buffer, AL_FREQUENCY, &freq );
    qalGetBufferi( buffer, AL_CHANNELS, &channels );

	if( qalGetError() != AL_NO_ERROR ) {
        return 0;
	}
    return (ALuint)((ALfloat)(size/(bits/8)/channels) * 1000.0 / freq + 0.5f);
}

/*
* S_ErrorMessage
*/
const char *S_ErrorMessage( ALenum error )
{
	switch( error )
	{
	case AL_NO_ERROR:
		return "No error";
	case AL_INVALID_NAME:
		return "Invalid name";
	case AL_INVALID_ENUM:
		return "Invalid enumerator";
	case AL_INVALID_VALUE:
		return "Invalid value";
	case AL_INVALID_OPERATION:
		return "Invalid operation";
	case AL_OUT_OF_MEMORY:
		return "Out of memory";
	default:
		return "Unknown error";
	}
}

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
#endif

#ifdef _WIN32
#define ALDRIVER "OpenAL32.dll"
#define ALDEVICE_DEFAULT "Generic Software"
#elif defined ( __MACOSX__ )
#define ALDRIVER "/System/Library/Frameworks/OpenAL.framework/OpenAL"
#define ALDEVICE_DEFAULT NULL
#else
#define ALDRIVER "libopenal.so.1"
#define ALDRIVER_ALT "libopenal.so.0"
#define ALDEVICE_DEFAULT NULL
#endif

/*
* S_Init
*/
qboolean S_Init( void *hwnd, int maxEntities, qboolean verbose )
{
	int numDevices;
	int userDeviceNum = -1;
	char *devices, *defaultDevice;

	soundpool = S_MemAllocPool( "OpenAL sound module" );

	alDevice = NULL;
	alContext = NULL;

#ifdef OPENAL_RUNTIME
	if( !QAL_Init( ALDRIVER, verbose ) )
	{
#ifdef ALDRIVER_ALT
		if( !QAL_Init( ALDRIVER_ALT, verbose ) )
#endif
		{
			Com_Printf( "Failed to load OpenAL library: %s\n", ALDRIVER );
			goto fail_no_device;
		}
	}
#endif

	// get system default device identifier
	defaultDevice = ( char * )qalcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );
	if( !defaultDevice )
	{
		Com_Printf( "Failed to get openAL default device\n" );
		goto fail_no_device;
	}

	s_openAL_device = trap_Cvar_Get( "s_openAL_device", ALDEVICE_DEFAULT ? ALDEVICE_DEFAULT : defaultDevice, CVAR_ARCHIVE|CVAR_LATCH_SOUND );

	devices = ( char * )qalcGetString( NULL, ALC_DEVICE_SPECIFIER );
	for( numDevices = 0; *devices; devices += strlen( devices ) + 1, numDevices++ )
	{
		if( !Q_stricmp( s_openAL_device->string, devices ) )
		{
			userDeviceNum = numDevices;

			// force case sensitive
			if( strcmp( s_openAL_device->string, devices ) )
				trap_Cvar_ForceSet( "s_openAL_device", devices );
		}
	}

	if( !numDevices )
	{
		Com_Printf( "Failed to get openAL devices\n" );
		goto fail_no_device;
	}

	// the device assigned by the user is not available
	if( userDeviceNum == -1 )
	{
		Com_Printf( "'s_openAL_device': incorrect device name, reseting to default\n" );

		trap_Cvar_ForceSet( "s_openAL_device", ALDEVICE_DEFAULT ? ALDEVICE_DEFAULT : defaultDevice );

		devices = ( char * )qalcGetString( NULL, ALC_DEVICE_SPECIFIER );
		for( numDevices = 0; *devices; devices += strlen( devices ) + 1, numDevices++ )
		{
			if( !Q_stricmp( s_openAL_device->string, devices ) )
				userDeviceNum = numDevices;
		}

		if( userDeviceNum == -1 )
			trap_Cvar_ForceSet( "s_openAL_device", defaultDevice );
	}

	alDevice = qalcOpenDevice( (const ALchar *)s_openAL_device->string );
	if( !alDevice )
	{
		Com_Printf( "Failed to open device\n" );
		goto fail_no_device;
	}

	// Create context
	alContext = qalcCreateContext( alDevice, NULL );
	if( !alContext )
	{
		Com_Printf( "Failed to create context\n" );
		goto fail;
	}
	qalcMakeContextCurrent( alContext );

	if( verbose )
	{
		Com_Printf( "OpenAL initialized\n" );

		if( numDevices )
		{
			int i;

			Com_Printf( "  Devices:    " );

			devices = ( char * )qalcGetString( NULL, ALC_DEVICE_SPECIFIER );
			for( i = 0; *devices; devices += strlen( devices ) + 1, i++ )
				Com_Printf( "%s%s", devices, ( i < numDevices - 1 ) ? ", " : "" );
			Com_Printf( "\n" );

			if( defaultDevice && *defaultDevice )
				Com_Printf( "  Default system device: %s\n", defaultDevice );

			Com_Printf( "\n" );
		}

		Com_Printf( "  Device:     %s\n", qalcGetString( alDevice, ALC_DEVICE_SPECIFIER ) );
		Com_Printf( "  Vendor:     %s\n", qalGetString( AL_VENDOR ) );
		Com_Printf( "  Version:    %s\n", qalGetString( AL_VERSION ) );
		Com_Printf( "  Renderer:   %s\n", qalGetString( AL_RENDERER ) );
		Com_Printf( "  Extensions: %s\n", qalGetString( AL_EXTENSIONS ) );
	}

	// Check for Linux shutdown race condition
	if( !Q_stricmp( qalGetString( AL_VENDOR ), "J. Valenzuela" ) )
		snd_shutdown_bug = qtrue;

	s_volume = trap_Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	s_musicvolume = trap_Cvar_Get( "s_musicvolume", "0.2", CVAR_ARCHIVE );
	s_doppler = trap_Cvar_Get( "s_doppler", "1.0", CVAR_ARCHIVE );
	s_sound_velocity = trap_Cvar_Get( "s_sound_velocity", "10976", CVAR_DEVELOPER );
	s_stereo2mono = trap_Cvar_Get ( "s_stereo2mono", "0", CVAR_ARCHIVE );

	qalDopplerFactor( s_doppler->value );
	qalDopplerVelocity( s_sound_velocity->value > 0.0f ? s_sound_velocity->value : 0.0f );
	if( qalSpeedOfSound ) // opelAL 1.1 only. alDopplerVelocity being deprecated
		qalSpeedOfSound( s_sound_velocity->value > 0.0f ? s_sound_velocity->value : 0.0f );

	s_doppler->modified = qfalse;

	S_SetAttenuationModel( S_DEFAULT_ATTENUATION_MODEL, S_DEFAULT_ATTENUATION_MAXDISTANCE, S_DEFAULT_ATTENUATION_REFDISTANCE );

	S_LockBackgroundTrack( qfalse );

	if( !S_InitDecoders( verbose ) )
	{
		Com_Printf( "Failed to init decoders\n" );
		goto fail;
	}
	if( !S_InitBuffers() )
	{
		Com_Printf( "Failed to init buffers\n" );
		goto fail;
	}
	if( !S_InitSources( maxEntities, verbose ) )
	{
		Com_Printf( "Failed to init sources\n" );
		goto fail;
	}

#ifdef ENABLE_PLAY
	trap_Cmd_AddCommand( "play", S_Play );
#endif
	trap_Cmd_AddCommand( "music", S_Music );
	trap_Cmd_AddCommand( "stopmusic", S_StopMusic );
	trap_Cmd_AddCommand( "prevmusic", S_PrevMusic );
	trap_Cmd_AddCommand( "nextmusic", S_NextMusic );
	trap_Cmd_AddCommand( "pausemusic", S_PauseMusic );
	trap_Cmd_AddCommand( "soundlist", S_SoundList );
	trap_Cmd_AddCommand( "s_devices", S_ListDevices );

	return qtrue;

fail:
	if( alContext )
	{
		if( !snd_shutdown_bug )
			qalcMakeContextCurrent( NULL );

		qalcDestroyContext( alContext );
		alContext = NULL;
	}

	if( alDevice )
	{
		qalcCloseDevice( alDevice );
		alDevice = NULL;
	}

fail_no_device:
	S_MemFreePool( &soundpool );
	return qfalse;
}

/*
* S_Shutdown
*/
void S_Shutdown( qboolean verbose )
{
	S_StopStreams();
	S_StopBackgroundTrack();

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

	S_ShutdownSources();
	S_ShutdownBuffers();
	S_ShutdownDecoders( verbose );

	if( alContext )
	{
		if( !snd_shutdown_bug )
			qalcMakeContextCurrent( NULL );

		qalcDestroyContext( alContext );
		alContext = NULL;
	}

	if( alDevice )
	{
		qalcCloseDevice( alDevice );
		alDevice = NULL;
	}

	QAL_Shutdown();

	S_MemFreePool( &soundpool );
}

/*
* S_SetAttenuationModel
*/
void S_SetAttenuationModel( int model, float maxdistance, float refdistance )
{
	s_attenuation_model = model;
	s_attenuation_maxdistance = maxdistance;
	s_attenuation_refdistance = refdistance;

	switch( model )
	{
	case 0:
		qalDistanceModel( AL_LINEAR_DISTANCE );
		break;
	case 1:
	default:
		qalDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );
		break;
	case 2:
		qalDistanceModel( AL_INVERSE_DISTANCE );
		break;
	case 3:
		qalDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
		break;
	case 4:
		qalDistanceModel( AL_EXPONENT_DISTANCE );
		break;
	case 5:
		qalDistanceModel( AL_EXPONENT_DISTANCE_CLAMPED );
		break;
	}
}

/*
* S_Update
*/
void S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, qboolean avidump )
{
	float orientation[6];

	orientation[0] = axis[AXIS_FORWARD+0];
	orientation[1] = axis[AXIS_FORWARD+1];
	orientation[2] = axis[AXIS_FORWARD+2];
	orientation[3] = axis[AXIS_UP+0];
	orientation[4] = axis[AXIS_UP+1];
	orientation[5] = axis[AXIS_UP+2];

	qalListenerfv( AL_POSITION, origin );
	qalListenerfv( AL_VELOCITY, velocity );
	qalListenerfv( AL_ORIENTATION, orientation );

	S_UpdateMusic();
	S_UpdateStreams();
	S_UpdateSources();

	s_volume->modified = qfalse; // Checked by src and stream
	s_musicvolume->modified = qfalse; // Checked by stream and music

	if( s_doppler->modified )
	{
		if( s_doppler->value > 0.0f )
			qalDopplerFactor( s_doppler->value );
		else
			qalDopplerFactor( 0.0f );
		s_doppler->modified = qfalse;
	}

	if( s_sound_velocity->modified )
	{
		qalDopplerVelocity( s_sound_velocity->value > 0.0f ? s_sound_velocity->value : 0.0f );
		if( qalSpeedOfSound )
			qalSpeedOfSound( s_sound_velocity->value > 0.0f ? s_sound_velocity->value : 0.0f );
		s_sound_velocity->modified = qfalse;
	}
}

/*
* S_StopAllSounds
*/
void S_StopAllSounds( void )
{
	S_StopStreams();
	S_StopAllSources();
	S_StopBackgroundTrack();
}

/*
* S_Activate
*/
void S_Activate( qboolean activate )
{
	S_LockBackgroundTrack( !activate );

	// TODO: Actually stop playing sounds while not active?
	if( activate )
		qalListenerf( AL_GAIN, 1 );
	else
		qalListenerf( AL_GAIN, 0 );
}
