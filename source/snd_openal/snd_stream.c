/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "snd_local.h"

static src_t *src = NULL;
static qboolean is_playing = qfalse;
static qboolean use_musicvolume = qfalse;
static ALuint source;
static ALfloat played_length = 0;
static unsigned int systime, waittime;

/*
* Local helper functions
*/

static void allocate_channel( void )
{
	// Allocate a source at high priority
	src = S_AllocSource( SRCPRI_STREAM, -2, 0 );
	if( !src )
		return;

	S_LockSource( src );
	source = S_GetALSource( src );

	qalSourcei( source, AL_BUFFER, 0 );
	qalSourcei( source, AL_LOOPING, AL_FALSE );
	qalSource3f( source, AL_POSITION, 0.0, 0.0, 0.0 );
	qalSource3f( source, AL_VELOCITY, 0.0, 0.0, 0.0 );
	qalSource3f( source, AL_DIRECTION, 0.0, 0.0, 0.0 );
	qalSourcef( source, AL_ROLLOFF_FACTOR, 0.0 );
	qalSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
	qalSourcef( source, AL_GAIN, ( use_musicvolume ? s_musicvolume->value : s_volume->value ) );
}

static void free_channel( void )
{
	// Release the output source
	S_UnlockSource( src );
	source = 0;
	src = NULL;
}

/*
* Sound system wide functions (snd_local.h)
*/

void S_UpdateStream( void )
{
	int processed = 0;
	ALint state;
	ALuint buffer;
	float processed_length;
	unsigned int prevtime, interval;

	prevtime = systime;
	systime = trap_Milliseconds();
	interval = systime - prevtime;

	if( !src ) {
		waittime += interval;
		return;
	}

	processed = 0;
	processed_length = 0;

	// Un-queue any processed buffers, and delete them
	qalGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );
	if( processed )
	{
		do
		{
			qalSourceUnqueueBuffers( source, 1, &buffer );
			processed_length += S_GetBufferLength( buffer );
			qalDeleteBuffers( 1, &buffer );
		} while( --processed );
	}

	played_length += processed_length;

	// If it's stopped, release the source
	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( state == AL_STOPPED )
	{
		is_playing = qfalse;
		qalSourceStop( source );
		free_channel();
		waittime += interval;
		return;
	}

	if( ( use_musicvolume && s_musicvolume->modified ) || ( !use_musicvolume && s_volume->modified ) )
		qalSourcef( source, AL_GAIN, ( use_musicvolume ? s_musicvolume->value : s_volume->value ) );
}

void S_StopStream( void )
{
	if( !src )
		return;

	is_playing = qfalse;
	played_length = 0;
	qalSourceStop( source );
	free_channel();
}

/*
* Global functions (sound.h)
*/
void S_RawSamples( unsigned int samples, unsigned int rate, unsigned short width, unsigned short channels, const qbyte *data, qboolean music )
{
	ALuint buffer;
	ALuint format;
	ALint state;
	ALenum error;

	use_musicvolume = music;
	format = S_SoundFormat( width, channels );
	systime = trap_Milliseconds();

	// Create the source if necessary
	if( !src )
	{
		allocate_channel();
		if( !src )
		{
			Com_Printf( "Couldn't allocate streaming source\n" );
			return;
		}
	}

	qalGenBuffers( 1, &buffer );
	if( ( error = qalGetError() ) != AL_NO_ERROR )
	{
		Com_Printf( "Couldn't create a sound buffer (%s)\n", S_ErrorMessage( error ) );
		return;
	}

	qalBufferData( buffer, format, data, ( samples * width * channels ), rate );
	if( ( error = qalGetError() ) != AL_NO_ERROR )
	{
		Com_Printf( "Couldn't fill sound buffer (%s)", S_ErrorMessage( error ) );
		return;
	}

	qalSourceQueueBuffers( source, 1, &buffer );
	if( ( error = qalGetError() ) != AL_NO_ERROR )
	{
		Com_Printf( "Couldn't queue sound buffer (%s)", S_ErrorMessage( error ) );
		return;
	}

	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( !is_playing )
	{
		qalSourcePlay( source );
		is_playing = qtrue;
	}
}

/*
* S_GetRawSamplesTime
*/
unsigned int S_GetRawSamplesTime( void )
{
	float pos = 0;

	if( src && is_playing ) {
		qalGetSourcef( source, AL_SEC_OFFSET, &pos );
	}	
	return (played_length + pos) * 1000 + waittime;
}
