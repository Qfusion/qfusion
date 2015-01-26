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

#include "tv_local.h"

#include "tv_relay_module.h"

#include "tv_upstream.h"
#include "tv_relay.h"
#include "tv_downstream.h"

typedef struct tv_module_s tv_module_t;

struct tv_module_s
{
	int count;
	void *handle;
	tv_module_import_t import;
	tv_module_export_t *export;
};

static tv_module_t *modules = NULL;

EXTERN_API_FUNC void *GetTVModuleAPI( void * );

//======================================================================
// TV_Module versions of the CM functions passed to the game module
// they only add relay->cms as the first parameter
//======================================================================

static inline int TV_Module_CM_TransformedPointContents( relay_t *relay, vec3_t p, struct cmodel_s *cmodel,
														vec3_t origin, vec3_t angles )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_TransformedPointContents: Relay not set\n" );
		return 0;
	}

	return CM_TransformedPointContents( relay->cms, p, cmodel, origin, angles );
}

static inline void TV_Module_CM_TransformedBoxTrace( relay_t *relay, trace_t *tr, vec3_t start, vec3_t end,
													vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_TransformedBoxTrace: Relay not set\n" );
		return;
	}

	CM_TransformedBoxTrace( relay->cms, tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline void TV_Module_CM_RoundUpToHullSize( relay_t *relay, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_RoundUpToHullSize: Relay not set\n" );
		return;
	}

	CM_RoundUpToHullSize( relay->cms, mins, maxs, cmodel );
}

static inline struct cmodel_s *TV_Module_CM_InlineModel( relay_t *relay, int num )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_InlineModel: Relay not set\n" );
		return NULL;
	}

	return CM_InlineModel( relay->cms, num );
}

static inline void TV_Module_CM_InlineModelBounds( relay_t *relay, struct cmodel_s *cmodel, vec3_t mins,
												  vec3_t maxs )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_InlineModelBounds: Relay not set\n" );
		return;
	}

	CM_InlineModelBounds( relay->cms, cmodel, mins, maxs );
}

static inline struct cmodel_s *TV_Module_CM_ModelForBBox( relay_t *relay, vec3_t mins, vec3_t maxs )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_ModelForBbox: Relay not set\n" );
		return NULL;
	}

	return CM_ModelForBBox( relay->cms, mins, maxs );
}

static inline struct cmodel_s *TV_Module_CM_OctagonModelForBBox( relay_t *relay, vec3_t mins, vec3_t maxs )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_ModelForBbox: Relay not set\n" );
		return NULL;
	}

	return CM_OctagonModelForBBox( relay->cms, mins, maxs );
}

static inline bool TV_Module_CM_AreasConnected( relay_t *relay, int area1, int area2 )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_AreasConnected: Relay not set\n" );
		return false;
	}

	return CM_AreasConnected( relay->cms, area1, area2 );
}

static inline int TV_Module_CM_BoxLeafnums( relay_t *relay, vec3_t mins, vec3_t maxs, int *list, int listsize,
										   int *topnode )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_BoxLeafnums: Relay not set\n" );
		return 0;
	}

	return CM_BoxLeafnums( relay->cms, mins, maxs, list, listsize, topnode );
}

static inline int TV_Module_CM_LeafCluster( relay_t *relay, int leafnum )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_LeafCluster: Relay not set\n" );
		return 0;
	}

	return CM_LeafCluster( relay->cms, leafnum );
}

static inline int TV_Module_CM_LeafArea( relay_t *relay, int leafnum )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_CM_LeafArea: Relay not set\n" );
		return 0;
	}

	return CM_LeafArea( relay->cms, leafnum );
}

//======================================================================


/*
* TV_Module_DropClient
*/
static void TV_Module_DropClient( relay_t *relay, int numClient, int type, const char *message )
{
	client_t *client;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_DropClient: Relay not set\n" );
		return;
	}

	if( numClient < 0 || numClient >= tv_maxclients->integer )
		TV_Relay_Error( relay, "TV_Module_DropClient: Invalid numClient" );

	client = &tvs.clients[numClient];

	if( client->state == CS_FREE || client->state == CS_ZOMBIE || client->relay != relay )
		TV_Relay_Error( relay, "TV_Module_DropClient: Invalid client" );

	if( message )
		TV_Downstream_DropClient( client, type, "%s", message );
	else
		TV_Downstream_DropClient( client, type, "" );
}

/*
* TV_Module_GetClientState
* Game code asks for the state of this client
*/
static int TV_Module_GetClientState( relay_t *relay, int numClient )
{
	client_t *client;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_GetClientState: Relay not set\n" );
		return -1;
	}

	if( numClient < 0 || numClient >= tv_maxclients->integer )
		TV_Relay_Error( relay, "TV_Module_GetClientState: Invalid numClient" );

	client = &tvs.clients[numClient];

	if( client->relay != relay )
		TV_Relay_Error( relay, "TV_Module_GetClientState: Invalid client" );

	return client->state;
}

