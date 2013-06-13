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

#include "qcommon.h"

#include "snap_write.h"

/*
=========================================================================

Encode a client frame onto the network channel

=========================================================================
*/

/*
* SNAP_EmitPacketEntities
*
* Writes a delta update of an entity_state_t list to the message.
*/
static void SNAP_EmitPacketEntities( ginfo_t *gi, client_snapshot_t *from, client_snapshot_t *to, msg_t *msg, entity_state_t *baselines, entity_state_t *client_entities, int num_client_entities )
{
	entity_state_t *oldent, *newent;
	int oldindex, newindex;
	int oldnum, newnum;
	int from_num_entities;
	int bits;

	MSG_WriteByte( msg, svc_packetentities );

	if( !from )
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while( newindex < to->num_entities || oldindex < from_num_entities )
	{
		if( newindex >= to->num_entities )
		{
			newent = NULL;
			newnum = 9999;
		}
		else
		{
			newent = &client_entities[( to->first_entity+newindex )%num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities )
		{
			oldent = NULL;
			oldnum = 9999;
		}
		else
		{
			oldent = &client_entities[( from->first_entity+oldindex )%num_client_entities];
			oldnum = oldent->number;
		}

		if( newnum == oldnum )
		{
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping ( wsw : jal : I removed it from the players )
			MSG_WriteDeltaEntity( oldent, newent, msg, qfalse, ( ( EDICT_NUM( newent->number ) )->r.svflags & SVF_TRANSMITORIGIN2 ) ? qtrue : qfalse );
			oldindex++;
			newindex++;
			continue;
		}

		if( newnum < oldnum )
		{
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity( &baselines[newnum], newent, msg, qtrue, ( ( EDICT_NUM( newent->number ) )->r.svflags & SVF_TRANSMITORIGIN2 ) ? qtrue : qfalse );
			newindex++;
			continue;
		}

		if( newnum > oldnum )
		{
			// the old entity isn't present in the new message
			bits = U_REMOVE;
			if( oldnum >= 256 )
				bits |= ( U_NUMBER16 | U_MOREBITS1 );

			MSG_WriteByte( msg, bits&255 );
			if( bits & 0x0000ff00 )
				MSG_WriteByte( msg, ( bits>>8 )&255 );

			if( bits & U_NUMBER16 )
				MSG_WriteShort( msg, oldnum );
			else
				MSG_WriteByte( msg, oldnum );

			oldindex++;
			continue;
		}
	}

	MSG_WriteShort( msg, 0 ); // end of packetentities
}

/*
* SNAP_WriteDeltaGameStateToClient
*/
static void SNAP_WriteDeltaGameStateToClient( client_snapshot_t *from, client_snapshot_t *to, msg_t *msg )
{
	int i;
	short statbits;
	qbyte bits;
	game_state_t *gameState, *deltaGameState;
	game_state_t dummy;

	gameState = &to->gameState;
	if( !from )
	{
		memset( &dummy, 0, sizeof( dummy ) );
		deltaGameState = &dummy;
	}
	else
		deltaGameState = &from->gameState;

	// FIXME: This protocol needs optimization

	assert( MAX_GAME_STATS == 16 );
	assert( MAX_GAME_LONGSTATS == 8 );

	bits = 0;
	for( i = 0; i < MAX_GAME_LONGSTATS; i++ )
	{
		if( deltaGameState->longstats[i] != gameState->longstats[i] )
			bits |= 1<<i;
	}

	statbits = 0;
	for( i = 0; i < MAX_GAME_STATS; i++ )
	{
		if( deltaGameState->stats[i] != gameState->stats[i] )
			statbits |= 1<<i;
	}

	MSG_WriteByte( msg, svc_match );
	MSG_WriteByte( msg, bits );
	MSG_WriteShort( msg, statbits );

	if( bits )
	{
		for( i = 0; i < MAX_GAME_LONGSTATS; i++ )
		{
			if( bits & ( 1<<i ) )
				MSG_WriteLong( msg, (int)gameState->longstats[i] );
		}
	}

	if( statbits )
	{
		for( i = 0; i < MAX_GAME_STATS; i++ )
		{
			if( statbits & ( 1<<i ) )
				MSG_WriteShort( msg, gameState->stats[i] );
		}
	}
}

/*
* SNAP_WritePlayerstateToClient
*/
static void SNAP_WritePlayerstateToClient( player_state_t *ops, player_state_t *ps, msg_t *msg )
{
	int i;
	int pflags;
	player_state_t dummy;
	int statbits[SNAP_STATS_LONGS];

	if( !ops )
	{
		memset( &dummy, 0, sizeof( dummy ) );
		ops = &dummy;
	}

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if( ps->pmove.pm_type != ops->pmove.pm_type )
		pflags |= PS_M_TYPE;

	if( ps->pmove.origin[0] != ops->pmove.origin[0] )
		pflags |= PS_M_ORIGIN0;
	if( ps->pmove.origin[1] != ops->pmove.origin[1] )
		pflags |= PS_M_ORIGIN1;
	if( ps->pmove.origin[2] != ops->pmove.origin[2] )
		pflags |= PS_M_ORIGIN2;

	if( ps->pmove.velocity[0] != ops->pmove.velocity[0] )
		pflags |= PS_M_VELOCITY0;
	if( ps->pmove.velocity[1] != ops->pmove.velocity[1] )
		pflags |= PS_M_VELOCITY1;
	if( ps->pmove.velocity[2] != ops->pmove.velocity[2] )
		pflags |= PS_M_VELOCITY2;

	if( ps->pmove.pm_time != ops->pmove.pm_time )
		pflags |= PS_M_TIME;

	if( ps->pmove.pm_flags != ops->pmove.pm_flags )
		pflags |= PS_M_FLAGS;

	if( ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0] )
		pflags |= PS_M_DELTA_ANGLES0;
	if( ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1] )
		pflags |= PS_M_DELTA_ANGLES1;
	if( ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2] )
		pflags |= PS_M_DELTA_ANGLES2;

	if( ps->event[0] )
		pflags |= PS_EVENT;

	if( ps->event[1] )
		pflags |= PS_EVENT2;

	if( ps->viewangles[0] != ops->viewangles[0]
	|| ps->viewangles[1] != ops->viewangles[1]
	|| ps->viewangles[2] != ops->viewangles[2] )
		pflags |= PS_VIEWANGLES;

	if( ps->pmove.gravity != ops->pmove.gravity )
		pflags |= PS_M_GRAVITY;

	if( ps->weaponState != ops->weaponState )
		pflags |= PS_WEAPONSTATE;

	if( ps->fov != ops->fov )
		pflags |= PS_FOV;

	if( ps->POVnum != ops->POVnum )
		pflags |= PS_POVNUM;

	if( ps->playerNum != ops->playerNum )
		pflags |= PS_PLAYERNUM;

	if( ps->viewheight != ops->viewheight )
		pflags |= PS_VIEWHEIGHT;

	for( i = 0; i < PM_STAT_SIZE; i++ )
	{
		if( ps->pmove.stats[i] != ops->pmove.stats[i] )
		{
			pflags |= PS_PMOVESTATS;
			break;
		}
	}

	for( i = 0; i < MAX_ITEMS; i++ )
	{
		if( ps->inventory[i] != ops->inventory[i] )
		{
			pflags |= PS_INVENTORY;
			break;
		}
	}

	if( ps->plrkeys != ops->plrkeys )
		pflags |= PS_PLRKEYS;

	//
	// write it
	//
	MSG_WriteByte( msg, svc_playerinfo );

	if( pflags & 0xff000000 )
		pflags |= PS_MOREBITS3 | PS_MOREBITS2 | PS_MOREBITS1;
	else if( pflags & 0x00ff0000 )
		pflags |= PS_MOREBITS2 | PS_MOREBITS1;
	else if( pflags & 0x0000ff00 )
		pflags |= PS_MOREBITS1;

	MSG_WriteByte( msg, pflags&255 );

	if( pflags & 0xff000000 )
	{
		MSG_WriteByte( msg, ( pflags>>8 )&255 );
		MSG_WriteByte( msg, ( pflags>>16 )&255 );
		MSG_WriteByte( msg, ( pflags>>24 )&255 );
	}
	else if( pflags & 0x00ff0000 )
	{
		MSG_WriteByte( msg, ( pflags>>8 )&255 );
		MSG_WriteByte( msg, ( pflags>>16 )&255 );
	}
	else if( pflags & 0x0000ff00 )
	{
		MSG_WriteByte( msg, ( pflags>>8 )&255 );
	}

	//
	// write the pmove_state_t
	//
	if( pflags & PS_M_TYPE )
		MSG_WriteByte( msg, ps->pmove.pm_type );

	if( pflags & PS_M_ORIGIN0 )
		MSG_WriteInt3( msg, (int)( ps->pmove.origin[0]*PM_VECTOR_SNAP ) );
	if( pflags & PS_M_ORIGIN1 )
		MSG_WriteInt3( msg, (int)( ps->pmove.origin[1]*PM_VECTOR_SNAP ) );
	if( pflags & PS_M_ORIGIN2 )
		MSG_WriteInt3( msg, (int)( ps->pmove.origin[2]*PM_VECTOR_SNAP ) );

	if( pflags & PS_M_VELOCITY0 )
		MSG_WriteInt3( msg, (int)( ps->pmove.velocity[0]*PM_VECTOR_SNAP ) );
	if( pflags & PS_M_VELOCITY1 )
		MSG_WriteInt3( msg, (int)( ps->pmove.velocity[1]*PM_VECTOR_SNAP ) );
	if( pflags & PS_M_VELOCITY2 )
		MSG_WriteInt3( msg, (int)( ps->pmove.velocity[2]*PM_VECTOR_SNAP ) );

	if( pflags & PS_M_TIME )
		MSG_WriteByte( msg, ps->pmove.pm_time );

	if( pflags & PS_M_FLAGS )
		MSG_WriteShort( msg, ps->pmove.pm_flags );

	if( pflags & PS_M_DELTA_ANGLES0 )
		MSG_WriteShort( msg, ps->pmove.delta_angles[0] );
	if( pflags & PS_M_DELTA_ANGLES1 )
		MSG_WriteShort( msg, ps->pmove.delta_angles[1] );
	if( pflags & PS_M_DELTA_ANGLES2 )
		MSG_WriteShort( msg, ps->pmove.delta_angles[2] );

	if( pflags & PS_EVENT )
	{
		if( !ps->eventParm[0] )
			MSG_WriteByte( msg, ps->event[0] & ~EV_INVERSE );
		else
		{
			MSG_WriteByte( msg, ps->event[0] | EV_INVERSE );
			MSG_WriteByte( msg, ps->eventParm[0] );
		}
	}

	if( pflags & PS_EVENT2 )
	{
		if( !ps->eventParm[1] )
			MSG_WriteByte( msg, ps->event[1] & ~EV_INVERSE );
		else
		{
			MSG_WriteByte( msg, ps->event[1] | EV_INVERSE );
			MSG_WriteByte( msg, ps->eventParm[1] );
		}
	}

	if( pflags & PS_VIEWANGLES )
	{
		MSG_WriteAngle16( msg, ps->viewangles[0] );
		MSG_WriteAngle16( msg, ps->viewangles[1] );
		MSG_WriteAngle16( msg, ps->viewangles[2] );
	}

	if( pflags & PS_M_GRAVITY )
		MSG_WriteShort( msg, ps->pmove.gravity );

	if( pflags & PS_WEAPONSTATE )
		MSG_WriteByte( msg, ps->weaponState );

	if( pflags & PS_FOV )
		MSG_WriteByte( msg, (qbyte)ps->fov );

	if( pflags & PS_POVNUM )
		MSG_WriteByte( msg, (qbyte)ps->POVnum );

	if( pflags & PS_PLAYERNUM )
		MSG_WriteByte( msg, (qbyte)ps->playerNum );

	if( pflags & PS_VIEWHEIGHT )
		MSG_WriteChar( msg, (char)ps->viewheight );

	if( pflags & PS_PMOVESTATS )
	{
		int pmstatbits;
		
		pmstatbits = 0;
		for( i = 0; i < PM_STAT_SIZE; i++ )
		{
			if( ps->pmove.stats[i] != ops->pmove.stats[i] )
				pmstatbits |= ( 1<<i );
		}

		MSG_WriteShort( msg, pmstatbits & 0xFFFF );

		for( i = 0; i < PM_STAT_SIZE; i++ )
		{
			if( pmstatbits & ( 1<<i ) )
				MSG_WriteShort( msg, ps->pmove.stats[i] );
		}
	}

	if( pflags & PS_INVENTORY )
	{
		int invstatbits[SNAP_INVENTORY_LONGS];

		// send inventory stats
		memset( invstatbits, 0, sizeof( invstatbits ) );
		for( i = 0; i < MAX_ITEMS; i++ )
		{
			if( ps->inventory[i] != ops->inventory[i] )
				invstatbits[i>>5] |= ( 1<<(i&31) );
		}

		for( i = 0; i < SNAP_INVENTORY_LONGS; i++ ) {
			MSG_WriteLong( msg, invstatbits[i] );
		}

		for( i = 0; i < MAX_ITEMS; i++ )
		{
			if( invstatbits[i>>5] & ( 1<<(i&31) ) )
				MSG_WriteByte( msg, (qbyte)ps->inventory[i] );
		}
	}

	if( pflags & PS_PLRKEYS )
		MSG_WriteByte( msg, ps->plrkeys );

	// send stats
	memset( statbits, 0, sizeof( statbits ) );
	for( i = 0; i < PS_MAX_STATS; i++ )
	{
		if( ps->stats[i] != ops->stats[i] )
			statbits[i>>5] |= 1<<(i&31);
	}

	for( i = 0; i < SNAP_STATS_LONGS; i++ ) {
		MSG_WriteLong( msg, statbits[i] );
	}

	for( i = 0; i < PS_MAX_STATS; i++ )
	{
		if( statbits[i>>5] & ( 1<<(i&31) ) )
			MSG_WriteShort( msg, ps->stats[i] );
	}
}

