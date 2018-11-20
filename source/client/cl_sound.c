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
#include "client.h"

#ifndef _MSC_VER
static void CL_SoundModule_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static void CL_SoundModule_Error( const char *msg );
#endif

static void CL_SoundModule_Error( const char *msg ) {
	Com_Error( ERR_FATAL, "%s", msg );
}

void CL_SoundModule_Init( bool verbose ) {
	if( !S_Init( verbose ) ) {
		abort();
	}

	// check memory integrity
	Mem_DebugCheckSentinelsGlobal();
}

void CL_SoundModule_Shutdown() {
	S_Shutdown();
}

void CL_SoundModule_StopAllSounds( bool stopMusic ) {
	S_StopAllSounds( stopMusic );
}

void CL_SoundModule_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, int64_t now ) {
	S_Update( origin, velocity, axis, now );
}

void CL_SoundModule_UpdateEntity( int entNum, vec3_t origin, vec3_t velocity ) {
	S_UpdateEntity( entNum, origin, velocity );
}

void CL_SoundModule_SetWindowFocus( bool focused ) {
	S_SetWindowFocus( focused );
}

struct sfx_s * CL_SoundModule_RegisterSound( const char * filename ) {
	return S_RegisterSound( filename );
}

void CL_SoundModule_StartFixedSound( struct sfx_s *sfx, const vec3_t origin, int channel, float volume,
									 float attenuation ) {
	S_StartFixedSound( sfx, origin, channel, volume, attenuation );
}

void CL_SoundModule_StartEntitySound( struct sfx_s *sfx, int entnum, int channel, float volume, float attenuation ) {
	S_StartEntitySound( sfx, entnum, channel, volume, attenuation );
}

void CL_SoundModule_StartGlobalSound( struct sfx_s *sfx, int channel, float volume ) {
	S_StartGlobalSound( sfx, channel, volume );
}

void CL_SoundModule_StartLocalSound( struct sfx_s *sfx, int channel, float volume ) {
	S_StartLocalSound( sfx, channel, volume );
}

void CL_SoundModule_ImmediateSound( struct sfx_s *sfx, int entnum, float volume, float attenuation, int64_t now ) {
	S_ImmediateSound( sfx, entnum, volume, attenuation, now );
}

void CL_SoundModule_StartBackgroundTrack( struct sfx_s *sfx ) {
	S_StartBackgroundTrack( sfx );
}

void CL_SoundModule_StartMenuMusic() {
	S_StartMenuMusic();
}

void CL_SoundModule_StopBackgroundTrack( void ) {
	S_StopBackgroundTrack();
}

void CL_SoundModule_BeginAviDemo( void ) {
	S_BeginAviDemo();
}

void CL_SoundModule_StopAviDemo( void ) {
	S_StopAviDemo();
}
