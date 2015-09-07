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

#include "tvm_local.h"

#include "tvm_chase.h"
#include "tvm_client.h"
#include "tvm_misc.h"
#include "tvm_clip.h"
#include "tvm_pmove.h"
#include "tvm_spawnpoints.h"

//==============================================================

/*
* InitClientPersistant
*/
static void InitClientPersistant( gclient_t *client )
{
	assert( client );

	memset( &client->pers, 0, sizeof( client->pers ) );

	client->pers.connected = true;
}

/*
* TVM_ClientEndSnapFrame
* 
* Called for each player at the end of the server frame
* and right after spawning
*/
void TVM_ClientEndSnapFrame( edict_t *ent )
{
	edict_t *spec;
	tvm_relay_t *relay = ent->relay;

	assert( ent && ent->local && ent->r.client && !ent->r.client->chase.active );

	if( relay->playernum < 0 )
		spec = NULL;
	else
		spec = relay->edicts + relay->playernum + 1;

	if( relay->frame.valid && !relay->frame.multipov )
	{
		assert( spec );
		ent->r.client->ps = spec->r.client->ps;
		ent->r.client->ps.pmove.pm_type = PM_CHASECAM;
		ent->r.client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
		ent->r.client->ps.POVnum = PLAYERNUM( spec ) + 1;
		ent->s = spec->s;
		ent->s.number = ENTNUM( ent );
		return;
	}

	if( spec && spec->r.inuse && spec->r.client )
	{
		memcpy( ent->r.client->ps.stats, spec->r.client->ps.stats, sizeof( ent->r.client->ps.stats ) );
		memcpy( ent->r.client->ps.inventory, spec->r.client->ps.inventory, sizeof( ent->r.client->ps.inventory ) );
	}
	else
	{
		memset( ent->r.client->ps.stats, 0, sizeof( ent->r.client->ps.stats ) );
		memset( ent->r.client->ps.inventory, 0, sizeof( ent->r.client->ps.inventory ) );
	}

	ent->r.client->ps.viewheight = ent->viewheight;
	if( relay->playernum < 0 )
		ent->r.client->ps.POVnum = 255; // FIXME
	else
		ent->r.client->ps.POVnum = relay->playernum + 1;
}

/*
* TVM_ClientIsZoom
*/
bool TVM_ClientIsZoom( edict_t *ent )
{
	assert( ent && ent->local && ent->r.client );

#if 0
	if( ent->r.client->ps.stats[STAT_HEALTH] <= 0 )
		return false;
#endif

	if( ent->snap.buttons & BUTTON_ZOOM )
		return true;

	return false;
}

/*
* TVM_ClientBegin
* 
* called when a client has finished connecting, and is ready
* to be placed into the game. This will happen every level load.
*/
void TVM_ClientBegin( tvm_relay_t *relay, edict_t *ent )
{
	edict_t *spot, *other;
	int i, specs;
	char hostname[MAX_CONFIGSTRING_CHARS];

	assert( ent && ent->local && ent->r.client );

	//TVM_Printf( "Begin: %s\n", ent->r.client->pers.netname );

	ent->r.client->pers.connecting = false;

	spot = TVM_SelectSpawnPoint( ent );
	if( spot )
	{
		VectorCopy( spot->s.origin, ent->s.origin );
		VectorCopy( spot->s.origin, ent->s.old_origin );
		VectorCopy( spot->s.angles, ent->s.angles );
		VectorCopy( spot->s.origin, ent->r.client->ps.pmove.origin );
		VectorCopy( spot->s.angles, ent->r.client->ps.viewangles );
	}
	else
	{
		VectorClear( ent->s.origin );
		VectorClear( ent->s.old_origin );
		VectorClear( ent->s.angles );
		VectorClear( ent->r.client->ps.pmove.origin );
		VectorClear( ent->r.client->ps.viewangles );
	}

	ent->s.teleported = true;
	// set the delta angle
	for( i = 0; i < 3; i++ )
		ent->r.client->ps.pmove.delta_angles[i] = ANGLE2SHORT( ent->s.angles[i] ) - ent->r.client->pers.cmd_angles[i];

	specs = 0;
	for( i = 0; i < relay->local_maxclients; i++ )
	{
		other = relay->local_edicts + i;
		if( other == ent )
			continue;
		if( !other->r.inuse || !other->r.client )
			continue;
		if( trap_GetClientState( relay, PLAYERNUM( other ) ) != CS_SPAWNED )
			continue;
		specs++;
	}

	Q_strncpyz( hostname, relay->configStrings[CS_HOSTNAME], sizeof( hostname ) );
	TVM_PrintMsg( relay, ent, S_COLOR_ORANGE "Welcome to %s! There %s currently %i spectator%s on this channel.\n",
		COM_RemoveColorTokens( hostname ), (specs == 1 ? "is" : "are"), specs, (specs == 1 ? "" : "s") );

	TVM_PrintMsg( relay, ent, S_COLOR_ORANGE "For more information about chase camera modes type 'chase help' at console.\n" );

	if( ent->r.client->chase.active )
		TVM_ChaseClientEndSnapFrame( ent );
	else
		TVM_ClientEndSnapFrame( ent );
}

