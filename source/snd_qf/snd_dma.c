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
// snd_dma.c -- main control for any streaming sound output device


#include "snd_local.h"
#include "snd_cmdque.h"
#include "../qalgo/q_trie.h"

// =======================================================================
// Internal sound data & structures
// =======================================================================

#define FORWARD 0
#define RIGHT   1
#define UP      2

channel_t channels[MAX_CHANNELS];

bool snd_initialized = false;

dma_t dma;

static vec3_t listenerOrigin;
static vec3_t listenerVelocity;
static mat3_t listenerAxis;

volatile unsigned int soundtime;      // sample PAIRS
volatile unsigned int paintedtime;    // sample PAIRS

#define     MAX_LOOPSFX 128
loopsfx_t loop_sfx[MAX_LOOPSFX];
int num_loopsfx;

#define     MAX_PLAYSOUNDS  128
playsound_t s_playsounds[MAX_PLAYSOUNDS];
playsound_t s_freeplays;
playsound_t s_pendingplays;

rawsound_t *raw_sounds[MAX_RAW_SOUNDS];

#define UPDATE_MSEC 10
static int64_t s_last_update_time;

static int s_attenuation_model = 0;
static float s_attenuation_maxdistance = 0;
static float s_attenuation_refdistance = 0;

bool s_active = false;

static bool s_aviDump;
static unsigned s_aviNumSamples;
static int s_aviDumpFile;
static char *s_aviDumpFileName;

static entity_spatialization_t s_ent_spatialization[MAX_EDICTS];

static void S_StopAllSounds( bool clear, bool stopMusic );
static void S_ClearSoundTime( void );
static void S_ClearRawSounds( void );
static void S_FreeRawSounds( void );
static void S_BeginAviDemo( void );
static void S_StopAviDemo( void );

// highfrequency attenuation parameters
// 340/0.15 (speed of sound/width of head) gives us 2267hz
// but this sounds little too muffled so lets compromise
#define HQ_HF_FREQUENCY     3300.0
#define HQ_HF_DAMP      0.25
// 340/0.15 for ear delay, lets round it nice and aim for 20 samples/44100
#define HQ_EAR_DELAY    2205

static float s_lpf_cw;

/*
* S_SoundList
*/
static void S_SoundList_f( void ) {
	int i;
	sfx_t *sfx;
	sfxcache_t *sc;
	int size, total;

	total = 0;
	for( sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++ ) {
		if( !sfx->name[0] ) {
			continue;
		}
		sc = sfx->cache;
		if( sc ) {
			size = sc->length * sc->width * sc->channels;
			total += size;
			if( sc->loopstart < sc->length ) {
				Com_Printf( "L" );
			} else {
				Com_Printf( " " );
			}
			Com_Printf( "(%2db) %6i : %s\n", sc->width * 8, size, sfx->name );
		} else {
			if( sfx->name[0] == '*' ) {
				Com_Printf( "  placeholder : %s\n", sfx->name );
			} else {
				Com_Printf( "  not loaded  : %s\n", sfx->name );
			}
		}
	}
	Com_Printf( "Total resident: %i\n", total );
}

/*
* S_Init
*/
static bool S_Init( int maxEntities, bool verbose ) {
	if( !SNDDMA_Init( verbose ) ) {
		return false;
	}

	s_active = true;
	s_last_update_time = 0;

	if( verbose ) {
		Com_Printf( "Sound sampling rate: %i\n", dma.speed );
	}

	SNDOGG_Init( verbose );

	num_loopsfx = 0;

	memset( raw_sounds, 0, sizeof( raw_sounds ) );

	S_InitScaletable();

	// highfrequency attenuation filter
	s_lpf_cw = S_LowpassCW( HQ_HF_FREQUENCY, dma.speed );

	S_ClearSoundTime();

	S_StopAllSounds( true, true );

	S_LockBackgroundTrack( false );

	return true;
}

/*
* S_Shutdown
*/
static void S_Shutdown( bool verbose ) {
	S_StopAllSounds( true, true );

	S_StopAviDemo();

	S_LockBackgroundTrack( false );

	S_StopBackgroundTrack();

	S_FreeRawSounds();

	SNDDMA_Shutdown( verbose );

	SNDOGG_Shutdown( verbose );

	num_loopsfx = 0;
}


/*
* S_ClearSoundTime
*/
static void S_ClearSoundTime( void ) {
	soundtime = 0;
	paintedtime = 0;
	S_ClearRawSounds();
}

//=============================================================================

