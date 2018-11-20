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

#include "client.h"
#include "ftlib.h"
#include "ftlib/ftlib_local.h"

mempool_t *ftlib_mempool;

static void CL_FTLibModule_Error( const char *msg ) {
	Com_Error( ERR_FATAL, "%s", msg );
}

static void CL_FTLibModule_Print( const char *msg ) {
	Com_Printf( "%s", msg );
}

static struct shader_s *CL_FTLibModule_RegisterPic( const char *name ) {
	return re.RegisterPic( name );
}

static struct shader_s *CL_FTLibModule_RegisterRawPic( const char *name, int width, int height, uint8_t *data, int samples ) {
	return re.RegisterRawPic( name, width, height, data, samples );
}

static struct shader_s *CL_FTLibModule_RegisterRawAlphaMask( const char *name, int width, int height, uint8_t *data ) {
	return re.RegisterRawAlphaMask( name, width, height, data );
}

static void CL_FTLibModule_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

static void CL_FTLibModule_ReplaceRawSubPic( struct shader_s *shader, int x, int y, int width, int height, uint8_t *data ) {
	re.ReplaceRawSubPic( shader, x, y, width, height, data );
}

static void CL_FTLibModule_Scissor( int x, int y, int w, int h ) {
	re.Scissor( x, y, w, h );
}

static void CL_FTLibModule_GetScissor( int *x, int *y, int *w, int *h ) {
	re.GetScissor( x, y, w, h );
}

static void CL_FTLibModule_ResetScissor( void ) {
	re.ResetScissor();
}

/*
* FTLIB_LoadLibrary
*/
void FTLIB_LoadLibrary( bool verbose ) {
	static ftlib_import_t import;

	import.Print = &CL_FTLibModule_Print;
	import.Error = &CL_FTLibModule_Error;

	import.Cvar_Get = &Cvar_Get;
	import.Cvar_Set = &Cvar_Set;
	import.Cvar_SetValue = &Cvar_SetValue;
	import.Cvar_ForceSet = &Cvar_ForceSet;
	import.Cvar_String = &Cvar_String;
	import.Cvar_Value = &Cvar_Value;

	import.Cmd_Argc = &Cmd_Argc;
	import.Cmd_Argv = &Cmd_Argv;
	import.Cmd_Args = &Cmd_Args;

	import.Cmd_AddCommand = &Cmd_AddCommand;
	import.Cmd_RemoveCommand = &Cmd_RemoveCommand;
	import.Cmd_ExecuteText = &Cbuf_ExecuteText;
	import.Cmd_Execute = &Cbuf_Execute;
	import.Cmd_SetCompletionFunc = &Cmd_SetCompletionFunc;

	import.FS_FOpenFile = &FS_FOpenFile;
	import.FS_Read = &FS_Read;
	import.FS_Write = &FS_Write;
	import.FS_Print = &FS_Print;
	import.FS_Tell = &FS_Tell;
	import.FS_Seek = &FS_Seek;
	import.FS_Eof = &FS_Eof;
	import.FS_Flush = &FS_Flush;
	import.FS_FCloseFile = &FS_FCloseFile;
	import.FS_RemoveFile = &FS_RemoveFile;
	import.FS_GetFileList = &FS_GetFileList;

	import.R_RegisterPic = &CL_FTLibModule_RegisterPic;
	import.R_RegisterRawPic = &CL_FTLibModule_RegisterRawPic;
	import.R_RegisterRawAlphaMask = &CL_FTLibModule_RegisterRawAlphaMask;
	import.R_DrawStretchPic = &CL_FTLibModule_DrawStretchPic;
	import.R_ReplaceRawSubPic = &CL_FTLibModule_ReplaceRawSubPic;
	import.R_Scissor = &CL_FTLibModule_Scissor;
	import.R_GetScissor = &CL_FTLibModule_GetScissor;
	import.R_ResetScissor = &CL_FTLibModule_ResetScissor;

	import.Sys_Milliseconds = &Sys_Milliseconds;
	import.Sys_Microseconds = &Sys_Microseconds;

	import.Sys_LoadLibrary = &Com_LoadSysLibrary;
	import.Sys_UnloadLibrary = Com_UnloadLibrary;

	// load dynamic library
	if( verbose ) {
		Com_Printf( "Loading Fonts module... " );
	}

	// load succeeded
	GetFTLibAPI( &import );
	ftlib_mempool = Mem_AllocPool( NULL, "Fonts Library Module" );

	bool ok = FTLIB_Init( verbose );
	if( ok ) {
		if( verbose ) {
			Com_Printf( "Success.\n" );
		}
	} else {
		// initialization failed
		Mem_FreePool( &ftlib_mempool );
		if( verbose ) {
			Com_Printf( "Initialization failed.\n" );
		}
		FTLIB_UnloadLibrary( verbose );
	}

	Mem_DebugCheckSentinelsGlobal();
}

/*
* FTLIB_UnloadLibrary
*/
void FTLIB_UnloadLibrary( bool verbose ) {
	FTLIB_Shutdown( verbose );
}

/*
* FTLIB_StringWidth
*/
size_t FTLIB_StringWidth( const char *str, struct qfontface_s *font, size_t maxlen, int flags ) {
	return FTLIB_strWidth( str, font, maxlen, flags );
}

/*
* FTLIB_SetDrawCharIntercept
*/
fdrawchar_t FTLIB_SetDrawCharIntercept( fdrawchar_t intercept ) {
	return FTLIB_SetDrawIntercept( intercept );
}