/*
* SNAP_WriteMultiPOVCommands
*/
static void SNAP_WriteMultiPOVCommands( ginfo_t *gi, client_t *client, msg_t *msg, unsigned int frameNum )
{
	int i, index;
	client_t *cl;
	int positions[MAX_CLIENTS];
	char *command;
	int maxnumtargets, numtargets, maxtarget;
	unsigned int framenum;
	qbyte targets[MAX_CLIENTS/8];

	// find the first command to send from every client
	maxnumtargets = maxtarget = 0;
	for( i = 0; i < gi->max_clients; i++ )
	{
		cl = gi->clients + i;

		if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->r.svflags & SVF_NOCLIENT ) ) && cl != client ) )
			continue;

		maxnumtargets++;
		for( positions[i] = cl->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1;
			positions[i] <= cl->gameCommandCurrent; positions[i]++ )
		{
			index = positions[i] & ( MAX_RELIABLE_COMMANDS - 1 );

			// we need to check for too new commands too, because gamecommands for the next snap are generated
			// all the time, and we might want to create a server demo frame or something in between snaps
			if( cl->gameCommands[index].command[0] && cl->gameCommands[index].framenum + 256 >= frameNum &&
				cl->gameCommands[index].framenum <= frameNum &&
				( client->lastframe >= 0 && cl->gameCommands[index].framenum > (unsigned int)client->lastframe ) )
				break;
		}
	}

	// send all messages, combining similar messages together to save space
	do
	{
		command = NULL;
		maxtarget = 0;
		numtargets = 0;
		framenum = 0;

		// we find the message with the earliest framenum, and collect all recipients for that
		for( i = 0; i < gi->max_clients; i++ )
		{
			cl = gi->clients + i;

			if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->r.svflags & SVF_NOCLIENT ) ) && cl != client ) )
				continue;

			if( positions[i] > cl->gameCommandCurrent )
				continue;

			index = positions[i] & ( MAX_RELIABLE_COMMANDS - 1 );

			if( command && !strcmp( cl->gameCommands[index].command, command ) &&
				framenum == cl->gameCommands[index].framenum )
			{
				targets[i>>3] |= 1 << ( i&7 );
				maxtarget = i+1;
				numtargets++;
			}
			else if( !command || cl->gameCommands[index].framenum < framenum )
			{
				command = cl->gameCommands[index].command;
				framenum = cl->gameCommands[index].framenum;
				memset( targets, 0, sizeof( targets ) );
				targets[i>>3] |= 1 << ( i&7 );
				maxtarget = i+1;
				numtargets = 1;
			}

			if( numtargets == maxnumtargets )
				break;
		}

		// send it
		if( command )
		{
			// never write a command if it's of a higher framenum
			if( frameNum >= framenum )
			{
				// do not allow the message buffer to overflow (can happen on flood updates)
				if( msg->cursize + strlen( command ) + 512 > msg->maxsize )
					continue;

				MSG_WriteShort( msg, frameNum - framenum );
				MSG_WriteString( msg, command );

				// 0 means everyone
				if( numtargets == maxnumtargets )
				{
					MSG_WriteByte( msg, 0 );
				}
				else
				{
					int bytes = (maxtarget + 7)/8;
					MSG_WriteByte( msg, bytes );
					MSG_WriteData( msg, targets, bytes );
				}
			}

			for( i = 0; i < maxtarget; i++ )
				if( targets[i>>3] & ( 1<<( i&7 ) ) )
					positions[i]++;
		}
	} while( command );
}

