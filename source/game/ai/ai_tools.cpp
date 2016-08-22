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
