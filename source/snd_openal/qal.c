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
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Dynamically loads OpenAL

#include "snd_local.h"

#ifdef OPENAL_RUNTIME

/*#if USE_SDL_VIDEO
#include "SDL.h"
#include "SDL_loadso.h"
#define OBJTYPE void *
#define OBJLOAD(x) SDL_LoadObject(x)
#define SYMLOAD(x,y) SDL_LoadFunction(x,y)
#define OBJFREE(x) SDL_UnloadObject(x)*/

#if defined _WIN32
#include <windows.h>
#define OBJTYPE HMODULE
#define OBJLOAD( x ) LoadLibrary( x )
#define SYMLOAD( x, y ) GetProcAddress( x, y )
#define OBJFREE( x ) FreeLibrary( x )

#else
#include <dlfcn.h>
#define OBJTYPE void *
#define OBJLOAD( x ) dlopen( x, RTLD_LAZY | RTLD_GLOBAL )
#define SYMLOAD( x, y ) dlsym( x, y )
#define OBJFREE( x ) dlclose( x )
#endif

#if !(defined _WIN32 || defined __ANDROID__)
#include <unistd.h>
#include <sys/types.h>
#endif

LPALENABLE qalEnable;
LPALDISABLE qalDisable;
LPALISENABLED qalIsEnabled;
LPALGETSTRING qalGetString;
LPALGETBOOLEANV qalGetBooleanv;
LPALGETINTEGERV qalGetIntegerv;
LPALGETFLOATV qalGetFloatv;
LPALGETDOUBLEV qalGetDoublev;
LPALGETBOOLEAN qalGetBoolean;
LPALGETINTEGER qalGetInteger;
LPALGETFLOAT qalGetFloat;
LPALGETDOUBLE qalGetDouble;
LPALGETERROR qalGetError;
LPALISEXTENSIONPRESENT qalIsExtensionPresent;
LPALGETPROCADDRESS qalGetProcAddress;
LPALGETENUMVALUE qalGetEnumValue;
LPALLISTENERF qalListenerf;
LPALLISTENER3F qalListener3f;
LPALLISTENERFV qalListenerfv;
LPALLISTENERI qalListeneri;
LPALGETLISTENERF qalGetListenerf;
LPALGETLISTENER3F qalGetListener3f;
LPALGETLISTENERFV qalGetListenerfv;
LPALGETLISTENERI qalGetListeneri;
LPALGENSOURCES qalGenSources;
LPALDELETESOURCES qalDeleteSources;
LPALISSOURCE qalIsSource;
LPALSOURCEF qalSourcef;
LPALSOURCE3F qalSource3f;
LPALSOURCEFV qalSourcefv;
LPALSOURCEI qalSourcei;
LPALGETSOURCEF qalGetSourcef;
LPALGETSOURCE3F qalGetSource3f;
LPALGETSOURCEFV qalGetSourcefv;
LPALGETSOURCEI qalGetSourcei;
LPALSOURCEPLAYV qalSourcePlayv;
LPALSOURCESTOPV qalSourceStopv;
LPALSOURCEREWINDV qalSourceRewindv;
LPALSOURCEPAUSEV qalSourcePausev;
LPALSOURCEPLAY qalSourcePlay;
LPALSOURCESTOP qalSourceStop;
LPALSOURCEREWIND qalSourceRewind;
LPALSOURCEPAUSE qalSourcePause;
LPALSOURCEQUEUEBUFFERS qalSourceQueueBuffers;
LPALSOURCEUNQUEUEBUFFERS qalSourceUnqueueBuffers;
LPALGENBUFFERS qalGenBuffers;
LPALDELETEBUFFERS qalDeleteBuffers;
LPALISBUFFER qalIsBuffer;
LPALBUFFERDATA qalBufferData;
LPALGETBUFFERF qalGetBufferf;
LPALGETBUFFERI qalGetBufferi;
LPALDOPPLERFACTOR qalDopplerFactor;
LPALDOPPLERVELOCITY qalDopplerVelocity;
LPALSPEEDOFSOUND qalSpeedOfSound;
LPALDISTANCEMODEL qalDistanceModel;

LPALCCREATECONTEXT qalcCreateContext;
LPALCMAKECONTEXTCURRENT qalcMakeContextCurrent;
LPALCPROCESSCONTEXT qalcProcessContext;
LPALCSUSPENDCONTEXT qalcSuspendContext;
LPALCDESTROYCONTEXT qalcDestroyContext;
LPALCGETCURRENTCONTEXT qalcGetCurrentContext;
LPALCGETCONTEXTSDEVICE qalcGetContextsDevice;
LPALCOPENDEVICE qalcOpenDevice;
LPALCCLOSEDEVICE qalcCloseDevice;
LPALCGETERROR qalcGetError;
LPALCISEXTENSIONPRESENT qalcIsExtensionPresent;
LPALCGETPROCADDRESS qalcGetProcAddress;
LPALCGETENUMVALUE qalcGetEnumValue;
LPALCGETSTRING qalcGetString;
LPALCGETINTEGERV qalcGetIntegerv;