/*
* SNAP_RelayMultiPOVCommands
*/
static void SNAP_RelayMultiPOVCommands( ginfo_t *gi, client_t *client, msg_t *msg, int numcmds, gcommand_t *commands, const char *commandsData )
{
	int i, index;
	int first_index, last_index;
	gcommand_t *gcmd;
	const char *command;

	first_index = numcmds - MAX_RELIABLE_COMMANDS;
	last_index = first_index + MAX_RELIABLE_COMMANDS;

	clamp_low( first_index, 0 );
	clamp_high( last_index, numcmds );

	for( index = first_index, gcmd = commands + index; index < last_index; index++, gcmd++ )
	{
		command = commandsData + gcmd->commandOffset;

		// do not allow the message buffer to overflow (can happen on flood updates)
		if( msg->cursize + strlen( command ) + 512 > msg->maxsize )
			continue;

		MSG_WriteShort( msg, 0 );
		MSG_WriteString( msg, command );

		if( gcmd->all )
		{
			MSG_WriteByte( msg, 0 );
		}
		else
		{
			int maxtarget, bytes;

			maxtarget = 0;
			for( i = 0; i < MAX_CLIENTS; i++ )
				if( gcmd->targets[i>>3] & ( 1<<( i&7 ) ) )
					maxtarget = i+1;

			bytes = (maxtarget + 7)/8;
			MSG_WriteByte( msg, bytes );
			MSG_WriteData( msg, gcmd->targets, bytes );
		}
	}
}

