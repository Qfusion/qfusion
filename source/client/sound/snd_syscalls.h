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

extern sound_import_t sndi_imp_local;

typedef struct qthread_s qthread_t;
typedef struct qmutex_s qmutex_t;
typedef struct qbufPipe_s qbufPipe_t;

static inline void trap_Print( const char *msg ) {
	sndi_imp_local.Print( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif


static inline void trap_Error( const char *msg ) {
	sndi_imp_local.Error( msg );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags ) {
	return sndi_imp_local.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value ) {
	return sndi_imp_local.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value ) {
	sndi_imp_local.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, const char *value ) {
	return sndi_imp_local.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name ) {
	return sndi_imp_local.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name ) {
	return sndi_imp_local.Cvar_String( name );
}

static inline int trap_Cmd_Argc( void ) {
	return sndi_imp_local.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return sndi_imp_local.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return sndi_imp_local.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( char *name, void ( *cmd )( void ) ) {
	sndi_imp_local.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name ) {
	sndi_imp_local.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, char *text ) {
	sndi_imp_local.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cmd_Execute( void ) {
	sndi_imp_local.Cmd_Execute();
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return sndi_imp_local.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return sndi_imp_local.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return sndi_imp_local.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg ) {
	return sndi_imp_local.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file ) {
	return sndi_imp_local.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return sndi_imp_local.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return sndi_imp_local.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return sndi_imp_local.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) {
	sndi_imp_local.FS_FCloseFile( file );
}

static inline bool trap_FS_RemoveFile( const char *filename ) {
	return sndi_imp_local.FS_RemoveFile( filename );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return sndi_imp_local.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline bool trap_FS_IsUrl( const char *url ) {
	return sndi_imp_local.FS_IsUrl( url );
}

// misc
static inline int64_t trap_Milliseconds( void ) {
	return sndi_imp_local.Sys_Milliseconds();
}

static inline void trap_Sleep( unsigned int milliseconds ) {
	sndi_imp_local.Sys_Sleep( milliseconds );
}

static inline struct mempool_s *trap_MemAllocPool( const char *name, const char *filename, int fileline ) {
	return sndi_imp_local.Mem_AllocPool( name, filename, fileline );
}

static inline ATTRIBUTE_MALLOC void *trap_MemAlloc( struct mempool_s *pool, size_t size, const char *filename, int fileline ) {
	return sndi_imp_local.Mem_Alloc( pool, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	sndi_imp_local.Mem_Free( data, filename, fileline );
}

static inline void trap_MemFreePool( struct mempool_s **pool, const char *filename, int fileline ) {
	sndi_imp_local.Mem_FreePool( pool, filename, fileline );
}

static inline void trap_MemEmptyPool( struct mempool_s *pool, const char *filename, int fileline ) {
	sndi_imp_local.Mem_EmptyPool( pool, filename, fileline );
}

static inline void trap_GetEntitySpatilization( int entnum, vec3_t origin, vec3_t velocity ) {
	sndi_imp_local.GetEntitySpatilization( entnum, origin, velocity );
}

static inline void *trap_LoadLibrary( char *name, dllfunc_t *funcs ) {
	return sndi_imp_local.Sys_LoadLibrary( name, funcs );
}

static inline void trap_UnloadLibrary( void **lib ) {
	sndi_imp_local.Sys_UnloadLibrary( lib );
}

static inline struct qthread_s *trap_Thread_Create( void *( *routine )( void* ), void *param ) {
	return sndi_imp_local.Thread_Create( routine, param );
}

static inline void trap_Thread_Join( struct qthread_s *thread ) {
	sndi_imp_local.Thread_Join( thread );
}

static inline void trap_Thread_Yield( void ) {
	sndi_imp_local.Thread_Yield();
}

static inline struct qmutex_s *trap_Mutex_Create( void ) {
	return sndi_imp_local.Mutex_Create();
}

static inline void trap_Mutex_Destroy( struct qmutex_s **mutex ) {
	sndi_imp_local.Mutex_Destroy( mutex );
}

static inline void trap_Mutex_Lock( struct qmutex_s *mutex ) {
	sndi_imp_local.Mutex_Lock( mutex );
}

static inline void trap_Mutex_Unlock( struct qmutex_s *mutex ) {
	sndi_imp_local.Mutex_Unlock( mutex );
}

static inline qbufPipe_t *trap_BufPipe_Create( size_t bufSize, int flags ) {
	return sndi_imp_local.BufPipe_Create( bufSize, flags );
}

static inline void trap_BufPipe_Destroy( qbufPipe_t **pqueue ) {
	sndi_imp_local.BufPipe_Destroy( pqueue );
}

static inline void trap_BufPipe_Finish( qbufPipe_t *queue ) {
	sndi_imp_local.BufPipe_Finish( queue );
}

static inline void trap_BufPipe_WriteCmd( qbufPipe_t *queue, const void *cmd, unsigned cmd_size ) {
	sndi_imp_local.BufPipe_WriteCmd( queue, cmd, cmd_size );
}

static inline int trap_BufPipe_ReadCmds( qbufPipe_t *queue, unsigned( **cmdHandlers )( const void * ) ) {
	return sndi_imp_local.BufPipe_ReadCmds( queue, cmdHandlers );
}

static inline void trap_BufPipe_Wait( qbufPipe_t *queue, int ( *read )( qbufPipe_t *, unsigned( ** )( const void * ), bool ),
									  unsigned( **cmdHandlers )( const void * ), unsigned timeout_msec ) {
	sndi_imp_local.BufPipe_Wait( queue, read, cmdHandlers, timeout_msec );
}
