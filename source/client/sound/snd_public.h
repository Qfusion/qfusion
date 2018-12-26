#pragma once

bool S_Init();
void S_Shutdown();

struct sfx_s * S_RegisterSound( const char * filename );
int64_t S_SoundLengthMilliseconds( const struct sfx_s * sfx );

void S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, int64_t now );
void S_UpdateEntity( int ent_num, const vec3_t origin, const vec3_t velocity );

void S_SetWindowFocus( bool focused );

void S_StartFixedSound( struct sfx_s * sfx, const vec3_t origin, int channel, float volume, float attenuation );
void S_StartEntitySound( struct sfx_s * sfx, int ent_num, int channel, float volume, float attenuation );
void S_StartGlobalSound( struct sfx_s * sfx, int channel, float volume );
void S_StartLocalSound( struct sfx_s * sfx, int channel, float volume );
void S_ImmediateSound( struct sfx_s * sfx, int ent_num, float volume, float attenuation, int64_t now );
void S_StopAllSounds( bool stopMusic );

void S_StartMenuMusic();
void S_StopBackgroundTrack();

void S_BeginAviDemo();
void S_StopAviDemo();
