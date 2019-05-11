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

static ftlib_export_t *ftlib_export;
static void *ftlib_libhandle = NULL;
static mempool_t *ftlib_mempool;

#ifndef _MSC_VER
static void CL_FTLibModule_Error( const char *msg ) __attribute( ( noreturn ) );
#else
__declspec( noreturn ) static void CL_FTLibModule_Error( const char *msg );
#endif

static void CL_FTLibModule_Error( const char *msg ) {
	Com_Error( ERR_FATAL, "%s", msg );
}

static void CL_FTLibModule_Print( const char *msg ) {
	Com_Printf( "%s", msg );
}

static void *CL_FTLibModule_MemAlloc( mempool_t *pool, size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( pool, size, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static void CL_FTLibModule_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static mempool_t *CL_FTLibModule_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool( ftlib_mempool, name, MEMPOOL_CINMODULE, filename, fileline );
}

static void CL_FTLibModule_MemFreePool( mempool_t **pool, const char *filename, int fileline ) {
	_Mem_FreePool( pool, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static void CL_FTLibModule_MemEmptyPool( mempool_t *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool( pool, MEMPOOL_CINMODULE, 0, filename, fileline );
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
	dllfunc_t funcs[2];
	void *( *GetFTLibAPI )( void * );

	assert( !ftlib_libhandle );

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
	import.FS_IsUrl = &FS_IsUrl;

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

	import.Mem_AllocPool = &CL_FTLibModule_MemAllocPool;
	import.Mem_Alloc = &CL_FTLibModule_MemAlloc;
	import.Mem_Realloc = &_Mem_Realloc;
	import.Mem_Free = &CL_FTLibModule_MemFree;
	import.Mem_FreePool = &CL_FTLibModule_MemFreePool;
	import.Mem_EmptyPool = &CL_FTLibModule_MemEmptyPool;

	// load dynamic library
	ftlib_export = NULL;
	if( verbose ) {
		Com_Printf( "Loading Fonts module... " );
	}

	funcs[0].name = "GetFTLibAPI";
	funcs[0].funcPointer = ( void ** ) &GetFTLibAPI;
	funcs[1].name = NULL;
	ftlib_libhandle = Com_LoadLibrary( LIB_DIRECTORY "/" LIB_PREFIX "ftlib" LIB_SUFFIX, funcs );

	if( ftlib_libhandle ) {
		// load succeeded
		int api_version;

		ftlib_export = GetFTLibAPI( &import );
		ftlib_mempool = Mem_AllocPool( NULL, "Fonts Library Module" );

		api_version = ftlib_export->API();

		if( api_version == FTLIB_API_VERSION ) {
			if( ftlib_export->Init( verbose ) ) {
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
		} else {
			// wrong version
			Mem_FreePool( &ftlib_mempool );
			Com_Printf( "ftlib_LoadLibrary: wrong version: %i, not %i.\n", api_version, FTLIB_API_VERSION );
			FTLIB_UnloadLibrary( verbose );
		}
	} else {
		if( verbose ) {
			Com_Printf( "Not found.\n" );
		}
	}

	Mem_DebugCheckSentinelsGlobal();
}

/*
* FTLIB_UnloadLibrary
*/
void FTLIB_UnloadLibrary( bool verbose ) {
	if( ftlib_export != NULL ) {
		ftlib_export->Shutdown( verbose );
		ftlib_export = NULL;
	}

	if( !ftlib_libhandle ) {
		return;
	}

	assert( ftlib_libhandle != NULL );

	Com_UnloadLibrary( &ftlib_libhandle );

	assert( !ftlib_libhandle );

	if( verbose ) {
		Com_Printf( "Fonts module unloaded.\n" );
	}
}

/*
* FTLIB_RegisterFont
*/
struct qfontface_s *FTLIB_RegisterFont( const char *family, const char *fallback, int style, unsigned int size ) {
	return ftlib_export ? ftlib_export->RegisterFont( family, fallback, style, size ) : NULL;
}

/*
* FTLIB_TouchFont
*/
void FTLIB_TouchFont( struct qfontface_s *qfont ) {
	if( ftlib_export ) {
		ftlib_export->TouchFont( qfont );
	}
}

/*
* FTLIB_TouchAllFonts
*/
void FTLIB_TouchAllFonts( void ) {
	if( ftlib_export ) {
		ftlib_export->TouchAllFonts();
	}
}

/*
* FTLIB_PrecacheFonts
*/
void FTLIB_PrecacheFonts( bool verbose ) {
	if( ftlib_export ) {
		ftlib_export->PrecacheFonts( verbose );
	}
}

/*
* FTLIB_FreeFonts
*/
void FTLIB_FreeFonts( bool verbose ) {
	if( ftlib_export ) {
		ftlib_export->FreeFonts( verbose );
	}
}

// drawing functions

/*
* FTLIB_FontSize
*/
size_t FTLIB_FontSize( struct qfontface_s *font ) {
	return ftlib_export ? ftlib_export->FontSize( font ) : 0;
}

/*
* FTLIB_FontHeight
*/
size_t FTLIB_FontHeight( struct qfontface_s *font ) {
	return ftlib_export ? ftlib_export->FontHeight( font ) : 0;
}

/*
* FTLIB_StringWidth
*/
size_t FTLIB_StringWidth( const char *str, struct qfontface_s *font, size_t maxlen, int flags ) {
	return ftlib_export ? ftlib_export->StringWidth( str, font, maxlen, flags ) : 0;
}

/*
* FTLIB_StrlenForWidth
*/
size_t FTLIB_StrlenForWidth( const char *str, struct qfontface_s *font, size_t maxwidth, int flags ) {
	return ftlib_export ? ftlib_export->StrlenForWidth( str, font, maxwidth, flags ) : 0;
}

/*
* FTLIB_FontUnderline
*/
int FTLIB_FontUnderline( struct qfontface_s *font, int *thickness ) {
	if( ftlib_export ) {
		return ftlib_export->FontUnderline( font, thickness );
	}

	if( thickness ) {
		*thickness = 0;
	}
	return 0;
}

/*
* FTLIB_FontAdvance
*/
size_t FTLIB_FontAdvance( struct qfontface_s *font ) {
	return ftlib_export ? ftlib_export->FontAdvance( font ) : 0;
}

/*
* FTLIB_FontXHeight
*/
size_t FTLIB_FontXHeight( struct qfontface_s *font ) {
	return ftlib_export ? ftlib_export->FontXHeight( font ) : 0;
}

/*
* FTLIB_SetDrawCharIntercept
*/
fdrawchar_t FTLIB_SetDrawCharIntercept( fdrawchar_t intercept ) {
	return ftlib_export ? ftlib_export->SetDrawIntercept( intercept ) : 0;
}

/*
* FTLIB_DrawRawChar
*/
void FTLIB_DrawRawChar( int x, int y, wchar_t num, struct qfontface_s *font, vec4_t color ) {
	if( ftlib_export ) {
		ftlib_export->DrawRawChar( x, y, num, font, color );
	}
}

/*
* FTLIB_DrawClampChar
*/
void FTLIB_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color ) {
	if( ftlib_export ) {
		ftlib_export->DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, color );
	}
}

/*
* FTLIB_DrawClampString
*/
void FTLIB_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color, int flags ) {
	if( ftlib_export ) {
		ftlib_export->DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color, flags );
	}
}

/*
* FTLIB_DrawRawString
*/
size_t FTLIB_DrawRawString( int x, int y, const char *str, size_t maxwidth, int *width, struct qfontface_s *font, vec4_t color, int flags ) {
	return ftlib_export ? ftlib_export->DrawRawString( x, y, str, maxwidth, width, font, color, flags ) : 0;
}

/*
* FTLIB_DrawMultilineString
*/
int FTLIB_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, struct qfontface_s *font, vec4_t color, int flags ) {
	return ftlib_export ? ftlib_export->DrawMultilineString( x, y, str, halign, maxwidth, maxlines, font, color, flags ) : 0;
}
