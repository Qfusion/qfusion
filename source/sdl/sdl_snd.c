/*
 * SDL implementation for Warsow from io.q3
 *
 * Adapted by Andreas Schneider <mail@cynapses.org>
 *
 * For config have a look there
 * http://icculus.org/lgfaq/#setthatdriver
 */

/*
 * SDL implementation for Quake 3: Arena's GPL source release.
 *
 * This is a replacement of the Linux/OpenSoundSystem code with
 *  an SDL backend, since it allows us to trivially point just about any
 *  existing 2D audio backend known to man on any platform at the code,
 *  plus it benefits from all of SDL's tapdancing to support buggy drivers,
 *  etc, and gets us free ALSA support, too.
 *
 * This is the best idea for a direct modernization of the Linux sound code
 *  in Quake 3. However, it would be nice to replace this with true 3D
 *  positional audio, compliments of OpenAL...
 *
 * Written by Ryan C. Gordon (icculus@icculus.org). Please refer to
 *    http://icculus.org/quake3/ for the latest version of this code.
 *
 *  Patches and comments are welcome at the above address.
 *
 * I cut-and-pasted this from linux_snd.c, and moved it to SDL line-by-line.
 *  There is probably some cruft that could be removed here.
 *
 * You should define USE_SDL=1 and then add this to the makefile.
 *  USE_SDL will disable the Open Sound System target.
 */

