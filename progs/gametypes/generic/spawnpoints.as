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

// CPM-style spawn indicators for warmup. 
// Add from gametype scripts with:
// CreateSpawnIndicators(<spawn entity>,TEAM_PLAYERS/TEAM_ALPHA/TEAM_BETA)
// And remember to remove indicators after warmup: SpawnIndicators::DeleteAll()

class Spawnpoint
{
    Entity @model;
    Entity @decal;

    private void Free()
    {
        if ( @this.model != null )
        {
            this.model.freeEntity();
            @this.model = null;
        }

        if ( @this.decal != null )
        {
            this.decal.freeEntity();
            @this.decal = null;
        }
    }

    Spawnpoint()
    {
 		@this.model = null;
		@this.decal = null;	
    }

    ~Spawnpoint()
    {
 		Free();
    }

    void CreateDecal( Entity @spawn, int team, int modelindex, int decalindex )
	{
        if ( @spawn == null )
            return;
    
        if ( ( G_PointContents( spawn.origin ) & CONTENTS_NODROP ) != 0 )
            return;
  
        Trace tr;
        tr.doTrace( spawn.origin, vec3Origin, vec3Origin, spawn.origin , -1, ::MASK_DEADSOLID );

		Entity @model = @G_SpawnEntity( "spawnpoint_model" );
		@this.model = @model;
        model.type = ET_GENERIC;
        model.solid = SOLID_NOT;
        model.origin = tr.endPos + Vec3( 0.0f, 0.0f, 16.0f );
        model.modelindex = modelindex;
        model.svflags = SVF_BROADCAST;
        model.takeDamage = DAMAGE_NO;
        model.team = team;
		model.effects = EF_ROTATE_AND_BOB;
        model.linkEntity();

		Entity @decal = @G_SpawnEntity( "spawnpoint_decal" );
        @this.decal = @decal;
        decal.type = ET_DECAL;
        decal.solid = SOLID_NOT;
        decal.origin = tr.endPos;
        decal.origin2 = Vec3( 0.0f, 0.0f, 1.0f );
        decal.modelindex = decalindex;
        decal.modelindex2 = 0; // rotation angle for ET_DECAL
        decal.team = team;
        decal.frame = 26; // radius in case of ET_DECAL
        decal.svflags = SVF_BROADCAST|SVF_TRANSMITORIGIN2;
        decal.takeDamage = DAMAGE_NO;
        decal.linkEntity();
    }
}

namespace SpawnIndicators
{

const uint MAX_INDICATORS = 64;
array<Spawnpoint @> indicators(MAX_INDICATORS);
uint numIndicators = 0;

void Create( const String &className, int team )
{
    ::Entity @spawn;
	int modelindex, decalindex;
 
	// precache
    modelindex = ::G_ModelIndex( "models/objects/misc/playerspawn.md3" );
    decalindex = ::G_ImageIndex( "gfx/misc/playerspawnmarker" );

    // count spawns for allocating the array
	array<Entity @> @ents = G_FindByClassname( className );

	uint maxIndicators = ents.size();
	if( numIndicators + maxIndicators > MAX_INDICATORS )
		maxIndicators = MAX_INDICATORS - numIndicators;

	for( uint i = 0; i < maxIndicators; i++ )
    {
		Spawnpoint indicator();
        indicator.CreateDecal( ents[i], team, modelindex, decalindex );
		@indicators[numIndicators++] = @indicator;
   }	
}

void DeleteAll()
{
	for( uint i = 0; i < numIndicators; i++ ) {
		@indicators[i] = null;
	}
	numIndicators = 0;
}

}