static OBJTYPE OpenALLib = NULL;

static bool alinit_fail = false;

/*
* GPA
*/
static void *GPA( char *str ) {
	void *rv;

	rv = SYMLOAD( OpenALLib, str );
	if( !rv ) {
		Com_Printf( " Couldn't load symbol: %s\n", str );
		alinit_fail = true;
		return NULL;
	} else {
		//Com_DPrintf( " Loaded symbol: %s (0x%08X)\n", str, rv);
		return rv;
	}
}

/*
* QAL_Init
*/
bool QAL_Init( const char *libname, bool verbose ) {
	if( OpenALLib ) {
		return true;
	}

	if( verbose ) {
		Com_Printf( "Loading OpenAL library: %s\n", libname );
	}

	if( ( OpenALLib = OBJLOAD( libname ) ) == 0 ) {
#ifdef _WIN32
		return false;
#else
		char fn[2048];

		if( getcwd( fn, sizeof( fn ) ) == NULL ) {
			return false;
		}

		Q_strncatz( fn, "/", sizeof( fn ) );
		Q_strncatz( fn, libname, sizeof( fn ) );

		if( ( OpenALLib = OBJLOAD( fn ) ) == 0 ) {
			return false;
		}
#endif
	}

	alinit_fail = false;

	*(void**)&qalEnable = GPA( "alEnable" );
	*(void**)&qalDisable = GPA( "alDisable" );
	*(void**)&qalIsEnabled = GPA( "alIsEnabled" );
	*(void**)&qalGetString = GPA( "alGetString" );
	*(void**)&qalGetBooleanv = GPA( "alGetBooleanv" );
	*(void**)&qalGetIntegerv = GPA( "alGetIntegerv" );
	*(void**)&qalGetFloatv = GPA( "alGetFloatv" );
	*(void**)&qalGetDoublev = GPA( "alGetDoublev" );
	*(void**)&qalGetBoolean = GPA( "alGetBoolean" );
	*(void**)&qalGetInteger = GPA( "alGetInteger" );
	*(void**)&qalGetFloat = GPA( "alGetFloat" );
	*(void**)&qalGetDouble = GPA( "alGetDouble" );
	*(void**)&qalGetError = GPA( "alGetError" );
	*(void**)&qalIsExtensionPresent = GPA( "alIsExtensionPresent" );
	*(void**)&qalGetProcAddress = GPA( "alGetProcAddress" );
	*(void**)&qalGetEnumValue = GPA( "alGetEnumValue" );
	*(void**)&qalListenerf = GPA( "alListenerf" );
	*(void**)&qalListener3f = GPA( "alListener3f" );
	*(void**)&qalListenerfv = GPA( "alListenerfv" );
	*(void**)&qalListeneri = GPA( "alListeneri" );
	*(void**)&qalGetListenerf = GPA( "alGetListenerf" );
	*(void**)&qalGetListener3f = GPA( "alGetListener3f" );
	*(void**)&qalGetListenerfv = GPA( "alGetListenerfv" );
	*(void**)&qalGetListeneri = GPA( "alGetListeneri" );
	*(void**)&qalGenSources = GPA( "alGenSources" );
	*(void**)&qalDeleteSources = GPA( "alDeleteSources" );
	*(void**)&qalIsSource = GPA( "alIsSource" );
	*(void**)&qalSourcef = GPA( "alSourcef" );
	*(void**)&qalSource3f = GPA( "alSource3f" );
	*(void**)&qalSourcefv = GPA( "alSourcefv" );
	*(void**)&qalSourcei = GPA( "alSourcei" );
	*(void**)&qalGetSourcef = GPA( "alGetSourcef" );
	*(void**)&qalGetSource3f = GPA( "alGetSource3f" );
	*(void**)&qalGetSourcefv = GPA( "alGetSourcefv" );
	*(void**)&qalGetSourcei = GPA( "alGetSourcei" );
	*(void**)&qalSourcePlayv = GPA( "alSourcePlayv" );
	*(void**)&qalSourceStopv = GPA( "alSourceStopv" );
	*(void**)&qalSourceRewindv = GPA( "alSourceRewindv" );
	*(void**)&qalSourcePausev = GPA( "alSourcePausev" );
	*(void**)&qalSourcePlay = GPA( "alSourcePlay" );
	*(void**)&qalSourceStop = GPA( "alSourceStop" );
	*(void**)&qalSourceRewind = GPA( "alSourceRewind" );
	*(void**)&qalSourcePause = GPA( "alSourcePause" );
	*(void**)&qalSourceQueueBuffers = GPA( "alSourceQueueBuffers" );
	*(void**)&qalSourceUnqueueBuffers = GPA( "alSourceUnqueueBuffers" );
	*(void**)&qalGenBuffers = GPA( "alGenBuffers" );
	*(void**)&qalDeleteBuffers = GPA( "alDeleteBuffers" );
	*(void**)&qalIsBuffer = GPA( "alIsBuffer" );
	*(void**)&qalBufferData = GPA( "alBufferData" );
	*(void**)&qalGetBufferf = GPA( "alGetBufferf" );
	*(void**)&qalGetBufferi = GPA( "alGetBufferi" );
	*(void**)&qalDopplerFactor = GPA( "alDopplerFactor" );
	*(void**)&qalDopplerVelocity = GPA( "alDopplerVelocity" );
	*(void**)&qalSpeedOfSound = GPA( "alSpeedOfSound" );
	*(void**)&qalDistanceModel = GPA( "alDistanceModel" );

	*(void**)&qalcCreateContext = GPA( "alcCreateContext" );
	*(void**)&qalcMakeContextCurrent = GPA( "alcMakeContextCurrent" );
	*(void**)&qalcProcessContext = GPA( "alcProcessContext" );
	*(void**)&qalcSuspendContext = GPA( "alcSuspendContext" );
	*(void**)&qalcDestroyContext = GPA( "alcDestroyContext" );
	*(void**)&qalcGetCurrentContext = GPA( "alcGetCurrentContext" );
	*(void**)&qalcGetContextsDevice = GPA( "alcGetContextsDevice" );
	*(void**)&qalcOpenDevice = GPA( "alcOpenDevice" );
	*(void**)&qalcCloseDevice = GPA( "alcCloseDevice" );
	*(void**)&qalcGetError = GPA( "alcGetError" );
	*(void**)&qalcIsExtensionPresent = GPA( "alcIsExtensionPresent" );
	*(void**)&qalcGetProcAddress = GPA( "alcGetProcAddress" );
	*(void**)&qalcGetEnumValue = GPA( "alcGetEnumValue" );
	*(void**)&qalcGetString = GPA( "alcGetString" );
	*(void**)&qalcGetIntegerv = GPA( "alcGetIntegerv" );

	if( alinit_fail ) {
		QAL_Shutdown();
		Com_Printf( " Error: One or more symbols not found.\n" );
		return false;
	}

	return true;
}

