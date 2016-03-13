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
#include "../snd_qf/snd_local.h"
#include <dsound.h>

HRESULT ( WINAPI *pDirectSoundCreate )( GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter );

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400

#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum { SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL } sndinitstat;

cvar_t *s_wavonly;
cvar_t *s_globalfocus;

static bool	dsound_init;
static bool	wav_init;
static bool	snd_isdirect, snd_iswave;

static int sample16;
static unsigned int snd_sent, snd_completed;

/*
* Global variables. Must be visible to window-procedure function
*  so it can unlock and free the data block after it has been played.
*/


HANDLE hData;
HPSTR lpData, lpData2;

HGLOBAL	hWaveHdr;
LPWAVEHDR lpWaveHdr;

HWAVEOUT hWaveOut;

WAVEOUTCAPS wavecaps;

DWORD gSndBufSize;

MMTIME mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;
HWND cl_hwnd;

static bool SNDDMA_InitWav( bool verbose );
static sndinitstat SNDDMA_InitDirect( bool verbose );

static void FreeSound( bool verbose );

static const char *DSoundError( int error )
{
	switch( error )
	{
	case DSERR_BUFFERLOST:
		return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL:
		return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM:
		return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED:
		return "DSERR_PRIOLEVELNEEDED";
	}

	return "unknown";
}

/*
** DS_CreateBuffers
*/
static bool DS_CreateBuffers( void )
{
	DSBUFFERDESC dsbuf;
	DSBCAPS	dsbcaps;
	WAVEFORMATEX format;
	DWORD dwWrite;

	memset( &format, 0, sizeof( format ) );
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;

	if( developer->integer )
		Com_Printf( "Creating DS buffer\n" );

	if( developer->integer )
		Com_Printf( "...setting PRIORITY coop level: " );
	if( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_PRIORITY ) )
	{
		if( developer->integer )
			Com_Printf( "failed\n" );
		FreeSound( false );
		return false;
	}
	if( developer->integer )
		Com_Printf( "ok\n" );

	// create the secondary buffer we'll actually work with
	memset( &dsbuf, 0, sizeof( dsbuf ) );
	dsbuf.dwSize = sizeof( DSBUFFERDESC );
	dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCHARDWARE | DSBCAPS_GLOBALFOCUS;
	dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
	dsbuf.lpwfxFormat = &format;

	memset( &dsbcaps, 0, sizeof( dsbcaps ) );
	dsbcaps.dwSize = sizeof( dsbcaps );

	if( developer->integer )
		Com_Printf( "...creating secondary buffer: " );
	if( DS_OK != pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSBuf, NULL ) )
	{
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE | DSBCAPS_GLOBALFOCUS;

		if( DS_OK != pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSBuf, NULL ) )
		{
			if( developer->integer )
				Com_Printf( "failed\n" );
			FreeSound( false );
			return false;
		}

		if( developer->integer )
			Com_Printf( "ok\n...forced to software\n" );
	}
	else
	{
		if( developer->integer )
			Com_Printf( "ok\n...locked hardware\n" );
	}

	dma.channels = format.nChannels;
	dma.samplebits = format.wBitsPerSample;
	dma.speed = format.nSamplesPerSec;
	dma.msec_per_sample = 1000.0 / dma.speed;

	if( DS_OK != pDSBuf->lpVtbl->GetCaps( pDSBuf, &dsbcaps ) )
	{
		if( developer->integer )
			Com_Printf( "*** GetCaps failed ***\n" );
		FreeSound( false );
		return false;
	}

	// Make sure mixer is active
	if( DS_OK != pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING ) )
	{
		if( developer->integer )
			Com_Printf( "*** Play failed ***\n" );
		FreeSound( false );
		return false;
	}

	if( developer->integer )
		Com_Printf( "   %d channel(s)\n"
		"   %d bits/sample\n"
		"   %d bytes/sec\n",
		dma.channels, dma.samplebits, dma.speed );

	gSndBufSize = dsbcaps.dwBufferBytes;

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->lpVtbl->Stop( pDSBuf );
	pDSBuf->lpVtbl->GetCurrentPosition( pDSBuf, &mmstarttime.u.sample, &dwWrite );
	pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	dma.samples = gSndBufSize/( dma.samplebits/8 );
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = (unsigned char *) lpData;
	sample16 = ( dma.samplebits/8 ) - 1;

	return true;
}

