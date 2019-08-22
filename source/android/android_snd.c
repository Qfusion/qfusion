/*
Copyright (C) 2015 SiPlus, Chasseur de bots

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

#include "../snd_qf/snd_local.h"
#include <SLES/OpenSLES.h>

static cvar_t *s_bits = NULL;
static cvar_t *s_channels = NULL;

static SLObjectItf snddma_android_engine = NULL;
static SLObjectItf snddma_android_outputMix = NULL;
static SLObjectItf snddma_android_player = NULL;
static SLBufferQueueItf snddma_android_bufferQueue;
static SLPlayItf snddma_android_play;

static struct qmutex_s *snddma_android_mutex = NULL;

static int snddma_android_pos;
static int snddma_android_size;

void S_Activate( bool active ) {
	if( active ) {
		memset( dma.buffer, ( dma.samplebits == 8 ) ? 128 : 0, snddma_android_size * 2 );
		( *snddma_android_bufferQueue )->Enqueue( snddma_android_bufferQueue, dma.buffer, snddma_android_size );
		( *snddma_android_play )->SetPlayState( snddma_android_play, SL_PLAYSTATE_PLAYING );
	} else {
		if( s_globalfocus->integer ) {
			return;
		}
		( *snddma_android_play )->SetPlayState( snddma_android_play, SL_PLAYSTATE_STOPPED );
		( *snddma_android_bufferQueue )->Clear( snddma_android_bufferQueue );
	}
}

static void SNDDMA_Android_Callback( SLBufferQueueItf bq, void *context ) {
	uint8_t *buffer2;

	trap_Mutex_Lock( snddma_android_mutex );

	buffer2 = ( uint8_t * )dma.buffer + snddma_android_size;
	memcpy( buffer2, dma.buffer, snddma_android_size );
	( *bq )->Enqueue( bq, buffer2, snddma_android_size );
	memset( dma.buffer, ( dma.samplebits == 8 ) ? 128 : 0, snddma_android_size );
	snddma_android_pos += dma.samples;

	trap_Mutex_Unlock( snddma_android_mutex );
}

static const char *SNDDMA_Android_Init( void ) {
	SLresult result;

	SLEngineItf engine;

	int freq;

	SLDataLocator_BufferQueue sourceLocator;
	SLDataFormat_PCM sourceFormat;
	SLDataSource source;

	SLDataLocator_OutputMix sinkLocator;
	SLDataSink sink;

	SLInterfaceID audioPlayerInterfaceIDs[] = { SL_IID_BUFFERQUEUE, SL_IID_PLAY };
	SLboolean audioPlayerInterfacesRequired[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

	int samples;

	result = slCreateEngine( &snddma_android_engine, 0, NULL, 0, NULL, NULL );
	if( result != SL_RESULT_SUCCESS ) {
		return "slCreateEngine";
	}
	result = ( *snddma_android_engine )->Realize( snddma_android_engine, SL_BOOLEAN_FALSE );
	if( result != SL_RESULT_SUCCESS ) {
		return "engine->Realize";
	}
	result = ( *snddma_android_engine )->GetInterface( snddma_android_engine, SL_IID_ENGINE, &engine );
	if( result != SL_RESULT_SUCCESS ) {
		return "engine->GetInterface(ENGINE)";
	}

	result = ( *engine )->CreateOutputMix( engine, &snddma_android_outputMix, 0, NULL, NULL );
	if( result != SL_RESULT_SUCCESS ) {
		return "engine->CreateOutputMix";
	}
	result = ( *snddma_android_outputMix )->Realize( snddma_android_outputMix, SL_BOOLEAN_FALSE );
	if( result != SL_RESULT_SUCCESS ) {
		return "outputMix->Realize";
	}

	if( s_khz->integer >= 44 ) {
		freq = 44100;
	} else if( s_khz->integer >= 22 ) {
		freq = 22050;
	} else {
		freq = 11025;
	}

	sourceLocator.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
	sourceLocator.numBuffers = 2;
	sourceFormat.formatType = SL_DATAFORMAT_PCM;
	sourceFormat.numChannels = Q_bound( 1, s_channels->integer, 2 );
	sourceFormat.samplesPerSec = freq * 1000;
	sourceFormat.bitsPerSample = ( ( s_bits->integer >= 16 ) ? 16 : 8 );
	sourceFormat.containerSize = sourceFormat.bitsPerSample;
	sourceFormat.channelMask = ( ( sourceFormat.numChannels == 2 ) ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER );
	sourceFormat.endianness = SL_BYTEORDER_LITTLEENDIAN;
	source.pLocator = &sourceLocator;
	source.pFormat = &sourceFormat;

	sinkLocator.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	sinkLocator.outputMix = snddma_android_outputMix;
	sink.pLocator = &sinkLocator;
	sink.pFormat = NULL;

	result = ( *engine )->CreateAudioPlayer( engine, &snddma_android_player, &source, &sink, 1, audioPlayerInterfaceIDs, audioPlayerInterfacesRequired );
	if( result != SL_RESULT_SUCCESS ) {
		return "engine->CreateAudioPlayer";
	}
	result = ( *snddma_android_player )->Realize( snddma_android_player, SL_BOOLEAN_FALSE );
	if( result != SL_RESULT_SUCCESS ) {
		return "player->Realize";
	}
	result = ( *snddma_android_player )->GetInterface( snddma_android_player, SL_IID_BUFFERQUEUE, &snddma_android_bufferQueue );
	if( result != SL_RESULT_SUCCESS ) {
		return "player->GetInterface(BUFFERQUEUE)";
	}
	result = ( *snddma_android_player )->GetInterface( snddma_android_player, SL_IID_PLAY, &snddma_android_play );
	if( result != SL_RESULT_SUCCESS ) {
		return "player->GetInterface(PLAY)";
	}
	result = ( *snddma_android_bufferQueue )->RegisterCallback( snddma_android_bufferQueue, SNDDMA_Android_Callback, NULL );
	if( result != SL_RESULT_SUCCESS ) {
		return "bufferQueue->RegisterCallback";
	}

	if( freq <= 11025 ) {
		samples = 1024;
	} else if( freq <= 22050 ) {
		samples = 2048;
	} else {
		samples = 4096;
	}

	dma.channels = sourceFormat.numChannels;
	dma.samples = samples * sourceFormat.numChannels;
	dma.submission_chunk = 1;
	dma.samplebits = sourceFormat.bitsPerSample;
	dma.speed = freq;
	dma.msec_per_sample = 1000.0 / freq;
	snddma_android_size = dma.samples * ( sourceFormat.bitsPerSample >> 3 );
	dma.buffer = malloc( snddma_android_size * 2 );
	if( !dma.buffer ) {
		return "malloc";
	}

	snddma_android_mutex = trap_Mutex_Create();

	snddma_android_pos = 0;

	S_Activate( true );

	return NULL;
}

bool SNDDMA_Init( void *hwnd, bool verbose ) {
	const char *initError;

	if( verbose ) {
		Com_Printf( "OpenSL ES audio device initializing...\n" );
	}

	if( !s_bits ) {
		s_bits = trap_Cvar_Get( "s_bits", "16", CVAR_ARCHIVE | CVAR_LATCH_SOUND );
		s_channels = trap_Cvar_Get( "s_channels", "2", CVAR_ARCHIVE | CVAR_LATCH_SOUND );
	}

	initError = SNDDMA_Android_Init();
	if( initError ) {
		Com_Printf( "SNDDMA_Init: %s failed.\n", initError );
		SNDDMA_Shutdown( verbose );
		return false;
	}

	if( verbose ) {
		Com_Printf( "OpenSL ES audio initialized.\n" );
	}

	return true;
}

int SNDDMA_GetDMAPos( void ) {
	return snddma_android_pos;
}

void SNDDMA_Shutdown( bool verbose ) {
	if( verbose ) {
		Com_Printf( "Closing OpenSL ES audio device...\n" );
	}

	if( snddma_android_player ) {
		( *snddma_android_player )->Destroy( snddma_android_player );
		snddma_android_player = NULL;
	}
	if( snddma_android_outputMix ) {
		( *snddma_android_outputMix )->Destroy( snddma_android_outputMix );
		snddma_android_outputMix = NULL;
	}
	if( snddma_android_engine ) {
		( *snddma_android_engine )->Destroy( snddma_android_engine );
		snddma_android_engine = NULL;
	}

	if( dma.buffer ) {
		free( dma.buffer );
		dma.buffer = NULL;
	}

	if( snddma_android_mutex ) {
		trap_Mutex_Destroy( &snddma_android_mutex );
	}

	if( verbose ) {
		Com_Printf( "OpenSL ES audio device shut down.\n" );
	}
}

void SNDDMA_Submit( void ) {
	trap_Mutex_Unlock( snddma_android_mutex );
}

void SNDDMA_BeginPainting( void ) {
	trap_Mutex_Lock( snddma_android_mutex );
}
