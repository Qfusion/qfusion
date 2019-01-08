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

// gs_weapondefs.c	-	hard coded weapon definitions

#include "q_arch.h"
#include "q_math.h"
#include "q_shared.h"
#include "q_comref.h"
#include "q_collision.h"
#include "gs_public.h"

#define INSTANT 0

#define WEAPONDOWN_FRAMETIME 50
#define WEAPONUP_FRAMETIME 50
#define DEFAULT_BULLET_SPREAD 350

gs_weapon_definition_t gs_weaponDefs[] =
{
	{
		"no weapon",
		WEAP_NONE,
		{
			FIRE_MODE_STRONG,               // fire mode
			AMMO_NONE,                      // ammo tag
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			0,                              // reload frametime
			0,                              // cooldown frametime
			0,                              // projectile timeout
			false,                          // smooth refire

			//damages
			0,                              // damage
			0,                              // selfdamage ratio
			0,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			0,                              // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			0,                              // weapon pickup amount
			0,                              // pickup amount
			0,                              // max amount
			0                               // low ammo threshold
		},

		{
			FIRE_MODE_WEAK,                 // fire mode
			AMMO_NONE,                      // ammo tag
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			0,                              // reload frametime
			0,                              // cooldown frametime
			0,                              // projectile timeout
			false,                          // smooth refire

			//damages
			0,                              // damage
			0,                              // selfdamage ratio
			0,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			0,                              // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			0,                              // weapon pickup amount
			0,                              // pickup amount
			0,                              // max amount
			0                               // low ammo threshold
		},
	},

	{
		"Gunblade",
		WEAP_GUNBLADE,
		{
			FIRE_MODE_STRONG,
			AMMO_GUNBLADE,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			600,                            // reload frametime
			0,                              // cooldown frametime
			5000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			35,                             // damage
			1.0,                            // selfdamage ratio
			90,                             // knockback
			80,                             // splash radius
			8,                              // splash minimum damage
			10,                             // splash minimum knockback

			//projectile def
			3000,                           // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			0,                              // weapon pickup amount
			0,                              // pickup amount
			1,                              // max amount
			0                               // low ammo threshold
		},

		{
			FIRE_MODE_WEAK,
			AMMO_NONE,
			0,                              // ammo usage per shot
			0,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			600,                            // reload frametime
			0,                              // cooldown frametime
			64,                             // projectile timeout  / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			50,                             // damage
			0,                              // selfdamage ratio
			50,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			0,                              // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			0,                              // weapon pickup amount
			0,                              // pickup amount
			0,                              // max amount
			0                               // low ammo threshold
		},
	},

	{
		"Machinegun",
		WEAP_MACHINEGUN,
		{
			FIRE_MODE_STRONG,
			AMMO_BULLETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			75,                            // reload frametime
			0,                              // cooldown frametime
			6000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			12,                             // damage
			0,                              // selfdamage ratio
			5,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			3000,                        // speed
			0,                             // spread
			0,                             // v_spread

			//ammo
			50,                             // weapon pickup amount
			50,                             // pickup amount
			100,                            // max amount
			20                              // low ammo threshold
		},

		{ },
	},

	{
		"Riotgun",
		WEAP_RIOTGUN,
		{
			FIRE_MODE_STRONG,
			AMMO_SHELLS,
			1,                              // ammo usage per shot
			20,                             // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1100,                           // reload frametime
			0,                              // cooldown frametime
			8192,                           // projectile timeout / projectile range for instant weapons
			false,                          // smooth refire

			//damages
			5,                              // damage
			0,                              // selfdamage ratio (rg cant selfdamage)
			7,                              // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			90,                             // spread
			90,                             // v_spread

			//ammo
			10,                             // weapon pickup amount
			10,                             // pickup amount
			20,                             // max amount
			3                               // low ammo threshold
		},

		{ },
	},

	{
		"Grenade Launcher",
		WEAP_GRENADELAUNCHER,
		{
			FIRE_MODE_STRONG,
			AMMO_GRENADES,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			800,                            // reload frametime
			0,                              // cooldown frametime
			1250,                           // projectile timeout
			false,                          // smooth refire

			//damages
			80,                             // damage
			0.5,                           // selfdamage ratio
			100,                                // knockback
			125,                            // splash radius
			15,                             // splash minimum damage
			35,                             // splash minimum knockback

			//projectile def
			1000,                           // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			10,                             // weapon pickup amount
			10,                             // pickup amount
			20,                             // max amount
			3                               // low ammo threshold
		},

		{ },
	},

	{
		"Rocket Launcher",
		WEAP_ROCKETLAUNCHER,
		{
			FIRE_MODE_STRONG,
			AMMO_ROCKETS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1000,                            // reload frametime
			0,                              // cooldown frametime
			10000,                          // projectile timeout
			false,                          // smooth refire

			//damages
			80,                             // damage
			0.5,                           // selfdamage ratio
			100,                                // knockback
			120,                            // splash radius
			15,                             // splash minimum damage
			45,                             // splash minimum knockback

			//projectile def
			1250,                           // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			5,                              // weapon pickup amount
			10,                             // pickup amount
			20,                             // max amount
			3                               // low ammo threshold
		},

		{ },
	},

	{
		"Plasmagun",
		WEAP_PLASMAGUN,
		{
			FIRE_MODE_STRONG,
			AMMO_PLASMA,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			100,                            // reload frametime
			0,                              // cooldown frametime
			5000,                           // projectile timeout
			false,                          // smooth refire

			//damages
			15,                             // damage
			0.5,                            // selfdamage ratio
			20,                             // knockback
			45,                             // splash radius
			5,                              // splash minimum damage
			1,                              // splash minimum knockback

			//projectile def
			2500,                           // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			50,                             // weapon pickup amount
			100,                            // pickup amount
			150,                            // max amount
			20                              // low ammo threshold
		},

		{ },
	},

	{
		"Lasergun",
		WEAP_LASERGUN,
		{
			FIRE_MODE_STRONG,
			AMMO_LASERS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			50,                             // reload frametime
			0,                              // cooldown frametime
			700,                            // projectile timeout / projectile range for instant weapons
			true,                           // smooth refire

			//damages
			8,                              // damage
			0,                              // selfdamage ratio (lg cant damage)
			14,                             // knockback
			0,                              // splash radius
			0,                              // splash minimum damage
			0,                              // splash minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			50,                             // weapon pickup amount
			100,                            // pickup amount
			150,                            // max amount
			20                              // low ammo threshold
		},

		{ },
	},

	{
		"Electrobolt",
		WEAP_ELECTROBOLT,
		{
			FIRE_MODE_STRONG,
			AMMO_BOLTS,
			1,                              // ammo usage per shot
			1,                              // projectiles fired each shot

			//timings (in msecs)
			WEAPONUP_FRAMETIME,             // weapon up frametime
			WEAPONDOWN_FRAMETIME,           // weapon down frametime
			1250,                           // reload frametime
			0,                              // cooldown frametime
			900,                            // min damage range
			false,                          // smooth refire

			//damages
			75,                             // damage
			0,                              // selfdamage ratio
			80,                             // knockback
			0,                              // splash radius
			75,                             // minimum damage
			35,                             // minimum knockback

			//projectile def
			INSTANT,                        // speed
			0,                              // spread
			0,                              // v_spread

			//ammo
			5,                              // weapon pickup amount
			10,                             // pickup amount
			10,                             // max amount
			3                               // low ammo threshold
		},

		{ },
	},
};

#define GS_NUMWEAPONDEFS ( sizeof( gs_weaponDefs ) / sizeof( gs_weapon_definition_t ) )

/*
* GS_GetWeaponDef
*/
gs_weapon_definition_t *GS_GetWeaponDef( int weapon ) {
	assert( GS_NUMWEAPONDEFS == WEAP_TOTAL );
	assert( weapon >= 0 && weapon < WEAP_TOTAL );
	return &gs_weaponDefs[weapon];
}
