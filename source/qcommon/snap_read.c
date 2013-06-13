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

#include "snap_read.h"

/*
=========================================================================

UTILITY FUNCTIONS

=========================================================================
*/

const char * const svc_strings[256] =
{
	"svc_bad",
	"svc_nop",
	"svc_servercmd",
	"svc_serverdata",
	"svc_spawnbaseline",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_gamecommands",
	"svc_match",
	"svc_clcack",
	"svc_servercs", // reliable command as unreliable for demos
	"svc_frame",
	"svc_demoinfo",
	"svc_extension"
};

void _SHOWNET( msg_t *msg, const char *s, int shownet )
{
	if( shownet >= 2 )
		Com_Printf( "%3i:%s\n", msg->readcount-1, s );
}

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
* SNAP_ParseDeltaGameState
*/
static void SNAP_ParseDeltaGameState( msg_t *msg, snapshot_t *oldframe, snapshot_t *newframe )
{
	short statbits;
	qbyte bits;
	int i;
	game_state_t *gameState;

	// start from old state or 0 if none
	gameState = &newframe->gameState;
	if( oldframe )
		*gameState = oldframe->gameState;
	else
		memset( gameState, 0, sizeof( game_state_t ) );

	assert( MAX_GAME_STATS == 16 );
	assert( MAX_GAME_LONGSTATS == 8 );

	//memcpy( gameState, deltaGameState, sizeof( game_state_t ) );

	bits = (qbyte)MSG_ReadByte( msg );
	statbits = MSG_ReadShort( msg );

	if( bits )
	{
		for( i = 0; i < MAX_GAME_LONGSTATS; i++ )
		{
			if( bits & ( 1<<i ) )
				gameState->longstats[i] = (unsigned int)MSG_ReadLong( msg );
		}
	}

	if( statbits )
	{
		for( i = 0; i < MAX_GAME_STATS; i++ )
		{
			if( statbits & ( 1<<i ) )
				gameState->stats[i] = MSG_ReadShort( msg );
		}
	}
}

