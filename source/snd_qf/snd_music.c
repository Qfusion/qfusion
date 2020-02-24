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

#define BACKGROUND_TRACK_PRELOAD_MSEC       200

bgTrack_t *s_bgTrack;
bgTrack_t *s_bgTrackHead;
static bool s_bgTrackPaused = false;  // the track is manually paused
static int s_bgTrackLocked = 0;       // the track is blocked by the game (e.g. the window's minimized)
static bool s_bgTrackMuted = false;
static volatile bool s_bgTrackBuffering = false;
static volatile bool s_bgTrackLoading = false; // unset by s_bgOpenThread when finished loading
static struct qthread_s *s_bgOpenThread;

/*
* S_BackgroundTrack_FindNextChunk
*/
static bool S_BackgroundTrack_FindNextChunk( char *name, int *last_chunk, int file ) {
	char chunkName[4];
	int iff_chunk_len;

	while( 1 ) {
		trap_FS_Seek( file, *last_chunk, FS_SEEK_SET );

		if( trap_FS_Eof( file ) ) {
			return false; // didn't find the chunk

		}
		trap_FS_Seek( file, 4, FS_SEEK_CUR );
		if( trap_FS_Read( &iff_chunk_len, sizeof( iff_chunk_len ), file ) < 4 ) {
			return false;
		}
		iff_chunk_len = LittleLong( iff_chunk_len );
		if( iff_chunk_len < 0 ) {
			return false; // didn't find the chunk

		}
		trap_FS_Seek( file, -8, FS_SEEK_CUR );
		*last_chunk = trap_FS_Tell( file ) + 8 + ( ( iff_chunk_len + 1 ) & ~1 );
		if( trap_FS_Read( chunkName, 4, file ) < 4 ) {
			return false;
		}
		if( !strncmp( chunkName, name, 4 ) ) {
			return true;
		}
	}
}

/*
* S_BackgroundTrack_GetWavinfo
*/
static int S_BackgroundTrack_GetWavinfo( const char *name, wavinfo_t *info ) {
	short t;
	int samples, file;
	int iff_data, last_chunk;
	char chunkName[4];

	last_chunk = 0;
	memset( info, 0, sizeof( wavinfo_t ) );

	trap_FS_FOpenFile( name, &file, FS_READ );
	if( !file ) {
		return 0;
	}

	// find "RIFF" chunk
	if( !S_BackgroundTrack_FindNextChunk( "RIFF", &last_chunk, file ) ) {
		//Com_Printf( "Missing RIFF chunk\n" );
		return 0;
	}

	trap_FS_Read( chunkName, 4, file );
	if( !strncmp( chunkName, "WAVE", 4 ) ) {
		Com_Printf( "Missing WAVE chunk\n" );
		return 0;
	}

	// get "fmt " chunk
	iff_data = trap_FS_Tell( file ) + 4;
	last_chunk = iff_data;
	if( !S_BackgroundTrack_FindNextChunk( "fmt ", &last_chunk, file ) ) {
		Com_Printf( "Missing fmt chunk\n" );
		return 0;
	}

	trap_FS_Read( chunkName, 4, file );

	trap_FS_Read( &t, sizeof( t ), file );
	if( LittleShort( t ) != 1 ) {
		Com_Printf( "Microsoft PCM format only\n" );
		return 0;
	}

	trap_FS_Read( &t, sizeof( t ), file );
	info->channels = LittleShort( t );

	trap_FS_Read( &info->rate, sizeof( info->rate ), file );
	info->rate = LittleLong( info->rate );

	trap_FS_Seek( file, 4 + 2, FS_SEEK_CUR );

	trap_FS_Read( &t, sizeof( t ), file );
	info->width = LittleShort( t ) / 8;

	// find data chunk
	last_chunk = iff_data;
	if( !S_BackgroundTrack_FindNextChunk( "data", &last_chunk, file ) ) {
		Com_Printf( "Missing data chunk\n" );
		return 0;
	}

	trap_FS_Read( &samples, sizeof( samples ), file );
	info->samples = LittleLong( samples ) / info->width / info->channels;

	info->dataofs = trap_FS_Tell( file );

	return file;
}

/*
* S_BackgroundTrack_OpenWav
*/
static bool S_BackgroundTrack_OpenWav( struct bgTrack_s *track ) {
	track->file = S_BackgroundTrack_GetWavinfo( track->filename, &track->info );
	return ( track->file != 0 );
}

// =================================