/*
* TVM_ClientUserInfoChanged
* 
* called whenever the player updates a userinfo variable.
* 
* The game can override any of the settings in place
* (forcing skins or names, etc) before copying it off.
*/
void TVM_ClientUserinfoChanged( tvm_relay_t *relay, edict_t *ent, char *userinfo )
{
	gclient_t *cl;

	assert( ent && ent->local && ent->r.client );

	cl = ent->r.client;

	// check for malformed or illegal info strings
	if( !Info_Validate( userinfo ) )
	{
		trap_DropClient( relay, PLAYERNUM( ent ), DROP_TYPE_GENERAL, "Error: Invalid userinfo" );
		return;
	}

	if( !Info_ValueForKey( userinfo, "name" ) )
	{
		trap_DropClient( relay, PLAYERNUM( ent ), DROP_TYPE_GENERAL, "Error: No name set" );
		return;
	}

	// set name, it's validated and possibly changed first
	Q_strncpyz( cl->pers.netname, Info_ValueForKey( userinfo, "name" ), sizeof( cl->pers.netname ) );

	// save off the userinfo in case we want to check something later
	Q_strncpyz( cl->pers.userinfo, userinfo, sizeof( cl->pers.userinfo ) );
}

/*
* TVM_CanConnect
*/
bool TVM_CanConnect( tvm_relay_t *relay, char *userinfo )
{
	return true;
}