/*
* SNAP_ParsePlayerstate
*/
static void SNAP_ParsePlayerstate( msg_t *msg, player_state_t *oldstate, player_state_t *state )
{
	int flags;
	int i, b;
	int statbits[SNAP_STATS_LONGS];

	// clear to old value before delta parsing
	if( oldstate )
		memcpy( state, oldstate, sizeof( *state ) );
	else
		memset( state, 0, sizeof( *state ) );

	flags = (qbyte)MSG_ReadByte( msg );
	if( flags & PS_MOREBITS1 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		flags |= b<<8;
	}
	if( flags & PS_MOREBITS2 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		flags |= b<<16;
	}
	if( flags & PS_MOREBITS3 )
	{
		b = (qbyte)MSG_ReadByte( msg );
		flags |= b<<24;
	}

	//
	// parse the pmove_state_t
	//
	if( flags & PS_M_TYPE )
		state->pmove.pm_type = (qbyte)MSG_ReadByte( msg );

	if( flags & PS_M_ORIGIN0 )
		state->pmove.origin[0] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );
	if( flags & PS_M_ORIGIN1 )
		state->pmove.origin[1] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );
	if( flags & PS_M_ORIGIN2 )
		state->pmove.origin[2] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );

	if( flags & PS_M_VELOCITY0 )
		state->pmove.velocity[0] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );
	if( flags & PS_M_VELOCITY1 )
		state->pmove.velocity[1] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );
	if( flags & PS_M_VELOCITY2 )
		state->pmove.velocity[2] = ( (float)MSG_ReadInt3( msg )*( 1.0/PM_VECTOR_SNAP ) );

	if( flags & PS_M_TIME )
		state->pmove.pm_time = (qbyte)MSG_ReadByte( msg );

	if( flags & PS_M_FLAGS )
		state->pmove.pm_flags = MSG_ReadShort( msg );

	if( flags & PS_M_DELTA_ANGLES0 )
		state->pmove.delta_angles[0] = MSG_ReadShort( msg );
	if( flags & PS_M_DELTA_ANGLES1 )
		state->pmove.delta_angles[1] = MSG_ReadShort( msg );
	if( flags & PS_M_DELTA_ANGLES2 )
		state->pmove.delta_angles[2] = MSG_ReadShort( msg );

	if( flags & PS_EVENT )
	{
		state->event[0] = MSG_ReadByte( msg );
		if( state->event[0] & EV_INVERSE )
			state->eventParm[0] = MSG_ReadByte( msg );
		else
			state->eventParm[0] = 0;

		state->event[0] &= ~EV_INVERSE;
	}
	else
	{
		state->event[0] = state->eventParm[0] = 0;
	}

	if( flags & PS_EVENT2 )
	{
		state->event[1] = MSG_ReadByte( msg );
		if( state->event[1] & EV_INVERSE )
			state->eventParm[1] = MSG_ReadByte( msg );
		else
			state->eventParm[1] = 0;

		state->event[1] &= ~EV_INVERSE;
	}
	else
	{
		state->event[1] = state->eventParm[1] = 0;
	}

	if( flags & PS_VIEWANGLES )
	{
		state->viewangles[0] = MSG_ReadAngle16( msg );
		state->viewangles[1] = MSG_ReadAngle16( msg );
		state->viewangles[2] = MSG_ReadAngle16( msg );
	}

	if( flags & PS_M_GRAVITY )
		state->pmove.gravity = MSG_ReadShort( msg );

	if( flags & PS_WEAPONSTATE )
		state->weaponState = (qbyte)MSG_ReadByte( msg );

	if( flags & PS_FOV )
		state->fov = (qbyte)MSG_ReadByte( msg );

	if( flags & PS_POVNUM )
		state->POVnum = (qbyte)MSG_ReadByte( msg );
	if( state->POVnum == 0 )
		Com_Error( ERR_DROP, "SNAP_ParsePlayerstate: Invalid POVnum %i", state->POVnum );

	if( flags & PS_PLAYERNUM )
		state->playerNum = (qbyte)MSG_ReadByte( msg );
	if( state->playerNum >= MAX_CLIENTS )
		Com_Error( ERR_DROP, "SNAP_ParsePlayerstate: Invalid playerNum %i", state->playerNum );

	if( flags & PS_VIEWHEIGHT )
		state->viewheight = MSG_ReadChar( msg );

	if( flags & PS_PMOVESTATS )
	{
		int pmstatbits = MSG_ReadShort( msg );
		for( i = 0; i < PM_STAT_SIZE; i++ )
		{
			if( pmstatbits & ( 1<<i ) )
				state->pmove.stats[i] = MSG_ReadShort( msg );
		}
	}

	if( flags & PS_INVENTORY )
	{
		int invstatbits[SNAP_INVENTORY_LONGS];

		// parse inventory
		for( i = 0; i < SNAP_INVENTORY_LONGS; i++ ) {
			invstatbits[i] = MSG_ReadLong( msg );
		}

		for( i = 0; i < MAX_ITEMS; i++ )
		{
			if( invstatbits[i>>5] & ( 1<<(i&31) ) )
				state->inventory[i] = MSG_ReadByte( msg );
		}
	}

	if( flags & PS_PLRKEYS )
		state->plrkeys = MSG_ReadByte( msg );

	// parse stats
	for( i = 0; i < SNAP_STATS_LONGS; i++ ) {
		statbits[i] = MSG_ReadLong( msg );
	}

	for( i = 0; i < PS_MAX_STATS; i++ )
	{
		if( statbits[i>>5] & ( 1<<(i&31) ) )
			state->stats[i] = MSG_ReadShort( msg );
	}
}

/*
* SNAP_ParseEntityBits
*/
static int SNAP_ParseEntityBits( msg_t *msg, unsigned *bits )
{
	return MSG_ReadEntityBits( msg, bits );
}