/*
* SNAP_WriteFrameSnapToClient
*/
void SNAP_WriteFrameSnapToClient( ginfo_t *gi, client_t *client, msg_t *msg, unsigned int frameNum, unsigned int gameTime,
								 entity_state_t *baselines, client_entities_t *client_entities,
								 int numcmds, gcommand_t *commands, const char *commandsData )
{
	client_snapshot_t *frame, *oldframe;
	int flags, i, index, pos, length, supcnt;

	// this is the frame we are creating
	frame = &client->snapShots[frameNum & UPDATE_MASK];

	// for non-reliable clients we need to send nodelta frame until the client responds
	if( client->nodelta && !client->reliable )
	{
		if( !client->nodelta_frame )
			client->nodelta_frame = frameNum;
		else if( client->lastframe >= client->nodelta_frame )
			client->nodelta = qfalse;
	}

	if( client->lastframe <= 0 || (unsigned)client->lastframe > frameNum || client->nodelta )
	{
		// client is asking for a not compressed retransmit
		oldframe = NULL;
	}
	//else if( frameNum >= client->lastframe + (UPDATE_BACKUP - 3) )
	else if( frameNum >= (unsigned)client->lastframe + UPDATE_MASK )
	{
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
	}
	else
	{
		// we have a valid message to delta from
		oldframe = &client->snapShots[client->lastframe & UPDATE_MASK];
		if( oldframe->multipov != frame->multipov )
			oldframe = NULL;		// don't delta compress a frame of different POV type
	}

	if( client->nodelta && client->reliable )
		client->nodelta = qfalse;

	MSG_WriteByte( msg, svc_frame );

	pos = msg->cursize;
	MSG_WriteShort( msg, 0 );		// we will write length here

	MSG_WriteLong( msg, gameTime );	// serverTimeStamp
	MSG_WriteLong( msg, frameNum );
	MSG_WriteLong( msg, client->lastframe );
	MSG_WriteLong( msg, frame->UcmdExecuted );

	flags = 0;
	if( oldframe != NULL )
		flags |= FRAMESNAP_FLAG_DELTA;
	if( frame->allentities )
		flags |= FRAMESNAP_FLAG_ALLENTITIES;
	if( frame->multipov )
		flags |= FRAMESNAP_FLAG_MULTIPOV;
	MSG_WriteByte( msg, flags );

	supcnt = client->suppressCount;
#ifndef RATEKILLED
	supcnt = 0;
#endif
	client->suppressCount = 0;
	MSG_WriteByte( msg, supcnt );	// rate dropped packets

	// add game comands
	MSG_WriteByte( msg, svc_gamecommands );
	if( frame->multipov )
	{
		if( frame->relay )
			SNAP_RelayMultiPOVCommands( gi, client, msg, numcmds, commands, commandsData );
		else
			SNAP_WriteMultiPOVCommands( gi, client, msg, frameNum );
	}
	else
	{
		for( i = client->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1; i <= client->gameCommandCurrent; i++ )
		{
			index = i & ( MAX_RELIABLE_COMMANDS - 1 );

			// check that it is valid command and that has not already been sent
			// we can only allow commands from certain amount of old frames, so the short won't overflow
			if( !client->gameCommands[index].command[0] || client->gameCommands[index].framenum + 256 < frameNum ||
				client->gameCommands[index].framenum > frameNum ||
				( client->lastframe >= 0 && client->gameCommands[index].framenum <= (unsigned)client->lastframe ) )
				continue;

			// do not allow the message buffer to overflow (can happen on flood updates)
			if( msg->cursize + strlen( client->gameCommands[index].command ) + 512 > msg->maxsize )
				continue;

			// send it
			MSG_WriteShort( msg, frameNum - client->gameCommands[index].framenum );
			MSG_WriteString( msg, client->gameCommands[index].command );
		}
	}
	MSG_WriteShort( msg, -1 );

	// send over the areabits
	MSG_WriteByte( msg, frame->areabytes );
	MSG_WriteData( msg, frame->areabits, frame->areabytes );

	SNAP_WriteDeltaGameStateToClient( oldframe, frame, msg );

	// delta encode the playerstate
	for( i = 0; i < frame->numplayers; i++ )
	{
		if( oldframe && oldframe->numplayers > i )
			SNAP_WritePlayerstateToClient( &oldframe->ps[i], &frame->ps[i], msg );
		else
			SNAP_WritePlayerstateToClient( NULL, &frame->ps[i], msg );
	}
	MSG_WriteByte( msg, 0 );

	// delta encode the entities
	SNAP_EmitPacketEntities( gi, oldframe, frame, msg, baselines, client_entities ? client_entities->entities : NULL, client_entities ? client_entities->num_entities : 0 );

	// write length into reserved space
	length = msg->cursize - pos - 2;
	msg->cursize = pos;
	MSG_WriteShort( msg, length );
	msg->cursize += length;

	client->lastSentFrameNum = frameNum;
}

