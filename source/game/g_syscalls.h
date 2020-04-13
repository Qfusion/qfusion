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

#ifdef GAME_HARD_LINKED
#define GAME_IMPORT gi_imp_local
#endif

extern game_import_t GAME_IMPORT;

static inline void trap_Print( const char *msg ) {
	GAME_IMPORT.Print( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif

static inline void trap_Error( const char *msg ) {
	GAME_IMPORT.Error( msg );
}

static inline void trap_GameCmd( struct edict_s *ent, const char *cmd ) {
	GAME_IMPORT.GameCmd( ent, cmd );
}

static inline void trap_ConfigString( int num, const char *string ) {
	GAME_IMPORT.ConfigString( num, string );
}

static inline const char *trap_GetConfigString( int num ) {
	return GAME_IMPORT.GetConfigString( num );
}

static inline void trap_PureSound( const char *name ) {
	GAME_IMPORT.PureSound( name );
}

static inline void trap_PureModel( const char *name ) {
	GAME_IMPORT.PureModel( name );
}

static inline int trap_ModelIndex( const char *name ) {
	return GAME_IMPORT.ModelIndex( name );
}

static inline int trap_SoundIndex( const char *name ) {
	return GAME_IMPORT.SoundIndex( name );
}

static inline int trap_ImageIndex( const char *name ) {
	return GAME_IMPORT.ImageIndex( name );
}

static inline int trap_SkinIndex( const char *name ) {
	return GAME_IMPORT.SkinIndex( name );
}

static inline int64_t trap_Milliseconds( void ) {
	return GAME_IMPORT.Milliseconds();
}

static inline bool trap_inPVS( const vec3_t p1, const vec3_t p2 ) {
	return GAME_IMPORT.inPVS( p1, p2 ) == true;
}

static inline int trap_CM_TransformedPointContents( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles ) {
	return GAME_IMPORT.CM_TransformedPointContents( p, cmodel, origin, angles );
}

static inline void trap_CM_TransformedBoxTrace( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles ) {
	GAME_IMPORT.CM_TransformedBoxTrace( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline void trap_CM_RoundUpToHullSize( vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel ) {
	GAME_IMPORT.CM_RoundUpToHullSize( mins, maxs, cmodel );
}

static inline int trap_CM_NumInlineModels( void ) {
	return GAME_IMPORT.CM_NumInlineModels();
}

static inline struct cmodel_s *trap_CM_InlineModel( int num ) {
	return GAME_IMPORT.CM_InlineModel( num );
}

static inline void trap_CM_InlineModelBounds( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	GAME_IMPORT.CM_InlineModelBounds( cmodel, mins, maxs );
}

static inline struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return GAME_IMPORT.CM_ModelForBBox( mins, maxs );
}

static inline struct cmodel_s *trap_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return GAME_IMPORT.CM_OctagonModelForBBox( mins, maxs );
}

static inline void trap_CM_SetAreaPortalState( int area, int otherarea, bool open ) {
	GAME_IMPORT.CM_SetAreaPortalState( area, otherarea, open == true ? true : false );
}

static inline bool trap_CM_AreasConnected( int area1, int area2 ) {
	return GAME_IMPORT.CM_AreasConnected( area1, area2 ) == true;
}

static inline int trap_CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode ) {
	return GAME_IMPORT.CM_BoxLeafnums( mins, maxs, list, listsize, topnode );
}

static inline int trap_CM_LeafCluster( int leafnum ) {
	return GAME_IMPORT.CM_LeafCluster( leafnum );
}

static inline int trap_CM_LeafArea( int leafnum ) {
	return GAME_IMPORT.CM_LeafArea( leafnum );
}

static inline ATTRIBUTE_MALLOC void *trap_MemAlloc( size_t size, const char *filename, int fileline ) {
	return GAME_IMPORT.Mem_Alloc( size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	GAME_IMPORT.Mem_Free( data, filename, fileline );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags ) {
	return GAME_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value ) {
	return GAME_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value ) {
	GAME_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, const char *value ) {
	return GAME_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name ) {
	return GAME_IMPORT.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name ) {
	return GAME_IMPORT.Cvar_String( name );
}

static inline int trap_Cmd_Argc( void ) {
	return GAME_IMPORT.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return GAME_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return GAME_IMPORT.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( const char *name, void ( *cmd )( void ) ) {
	GAME_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( const char *cmd_name ) {
	GAME_IMPORT.Cmd_RemoveCommand( cmd_name );
}

// fs
static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return GAME_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return GAME_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return GAME_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg ) {
	return GAME_IMPORT.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file ) {
	return GAME_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return GAME_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return GAME_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return GAME_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) {
	GAME_IMPORT.FS_FCloseFile( file );
}

static inline bool trap_FS_RemoveFile( const char *filename ) {
	return GAME_IMPORT.FS_RemoveFile( filename ) == true;
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return GAME_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline const char *trap_FS_FirstExtension( const char *filename, const char *extensions[], int num_extensions ) {
	return GAME_IMPORT.FS_FirstExtension( filename, extensions, num_extensions );
}

static inline bool trap_FS_MoveFile( const char *src, const char *dst ) {
	return GAME_IMPORT.FS_MoveFile( src, dst ) == true;
}

static inline bool trap_ML_Update( void ) {
	return GAME_IMPORT.ML_Update() == true;
}

static inline bool trap_ML_FilenameExists( const char *filename ) {
	return GAME_IMPORT.ML_FilenameExists( filename ) == true;
}

static inline const char *trap_ML_GetFullname( const char *filename ) {
	return GAME_IMPORT.ML_GetFullname( filename );
}

static inline size_t trap_ML_GetMapByNum( int num, char *out, size_t size ) {
	return GAME_IMPORT.ML_GetMapByNum( num, out, size );
}

static inline void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	GAME_IMPORT.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cbuf_Execute( void ) {
	GAME_IMPORT.Cbuf_Execute();
}

static inline int trap_FakeClientConnect( char *fakeUserinfo, char *fakeSocketType, const char *fakeIP ) {
	return GAME_IMPORT.FakeClientConnect( fakeUserinfo, fakeSocketType, fakeIP );
}

static inline int trap_GetClientState( int numClient ) {
	return GAME_IMPORT.GetClientState( numClient );
}

static inline void trap_ExecuteClientThinks( int clientNum ) {
	GAME_IMPORT.ExecuteClientThinks( clientNum );
}

static inline void trap_DropClient( edict_t *ent, int type, const char *message ) {
	GAME_IMPORT.DropClient( ent, type, message );
}

static inline void trap_LocateEntities( struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts ) {
	GAME_IMPORT.LocateEntities( edicts, edict_size, num_edicts, max_edicts );
}

static inline struct angelwrap_api_s *trap_asGetAngelExport( void ) {
	return GAME_IMPORT.asGetAngelExport();
}

// Matchmaking
static inline struct stat_query_api_s *trap_GetStatQueryAPI( void ) {
	return GAME_IMPORT.GetStatQueryAPI();
}

static inline void trap_MM_SendQuery( struct stat_query_s *query ) {
	GAME_IMPORT.MM_SendQuery( query );
}

static inline void trap_MM_GameState( bool state ) {
	GAME_IMPORT.MM_GameState( state == true ? true : false );
}