/*
* SNAP_DeltaEntity
*
* Parses deltas from the given base and adds the resulting entity
* to the current frame
*/
static void SNAP_DeltaEntity( msg_t *msg, snapshot_t *frame, int newnum, entity_state_t *old, unsigned bits )
{
	entity_state_t *state;

	state = &frame->parsedEntities[frame->numEntities & ( MAX_PARSE_ENTITIES-1 )];
	frame->numEntities++;
	MSG_ReadDeltaEntity( msg, old, state, newnum, bits );
}

/*
* SNAP_ParseBaseline
*/
void SNAP_ParseBaseline( msg_t *msg, entity_state_t *baselines )
{
	entity_state_t *es;
	unsigned bits;
	int newnum;
	entity_state_t nullstate, tmp;

	memset( &nullstate, 0, sizeof( nullstate ) );
	newnum = MSG_ReadEntityBits( msg, &bits );

	es = (baselines ? &baselines[newnum] : &tmp);
	MSG_ReadDeltaEntity( msg, &nullstate, es, newnum, bits );
}

/*
* SNAP_ParsePacketEntities
*
* An svc_packetentities has just been parsed, deal with the
* rest of the data stream.
*/
static void SNAP_ParsePacketEntities( msg_t *msg, snapshot_t *oldframe, snapshot_t *newframe, entity_state_t *baselines, int shownet )
{
	int newnum;
	unsigned bits;
	entity_state_t *oldstate = NULL;
	int oldindex, oldnum;

	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	if( !oldframe )
		oldnum = 99999;
	else if( oldindex >= oldframe->numEntities )
	{
		oldnum = 99999;
	}
	else
	{
		oldstate = &oldframe->parsedEntities[oldindex & ( MAX_PARSE_ENTITIES-1 )];
		oldnum = oldstate->number;
	}

	while( qtrue )
	{
		newnum = SNAP_ParseEntityBits( msg, &bits );
		if( newnum >= MAX_EDICTS )
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newnum );
		if( msg->readcount > msg->cursize )
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: end of message" );

		if( !newnum )
			break;

		while( oldnum < newnum )
		{
			// one or more entities from the old packet are unchanged
			if( shownet == 3 )
				Com_Printf( "   unchanged: %i\n", oldnum );

			SNAP_DeltaEntity( msg, newframe, oldnum, oldstate, 0 );

			oldindex++;
			if( oldindex >= oldframe->numEntities )
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &oldframe->parsedEntities[oldindex & ( MAX_PARSE_ENTITIES-1 )];
				oldnum = oldstate->number;
			}
		}

		// delta from baseline
		if( oldnum > newnum )
		{
			if( bits & U_REMOVE )
			{
				Com_Printf( "U_REMOVE: oldnum > newnum (can't remove from baseline!)\n" );
				continue;
			}

			// delta from baseline
			if( shownet == 3 )
				Com_Printf( "   baseline: %i\n", newnum );

			SNAP_DeltaEntity( msg, newframe, newnum, &baselines[newnum], bits );
			continue;
		}

		if( oldnum == newnum )
		{
			if( bits & U_REMOVE )
			{
				// the entity present in oldframe is not in the current frame
				if( shownet == 3 )
					Com_Printf( "   remove: %i\n", newnum );

				if( oldnum != newnum )
					Com_Printf( "U_REMOVE: oldnum != newnum\n" );

				oldindex++;
				if( oldindex >= oldframe->numEntities )
				{
					oldnum = 99999;
				}
				else
				{
					oldstate = &oldframe->parsedEntities[oldindex & ( MAX_PARSE_ENTITIES-1 )];
					oldnum = oldstate->number;
				}
				continue;
			}

			// delta from previous state
			if( shownet == 3 )
				Com_Printf( "   delta: %i\n", newnum );

			SNAP_DeltaEntity( msg, newframe, newnum, oldstate, bits );

			oldindex++;
			if( oldindex >= oldframe->numEntities )
			{
				oldnum = 99999;
			}
			else
			{
				oldstate = &oldframe->parsedEntities[oldindex & ( MAX_PARSE_ENTITIES-1 )];
				oldnum = oldstate->number;
			}
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while( oldnum != 99999 )
	{
		// one or more entities from the old packet are unchanged
		if( shownet == 3 )
			Com_Printf( "   unchanged: %i\n", oldnum );

		SNAP_DeltaEntity( msg, newframe, oldnum, oldstate, 0 );

		oldindex++;
		if( oldindex >= oldframe->numEntities )
		{
			oldnum = 99999;
		}
		else
		{
			oldstate = &oldframe->parsedEntities[oldindex & ( MAX_PARSE_ENTITIES-1 )];
			oldnum = oldstate->number;
		}
	}
}