/*
* S_PickChannel
*
* Picks a channel based on priorities, empty slots, number of channels
*/
channel_t *S_PickChannel( int entnum, int entchannel ) {
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t *ch;

	if( entchannel < 0 ) {
		S_Error( "S_PickChannel: entchannel < 0" );
	}

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for( ch_idx = 0; ch_idx < MAX_CHANNELS; ch_idx++ ) {
		if( entchannel != 0 // channel 0 never overrides
			&& channels[ch_idx].entnum == entnum
			&& channels[ch_idx].entchannel == entchannel ) { // always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		// wsw: Medar: Disabled. Don't wanna use cl.playernum much in client code, because of chasecam and stuff
		//if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
		//	continue;

		if( channels[ch_idx].end < life_left + paintedtime ) {
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if( first_to_die == -1 ) {
		return NULL;
	}

	ch = &channels[first_to_die];
	memset( ch, 0, sizeof( *ch ) );

	return ch;
}

/*
* S_SetAttenuationModel
*/
static void S_SetAttenuationModel( int model, float maxdistance, float refdistance ) {
	s_attenuation_model = model;
	s_attenuation_maxdistance = maxdistance;
	s_attenuation_refdistance = refdistance;
}

/*
* S_GainForAttenuation
*/
static float S_GainForAttenuation( float dist, float attenuation ) {
	if( !attenuation ) {
		return 1.0f;
	}
	return Q_GainForAttenuation( s_attenuation_model, s_attenuation_maxdistance, s_attenuation_refdistance, dist, attenuation );
}

/*
* S_SpatializeOrigin
*/
#define Q3STEREODIRECTION
static void S_SpatializeOrigin( const vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol ) {
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;

	// calculate stereo separation and distance attenuation
	VectorSubtract( origin, listenerOrigin, source_vec );

	dist = VectorNormalize( source_vec );

	if( dma.channels == 1 || !dist_mult ) { // no attenuation = no spatialization
		rscale = 1.0f;
		lscale = 1.0f;
	} else {
#ifdef Q3STEREODIRECTION
		vec3_t vec;
		Matrix3_TransformVector( listenerAxis, source_vec, vec );
		dot = vec[1];
#else
		dot = DotProduct( source_vec, listenerAxis[RIGHT] );
#endif
		rscale = 0.5 * ( 1.0 + dot );
		lscale = 0.5 * ( 1.0 - dot );
		if( rscale < 0 ) {
			rscale = 0;
		}
		if( lscale < 0 ) {
			lscale = 0;
		}
	}

	dist = S_GainForAttenuation( dist, dist_mult );

	// add in distance effect
	scale = dist * rscale;
	*right_vol = (int) ( master_vol * scale );
	if( *right_vol < 0 ) {
		*right_vol = 0;
	}

	scale = dist * lscale;
	*left_vol = (int) ( master_vol * scale );
	if( *left_vol < 0 ) {
		*left_vol = 0;
	}
}

/*
* S_SpatializeOriginHF
*/
static void S_SpatializeOriginHQ( const vec3_t origin, float master_vol, float dist_mult,
								  int *left_vol, int *right_vol, int *lcoeff, int *rcoeff, unsigned int *ldelay, unsigned int *rdelay ) {
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t vec, source_vec;
	vec_t lgainhf, rgainhf;

	// calculate stereo separation and distance attenuation
	VectorSubtract( origin, listenerOrigin, vec );
	Matrix3_TransformVector( listenerAxis, vec, source_vec );

	dist = VectorNormalize( source_vec );

	if( dma.channels == 1 || !dist_mult ) { // no attenuation = no spatialization
		rscale = 1.0f;
		lscale = 1.0f;
		lgainhf = 1.0f;
		rgainhf = 1.0f;
		if( ldelay && rdelay ) {
			*ldelay = *rdelay = 0;
		}
	} else {
		// legacy panning
		// correlate some of the stereo-separation that hf dampening
		// causes (HQ_HF_DAMP * 0.5)
		dot = source_vec[1];
		rscale = 0.5 * ( 1.0 + ( dot * ( 1.0 - HQ_HF_DAMP * 0.25 ) ) );
		lscale = 0.5 * ( 1.0 - ( dot * ( 1.0 - HQ_HF_DAMP * 0.25 ) ) );
		if( rscale < 0 ) {
			rscale = 0;
		}
		if( lscale < 0 ) {
			lscale = 0;
		}

		// pseudo acoustics, apply delay to opposite ear of where the
		// sound originates based on the angle
		if( ldelay && rdelay ) {
			// HQ_EAR_DELAY ~ 1/(0.15/340.0)
			float max_delay = dma.speed * s_separationDelay->value / HQ_EAR_DELAY;
			if( dot < 0.0 ) {
				// delay right ear (sound from left side)
				*rdelay = (int)( max_delay * -dot );
				*ldelay = 0;
			} else {
				// delay left ear (sound from right side)
				*ldelay = (int)( max_delay * dot );
				*rdelay = 0;
			}
		}

		// pseudo acoustics, apply high-frequency damping based on
		// the angle, separately for both ears and then for
		// sound source behind the listener
		rgainhf = lgainhf = 1.0;

		// right ear, left ear
		if( dot < 0  ) {
			rgainhf = 1.0 + dot * HQ_HF_DAMP * 0.5;
		} else if( dot > 0 ) {
			lgainhf = 1.0 - dot * HQ_HF_DAMP * 0.5;
		}

		// behind head for both ears
		dot = source_vec[0];
		if( dot < 0.0 ) {
			float g = 1.0 + dot * HQ_HF_DAMP;
			rgainhf *= g;
			lgainhf *= g;
		}
	}

	dist = S_GainForAttenuation( dist, dist_mult );

	// add in distance effect
	scale = dist * rscale;
	*right_vol = (int) ( master_vol * scale );
	if( *right_vol < 0 ) {
		*right_vol = 0;
	}

	scale = dist * lscale;
	*left_vol = (int) ( master_vol * scale );
	if( *left_vol < 0 ) {
		*left_vol = 0;
	}

	// highfrequency coefficients
	if( lcoeff && rcoeff ) {
		*lcoeff = (int)( S_LowpassCoeff( lgainhf, s_lpf_cw ) * 65535.0f );
		*rcoeff = (int)( S_LowpassCoeff( rgainhf, s_lpf_cw ) * 65535.0f );
	}
}

/*
* S_Spatialize
*/
static void S_SpatializeChannel( channel_t *ch ) {
	vec3_t origin, velocity;

	if( ch->fixed_origin ) {
		VectorCopy( ch->origin, origin );
		VectorClear( velocity );
	} else {
		VectorCopy( s_ent_spatialization[ch->entnum].origin, origin );
		VectorCopy( s_ent_spatialization[ch->entnum].velocity, velocity );
	}

	if( s_pseudoAcoustics->value ) {
		S_SpatializeOriginHQ( origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol,
							  &ch->lpf_lcoeff, &ch->lpf_rcoeff, &ch->ldelay, &ch->rdelay );
	} else {
		S_SpatializeOrigin( origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol );
		ch->lpf_lcoeff = ch->lpf_rcoeff = 0.0f;
		ch->ldelay = ch->rdelay = 0;
	}
}

/*
* S_SetEntitySpatialization
*/
void S_SetEntitySpatialization( int entnum, const vec3_t origin, const vec3_t velocity ) {
	if( entnum < 0 || entnum >= MAX_EDICTS ) {
		return;
	}
	VectorCopy( origin, s_ent_spatialization[entnum].origin );
	VectorCopy( velocity, s_ent_spatialization[entnum].velocity );
}

/*
* S_AllocPlaysound
*/
static playsound_t *S_AllocPlaysound( void ) {
	playsound_t *ps;

	ps = s_freeplays.next;
	if( ps == &s_freeplays ) {
		return NULL; // no free playsounds

	}
	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}

/*
* S_FreePlaysound
*/
static void S_FreePlaysound( playsound_t *ps ) {
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}

/*
* S_IssuePlaysound
* Only called by the update loop
*/
void S_IssuePlaysound( playsound_t *ps ) {
	channel_t *ch;
	sfxcache_t *sc;

	if( s_show->integer ) {
		Com_Printf( "Issue %i\n", ps->begin );
	}
	// pick a channel to play on
	ch = S_PickChannel( ps->entnum, ps->entchannel );
	if( !ch ) {
		S_FreePlaysound( ps );
		return;
	}
	sc = S_LoadSound( ps->sfx );
	if( !sc ) {
		S_FreePlaysound( ps );
		return;
	}

	// spatialize
	ch->dist_mult = ps->attenuation;
	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy( ps->origin, ch->origin );
	ch->fixed_origin = ps->fixed_origin;

	S_SpatializeChannel( ch );

	ch->pos = 0;
	ch->end = paintedtime + sc->length;

	// free the playsound
	S_FreePlaysound( ps );
}

/*
* S_ClearPlaysounds
*/
static void S_ClearPlaysounds( void ) {
	int i;

	num_loopsfx = 0;

	memset( s_playsounds, 0, sizeof( s_playsounds ) );
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for( i = 0; i < MAX_PLAYSOUNDS; i++ ) {
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	memset( channels, 0, sizeof( channels ) );
}

// =======================================================================
// Start a sound effect
// =======================================================================

/*
* S_StartSound
*/
static void S_StartSound( sfx_t *sfx, const vec3_t origin, int entnum, int entchannel, float fvol, float attenuation ) {
	sfxcache_t *sc;
	int vol;
	playsound_t *ps, *sort;

	if( !sfx ) {
		return;
	}

	// make sure the sound is loaded
	sc = S_LoadSound( sfx );
	if( !sc ) {
		return; // couldn't load the sound's data

	}
	vol = fvol * 255;

	// make the playsound_t
	ps = S_AllocPlaysound();
	if( !ps ) {
		return;
	}

	if( origin ) {
		VectorCopy( origin, ps->origin );
		ps->fixed_origin = true;
	} else {
		ps->fixed_origin = false;
	}

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sfx = sfx;

	ps->begin = paintedtime;

	// sort into the pending sound list
	for( sort = s_pendingplays.next;
		 sort != &s_pendingplays && sort->begin <= ps->begin;
		 sort = sort->next )
		;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}

/*
* S_StartFixedSound
*/
static void S_StartFixedSound( sfx_t *sfx, const vec3_t origin, int channel, float fvol, float attenuation ) {
	S_StartSound( sfx, origin, 0, channel, fvol, attenuation );
}

/*
* S_StartRelativeSound
*/
static void S_StartRelativeSound( sfx_t *sfx, int entnum, int channel, float fvol, float attenuation ) {
	S_StartSound( sfx, NULL, entnum, channel, fvol, attenuation );
}

/*
* S_StartGlobalSound
*/
static void S_StartGlobalSound( sfx_t *sfx, int channel, float fvol ) {
	S_StartSound( sfx, NULL, 0, channel, fvol, ATTN_NONE );
}

/*
* S_Clear
*/
static void S_Clear( void ) {
	int clear;

	num_loopsfx = 0;

	S_ClearRawSounds();

	if( dma.samplebits == 8 ) {
		clear = 0x80;
	} else {
		clear = 0;
	}

	SNDDMA_BeginPainting();
	if( dma.buffer ) {
		memset( dma.buffer, clear, dma.samples * dma.samplebits / 8 );
	}
	SNDDMA_Submit();
}

/*
* S_StopAllSounds
*/
static void S_StopAllSounds( bool clear, bool stopMusic ) {
	// clear all the playsounds and channels
	S_ClearPlaysounds();

	if( stopMusic ) {
		S_StopBackgroundTrack();
	}

	if( clear ) {
		S_Clear();
	}
}

/*
* S_AddLoopSound
*/
void S_AddLoopSound( sfx_t *sfx, int entnum, float fvol, float attenuation ) {
	if( !sfx || num_loopsfx >= MAX_LOOPSFX ) {
		return;
	}
	if( entnum < 0 || entnum >= MAX_EDICTS ) {
		return;
	}

	loop_sfx[num_loopsfx].sfx = sfx;
	loop_sfx[num_loopsfx].volume = 255.0 * fvol;
	loop_sfx[num_loopsfx].attenuation = attenuation;
	loop_sfx[num_loopsfx].entnum = entnum;

	num_loopsfx++;
}

/*
* S_LoopSoundOrigin
*/
static const vec_t *S_LoopSoundOrigin( loopsfx_t *loopsfx ) {
	int entnum = loopsfx->entnum;
	return entnum < 0 || entnum >= MAX_EDICTS ? listenerOrigin :
		   s_ent_spatialization[entnum].origin;
}

/*
* S_AddLoopSounds
*/
static void S_AddLoopSounds( void ) {
	int i, j;
	int left, right, left_total, right_total;
	channel_t *ch;
	sfx_t *sfx;
	sfxcache_t *sc;

	for( i = 0; i < num_loopsfx; i++ ) {
		if( !loop_sfx[i].sfx ) {
			continue;
		}

		sfx = loop_sfx[i].sfx;
		sc = sfx->cache;
		if( !sc ) {
			continue;
		}

		// find the total contribution of all sounds of this type
		if( loop_sfx[i].attenuation ) {
			S_SpatializeOrigin( S_LoopSoundOrigin( &loop_sfx[i] ),
								loop_sfx[i].volume, loop_sfx[i].attenuation, &left_total, &right_total );

			for( j = i + 1; j < num_loopsfx; j++ ) {
				if( loop_sfx[j].sfx != loop_sfx[i].sfx ) {
					continue;
				}
				if( loop_sfx[j].entnum == loop_sfx[i].entnum ) {
					loop_sfx[j].sfx = NULL; // don't check this again later
					continue;
				}

				loop_sfx[j].sfx = NULL; // don't check this again later

				S_SpatializeOrigin( S_LoopSoundOrigin( &loop_sfx[j] ),
									loop_sfx[i].volume, loop_sfx[i].attenuation, &left, &right );
				left_total += left;
				right_total += right;
			}

			if( left_total == 0 && right_total == 0 ) {
				continue; // not audible
			}
		} else {
			for( j = i + 1; j < num_loopsfx; j++ ) {
				if( loop_sfx[j].sfx != loop_sfx[i].sfx ) {
					continue;
				}
				if( loop_sfx[j].entnum == loop_sfx[i].entnum ) {
					loop_sfx[j].sfx = NULL; // don't check this again later
					continue;
				}
			}
			left_total = loop_sfx[i].volume;
			right_total = loop_sfx[i].volume;
		}

		// allocate a channel
		ch = S_PickChannel( 0, 0 );
		if( !ch ) {
			return;
		}

		if( left_total > 255 ) {
			left_total = 255;
		}
		if( right_total > 255 ) {
			right_total = 255;
		}
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->autosound = true; // remove next frame
		ch->sfx = sfx;
		ch->pos = paintedtime % sc->length;
		ch->end = paintedtime + sc->length - ch->pos;
	}

	num_loopsfx = 0;
}

//=============================================================================

#define S_RAW_SOUND_IDLE_SEC            10  // time interval for idling raw sound before it's freed
#define S_RAW_SOUND_BGTRACK             -2
#define S_RAW_SOUND_OTHER               -1
#define S_RAW_SAMPLES_PRECISION_BITS    14

/*
* S_FindRawSound
*/
static rawsound_t *S_FindRawSound( int entnum, bool addNew ) {
	int i, free;
	int best, best_time;
	rawsound_t *rawsound;

	// check for replacement sound, or find the best one to replace
	best = free = -1;
	best_time = 0x7fffffff;
	for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
		rawsound = raw_sounds[i];

		if( free < 0 && !rawsound ) {
			free = i;
		} else if( rawsound ) {
			int time;

			if( rawsound->entnum == entnum ) {
				// exact match
				return rawsound;
			}

			time = rawsound->rawend - paintedtime;
			if( time < best_time ) {
				best = i;
				best_time = time;
			}
		}
	}

	if( !addNew ) {
		return NULL;
	}

	if( free >= 0 ) {
		best = free;
	}
	if( best < 0 ) {
		// no free slots
		return NULL;
	}

	if( !raw_sounds[best] ) {
		raw_sounds[best] = S_Malloc( sizeof( *rawsound )
									 + sizeof( portable_samplepair_t ) * MAX_RAW_SAMPLES );
	}

	rawsound = raw_sounds[best];
	rawsound->entnum = entnum;
	rawsound->rawend = 0;
	rawsound->left_volume = rawsound->right_volume = 0; // will be spatialized later
	return rawsound;
}

/*
* S_RawSamplesStereo
*/
static unsigned int S_RawSamplesStereo( portable_samplepair_t *rawsamples, unsigned int rawend,
										unsigned int samples, unsigned int rate, unsigned short width,
										unsigned short channels, const uint8_t *data ) {
	unsigned src, dst;
	unsigned fracstep, samplefrac;

	if( rawend < paintedtime ) {
		rawend = paintedtime;
	}

	fracstep = ( (double) rate / (double) dma.speed ) * (double)( 1 << S_RAW_SAMPLES_PRECISION_BITS );
	samplefrac = 0;

	if( width == 2 ) {
		const short *in = (const short *)data;

		if( channels == 2 ) {
			for( src = 0; src < samples; samplefrac += fracstep, src = ( samplefrac >> S_RAW_SAMPLES_PRECISION_BITS ) ) {
				dst = rawend++ & ( MAX_RAW_SAMPLES - 1 );
				rawsamples[dst].left = in[src * 2];
				rawsamples[dst].right = in[src * 2 + 1];
			}
		} else {
			for( src = 0; src < samples; samplefrac += fracstep, src = ( samplefrac >> S_RAW_SAMPLES_PRECISION_BITS ) ) {
				dst = rawend++ & ( MAX_RAW_SAMPLES - 1 );
				rawsamples[dst].left = in[src];
				rawsamples[dst].right = in[src];
			}
		}
	} else {
		if( channels == 2 ) {
			const char *in = (const char *)data;

			for( src = 0; src < samples; samplefrac += fracstep, src = ( samplefrac >> S_RAW_SAMPLES_PRECISION_BITS ) ) {
				dst = rawend++ & ( MAX_RAW_SAMPLES - 1 );
				rawsamples[dst].left = in[src * 2] << 8;
				rawsamples[dst].right = in[src * 2 + 1] << 8;
			}
		} else {
			for( src = 0; src < samples; samplefrac += fracstep, src = ( samplefrac >> S_RAW_SAMPLES_PRECISION_BITS ) ) {
				dst = rawend++ & ( MAX_RAW_SAMPLES - 1 );
				rawsamples[dst].left = ( data[src] - 128 ) << 8;
				rawsamples[dst].right = ( data[src] - 128 ) << 8;
			}
		}
	}

	return rawend;
}

/*
* S_RawEntSamples
*/
static void S_RawEntSamples( int entnum, unsigned int samples, unsigned int rate, unsigned short width,
							 unsigned short channels, const uint8_t *data, int snd_vol ) {
	rawsound_t *rawsound;

	if( snd_vol < 0 ) {
		snd_vol = 0;
	}

	rawsound = S_FindRawSound( entnum, true );
	if( !rawsound ) {
		return;
	}

	rawsound->volume = snd_vol;
	rawsound->attenuation = ATTN_NONE;
	rawsound->rawend = S_RawSamplesStereo( rawsound->rawsamples, rawsound->rawend,
										   samples, rate, width, channels, data );
	rawsound->left_volume = rawsound->right_volume = snd_vol;
}

/*
* S_RawSamples2
*/
void S_RawSamples2( unsigned int samples, unsigned int rate, unsigned short width,
					unsigned short channels, const uint8_t *data, int snd_vol ) {
	S_RawEntSamples( S_RAW_SOUND_BGTRACK, samples, rate, width, channels, data, snd_vol );
}

/*
* S_RawSamples
*/
void S_RawSamples( unsigned int samples, unsigned int rate, unsigned short width,
				   unsigned short channels, const uint8_t *data, bool music ) {
	int snd_vol;
	int entnum;

	if( music ) {
		snd_vol = s_musicvolume->value * 255;
		entnum = S_RAW_SOUND_BGTRACK;
	} else {
		snd_vol = s_volume->value * 255;
		entnum = S_RAW_SOUND_OTHER;
	}

	S_RawEntSamples( entnum, samples, rate, width, channels, data, snd_vol );
}

/*
* S_PositionedRawSamples
*/
static void S_PositionedRawSamples( int entnum, float fvol, float attenuation,
									unsigned int samples, unsigned int rate,
									unsigned short width, unsigned short channels, const uint8_t *data ) {
	rawsound_t *rawsound;

	if( entnum < 0 || entnum >= MAX_EDICTS ) {
		return;
	}

	rawsound = S_FindRawSound( entnum, true );
	if( !rawsound ) {
		return;
	}

	rawsound->volume = s_volume->value * fvol * 255;
	rawsound->attenuation = attenuation;
	rawsound->rawend = S_RawSamplesStereo( rawsound->rawsamples, rawsound->rawend,
										   samples, rate, width, channels, data );
}

/*
* S_GetRawSamplesLength
*/
unsigned int S_GetRawSamplesLength( void ) {
	rawsound_t *rawsound;

	rawsound = S_FindRawSound( S_RAW_SOUND_BGTRACK, false );
	if( !rawsound ) {
		return 0;
	}

	return rawsound->rawend <= paintedtime
		   ? 0
		   : (float)( rawsound->rawend - paintedtime ) * dma.msec_per_sample;
}

/*
* S_GetPositionedRawSamplesLength
*/
unsigned int S_GetPositionedRawSamplesLength( int entnum ) {
	rawsound_t *rawsound;

	if( entnum < 0 ) {
		entnum = 0;
	}

	rawsound = S_FindRawSound( entnum, false );
	if( !rawsound ) {
		return 0;
	}

	return rawsound->rawend <= paintedtime
		   ? 0
		   : (float)( rawsound->rawend - paintedtime ) * dma.msec_per_sample;
}

/*
* S_FreeIdleRawSounds
*
* Free raw sound that have been idling for too long.
*/
static void S_FreeIdleRawSounds( void ) {
	int i;

	for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
		rawsound_t *rawsound = raw_sounds[i];

		if( !rawsound ) {
			continue;
		}
		if( rawsound->rawend >= paintedtime ) {
			continue;
		}

		if( ( paintedtime - rawsound->rawend ) / dma.speed >= S_RAW_SOUND_IDLE_SEC ) {
			S_Free( rawsound );
			raw_sounds[i] = NULL;
		}
	}
}

/*
* S_ClearRawSounds
*/
static void S_ClearRawSounds( void ) {
	int i;

	for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
		rawsound_t *rawsound = raw_sounds[i];

		if( !rawsound ) {
			continue;
		}
		rawsound->rawend = 0;
	}
}

/*
* S_SpatializeRawSounds
*/
static void S_SpatializeRawSounds( void ) {
	int i;

	for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
		int left, right;
		rawsound_t *rawsound = raw_sounds[i];

		if( !rawsound ) {
			continue;
		}

		if( rawsound->rawend < paintedtime ) {
			rawsound->left_volume = rawsound->right_volume = 0;
			continue;
		}

		// spatialization
		if( rawsound->attenuation && rawsound->entnum >= 0 && rawsound->entnum < MAX_EDICTS ) {
			S_SpatializeOrigin( s_ent_spatialization[rawsound->entnum].origin,
								rawsound->volume, rawsound->attenuation, &left, &right );
		} else {
			left = right = rawsound->volume;
		}

		rawsound->left_volume = left;
		rawsound->right_volume = right;
	}
}

