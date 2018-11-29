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

// gs_items.c	-	game shared items definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

#define SHELL_TIME  30
#define QUAD_TIME   30
#define REGEN_TIME  30

/*
*
* ITEM DEFS
*
*/

gsitem_t itemdefs[] =
{
	{
		NULL
	}, // leave index 0 alone


	//-----------------------------------------------------------
	// WEAPONS ITEMS
	//-----------------------------------------------------------

	// WEAP_GUNBLADE = 1

	//QUAKED weapon_gunblade
	//always owned, never in the world
	{
		"weapon_gunblade",          // entity name
		WEAP_GUNBLADE, // item tag, weapon model for weapons
		IT_WEAPON,                  // item type
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_STAY_COOP, // game flags

		{ PATH_GUNBLADE_MODEL, 0 }, // models 1 and 2
		PATH_GUNBLADE_ICON,         // icon
		NULL,                       // image for simpleitem
		S_PICKUP_WEAPON,            // pickup sound
		0,                          // effects

		"Gunblade",                 // pickup name
		"GB",                       // short name
		S_COLOR_WHITE,              // message color  // TODO: add color
		1,                          // count of items given at pickup
		1,
		AMMO_GUNBLADE,                 // strong ammo tag
		AMMO_WEAK_GUNBLADE,         // weak ammo tag
		NULL,                       // miscelanea info pointer
		PATH_GUNBLADEBLAST_STRONG_MODEL, NULL, NULL
	},

	//QUAKED weapon_machinegun
	//always owned, never in the world
	{
		"weapon_machinegun",
		WEAP_MACHINEGUN,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_MACHINEGUN_MODEL, PATH_MACHINEGUN_BARREL_MODEL },
		PATH_MACHINEGUN_ICON,
		PATH_MACHINEGUN_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Machinegun", "MG", S_COLOR_GREY,
		1,
		1,
		AMMO_BULLETS,
		AMMO_WEAK_BULLETS,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED riotgun
	{
		"weapon_riotgun",
		WEAP_RIOTGUN,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_RIOTGUN_MODEL, 0 },
		PATH_RIOTGUN_ICON,
		PATH_RIOTGUN_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Riotgun", "RG", S_COLOR_ORANGE,
		1,
		1,
		AMMO_SHELLS,
		AMMO_WEAK_SHELLS,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"weapon_grenadelauncher",
		WEAP_GRENADELAUNCHER,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_GRENADELAUNCHER_MODEL, PATH_GRENADELAUNCHER_BARREL_MODEL },
		PATH_GRENADELAUNCHER_ICON,
		PATH_GRENADELAUNCHER_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Grenade Launcher", "GL", S_COLOR_BLUE,
		1,
		1,
		AMMO_GRENADES,
		AMMO_WEAK_GRENADES,
		NULL,
		PATH_GRENADE_WEAK_MODEL " " PATH_GRENADE_STRONG_MODEL,
		NULL, NULL
	},

	//QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"weapon_rocketlauncher",
		WEAP_ROCKETLAUNCHER,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_ROCKETLAUNCHER_MODEL, 0 },
		PATH_ROCKETLAUNCHER_ICON,
		PATH_ROCKETLAUNCHER_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Rocket Launcher", "RL", S_COLOR_RED,
		1,
		1,
		AMMO_ROCKETS,
		AMMO_WEAK_ROCKETS,
		NULL,
		PATH_ROCKET_WEAK_MODEL " " PATH_ROCKET_STRONG_MODEL,
		S_WEAPON_ROCKET_W_FLY " " S_WEAPON_ROCKET_S_FLY,
		NULL
	},

	//QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"weapon_plasmagun",
		WEAP_PLASMAGUN,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_PLASMAGUN_MODEL, PATH_PLASMAGUN_BARREL_MODEL },
		PATH_PLASMAGUN_ICON,
		PATH_PLASMAGUN_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Plasmagun", "PG", S_COLOR_GREEN,
		1,
		1,
		AMMO_PLASMA,
		AMMO_WEAK_PLASMA,
		NULL,
		PATH_PLASMA_WEAK_MODEL " " PATH_PLASMA_STRONG_MODEL,
		S_WEAPON_PLASMAGUN_W_FLY " " S_WEAPON_PLASMAGUN_S_FLY,
		NULL
	},

	//QUAKED lasergun
	{
		"weapon_lasergun",
		WEAP_LASERGUN,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_LASERGUN_MODEL, 0 },
		PATH_LASERGUN_ICON,
		PATH_LASERGUN_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Lasergun", "LG", S_COLOR_YELLOW,
		1,
		1,
		AMMO_LASERS,
		AMMO_WEAK_LASERS,
		NULL,
		NULL,
		S_WEAPON_LASERGUN_S_HUM " " S_WEAPON_LASERGUN_W_HUM " "
		S_WEAPON_LASERGUN_S_QUAD_HUM " " S_WEAPON_LASERGUN_W_QUAD_HUM " "
		S_WEAPON_LASERGUN_S_STOP " " S_WEAPON_LASERGUN_W_STOP " "
		S_WEAPON_LASERGUN_HIT_0 " " S_WEAPON_LASERGUN_HIT_1 " " S_WEAPON_LASERGUN_HIT_2,
		NULL
	},

	//QUAKED electrobolt
	{
		"weapon_electrobolt",
		WEAP_ELECTROBOLT,
		IT_WEAPON,
		ITFLAG_PICKABLE | ITFLAG_USABLE | ITFLAG_DROPABLE | ITFLAG_STAY_COOP,

		{ PATH_ELECTROBOLT_MODEL, 0 },
		PATH_ELECTROBOLT_ICON,
		PATH_ELECTROBOLT_SIMPLEITEM,
		S_PICKUP_WEAPON,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Electrobolt", "EB", S_COLOR_CYAN,
		1,
		1,
		AMMO_BOLTS,
		AMMO_WEAK_BOLTS,
		NULL,
		/*PATH_ELECTROBOLT_WEAK_MODEL*/ NULL,
		S_WEAPON_ELECTROBOLT_HIT,
		NULL
	},

	//-----------------------------------------------------------
	// AMMO ITEMS
	//-----------------------------------------------------------

	// AMMO_CELLS = WEAP_TOTAL

	//QUAKED ammo_gunblade (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_gunblade",
		AMMO_GUNBLADE,
		IT_AMMO,
		ITFLAG_PICKABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_GUNBLADE_AMMO_ICON,
		PATH_GUNBLADE_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Cells", "cells", S_COLOR_YELLOW,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_machinegun",
		AMMO_BULLETS,
		IT_AMMO,
		ITFLAG_PICKABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_MACHINEGUN_AMMO_ICON,
		PATH_MACHINEGUN_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Bullets", "bullets", S_COLOR_GREY,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_riotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_riotgun",
		AMMO_SHELLS,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_RIOTGUN_AMMO_ICON,
		PATH_RIOTGUN_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Shells", "shells", S_COLOR_ORANGE,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_grenadelauncher",
		AMMO_GRENADES,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_GRENADELAUNCHER_AMMO_ICON,
		PATH_GRENADELAUNCHER_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Grenades", "grens", S_COLOR_BLUE,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_rocketlauncher",
		AMMO_ROCKETS,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_ROCKETLAUNCHER_AMMO_ICON,
		PATH_ROCKETLAUNCHER_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Rockets", "rockets", S_COLOR_RED,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_plasmagun",
		AMMO_PLASMA,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_PLASMAGUN_AMMO_ICON,
		PATH_PLASMAGUN_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Plasma", "plasma", S_COLOR_GREEN,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_lasergun (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_lasergun",
		AMMO_LASERS,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_LASERGUN_AMMO_ICON,
		PATH_LASERGUN_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Lasers", "lasers", S_COLOR_YELLOW,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_electrobolt (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_electrobolt",
		AMMO_BOLTS,
		IT_AMMO,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_ELECTROBOLT_AMMO_ICON,
		PATH_ELECTROBOLT_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Bolts", "bolts", S_COLOR_CYAN,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//------------------------
	// WEAK AMMOS
	// most are not pickable, so not spawnable, and given at picking up the weapon
	//------------------------
	//QUAKED ammo_gunblade_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_gunblade_weak",
		AMMO_WEAK_GUNBLADE,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Blades", "weak blades", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_machinegun_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_machinegun_weak",
		AMMO_WEAK_BULLETS,
		IT_AMMO,
		0,              // NOT SPAWNABLE

		{ PATH_AMMO_BOX_MODEL, PATH_AMMO_BOX_MODEL2 },
		PATH_MACHINEGUN_AMMO_ICON,
		PATH_MACHINEGUN_AMMO_ICON,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE | EF_AMMOBOX,

		"Weak bullets", "11mm", S_COLOR_GREY,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_riotgun_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_riotgun_weak",
		AMMO_WEAK_SHELLS,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Shells", "weak shells", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_grenadelauncher_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_grenadelauncher_weak",
		AMMO_WEAK_GRENADES,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Grenades", "weak grens", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_rocketlauncher_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_rocketlauncher_weak",
		AMMO_WEAK_ROCKETS,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Rockets", "weak rox", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_plasmagun_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_plasmagun_weak",
		AMMO_WEAK_PLASMA,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Plasma", "weak plasma", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_lasergun_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_lasergun_weak",
		AMMO_WEAK_LASERS,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Lasers", "weak lasers", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED ammo_electrobolt_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"ammo_electrobolt_weak",
		AMMO_WEAK_BOLTS,
		IT_AMMO,
		0,

		{ 0, 0 },
		NULL,
		NULL,
		NULL,
		0,

		"Weak Bolts", "weak bolts", NULL,
		0, // actual value comes from weapondefs instead
		0, // actual value comes from weapondefs instead
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//------------------------
	// HEALTH ITEMS
	//------------------------

	//QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_health_small",
		HEALTH_SMALL,
		IT_HEALTH,
		ITFLAG_PICKABLE,

		{ PATH_SMALL_HEALTH_MODEL, 0 },
		PATH_HEALTH_5_ICON,
		PATH_HEALTH_5_SIMPLEITEM,
		S_PICKUP_HEALTH_SMALL,
		EF_ROTATE_AND_BOB,

		"5 Health", "5H", S_COLOR_GREEN,
		5,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_health_medium (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_health_medium",
		HEALTH_MEDIUM,
		IT_HEALTH,
		ITFLAG_PICKABLE,

		{ PATH_MEDIUM_HEALTH_MODEL, 0 },
		PATH_HEALTH_25_ICON,
		PATH_HEALTH_25_SIMPLEITEM,
		S_PICKUP_HEALTH_MEDIUM,
		EF_ROTATE_AND_BOB,

		"25 Health", "25H", S_COLOR_YELLOW,
		25,
		100,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_health_large",
		HEALTH_LARGE,
		IT_HEALTH,
		ITFLAG_PICKABLE,

		{ PATH_LARGE_HEALTH_MODEL, 0 },
		PATH_HEALTH_50_ICON,
		PATH_HEALTH_50_SIMPLEITEM,
		S_PICKUP_HEALTH_LARGE,
		EF_ROTATE_AND_BOB,

		"50 Health", "50H", S_COLOR_ORANGE,
		50,
		100,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_health_mega",
		HEALTH_MEGA,
		IT_HEALTH,
		ITFLAG_PICKABLE,

		{ PATH_MEGA_HEALTH_MODEL, 0 },
		PATH_HEALTH_100_ICON,
		PATH_HEALTH_100_SIMPLEITEM,
		S_PICKUP_HEALTH_MEGA,
		EF_ROTATE_AND_BOB,

		"Mega Health", "MH", S_COLOR_MAGENTA,
		100,
		200,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_health_ultra (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_health_ultra",
		HEALTH_ULTRA,
		IT_HEALTH,
		ITFLAG_PICKABLE,

		{ PATH_ULTRA_HEALTH_MODEL, 0 },
		PATH_HEALTH_ULTRA_ICON,
		PATH_HEALTH_ULTRA_SIMPLEITEM,
		S_PICKUP_HEALTH_MEGA,
		EF_ROTATE_AND_BOB,

		"Ultra Health", "UH", S_COLOR_CYAN,
		100,
		200,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},


	//------------------------
	// POWERUP ITEMS
	//------------------------
	//QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_quad",
		POWERUP_QUAD,
		IT_POWERUP,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_QUAD_MODEL, PATH_QUAD_LIGHT_MODEL },
		PATH_QUAD_ICON,
		PATH_QUAD_SIMPLEITEM,
		S_PICKUP_QUAD,
		EF_OUTLINE | EF_ROTATE_AND_BOB,

		"Quad Damage", "QUAD", NULL,
		QUAD_TIME,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL,
		S_QUAD_FIRE " " S_ITEM_QUAD_RESPAWN,
		// S_QUAD_USE " " S_QUAD_FIRE,
		NULL
	},

	//QUAKED item_warshell (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_warshell",
		POWERUP_SHELL,
		IT_POWERUP,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_WARSHELL_BELT_MODEL, PATH_WARSHELL_SPHERE_MODEL },
		PATH_SHELL_ICON,
		PATH_SHELL_SIMPLEITEM,
		S_PICKUP_SHELL,
		EF_OUTLINE | EF_ROTATE_AND_BOB,

		"WarShell", "Shell", NULL,
		SHELL_TIME,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL,
		S_ITEM_WARSHELL_RESPAWN,
		//S_SHELL_USE,
		NULL
	},

	//QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_regen",
		POWERUP_REGEN,
		IT_POWERUP,
		ITFLAG_PICKABLE | ITFLAG_DROPABLE,

		{ PATH_REGEN_MODEL },
		PATH_REGEN_ICON,
		PATH_REGEN_SIMPLEITEM,
		S_PICKUP_REGEN,
		EF_OUTLINE | EF_ROTATE_AND_BOB,

		"Regeneration", "Regen", NULL,
		REGEN_TIME,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL,
		S_ITEM_REGEN_RESPAWN,
		NULL
	},

	//QUAKED item_ammopack_weak (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_ammopack_weak",
		AMMO_PACK_WEAK,
		IT_AMMO,
		ITFLAG_PICKABLE,

		{ PATH_AMMO_PACK_MODEL, 0 },
		PATH_AMMOPACK_ICON,
		PATH_AMMOPACK_SIMPLEITEM,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Ammo Pack Weak", "weakpack", NULL,
		1,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_ammopack_strong (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_ammopack_strong",
		AMMO_PACK_STRONG,
		IT_AMMO,
		ITFLAG_PICKABLE,

		{ PATH_AMMO_PACK_MODEL, 0 },
		PATH_AMMOPACK_ICON,
		PATH_AMMOPACK_SIMPLEITEM,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Ammo Pack Strong", "strongpack", NULL,
		1,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	//QUAKED item_ammopack (.3 .3 1) (-16 -16 -16) (16 16 16)
	{
		"item_ammopack",
		AMMO_PACK,
		IT_AMMO,
		ITFLAG_PICKABLE,

		{ PATH_AMMO_PACK_MODEL, 0 },
		PATH_AMMOPACK_ICON,
		PATH_AMMOPACK_SIMPLEITEM,
		S_PICKUP_AMMO,
		EF_ROTATE_AND_BOB | EF_OUTLINE,

		"Ammo Pack", "pack", NULL,
		1,
		0,
		AMMO_NONE,
		AMMO_NONE,
		NULL,
		NULL, NULL, NULL
	},

	// end of list marker
	{ NULL }
};

