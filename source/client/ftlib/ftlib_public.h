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

#pragma once

#include "qcommon/qcommon.h"

// ftlib_public.h - font provider subsystem

struct shader_s;
struct qfontface_s;

typedef void ( *fdrawchar_t )( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );

//
// functions provided by the main engine
//
typedef struct {
	// drops to console a client game error
	void ( *Error )( const char *msg );

	// console messages
	void ( *Print )( const char *msg );

	// console variable interaction
	cvar_t *( *Cvar_Get )( const char *name, const char *value, int flags );
	cvar_t *( *Cvar_Set )( const char *name, const char *value );
	void ( *Cvar_SetValue )( const char *name, float value );
	cvar_t *( *Cvar_ForceSet )( const char *name, const char *value );      // will return 0 0 if not found
	float ( *Cvar_Value )( const char *name );
	const char *( *Cvar_String )( const char *name );

	int ( *Cmd_Argc )( void );
	char *( *Cmd_Argv )( int arg );
	char *( *Cmd_Args )( void );        // concatenation of all argv >= 1

	void ( *Cmd_AddCommand )( const char *name, void ( *cmd )( void ) );
	void ( *Cmd_RemoveCommand )( const char *cmd_name );
	void ( *Cmd_ExecuteText )( int exec_when, const char *text );
	void ( *Cmd_Execute )( void );
	void ( *Cmd_SetCompletionFunc )( const char *cmd_name, char **( *completion_func )( const char *partial ) );

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int ( *FS_FOpenFile )( const char *filename, int *filenum, int mode );
	int ( *FS_Read )( void *buffer, size_t len, int file );
	int ( *FS_Write )( const void *buffer, size_t len, int file );
	int ( *FS_Print )( int file, const char *msg );
	int ( *FS_Tell )( int file );
	int ( *FS_Seek )( int file, int offset, int whence );
	int ( *FS_Eof )( int file );
	int ( *FS_Flush )( int file );
	void ( *FS_FCloseFile )( int file );
	bool ( *FS_RemoveFile )( const char *filename );
	int ( *FS_GetFileList )( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end );

	// clock
	int64_t ( *Sys_Milliseconds )( void );
	uint64_t ( *Sys_Microseconds )( void );

	void *( *Sys_LoadLibrary )( const char *name, dllfunc_t * funcs );
	void ( *Sys_UnloadLibrary )( void **lib );

	// renderer
	struct shader_s *( *R_RegisterPic )( const char *name );
	struct shader_s * ( *R_RegisterRawPic )( const char *name, int width, int height, uint8_t * data, int samples );
	struct shader_s * ( *R_RegisterRawAlphaMask )( const char *name, int width, int height, uint8_t * data );
	void ( *R_DrawStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );
	void ( *R_ReplaceRawSubPic )( struct shader_s *shader, int x, int y, int width, int height, uint8_t *data );
	void ( *R_Scissor )( int x, int y, int w, int h );
	void ( *R_GetScissor )( int *x, int *y, int *w, int *h );
	void ( *R_ResetScissor )( void );
} ftlib_import_t;
