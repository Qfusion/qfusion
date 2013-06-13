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

#ifdef SOUND_HARD_LINKED
#define SOUND_IMPORT sndi_imp_local
#endif

extern sound_import_t SOUND_IMPORT;

static inline void trap_Print( const char *msg )
{
	SOUND_IMPORT.Print( msg );
}

static inline void trap_Error( const char *msg )
{
	SOUND_IMPORT.Error( msg );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags )
{
	return SOUND_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value )
{
	return SOUND_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value )
{
	SOUND_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, char *value )
{
	return SOUND_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name )
{
	return SOUND_IMPORT.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name )
{
	return SOUND_IMPORT.Cvar_String( name );
}

static inline int trap_Cmd_Argc( void )
{
	return SOUND_IMPORT.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg )
{
	return SOUND_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void )
{
	return SOUND_IMPORT.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( char *name, void ( *cmd )(void) )
{
	SOUND_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name )
{
	SOUND_IMPORT.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, char *text )
{
	SOUND_IMPORT.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cmd_Execute( void )
{
	SOUND_IMPORT.Cmd_Execute();
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode )
{
	return SOUND_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file )
{
	return SOUND_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file )
{
	return SOUND_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg )
{
	return SOUND_IMPORT.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file )
{
	return SOUND_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence )
{
	return SOUND_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file )
{
	return SOUND_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file )
{
	return SOUND_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file )
{
	SOUND_IMPORT.FS_FCloseFile( file );
}

static inline qboolean trap_FS_RemoveFile( const char *filename )
{
	return SOUND_IMPORT.FS_RemoveFile( filename );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end )
{
	return SOUND_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline qboolean trap_FS_IsUrl( const char *url )
{
	return SOUND_IMPORT.FS_IsUrl( url );
}

// misc
static inline unsigned int trap_Milliseconds( void )
{
	return SOUND_IMPORT.Milliseconds();
}

static inline void trap_PageInMemory( qbyte *buffer, int size )
{
	SOUND_IMPORT.PageInMemory( buffer, size );
}

static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline )
{
	return SOUND_IMPORT.Mem_AllocPool( name, filename, fileline );
}

static inline void *trap_MemAlloc( struct mempool_s *pool, size_t size, const char *filename, int fileline )
{
	return SOUND_IMPORT.Mem_Alloc( pool, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline )
{
	SOUND_IMPORT.Mem_Free( data, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline )
{
	SOUND_IMPORT.Mem_FreePool( pool, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline )
{
	SOUND_IMPORT.Mem_EmptyPool( pool, filename, fileline );
}

static inline void trap_GetEntitySpatilization( int entnum, vec3_t origin, vec3_t velocity )
{
	SOUND_IMPORT.GetEntitySpatilization( entnum, origin, velocity );
}

static inline void *trap_LoadLibrary( char *name, dllfunc_t *funcs )
{
	return SOUND_IMPORT.LoadLibrary( name, funcs );
}

static inline void trap_UnloadLibrary( void **lib )
{
	SOUND_IMPORT.UnloadLibrary( lib );
}
