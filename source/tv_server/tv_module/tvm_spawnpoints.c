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

#include "tvm_spawnpoints.h"

#include "tvm_chase.h"
#include "tvm_client.h"
#include "tvm_clip.h"
#include "tvm_pmove.h"
#include "tvm_misc.h"

/*
* TVM_SelectSpawnPoint
*/
edict_t *TVM_SelectSpawnPoint( edict_t *ent )
{
	// pick a target to chase and spawn there
	if( tv_chasemode->integer >= 0 )
	{
		TVM_ChasePlayer( ent, NULL, tv_chasemode->integer );
		if( ent->r.client->chase.target )
			return ent->relay->edicts + ent->r.client->chase.target;
	}

	// spawn at random point in demo playback
	if( ent->relay->playernum < 0 )
	{
		int i, numtargets;
		edict_t *target, *targets[MAX_EDICTS];

		numtargets = 0;
		for( i = 1; i < ent->relay->numentities; i++ )
		{
			target = ent->relay->edicts + i;
			if( !target->r.inuse )
				continue;
			targets[numtargets++] = target;
		}

		if( numtargets )
			return targets[rand()%numtargets];
		return ent->relay->edicts;
	}

	// spawn at TV spec position
	return ent->relay->edicts + ent->relay->playernum + 1;
}
