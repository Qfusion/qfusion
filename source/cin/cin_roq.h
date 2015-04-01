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

#ifndef _CIN_ROQ_H_
#define _CIN_ROQ_H_

#include "cin_local.h"

#define ROQ_FILE_EXTENSIONS ".roq"

bool RoQ_Init_CIN( cinematics_t *cin );
bool RoQ_HasOggAudio_CIN( cinematics_t *cin );
void RoQ_Shutdown_CIN( cinematics_t *cin );
void RoQ_Reset_CIN( cinematics_t *cin );
bool RoQ_NeedNextFrame_CIN( cinematics_t *cin );
cin_yuv_t *RoQ_ReadNextFrameYUV_CIN( cinematics_t *cin, bool *redraw );

#endif
