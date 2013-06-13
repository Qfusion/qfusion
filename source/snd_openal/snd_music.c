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

#define MUSIC_BUFFERS		8
#define MUSIC_BUFFER_SIZE	8192

static src_t *src = NULL;
static ALuint source;
static ALuint buffers[MUSIC_BUFFERS];

static qboolean queued_buffers;
static qboolean alloced_buffers;

static qbyte decode_buffer[MUSIC_BUFFER_SIZE];

// =================================

static bgTrack_t *s_bgTrack;
static bgTrack_t *s_bgTrackHead;

#define BACKGROUND_TRACK_BUFFERING_SIZE		MUSIC_BUFFER_SIZE*4+4000

static qboolean s_bgTrackPaused = qfalse;  // the track is manually paused
static qboolean s_bgTrackLocked = qfalse;  // the track is blocked by the game (e.g. the window's minimized)
static qboolean s_bgTrackBuffering = qfalse;

/*
* S_AllocTrack
*/
static bgTrack_t *S_AllocTrack( const char *filename )
{
	bgTrack_t *track;

	track = S_Malloc( sizeof( *track ) + strlen( filename ) + 1 );
	track->stream = NULL;
	track->ignore = qfalse;
	track->filename = (char *)((qbyte *)track + sizeof( *track ));
	strcpy( track->filename, filename );
	track->isUrl = trap_FS_IsUrl( filename );
	track->anext = s_bgTrackHead;
	s_bgTrackHead = track;

	return track;
}

/*
* S_ValidMusicFile
*/
static qboolean S_ValidMusicFile( bgTrack_t *track )
{
	return (track->stream != NULL) && ( !track->isUrl || !S_EoStream( track->stream ) );
}

/*
* S_CloseMusicTrack
*/
static void S_CloseMusicTrack( bgTrack_t *track )
{
	if( !track->stream )
		return;

	S_CloseStream( track->stream );
	track->stream = NULL;
}

/*
* S_OpenMusicTrack
*/
static qboolean S_OpenMusicTrack( bgTrack_t *track )
{
	const char *filename = track->filename;

	if( track->ignore )
		return qfalse;

mark0:
	s_bgTrackBuffering = qfalse;

	if( !track->stream )
	{
		qboolean delay = qfalse;

		track->stream = S_OpenStream( filename, &delay );
		if( track->stream && delay )
		{
			// let the background track buffer for a while
			Com_Printf( "S_OpenMusicTrack: buffering %s...\n", track->filename );
			s_bgTrackBuffering = qtrue;
		}
	}
	else
	{
		if( !S_ResetStream( track->stream ) )
		{
			// if seeking failed for whatever reason (stream?), try reopening again
			S_CloseMusicTrack( track );
			goto mark0;
		}
	}

	if( !S_ValidMusicFile( track ) )
	{
		S_CloseMusicTrack( track );

		// mark as permanently invalid
		track->ignore = qtrue;
		Com_Printf( "Invalid music file %s\n", filename );
		return qfalse;
	}

	return qtrue;
}

/*
* S_PrevMusicTrack
*/
static bgTrack_t *S_PrevMusicTrack( bgTrack_t *track )
{
	bgTrack_t *prev;

	prev = track ? track->prev : NULL;
	if( prev ) track = prev->next; // HACK to prevent endless loops where original 'track' comes from stack
	while( prev && prev != track )
	{
		if( !prev->ignore )
		{
			// already marked as invalid so don't try opening again
			if( S_OpenMusicTrack( prev ) )
				break;
		}
		prev = prev->next;
	}

	return prev;
}

/*
* S_NextMusicTrack
*/
static bgTrack_t *S_NextMusicTrack( bgTrack_t *track )
{
	bgTrack_t *next;

	next = track ? track->next : NULL;
	if( next ) track = next->prev; // HACK to prevent endless loops where original 'track' comes from stack
	while( next && next != track )
	{
		if( !next->ignore )
		{
			// already marked as invalid so don't try opening again
			if( S_OpenMusicTrack( next ) )
				break;
		}
		next = next->next;
	}

	return next;
}

// =================================

#define MAX_PLAYLIST_ITEMS 1024
typedef struct playlistItem_s
{
	bgTrack_t *track;
	int order;
} playlistItem_t;

/*
* R_SortPlaylistItems
*/
static int R_PlaylistItemCmp( const playlistItem_t *i1, const playlistItem_t *i2 )
{
	if( i1->order > i2->order )
		return 1;
	if( i2->order > i1->order )
		return -1;
	return 0;
}

