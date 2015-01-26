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

#ifdef TV_MODULE_HARD_LINKED
#define TV_MODULE_IMPORT gi_imp_local
#endif

extern tv_module_import_t TV_MODULE_IMPORT;

static inline void trap_Print( const char *msg )
{
	TV_MODULE_IMPORT.Print( msg );
}

static inline void trap_Error( const char *msg )
{
	TV_MODULE_IMPORT.Error( msg );
}

static inline void trap_RelayError( tvm_relay_t *relay, const char *msg )
{
	TV_MODULE_IMPORT.RelayError( relay->server, msg );
}

static inline void trap_GameCmd( tvm_relay_t *relay, int numClient, const char *cmd )
{
	TV_MODULE_IMPORT.GameCmd( relay->server, numClient, cmd );
}

static inline void trap_ConfigString( tvm_relay_t *relay, int num, const char *string )
{
	TV_MODULE_IMPORT.ConfigString( relay->server, num, string );
}

static inline unsigned int trap_Milliseconds( void )
{
	return TV_MODULE_IMPORT.Milliseconds();
}

static inline int trap_CM_TransformedPointContents( tvm_relay_t *relay, vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles )
{
	return TV_MODULE_IMPORT.CM_TransformedPointContents( relay->server, p, cmodel, origin, angles );
}
static inline void trap_CM_TransformedBoxTrace( tvm_relay_t *relay, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles )
{
	TV_MODULE_IMPORT.CM_TransformedBoxTrace( relay->server, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}
static inline void trap_CM_RoundUpToHullSize( tvm_relay_t *relay, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel )
{
	TV_MODULE_IMPORT.CM_RoundUpToHullSize( relay->server, mins, maxs, cmodel );
}
static inline struct cmodel_s *trap_CM_InlineModel( tvm_relay_t *relay, int num )
{
	return TV_MODULE_IMPORT.CM_InlineModel( relay->server, num );
}
static inline void trap_CM_InlineModelBounds( tvm_relay_t *relay, struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs )
{
	TV_MODULE_IMPORT.CM_InlineModelBounds( relay->server, cmodel, mins, maxs );
}
static inline struct cmodel_s *trap_CM_ModelForBBox( tvm_relay_t *relay, vec3_t mins, vec3_t maxs )
{
	return TV_MODULE_IMPORT.CM_ModelForBBox( relay->server, mins, maxs );
}
static inline bool trap_CM_AreasConnected( tvm_relay_t *relay, int area1, int area2 )
{
	return TV_MODULE_IMPORT.CM_AreasConnected( relay->server, area1, area2 );
}
static inline int trap_CM_BoxLeafnums( tvm_relay_t *relay, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode )
{
	return TV_MODULE_IMPORT.CM_BoxLeafnums( relay->server, mins, maxs, list, listsize, topnode );
}
static inline int trap_CM_LeafCluster( tvm_relay_t *relay, int leafnum )
{
	return TV_MODULE_IMPORT.CM_LeafCluster( relay->server, leafnum );
}
static inline int trap_CM_LeafArea( tvm_relay_t *relay, int leafnum )
{
	return TV_MODULE_IMPORT.CM_LeafArea( relay->server, leafnum );
}

static inline void *trap_MemAlloc( void *relay_server, size_t size, const char *filename, int fileline )
{
	return TV_MODULE_IMPORT.Mem_Alloc( relay_server, size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline )
{
	TV_MODULE_IMPORT.Mem_Free( data, filename, fileline );
}

// dynvars
static inline dynvar_t *trap_Dynvar_Create( const char *name, bool console, dynvar_getter_f getter, dynvar_setter_f setter )
{
	return TV_MODULE_IMPORT.Dynvar_Create( name, console, getter, setter );
}

static inline void trap_Dynvar_Destroy( dynvar_t *dynvar )
{
	TV_MODULE_IMPORT.Dynvar_Destroy( dynvar );
}

static inline dynvar_t *trap_Dynvar_Lookup( const char *name )
{
	return TV_MODULE_IMPORT.Dynvar_Lookup( name );
}

static inline const char *trap_Dynvar_GetName( dynvar_t *dynvar )
{
	return TV_MODULE_IMPORT.Dynvar_GetName( dynvar );
}

static inline dynvar_get_status_t trap_Dynvar_GetValue( dynvar_t *dynvar, void **value )
{
	return TV_MODULE_IMPORT.Dynvar_GetValue( dynvar, value );
}

static inline dynvar_set_status_t trap_Dynvar_SetValue( dynvar_t *dynvar, void *value )
{
	return TV_MODULE_IMPORT.Dynvar_SetValue( dynvar, value );
}

static inline void trap_Dynvar_AddListener( dynvar_t *dynvar, dynvar_listener_f listener )
{
	TV_MODULE_IMPORT.Dynvar_AddListener( dynvar, listener );
}

static inline void trap_Dynvar_RemoveListener( dynvar_t *dynvar, dynvar_listener_f listener )
{
	TV_MODULE_IMPORT.Dynvar_RemoveListener( dynvar, listener );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags )
{
	return TV_MODULE_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value )
{
	return TV_MODULE_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value )
{
	TV_MODULE_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, const char *value )
{
	return TV_MODULE_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name )
{
	return TV_MODULE_IMPORT.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name )
{
	return TV_MODULE_IMPORT.Cvar_String( name );
}

// cmds
static inline void trap_Cmd_TokenizeString( const char *text )
{
	TV_MODULE_IMPORT.Cmd_TokenizeString( text );
}

static inline int trap_Cmd_Argc( void )
{
	return TV_MODULE_IMPORT.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg )
{
	return TV_MODULE_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void )
{
	return TV_MODULE_IMPORT.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( char *name, void ( *cmd )(void) )
{
	TV_MODULE_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( char *cmd_name )
{
	TV_MODULE_IMPORT.Cmd_RemoveCommand( cmd_name );
}

// fs
static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode )
{
	return TV_MODULE_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file )
{
	return TV_MODULE_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file )
{
	return TV_MODULE_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg )
{
	return TV_MODULE_IMPORT.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file )
{
	return TV_MODULE_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence )
{
	return TV_MODULE_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file )
{
	return TV_MODULE_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file )
{
	return TV_MODULE_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file )
{
	TV_MODULE_IMPORT.FS_FCloseFile( file );
}

static inline bool trap_FS_RemoveFile( const char *filename )
{
	return TV_MODULE_IMPORT.FS_RemoveFile( filename );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end )
{
	return TV_MODULE_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline const char *trap_FS_FirstExtension( const char *filename, const char *extensions[], int num_extensions )
{
	return TV_MODULE_IMPORT.FS_FirstExtension( filename, extensions, num_extensions );
}

// misc
static inline void trap_AddCommandString( char *text )
{
	TV_MODULE_IMPORT.AddCommandString( text );
}

static inline int trap_GetClientState( tvm_relay_t *relay, int numClient )
{
	return TV_MODULE_IMPORT.GetClientState( relay->server, numClient );
}

static inline void trap_ExecuteClientThinks( tvm_relay_t *relay, int clientNum )
{
	TV_MODULE_IMPORT.ExecuteClientThinks( relay->server, clientNum );
}

static inline void trap_DropClient( tvm_relay_t *relay, int numClient, int type, char *message )
{
	TV_MODULE_IMPORT.DropClient( relay->server, numClient, type, message );
}

static inline void trap_LocateEntities( tvm_relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts )
{
	TV_MODULE_IMPORT.LocateEntities( relay->server, edicts, edict_size, num_edicts, max_edicts );
}

static inline void trap_LocateLocalEntities( tvm_relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts )
{
	TV_MODULE_IMPORT.LocateLocalEntities( relay->server, edicts, edict_size, num_edicts, max_edicts );
}
