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
--------------------------------------------------------------
The ACE Bot is a product of Steve Yeager, and is available from
the ACE Bot homepage, at http://www.axionfx.com/ace.

This program is a modification of the ACE Bot, and is therefore
in NO WAY supported by Steve Yeager.
*/

#include "ai_local.h"

void AI_Cheat_NoTarget( edict_t *ent )
{
	if( !sv_cheats->integer )
		return;

	ent->flags ^= FL_NOTARGET;
	if( ent->flags & FL_NOTARGET )
		G_PrintMsg( ent, "Bot Notarget ON\n" );
	else
		G_PrintMsg( ent, "Bot Notarget OFF\n" );
}


//==========================================
// AIDebug_ToogleBotDebug
//==========================================
void AIDebug_ToogleBotDebug( void )
{
	if( !sv_cheats->integer )
	{
		G_Printf( "BOT: Debug Mode can't be enabled without sv_cheats\n" );
		nav.debugMode = false;
		return;
	}

	if( nav.debugMode )
	{
		G_Printf( "BOT: Debug Mode Off\n" );
		nav.debugMode = false;
		return;
	}

	//Activate debug mode
	G_Printf( "BOT: Debug Mode On\n" );
	nav.debugMode = true;
}

//=======================================================================
//							NODE TOOLS
//=======================================================================

//==========================================
// AITools_DrawLine
// Just so I don't hate to write the event every time
//==========================================
void AITools_DrawLine( vec3_t origin, vec3_t dest )
{
	edict_t	*event;

	event = G_SpawnEvent( EV_GREEN_LASER, 0, origin );
	event->r.svflags = SVF_TRANSMITORIGIN2;
	VectorCopy( dest, event->s.origin2 );
	G_SetBoundsForSpanEntity( event, 8 );
	GClip_LinkEntity( event );
}

//==========================================
// AITools_DrawColorLine
// Just so I don't hate to write the event every time
//==========================================
void AITools_DrawColorLine( vec3_t origin, vec3_t dest, int color, int parm )
{
	edict_t	*event;

	event = G_SpawnEvent( EV_PNODE, parm, origin );
	event->s.colorRGBA = color;
	event->r.svflags = SVF_TRANSMITORIGIN2;
	VectorCopy( dest, event->s.origin2 );
	G_SetBoundsForSpanEntity( event, 8 );
	GClip_LinkEntity( event );
}

void AITools_DrawAxis( vec3_t origin, int color )
{
	vec3_t origin2;

	VectorCopy( origin, origin2 );
	origin2[0] += 24;
	AITools_DrawColorLine( origin, origin2, color, 0 );

	VectorCopy( origin, origin2 );
	origin2[1] += 24;
	AITools_DrawColorLine( origin, origin2, color, 0 );

	VectorCopy( origin, origin2 );
	origin2[2] += 24;
	AITools_DrawColorLine( origin, origin2, color, 0 );
}


//==========================================
// AITools_DrawPath
// Draws the current path (floods as hell also)
//==========================================
void AITools_DrawPath( edict_t *self, int node_to )
{
	static unsigned int drawnpath_timeout;
	int count = 0;
	int pos = 0;

	//don't draw it every frame (flood)
	if( level.time < drawnpath_timeout )
		return;

	drawnpath_timeout = level.time + 4 * game.snapFrameTime;

	if( self->ai->path.goalNode != node_to )
		return;

	pos = self->ai->path.numNodes;

	// Now set up and display the path
	while( self->ai->path.nodes[pos] != node_to && count < 32 && pos > 0 )
	{
		edict_t	*event;

		event = G_SpawnEvent( EV_GREEN_LASER, 0, nodes[self->ai->path.nodes[pos]].origin );
		event->r.svflags = SVF_TRANSMITORIGIN2;
		VectorCopy( nodes[self->ai->path.nodes[pos-1]].origin, event->s.origin2 );
		G_SetBoundsForSpanEntity( event, 8 );
		GClip_LinkEntity( event );

		pos--;
		count++;
	}
}