/*
* TV_Module_GameCmd
* 
* Sends the server command to clients.
* if numClient is -1 the command will be sent to all connected clients
*/
static void TV_Module_GameCmd( relay_t *relay, int numClient, const char *cmd )
{
	int i;
	client_t *client;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_GameCmd: Relay not set\n" );
		return;
	}

	if( !cmd || !cmd[0] )
		TV_Relay_Error( relay, "TV_Module_GameCmd: Missing command" );

	if( numClient < -1 || numClient >= tv_maxclients->integer )
		TV_Relay_Error( relay, "TV_Module_GameCmd: Invalid numClient" );

	if( numClient == -1 )
	{
		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state < CS_SPAWNED )
				continue;
			if( client->relay != relay )
				continue;

			TV_Downstream_AddGameCommand( relay, client, cmd );
		}
	}
	else
	{
		client = &tvs.clients[numClient];

		if( client->state == CS_FREE || client->state == CS_ZOMBIE || client->relay != relay )
			TV_Relay_Error( relay, "TV_Module_GameCmd: Invalid client" );

		TV_Downstream_AddGameCommand( relay, client, cmd );
	}
}

/*
* TV_Module_Print
* 
* Debug print to server console
*/
static void TV_Module_Print( const char *msg )
{
	//if( !msg )
	//	TV_Relay_Error( relay, "TV_Module_Print: message missing" );

	Com_Printf( "%s", msg );
}

/*
* TV_Module_Error
*/
static void TV_Module_Error( const char *msg )
{
	if( msg )
		Com_Error( ERR_FATAL, "Game error: %s", msg );
	else
		Com_Error( ERR_FATAL, "Game error" );
}

/*
* TV_Module_RelayError
*/
static void TV_Module_RelayError( relay_t *relay, const char *msg )
{
	if( !relay )
		TV_Module_Error( va( "RelayError without relay: %s\n", msg ) );

	if( msg )
		TV_Relay_Error( relay, "Game error: %s", msg );
	else
		TV_Relay_Error( relay, "Game error" );
}

/*
* TV_Module_MemAlloc
*/
static void *TV_Module_MemAlloc( relay_t *relay, size_t size, const char *filename, int fileline )
{
	return _Mem_Alloc( relay->module_mempool, size, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}

/*
* TV_Module_MemFree
*/
static void TV_Module_MemFree( void *data, const char *filename, int fileline )
{
	_Mem_Free( data, MEMPOOL_GAMEPROGS, 0, filename, fileline );
}

/*
* TV_Module_LocateEntities
*/
static void TV_Module_LocateEntities( relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts,
									 int max_edicts )
{
	if( !relay )
	{
		Com_Printf( "Error: TV_Module_LocateEntities: Relay not set\n" );
		return;
	}

	if( !edicts || edict_size < sizeof( entity_shared_t ) )
		TV_Relay_Error( relay, "TV_Module_LocateEntities: bad edicts" );

	relay->gi.edicts = edicts;
	relay->gi.edict_size = edict_size;
	relay->gi.num_edicts = num_edicts;
	relay->gi.max_edicts = max_edicts;
	relay->gi.max_clients = min( num_edicts, MAX_CLIENTS );
}

/*
* TV_Module_LocateLocalEntities
*/
static void TV_Module_LocateLocalEntities( relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts,
										  int max_edicts )
{
	int i;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_LocateLocalEntities: Relay not set\n" );
		return;
	}

	if( !edicts || edict_size < sizeof( entity_shared_t ) )
		TV_Relay_Error( relay, "TV_Module_LocateLocalEntities: bad edicts" );

	relay->gi.local_edicts = edicts;
	relay->gi.local_edict_size = edict_size;
	relay->gi.local_num_edicts = num_edicts;
	relay->gi.local_max_edicts = max_edicts;

	for( i = 0; i < tv_maxclients->integer; i++ )
	{
		if( tvs.clients[i].relay == relay )
		{
			tvs.clients[i].edict = LOCAL_EDICT_NUM( relay, i );
		}
	}
}

/*
* TV_Module_ExecuteClientThinks
*/
static void TV_Module_ExecuteClientThinks( relay_t *relay, int clientNum )
{
	client_t *client;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_ExecuteClientThinks: Relay not set\n" );
		return;
	}

	if( clientNum < 0 || clientNum >= tv_maxclients->integer )
		TV_Relay_Error( relay, "TV_Module_ExecuteClientThinks: Invalid clientNum" );

	client = tvs.clients + clientNum;

	if( client->state < CS_SPAWNED || client->relay != relay )
		TV_Relay_Error( relay, "TV_Module_ExecuteClientThinks: Invalid clientNum" );

	TV_Downstream_ExecuteClientThinks( relay, client );
}