/*
* S_FreeRawSounds
*/
static void S_FreeRawSounds( void ) {
	int i;

	// free raw samples
	for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
		if( raw_sounds[i] ) {
			S_Free( raw_sounds[i] );
		}
	}
	memset( raw_sounds, 0, sizeof( raw_sounds ) );
}

//=============================================================================

/*
* GetSoundtime
*/
static void GetSoundtime( void ) {
	unsigned int samplepos;
	static unsigned int buffers;
	static unsigned int oldsamplepos;
	unsigned int fullsamples;

	fullsamples = dma.samples / dma.channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if( samplepos < oldsamplepos ) {
		buffers++;          // buffer wrapped

		if( paintedtime > 0x40000000 ) {
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds( true, false );
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / dma.channels;
}

/*
* S_Update_
*/
static void S_Update_() {
	unsigned endtime;
	unsigned samps;
	float gain = s_active ? 1.0 : 0.0f;

	SNDDMA_BeginPainting();

	if( !dma.buffer ) {
		return;
	}

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if( paintedtime < soundtime ) {
		//Com_DPrintf( "S_Update_ : overflow\n" );
		paintedtime = soundtime;
	}

	// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;

	// mix to an even submission block size
	endtime = ( endtime + dma.submission_chunk - 1 ) & ~( dma.submission_chunk - 1 );
	samps = dma.samples >> ( dma.channels - 1 );
	if( (int)( endtime - soundtime ) > samps ) {
		endtime = soundtime + samps;
	}

	if( s_aviDump && s_aviDumpFile ) {
		s_aviNumSamples += S_PaintChannels( endtime, s_aviDumpFile, gain );
	} else {
		S_PaintChannels( endtime, 0, gain );
	}

	SNDDMA_Submit();
}

/*
* S_Spatialize
*/
static void S_Spatialize( void ) {
	int i;
	channel_t *ch;

	S_FreeIdleRawSounds();

	// update spatialization for dynamic sounds
	ch = channels;
	for( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if( !ch->sfx ) {
			continue;
		}
		if( ch->autosound ) {
			// autosounds are regenerated fresh each frame
			memset( ch, 0, sizeof( *ch ) );
			continue;
		}
		S_SpatializeChannel( ch ); // respatialize channel
		if( !ch->leftvol && !ch->rightvol ) {
			memset( ch, 0, sizeof( *ch ) );
			continue;
		}
	}

	S_AddLoopSounds();

	S_SpatializeRawSounds();
}

/*
* S_Update
*/
static void S_Update( void ) {
	int i;
	int total;
	channel_t *ch;

	// rebuild scale tables if volume is modified
	if( s_volume->modified ) {
		S_InitScaletable();
	}

	//
	// debugging output
	//
	if( s_show->integer ) {
		total = 0;
		ch = channels;
		for( i = 0; i < MAX_CHANNELS; i++, ch++ )
			if( ch->sfx && ( ch->leftvol || ch->rightvol ) ) {
				Com_Printf( "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name );
				total++;
			}

		Com_Printf( "----(%i)---- painted: %i\n", total, paintedtime );
	}

	// mix some sound
	S_UpdateBackgroundTrack();

	S_Update_();
}

/*
* S_BeginAviDemo
*/
static void S_BeginAviDemo( void ) {
	size_t checkname_size;
	char *checkname;
	const char *filename = "wavdump";

	if( s_aviDumpFile ) {
		S_StopAviDemo();
	}

	checkname_size = sizeof( char ) * ( strlen( "avi/" ) + strlen( filename ) + 4 + 1 );
	checkname = S_Malloc( checkname_size );
	Q_snprintfz( checkname, checkname_size, "avi/%s.wav", filename );

	if( trap_FS_FOpenFile( checkname, &s_aviDumpFile, FS_WRITE ) == -1 ) {
		Com_Printf( "S_BeginAviDemo: Failed to open %s for writing.\n", checkname );
	} else {
		int i;
		short s;

		// write the WAV header

		trap_FS_Write( "RIFF", 4, s_aviDumpFile );  // "RIFF"
		i = LittleLong( INT_MAX );
		trap_FS_Write( &i, 4, s_aviDumpFile );      // WAVE chunk length
		trap_FS_Write( "WAVE", 4, s_aviDumpFile );  // "WAVE"

		trap_FS_Write( "fmt ", 4, s_aviDumpFile );  // "fmt "
		i = LittleLong( 16 );
		trap_FS_Write( &i, 4, s_aviDumpFile );      // fmt chunk size
		s = LittleShort( 1 );
		trap_FS_Write( &s, 2, s_aviDumpFile );      // audio format. 1 - PCM uncompressed
		s = LittleShort( dma.channels );
		trap_FS_Write( &s, 2, s_aviDumpFile );      // number of channels
		i = LittleLong( dma.speed );
		trap_FS_Write( &i, 4, s_aviDumpFile );      // sample rate
		i = LittleLong( dma.speed * dma.channels * ( dma.samplebits / 8 ) );
		trap_FS_Write( &i, 4, s_aviDumpFile );      // byte rate
		s = LittleShort( dma.channels * ( dma.samplebits / 8 ) );
		trap_FS_Write( &s, 2, s_aviDumpFile );      // block align
		s = LittleLong( dma.samplebits );
		trap_FS_Write( &s, 2, s_aviDumpFile );      // block align

		trap_FS_Write( "data", 4, s_aviDumpFile );  // "data"
		i = LittleLong( INT_MAX - 36 );
		trap_FS_Write( &i, 4, s_aviDumpFile );      // data chunk length

		s_aviDumpFileName = S_Malloc( checkname_size );
		memcpy( s_aviDumpFileName, checkname, checkname_size );
	}

	S_Free( checkname );
}

/*
* S_StopAviDemo
*/
static void S_StopAviDemo( void ) {
	if( s_aviDumpFile ) {
		// don't leave empty files
		if( !s_aviNumSamples ) {
			trap_FS_FCloseFile( s_aviDumpFile );
			trap_FS_RemoveFile( s_aviDumpFileName );
		} else {
			unsigned size;

			// fill in the missing values in RIFF header
			size = ( s_aviNumSamples * dma.channels * ( dma.samplebits / 8 ) ) + 36;
			trap_FS_Seek( s_aviDumpFile, 4, FS_SEEK_SET );
			trap_FS_Write( &size, 4, s_aviDumpFile );

			size -= 36;
			trap_FS_Seek( s_aviDumpFile, 40, FS_SEEK_SET );
			trap_FS_Write( &size, 4, s_aviDumpFile );

			trap_FS_FCloseFile( s_aviDumpFile );
		}

		s_aviDumpFile = 0;
	}

	s_aviNumSamples = 0;

	if( s_aviDumpFileName ) {
		S_Free( s_aviDumpFileName );
		s_aviDumpFileName = NULL;
	}
}

// =====================================================================

/*
* S_HandleInitCmd
*/
static unsigned S_HandleInitCmd( const sndCmdInit_t *cmd ) {
	//Com_Printf("S_HandleShutdownCmd\n");
	S_Init( cmd->maxents, cmd->verbose );
	return sizeof( *cmd );
}

/*
* S_HandleShutdownCmd
*/
static unsigned S_HandleShutdownCmd( const sndCmdShutdown_t *cmd ) {
	//Com_Printf("S_HandleShutdownCmd\n");
	S_Shutdown( cmd->verbose );
	return 0; // terminate
}

/*
* S_HandleClearCmd
*/
static unsigned S_HandleClearCmd( const sndCmdClear_t *cmd ) {
	//Com_Printf("S_HandleClearCmd\n");
	S_Clear();
	return sizeof( *cmd );
}

/*
* S_HandleStopCmd
*/
static unsigned S_HandleStopCmd( const sndCmdStop_t *cmd ) {
	//Com_Printf("S_HandleStopCmd\n");
	S_StopAllSounds( cmd->clear, cmd->stopMusic );
	return sizeof( *cmd );
}

/*
* S_HandleFreeSfxCmd
*/
static unsigned S_HandleFreeSfxCmd( const sndCmdFreeSfx_t *cmd ) {
	sfx_t *sfx;
	//Com_Printf("S_HandleFreeSfxCmd\n");
	sfx = known_sfx + cmd->sfx;
	if( sfx->cache ) {
		S_Free( sfx->cache );
		sfx->cache = NULL;
	}
	return sizeof( *cmd );
}

/*
* S_HandleLoadSfxCmd
*/
static unsigned S_HandleLoadSfxCmd( const sndCmdLoadSfx_t *cmd ) {
	sfx_t *sfx;
	//Com_Printf("S_HandleLoadSfxCmd\n");
	sfx = known_sfx + cmd->sfx;
	S_LoadSound( sfx );
	return sizeof( *cmd );
}

/*
* S_HandleSetAttenuationModelCmd
*/
static unsigned S_HandleSetAttenuationModelCmd( const sndCmdSetAttenuationModel_t *cmd ) {
	//Com_Printf("S_HandleSetAttenuationModelCmd\n");
	S_SetAttenuationModel( cmd->model, cmd->maxdistance, cmd->refdistance );
	return sizeof( *cmd );
}

/*
* S_HandleSetEntitySpatializationCmd
*/
static unsigned S_HandleSetEntitySpatializationCmd( const sndCmdSetEntitySpatialization_t *cmd ) {
	//Com_Printf("S_HandleSetEntitySpatializationCmd\n");
	S_SetEntitySpatialization( cmd->entnum, cmd->origin, cmd->velocity );
	return sizeof( *cmd );
}

/*
* S_HandleSetListenerCmd
*/
static unsigned S_HandleSetListenerCmd( const sndCmdSetListener_t *cmd ) {
	//Com_Printf("S_HandleSetListenerCmd\n");
	VectorCopy( cmd->origin, listenerOrigin );
	VectorCopy( cmd->velocity, listenerVelocity );
	Matrix3_Copy( cmd->axis, listenerAxis );
	s_aviDump = cmd->avidump;
	S_Spatialize();
	return sizeof( *cmd );
}

/*
* S_HandleStartLocalSoundCmd
*/
static unsigned S_HandleStartLocalSoundCmd( const sndCmdStartLocalSound_t *cmd ) {
	//Com_Printf("S_HandleStartLocalSoundCmd\n");
	S_StartGlobalSound( known_sfx + cmd->sfx, 0, cmd->fvol );
	return sizeof( *cmd );
}

/*
* S_HandleStartFixedSoundCmd
*/
static unsigned S_HandleStartFixedSoundCmd( const sndCmdStartFixedSound_t *cmd ) {
	//Com_Printf("S_HandleStartFixedSoundCmd\n");
	S_StartFixedSound( known_sfx + cmd->sfx, cmd->origin, cmd->channel, cmd->fvol, cmd->attenuation );
	return sizeof( *cmd );
}

/*
* S_HandleStartRelativeSoundCmd
*/
static unsigned S_HandleStartRelativeSoundCmd( const sndCmdStartRelativeSound_t *cmd ) {
	//Com_Printf("S_HandleStartRelativeSoundCmd\n");
	S_StartRelativeSound( known_sfx + cmd->sfx, cmd->entnum, cmd->channel, cmd->fvol, cmd->attenuation );
	return sizeof( *cmd );
}

/*
* S_HandleStartGlobalSoundCmd
*/
static unsigned S_HandleStartGlobalSoundCmd( const sndCmdStartGlobalSound_t *cmd ) {
	//Com_Printf("S_HandleStartGlobalSoundCmd\n");
	S_StartGlobalSound( known_sfx + cmd->sfx, cmd->channel, cmd->fvol );
	return sizeof( *cmd );
}

/*
* S_HandleStartBackgroundTrackCmd
*/
static unsigned S_HandleStartBackgroundTrackCmd( const sndCmdStartBackgroundTrack_t *cmd ) {
	//Com_Printf("S_HandleStartBackgroundTrackCmd\n");
	S_StartBackgroundTrack( cmd->intro, cmd->loop, cmd->mode );
	return sizeof( *cmd );
}

/*
* S_HandleStopBackgroundTrackCmd
*/
static unsigned S_HandleStopBackgroundTrackCmd( const sndCmdStopBackgroundTrack_t *cmd ) {
	//Com_Printf("S_HandleStopBackgroundTrackCmd\n");
	S_StopBackgroundTrack();
	return sizeof( *cmd );
}

/*
* S_HandleLockBackgroundTrackCmd
*/
static unsigned S_HandleLockBackgroundTrackCmd( const sndCmdLockBackgroundTrack_t *cmd ) {
	//Com_Printf("S_HandleLockBackgroundTrackCmd\n");
	S_LockBackgroundTrack( cmd->lock );
	return sizeof( *cmd );
}

/*
* S_HandleAddLoopSoundCmd
*/
static unsigned S_HandleAddLoopSoundCmd( const sndAddLoopSoundCmd_t *cmd ) {
	//Com_Printf("S_HandleAddLoopSoundCmd\n");
	S_AddLoopSound( known_sfx + cmd->sfx, cmd->entnum, cmd->fvol, cmd->attenuation );
	return sizeof( *cmd );
}

/*
* S_HandleAdvanceBackgroundTrackCmd
*/
static unsigned S_HandleAdvanceBackgroundTrackCmd( const sndAdvanceBackgroundTrackCmd_t *cmd ) {
	//Com_Printf("S_HandleAdvanceBackgroundTrackCmd\n");
	if( cmd->val < 0 ) {
		S_PrevBackgroundTrack();
	} else if( cmd->val > 0 ) {
		S_NextBackgroundTrack();
	}
	return sizeof( *cmd );
}

/*
* S_HandlePauseBackgroundTrackCmd
*/
static unsigned S_HandlePauseBackgroundTrackCmd( const sndPauseBackgroundTrackCmd_t *cmd ) {
	//Com_Printf("S_HandlePauseBackgroundTrackCmd\n");
	S_PauseBackgroundTrack();
	return sizeof( *cmd );
}

/*
* S_HandleActivateCmd
*/
static unsigned S_HandleActivateCmd( const sndActivateCmd_t *cmd ) {
	bool active;
	//Com_Printf("S_HandleActivateCmd\n");
	active = cmd->active ? true : false;
	if( s_active != active ) {
		s_active = active;
		S_LockBackgroundTrack( !s_active );
		if( active ) {
			S_Activate( true );
			S_Clear();
		} else {
			S_Clear();
			S_Activate( true );
		}
	}
	return sizeof( *cmd );
}

/*
* S_HandleAviDemoCmd
*/
static unsigned S_HandleAviDemoCmd( const sndAviDemo_t *cmd ) {
	if( cmd->begin ) {
		S_BeginAviDemo();
	} else {
		S_StopAviDemo();
	}
	return sizeof( *cmd );
}

/*
* S_HandleRawSamplesCmd
*/
static unsigned S_HandleRawSamplesCmd( const sndRawSamplesCmd_t *cmd ) {
	S_RawSamples( cmd->samples, cmd->rate, cmd->width, cmd->channels,
				  cmd->data, cmd->music );
	S_Free( ( void * )cmd->data );
	return sizeof( *cmd );
}

/*
* S_HandlePositionedRawSamplesCmd
*/
static unsigned S_HandlePositionedRawSamplesCmd( const sndPositionedRawSamplesCmd_t *cmd ) {
	S_PositionedRawSamples( cmd->entnum, cmd->fvol, cmd->attenuation,
							cmd->samples, cmd->rate, cmd->width, cmd->channels, cmd->data );
	S_Free( ( void * )cmd->data );
	return sizeof( *cmd );
}

/*
* S_HandleStuffCmd
*/
static unsigned S_HandleStuffCmd( const sndStuffCmd_t *cmd ) {
	if( !Q_stricmp( cmd->text, "soundlist" ) ) {
		S_SoundList_f();
	}
	return sizeof( *cmd );
}

/*
* S_HandleSetMulEntitySpatializationCmd
*/
static unsigned S_HandleSetMulEntitySpatializationCmd( const sndCmdSetMulEntitySpatialization_t *cmd ) {
	unsigned i;
	//Com_Printf("S_HandleSetEntitySpatializationCmd\n");
	for( i = 0; i < cmd->numents; i++ )
		S_SetEntitySpatialization( cmd->entnum[i], cmd->origin[i], cmd->velocity[i] );
	return sizeof( *cmd );
}

static pipeCmdHandler_t sndCmdHandlers[SND_CMD_NUM_CMDS] =
{
	/* SND_CMD_INIT */
	(pipeCmdHandler_t)S_HandleInitCmd,
	/* SND_CMD_SHUTDOWN */
	(pipeCmdHandler_t)S_HandleShutdownCmd,
	/* SND_CMD_CLEAR */
	(pipeCmdHandler_t)S_HandleClearCmd,
	/* SND_CMD_STOP_ALL_SOUNDS */
	(pipeCmdHandler_t)S_HandleStopCmd,
	/* SND_CMD_FREE_SFX */
	(pipeCmdHandler_t)S_HandleFreeSfxCmd,
	/* SND_CMD_LOAD_SFX */
	(pipeCmdHandler_t)S_HandleLoadSfxCmd,
	/* SND_CMD_SET_ATTENUATION_MODEL */
	(pipeCmdHandler_t)S_HandleSetAttenuationModelCmd,
	/* SND_CMD_SET_ENTITY_SPATIALIZATION */
	(pipeCmdHandler_t)S_HandleSetEntitySpatializationCmd,
	/* SND_CMD_SET_LISTENER */
	(pipeCmdHandler_t)S_HandleSetListenerCmd,
	/* SND_CMD_START_LOCAL_SOUND */
	(pipeCmdHandler_t)S_HandleStartLocalSoundCmd,
	/* SND_CMD_START_FIXED_SOUND */
	(pipeCmdHandler_t)S_HandleStartFixedSoundCmd,
	/* SND_CMD_START_GLOBAL_SOUND */
	(pipeCmdHandler_t)S_HandleStartGlobalSoundCmd,
	/* SND_CMD_START_RELATIVE_SOUND */
	(pipeCmdHandler_t)S_HandleStartRelativeSoundCmd,
	/* SND_CMD_START_BACKGROUND_TRACK */
	(pipeCmdHandler_t)S_HandleStartBackgroundTrackCmd,
	/* SND_CMD_STOP_BACKGROUND_TRACK */
	(pipeCmdHandler_t)S_HandleStopBackgroundTrackCmd,
	/* SND_CMD_LOCK_BACKGROUND_TRACK */
	(pipeCmdHandler_t)S_HandleLockBackgroundTrackCmd,
	/* SND_CMD_ADD_LOOP_SOUND */
	(pipeCmdHandler_t)S_HandleAddLoopSoundCmd,
	/* SND_CMD_ADVANCE_BACKGROUND_TRACK */
	(pipeCmdHandler_t)S_HandleAdvanceBackgroundTrackCmd,
	/* SND_CMD_PAUSE_BACKGROUND_TRACK */
	(pipeCmdHandler_t)S_HandlePauseBackgroundTrackCmd,
	/* SND_CMD_ACTIVATE */
	(pipeCmdHandler_t)S_HandleActivateCmd,
	/* SND_CMD_AVI_DEMO */
	(pipeCmdHandler_t)S_HandleAviDemoCmd,
	/* SND_CMD_RAW_SAMPLES */
	(pipeCmdHandler_t)S_HandleRawSamplesCmd,
	/* SND_CMD_POSITIONED_RAW_SAMPLES */
	(pipeCmdHandler_t)S_HandlePositionedRawSamplesCmd,
	/* SND_CMD_STUFFCMD */
	(pipeCmdHandler_t)S_HandleStuffCmd,
	/* SND_CMD_SET_MUL_ENTITY_SPATIALIZATION */
	(pipeCmdHandler_t)S_HandleSetMulEntitySpatializationCmd,
};

/*
* S_EnqueuedCmdsWaiter
*/
static int S_EnqueuedCmdsWaiter( sndCmdPipe_t *queue, pipeCmdHandler_t *cmdHandlers, bool timeout ) {
	int read = S_ReadEnqueuedCmds( queue, cmdHandlers );
	int64_t now = trap_Milliseconds();

	if( read < 0 ) {
		// shutdown
		return read;
	}

	if( timeout || now >= s_last_update_time + UPDATE_MSEC ) {
		s_last_update_time = now;
		S_Update();
	}

	return read;
}

/*
* S_BackgroundUpdateProc
*/
void *S_BackgroundUpdateProc( void *param ) {
	sndCmdPipe_t *s_cmdPipe = param;

	S_WaitEnqueuedCmds( s_cmdPipe, S_EnqueuedCmdsWaiter, sndCmdHandlers, UPDATE_MSEC );

	return NULL;
}
