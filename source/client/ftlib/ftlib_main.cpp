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

#include "ftlib_local.h"

struct mempool_s *ftlibPool;

/*
* FTLIB_Init
*/
bool FTLIB_Init() {
	ftlibPool = Mem_AllocPool( NULL, "Fonts Library Module" );

	FTLIB_InitSubsystems();

	return true;
}

/*
* FTLIB_Shutdown
*/
void FTLIB_Shutdown() {
	FTLIB_ShutdownSubsystems();

	Mem_FreePool( &ftlibPool );
}

/*
* FTLIB_CopyString
*/
char *FTLIB_CopyString( const char *in ) {
	char *out;

	out = ( char* )Mem_Alloc( ftlibPool, sizeof( char ) * ( strlen( in ) + 1 ) );
	Q_strncpyz( out, in, sizeof( char ) * ( strlen( in ) + 1 ) );

	return out;
}