/*
* TV_Module_ConfigString
*/
static void TV_Module_ConfigString( relay_t *relay, int index, const char *val )
{
	size_t len;

	if( !relay )
	{
		Com_Printf( "Error: TV_Module_ConfigString: Relay not set\n" );
		return;
	}

	if( !val )
		TV_Relay_Error( relay, "TV_Module_ConfigString: No value" );

	if( index < 0 || index >= MAX_CONFIGSTRINGS )
		TV_Relay_Error( relay, "TV_Module_ConfigString: Bad index" );

	len = strlen( val );
	if( len >= sizeof( relay->configstrings[0] ) )
	{
		Com_Printf( "WARNING: 'TV_Module_ConfigString', configstring %i overflowed (%i)\n", index, len );
		len = sizeof( relay->configstrings[0] ) - 1;
	}

	if( !COM_ValidateConfigstring( val ) )
	{
		Com_Printf( "WARNING: 'TV_Module_ConfigString' invalid configstring %i: %s\n", index, val );
		return;
	}

	// ignore if no changes
	if( !strncmp( relay->configstrings[index], val, len ) && relay->configstrings[index][len] == '\0' )
		return;

	// change the string in sv
	Q_strncpyz( relay->configstrings[index], val, sizeof( relay->configstrings[index] ) );

	{
		// We have to manually broadcast this one.
		client_t *client;
		int i;
		for( i = 0, client = tvs.clients; i < tv_maxclients->integer; i++, client++ )
		{
			if( client->state < CS_CONNECTED )
				continue;
			if( client->relay != relay )
				continue;
			TV_Downstream_SendServerCommand( client, "cs %i \"%s\"", index, val );
		}
	}
}


//==============================================

/*
* TV_ReleaseModule
*/
void TV_ReleaseModule( const char *game )
{
	tv_module_t *iter;

	assert( game && strlen( game ) < MAX_CONFIGSTRING_CHARS );
	assert( COM_ValidateRelativeFilename( game ) && !strchr( game, '/' ) );

	// see if it's already loaded
	iter = modules;
	if( !iter )
		Com_Error( ERR_FATAL, "Attempting to release non-existing module" );
	assert( iter->count > 0 );

	iter->count--;

	if( !iter->count )
	{
		iter->export->Shutdown();
#ifndef TV_MODULE_HARD_LINKED
		Com_UnloadGameLibrary( &iter->handle );
#endif
		Mem_Free( iter );

		modules = NULL;
	}
}

