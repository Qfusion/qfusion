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

#include "../cin/cin_public.h"

void CIN_LoadLibrary( bool verbose );
void CIN_UnloadLibrary( bool verbose );

struct cinematics_s *CIN_Open( const char *name, unsigned int start_time, 
	int flags, bool *yuv, float *framerate );

bool CIN_HasOggAudio( struct cinematics_s *cin );

const char *CIN_FileName( struct cinematics_s *cin );

bool CIN_NeedNextFrame( struct cinematics_s *cin, unsigned int curtime );

uint8_t *CIN_ReadNextFrame( struct cinematics_s *cin, int *width, int *height, 
	int *aspect_numerator, int *aspect_denominator, bool *redraw );

ref_yuv_t *CIN_ReadNextFrameYUV( struct cinematics_s *cin, int *width, int *height, 
	int *aspect_numerator, int *aspect_denominator, bool *redraw );

bool CIN_AddRawSamplesListener( struct cinematics_s *cin, void *listener,
	cin_raw_samples_cb_t rs, cin_get_raw_samples_cb_t grs );

void CIN_Reset( struct cinematics_s *cin, unsigned int cur_time );

void CIN_Close( struct cinematics_s *cin );