/*
** DS_DestroyBuffers
*/
static void DS_DestroyBuffers( void )
{
	if( developer->integer )
		Com_Printf( "Destroying DS buffers\n" );
	if( pDS )
	{
		if( developer->integer )
			Com_Printf( "...setting NORMAL coop level\n" );
		pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_NORMAL );
	}

	if( pDSBuf )
	{
		if( developer->integer )
			Com_Printf( "...stopping and releasing sound buffer\n" );
		pDSBuf->lpVtbl->Stop( pDSBuf );
		pDSBuf->lpVtbl->Release( pDSBuf );
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if( pDSPBuf && ( pDSBuf != pDSPBuf ) )
	{
		if( developer->integer )
			Com_Printf( "...releasing primary buffer\n" );
		pDSPBuf->lpVtbl->Release( pDSPBuf );
	}
	pDSBuf = NULL;
	pDSPBuf = NULL;

	dma.buffer = NULL;
}

/*
* FreeSound
*/
static void FreeSound( bool verbose )
{
	int i;

	if( verbose )
		Com_Printf( "Shutting down sound system\n" );

	if( pDS )
		DS_DestroyBuffers();

	if( hWaveOut )
	{
		if( verbose )
			Com_Printf( "...resetting waveOut\n" );
		waveOutReset( hWaveOut );

		if( lpWaveHdr )
		{
			if( verbose )
				Com_Printf( "...unpreparing headers\n" );
			for( i = 0; i < WAV_BUFFERS; i++ )
				waveOutUnprepareHeader( hWaveOut, lpWaveHdr+i, sizeof( WAVEHDR ) );
		}

		if( verbose )
			Com_Printf( "...closing waveOut\n" );
		waveOutClose( hWaveOut );

		if( hWaveHdr )
		{
			if( verbose )
				Com_Printf( "...freeing WAV header\n" );
			GlobalUnlock( hWaveHdr );
			GlobalFree( hWaveHdr );
		}

		if( hData )
		{
			if( verbose )
				Com_Printf( "...freeing WAV buffer\n" );
			GlobalUnlock( hData );
			GlobalFree( hData );
		}
	}

	if( pDS )
	{
		if( verbose )
			Com_Printf( "...releasing DS object\n" );
		pDS->lpVtbl->Release( pDS );
	}

	if( hInstDS )
	{
		if( verbose )
			Com_Printf( "...freeing DSOUND.DLL\n" );
		FreeLibrary( hInstDS );
		hInstDS = NULL;
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

/*
* SNDDMA_InitDirect
* 
* Direct-Sound support
*/
sndinitstat SNDDMA_InitDirect( bool verbose )
{
	DSCAPS dscaps;
	HRESULT	hresult;

	dma.channels = 2;
	dma.samplebits = 16;

	if( s_khz->integer == 48 )
		dma.speed = 48000;
	else if( s_khz->integer == 44 )
		dma.speed = 44100;
	else if( s_khz->integer == 22 )
		dma.speed = 22050;
	else
		dma.speed = 11025;
	dma.msec_per_sample = 1000.0 / dma.speed;

	if( verbose )
		Com_Printf( "Initializing DirectSound\n" );

	if( !hInstDS )
	{
		if( verbose )
			Com_Printf( "...loading dsound.dll: " );

		hInstDS = LoadLibrary( "dsound.dll" );

		if( hInstDS == NULL )
		{
			if( verbose )
				Com_Printf( "failed\n" );
			return SIS_FAILURE;
		}

		if( verbose )
			Com_Printf( "ok\n" );
		pDirectSoundCreate = (void *)GetProcAddress( hInstDS, "DirectSoundCreate" );

		if( !pDirectSoundCreate )
		{
			if( verbose )
				Com_Printf( "*** couldn't get DS proc addr ***\n" );
			return SIS_FAILURE;
		}
	}

	if( verbose )
		Com_Printf( "...creating DS object: " );
	while( ( hresult = pDirectSoundCreate( NULL, &pDS, NULL ) ) != DS_OK )
	{
		if( hresult != DSERR_ALLOCATED )
		{
			if( verbose )
				Com_Printf( "failed\n" );
			return SIS_FAILURE;
		}

		if( MessageBox( NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run with no sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION ) != IDRETRY )
		{
			if( verbose )
				Com_Printf( "failed, hardware already in use\n" );
			return SIS_NOTAVAIL;
		}
	}
	if( verbose )
		Com_Printf( "ok\n" );

	dscaps.dwSize = sizeof( dscaps );

	if( DS_OK != pDS->lpVtbl->GetCaps( pDS, &dscaps ) )
	{
		if( verbose )
			Com_Printf( "*** couldn't get DS caps ***\n" );
	}

	if( dscaps.dwFlags & DSCAPS_EMULDRIVER )
	{
		if( verbose )
			Com_Printf( "...no DSound driver found\n" );
		FreeSound( verbose );
		return SIS_FAILURE;
	}

	if( !DS_CreateBuffers() )
		return SIS_FAILURE;

	dsound_init = true;

	if( verbose )
		Com_Printf( "...completed successfully\n" );

	return SIS_SUCCESS;
}

/*
* SNDDM_InitWav
* 
* Crappy windows multimedia base
*/
bool SNDDMA_InitWav( bool verbose )
{
	WAVEFORMATEX format;
	int i;
	HRESULT	hr;

	if( verbose )
		Com_Printf( "Initializing wave sound\n" );

	snd_sent = 0;
	snd_completed = 0;

	dma.channels = 2;
	dma.samplebits = 16;

	if( s_khz->integer == 48 )
		dma.speed = 48000;
	else if( s_khz->integer == 44 )
		dma.speed = 44100;
	else if( s_khz->integer == 22 )
		dma.speed = 22050;
	else
		dma.speed = 11025;
	dma.msec_per_sample = 1000.0 / dma.speed;

	memset( &format, 0, sizeof( format ) );
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	if( verbose )
		Com_Printf( "...opening waveform device: " );
	while( ( hr = waveOutOpen( (LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
		&format,
		0, 0L, CALLBACK_NULL ) ) != MMSYSERR_NOERROR )
	{
		if( hr != MMSYSERR_ALLOCATED )
		{
			if( verbose )
				Com_Printf( "failed\n" );
			return false;
		}

		if( MessageBox( NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run with no sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION ) != IDRETRY )
		{
			if( verbose )
				Com_Printf( "hw in use\n" );
			return false;
		}
	}
	if( verbose )
		Com_Printf( "ok\n" );

	/*
	* Allocate and lock memory for the waveform data. The memory
	* for waveform data must be globally allocated with
	* GMEM_MOVEABLE and GMEM_SHARE flags.

	*/
	if( verbose )
		Com_Printf( "...allocating waveform buffer: " );
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize );
	if( !hData )
	{
		if( verbose )
			Com_Printf( " failed\n" );
		FreeSound( verbose );
		return false;
	}
	if( verbose )
		Com_Printf( "ok\n" );

	if( verbose )
		Com_Printf( "...locking waveform buffer: " );
	lpData = GlobalLock( hData );
	if( !lpData )
	{
		if( verbose )
			Com_Printf( " failed\n" );
		FreeSound( verbose );
		return false;
	}
	memset( lpData, 0, gSndBufSize );
	if( verbose )
		Com_Printf( "ok\n" );

	/*
	* Allocate and lock memory for the header. This memory must
	* also be globally allocated with GMEM_MOVEABLE and
	* GMEM_SHARE flags.
	*/
	if( verbose )
		Com_Printf( "...allocating waveform header: " );
	hWaveHdr = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof( WAVEHDR ) * WAV_BUFFERS );

	if( hWaveHdr == NULL )
	{
		if( verbose )
			Com_Printf( "failed\n" );
		FreeSound( verbose );
		return false;
	}
	if( verbose )
		Com_Printf( "ok\n" );

	if( verbose )
		Com_Printf( "...locking waveform header: " );
	lpWaveHdr = (LPWAVEHDR) GlobalLock( hWaveHdr );

	if( lpWaveHdr == NULL )
	{
		if( verbose )
			Com_Printf( "failed\n" );
		FreeSound( verbose );
		return false;
	}
	memset( lpWaveHdr, 0, sizeof( WAVEHDR ) * WAV_BUFFERS );
	if( verbose )
		Com_Printf( "ok\n" );

	/* After allocation, set up and prepare headers. */
	if( verbose )
		Com_Printf( "...preparing headers: " );
	for( i = 0; i < WAV_BUFFERS; i++ )
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if( waveOutPrepareHeader( hWaveOut, lpWaveHdr+i, sizeof( WAVEHDR ) ) !=
			MMSYSERR_NOERROR )
		{
			if( verbose )
				Com_Printf( "failed\n" );
			FreeSound( verbose );
			return false;
		}
	}
	if( verbose )
		Com_Printf( "ok\n" );

	dma.samples = gSndBufSize/( dma.samplebits/8 );
	dma.samplepos = 0;
	dma.submission_chunk = 512;
	dma.buffer = (unsigned char *) lpData;
	sample16 = ( dma.samplebits/8 ) - 1;

	wav_init = true;

	return true;
}

