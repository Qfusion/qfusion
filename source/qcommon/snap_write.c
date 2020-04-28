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
static void SNAP_EmitPacketEntities( ginfo_t *gi, client_snapshot_t *from, client_snapshot_t *to, msg_t *msg, entity_state_t *baselines, entity_state_t *client_entities, int num_client_entities ) {
	entity_state_t *oldent, *newent;
	int oldindex, newindex;
	int oldnum, newnum;
	int from_num_entities;

	MSG_WriteUint8( msg, svc_packetentities );

	if( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->num_entities;
	}

	newindex = 0;
	oldindex = 0;
	while( newindex < to->num_entities || oldindex < from_num_entities ) {
		if( newindex >= to->num_entities ) {
			newent = NULL;
			newnum = 9999;
		} else {
			newent = &client_entities[( to->first_entity + newindex ) % num_client_entities];
			newnum = newent->number;
		}

		if( oldindex >= from_num_entities ) {
			oldent = NULL;
			oldnum = 9999;
		} else {
			oldent = &client_entities[( from->first_entity + oldindex ) % num_client_entities];
			oldnum = oldent->number;
		}

		if( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping ( wsw : jal : I removed it from the players )
			MSG_WriteDeltaEntity( msg, oldent, newent, false );
			oldindex++;
			newindex++;
			continue;
		}

		if( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity( msg, &baselines[newnum], newent, true );
			newindex++;
			continue;
		}

		if( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteDeltaEntity( msg, oldent, NULL, false );
			oldindex++;
			continue;
		}
	}

	MSG_WriteEntityNumber( msg, 0, false, 0 ); // end of packetentities
}

/*
* SNAP_WriteDeltaGameStateToClient
*/
static void SNAP_WriteDeltaGameStateToClient( client_snapshot_t *from, client_snapshot_t *to, msg_t *msg ) {
	MSG_WriteUint8( msg, svc_match );
	MSG_WriteDeltaGameState( msg, from ? &from->gameState : NULL, &to->gameState );
}

/*
* SNAP_WritePlayerstateToClient
*/
static void SNAP_WritePlayerstateToClient( msg_t *msg, const player_state_t *ops, player_state_t *ps ) {
	MSG_WriteUint8( msg, svc_playerinfo );
	MSG_WriteDeltaPlayerState( msg, ops, ps );
}