/*
* TV_GetModule
*/
tv_module_t *TV_GetModule( const char *game )
{
	int apiversion;
	tv_module_t *iter;
	void *( *builtinAPIfunc )(void *) = NULL;
#ifdef TV_MODULE_HARD_LINKED
	builtinAPIfunc = GetTVModuleAPI;
#endif

	assert( game && strlen( game ) < MAX_CONFIGSTRING_CHARS );
	assert( COM_ValidateRelativeFilename( game ) && !strchr( game, '/' ) );

	// see if it's already loaded
	iter = modules;
	if( iter )
	{
		iter->count++;
		return iter;
	}

	// make a new one
	iter = Mem_Alloc( tv_mempool, sizeof( tv_module_t ) );
	iter->count = 1;

	// load a new game dll
	iter->import.Print = TV_Module_Print;
	iter->import.Error = TV_Module_Error;
	iter->import.RelayError = TV_Module_RelayError;
	iter->import.GameCmd = TV_Module_GameCmd;
	iter->import.ConfigString = TV_Module_ConfigString;

	iter->import.CM_TransformedPointContents = TV_Module_CM_TransformedPointContents;
	iter->import.CM_TransformedBoxTrace = TV_Module_CM_TransformedBoxTrace;
	iter->import.CM_RoundUpToHullSize = TV_Module_CM_RoundUpToHullSize;
	iter->import.CM_InlineModel = TV_Module_CM_InlineModel;
	iter->import.CM_InlineModelBounds = TV_Module_CM_InlineModelBounds;
	iter->import.CM_ModelForBBox = TV_Module_CM_ModelForBBox;
	iter->import.CM_OctagonModelForBBox = TV_Module_CM_OctagonModelForBBox;
	iter->import.CM_AreasConnected = TV_Module_CM_AreasConnected;
	iter->import.CM_BoxLeafnums = TV_Module_CM_BoxLeafnums;
	iter->import.CM_LeafCluster = TV_Module_CM_LeafCluster;
	iter->import.CM_LeafArea = TV_Module_CM_LeafArea;

	iter->import.Milliseconds = Sys_Milliseconds;

	iter->import.FS_FOpenFile = FS_FOpenFile;
	iter->import.FS_Read = FS_Read;
	iter->import.FS_Write = FS_Write;
	iter->import.FS_Print = FS_Print;
	iter->import.FS_Tell = FS_Tell;
	iter->import.FS_Seek = FS_Seek;
	iter->import.FS_Eof = FS_Eof;
	iter->import.FS_Flush = FS_Flush;
	iter->import.FS_FCloseFile = FS_FCloseFile;
	iter->import.FS_RemoveFile = FS_RemoveFile;
	iter->import.FS_GetFileList = FS_GetFileList;
	iter->import.FS_FirstExtension = FS_FirstExtension;

	iter->import.Mem_Alloc = TV_Module_MemAlloc;
	iter->import.Mem_Free = TV_Module_MemFree;

	iter->import.Dynvar_Create = Dynvar_Create;
	iter->import.Dynvar_Destroy = Dynvar_Destroy;
	iter->import.Dynvar_Lookup = Dynvar_Lookup;
	iter->import.Dynvar_GetName = Dynvar_GetName;
	iter->import.Dynvar_GetValue = Dynvar_GetValue;
	iter->import.Dynvar_SetValue = Dynvar_SetValue;
	iter->import.Dynvar_AddListener = Dynvar_AddListener;
	iter->import.Dynvar_RemoveListener = Dynvar_RemoveListener;

	iter->import.Cvar_Get = Cvar_Get;
	iter->import.Cvar_Set = Cvar_Set;
	iter->import.Cvar_SetValue = Cvar_SetValue;
	iter->import.Cvar_ForceSet = Cvar_ForceSet;
	iter->import.Cvar_Value = Cvar_Value;
	iter->import.Cvar_String = Cvar_String;

	iter->import.Cmd_TokenizeString = Cmd_TokenizeString;
	iter->import.Cmd_Argc = Cmd_Argc;
	iter->import.Cmd_Argv = Cmd_Argv;
	iter->import.Cmd_Args = Cmd_Args;
	iter->import.Cmd_AddCommand = Cmd_AddCommand;
	iter->import.Cmd_RemoveCommand = Cmd_RemoveCommand;

	iter->import.AddCommandString = Cbuf_AddText;

	iter->import.DropClient = TV_Module_DropClient;
	iter->import.GetClientState = TV_Module_GetClientState;
	iter->import.ExecuteClientThinks = TV_Module_ExecuteClientThinks;

	iter->import.LocateEntities = TV_Module_LocateEntities;
	iter->import.LocateLocalEntities = TV_Module_LocateLocalEntities;

	if( builtinAPIfunc ) {
		iter->export = builtinAPIfunc( &iter->import );
	}
	else {
		iter->export = (tv_module_export_t *)Com_LoadGameLibrary( "tv", "GetTVModuleAPI", &iter->handle, 
			&iter->import, false, NULL );
	}
	if( !iter->export )
	{
		Mem_Free( iter );
		return NULL;
	}

	apiversion = iter->export->API();
	if( apiversion != TV_MODULE_API_VERSION )
	{
		Mem_Free( iter );
		return NULL;
	}

	modules = iter;

	iter->export->Init( game, tv_maxclients->integer );

	return iter;
}

/*
* TV_Relay_FreeModule
*/
static void TV_Relay_FreeModule( relay_t *relay )
{
	relay->module_export->ShutdownRelay( relay->module );
	relay->module = NULL;
	relay->num_active_specs = 0;
	Mem_FreePool( &relay->module_mempool );
}

/*
* TV_Relay_ShutdownModule
* 
* Called when either the entire server is being killed, or
* it is changing to a different game directory.
*/
void TV_Relay_ShutdownModule( relay_t *relay )
{
	if( !relay->module_export )
		return;

	TV_Relay_FreeModule( relay );

	TV_ReleaseModule( relay->game );

	// update upstream name for upstream
	TV_Relay_UpstreamUserinfoChanged( relay );
}

/*
* TV_Relay_InitModule
* 
* Init the game subsystem for a new map
*/
void TV_Relay_InitModule( relay_t *relay )
{
	tv_module_t *module;

	if( relay->module )
		TV_Relay_FreeModule( relay );

	module = TV_GetModule( relay->game );
	if( !module || !module->export )
		TV_Relay_Error( relay, "Failed to load module" );

	relay->module_export = module->export;
	relay->module_mempool = _Mem_AllocPool( relay->upstream->mempool, va( "TV Module Progs" ), MEMPOOL_GAMEPROGS, __FILE__, __LINE__ );
	relay->module = relay->module_export->InitRelay( relay, relay->snapFrameTime, relay->playernum );

	Mem_DebugCheckSentinelsGlobal( );

	// update upstream name for upstream
	TV_Relay_UpstreamUserinfoChanged( relay );
}