/*
=============================================================================

Build a client frame structure

=============================================================================
*/

/*
* SNAP_FatPVS
*
* The client will interpolate the view position,
* so we can't use a single PVS point
*/
static void SNAP_FatPVS( cmodel_state_t *cms, vec3_t org, qbyte *fatpvs )
{
	memset( fatpvs, 0, CM_ClusterRowSize( cms ) );
	CM_MergePVS( cms, org, fatpvs );
}

/*
* SNAP_BitsCullEntity
*/
static qboolean SNAP_BitsCullEntity( cmodel_state_t *cms, edict_t *ent, qbyte *bits, int max_clusters )
{
	int i, l;

	// too many leafs for individual check, go by headnode
	if( ent->r.num_clusters == -1 )
	{
		if( !CM_HeadnodeVisible( cms, ent->r.headnode, bits ) )
			return qtrue;
		return qfalse;
	}

	// check individual leafs
	for( i = 0; i < max_clusters; i++ )
	{
		l = ent->r.clusternums[i];
		if( bits[l >> 3] & ( 1 << ( l&7 ) ) )
			return qfalse;
	}

	return qtrue;	// not visible/audible
}

#define SNAP_PVSCullEntity(cms,fatpvs,ent) SNAP_BitsCullEntity(cms,ent,fatpvs,ent->r.num_clusters)

//=====================================================================