STATIC_ASSERT( ARRAY_COUNT( itemdefs ) >= GS_MAX_ITEM_TAGS );
STATIC_ASSERT( ARRAY_COUNT( itemdefs ) <= GS_MAX_ITEM_TAGS + 2 );

//====================================================================

/*
* GS_FindItemByTag
*/
gsitem_t *GS_FindItemByTag( const int tag ) {
	gsitem_t *it;

	assert( tag >= 0 );
	assert( tag < GS_MAX_ITEM_TAGS );

	if( tag <= 0 || tag >= GS_MAX_ITEM_TAGS ) {
		return NULL;
	}

	it = &itemdefs[tag];

	assert( tag == it->tag );
	if( tag == it->tag ) {
		return it;
	}

	for( it = &itemdefs[1]; it->classname; it++ ) {
		if( tag == it->tag ) {
			return it;
		}
	}

	return NULL;
}

/*
* GS_FindItemByClassname
*/
const gsitem_t *GS_FindItemByClassname( const char *classname ) {
	const gsitem_t *it;

	if( !classname ) {
		return NULL;
	}

	for( it = &itemdefs[1]; it->classname; it++ ) {
		if( !Q_stricmp( classname, it->classname ) ) {
			return it;
		}
	}

	return NULL;
}

/*
* GS_FindItemByName
*/
const gsitem_t *GS_FindItemByName( const char *name ) {
	const gsitem_t *it;

	if( !name ) {
		return NULL;
	}

	for( it = &itemdefs[1]; it->classname; it++ ) {
		if( !Q_stricmp( name, it->name ) || !Q_stricmp( name, it->shortname ) ) {
			return it;
		}
	}

	return NULL;
}

