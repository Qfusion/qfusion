/*
Copyright (C) 2008 German Garcia

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

#ifndef __QAS_SYSCALLS_H__
#define __QAS_SYSCALLS_H__

#ifdef ANGELWRAP_HARD_LINKED
#define ANGELWRAP_IMPORT qasi_imp_local
#endif

extern angelwrap_import_t ANGELWRAP_IMPORT;

static inline void trap_Print( const char *msg ) {
	ANGELWRAP_IMPORT.Print( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif

static inline void trap_Error( const char *msg ) {
	ANGELWRAP_IMPORT.Error( msg );
}

static inline int64_t trap_Milliseconds( void ) {
	return ANGELWRAP_IMPORT.Milliseconds();
}

static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags ) {
	return ANGELWRAP_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value ) {
	return ANGELWRAP_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value ) {
	ANGELWRAP_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, char *value ) {
	return ANGELWRAP_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name ) {
	return ANGELWRAP_IMPORT.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name ) {
	return ANGELWRAP_IMPORT.Cvar_String( name );
}

static inline int trap_Cmd_Argc( void ) {
	return ANGELWRAP_IMPORT.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return ANGELWRAP_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return ANGELWRAP_IMPORT.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( char *name, void ( *cmd )( void ) ) {
	ANGELWRAP_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name ) {
	ANGELWRAP_IMPORT.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, char *text ) {
	ANGELWRAP_IMPORT.Cmd_ExecuteText( exec_when, text );
}
static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return ANGELWRAP_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return ANGELWRAP_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return ANGELWRAP_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg ) {
	return ANGELWRAP_IMPORT.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file ) {
	return ANGELWRAP_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return ANGELWRAP_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return ANGELWRAP_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return ANGELWRAP_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) {
	ANGELWRAP_IMPORT.FS_FCloseFile( file );
}

static inline time_t trap_FS_SysMTime( int file ) {
	return ANGELWRAP_IMPORT.FS_SysMTime( file );
}

static inline int trap_FS_FGetFullPathName( int file, char *buffer, int buffer_size ) {
	return ANGELWRAP_IMPORT.FS_FGetFullPathName( file, buffer, buffer_size );
}

static inline int trap_FS_GetFullPathName( char *pathname, char *buffer, int buffer_size ) {
	return ANGELWRAP_IMPORT.FS_GetFullPathName( pathname, buffer, buffer_size );
}

static inline bool trap_FS_RemoveFile( const char *filename )
{
	return ANGELWRAP_IMPORT.FS_RemoveFile( filename ) == true;
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return ANGELWRAP_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline const char *trap_FS_FirstExtension( const char *filename, const char *extensions[], int num_extensions ) {
	return ANGELWRAP_IMPORT.FS_FirstExtension( filename, extensions, num_extensions );
}

static inline bool trap_FS_MoveFile( const char *src, const char *dst ) {
	return ANGELWRAP_IMPORT.FS_MoveFile( src, dst ) == true;
}

static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return ANGELWRAP_IMPORT.Mem_AllocPool( name, filename, fileline );
}

static inline ATTRIBUTE_MALLOC void *trap_MemAlloc( struct mempool_s *pool, size_t size, const char *filename, int fileline ) {
	return ANGELWRAP_IMPORT.Mem_Alloc( pool, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	ANGELWRAP_IMPORT.Mem_Free( data, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	ANGELWRAP_IMPORT.Mem_FreePool( pool, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	ANGELWRAP_IMPORT.Mem_EmptyPool( pool, filename, fileline );
}

#endif // __QAS_SYSCALLS_H__