/*
* SNAP_ParseFrameHeader
*/
static snapshot_t *SNAP_ParseFrameHeader( msg_t *msg, snapshot_t *newframe, int *suppressCount, snapshot_t *backup, qboolean skipBody )
{
	int len, pos;
	int areabytes;
	qbyte *areabits;
	unsigned int serverTime;
	int flags, snapNum, supCnt;

	// get total length
	len = MSG_ReadShort( msg );
	pos = msg->readcount;

	// get the snapshot id
	serverTime = (unsigned)MSG_ReadLong( msg );
	snapNum = MSG_ReadLong( msg );

	if( backup )
		newframe = &backup[snapNum & UPDATE_MASK];

	areabytes = newframe->areabytes;
	areabits = newframe->areabits;
	memset( newframe, 0, sizeof( snapshot_t ) );
	newframe->areabytes = areabytes;
	newframe->areabits = areabits;

	newframe->serverTime = serverTime;
	newframe->serverFrame = snapNum;
	newframe->deltaFrameNum = MSG_ReadLong( msg );
	newframe->ucmdExecuted = MSG_ReadLong( msg );

	flags = MSG_ReadByte( msg );
	newframe->delta = ( flags & FRAMESNAP_FLAG_DELTA ) ? qtrue : qfalse;
	newframe->multipov = ( flags & FRAMESNAP_FLAG_MULTIPOV ) ? qtrue : qfalse;
	newframe->allentities = ( flags & FRAMESNAP_FLAG_ALLENTITIES ) ? qtrue : qfalse;

	supCnt = MSG_ReadByte( msg );
	if( suppressCount )
	{
		*suppressCount = supCnt;
#ifdef RATEKILLED
		*suppressCount = 0;
#endif
	}

	// validate the new frame
	newframe->valid = qfalse;

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if( !newframe->delta )
	{
		newframe->valid = qtrue; // uncompressed frame
	}
	else
	{
		if( newframe->deltaFrameNum <= 0 )
		{
			Com_Printf( "Invalid delta frame (not supposed to happen!).\n" );
		}
		else if( backup )
		{
			snapshot_t *deltaframe = &backup[newframe->deltaFrameNum & UPDATE_MASK];
			if( !deltaframe->valid )
			{
				// should never happen
				Com_Printf( "Delta from invalid frame (not supposed to happen!).\n" );
			}
			else if( deltaframe->serverFrame != newframe->deltaFrameNum )
			{
				// The frame that the server did the delta from
				// is too old, so we can't reconstruct it properly.
				Com_Printf( "Delta frame too old.\n" );
			}
			else
			{
				newframe->valid = qtrue; // valid delta parse
			}
		}
		else
		{
			newframe->valid = skipBody;
		}
	}

	if( skipBody )
		MSG_SkipData( msg, len - (msg->readcount - pos) );

	return newframe;
}

/*
* SNAP_SkipFrame
*/
void SNAP_SkipFrame( msg_t *msg, snapshot_t *header )
{
	static snapshot_t frame;
	SNAP_ParseFrameHeader( msg, header ? header : &frame, NULL, NULL, qtrue );
}