/*
   Original copyright on Q3A sources:
   ===========================================================================
   Copyright (C) 1999-2005 Id Software, Inc.

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

#include <stdlib.h>
#include <stdio.h>

#include <SDL.h>

#include "../snd_qf/snd_local.h"

static int snd_inited = 0;

void S_Activate( bool active ) {
}

/* The audio callback. All the magic happens here. */
static unsigned dmapos = 0;
static unsigned dmasize = 0;
static void sdl_audio_callback( void *userdata, Uint8 *stream, int len ) {
	int pos = dmapos % dmasize;

	if( !snd_inited ) { /* shouldn't happen, but just in case... */
		memset( stream, '\0', len );
		return;
	} else if( len > 0 ) {
		unsigned tobufend = dmasize - pos; /* bytes to buffer's end. */
		unsigned len1 = len;
		int len2 = 0;

		if( len1 > tobufend ) {
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy( stream, dma.buffer + pos, len1 );
		if( len2 <= 0 ) {
			dmapos += len1;
		} else { /* wraparound? */
			memcpy( stream + len1, dma.buffer, len2 );
			dmapos = len2;
		}
	}
}

static void print_audiospec( const char *str, const SDL_AudioSpec *spec ) {
	Com_Printf( "%s:\n", str );

// I'm sorry this is nasty.
#define PRINT_AUDIO_FMT( x )              \
	if( spec->format == x ) {               \
		Com_Printf( "Format: %s\n", #x );} \
	else
	PRINT_AUDIO_FMT( AUDIO_U8 )
	PRINT_AUDIO_FMT( AUDIO_S8 )
	PRINT_AUDIO_FMT( AUDIO_U16LSB )
	PRINT_AUDIO_FMT( AUDIO_S16LSB )
	PRINT_AUDIO_FMT( AUDIO_U16MSB )
	PRINT_AUDIO_FMT( AUDIO_S16MSB )
	Com_Printf( "Format: UNKNOWN\n" );
#undef PRINT_AUDIO_FMT

	Com_Printf( "Freq: %d\n", (int)spec->freq );
	Com_Printf( "Samples: %d\n", (int)spec->samples );
	Com_Printf( "Channels: %d\n", (int)spec->channels );
	Com_Printf( "\n" );
}

bool SNDDMA_Init( void *hwnd, bool verbose ) {
	char drivername[128];
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	int tmp;

	if( snd_inited ) {
		return 1;
	}

	if( verbose ) {
		Com_Printf( "SDL Audio driver initializing...\n" );
	}

	trap_Cvar_Get( "s_bits", "16", CVAR_ARCHIVE | CVAR_LATCH_SOUND );
	trap_Cvar_Get( "s_channels", "2", CVAR_ARCHIVE | CVAR_LATCH_SOUND );

	if( !SDL_WasInit( SDL_INIT_AUDIO ) ) {
		if( verbose ) {
			Com_Printf( "Calling SDL_Init(SDL_INIT_AUDIO)...\n" );
		}
		if( SDL_Init( SDL_INIT_AUDIO ) == -1 ) {
			Com_Printf( "SDL_Init(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError() );
			return false;
		}
		if( verbose ) {
			Com_Printf( "SDL_Init(SDL_INIT_AUDIO) passed.\n" );
		}
	}

#if SDL_VERSION_ATLEAST( 2, 0, 0 )
	if( SDL_GetCurrentAudioDriver() ) {
		Q_strncpyz( drivername, SDL_GetCurrentAudioDriver(), sizeof( drivername ) );
	} else {
		Q_strncpyz( drivername, "(UNKNOWN)", sizeof( drivername ) );
	}
#else
	if( SDL_AudioDriverName( drivername, sizeof( drivername ) ) == NULL ) {
		Q_strncpyz( drivername, "(UNKNOWN)", sizeof( drivername ) );
	}
#endif

	if( verbose ) {
		Com_Printf( "SDL audio driver is \"%s\"\n", drivername );
	}

	memset( &desired, '\0', sizeof( desired ) );
	memset( &obtained, '\0', sizeof( obtained ) );

	if( s_khz->integer == 44 ) {
		desired.freq = 44100;
	} else if( s_khz->integer == 22 ) {
		desired.freq = 22050;
	} else {
		desired.freq = 11025;
	}

	desired.format = (int)trap_Cvar_Value( "s_bits") != 16 ? AUDIO_U8 : AUDIO_S16SYS;

	// I dunno if this is the best idea, but I'll give it a try...
	//  should probably check a cvar for this...
	// just pick a sane default.
	if( desired.freq <= 11025 ) {
		desired.samples = 256;
	} else if( desired.freq <= 22050 ) {
		desired.samples = 512;
	} else if( desired.freq <= 44100 ) {
		desired.samples = 1024;
	} else {
		desired.samples = 2048; // (*shrug*)

	}
	desired.channels = (int)trap_Cvar_Value( "s_channels" );
	desired.callback = sdl_audio_callback;

	if( SDL_OpenAudio( &desired, &obtained ) == -1 ) {
		Com_Printf( "SDL_OpenAudio() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return false;
	}

	if( verbose ) {
		print_audiospec( "Format we requested from SDL audio device", &desired );
		print_audiospec( "Format we actually got", &obtained );
	}

	// dma.samples needs to be big, or id's mixer will just refuse to
	//  work at all; we need to keep it significantly bigger than the
	//  amount of SDL callback samples, and just copy a little each time
	//  the callback runs.
	// 32768 is what the OSS driver filled in here on my system. I don't
	//  know if it's a good value overall, but at least we know it's
	//  reasonable...this is why I let the user override.
	tmp = ( obtained.samples * obtained.channels ) * 4;

	if( tmp & ( tmp - 1 ) ) { // not a power of two? Seems to confuse something.
		int val = 1;
		while( val < tmp )
			val <<= 1;

		val >>= 1;
		if( verbose ) {
			Com_Printf( "WARNING: sdlmixsamps wasn't a power of two (%d), so we made it one (%d).\n", tmp, val );
		}
		tmp = val;
	}

	dmapos = 0;
	dma.samplebits = obtained.format & 0xFF; // first byte of format is bits.
	dma.channels = obtained.channels;
	dma.samples = tmp;
	dma.submission_chunk = 1;
	dma.speed = obtained.freq;
	dma.msec_per_sample = 1000.0 / dma.speed;
	dmasize = ( dma.samples * ( dma.samplebits / 8 ) );
	dma.buffer = calloc( 1, dmasize );

	if( verbose ) {
		Com_Printf( "Starting SDL audio callback...\n" );
	}
	SDL_PauseAudio( 0 ); // start callback.

	if( verbose ) {
		Com_Printf( "SDL audio initialized.\n" );
	}
	snd_inited = 1;
	return true;
}

int SNDDMA_GetDMAPos( void ) {
	return dmapos / ( dma.samplebits / 8 );
}

void SNDDMA_Shutdown( bool verbose ) {
	if( verbose ) {
		Com_Printf( "Closing SDL audio device...\n" );
	}

	SDL_PauseAudio( 1 );
	SDL_CloseAudio();
	SDL_QuitSubSystem( SDL_INIT_AUDIO );

	free( dma.buffer );
	dma.buffer = NULL;
	dmapos = dmasize = 0;
	snd_inited = 0;

	if( verbose ) {
		Com_Printf( "SDL audio device shut down.\n" );
	}
}

/*
   ==============
   SNDDMA_Submit

   Send sound to device if buffer isn't really the dma buffer
   ===============
 */
void SNDDMA_Submit( void ) {
	SDL_UnlockAudio();
}

void SNDDMA_BeginPainting( void ) {
	SDL_LockAudio();
}
