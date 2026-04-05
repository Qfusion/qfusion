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

void ammo_lightning( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_LASERS );
}

void ammo_instas( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_INSTAS );
}

void ammo_bfg( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_PLASMA );
}

void ammo_hmg( Entity @ent )
{
	GENERIC_SpawnItem( ent, AMMO_BULLETS );
}

/*
** WEAPON ITEMS
*/
void weapon_gauntlet( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_GUNBLADE );
}

void weapon_lightning( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_LASERGUN );
}

void weapon_hmg( Entity @ent )
{
	GENERIC_SpawnItem( ent, WEAP_MACHINEGUN );
}

/*
** ARMOR ITEMS
*/
void item_armor_jacket( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_GA );
}

void item_armor_combat( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_YA );
}

void item_armor_body( Entity @ent )
{
	GENERIC_SpawnItem( ent, ARMOR_RA );
}


/*
** HEALTH ITEMS
*/
void Q3_item_health( Entity @ent )
{
	GENERIC_SpawnItem( ent, HEALTH_MEDIUM );
}

/*
** POWERUPS
*/
void item_enviro( Entity @ent )
{
	GENERIC_SpawnItem( ent, POWERUP_SHELL );
}