/*
* S_AllocTrack
*/
static bgTrack_t *S_AllocTrack( const char *filename ) {
	bgTrack_t *track;

	track = S_Malloc( sizeof( *track ) + strlen( filename ) + 1 );
	track->ignore = false;
	track->filename = (char *)( (uint8_t *)track + sizeof( *track ) );
	strcpy( track->filename, filename );
	track->muteOnPause = false;
	track->anext = s_bgTrackHead;
	s_bgTrackHead = track;

	return track;
}

/*
* S_CloseMusicTrack
*/
static void S_CloseMusicTrack( bgTrack_t *track ) {
	if( !track->file ) {
		return;
	}

	if( track->close ) {
		track->close( track );
	} else {
		trap_FS_FCloseFile( track->file );
	}
	track->file = 0;
}

/*
* S_OpenMusicTrack
*/
static bool S_OpenMusicTrack( bgTrack_t *track ) {
	if( track->ignore ) {
		return false;
	}

mark0:
	if( !track->file ) {
		bool opened;

		memset( &track->info, 0, sizeof( track->info ) );

		// try ogg
		track->open = SNDOGG_OpenTrack;
		opened = track->open( track );

		// try wav
		if( !opened ) {
			track->open = S_BackgroundTrack_OpenWav;
			opened = track->open( track );
		}
	} else {
		bool ok;

		if( track->reset ) {
			ok = track->reset( track );
		} else {
			ok = trap_FS_Seek( track->file, track->info.dataofs, FS_SEEK_SET ) == 0;
		}

		// if seeking failed for whatever reason (stream?), try reopening again
		if( !ok ) {
			S_CloseMusicTrack( track );
			goto mark0;
		}
	}

	return true;
}

/*
* S_PrevMusicTrack
*/
static bgTrack_t *S_PrevMusicTrack( bgTrack_t *track ) {
	bgTrack_t *prev;

	prev = track ? track->prev : NULL;
	if( prev ) {
		track = prev->next;        // HACK to prevent endless loops where original 'track' comes from stack
	}
	while( prev && prev != track ) {
		if( !prev->ignore ) {
			break;
		}
		prev = prev->next;
	}

	return prev;
}

/*
* S_NextMusicTrack
*/
static bgTrack_t *S_NextMusicTrack( bgTrack_t *track ) {
	bgTrack_t *next;

	next = track ? track->next : NULL;
	if( next ) {
		track = next->prev;        // HACK to prevent endless loops where original 'track' comes from stack
	}
	while( next && next != track ) {
		if( !next->ignore ) {
			break;
		}
		next = next->next;
	}

	return next;
}

// =================================

#define MAX_PLAYLIST_ITEMS 1024
typedef struct playlistItem_s {
	bgTrack_t *track;
	int order;
} playlistItem_t;

/*
* R_SortPlaylistItems
*/
static int R_PlaylistItemCmp( const playlistItem_t *i1, const playlistItem_t *i2 ) {
	if( i1->order > i2->order ) {
		return 1;
	}
	if( i2->order > i1->order ) {
		return -1;
	}
	return 0;
}

void R_SortPlaylistItems( int numItems, playlistItem_t *items ) {
	qsort( items, numItems, sizeof( *items ), ( int ( * )( const void *, const void * ) )R_PlaylistItemCmp );
}