static void R_SortPlaylistItems( int numItems, playlistItem_t *items )
{
	qsort( items, numItems, sizeof( *items ), (int (*)(const void *, const void *))R_PlaylistItemCmp );
}

/*
* S_ReadPlaylistFile
*/
static qboolean S_ReadPlaylistFile( const char *filename, qboolean shuffle )
{
	int filenum, length;
	char *tmpname = 0;
	size_t tmpname_size = 0;
	char *data, *line, *entry;
	playlistItem_t items[MAX_PLAYLIST_ITEMS];
	int i, numItems = 0;

	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length < 0 )
		return qfalse;

	// load the playlist into memory
	data = S_Malloc( length + 1 );
	trap_FS_Read( data, length, filenum );
	trap_FS_FCloseFile( filenum );

	srand( time( NULL ) );

	while( *data )
	{
		size_t s;

		entry = data;

		// read the whole line
		for( line = data; *line != '\0' && *line != '\n'; line++ );

		// continue reading from the next character, if possible
		data = (*line == '\0' ? line : line + 1);

		*line = '\0';

		// trim whitespaces, tabs, etc
		entry = Q_trim( entry );

		// special M3U entry or comment
		if( !*entry || *entry == '#' )
			continue;

		if( trap_FS_IsUrl( entry ) )
		{
			items[numItems].track = S_AllocTrack( entry );
		}
		else
		{
			// append the entry name to playlist path
			s = strlen( filename ) + 1 + strlen( entry ) + 1;
			if( s > tmpname_size )
			{
				if( tmpname )
					S_Free( tmpname );
				tmpname_size = s;
				tmpname = S_Malloc( tmpname_size );
			}

			Q_strncpyz( tmpname, filename, tmpname_size );
			COM_StripFilename( tmpname );
			Q_strncatz( tmpname, "/", tmpname_size );
			Q_strncatz( tmpname, entry, tmpname_size );
			COM_SanitizeFilePath( tmpname );

			items[numItems].track = S_AllocTrack( tmpname );
		}

		if( ++numItems == MAX_PLAYLIST_ITEMS )
			break;
	}

	if( tmpname )
	{
		S_Free( tmpname );
		tmpname = NULL;
	}

	if( !numItems )
		return qfalse;

	// set the playing order
	for( i = 0; i < numItems; i++ )
		items[i].order = (shuffle ? (rand() % numItems) : i);

	// sort the playlist
	R_SortPlaylistItems( numItems, items );

	// link the playlist
	s_bgTrack = items[0].track;
	for( i = 1; i < numItems; i++ )
	{
		items[i-1].track->next = items[i].track;
		items[i].track->prev = items[i-1].track;
	}
	items[numItems-1].track->next = items[0].track;
	items[0].track->prev = items[numItems-1].track;

	return qtrue;
}

/*
* S_AdvanceBackgroundTrack
*/
static qboolean S_AdvanceBackgroundTrack( int n )
{
	bgTrack_t *track;

	if( n < 0 )
		track = S_PrevMusicTrack( s_bgTrack );
	else
		track = S_NextMusicTrack( s_bgTrack );

	if( track && track != s_bgTrack )
	{
		if( s_bgTrack->isUrl )
			S_CloseMusicTrack( s_bgTrack );
		s_bgTrack = track;
		return qtrue;
	}

	return qfalse;
}

// =================================

/*
* Local helper functions
*/

static void music_source_get( void )
{
	// Allocate a source at high priority
	src = S_AllocSource( SRCPRI_STREAM, -2, 0 );
	if( !src )
		return;

	S_LockSource( src );
	source = S_GetALSource( src );

	qalSource3f( source, AL_POSITION, 0.0, 0.0, 0.0 );
	qalSource3f( source, AL_VELOCITY, 0.0, 0.0, 0.0 );
	qalSource3f( source, AL_DIRECTION, 0.0, 0.0, 0.0 );
	qalSourcef( source, AL_ROLLOFF_FACTOR, 0.0 );
	qalSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
	qalSourcef( source, AL_GAIN, s_musicvolume->value );
}

static void music_source_free( void )
{
	if( src )
		S_UnlockSource( src );
	source = 0;
	src = NULL;
}