#define	MAX_SNAPSHOT_ENTITIES	1024
typedef struct
{
	int numSnapshotEntities;
	int snapshotEntities[MAX_SNAPSHOT_ENTITIES];
	int entityAddedToSnapList[MAX_EDICTS];
} snapshotEntityNumbers_t;

/*
* SNAP_AddEntNumToSnapList
*/
static void SNAP_AddEntNumToSnapList( int entNum, snapshotEntityNumbers_t *entsList )
{
	if( entsList->numSnapshotEntities >= MAX_SNAPSHOT_ENTITIES )  // silent ignore of overflood
		return;

	// don't double add entities
	if( entsList->entityAddedToSnapList[entNum] )
		return;

	entsList->snapshotEntities[entsList->numSnapshotEntities++] = entNum;
	entsList->entityAddedToSnapList[entNum] = qtrue;
}

/*
* SNAP_SortSnapList
*/
static void SNAP_SortSnapList( snapshotEntityNumbers_t *entsList )
{
	int i;

	// avoid adding world to the list by all costs
	entsList->numSnapshotEntities = 0;
	for( i = 1; i < MAX_EDICTS; i++ )
	{
		if( entsList->entityAddedToSnapList[i] == qtrue )
			entsList->snapshotEntities[entsList->numSnapshotEntities++] = i;
	}
}

/*
* SNAP_GainForAttenuation
*/
static float SNAP_GainForAttenuation( float dist, float attenuation )
{
	int model = S_DEFAULT_ATTENUATION_MODEL;
	float maxdistance = S_DEFAULT_ATTENUATION_MAXDISTANCE;
	float refdistance = S_DEFAULT_ATTENUATION_REFDISTANCE;

#if !defined(PUBLIC_BUILD) && !defined(DEDICATED_ONLY) && !defined(TV_SERVER_ONLY)
#define DUMMY_CVAR ( cvar_t * )((void *)1)
	static cvar_t *s_attenuation_model = DUMMY_CVAR;
	static cvar_t *s_attenuation_maxdistance = DUMMY_CVAR;
	static cvar_t *s_attenuation_refdistance = DUMMY_CVAR;

	if( s_attenuation_model == DUMMY_CVAR )
		s_attenuation_model = Cvar_Find( "s_attenuation_model" );
	if( s_attenuation_maxdistance == DUMMY_CVAR )
		s_attenuation_maxdistance = Cvar_Find( "s_attenuation_maxdistance" );
	if( s_attenuation_refdistance == DUMMY_CVAR )
		s_attenuation_refdistance = Cvar_Find( "s_attenuation_refdistance" );

	if( s_attenuation_model && s_attenuation_model != DUMMY_CVAR )
		model = s_attenuation_model->integer;
	if( s_attenuation_maxdistance && s_attenuation_maxdistance != DUMMY_CVAR )
		maxdistance = s_attenuation_maxdistance->value;
	if( s_attenuation_refdistance && s_attenuation_refdistance != DUMMY_CVAR )
		refdistance = s_attenuation_refdistance->value;
#undef DUMMY_CVAR
#endif

	return Q_GainForAttenuation( model, maxdistance, refdistance, dist, attenuation );
}

/*
* SNAP_SnapCullSoundEntity
*/
static qboolean SNAP_SnapCullSoundEntity( cmodel_state_t *cms, edict_t *ent, vec3_t listener_origin, float attenuation )
{
	float gain, dist;

	if( !attenuation )
		return qfalse;

	dist = DistanceFast( ent->s.origin, listener_origin ) - 256; // extend the influence sphere cause the player could be moving
	gain = SNAP_GainForAttenuation( dist < 0 ? 0 : dist, attenuation );
	if( gain > 0.03 )  // curved attenuations can keep barely audible sounds for long distances
		return qfalse;

	return qtrue;
}

/*
* SNAP_SnapCullEntity
*/
static qboolean SNAP_SnapCullEntity( cmodel_state_t *cms, edict_t *ent, edict_t *clent, client_snapshot_t *frame, vec3_t vieworg, qbyte *fatpvs )
{
	qbyte *areabits;

	// filters: this entity has been disabled for comunication
	if( ent->r.svflags & SVF_NOCLIENT )
		return qtrue;

	// send all entities
	if( frame->allentities )
		return qfalse;

	// filters: transmit only to clients in the same team as this entity
	// broadcasting is less important than team specifics
	if( ( ent->r.svflags & SVF_ONLYTEAM ) && ( clent && ent->s.team != clent->s.team ) )
		return qtrue;

	// send only to owner
	if( ( ent->r.svflags & SVF_ONLYOWNER ) && ( clent && ent->s.ownerNum != clent->s.number ) )
		return qtrue;

	if( ent->r.svflags & SVF_BROADCAST )  // send to everyone
		return qfalse;

	if( ent->r.areanum < 0 )
		return qtrue;
	if( frame->clientarea >= 0 )
	{
		// this is the same as CM_AreasConnected but portal's visibility included
		areabits = frame->areabits + frame->clientarea * CM_AreaRowSize( cms );
		if( !( areabits[ent->r.areanum>>3] & ( 1<<( ent->r.areanum&7 ) ) ) )
		{
			// doors can legally straddle two areas, so we may need to check another one
			if( ent->r.areanum2 < 0 || !( areabits[ent->r.areanum2>>3] & ( 1<<( ent->r.areanum2&7 ) ) ) )
				return qtrue; // blocked by a door
		}
	}

	// sound entities culling
	if( ent->r.svflags & SVF_SOUNDCULL )
		return SNAP_SnapCullSoundEntity( cms, ent, vieworg, (float)(ent->s.attenuation / 16.0f) );

	// if not a sound entity but the entity is only a sound
	if( !ent->s.modelindex && !ent->s.events[0] && !ent->s.light && !ent->s.effects && ent->s.sound )
	{
#define	ATTN_STATIC		5 // FIXME!
		return SNAP_SnapCullSoundEntity( cms, ent, vieworg, ATTN_STATIC );
#undef ATTN_STATIC
	}

	return SNAP_PVSCullEntity( cms, fatpvs, ent );			// cull by PVS
}