/*
* TVM_ClientConnect
*/
void TVM_ClientConnect( tvm_relay_t *relay, edict_t *ent, char *userinfo )
{
	edict_t *spec;

	assert( relay );
	assert( ent );
	assert( userinfo );

	// make sure we start with known default
	if( ent->relay->playernum < 0 )
		spec = NULL;
	else
		spec = ent->relay->edicts + ent->relay->playernum + 1;
	ent->local = true;
	ent->relay = relay;
	ent->r.inuse = true;
	ent->r.svflags = SVF_NOCLIENT;
	ent->s.team = spec ? spec->s.team : 0;
	ent->r.client = relay->local_clients + PLAYERNUM( ent );

	memset( ent->r.client, 0, sizeof( *ent->r.client ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );
	InitClientPersistant( ent->r.client );

	TVM_ClientUserinfoChanged( relay, ent, userinfo );

	//TVM_Printf( "Connect: %s\n", ent->r.client->pers.netname );

	ent->r.client->pers.connected = true;
	ent->r.client->pers.connecting = true;
}

/*
* TVM_ClientDisconnect
* 
* Called when a player drops from the server.
* Will not be called between levels.
*/
void TVM_ClientDisconnect( tvm_relay_t *relay, edict_t *ent )
{
	assert( ent && ent->local && ent->r.client );

	//TVM_Printf( "Disconnect: %s\n", ent->r.client->pers.netname );

	ent->r.inuse = false;
	ent->r.svflags = SVF_NOCLIENT;
	memset( ent->r.client, 0, sizeof( *ent->r.client ) );
	ent->r.client->ps.playerNum = PLAYERNUM( ent );

	GClip_UnlinkEntity( ent->relay, ent );
}

/*
* TVM_ClientMultiviewChanged
* 
* This will be called when client tries to change multiview mode
* Mode change can be disallowed by returning false
*/
bool TVM_ClientMultiviewChanged( tvm_relay_t *relay, edict_t *ent, bool multiview )
{
	assert( ent && ent->local && ent->r.client );

	ent->r.client->pers.multiview = multiview;
	return true;
}

static void TVM_ClientMakePlrkeys( gclient_t *client, usercmd_t *ucmd )
{
	assert( client );
	assert( ucmd );

	client->plrkeys = 0; // clear it first

	if( ucmd->forwardmove > 0 )
		client->plrkeys |= ( 1 << KEYICON_FORWARD );
	if( ucmd->forwardmove < 0 )
		client->plrkeys |= ( 1 << KEYICON_BACKWARD );
	if( ucmd->sidemove > 0 )
		client->plrkeys |= ( 1 << KEYICON_RIGHT );
	if( ucmd->sidemove < 0 )
		client->plrkeys |= ( 1 << KEYICON_LEFT );
	if( ucmd->upmove > 0 )
		client->plrkeys |= ( 1 << KEYICON_JUMP );
	if( ucmd->upmove < 0 )
		client->plrkeys |= ( 1 << KEYICON_CROUCH );
	if( ucmd->buttons & BUTTON_ATTACK )
		client->plrkeys |= ( 1 << KEYICON_FIRE );
	if( ucmd->buttons & BUTTON_SPECIAL )
		client->plrkeys |= ( 1 << KEYICON_SPECIAL );
}

/*
* TVM_ClientThink
* 
* This will be called once for each client frame, which will
* usually be a couple times for each server frame.
*/
void TVM_ClientThink( tvm_relay_t *relay, edict_t *ent, usercmd_t *ucmd, int timeDelta )
{
	gclient_t *client;
	static pmove_t pm;

	assert( ent && ent->local && ent->r.client );
	assert( ucmd );

	client = ent->r.client;
	client->ps.playerNum = PLAYERNUM( ent );
	client->ps.POVnum = client->ps.playerNum + 1;
	client->buttons = 0;
	VectorCopy( ucmd->angles, client->pers.cmd_angles );
	client->timeDelta = timeDelta;

	VectorCopy( ent->s.origin, client->ps.pmove.origin );
	VectorCopy( ent->velocity, client->ps.pmove.velocity );

	client->ps.pmove.gravity = 850;

	// in QFusion this was applied to both ps.pmove and pm.s, so it never activated snap initial
	if( !ent->relay->frame.multipov )
	{
		client->ps.pmove.pm_type = PM_CHASECAM;
	}
	else
	{
		edict_t *spec;

		if( ent->relay->playernum < 0 )
			spec = NULL;
		else
			spec = &ent->relay->edicts[ent->relay->playernum + 1];

		if( spec && spec->r.client && spec->r.client->ps.pmove.pm_type == PM_FREEZE )
		{
			client->ps.pmove.pm_type = PM_FREEZE;
			if( !VectorCompare( ent->s.origin, spec->s.origin ) )
			{
				ent->s.teleported = true;
				VectorCopy( spec->s.origin, ent->s.origin );
				VectorCopy( spec->s.angles, ent->s.angles );
				VectorCopy( spec->s.angles, client->ps.viewangles );
			}
		}
		else if( client->chase.active )
		{
			client->ps.pmove.pm_type = PM_CHASECAM;
		}
		else
		{
			client->ps.pmove.pm_type = PM_SPECTATOR;
		}
	}

	memset( &pm, 0, sizeof( pmove_t ) );
	pm.cmd = *ucmd;
	pm.playerState = &client->ps;
	pm.playerState->pmove.stats[PM_STAT_MAXSPEED] = 320;

	VectorCopy( ent->r.mins, pm.mins );
	VectorCopy( ent->r.maxs, pm.maxs );

	if( memcmp( &client->old_pmove, &client->ps.pmove, sizeof( pmove_state_t ) ) )
		pm.snapinitial = true;

	// perform a pmove
	TVM_Pmove( &pm );

	// save results of pmove
	client->old_pmove = client->ps.pmove;

	// update the entity with the new position
	VectorCopy( client->ps.pmove.origin, ent->s.origin );
	VectorCopy( client->ps.pmove.velocity, ent->velocity );
	VectorCopy( pm.mins, ent->r.mins );
	VectorCopy( pm.maxs, ent->r.maxs );
	VectorCopy( client->ps.viewangles, ent->s.angles );
	ent->viewheight = client->ps.viewheight;

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	if( pm.groundentity != -1 )
	{
		ent->groundentity = &ent->relay->edicts[pm.groundentity];
		ent->groundentity_linkcount = ent->groundentity->r.linkcount;
	}
	else
	{
		ent->groundentity = NULL;
	}

	GClip_LinkEntity( ent->relay, ent );

	// during the min respawn time, clear all buttons
	if( client->ps.pmove.stats[PM_STAT_NOUSERCONTROL] <= 0 )
		client->buttons = ucmd->buttons;

	// generating plrkeys (optimized for net communication)
	TVM_ClientMakePlrkeys( client, ucmd );
}
