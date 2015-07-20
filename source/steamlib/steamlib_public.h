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

#ifndef _STEAMLIB_PUBLIC_H_
#define _STEAMLIB_PUBLIC_H_

#include <stdint.h>
#include <stddef.h> // For size_t

// steamlib_public.h - steam integration subsystem

#define	STEAMLIB_API_VERSION 3

//===============================================================

//
// functions provided by the main engine
//
typedef struct
{
	// halts the application or drops to console
	void ( *Com_Error )( int code, const char *format, ... );

	// console messages
	void ( *Com_Printf )( const char *format, ... );
	void ( *Com_DPrintf )( const char *format, ... );

	// console commands
	void ( *Cbuf_ExecuteText )( int exec_when, const char *text );
} steamlib_import_t;

//
// functions exported by the steam integration subsystem
//
typedef struct
{
	// if API is different, the dll cannot be used
	int ( *API )( void );

	int ( *Init )( void );
	void ( *RunFrame )( void );
	void ( *Shutdown )( void );

	uint64_t ( *GetSteamID )( void );
	int ( *GetAuthSessionTicket )( void (*callback)( void *, size_t ) );

	void ( *AdvertiseGame )( const uint8_t *ip, unsigned short port );

	void ( *GetPersonaName )( char *name, size_t namesize );
} steamlib_export_t;

#endif