/*
* SNDDMA_Init
* 
* Try to find a sound device to mix for.
* Returns false if nothing is found.
*/
bool SNDDMA_Init( void *hwnd, bool verbose )
{
	sndinitstat stat;

	cl_hwnd = ( HWND )hwnd;

	memset( (void *)&dma, 0, sizeof( dma ) );

	s_wavonly = trap_Cvar_Get( "s_wavonly", "0", CVAR_LATCH_SOUND|CVAR_ARCHIVE );

	s_globalfocus->modified = false;

	dsound_init = wav_init = false;

	stat = SIS_FAILURE; // assume DirectSound won't initialize

	/* Init DirectSound */
	if( !s_wavonly->integer )
	{
		if( /*snd_isdirect*/1 )
		{
			stat = SNDDMA_InitDirect( verbose );

			if( stat == SIS_SUCCESS )
			{
				snd_isdirect = true;

				if( verbose )
					Com_Printf( "dsound init succeeded\n" );
			}
			else
			{
				snd_isdirect = false;
				Com_Printf( "*** dsound init failed ***\n" );
			}
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if( !dsound_init && ( stat != SIS_NOTAVAIL ) )
	{
		if( /*snd_iswave*/1 )
		{
			snd_iswave = SNDDMA_InitWav( verbose );

			if( snd_iswave )
			{
				if( verbose )
					Com_Printf( "Wave sound init succeeded\n" );
			}
			else
			{
				Com_Printf( "Wave sound init failed\n" );
			}
		}
	}

	if( !dsound_init && !wav_init )
	{
		Com_Printf( "*** No sound device initialized ***\n" );
		return false;
	}

	return true;
}

/*
* SNDDMA_GetDMAPos
* 
* return the current sample position (in mono samples read)
* inside the recirculating dma buffer, so the mixing code will know
* how many sample are required to fill it up.
*/
int SNDDMA_GetDMAPos( void )
{
	MMTIME mmtime;
	int s = 0;
	DWORD dwWrite;

	if( dsound_init )
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition( pDSBuf, &mmtime.u.sample, &dwWrite );
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if( wav_init )
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;
	s &= ( dma.samples-1 );

	return s;
}

/*
* SNDDMA_BeginPainting
* 
* Makes sure dma.buffer is valid
*/
DWORD locksize;
void SNDDMA_BeginPainting( void )
{
	int reps;
	DWORD dwSize2;
	DWORD *pbuf, *pbuf2;
	HRESULT	hresult;
	DWORD dwStatus;

	if( !pDSBuf )
		return;

	// if the buffer was lost or stopped, restore it and/or restart it
	if( pDSBuf->lpVtbl->GetStatus( pDSBuf, &dwStatus ) != DS_OK )
		Com_Printf( "Couldn't get sound buffer status\n" );

	if( dwStatus & DSBSTATUS_BUFFERLOST )
		pDSBuf->lpVtbl->Restore( pDSBuf );

	if( !( dwStatus & DSBSTATUS_PLAYING ) )
		pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	// lock the dsound buffer

	reps = 0;
	dma.buffer = NULL;

	while( ( hresult = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, (LPVOID *)&pbuf, &locksize,
		(LPVOID *)&pbuf2, &dwSize2, 0 ) ) != DS_OK )
	{
		if( hresult != DSERR_BUFFERLOST )
		{
			Com_Printf( "S_TransferStereo16: Lock failed with error '%s'\n", DSoundError( hresult ) );
			SF_Shutdown( true );
			return;
		}
		else
		{
			pDSBuf->lpVtbl->Restore( pDSBuf );
		}

		if( ++reps > 2 )
			return;
	}
	dma.buffer = (unsigned char *)pbuf;
}

/*
* SNDDMA_Submit
* 
* Send sound to device if buffer isn't really the dma buffer
* Also unlocks the dsound buffer
*/
void SNDDMA_Submit( void )
{
	LPWAVEHDR h;
	int wResult;

	if( !dma.buffer )
		return;

	// unlock the dsound buffer
	if( pDSBuf )
		pDSBuf->lpVtbl->Unlock( pDSBuf, dma.buffer, locksize, NULL, 0 );

	if( !wav_init )
		return;

	//
	// find which sound blocks have completed
	//
	while( 1 )
	{
		if( snd_completed == snd_sent )
		{
			//Com_Printf( "Sound overrun\n" );
			break;
		}

		if( !( lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE ) )
		{
			break;
		}

		snd_completed++; // this buffer has been played
	}

	//
	// submit a few new sound blocks
	//
	while( ( ( snd_sent - snd_completed ) >> sample16 ) < 8 )
	{
		h = lpWaveHdr + ( snd_sent&WAV_MASK );
		if( paintedtime/256 <= snd_sent )
			break;
		snd_sent++;
		/*
		* Now the data block can be sent to the output device. The
		* waveOutWrite function returns immediately and waveform
		* data is sent to the output device in the background.
		*/
		if( s_active )
		{
			wResult = waveOutWrite( hWaveOut, h, sizeof( WAVEHDR ) );

			if( wResult != MMSYSERR_NOERROR )
			{
				Com_Printf( "Failed to write block to device\n" );
				FreeSound( true );
				return;
			}
		}
	}
}

/*
* SNDDMA_Shutdown
* 
* Reset the sound device for exiting
*/
void SNDDMA_Shutdown( bool verbose )
{
	FreeSound( verbose );
}


/*
* S_Activate
* 
* Called when the main window gains or loses focus.
* The window have been destroyed and recreated
* between a deactivate and an activate.
*/
void S_Activate( bool active )
{
	if( !pDS )
		return;

	if( s_globalfocus->modified ) {
		SNDDMA_Shutdown( false );

		SNDDMA_InitDirect( false );

		s_globalfocus->modified = false;

		if( !pDS )
			return;
	}

	// just set the priority for directsound
	if( pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_PRIORITY ) != DS_OK )
	{
		Com_Printf( "DirectSound SetCooperativeLevel failed\n" );
		SNDDMA_Shutdown( false );
	}
}
