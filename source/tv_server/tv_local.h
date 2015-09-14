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

#ifndef __TV_LOCAL_H
#define __TV_LOCAL_H

#include "../qcommon/qcommon.h"

#include "./tv_module/tvm_public.h"

typedef struct upstream_s upstream_t;

typedef struct ginfo_s
{
	struct edict_s *edicts;
	struct client_s *clients;
	int edict_size;
	int num_edicts;         // current number, <= max_edicts
	int max_edicts;
	int max_clients;		// <= sv_maxclients, <= max_edicts

	struct edict_s *local_edicts;
	int local_edict_size;
	int local_num_edicts;
	int local_max_edicts;
} ginfo_t;

struct gclient_s
{
	player_state_t ps;  // communicated by server to clients
	client_shared_t	r;
};

struct edict_s
{
	entity_state_t s;
	entity_shared_t	r;
};

#ifdef TCP_ALLOW_TVCONNECT
#define MAX_INCOMING_CONNECTIONS 256
typedef struct
{
	bool active;
	unsigned int time;      // for timeout
	socket_t socket;
	netadr_t address;
} incoming_t;
#endif

#define	MAX_CHALLENGES	1024
typedef struct
{
	netadr_t adr;
	int challenge;
	int time;
} challenge_t;

// MAX_SNAP_ENTITIES is the guess of what we consider maximum amount of entities
// to be sent to a client into a snap. It's used for finding size of the backup storage
#define MAX_SNAP_ENTITIES 64

typedef struct
{
	char *name;
	uint8_t *data;            // file being downloaded
	int size;               // total bytes (can't use EOF because of paks)
	unsigned int timeout;   // so we can free the file being downloaded
	// if client omits sending success or failure message
} client_download_t;

typedef struct
{
	bool allentities;
	bool multipov;
	bool relay;
	int clientarea;
	int numareas;
	size_t areabytes;
	uint8_t *areabits;                // portalarea visibility bits
	int numplayers;
	int ps_size;
	player_state_t *ps;                 // [numplayers]
	int num_entities;
	int first_entity;                   // into the circular sv_packet_entities[]
	unsigned int sentTimeStamp;         // time at what this frame snap was sent to the clients
	unsigned int UcmdExecuted;
	game_state_t gameState;
} client_snapshot_t;

typedef enum { RD_NONE, RD_PACKET } redirect_t;

#define	TV_OUTPUTBUF_LENGTH ( MAX_MSGLEN - 16 )
extern char tv_outputbuf[TV_OUTPUTBUF_LENGTH];

typedef struct
{
	const socket_t *socket;
	const netadr_t *address;
} flush_params_t;

void TV_FlushRedirect( int sv_redirected, const char *outputbuf, const void *extra );

typedef struct
{
	unsigned int framenum;
	char command[MAX_STRING_CHARS];
} game_command_t;

#define MAX_FLOOD_MESSAGES 32
typedef struct
{
	unsigned int locktill;           // locked from talking
	unsigned int when[MAX_FLOOD_MESSAGES];           // when messages were said
	int whenhead;             // head pointer for when said
} client_flood_t;

#define	LATENCY_COUNTS	16
typedef struct client_s
{
	sv_client_state_t state;

	char userinfo[MAX_INFO_STRING];             // name, etc

	relay_t	*relay;

	bool reliable;                  // no need for acks, upstream is reliable
	bool mv;                        // send multiview data to the client
	bool individual_socket;         // client has it's own socket that has to be checked separately

	socket_t socket;

	char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	unsigned int reliableSequence;      // last added reliable message, not necesarily sent or acknowledged yet
	unsigned int reliableAcknowledge;   // last acknowledged reliable message
	unsigned int reliableSent;          // last sent reliable message, not necesarily acknowledged yet
	int suppressCount;					// number of messages rate suppressed

	game_command_t gameCommands[MAX_RELIABLE_COMMANDS];
	int gameCommandCurrent;             // position in the gameCommands table

	unsigned int clientCommandExecuted; // last client-command we received

	unsigned int UcmdTime;
	unsigned int UcmdExecuted;          // last client-command we executed
	unsigned int UcmdReceived;          // last client-command we received
	usercmd_t ucmds[CMD_BACKUP];        // each message will send several old cmds

	unsigned int lastPacketSentTime;    // time when we sent the last message to this client
	unsigned int lastPacketReceivedTime; // time when we received the last message from this client
	unsigned lastconnect;

	int lastframe;                  // used for delta compression etc.
	bool nodelta;               // send one non delta compressed frame trough
	int nodelta_frame;              // when we get confirmation of this frame, the non-delta frame is trough
	usercmd_t lastcmd;              // for filling in big drops
	unsigned int lastSentFrameNum;  // for knowing which was last frame we sent

	int frame_latency[LATENCY_COUNTS];
	int ping;
	edict_t	*edict;                     // EDICT_NUM(clientnum+1)
	char name[MAX_INFO_VALUE];          // extracted from userinfo, high bits masked

	// The sounds datagram is written to by multicasted sound commands
	// It can be harmlessly overflowed.
	msg_t soundsmsg;
	uint8_t soundsmsgData[MAX_MSGLEN];

	client_snapshot_t snapShots[UPDATE_BACKUP]; // updates can be delta'd from here

	client_download_t download;

	int challenge;                  // challenge of this user, randomly generated
	bool tv;
	client_flood_t flood;

	netchan_t netchan;
} client_t;

typedef struct
{
	int spawncount;
	unsigned int next_heartbeat;
	unsigned int framenum;
	unsigned int lastrun;
	unsigned int snapFrameTime;
} tv_lobby_t;

typedef struct
{
	unsigned int realtime;

	tv_lobby_t lobby;

	netadr_t address;
	netadr_t addressIPv6;

	// downstream
#ifdef TCP_ALLOW_TVCONNECT
	socket_t socket_tcp;
	socket_t socket_tcp6;
#endif
	socket_t socket_udp;
	socket_t socket_udp6;

	challenge_t challenges[MAX_CHALLENGES];
#ifdef TCP_ALLOW_TVCONNECT
	incoming_t incoming[MAX_INCOMING_CONNECTIONS];
#endif

	client_t *clients;    // [tv_maxclients->integer];
	int nummvclients;

	// relay
	int numupstreams;
	upstream_t **upstreams; // maxrelay
} tv_t;

extern mempool_t *tv_mempool;

// some hax, because we want to save the file and line where the copy was called
// from, not the file and line from ZoneCopyString function
char *_TVCopyString_Pool( mempool_t *pool, const char *in, const char *filename, int fileline );
#define TV_CopyString( in ) _TVCopyString_Pool( tv_mempool, __FILE__, __LINE__ )

extern cvar_t *tv_password;

extern cvar_t *tv_rcon_password;

extern cvar_t *tv_name;
extern cvar_t *tv_timeout;
extern cvar_t *tv_zombietime;
extern cvar_t *tv_maxclients;
extern cvar_t *tv_maxmvclients;
extern cvar_t *tv_compresspackets;
extern cvar_t *tv_reconnectlimit;
extern cvar_t *tv_public;
extern cvar_t *tv_autorecord;
extern cvar_t *tv_lobbymusic;

extern cvar_t *tv_masterservers;
extern cvar_t *tv_masterservers_steam;

extern cvar_t *tv_floodprotection_messages;
extern cvar_t *tv_floodprotection_seconds;
extern cvar_t *tv_floodprotection_penalty;

extern cvar_t *tv_port;

extern tv_t tvs;

#endif // __TV_LOCAL_H
