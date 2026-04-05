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

void ammo_shells( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_SHELLS );
}

void ammo_grenades( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_GRENADES );
}

void ammo_bullets( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_BULLETS );
}

void ammo_rockets( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_ROCKETS );
}

void ammo_cells( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_PLASMA );
}

void ammo_slugs( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_BOLTS );
}

void item_bandolier( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_PACK );
}

void item_pack( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_PACK );
}

/*
** WEAPON ITEMS
*/
void weapon_shotgun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_RIOTGUN );
}

void weapon_chaingun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_MACHINEGUN );
}

void weapon_hyperblaster( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_PLASMAGUN );
}

void weapon_railgun( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_ELECTROBOLT );
}

void weapon_bfg( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_PLASMAGUN );
}

/*
** ARMOR ITEMS
*/
void item_power_screen( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_YA );
}

void item_power_shield( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_RA );
}
