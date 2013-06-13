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
#include "g_local.h"

//
// g_clip.c - entity contact detection. (high level object sorting to reduce interaction tests)
//


//===============================================================================
//
//ENTITY AREA CHECKING
//
//FIXME: this use of "area" is different from the bsp file use
//===============================================================================

#define EDICT_NUM( n ) ( (edict_t *)( game.edicts + n ) )
#define NUM_FOR_EDICT( e ) ( ENTNUM( e ) )

typedef struct areanode_s
{
	int axis;       // -1 = leaf node
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

#define	AREA_DEPTH  5
#define	AREA_NODES  64

areanode_t sv_areanodes[AREA_NODES];
int sv_numareanodes;

extern cvar_t *g_antilag;
extern cvar_t *g_antilag_maxtimedelta;

#define	CFRAME_UPDATE_BACKUP	64  // copies of entity_state_t to keep buffered (1 second of backup at 62 fps).
#define	CFRAME_UPDATE_MASK	( CFRAME_UPDATE_BACKUP-1 )

typedef struct c4clipedict_s
{
	entity_state_t s;
	entity_shared_t	r;
} c4clipedict_t;

//backups of all server frames areas and edicts
typedef struct c4frame_s
{
	c4clipedict_t clipEdicts[MAX_EDICTS];   // fixme: there is a g_maxentities cvar. We have to adjust to it
	int numedicts;

	unsigned int timestamp;
	unsigned int framenum;
} c4frame_t;

c4frame_t sv_collisionframes[CFRAME_UPDATE_BACKUP];
static unsigned int sv_collisionFrameNum = 0;

void GClip_BackUpCollisionFrame( void )
{
	c4frame_t *cframe;
	edict_t	*svedict;
	int i;

	if( !g_antilag->integer )
		return;

	// fixme: should check for any validation here?

	cframe = &sv_collisionframes[sv_collisionFrameNum & CFRAME_UPDATE_MASK];
	cframe->timestamp = game.serverTime;
	cframe->framenum = sv_collisionFrameNum;
	sv_collisionFrameNum++;

	//memset( cframe->clipEdicts, 0, sizeof(cframe->clipEdicts) );

	//backup edicts
	for( i = 0; i < game.numentities; i++ )
	{
		svedict = &game.edicts[i];

		cframe->clipEdicts[i].r.inuse = svedict->r.inuse;
		cframe->clipEdicts[i].r.solid = svedict->r.solid;
		if( !svedict->r.inuse || svedict->r.solid == SOLID_NOT 
			|| ( svedict->r.solid == SOLID_TRIGGER && !(i >= 1 && i <= gs.maxclients) ) )
			continue;

		cframe->clipEdicts[i].r = svedict->r;
		cframe->clipEdicts[i].s = svedict->s;
	}
	cframe->numedicts = game.numentities;
}

static c4clipedict_t *GClip_GetClipEdictForDeltaTime( int entNum, int deltaTime )
{
	static int index = 0;
	static c4clipedict_t clipEnts[8];
	static c4clipedict_t *clipent;
	static c4clipedict_t clipentNewer; // for interpolation
	c4frame_t *cframe = NULL;
	unsigned int backTime, cframenum, backframes, i;
	edict_t	*ent = game.edicts + entNum;

	// pick one of the 8 slots to prevent overwritings
	clipent = &clipEnts[index];
	index = ( index + 1 )&7;

	if( !entNum || deltaTime >= 0 || !g_antilag->integer )
	{                                                    // current time entity
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	if( !ent->r.inuse || ent->r.solid == SOLID_NOT 
		|| ( ent->r.solid == SOLID_TRIGGER && !(entNum >= 1 && entNum <= gs.maxclients) ) )
	{
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	// clamp delta time inside the backed up limits
	backTime = abs( deltaTime );
	if( g_antilag_maxtimedelta->integer )
	{
		if( g_antilag_maxtimedelta->integer < 0 )
			trap_Cvar_SetValue( "g_antilag_maxtimedelta", abs( g_antilag_maxtimedelta->integer ) );
		if( backTime > (unsigned int)g_antilag_maxtimedelta->integer )
			backTime = (unsigned int)g_antilag_maxtimedelta->integer;
	}

	// find the first snap with timestamp < than realtime - backtime
	cframenum = sv_collisionFrameNum;
	for( backframes = 1; backframes < CFRAME_UPDATE_BACKUP && backframes < sv_collisionFrameNum; backframes++ ) // never overpass limits
	{
		cframe = &sv_collisionframes[( cframenum-backframes ) & CFRAME_UPDATE_MASK];
		// if solid has changed, we can't keep moving backwards
		if( ent->r.solid != cframe->clipEdicts[entNum].r.solid || ent->r.inuse != cframe->clipEdicts[entNum].r.inuse )
		{
			backframes--;
			if( backframes == 0 )
			{           // we can't step back from first
				cframe = NULL;
			}
			else
			{
				cframe = &sv_collisionframes[( cframenum-backframes ) & CFRAME_UPDATE_MASK];
			}
			break;
		}

		if( game.serverTime >= cframe->timestamp + backTime )
			break;
	}

	if( !cframe )
	{           // current time entity
		clipent->r = ent->r;
		clipent->s = ent->s;
		return clipent;
	}

	// setup with older for the data that is not interpolated
	*clipent = cframe->clipEdicts[entNum];

	// if we found an older than desired backtime frame, interpolate to find a more precise position.
	if( game.serverTime > cframe->timestamp+backTime )
	{
		float lerpFrac;

		if( backframes == 1 )
		{               // interpolate from 1st backed up to current
			lerpFrac = (float)( ( game.serverTime - backTime ) - cframe->timestamp ) / (float)( game.serverTime - cframe->timestamp );
			clipentNewer.r = ent->r;
			clipentNewer.s = ent->s;
		}
		else
		{ // interpolate between 2 backed up
			c4frame_t *cframeNewer = &sv_collisionframes[( cframenum-( backframes-1 ) ) & CFRAME_UPDATE_MASK];
			lerpFrac = (float)( ( game.serverTime - backTime ) - cframe->timestamp ) / (float)( cframeNewer->timestamp - cframe->timestamp );
			clipentNewer = cframeNewer->clipEdicts[entNum];
		}

		//G_Printf( "backTime:%i cframeBackTime:%i backFrames:%i lerfrac:%f\n", backTime, game.serverTime - cframe->timestamp, backframes, lerpFrac );

		// interpolate
		VectorLerp( clipent->s.origin, lerpFrac, clipentNewer.s.origin, clipent->s.origin );
		VectorLerp( clipent->r.mins, lerpFrac, clipentNewer.r.mins, clipent->r.mins );
		VectorLerp( clipent->r.maxs, lerpFrac, clipentNewer.r.maxs, clipent->r.maxs );
		for( i = 0; i < 3; i++ )
			clipent->s.angles[i] = LerpAngle( clipent->s.angles[i], clipentNewer.s.angles[i], lerpFrac );
	}

	//G_Printf( "backTime:%i cframeBackTime:%i backFrames:%i\n", backTime, game.serverTime - cframe->timestamp, backframes );

	// back time entity
	return clipent;
}

// ClearLink is used for new headnodes
static void GClip_ClearLink( link_t *l )
{
	l->prev = l->next = l;
	l->entNum = 0;
}

static void GClip_RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
	l->entNum = 0;
}

static void GClip_InsertLinkBefore( link_t *l, link_t *before, int entNum )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
	l->entNum = entNum;
}

/*
* GClip_CreateAreaNode
* Builds a uniformly subdivided tree for the given world size
*/
static areanode_t *GClip_CreateAreaNode( int depth, vec3_t mins, vec3_t maxs )
{
	areanode_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes++];
	GClip_ClearLink( &anode->trigger_edicts );
	GClip_ClearLink( &anode->solid_edicts );

	if( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract( maxs, mins, size );
	if( size[0] > size[1] )
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * ( maxs[anode->axis] + mins[anode->axis] );
	VectorCopy( mins, mins1 );
	VectorCopy( mins, mins2 );
	VectorCopy( maxs, maxs1 );
	VectorCopy( maxs, maxs2 );

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = GClip_CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = GClip_CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

/*
* GClip_ClearWorld
* called after the world model has been loaded, before linking any entities
*/
void GClip_ClearWorld( void )
{
	vec3_t mins, maxs;
	struct cmodel_s *cmodel;

	memset( sv_areanodes, 0, sizeof( sv_areanodes ) );
	sv_numareanodes = 0;

	cmodel = trap_CM_InlineModel( 0 );
	trap_CM_InlineModelBounds( cmodel, mins, maxs );
	GClip_CreateAreaNode( 0, mins, maxs );
}


/*
* GClip_UnlinkEntity
* call before removing an entity, and before trying to move one,
* so it doesn't clip against itself
*/
void GClip_UnlinkEntity( edict_t *ent )
{
	if( !ent->r.area.prev )
		return; // not linked in anywhere
	GClip_RemoveLink( &ent->r.area );
	ent->r.area.prev = ent->r.area.next = NULL;
	ent->linked = false;
}


/*
* GClip_LinkEntity
* Needs to be called any time an entity changes origin, mins, maxs,
* or solid.  Automatically unlinks if needed.
* sets ent->v.absmin and ent->v.absmax
* sets ent->leafnums[] for pvs determination even if the entity
* is not solid
*/
#define MAX_TOTAL_ENT_LEAFS	128
void GClip_LinkEntity( edict_t *ent )
{
	areanode_t *node;
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int clusters[MAX_TOTAL_ENT_LEAFS];
	int num_leafs;
	int i, j, k;
	int area;
	int topnode;

	if( ent->r.area.prev )
		GClip_UnlinkEntity( ent ); // unlink from old position

	if( ent == game.edicts )
		return; // don't add the world

	if( !ent->r.inuse )
		return;

	// set the size
	VectorSubtract( ent->r.maxs, ent->r.mins, ent->r.size );

	if( ent->r.solid == SOLID_NOT || ( ent->r.svflags & SVF_PROJECTILE ) )
	{
		ent->s.solid = 0;
	}
	else if( ISBRUSHMODEL( ent->s.modelindex ) )
	{
		// the only predicted SOLID_TRIGGER entity is ET_PUSH_TRIGGER
		if( ent->r.solid != SOLID_TRIGGER || ent->s.type == ET_PUSH_TRIGGER )
			ent->s.solid = SOLID_BMODEL;
		else
			ent->s.solid = 0;
	}
	else // encode the size into the entity_state for client prediction
	{
		if( ent->r.solid == SOLID_TRIGGER )
		{
			ent->s.solid = 0;
		}
		else
		{
			// assume that x/y are equal and symetric
			i = ent->r.maxs[0]/8;
			clamp( i, 1, 31 );

			// z is not symetric
			j = ( -ent->r.mins[2] )/8;
			clamp( j, 1, 31 );

			// and z maxs can be negative...
			k = ( ent->r.maxs[2]+32 )/8;
			clamp( k, 1, 63 );

			ent->s.solid = ( k<<10 ) | ( j<<5 ) | i;
		}
	}

	// set the abs box
	if( ISBRUSHMODEL( ent->s.modelindex ) &&
		( ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2] ) )
	{ 
		// expand for rotation
		float radius;

		radius = RadiusFromBounds( ent->r.mins, ent->r.maxs );

		for( i = 0; i < 3; i++ )
		{
			ent->r.absmin[i] = ent->s.origin[i] - radius;
			ent->r.absmax[i] = ent->s.origin[i] + radius;
		}
	}
	else // axis aligned
	{ 
		VectorAdd( ent->s.origin, ent->r.mins, ent->r.absmin );
		VectorAdd( ent->s.origin, ent->r.maxs, ent->r.absmax );
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->r.absmin[0] -= 1;
	ent->r.absmin[1] -= 1;
	ent->r.absmin[2] -= 1;
	ent->r.absmax[0] += 1;
	ent->r.absmax[1] += 1;
	ent->r.absmax[2] += 1;

	// link to PVS leafs
	ent->r.num_clusters = 0;
	ent->r.areanum = ent->r.areanum2 = -1;

	// get all leafs, including solids
	num_leafs = trap_CM_BoxLeafnums( ent->r.absmin, ent->r.absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// set areas
	for( i = 0; i < num_leafs; i++ )
	{
		clusters[i] = trap_CM_LeafCluster( leafs[i] );
		area = trap_CM_LeafArea( leafs[i] );
		if( area > -1 )
		{
			// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if( ent->r.areanum > -1 && ent->r.areanum != area )
			{
				if( ent->r.areanum2 > -1 && ent->r.areanum2 != area )
				{
					if( developer->integer )
						G_Printf( "Object %s touching 3 areas at %f %f %f\n",
						( ent->classname ? ent->classname : "" ),
						ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2] );
				}
				ent->r.areanum2 = area;
			}
			else
				ent->r.areanum = area;
		}
	}

	if( num_leafs >= MAX_TOTAL_ENT_LEAFS )
	{ // assume we missed some leafs, and mark by headnode
		ent->r.num_clusters = -1;
		ent->r.headnode = topnode;
	}
	else
	{
		ent->r.num_clusters = 0;
		for( i = 0; i < num_leafs; i++ )
		{
			if( clusters[i] == -1 )
				continue; // not a visible leaf
			for( j = 0; j < i; j++ )
				if( clusters[j] == clusters[i] )
					break;
			if( j == i )
			{
				if( ent->r.num_clusters == MAX_ENT_CLUSTERS )
				{
					// assume we missed some leafs, and mark by headnode
					ent->r.num_clusters = -1;
					ent->r.headnode = topnode;
					break;
				}

				ent->r.clusternums[ent->r.num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if( !ent->r.linkcount && !( ent->r.svflags & SVF_TRANSMITORIGIN2 ) )
	{
		VectorCopy( ent->s.origin, ent->s.old_origin );
		ent->olds = ent->s;
	}
	ent->r.linkcount++;
	ent->linked = true;

	if( ent->r.solid == SOLID_NOT )
		return;

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while( 1 )
	{
		if( node->axis == -1 )
			break;
		if( ent->r.absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( ent->r.absmax[node->axis] < node->dist )
			node = node->children[1];
		else
			break; // crosses the node
	}

	// link it in
	if( ent->r.solid == SOLID_TRIGGER )
		GClip_InsertLinkBefore( &ent->r.area, &node->trigger_edicts, NUM_FOR_EDICT( ent ) );
	else
		GClip_InsertLinkBefore( &ent->r.area, &node->solid_edicts, NUM_FOR_EDICT( ent ) );
}

/*
* GClip_SetAreaPortalState
* 
* Finds an areaportal leaf entity is connected with,
* and also finds two leafs from different areas connected
* with the same entity.
*/
void GClip_SetAreaPortalState( edict_t *ent, bool open )
{
	// entity must touch at least two areas
	if( ent->r.areanum < 0 || ent->r.areanum2 < 0 )
		return;

	// change areaportal's state
	trap_CM_SetAreaPortalState( ent->r.areanum, ent->r.areanum2, open );
}


/*
* GClip_AreaEdicts
* fills in a table of edict ids with edicts that have bounding boxes
* that intersect the given area.  It is possible for a non-axial bmodel
* to be returned that doesn't actually intersect the area on an exact
* test.
* returns the number of pointers filled in
* ??? does this always return the world?
*/
static int GClip_AreaEdicts( vec3_t mins, vec3_t maxs, int *list, int maxcount, int areatype, int timeDelta )
{
	link_t *l, *start;
	c4clipedict_t *clipEnt;
	int stackdepth = 0, count = 0;
	areanode_t *localstack[AREA_NODES], *node = sv_areanodes;

	while( 1 )
	{
		// touch linked edicts
		if( areatype == AREA_SOLID )
			start = &node->solid_edicts;
		else
			start = &node->trigger_edicts;

		for( l = start->next; l != start; l = l->next )
		{
			clipEnt = GClip_GetClipEdictForDeltaTime( l->entNum, timeDelta );

			if( clipEnt->r.solid == SOLID_NOT )
				continue; // deactivated

			if( !BoundsIntersect( clipEnt->r.absmin, clipEnt->r.absmax, mins, maxs ) )
				continue; // not touching

			if( count == maxcount )
			{
				G_Printf( "G_AreaEdicts: MAXCOUNT\n" );
				return count;
			}
			list[count++] = l->entNum;
		}

		if( node->axis == -1 )
			goto checkstack; // terminal node

		// recurse down both sides
		if( maxs[node->axis] > node->dist )
		{
			if( mins[node->axis] < node->dist )
			{
				localstack[stackdepth++] = node->children[0];
				node = node->children[1];
				continue;
			}
			node = node->children[0];
			continue;
		}
		if( mins[node->axis] < node->dist )
		{
			node = node->children[1];
			continue;
		}

checkstack:
		if( !stackdepth )
			return count;
		node = localstack[--stackdepth];
	}

	return count;
}

/*
* GClip_CollisionModelForEntity
* 
* Returns a collision model that can be used for testing or clipping an
* object of mins/maxs size.
*/
static struct cmodel_s *GClip_CollisionModelForEntity( entity_state_t *s, entity_shared_t *r )
{
	struct cmodel_s	*model;

	if( ISBRUSHMODEL( s->modelindex ) )
	{ 
		// explicit hulls in the BSP model
		model = trap_CM_InlineModel( s->modelindex );
		if( !model )
			G_Error( "MOVETYPE_PUSH with a non bsp model" );

		return model;
	}

	// create a temp hull from bounding box sizes
	if( s->type == ET_PLAYER || s->type == ET_CORPSE )
		return trap_CM_OctagonModelForBBox( r->mins, r->maxs );
	else
		return trap_CM_ModelForBBox( r->mins, r->maxs );
}


/*
* G_PointContents
* returns the CONTENTS_* value from the world at the given point.
* Quake 2 extends this to also check entities, to allow moving liquids
*/
static int GClip_PointContents( vec3_t p, int timeDelta )
{
	c4clipedict_t *clipEnt;
	int touch[MAX_EDICTS];
	int i, num;
	int contents, c2;
	struct cmodel_s	*cmodel;
	float *angles;

	// get base contents from world
	contents = trap_CM_TransformedPointContents( p, NULL, NULL, NULL );

	// or in contents from all the other entities
	num = GClip_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID, timeDelta );

	for( i = 0; i < num; i++ )
	{
		clipEnt = GClip_GetClipEdictForDeltaTime( touch[i], timeDelta );

		// might intersect, so do an exact clip
		cmodel = GClip_CollisionModelForEntity( &clipEnt->s, &clipEnt->r );

		if( !ISBRUSHMODEL( clipEnt->s.modelindex ) )
			angles = vec3_origin; // boxes don't rotate
		else
			angles = clipEnt->s.angles;

		c2 = trap_CM_TransformedPointContents( p, cmodel, clipEnt->s.origin, clipEnt->s.angles );
		contents |= c2;
	}

	return contents;
}

int G_PointContents( vec3_t p )
{
	return GClip_PointContents( p, 0 );
}

int G_PointContents4D( vec3_t p, int timeDelta )
{
	return GClip_PointContents( p, timeDelta );
}

//===========================================================================

typedef struct
{
	vec3_t boxmins, boxmaxs;    // enclose the test object along entire move
	float *mins, *maxs;         // size of the moving object
	vec3_t mins2, maxs2;        // size when clipping against mosnters
	float *start, *end;
	trace_t	*trace;
	int passent;
	int contentmask;
} moveclip_t;

/*
* GClip_ClipMoveToEntities
*/
/*static*/ void GClip_ClipMoveToEntities( moveclip_t *clip, int timeDelta )
{
	int i, num;
	c4clipedict_t *touch;
	int touchlist[MAX_EDICTS];
	trace_t	trace;
	struct cmodel_s	*cmodel;
	float *angles;

	num = GClip_AreaEdicts( clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID, timeDelta );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ )
	{
		touch = GClip_GetClipEdictForDeltaTime( touchlist[i], timeDelta );
		if( clip->passent >= 0 )
		{
			// when they are offseted in time, they can be a different pointer but be the same entity
			if( touch->s.number == clip->passent )
				continue;
			if( touch->r.owner && ( touch->r.owner->s.number == clip->passent ) )
				continue;
			if( game.edicts[clip->passent].r.owner && ( game.edicts[clip->passent].r.owner->s.number == touch->s.number ) )
				continue;

			// wsw : jal : never clipmove against SVF_PROJECTILE entities
			if( touch->r.svflags & SVF_PROJECTILE )
				continue;
		}

		if( ( touch->r.svflags & SVF_CORPSE ) && !( clip->contentmask & CONTENTS_CORPSE ) )
			continue;

		// might intersect, so do an exact clip
		cmodel = GClip_CollisionModelForEntity( &touch->s, &touch->r );

		if( ISBRUSHMODEL( touch->s.modelindex ) )
			angles = touch->s.angles;
		else
			angles = vec3_origin; // boxes don't rotate

		trap_CM_TransformedBoxTrace( &trace, clip->start, clip->end,
			clip->mins, clip->maxs, cmodel, clip->contentmask,
			touch->s.origin, angles );

		if( trace.allsolid || trace.fraction < clip->trace->fraction )
		{
			trace.ent = touch->s.number;
			*( clip->trace ) = trace;
		}
		else if( trace.startsolid )
			clip->trace->startsolid = qtrue;
		if( clip->trace->allsolid )
			return;
	}
}


/*
* GClip_TraceBounds
*/
static void GClip_TraceBounds( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs )
{
	int i;

	for( i = 0; i < 3; i++ )
	{
		if( end[i] > start[i] )
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
* G_Trace
* 
* Moves the given mins/maxs volume through the world from start to end.
* 
* Passedict and edicts owned by passedict are explicitly not checked.
* ------------------------------------------------------------------
* mins and maxs are relative

* if the entire move stays in a solid volume, trace.allsolid will be set,
* trace.startsolid will be set, and trace.fraction will be 0

* if the starting point is in a solid, it will be allowed to move out
* to an open area

* passedict is explicitly excluded from clipping checks (normally NULL)
*/
static void GClip_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask, int timeDelta )
{
	moveclip_t clip;

	if( !tr )
		return;

	if( !mins )
		mins = vec3_origin;
	if( !maxs )
		maxs = vec3_origin;

	if( passedict == world )
	{
		memset( tr, 0, sizeof( trace_t ) );
		tr->fraction = 1;
		tr->ent = -1;
	}
	else
	{
		// clip to world
		trap_CM_TransformedBoxTrace( tr, start, end, mins, maxs, NULL, contentmask, NULL, NULL );
		tr->ent = tr->fraction < 1.0 ? world->s.number : -1;
		if( tr->fraction == 0 )
			return; // blocked by the world
	}

	memset( &clip, 0, sizeof( moveclip_t ) );
	clip.trace = tr;
	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passent = passedict ? ENTNUM( passedict ) : -1;

	VectorCopy( mins, clip.mins2 );
	VectorCopy( maxs, clip.maxs2 );

	// create the bounding box of the entire move
	GClip_TraceBounds( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to other solid entities
	GClip_ClipMoveToEntities( &clip, timeDelta );
}

void G_Trace( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask )
{
	GClip_Trace( tr, start, mins, maxs, end, passedict, contentmask, 0 );
}

void G_Trace4D( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask, int timeDelta )
{
	GClip_Trace( tr, start, mins, maxs, end, passedict, contentmask, timeDelta );
}
//===========================================================================


/*
* GClip_SetBrushModel
* 
* Also sets mins and maxs for inline bmodels
*/
void GClip_SetBrushModel( edict_t *ent, const char *name )
{
	struct cmodel_s *cmodel;

	if( !name )
		G_Error( "GClip_SetBrushModel: NULL model in '%s'", ent->classname ? ent->classname : "no classname" );

	if( !name[0] )
	{
		ent->s.modelindex = 0;
		return;
	}

	if( name[0] != '*' )
	{
		ent->s.modelindex = trap_ModelIndex( name );
		return;
	}

	// if it is an inline model, get the size information for it

	// world model is special
	if( !strcmp( name, "*0" ) )
	{
		ent->s.modelindex = 0;
		cmodel = trap_CM_InlineModel( 0 );
		trap_CM_InlineModelBounds( cmodel, ent->r.mins, ent->r.maxs );
		return;
	}

	// brush model
	ent->s.modelindex = trap_ModelIndex( name );
	assert( ent->s.modelindex == (unsigned int)atoi( name + 1 ) );
	cmodel = trap_CM_InlineModel( ent->s.modelindex );
	trap_CM_InlineModelBounds( cmodel, ent->r.mins, ent->r.maxs );
	GClip_LinkEntity( ent );
}

/*
* GClip_EntityContact
*/
static bool GClip_EntityContact( vec3_t mins, vec3_t maxs, edict_t *ent )
{
	trace_t tr;
	struct cmodel_s *model;

	if( !mins )
		mins = vec3_origin;
	if( !maxs )
		maxs = vec3_origin;

	if( ISBRUSHMODEL( ent->s.modelindex ) )
	{
		model = trap_CM_InlineModel( ent->s.modelindex );
		if( !model )
			G_Error( "MOVETYPE_PUSH with a non bsp model" );

		trap_CM_TransformedBoxTrace( &tr, vec3_origin, vec3_origin, mins, maxs, model, MASK_ALL, ent->s.origin, ent->s.angles );

		return tr.startsolid || tr.allsolid ? true : false;
	}

	return ( BoundsIntersect( mins, maxs, ent->r.absmin, ent->r.absmax ) ) == qtrue;
}


/*
* GClip_TouchTriggers
*/
void GClip_TouchTriggers( edict_t *ent )
{
	int i, num;
	edict_t	*hit;
	int touch[MAX_EDICTS];
	vec3_t mins, maxs;

	// dead things don't activate triggers!
	if( ent->r.client && G_IsDead( ent ) )
		return;

	VectorAdd( ent->s.origin, ent->r.mins, mins );
	VectorAdd( ent->s.origin, ent->r.maxs, maxs );

	// FIXME: should be s.origin + mins and s.origin + maxs because of absmin and absmax padding?
	num = GClip_AreaEdicts( ent->r.absmin, ent->r.absmax, touch, MAX_EDICTS, AREA_TRIGGERS, 0 );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ )
	{
		if( !ent->r.inuse )
			break;

		hit = &game.edicts[touch[i]];
		if( !hit->r.inuse )
			continue;

		if( !hit->touch && !hit->asTouchFunc )
			continue;

		if( !hit->item && !GClip_EntityContact( mins, maxs, hit ) )
			continue;

		G_CallTouch( hit, ent, NULL, 0 );
	}
}

void G_PMoveTouchTriggers( pmove_t *pm )
{
	int i, num;
	edict_t	*hit;
	int touch[MAX_EDICTS];
	vec3_t mins, maxs;
	edict_t	*ent;

	if( pm->playerState->POVnum <= 0 || (int)pm->playerState->POVnum > gs.maxclients )
		return;

	ent = game.edicts + pm->playerState->POVnum;
	if( !ent->r.client || G_IsDead( ent ) )  // dead things don't activate triggers!
		return;

	// update the entity with the new position
	VectorCopy( pm->playerState->pmove.origin, ent->s.origin );
	VectorCopy( pm->playerState->pmove.velocity, ent->velocity );
	VectorCopy( pm->playerState->viewangles, ent->s.angles );
	ent->viewheight = pm->playerState->viewheight;
	VectorCopy( pm->mins, ent->r.mins );
	VectorCopy( pm->maxs, ent->r.maxs );

	ent->waterlevel = pm->waterlevel;
	ent->watertype = pm->watertype;
	if( pm->groundentity == -1 )
	{
		ent->groundentity = NULL;
	}
	else
	{
		ent->groundentity = &game.edicts[pm->groundentity];
		ent->groundentity_linkcount = ent->groundentity->r.linkcount;
	}

	GClip_LinkEntity( ent );

	VectorAdd( pm->playerState->pmove.origin, pm->mins, mins );
	VectorAdd( pm->playerState->pmove.origin, pm->maxs, maxs );

	num = GClip_AreaEdicts( mins, maxs, touch, MAX_EDICTS, AREA_TRIGGERS, 0 );

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for( i = 0; i < num; i++ )
	{
		if( !ent->r.inuse )
			break;

		hit = &game.edicts[touch[i]];
		if( !hit->r.inuse )
			continue;

		if( !hit->touch && !hit->asTouchFunc )
			continue;

		if( !hit->item && !GClip_EntityContact( mins, maxs, hit ) )
			continue;

		G_CallTouch( hit, ent, NULL, 0 );
	}
}

/*
* GClip_FindBoxInRadius
* Returns entities that have their boxes within a spherical area
*/
edict_t *GClip_FindBoxInRadius4D( edict_t *from, vec3_t org, float rad, int timeDelta )
{
	int i, j;
	c4clipedict_t *check;
	vec3_t mins, maxs;
	int fromNum;

	if( !from ) from = world;
	fromNum = ENTNUM( from ) + 1;

	for( i = fromNum; i < game.numentities; i++ )
	{
		if( !game.edicts[i].r.inuse )
			continue;

		check = GClip_GetClipEdictForDeltaTime( i, timeDelta );
		if( !check->r.inuse )
			continue;
		if( check->r.solid == SOLID_NOT )
			continue;
		// make absolute mins and maxs
		for( j = 0; j < 3; j++ )
		{
			mins[j] = check->s.origin[j] + check->r.mins[j];
			maxs[j] = check->s.origin[j] + check->r.maxs[j];
		}
		if( !BoundsAndSphereIntersect( mins, maxs, org, rad ) )
			continue;

		return &game.edicts[i]; // return realtime entity
	}

	return NULL;
}

void G_SplashFrac4D( int entNum, vec3_t hitpoint, float maxradius, vec3_t pushdir, float *kickFrac, float *dmgFrac, int timeDelta )
{
	c4clipedict_t *clipEnt;

	clipEnt = GClip_GetClipEdictForDeltaTime( entNum, timeDelta );
	G_SplashFrac( clipEnt->s.origin, clipEnt->r.mins, clipEnt->r.maxs, hitpoint, maxradius, pushdir, kickFrac, dmgFrac );
}

entity_state_t *G_GetEntityStateForDeltaTime( int entNum, int deltaTime )
{
	c4clipedict_t *clipEnt;

	if( entNum == -1 )
		return NULL;

	assert( entNum >= 0 && entNum < MAX_EDICTS );

	clipEnt = GClip_GetClipEdictForDeltaTime( entNum, deltaTime );

	return &clipEnt->s;
}