/*
* GS_Cmd_UseItem
*/
const gsitem_t *GS_Cmd_UseItem( player_state_t *playerState, const char *string, int typeMask ) {
	const gsitem_t *item = NULL;

	assert( playerState );

	if( playerState->pmove.pm_type >= PM_SPECTATOR ) {
		return NULL;
	}

	if( !string || !string[0] ) {
		return NULL;
	}

	if( Q_isdigit( string ) ) {
		int tag = atoi( string );
		item = GS_FindItemByTag( tag );
	} else {
		item = GS_FindItemByName( string );
	}

	if( !item ) {
		return NULL;
	}

	if( typeMask && !( item->type & typeMask ) ) {
		return NULL;
	}

	// we don't have this item in the inventory
	if( !playerState->inventory[item->tag] ) {
		if( gs.module == GS_MODULE_CGAME && !( item->type & IT_WEAPON ) ) {
			gs.api.Printf( "Item %s is not in inventory\n", item->name );
		}
		return NULL;
	}

	// see if we can use it

	if( !( item->flags & ITFLAG_USABLE ) ) {
		return NULL;
	}

	if( item->type & IT_WEAPON ) {
		if( !( playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WEAPONSWITCH ) ) {
			return NULL;
		}

		if( item->tag == playerState->stats[STAT_PENDING_WEAPON] ) { // it's already being loaded
			return NULL;
		}

		// check for need of any kind of ammo/fuel/whatever
		if( item->ammo_tag != AMMO_NONE && item->weakammo_tag != AMMO_NONE ) {
			gs_weapon_definition_t *weapondef = GS_GetWeaponDef( item->tag );

			if( weapondef ) {
				// do we have any of these ammos ?
				if( playerState->inventory[item->weakammo_tag] >= weapondef->firedef_weak.usage_count ) {
					return item;
				}

				if( playerState->inventory[item->ammo_tag] >= weapondef->firedef.usage_count ) {
					return item;
				}
			}

			return NULL;
		}

		return item; // one of the weapon modes doesn't require ammo to be fired
	}

	if( item->type & IT_AMMO ) {
		return item;
	}

	if( item->type & IT_HEALTH ) {
		return item;
	}

	if( item->type & IT_POWERUP ) {
		return item;
	}

	return NULL;
}

