/*
Copyright (C) 2012 Victor Luchits

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

#ifndef _CIN_LOCAL_H_
#define _CIN_LOCAL_H_

#include "../gameshared/q_arch.h"
#include "../gameshared/q_math.h"
#include "../gameshared/q_shared.h"
#include "../gameshared/q_cvar.h"

typedef struct { char *name; void **funcPointer; } dllfunc_t;

#include "cin_public.h"
#include "cin_syscalls.h"

#define CIN_Alloc( pool, size ) trap_MemAlloc( pool, size, __FILE__, __LINE__ )
#define CIN_Free( mem ) trap_MemFree( mem, __FILE__, __LINE__ )
#define CIN_AllocPool( name ) trap_MemAllocPool( name, __FILE__, __LINE__ )
#define CIN_FreePool( pool ) trap_MemFreePool( pool, __FILE__, __LINE__ )
#define CIN_EmptyPool( pool ) trap_MemEmptyPool( pool, __FILE__, __LINE__ )

#define CIN_MAX_RAW_SAMPLES_LISTENERS 8

typedef struct
{
	void *listener;
	cin_raw_samples_cb_t raw_samples;
	cin_get_raw_samples_cb_t get_raw_samples;
} cin_raw_samples_listener_t;

typedef struct cinematics_s
{
	char		*name;

	int			flags;
	float		framerate;

	unsigned int s_rate;
	unsigned short s_width;
	unsigned short s_channels;
	unsigned int s_samples_length;

	int			width;
	int			height;
	int			aspect_numerator, aspect_denominator;

	int			file;
	int			headerlen;

	unsigned int cur_time;
	unsigned int start_time;		// Sys_Milliseconds for first cinematic frame
	unsigned int frame;

	bool	yuv;

	uint8_t		*vid_buffer;

	bool	haveAudio;			// only valid for the current frame
	int			num_listeners;
	cin_raw_samples_listener_t listeners[CIN_MAX_RAW_SAMPLES_LISTENERS];

	int			type;
	void		*fdata;				// format-dependent data
	struct mempool_s *mempool;
} cinematics_t;

void Com_DPrintf( const char *format, ... );

int CIN_API( void );
bool CIN_Init( bool verbose );
void CIN_Shutdown( bool verbose );
char *CIN_CopyString( const char *in );

struct cinematics_s *CIN_Open( const char *name, unsigned int start_time, 
	int flags, bool *yuv, float *framerate );

bool CIN_HasOggAudio( cinematics_t *cin );

const char *CIN_FileName( cinematics_t *cin );

bool CIN_NeedNextFrame( cinematics_t *cin, unsigned int curtime );

uint8_t *CIN_ReadNextFrame( cinematics_t *cin, int *width, int *height, 
	int *aspect_numerator, int *aspect_denominator, bool *redraw );

cin_yuv_t *CIN_ReadNextFrameYUV( cinematics_t *cin, int *width, int *height, 
	int *aspect_numerator, int *aspect_denominator, bool *redraw );

void CIN_ClearRawSamplesListeners( cinematics_t *cin );

bool CIN_AddRawSamplesListener( cinematics_t *cin, void *listener, 
	cin_raw_samples_cb_t raw_samples, cin_get_raw_samples_cb_t get_raw_samples );

void CIN_RawSamplesToListeners( cinematics_t *cin, unsigned int samples, unsigned int rate, 
		unsigned short width, unsigned short channels, const uint8_t *data );

unsigned int CIN_GetRawSamplesLengthFromListeners( cinematics_t *cin );

void CIN_Reset( cinematics_t *cin, unsigned int cur_time );

void CIN_Close( cinematics_t *cin );

#endif
