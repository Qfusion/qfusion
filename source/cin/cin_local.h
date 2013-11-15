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

#include "cin_public.h"
#include "cin_syscalls.h"

#define CIN_Alloc( pool, size ) trap_MemAlloc( pool, size, __FILE__, __LINE__ )
#define CIN_Free( mem ) trap_MemFree( mem, __FILE__, __LINE__ )
#define CIN_AllocPool( name ) trap_MemAllocPool( name, __FILE__, __LINE__ )
#define CIN_FreePool( pool ) trap_MemFreePool( pool, __FILE__, __LINE__ )
#define CIN_EmptyPool( pool ) trap_MemEmptyPool( pool, __FILE__, __LINE__ )

#define CIN_LOOP					1
#define CIN_AUDIO					2

void Com_DPrintf( const char *format, ... );

typedef struct cinematics_s
{
	char		*name;

	int			flags;
	float		framerate;

	unsigned int s_rate;
	unsigned short s_width;
	unsigned short s_channels;

	int			width;
	int			height;
	int			aspect_numerator, aspect_denominator;

	int			file;
	int			headerlen;

	unsigned int cur_time;
	unsigned int start_time;              // Sys_Milliseconds for first cinematic frame
	unsigned int frame;

	qboolean	yuv;

	qbyte		*vid_buffer;

	int			type;
	void		*fdata;				// format-dependent data
	struct mempool_s *mempool;
} cinematics_t;

int CIN_API( void );
qboolean CIN_Init( qboolean verbose );
void CIN_Shutdown( qboolean verbose );
char *CIN_CopyString( const char *in );

struct cinematics_s *CIN_Open( const char *name, unsigned int start_time, qboolean loop, qboolean audio, qboolean *yuv );
qboolean CIN_NeedNextFrame( struct cinematics_s *cin, unsigned int curtime );
qbyte *CIN_ReadNextFrame( struct cinematics_s *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, qboolean *redraw );
cin_yuv_t *CIN_ReadNextFrameYUV( struct cinematics_s *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, qboolean *redraw );
void CIN_Close( struct cinematics_s *cin );

#endif