/*
* GS_Cmd_UseWeaponStep_f
*/
static const gsitem_t *GS_Cmd_UseWeaponStep_f( player_state_t *playerState, int step, int predictedWeaponSwitch ) {
	const gsitem_t *item;
	int curSlot, newSlot;

	assert( playerState );

	if( playerState->pmove.pm_type >= PM_SPECTATOR ) {
		return NULL;
	}

	if( !( playerState->pmove.stats[PM_STAT_FEATURES] & PMFEAT_WEAPONSWITCH ) ) {
		return NULL;
	}

	if( step != -1 && step != 1 ) {
		step = 1;
	}

	if( predictedWeaponSwitch && predictedWeaponSwitch != playerState->stats[STAT_PENDING_WEAPON] ) {
		curSlot = predictedWeaponSwitch;
	} else {
		curSlot = playerState->stats[STAT_PENDING_WEAPON];
	}

	clamp( curSlot, 0, WEAP_TOTAL - 1 );
	newSlot = curSlot;
	do {
		newSlot += step;
		if( newSlot >= WEAP_TOTAL ) {
			newSlot = 0;
		}
		if( newSlot < 0 ) {
			newSlot = WEAP_TOTAL - 1;
		}

		if( ( item = GS_Cmd_UseItem( playerState, va( "%i", newSlot ), IT_WEAPON ) ) != NULL ) {
			return item;
		}
	} while( newSlot != curSlot );

	return NULL;
}

/*
* GS_Cmd_NextWeapon_f
*/
const gsitem_t *GS_Cmd_NextWeapon_f( player_state_t *playerState, int predictedWeaponSwitch ) {
	return GS_Cmd_UseWeaponStep_f( playerState, 1, predictedWeaponSwitch );
}

/*
* GS_Cmd_PrevWeapon_f
*/
const gsitem_t *GS_Cmd_PrevWeapon_f( player_state_t *playerState, int predictedWeaponSwitch ) {
	return GS_Cmd_UseWeaponStep_f( playerState, -1, predictedWeaponSwitch );
}
