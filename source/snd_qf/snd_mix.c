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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "snd_local.h"

#define PAINTBUFFER_SIZE    2048
static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
static int snd_scaletable[32][256];
static int *snd_p, snd_linear_count, snd_vol, music_vol;
static short *snd_out;

#if defined ( __arm__ ) && defined ( __GNUC__ )
// 40-50% faster than the C version.
// Uses signed saturation instruction (available since ARMv6 or Thumb2) instead of comparisons,
// with the right shift being a part of the saturation instruction.

static void S_WriteLinearBlastStereo16( void ) {
	int i;
	int val;

	for( i = 0; i < snd_linear_count; i += 2 ) {
		val = snd_p[i];
		__asm__ ( "ssat %0, #16, %0, asr #8\n" : "+r" ( val ) );
		snd_out[i] = val;

		val = snd_p[i + 1];
		__asm__ ( "ssat %0, #16, %0, asr #8\n" : "+r" ( val ) );
		snd_out[i + 1] = val;
	}
}

static void S_WriteSwappedLinearBlastStereo16( void ) {
	int i;
	int val;

	for( i = 0; i < snd_linear_count; i += 2 ) {
		val = snd_p[i + 1];
		__asm__ ( "ssat %0, #16, %0, asr #8\n" : "+r" ( val ) );
		snd_out[i] = val;

		val = snd_p[i];
		__asm__ ( "ssat %0, #16, %0, asr #8\n" : "+r" ( val ) );
		snd_out[i + 1] = val;
	}
}
#elif defined ( _MSC_VER ) && defined( id386 )
static ATTRIBUTE_NAKED void S_WriteLinearBlastStereo16( void ) {
	__asm {
		push edi
		push ebx
		mov ecx, ds:dword ptr[snd_linear_count]
		mov ebx, ds:dword ptr[snd_p]
		mov edi, ds:dword ptr[snd_out]

LWLBLoopTop:
		mov eax, ds:dword ptr[-8 + ebx + ecx * 4]
		sar eax, 8
		cmp eax, 07FFFh
		jg LClampHigh
		cmp eax, 0FFFF8000h
		jnl LClampDone
		mov eax, 0FFFF8000h
		jmp LClampDone

LClampHigh:
		mov eax, 07FFFh

LClampDone:
		mov edx, ds:dword ptr[-4 + ebx + ecx * 4]
		sar edx, 8
		cmp edx, 07FFFh
		jg LClampHigh2
		cmp edx, 0FFFF8000h
		jnl LClampDone2
		mov edx, 0FFFF8000h
		jmp LClampDone2

LClampHigh2:
		mov edx, 07FFFh

LClampDone2:
		shl edx, 16
		and eax, 0FFFFh
		or edx, eax
		mov ds:dword ptr[-4 + edi + ecx * 2], edx
		sub ecx, 2
		jnz LWLBLoopTop
		pop ebx
		pop edi
		ret
	}
}

static ATTRIBUTE_NAKED void S_WriteSwappedLinearBlastStereo16( void ) {
	__asm {
		push edi
		push ebx
		mov ecx, ds:dword ptr[snd_linear_count]
		mov ebx, ds:dword ptr[snd_p]
		mov edi, ds:dword ptr[snd_out]

LWLBLoopTop:
		mov eax, ds:dword ptr[-4 + ebx + ecx * 4]
		sar eax, 8
		cmp eax, 07FFFh
		jg LClampHigh
		cmp eax, 0FFFF8000h
		jnl LClampDone
		mov eax, 0FFFF8000h
		jmp LClampDone

LClampHigh:
		mov eax, 07FFFh

LClampDone:
		mov edx, ds:dword ptr[-8 + ebx + ecx * 4]
		sar edx, 8
		cmp edx, 07FFFh
		jg LClampHigh2
		cmp edx, 0FFFF8000h
		jnl LClampDone2
		mov edx, 0FFFF8000h
		jmp LClampDone2

LClampHigh2:
		mov edx, 07FFFh

LClampDone2:
		shl edx, 16
		and eax, 0FFFFh
		or edx, eax
		mov ds:dword ptr[-4 + edi + ecx * 2], edx
		sub ecx, 2
		jnz LWLBLoopTop
		pop ebx
		pop edi
		ret
	}
}
#else
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4310 )       // cast truncates constant value
#endif
static void S_WriteLinearBlastStereo16( void ) {
	int i;
	int val;

	for( i = 0; i < snd_linear_count; i += 2 ) {
		val = snd_p[i] >> 8;
		snd_out[i] = Q_bound( (short)0x8000, val, 0x7fff );

		val = snd_p[i + 1] >> 8;
		snd_out[i + 1] = Q_bound( (short)0x8000, val, 0x7fff );
	}
}