/*
* SNAP_BuildSnapEntitiesList
*/
static void SNAP_BuildSnapEntitiesList( cmodel_state_t *cms, ginfo_t *gi, edict_t *clent, vec3_t vieworg, vec3_t skyorg, qbyte *fatpvs, client_snapshot_t *frame, snapshotEntityNumbers_t *entsList )
{
	int leafnum = -1, clusternum = -1, clientarea = -1;
	int entNum;
	edict_t	*ent;

	// find the client's PVS
	if( frame->allentities )
	{
		clientarea = -1;
	}
	else
	{
		leafnum = CM_PointLeafnum( cms, vieworg );
		clusternum = CM_LeafCluster( cms, leafnum );
		clientarea = CM_LeafArea( cms, leafnum );
	}

	frame->clientarea = clientarea;
	frame->areabytes = CM_WriteAreaBits( cms, frame->areabits );

	if( clent )
	{
		SNAP_FatPVS( cms, vieworg, fatpvs );

		// if the client is outside of the world, don't send him any entity (excepting himself)
		if( !frame->allentities && clusternum == -1 )
		{
			entNum = NUM_FOR_EDICT( clent );
			if( clent->s.number != entNum )
			{
				Com_Printf( "FIXING CLENT->S.NUMBER: %i %i!!!\n", clent->s.number, entNum );
				clent->s.number = entNum;
			}

			// FIXME we should send all the entities who's POV we are sending if frame->multipov
			SNAP_AddEntNumToSnapList( entNum, entsList );
			return;
		}
	}

	// no need of merging when we are sending the whole level
	if( !frame->allentities && clientarea >= 0 )
	{
		// make a pass checking for sky portal and portal entities and merge PVS in case of finding any
		if( skyorg )
			CM_MergeVisSets( cms, skyorg, fatpvs, frame->areabits + clientarea * CM_AreaRowSize( cms ) );

		for( entNum = 1; entNum < gi->num_edicts; entNum++ )
		{
			ent = EDICT_NUM( entNum );
			if( ent->r.svflags & SVF_PORTAL )
			{
				// merge visibility sets if portal
				if( SNAP_SnapCullEntity( cms, ent, clent, frame, vieworg, fatpvs ) )
					continue;

				if( !VectorCompare( ent->s.origin, ent->s.origin2 ) )
					CM_MergeVisSets( cms, ent->s.origin2, fatpvs, frame->areabits + clientarea * CM_AreaRowSize( cms ) );
			}
		}
	}

	// add the entities to the list
	for( entNum = 1; entNum < gi->num_edicts; entNum++ )
	{
		ent = EDICT_NUM( entNum );

		// fix number if broken
		if( ent->s.number != entNum )
		{
			Com_Printf( "FIXING ENT->S.NUMBER: %i %i!!!\n", ent->s.number, entNum );
			ent->s.number = entNum;
		}

		// always add the client entity, even if SVF_NOCLIENT
		if( ( ent != clent ) && SNAP_SnapCullEntity( cms, ent, clent, frame, vieworg, fatpvs ) )
			continue;

		// add it
		SNAP_AddEntNumToSnapList( entNum, entsList );

		if( ent->r.svflags & SVF_FORCEOWNER )
		{
			// make sure owner number is valid too
			if( ent->s.ownerNum > 0 && ent->s.ownerNum < gi->num_edicts )
			{
				SNAP_AddEntNumToSnapList( ent->s.ownerNum, entsList );
			}
			else
			{
				Com_Printf( "FIXING ENT->S.OWNERNUM: %i %i!!!\n", ent->s.type, ent->s.ownerNum );
				ent->s.ownerNum = 0;
			}
		}
	}

	SNAP_SortSnapList( entsList );
}

