/*
qfusion
Copyright (c) 2014, Victor Luchits, All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.
*/

#ifndef _STEAMLIB_LOCAL_H_
#define _STEAMLIB_LOCAL_H_

#if defined(_WIN32) || defined (__CYGWIN__)
# define STEAMDLL_EXPORT __declspec(dllexport)
#elif __GNUC__ >= 4
# define STEAMDLL_EXPORT __attribute__((visibility("default")))
#else
# define STEAMDLL_EXPORT
#endif

#include "steamlib_public.h"
#include "steamlib_syscalls.h"

namespace WSWSTEAM {

int SteamLib_API( void );

int SteamLib_Init( void );
void SteamLib_RunFrame( void );
void SteamLib_Shutdown( void );

uint64_t SteamLib_GetSteamID( void );
int SteamLib_GetAuthSessionTicket( void (*callback)( void *, size_t ) );

void SteamLib_AdvertiseGame( const uint8_t *ip, unsigned short port );

void SteamLib_GetPersonaName( char *name, size_t namesize );

}

#endif