static void S_WriteSwappedLinearBlastStereo16( void ) {
	int i;
	int val;

	for( i = 0; i < snd_linear_count; i += 2 ) {
		val = snd_p[i + 1] >> 8;
		snd_out[i] = Q_bound( (short)0x8000, val, 0x7fff );

		val = snd_p[i] >> 8;
		snd_out[i + 1] = Q_bound( (short)0x8000, val, 0x7fff );
	}
#ifdef _MSC_VER
#pragma warning( pop )
#endif
}
#endif

static void S_TransferStereo16( unsigned int *pbuf, int endtime ) {
	int lpos;
	int lpaintedtime;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

	while( lpaintedtime < endtime ) {
		// handle recirculating buffer issues
		lpos = lpaintedtime & ( ( dma.samples >> 1 ) - 1 );

		snd_out = (short *) pbuf + ( lpos << 1 );

		snd_linear_count = ( dma.samples >> 1 ) - lpos;
		if( lpaintedtime + snd_linear_count > endtime ) {
			snd_linear_count = endtime - lpaintedtime;
		}

		snd_linear_count <<= 1;

		// write a linear blast of samples
		if( s_swapstereo->integer ) {
			S_WriteSwappedLinearBlastStereo16();
		} else {
			S_WriteLinearBlastStereo16();
		}

		snd_p += snd_linear_count;
		lpaintedtime += ( snd_linear_count >> 1 );
	}
}

/*
* S_ClearPaintBuffer
*/
void S_ClearPaintBuffer( void ) {
	memset( paintbuffer, 0, sizeof( paintbuffer ) );
}

/*
* S_TransferPaintBuffer
*/
static void S_TransferPaintBuffer( int endtime ) {
	int out_idx;
	int count;
	int out_mask;
	int *p;
	int step;
	int val;
	unsigned int *pbuf;

	pbuf = (unsigned int *)dma.buffer;
/*
    if( s_testsound->integer )
    {
        int i;
        int count;

        // write a fixed sine wave
        count = endtime - paintedtime;
        for( i = 0; i < count; i++ )
            paintbuffer[i].left = paintbuffer[i].right = sin( ( paintedtime+i )*0.1 )*20000*256;
    }
*/
	if( dma.samplebits == 16 && dma.channels == 2 ) { // optimized case
		S_TransferStereo16( pbuf, endtime );
	} else {   // general case
		p = (int *)paintbuffer;
		count = ( endtime - paintedtime ) * dma.channels;
		out_mask = dma.samples - 1;
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if( dma.samplebits == 16 ) {
			short *out = (short *)pbuf;
			while( count-- ) {
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) {
					val = 0x7fff;
				} else if( val < -32768 ) {
					val = -32768;
				}
				out[out_idx] = val;
				out_idx = ( out_idx + 1 ) & out_mask;
			}
		} else if( dma.samplebits == 8 ) {
			unsigned char *out = (unsigned char *)pbuf;
			while( count-- ) {
				val = *p >> 8;
				p += step;
				if( val > 0x7fff ) {
					val = 0x7fff;
				} else if( val < -32768 ) {
					val = -32768;
				}
				out[out_idx] = ( val >> 8 ) + 128;
				out_idx = ( out_idx + 1 ) & out_mask;
			}
		}
	}
}

