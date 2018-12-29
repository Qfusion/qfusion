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

//
// console
//

#define NUM_CON_TIMES 4

// wsw : aiwa : global definition to activate case-sensitivity of console (1 == activated)
#define     CON_CASE_SENSITIVE 0

struct qfontface_s;

extern volatile bool con_initialized;

void Con_CheckResize( void );
void Con_Init( void );
void Con_Shutdown( void );
void Con_DrawConsole( void );
void Con_Print( const char *txt );
void Con_PrintSilent( const char *txt );
void Con_CenteredPrint( char *text );
void Con_Clear_f( void );
void Con_DrawNotify( void );
void Con_DrawChat( int x, int y, int width, struct qfontface_s *font );
void Con_ClearNotify( void );
void Con_ToggleConsole( void );
void Con_Paste( void );
void Con_Close( void );
void Con_SetMessageMode( void );

/**
 * Returns pixel ratio that is suitable for use in the console.
 *
 * @return the pixel ratio
 */
float Con_GetPixelRatio( void );

void Con_KeyDown( int key );
void Con_CharEvent( wchar_t key );
void Con_MessageKeyDown( int key );
void Con_MessageCharEvent( wchar_t key );

int Q_ColorCharCount( const char *s, int byteofs );
int Q_ColorCharOffset( const char *s, int charcount );