static qboolean music_process( ALuint b )
{
	int l = 0;
	ALuint format;
	ALenum error;
	snd_stream_t *music_stream;

start:
	if( s_bgTrackBuffering )
		return qtrue;

	music_stream = s_bgTrack->stream;
	if( music_stream ) {
		l = S_ReadStream( music_stream, MUSIC_BUFFER_SIZE, decode_buffer );
	}
	else {
		l = 0;
	}

	if( !l )
	{
		bgTrack_t *cur;

		cur = s_bgTrack;
		if( !S_AdvanceBackgroundTrack( 1 ) )
		{
			if( !S_ValidMusicFile( s_bgTrack ) )
				return qfalse;
		}
		else
		{
			// we've advanced to the next track, close this one
			S_CloseMusicTrack( cur );
			goto start;
		}

		if( !S_ResetStream( music_stream ) )
		{
			// if failed, close the track?
			return qfalse;
		}

		goto start;
	}

	format = S_SoundFormat( music_stream->info.width, music_stream->info.channels );
	qalBufferData( b, format, decode_buffer, l, music_stream->info.rate );
	if( ( error = qalGetError() ) != AL_NO_ERROR )
		return qfalse;

	return qtrue;
}

/*
* Sound system wide functions (snd_loc.h)
*/

void S_UpdateMusic( void )
{
	int i, processed;
	ALint state;
	ALenum error;
	ALuint b;
	int num_processed_buffers;
	ALuint processed_buffers[MUSIC_BUFFERS];

	if( !s_bgTrack )
		return;
	if( !s_musicvolume->value && !s_bgTrack->isUrl )
		return;
	if( s_bgTrackPaused || s_bgTrackLocked )
		return;

	if( s_bgTrackBuffering )
	{
		if( S_EoStream( s_bgTrack->stream ) ) {
			// we should now advance to the next track
			S_CloseMusicTrack( s_bgTrack );
		}
		else {
			if( S_FTellSteam( s_bgTrack->stream ) < BACKGROUND_TRACK_BUFFERING_SIZE )
				return;

			// in case we delayed openening to let the stream be cached for a while,
			// start actually reading from it now
			if( !S_ContOpenStream( s_bgTrack->stream ) ) {
				// let music_process do the dirty job of advancing to the next track
				S_CloseMusicTrack( s_bgTrack );
				s_bgTrack->ignore = qtrue;
			}
		}
		s_bgTrackBuffering = qfalse;
	}

	if( !queued_buffers )
	{
		// if we haven't queued any buffers yet, do it now
		num_processed_buffers = MUSIC_BUFFERS;
		memcpy( processed_buffers, buffers, sizeof( buffers ) );
	}
	else
	{
		num_processed_buffers = 0;

		processed = 0;
		qalGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );
		while( processed-- )
		{
			qalSourceUnqueueBuffers( source, 1, &b );
			processed_buffers[num_processed_buffers++] = b;
		}
	}

	for( i = 0; i < num_processed_buffers; i++ )
	{
		b = processed_buffers[i];
		if( !music_process( b ) )
		{
			Com_Printf( "Error processing music data\n" );
			S_StopBackgroundTrack();
			return;
		}

		qalSourceQueueBuffers( source, 1, &b );
		if( ( error = qalGetError() ) != AL_NO_ERROR )
		{
			Com_Printf( "Couldn't queue music data (%s)\n", S_ErrorMessage( error ) );
			S_StopBackgroundTrack();
			return;
		}
	}

	// If it's not still playing, give it a kick
	qalGetSourcei( source, AL_SOURCE_STATE, &state );
	if( !queued_buffers || state != AL_PLAYING )
	{
		queued_buffers = qtrue;
		qalSourcePlay( source );
	}

	if( s_musicvolume->modified )
		qalSourcef( source, AL_GAIN, s_musicvolume->value );
}

/*
* Global functions (sound.h)
*/

