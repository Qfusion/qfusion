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

void _SHOWNET( msg_t *msg, const char *s, int shownet ) {
	if( shownet >= 2 ) {
		Com_Printf( "%3i:%s\n", (int)(msg->readcount - 1), s );
	}
}

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
* SNAP_ParseDeltaGameState
*/
static void SNAP_ParseDeltaGameState( msg_t *msg, snapshot_t *oldframe, snapshot_t *newframe ) {
	MSG_ReadDeltaGameState( msg, oldframe ? &oldframe->gameState : NULL, &newframe->gameState );
}

/*
* SNAP_ParsePlayerstate
*/
static void SNAP_ParsePlayerstate( msg_t *msg, const player_state_t *oldstate, player_state_t *state ) {
	MSG_ReadDeltaPlayerState( msg, oldstate, state );
}

/*
* SNAP_ParseDeltaEntity
*
* Parses deltas from the given base and adds the resulting entity to the current frame
*/
static void SNAP_ParseDeltaEntity( msg_t *msg, snapshot_t *frame, int newnum, entity_state_t *old, unsigned byteMask ) {
	entity_state_t *state;

	state = &frame->parseEntities[(frame->firstEntity + frame->numEntities) & ( MAX_PARSE_ENTITIES - 1 )];
	frame->numEntities++;
	MSG_ReadDeltaEntity( msg, old, state, newnum, byteMask );
}

/*
* SNAP_ParseBaseline
*/
void SNAP_ParseBaseline( msg_t *msg, entity_state_t *baselines ) {
	bool remove;
	int newnum;
	unsigned byteMask;

	newnum = MSG_ReadEntityNumber( msg, &remove, &byteMask );
	assert( remove == false );

	if( !remove ) {
		entity_state_t *es;
		entity_state_t nullstate, tmp;

		memset( &nullstate, 0, sizeof( nullstate ) );

		es = ( baselines ? &baselines[newnum] : &tmp );
		MSG_ReadDeltaEntity( msg, &nullstate, es, newnum, byteMask );
	}
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
	bool remove;
	unsigned byteMask;
	entity_state_t *oldstate = NULL;
	int oldindex, oldnum;

	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	if( !oldframe ) {
		oldnum = 99999;
	} else if( oldindex >= oldframe->numEntities ) {
		oldnum = 99999;
	} else {
		oldstate = &oldframe->parseEntities[( oldindex + oldframe->firstEntity ) & ( MAX_PARSE_ENTITIES - 1 )];
		oldnum = oldstate->number;
	}

	while( true ) {
		newnum = MSG_ReadEntityNumber( msg, &remove, &byteMask );
		if( newnum >= MAX_EDICTS ) {
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newnum );
		}
		if( msg->readcount > msg->cursize ) {
			Com_Error( ERR_DROP, "CL_ParsePacketEntities: end of message" );
		}

		if( !newnum ) {
			break;
		}

		while( oldnum < newnum ) {
			// one or more entities from the old packet are unchanged
			if( shownet == 3 ) {
				Com_Printf( "   unchanged: %i\n", oldnum );
			}

			SNAP_ParseDeltaEntity( msg, newframe, oldnum, oldstate, 0 );

			oldindex++;
			if( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &oldframe->parseEntities[( oldindex + oldframe->firstEntity ) & ( MAX_PARSE_ENTITIES - 1 )];
				oldnum = oldstate->number;
			}
		}

		// delta from baseline
		if( oldnum > newnum ) {
			if( remove ) {
				Com_Printf( "U_REMOVE: oldnum > newnum (can't remove from baseline!)\n" );
				continue;
			}

			// delta from baseline
			if( shownet == 3 ) {
				Com_Printf( "   baseline: %i\n", newnum );
			}

			SNAP_ParseDeltaEntity( msg, newframe, newnum, &baselines[newnum], byteMask );
			continue;
		}

		if( oldnum == newnum ) {
			if( remove ) {
				// the entity present in oldframe is not in the current frame
				if( shownet == 3 ) {
					Com_Printf( "   remove: %i\n", newnum );
				}

				if( oldnum != newnum ) {
					Com_Printf( "U_REMOVE: oldnum != newnum\n" );
				}

				oldindex++;
				if( oldindex >= oldframe->numEntities ) {
					oldnum = 99999;
				} else {
					oldstate =
						&oldframe->parseEntities[( oldindex + oldframe->firstEntity ) & ( MAX_PARSE_ENTITIES - 1 )];
					oldnum = oldstate->number;
				}
				continue;
			}

			// delta from previous state
			if( shownet == 3 ) {
				Com_Printf( "   delta: %i\n", newnum );
			}

			SNAP_ParseDeltaEntity( msg, newframe, newnum, oldstate, byteMask );

			oldindex++;
			if( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = &oldframe->parseEntities[( oldindex + oldframe->firstEntity ) & ( MAX_PARSE_ENTITIES - 1 )];
				oldnum = oldstate->number;
			}
			continue;
		}
	}

	// any remaining entities in the old frame are copied over
	while( oldnum != 99999 ) {
		// one or more entities from the old packet are unchanged
		if( shownet == 3 ) {
			Com_Printf( "   unchanged: %i\n", oldnum );
		}

		SNAP_ParseDeltaEntity( msg, newframe, oldnum, oldstate, 0 );

		oldindex++;
		if( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = &oldframe->parseEntities[( oldindex + oldframe->firstEntity ) & ( MAX_PARSE_ENTITIES - 1 )];
			oldnum = oldstate->number;
		}
	}
}

