/*
Copyright (C) 2009-2010 Chasseur de bots

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

void CTFT_RespawnQueuedPlayers()
{
    for ( int i = 0; i < maxClients; i++ )
    {
    	GetPlayer( i ).checkRespawnQueue();
    }
}

void CTFT_ResetRespawnQueue()
{
    for ( int i = 0; i < maxClients; i++ )
    {
    	GetPlayer( i ).resetTimers();
    }
}

void CTFT_RemoveTurrets()
{
    for ( int i = 0; i < MAX_TURRETS; i++ )
    {
        if ( gtTurrets[i].inuse == true )
        {
            gtTurrets[i].Free();
        }
    }
}

void CTFT_RemoveDispensers()
{
    for ( int i = 0; i < MAX_DISPENSERS; i++ )
    {
        if ( gtDispensers[i].inuse == true )
        {
            gtDispensers[i].Free();
        }
    }
}

void CTFT_RemoveRevivers()
{
    for ( int i = 0; i < maxClients; i++ )
    {
        if ( gtRevivers[i].inuse == true )
        {
            gtRevivers[i].Free();
        }
    }
}

void CTFT_RemoveBombs()
{
    for ( int i = 0; i < MAX_BOMBS; i++ )
    {
        if ( gtBombs[i].inuse == true )
        {
            gtBombs[i].Free();
        }
    }
}

bool CTFT_IsMaxTurretsReached( int team )
{
    int ii = 0;
    for ( int i = 0; i < MAX_TURRETS; i++ )
    {
        if ( gtTurrets[i].inuse == true && gtTurrets[i].bodyEnt.team == team )
        {
            ii++;
        }
    }

    if( ii >= CTFT_MAXTURRETS_PER_TEAM )
        return true;
    else
        return false;
}

bool CTFT_IsMaxDispensersReached( int team )
{
    int ii = 0;
    for ( int i = 0; i < MAX_DISPENSERS; i++ )
    {
        if ( gtDispensers[i].inuse == true && gtDispensers[i].bodyEnt.team == team )
        {
            ii++;
        }
    }

    if( ii >= CTFT_MAXDISPENSERS_PER_TEAM )
        return true;
    else
        return false;
}

bool CTFT_RemoveItemsByName( String type )
{
	Item @tmp = G_GetItemByName( type );
	if (@tmp == null)
		return false;
	
	array<Entity @> @ents = G_FindByClassname( tmp.classname );
    for( uint i = 0; i < ents.size(); i++ )
   		ents[i].freeEntity();
	return true;
}

bool CTFT_TeamHasTooMany( int team, String type, uint count )
{
	Item @tmp = G_GetItemByName( type );
	if (@tmp == null)
		return false;

	array<Entity @> @ents = G_FindByClassname( tmp.classname );
	if( ents.size() > count )
		return true;
	return false;
}

void CTFT_DeathDrop( Client @client, String type )
{
    Item @item;
    Entity @dropped = null;

    @item = @G_GetItemByName( type );

    if ( @item == null )
        return;

    if ( @client == null )
        return;

	if ( CTFT_TeamHasTooMany( client.team, type, 16 ) )
	{
		return;
	}

	int randombubbles = int( brandom( 2, 4 ) );

	for ( int i = 0 ; i < randombubbles ; i++ )
	{
		@dropped = @client.getEnt().dropItem( item.tag );
		if ( @dropped == null )
		{
			client.printMessage( "Couldn't drop a " + item.name + "\n" );
		}
		else
		{
			float z = brandom(150,270);
			float x = brandom(1,359);
			float y = brandom(1,359);

			Vec3 velocity;
			velocity = dropped.velocity;
			velocity.z = 0;
			velocity.normalize();
			velocity *= 400;
			velocity.z = z;
			velocity.x = x;
			velocity.y = y;
			dropped.velocity = velocity;
		}
	}
}

bool CTFT_LookAtEntity( Vec3 origin, Vec3 angles, Entity @lookTarget, int ignoreNum, bool lockPitch, int backOffset, int upOffset, Vec3 &out lookOrigin, Vec3 &out lookAngles )
{
    if ( @lookTarget == null )
        return false;

    bool visible = true;

    Vec3 start, end, mins, maxs, dir;
    Trace trace;

    start = end = origin;
    if ( upOffset != 0 )
    {
        end.z += upOffset;
        trace.doTrace( start, vec3Origin, vec3Origin, end, ignoreNum, MASK_OPAQUE );
        if ( trace.fraction < 1.0f )
        {
            start = trace.endPos + ( trace.planeNormal * 0.1f );
        }
    }

    lookTarget.getSize( mins, maxs );
    end = lookTarget.origin + ( 0.5 * ( maxs + mins ) );

    if ( !trace.doTrace( start, vec3Origin, vec3Origin, end, ignoreNum, MASK_OPAQUE ) )
    {
        if ( trace.entNum != lookTarget.entNum )
            visible = false;
    }

    if ( lockPitch )
        end.z = lookOrigin.z;

    if ( backOffset != 0 )
    {
        // trace backwards from dest to origin projected to backoffset
        dir = start - end;
        dir.normalize();
        Vec3 newStart = start + ( dir * backOffset );

        trace.doTrace( start, vec3Origin, vec3Origin, newStart, ignoreNum, MASK_OPAQUE );
        start = trace.endPos;
        if ( trace.fraction < 1.0f )
        {
            start += ( trace.planeNormal * 0.1f );
        }
    }

    dir = end - start;

    lookOrigin = start;
    lookAngles = dir.toAngles();

    return visible;
}
