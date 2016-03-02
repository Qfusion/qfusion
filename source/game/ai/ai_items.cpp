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

#include "bot.h"

//ACE

//==========================================
// AI_EnemyAdded
// Add the Player to our list
//==========================================
void AI_EnemyAdded( edict_t *ent )
{
	AI_AddGoalEntity( ent );
}

//==========================================
// AI_EnemyRemoved
// Remove the Player from list
//==========================================
void AI_EnemyRemoved( edict_t *ent )
{
	AI_RemoveGoalEntity( ent );
}

//==========================================
// AI_ItemIsReachable
// Can we get there? Jalfixme: this needs better checks a lot
//==========================================
bool Ai::ShortRangeReachable( vec3_t goal )
{
	trace_t trace;
	vec3_t v;

	VectorCopy( self->r.mins, v );
	v[2] += AI_STEPSIZE;

	G_Trace( &trace, self->s.origin, v, self->r.maxs, goal, self, MASK_NODESOLID );
	//trace = gi.trace ( self->s.origin, v, self->maxs, goal, self, MASK_NODESOLID );

	// Yes we can see it
	if( trace.fraction == 1.0 )
		return true;
	else
		return false;
}
