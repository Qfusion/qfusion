/*
Copyright (C) 2002-2003 Victor Luchits

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

extern ftlib_import_t ftlibi_imp_local;

static inline void trap_Print( const char *msg ) {
	ftlibi_imp_local.Print( msg );
}

static inline void trap_Error( const char *msg ) {
	ftlibi_imp_local.Error( msg );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags ) {
	return ftlibi_imp_local.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value ) {
	return ftlibi_imp_local.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value ) {
	ftlibi_imp_local.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, const char *value ) {
	return ftlibi_imp_local.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name ) {
	return ftlibi_imp_local.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name ) {
	return ftlibi_imp_local.Cvar_String( name );
}

static inline int trap_Cmd_Argc( void ) {
	return ftlibi_imp_local.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return ftlibi_imp_local.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return ftlibi_imp_local.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( char *name, void ( *cmd )( void ) ) {
	ftlibi_imp_local.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name ) {
	ftlibi_imp_local.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, char *text ) {
	ftlibi_imp_local.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cmd_Execute( void ) {
	ftlibi_imp_local.Cmd_Execute();
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return ftlibi_imp_local.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return ftlibi_imp_local.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return ftlibi_imp_local.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg ) {
	return ftlibi_imp_local.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file ) {
	return ftlibi_imp_local.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return ftlibi_imp_local.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return ftlibi_imp_local.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return ftlibi_imp_local.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) {
	ftlibi_imp_local.FS_FCloseFile( file );
}

static inline bool trap_FS_RemoveFile( const char *filename ) {
	return ftlibi_imp_local.FS_RemoveFile( filename );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return ftlibi_imp_local.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

// clock
static inline int64_t trap_Milliseconds( void ) {
	return ftlibi_imp_local.Sys_Milliseconds();
}

static inline uint64_t trap_Microseconds( void ) {
	return ftlibi_imp_local.Sys_Microseconds();
}

// renderer
static inline struct shader_s *trap_R_RegisterPic( const char *name ) {
	return ftlibi_imp_local.R_RegisterPic( name );
}

static inline struct shader_s *trap_R_RegisterRawPic( const char *name, int width, int height, uint8_t *data, int samples ) {
	return ftlibi_imp_local.R_RegisterRawPic( name, width, height, data, samples );
}

static inline struct shader_s *trap_R_RegisterRawAlphaMask( const char *name, int width, int height, uint8_t *data ) {
	return ftlibi_imp_local.R_RegisterRawAlphaMask( name, width, height, data );
}

static inline void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader ) {
	ftlibi_imp_local.R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

static inline void trap_R_ReplaceRawSubPic( struct shader_s *shader, int x, int y, int width, int height, uint8_t *data ) {
	ftlibi_imp_local.R_ReplaceRawSubPic( shader, x, y, width, height, data );
}

static inline void trap_R_Scissor( int x, int y, int w, int h ) {
	ftlibi_imp_local.R_Scissor( x, y, w, h );
}

static inline void trap_R_GetScissor( int *x, int *y, int *w, int *h ) {
	ftlibi_imp_local.R_GetScissor( x, y, w, h );
}

static inline void trap_R_ResetScissor( void ) {
	ftlibi_imp_local.R_ResetScissor();
}

// memory
struct mempool_t;
extern mempool_t * ftlib_mempool;
static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return _Mem_AllocPool( ftlib_mempool, name, MEMPOOL_CINMODULE, filename, fileline );
}

static inline ATTRIBUTE_MALLOC void *trap_MemAlloc( struct mempool_s *pool, size_t size, const char *filename, int fileline ) {
	return _Mem_Alloc( pool, size, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static inline void *trap_MemRealloc( void *data, size_t size, const char *filename, int fileline ) {
	return _Mem_Realloc( data, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	_Mem_Free( data, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	_Mem_FreePool( pool, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	_Mem_EmptyPool( pool, MEMPOOL_CINMODULE, 0, filename, fileline );
}

static inline void *trap_LoadLibrary( char *name, dllfunc_t *funcs ) {
	return ftlibi_imp_local.Sys_LoadLibrary( name, funcs );
}

static inline void trap_UnloadLibrary( void **lib ) {
	ftlibi_imp_local.Sys_UnloadLibrary( lib );
}