/*
* S_DumpPaintBuffer
*/
static void S_DumpPaintBuffer( int endtime, int file ) {
	int in_idx;
	int len, count;
	int in_mask;
	unsigned int *pbuf;
	uint8_t *raw;

	pbuf = (unsigned int *)dma.buffer;
	count = ( endtime - paintedtime ) * dma.channels;
	in_mask = dma.samples - 1;
	in_idx = paintedtime * dma.channels & in_mask;

	len = count * dma.samplebits / 8;
	raw = S_Malloc( len );

	if( dma.samplebits == 16 ) {
		short *in = (short *)pbuf;
		short *out = (short *)raw;
		while( count-- ) {
			*out++ = LittleShort( in[in_idx] );
			in_idx = ( in_idx + 1 ) & in_mask;
		}
	} else {
		unsigned char *in = (unsigned char *)pbuf;
		unsigned char *out = (unsigned char *)raw;
		while( count-- ) {
			*out++ = in[in_idx];
			in_idx = ( in_idx + 1 ) & in_mask;
		}
	}

	trap_FS_Write( raw, len, file );

	S_Free( raw );
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static void S_PaintChannelFrom8( channel_t *ch, sfxcache_t *sc, unsigned int endtime, int offset );
static void S_PaintChannelFrom16( channel_t *ch, sfxcache_t *sc, unsigned int endtime, int offset );
static void S_PaintChannelFrom8HQ( channel_t *ch, sfxcache_t *sc, unsigned int endtime, int offset );
static void S_PaintChannelFrom16HQ( channel_t *ch, sfxcache_t *sc, unsigned int endtime, int offset );

int S_PaintChannels( unsigned int endtime, int dumpfile, float gain ) {
	unsigned int i;
	unsigned int end;
	unsigned int total;
	channel_t *ch;
	sfxcache_t *sc;
	unsigned int ltime, count;
	playsound_t *ps;

	total = 0;
	snd_vol = s_volume->value * gain * 256;
	music_vol = s_musicvolume->value * gain * 256;

	while( paintedtime < endtime ) {
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if( endtime - paintedtime > PAINTBUFFER_SIZE ) {
			end = paintedtime + PAINTBUFFER_SIZE;
		}

		// start any playsounds
		while( 1 ) {
			ps = s_pendingplays.next;
			if( ps == &s_pendingplays ) {
				break; // no more pending sounds

			}
			if( (int)ps->begin <= paintedtime ) {
				S_IssuePlaysound( ps );
				continue;
			}

			if( (int)ps->begin < end ) {
				end = ps->begin; // stop here
			}
			break;
		}

		// clear the paint buffer
		memset( paintbuffer, 0, ( end - paintedtime ) * sizeof( portable_samplepair_t ) );

		// paint in the raw samples
		for( i = 0; i < MAX_RAW_SOUNDS; i++ ) {
			// copy from the streaming sound source
			int s;
			unsigned j, stop;
			rawsound_t *rawsound = raw_sounds[i];

			if( !rawsound ) {
				continue;
			}
			if( !rawsound->left_volume && !rawsound->right_volume ) {
				// not audible
				continue;
			}

			stop = ( end < rawsound->rawend ) ? end : rawsound->rawend;
			for( j = paintedtime; j < stop; j++ ) {
				s = j & ( MAX_RAW_SAMPLES - 1 );
				paintbuffer[j - paintedtime].left += rawsound->rawsamples[s].left * rawsound->left_volume;
				paintbuffer[j - paintedtime].right += rawsound->rawsamples[s].right * rawsound->right_volume;
			}
		}

		// paint in the channels.
		ch = channels;
		for( i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			ltime = paintedtime;

			while( ltime < end ) {
				if( !ch->sfx || ( !ch->leftvol && !ch->rightvol ) ) {
					break;
				}

				count = 0;

				// max painting is to the end of the buffer
				if( end > ltime ) {
					count = end - ltime;
				}

				// might be stopped by running out of data
				if( ch->end < end ) {
					count = ch->end > ltime ? ch->end - ltime : 0;
				}

				sc = S_LoadSound( ch->sfx );
				if( !sc ) {
					break;
				}

				if( count > 0 && ch->sfx ) {
					if( s_pseudoAcoustics->value ) {
						if( sc->width == 1 ) {
							S_PaintChannelFrom8HQ( ch, sc, count, ltime - paintedtime );
						} else {
							S_PaintChannelFrom16HQ( ch, sc, count, ltime - paintedtime );
						}
					} else {
						if( sc->width == 1 ) {
							S_PaintChannelFrom8( ch, sc, count, ltime - paintedtime );
						} else {
							S_PaintChannelFrom16( ch, sc, count, ltime - paintedtime );
						}
					}
					ltime += count;
				}

				// if at end of loop, restart
				if( ltime >= ch->end ) {
					if( ch->autosound ) { // autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					} else if( sc->loopstart < sc->length ) {
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					} else {   // channel just stopped
						ch->sfx = NULL;
					}
				}
			}
		}

		// dump to file
		if( dumpfile ) {
			S_DumpPaintBuffer( end, dumpfile );
		}

		// transfer out according to DMA format
		total += end - paintedtime;
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}

	return total;
}

void S_InitScaletable( void ) {
	int i, j;
	int scale;

	s_volume->modified = false;
	for( i = 0; i < 32; i++ ) {
		scale = i * 8 * 256 * s_volume->value;
		for( j = 0; j < 256; j++ )
			snd_scaletable[i][j] = ( (signed char)j ) * scale;
	}
}

static void S_PaintChannelFrom8( channel_t *ch, sfxcache_t *sc, unsigned int count, int offset ) {
	unsigned int i;
	int j;
	int *lscale, *rscale;
	unsigned char *sfx;
	portable_samplepair_t *samp;

	if( ch->leftvol > 255 ) {
		ch->leftvol = 255;
	}
	if( ch->rightvol > 255 ) {
		ch->rightvol = 255;
	}

	if( !s_volume->value ) {
		ch->pos += count;
		return;
	}

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	samp = &paintbuffer[offset];

	if( sc->channels == 2 ) {
		sfx = (unsigned char *)sc->data + ch->pos * 2;

		for( i = 0; i < count; i++, samp++ ) {
			samp->left += lscale[*sfx++];
			samp->right += rscale[*sfx++];
		}
	} else {
		sfx = (unsigned char *)sc->data + ch->pos;

		for( i = 0; i < count; i++, samp++ ) {
			j = *sfx++;
			samp->left += lscale[j];
			samp->right += rscale[j];
		}
	}

	ch->pos += count;
}

static void S_PaintChannelFrom16( channel_t *ch, sfxcache_t *sc, unsigned int count, int offset ) {
	unsigned int i;
	int j;
	int leftvol, rightvol;
	signed short *sfx;
	portable_samplepair_t *samp;

	if( !snd_vol ) {
		ch->pos += count;
		return;
	}

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	samp = &paintbuffer[offset];

	if( sc->channels == 2 ) {
		sfx = (signed short *)sc->data + ch->pos * 2;

		for( i = 0; i < count; i++, samp++ ) {
			samp->left += ( *sfx++ *leftvol ) >> 8;
			samp->right += ( *sfx++ *rightvol ) >> 8;
		}
	} else {
		sfx = (signed short *)sc->data + ch->pos;

		for( i = 0; i < count; i++, samp++ ) {
			j = *sfx++;
			samp->left += ( j * leftvol ) >> 8;
			samp->right += ( j * rightvol ) >> 8;
		}
	}

	ch->pos += count;
}

static void S_PaintChannelFrom8HQ( channel_t *ch, sfxcache_t *sc, unsigned int count, int offset ) {
	unsigned int i;
	int j, k;
	int *lscale, *rscale;
	unsigned char *sfx;
	portable_samplepair_t *samp;

	if( ch->leftvol > 255 ) {
		ch->leftvol = 255;
	}
	if( ch->rightvol > 255 ) {
		ch->rightvol = 255;
	}

	if( !s_volume->value ) {
		ch->pos += count;
		return;
	}

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	samp = &paintbuffer[offset];

	if( sc->channels == 2 ) {
		sfx = (unsigned char *)sc->data + ch->pos * 2;

		for( i = 0; i < count; i++, samp++ ) {
			samp->left += lscale[*sfx++];
			samp->right += rscale[*sfx++];
		}
	} else {
		sfx = (unsigned char *)sc->data + ch->pos;

		// initialize our counter here
		i = 0;
		if( ch->pos < ch->ldelay ) {
			// left channel delayed, write first right channels
			unsigned int rights = min( count, ch->ldelay - ch->pos );
			for( ; i < rights; i++, samp++ ) {
				j = *sfx++;
				j = S_Lowpass2pole( j << 8, &ch->lpf_history[2], ch->lpf_rcoeff ) >> 8;
				samp->right += rscale[j & 255];
			}
		} else if( ch->pos < ch->rdelay ) {
			// right channel delayed, write first left channels
			unsigned int lefts = min( count, ch->rdelay - ch->pos );
			for( ; i < lefts; i++, samp++ ) {
				j = *sfx++;
				j = S_Lowpass2pole( j << 8, &ch->lpf_history[0], ch->lpf_lcoeff ) >> 8;
				samp->left += lscale[j & 255];
			}
		}

		// write the common samples for both channels
		for( ; i < count; i++, samp++, sfx++ ) {
			j = *( sfx - ch->ldelay ) << 8;
			k = *( sfx - ch->rdelay ) << 8;

			j = S_Lowpass2pole( j, &ch->lpf_history[0], ch->lpf_lcoeff ) >> 8;
			k = S_Lowpass2pole( k, &ch->lpf_history[2], ch->lpf_rcoeff ) >> 8;
			samp->left += lscale[j & 255];
			samp->right += rscale[k & 255];
		}

		// TODO: write the rest of the delayed channel
	}

	ch->pos += count;
}

static void S_PaintChannelFrom16HQ( channel_t *ch, sfxcache_t *sc, unsigned int count, int offset ) {
	unsigned int i;
	int j, k;
	int leftvol, rightvol;
	signed short *sfx;
	portable_samplepair_t *samp;

	if( !snd_vol ) {
		ch->pos += count;
		return;
	}

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	samp = &paintbuffer[offset];

	if( sc->channels == 2 ) {
		sfx = (signed short *)sc->data + ch->pos * 2;

		for( i = 0; i < count; i++, samp++ ) {
			samp->left += ( *sfx++ *leftvol ) >> 8;
			samp->right += ( *sfx++ *rightvol ) >> 8;
		}
	} else {
		sfx = (signed short *)sc->data + ch->pos;

		// initialize our counter here
		i = 0;
		if( ch->pos < ch->ldelay ) {
			// left channel delayed, write first right channels
			unsigned int rights = min( count, ch->ldelay - ch->pos );
			for( ; i < rights; i++, samp++ ) {
				j = *sfx++;
				samp->right += ( S_Lowpass2pole( j, &ch->lpf_history[2], ch->lpf_rcoeff ) * rightvol ) >> 8;
			}
		} else if( ch->pos < ch->rdelay ) {
			// right channel delayed, write first left channels
			unsigned int lefts = min( count, ch->rdelay - ch->pos );
			for( ; i < lefts; i++, samp++ ) {
				j = *sfx++;
				samp->left += ( S_Lowpass2pole( j, &ch->lpf_history[0], ch->lpf_lcoeff ) * leftvol ) >> 8;
			}
		}

		// write the common samples for both chann[1]els
		for( ; i < count; i++, samp++, sfx++ ) {
			j = *( sfx - ch->ldelay );
			k = *( sfx - ch->rdelay );

			samp->left += ( S_Lowpass2pole( j, &ch->lpf_history[0], ch->lpf_lcoeff ) * leftvol ) >> 8;
			samp->right += ( S_Lowpass2pole( k, &ch->lpf_history[2], ch->lpf_rcoeff ) * rightvol ) >> 8;
		}

		// TODO: write the rest of the delayed channel
	}

	ch->pos += count;
}