/*
* SNAP_WriteMultiPOVCommands
*/
static void SNAP_WriteMultiPOVCommands( ginfo_t *gi, client_t *client, msg_t *msg, int64_t frameNum ) {
	int i, index;
	client_t *cl;
	int positions[MAX_CLIENTS];
	int maxnumtargets;
	const char *command;

	// find the first command to send from every client
	maxnumtargets = 0;
	for( i = 0; i < gi->max_clients; i++ ) {
		cl = gi->clients + i;

		if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->r.svflags & SVF_NOCLIENT ) ) && cl != client ) ) {
			continue;
		}

		maxnumtargets++;
		for( positions[i] = cl->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1;
			 positions[i] <= cl->gameCommandCurrent; positions[i]++ ) {
			index = positions[i] & ( MAX_RELIABLE_COMMANDS - 1 );

			// we need to check for too new commands too, because gamecommands for the next snap are generated
			// all the time, and we might want to create a server demo frame or something in between snaps
			if( cl->gameCommands[index].command[0] && cl->gameCommands[index].framenum + 256 >= frameNum &&
				cl->gameCommands[index].framenum <= frameNum &&
				( client->lastframe >= 0 && cl->gameCommands[index].framenum > client->lastframe ) ) {
				break;
			}
		}
	}

	// send all messages, combining similar messages together to save space
	do {
		int numtargets = 0, maxtarget = 0;
		int64_t framenum = 0;
		uint8_t targets[MAX_CLIENTS / 8];

		command = NULL;
		memset( targets, 0, sizeof( targets ) );

		// we find the message with the earliest framenum, and collect all recipients for that
		for( i = 0; i < gi->max_clients; i++ ) {
			cl = gi->clients + i;

			if( cl->state < CS_SPAWNED || ( ( !cl->edict || ( cl->edict->r.svflags & SVF_NOCLIENT ) ) && cl != client ) ) {
				continue;
			}

			if( positions[i] > cl->gameCommandCurrent ) {
				continue;
			}

			index = positions[i] & ( MAX_RELIABLE_COMMANDS - 1 );

			if( command && !strcmp( cl->gameCommands[index].command, command ) &&
				framenum == cl->gameCommands[index].framenum ) {
				targets[i >> 3] |= 1 << ( i & 7 );
				maxtarget = i + 1;
				numtargets++;
			} else if( !command || cl->gameCommands[index].framenum < framenum ) {
				command = cl->gameCommands[index].command;
				framenum = cl->gameCommands[index].framenum;
				memset( targets, 0, sizeof( targets ) );
				targets[i >> 3] |= 1 << ( i & 7 );
				maxtarget = i + 1;
				numtargets = 1;
			}

			if( numtargets == maxnumtargets ) {
				break;
			}
		}

		// send it
		if( command ) {
			// never write a command if it's of a higher framenum
			if( frameNum >= framenum ) {
				// do not allow the message buffer to overflow (can happen on flood updates)
				if( msg->cursize + strlen( command ) + 512 > msg->maxsize ) {
					continue;
				}

				MSG_WriteInt16( msg, frameNum - framenum );
				MSG_WriteString( msg, command );

				// 0 means everyone
				if( numtargets == maxnumtargets ) {
					MSG_WriteUint8( msg, 0 );
				} else {
					int bytes = ( maxtarget + 7 ) / 8;
					MSG_WriteUint8( msg, bytes );
					MSG_WriteData( msg, targets, bytes );
				}
			}

			for( i = 0; i < maxtarget; i++ )
				if( targets[i >> 3] & ( 1 << ( i & 7 ) ) ) {
					positions[i]++;
				}
		}
	} while( command );
}

/*
* SNAP_RelayMultiPOVCommands
*/
static void SNAP_RelayMultiPOVCommands( ginfo_t *gi, client_t *client, msg_t *msg, int numcmds, gcommand_t *commands, const char *commandsData ) {
	int i, index;
	int first_index, last_index;
	gcommand_t *gcmd;
	const char *command;

	first_index = numcmds - MAX_RELIABLE_COMMANDS;
	last_index = first_index + MAX_RELIABLE_COMMANDS;

	clamp_low( first_index, 0 );
	clamp_high( last_index, numcmds );

	for( index = first_index, gcmd = commands + index; index < last_index; index++, gcmd++ ) {
		command = commandsData + gcmd->commandOffset;

		// do not allow the message buffer to overflow (can happen on flood updates)
		if( msg->cursize + strlen( command ) + 512 > msg->maxsize ) {
			continue;
		}

		MSG_WriteInt16( msg, 0 );
		MSG_WriteString( msg, command );

		if( gcmd->all ) {
			MSG_WriteUint8( msg, 0 );
		} else {
			int maxtarget, bytes;

			maxtarget = 0;
			for( i = 0; i < MAX_CLIENTS; i++ )
				if( gcmd->targets[i >> 3] & ( 1 << ( i & 7 ) ) ) {
					maxtarget = i + 1;
				}

			bytes = ( maxtarget + 7 ) / 8;
			MSG_WriteUint8( msg, bytes );
			MSG_WriteData( msg, gcmd->targets, bytes );
		}
	}
}