/*
* SNAP_ParseFrameHeader
*/
static snapshot_t *SNAP_ParseFrameHeader( msg_t *msg, snapshot_t *newframe, int *suppressCount, snapshot_t *backup,
	entity_state_t *parseEntities, int *firstParseEntity )
{
	int len, pos;
	int areabytes;
	uint8_t *areabits;
	int64_t serverTime;
	int flags, snapNum, supCnt;

	// get total length
	len = MSG_ReadInt16( msg );
	pos = msg->readcount;

	// get the snapshot id
	serverTime = MSG_ReadIntBase128( msg );
	snapNum = MSG_ReadUintBase128( msg );

	if( backup ) {
		newframe = &backup[snapNum & UPDATE_MASK];
	}

	areabytes = newframe->areabytes;
	areabits = newframe->areabits;
	memset( newframe, 0, sizeof( snapshot_t ) );
	newframe->areabytes = areabytes;
	newframe->areabits = areabits;

	newframe->parseEntities = parseEntities;
	newframe->firstEntity = *firstParseEntity;

	newframe->serverTime = serverTime;
	newframe->serverFrame = snapNum;
	newframe->deltaFrameNum = MSG_ReadUintBase128( msg );
	newframe->ucmdExecuted = MSG_ReadUintBase128( msg );

	flags = MSG_ReadUint8( msg );
	newframe->delta = ( flags & FRAMESNAP_FLAG_DELTA ) ? true : false;
	newframe->multipov = ( flags & FRAMESNAP_FLAG_MULTIPOV ) ? true : false;
	newframe->allentities = ( flags & FRAMESNAP_FLAG_ALLENTITIES ) ? true : false;

	supCnt = MSG_ReadUint8( msg );
	if( suppressCount ) {
		*suppressCount = supCnt;
#ifdef RATEKILLED
		*suppressCount = 0;
#endif
	}

	// validate the new frame
	newframe->valid = false;

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message
	if( !newframe->delta ) {
		newframe->valid = true; // uncompressed frame
	} else {
		if( newframe->deltaFrameNum <= 0 ) {
			Com_Printf( "Invalid delta frame (not supposed to happen!).\n" );
		} else if( backup ) {
			snapshot_t *deltaframe = &backup[newframe->deltaFrameNum & UPDATE_MASK];
			if( !deltaframe->valid ) {
				// should never happen
				Com_Printf( "Delta from invalid frame (not supposed to happen!).\n" );
			} else if( deltaframe->serverFrame != newframe->deltaFrameNum ) {
				// The frame that the server did the delta from
				// is too old, so we can't reconstruct it properly.
				Com_Printf( "Delta frame too old.\n" );
			} else {
				newframe->valid = true; // valid delta parse
			}
		}
	}

	return newframe;
}

