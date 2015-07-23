/*
Copyright (C) 2014 Victor Luchits

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

#ifndef _STEAM_LIB_H_
#define _STEAM_LIB_H_

void Steam_LoadLibrary( void );
void Steam_UnloadLibrary( void );
void Steam_Init( void );
void Steam_RunFrame( void );
void Steam_Shutdown( void );
uint64_t Steam_GetSteamID( void );
int Steam_GetAuthSessionTicket( void (*callback)( void *, size_t ) );
void Steam_AdvertiseGame( const uint8_t *ip, unsigned short port );
void Steam_GetPersonaName( char *name, size_t namesize );

#endif // _STEAM_LIB_H_