/*
* S_ReadPlaylistFile
*/
static bgTrack_t *S_ReadPlaylistFile( const char *filename, bool shuffle, bool loop ) {
	int filenum, length;
	char *tmpname = 0;
	size_t tmpname_size = 0;
	char *data, *line, *entry;
	playlistItem_t items[MAX_PLAYLIST_ITEMS];
	int i, numItems = 0;

	length = trap_FS_FOpenFile( filename, &filenum, FS_READ );
	if( length < 0 ) {
		return NULL;
	}

	// load the playlist into memory
	data = S_Malloc( length + 1 );
	trap_FS_Read( data, length, filenum );
	trap_FS_FCloseFile( filenum );

	srand( time( NULL ) );

	while( *data ) {
		size_t s;

		entry = data;

		// read the whole line
		for( line = data; *line != '\0' && *line != '\n'; line++ ) ;

		// continue reading from the next character, if possible
		data = ( *line == '\0' ? line : line + 1 );

		*line = '\0';

		// trim whitespaces, tabs, etc
		entry = Q_trim( entry );

		// special M3U entry or comment
		if( !*entry || *entry == '#' ) {
			continue;
		}

		if( trap_FS_IsUrl( entry ) ) {
			items[numItems].track = S_AllocTrack( entry );
		} else {
			// append the entry name to playlist path
			s = strlen( filename ) + 1 + strlen( entry ) + 1;
			if( s > tmpname_size ) {
				if( tmpname ) {
					S_Free( tmpname );
				}
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

		if( ++numItems == MAX_PLAYLIST_ITEMS ) {
			break;
		}
	}

	if( tmpname ) {
		S_Free( tmpname );
		tmpname = NULL;
	}

	if( !numItems ) {
		return NULL;
	}

	// set the playing order
	for( i = 0; i < numItems; i++ )
		items[i].order = ( shuffle ? ( rand() % numItems ) : i );

	// sort the playlist
	R_SortPlaylistItems( numItems, items );

	// link the playlist
	for( i = 1; i < numItems; i++ ) {
		items[i - 1].track->next = items[i].track;
		items[i].track->prev = items[i - 1].track;
		items[i].track->loop = loop;
	}
	items[numItems - 1].track->next = items[0].track;
	items[0].track->prev = items[numItems - 1].track;
	items[0].track->loop = loop;

	return items[0].track;
}

// =================================

/*
* S_OpenBackgroundTrackProc
*/
static void *S_OpenBackgroundTrackProc( void *ptrack ) {
	bgTrack_t *track = ptrack;

	S_OpenMusicTrack( track );

	s_bgTrack = track;
	s_bgTrackLoading = false;
	return NULL;
}

/*
* S_OpenBackgroundTrackTask
*/
static void S_OpenBackgroundTrackTask( bgTrack_t *track ) {
	s_bgTrackLoading = true;
	s_bgTrackBuffering = false;
	s_bgOpenThread = trap_Thread_Create( S_OpenBackgroundTrackProc, track );
}

/*
* S_CloseBackgroundTrackTask
*/
static void S_CloseBackgroundTrackTask( void ) {
	s_bgTrackBuffering = false;
	trap_Thread_Join( s_bgOpenThread );
	s_bgOpenThread = NULL;
}

/*
* S_StartBackgroundTrack
*/
void S_StartBackgroundTrack( const char *intro, const char *loop, int mode ) {
	const char *ext;
	bgTrack_t *introTrack, *loopTrack;
	bgTrack_t *firstTrack = NULL;

	S_StopBackgroundTrack();

	if( !intro || !intro[0] ) {
		return;
	}

	s_bgTrackMuted = false;
	s_bgTrackPaused = false;

	ext = COM_FileExtension( intro );
	if( ext && !Q_stricmp( ext, ".m3u" ) ) {
		// mode bits:
		// 1 - shuffle
		// 2 - loop the selected track
		// 4 - stream (even if muted)

		firstTrack = S_ReadPlaylistFile( intro,
										 mode & 1 ? true : false, mode & 2 ? true : false );
		if( firstTrack ) {
			goto start_playback;
		}
	}

	// the intro track loops unless another loop track has been specified
	introTrack = S_AllocTrack( intro );
	introTrack->loop = true;
	introTrack->next = introTrack->prev = introTrack;
	introTrack->muteOnPause = ( mode & 4 ) != 0;

	if( loop && loop[0] && Q_stricmp( intro, loop ) ) {
		loopTrack = S_AllocTrack( loop );
		if( S_OpenMusicTrack( loopTrack ) ) {
			S_CloseMusicTrack( loopTrack );

			introTrack->next = introTrack->prev = loopTrack;
			introTrack->loop = false;

			loopTrack->loop = true;
			loopTrack->muteOnPause = ( mode & 4 ) != 0;
			loopTrack->next = loopTrack->prev = loopTrack;
		}
	}

	firstTrack = introTrack;

start_playback:

	if( !firstTrack || firstTrack->ignore ) {
		S_StopBackgroundTrack();
		return;
	}

	S_OpenBackgroundTrackTask( firstTrack );
}

/*
* S_StopBackgroundTrack
*/
void S_StopBackgroundTrack( void ) {
	bgTrack_t *next;

	S_CloseBackgroundTrackTask();

	while( s_bgTrackHead ) {
		next = s_bgTrackHead->anext;

		S_CloseMusicTrack( s_bgTrackHead );
		S_Free( s_bgTrackHead );

		s_bgTrackHead = next;
	}

	s_bgTrack = NULL;
	s_bgTrackHead = NULL;

	s_bgTrackMuted = false;
	s_bgTrackPaused = false;
}

/*
* S_AdvanceBackgroundTrack
*/
static bool S_AdvanceBackgroundTrack( int n ) {
	bgTrack_t *track;

	if( n < 0 ) {
		track = S_PrevMusicTrack( s_bgTrack );
	} else {
		track = S_NextMusicTrack( s_bgTrack );
	}

	if( track && track != s_bgTrack ) {
		S_CloseBackgroundTrackTask();
		S_CloseMusicTrack( s_bgTrack );
		S_OpenBackgroundTrackTask( track );
		return true;
	}

	return false;
}

/*
* S_PrevBackgroundTrack
*/
void S_PrevBackgroundTrack( void ) {
	S_AdvanceBackgroundTrack( -1 );
}

/*
* S_NextBackgroundTrack
*/
void S_NextBackgroundTrack( void ) {
	S_AdvanceBackgroundTrack(  1 );
}

/*
* S_PauseBackgroundTrack
*/
void S_PauseBackgroundTrack( void ) {
	if( !s_bgTrack ) {
		return;
	}

	// in case of a streaming URL or video, just mute/unmute so
	// the stream is not interrupted
	if( s_bgTrack->muteOnPause ) {
		s_bgTrackMuted = !s_bgTrackMuted;
		return;
	}

	s_bgTrackPaused = !s_bgTrackPaused;
}

/*
* S_LockBackgroundTrack
*/
void S_LockBackgroundTrack( bool lock ) {
	if( s_bgTrack ) {
		s_bgTrackLocked += lock ? 1 : -1;
		if( s_bgTrackLocked < 0 ) {
			s_bgTrackLocked = 0;
		}
	} else {
		s_bgTrackLocked = 0;
	}
}

//=============================================================================

/*
* byteSwapRawSamples
* Medar: untested
*/
static void byteSwapRawSamples( int samples, int width, int channels, const uint8_t *data ) {
	int i;

	if( LittleShort( 256 ) == 256 ) {
		return;
	}

	if( width != 2 ) {
		return;
	}

	if( channels == 2 ) {
		samples <<= 1;
	}

	for( i = 0; i < samples; i++ )
		( (short *)data )[i] = BigShort( ( (short *)data )[i] );
}

/*
* S_UpdateBackgroundTrack
*/
void S_UpdateBackgroundTrack( void ) {
	int bps;
	int samples, maxSamples;
	int read, maxRead, total;
	float scale;
	uint8_t data[MAX_RAW_SAMPLES * 4];

	if( !s_bgTrack ) {
		return;
	}
	if( !s_musicvolume->value && !s_bgTrack->muteOnPause ) {
		return;
	}
	if( s_bgTrackLoading || s_bgTrackPaused || s_bgTrackLocked > 0 ) {
		return;
	}

	if( !s_bgTrack->info.channels || !s_bgTrack->info.width ) {
		s_bgTrack->ignore = true;
		S_AdvanceBackgroundTrack( 1 );
		return;
	}

	bps = s_bgTrack->info.channels / s_bgTrack->info.width;
	scale = (float)s_bgTrack->info.rate / dma.speed;
	maxSamples = sizeof( data ) / bps;

	while( 1 ) {
		unsigned int rawSamplesLength;

		rawSamplesLength = S_GetRawSamplesLength();
		if( rawSamplesLength >= BACKGROUND_TRACK_PRELOAD_MSEC ) {
			return;
		}

		samples = BACKGROUND_TRACK_PRELOAD_MSEC - rawSamplesLength;
		samples = (float)samples / 1000.0 * s_bgTrack->info.rate / scale;

		if( samples > maxSamples ) {
			samples = maxSamples;
		}
		if( samples > MAX_RAW_SAMPLES ) {
			samples = MAX_RAW_SAMPLES;
		}

		maxRead = samples * s_bgTrack->info.channels * s_bgTrack->info.width;

		total = 0;
		while( total < samples ) {
			if( s_bgTrack->read ) {
				read = s_bgTrack->read( s_bgTrack, data + total, samples - total );
			} else {
				read = trap_FS_Read( data + total, (samples - total) * bps, s_bgTrack->file ) / bps;
			}

			if( !read ) {
				bool ok;

				if( !s_bgTrack->loop ) {
					if( !S_AdvanceBackgroundTrack( 1 ) ) {
						S_StopBackgroundTrack();
						return;
					}
					if( s_bgTrackBuffering || s_bgTrackLoading ) {
						return;
					}
				}

				if( s_bgTrack->reset ) {
					ok = s_bgTrack->reset( s_bgTrack );
				} else {
					ok = trap_FS_Seek( s_bgTrack->file, s_bgTrack->info.dataofs, FS_SEEK_SET ) == 0;
				}

				if( !ok ) {
					// if the seek have failed we're going to loop here forever unless
					// we stop now
					S_StopBackgroundTrack();
					return;
				}
			}

			total += read;
		}

		byteSwapRawSamples( samples, s_bgTrack->info.width,
							s_bgTrack->info.channels, data );

		S_RawSamples2( samples, s_bgTrack->info.rate, s_bgTrack->info.width,
					   s_bgTrack->info.channels, data, s_bgTrackMuted ? 0 : s_musicvolume->value * 255 );
	}
}
