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
// vid.h -- video driver defs

#pragma once

typedef struct {
	unsigned width, height;             // coordinates from main game
} viddef_t;

extern viddef_t viddef;             // global video state

void VID_Init();
void VID_Shutdown();
void VID_CheckChanges();

// The sound module may require the handle when using directsound
void VID_FlashWindow();

void VID_AppActivate( bool active, bool minimize );
bool VID_AppIsActive( void );
bool VID_AppIsMinimized( void );
int VID_GetWindowWidth( void );
int VID_GetWindowHeight( void );
float VID_GetPixelRatio( void );