/*
* SNAP_BuildClientFrameSnap
*
* Decides which entities are going to be visible to the client, and
* copies off the playerstat and areabits.
*/
void SNAP_BuildClientFrameSnap( cmodel_state_t *cms, ginfo_t *gi, unsigned int frameNum, unsigned int timeStamp,
							   fatvis_t *fatvis, client_t *client,
							   game_state_t *gameState, client_entities_t *client_entities,
							   qboolean relay, mempool_t *mempool )
{
	int e, i, ne;
	vec3_t org;
	edict_t	*ent, *clent;
	client_snapshot_t *frame;
	entity_state_t *state;
	int numplayers, numareas;
	snapshotEntityNumbers_t entsList;

	assert( gameState );

	clent = client->edict;
	if( clent && !clent->r.client )		// allow NULL ent for server record
		return;		// not in game yet

	if( clent )
	{
		VectorCopy( clent->s.origin, org );
		org[2] += clent->r.client->ps.viewheight;
	}
	else
	{
		assert( client->mv );
		VectorClear( org );
	}

	// this is the frame we are creating
	frame = &client->snapShots[frameNum & UPDATE_MASK];
	frame->sentTimeStamp = timeStamp;
	frame->UcmdExecuted = client->UcmdExecuted;
	frame->relay = relay;

	if( client->mv )
	{
		frame->multipov = qtrue;
		frame->allentities = qtrue;
	}
	else
	{
		frame->multipov = qfalse;
		frame->allentities = qfalse;
	}

	// areaportals matrix
	numareas = CM_NumAreas( cms );
	if( frame->numareas < numareas )
	{
		frame->numareas = numareas;

		numareas *= CM_AreaRowSize( cms );
		if( frame->areabits )
		{
			Mem_Free( frame->areabits );
			frame->areabits = NULL;
		}
		frame->areabits = (qbyte*)Mem_Alloc( mempool, numareas );
	}

	// grab the current player_state_t
	if( frame->multipov )
	{
		frame->numplayers = 0;
		for( i = 0; i < gi->max_clients; i++ )
		{
			ent = EDICT_NUM( i+1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->r.svflags & SVF_NOCLIENT ) ) )
				frame->numplayers++;
		}
	}
	else
	{
		frame->numplayers = 1;
	}

	if( frame->ps_size < frame->numplayers )
	{
		if( frame->ps )
		{
			Mem_Free( frame->ps );
			frame->ps = NULL;
		}

		frame->ps = ( player_state_t* )Mem_Alloc( mempool, sizeof( player_state_t )*frame->numplayers );
		frame->ps_size = frame->numplayers;
	}

	if( frame->multipov )
	{
		numplayers = 0;
		for( i = 0; i < gi->max_clients; i++ )
		{
			ent = EDICT_NUM( i+1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->r.svflags & SVF_NOCLIENT ) ) )
			{
				frame->ps[numplayers] = ent->r.client->ps;
				frame->ps[numplayers].playerNum = i;
				numplayers++;
			}
		}
	}
	else
	{
		frame->ps[0] = clent->r.client->ps;
		frame->ps[0].playerNum = NUM_FOR_EDICT( clent ) - 1;
	}

	// build up the list of visible entities
	//=============================
	entsList.numSnapshotEntities = 0;
	memset( entsList.entityAddedToSnapList, 0, sizeof( entsList.entityAddedToSnapList ) );
	SNAP_BuildSnapEntitiesList( cms, gi, clent, org, fatvis->skyorg, fatvis->pvs, frame, &entsList );

	//Com_Printf( "Snap NumEntities:%i\n", entsList.numSnapshotEntities );

	if( developer->integer )
	{
		int olde = -1;
		for( e = 0; e < entsList.numSnapshotEntities; e++ )
		{
			if( olde >= entsList.snapshotEntities[e] )
				Com_Printf( "WARNING 'SV_BuildClientFrameSnap': Unsorted entities list\n" );
			olde = entsList.snapshotEntities[e];
		}
	}

	// store current match state information
	frame->gameState = *gameState;

	//=============================

	// dump the entities list
	ne = client_entities->next_entities;
	frame->num_entities = 0;
	frame->first_entity = ne;

	for( e = 0; e < entsList.numSnapshotEntities; e++ )
	{
		// add it to the circular client_entities array
		ent = EDICT_NUM( entsList.snapshotEntities[e] );
		state = &client_entities->entities[ne%client_entities->num_entities];

		*state = ent->s;
		state->svflags = ent->r.svflags;

		// don't mark *any* missiles as solid
		if( ent->r.svflags & SVF_PROJECTILE )
			state->solid = 0;

		frame->num_entities++;
		ne++;
	}

	client_entities->next_entities = ne;
}

/*
* SNAP_FreeClientFrame
*
* Free structs and arrays we allocated in SNAP_BuildClientFrameSnap
*/
static void SNAP_FreeClientFrame( client_snapshot_t *frame )
{
	if( frame->areabits )
	{
		Mem_Free( frame->areabits );
		frame->areabits = NULL;
	}
	frame->numareas = 0;

	if( frame->ps )
	{
		Mem_Free( frame->ps );
		frame->ps = NULL;
	}
	frame->ps_size = 0;
}

/*
* SNAP_FreeClientFrames
*
*/
void SNAP_FreeClientFrames( client_t *client )
{
	int i;
	client_snapshot_t *frame;

	for( i = 0; i < UPDATE_BACKUP; i++ )
	{
		frame = &client->snapShots[i];
		SNAP_FreeClientFrame( frame );
	}
}
