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

#ifndef __TVM_PUBLIC_H
#define __TVM_PUBLIC_H

#define TV_MODULE_API_VERSION 7

//===============================================================

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s *prev, *next;
	int entNum;
} link_t;

#define	MAX_ENT_CLUSTERS    16

typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;
typedef struct relay_s relay_t;
typedef struct tvm_relay_s tvm_relay_t;

typedef struct
{
	int ping;
	int health;
	int frags;
} client_shared_t;

typedef struct
{
	gclient_t *client;
	bool inuse;
	int linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t area;                // linked to a division node or leaf

	int num_clusters;           // if -1, use headnode instead
	int clusternums[MAX_ENT_CLUSTERS];
	int headnode;               // unused if num_clusters != -1
	int areanum, areanum2;

	//================================

	int svflags;                // SVF_NOCLIENT, SVF_MONSTER, etc
	vec3_t mins, maxs;
	vec3_t absmin, absmax, size;
	solid_t	solid;
	int clipmask;
	edict_t	*owner;
#ifdef TVCOLLISION4D
	vec3_t deltaOrigin4D;
#endif
} entity_shared_t;

//===============================================================

#define	MAX_PARSE_ENTITIES	1024
typedef struct snapshot_s
{
	bool valid;             // cleared if delta parsing was invalid
	int serverFrame;
	unsigned int serverTime;    // time in the server when frame was created
	unsigned int ucmdExecuted;
	bool delta;
	bool allentities;
	bool multipov;
	int deltaFrameNum;
	size_t areabytes;
	uint8_t *areabits;             // portalarea visibility bits
	int numplayers;
	player_state_t playerState;
	player_state_t playerStates[MAX_CLIENTS];
	int numEntities;
	entity_state_t parsedEntities[MAX_PARSE_ENTITIES];
	game_state_t gameState;
	int numgamecommands;
	gcommand_t gamecommands[MAX_PARSE_GAMECOMMANDS];
	char gamecommandsData[(MAX_STRING_CHARS / 16) * MAX_PARSE_GAMECOMMANDS];
	size_t gamecommandsDataHead;
} snapshot_t;

//===============================================================

//
// functions provided by the main engine
//
typedef struct
{
	void ( *Print )( const char *msg );
	void ( *Error )( const char *msg );
	void ( *RelayError )( relay_t *relay, const char *msg );

	// server commands sent to clients
	void ( *GameCmd )( relay_t *relay, int numClient, const char *cmd );
	void ( *ConfigString )( relay_t *relay, int number, const char *value );

	unsigned int ( *Milliseconds )( void );

	struct cmodel_s	*( *CM_InlineModel )( relay_t *relay, int num );
	int ( *CM_TransformedPointContents )( relay_t *relay, vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles );
	void ( *CM_RoundUpToHullSize )( relay_t *relay, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel );
	void ( *CM_TransformedBoxTrace )( relay_t *relay, trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles );
	void ( *CM_InlineModelBounds )( relay_t *relay, struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs );
	struct cmodel_s	*( *CM_ModelForBBox )( relay_t *relay, vec3_t mins, vec3_t maxs );
	struct cmodel_s	*( *CM_OctagonModelForBBox )( relay_t *relay, vec3_t mins, vec3_t maxs );
	bool ( *CM_AreasConnected )( relay_t *relay, int area1, int area2 );
	int ( *CM_BoxLeafnums )( relay_t *relay, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode );
	int ( *CM_LeafCluster )( relay_t *relay, int leafnum );
	int ( *CM_LeafArea )( relay_t *relay, int leafnum );

	// managed memory allocation
	void *( *Mem_Alloc )( relay_t *relay_server, size_t size, const char *filename, int fileline );
	void ( *Mem_Free )( void *data, const char *filename, int fileline );

	// dynvars
	dynvar_t *( *Dynvar_Create )( const char *name, bool console, dynvar_getter_f getter, dynvar_setter_f setter );
	void ( *Dynvar_Destroy )( dynvar_t *dynvar );
	dynvar_t *( *Dynvar_Lookup )( const char *name );
	const char *( *Dynvar_GetName )( dynvar_t *dynvar );
	dynvar_get_status_t ( *Dynvar_GetValue )( dynvar_t *dynvar, void **value );
	dynvar_set_status_t ( *Dynvar_SetValue )( dynvar_t *dynvar, void *value );
	void ( *Dynvar_AddListener )( dynvar_t *dynvar, dynvar_listener_f listener );
	void ( *Dynvar_RemoveListener )( dynvar_t *dynvar, dynvar_listener_f listener );

	// console variable interaction
	cvar_t *( *Cvar_Get )( const char *name, const char *value, int flags );
	cvar_t *( *Cvar_Set )( const char *name, const char *value );
	void ( *Cvar_SetValue )( const char *name, float value );
	cvar_t *( *Cvar_ForceSet )( const char *name, const char *value );  // will return 0 0 if not found
	float ( *Cvar_Value )( const char *name );
	const char *( *Cvar_String )( const char *name );

	// ClientCommand and ServerCommand parameter access
	void ( *Cmd_TokenizeString )( const char *text );
	int ( *Cmd_Argc )( void );
	char *( *Cmd_Argv )( int arg );
	char *( *Cmd_Args )( void );        // concatenation of all argv >= 1

	void ( *Cmd_AddCommand )( const char *name, void ( *cmd )( void ) );
	void ( *Cmd_RemoveCommand )( const char *cmd_name );

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
	const char *( *FS_FirstExtension )( const char *filename, const char *extensions[], int num_extensions );

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void ( *AddCommandString )( const char *text );

	// client stuff
	void ( *DropClient )( relay_t *relay, int numClient, int type, const char *message );
	int ( *GetClientState )( relay_t *relay, int numClient );
	void ( *ExecuteClientThinks )( relay_t *relay, int clientNum );

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	void ( *LocateEntities )( relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts );
	void ( *LocateLocalEntities )( relay_t *relay, struct edict_s *edicts, int edict_size, int num_edicts, int max_edicts );
} tv_module_import_t;