/*
* SNAP_ParseFrame
*/
snapshot_t *SNAP_ParseFrame( msg_t *msg, snapshot_t *lastFrame, int *suppressCount, snapshot_t *backup,
	entity_state_t *parseEntities, int *firstParseEntity, entity_state_t *baselines, int showNet )
{
	int cmd;
	size_t len;
	snapshot_t  *deltaframe;
	int numplayers;
	char *text;
	int framediff, numtargets;
	gcommand_t *gcmd;
	snapshot_t  *newframe;

	// read header
	newframe = SNAP_ParseFrameHeader( msg, NULL, suppressCount, backup, parseEntities, firstParseEntity );
	deltaframe = NULL;

	if( showNet == 3 ) {
		Com_Printf( "   frame:%" PRIi64 "  old:%" PRIi64 "%s\n", newframe->serverFrame, newframe->deltaFrameNum,
					( newframe->delta ? "" : " no delta" ) );
	}

	if( newframe->delta ) {
		if( newframe->deltaFrameNum > 0 ) {
			deltaframe = &backup[newframe->deltaFrameNum & UPDATE_MASK];
		}
	}

	// read game commands
	cmd = MSG_ReadUint8( msg );
	if( cmd != svc_gamecommands ) {
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not gamecommands" );
	}

	numtargets = 0;
	while( ( framediff = MSG_ReadInt16( msg ) ) != -1 ) {
		text = MSG_ReadString( msg );

		// see if it's valid and not yet handled
		if( newframe->valid &&
			( !lastFrame || !lastFrame->valid || newframe->serverFrame > lastFrame->serverFrame + framediff ) ) {
			newframe->numgamecommands++;
			if( newframe->numgamecommands > MAX_PARSE_GAMECOMMANDS ) {
				Com_Error( ERR_DROP, "SNAP_ParseFrame: too many gamecommands" );
			}
			if( newframe->gamecommandsDataHead + strlen( text ) >= sizeof( newframe->gamecommandsData ) ) {
				Com_Error( ERR_DROP, "SNAP_ParseFrame: too much gamecommands" );
			}

			gcmd = &newframe->gamecommands[newframe->numgamecommands - 1];
			gcmd->all = true;

			Q_strncpyz( newframe->gamecommandsData + newframe->gamecommandsDataHead, text,
						sizeof( newframe->gamecommandsData ) - newframe->gamecommandsDataHead );
			gcmd->commandOffset = newframe->gamecommandsDataHead;
			newframe->gamecommandsDataHead += strlen( text ) + 1;

			if( newframe->multipov ) {
				numtargets = MSG_ReadUint8( msg );
				if( numtargets ) {
					if( numtargets > sizeof( gcmd->targets ) ) {
						Com_Error( ERR_DROP, "SNAP_ParseFrame: too many gamecommand targets" );
					}
					gcmd->all = false;
					MSG_ReadData( msg, gcmd->targets, numtargets );
				}
			}
		} else if( newframe->multipov ) {   // otherwise, ignore it
			numtargets = MSG_ReadUint8( msg );
			MSG_SkipData( msg, numtargets );
		}
	}

	// read areabits
	len = (size_t)MSG_ReadUint8( msg );
	if( len > newframe->areabytes ) {
		Com_Error( ERR_DROP, "Invalid areabits size: %" PRIuPTR " > %" PRIuPTR, (uintptr_t)len, (uintptr_t)newframe->areabytes );
	}
	memset( newframe->areabits, 0, newframe->areabytes );
	MSG_ReadData( msg, newframe->areabits, len );

	// read match info
	cmd = MSG_ReadUint8( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_match ) {
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not match info" );
	}
	SNAP_ParseDeltaGameState( msg, deltaframe, newframe );

	// read playerinfos
	numplayers = 0;
	while( ( cmd = MSG_ReadUint8( msg ) ) ) {
		_SHOWNET( msg, svc_strings[cmd], showNet );
		if( cmd != svc_playerinfo ) {
			Com_Error( ERR_DROP, "SNAP_ParseFrame: not playerinfo" );
		}
		if( deltaframe && deltaframe->numplayers >= numplayers ) {
			SNAP_ParsePlayerstate( msg, &deltaframe->playerStates[numplayers], &newframe->playerStates[numplayers] );
		} else {
			SNAP_ParsePlayerstate( msg, NULL, &newframe->playerStates[numplayers] );
		}
		numplayers++;
	}
	newframe->numplayers = numplayers;
	newframe->playerState = newframe->playerStates[0];

	// read packet entities
	cmd = MSG_ReadUint8( msg );
	_SHOWNET( msg, svc_strings[cmd], showNet );
	if( cmd != svc_packetentities ) {
		Com_Error( ERR_DROP, "SNAP_ParseFrame: not packetentities" );
	}

	SNAP_ParsePacketEntities( msg, deltaframe, newframe, baselines, showNet );
	*firstParseEntity += newframe->numEntities;

	return newframe;
}
