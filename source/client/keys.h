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

#include "../gameshared/q_keycodes.h"

extern int anykeydown;

void Key_CharEvent( int key, qwchar charkey );
void Key_Event( int key, qboolean down, unsigned time );
void Key_MouseEvent( int key, qboolean down, unsigned time );
void Key_Init( void );
void Key_Shutdown( void );
void Key_WriteBindings( int file );
void Key_SetBinding( int keynum, const char *binding );
void Key_ClearStates( void );
const char *Key_GetBindingBuf( int binding );

const char *Key_KeynumToString( int keynum );

int Key_StringToKeynum( const char *str );
qboolean Key_IsDown( int keynum );

// wsw : aiwa : delegate pattern to forward key strokes to arbitrary code
// delegates can be stacked, the topmost delegate is sent the key
typedef void ( *key_delegate_f )( int key, qboolean *key_down );
typedef void ( *key_char_delegate_f )( qwchar c );
keydest_t Key_DelegatePush( key_delegate_f key_del, key_char_delegate_f char_del );  // returns previous dest
void Key_DelegatePop( keydest_t next_dest );