//
// functions exported by the game subsystem
//
typedef struct
{
	// if API is different, the dll cannot be used
	int ( *API )( void );

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void ( *Init )( const char *game, unsigned int maxclients );
	void ( *Shutdown )( void );

	tvm_relay_t *( *InitRelay )( relay_t *relay_server, unsigned int snapFrameTime, int playernum );
	void ( *ShutdownRelay )( tvm_relay_t *relay );

	// each new level entered will cause a call to SpawnEntities
	void ( *SpawnEntities )( tvm_relay_t *relay, const char *mapname, const char *entstring, int entstrlen );
	void ( *SetAudoTrack )( tvm_relay_t *relay, const char *track );

	bool ( *CanConnect )( tvm_relay_t *relay, char *userinfo );
	void ( *ClientConnect )( tvm_relay_t *relay, edict_t *ent, char *userinfo );
	void ( *ClientBegin )( tvm_relay_t *relay, edict_t *ent );
	void ( *ClientUserinfoChanged )( tvm_relay_t *relay, edict_t *ent, char *userinfo );
	bool ( *ClientMultiviewChanged )( tvm_relay_t *relay, edict_t *ent, bool multiview );
	void ( *ClientDisconnect )( tvm_relay_t *relay, edict_t *ent );
	bool ( *ClientCommand )( tvm_relay_t *relay, edict_t *ent );
	void ( *ClientThink )( tvm_relay_t *relay, edict_t *ent, usercmd_t *cmd, int timeDelta );

	void ( *NewFrameSnapshot )( tvm_relay_t *relay, snapshot_t *frame );
	bool ( *ConfigString )( tvm_relay_t *relay, int number, const char *value );

	void ( *RunFrame )( tvm_relay_t *relay, unsigned int msec );
	void ( *SnapFrame )( tvm_relay_t *relay );
	void ( *ClearSnap )( tvm_relay_t *relay );

	game_state_t *( *GetGameState )( tvm_relay_t *relay );
	bool ( *AllowDownload )( tvm_relay_t *relay, edict_t *ent, const char *requestname, const char *uploadname );
} tv_module_export_t;

#endif // __TVM_PUBLIC_H
