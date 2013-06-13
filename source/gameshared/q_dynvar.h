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

#ifndef GAME_QDYNVAR_H
#define GAME_QDYNVAR_H

#include "q_arch.h"

#ifdef __cplusplus
extern "C" {
#endif

//==========================================================
//
//DYNVARS (dynamic variables)
//
//==========================================================

// layout of dynvar_s structure hidden from users
struct dynvar_s;
typedef struct dynvar_s dynvar_t;

typedef enum dynvar_get_status_t
{
	DYNVAR_GET_OK,
	DYNVAR_GET_WRITEONLY,
	DYNVAR_GET_TRANSIENT
} dynvar_get_status_t;

typedef enum dynvar_set_status_t
{
	DYNVAR_SET_OK,
	DYNVAR_SET_READONLY,
	DYNVAR_SET_INVALID,
	DYNVAR_SET_TRANSIENT
} dynvar_set_status_t;

// getter function signature
// getter writes current value to *val (if not WRITEONLY) and returns status
typedef dynvar_get_status_t ( *dynvar_getter_f )( void **val );

// setter function signature
// setter reads new value from val (if not READONLY) and returns status
typedef dynvar_set_status_t ( *dynvar_setter_f )( void *val );

// listener function signature
// called by Dynvar_SetValue, after the setter function returned with DYNVAR_SET_OK
typedef void ( *dynvar_listener_f )( void *val );

#ifdef __cplusplus
};
#endif

#endif // GAME_QDYNVAR_H