//==========================================
// AITools_ShowPlinks
// Draws lines from the current node to it's plinks nodes
//==========================================
static void AITools_ShowPlinks( edict_t *target )
{
	static unsigned int debugdrawplinks_timeout;
	int current_node;
	int plink_node;
	int i;
	nav_ents_t *goalEnt;

	if( !target || !target->r.client || !target->r.client->level.showPLinks )
		return;

	//do it
	current_node = Ai::FindClosestReachableNode( target->s.origin, target, NODE_DENSITY * 3, NODE_ALL );

	// draw the axis where the node is
	if( nodes[current_node].flags & NODEFLAGS_SERVERLINK )
		AITools_DrawAxis( nodes[current_node].origin, COLOR_RGBA( 255, 25, 25, 255 ) );
	else
		AITools_DrawAxis( nodes[current_node].origin, COLOR_RGBA( 210, 250, 250, 255 ) );

	//don't draw the links every frame (flood)
	if( level.time < debugdrawplinks_timeout )
		return;
	debugdrawplinks_timeout = level.time + 4 * game.snapFrameTime;

	if( nav.editmode || !nav.loaded )
		return;

	FOREACH_GOALENT( goalEnt )
	{
		i = goalEnt->id;
		if( goalEnt->node == current_node )
		{
			if( !goalEnt->ent->classname )
				G_CenterPrintMsg( target, "no classname" );
			else
				G_CenterPrintMsg( target, "%s", goalEnt->ent->classname );
			break;
		}
	}

	// no links to draw
	if( !pLinks[current_node].numLinks )
		return;

	for( i = 0; i < pLinks[current_node].numLinks; i++ )
	{
		plink_node = pLinks[current_node].nodes[i];
		if( pLinks[current_node].moveType[i] == LINK_ROCKETJUMP )
			AITools_DrawColorLine( nodes[current_node].origin,
			nodes[plink_node].origin, COLOR_RGBA( 0xff, 0x00, 0x00, 0x80 ), 0 );
		else if( pLinks[current_node].moveType[i] == LINK_JUMP )
			AITools_DrawColorLine( nodes[current_node].origin,
			nodes[plink_node].origin, COLOR_RGBA( 0x00, 0x00, 0xff, 0x80 ), 0 );
		else
			AITools_DrawColorLine( nodes[current_node].origin,
			nodes[plink_node].origin, COLOR_RGBA( 0x00, 0xff, 0x00, 0x80 ), 0 );
	}
}

void Cmd_deleteClosestNode_f( edict_t *ent )
{
	int node;

	if( ent && nav.num_nodes > 0 )
	{
		node = Ai::FindClosestReachableNode( ent->s.origin, ent, NODE_DENSITY*3, NODE_ALL );
		AI_DeleteNode( node );
	}
}

void Cmd_ShowPLinks_f( edict_t *ent )
{
	if( !sv_cheats->integer )
	{
		G_PrintMsg( ent, "Cheats are not enabled in this server\n" );
		return;
	}

	ent->r.client->level.showPLinks = !ent->r.client->level.showPLinks ? true : false;
}

void Cmd_SaveNodes_f( void )
{
	AI_SaveNavigation();
}

//=======================================================================
//=======================================================================

//==========================================
// AITools_Frame
// Gives think time to the debug tools found
// in this archive (those witch need it)
//==========================================
void AITools_Frame( void )
{
	edict_t *ent;
	for( ent = game.edicts + 1; PLAYERNUM( ent ) < gs.maxclients; ent++ )
	{
		if( !ent->r.inuse )
			continue;
		if( trap_GetClientState( PLAYERNUM( ent ) ) < CS_SPAWNED )
			continue;

		AITools_DropNodes( ent );
		AITools_ShowPlinks( ent );
	}
}
