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

/*
** AMMO ITEMS
*/

const uint WEAPON_SHOTGUN = 1;
const uint WEAPON_ROCKET = 2;
const uint WEAPON_SPIKES = 4;
const uint WEAPON_BIG = 8;

void item_weapon( Entity @ent )
{
	if( (ent.spawnFlags & WEAPON_ROCKET) == WEAPON_ROCKET )
	{
		if( (ent.spawnFlags & WEAPON_BIG) == WEAPON_BIG )
			GENERIC_SpawnItem( ent, AMMO_ROCKETS );
		else
			GENERIC_SpawnItem( ent, AMMO_WEAK_ROCKETS );	
	}
	else if( (ent.spawnFlags & WEAPON_SPIKES) == WEAPON_SPIKES )
	{
		if( (ent.spawnFlags & WEAPON_BIG) == WEAPON_BIG )
			GENERIC_SpawnItem( ent, AMMO_PLASMA );		
		else
			GENERIC_SpawnItem( ent, AMMO_WEAK_PLASMA );
	}	
	else if( (ent.spawnFlags & WEAPON_SHOTGUN) == WEAPON_SHOTGUN )
	{
		if( (ent.spawnFlags & WEAPON_BIG) == WEAPON_BIG )
			GENERIC_SpawnItem( ent, AMMO_SHELLS );
		else
			GENERIC_SpawnItem( ent, AMMO_WEAK_SHELLS );
	}
}

const uint WEAPON_BIG2 = 1;

void item_shells( Entity @ent )
{
	if( (ent.spawnFlags & WEAPON_BIG2) == WEAPON_BIG2 )
		GENERIC_SpawnItem( ent, AMMO_SHELLS );
	else
		GENERIC_SpawnItem( ent, AMMO_WEAK_SHELLS );
}

void item_spikes( Entity @ent )
{
	if( (ent.spawnFlags & WEAPON_BIG2) == WEAPON_BIG2 )
		GENERIC_SpawnItem( ent, AMMO_PLASMA );
	else
		GENERIC_SpawnItem( ent, AMMO_WEAK_PLASMA );
}

void item_rockets( Entity @ent )
{
	if( (ent.spawnFlags & WEAPON_BIG2) == WEAPON_BIG2 )
		GENERIC_SpawnItem( ent, AMMO_ROCKETS );
	else
		GENERIC_SpawnItem( ent, AMMO_WEAK_ROCKETS );
}

void item_cells( Entity @ent )
{
	if( (ent.spawnFlags & WEAPON_BIG2) == WEAPON_BIG2 )
		GENERIC_SpawnItem( ent, AMMO_LASERS );
	else
		GENERIC_SpawnItem( ent, AMMO_WEAK_LASERS );
}

/*
** WEAPON ITEMS
*/
void weapon_supershotgun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_RIOTGUN );
}

void weapon_nailgun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_PLASMAGUN );
}

void weapon_supernailgun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_PLASMAGUN );
}

/*
** ARMOR ITEMS
*/
void item_armor1( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_GA );
}

void item_armor2( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_YA );
}

void item_armorInv( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_RA );
}

/*
** HEALTH ITEMS
*/
const uint	H_ROTTEN = 1;
const uint	H_MEGA = 2;

void Q1_item_health( Entity @ent )
{
	if( ( ent.spawnFlags & H_ROTTEN ) == H_ROTTEN )
		GENERIC_SpawnItem( ent, HEALTH_MEDIUM );
	else if( ( ent.spawnFlags & H_MEGA ) == H_MEGA )
		GENERIC_SpawnItem( ent, HEALTH_MEGA );
	else
		GENERIC_SpawnItem( ent, HEALTH_LARGE );
	ent.spawnFlags = 0;
}

/*
** ARTIFACTS
*/
void item_artifact_super_damage( Entity @ent )
{
	GENERIC_SpawnItem( ent, POWERUP_QUAD );
}

void item_artifact_invulnerability( Entity @ent )
{
	GENERIC_SpawnItem( ent, POWERUP_SHELL );
}

void item_artifact_envirosuit( Entity @ent )
{
	GENERIC_SpawnItem( ent, POWERUP_SHELL );
}

/*
** KEYS
*
* Silver and gold keys are invisible but can still fire targeted entities
*/
void item_key_touch( Entity @ent, Entity @other, const Vec3 planeNormal, int surfFlags )
{
	if( @other.client == null )
		return;
	ent.useTargets( other );
}

void item_key( Entity @ent )
{
	@ent.touch = item_key_touch;
	ent.modelindex = 0;
	ent.solid = SOLID_TRIGGER;
	ent.setSize( Vec3( -16, -16, -24 ), Vec3(  16,  16,  32 ) );
	ent.linkEntity();
}

void item_key1( Entity @ent )
{
	item_key( ent );
}

void item_key2( Entity @ent )
{
	item_key( ent );
}