/*
* QAL_Shutdown
*/
void QAL_Shutdown( void ) {
	if( OpenALLib ) {
		OBJFREE( OpenALLib );
		OpenALLib = NULL;
	}

	qalEnable = NULL;
	qalDisable = NULL;
	qalIsEnabled = NULL;
	qalGetString = NULL;
	qalGetBooleanv = NULL;
	qalGetIntegerv = NULL;
	qalGetFloatv = NULL;
	qalGetDoublev = NULL;
	qalGetBoolean = NULL;
	qalGetInteger = NULL;
	qalGetFloat = NULL;
	qalGetDouble = NULL;
	qalGetError = NULL;
	qalIsExtensionPresent = NULL;
	qalGetProcAddress = NULL;
	qalGetEnumValue = NULL;
	qalListenerf = NULL;
	qalListener3f = NULL;
	qalListenerfv = NULL;
	qalListeneri = NULL;
	qalGetListenerf = NULL;
	qalGetListener3f = NULL;
	qalGetListenerfv = NULL;
	qalGetListeneri = NULL;
	qalGenSources = NULL;
	qalDeleteSources = NULL;
	qalIsSource = NULL;
	qalSourcef = NULL;
	qalSource3f = NULL;
	qalSourcefv = NULL;
	qalSourcei = NULL;
	qalGetSourcef = NULL;
	qalGetSource3f = NULL;
	qalGetSourcefv = NULL;
	qalGetSourcei = NULL;
	qalSourcePlayv = NULL;
	qalSourceStopv = NULL;
	qalSourceRewindv = NULL;
	qalSourcePausev = NULL;
	qalSourcePlay = NULL;
	qalSourceStop = NULL;
	qalSourceRewind = NULL;
	qalSourcePause = NULL;
	qalSourceQueueBuffers = NULL;
	qalSourceUnqueueBuffers = NULL;
	qalGenBuffers = NULL;
	qalDeleteBuffers = NULL;
	qalIsBuffer = NULL;
	qalBufferData = NULL;
	qalGetBufferf = NULL;
	qalGetBufferi = NULL;
	qalDopplerFactor = NULL;
	qalDopplerVelocity = NULL;
	qalSpeedOfSound = NULL;
	qalDistanceModel = NULL;

	qalcCreateContext = NULL;
	qalcMakeContextCurrent = NULL;
	qalcProcessContext = NULL;
	qalcSuspendContext = NULL;
	qalcDestroyContext = NULL;
	qalcGetCurrentContext = NULL;
	qalcGetContextsDevice = NULL;
	qalcOpenDevice = NULL;
	qalcCloseDevice = NULL;
	qalcGetError = NULL;
	qalcIsExtensionPresent = NULL;
	qalcGetProcAddress = NULL;
	qalcGetEnumValue = NULL;
	qalcGetString = NULL;
	qalcGetIntegerv = NULL;
}
#else
bool QAL_Init( const char *libname, bool verbose ) {
	return true;
}
void QAL_Shutdown( void ) {
}
#endif