void S_StartBackgroundTrack( const char *intro, const char *loop )
{
	int count;
	const char *ext;
	bgTrack_t *t, f;
	bgTrack_t *introTrack, *loopTrack;
	ALenum error;
	int mode = 0;

	// Stop any existing music that might be playing
	S_StopBackgroundTrack();

	if( !intro || !intro[0] )
		return;

	s_bgTrackPaused = qfalse;

	ext = COM_FileExtension( intro );
	if( ext && !Q_stricmp( ext, ".m3u" ) )
	{
		// mode bits:
		// 1 - shuffle
		// 2 - loop the selected track
		if( loop && loop[0] )
			mode = atoi( loop );

		if( S_ReadPlaylistFile( intro, mode & 1 ? qtrue : qfalse ) )
			goto start_playback;
	}

	// the intro track loops unless another loop track has been specified
	introTrack = S_AllocTrack( intro );
	introTrack->next = introTrack->prev = introTrack;

	if( loop && loop[0] && Q_stricmp( intro, loop ) )
	{
		loopTrack = S_AllocTrack( loop );
		if( S_OpenMusicTrack( loopTrack ) )
		{
			S_CloseMusicTrack( loopTrack );
			loopTrack->next = introTrack->next = introTrack->prev = loopTrack;
			loopTrack->prev = introTrack;
		}
	}

	s_bgTrack = introTrack;

start_playback:
	// this effectively precaches the first 15 scheduled tracks in the playlist
	for( count = 0, t = s_bgTrack; count < 15 && t; count++ )
	{
		if( !t->isUrl )
		{
			S_OpenMusicTrack( t );

			if( t->next == t || t->next == s_bgTrack )
				break; // break on an endless loop or full cycle
			if( !t->ignore && ( mode & 2 ) )
			{
				// no point in precaching the whole playlist when we're only going
				// to loop one single track
				break;
			}
		}
		t = t->next;
	}

	// start playback with the first valid track
	if( count > 1 )
	{
		memset( &f, 0, sizeof( f ) );
		f.next = s_bgTrack;
		s_bgTrack = S_NextMusicTrack( &f );
	}
	else if( s_bgTrack && s_bgTrack->isUrl )
	{
		S_OpenMusicTrack( s_bgTrack );
	}

	if( !s_bgTrack || s_bgTrack->ignore )
	{
		S_StopBackgroundTrack();
		return;
	}

	if( mode & 2 )
	{
		// loop the same track over and over
		s_bgTrack->next = s_bgTrack->prev = s_bgTrack;
	}

	music_source_get();
	if( !src )
	{
		Com_Printf( "Error couldn't get source for music\n" );
		S_StopBackgroundTrack();
		return;
	}

	alloced_buffers = qfalse;
	queued_buffers = qfalse;

	qalGenBuffers( MUSIC_BUFFERS, buffers );
	if( ( error = qalGetError() ) != AL_NO_ERROR )
	{
		Com_Printf( "Error couldn't generate music buffers (%s)\n", S_ErrorMessage( error ) );
		S_StopBackgroundTrack();
		return;
	}

	alloced_buffers = qtrue;
}

void S_StopBackgroundTrack( void )
{
	bgTrack_t *next;

	if( source )
		qalSourceStop( source );

	if( alloced_buffers ) {
		qalSourceUnqueueBuffers( source, MUSIC_BUFFERS, buffers );
		qalDeleteBuffers( MUSIC_BUFFERS, buffers );
		alloced_buffers = queued_buffers = qfalse;
	}

	music_source_free();

	qalGetError();

	while( s_bgTrackHead )
	{
		next = s_bgTrackHead->anext;

		S_CloseMusicTrack( s_bgTrackHead );
		S_Free( s_bgTrackHead );

		s_bgTrackHead = next;
	}

	s_bgTrack = NULL;
	s_bgTrackHead = NULL;

	s_bgTrackBuffering = qfalse;
	
	s_bgTrackPaused = qfalse;
}

/*
* S_PrevBackgroundTrack
*/
void S_PrevBackgroundTrack( void )
{
	S_AdvanceBackgroundTrack( -1 );
}

/*
* S_NextBackgroundTrack
*/
void S_NextBackgroundTrack( void )
{
	S_AdvanceBackgroundTrack(  1 );
}

/*
* S_PauseBackgroundTrack
*/
void S_PauseBackgroundTrack( void )
{
	if( !s_bgTrack ) {
		return;
	}

	// in case of a streaming URL, reset the stream
	if( s_bgTrack->isUrl ) {
		if( s_bgTrackPaused ) {
			S_OpenMusicTrack( s_bgTrack );
		}
		else {
			S_CloseMusicTrack( s_bgTrack );
		}
	}

	s_bgTrackPaused = !s_bgTrackPaused;
}

/*
* S_LockBackgroundTrack
*/
void S_LockBackgroundTrack( qboolean lock )
{
	if( s_bgTrack && !s_bgTrack->isUrl ) {
		s_bgTrackLocked = lock;
	} else {
		s_bgTrackLocked = qfalse;
	}
}
