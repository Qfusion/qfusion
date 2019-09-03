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
// input.h -- external (non-keyboard) input devices

void IN_Init( void );
void IN_Shutdown( void );
void IN_Restart( void );

void IN_Commands( void ); // opportunity for devices to stick commands on the script buffer
void IN_GetMouseMovement( int *dx, int *dy );
void IN_GetMousePosition( int *x, int *y );
void IN_GetThumbsticks( vec4_t sticks );

void IN_Frame( void );

unsigned int IN_SupportedDevices( void );

void IN_ShowSoftKeyboard( bool show );

void IN_GetInputLanguage( char *dest, size_t size );

void IN_IME_Enable( bool enable );
size_t IN_IME_GetComposition( char *str, size_t strSize, size_t *cursorPos, size_t *convStart, size_t *convLen );
unsigned int IN_IME_GetCandidates( char * const *cands, size_t candSize, unsigned int maxCands, int *selected, int *firstKey );