/*
* SNAP_ParseFrame
*/
snapshot_t *SNAP_ParseFrame( msg_t *msg, snapshot_t *lastFrame, int *suppressCount, snapshot_t *backup, entity_state_t *baselines, int showNet )
{
	int cmd;
	size_t len;
	snapshot_t	*deltaframe;
	int numplayers;
	char *text;
	int framediff, numtargets;
	gcommand_t *gcmd;
	snapshot_t	*newframe;

	// read header
	newframe = SNAP_ParseFrameHeader( msg, NULL, suppressCount, backup, qfalse );
	deltaframe = NULL;

	if( showNet == 3 )
	{
		Com_Printf( "   frame:%i  old:%i%s\n", newframe->serverFrame, newframe->deltaFrameNum,
			( newframe->delta ? "" : " no delta" ) );
	}

	if( newframe->delta )
	{
		if( newframe->deltaFrameNum > 0 )
			deltaframe = &backup[newframe->deltaFrameNum & UPDATE_MASK];
	}

	// read game commands
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_gamecommands )
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not gamecommands" );

	numtargets = 0;
	while( ( framediff = MSG_ReadShort( msg ) ) != -1 )
	{
		text = MSG_ReadString( msg );

		// see if it's valid and not yet handled
		if( newframe->valid &&
			( !lastFrame || !lastFrame->valid || newframe->serverFrame > lastFrame->serverFrame + framediff ) )
		{
			newframe->numgamecommands++;
			if( newframe->numgamecommands > MAX_PARSE_GAMECOMMANDS )
				Com_Error( ERR_DROP, "SNAP_ParseFrame: too many gamecommands" );
			if( newframe->gamecommandsDataHead + strlen( text ) >= sizeof( newframe->gamecommandsData ) )
				Com_Error( ERR_DROP, "SNAP_ParseFrame: too much gamecommands" );

			gcmd = &newframe->gamecommands[newframe->numgamecommands - 1];
			gcmd->all = qtrue;

			Q_strncpyz( newframe->gamecommandsData + newframe->gamecommandsDataHead, text,
				sizeof( newframe->gamecommandsData ) - newframe->gamecommandsDataHead );
			gcmd->commandOffset = newframe->gamecommandsDataHead;
			newframe->gamecommandsDataHead += strlen( text ) + 1;

			if( newframe->multipov )
			{
				numtargets = MSG_ReadByte( msg );
				if( numtargets )
				{
					gcmd->all = qfalse;
					MSG_ReadData( msg, gcmd->targets, numtargets );
				}
			}
		}
		else if( newframe->multipov ) // otherwise, ignore it
		{
			numtargets = MSG_ReadByte( msg );
			MSG_SkipData( msg, numtargets );
		}
	}

	// read areabits
	len = (size_t)MSG_ReadByte( msg );
	if( len > newframe->areabytes )
		Com_Error( ERR_DROP, "Invalid areabits size: %u > %u", len, newframe->areabytes );
	memset( newframe->areabits, 0, newframe->areabytes );
	MSG_ReadData( msg, newframe->areabits, len );

	// read match info
	cmd = MSG_ReadByte( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_match )
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not match info" );
	SNAP_ParseDeltaGameState( msg, deltaframe, newframe );

	// read playerinfos
	numplayers = 0;
	while( ( cmd = MSG_ReadByte( msg ) ) )
	{
		_SHOWNET( msg, svc_strings[cmd], showNet );
		if( cmd != svc_playerinfo )
			Com_Error( ERR_DROP, "SNAP_ParseFrame: not playerinfo" );
		if( deltaframe && deltaframe->numplayers >= numplayers )
			SNAP_ParsePlayerstate( msg, &deltaframe->playerStates[numplayers], &newframe->playerStates[numplayers] );
		else
			SNAP_ParsePlayerstate( msg, NULL, &newframe->playerStates[numplayers] );
		numplayers++;
	}
	newframe->numplayers = numplayers;
	newframe->playerState = newframe->playerStates[0];

	// read packet entities
	cmd = MSG_ReadByte( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_packetentities )
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not packetentities" );
	SNAP_ParsePacketEntities( msg, deltaframe, newframe, baselines, showNet );

	return newframe;
}