/*
* SNAP_WriteFrameSnapToClient
*/
void SNAP_WriteFrameSnapToClient( ginfo_t *gi, client_t *client, msg_t *msg, int64_t frameNum, int64_t gameTime,
								  entity_state_t *baselines, client_entities_t *client_entities,
								  int numcmds, gcommand_t *commands, const char *commandsData ) {
	client_snapshot_t *frame, *oldframe;
	int flags, i, index, pos, length, supcnt;

	// this is the frame we are creating
	frame = &client->snapShots[frameNum & UPDATE_MASK];

	// for non-reliable clients we need to send nodelta frame until the client responds
	if( client->nodelta && !client->reliable ) {
		if( !client->nodelta_frame ) {
			client->nodelta_frame = frameNum;
		} else if( client->lastframe >= client->nodelta_frame ) {
			client->nodelta = false;
		}
	}

	if( client->lastframe <= 0 || client->lastframe > frameNum || client->nodelta ) {
		// client is asking for a not compressed retransmit
		oldframe = NULL;
	}
	//else if( frameNum >= client->lastframe + (UPDATE_BACKUP - 3) )
	else if( frameNum >= client->lastframe + UPDATE_MASK ) {
		// client hasn't gotten a good message through in a long time
		oldframe = NULL;
	} else {
		// we have a valid message to delta from
		oldframe = &client->snapShots[client->lastframe & UPDATE_MASK];
		if( oldframe->multipov != frame->multipov ) {
			oldframe = NULL;        // don't delta compress a frame of different POV type
		}
	}

	if( client->nodelta && client->reliable ) {
		client->nodelta = false;
	}

	MSG_WriteUint8( msg, svc_frame );

	pos = msg->cursize;
	MSG_WriteInt16( msg, 0 );       // we will write length here

	MSG_WriteIntBase128( msg, gameTime ); // serverTimeStamp
	MSG_WriteUintBase128( msg, frameNum );
	MSG_WriteUintBase128( msg, client->lastframe );
	MSG_WriteUintBase128( msg, frame->UcmdExecuted );

	flags = 0;
	if( oldframe != NULL ) {
		flags |= FRAMESNAP_FLAG_DELTA;
	}
	if( frame->allentities ) {
		flags |= FRAMESNAP_FLAG_ALLENTITIES;
	}
	if( frame->multipov ) {
		flags |= FRAMESNAP_FLAG_MULTIPOV;
	}
	MSG_WriteUint8( msg, flags );

#ifdef RATEKILLED
	supcnt = client->suppressCount;
#else
	supcnt = 0;
#endif

	client->suppressCount = 0;
	MSG_WriteUint8( msg, supcnt );   // rate dropped packets

	// add game comands
	MSG_WriteUint8( msg, svc_gamecommands );
	if( frame->multipov ) {
		if( frame->relay ) {
			SNAP_RelayMultiPOVCommands( gi, client, msg, numcmds, commands, commandsData );
		} else {
			SNAP_WriteMultiPOVCommands( gi, client, msg, frameNum );
		}
	} else {
		for( i = client->gameCommandCurrent - MAX_RELIABLE_COMMANDS + 1; i <= client->gameCommandCurrent; i++ ) {
			index = i & ( MAX_RELIABLE_COMMANDS - 1 );

			// check that it is valid command and that has not already been sent
			// we can only allow commands from certain amount of old frames, so the short won't overflow
			if( !client->gameCommands[index].command[0] || client->gameCommands[index].framenum + 256 < frameNum ||
				client->gameCommands[index].framenum > frameNum ||
				( client->lastframe >= 0 && client->gameCommands[index].framenum <= (unsigned)client->lastframe ) ) {
				continue;
			}

			// do not allow the message buffer to overflow (can happen on flood updates)
			if( msg->cursize + strlen( client->gameCommands[index].command ) + 512 > msg->maxsize ) {
				continue;
			}

			// send it
			MSG_WriteInt16( msg, frameNum - client->gameCommands[index].framenum );
			MSG_WriteString( msg, client->gameCommands[index].command );
		}
	}
	MSG_WriteInt16( msg, -1 );

	// send over the areabits
	MSG_WriteUint8( msg, frame->areabytes );
	MSG_WriteData( msg, frame->areabits, frame->areabytes );

	SNAP_WriteDeltaGameStateToClient( oldframe, frame, msg );

	// delta encode the playerstate
	for( i = 0; i < frame->numplayers; i++ ) {
		if( oldframe && oldframe->numplayers > i ) {
			SNAP_WritePlayerstateToClient( msg, &oldframe->ps[i], &frame->ps[i] );
		} else {
			SNAP_WritePlayerstateToClient( msg, NULL, &frame->ps[i] );
		}
	}
	MSG_WriteUint8( msg, 0 );

	// delta encode the entities
	SNAP_EmitPacketEntities( gi, oldframe, frame, msg, baselines, client_entities ? client_entities->entities : NULL, client_entities ? client_entities->num_entities : 0 );

	// write length into reserved space
	length = msg->cursize - pos - 2;
	msg->cursize = pos;
	MSG_WriteInt16( msg, length );
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
static void SNAP_FatPVS( cmodel_state_t *cms, const vec3_t org, uint8_t *fatpvs ) {
	memset( fatpvs, 0, CM_ClusterRowSize( cms ) );
	CM_MergePVS( cms, org, fatpvs );
}

/*
* SNAP_BitsCullEntity
*/
static bool SNAP_BitsCullEntity( cmodel_state_t *cms, edict_t *ent, uint8_t *bits, int max_clusters ) {
	int i, l;

	// too many leafs for individual check, go by headnode
	if( ent->r.num_clusters == -1 ) {
		if( !CM_HeadnodeVisible( cms, ent->r.headnode, bits ) ) {
			return true;
		}
		return false;
	}

	// check individual leafs
	for( i = 0; i < max_clusters; i++ ) {
		l = ent->r.clusternums[i];
		if( bits[l >> 3] & ( 1 << ( l & 7 ) ) ) {
			return false;
		}
	}

	return true;    // not visible/audible
}

#define SNAP_PVSCullEntity( cms,fatpvs,ent ) SNAP_BitsCullEntity( cms,ent,fatpvs,ent->r.num_clusters )

//=====================================================================

typedef struct {
	int numSnapshotEntities;
	int snapshotEntities[MAX_SNAPSHOT_ENTITIES];
	uint8_t entityAddedToSnapList[MAX_EDICTS / 8];
} snapshotEntityNumbers_t;

/*
* SNAP_AddEntNumToSnapList
*/
static bool SNAP_AddEntNumToSnapList( int entNum, snapshotEntityNumbers_t *entList ) {
	if( entNum >= MAX_EDICTS ) {
		return false;
	}
	if( entList->numSnapshotEntities >= MAX_SNAPSHOT_ENTITIES ) { // silent ignore of overflood
		return false;
	}

	// don't double add entities
	if( entList->entityAddedToSnapList[entNum >> 3] & (1 << (entNum & 7)) ) {
		return false;
	}

	entList->snapshotEntities[entList->numSnapshotEntities++] = entNum;
	entList->entityAddedToSnapList[entNum >> 3] |=  (1 << (entNum & 7));
	return true;
}

/*
* SNAP_SortSnapList
*/
static void SNAP_SortSnapList( snapshotEntityNumbers_t *entsList ) {
	int i;

	entsList->numSnapshotEntities = 0;

	// avoid adding world to the list by all costs
	for( i = 1; i < MAX_EDICTS; i++ ) {
		if( entsList->entityAddedToSnapList[i >> 3] & (1 << (i & 7)) ) {
			entsList->snapshotEntities[entsList->numSnapshotEntities++] = i;
		}
	}
}

/*
* SNAP_GainForAttenuation
*/
static float SNAP_GainForAttenuation( float dist, float attenuation ) {
	int model = S_DEFAULT_ATTENUATION_MODEL;
	float maxdistance = S_DEFAULT_ATTENUATION_MAXDISTANCE;
	float refdistance = S_DEFAULT_ATTENUATION_REFDISTANCE;

#if !defined( PUBLIC_BUILD ) && !defined( DEDICATED_ONLY )
#define DUMMY_CVAR ( cvar_t * )( (void *)1 )
	static cvar_t *s_attenuation_model = DUMMY_CVAR;
	static cvar_t *s_attenuation_maxdistance = DUMMY_CVAR;
	static cvar_t *s_attenuation_refdistance = DUMMY_CVAR;

	if( s_attenuation_model == DUMMY_CVAR ) {
		s_attenuation_model = Cvar_Find( "s_attenuation_model" );
	}
	if( s_attenuation_maxdistance == DUMMY_CVAR ) {
		s_attenuation_maxdistance = Cvar_Find( "s_attenuation_maxdistance" );
	}
	if( s_attenuation_refdistance == DUMMY_CVAR ) {
		s_attenuation_refdistance = Cvar_Find( "s_attenuation_refdistance" );
	}

	if( s_attenuation_model && s_attenuation_model != DUMMY_CVAR ) {
		model = s_attenuation_model->integer;
	}
	if( s_attenuation_maxdistance && s_attenuation_maxdistance != DUMMY_CVAR ) {
		maxdistance = s_attenuation_maxdistance->value;
	}
	if( s_attenuation_refdistance && s_attenuation_refdistance != DUMMY_CVAR ) {
		refdistance = s_attenuation_refdistance->value;
	}
#undef DUMMY_CVAR
#endif

	return Q_GainForAttenuation( model, maxdistance, refdistance, dist, attenuation );
}

/*
* SNAP_SnapCullSoundEntity
*/
static bool SNAP_SnapCullSoundEntity( cmodel_state_t *cms, edict_t *ent, const vec3_t listener_origin, 
									float attenuation ) {
	float gain, dist;

	if( !attenuation ) {
		return false;
	}

	// extend the influence sphere cause the player could be moving
	dist = DistanceFast( ent->s.origin, listener_origin ) - 128;
	gain = SNAP_GainForAttenuation( dist < 0 ? 0 : dist, attenuation );
	if( gain > 0.05 ) { // curved attenuations can keep barely audible sounds for long distances
		return false;
	}

	return true;
}

/*
* SNAP_SnapCullEntity
*/
static bool SNAP_SnapCullEntity( cmodel_state_t *cms, edict_t *ent, edict_t *clent, client_snapshot_t *frame, 
								const vec3_t vieworg, int viewarea, uint8_t *fatpvs ) {
	bool snd_cull_only = false;
	bool snd_culled = false;

	// filters: this entity has been disabled for comunication
	if( ent->r.svflags & SVF_NOCLIENT ) {
		return true;
	}

	// send all entities
	if( frame->allentities ) {
		return false;
	}

	// filters: transmit only to clients in the same team as this entity
	// broadcasting is less important than team specifics
	if( ( ent->r.svflags & SVF_ONLYTEAM ) && ( clent && ent->s.team != clent->s.team ) ) {
		return true;
	}

	// send only to owner
	if( ( ent->r.svflags & SVF_ONLYOWNER ) && ( clent && ent->s.ownerNum != clent->s.number ) ) {
		return true;
	}

	if( ent->r.svflags & SVF_BROADCAST ) { // send to everyone
		return false;
	}

	if( ( ent->r.svflags & SVF_FORCETEAM ) && ( clent && ent->s.team == clent->s.team ) ) {
		return false;
	}

	if( ent->r.areanum < 0 ) {
		return true;
	}
	if( viewarea >= 0 ) {
		// this is the same as CM_AreasConnected but portal's visibility included
		uint8_t *areabits = frame->areabits + viewarea * CM_AreaRowSize( cms );
		if( !( areabits[ent->r.areanum >> 3] & ( 1 << ( ent->r.areanum & 7 ) ) ) ) {
			// doors can legally straddle two areas, so we may need to check another one
			if( ent->r.areanum2 < 0 || !( areabits[ent->r.areanum2 >> 3] & ( 1 << ( ent->r.areanum2 & 7 ) ) ) ) {
				return true; // blocked by a door
			}
		}
	}

	// sound entities culling
	if( ent->r.svflags & SVF_SOUNDCULL ) {
		snd_cull_only = true;
	}
	// if not a sound entity but the entity is only a sound
	else if( !ent->s.modelindex && !ent->s.events[0] && !ent->s.light && !ent->s.effects && ent->s.sound ) {
		snd_cull_only = true;
	}

	// PVS culling alone may not be used on pure sounds, entities with
	// events and regular entities emitting sounds
	if( snd_cull_only || ent->s.events[0] || ent->s.sound ) {
		snd_culled = SNAP_SnapCullSoundEntity( cms, ent, vieworg, ent->s.attenuation );
	}

	// pure sound emitters don't use PVS culling at all
	if( snd_cull_only && snd_culled ) {
		return true;
	}
	return snd_culled && SNAP_PVSCullEntity( cms, fatpvs, ent );    // cull by PVS
}

/*
* SNAP_AddEntitiesVisibleAtOrigin
*/
static void SNAP_AddEntitiesVisibleAtOrigin( cmodel_state_t *cms, ginfo_t *gi, edict_t *clent, const vec3_t vieworg, 
											int viewarea, client_snapshot_t *frame, snapshotEntityNumbers_t *entList ) {
	int entNum;
	edict_t *ent;
	uint8_t *pvs;

	pvs = alloca( CM_ClusterRowSize( cms ) );
	SNAP_FatPVS( cms, vieworg, pvs );

	// add the entities to the list
	for( entNum = 1; entNum < gi->num_edicts; entNum++ ) {
		ent = EDICT_NUM( entNum );

		// fix number if broken
		if( ent->s.number != entNum ) {
			Com_Printf( "FIXING ENT->S.NUMBER: %i %i!!!\n", ent->s.number, entNum );
			ent->s.number = entNum;
		}

		// always add the client entity, even if SVF_NOCLIENT
		if( ( ent != clent ) && SNAP_SnapCullEntity( cms, ent, clent, frame, vieworg, viewarea, pvs ) ) {
			continue;
		}

		// add it
		if( !SNAP_AddEntNumToSnapList( entNum, entList ) ) {
			continue;
		}

		if( ent->r.svflags & SVF_FORCEOWNER ) {
			// make sure owner number is valid too
			if( ent->s.ownerNum > 0 && ent->s.ownerNum < gi->num_edicts ) {
				SNAP_AddEntNumToSnapList( ent->s.ownerNum, entList );
			} else {
				Com_Printf( "FIXING ENT->S.OWNERNUM: %i %i!!!\n", ent->s.type, ent->s.ownerNum );
				ent->s.ownerNum = 0;
			}
		}

		if( ent->r.svflags & SVF_PORTAL ) {
			// if it's a portal entity and not a mirror,
			// recursively add everything from its camera positiom
			if( !VectorCompare( ent->s.origin, ent->s.origin2 ) ) {
				SNAP_AddEntitiesVisibleAtOrigin( cms, gi, clent, ent->s.origin2, ent->r.areanum, frame, entList );
			}
		}
	}
}

/*
* SNAP_BuildSnapEntitiesList
*/
static void SNAP_BuildSnapEntitiesList( cmodel_state_t *cms, ginfo_t *gi, edict_t *clent, const vec3_t vieworg, 
										client_snapshot_t *frame, snapshotEntityNumbers_t *entList ) {
	int entNum;
	int leafnum, clientarea;

	entList->numSnapshotEntities = 0;
	memset( entList->entityAddedToSnapList, 0, sizeof( entList->entityAddedToSnapList ) );

	// find the client's PVS
	leafnum = CM_PointLeafnum( cms, vieworg );
	clientarea = CM_LeafArea( cms, leafnum );

	frame->clientarea = clientarea;
	frame->areabytes = CM_WriteAreaBits( cms, frame->areabits );

	// always add the client entity
	if( clent ) {
		entNum = NUM_FOR_EDICT( clent );
		if( clent->s.number != entNum ) {
			Com_Printf( "FIXING CLENT->S.NUMBER: %i %i!!!\n", clent->s.number, entNum );
			clent->s.number = entNum;
		}

		// FIXME we should send all the entities who's POV we are sending if frame->multipov
		SNAP_AddEntNumToSnapList( entNum, entList );
	}

	// if the client is outside of the world, don't send him any entity
	if( clientarea >= 0 || frame->allentities ) {
		SNAP_AddEntitiesVisibleAtOrigin( cms, gi, clent, vieworg, clientarea, frame, entList );
	}

	SNAP_SortSnapList( entList );
}

/*
* SNAP_BuildClientFrameSnap
*
* Decides which entities are going to be visible to the client, and
* copies off the playerstat and areabits.
*/
void SNAP_BuildClientFrameSnap( cmodel_state_t *cms, ginfo_t *gi, int64_t frameNum, int64_t timeStamp,
								client_t *client,
								game_state_t *gameState, client_entities_t *client_entities,
								bool relay, mempool_t *mempool ) {
	int e, i, ne;
	vec3_t org;
	edict_t *ent, *clent;
	client_snapshot_t *frame;
	entity_state_t *state;
	int numplayers, numareas;
	snapshotEntityNumbers_t entsList;

	assert( gameState );

	clent = client->edict;
	if( clent && !clent->r.client ) {   // allow NULL ent for server record
		return;     // not in game yet

	}
	if( clent ) {
		VectorCopy( clent->s.origin, org );
		org[2] += clent->r.client->ps.viewheight;
	} else {
		assert( client->mv );
		VectorClear( org );
	}

	// this is the frame we are creating
	frame = &client->snapShots[frameNum & UPDATE_MASK];
	frame->sentTimeStamp = timeStamp;
	frame->UcmdExecuted = client->UcmdExecuted;
	frame->relay = relay;

	if( client->mv ) {
		frame->multipov = true;
		frame->allentities = true;
	} else {
		frame->multipov = false;
		frame->allentities = false;
	}

	// areaportals matrix
	numareas = CM_NumAreas( cms );
	if( frame->numareas < numareas ) {
		frame->numareas = numareas;

		numareas *= CM_AreaRowSize( cms );
		if( frame->areabits ) {
			Mem_Free( frame->areabits );
			frame->areabits = NULL;
		}
		frame->areabits = (uint8_t*)Mem_Alloc( mempool, numareas );
	}

	// grab the current player_state_t
	if( frame->multipov ) {
		frame->numplayers = 0;
		for( i = 0; i < gi->max_clients; i++ ) {
			ent = EDICT_NUM( i + 1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->r.svflags & SVF_NOCLIENT ) ) ) {
				frame->numplayers++;
			}
		}
	} else {
		frame->numplayers = 1;
	}

	if( frame->ps_size < frame->numplayers ) {
		if( frame->ps ) {
			Mem_Free( frame->ps );
			frame->ps = NULL;
		}

		frame->ps = ( player_state_t* )Mem_Alloc( mempool, sizeof( player_state_t ) * frame->numplayers );
		frame->ps_size = frame->numplayers;
	}

	if( frame->multipov ) {
		numplayers = 0;
		for( i = 0; i < gi->max_clients; i++ ) {
			ent = EDICT_NUM( i + 1 );
			if( ( clent == ent ) || ( ent->r.inuse && ent->r.client && !( ent->r.svflags & SVF_NOCLIENT ) ) ) {
				frame->ps[numplayers] = ent->r.client->ps;
				frame->ps[numplayers].playerNum = i;
				numplayers++;
			}
		}
	} else {
		frame->ps[0] = clent->r.client->ps;
		frame->ps[0].playerNum = NUM_FOR_EDICT( clent ) - 1;
	}

	// build up the list of visible entities
	//=============================
	SNAP_BuildSnapEntitiesList( cms, gi, clent, org, frame, &entsList );

	// store current match state information
	frame->gameState = *gameState;

	//=============================

	// dump the entities list
	ne = client_entities->next_entities;
	frame->num_entities = 0;
	frame->first_entity = ne;

	for( e = 0; e < entsList.numSnapshotEntities; e++ ) {
		// add it to the circular client_entities array
		ent = EDICT_NUM( entsList.snapshotEntities[e] );
		state = &client_entities->entities[ne % client_entities->num_entities];

		*state = ent->s;
		state->svflags = ent->r.svflags;

		// don't mark *any* missiles as solid
		if( ent->r.svflags & SVF_PROJECTILE ) {
			state->solid = 0;
		}

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
static void SNAP_FreeClientFrame( client_snapshot_t *frame ) {
	if( frame->areabits ) {
		Mem_Free( frame->areabits );
		frame->areabits = NULL;
	}
	frame->numareas = 0;

	if( frame->ps ) {
		Mem_Free( frame->ps );
		frame->ps = NULL;
	}
	frame->ps_size = 0;
}

/*
* SNAP_FreeClientFrames
*
*/
void SNAP_FreeClientFrames( client_t *client ) {
	int i;
	client_snapshot_t *frame;

	for( i = 0; i < UPDATE_BACKUP; i++ ) {
		frame = &client->snapShots[i];
		SNAP_FreeClientFrame( frame );
	}
}
